"""Compare adaptive strength transition policies at a known scene cut."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill, load_image
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(np.maximum(rgb, 0.0))
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="CC0 source video")
    parser.add_argument("--low-key", type=Path,
                        default=Path(__file__).resolve().parent / "assets" / "flash-photo-plate.png")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "exposure-cut")
    parser.add_argument("--cut-frame", type=int, default=16,
                        help="known cut index in the concatenated study sequence")
    parser.add_argument("--total-frames", type=int, default=32,
                        help="total sequence frames; low-key still fills frames after the cut")
    parser.add_argument("--reference-p95", type=float, default=0.15)
    parser.add_argument("--base-strength", type=float, default=2.3)
    parser.add_argument("--min-strength", type=float, default=0.4)
    parser.add_argument("--max-strength", type=float, default=2.3)
    parser.add_argument("--tau", type=float, default=0.75)
    parser.add_argument("--reset-ramps", nargs="+", type=float, default=(0.0, 0.1, 0.25, 0.5),
                        help="EMA time constants after the known cut; zero resets immediately")
    parser.add_argument("--max-dimension", type=int, default=360)
    args = parser.parse_args()
    if not args.input.exists() or not args.low_key.exists():
        parser.error("input video and low-key image must both exist")
    if args.total_frames < 4 or not (1 <= args.cut_frame < args.total_frames - 1):
        parser.error("cut-frame must leave at least two frames in each shot")
    if args.tau < 0.0 or args.reference_p95 <= 0.0:
        parser.error("tau must be nonnegative and reference-p95 positive")
    args.output.mkdir(parents=True, exist_ok=True)

    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS)) or 24.0
    capture.set(cv2.CAP_PROP_POS_FRAMES, 0)
    frames: list[np.ndarray] = []
    try:
        for _ in range(args.cut_frame):
            ok, bgr = capture.read()
            if not ok:
                break
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
            scale = min(1.0, args.max_dimension / max(rgb.shape[:2]))
            if scale < 1.0:
                rgb = cv2.resize(rgb, (round(rgb.shape[1] * scale), round(rgb.shape[0] * scale)),
                                 interpolation=cv2.INTER_AREA)
            frames.append(rgb)
    finally:
        capture.release()
    if len(frames) != args.cut_frame:
        raise OSError(f"Video supplied only {len(frames)} frames before requested cut")
    low = load_image(args.low_key)
    low_rgb = low[..., :3].astype(np.float32)
    if low_rgb.max() > 1.0:
        low_rgb /= 255.0
    scale = min(1.0, args.max_dimension / max(low_rgb.shape[:2]))
    if scale < 1.0:
        low_rgb = cv2.resize(low_rgb, (round(low_rgb.shape[1] * scale),
                                       round(low_rgb.shape[0] * scale)),
                             interpolation=cv2.INTER_AREA)
    frames.extend([low_rgb.copy() for _ in range(args.total_frames - args.cut_frame)])
    frame_count = len(frames)

    reset_ramps = sorted(set(max(0.0, float(tau)) for tau in args.reset_ramps))
    names = ["unsmoothed", "ema_no_reset"] + [f"cut_reset_tau_{tau:g}s" for tau in reset_ramps]
    previous = {name: None for name in names}
    records: dict[str, list[dict[str, float | int]]] = {name: [] for name in names}
    thumbnails: dict[str, list[np.ndarray]] = {name: [] for name in names}
    picks = {max(0, args.cut_frame - 2), args.cut_frame - 1, args.cut_frame,
             min(frame_count - 1, args.cut_frame + 1), frame_count - 1}
    for index, rgb in enumerate(frames):
        rgba = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
        p95 = float(np.percentile(luma(rgb), 95))
        target = float(np.clip(args.base_strength * args.reference_p95 / max(p95, 1.0e-6),
                               args.min_strength, args.max_strength))
        progress = index / max(1, frame_count - 1)
        for name in names:
            reset = name.startswith("cut_reset_tau_") and index == args.cut_frame
            prev = previous[name]
            if name == "unsmoothed" or prev is None or (reset and float(name.removeprefix("cut_reset_tau_").removesuffix("s")) == 0.0):
                strength = target
            else:
                tau = args.tau if name == "ema_no_reset" else float(
                    name.removeprefix("cut_reset_tau_").removesuffix("s"))
                alpha = 1.0 if tau <= 0.0 else 1.0 - np.exp(-1.0 / (fps * tau))
                strength = float(prev + alpha * (target - prev))
            previous[name] = strength
            result = flash_fill(
                rgba, progress, mode="radial", source_position=(0.50, 0.14),
                strength=strength, softness=0.19, temperature=(1.0, 0.94, 0.82),
                attack_duration=0.72, peak_time=0.78, decay_rate=3.0,
                scatter_strength=0.6, highlight_threshold=0.28, highlight_range=0.52,
                global_fill=0.02, front_duration=0.72, radial_falloff=2.7,
                highlight_rolloff=0.8)
            record: dict[str, float | int] = {
                "frame": index, "time_seconds": index / fps, "shot": 0 if index < args.cut_frame else 1,
                "known_cut_reset": int(reset), "source_p95_linear_luma": p95,
                "target_strength": target, "effective_strength": strength,
                "absolute_target_error": abs(strength - target),
            }
            if records[name]:
                record["adjacent_strength_delta"] = abs(
                    strength - float(records[name][-1]["effective_strength"]))
            records[name].append(record)
            if index in picks:
                thumb = cv2.resize(cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGR),
                                   (240, 400), interpolation=cv2.INTER_AREA)
                cv2.putText(thumb, f"{name} t={index / fps:.2f} s={strength:.2f}",
                            (5, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.42,
                            (255, 255, 255), 1, cv2.LINE_AA)
                thumbnails[name].append(thumb)

    summaries = []
    panels = []
    for name in names:
        rows = records[name]
        post = rows[args.cut_frame:]
        post_lag = [float(row["absolute_target_error"]) for row in post]
        summaries.append({
            "strategy": name,
            "post_cut_mean_abs_target_error": float(np.mean(post_lag)),
            "post_cut_first_frame_abs_target_error": post_lag[0],
            "post_cut_mean_strength": float(np.mean([float(r["effective_strength"]) for r in post])),
            "cut_strength_step": abs(float(rows[args.cut_frame]["effective_strength"])
                                      - float(rows[args.cut_frame - 1]["effective_strength"])),
            "first_post_cut_strength_step": float(rows[args.cut_frame]["adjacent_strength_delta"]),
            "first_post_cut_lag_fraction": float(rows[args.cut_frame]["absolute_target_error"])
                                           / max(1.0e-9, abs(float(rows[args.cut_frame]["target_strength"])
                                                             - float(rows[args.cut_frame - 1]["effective_strength"]))),
            "frames": rows,
        })
        tiles = thumbnails[name]
        header = np.full((30, 240 * len(tiles), 3), (22, 27, 37), np.uint8)
        cv2.putText(header, name, (8, 21), cv2.FONT_HERSHEY_SIMPLEX, 0.55,
                    (245, 245, 245), 1, cv2.LINE_AA)
        panels.append(np.vstack((header, np.hstack(tiles))))
    report = {
        "experiment": "adaptive flash strength transition across a known hard cut",
        "input_video": str(args.input.resolve()), "low_key_image": str(args.low_key.resolve()),
        "synthetic_cut_frame": args.cut_frame, "fps": fps, "tau_seconds": args.tau,
        "strength_rule": "clip(base_strength * reference_p95 / source_p95, min_strength, max_strength)",
        "reset_ramp_taus_seconds": reset_ramps,
        "limitations": [
            "The low-key still is repeated after a deliberately inserted hard cut.",
            "Reset timing uses the known cut index; automatic cut detection is not tested.",
            "Strength and clipping heuristics are controls, not aesthetic ground truth.",
            "The flash progress continues across the cut, isolating adaptation reset behavior.",
        ], "strategies": summaries,
    }
    cv2.imwrite(str(args.output / "flash-exposure-cut-comparison.png"), np.vstack(panels))
    (args.output / "flash-exposure-cut-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Saved cut-aware exposure study to {args.output.resolve()}")


if __name__ == "__main__":
    main()
