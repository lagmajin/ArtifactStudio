"""Render paired grain-on/off photos and inspect signed residuals by ROI."""

from __future__ import annotations

import json
import platform
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_grain_seed_variance_study import (  # noqa: E402
    ASSET_NAMES, CAPTURE as _SEED_CAPTURE, CROP_RECTS, INPUT_DIR, MANIFEST,
    PRESETS, REFERENCE_SIZE, SELECTED_SEEDS, SIZES, load_source, make_regions,
    resize_long_edge,
)
sys.path.insert(0, str(ROOT))
from full_suite_photo_tone_study import region_error  # noqa: E402
from retro_lab import apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-paired-residual"
DIFF_GAIN = 8.0  # fixed visual gain for the signed linear-luma residual image


def common_view(image: Image.Image, dimensions: tuple[int, int]) -> np.ndarray:
    rgb = image.convert("RGB")
    if rgb.size != dimensions:
        rgb = rgb.resize(dimensions, Image.Resampling.LANCZOS)
    return np.asarray(rgb, dtype=np.uint8)


def residual_heatmap(grain_on: np.ndarray, grain_off: np.ndarray) -> np.ndarray:
    on = srgb_to_linear(grain_on.astype(np.float32) / 255.0)
    off = srgb_to_linear(grain_off.astype(np.float32) / 255.0)
    signed_luma = (on - off) @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    positive = np.clip(signed_luma * DIFF_GAIN, 0.0, 1.0)
    negative = np.clip(-signed_luma * DIFF_GAIN, 0.0, 1.0)
    magnitude = np.maximum(positive, negative)
    heat = np.stack((positive, magnitude * 0.12, negative), axis=2)
    return np.uint8(np.clip(heat * 255.0 + 0.5, 0.0, 255.0))


def paired_region_metrics(grain_on: np.ndarray, grain_off: np.ndarray,
                          mask: np.ndarray) -> dict[str, object]:
    pixel_count = int(np.count_nonzero(mask))
    if not pixel_count:
        return {"pixel count": 0, "metrics": None}
    on_linear = srgb_to_linear(grain_on.astype(np.float32) / 255.0)
    off_linear = srgb_to_linear(grain_off.astype(np.float32) / 255.0)
    delta = on_linear[mask] - off_linear[mask]
    abs_delta = np.abs(delta)
    luma = delta @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    code_delta = grain_on.astype(np.int16) - grain_off.astype(np.int16)
    changed = np.any(code_delta != 0, axis=2)
    endpoint0_changed = (grain_on == 0) != (grain_off == 0)
    endpoint255_changed = (grain_on == 255) != (grain_off == 255)
    return {"pixel count": pixel_count, "metrics": {
        "linear RGB MAE": float(np.mean(abs_delta)),
        "linear RGB RMSE": float(np.sqrt(np.mean(delta * delta))),
        "linear RGB P95 absolute error": float(np.percentile(abs_delta, 95)),
        "linear Rec709 luma signed mean": float(np.mean(luma)),
        "linear Rec709 luma RMS": float(np.sqrt(np.mean(luma * luma))),
        "linear Rec709 luma P95 absolute": float(np.percentile(np.abs(luma), 95)),
        "positive luma residual pixel ratio": float(np.mean(luma > 0.0)),
        "negative luma residual pixel ratio": float(np.mean(luma < 0.0)),
        "RGB8 changed pixel ratio": float(np.mean(changed[mask])),
        "RGB8 endpoint-zero state transition component ratio": float(np.mean(endpoint0_changed[mask])),
        "RGB8 endpoint-255 state transition component ratio": float(np.mean(endpoint255_changed[mask])),
    }}


def fit_image(image: Image.Image, box: tuple[int, int]) -> Image.Image:
    image = image.convert("RGB")
    image.thumbnail(box, Image.Resampling.LANCZOS)
    return image


def paste(sheet: Image.Image, draw: ImageDraw.ImageDraw, font: ImageFont.ImageFont,
          image: Image.Image, row: int, col: int, label: str,
          crop: bool, source_name: str, cell: tuple[int, int], label_h: int) -> None:
    cell_w, cell_h = cell
    x, y = col * cell_w, row * (cell_h + label_h)
    draw.text((x + 3, y + 3), label[:28], fill=(245, 245, 245), font=font)
    if crop:
        u0, v0, u1, v1 = CROP_RECTS[source_name]
        image = image.crop((round(u0 * image.width), round(v0 * image.height),
                            round(u1 * image.width), round(v1 * image.height)))
    fitted = fit_image(image, cell)
    sheet.paste(fitted, (x + (cell_w - fitted.width) // 2,
                         y + label_h + (cell_h - fitted.height) // 2))


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    sources: dict[str, object] = {}
    records: list[dict[str, object]] = []
    rows = len(ASSET_NAMES) * len(PRESETS)
    paired_columns = len(SIZES) * len(SELECTED_SEEDS) * 2
    residual_columns = len(SIZES) * len(SELECTED_SEEDS)
    paired_sheets = {False: Image.new("RGB", (paired_columns * 112, rows * 100), (27, 28, 31)),
                     True: Image.new("RGB", (paired_columns * 112, rows * 100), (27, 28, 31))}
    residual_sheets = {False: Image.new("RGB", (residual_columns * 140, rows * 126), (27, 28, 31)),
                       True: Image.new("RGB", (residual_columns * 140, rows * 126), (27, 28, 31))}
    paired_draw = {crop: (ImageDraw.Draw(sheet), ImageFont.load_default()) for crop, sheet in paired_sheets.items()}
    residual_draw = {crop: (ImageDraw.Draw(sheet), ImageFont.load_default()) for crop, sheet in residual_sheets.items()}

    row = 0
    for source_name in ASSET_NAMES:
        original, source_meta = load_source(source_name, manifest)
        sources[source_name] = source_meta
        inputs = {str(size): resize_long_edge(original, size) for size in SIZES}
        reference_dimensions = inputs[str(REFERENCE_SIZE)].size
        input_reference = np.asarray(inputs[str(REFERENCE_SIZE)].convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(input_reference)
        for preset_name, preset in PRESETS.items():
            for size in SIZES:
                key = str(size)
                image = inputs[key].convert("RGBA")
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                grain_off_native = apply_retro(image, **off_settings, seed=SELECTED_SEEDS[0])
                grain_off = common_view(grain_off_native, reference_dimensions)
                for seed_index, seed in enumerate(SELECTED_SEEDS):
                    grain_on_native = apply_retro(image, **preset, seed=seed)
                    source_alpha = np.asarray(image.getchannel("A"))
                    if not np.array_equal(source_alpha, np.asarray(grain_on_native.getchannel("A"))):
                        raise RuntimeError(f"alpha mismatch {source_name}/{preset_name}/{size}/{seed}")
                    grain_on = common_view(grain_on_native, reference_dimensions)
                    heat = residual_heatmap(grain_on, grain_off)
                    paired_column = SIZES.index(size) * len(SELECTED_SEEDS) * 2 + seed_index * 2
                    residual_column = SIZES.index(size) * len(SELECTED_SEEDS) + seed_index
                    for crop in (False, True):
                        draw, font = paired_draw[crop]
                        paste(paired_sheets[crop], draw, font, Image.fromarray(grain_off), row,
                              paired_column, f"{preset_name} off {size} s{seed}", crop, source_name,
                              (112, 78), 22)
                        paste(paired_sheets[crop], draw, font, Image.fromarray(grain_on), row,
                              paired_column + 1, f"{preset_name} on {size} s{seed}", crop, source_name,
                              (112, 78), 22)
                        draw, font = residual_draw[crop]
                        paste(residual_sheets[crop], draw, font, Image.fromarray(heat), row,
                              residual_column, f"{preset_name} x{DIFF_GAIN:g} {size} s{seed}",
                              crop, source_name, (140, 104), 22)

                    on_float = grain_on.astype(np.float32) / 255.0
                    off_float = grain_off.astype(np.float32) / 255.0
                    region_metrics = {region: paired_region_metrics(grain_on, grain_off, mask)
                                      for region, mask in regions.items()}
                    records.append({"source": source_name, "preset": preset_name,
                                    "size": size, "seed": seed,
                                    "native dimensions": list(image.size),
                                    "normalized display dimensions": list(reference_dimensions),
                                    "grain amount": float(preset["grain"]),
                                    "region metrics": region_metrics})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    paired_sheets[False].save(CAPTURE / "grain-on-off-paired-overview.png")
    paired_sheets[True].save(CAPTURE / "grain-on-off-paired-detail-crops.png")
    residual_sheets[False].save(CAPTURE / "grain-on-off-signed-residual-x8.png")
    residual_sheets[True].save(CAPTURE / "grain-on-off-signed-residual-detail-x8.png")

    report = {
        "question": "What signed output changes are caused by enabling current-suite grain, per seed and source tone/detail ROI?",
        "evaluation plan": {
            "baseline": "The same-size current apply_retro preset with grain=0; other stages remain active in the same order. Not an approved appearance target.",
            "sources": "Same five SHA-256 verified photos, EXIF orientation, ICC-to-sRGB handling, and provenance as experiments 102–108.",
            "conditions": f"Four PRESETS; input long edges {list(SIZES)}; selected seeds {list(SELECTED_SEEDS)}; every output is Lanczos-normalized to exact 512px-input dimensions.",
            "pairing": "For each source/preset/size/seed, compare full suite output to the same-size, same-preset grain-disabled output. This retains nonlinear grain/clip interaction and does not rerun quantization after grain.",
            "regions": "The seven source-luma/gradient ROIs from the 512px input: whole, shadow, deep shadow, midtone, highlight, smooth and detail. Empty masks remain null.",
            "metrics": "Linear RGB MAE/RMSE/P95, signed Rec709-luma mean/RMS/P95, positive/negative residual ratio, RGB8 changed-pixel ratio and code-endpoint state transitions.",
            "visuals": f"Pair images plus signed-luma residual heatmaps at fixed gain ×{DIFF_GAIN:g} (red=positive, blue=negative), across all 5 photos/4 presets/2 sizes/4 seeds; normalized subject crops included. Heatmap is a display aid, not measurement source.",
            "alpha and temporal": "Opaque photo alpha passthrough is checked. Seed-paired stills are not a temporal sequence.",
            "performance": "Not measured. No Artifact/Core/GPU or product decode path.",
            "acceptance": "Descriptive paired residual; no approved target, threshold, quality rank or pass/fail.",
            "unverified": "Natural viewer preference, calibrated display, animated temporal stability, HDR/profile diversity, product preview/export, performance/memory.",
        },
        "sources and provenance": sources,
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "pillow": Image.__version__, "platform": platform.platform()},
        "selected seeds": list(SELECTED_SEEDS),
        "records": records,
        "visuals": {"paired overview": str((CAPTURE / "grain-on-off-paired-overview.png").relative_to(ROOT)),
                    "paired detail crops": str((CAPTURE / "grain-on-off-paired-detail-crops.png").relative_to(ROOT)),
                    "signed residual x8": str((CAPTURE / "grain-on-off-signed-residual-x8.png").relative_to(ROOT)),
                    "signed residual detail x8": str((CAPTURE / "grain-on-off-signed-residual-detail-x8.png").relative_to(ROOT))},
        "limits": "All paired images are RGB8 after current apply_retro and common Lanczos display resize. Signed heatmaps use fixed ×8 gain and can saturate. Grain-off output keeps pixelation/quantization/channel-offset/scanlines; the measured delta includes final clip and RGB8 conversion interaction.",
    }
    (CAPTURE / "photo-grain-paired-residual-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Rendered {len(result['records'])} paired grain residual cases with four image sheets.")
