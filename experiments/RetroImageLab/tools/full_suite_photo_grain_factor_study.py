"""Factor grain spatial scale, cross-resolution field alignment, and amplitude."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_display_scale_hypothesis_study import (  # noqa: E402
    ASSET_NAMES, MANIFEST, PRESETS, REFERENCE_SIZE,
    SEED, SIZES, STAGES, apply_retro, display_image, display_scaled_apply,
    load_source, make_regions, region_error, resize_long_edge, srgb_to_linear,
)
from full_suite_photo_display_scale_hypothesis_study import CROP_RECTS  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-factor"
NON_GRAIN_STAGES = set(STAGES) - {"grain"}
VARIANTS = {
    "native_pixel_nominal": ("native_pixel", "aligned_seed", "nominal_sigma"),
    "native_pixel_amplitude_scaled": ("native_pixel", "aligned_seed", "resolution_scaled_sigma"),
    "display_cell_aligned_nominal": ("display_cell", "aligned_seed", "nominal_sigma"),
    "display_cell_aligned_amplitude_scaled": ("display_cell", "aligned_seed", "resolution_scaled_sigma"),
    "display_cell_independent_nominal": ("display_cell", "independent_seed", "nominal_sigma"),
    "display_cell_independent_amplitude_scaled": ("display_cell", "independent_seed", "resolution_scaled_sigma"),
}


def image_metrics(candidate: np.ndarray, reference: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    if not np.any(mask):
        return {"pixel count": 0, "metrics": None}
    delta = srgb_to_linear(candidate)[mask] - srgb_to_linear(reference)[mask]
    absolute = np.abs(delta)
    return {"pixel count": int(np.count_nonzero(mask)), "metrics": {
        "linear RGB MAE": float(np.mean(absolute)),
        "linear RGB RMSE": float(np.sqrt(np.mean(delta * delta))),
        "linear RGB P95 absolute error": float(np.percentile(absolute, 95)),
        "pixel ratio max channel >1/255": float(np.mean(np.max(absolute, axis=1) > 1 / 255)),
    }}


def grain_residual_metrics(grained: np.ndarray, grainless: np.ndarray) -> dict[str, float]:
    g = srgb_to_linear(grained)
    b = srgb_to_linear(grainless)
    residual = g - b
    luma = residual @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    centered = luma - np.mean(luma)
    window = np.outer(np.hanning(luma.shape[0]), np.hanning(luma.shape[1])).astype(np.float32)
    power = np.abs(np.fft.fftshift(np.fft.fft2(centered * window))) ** 2
    fy = np.fft.fftshift(np.fft.fftfreq(luma.shape[0]))
    fx = np.fft.fftshift(np.fft.fftfreq(luma.shape[1]))
    yy, xx = np.meshgrid(fy, fx, indexing="ij")
    radius = np.sqrt(xx * xx + yy * yy)
    valid = radius > 0
    total = float(np.sum(power[valid]))
    high = float(np.sum(power[valid & (radius >= .20)]) / total) if total else 0.0
    correlations = {}
    for lag in (1, 2, 4, 8):
        if lag < luma.shape[1]:
            correlations[str(lag)] = float(np.corrcoef(luma[:, :-lag].ravel(), luma[:, lag:].ravel())[0, 1])
    return {
        "linear Rec709 luma residual RMS": float(np.sqrt(np.mean(luma * luma))),
        "linear Rec709 luma residual variance": float(np.var(luma)),
        "radial power fraction >=.20 cycles/display-pixel": high,
        "horizontal residual correlation by lag": correlations,
        "output low-code channel fraction": float(np.mean(grained <= 0.0)),
        "output high-code channel fraction": float(np.mean(grained >= 1.0)),
        "grainless low-code channel fraction": float(np.mean(grainless <= 0.0)),
        "grainless high-code channel fraction": float(np.mean(grainless >= 1.0)),
    }


def paste_cell(sheet: Image.Image, draw: ImageDraw.ImageDraw, font: ImageFont.ImageFont,
               rgb: np.ndarray, row: int, column: int, label: str,
               crop: bool, source_name: str) -> None:
    cell_w, cell_h, label_h = 200, 170, 22
    x, y = column * cell_w, row * (cell_h + label_h)
    draw.text((x + 3, y + 4), label[:34], fill=(245, 245, 245), font=font)
    image = Image.fromarray(rgb, "RGB")
    if crop:
        u0, v0, u1, v1 = CROP_RECTS[source_name]
        image = image.crop((round(u0 * image.width), round(v0 * image.height),
                            round(u1 * image.width), round(v1 * image.height)))
    image.thumbnail((cell_w, cell_h), Image.Resampling.LANCZOS)
    sheet.paste(image, (x + (cell_w - image.width) // 2,
                        y + label_h + (cell_h - image.height) // 2))


def main() -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    source_info: dict[str, object] = {}
    records: list[dict[str, object]] = []
    row_count = len(ASSET_NAMES) * len(PRESETS)
    sheets: dict[tuple[str, bool], Image.Image] = {}
    draws: dict[tuple[str, bool], tuple[ImageDraw.ImageDraw, ImageFont.ImageFont]] = {}
    for variant in VARIANTS:
        for crop in (False, True):
            key = (variant, crop)
            sheets[key] = Image.new("RGB", (len(SIZES) * 200, row_count * 192), (27, 28, 31))
            draws[key] = (ImageDraw.Draw(sheets[key]), ImageFont.load_default())

    row = 0
    for source_name in ASSET_NAMES:
        original, source_meta = load_source(source_name, manifest)
        source_info[source_name] = source_meta
        inputs = {str(size): resize_long_edge(original, size) for size in SIZES}
        reference_dimensions = inputs[str(REFERENCE_SIZE)].size
        input_ref = display_image(inputs[str(REFERENCE_SIZE)], reference_dimensions).astype(np.float32) / 255.0
        regions = make_regions(input_ref)
        for preset_name, preset in PRESETS.items():
            outputs: dict[str, dict[str, np.ndarray]] = {variant: {} for variant in VARIANTS}
            alpha_checks: dict[str, dict[str, bool]] = {variant: {} for variant in VARIANTS}
            grainless_views: dict[str, np.ndarray] = {}
            amplitude_by_size: dict[str, dict[str, float]] = {variant: {} for variant in VARIANTS}

            for size in SIZES:
                key = str(size)
                image = inputs[key].convert("RGBA")
                fixed = apply_retro(image, **preset, seed=SEED)
                fixed_view = display_image(fixed, reference_dimensions)
                for variant, (grain_scale, alignment, amplitude) in VARIANTS.items():
                    modified = dict(preset)
                    resolution_ratio = max(image.width / reference_dimensions[0], image.height / reference_dimensions[1])
                    sigma_multiplier = resolution_ratio if amplitude == "resolution_scaled_sigma" else 1.0
                    modified["grain"] = float(preset["grain"]) * sigma_multiplier
                    amplitude_by_size[variant][key] = sigma_multiplier
                    if grain_scale == "native_pixel":
                        scaled_stages = NON_GRAIN_STAGES
                    else:
                        scaled_stages = set(STAGES)
                    if alignment == "aligned_seed" or size == REFERENCE_SIZE:
                        noise_seed = SEED
                    else:
                        noise_seed = SEED + size * 101 + 17
                    result = display_scaled_apply(image, modified, reference_dimensions, noise_seed, scaled_stages)
                    view = display_image(result, reference_dimensions)
                    outputs[variant][key] = view
                    alpha_checks[variant][key] = bool(np.array_equal(np.asarray(image.getchannel("A")),
                                                                      np.asarray(result.getchannel("A"))))
                    if size == REFERENCE_SIZE and not np.array_equal(fixed_view, view):
                        max_delta = int(np.max(np.abs(fixed_view.astype(np.int16) - view.astype(np.int16))))
                        raise RuntimeError(f"512px parity failed for {source_name}/{preset_name}/{variant}: {max_delta}")

                grainless = dict(preset)
                grainless["grain"] = 0.0
                grainless_view = display_scaled_apply(image, grainless, reference_dimensions, SEED,
                                                      set(STAGES))
                grainless_views[key] = display_image(grainless_view, reference_dimensions)

            fixed_ref = outputs["native_pixel_nominal"][str(REFERENCE_SIZE)].astype(np.float32) / 255.0
            per_variant: dict[str, dict[str, object]] = {}
            for variant in VARIANTS:
                variant_ref = outputs[variant][str(REFERENCE_SIZE)].astype(np.float32) / 255.0
                by_size: dict[str, object] = {}
                for size in SIZES:
                    key = str(size)
                    candidate = outputs[variant][key].astype(np.float32) / 255.0
                    fixed_current = outputs["native_pixel_nominal"][key].astype(np.float32) / 255.0
                    grainless = grainless_views[key].astype(np.float32) / 255.0
                    by_size[key] = {
                        "drift from variant 512px result": {
                            region: region_error(variant_ref, candidate, mask) for region, mask in regions.items()},
                        "drift from fixed-pixel 512px output": {
                            region: region_error(fixed_ref, candidate, mask) for region, mask in regions.items()},
                        "same-size difference versus nominal fixed-pixel grain": {
                            region: region_error(fixed_current, candidate, mask) for region, mask in regions.items()},
                        "grain residual over normalized non-grain suite": {
                            region: image_metrics(candidate, grainless, mask) for region, mask in regions.items()},
                        "grain residual spectrum and clipping": grain_residual_metrics(candidate, grainless),
                    }
                per_variant[variant] = by_size

            for variant in VARIANTS:
                for size in SIZES:
                    key = str(size)
                    label = f"{Path(source_name).stem} / {preset_name} / {size}"
                    draw, font = draws[(variant, False)]
                    paste_cell(sheets[(variant, False)], draw, font, outputs[variant][key], row,
                               SIZES.index(size), label, False, source_name)
                    draw, font = draws[(variant, True)]
                    paste_cell(sheets[(variant, True)], draw, font, outputs[variant][key], row,
                               SIZES.index(size), label, True, source_name)
            records.append({"source": source_name, "preset": preset_name, "preset settings": preset,
                            "input dimensions by long edge": {key: list(value.size) for key, value in inputs.items()},
                            "normalized display dimensions": list(reference_dimensions), "seed": SEED,
                            "sigma multiplier by variant and size": amplitude_by_size,
                            "alpha exact match by variant and resolution": alpha_checks,
                            "variants": per_variant, "outputs": outputs})
            row += 1

    CAPTURE.mkdir(parents=True, exist_ok=True)
    sheet_paths: dict[str, str] = {}
    for (variant, crop), sheet in sheets.items():
        suffix = "detail-crop" if crop else "overview"
        path = CAPTURE / f"{variant}-{suffix}.png"
        sheet.save(path)
        sheet_paths[f"{variant} {suffix}"] = str(path.relative_to(ROOT))

    summaries: dict[str, dict[str, object]] = {}
    for variant in VARIANTS:
        summaries[variant] = {}
        for size in SIZES:
            key = str(size)
            drift = [r["variants"][variant][key]["drift from variant 512px result"]["whole image"]["metrics"]["linear RGB RMSE"]
                     for r in records]
            residual = [r["variants"][variant][key]["grain residual spectrum and clipping"]["linear Rec709 luma residual RMS"]
                        for r in records]
            high_power = [r["variants"][variant][key]["grain residual spectrum and clipping"]
                          ["radial power fraction >=.20 cycles/display-pixel"] for r in records]
            summaries[variant][key] = {
                "512-reference drift RMSE median/max": [float(np.median(drift)), float(np.max(drift))],
                "grain residual luma RMS median/max": [float(np.median(residual)), float(np.max(residual))],
                "grain residual high-frequency power median": float(np.median(high_power)),
                "cases lower drift than nominal native-pixel grain": sum(
                    x < r["variants"]["native_pixel_nominal"][key]["drift from variant 512px result"]
                    ["whole image"]["metrics"]["linear RGB RMSE"] for x, r in zip(drift, records)),
            }

    report = {
        "question": "Within the display-scaled non-grain suite, how do grain cell size, cross-resolution seed-field alignment, and sigma scaling separately alter the fixed-display response?",
        "evaluation plan": {
            "baseline": "Current fixed-pixel suite output at 512px. It is a reproducibility pivot, not an approved target appearance.",
            "sources": "Same five SHA-verified photos, EXIF orientation and ICC-to-sRGB handling as experiments 102–105.",
            "conditions": f"Four PRESETS, seed {SEED}, long edge {list(SIZES)}, exact common dimensions based on the 512px input. Non-grain stages use the display-coordinate hypothesis from experiment 105.",
            "factor matrix": {name: {"grain cell size": dims, "cross-resolution field": alignment,
                                     "sigma amplitude": amplitude}
                             for name, (dims, alignment, amplitude) in VARIANTS.items()},
            "amplitude rule": "nominal sigma is unchanged; resolution_scaled_sigma multiplies sigma by max(input width/reference width, input height/reference height). This is a compensation hypothesis, not a perceptual or physical law.",
            "alignment rule": "aligned variants reuse one seeded 512-coordinate random field; independent variants use a deterministic size-specific seed for the normalized field. Native-pixel mode uses the current full-resolution iid Gaussian generator.",
            "512px parity gate": "Every variant must exactly equal the current full-suite RGB8 output at 512px for every source/preset, otherwise abort.",
            "metrics": "Regional linear RGB drift, grain residual relative to a normalized non-grain pipeline, residual luma RMS/variance, display-space radial high-frequency power, horizontal residual correlation by lag, and RGB endpoint occupancy.",
            "visual review": "Aspect-preserving overview and normalized subject crop for all variants, resolutions, sources and presets. No independent reviewer, calibrated display, approved target or threshold.",
            "alpha and temporal": "Opaque photos; output alpha passthrough is checked. Cross-resolution field alignment on stills does not test animated grain temporal stability.",
            "performance": "Not measured; each configuration is a Python equation-model counterfactual.",
            "acceptance": "Descriptive factor study; closeness to 512px is not quality. No pass/fail or look ranking.",
            "unverified": "Human preference, chosen view size, motion footage, time continuity, product/Core/GPU behavior, HDR/profile diversity, runtime and memory.",
        },
        "sources and provenance": source_info,
        "variant summaries": summaries,
        "records": [{k: v for k, v in r.items() if k != "outputs"} for r in records],
        "sheets": sheet_paths,
        "limits": "The matrix factors noise cell size, field alignment and sigma multiplier, but all runs share the experiment-105 display-scaled non-grain model. Clipping, quantization and grain interact; measured residual is not the unbounded Gaussian field itself. The 512px parity pivot is not ground truth.",
    }
    (CAPTURE / "photo-grain-factor-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Compared {len(result['records'])} photo/preset cases across {len(VARIANTS)} grain variants and {len(SIZES)} resolutions.")
