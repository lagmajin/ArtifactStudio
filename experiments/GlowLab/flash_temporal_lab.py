"""Compare flash decay envelopes at fixed real-time duration and frame rate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill, load_image
from glow_lab import srgb_to_linear, to_u8


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path,
                        default=Path(__file__).resolve().parent / "assets" / "flash-photo-plate.png")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "temporal")
    parser.add_argument("--duration-seconds", type=float, default=1.0)
    parser.add_argument("--fps", type=float, default=24.0)
    args = parser.parse_args()
    if args.duration_seconds <= 0.0 or args.fps <= 0.0:
        parser.error("duration and fps must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    source = load_image(args.input)
    max_width = 768
    if source.shape[1] > max_width:
        scale = max_width / source.shape[1]
        source = cv2.resize(source, (max_width, int(round(source.shape[0] * scale))),
                            interpolation=cv2.INTER_AREA)
    frame_count = max(2, int(round(args.duration_seconds * args.fps)) + 1)
    actual_fps = (frame_count - 1) / args.duration_seconds
    times = np.linspace(0.0, 1.0, frame_count, dtype=np.float32)
    rates = (3.0, 8.0, 18.0)
    selected = sorted(set(int(round(t * (frame_count - 1))) for t in (0.0, 0.5, 0.68, 0.72, 0.76, 0.82, 0.92, 1.0)))
    rows: list[np.ndarray] = []
    report: dict[str, object] = {
        "input_image": str(args.input.resolve()),
        "input_dimensions": {"width": int(source.shape[1]), "height": int(source.shape[0])},
        "duration_seconds": args.duration_seconds,
        "nominal_fps": args.fps,
        "effective_fps": actual_fps,
        "frame_count": frame_count,
        "attack_duration": 0.72,
        "peak_time": 0.72,
        "front_duration": 0.68,
        "decay_rates_per_second": [],
    }
    for rate in rates:
        frames: list[np.ndarray] = []
        means: list[float] = []
        for progress in times:
            frame = flash_fill(
                source, float(progress), mode="radial", source_position=(0.04, 0.52),
                strength=4.2, softness=0.19, attack_duration=0.72,
                peak_time=0.72, decay_rate=rate * args.duration_seconds,
                scatter_strength=1.0, highlight_threshold=0.28, highlight_range=0.52,
                global_fill=0.06, front_duration=0.68, radial_falloff=2.4)
            assert np.isfinite(frame).all()
            assert np.array_equal(frame[..., 3], source[..., 3])
            frames.append(frame)
            linear = srgb_to_linear(frame[..., :3])
            luma = linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722
            means.append(float(luma.mean()))
        pair_delta = np.diff(np.asarray(means))
        peak_index = int(np.argmax(means))
        after_peak = pair_delta[peak_index:] if peak_index < len(pair_delta) else np.asarray([])
        rate_entry: dict[str, object] = {
            "decay_rate_per_second": rate,
            "mean_linear_luma_by_frame": means,
            "frame_mean_linear_luma_delta": pair_delta.tolist(),
            "peak_frame": peak_index,
            "peak_time_seconds": float(times[peak_index]) * args.duration_seconds,
            "largest_mean_frame_drop_after_peak": float(max(0.0, -float(after_peak.min()))) if after_peak.size else 0.0,
            "mean_peak_to_end_drop": means[peak_index] - means[-1],
            "frames": [],
        }
        panels = []
        for index in selected:
            image = cv2.cvtColor(to_u8(frames[index]), cv2.COLOR_RGBA2BGRA)
            thumb = cv2.resize(image, (480, int(round(image.shape[0] * 480 / image.shape[1]))),
                               interpolation=cv2.INTER_AREA)
            header = np.full((36, thumb.shape[1], 4), (22, 27, 37, 255), dtype=np.uint8)
            seconds = float(times[index]) * args.duration_seconds
            cv2.putText(header, f"{seconds:.2f}s  /  frame {index}", (12, 25),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.48, (245, 245, 245, 255), 1, cv2.LINE_AA)
            panels.append(np.vstack((header, thumb)))
            out_path = args.output / f"decay-{rate:g}" / f"flash-frame-{index:04d}.png"
            out_path.parent.mkdir(parents=True, exist_ok=True)
            cv2.imwrite(str(out_path), image)
            rate_entry["frames"].append({"index": index, "time_seconds": seconds,
                                         "mean_linear_luma": means[index]})
        rows.append(np.hstack(panels))
        report["decay_rates_per_second"].append(rate_entry)
    cv2.imwrite(str(args.output / "flash-decay-comparison.png"), np.vstack(rows))
    with (args.output / "flash-decay-metrics.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved temporal flash study to {args.output.resolve()}")


if __name__ == "__main__":
    main()
