"""Calibrate alternate grain covariance to current mono output-luma RMS, then validate on held-out seeds."""

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
from full_suite_photo_grain_color_covariance_study import (  # noqa: E402
    make_deltas, region_metrics, resize_to_view,
)
from full_suite_photo_grain_quantization_counterfactual_study import render_prebyte_float  # noqa: E402
from retro_lab import PRESETS, apply_retro  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-equal-luma-calibration"
SOURCES = ASSET_NAMES
SIZES = (512, 2048)
DISPLAY_EDGE = 512
TRAIN_SEEDS = (31111, 31116)
VALIDATION_SEEDS = (31121, 31126)
PROBE_FACTORS = (1.0, 2.0, 4.0)
ALTERNATES = ("independent-rgb", "zero-luma-chroma")
VARIANTS = ("current-monochrome", *ALTERNATES)


def common_view(image: Image.Image, size: tuple[int, int]) -> np.ndarray:
    rgb = image.convert("RGB")
    if rgb.size != size:
        rgb = rgb.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(rgb, dtype=np.uint8)


def output_metrics(on: np.ndarray, off: np.ndarray, mask: np.ndarray) -> dict[str, object]:
    result = region_metrics(on, off, mask)
    metrics = result["metrics"]
    if metrics is None:
        return result
    pixels = on[mask]
    metrics["display RGB8 endpoint channel ratio"] = float(np.mean((pixels == 0) | (pixels == 255)))
    off_pixels = off[mask]
    metrics["grain-off display RGB8 endpoint channel ratio"] = float(np.mean((off_pixels == 0) | (off_pixels == 255)))
    return result


def current_mono(image: Image.Image, preset: dict[str, object], seed: int,
                 target_size: tuple[int, int]) -> tuple[np.ndarray, np.ndarray]:
    delta = make_deltas(image.height, image.width, float(preset["grain"]), seed)["current-monochrome"]
    replay = render_prebyte_float(image, preset, seed, grain_delta=delta)
    replay_view = resize_to_view(np.uint8(replay * 255.0), target_size)
    actual_view = common_view(apply_retro(image, **preset, seed=seed), target_size)
    if not np.array_equal(replay_view, actual_view):
        raise RuntimeError(f"current mono replay mismatch at seed {seed}")
    return actual_view, replay_view


def calibrate_factor(target: float, responses: list[tuple[float, float]]) -> dict[str, object]:
    """Piecewise-linear interpolation of median output response, with no extrapolation."""
    ordered = [(0.0, 0.0), *sorted(responses)]
    monotonic = all(ordered[i + 1][1] >= ordered[i][1] for i in range(len(ordered) - 1))
    for (f0, y0), (f1, y1) in zip(ordered[:-1], ordered[1:]):
        if y0 <= target <= y1 and y1 > y0:
            factor = f0 + (target - y0) * (f1 - f0) / (y1 - y0)
            return {"factor": float(factor), "bracket": [f0, f1], "target bracketed": True,
                    "probe response monotonic": monotonic, "calibration residual at target": target}
    maximum_factor, maximum_response = ordered[-1]
    if target > maximum_response:
        return {"factor": maximum_factor, "bracket": None, "target bracketed": False,
                "probe response monotonic": monotonic,
                "unmatched target gap": float(target - maximum_response),
                "calibration residual at max probe": float(maximum_response)}
    return {"factor": 0.0, "bracket": None, "target bracketed": False,
            "probe response monotonic": monotonic,
            "unmatched target gap": float(max(0.0, ordered[0][1] - target)),
            "calibration residual at min probe": float(ordered[0][1])}


def save_sheet(source_name: str, preset_name: str,
               views: dict[tuple[str, int], np.ndarray],
               baselines: dict[int, np.ndarray], inputs: dict[int, np.ndarray]) -> None:
    labels = ("input", "grain-off", *VARIANTS)
    tile, label_h = 192, 22
    sheet = Image.new("RGB", (len(SIZES) * tile, len(labels) * (tile + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, label in enumerate(labels):
        for col, size in enumerate(SIZES):
            x, y = col * tile, row * (tile + label_h)
            draw.text((x + 3, y + 3), f"{label} / input {size} / seed {VALIDATION_SEEDS[0]}",
                      fill=(245, 245, 245), font=font)
            if label == "input":
                view = inputs[size]
            elif label == "grain-off":
                view = baselines[size]
            else:
                view = views[(label, size)]
            thumb = Image.fromarray(view, "RGB")
            thumb.thumbnail((tile, tile), Image.Resampling.LANCZOS)
            sheet.paste(thumb, (x + (tile - thumb.width) // 2,
                                y + label_h + (tile - thumb.height) // 2))
    stem = Path(source_name).stem
    sheet.save(CAPTURE / f"{stem}-{preset_name}.jpg", quality=94)
    crop = CROP_RECTS[source_name]
    rect = (round(crop[0] * DISPLAY_EDGE), round(crop[1] * DISPLAY_EDGE),
            round(crop[2] * DISPLAY_EDGE), round(crop[3] * DISPLAY_EDGE))
    detail = Image.new("RGB", (len(SIZES) * 256, len(labels) * 276), (28, 29, 33))
    detail_draw = ImageDraw.Draw(detail)
    for row, label in enumerate(labels):
        for col, size in enumerate(SIZES):
            x, y = col * 256, row * 276
            detail_draw.text((x + 3, y + 3), f"{label} / input {size} / detail",
                             fill=(245, 245, 245), font=font)
            if label == "input":
                view = inputs[size]
            elif label == "grain-off":
                view = baselines[size]
            else:
                view = views[(label, size)]
            image = Image.fromarray(view, "RGB").crop(rect)
            image.thumbnail((256, 256), Image.Resampling.LANCZOS)
            detail.paste(image, (x + (256 - image.width) // 2,
                                 y + 20 + (256 - image.height) // 2))
    detail.save(CAPTURE / f"{stem}-{preset_name}-detail.jpg", quality=95)


def main() -> dict[str, object]:
    CAPTURE.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    source_metadata: dict[str, object] = {}
    calibration_records: list[dict[str, object]] = []
    validation_records: list[dict[str, object]] = []
    parity_checks = 0

    for source_name in SOURCES:
        original, source_meta = load_source(source_name, manifest)
        source_metadata[source_name] = source_meta
        images = {size: resize_long_edge(original, size).convert("RGBA") for size in SIZES}
        ref_size = (images[512].width, images[512].height)
        source_ref = np.asarray(images[512].convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(source_ref)
        inputs = {size: common_view(images[size], ref_size) for size in SIZES}

        for preset_name, preset in PRESETS.items():
            sheet_views: dict[tuple[str, int], np.ndarray] = {}
            sheet_baselines: dict[int, np.ndarray] = {}
            for size in SIZES:
                image = images[size]
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                off = apply_retro(image, **off_settings, seed=TRAIN_SEEDS[0])
                off_view = common_view(off, ref_size)
                sheet_baselines[size] = off_view
                whole_mask = regions["whole image"]

                train_reference: dict[int, dict[str, object]] = {}
                probe_values: dict[str, dict[float, list[float]]] = {
                    variant: {factor: [] for factor in PROBE_FACTORS} for variant in ALTERNATES}
                for seed in TRAIN_SEEDS:
                    mono, replay = current_mono(image, preset, seed, ref_size)
                    parity_checks += 1
                    train_reference[seed] = {"rgb": mono,
                                             "luma rms": output_metrics(mono, off_view, whole_mask)["metrics"]["linear Rec709 luma RMS"]}
                    for variant in ALTERNATES:
                        for factor in PROBE_FACTORS:
                            sigma = float(preset["grain"]) * factor
                            delta = make_deltas(image.height, image.width, sigma, seed)[variant]
                            prebyte = render_prebyte_float(image, preset, seed, grain_delta=delta)
                            output = resize_to_view(np.uint8(prebyte * 255.0), ref_size)
                            measured = output_metrics(output, off_view, whole_mask)["metrics"]["linear Rec709 luma RMS"]
                            probe_values[variant][factor].append(float(measured))

                target = float(np.median([row["luma rms"] for row in train_reference.values()]))
                fitted: dict[str, object] = {}
                for variant in ALTERNATES:
                    responses = [(factor, float(np.median(probe_values[variant][factor])))
                                 for factor in PROBE_FACTORS]
                    fit = calibrate_factor(target, responses)
                    fit.update({"probe responses by factor": {
                        str(factor): [float(v) for v in probe_values[variant][factor]]
                        for factor in PROBE_FACTORS},
                        "median probe curve": {str(f): y for f, y in responses}})
                    fitted[variant] = fit
                calibration_records.append({"source": source_name, "preset": preset_name,
                                            "input long edge": size, "train seeds": list(TRAIN_SEEDS),
                                            "target current-mono whole luma RMS": target,
                                            "current-mono train values": {
                                                str(seed): train_reference[seed]["luma rms"] for seed in TRAIN_SEEDS},
                                            "fits": fitted})

                for seed in VALIDATION_SEEDS:
                    mono, _ = current_mono(image, preset, seed, ref_size)
                    parity_checks += 1
                    mono_metrics = {roi: output_metrics(mono, off_view, mask)
                                    for roi, mask in regions.items()}
                    heldout_target = float(mono_metrics["whole image"]["metrics"]["linear Rec709 luma RMS"])
                    validation_records.append({"source": source_name, "preset": preset_name,
                                               "input long edge": size, "seed": seed,
                                               "variant": "current-monochrome", "factor": 1.0,
                                               "calibration train median target": target,
                                               "target current-mono whole luma RMS": heldout_target,
                                               "ROI metrics": mono_metrics})
                    if seed == VALIDATION_SEEDS[0]:
                        sheet_views[("current-monochrome", size)] = mono

                    for variant in ALTERNATES:
                        factor = float(fitted[variant]["factor"])
                        sigma = float(preset["grain"]) * factor
                        delta = make_deltas(image.height, image.width, sigma, seed)[variant]
                        prebyte = render_prebyte_float(image, preset, seed, grain_delta=delta)
                        on = resize_to_view(np.uint8(prebyte * 255.0), ref_size)
                        roi_metrics = {roi: output_metrics(on, off_view, mask)
                                       for roi, mask in regions.items()}
                        record = {"source": source_name, "preset": preset_name,
                                  "input long edge": size, "seed": seed,
                                  "variant": variant, "calibrated factor": factor,
                                  "calibrated grain sigma": sigma,
                                  "calibration train median target": target,
                                  "target current-mono whole luma RMS": heldout_target,
                                  "ROI metrics": roi_metrics}
                        validation_records.append(record)
                        if seed == VALIDATION_SEEDS[0]:
                            sheet_views[(variant, size)] = on
            save_sheet(source_name, preset_name, sheet_views, sheet_baselines, inputs)

    aggregate: dict[str, object] = {}
    for size in SIZES:
        aggregate[str(size)] = {}
        for variant in VARIANTS:
            per_variant = [r for r in validation_records
                           if r["input long edge"] == size and r["variant"] == variant]
            ratio = []
            chroma = []
            p95 = []
            endpoints = []
            for row in per_variant:
                whole = row["ROI metrics"]["whole image"]["metrics"]
                if whole:
                    ratio.append(float(whole["linear Rec709 luma RMS"]) /
                                 max(float(row["target current-mono whole luma RMS"]), 1e-12))
                    chroma.append(float(whole["linear chroma-axis RMS"]))
                    p95.append(float(whole["linear luma P95 abs"]))
                    endpoints.append(float(whole["display RGB8 endpoint channel ratio"]))
            aggregate[str(size)][variant] = {
                "validated source/preset/seed cases": len(ratio),
                "median luma RMS / mono target ratio": float(np.median(ratio)),
                "median luma RMS ratio absolute error": float(np.median(np.abs(np.asarray(ratio) - 1.0))),
                "median chroma-axis RMS": float(np.median(chroma)),
                "median luma P95 absolute": float(np.median(p95)),
                "median output endpoint channel ratio": float(np.median(endpoints)),
            }

    fit_summary: dict[str, object] = {}
    for variant in ALTERNATES:
        rows = [r["fits"][variant] for r in calibration_records]
        bracketed = [r for r in rows if r["target bracketed"]]
        fit_summary[variant] = {"calibration groups": len(rows),
                                "target bracketed groups": len(bracketed),
                                "factor median": float(np.median([r["factor"] for r in rows])),
                                "factor P10/P90": [float(np.percentile([r["factor"] for r in rows], q))
                                                   for q in (10, 90)],
                                "monotonic training response groups": sum(
                                    bool(r["probe response monotonic"]) for r in rows)}

    report = {"study": "full-suite-photo-grain-equal-luma-calibration", "date": "2026-11-11",
              "environment": {"python": platform.python_version(), "numpy": np.__version__,
                              "pillow": Image.__version__},
              "plan": {"question": "How much alternate-channel grain is needed to match the current mono preset's whole-image output luma RMS, and what chroma/headroom costs remain on held-out seeds?",
                       "sources": source_metadata, "presets": {k: dict(v) for k, v in PRESETS.items()},
                       "input long edges": list(SIZES), "common display": "512 input pixel dimensions; 2048 output Lanczos RGB8 resized in encoded sRGB",
                       "training seeds": list(TRAIN_SEEDS), "held-out validation seeds": list(VALIDATION_SEEDS),
                       "probe factors": list(PROBE_FACTORS),
                       "calibration target": "Current monochrome apply_retro output-minus-grain=0 linear Rec.709 luma RMS over whole 512-display image; fit is per source/preset/input-size using the median of two training seeds.",
                       "fit rule": "Piecewise-linear interpolation between zero response and measured median output response at factors 1/2/4; no extrapolation. If the target is not bracketed, use the nearest tested bound and record the unmatched gap.",
                       "validation": "Two unseen deterministic seeds; fit remains frozen, and each candidate is compared with that same held-out seed's current-mono output RMS plus the same grain-off baseline. Report whole-image target ratio plus regional luma/chroma/P95 and final RGB8 endpoint channel occupancy.",
                       "acceptance": "Descriptive; no approved target, observer, perceptual equivalence tolerance, or quality pass/fail."},
              "current mono RGB8 replay parity checks": parity_checks,
              "calibration fit summary": fit_summary,
              "held-out aggregate by input size and variant": aggregate,
              "calibration records": calibration_records,
              "held-out validation records": validation_records,
              "limitations": ["two training and two held-out seeds are small deterministic samples, not population confidence intervals",
                              "whole-image luma matching does not equalize local ROIs, perceived grain strength, chroma, or texture spectrum",
                              "sRGB code-value additive noise, clamp, truncating RGB8 output, and encoded-sRGB downsampling interact nonlinearly",
                              "endpoint occupancy is a final RGB8 image statistic, not native pre-clamp clipping attribution",
                              "no alpha, HDR, human preference, Artifact runtime/Core/GPU path, preview/export, performance, or memory assessment"]}
    path = CAPTURE / "grain-equal-luma-calibration-study.json"
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(path), "calibration groups": len(calibration_records),
                      "validation cases": len(validation_records), "mono parity checks": parity_checks,
                      "fit summary": fit_summary, "validation summary": aggregate}, indent=2))
    return report


if __name__ == "__main__":
    main()
