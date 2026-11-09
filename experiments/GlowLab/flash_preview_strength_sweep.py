"""Measure fixed flash-strength candidates across a video clip."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from flash_preset import DEFAULTS
from glow_lab import srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(np.maximum(rgb, 0.0))
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "preview-strength-sweep")
    parser.add_argument("--start-seconds", type=float, default=0.0)
    parser.add_argument("--end-seconds", type=float, default=4.0)
    parser.add_argument("--strengths", nargs="+", type=float, default=(0.4, 0.8, 1.2, 2.3))
    parser.add_argument("--max-dimension", type=int, default=640)
    args = parser.parse_args()
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if args.start_seconds < 0.0 or args.end_seconds <= args.start_seconds:
        parser.error("video range must be nonnegative and end after start")
    if args.max_dimension < 64:
        parser.error("max-dimension must be at least 64")
    strengths = sorted(set(max(0.0, float(value)) for value in args.strengths))
    if not strengths:
        parser.error("at least one strength is required")
    args.output.mkdir(parents=True, exist_ok=True)

    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS)) or 24.0
    declared_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    start_frame = max(0, int(round(args.start_seconds * fps)))
    end_frame = min(declared_count - 1, int(round(args.end_seconds * fps)))
    frame_count = end_frame - start_frame + 1
    if frame_count < 1:
        capture.release()
        parser.error("selected range contains no frames")
    width0 = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    height0 = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    scale = min(1.0, args.max_dimension / max(width0, height0))
    width, height = max(1, round(width0 * scale)), max(1, round(height0 * scale))
    sample_indices = sorted(set(round(v * (frame_count - 1) / 4) for v in range(5)))
    per_strength: dict[float, dict[str, list[float]]] = {
        strength: {"clip": [], "luma_delta": []} for strength in strengths
    }
    source_thumbnails: dict[int, np.ndarray] = {}
    result_thumbnails: dict[float, dict[int, np.ndarray]] = {
        strength: {} for strength in strengths
    }
    duration = max((frame_count - 1) / fps, 1.0 / fps)
    capture.set(cv2.CAP_PROP_POS_FRAMES, start_frame)
    try:
        for index in range(frame_count):
            ok, bgr = capture.read()
            if not ok:
                frame_count = index
                break
            if bgr.shape[1] != width or bgr.shape[0] != height:
                bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
            source = np.dstack((rgb, np.ones((height, width), np.float32)))
            source_luma = linear_luma(rgb)
            progress = index / max(frame_count - 1, 1)
            for strength in strengths:
                result = flash_fill(
                    source, progress, mode=DEFAULTS["mode"],
                    source_position=tuple(DEFAULTS["source_position"]), strength=strength,
                    softness=DEFAULTS["softness"], temperature=tuple(DEFAULTS["temperature"]),
                    attack_duration=DEFAULTS["attack_duration"],
                    peak_time=DEFAULTS["peak_time"],
                    decay_rate=DEFAULTS["decay_rate_per_second"] * duration,
                    scatter_strength=DEFAULTS["scatter"],
                    highlight_threshold=DEFAULTS["highlight_threshold"],
                    highlight_range=DEFAULTS["highlight_range"],
                    global_fill=DEFAULTS["global_fill"],
                    front_duration=DEFAULTS["front_duration"],
                    radial_falloff=DEFAULTS["radial_falloff"],
                    highlight_rolloff=DEFAULTS["highlight_rolloff"])
                per_strength[strength]["clip"].append(float(np.mean(
                    (result[..., :3] >= 1.0) & (rgb < 1.0))))
                per_strength[strength]["luma_delta"].append(float(
                    (linear_luma(result[..., :3]) - source_luma).mean()))
                if index in sample_indices:
                    thumb = cv2.resize(cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGR),
                                       (240, max(1, round(height * 240 / width))),
                                       interpolation=cv2.INTER_AREA)
                    result_thumbnails[strength][index] = thumb
            if index in sample_indices:
                source_thumbnails[index] = cv2.resize(
                    cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGR),
                    (240, max(1, round(height * 240 / width))), interpolation=cv2.INTER_AREA)
    finally:
        capture.release()

    summaries = []
    rows = []
    for label, strength, thumbnails in [("SOURCE", None, source_thumbnails)] + [
            (f"STRENGTH {value:g}", value, result_thumbnails[value]) for value in strengths]:
        cells = []
        for index in sample_indices:
            thumb = thumbnails.get(index)
            if thumb is None:
                thumb = np.zeros((max(1, round(height * 240 / width)), 240, 3), np.uint8)
            cells.append(thumb)
        header = np.full((30, 240 * len(cells), 3), (22, 27, 37), np.uint8)
        cv2.putText(header, label, (8, 21), cv2.FONT_HERSHEY_SIMPLEX, 0.48,
                    (245, 245, 245), 1, cv2.LINE_AA)
        rows.append(np.vstack((header, np.hstack(cells))))
    report = {
        "experiment": "fixed preview strength sweep",
        "input_video": str(args.input.resolve()),
        "source_frame_range": [start_frame, start_frame + frame_count - 1],
        "frame_count": frame_count,
        "fps": fps,
        "preview_defaults": DEFAULTS,
        "strengths": [{
            "strength": strength,
            "mean_newly_clipped_channel_fraction": float(np.mean(values["clip"])),
            "max_newly_clipped_channel_fraction": float(np.max(values["clip"])),
            "frames_over_8_percent_new_clipping": int(np.sum(np.asarray(values["clip"]) > 0.08)),
            "mean_linear_luma_delta": float(np.mean(values["luma_delta"])),
            "per_frame_newly_clipped_channel_fraction": values["clip"],
        } for strength, values in per_strength.items()],
        "limitations": [
            "The fixture is one bright winter walkway shot and does not establish a universal preset.",
            "Newly clipped channel fraction is a highlight-preservation proxy, not an aesthetic score.",
            "Reduced-resolution SDR output does not validate HDR or camera-calibrated flash exposure.",
        ],
    }
    (args.output / "flash-preview-strength-sweep-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    cv2.imwrite(str(args.output / "flash-preview-strength-sweep-contact-sheet.png"), np.vstack(rows))
    print(f"Saved preview strength sweep to {args.output.resolve()}")


if __name__ == "__main__":
    main()
