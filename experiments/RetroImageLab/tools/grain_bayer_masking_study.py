"""Quantify how suite grain changes the coherent Bayer-pattern component."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import ASSETS, load_asset  # noqa: E402
from retro_lab import BAYER_4, ordered_quantize  # noqa: E402

CAPTURE = ROOT / "captures" / "grain-bayer-masking"
SIZE = 256
BASE_SEED = 50371
SEEDS = tuple(BASE_SEED + i * 97 for i in range(16))
GRAIN_SIGMAS = (0.0, 0.0075, 0.015, 0.025, 0.035)
PHASES = ((0, 0), (1, 0), (0, 1), (1, 1))
CONFIGS = (("8-level dither", 8, 0.5), ("12-level print-like", 12, 0.7), ("24-level arcade-like", 24, 0.35))


def make_synthetic() -> dict[str, tuple[np.ndarray, np.ndarray, dict[str, object]]]:
    yy, xx = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32)
    flat = np.full((SIZE, SIZE, 3), 0.5, dtype=np.float32)
    ramp_x = 0.22 + 0.52 * (xx / (SIZE - 1))
    ramp_y = 0.28 + 0.38 * (yy / (SIZE - 1))
    smooth = np.stack((ramp_x, ramp_y, 0.25 + 0.44 * (0.5 * (xx + yy) / (SIZE - 1))), axis=2)
    smooth += (0.07 * np.exp(-((xx - 128) ** 2 + (yy - 114) ** 2) / (2 * 35 ** 2)))[:, :, None]
    smooth = np.clip(smooth, 0.12, 0.88).astype(np.float32)
    opaque = np.ones((SIZE, SIZE), dtype=bool)
    return {
        "synthetic-flat-midgray": (flat, opaque, {"kind": "deterministic synthetic control", "description": "flat RGB=.5"}),
        "synthetic-smooth-color-ramp": (smooth, opaque, {"kind": "deterministic synthetic control", "description": "smooth unclipped RGB ramp and soft color mound"}),
    }


def load_natural(name: str) -> tuple[np.ndarray, np.ndarray, dict[str, object]]:
    spec = next(spec for spec in ASSETS if spec[3] == name)
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    side = min(h, w)
    x0, y0 = (w - side) // 2, (h - side) // 2
    crop = Image.fromarray(rgba[y0:y0 + side, x0:x0 + side], "RGBA").resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    values = np.asarray(crop, dtype=np.float32) / 255.0
    visible = values[:, :, 3] > (250 / 255)
    return values[:, :, :3], visible, {"source": spec[0], "sha256": digest, "provenance": provenance,
                                       "original dimensions": [w, h], "center crop": [x0, y0, side, side]}


def phase_matrix(dx: int, dy: int) -> np.ndarray:
    return np.roll(np.roll(BAYER_4, dy, axis=0), dx, axis=1)


def fit_coherent_pattern(output: np.ndarray, no_dither: np.ndarray, template: np.ndarray,
                         valid: np.ndarray) -> dict[str, float]:
    residual = output - no_dither
    mask = valid[:, :, None]
    expanded = np.broadcast_to(mask, residual.shape)
    t = template[expanded].astype(np.float64)
    r = residual[expanded].astype(np.float64)
    template_energy = float(np.mean(t * t)) if t.size else 0.0
    residual_energy = float(np.mean(r * r)) if r.size else 0.0
    if template_energy < 1e-15:
        beta, share = 0.0, 0.0
    else:
        beta = float(np.mean(r * t) / template_energy)
        coherent_energy = beta * beta * template_energy
        share = float(coherent_energy / residual_energy) if residual_energy > 1e-15 else 0.0
    visible = output[expanded]
    return {
        "Bayer template RMS": float(np.sqrt(template_energy)),
        "residual RMS from no-dither quantized baseline": float(np.sqrt(residual_energy)),
        "Bayer coherent projection beta": beta,
        "Bayer coherent energy share": share,
        "visible low/high clip channel ratio": [float(np.mean(visible <= 0.0)), float(np.mean(visible >= 1.0))] if visible.size else [0.0, 0.0],
    }


def summarize(values: list[dict[str, float]]) -> dict[str, object]:
    keys = values[0].keys()
    result: dict[str, object] = {}
    for key in keys:
        if key == "visible low/high clip channel ratio":
            result[key] = [float(np.mean([v[key][i] for v in values])) for i in range(2)]
            continue
        array = np.asarray([v[key] for v in values], dtype=np.float64)
        result[key] = {"mean": float(np.mean(array)), "p05": float(np.percentile(array, 5)),
                       "p95": float(np.percentile(array, 95)), "std across seeds": float(np.std(array, ddof=1)) if len(array) > 1 else 0.0}
    return result


def make_sheet(tiles: list[tuple[str, np.ndarray]]) -> Path:
    tile_w, tile_h, label_h = 192, 144, 20
    columns = 5
    rows = (len(tiles) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * tile_w, rows * (tile_h + label_h)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for i, (label, image) in enumerate(tiles):
        col, row = i % columns, i // columns
        x, y = col * tile_w, row * (tile_h + label_h)
        draw.text((x + 3, y + 3), label, fill=(245, 245, 245), font=font)
        sheet.paste(Image.fromarray(image, "RGB").resize((tile_w, tile_h), Image.Resampling.NEAREST), (x, y + label_h))
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "grain-bayer-masking-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    sources = make_synthetic()
    for name in ("clover_full", "silver_feather"):
        sources[name] = load_natural(name)
    outputs: list[dict[str, object]] = []
    tiles: list[tuple[str, np.ndarray]] = []
    noise_fields = {seed: np.random.default_rng(seed).normal(0, 1, (SIZE, SIZE, 1)).astype(np.float32) for seed in SEEDS}
    for source_name, (rgb, visible_mask, provenance) in sources.items():
        for config_name, levels, dither_strength in CONFIGS:
            for phase in PHASES:
                matrix = phase_matrix(*phase)
                dithered = ordered_quantize(rgb, levels, matrix, dither_strength)
                no_dither = ordered_quantize(rgb, levels, BAYER_4, 0.0)
                template = dithered - no_dither
                template_rms = float(np.sqrt(np.mean(template[np.broadcast_to(visible_mask[:, :, None], template.shape)] ** 2)))
                for sigma in GRAIN_SIGMAS:
                    seed_metrics = []
                    for seed in SEEDS:
                        noise = noise_fields[seed] * sigma
                        output = np.clip(dithered + noise, 0.0, 1.0)
                        seed_metrics.append(fit_coherent_pattern(output, no_dither, template, visible_mask))
                    summarized = summarize(seed_metrics)
                    outputs.append({"source": source_name,
                                    "palette condition": config_name, "levels": levels,
                                    "dither strength": dither_strength, "Bayer phase": list(phase),
                                    "grain sigma": sigma, "seed count": len(SEEDS),
                                    "template RMS visible": template_rms, "summary": summarized})
                    if source_name == "synthetic-flat-midgray" and phase == (0, 0) and config_name == "12-level print-like":
                        display = np.uint8(np.clip(output * 255 + 0.5, 0, 255))
                        tiles.append((f"flat / sigma {sigma:.4f} / seed {seed}", display))
    sheet = make_sheet(tiles)
    report = {
        "question": "At matched palette settings, how does post-quantization grain change the coherent Bayer-periodic component relative to residual texture?",
        "evaluation plan": {
            "objective": "Measure the projection of grain-enabled suite output onto the exact Bayer-dither contribution, and how that coherent share changes with grain sigma, phase, palette levels, source, and noise seed.",
            "baseline": "No-dither quantized output; Bayer template is dithered minus no-dither output at the same levels. Grain is added after quantization as in retro_lab.apply_retro.",
            "sources": "Two deterministic synthetic controls (flat mid-gray and smooth unclipped color ramp) plus SHA/provenance-verified CC0 clover and silver feather center crops, 256x256.",
            "conditions": "8/.5, 12/.7, 24/.35 level/dither configurations; Bayer shifts (0,0),(1,0),(0,1),(1,1); sigma 0/.0075/.015/.025/.035; 16 independent fixed noise seeds per condition. RGB code values; natural cutouts use alpha>250/255 mask.",
            "metrics": "Bayer coherent projection beta and its share of total residual energy relative to no-dither quantization; residual RMS; visible clip ratios; seed mean/p05/p95/std. This is a signal decomposition proxy, not perceptual visibility or quality.",
            "visual review": "Generated flat-field contact sheet at seed 16 for all sigma values; reviewed at thumbnail scale. It demonstrates phase pattern vs additive grain but does not establish masking threshold.",
            "acceptance": "No approved target or perceptual threshold; no pass/fail conclusion.",
            "unverified": "Perceptual masking, moving footage, alpha edge composite, ICC/HDR, Core/GPU parity, runtime/performance and human study.",
        },
        "seed list": list(SEEDS),
        "sources": {name: provenance for name, (_, _, provenance) in sources.items()},
        "results": outputs,
        "contact sheet": str(sheet.relative_to(ROOT)),
        "limits": "Projection is an exact template fit, but coherent energy share depends on chosen no-dither baseline and spatial mask. Bayer phase and noise are deterministic. Results do not prove a viewer can or cannot see the pattern.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "grain-bayer-masking-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Measured Bayer-coherent residual share across four source types and grain strengths.")
