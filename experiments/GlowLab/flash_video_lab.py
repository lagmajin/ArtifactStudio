"""Apply the image-space flash R&D effect to every frame of a video clip."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from flash_cut_detection import grayscale_cut_metrics
from flash_preset import load_preset
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    linear = srgb_to_linear(rgb)
    return linear[..., 0] * 0.2126 + linear[..., 1] * 0.7152 + linear[..., 2] * 0.0722


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="input video clip")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "video")
    parser.add_argument("--start-seconds", type=float, default=0.0,
                        help="inclusive start time within the input clip")
    parser.add_argument("--end-seconds", type=float,
                        help="inclusive end time; defaults to the end of the input clip")
    parser.add_argument("--preset", type=Path,
                        help="load a look saved by flash_video_preview.py")
    parser.add_argument("--video-output", type=Path,
                        help="optional silent MP4/AVI output in addition to PNG frames")
    parser.add_argument("--no-png-frames", action="store_true",
                        help="do not write a PNG for every processed frame")
    parser.add_argument("--mode", choices=("directional", "radial"))
    parser.add_argument("--source-position", nargs=2, type=float,
                        metavar=("X", "Y"), help="radial source in normalized image coordinates")
    parser.add_argument("--direction", nargs=2, type=float,
                        metavar=("X", "Y"), help="direction vector for a directional front")
    parser.add_argument("--synthetic-exposure-stops", type=float, default=0.0,
                        help="uniform linear-light source exposure offset; does not simulate sensor noise")
    parser.add_argument("--strength", type=float)
    parser.add_argument("--scatter", type=float)
    parser.add_argument("--scatter-front-coupling", type=float,
                        help="couple highlight scatter to the traveling front (0=global, 1=front-gated)")
    parser.add_argument("--veil-gain", type=float,
                        help="broad veiling-light gain (default 0.025)")
    parser.add_argument("--global-fill", type=float)
    parser.add_argument("--front-duration", type=float)
    parser.add_argument("--decay-rate-per-second", type=float)
    parser.add_argument("--highlight-rolloff", type=float,
                        help="linear highlight shoulder from 0 (hard clip) to 1")
    parser.add_argument("--softness", type=float, help="flash-front softness")
    parser.add_argument("--temperature", nargs=3, type=float, metavar=("R", "G", "B"),
                        help="flash tint multipliers")
    parser.add_argument("--attack-duration", type=float,
                        help="normalized rise duration for the selected clip range")
    parser.add_argument("--peak-time", type=float,
                        help="normalized peak time for the selected clip range")
    parser.add_argument("--radial-falloff", type=float, help="radial exposure falloff")
    parser.add_argument("--highlight-threshold", type=float, help="highlight extraction threshold")
    parser.add_argument("--highlight-range", type=float, help="highlight extraction softness range")
    parser.add_argument("--highlight-percentiles", nargs=2, type=float,
                        metavar=("THRESHOLD_PCTL", "RANGE_END_PCTL"),
                        help="derive threshold and range end from each frame's linear-luma percentiles")
    parser.add_argument("--highlight-smoothing-seconds", type=float, default=0.0,
                        help="causal EMA time constant for percentile-derived thresholds; 0 disables smoothing")
    parser.add_argument("--highlight-reset-frames", nargs="*", type=int, default=(),
                        help="known shot-cut frame indices relative to the selected range; reset percentile EMA there")
    parser.add_argument("--detect-highlight-cuts", action="store_true",
                        help="also reset percentile EMA on large grayscale-histogram changes")
    parser.add_argument("--highlight-cut-threshold", type=float, default=0.35,
                        help="Bhattacharyya grayscale-histogram distance for opt-in cut reset (0..1)")
    parser.add_argument("--highlight-cut-spatial-threshold", type=float, default=0.12,
                        help="thumbnail mean-absolute grayscale difference for opt-in cut reset (0..1)")
    parser.add_argument("--max-dimension", type=int, default=1280)
    parser.add_argument("--motion-compensated-metrics", action="store_true",
                        help="estimate backward Farneback flow for temporal metrics")
    parser.add_argument("--flow-max-displacement", type=float, default=48.0,
                        help="exclude flow vectors longer than this many output pixels")
    args = parser.parse_args()
    settings = load_preset(args.preset)
    for argument, setting in (("mode", "mode"), ("source_position", "source_position"),
                              ("direction", "direction"),
                              ("strength", "strength"), ("scatter", "scatter"),
                              ("scatter_front_coupling", "scatter_front_coupling"),
                              ("veil_gain", "veil_gain"),
                              ("global_fill", "global_fill"),
                              ("front_duration", "front_duration"),
                              ("decay_rate_per_second", "decay_rate_per_second"),
                              ("highlight_rolloff", "highlight_rolloff"),
                              ("softness", "softness"),
                              ("attack_duration", "attack_duration"),
                              ("peak_time", "peak_time"),
                              ("radial_falloff", "radial_falloff"),
                              ("highlight_threshold", "highlight_threshold"),
                              ("highlight_range", "highlight_range")):
        if getattr(args, argument) is None:
            setattr(args, argument, settings[setting])
    if args.temperature is None:
        args.temperature = settings["temperature"]
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if args.max_dimension < 64:
        parser.error("max-dimension must be at least 64")
    if args.highlight_percentiles is not None:
        low, high = args.highlight_percentiles
        if not (0.0 <= low < high <= 100.0):
            parser.error("highlight percentiles must satisfy 0 <= threshold percentile < range-end percentile <= 100")
    elif args.highlight_smoothing_seconds > 0.0:
        parser.error("highlight-smoothing-seconds requires --highlight-percentiles")
    if args.highlight_smoothing_seconds < 0.0:
        parser.error("highlight-smoothing-seconds must be nonnegative")
    if not 0.0 < args.highlight_cut_threshold <= 1.0:
        parser.error("highlight-cut-threshold must be greater than 0 and at most 1")
    if not 0.0 < args.highlight_cut_spatial_threshold <= 1.0:
        parser.error("highlight-cut-spatial-threshold must be greater than 0 and at most 1")
    if args.detect_highlight_cuts and (args.highlight_percentiles is None or
                                       args.highlight_smoothing_seconds <= 0.0):
        parser.error("detect-highlight-cuts requires percentile thresholds and positive smoothing")
    args.output.mkdir(parents=True, exist_ok=True)
    cap = cv2.VideoCapture(str(args.input))
    if not cap.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(cap.get(cv2.CAP_PROP_FPS))
    declared_count = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    if fps <= 0.0:
        fps = 24.0
    if declared_count < 2:
        cap.release()
        raise ValueError("Input video must report at least two frames")
    source_duration = (declared_count - 1) / fps
    if args.start_seconds < 0.0 or args.start_seconds >= source_duration:
        cap.release()
        parser.error("start-seconds must be inside the input video")
    if args.end_seconds is not None and args.end_seconds <= args.start_seconds:
        cap.release()
        parser.error("end-seconds must be greater than start-seconds")
    if args.end_seconds is not None and args.end_seconds > source_duration + (0.5 / fps):
        cap.release()
        parser.error("end-seconds exceeds the input video duration")
    start_frame = int(round(args.start_seconds * fps))
    end_frame = (declared_count - 1 if args.end_seconds is None else
                 min(declared_count - 1, int(round(args.end_seconds * fps))))
    selected_count = end_frame - start_frame + 1
    if selected_count < 2:
        cap.release()
        parser.error("selected range must include at least two frames")
    highlight_reset_frames = sorted(set(args.highlight_reset_frames))
    if highlight_reset_frames and (args.highlight_percentiles is None or
                                   args.highlight_smoothing_seconds <= 0.0):
        cap.release()
        parser.error("highlight-reset-frames requires percentile thresholds with positive smoothing")
    if any(frame <= 0 or frame >= selected_count for frame in highlight_reset_frames):
        cap.release()
        parser.error("highlight reset frames must be inside the selected range and greater than zero")
    detected_cut_frames: list[int] = []
    detected_cut_scores: dict[int, dict[str, float]] = {}
    previous_cut_frame: np.ndarray | None = None
    duration = (selected_count - 1) / fps
    actual_fps = (selected_count - 1) / duration
    sample_indices = sorted(set(int(round(t * (selected_count - 1)))
                                for t in (0.0, 0.2, 0.4, 0.6, 0.8, 1.0)))
    contact_rows: list[np.ndarray] = []
    frame_metrics: list[dict[str, float | int]] = []
    previous_output: np.ndarray | None = None
    previous_source_luma: np.ndarray | None = None
    previous_flash_delta: np.ndarray | None = None
    previous_source_gray: np.ndarray | None = None
    source_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    source_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    scale = min(1.0, args.max_dimension / max(source_height, source_width))
    output_width = max(1, int(round(source_width * scale)))
    output_height = max(1, int(round(source_height * scale)))
    writer = None
    if args.video_output is not None:
        args.video_output.parent.mkdir(parents=True, exist_ok=True)
        suffix = args.video_output.suffix.lower()
        fourcc = cv2.VideoWriter_fourcc(*("XVID" if suffix == ".avi" else "mp4v"))
        writer = cv2.VideoWriter(str(args.video_output), fourcc, fps, (output_width, output_height))
        if not writer.isOpened():
            cap.release()
            raise OSError(f"Could not open video output: {args.video_output}")
    cap.set(cv2.CAP_PROP_POS_FRAMES, start_frame)
    processed_count = 0
    smoothed_highlight_threshold: float | None = None
    smoothed_highlight_range: float | None = None
    while processed_count < selected_count:
        ok, bgr = cap.read()
        if not ok:
            break
        if bgr.shape[1] != output_width or bgr.shape[0] != output_height:
            bgr = cv2.resize(bgr, (output_width, output_height), interpolation=cv2.INTER_AREA)
        histogram_cut_score = 0.0
        spatial_cut_score = 0.0
        if args.detect_highlight_cuts:
            if previous_cut_frame is not None:
                histogram_cut_score, spatial_cut_score = grayscale_cut_metrics(previous_cut_frame, bgr)
                if (histogram_cut_score >= args.highlight_cut_threshold or
                        spatial_cut_score >= args.highlight_cut_spatial_threshold):
                    detected_cut_frames.append(processed_count)
                    detected_cut_scores[processed_count] = {
                        "histogram": histogram_cut_score,
                        "spatial": spatial_cut_score,
                    }
            previous_cut_frame = bgr
        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        if args.synthetic_exposure_stops != 0.0:
            exposure_gain = 2.0 ** args.synthetic_exposure_stops
            rgb = np.clip(linear_to_srgb(srgb_to_linear(rgb) * exposure_gain), 0.0, 1.0)
        source = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
        source_luma = linear_luma(source[..., :3])
        source_clipped_channels = source[..., :3] >= 1.0
        if args.highlight_percentiles is None:
            frame_highlight_threshold = args.highlight_threshold
            frame_highlight_range = args.highlight_range
            target_highlight_threshold = frame_highlight_threshold
            target_highlight_range = frame_highlight_range
        else:
            target_highlight_threshold, range_end = np.percentile(
                source_luma, args.highlight_percentiles)
            target_highlight_threshold = float(target_highlight_threshold)
            target_highlight_range = max(0.01, float(range_end - target_highlight_threshold))
            if args.highlight_smoothing_seconds > 0.0:
                reset_at_cut = (processed_count in highlight_reset_frames or
                                processed_count in detected_cut_frames)
                if smoothed_highlight_threshold is None or reset_at_cut:
                    smoothed_highlight_threshold = target_highlight_threshold
                    smoothed_highlight_range = target_highlight_range
                else:
                    alpha = 1.0 - np.exp(-1.0 / (fps * args.highlight_smoothing_seconds))
                    smoothed_highlight_threshold += alpha * (
                        target_highlight_threshold - smoothed_highlight_threshold)
                    smoothed_highlight_range += alpha * (
                        target_highlight_range - smoothed_highlight_range)
                frame_highlight_threshold = smoothed_highlight_threshold
                frame_highlight_range = smoothed_highlight_range
            else:
                frame_highlight_threshold = target_highlight_threshold
                frame_highlight_range = target_highlight_range
        source_gray = cv2.cvtColor(bgr, cv2.COLOR_BGR2GRAY)
        index = processed_count
        source_frame_index = start_frame + index
        progress = index / (selected_count - 1)
        effect = flash_fill(
            source, progress, mode=args.mode,
            source_position=tuple(args.source_position), direction=tuple(args.direction),
            strength=args.strength,
            softness=args.softness, temperature=tuple(args.temperature),
            attack_duration=args.attack_duration, peak_time=args.peak_time,
            decay_rate=args.decay_rate_per_second * duration,
            scatter_strength=args.scatter,
            scatter_front_coupling=args.scatter_front_coupling,
            veil_gain=args.veil_gain,
            global_fill=args.global_fill, front_duration=args.front_duration,
            radial_falloff=args.radial_falloff,
            highlight_threshold=frame_highlight_threshold,
            highlight_range=frame_highlight_range,
            highlight_rolloff=args.highlight_rolloff)
        assert np.isfinite(effect).all()
        assert np.array_equal(effect[..., 3], source[..., 3])
        if index == 0:
            assert np.allclose(effect[..., :3], source[..., :3], atol=2.0e-6)
        effect_bgra = cv2.cvtColor(to_u8(effect), cv2.COLOR_RGBA2BGRA)
        if not args.no_png_frames:
            output_path = args.output / f"flash-frame-{index:04d}.png"
            cv2.imwrite(str(output_path), effect_bgra)
        if writer is not None:
            writer.write(effect_bgra[..., :3])
        output_luma = linear_luma(effect[..., :3])
        flash_delta = output_luma - source_luma
        frame_metrics.append({
            "frame": index,
            "source_frame": source_frame_index,
            "time_seconds": index / fps,
            "source_time_seconds": source_frame_index / fps,
            "normalized_progress": progress,
            "source_mean_linear_luma": float(source_luma.mean()),
            "output_mean_linear_luma": float(output_luma.mean()),
            "mean_linear_luma_delta": float(flash_delta.mean()),
            "flash_contribution_mean_linear_luma": float(flash_delta.mean()),
            "flash_brightened_pixel_fraction": float(np.mean(flash_delta > 1.0 / 255.0)),
            "clipped_channel_fraction": float(np.mean(effect[..., :3] >= 1.0)),
            "source_clipped_channel_fraction": float(np.mean(source_clipped_channels)),
            "newly_clipped_channel_fraction": float(np.mean(
                (~source_clipped_channels) & (effect[..., :3] >= 1.0))),
            "highlight_threshold_used": frame_highlight_threshold,
            "highlight_range_used": frame_highlight_range,
            "highlight_threshold_target": target_highlight_threshold,
            "highlight_range_target": target_highlight_range,
            "highlight_seed_fraction": float(np.mean(source_luma >= frame_highlight_threshold)),
            "highlight_reset_applied": int(
                processed_count in highlight_reset_frames or processed_count in detected_cut_frames),
            "highlight_cut_detected": int(processed_count in detected_cut_frames),
            "grayscale_histogram_cut_score": histogram_cut_score,
            "grayscale_spatial_cut_score": spatial_cut_score,
            "changed_pixel_fraction_srgb_gt_1_over_255": float(
                np.mean(np.max(np.abs(effect[..., :3] - source[..., :3]), axis=2) > 1.0 / 255.0)),
        })
        if previous_output is not None:
            frame_metrics[-1]["adjacent_output_mean_abs_linear_luma_delta"] = float(
                np.abs(output_luma - previous_output).mean())
        if previous_source_luma is not None:
            frame_metrics[-1]["adjacent_source_mean_abs_linear_luma_delta"] = float(
                np.abs(source_luma - previous_source_luma).mean())
        if previous_flash_delta is not None:
            frame_metrics[-1]["adjacent_flash_contribution_mean_abs_delta"] = float(
                np.abs(flash_delta - previous_flash_delta).mean())
        if args.motion_compensated_metrics and previous_source_gray is not None:
            backward_flow = cv2.calcOpticalFlowFarneback(
                source_gray, previous_source_gray, None,
                pyr_scale=0.5, levels=3, winsize=19, iterations=3,
                poly_n=5, poly_sigma=1.2, flags=0)
            flow_y, flow_x = np.mgrid[0:output_height, 0:output_width].astype(np.float32)
            map_x = flow_x + backward_flow[..., 0]
            map_y = flow_y + backward_flow[..., 1]
            flow_magnitude = np.hypot(backward_flow[..., 0], backward_flow[..., 1])
            valid = ((map_x >= 0.0) & (map_x < output_width - 1) &
                     (map_y >= 0.0) & (map_y < output_height - 1) &
                     np.isfinite(flow_magnitude) &
                     (flow_magnitude <= max(0.0, args.flow_max_displacement)))
            warped_previous_source = cv2.remap(
                previous_source_luma, map_x, map_y, cv2.INTER_LINEAR,
                borderMode=cv2.BORDER_CONSTANT, borderValue=0)
            warped_previous_flash = cv2.remap(
                previous_flash_delta, map_x, map_y, cv2.INTER_LINEAR,
                borderMode=cv2.BORDER_CONSTANT, borderValue=0)
            if np.any(valid):
                frame_metrics[-1]["motion_compensated_source_mean_abs_delta"] = float(
                    np.abs(source_luma[valid] - warped_previous_source[valid]).mean())
                frame_metrics[-1]["motion_compensated_flash_contribution_mean_abs_delta"] = float(
                    np.abs(flash_delta[valid] - warped_previous_flash[valid]).mean())
            frame_metrics[-1]["motion_compensated_valid_fraction"] = float(valid.mean())
            frame_metrics[-1]["backward_flow_mean_displacement_px"] = (
                float(flow_magnitude[valid].mean()) if np.any(valid) else 0.0)
        if index in sample_indices:
            source_bgra = cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGRA)
            thumb_h = max(1, int(round(source.shape[0] * 480 / source.shape[1])))
            source_thumb = cv2.resize(source_bgra, (480, thumb_h), interpolation=cv2.INTER_AREA)
            effect_thumb = cv2.resize(effect_bgra, (480, thumb_h), interpolation=cv2.INTER_AREA)
            pair = np.hstack((source_thumb, effect_thumb))
            header = np.full((38, pair.shape[1], 4), (22, 27, 37, 255), np.uint8)
            cv2.putText(header, f"SOURCE  |  FLASH  {source_frame_index / fps:.2f}s  ({progress:.0%})",
                        (12, 26), cv2.FONT_HERSHEY_SIMPLEX, 0.52,
                        (245, 245, 245, 255), 1, cv2.LINE_AA)
            contact_rows.append(np.vstack((header, pair)))
        previous_output = output_luma
        previous_source_luma = source_luma
        previous_flash_delta = flash_delta
        previous_source_gray = source_gray
        processed_count += 1
    cap.release()
    if writer is not None:
        writer.release()
    if processed_count < 2:
        raise ValueError("Video range contains fewer than two decodable frames")
    cv2.imwrite(str(args.output / "flash-video-comparison.png"), np.vstack(contact_rows))
    temporal_pairs: dict[str, list[float]] = {}
    for key in ("adjacent_source_mean_abs_linear_luma_delta",
                "adjacent_output_mean_abs_linear_luma_delta",
                "adjacent_flash_contribution_mean_abs_delta",
                "motion_compensated_source_mean_abs_delta",
                "motion_compensated_flash_contribution_mean_abs_delta"):
        temporal_pairs[key] = [float(metric[key]) for metric in frame_metrics if key in metric]
    temporal_summary = {
        key: {"mean": float(np.mean(values)), "max": float(np.max(values)),
              "pair_count": len(values)}
        for key, values in temporal_pairs.items() if values
    }
    all_highlight_reset_frames = sorted(set(highlight_reset_frames) | set(detected_cut_frames))
    report = {
        "input_video": str(args.input.resolve()),
        "preset": str(args.preset.resolve()) if args.preset else None,
        "declared_frame_count": declared_count,
        "decoded_frame_count": processed_count,
        "width": output_width,
        "height": output_height,
        "source_fps": fps,
        "source_start_frame": start_frame,
        "source_end_frame_requested": end_frame,
        "source_start_seconds": start_frame / fps,
        "source_end_seconds_requested": end_frame / fps,
        "duration_seconds": duration,
        "effective_fps": actual_fps,
        "mode": args.mode,
        "source_position": list(args.source_position),
        "direction": list(args.direction),
        "strength": args.strength,
        "synthetic_exposure_stops": args.synthetic_exposure_stops,
        "scatter": args.scatter,
        "scatter_front_coupling": args.scatter_front_coupling,
        "veil_gain": args.veil_gain,
        "global_fill": args.global_fill,
        "front_duration": args.front_duration,
        "decay_rate_per_second": args.decay_rate_per_second,
        "highlight_rolloff": args.highlight_rolloff,
        "softness": args.softness,
        "temperature": list(args.temperature),
        "attack_duration": args.attack_duration,
        "peak_time": args.peak_time,
        "radial_falloff": args.radial_falloff,
        "highlight_threshold": args.highlight_threshold,
        "highlight_range": args.highlight_range,
        "highlight_percentiles": list(args.highlight_percentiles) if args.highlight_percentiles else None,
        "highlight_threshold_mode": (
            "frame_percentile_ema_with_resets"
            if args.highlight_percentiles and args.highlight_smoothing_seconds > 0.0 and all_highlight_reset_frames
            else "frame_percentile_ema" if args.highlight_percentiles and args.highlight_smoothing_seconds > 0.0
            else "frame_percentile" if args.highlight_percentiles else "fixed"),
        "highlight_smoothing_seconds": args.highlight_smoothing_seconds,
        "highlight_reset_frames": all_highlight_reset_frames,
        "highlight_manual_reset_frames": highlight_reset_frames,
        "highlight_cut_detection_enabled": args.detect_highlight_cuts,
        "highlight_cut_threshold": args.highlight_cut_threshold if args.detect_highlight_cuts else None,
        "highlight_cut_spatial_threshold": (
            args.highlight_cut_spatial_threshold if args.detect_highlight_cuts else None),
        "highlight_detected_cut_frames": detected_cut_frames,
        "highlight_detected_cut_scores": {str(k): v for k, v in detected_cut_scores.items()},
        "video_output": str(args.video_output.resolve()) if args.video_output else None,
        "png_frames_written": not args.no_png_frames,
        "audio_copied": False,
        "temporal_metrics_note": (
            "Source and effect contributions are reported separately. Adjacent source luma is not "
            "motion-compensated unless --motion-compensated-metrics is enabled; Farneback flow "
            "is approximate and invalid/out-of-range pixels are excluded. Flash contribution is "
            "output-minus-source linear luma."),
        "motion_compensated_metrics_enabled": args.motion_compensated_metrics,
        "flow_max_displacement_px": args.flow_max_displacement if args.motion_compensated_metrics else None,
        "temporal_summary": temporal_summary,
        "frames": frame_metrics,
    }
    with (args.output / "flash-video-metrics.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved video flash sequence to {args.output.resolve()} ({processed_count} frames)")


if __name__ == "__main__":
    main()
