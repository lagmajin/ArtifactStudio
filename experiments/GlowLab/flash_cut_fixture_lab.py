"""Create a reproducible exposure-cut video from a CC0 clip frame."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_cut_detection import grayscale_cut_metrics
from glow_lab import linear_to_srgb, srgb_to_linear


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="source video")
    parser.add_argument("--cut-frame", type=int, default=16,
                        help="first frame index in the low-key repeated section")
    parser.add_argument("--total-frames", type=int, default=32)
    parser.add_argument("--post-cut-source-frame", type=int,
                        help="frame used after the cut; defaults to the final pre-cut source frame")
    parser.add_argument("--post-cut-stops", type=float, default=-2.0)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" /
                        "highlight-cut-rd1" / "known-exposure-cut.avi")
    args = parser.parse_args()
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if args.cut_frame < 1 or args.total_frames <= args.cut_frame:
        parser.error("total-frames must exceed a positive cut-frame")

    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS))
    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    if fps <= 0.0:
        capture.release()
        raise ValueError("Input video must report a positive frame rate")
    if args.cut_frame > frame_count:
        capture.release()
        parser.error("cut-frame exceeds the source video frame count")
    post_cut_source_frame = (args.cut_frame - 1 if args.post_cut_source_frame is None
                             else args.post_cut_source_frame)
    if not 0 <= post_cut_source_frame < frame_count:
        capture.release()
        parser.error("post-cut-source-frame must be inside the source video")
    frames: list[np.ndarray] = []
    for _ in range(args.cut_frame):
        ok, bgr = capture.read()
        if not ok:
            capture.release()
            raise OSError("Source video ended before the requested cut frame")
        frames.append(bgr)
    capture.release()

    if post_cut_source_frame < args.cut_frame:
        post_cut_bgr = frames[post_cut_source_frame]
    else:
        capture = cv2.VideoCapture(str(args.input))
        capture.set(cv2.CAP_PROP_POS_FRAMES, post_cut_source_frame)
        ok, post_cut_bgr = capture.read()
        capture.release()
        if not ok:
            raise OSError(f"Could not read post-cut source frame {post_cut_source_frame}")

    height, width = frames[0].shape[:2]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    writer = cv2.VideoWriter(str(args.output), cv2.VideoWriter_fourcc(*"MJPG"), fps,
                             (width, height))
    if not writer.isOpened():
        raise OSError(f"Could not create synthetic cut video: {args.output}")
    try:
        for frame in frames:
            if frame.shape[:2] != (height, width):
                raise ValueError("Source video frame dimensions changed during decode")
            writer.write(frame)
        if post_cut_bgr.shape[:2] != (height, width):
            raise ValueError("Post-cut source frame dimensions do not match the pre-cut sequence")
        rgb = cv2.cvtColor(post_cut_bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        gain = 2.0 ** args.post_cut_stops
        dark_rgb = np.clip(linear_to_srgb(srgb_to_linear(rgb) * gain), 0.0, 1.0)
        dark_bgr = cv2.cvtColor(np.clip(np.round(dark_rgb * 255.0), 0, 255).astype(np.uint8),
                                cv2.COLOR_RGB2BGR)
        for _ in range(args.total_frames - args.cut_frame):
            writer.write(dark_bgr)
    finally:
        writer.release()

    check = cv2.VideoCapture(str(args.output))
    cut_histogram_distances: list[float] = []
    cut_spatial_distances: list[float] = []
    previous_frame = None
    decoded_frame_count = 0
    while True:
        ok, bgr = check.read()
        if not ok:
            break
        if previous_frame is not None:
            histogram_distance, spatial_distance = grayscale_cut_metrics(previous_frame, bgr)
            cut_histogram_distances.append(histogram_distance)
            cut_spatial_distances.append(spatial_distance)
        previous_frame = bgr
        decoded_frame_count += 1
    check.release()

    report = {
        "input_video": str(args.input.resolve()),
        "output_video": str(args.output.resolve()),
        "source_fps": fps,
        "source_dimensions": [width, height],
        "cut_frame_index": args.cut_frame,
        "total_frames": args.total_frames,
        "post_cut_source_frame": post_cut_source_frame,
        "post_cut_exposure_stops": args.post_cut_stops,
        "exposure_model": "uniform linear-light scale; sensor noise is not simulated",
        "cut_type": ("same image, hard exposure step, then repeated still"
                     if post_cut_source_frame == args.cut_frame - 1
                     else "different source frame, then repeated still"),
        "grayscale_histogram_distance_bins": 32,
        "pre_cut_adjacent_histogram_distance_max": max(
            cut_histogram_distances[:max(args.cut_frame - 1, 0)], default=0.0),
        "cut_histogram_distance": cut_histogram_distances[args.cut_frame - 1],
        "post_cut_adjacent_histogram_distance_max": max(
            cut_histogram_distances[args.cut_frame:], default=0.0),
        "pre_cut_adjacent_spatial_distance_max": max(
            cut_spatial_distances[:max(args.cut_frame - 1, 0)], default=0.0),
        "cut_spatial_distance": cut_spatial_distances[args.cut_frame - 1],
        "post_cut_adjacent_spatial_distance_max": max(
            cut_spatial_distances[args.cut_frame:], default=0.0),
        "suggested_opt_in_detection_threshold": 0.35,
        "suggested_opt_in_spatial_threshold": 0.12,
        "decoded_frame_count": decoded_frame_count,
    }
    report_path = args.output.with_suffix(args.output.suffix + ".json")
    with report_path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved {args.total_frames}-frame exposure-cut fixture to {args.output.resolve()}")


if __name__ == "__main__":
    main()
