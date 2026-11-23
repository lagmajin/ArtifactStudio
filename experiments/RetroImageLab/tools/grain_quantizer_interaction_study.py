"""Probe how grain interacts with palette quantization and Bayer phase."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_weighting_study import ASSETS as VERIFIED_ASSETS, load_asset  # noqa: E402
from retro_lab import BAYER_4, ordered_quantize  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-quantizer-interaction"
SEED = 34091
PHASES = tuple((x, y) for y in range(4) for x in range(4))
PALETTES = {"print-suite": (12, 0.7, 0.035), "fixed-8": (8, 0.7, 0.035), "fixed-4": (4, 1.0, 0.035)}
ASSET_SPECS = (
    ("clover_full", "clover edge / natural texture"),
    ("silver_feather", "feather fiber / highlight"),
    ("pixel_hair_atlas", "directional textured cutout"),
)


def load_crop(name: str, size: int = 256) -> tuple[np.ndarray, dict[str, object]]:
    spec = next(spec for spec in VERIFIED_ASSETS if spec[3] == name)
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    side = min(h, w)
    x0, y0 = (w - side) // 2, (h - side) // 2
    crop = Image.fromarray(rgba[y0:y0 + side, x0:x0 + side], "RGBA").resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(crop, dtype=np.uint8), {"source": spec[0], "sha256": digest, "provenance": provenance,
                                             "original dimensions": [w, h], "center crop": [x0, y0, side, side]}


def shift_matrix(matrix: np.ndarray, dx: int, dy: int) -> np.ndarray:
    return np.roll(np.roll(matrix, dy, axis=0), dx, axis=1)


def quantize(rgb: np.ndarray, levels: int, strength: float, phase: tuple[int, int]) -> np.ndarray:
    bayer = shift_matrix(BAYER_4, phase[0], phase[1])
    return ordered_quantize(rgb, levels, bayer, strength)


def palette_distances(rgb: np.ndarray, levels: int) -> np.ndarray:
    palette = np.linspace(0.0, 1.0, levels, dtype=np.float32)
    return np.min(np.abs(rgb[..., None] - palette[None, None, None, :]), axis=3)


def edge_mask(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    luma = rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    gx = np.zeros_like(luma)
    gy = np.zeros_like(luma)
    gx[:, 1:-1] = (luma[:, 2:] - luma[:, :-2]) * 0.5
    gy[1:-1] = (luma[2:] - luma[:-2]) * 0.5
    grad = np.hypot(gx, gy)
    visible = alpha > 0.98
    threshold = float(np.percentile(grad[visible], 85)) if np.any(visible) else 0.0
    return (grad >= threshold) & visible


def evaluate_order(source: np.ndarray, noise: np.ndarray, levels: int, strength: float,
                   phase: tuple[int, int]) -> dict[str, object]:
    rgb = source[:, :, :3].astype(np.float32) / 255.0
    alpha = source[:, :, 3].astype(np.float32) / 255.0
    edge = edge_mask(rgb, alpha)
    residual = noise[:, :, None]
    # Counterfactual: perturb source RGB first, then let palette quantization absorb some grain.
    grain_then_quantize = quantize(np.clip(rgb + residual, 0.0, 1.0), levels, strength, phase)
    # Current suite semantics: quantize first, then add grain (scanline is disabled in this isolation).
    quantize_then_grain = np.clip(quantize(rgb, levels, strength, phase) + residual, 0.0, 1.0)
    outputs = {"suite: quantize→grain": quantize_then_grain, "counterfactual: grain→quantize": grain_then_quantize}
    metrics: dict[str, object] = {}
    for label, out in outputs.items():
        distance = palette_distances(out, levels)
        grain_signal = out - quantize(rgb, levels, strength, phase)
        composite_weight = alpha[:, :, None]
        per_pixel = np.max(np.abs(grain_signal) * composite_weight, axis=2)
        metrics[label] = {
            "unique channel values": int(np.unique(np.uint8(np.clip(out * 255 + 0.5, 0, 255))).size),
            "mean nearest palette channel distance": float(np.mean(distance[alpha > 0])),
            "mean nearest palette distance at edge": float(np.mean(distance[edge])) if np.any(edge) else 0.0,
            "visible grain residual RMS": float(np.sqrt(np.mean((grain_signal * composite_weight[:, :, None]) ** 2))),
            "visible grain residual RMS at edge": float(np.sqrt(np.mean((grain_signal[edge] * alpha[edge, None]) ** 2))) if np.any(edge) else 0.0,
            "grain visible change ratio >1/255": float(np.mean(per_pixel > (1.0 / 255.0))),
            "alpha changed ratio": 0.0,
        }
    max_delta = np.max(np.abs(outputs["suite: quantize→grain"] - outputs["counterfactual: grain→quantize"]), axis=2) * alpha
    metrics["stage order"] = {
        "visible RGB MAE between orders": float(np.mean(np.abs(outputs["suite: quantize→grain"] - outputs["counterfactual: grain→quantize"]) * alpha[:, :, None])),
        "edge visible RGB MAE between orders": float(np.mean(np.abs(outputs["suite: quantize→grain"][edge] - outputs["counterfactual: grain→quantize"][edge]) * alpha[edge, None])) if np.any(edge) else 0.0,
        "changed visible pixel ratio >1/255": float(np.mean(max_delta > (1.0 / 255.0))),
    }
    metrics["edge pixels"] = int(np.count_nonzero(edge))
    metrics["visible pixels"] = int(np.count_nonzero(alpha > 0))
    return metrics


def make_sheet(tiles: list[tuple[str, str, np.ndarray]]) -> Path:
    tile_w, tile_h, label_h = 192, 144, 20
    sheet = Image.new("RGB", (tile_w * 4, label_h + (tile_h + label_h) * len(tiles)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for col, label in enumerate(("source", "suite: quantize → grain", "counterfactual: grain → quantize", "order difference ×8")):
        draw.text((col * tile_w + 3, 3), label, fill=(245, 245, 245), font=font)
    for row, (asset, condition, rgba) in enumerate(tiles):
        y = label_h + row * (tile_h + label_h)
        draw.text((3, y + 3), f"{asset} / {condition}", fill=(245, 245, 245), font=font)
        for col, rgb in enumerate((rgba[:, :, :3], rgba[:, :, 3:6], rgba[:, :, 6:9], rgba[:, :, 9:12])):
            x = col * tile_w
            image = Image.fromarray(rgb.astype(np.uint8), "RGB").resize((tile_w, tile_h), Image.Resampling.NEAREST)
            sheet.paste(image, (x, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "grain-quantizer-interaction-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    rng = np.random.default_rng(SEED)
    rows: list[dict[str, object]] = []
    tiles: list[tuple[str, str, np.ndarray]] = []
    sources = {name: load_crop(name) for name, role in ASSET_SPECS}
    for name, role in ASSET_SPECS:
        source, provenance = sources[name]
        rgb = source[:, :, :3].astype(np.float32) / 255.0
        alpha = source[:, :, 3].astype(np.float32) / 255.0
        noise_unit = rng.normal(0, 1, rgb.shape[:2]).astype(np.float32)
        for condition, (levels, strength, sigma) in PALETTES.items():
            noise = noise_unit * sigma
            phase_results = []
            for phase_index, phase in enumerate(PHASES):
                metrics = evaluate_order(source, noise, levels, strength, phase)
                phase_results.append(metrics)
                rows.append({"asset": name, "asset role": role, "palette condition": condition,
                             "levels": levels, "dither strength": strength, "grain sigma": sigma,
                             "bayer phase": list(phase), "metrics": metrics})
                if condition == "print-suite" and phase_index in (0, 1, 6, 7):
                    base_quant = quantize(rgb, levels, strength, phase)
                    a = np.clip(rgb, 0, 1)
                    b = np.clip(base_quant + noise[:, :, None], 0, 1)
                    c = quantize(np.clip(rgb + noise[:, :, None], 0, 1), levels, strength, phase)
                    diff = np.clip(np.abs(b - c) * 8, 0, 1)
                    rgba = np.concatenate((a, b, c, diff), axis=2)
                    rgba = np.concatenate((rgba, alpha[:, :, None]), axis=2)
                    outputs = []
                    for candidate in (a, b, c, diff):
                        display = np.uint8(np.clip(candidate * 255 + 0.5, 0, 255))
                        outputs.append(display)
                    tiles.append((name, f"{condition} phase{phase[0]},{phase[1]}", np.concatenate(outputs, axis=2)))
            for metric_path in (("suite: quantize→grain", "visible grain residual RMS"),
                                ("counterfactual: grain→quantize", "visible grain residual RMS"),
                                ("stage order", "visible RGB MAE between orders")):
                values = np.asarray([item[metric_path[0]][metric_path[1]] for item in phase_results], dtype=np.float64)
                # phase_results capture the chosen phase domain; report variability, not as an acceptance score.
                rows.append({"asset": name, "palette condition": condition,
                             "phase variability summary": f"{metric_path[0]} / {metric_path[1]}",
                             "min": float(np.min(values)), "max": float(np.max(values)), "range": float(np.ptp(values))})
    sheet = make_sheet(tiles)
    report = {
        "question": "How do stage order and Bayer phase affect grain survival and palette adherence across the suite?",
        "evaluation plan": {
            "purpose": "Compare the actual suite order (ordered quantization then grain) against grain then quantization, and sweep all 16 Bayer origins under the same noise field.",
            "baseline": "Current retro_lab ordered_quantize then grain, with scanline disabled to isolate these stages; comparison order is diagnostic counterfactual, not a proposed default.",
            "assets": "SHA-256 and provenance verified CC0 clover cutout, silver feather cutout, and pixel-hair cutout; all center-cropped and Lanczos resized to 256x256. Roles are edge/natural texture, highlight/fiber, and directional texture.",
            "conditions": "Seed 34091, same Gaussian field for both orders and all phase shifts per asset-condition. Three palette stress conditions: print-suite (12 levels, dither .7, sigma .035), fixed-8 (.7,.035), fixed-4 (1,.035). sRGB code values; source alpha retained in visible-weight metrics.",
            "metrics": "Nearest scalar palette distance, grain residual RMS and >1/255 visible ratio, edge-specific values, stage-order difference, alpha invariant, and phase min/max range. These describe algorithmic behavior, not color quality.",
            "visual review": "Generated contact sheet includes source, both orders, and amplified absolute difference at representative Bayer phases; researcher reviewed sheet at thumbnail scale. Full-resolution edge crop and playback not yet reviewed.",
            "acceptance": "No approved target or predeclared perceptual threshold exists. No quality pass/fail or preset ranking.",
            "unverified": "Production render route, real motion, GPU/backend parity, HDR/transfer metadata, alternate background appearance, runtime/memory, human panel.",
        },
        "phase sweep": [list(phase) for phase in PHASES],
        "assets": [{"name": name, "role": role, **sources[name][1]} for name, role in ASSET_SPECS],
        "results": rows,
        "contact sheet": str(sheet.relative_to(ROOT)),
        "limits": "Quantization levels are scalar RGB quantization, not a learned or fixed chromatic palette. Reordering makes output distributions intentionally different. Phase shifts use toroidal matrix roll; image boundary does not wrap. Synthetic field model and three CC0 assets do not establish a preferred recipe.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-quantizer-interaction-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Measured grain/quantizer order and Bayer phase across three image roles.")
