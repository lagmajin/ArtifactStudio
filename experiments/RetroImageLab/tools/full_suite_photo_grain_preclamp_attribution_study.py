"""Attribute grain-related pre-clamp excursions versus existing stage endpoints."""

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
    ASSET_NAMES, MANIFEST, load_source, resize_long_edge,
)
from full_suite_photo_tone_study import make_regions  # noqa: E402
sys.path.insert(0, str(ROOT))
from full_suite_photo_grain_color_covariance_study import (  # noqa: E402
    make_deltas, region_metrics, resize_to_view,
)
from full_suite_photo_grain_equal_luma_calibration_study import (  # noqa: E402
    VALIDATION_SEEDS, common_view,
)
from full_suite_photo_grain_quantization_counterfactual_study import render_prebyte_float  # noqa: E402
from retro_lab import PRESETS, apply_retro  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-preclamp-attribution"
SIZES = (512, 2048)
SOURCES = ASSET_NAMES
SEEDS = VALIDATION_SEEDS
VARIANTS = ("current-monochrome", "independent-rgb-matched-luma",
            "chroma-factor-4", "chroma-matched-luma")
DISPLAY_SEED = SEEDS[0]


def load_report(folder: str, filename: str) -> dict[str, object]:
    return json.loads((ROOT / "captures" / folder / filename).read_text(encoding="utf-8"))


def resized_mask(mask: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    image = Image.fromarray(np.uint8(mask) * 255, "L")
    if image.size != size:
        image = image.resize(size, Image.Resampling.NEAREST)
    return np.asarray(image, dtype=np.uint8) > 0


def endpoint_ratio(rgb: np.ndarray, mask: np.ndarray) -> float | None:
    if not np.any(mask):
        return None
    selected = rgb[mask]
    return float(np.mean((selected == 0) | (selected == 255)))


def stage_region_metrics(pregrain: np.ndarray, preclamp: np.ndarray,
                         output_native: np.ndarray, baseline_native: np.ndarray,
                         mask: np.ndarray) -> dict[str, object]:
    count = int(np.count_nonzero(mask))
    if not count:
        return {"pixel count": 0, "metrics": None}
    base = pregrain[mask]
    raw = preclamp[mask]
    low = raw < 0.0
    high = raw > 1.0
    crossed_low = (base > 0.0) & low
    crossed_high = (base < 1.0) & high
    below_depth = np.maximum(-raw, 0.0)
    above_depth = np.maximum(raw - 1.0, 0.0)
    new_endpoint = ((base > 0.0) & (output_native[mask] == 0)) | \
                   ((base < 1.0) & (output_native[mask] == 255))
    on_endpoint = (output_native[mask] == 0) | (output_native[mask] == 255)
    off_endpoint = (baseline_native[mask] == 0) | (baseline_native[mask] == 255)
    return {"pixel count": count, "metrics": {
        "pregrain endpoint channel ratio": float(np.mean((base <= 0.0) | (base >= 1.0))),
        "preclamp below-zero channel ratio": float(np.mean(low)),
        "preclamp above-one channel ratio": float(np.mean(high)),
        "new below-zero crossing channel ratio": float(np.mean(crossed_low)),
        "new above-one crossing channel ratio": float(np.mean(crossed_high)),
        "new out-of-range crossing channel ratio": float(np.mean(crossed_low | crossed_high)),
        "mean underflow excursion depth": float(np.mean(below_depth)),
        "mean overflow excursion depth": float(np.mean(above_depth)),
        "P95 excursion depth over all channels": float(np.percentile(np.maximum(below_depth, above_depth), 95)),
        "native RGB8 endpoint channel ratio": endpoint_ratio(output_native[mask], np.ones(count, dtype=bool)),
        "grain-off native RGB8 endpoint channel ratio": endpoint_ratio(baseline_native[mask], np.ones(count, dtype=bool)),
        "new RGB8 endpoint channel ratio versus grain-off": float(np.mean(~off_endpoint & on_endpoint)),
        "removed RGB8 endpoint channel ratio versus grain-off": float(np.mean(off_endpoint & ~on_endpoint)),
        "net RGB8 endpoint channel ratio change versus grain-off": float(np.mean(on_endpoint) - np.mean(off_endpoint)),
        "new native RGB8 endpoint channel ratio": float(np.mean(new_endpoint)),
    }}


def save_crossing_sheet(source: str, preset_name: str,
                        rows: list[dict[str, object]]) -> None:
    tile, label_h = 192, 22
    header_h = 24
    sheet = Image.new("RGB", (len(SIZES) * tile, header_h + len(VARIANTS) * (tile + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    draw.text((4, 4), "Red: new grain-induced [0,1] crossing | Green: pre-grain stage endpoint",
              fill=(245, 245, 245), font=font)
    short = {"current-monochrome": "mono", "independent-rgb-matched-luma": "RGB matched",
             "chroma-factor-4": "chroma x4", "chroma-matched-luma": "chroma fitted"}
    for row, variant in enumerate(VARIANTS):
        for col, size in enumerate(SIZES):
            x, y = col * tile, header_h + row * (tile + label_h)
            draw.text((x + 3, y + 3), f"{short[variant]} / {size} px",
                      fill=(245, 245, 245), font=font)
            match = next(r for r in rows if r["variant"] == variant and r["input long edge"] == size)
            image = match["display crossing map"]
            thumb = Image.fromarray(image, "RGB")
            thumb.thumbnail((tile, tile), Image.Resampling.NEAREST)
            sheet.paste(thumb, (x + (tile - thumb.width) // 2,
                                y + label_h + (tile - thumb.height) // 2))
    stem = Path(source).stem
    sheet.save(CAPTURE / f"{stem}-{preset_name}-crossings.jpg", quality=95)


def main() -> dict[str, object]:
    CAPTURE.mkdir(parents=True, exist_ok=True)
    fit_report = load_report("full-suite-photo-grain-equal-luma-calibration",
                             "grain-equal-luma-calibration-study.json")
    headroom_report = load_report("full-suite-photo-grain-chroma-headroom",
                                  "grain-chroma-headroom-study.json")
    independent_fits = {(r["source"], r["preset"], r["input long edge"]):
                        float(r["fits"]["independent-rgb"]["factor"])
                        for r in fit_report["calibration records"]}
    chroma_fits = {(r["source"], r["preset"], r["input long edge"]): float(r["fit"]["factor"])
                   for r in headroom_report["calibration records"]}
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    source_records: dict[str, object] = {}
    records: list[dict[str, object]] = []
    parity_checks = 0
    crossing_maps: dict[tuple[str, str], list[dict[str, object]]] = {}

    for source_name in SOURCES:
        original, metadata = load_source(source_name, manifest)
        source_records[source_name] = metadata
        images = {size: resize_long_edge(original, size).convert("RGBA") for size in SIZES}
        ref_size = (images[512].width, images[512].height)
        ref_rgb = np.asarray(images[512].convert("RGB"), dtype=np.float32) / 255.0
        regions_view = make_regions(ref_rgb)

        for preset_name, preset in PRESETS.items():
            for size in SIZES:
                image = images[size]
                dimensions = (image.width, image.height)
                native_regions = {key: resized_mask(mask, dimensions)
                                  for key, mask in regions_view.items()}
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                off_native = np.asarray(apply_retro(image, **off_settings, seed=SEEDS[0]).convert("RGB"),
                                        dtype=np.uint8)
                off_view = common_view(Image.fromarray(off_native, "RGB"), ref_size)
                source_rows: list[dict[str, object]] = []

                for seed in SEEDS:
                    factors = {
                        "current-monochrome": 1.0,
                        "independent-rgb-matched-luma": independent_fits[(source_name, preset_name, size)],
                        "chroma-factor-4": 4.0,
                        "chroma-matched-luma": chroma_fits[(source_name, preset_name, size)],
                    }
                    for variant in VARIANTS:
                        sigma = float(preset["grain"]) * factors[variant]
                        delta = make_deltas(image.height, image.width, sigma, seed)[
                            "current-monochrome" if variant == "current-monochrome" else
                            "independent-rgb" if variant == "independent-rgb-matched-luma" else
                            "zero-luma-chroma"]
                        pregrain, preclamp, clipped = render_prebyte_float(
                            image, preset, seed, grain_delta=delta, return_stages=True)
                        native_rgb = np.uint8(clipped * 255.0)
                        if variant == "current-monochrome":
                            actual = np.asarray(apply_retro(image, **preset, seed=seed).convert("RGB"),
                                                dtype=np.uint8)
                            if not np.array_equal(native_rgb, actual):
                                raise RuntimeError(f"mono byte parity failed {source_name}/{preset_name}/{size}/{seed}")
                            parity_checks += 1
                        display_rgb = resize_to_view(native_rgb, ref_size)
                        display_pregrain = resize_to_view(np.uint8(np.clip(pregrain, 0, 1) * 255.0), ref_size)
                        display_preclamp = resize_to_view(np.uint8(np.clip(preclamp, 0, 1) * 255.0), ref_size)
                        roi_metrics: dict[str, object] = {}
                        for roi_name, native_mask in native_regions.items():
                            view_mask = regions_view[roi_name]
                            native_stats = stage_region_metrics(pregrain, preclamp, native_rgb,
                                                               off_native, native_mask)
                            view_endpoint = endpoint_ratio(display_rgb, view_mask)
                            base_endpoint = endpoint_ratio(off_view, view_mask)
                            delta_metrics = region_metrics(display_rgb, off_view, view_mask)
                            if delta_metrics["metrics"] is not None:
                                delta_metrics["metrics"]["display RGB8 endpoint channel ratio"] = view_endpoint
                                delta_metrics["metrics"]["grain-off display endpoint channel ratio"] = base_endpoint
                            roi_metrics[roi_name] = {
                                "native pre-clamp": native_stats,
                                "common 512 display residual / endpoints": delta_metrics,
                                "display clipped-stage endpoint channel ratio": endpoint_ratio(display_preclamp, view_mask),
                            }
                        newcross = ((pregrain > 0.0) & (preclamp < 0.0)) | \
                                   ((pregrain < 1.0) & (preclamp > 1.0))
                        cross_pixel = np.any(newcross, axis=2)
                        # Red marks pixels with at least one new clamp crossing; green marks pre-existing stage endpoints.
                        base_endpoint_pixel = np.any((pregrain <= 0.0) | (pregrain >= 1.0), axis=2)
                        map_rgb = np.zeros((image.height, image.width, 3), dtype=np.uint8)
                        map_rgb[:, :, 0] = np.where(cross_pixel, 255, 0)
                        map_rgb[:, :, 1] = np.where(base_endpoint_pixel & ~cross_pixel, 190, 0)
                        display_map = resize_to_view(map_rgb, ref_size)
                        record = {"source": source_name, "preset": preset_name,
                                  "input long edge": size, "seed": seed,
                                  "variant": variant, "sigma factor": factors[variant],
                                  "grain sigma": sigma, "ROIs": roi_metrics}
                        records.append(record)
                        if seed == DISPLAY_SEED:
                            source_rows.append({"variant": variant, "input long edge": size,
                                                "display crossing map": display_map})
                crossing_maps[(source_name, preset_name)] = crossing_maps.get((source_name, preset_name), []) + source_rows
            # Same source/preset rows have both sizes and are ready after the size loop.
            sheet_rows = [row for row in crossing_maps[(source_name, preset_name)]
                          if row["input long edge"] in SIZES]
            # Remove duplicate old rows defensively if this script is called from a persistent module state.
            unique: dict[tuple[str, int], dict[str, object]] = {}
            for row in sheet_rows:
                unique[(row["variant"], row["input long edge"])] = row
            save_crossing_sheet(source_name, preset_name, list(unique.values()))

    summary: dict[str, object] = {}
    for variant in VARIANTS:
        selected = [r for r in records if r["variant"] == variant]
        summary[variant] = {"cases": len(selected)}
        for size in SIZES:
            per_size = [r for r in selected if r["input long edge"] == size]
            whole = [r["ROIs"]["whole image"] for r in per_size]
            fields = (
                ("new preclamp crossing channel ratio",
                 lambda row: row["native pre-clamp"]["metrics"]["new out-of-range crossing channel ratio"]),
                ("pregrain endpoint channel ratio",
                 lambda row: row["native pre-clamp"]["metrics"]["pregrain endpoint channel ratio"]),
                ("native RGB8 endpoint channel ratio",
                 lambda row: row["native pre-clamp"]["metrics"]["native RGB8 endpoint channel ratio"]),
                ("grain-off native RGB8 endpoint channel ratio",
                 lambda row: row["native pre-clamp"]["metrics"]["grain-off native RGB8 endpoint channel ratio"]),
                ("new RGB8 endpoint channel ratio versus grain-off",
                 lambda row: row["native pre-clamp"]["metrics"]["new RGB8 endpoint channel ratio versus grain-off"]),
                ("removed RGB8 endpoint channel ratio versus grain-off",
                 lambda row: row["native pre-clamp"]["metrics"]["removed RGB8 endpoint channel ratio versus grain-off"]),
                ("net RGB8 endpoint channel ratio change versus grain-off",
                 lambda row: row["native pre-clamp"]["metrics"]["net RGB8 endpoint channel ratio change versus grain-off"]),
                ("display RGB8 endpoint channel ratio",
                 lambda row: row["common 512 display residual / endpoints"]["metrics"]["display RGB8 endpoint channel ratio"]),
                ("display residual linear luma RMS",
                 lambda row: row["common 512 display residual / endpoints"]["metrics"]["linear Rec709 luma RMS"]),
            )
            summary[variant][str(size)] = {key: float(np.median([extract(row) for row in whole]))
                                           for key, extract in fields}

    report = {"study": "full-suite-photo-grain-preclamp-attribution", "date": "2026-11-11",
              "environment": {"python": platform.python_version(), "numpy": np.__version__,
                              "pillow": Image.__version__},
              "plan": {"question": "Which final RGB8 endpoints were already present before grain, and which native float channels cross [0,1] because of added grain?",
                       "sources": source_records, "presets": {k: dict(v) for k, v in PRESETS.items()},
                       "input long edges": list(SIZES), "seeds": list(SEEDS),
                       "variants": {"current-monochrome": "Current suite mono sigma, factor1.",
                                    "independent-rgb-matched-luma": "Experiment117 train-fitted independent RGB sigma.",
                                    "chroma-factor-4": "Code-domain luma-null chroma at 4x preset sigma.",
                                    "chroma-matched-luma": "Experiment118 train-fitted luma target factor, capped at 8x."},
                       "stage tap": "Float RGB immediately after scanlines, then after additive grain before clip, then current [0,1] clip and truncating RGB8 conversion. Default replay path is unchanged; stage copies are created only for this offline analysis.",
                       "crossing definitions": "New low crossing: pregrain>0 and preclamp<0. New high crossing: pregrain<1 and preclamp>1. Existing stage endpoints are pregrain<=0 or >=1. Also count final native/display RGB8 code 0/255, which can include non-grain and quantization effects.",
                       "ROI": "Seven source-defined whole/shadow/deep-shadow/midtone/highlight/smooth/detail regions from reference512, nearest-mapped to native dimensions.",
                       "acceptance": "No approved visual target or perceptual threshold; descriptive attribution study."},
              "current mono RGB8 parity checks": parity_checks,
              "summary median by variant and input size": summary,
              "records": records,
              "limitations": ["only two held-out deterministic seeds; no confidence interval claim",
                              "native excursion is after suite's own transfer/code-value stages, not a GPU or product buffer path",
                              "RGB8 endpoints do not by themselves establish visible clipping defect",
                              "no alpha/HDR/human preference/preview-export/Core-GPU/performance assessment"]}
    path = CAPTURE / "grain-preclamp-attribution-study.json"
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(path), "records": len(records),
                      "mono parity gates": parity_checks, "summary": summary}, indent=2))
    return report


if __name__ == "__main__":
    main()
