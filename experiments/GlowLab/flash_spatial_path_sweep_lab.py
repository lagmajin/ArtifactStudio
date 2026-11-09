"""Compare flash-fill spatial paths over representative frames of a video."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from flash_preset import DEFAULTS
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(np.maximum(rgb, 0.0))
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "spatial-path-sweep")
    parser.add_argument("--start-seconds", type=float, default=0.0)
    parser.add_argument("--end-seconds", type=float, default=4.0)
    parser.add_argument("--strength", type=float, default=0.8)
    parser.add_argument("--synthetic-exposure-stops", type=float, default=0.0,
                        help="uniform linear-light source exposure offset; does not simulate sensor noise")
    parser.add_argument("--max-dimension", type=int, default=720)
    args = parser.parse_args()
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if args.start_seconds < 0.0 or args.end_seconds <= args.start_seconds:
        parser.error("video range must be nonnegative and end after start")
    if args.max_dimension < 64 or args.strength < 0.0:
        parser.error("max-dimension must be at least 64 and strength must be nonnegative")
    args.output.mkdir(parents=True, exist_ok=True)

    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS)) or 24.0
    declared_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    start_frame = max(0, int(round(args.start_seconds * fps)))
    end_frame = min(declared_count - 1, int(round(args.end_seconds * fps)))
    frame_count = end_frame - start_frame + 1
    if frame_count < 2:
        capture.release()
        parser.error("selected range must contain at least two frames")
    width0 = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height0 = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    scale = min(1.0, args.max_dimension / max(width0, height0))
    width = max(1, round(width0 * scale))
    height = max(1, round(height0 * scale))
    progress_values = (0.18, 0.36, 0.54, 0.72, 0.90)
    sample_indices = [round(value * (frame_count - 1)) for value in progress_values]
    variants = [
        {"name": "RADIAL / DEFAULT TOP-CENTER", "mode": "radial", "source_position": (0.50, 0.14)},
        {"name": "RADIAL / LEFT-MID", "mode": "radial", "source_position": (0.10, 0.48)},
        {"name": "RADIAL / LOWER-LEFT", "mode": "radial", "source_position": (0.08, 0.84)},
        {"name": "DIRECTIONAL / LEFT TO RIGHT", "mode": "directional", "direction": (1.0, 0.0)},
        {"name": "DIRECTIONAL / TOP TO BOTTOM", "mode": "directional", "direction": (0.0, 1.0)},
    ]
    thumb_width = 288
    thumb_height = max(1, round(height * thumb_width / width))
    row_images: dict[str, list[np.ndarray]] = {"SOURCE": []}
    metrics: dict[str, list[dict[str, float | int]]] = {variant["name"]: [] for variant in variants}
    for variant in variants:
        row_images[str(variant["name"])] = []
    duration = max((frame_count - 1) / fps, 1.0 / fps)
    try:
        for progress, relative_index in zip(progress_values, sample_indices):
            capture.set(cv2.CAP_PROP_POS_FRAMES, start_frame + relative_index)
            ok, bgr = capture.read()
            if not ok:
                raise OSError(f"Could not read frame {start_frame + relative_index}")
            if bgr.shape[1] != width or bgr.shape[0] != height:
                bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
            if args.synthetic_exposure_stops != 0.0:
                gain = 2.0 ** args.synthetic_exposure_stops
                rgb = np.clip(linear_to_srgb(srgb_to_linear(rgb) * gain), 0.0, 1.0)
            source = np.dstack((rgb, np.ones((height, width), np.float32)))
            source_luma = linear_luma(rgb)
            source_thumb = cv2.resize(cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGR),
                                      (thumb_width, thumb_height), interpolation=cv2.INTER_AREA)
            row_images["SOURCE"].append(source_thumb)
            for variant in variants:
                kwargs = {
                    "mode": variant["mode"],
                    "strength": args.strength,
                    "softness": DEFAULTS["softness"],
                    "temperature": tuple(DEFAULTS["temperature"]),
                    "attack_duration": DEFAULTS["attack_duration"],
                    "peak_time": DEFAULTS["peak_time"],
                    "decay_rate": DEFAULTS["decay_rate_per_second"] * duration,
                    "scatter_strength": DEFAULTS["scatter"],
                    "highlight_threshold": DEFAULTS["highlight_threshold"],
                    "highlight_range": DEFAULTS["highlight_range"],
                    "global_fill": DEFAULTS["global_fill"],
                    "front_duration": DEFAULTS["front_duration"],
                    "radial_falloff": DEFAULTS["radial_falloff"],
                    "highlight_rolloff": DEFAULTS["highlight_rolloff"],
                }
                if variant["mode"] == "radial":
                    kwargs["source_position"] = variant["source_position"]
                else:
                    kwargs["direction"] = variant["direction"]
                result = flash_fill(source, progress, **kwargs)
                result_luma = linear_luma(result[..., :3])
                delta = result_luma - source_luma
                metrics[str(variant["name"])].append({
                    "progress": progress,
                    "source_frame": start_frame + relative_index,
                    "time_seconds": (start_frame + relative_index) / fps,
                    "mean_linear_luma_delta": float(delta.mean()),
                    "front_coverage_over_0p01_delta": float(np.mean(delta > 0.01)),
                    "newly_clipped_channel_fraction": float(np.mean(
                        (result[..., :3] >= 1.0) & (rgb < 1.0))),
                })
                result_thumb = cv2.resize(cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGR),
                                          (thumb_width, thumb_height), interpolation=cv2.INTER_AREA)
                row_images[str(variant["name"])].append(result_thumb)
    finally:
        capture.release()

    contact_rows = []
    for name, thumbs in row_images.items():
        cells = []
        for progress, thumb in zip(progress_values, thumbs):
            header = np.full((27, thumb_width, 3), (22, 27, 37), np.uint8)
            cv2.putText(header, f"t={progress:.2f}", (8, 19), cv2.FONT_HERSHEY_SIMPLEX,
                        0.42, (245, 245, 245), 1, cv2.LINE_AA)
            cells.append(np.vstack((header, thumb)))
        row = np.hstack(cells)
        label = np.full((30, row.shape[1], 3), (16, 19, 26), np.uint8)
        cv2.putText(label, name, (8, 21), cv2.FONT_HERSHEY_SIMPLEX,
                    0.48, (245, 245, 245), 1, cv2.LINE_AA)
        contact_rows.append(np.vstack((label, row)))
    contact_sheet = np.vstack(contact_rows)
    contact_path = args.output / "flash-spatial-path-sweep-contact-sheet.png"
    if not cv2.imwrite(str(contact_path), contact_sheet):
        raise OSError(f"Could not write contact sheet: {contact_path}")
    report = {
        "experiment": "spatial path sweep on real CC0 video frames",
        "input_video": str(args.input.resolve()),
        "source_frame_range": [start_frame, end_frame],
        "frame_count": frame_count,
        "fps": fps,
        "strength": args.strength,
        "synthetic_exposure_stops": args.synthetic_exposure_stops,
        "preview_defaults": DEFAULTS,
        "progress_samples": progress_values,
        "variants": [
            {**{key: value for key, value in variant.items()},
             "samples": metrics[str(variant["name"])]}
            for variant in variants
        ],
        "limitations": [
            "The CC0 fixture is one bright winter walkway shot and contains no flash ground truth.",
            "The same image-space parameters are used for every frame; camera and scene geometry are not reconstructed.",
            "Exposure and coverage metrics do not rank aesthetic realism.",
            "Sampled progress and video time advance together, so this is a matched-frame look comparison, not temporal flicker analysis.",
            "Synthetic exposure offsets adjust linear image values uniformly and do not simulate sensor noise or real low-light color response.",
        ],
    }
    metrics_path = args.output / "flash-spatial-path-sweep-metrics.json"
    metrics_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Saved spatial path sweep to {args.output.resolve()}")


if __name__ == "__main__":
    main()
