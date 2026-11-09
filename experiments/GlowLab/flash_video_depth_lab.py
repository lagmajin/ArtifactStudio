"""Compare flat and monocular-depth flash fronts across a continuous video clip."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_depth_estimate import (
    MODEL_SIZE_BYTES,
    MODEL_SHA256,
    create_session,
    default_model_path,
    download_model,
    infer_raw_depth,
    normalize_relative_depth,
    sha256_file,
)
from flash_fill_lab import flash_fill
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    return (rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722)


def motion_warp_metric(previous: np.ndarray, current: np.ndarray,
                       backward_flow: np.ndarray, flow_max: float) -> tuple[float, float]:
    height, width = current.shape[:2]
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    map_x = x + backward_flow[..., 0]
    map_y = y + backward_flow[..., 1]
    magnitude = np.sqrt(backward_flow[..., 0] ** 2 + backward_flow[..., 1] ** 2)
    valid = ((magnitude <= flow_max) & (map_x >= 0.0) & (map_x < width - 1.0) &
             (map_y >= 0.0) & (map_y < height - 1.0))
    warped = cv2.remap(previous, map_x, map_y, cv2.INTER_LINEAR,
                       borderMode=cv2.BORDER_CONSTANT, borderValue=0.0)
    mean_delta = float(np.abs(current[valid] - warped[valid]).mean()) if np.any(valid) else 0.0
    return mean_delta, float(valid.mean())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="input video")
    parser.add_argument("--output", type=Path, required=True, help="output folder")
    parser.add_argument("--model", type=Path, default=default_model_path(), help="pinned local ONNX model")
    parser.add_argument("--download-model", action="store_true", help="download/verify the pinned model")
    parser.add_argument("--start-seconds", type=float, default=0.0)
    parser.add_argument("--end-seconds", type=float, help="exclusive end time; defaults to clip end")
    parser.add_argument("--max-dimension", type=int, default=1280)
    parser.add_argument("--synthetic-exposure-stops", type=float, default=-2.0,
                        help="uniform linear exposure applied after depth estimation; no sensor noise")
    parser.add_argument("--strength", type=float, default=2.0)
    parser.add_argument("--scatter", type=float, default=1.5)
    parser.add_argument("--scatter-front-coupling", type=float, default=1.0)
    parser.add_argument("--veil-gain", type=float, default=0.0005)
    parser.add_argument("--global-fill", type=float, default=0.005)
    parser.add_argument("--front-duration", type=float, default=0.72)
    parser.add_argument("--attack-duration", type=float, default=0.78)
    parser.add_argument("--peak-time", type=float, default=1.0)
    parser.add_argument("--decay-rate-per-second", type=float, default=2.4)
    parser.add_argument("--depth-front-strength", type=float, default=0.36)
    parser.add_argument("--flow-max-displacement", type=float, default=48.0)
    parser.add_argument("--no-depth-pngs", action="store_true", help="skip per-frame grayscale depth PNGs")
    args = parser.parse_args()
    if not args.input.is_file():
        parser.error(f"input video does not exist: {args.input}")
    if args.max_dimension < 64 or args.start_seconds < 0.0:
        parser.error("max-dimension must be >=64 and start-seconds must be nonnegative")
    if args.end_seconds is not None and args.end_seconds <= args.start_seconds:
        parser.error("end-seconds must be greater than start-seconds")
    if args.download_model:
        download_model(args.model)
    if (not args.model.is_file() or args.model.stat().st_size != MODEL_SIZE_BYTES or
            sha256_file(args.model) != MODEL_SHA256):
        parser.error("model file missing or not the pinned revision; pass --download-model")

    args.output.mkdir(parents=True, exist_ok=True)
    depth_dir = args.output / "depth-frames"
    raw_depth_dir = args.output / "raw-depth"
    raw_depth_dir.mkdir(parents=True, exist_ok=True)
    if not args.no_depth_pngs:
        depth_dir.mkdir(parents=True, exist_ok=True)
    session, ort = create_session(args.model)
    cap = cv2.VideoCapture(str(args.input))
    if not cap.isOpened():
        parser.error(f"could not open input video: {args.input}")
    fps = float(cap.get(cv2.CAP_PROP_FPS))
    declared_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    if fps <= 0.0 or declared_count < 2:
        cap.release()
        parser.error("input video must report positive fps and at least two frames")
    start_frame = int(round(args.start_seconds * fps))
    end_frame = (min(declared_count, int(round(args.end_seconds * fps)))
                 if args.end_seconds is not None else declared_count)
    start_frame = min(max(start_frame, 0), declared_count - 1)
    end_frame = min(max(end_frame, start_frame + 1), declared_count)
    cap.set(cv2.CAP_PROP_POS_FRAMES, start_frame)

    raw_map_paths: list[Path] = []
    source_indices: list[int] = []
    inference_seconds: list[float] = []
    model_input_dimensions: tuple[int, int] | None = None
    frame_dimensions: tuple[int, int] | None = None
    global_sample_parts: list[np.ndarray] = []
    for source_index in range(start_frame, end_frame):
        ok, bgr = cap.read()
        if not ok:
            break
        scale = min(1.0, args.max_dimension / max(bgr.shape[:2]))
        if scale < 1.0:
            bgr = cv2.resize(bgr, (round(bgr.shape[1] * scale), round(bgr.shape[0] * scale)),
                             interpolation=cv2.INTER_AREA)
        raw_depth, model_size, elapsed = infer_raw_depth(session, bgr)
        if frame_dimensions is None:
            frame_dimensions = raw_depth.shape
        elif raw_depth.shape != frame_dimensions:
            raise ValueError("video frame dimensions changed within the selected range")
        raw_path = raw_depth_dir / f"depth-raw-{source_index:06d}.npy"
        np.save(raw_path, raw_depth, allow_pickle=False)
        raw_map_paths.append(raw_path)
        global_sample_parts.append(raw_depth[::8, ::8].ravel().copy())
        source_indices.append(source_index)
        inference_seconds.append(elapsed)
        model_input_dimensions = model_size
        if len(raw_map_paths) % 10 == 0 or source_index + 1 == end_frame:
            print(f"Estimated depth for {len(raw_map_paths)} frames ({source_index + 1}/{end_frame})")
    cap.release()
    if len(raw_map_paths) < 2:
        raise ValueError("selected video range contains fewer than two decoded frames")
    assert frame_dimensions is not None
    height, width = frame_dimensions

    # A sparse, deterministic sample estimates one clip-wide robust scale without
    # retaining a second full copy of every depth map in memory.
    global_sample = np.concatenate(global_sample_parts)
    del global_sample_parts
    global_low, global_high = np.percentile(global_sample, (2.0, 98.0))
    if global_high - global_low <= 1.0e-8:
        raise ValueError("clip-wide relative depth has insufficient range")

    writer = cv2.VideoWriter(str(args.output / "flash-depth-comparison.avi"),
                             cv2.VideoWriter_fourcc(*"MJPG"), fps, (width * 3, height))
    if not writer.isOpened():
        raise OSError("Could not create MJPG comparison video")
    variants = ("flat", "per_frame_depth", "clip_normalized_depth")
    frame_metrics: list[dict[str, object]] = []
    temporal_pairs: dict[str, list[float]] = {
        key: [] for key in (
            "motion_compensated_source_mean_abs_delta",
            "motion_compensated_depth_frame_mean_abs_delta",
            "motion_compensated_depth_clip_mean_abs_delta",
            "motion_compensated_flash_flat_mean_abs_delta",
            "motion_compensated_flash_frame_depth_mean_abs_delta",
            "motion_compensated_flash_clip_depth_mean_abs_delta",
        )
    }
    previous_gray: np.ndarray | None = None
    previous_depth_frame: np.ndarray | None = None
    previous_depth_clip: np.ndarray | None = None
    previous_flash: dict[str, np.ndarray] = {}
    contact_rows: list[np.ndarray] = []
    sample_indices = set(np.linspace(0, len(raw_map_paths) - 1,
                                     min(6, len(raw_map_paths)), dtype=np.int32).tolist())
    exposure_gain = 2.0 ** args.synthetic_exposure_stops
    cap = cv2.VideoCapture(str(args.input))
    if not cap.isOpened():
        writer.release()
        raise OSError(f"could not reopen input video: {args.input}")
    cap.set(cv2.CAP_PROP_POS_FRAMES, start_frame)

    for index, (source_index, raw_path) in enumerate(zip(source_indices, raw_map_paths)):
        ok, source_bgr = cap.read()
        if not ok:
            writer.release()
            cap.release()
            raise OSError(f"could not reread source frame {source_index}")
        scale = min(1.0, args.max_dimension / max(source_bgr.shape[:2]))
        if scale < 1.0:
            source_bgr = cv2.resize(
                source_bgr,
                (round(source_bgr.shape[1] * scale), round(source_bgr.shape[0] * scale)),
                interpolation=cv2.INTER_AREA)
        if source_bgr.shape[:2] != (height, width):
            writer.release()
            cap.release()
            raise ValueError(f"source frame {source_index} dimensions changed between passes")
        raw_depth = np.load(raw_path, allow_pickle=False)
        per_frame_depth, (frame_low, frame_high) = normalize_relative_depth(raw_depth)
        depth_clip = np.clip((raw_depth - global_low) / (global_high - global_low), 0.0, 1.0)
        depth_u8 = np.round(depth_clip * 255.0).astype(np.uint8)
        if not args.no_depth_pngs:
            cv2.imwrite(str(depth_dir / f"depth-frame-{source_indices[index]:06d}.png"), depth_u8)

        rgb = cv2.cvtColor(source_bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        if exposure_gain != 1.0:
            rgb = np.clip(linear_to_srgb(srgb_to_linear(rgb) * exposure_gain), 0.0, 1.0)
        source = np.dstack((rgb, np.ones((height, width), np.float32)))
        progress = index / (len(raw_map_paths) - 1)
        effect_maps = {
            "flat": None,
            "per_frame_depth": per_frame_depth,
            "clip_normalized_depth": depth_clip,
        }
        flash_contributions: dict[str, np.ndarray] = {}
        outputs: dict[str, np.ndarray] = {}
        output_clips: dict[str, float] = {}
        for name, depth_map in effect_maps.items():
            output = flash_fill(
                source, progress, mode="radial", source_position=(0.08, 0.5),
                strength=args.strength, softness=0.19, temperature=(1.0, 0.94, 0.82),
                attack_duration=args.attack_duration, peak_time=args.peak_time,
                decay_rate=args.decay_rate_per_second * max(len(raw_map_paths) - 1, 1) / fps,
                scatter_strength=args.scatter, scatter_front_coupling=args.scatter_front_coupling,
                veil_gain=args.veil_gain, highlight_threshold=0.06, highlight_range=0.22,
                global_fill=args.global_fill, front_duration=args.front_duration,
                radial_falloff=2.7, highlight_rolloff=0.78,
                depth_map=depth_map,
                depth_front_strength=args.depth_front_strength if depth_map is not None else 0.0)
            outputs[name] = output
            output_luma = linear_luma(srgb_to_linear(output[..., :3]))
            source_luma = linear_luma(srgb_to_linear(source[..., :3]))
            flash_contributions[name] = output_luma - source_luma
            output_clips[name] = float(np.mean(output[..., :3] >= 1.0))

        source_luma = linear_luma(srgb_to_linear(source[..., :3]))
        record: dict[str, object] = {
            "frame": index,
            "source_frame": source_indices[index],
            "time_seconds": source_index / fps,
            "progress": progress,
            "model_inference_seconds": inference_seconds[index],
            "raw_depth_percentiles": {str(p): float(v) for p, v in zip(
                (2, 50, 98), np.percentile(raw_depth, (2, 50, 98)))},
            "frame_depth_scale": [float(frame_low), float(frame_high)],
            "clip_depth_scale": [float(global_low), float(global_high)],
            "source_mean_linear_luma": float(source_luma.mean()),
            "variants": {},
        }
        for name in variants:
            delta = flash_contributions[name]
            record["variants"][name] = {
                "mean_flash_linear_luma_delta": float(delta.mean()),
                "brightened_pixel_fraction": float(np.mean(delta > 1.0 / 255.0)),
                "output_clipped_channel_fraction": output_clips[name],
                "newly_clipped_channel_fraction": float(np.mean(
                    (outputs[name][..., :3] >= 1.0) & (source[..., :3] < 1.0))),
            }

        current_gray = cv2.cvtColor(source_bgr, cv2.COLOR_BGR2GRAY)
        current_depth_frame = per_frame_depth.astype(np.float32)
        current_depth_clip = depth_clip.astype(np.float32)
        if previous_gray is not None:
            backward_flow = cv2.calcOpticalFlowFarneback(
                current_gray, previous_gray, None, 0.5, 3, 19, 3, 5, 1.2, 0)
            source_delta, valid_fraction = motion_warp_metric(
                previous_source_luma, source_luma, backward_flow, args.flow_max_displacement)
            depth_frame_delta, _ = motion_warp_metric(
                previous_depth_frame, current_depth_frame, backward_flow, args.flow_max_displacement)
            depth_clip_delta, _ = motion_warp_metric(
                previous_depth_clip, current_depth_clip, backward_flow, args.flow_max_displacement)
            temporal_pairs["motion_compensated_source_mean_abs_delta"].append(source_delta)
            temporal_pairs["motion_compensated_depth_frame_mean_abs_delta"].append(depth_frame_delta)
            temporal_pairs["motion_compensated_depth_clip_mean_abs_delta"].append(depth_clip_delta)
            record["motion_compensated_valid_fraction"] = valid_fraction
            record["motion_compensated_depth_frame_mean_abs_delta"] = depth_frame_delta
            record["motion_compensated_depth_clip_mean_abs_delta"] = depth_clip_delta
            for name, key in (("flat", "motion_compensated_flash_flat_mean_abs_delta"),
                              ("per_frame_depth", "motion_compensated_flash_frame_depth_mean_abs_delta"),
                              ("clip_normalized_depth", "motion_compensated_flash_clip_depth_mean_abs_delta")):
                delta, _ = motion_warp_metric(previous_flash[name], flash_contributions[name],
                                              backward_flow, args.flow_max_displacement)
                temporal_pairs[key].append(delta)
                record[key] = delta
            record["motion_compensated_source_mean_abs_delta"] = source_delta

        comparison = np.hstack([cv2.cvtColor(to_u8(outputs[name]), cv2.COLOR_RGBA2BGR)
                                for name in variants])
        writer.write(comparison)
        if index in sample_indices:
            tile_width = min(width, 480)
            tile_height = max(1, round(height * tile_width / width))
            tiles = [cv2.resize(cv2.cvtColor(to_u8(outputs[name]), cv2.COLOR_RGBA2BGR),
                                (tile_width, tile_height), interpolation=cv2.INTER_AREA)
                     for name in variants]
            row = np.hstack(tiles)
            header = np.full((30, row.shape[1], 3), (22, 27, 36), np.uint8)
            cv2.putText(
                header,
                f"frame {source_index}  progress {progress:.0%}  |  flat  /  frame-normalized  /  clip-normalized",
                (10, 21), cv2.FONT_HERSHEY_SIMPLEX, 0.48, (245, 245, 245), 1, cv2.LINE_AA)
            contact_rows.append(np.vstack((header, row)))
        frame_metrics.append(record)
        previous_gray = current_gray
        previous_source_luma = source_luma
        previous_depth_frame = current_depth_frame
        previous_depth_clip = current_depth_clip
        previous_flash = flash_contributions

    writer.release()
    cap.release()
    cv2.imwrite(str(args.output / "flash-depth-contact-sheet.png"), np.vstack(contact_rows))

    temporal_summary = {
        key: {"mean": float(np.mean(values)), "max": float(np.max(values)), "pair_count": len(values)}
        for key, values in temporal_pairs.items() if values
    }
    report = {
        "input_video": str(args.input.resolve()),
        "model_repository": "onnx-community/depth-anything-v2-small",
        "model_revision": "4472b7362082ad9968fee890ca0f1e5aca36b93d",
        "model_sha256": sha256_file(args.model),
        "model_runtime": f"onnxruntime {ort.__version__}",
        "execution_provider": "CPUExecutionProvider",
        "input_frame_count_declared": declared_count,
        "processed_frame_count": len(raw_map_paths),
        "width": width,
        "height": height,
        "model_input_dimensions": {
            "width": model_input_dimensions[0], "height": model_input_dimensions[1]},
        "fps": fps,
        "duration_seconds": max(len(raw_map_paths) - 1, 0) / fps,
        "mean_depth_inference_seconds": float(np.mean(inference_seconds)),
        "max_depth_inference_seconds": float(np.max(inference_seconds)),
        "depth_normalization": {
            "per_frame_percentiles": [2.0, 98.0],
            "clip_percentiles": [2.0, 98.0],
            "clip_percentile_estimator": "per-frame spatial grid sample with 8-pixel stride",
            "clip_low": float(global_low),
            "clip_high": float(global_high),
        },
        "effect_settings": {
            "strength": args.strength,
            "scatter": args.scatter,
            "scatter_front_coupling": args.scatter_front_coupling,
            "veil_gain": args.veil_gain,
            "global_fill": args.global_fill,
            "depth_front_strength": args.depth_front_strength,
            "depth_normalization_variants": list(variants),
            "synthetic_exposure_stops": args.synthetic_exposure_stops,
        },
        "motion_compensated_temporal_summary": temporal_summary,
        "limitations": [
            "Relative monocular depth is not metric scene depth and does not resolve visibility or cast shadows.",
            "Per-frame normalization can change local depth contrast; clip normalization can change with shot range.",
            "Farneback motion compensation is approximate and does not prove depth flicker-free playback.",
            "Synthetic exposure is a uniform linear-light scale and does not model sensor noise.",
        ],
        "outputs": [
            "flash-depth-comparison.avi",
            "flash-depth-contact-sheet.png",
            "flash-video-depth-metrics.json",
            "raw-depth/" if raw_map_paths else None,
            "depth-frames/" if not args.no_depth_pngs else None,
        ],
        "frames": frame_metrics,
    }
    with (args.output / "flash-video-depth-metrics.json").open(
            "w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved depth-aware flash comparison to {args.output.resolve()} ({len(raw_map_paths)} frames)")


if __name__ == "__main__":
    main()
