"""Compare distance falloff and highlight-scatter controls on one video frame."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


FALLOFF_VALUES = (0.0, 2.7, 7.0)
HIGHLIGHT_PROFILES = (
    ("FIXED-BROAD", "fixed", 0.12, 0.75, None),
    ("FIXED-BALANCED", "fixed", 0.28, 0.52, None),
    ("FIXED-SELECTIVE", "fixed", 0.50, 0.30, None),
    ("REL-P70", "relative", 0.0, 0.0, (70.0, 95.0)),
    ("REL-P85", "relative", 0.0, 0.0, (85.0, 99.0)),
    ("REL-P95", "relative", 0.0, 0.0, (95.0, 99.8)),
)


def read_frame(path: Path, frame_index: int, max_dimension: int) -> tuple[np.ndarray, float]:
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {path}")
    try:
        frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
        fps = float(capture.get(cv2.CAP_PROP_FPS))
        if frame_index < 0 or frame_index >= frame_count:
            raise ValueError(f"frame-index {frame_index} outside video frame range 0..{frame_count - 1}")
        capture.set(cv2.CAP_PROP_POS_FRAMES, frame_index)
        ok, bgr = capture.read()
        if not ok:
            raise OSError(f"Could not read frame {frame_index} from {path}")
    finally:
        capture.release()
    scale = min(1.0, max_dimension / max(bgr.shape[:2]))
    if scale < 1.0:
        bgr = cv2.resize(bgr, (round(bgr.shape[1] * scale), round(bgr.shape[0] * scale)),
                         interpolation=cv2.INTER_AREA)
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    return np.dstack((rgb, np.ones(rgb.shape[:2], np.float32))), fps


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--frame-index", type=int, default=24)
    parser.add_argument("--progress", type=float, default=0.56)
    parser.add_argument("--exposure-stops", nargs="+", type=float, default=(0.0, -2.0),
                        help="linear-light exposure offsets to compare")
    parser.add_argument("--max-dimension", type=int, default=960)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "response-knob-sweep")
    args = parser.parse_args()
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if not 0.0 <= args.progress <= 1.0 or args.max_dimension < 64:
        parser.error("progress must be within 0..1 and max-dimension at least 64")
    args.output.mkdir(parents=True, exist_ok=True)
    base_source, fps = read_frame(args.input, args.frame_index, args.max_dimension)
    height, width = base_source.shape[:2]
    source_position = (0.12, 0.50)
    thumbnail_width = min(300, width)
    thumbnail_height = round(height * thumbnail_width / width)
    report: dict[str, object] = {
        "input_video": str(args.input.resolve()),
        "frame_index": args.frame_index,
        "source_fps": fps,
        "progress": args.progress,
        "strength_stops": 0.8,
        "source_position": source_position,
        "exposure_model": "uniform linear-light scale only; sensor noise is not simulated",
        "variants": [],
    }

    for stops in args.exposure_stops:
        source = base_source.copy()
        source[..., :3] = np.clip(
            linear_to_srgb(srgb_to_linear(source[..., :3]) * (2.0 ** stops)), 0.0, 1.0)
        source_linear = srgb_to_linear(source[..., :3])
        source_y = linear_luma(source_linear)
        source_bgr = cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGR)
        cv2.imwrite(str(args.output / f"source-ev_{'plus' if stops >= 0 else 'minus'}{abs(stops):.1f}.png"),
                    source_bgr)

        percentiles = [float(np.percentile(source_y, percentile))
                       for percentile in (70.0, 85.0, 95.0, 99.0, 99.8)]
        row_cards: list[list[np.ndarray]] = [[] for _ in HIGHLIGHT_PROFILES]
        for row, (profile_name, profile_mode, fixed_threshold, fixed_range,
                  percentile_pair) in enumerate(HIGHLIGHT_PROFILES):
            if percentile_pair is None:
                threshold, highlight_range = fixed_threshold, fixed_range
            else:
                threshold = float(np.percentile(source_y, percentile_pair[0]))
                upper = float(np.percentile(source_y, percentile_pair[1]))
                highlight_range = max(0.01, upper - threshold)
            for column, falloff in enumerate(FALLOFF_VALUES):
                common = dict(
                    mode="radial", source_position=source_position, strength=0.8,
                    softness=0.19, temperature=(1.0, 0.94, 0.82),
                    attack_duration=0.72, peak_time=0.78,
                    decay_rate=4.0, highlight_threshold=threshold,
                    highlight_range=highlight_range, global_fill=0.03,
                    front_duration=0.72, radial_falloff=falloff,
                    highlight_rolloff=1.0,
                )
                effected = flash_fill(source, args.progress, scatter_strength=0.6, **common)
                no_scatter = flash_fill(source, args.progress, scatter_strength=0.0, **common)
                effected_linear = srgb_to_linear(effected[..., :3])
                no_scatter_linear = srgb_to_linear(no_scatter[..., :3])
                output_y = linear_luma(effected_linear)
                scatter_delta = linear_luma(effected_linear - no_scatter_linear)
                highlight_mask = source_y >= threshold
                new_clip = (effected[..., :3] >= 1.0) & (source[..., :3] < 1.0)
                report["variants"].append({
                    "exposure_stops": stops,
                    "highlight_profile": profile_name.lower(),
                    "highlight_profile_mode": profile_mode,
                    "highlight_threshold": threshold,
                    "highlight_range": highlight_range,
                    "radial_falloff": falloff,
                    "mean_output_linear_luma": float(output_y.mean()),
                    "mean_flash_linear_luma_delta": float((output_y - source_y).mean()),
                    "new_clipped_channel_fraction": float(new_clip.mean()),
                    "source_highlight_fraction": float(highlight_mask.mean()),
                    "scatter_on_minus_off_mean_linear_luma_delta": float(scatter_delta.mean()),
                    "scatter_on_minus_off_highlight_mean_linear_luma_delta": (
                        float(scatter_delta[highlight_mask].mean()) if np.any(highlight_mask) else 0.0),
                })
                name = (f"ev_{'plus' if stops >= 0 else 'minus'}{abs(stops):.1f}_"
                        f"{profile_name.lower()}_falloff_{falloff:.1f}").replace(".", "p")
                cv2.imwrite(str(args.output / f"{name}.png"),
                            cv2.cvtColor(to_u8(effected), cv2.COLOR_RGBA2BGRA))
                thumb = cv2.resize(cv2.cvtColor(to_u8(effected), cv2.COLOR_RGBA2BGR),
                                   (thumbnail_width, thumbnail_height), interpolation=cv2.INTER_AREA)
                header = np.full((34, thumbnail_width, 3), (34, 39, 51), np.uint8)
                cv2.putText(header, f"{profile_name} / F {falloff:.1f}", (8, 23),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, (235, 238, 244), 1, cv2.LINE_AA)
                row_cards[row].append(np.vstack((header, thumb)))
        sheet = np.vstack([np.hstack(row) for row in row_cards])
        sheet_path = args.output / f"response-knobs-ev_{'plus' if stops >= 0 else 'minus'}{abs(stops):.1f}.png"
        cv2.imwrite(str(sheet_path), sheet)
        report.setdefault("source_luma_percentiles", {})[str(stops)] = {
            "p70": percentiles[0], "p85": percentiles[1], "p95": percentiles[2],
            "p99": percentiles[3], "p99_8": percentiles[4],
        }

    with (args.output / "response-knob-sweep.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved flash response sweep to {args.output.resolve()}")


if __name__ == "__main__":
    main()
