"""Probe display-scale parameter hypotheses against the fixed-pixel photo suite."""

from __future__ import annotations

import hashlib
import io
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageCms, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from full_suite_photo_tone_study import ASSET_NAMES, INPUT_DIR, MANIFEST, make_regions, region_error  # noqa: E402
sys.path.insert(0, str(ROOT))
from retro_lab import BAYER_4, PRESETS, apply_retro, channel_offset, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-display-scale"
SIZES = (512, 1024, 2048)
REFERENCE_SIZE = 512
SEED = 31111
STAGES = ("pixelation", "dither", "channel_offset", "scanlines", "grain")
VARIANTS = ("fixed_pixel", *STAGES, "all_display_scaled")
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
        dimensions = list(image.size)
        image.load()
        image = image.copy()
    return image, {"file": name, "SHA-256": digest, "source dimensions after EXIF orientation": dimensions,
                   "embedded ICC profile": profile_name, "working color condition": "ICC to sRGB; untagged assumed sRGB",
                   "provenance": provenance}


def resize_long_edge(image: Image.Image, long_edge: int) -> Image.Image:
    scale = long_edge / max(image.size)
    dimensions = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
    return image.resize(dimensions, Image.Resampling.LANCZOS)


def normalized_axis(length: int, reference_length: int) -> np.ndarray:
    return np.minimum(np.arange(length, dtype=np.int64) * reference_length // length,
                      reference_length - 1)


def display_scaled_apply(image: Image.Image, preset: dict[str, object], reference_dimensions: tuple[int, int],
                         seed: int, scaled_stages: set[str]) -> Image.Image:
    """Research-only mirror of apply_retro with selected spatial scales normalized."""
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    rgb_image = rgba.convert("RGB")
    width, height = rgb_image.size
    ref_width, ref_height = reference_dimensions
    scale_ratio = max(width / ref_width, height / ref_height)

    pixel_scale = float(preset["scale"])
    if "pixelation" in scaled_stages:
        pixel_scale = min(1.0, pixel_scale / scale_ratio)
    if pixel_scale < 1.0:
        small = (max(1, round(width * pixel_scale)), max(1, round(height * pixel_scale)))
        rgb_image = rgb_image.resize(small, Image.Resampling.BOX).resize((width, height), Image.Resampling.NEAREST)

    rgb = np.asarray(rgb_image, dtype=np.float32) / 255.0
    yy = normalized_axis(height, ref_height) if "dither" in scaled_stages else np.arange(height)
    xx = normalized_axis(width, ref_width) if "dither" in scaled_stages else np.arange(width)
    matrix = BAYER_4[yy[:, None] % BAYER_4.shape[0], xx[None, :] % BAYER_4.shape[1]]
    levels = max(2, int(preset["colors"]))
    dither = min(max(float(preset["dither"]), 0.0), 1.0)
    step = 1.0 / (levels - 1)
    rgb = np.clip(np.floor(np.clip(rgb + matrix[:, :, None] * step * dither, 0.0, 1.0) / step + 0.5) * step,
                  0.0, 1.0)

    shift = float(preset["aberration"])
    if "channel_offset" in scaled_stages:
        shift *= width / ref_width
    rgb = channel_offset(rgb, shift)

    if float(preset["scanlines"]) > 0.0:
        row_axis = normalized_axis(height, ref_height) if "scanlines" in scaled_stages else np.arange(height)
        dark = (row_axis % 2 == 1)[:, None, None]
        gain = np.where(dark, 1.0 - min(float(preset["scanlines"]), 0.9), 1.0)
        rgb *= gain

    grain_amount = float(preset["grain"])
    if grain_amount > 0.0:
        rng = np.random.default_rng(seed)
        if "grain" in scaled_stages:
            ref_noise = rng.normal(0.0, grain_amount, size=(ref_height, ref_width, 1)).astype(np.float32)
            yi = normalized_axis(height, ref_height)
            xi = normalized_axis(width, ref_width)
            noise = ref_noise[yi[:, None], xi[None, :], :]
        else:
            noise = rng.normal(0.0, grain_amount, size=(height, width, 1)).astype(np.float32)
        rgb += noise
    rgb = np.clip(rgb, 0.0, 1.0)
    output = Image.fromarray(np.uint8(rgb * 255.0), "RGB").convert("RGBA")
    output.putalpha(alpha)
    return output


def linear_delta_signature_error(full_ref: np.ndarray, full_candidate: np.ndarray,
                                 candidate_ref: np.ndarray, candidate: np.ndarray,
                                 mask: np.ndarray) -> dict[str, object]:
    full_drift = srgb_to_linear(full_candidate) - srgb_to_linear(full_ref)
    candidate_drift = srgb_to_linear(candidate) - srgb_to_linear(candidate_ref)
    delta = full_drift[mask] - candidate_drift[mask]
    if not delta.size:
        return {"pixel count": 0, "metrics": None}
    absolute = np.abs(delta)
    return {"pixel count": int(np.count_nonzero(mask)), "metrics": {
        "linear RGB MAE": float(np.mean(absolute)),
        "linear RGB RMSE": float(np.sqrt(np.mean(delta * delta))),
        "linear RGB P95 absolute error": float(np.percentile(absolute, 95)),
        "pixel ratio max channel >1/255": float(np.mean(np.max(absolute, axis=1) > (1 / 255))),
    }}


def display_image(image: Image.Image, dimensions: tuple[int, int]) -> np.ndarray:
    if image.size != dimensions:
        image = image.resize(dimensions, Image.Resampling.LANCZOS)
    return np.asarray(image.convert("RGB"), dtype=np.uint8)


def paste_cell(sheet: Image.Image, draw: ImageDraw.ImageDraw, font: ImageFont.ImageFont,
               rgb: np.ndarray, row: int, column: int, label: str, crop: bool, source_name: str) -> None:
    cell_w, cell_h, label_h = 200, 170, 22
    x, y = column * cell_w, row * (cell_h + label_h)
    draw.text((x + 3, y + 4), label[:33], fill=(245, 245, 245), font=font)
    image = Image.fromarray(rgb, "RGB")
    if crop:
        u0, v0, u1, v1 = CROP_RECTS[source_name]
        image = image.crop((round(u0 * image.width), round(v0 * image.height),
                            round(u1 * image.width), round(v1 * image.height)))
    image.thumbnail((cell_w, cell_h), Image.Resampling.LANCZOS)
    px, py = x + (cell_w - image.width) // 2, y + label_h + (cell_h - image.height) // 2
    sheet.paste(image, (px, py))


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
        input_displays = {size: display_image(image, reference_dimensions) for size, image in inputs.items()}
        input_ref = input_displays[str(REFERENCE_SIZE)].astype(np.float32) / 255.0
        regions = make_regions(input_ref)
        for preset_name, preset in PRESETS.items():
            views: dict[str, dict[str, np.ndarray]] = {variant: {} for variant in VARIANTS}
            alpha_checks: dict[str, dict[str, bool]] = {variant: {} for variant in VARIANTS}
            for size in SIZES:
                key = str(size)
                image = inputs[key].convert("RGBA")
                fixed = apply_retro(image, **preset, seed=SEED)
                fixed_view = display_image(fixed, reference_dimensions)
                views["fixed_pixel"][key] = fixed_view
                alpha_checks["fixed_pixel"][key] = bool(np.array_equal(np.asarray(image.getchannel("A")),
                                                                         np.asarray(fixed.getchannel("A"))))
                if size == REFERENCE_SIZE:
                    for variant in VARIANTS[1:]:
                        normalized = display_scaled_apply(image, preset, reference_dimensions, SEED,
                                                          set(STAGES) if variant == "all_display_scaled" else {variant})
                        normalized_view = display_image(normalized, reference_dimensions)
                        if not np.array_equal(fixed_view, normalized_view):
                            delta = int(np.max(np.abs(fixed_view.astype(np.int16) - normalized_view.astype(np.int16))))
                            raise RuntimeError(f"512px model parity failed for {source_name}/{preset_name}/{variant}: max code delta {delta}")
                        views[variant][key] = normalized_view
                        alpha_checks[variant][key] = bool(np.array_equal(np.asarray(image.getchannel("A")),
                                                                         np.asarray(normalized.getchannel("A"))))
                else:
                    for variant in VARIANTS[1:]:
                        normalized = display_scaled_apply(image, preset, reference_dimensions, SEED,
                                                          set(STAGES) if variant == "all_display_scaled" else {variant})
                        views[variant][key] = display_image(normalized, reference_dimensions)
                        alpha_checks[variant][key] = bool(np.array_equal(np.asarray(image.getchannel("A")),
                                                                         np.asarray(normalized.getchannel("A"))))

            fixed_ref = views["fixed_pixel"][str(REFERENCE_SIZE)].astype(np.float32) / 255.0
            fixed_drift: dict[str, object] = {}
            for size in SIZES:
                key = str(size)
                fixed_candidate = views["fixed_pixel"][key].astype(np.float32) / 255.0
                fixed_drift[key] = {region: region_error(fixed_ref, fixed_candidate, mask)
                                    for region, mask in regions.items()}

            variant_metrics: dict[str, dict[str, object]] = {}
            for variant in VARIANTS[1:]:
                variant_ref = views[variant][str(REFERENCE_SIZE)].astype(np.float32) / 255.0
                sizes: dict[str, object] = {}
                for size in SIZES:
                    key = str(size)
                    current = views[variant][key].astype(np.float32) / 255.0
                    fixed_current = views["fixed_pixel"][key].astype(np.float32) / 255.0
                    sizes[key] = {
                        "variant drift from its 512px result": {
                            region: region_error(variant_ref, current, mask) for region, mask in regions.items()},
                        "variant versus fixed-pixel output at this resolution": {
                            region: region_error(fixed_current, current, mask) for region, mask in regions.items()},
                        "change in resolution-drift signature versus fixed-pixel": {
                            region: linear_delta_signature_error(fixed_ref, fixed_current, variant_ref, current, mask)
                            for region, mask in regions.items()},
                    }
                variant_metrics[variant] = sizes
            for variant in VARIANTS:
                for size in SIZES:
                    draw, font = draws[(variant, False)]
                    paste_cell(sheets[(variant, False)], draw, font, views[variant][str(size)], row,
                               SIZES.index(size), f"{Path(source_name).stem} / {preset_name} / {size}", False, source_name)
                    draw, font = draws[(variant, True)]
                    paste_cell(sheets[(variant, True)], draw, font, views[variant][str(size)], row,
                               SIZES.index(size), f"{Path(source_name).stem} / {preset_name} / {size}", True, source_name)
            records.append({"source": source_name, "preset": preset_name, "preset settings": preset,
                            "input dimensions by long edge": {key: list(value.size) for key, value in inputs.items()},
                            "normalized display dimensions": list(reference_dimensions), "seed": SEED,
                            "alpha exact match by variant and resolution": alpha_checks,
                            "fixed-pixel output drift": fixed_drift,
                            "display-scale variants": variant_metrics,
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
    for variant in VARIANTS[1:]:
        summaries[variant] = {}
        for size in SIZES:
            key = str(size)
            full_rmse = [r["fixed-pixel output drift"][key]["whole image"]["metrics"]["linear RGB RMSE"]
                         for r in records]
            candidate_rmse = [r["display-scale variants"][variant][key]["variant drift from its 512px result"]
                              ["whole image"]["metrics"]["linear RGB RMSE"] for r in records]
            signature_rmse = [r["display-scale variants"][variant][key]
                              ["change in resolution-drift signature versus fixed-pixel"]["whole image"]
                              ["metrics"]["linear RGB RMSE"] for r in records]
            summaries[variant][key] = {
                "fixed-pixel drift median/max": [float(np.median(full_rmse)), float(np.max(full_rmse))],
                "display-scale variant drift median/max": [float(np.median(candidate_rmse)), float(np.max(candidate_rmse))],
                "drift signature change median/max": [float(np.median(signature_rmse)), float(np.max(signature_rmse))],
                "paired cases with lower drift than fixed-pixel": sum(c < f for c, f in zip(candidate_rmse, full_rmse)),
            }

    report = {
        "question": "Do pixel-space suite parameters normalized to a 512px display coordinate reduce output drift across 512/1024/2048 photo inputs in a Python research model?",
        "evaluation plan": {
            "baseline": "Current apply_retro output at 512px is a within-run invariance reference only; not an approved appearance target.",
            "sources": "Same five SHA-verified photographs, EXIF and ICC handling as prior photo studies; untagged images assumed sRGB.",
            "conditions": f"Four current PRESETS, seed {SEED}, proportional Lanczos input sizes {list(SIZES)}. Every output is normalized to the exact 512px input dimensions.",
            "model parity gate": "Every display-scaled variant must exactly match apply_retro RGB8 output at 512px for every photo/preset. A mismatch aborts the run.",
            "hypotheses": {
                "pixelation": "Divide scale by long-edge resolution ratio, keeping the downsampled pixel count near the 512px recipe count.",
                "dither": "Sample the existing 4x4 Bayer matrix on normalized 512px coordinates, holding display-space tile pitch and phase.",
                "channel_offset": "Scale horizontal aberration displacement by current/reference input width.",
                "scanlines": "Sample alternating dark/bright row phase from normalized 512px row coordinates.",
                "grain": "Generate one seeded 512px-coordinate field and nearest-map it to input resolution, holding grain size and cross-resolution field alignment.",
                "all_display_scaled": "Apply the five hypotheses together; stage interactions remain present.",
            },
            "metrics": "Per-region linear RGB MAE/RMSE/P95/changed-pixel ratio for each variant's own 512px drift, paired difference from fixed-pixel output at each resolution, and the difference between signed fixed-pixel/candidate drift fields.",
            "visual review": "Aspect-preserving overview and normalized subject crops for all sources, presets, variants and resolutions. No calibrated display, approved target, independent reviewer or quality threshold.",
            "alpha and temporal": "Inputs are opaque and output alpha exact-match is checked. The seeded still-image spatial field does not evaluate frame-to-frame temporal behavior.",
            "performance": "Not measured. This Python equation model prioritizes pixel output comparisons and adds no timing claim.",
            "acceptance": "Exploratory parameter hypothesis. Lower drift means greater similarity to the 512px reference, not higher image quality or product correctness.",
            "unverified": "Human preference, alternate target viewing size, animation/temporal grain, actual product resize and GPU/Core pipelines, HDR/profile variations, performance and memory.",
        },
        "sources and provenance": source_info,
        "variant summaries": summaries,
        "records": [{key: value for key, value in record.items() if key != "outputs"} for record in records],
        "sheets": sheet_paths,
        "limits": "The research model mirrors current Python equations and is guarded by exact 512px RGB8 parity, but higher-resolution behavior is a hypothesis implementation, not product code. Each normalized-stage variant is a separate nonlinear counterfactual; per-stage improvements are not additive. The 512px reference does not establish a desired look.",
    }
    (CAPTURE / "photo-display-scale-hypothesis-metrics.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Compared {len(result['records'])} photo/preset cases across {len(VARIANTS)} parameter models and {len(SIZES)} resolutions.")
