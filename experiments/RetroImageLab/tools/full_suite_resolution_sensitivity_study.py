"""Measure preset behavior when the same scene is rendered at different resolutions."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from adaptive_palette_temporal_stability_study import ASSETS, load_asset  # noqa: E402
from adaptive_palette_alpha_resize_study import composite  # noqa: E402
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-resolution"
SIZES = (128, 256, 512, 1024)
REFERENCE_SIZE = 256
NATURAL_ASSETS = ("clover_full", "silver_feather", "pixel_hair_atlas")
BACKGROUNDS = {"black": (0.0, 0.0, 0.0), "mid-gray": (0.5, 0.5, 0.5), "white": (1.0, 1.0, 1.0)}
SEED = 60419


def make_synthetic_scene(size: int = 512) -> tuple[np.ndarray, dict[str, object]]:
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    rgb = np.stack((0.2 + 0.58 * xx / (size - 1),
                    0.27 + 0.47 * yy / (size - 1),
                    0.22 + 0.35 * (xx + yy) / (2 * (size - 1))), axis=2)
    rgb += (0.12 * np.exp(-((xx - size * .72) ** 2 + (yy - size * .32) ** 2) / (2 * (size * .10) ** 2)))[:, :, None]
    image = Image.fromarray(np.uint8(np.clip(rgb * 255 + 0.5, 0, 255)), "RGB")
    draw = ImageDraw.Draw(image)
    draw.rectangle((22, 340, 174, 490), fill=(34, 61, 116), outline=(240, 216, 104), width=3)
    draw.ellipse((300, 60, 465, 225), fill=(171, 68, 58), outline=(255, 241, 216), width=4)
    for i in range(14):
        x = 190 + i * 19
        draw.line((x, 260, x + 72, 240), fill=(20 + i * 10, 230 - i * 8, 130 + i * 5), width=1 + (i % 3 == 0))
    for y in (18, 23, 29, 38):
        draw.line((205, y, 490, y), fill=(25, 34, 41), width=1)
    rgb_out = np.asarray(image, dtype=np.uint8)
    digest = hashlib.sha256(rgb_out.tobytes()).hexdigest()
    return rgb_out, {"kind": "generated synthetic control", "dimensions": [size, size],
                     "sha256 decoded RGB bytes": digest,
                     "description": "smooth RGB ramps, soft highlight, colored shape edges, thin diagonal and horizontal lines"}


def load_natural(name: str) -> tuple[np.ndarray, dict[str, object]]:
    spec = next(spec for spec in ASSETS if spec[3] == name)
    rgba, provenance, digest = load_asset(spec)
    h, w = rgba.shape[:2]
    side = min(h, w)
    x0, y0 = (w - side) // 2, (h - side) // 2
    crop = rgba[y0:y0 + side, x0:x0 + side]
    crop_image = Image.fromarray(crop, "RGBA")
    rgb = np.asarray(crop_image.convert("RGB"), dtype=np.uint8)
    alpha = np.asarray(crop_image.getchannel("A"), dtype=np.uint8)
    mid_gray = composite(rgb, alpha, BACKGROUNDS["mid-gray"])
    return np.uint8(np.clip(mid_gray * 255 + 0.5, 0, 255)), {
        "source": spec[0], "sha256": digest, "provenance": provenance,
        "original dimensions": [w, h], "center crop": [x0, y0, side, side],
        "compositing": "precomposited in linear light onto mid-gray before resolution sweep; alpha not part of sweep",
        "compositing": "linear-light onto mid-gray; other backgrounds not included in this resolution comparison",
    }


def resize_rgb(rgb: np.ndarray, size: int) -> np.ndarray:
    return np.asarray(Image.fromarray(rgb, "RGB").resize((size, size), Image.Resampling.LANCZOS), dtype=np.uint8)


def render(rgb8: np.ndarray, preset: dict[str, float | int], seed: int, disabled: str | None = None) -> np.ndarray:
    settings = dict(preset)
    if disabled is not None:
        settings[disabled] = 0.0
    image = Image.fromarray(rgb8, "RGB").convert("RGBA")
    return np.asarray(apply_retro(image, **settings, seed=seed).convert("RGB"), dtype=np.uint8)


def normalize_view(rgb8: np.ndarray) -> np.ndarray:
    return resize_rgb(rgb8, REFERENCE_SIZE).astype(np.float32) / 255.0


def spectrum_metrics(rgb: np.ndarray) -> dict[str, float]:
    luma = srgb_to_linear(rgb) @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    centered = luma - np.mean(luma)
    window = np.outer(np.hanning(luma.shape[0]), np.hanning(luma.shape[1])).astype(np.float32)
    power = np.abs(np.fft.fftshift(np.fft.fft2(centered * window))) ** 2
    freq = np.fft.fftshift(np.fft.fftfreq(luma.shape[0]))
    yy, xx = np.meshgrid(freq, freq, indexing="ij")
    radius = np.sqrt(xx * xx + yy * yy)
    valid = radius > 0
    total = float(np.sum(power[valid]))
    high = float(np.sum(power[valid & (radius >= 0.20)]) / total) if total else 0.0
    order = np.argsort(radius[valid])
    radii = radius[valid][order]
    cumulative = np.cumsum(power[valid][order])
    median = float(radii[np.searchsorted(cumulative, cumulative[-1] * 0.5)]) if cumulative.size and cumulative[-1] else 0.0
    return {"luma variance": float(np.var(luma)), "power fraction >=.20 cycles/display-pixel": high,
            "spectral median cycles/display-pixel": median,
            "horizontal mean adjacent correlation": float(np.corrcoef(luma[:, :-1].ravel(), luma[:, 1:].ravel())[0, 1])}


def compare_view(candidate: np.ndarray, reference: np.ndarray, mask: np.ndarray) -> dict[str, float]:
    delta = srgb_to_linear(candidate) - srgb_to_linear(reference)
    values = delta[mask]
    per_pixel = np.max(np.abs(values), axis=1) if values.size else np.zeros(0, dtype=np.float32)
    return {
        "linear RGB MAE": float(np.mean(np.abs(values))) if values.size else 0.0,
        "linear RGB RMSE": float(np.sqrt(np.mean(values * values))) if values.size else 0.0,
        "linear RGB P95 absolute error": float(np.percentile(np.abs(values), 95)) if values.size else 0.0,
        "pixel ratio max channel >1/255": float(np.mean(per_pixel > (1 / 255))) if per_pixel.size else 0.0,
    }


def make_sheet(records: list[dict[str, object]]) -> Path:
    tile, label, cols = 160, 18, 4
    rows = len(records)
    sheet = Image.new("RGB", (cols * tile, rows * (tile + label)), (30, 31, 34))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, record in enumerate(records):
        y = row * (tile + label)
        draw.text((3, y + 3), f"{record['source']} / {record['preset']}", fill=(245, 245, 245), font=font)
        for col, size in enumerate(SIZES):
            image = Image.fromarray(record["views"][str(size)], "RGB").resize((tile, tile), Image.Resampling.NEAREST)
            sheet.paste(image, (col * tile, y + label))
            draw.text((col * tile + 3, y + label + 2), str(size), fill=(255, 255, 255), font=font)
    CAPTURE.mkdir(parents=True, exist_ok=True)
    path = CAPTURE / "full-suite-resolution-sheet.png"
    sheet.save(path)
    return path


def main() -> dict[str, object]:
    sources: dict[str, tuple[np.ndarray, dict[str, object]]] = {"synthetic-multiscale-chart": make_synthetic_scene()}
    for name in NATURAL_ASSETS:
        sources[name] = load_natural(name)
    records: list[dict[str, object]] = []
    sheet_records: list[dict[str, object]] = []
    source_info = {name: provenance for name, (_, provenance) in sources.items()}
    for source_name, (source_rgb, provenance) in sources.items():
        for preset_name, preset in PRESETS.items():
            views: dict[str, np.ndarray] = {}
            size_results = []
            native_stage_metrics = []
            for size in SIZES:
                input_rgb = resize_rgb(source_rgb, size)
                full = render(input_rgb, preset, SEED)
                no_grain = render(input_rgb, preset, SEED, "grain") if preset["grain"] else full
                no_aberration = render(input_rgb, preset, SEED, "aberration") if preset["aberration"] else full
                no_scanlines = render(input_rgb, preset, SEED, "scanlines") if preset["scanlines"] else full
                normalized = normalize_view(full)
                views[str(size)] = np.uint8(np.clip(normalized * 255 + 0.5, 0, 255))
                normalized_ablation = {
                    "grain": normalize_view(no_grain),
                    "aberration": normalize_view(no_aberration),
                    "scanlines": normalize_view(no_scanlines),
                }
                native = full.astype(np.float32) / 255.0
                size_results.append({
                    "output size": [size, size],
                    "normalized comparison versus 256 output": None,
                    "full suite normalized spectrum": spectrum_metrics(normalized),
                    "native full suite spectrum": spectrum_metrics(native),
                    "native grain ablation RGB8 RMS": float(np.sqrt(np.mean((full.astype(np.float32) - no_grain.astype(np.float32)) ** 2)) / 255.0),
                    "native effect ablation RGB8 MAE": {
                        "grain": float(np.mean(np.abs(full.astype(np.float32) - no_grain.astype(np.float32))) / 255.0),
                        "aberration": float(np.mean(np.abs(full.astype(np.float32) - no_aberration.astype(np.float32))) / 255.0),
                        "scanlines": float(np.mean(np.abs(full.astype(np.float32) - no_scanlines.astype(np.float32))) / 255.0),
                    },
                    "normalized effect ablation linear RGB MAE": {
                        name: float(np.mean(np.abs(srgb_to_linear(normalized) - srgb_to_linear(value))))
                        for name, value in normalized_ablation.items()
                    },
                    "grain normalized-view RMS": float(np.sqrt(np.mean((normalize_view(full) - normalize_view(no_grain)) ** 2))),
                })
            reference = views[str(REFERENCE_SIZE)].astype(np.float32) / 255.0
            # Diagnostic regions are fixed normalized-frame coordinates, deliberately not content masks.
            center_mask = np.zeros((REFERENCE_SIZE, REFERENCE_SIZE), dtype=bool)
            center_mask[64:192, 64:192] = True
            whole_mask = np.ones((REFERENCE_SIZE, REFERENCE_SIZE), dtype=bool)
            for item in size_results:
                view = views[str(item["output size"][0])].astype(np.float32) / 255.0
                item["normalized comparison versus 256 output"] = {
                    "whole": compare_view(view, reference, whole_mask),
                    "center ROI": compare_view(view, reference, center_mask),
                }
            records.append({"source": source_name, "source identity": provenance,
                            "preset": preset_name, "preset settings": preset,
                            "seed": SEED, "reference output size": [REFERENCE_SIZE, REFERENCE_SIZE],
                            "sizes": size_results})
            sheet_records.append({"source": source_name, "preset": preset_name, "views": views})
    sheet = make_sheet(sheet_records)
    report = {
        "question": "How stable is the same full retro preset look when output pixel dimensions change but the normalized display frame is held fixed?",
        "evaluation plan": {
            "objective": "Describe full-suite output drift across 128,256,512,1024 square outputs at a fixed normalized display size, and attribute native-resolution changes to grain, aberration, and scanline ablations.",
            "baseline": "The 256x256 output per source/preset is a comparison reference, not a desired look or quality oracle.",
            "sources": "One deterministic synthetic multiscale chart plus three SHA/provenance verified CC0 sources (clover cutout, silver feather cutout, pixel-hair cutout), center square-cropped. Natural alpha sources are linear-light composited onto mid-gray before resize; this study tests opaque suite processing.",
            "conditions": "Pillow Lanczos input resampling in sRGB code values; exact current PRESETS/apply_retro recipes, seed 60419, native sizes 128/256/512/1024. Every output is Lanczos normalized to 256 for same-frame comparison. Grain-off, aberration-off, scanline-off ablations are additional diagnostics.",
            "metrics": "Normalized-view linear RGB MAE/RMSE/P95/change ratio against 256, native and normalized luma spectrum, grain/aberration/scanline ablation RGB8 MAE, grain RMS. No cross-resolution content registration beyond common square crop.",
            "visual review": "4 sources × 4 presets, columns 128/256/512/1024 normalized to 256 and enlarged nearest; researcher reviewed the sheet and source-specific rows. No independent reviewer or approved reference.",
            "acceptance": "No resolution-invariance criterion or target output size defined; descriptive response only.",
            "unverified": "Actual application preview/export sampling, GPU/Core parity, display scaling, user-selected dimensions, ICC/HDR path, performance/memory and perceptual acceptance.",
        },
        "sources": source_info,
        "results": records,
        "contact sheet": str(sheet.relative_to(ROOT)),
        "limits": "Input resampling itself changes sampling/aliasing and is part of the resolution workflow. Metrics compare normalized displayed rasters, not intrinsic source quality. Ablations are nonlinear counterfactuals, not additive attribution. No Core/GPU execution.",
    }
    CAPTURE.mkdir(parents=True, exist_ok=True)
    (CAPTURE / "full-suite-resolution-metrics.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    main()
    print("Compared full-suite preset output at four resolutions across four sources.")
