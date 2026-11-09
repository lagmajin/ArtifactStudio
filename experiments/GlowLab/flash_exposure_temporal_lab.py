"""Study temporal smoothing for source-luminance-adaptive flash strength."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(np.maximum(rgb, 0.0))
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "exposure-temporal")
    parser.add_argument("--start-seconds", type=float, default=0.0)
    parser.add_argument("--end-seconds", type=float, default=2.0)
    parser.add_argument("--exposure-ramp-stops", type=float, default=2.0,
                        help="synthetic linear exposure ramp from +N/2 to -N/2 stops")
    parser.add_argument("--reference-p95", type=float, default=0.15,
                        help="linear-luma p95 that maps to base strength")
    parser.add_argument("--base-strength", type=float, default=2.3)
    parser.add_argument("--min-strength", type=float, default=0.4)
    parser.add_argument("--max-strength", type=float, default=2.3)
    parser.add_argument("--taus", nargs="+", type=float, default=(0.0, 0.25, 0.75),
                        help="EMA time constants in seconds; zero means unsmoothed")
    parser.add_argument("--max-dimension", type=int, default=480)
    args = parser.parse_args()
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if args.max_dimension < 64:
        parser.error("max-dimension must be at least 64")
    if args.end_seconds <= args.start_seconds or args.start_seconds < 0.0:
        parser.error("video range must be nonnegative and end after start")
    if args.reference_p95 <= 0.0:
        parser.error("reference-p95 must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS))
    if fps <= 0.0:
        fps = 24.0
    declared_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    start_frame = max(0, int(round(args.start_seconds * fps)))
    end_frame = min(declared_count - 1, int(round(args.end_seconds * fps)))
    frame_count = end_frame - start_frame + 1
    if frame_count < 2:
        capture.release()
        parser.error("selected video range must contain at least two frames")
    duration = (frame_count - 1) / fps
    source_w = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    source_h = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    scale = min(1.0, args.max_dimension / max(source_w, source_h))
    width, height = max(1, round(source_w * scale)), max(1, round(source_h * scale))
    taus = sorted(set(max(0.0, float(tau)) for tau in args.taus))
    if not taus:
        capture.release()
        parser.error("taus must contain at least one nonnegative value")
    strategies: dict[float, dict[str, object]] = {
        tau: {"previous_strength": None, "frames": [], "thumbnails": []}
        for tau in taus
    }
    sample_indices = {0, frame_count // 4, frame_count // 2,
                      (3 * frame_count) // 4, frame_count - 1}
    capture.set(cv2.CAP_PROP_POS_FRAMES, start_frame)
    try:
        for index in range(frame_count):
            ok, bgr = capture.read()
            if not ok:
                break
            if bgr.shape[1] != width or bgr.shape[0] != height:
                bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
            source = np.dstack((rgb, np.ones((height, width), np.float32)))
            source_linear = srgb_to_linear(source[..., :3])
            progress = index / (frame_count - 1)
            ramp_stops = (0.5 - progress) * args.exposure_ramp_stops
            ramp_gain = 2.0 ** ramp_stops
            exposed_linear = source_linear * ramp_gain
            exposed_rgb = np.maximum(linear_to_srgb(exposed_linear), 0.0)
            exposed = np.dstack((exposed_rgb, source[..., 3]))
            exposed_luma = linear_luma(exposed_rgb)
            source_p95 = float(np.percentile(exposed_luma, 95))
            target_strength = float(np.clip(
                args.base_strength * args.reference_p95 / max(source_p95, 1.0e-6),
                args.min_strength, args.max_strength))
            for tau, state in strategies.items():
                previous = state["previous_strength"]
                if previous is None or tau <= 0.0:
                    strength = target_strength
                else:
                    alpha = 1.0 - np.exp(-1.0 / (fps * tau))
                    strength = float(previous) + alpha * (target_strength - float(previous))
                state["previous_strength"] = strength
                result = flash_fill(
                    exposed, progress, mode="radial", source_position=(0.50, 0.14),
                    strength=strength, softness=0.19, temperature=(1.0, 0.94, 0.82),
                    attack_duration=0.72, peak_time=0.78, decay_rate=3.0 * duration,
                    scatter_strength=0.6, highlight_threshold=0.28, highlight_range=0.52,
                    global_fill=0.02, front_duration=0.72, radial_falloff=2.7,
                    highlight_rolloff=0.8)
                # Compare against the same display-referred input baseline as the output.
                source_luma = linear_luma(np.clip(exposed[..., :3], 0.0, 1.0))
                output_luma = linear_luma(result[..., :3])
                flash_delta = output_luma - source_luma
                frame_metrics: dict[str, float | int] = {
                    "frame": index,
                    "source_frame": start_frame + index,
                    "time_seconds": index / fps,
                    "progress": progress,
                    "exposure_ramp_stops": float(ramp_stops),
                    "synthetic_exposure_gain": float(ramp_gain),
                    "source_p95_linear_luma": source_p95,
                    "target_strength": target_strength,
                    "effective_strength": strength,
                    "mean_flash_luma_delta": float(flash_delta.mean()),
                    "newly_clipped_channel_fraction": float(np.mean(
                        (result[..., :3] >= 1.0) & (exposed[..., :3] < 1.0))),
                }
                frames = state["frames"]
                assert isinstance(frames, list)
                if frames:
                    previous_frame = frames[-1]
                    frame_metrics["adjacent_strength_delta"] = abs(
                        strength - float(previous_frame["effective_strength"]))
                    frame_metrics["adjacent_flash_delta_change"] = float(np.mean(
                        np.abs(flash_delta - state["previous_flash_delta"])))
                frames.append(frame_metrics)
                state["previous_flash_delta"] = flash_delta
                if index in sample_indices:
                    thumb = cv2.resize(cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGR),
                                       (320, max(1, round(height * 320 / width))),
                                       interpolation=cv2.INTER_AREA)
                    cv2.putText(thumb, f"t={progress:.2f} s={strength:.2f}",
                                (7, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.45,
                                (255, 255, 255), 1, cv2.LINE_AA)
                    thumbnails = state["thumbnails"]
                    assert isinstance(thumbnails, list)
                    thumbnails.append(thumb)
    finally:
        capture.release()

    report: dict[str, object] = {
        "experiment": "source-luminance strength adaptation with temporal EMA",
        "input_video": str(args.input.resolve()),
        "source_frame_range": [start_frame, end_frame],
        "decoded_frame_count": frame_count,
        "fps": fps,
        "duration_seconds": duration,
        "synthetic_exposure_ramp_total_stops": args.exposure_ramp_stops,
        "strength_rule": "clip(base_strength * reference_p95 / source_p95, min_strength, max_strength)",
        "base_strength": args.base_strength,
        "reference_p95": args.reference_p95,
        "min_strength": args.min_strength,
        "max_strength": args.max_strength,
        "limitations": [
            "The exposure ramp is synthetic and is applied in linear light to the source frames.",
            "The p95 mapping is a heuristic and the selected strength is not an aesthetic optimum.",
            "EMA may lag real illumination changes; cuts are not detected or reset.",
            "The analysis range is short and uses one CC0 high-key clip.",
        ],
        "strategies": [],
    }
    contact_panels: list[np.ndarray] = []
    for tau, state in strategies.items():
        frames = state["frames"]
        thumbnails = state["thumbnails"]
        strength_values = [float(frame["effective_strength"]) for frame in frames]
        target_strengths = [float(frame["target_strength"]) for frame in frames]
        flash_deltas = [float(frame["mean_flash_luma_delta"]) for frame in frames]
        adjacent_strength = [float(frame["adjacent_strength_delta"])
                             for frame in frames if "adjacent_strength_delta" in frame]
        adjacent_flash = [float(frame["adjacent_flash_delta_change"])
                          for frame in frames if "adjacent_flash_delta_change" in frame]
        report["strategies"].append({
            "tau_seconds": tau,
            "strength_mean": float(np.mean(strength_values)),
            "strength_min": float(np.min(strength_values)),
            "strength_max": float(np.max(strength_values)),
            "mean_abs_target_strength_error": float(np.mean(
                np.abs(np.asarray(strength_values) - np.asarray(target_strengths)))),
            "mean_adjacent_strength_delta": float(np.mean(adjacent_strength)) if adjacent_strength else 0.0,
            "max_adjacent_strength_delta": float(np.max(adjacent_strength)) if adjacent_strength else 0.0,
            "mean_adjacent_flash_delta_change": float(np.mean(adjacent_flash)) if adjacent_flash else 0.0,
            "max_adjacent_flash_delta_change": float(np.max(adjacent_flash)) if adjacent_flash else 0.0,
            "mean_flash_luma_delta": float(np.mean(flash_deltas)),
            "frames": frames,
        })
        thumb_width = 320
        header = np.full((36, thumb_width * len(thumbnails), 3), (22, 27, 37), np.uint8)
        cv2.putText(header, f"EMA TAU {tau:.2f}s", (10, 24), cv2.FONT_HERSHEY_SIMPLEX,
                    0.52, (245, 245, 245), 1, cv2.LINE_AA)
        contact_panels.append(np.vstack((header, np.hstack(thumbnails))))
    cv2.imwrite(str(args.output / "flash-exposure-temporal-comparison.png"),
                np.vstack(contact_panels))
    with (args.output / "flash-exposure-temporal-metrics.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved temporal exposure adaptation study to {args.output.resolve()}")


if __name__ == "__main__":
    main()
