"""Measure how suite-stage ablations change photo resolution response."""

from __future__ import annotations

import hashlib
import io
import json
import statistics
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_tone_study import ASSET_NAMES, INPUT_DIR, MANIFEST, make_regions, region_error  # noqa: E402
sys.path.insert(0, str(ROOT))
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-stage-isolation"
SIZES = (512, 1024, 2048)
REFERENCE_SIZE = 512
SEED = 31111
STAGES = {
    "pixelation": {"scale": 1.0},
    "quantization_dither": {"colors": 256, "dither": 0.0},
    "channel_offset": {"aberration": 0.0},
    "scanlines": {"scanlines": 0.0},
    "grain": {"grain": 0.0},
}
VARIANTS = ("full_suite", *STAGES)
CROP_RECTS = {
    "face_portrait_unsplash_cc0.jpg": (.30, .15, .70, .65),
    "black_white_high_iso_portrait_cc0.jpg": (.30, .35, .70, .75),
    "milky_way_grundlsee_cc0.jpg": (.28, .18, .68, .82),
    "clouds_sun_behind_cc0.jpg": (.25, .20, .65, .60),
    "blue_sky_clouds_cc0.jpg": (.30, .25, .70, .65),
}


def load_source(name: str, manifest: dict[str, object]) -> tuple[Image.Image, dict[str, object]]:
    path = INPUT_DIR / name
    provenance = manifest[name]
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != provenance["download_sha256"]:
        raise ValueError(f"source SHA-256 mismatch: {name}")
    with Image.open(path) as opened:
        image = ImageOps.exif_transpose(opened)
        embedded = image.info.get("icc_profile")
        profile_name = "none"
        if embedded:
            src = ImageCms.ImageCmsProfile(io.BytesIO(embedded))
            profile_name = ImageCms.getProfileName(src).strip()
            image = ImageCms.profileToProfile(image, src, ImageCms.createProfile("sRGB"), outputMode="RGB")
        else:
            image = image.convert("RGB")
        original_dimensions = list(image.size)
        image.load()
        image = image.copy()
    return image, {"file": name, "SHA-256": digest, "source dimensions after EXIF orientation": original_dimensions,
                   "embedded ICC profile": profile_name, "working color condition": "ICC to sRGB; untagged assumed sRGB",
                   "provenance": provenance}


def resize_long_edge(image: Image.Image, long_edge: int) -> Image.Image:
    scale = long_edge / max(image.size)
    size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    return image.resize(size, Image.Resampling.LANCZOS)


def normalize_display(image: Image.Image, dimensions: tuple[int, int]) -> Image.Image:
    rgb = image.convert("RGB")
    return rgb if rgb.size == dimensions else rgb.resize(dimensions, Image.Resampling.LANCZOS)


def apply_variant(image: Image.Image, preset: dict[str, object], variant: str) -> Image.Image:
    settings = dict(preset)
    if variant != "full_suite":
        settings.update(STAGES[variant])
    return apply_retro(image, **settings, seed=SEED)


def delta_signature_error(full_ref: np.ndarray, full_candidate: np.ndarray,
                          ablated_ref: np.ndarray, ablated_candidate: np.ndarray,
                          mask: np.ndarray) -> dict[str, object]:
    full_drift = srgb_to_linear(full_candidate) - srgb_to_linear(full_ref)
    ablated_drift = srgb_to_linear(ablated_candidate) - srgb_to_linear(ablated_ref)
    delta = full_drift[mask] - ablated_drift[mask]
    if not delta.size:
        return {"pixel count": 0, "metrics": None}
    absolute = np.abs(delta)
    return {"pixel count": int(np.count_nonzero(mask)), "metrics": {
        "linear RGB MAE": float(np.mean(absolute)),
        "linear RGB RMSE": float(np.sqrt(np.mean(delta * delta))),
        "linear RGB P95 absolute error": float(np.percentile(absolute, 95)),
        "pixel ratio max channel >1/255": float(np.mean(np.max(absolute, axis=1) > (1 / 255))),
    }}


def paste_sheet_cell(sheet: Image.Image, draw: ImageDraw.ImageDraw, font: ImageFont.ImageFont,
                     image: Image.Image, row: int, size_index: int, label: str, crop: bool) -> None:
    tile_w, tile_h, label_h = 200, 170, 22
    x, y = size_index * tile_w, row * (tile_h + label_h)
    draw.text((x + 3, y + 4), label[:32], fill=(245, 245, 245), font=font)
    if crop:
        u0, v0, u1, v1 = CROP_RECTS[next(name for name in ASSET_NAMES if Path(name).stem in label)]
        image = image.crop((round(u0 * image.width), round(v0 * image.height),
                            round(u1 * image.width), round(v1 * image.height)))
    image = image.convert("RGB")
    image.thumbnail((tile_w, tile_h), Image.Resampling.LANCZOS)
    px, py = x + (tile_w - image.width) // 2, y + label_h + (tile_h - image.height) // 2
    sheet.paste(image, (px, py))


def error_rmse(metrics: dict[str, object], region: str = "whole image") -> float | None:
    region_result = metrics.get(region)
    if not region_result or region_result["metrics"] is None:
        return None
    return float(region_result["metrics"]["linear RGB RMSE"])


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    source_info: dict[str, object] = {}
    records: list[dict[str, object]] = []
    rows = len(ASSET_NAMES) * len(PRESETS)
    sheets: dict[tuple[str, bool], Image.Image] = {}
    sheet_draws: dict[tuple[str, bool], tuple[ImageDraw.ImageDraw, ImageFont.ImageFont]] = {}
    for variant in VARIANTS:
        for crop in (False, True):
            key = (variant, crop)
            sheets[key] = Image.new("RGB", (len(SIZES) * 200, rows * 192), (27, 28, 31))
            sheet_draws[key] = (ImageDraw.Draw(sheets[key]), ImageFont.load_default())

    row = 0
    for source_name in ASSET_NAMES:
        original, provenance = load_source(source_name, manifest)
        source_info[source_name] = provenance
        inputs = {str(size): resize_long_edge(original, size) for size in SIZES}
        display_dimensions = inputs[str(REFERENCE_SIZE)].size
        input_displays = {size: normalize_display(image, display_dimensions) for size, image in inputs.items()}
        input_reference = np.asarray(input_displays[str(REFERENCE_SIZE)], dtype=np.float32) / 255.0
        regions = make_regions(input_reference)
        for preset_name, preset in PRESETS.items():
            views: dict[str, dict[str, np.ndarray]] = {variant: {} for variant in VARIANTS}
            alpha_exact: dict[str, dict[str, bool]] = {variant: {} for variant in VARIANTS}
            for variant in VARIANTS:
                for size in SIZES:
                    key = str(size)
                    image = inputs[key].convert("RGBA")
                    result = apply_variant(image, preset, variant)
                    source_alpha = np.asarray(image.getchannel("A"))
                    result_alpha = np.asarray(result.getchannel("A"))
                    alpha_exact[variant][key] = bool(np.array_equal(source_alpha, result_alpha))
                    view = normalize_display(result, display_dimensions)
                    views[variant][key] = np.asarray(view, dtype=np.uint8)
                    for crop in (False, True):
                        draw, font = sheet_draws[(variant, crop)]
                        label = f"{Path(source_name).stem} / {preset_name} / {size}"
                        paste_sheet_cell(sheets[(variant, crop)], draw, font, view, row, SIZES.index(size), label, crop)

            full_ref = views["full_suite"][str(REFERENCE_SIZE)].astype(np.float32) / 255.0
            full_by_size: dict[str, object] = {}
            for size in SIZES:
                key = str(size)
                full_candidate = views["full_suite"][key].astype(np.float32) / 255.0
                full_by_size[key] = {
                    "input resampling delta from 512px": {
                        region: region_error(input_reference,
                                             np.asarray(input_displays[key], dtype=np.float32) / 255.0, mask)
                        for region, mask in regions.items()},
                    "full-suite output drift from its 512px result": {
                        region: region_error(full_ref, full_candidate, mask)
                        for region, mask in regions.items()},
                }

            ablation_metrics: dict[str, dict[str, object]] = {}
            for stage in STAGES:
                ablated_ref = views[stage][str(REFERENCE_SIZE)].astype(np.float32) / 255.0
                by_size: dict[str, object] = {}
                for size in SIZES:
                    key = str(size)
                    ablated = views[stage][key].astype(np.float32) / 255.0
                    full_candidate = views["full_suite"][key].astype(np.float32) / 255.0
                    by_size[key] = {
                        "suite ablation effect at this resolution": {
                            region: region_error(full_candidate, ablated, mask)
                            for region, mask in regions.items()},
                        "ablated output drift from its own 512px result": {
                            region: region_error(ablated_ref, ablated, mask)
                            for region, mask in regions.items()},
                        "change in resolution-drift signature versus full suite": {
                            region: delta_signature_error(full_ref, full_candidate, ablated_ref, ablated, mask)
                            for region, mask in regions.items()},
                    }
                ablation_metrics[stage] = by_size

            records.append({"source": source_name, "preset": preset_name, "preset settings": preset,
                            "seed": SEED, "input dimensions by long edge": {key: list(value.size) for key, value in inputs.items()},
                            "normalized display dimensions": list(display_dimensions),
                            "alpha exact match by variant and resolution": alpha_exact,
                            "full suite": full_by_size, "stage ablations": ablation_metrics,
                            "outputs": views})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheet_paths: dict[str, str] = {}
    for (variant, crop), sheet in sheets.items():
        suffix = "detail-crop" if crop else "overview"
        path = CAPTURE / f"{variant}-{suffix}.png"
        sheet.save(path)
        sheet_paths[f"{variant} {suffix}"] = str(path.relative_to(ROOT))
    summaries: dict[str, dict[str, object]] = {}
    for stage in STAGES:
        summaries[stage] = {}
        for size in SIZES:
            key = str(size)
            same_size = [error_rmse(record["stage ablations"][stage][key]["suite ablation effect at this resolution"])
                         for record in records]
            drift = [error_rmse(record["stage ablations"][stage][key]["ablated output drift from its own 512px result"])
                     for record in records]
            response = [error_rmse(record["stage ablations"][stage][key]["change in resolution-drift signature versus full suite"])
                        for record in records]
            summaries[stage][key] = {
                "same-resolution ablation RMSE median/max": [statistics.median(same_size), max(same_size)],
                "ablated resolution-drift RMSE median/max": [statistics.median(drift), max(drift)],
                "resolution-drift signature change RMSE median/max": [statistics.median(response), max(response)],
            }

    report = {
        "question": "Across the same five photos and four presets, how does disabling one suite stage at a time change the normalized 512/1024/2048 resolution response?",
        "evaluation plan": {
            "baseline": "The full current Python suite at 512px is a within-variant comparison reference, not a target appearance.",
            "sources": "Same five SHA-verified real photos and rights notes as the photo tone/resolution studies. EXIF orientation is applied; embedded profiles convert to sRGB, untagged files are assumed sRGB.",
            "conditions": f"Current four PRESETS, fixed seed {SEED}, proportional Lanczos inputs with longest edge {list(SIZES)}. Every processed output is Lanczos-normalized to the exact 512-input dimensions before comparison.",
            "ablations": {"pixelation": "Set scale=1.0; skip BOX-downsample/NEAREST-upsample.",
                          "quantization_dither": "Set colors=256 and dither=0.0; disable the posterization/dither stage as a combined stage.",
                          "channel_offset": "Set aberration=0.0.", "scanlines": "Set scanlines=0.0.",
                          "grain": "Set grain=0.0."},
            "metrics": "Per-region linear RGB MAE/RMSE/P95/change ratio for (1) fixed-resolution full-vs-ablation, (2) each variant's resolution drift relative to its own 512 result, and (3) signed drift-signature difference between full and ablated variants. Input resampling-only drift is also retained.",
            "visual review": "Aspect-preserving common-display overview and fixed normalized subject crops for every source/preset/variant/resolution. No calibrated display, approved target, independent reviewer, or quality threshold.",
            "alpha and temporal": "Photo inputs are opaque; exact alpha passthrough is checked. Still-only deterministic processing has no temporal dimension.",
            "performance": "Not measured; this is an output-response counterfactual, not a timing or memory study.",
            "acceptance": "Descriptive nonlinear ablations; no quality ranking, pass/fail, or product acceptance threshold.",
            "unverified": "Stage interactions and clipping/order effects remain; exact per-stage causal attribution, native-resolution preference, product decode/display path, Core/GPU parity, performance/memory and human preference are not tested.",
        },
        "sources and provenance": source_info,
        "stage summaries": summaries,
        "records": [{k: v for k, v in record.items() if k != "outputs"} for record in records],
        "sheets": sheet_paths,
        "limits": "Each ablation changes a nonlinear full pipeline and may alter interactions with later stages; drift-signature difference is not an additive or uniquely causal contribution. Quantization and dither share one function and are disabled together. Input resampling and common-display filtering remain part of this comparison.",
    }
    (CAPTURE / "photo-stage-isolation-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Compared {len(result['records'])} photo/preset cases across {len(VARIANTS)} pipeline variants and {len(SIZES)} resolutions.")
