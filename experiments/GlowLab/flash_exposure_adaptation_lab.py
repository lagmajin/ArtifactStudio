"""Compare flash exposure strength against low-key and high-key source images."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill, load_image
from glow_lab import srgb_to_linear, to_u8


def resize_rgba(rgba: np.ndarray, max_dimension: int) -> np.ndarray:
    height, width = rgba.shape[:2]
    scale = min(1.0, max_dimension / max(height, width))
    if scale >= 1.0:
        return rgba
    size = (max(1, round(width * scale)), max(1, round(height * scale)))
    rgb = cv2.resize(rgba[..., :3], size, interpolation=cv2.INTER_AREA)
    alpha = cv2.resize(rgba[..., 3], size, interpolation=cv2.INTER_AREA)
    return np.dstack((rgb, alpha))


def load_video_frame(path: Path, seconds: float) -> tuple[np.ndarray, float]:
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {path}")
    fps = float(capture.get(cv2.CAP_PROP_FPS))
    if fps <= 0.0:
        fps = 24.0
    frame = max(0, int(round(seconds * fps)))
    try:
        capture.set(cv2.CAP_PROP_POS_FRAMES, frame)
        ok, bgr = capture.read()
        if not ok:
            raise OSError(f"Could not read frame {frame} from {path}")
        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        rgba = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
        return rgba, frame / fps
    finally:
        capture.release()


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(rgb)
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "exposure-adaptation")
    parser.add_argument("--image", type=Path, action="append", default=[],
                        help="8-bit image to include; may be repeated")
    parser.add_argument("--video", type=Path,
                        help="optional video; one frame is sampled for the high-key comparison")
    parser.add_argument("--video-seconds", type=float, default=1.0)
    parser.add_argument("--progress", type=float, default=0.68)
    parser.add_argument("--strengths", nargs="+", type=float,
                        default=(0.4, 0.8, 1.2, 1.6, 2.3))
    parser.add_argument("--target-new-clip-fraction", type=float, default=0.08,
                        help="choose the highest tested strength below this new-clip fraction")
    parser.add_argument("--max-dimension", type=int, default=960)
    args = parser.parse_args()
    if not args.image and args.video is None:
        parser.error("provide at least one --image or --video")
    if args.max_dimension < 64:
        parser.error("max-dimension must be at least 64")
    args.output.mkdir(parents=True, exist_ok=True)

    sources: list[tuple[str, str, np.ndarray, dict[str, object]]] = []
    for path in args.image:
        source = resize_rgba(load_image(path), args.max_dimension)
        sources.append((path.stem, str(path.resolve()), source, {"kind": "image"}))
    if args.video is not None:
        rgba, actual_seconds = load_video_frame(args.video, args.video_seconds)
        source = resize_rgba(rgba, args.max_dimension)
        sources.append(("video-frame", str(args.video.resolve()), source,
                        {"kind": "video_frame", "requested_seconds": args.video_seconds,
                         "actual_seconds": actual_seconds}))

    strengths = sorted(set(max(0.0, float(v)) for v in args.strengths))
    if not strengths:
        parser.error("strengths must contain at least one nonnegative value")
    report: dict[str, object] = {
        "experiment": "source-luminance-aware flash strength sweep",
        "progress": float(np.clip(args.progress, 0.0, 1.0)),
        "strength_candidates": strengths,
        "target_new_clip_fraction": float(np.clip(args.target_new_clip_fraction, 0.0, 1.0)),
        "parameters": {
            "mode": "radial", "source_position": [0.50, 0.14],
            "scatter": 0.6, "global_fill": 0.02, "front_duration": 0.72,
            "highlight_rolloff": 0.8,
        },
        "sources": [],
    }
    contact_panels: list[np.ndarray] = []
    for source_name, source_path, source, source_info in sources:
        source_luma = linear_luma(source[..., :3])
        source_clip_fraction = float(np.mean(source[..., :3] >= 1.0))
        source_luma_percentiles = {
            str(p): float(np.percentile(source_luma, p)) for p in (50, 90, 95, 99)
        }
        source_bgr = cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGR)
        thumb_size = (384, max(1, round(source.shape[0] * 384 / source.shape[1])))
        source_thumb = cv2.resize(source_bgr, thumb_size, interpolation=cv2.INTER_AREA)
        header = np.full((36, thumb_size[0], 3), (22, 27, 37), np.uint8)
        cv2.putText(header, f"{source_name} / SOURCE", (8, 24), cv2.FONT_HERSHEY_SIMPLEX,
                    0.48, (245, 245, 245), 1, cv2.LINE_AA)
        source_panels = [np.vstack((header, source_thumb))]
        candidate_results: list[dict[str, object]] = []

        for strength in strengths:
            result = flash_fill(
                source, float(report["progress"]), mode="radial", source_position=(0.50, 0.14),
                strength=strength, softness=0.19, temperature=(1.0, 0.94, 0.82),
                attack_duration=0.72, peak_time=0.78, decay_rate=3.0,
                scatter_strength=0.6, highlight_threshold=0.28, highlight_range=0.52,
                global_fill=0.02, front_duration=0.72, radial_falloff=2.7,
                highlight_rolloff=0.8)
            result_luma = linear_luma(result[..., :3])
            new_clipped = (result[..., :3] >= 1.0) & (source[..., :3] < 1.0)
            result_bgr = cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGR)
            thumb = cv2.resize(result_bgr, thumb_size, interpolation=cv2.INTER_AREA)
            panel_header = np.full((36, thumb_size[0], 3), (22, 27, 37), np.uint8)
            cv2.putText(panel_header, f"STRENGTH {strength:.2f}", (8, 24),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.48, (245, 245, 245), 1, cv2.LINE_AA)
            source_panels.append(np.vstack((panel_header, thumb)))
            cv2.imwrite(str(args.output / f"{source_name}-strength-{strength:.2f}.png"),
                        cv2.cvtColor(to_u8(result), cv2.COLOR_RGBA2BGRA))
            candidate_results.append({
                "strength": strength,
                "mean_linear_luma_delta": float((result_luma - source_luma).mean()),
                "output_mean_linear_luma": float(result_luma.mean()),
                "output_p95_linear_luma": float(np.percentile(result_luma, 95)),
                "output_p99_linear_luma": float(np.percentile(result_luma, 99)),
                "output_clipped_channel_fraction": float(np.mean(result[..., :3] >= 1.0)),
                "newly_clipped_channel_fraction": float(new_clipped.mean()),
                "brightened_pixel_fraction": float(np.mean(result_luma - source_luma > 1.0 / 255.0)),
            })
        eligible = [item for item in candidate_results
                    if item["newly_clipped_channel_fraction"] <= report["target_new_clip_fraction"]]
        selection = (max(eligible, key=lambda item: float(item["strength"]))
                     if eligible else None)
        report["sources"].append({
            "name": source_name,
            "path": source_path,
            **source_info,
            "width": int(source.shape[1]),
            "height": int(source.shape[0]),
            "source_mean_linear_luma": float(source_luma.mean()),
            "source_luma_percentiles": source_luma_percentiles,
            "source_clipped_channel_fraction": source_clip_fraction,
            "selected_strength_under_clip_target": (
                selection["strength"] if selection is not None else None),
            "clip_target_met": selection is not None,
            "candidates": candidate_results,
        })
        while len(source_panels) % 3:
            source_panels.append(np.zeros_like(source_panels[0]))
        contact_panels.append(np.vstack([
            np.hstack(source_panels[i:i + 3]) for i in range(0, len(source_panels), 3)
        ]))

    cv2.imwrite(str(args.output / "flash-exposure-adaptation-comparison.png"),
                np.vstack(contact_panels))
    with (args.output / "flash-exposure-adaptation-metrics.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved flash exposure adaptation study to {args.output.resolve()}")


if __name__ == "__main__":
    main()
