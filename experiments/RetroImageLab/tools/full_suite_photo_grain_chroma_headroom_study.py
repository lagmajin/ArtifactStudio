"""Probe the chroma-only grain headroom frontier beyond the factor-4 calibration bound."""

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
from full_suite_photo_grain_color_covariance_study import make_deltas, resize_to_view  # noqa: E402
from full_suite_photo_grain_equal_luma_calibration_study import (  # noqa: E402
    TRAIN_SEEDS, VALIDATION_SEEDS, calibrate_factor, common_view, output_metrics,
)
from full_suite_photo_grain_quantization_counterfactual_study import render_prebyte_float  # noqa: E402
from retro_lab import PRESETS, apply_retro  # noqa: E402

CAPTURE = ROOT / "captures" / "full-suite-photo-grain-chroma-headroom"
SOURCE_NAMES = ASSET_NAMES
SIZES = (512, 2048)
PROBE_FACTOR = 8.0
VALIDATION_DISPLAY_SEED = VALIDATION_SEEDS[0]


def save_sheet(source_name: str, preset_name: str,
               views: dict[tuple[str, int], np.ndarray], baselines: dict[int, np.ndarray],
               inputs: dict[int, np.ndarray]) -> None:
    labels = ("input", "grain-off", "current-mono", "chroma-factor-4", "chroma-luma-matched")
    tile, label_h = 192, 22
    sheet = Image.new("RGB", (len(SIZES) * tile, len(labels) * (tile + label_h)), (28, 29, 33))
    draw, font = ImageDraw.Draw(sheet), ImageFont.load_default()
    for row, label in enumerate(labels):
        for col, size in enumerate(SIZES):
            x, y = col * tile, row * (tile + label_h)
            draw.text((x + 3, y + 3), f"{label} / input {size} / seed {VALIDATION_DISPLAY_SEED}",
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
    rect = (round(crop[0] * 512), round(crop[1] * 512),
            round(crop[2] * 512), round(crop[3] * 512))
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
    prior_path = ROOT / "captures" / "full-suite-photo-grain-equal-luma-calibration" / \
        "grain-equal-luma-calibration-study.json"
    prior = json.loads(prior_path.read_text(encoding="utf-8"))
    prior_cal = {(r["source"], r["preset"], r["input long edge"]): r
                 for r in prior["calibration records"]}
    prior_validation = {(r["source"], r["preset"], r["input long edge"], r["seed"], r["variant"]): r
                       for r in prior["held-out validation records"]}
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    calibration_records: list[dict[str, object]] = []
    validation_records: list[dict[str, object]] = []
    sources: dict[str, object] = {}

    for source_name in SOURCE_NAMES:
        original, source_meta = load_source(source_name, manifest)
        sources[source_name] = source_meta
        images = {size: resize_long_edge(original, size).convert("RGBA") for size in SIZES}
        ref_size = (images[512].width, images[512].height)
        source_ref = np.asarray(images[512].convert("RGB"), dtype=np.float32) / 255.0
        regions = make_regions(source_ref)
        input_views = {size: common_view(images[size], ref_size) for size in SIZES}

        for preset_name, preset in PRESETS.items():
            views: dict[tuple[str, int], np.ndarray] = {}
            baselines: dict[int, np.ndarray] = {}
            for size in SIZES:
                image = images[size]
                off_settings = dict(preset)
                off_settings["grain"] = 0.0
                off = apply_retro(image, **off_settings, seed=TRAIN_SEEDS[0])
                off_view = common_view(off, ref_size)
                baselines[size] = off_view
                prior_row = prior_cal[(source_name, preset_name, size)]
                target = float(prior_row["target current-mono whole luma RMS"])
                prior_fit = prior_row["fits"]["zero-luma-chroma"]
                training_probe = prior_fit["probe responses by factor"]
                factor8_by_seed: dict[int, float] = {}
                for seed in TRAIN_SEEDS:
                    sigma = float(preset["grain"]) * PROBE_FACTOR
                    delta = make_deltas(image.height, image.width, sigma, seed)["zero-luma-chroma"]
                    prebyte = render_prebyte_float(image, preset, seed, grain_delta=delta)
                    out = resize_to_view(np.uint8(prebyte * 255.0), ref_size)
                    factor8_by_seed[seed] = float(output_metrics(
                        out, off_view, regions["whole image"])["metrics"]["linear Rec709 luma RMS"])
                responses = []
                for factor in (1.0, 2.0, 4.0):
                    values = training_probe[str(factor)]
                    responses.append((factor, float(np.median(values))))
                responses.append((PROBE_FACTOR, float(np.median(list(factor8_by_seed.values())))))
                fit = calibrate_factor(target, responses)
                calibration_records.append({"source": source_name, "preset": preset_name,
                                            "input long edge": size, "train seeds": list(TRAIN_SEEDS),
                                            "mono training target luma RMS": target,
                                            "factor 1/2/4/8 median output luma responses": {
                                                str(f): float(y) for f, y in responses},
                                            "factor 8 train responses by seed": {
                                                str(k): v for k, v in factor8_by_seed.items()},
                                            "fit": fit})

                for seed in VALIDATION_SEEDS:
                    mono_row = prior_validation[(source_name, preset_name, size, seed, "current-monochrome")]
                    mono_view = common_view(apply_retro(image, **preset, seed=seed), ref_size)
                    mono_metrics = {roi: output_metrics(mono_view, off_view, mask)
                                    for roi, mask in regions.items()}
                    held_target = float(mono_metrics["whole image"]["metrics"]["linear Rec709 luma RMS"])
                    validation_records.append({"source": source_name, "preset": preset_name,
                                               "input long edge": size, "seed": seed,
                                               "variant": "current-monochrome", "factor": 1.0,
                                               "target luma RMS": held_target,
                                               "prior matched-study target": mono_row["target current-mono whole luma RMS"],
                                               "ROI metrics": mono_metrics})
                    outputs: dict[str, np.ndarray] = {"current-mono": mono_view}
                    for label, factor in (("chroma-factor-4", 4.0),
                                          ("chroma-luma-matched", float(fit["factor"]))):
                        sigma = float(preset["grain"]) * factor
                        delta = make_deltas(image.height, image.width, sigma, seed)["zero-luma-chroma"]
                        prebyte = render_prebyte_float(image, preset, seed, grain_delta=delta)
                        out = resize_to_view(np.uint8(prebyte * 255.0), ref_size)
                        roi_metrics = {roi: output_metrics(out, off_view, mask)
                                       for roi, mask in regions.items()}
                        validation_records.append({"source": source_name, "preset": preset_name,
                                                   "input long edge": size, "seed": seed,
                                                   "variant": label, "factor": factor,
                                                   "calibration target luma RMS": target,
                                                   "held-out current mono target luma RMS": held_target,
                                                   "fit target bracketed": fit["target bracketed"],
                                                   "ROI metrics": roi_metrics})
                        outputs[label] = out
                    if seed == VALIDATION_DISPLAY_SEED:
                        for label, output in outputs.items():
                            views[(label, size)] = output
            save_sheet(source_name, preset_name, views, baselines, input_views)

    summary: dict[str, object] = {}
    for variant in ("current-monochrome", "chroma-factor-4", "chroma-luma-matched"):
        rows = [r for r in validation_records if r["variant"] == variant]
        ratios, chroma, endpoints = [], [], []
        for row in rows:
            whole = row["ROI metrics"]["whole image"]["metrics"]
            if whole:
                target_value = row["held-out current mono target luma RMS"] \
                    if "held-out current mono target luma RMS" in row else row["target luma RMS"]
                target = float(target_value)
                ratios.append(float(whole["linear Rec709 luma RMS"]) / max(target, 1e-12))
                chroma.append(float(whole["linear chroma-axis RMS"]))
                endpoints.append(float(whole["display RGB8 endpoint channel ratio"]))
        summary[variant] = {"validation cases": len(ratios),
                            "median luma RMS / held-out mono target": float(np.median(ratios)),
                            "median absolute luma target ratio error": float(np.median(np.abs(np.asarray(ratios) - 1))),
                            "median chroma-axis RMS": float(np.median(chroma)),
                            "median endpoint channel ratio": float(np.median(endpoints))}
    fit_count = sum(bool(row["fit"]["target bracketed"]) for row in calibration_records)
    report = {"study": "full-suite-photo-grain-chroma-headroom", "date": "2026-11-11",
              "environment": {"python": platform.python_version(), "numpy": np.__version__,
                              "pillow": Image.__version__},
              "plan": {"question": "Does extending zero-luma chroma grain to an 8x sigma bound reach the current mono output-luma RMS target, and what output chroma/headroom costs result?",
                       "prior calibration report": str(prior_path.relative_to(ROOT)),
                       "sources": sources, "presets": {k: dict(v) for k, v in PRESETS.items()},
                       "input long edges": list(SIZES), "training seeds": list(TRAIN_SEEDS),
                       "held-out seeds": list(VALIDATION_SEEDS), "new probe factor": PROBE_FACTOR,
                       "fit": "Reuse prior measured factor 1/2/4 training responses and add factor-8 response; piecewise-linear interpolate, never extrapolate. Hold-out candidates are evaluated against same-seed current-mono RMS.",
                       "endpoint metric": "Fraction of final RGB8 channels at code 0 or 255 in the common 512-display whole image; not a native pre-clamp attribution.",
                       "acceptance": "Descriptive only; no perceptual equivalence or quality threshold."},
              "groups": len(calibration_records), "bracketed calibration targets at <=8x": fit_count,
              "calibration records": calibration_records,
              "held-out aggregate": summary, "validation records": validation_records,
              "limitations": ["8x is a bounded exploratory probe, not a recommended grain range",
                              "RGB8 endpoint occupancy indicates display code endpoints and does not prove clipping originated in grain",
                              "whole-image luma match cannot equalize local response, chroma, perceived texture, or spatial spectrum",
                              "sRGB-code noise, transfer decode, quantization, clamp, and encoded-sRGB downsampling interact",
                              "no alpha/HDR/human preference/product runtime/Core/GPU/performance assessment"]}
    path = CAPTURE / "grain-chroma-headroom-study.json"
    path.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"report": str(path), "groups": len(calibration_records),
                      "targets bracketed <=8x": fit_count, "aggregate": summary}, indent=2))
    return report


if __name__ == "__main__":
    main()
