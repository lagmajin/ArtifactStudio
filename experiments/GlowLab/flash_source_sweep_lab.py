"""Sweep flash source position and color on one frame from a video clip."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from glow_lab import srgb_to_linear, to_u8


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "source-sweep")
    parser.add_argument("--progress", type=float, default=0.68)
    parser.add_argument("--strength", type=float, default=2.3)
    parser.add_argument("--decay-rate-per-second", type=float, default=3.0)
    parser.add_argument("--highlight-rolloff", type=float, default=1.0)
    args = parser.parse_args()
    cap = cv2.VideoCapture(str(args.input))
    if not cap.isOpened():
        parser.error(f"could not open video: {args.input}")
    frame_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = float(cap.get(cv2.CAP_PROP_FPS))
    duration = (frame_count - 1) / fps if fps > 0 and frame_count > 1 else 1.0
    index = int(round(np.clip(args.progress, 0.0, 1.0) * max(frame_count - 1, 0)))
    cap.set(cv2.CAP_PROP_POS_FRAMES, index)
    ok, bgr = cap.read()
    cap.release()
    if not ok:
        raise OSError(f"Could not read source frame {index} from {args.input}")
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    source = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
    args.output.mkdir(parents=True, exist_ok=True)
    positions = ((0.12, 0.50), (0.50, 0.14), (0.88, 0.50))
    colors = (("WARM", (1.0, 0.84, 0.62)),
              ("NEUTRAL", (1.0, 1.0, 1.0)),
              ("COOL", (0.66, 0.82, 1.0)))
    rows: list[np.ndarray] = []
    report: dict[str, object] = {
        "input_video": str(args.input.resolve()),
        "source_frame": index,
        "source_time_seconds": index / fps if fps > 0 else None,
        "progress": args.progress,
        "strength": args.strength,
        "decay_rate_per_second": args.decay_rate_per_second,
        "highlight_rolloff": args.highlight_rolloff,
        "variants": [],
    }
    for position in positions:
        cells = []
        for color_name, temperature in colors:
            result = flash_fill(
                source, args.progress, mode="radial", source_position=position,
                strength=args.strength, softness=0.19, temperature=temperature,
                attack_duration=0.72, peak_time=0.78,
                decay_rate=args.decay_rate_per_second * duration,
                scatter_strength=0.6, highlight_threshold=0.28, highlight_range=0.52,
                global_fill=0.03, front_duration=0.72, radial_falloff=2.7,
                highlight_rolloff=args.highlight_rolloff)
            assert np.isfinite(result).all()
            assert np.array_equal(result[..., 3], source[..., 3])
            linear = srgb_to_linear(result[..., :3])
            source_linear = srgb_to_linear(source[..., :3])
            luma = linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722
            source_luma = (source_linear[..., 0] * 0.2126 + source_linear[..., 1] * 0.7152 +
                           source_linear[..., 2] * 0.0722)
            report["variants"].append({
                "source_position": list(position),
                "temperature_name": color_name,
                "temperature_rgb": list(temperature),
                "mean_linear_luma_delta": float((luma - source_luma).mean()),
                "clipped_channel_fraction": float(np.mean(result[..., :3] >= 1.0)),
                "mean_linear_chroma_delta": float(np.mean(
                    (np.max(linear, axis=2) - np.min(linear, axis=2)) -
                    (np.max(source_linear, axis=2) - np.min(source_linear, axis=2)))),
            })
            output = cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA)
            thumb = cv2.resize(output, (360, int(round(output.shape[0] * 360 / output.shape[1]))),
                               interpolation=cv2.INTER_AREA)
            header = np.full((42, thumb.shape[1], 4), (22, 27, 37, 255), np.uint8)
            cv2.putText(header, color_name, (10, 28), cv2.FONT_HERSHEY_SIMPLEX,
                        0.53, (245, 245, 245, 255), 1, cv2.LINE_AA)
            cells.append(np.vstack((header, thumb)))
            name = f"source-{position[0]:.2f}-{position[1]:.2f}-{color_name.lower()}.png"
            cv2.imwrite(str(args.output / name), output)
        position_header = np.full((34, sum(c.shape[1] for c in cells), 4),
                                  (34, 39, 51, 255), np.uint8)
        cv2.putText(position_header, f"SOURCE POSITION  {position[0]:.2f}, {position[1]:.2f}",
                    (12, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.48,
                    (232, 236, 242, 255), 1, cv2.LINE_AA)
        rows.append(np.vstack((position_header, np.hstack(cells))))
    cv2.imwrite(str(args.output / "flash-source-color-sweep.png"), np.vstack(rows))
    with (args.output / "flash-source-color-sweep.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved source/color sweep to {args.output.resolve()}")


if __name__ == "__main__":
    main()
