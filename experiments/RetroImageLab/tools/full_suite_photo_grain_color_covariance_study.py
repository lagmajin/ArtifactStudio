"""Compare monochrome, independent RGB, and zero-luma chroma grain response."""

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
    ASSET_NAMES, CROP_RECTS, MANIFEST, load_source, resize_long_edge,
)
from full_suite_photo_tone_study import make_regions  # noqa: E402
sys.path.insert(0, str(ROOT))
from full_suite_photo_grain_quantization_counterfactual_study import render_prebyte_float  # noqa: E402
from retro_lab import PRESETS, apply_retro, srgb_to_linear  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-color-covariance"
SOURCES = ASSET_NAMES
SIZES = (512, 2048)
DISPLAY_EDGE = 512
SEEDS = (31111, 31116, 31121, 31126)
VARIANTS = ("current-monochrome", "independent-rgb", "zero-luma-chroma")
LUMA = np.array([.2126, .7152, .0722], dtype=np.float32)
CHROMA_1 = np.array([.7152, -.2126, 0.0], dtype=np.float64)
CHROMA_1 /= np.linalg.norm(CHROMA_1)
CHROMA_2 = np.cross(LUMA.astype(np.float64), CHROMA_1)
CHROMA_2 /= np.linalg.norm(CHROMA_2)


def common_view(image: Image.Image, size: tuple[int, int]) -> np.ndarray:
    rgb = image.convert("RGB")
    if rgb.size != size:
        rgb = rgb.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(rgb, dtype=np.uint8)


def make_deltas(height: int, width: int, sigma: float, seed: int) -> dict[str, np.ndarray]:
    # This 2D sample matches apply_retro's shape=(H,W,1) seeded draw exactly.
    mono_rng = np.random.default_rng(seed)
    mono = mono_rng.normal(0.0, sigma, size=(height, width)).astype(np.float32)

    independent_rng = np.random.default_rng(seed + 104729)
    independent = independent_rng.normal(0.0, sigma, size=(height, width, 3)).astype(np.float32)

    chroma_rng = np.random.default_rng(seed + 130363)
    scale = sigma * np.sqrt(1.5)
    a = chroma_rng.normal(0.0, scale, size=(height, width)).astype(np.float32)
    b = chroma_rng.normal(0.0, scale, size=(height, width)).astype(np.float32)
    chroma = a[:, :, None] * CHROMA_1.astype(np.float32) + b[:, :, None] * CHROMA_2.astype(np.float32)
    return {"current-monochrome": mono, "independent-rgb": independent,
            "zero-luma-chroma": chroma.astype(np.float32)}


def resize_to_view(rgb8: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    image = Image.fromarray(rgb8, "RGB")
    if image.size != size:
        image = image.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(image, dtype=np.uint8)


def region_metrics(on: np.ndarray, off: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"pixel count": 0, "metrics": None}
    on_linear = srgb_to_linear(on.astype(np.float32) / 255.0)
    off_linear = srgb_to_linear(off.astype(np.float32) / 255.0)
    delta = on_linear[mask] - off_linear[mask]
    luma = delta @ LUMA
    chroma_rg = delta[:, 0] - delta[:, 1]
    chroma_by = delta[:, 2] - .5 * (delta[:, 0] + delta[:, 1])
    chroma_energy = .5 * (chroma_rg * chroma_rg + chroma_by * chroma_by)
    return {"pixel count": count, "metrics": {
        "linear RGB RMS": float(np.sqrt(np.mean(delta * delta))),
        "linear Rec709 luma RMS": float(np.sqrt(np.mean(luma * luma))),
        "linear chroma-axis RMS": float(np.sqrt(np.mean(chroma_energy))),
        "linear luma signed mean": float(np.mean(luma)),
        "linear luma P95 abs": float(np.percentile(np.abs(luma), 95)),
        "linear chroma P95 axis magnitude": float(np.percentile(np.sqrt(chroma_energy), 95)),
        "RGB8 changed pixel ratio": float(np.mean(np.any(on[mask] != off[mask], axis=1))),
    }}


def save_sheets(source_name: str, preset_name: str,
                views: dict[tuple[str, int], np.ndarray], baseline: dict[int, np.ndarray],
                source_views: dict[int, np.ndarray]) -> None:
    labels = ("input", "grain-off", *VARIANTS)
    tile = 192
    label_h = 22
    sheet = Image.new("RGB", (len(SIZES) * tile, len(labels) * (tile + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, label in enumerate(labels):
        for col, size in enumerate(SIZES):
            x, y = col * tile, row * (tile + label_h)
            draw.text((x + 3, y + 3), f"{label} / input {size}", fill=(245, 245, 245), font=font)
            if label == "input":
                image = source_views[size]
            elif label == "grain-off":
                image = baseline[size]
            else:
                image = views[(label, size)]
            thumb = Image.fromarray(image, "RGB")
            thumb.thumbnail((tile, tile), Image.Resampling.LANCZOS)
            sheet.paste(thumb, (x + (tile - thumb.width) // 2,
                                y + label_h + (tile - thumb.height) // 2))
    stem = Path(source_name).stem
    sheet.save(CAPTURE / f"{stem}-{preset_name}.jpg", quality=94)

    crop = CROP_RECTS[source_name]
    x0, y0, x1, y1 = (round(crop[0] * DISPLAY_EDGE), round(crop[1] * DISPLAY_EDGE),
                       round(crop[2] * DISPLAY_EDGE), round(crop[3] * DISPLAY_EDGE))
    crop_w, crop_h = x1 - x0, y1 - y0
    detail = Image.new("RGB", (len(SIZES) * 256, len(labels) * 276), (28, 29, 33))
    detail_draw = ImageDraw.Draw(detail)
    for row, label in enumerate(labels):
        for col, size in enumerate(SIZES):
            x, y = col * 256, row * 276
            detail_draw.text((x + 3, y + 3), f"{label} / input {size} / detail", fill=(245, 245, 245), font=font)
            if label == "input":
                image = source_views[size]
            elif label == "grain-off":
                image = baseline[size]
            else:
                image = views[(label, size)]
            tile_image = Image.fromarray(image, "RGB").crop((x0, y0, x1, y1))
            tile_image.thumbnail((256, 256), Image.Resampling.LANCZOS)
            detail.paste(tile_image, (x + (256 - tile_image.width) // 2,
                                      y + 20 + (256 - tile_image.height) // 2))
    detail.save(CAPTURE / f"{stem}-{preset_name}-detail.jpg", quality=95)


def main() -> dict[str, object]:
    CAPTURE.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    source_meta: dict[str, object] = {}
    records: list[dict[str, object]] = []
    parity_checks = 0

    for source_name in SOURCES:
        original, meta = load_source(source_name, manifest)
        source_meta[source_name] = meta
        inputs = {size: resize_long_edge(original, size) for size in SIZES}
        ref_size = inputs[512].size
        source_ref = np.asarray(inputs[512].convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(source_ref)
        source_views = {size: common_view(inputs[size], ref_size) for size in SIZES}

        for preset_name, preset in PRESETS.items():
            views: dict[tuple[str, int], np.ndarray] = {}
            baseline_views: dict[int, np.ndarray] = {}
            selected_views: dict[tuple[str, int], np.ndarray] = {}
            for size in SIZES:
                source = inputs[size].convert("RGBA")
                height, width = source.height, source.width
                target_size = ref_size
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                off_full = apply_retro(source, **off_settings, seed=SEEDS[0])
                off_view = common_view(off_full, target_size)
                baseline_views[size] = off_view

                for seed in SEEDS:
                    deltas = make_deltas(height, width, float(preset["grain"]), seed)
                    mono_float = render_prebyte_float(source, preset, seed, grain_delta=deltas["current-monochrome"])
                    mono_rgb8 = np.uint8(mono_float * 255.0)
                    mono_actual = common_view(apply_retro(source, **preset, seed=seed), target_size)
                    mono_replay = resize_to_view(mono_rgb8, target_size)
                    if not np.array_equal(mono_replay, mono_actual):
                        raise RuntimeError(f"mono replay parity failed: {source_name}/{preset_name}/{size}/{seed}")
                    parity_checks += 1
                    variant_rgb8 = {"current-monochrome": mono_actual}
                    for variant in ("independent-rgb", "zero-luma-chroma"):
                        floated = render_prebyte_float(source, preset, seed, grain_delta=deltas[variant])
                        variant_rgb8[variant] = common_view(
                            Image.fromarray(np.uint8(floated * 255.0), "RGB"), target_size)

                    off_display = off_view
                    for variant, on_view in variant_rgb8.items():
                        field = deltas[variant]
                        field_rgb = np.repeat(field[:, :, None], 3, axis=2) if field.ndim == 2 else field
                        field_luma = field_rgb @ LUMA
                        field_chroma_rg = field_rgb[:, :, 0] - field_rgb[:, :, 1]
                        field_chroma_by = field_rgb[:, :, 2] - .5 * (field_rgb[:, :, 0] + field_rgb[:, :, 1])
                        if variant == "zero-luma-chroma" and float(np.max(np.abs(field_luma))) > 1e-6:
                            raise RuntimeError(f"chroma field is not luma-null in code domain: {source_name}/{seed}")
                        input_field_metrics = {
                            "per-channel RMS": [float(np.sqrt(np.mean(field_rgb[:, :, c] ** 2))) for c in range(3)],
                            "mean per-channel RMS": float(np.sqrt(np.mean(field_rgb * field_rgb))),
                            "Rec709 code-domain luma RMS": float(np.sqrt(np.mean(field_luma * field_luma))),
                            "chroma-axis RMS": float(np.sqrt(np.mean(.5 * (field_chroma_rg ** 2 + field_chroma_by ** 2)))),
                        }
                        delta_record = {roi: region_metrics(on_view, off_display, mask)
                                        for roi, mask in regions.items()}
                        records.append({"source": source_name, "preset": preset_name,
                                        "input long edge": size, "seed": seed,
                                        "variant": variant, "grain sigma": float(preset["grain"]),
                                        "pre-clamp sRGB-code noise field stats": input_field_metrics,
                                        "ROI metrics": delta_record})
                        if seed == SEEDS[0]:
                            selected_views[(variant, size)] = on_view
            save_sheets(source_name, preset_name, selected_views, baseline_views, source_views)

    summaries: dict[str, object] = {}
    rois = list(records[0]["ROI metrics"])
    for size in SIZES:
        summaries[str(size)] = {}
        for variant in VARIANTS:
            summaries[str(size)][variant] = {}
            for roi in rois:
                selected = [r["ROI metrics"][roi]["metrics"] for r in records
                            if r["input long edge"] == size and r["variant"] == variant
                            and r["ROI metrics"][roi]["metrics"] is not None]
                summaries[str(size)][variant][roi] = {
                    key: float(np.median([row[key] for row in selected]))
                    for key in selected[0]} if selected else None

    report = {"study": "full-suite-photo-grain-color-covariance", "date": "2026-11-11",
              "environment": {"python": platform.python_version(), "numpy": np.__version__,
                              "pillow": Image.__version__},
              "plan": {"question": "At matched pre-clamp RGB noise power, how do monochrome, independent-channel, and zero-luma chroma grain alter photo luma/chroma residuals in the current suite?",
                       "sources": source_meta, "input long edges": list(SIZES),
                       "common display": "512px long-edge source dimensions; 2048 outputs Lanczos resized in encoded sRGB RGB8 to match 512 input pixel dimensions",
                       "presets": {k: dict(v) for k, v in PRESETS.items()},
                       "seeds": list(SEEDS), "variants": {
                           "current-monochrome": "One Gaussian scalar sample per pixel added equally to R/G/B; generated with current suite seed and sigma.",
                           "independent-rgb": "Three independent Gaussian channel samples, each with the current preset sigma.",
                           "zero-luma-chroma": "Two independent Gaussian fields projected onto an orthonormal basis null to Rec.709 weights in the additive sRGB-code domain; scaled so expected mean per-channel input variance matches the other variants."},
                       "measurement": "Current grain-off output is paired with each RGB8 grain-on result, then decoded to linear RGB. Rec.709 luma and two opponent chroma axes (R-G, B-(R+G)/2) are summarized per source-luma/gradient ROI using RMS, signed mean/P95, and RGB8 changed ratio.",
                       "noise injection point": "After current pixelation, ordered quantization, channel offset, and scanlines; before clamp and RGB8 conversion, matching current suite stage location. Noise is additive in sRGB code values, not linear light.",
                       "acceptance": "No approved look, observer, or perceptual pass/fail threshold; descriptive response comparison only."},
              "RGB8 replay parity checks": parity_checks,
              "summary median by input size / variant / ROI": summaries,
              "seed records": records,
              "limitations": ["noise variants are counterfactuals and do not correspond to current preset settings",
                              "matching expected pre-clamp channel variance does not match post-clamp/output perceptual strength",
                              "2048-to-512 conversion uses encoded-sRGB Pillow Lanczos, not a verified product display path",
                              "photo set includes an internally restricted portrait source; generated captures remain local research artifacts",
                              "alpha, HDR, human preference, Artifact runtime/Core/GPU parity, preview/export, performance, and memory are not assessed"]}
    output = CAPTURE / "grain-color-covariance-study.json"
    output.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(output), "seed records": len(records),
                      "RGB8 replay parity checks": parity_checks,
                      "512 whole-image luma RMS": {
                          variant: summaries["512"][variant]["whole image"]["linear Rec709 luma RMS"]
                          for variant in VARIANTS},
                      "2048 whole-image luma RMS": {
                          variant: summaries["2048"][variant]["whole image"]["linear Rec709 luma RMS"]
                          for variant in VARIANTS},
                      "2048 whole-image chroma RMS": {
                          variant: summaries["2048"][variant]["whole image"]["linear chroma-axis RMS"]
                          for variant in VARIANTS}}, indent=2))
    return report


if __name__ == "__main__":
    main()
