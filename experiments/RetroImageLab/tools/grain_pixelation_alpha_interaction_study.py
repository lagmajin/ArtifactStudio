"""Measure how pixelation scale transports grain across alpha boundaries."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import ASSETS, get_masks, load_asset  # noqa: E402
from adaptive_palette_alpha_resize_study import BACKGROUNDS, composite  # noqa: E402
from retro_lab import BAYER_4, ordered_quantize  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-pixelation-alpha"
SEED = 78123
SIZE = 256
CASES = (("handheld", 0.50, 8, 0.0, 0.12, 0.025),
         ("arcade", 0.75, 24, 0.35, 0.20, 0.015),
         ("print", 1.00, 12, 0.70, 0.0, 0.035))
ASSET_NAMES = ("clover_full", "silver_feather")


def load_center_crop(name: str) -> tuple[np.ndarray, dict[str, object]]:
    spec = next(spec for spec in ASSETS if spec[3] == name)
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    side = min(h, w)
    x0, y0 = (w - side) // 2, (h - side) // 2
    resized = Image.fromarray(rgba[y0:y0 + side, x0:x0 + side], "RGBA").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    return np.asarray(resized, dtype=np.uint8), {
        "source": spec[0], "sha256": digest, "provenance": provenance,
        "original dimensions": [w, h], "center crop": [x0, y0, side, side],
    }


def pixelate(rgb: np.ndarray, scale: float) -> np.ndarray:
    h, w = rgb.shape[:2]
    small = (max(1, round(w * scale)), max(1, round(h * scale)))
    reduced = Image.fromarray(np.uint8(np.clip(rgb * 255 + 0.5, 0, 255)), "RGB").resize(small, Image.Resampling.BOX)
    return np.asarray(reduced.resize((w, h), Image.Resampling.NEAREST), dtype=np.float32) / 255.0


def spatial_metrics(residual: np.ndarray, mask: np.ndarray) -> dict[str, float]:
    field = np.mean(residual, axis=2)[mask]
    if field.size == 0:
        return {"RMS": 0.0, "horizontal correlation": 0.0, "vertical correlation": 0.0}
    rms = float(np.sqrt(np.mean(field * field)))
    centered = np.mean(residual, axis=2)
    pairs_x = mask[:, 1:] & mask[:, :-1]
    pairs_y = mask[1:, :] & mask[:-1, :]
    def corr(a: np.ndarray, b: np.ndarray, valid: np.ndarray) -> float:
        if not np.any(valid):
            return 0.0
        av, bv = a[valid], b[valid]
        av, bv = av - np.mean(av), bv - np.mean(bv)
        denom = float(np.sqrt(np.mean(av * av) * np.mean(bv * bv)))
        return float(np.mean(av * bv) / denom) if denom > 1e-12 else 0.0
    return {"RMS": rms,
            "horizontal correlation": corr(centered[:, :-1], centered[:, 1:], pairs_x),
            "vertical correlation": corr(centered[:-1, :], centered[1:, :], pairs_y)}


def evaluate(source: np.ndarray, sigma: float, output: np.ndarray, residual: np.ndarray,
             reference: np.ndarray, output_alpha: np.ndarray, masks: dict[str, np.ndarray]) -> dict[str, object]:
    metrics: dict[str, object] = {
        "alpha changed ratio": float(np.mean(output_alpha != source[:, :, 3])),
        "grain residual": {
            "interpretation": "output minus clean suite pipeline; includes both additive grain and any quantizer-bin changes caused by grain",
            "visible RMS": spatial_metrics(residual, source[:, :, 3] > 0)["RMS"],
            "edge RMS": spatial_metrics(residual, masks["alpha edge"])["RMS"],
            "edge spatial": spatial_metrics(residual, masks["alpha edge"]),
            "flat visible spatial": spatial_metrics(residual, masks["visible"] & ~masks["alpha edge"]),
        },
        "clip low/high channel ratio": [float(np.mean(output <= 0.0)), float(np.mean(output >= 1.0))],
    }
    for bg, color in BACKGROUNDS.items():
        candidate = composite(np.uint8(np.clip(output * 255 + 0.5, 0, 255)), output_alpha, color)
        clean = composite(np.uint8(np.clip(reference * 255 + 0.5, 0, 255)), source[:, :, 3], color)
        delta = np.abs(candidate - clean)
        metrics[f"{bg} composite linear-light sRGB-code MAE"] = {
            "whole": float(np.mean(delta)),
            "alpha edge": float(np.mean(delta[masks["alpha edge"]])) if np.any(masks["alpha edge"]) else 0.0,
            "partial alpha": float(np.mean(delta[masks["partial alpha"]])) if np.any(masks["partial alpha"]) else 0.0,
        }
    return metrics


def make_sheet(records: list[dict[str, object]]) -> Path:
    tile_w, tile_h, label_h = 192, 144, 20
    sheet = Image.new("RGB", (tile_w * 4, label_h + len(records) * (tile_h + label_h)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for col, label in enumerate(("source", "suite: post-pixelation", "before-pixelation", "order delta ×8")):
        draw.text((col * tile_w + 3, 3), label, fill=(245, 245, 245), font=font)
    for index, record in enumerate(records):
        y = label_h + index * (tile_h + label_h)
        draw.text((3, y + 3), f"{record['asset']} / {record['preset']} / scale {record['scale']}", fill=(245, 245, 245), font=font)
        for col, rgb in enumerate(record["display tiles"]):
            tile = Image.fromarray(rgb, "RGB").resize((tile_w, tile_h), Image.Resampling.NEAREST)
            sheet.paste(tile, (col * tile_w, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "grain-pixelation-alpha-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    rng = np.random.default_rng(SEED)
    noise_unit = rng.normal(0.0, 1.0, (SIZE, SIZE)).astype(np.float32)
    assets = {name: load_center_crop(name) for name in ASSET_NAMES}
    results: list[dict[str, object]] = []
    visual_records: list[dict[str, object]] = []
    for name, (source, provenance) in assets.items():
        rgb = source[:, :, :3].astype(np.float32) / 255.0
        masks = get_masks(source[:, :, 3])
        reference = rgb
        for preset, scale, levels, dither, scanlines, sigma in CASES:
            # Build exact suite baseline: resize -> quantize -> scanline; grain is then added.
            base_pixelated = pixelate(rgb, scale)
            baseline_q = ordered_quantize(base_pixelated, levels, BAYER_4, dither)
            row_gain = np.where(np.arange(SIZE)[:, None, None] % 2 == 1, 1.0 - scanlines, 1.0)
            clean_pixelated = np.clip(baseline_q * row_gain, 0, 1)
            output_a = np.clip(clean_pixelated + noise_unit[:, :, None] * sigma, 0.0, 1.0)
            residual_a = output_a - clean_pixelated
            noisy = pixelate(np.clip(rgb + noise_unit[:, :, None] * sigma, 0, 1), scale)
            output_b = np.clip(ordered_quantize(noisy, levels, BAYER_4, dither) * row_gain, 0, 1)
            residual_b = output_b - clean_pixelated
            alpha = source[:, :, 3]
            for label, output, residual in (("suite-post-pixelation", output_a, residual_a),
                                             ("grain-before-pixelation", output_b, residual_b)):
                metrics = evaluate(source, sigma, output, residual, clean_pixelated, alpha, masks)
                results.append({"asset": name, "preset": preset, "scale": scale, "levels": levels,
                                "dither": dither, "scanlines": scanlines, "grain sigma": sigma,
                                "order": label, "metrics": metrics})
            difference = np.clip(np.abs(output_a - output_b) * 8, 0, 1)
            display_tiles = [np.uint8(np.clip(x * 255 + 0.5, 0, 255))
                             for x in (rgb, output_a, output_b, difference)]
            visual_records.append({"asset": name, "preset": preset, "scale": scale,
                                   "display tiles": display_tiles})
    sheet = make_sheet(visual_records)
    report = {
        "question": "How does moving grain across the pixelation pipeline change residual scale and alpha-edge composites?",
        "evaluation plan": {
            "objective": "Compare actual resize/quantize/scanline/grain suite ordering with the same grain field applied before resize, using natural-alpha cutouts and representative backgrounds.",
            "source facts": "retro_lab.apply_retro uses BOX downsample + nearest upsample, ordered quantize, optional scanline, then grain; final output restores original source alpha.",
            "baseline": "Suite order for handheld (.5 scale), arcade (.75), and print (1.0); grain-before-pixelation is a diagnostic counterfactual.",
            "assets": "SHA-verified CC0 clover and silver feather cutouts, center crop and Lanczos resize to 256x256; provenance and crop coordinates included.",
            "conditions": "Same Gaussian mono noise field (seed 78123) for all pairs; preset grain sigmas .025/.015/.035, palette levels and dither from preset, scanline settings from preset. sRGB code-value RGB processing; original alpha retained.",
            "metrics": "Grain residual RMS/spatial correlation in visible, flat, and alpha-edge regions; low/high clipping; black/mid-gray/white linear-light composite error against clean source; alpha-plane equality.",
            "visual review": "Sheet shows source, both orders, and ×8 absolute difference at full 256 crop displayed nearest enlarged. Research author reviewed; no independent reviewer or approved target.",
            "acceptance": "No visual threshold has been set; no pass/fail quality conclusion.",
            "unverified": "Human preference, moving footage, production render route, GPU parity, HDR/ICC handling, performance/memory and approved criteria.",
        },
        "assets": [{"name": name, **provenance} for name, (_, provenance) in assets.items()],
        "results": results,
        "contact sheet": str(sheet.relative_to(ROOT)),
        "limits": "RGB resize model uses Pillow BOX/NEAREST and does not change alpha until final composition. Full de facto suite behavior is represented, but GPU/Core equivalence is not evaluated. Results describe order/scale sensitivity only.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-pixelation-alpha-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Compared grain placement across pixelation scales and alpha edges.")
