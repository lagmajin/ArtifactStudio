"""Interactive video-frame preview for the independent flash-fill R&D lab."""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from flash_cut_detection import grayscale_cut_metrics
from glow_lab import srgb_to_linear, to_u8
from flash_preset import load_preset, save_preset


def temperature_from_slider(value: int) -> tuple[float, float, float]:
    warm = np.asarray((1.0, 0.84, 0.62), np.float32)
    neutral = np.ones(3, np.float32)
    cool = np.asarray((0.66, 0.82, 1.0), np.float32)
    t = float(np.clip(value, 0, 200)) / 100.0
    color = warm * (1.0 - t) + neutral * t if t <= 1.0 else neutral * (2.0 - t) + cool * (t - 1.0)
    return tuple(float(v) for v in color)


def make_view(source: np.ndarray, effected: np.ndarray, mode: str,
              wipe_percent: int, label: str) -> np.ndarray:
    source_bgr = cv2.cvtColor(to_u8(source), cv2.COLOR_RGBA2BGR)
    effect_bgr = cv2.cvtColor(to_u8(effected), cv2.COLOR_RGBA2BGR)
    h, w = source_bgr.shape[:2]
    if mode == "source":
        view = source_bgr
    elif mode == "flash":
        view = effect_bgr
    elif mode == "split":
        gutter = np.full((h, 12, 3), (36, 39, 46), np.uint8)
        view = np.hstack((source_bgr, gutter, effect_bgr))
        cv2.putText(view, "SOURCE", (12, 28), cv2.FONT_HERSHEY_SIMPLEX,
                    0.62, (245, 245, 245), 1, cv2.LINE_AA)
        cv2.putText(view, "FLASH", (w + 24, 28), cv2.FONT_HERSHEY_SIMPLEX,
                    0.62, (245, 245, 245), 1, cv2.LINE_AA)
    else:
        split_x = int(round(w * np.clip(wipe_percent, 0, 100) / 100.0))
        view = effect_bgr.copy()
        view[:, :split_x] = source_bgr[:, :split_x]
        cv2.line(view, (split_x, 0), (split_x, h), (245, 245, 245), 2, cv2.LINE_AA)
        cv2.putText(view, "SOURCE", (12, 28), cv2.FONT_HERSHEY_SIMPLEX,
                    0.62, (245, 245, 245), 1, cv2.LINE_AA)
        cv2.putText(view, "FLASH", (min(w - 90, split_x + 12), 28), cv2.FONT_HERSHEY_SIMPLEX,
                    0.62, (245, 245, 245), 1, cv2.LINE_AA)
    cv2.rectangle(view, (0, h - 26), (view.shape[1], h), (18, 21, 28), -1)
    cv2.putText(view, label, (10, h - 8), cv2.FONT_HERSHEY_SIMPLEX,
                0.46, (235, 238, 244), 1, cv2.LINE_AA)
    return view


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="input video clip")
    parser.add_argument("--start-seconds", type=float, default=0.0)
    parser.add_argument("--end-seconds", type=float)
    parser.add_argument("--highlight-reset-seconds", nargs="*", type=float, default=(),
                        help="known shot-cut times in source seconds; reset percentile EMA at these frames")
    parser.add_argument("--detect-highlight-cuts", action="store_true",
                        help="also reset percentile EMA on large grayscale-histogram changes during forward playback")
    parser.add_argument("--highlight-cut-threshold", type=float, default=0.35,
                        help="Bhattacharyya grayscale-histogram distance for opt-in cut reset (0..1)")
    parser.add_argument("--highlight-cut-spatial-threshold", type=float, default=0.12,
                        help="thumbnail mean-absolute grayscale difference for opt-in cut reset (0..1)")
    parser.add_argument("--max-dimension", type=int, default=960)
    parser.add_argument("--preset", type=Path, help="load shared flash-look JSON settings")
    parser.add_argument("--save-preset", type=Path,
                        help="save current interactive settings here when pressing P")
    parser.add_argument("--snapshot-output", type=Path,
                        help="write one default wipe preview and exit without opening a window")
    parser.add_argument("--snapshot-progress", type=float, default=0.68,
                        help="normalized effect progress for --snapshot-output")
    parser.add_argument("--snapshot-view", choices=("wipe", "split", "source", "flash"),
                        default="wipe", help="comparison layout for --snapshot-output")
    args = parser.parse_args()
    settings = load_preset(args.preset)
    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS))
    count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    if fps <= 0.0:
        fps = 24.0
    if count < 2:
        capture.release()
        raise ValueError("Video must report at least two frames")
    if args.max_dimension < 64:
        capture.release()
        parser.error("max-dimension must be at least 64")
    if not 0.0 < args.highlight_cut_threshold <= 1.0:
        capture.release()
        parser.error("highlight-cut-threshold must be greater than 0 and at most 1")
    if not 0.0 < args.highlight_cut_spatial_threshold <= 1.0:
        capture.release()
        parser.error("highlight-cut-spatial-threshold must be greater than 0 and at most 1")
    if args.start_seconds < 0.0 or (args.end_seconds is not None and
                                    args.end_seconds <= args.start_seconds):
        capture.release()
        parser.error("video range must be nonnegative and end after start")
    source_duration = (count - 1) / fps
    if args.end_seconds is not None and args.end_seconds > source_duration + 0.5 / fps:
        capture.release()
        parser.error("end-seconds exceeds input video duration")
    start_frame = int(round(args.start_seconds * fps))
    end_frame = count - 1 if args.end_seconds is None else min(
        count - 1, int(round(args.end_seconds * fps)))
    if start_frame >= end_frame:
        capture.release()
        parser.error("selected range must contain at least two frames")
    selected_count = end_frame - start_frame + 1
    selected_end_seconds = end_frame / fps
    if any(t < start_frame / fps or t > selected_end_seconds
           for t in args.highlight_reset_seconds):
        capture.release()
        parser.error("highlight reset seconds must lie inside the selected source range")
    highlight_reset_indices = {
        int(round(t * fps)) - start_frame for t in args.highlight_reset_seconds
    }
    source_width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    source_height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    scale = min(1.0, args.max_dimension / max(source_height, source_width))
    width = max(1, int(round(source_width * scale)))
    height = max(1, int(round(source_height * scale)))

    if args.snapshot_output is not None:
        snapshot_progress = float(np.clip(args.snapshot_progress, 0.0, 1.0))
        frame_index = int(round(snapshot_progress * (selected_count - 1)))
        snapshot_progress = frame_index / (selected_count - 1)
        capture.set(cv2.CAP_PROP_POS_FRAMES, start_frame + frame_index)
        ok, bgr = capture.read()
        capture.release()
        if not ok:
            raise OSError(f"Could not read preview frame {start_frame + frame_index}")
        if bgr.shape[1] != width or bgr.shape[0] != height:
            bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
        source = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
        duration = (selected_count - 1) / fps
        effected = flash_fill(
            source, snapshot_progress, mode=settings["mode"],
            source_position=tuple(settings["source_position"]),
            direction=tuple(settings["direction"]),
            strength=settings["strength"], softness=settings["softness"],
            temperature=tuple(settings["temperature"]),
            attack_duration=settings["attack_duration"], peak_time=settings["peak_time"],
            decay_rate=settings["decay_rate_per_second"] * duration,
            scatter_strength=settings["scatter"],
            scatter_front_coupling=settings["scatter_front_coupling"],
            veil_gain=settings["veil_gain"],
            highlight_threshold=settings["highlight_threshold"],
            highlight_range=settings["highlight_range"],
            global_fill=settings["global_fill"], front_duration=settings["front_duration"],
            radial_falloff=settings["radial_falloff"],
            highlight_rolloff=settings["highlight_rolloff"])
        source_seconds = (start_frame + frame_index) / fps
        view = make_view(source, effected, args.snapshot_view, 50,
                         f"{source_seconds:.2f}s | effect {snapshot_progress:.0%} | "
                         f"{settings['mode']} | "
                         f"{args.snapshot_view.upper()}")
        args.snapshot_output.parent.mkdir(parents=True, exist_ok=True)
        if not cv2.imwrite(str(args.snapshot_output), view):
            raise OSError(f"Could not write preview snapshot: {args.snapshot_output}")
        print(f"Saved preview snapshot to {args.snapshot_output.resolve()}")
        return

    window = "Glow Lab / Flash Video Preview"
    cv2.namedWindow(window, cv2.WINDOW_NORMAL)
    trackbar = lambda name, value, maximum: cv2.createTrackbar(
        name, window, int(value), int(maximum), lambda _value: None)
    trackbar("Frame", 0, selected_count - 1)
    trackbar("Wipe %", 50, 100)
    trackbar("Strength x0.1", round(settings["strength"] * 10), 80)
    trackbar("Softness x0.01", round(settings["softness"] * 100), 50)
    trackbar("Source X %", round(settings["source_position"][0] * 100), 100)
    trackbar("Source Y %", round(settings["source_position"][1] * 100), 100)
    trackbar("Direction X %", round((settings["direction"][0] + 1.0) * 100), 200)
    trackbar("Direction Y %", round((settings["direction"][1] + 1.0) * 100), 200)
    trackbar("Scatter x0.1", round(settings["scatter"] * 10), 100)
    trackbar("Scatter Front %", round(settings["scatter_front_coupling"] * 100), 100)
    trackbar("Veil gain x1000", round(settings["veil_gain"] * 1000), 50)
    trackbar("Global Fill %", round(settings["global_fill"] * 100), 100)
    trackbar("Radial Falloff x0.1", round(settings["radial_falloff"] * 10), 100)
    trackbar("Highlight Threshold %", round(settings["highlight_threshold"] * 100), 100)
    trackbar("Highlight Range %", round(settings["highlight_range"] * 100), 100)
    trackbar("Highlight Adapt 0/1/2", settings["highlight_adaptive_mode"], 2)
    trackbar("Threshold Percentile", round(settings["highlight_threshold_percentile"]), 99)
    trackbar("Range End Percentile", round(settings["highlight_range_end_percentile"]), 100)
    trackbar("Highlight Smooth x0.1s", round(settings["highlight_smoothing_seconds"] * 10), 50)
    trackbar("Front %", round(settings["front_duration"] * 100), 100)
    trackbar("Decay /s x0.1", round(settings["decay_rate_per_second"] * 10), 300)
    trackbar("Rolloff %", round(settings["highlight_rolloff"] * 100), 100)
    trackbar("Temperature", settings["temperature_slider"], 200)

    view_mode = "wipe"
    spatial_mode = settings["mode"]
    playing = False
    last_step = time.perf_counter()
    cached_index = -1
    cached_source: np.ndarray | None = None
    cached_effect: np.ndarray | None = None
    last_render_key: tuple[object, ...] | None = None
    previous_cut_frame: np.ndarray | None = None
    previous_cut_frame_index = -1
    detected_cut_indices: set[int] = set()
    adapt_state_index = -1
    adapt_state_config: tuple[int, int, int, int] | None = None
    adapt_state_threshold: float | None = None
    adapt_state_range: float | None = None
    try:
        while True:
            now = time.perf_counter()
            current_index = cv2.getTrackbarPos("Frame", window)
            if playing and now - last_step >= 1.0 / max(fps, 1.0):
                current_index = (current_index + 1) % selected_count
                cv2.setTrackbarPos("Frame", window, current_index)
                last_step = now
            if current_index != cached_index:
                capture.set(cv2.CAP_PROP_POS_FRAMES, start_frame + current_index)
                ok, bgr = capture.read()
                if not ok:
                    break
                if bgr.shape[1] != width or bgr.shape[0] != height:
                    bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
                if args.detect_highlight_cuts:
                    if (previous_cut_frame is not None and
                            current_index == previous_cut_frame_index + 1):
                        hist_score, spatial_score = grayscale_cut_metrics(previous_cut_frame, bgr)
                        if (hist_score >= args.highlight_cut_threshold or
                                spatial_score >= args.highlight_cut_spatial_threshold):
                            detected_cut_indices.add(current_index)
                    previous_cut_frame = bgr
                    previous_cut_frame_index = current_index
                rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
                cached_source = np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))
                cached_index = current_index
            if cached_source is None:
                break
            progress = current_index / (selected_count - 1)
            strength = cv2.getTrackbarPos("Strength x0.1", window) / 10.0
            softness = max(0.01, cv2.getTrackbarPos("Softness x0.01", window) / 100.0)
            source_position = (cv2.getTrackbarPos("Source X %", window) / 100.0,
                               cv2.getTrackbarPos("Source Y %", window) / 100.0)
            direction = (cv2.getTrackbarPos("Direction X %", window) / 100.0 - 1.0,
                         cv2.getTrackbarPos("Direction Y %", window) / 100.0 - 1.0)
            if np.hypot(*direction) < 1.0e-3:
                direction = (1.0, 0.0)
            scatter = cv2.getTrackbarPos("Scatter x0.1", window) / 10.0
            scatter_front_coupling = cv2.getTrackbarPos("Scatter Front %", window) / 100.0
            veil_gain = cv2.getTrackbarPos("Veil gain x1000", window) / 1000.0
            global_fill = cv2.getTrackbarPos("Global Fill %", window) / 100.0
            radial_falloff = cv2.getTrackbarPos("Radial Falloff x0.1", window) / 10.0
            highlight_threshold = cv2.getTrackbarPos("Highlight Threshold %", window) / 100.0
            highlight_range = max(0.01, cv2.getTrackbarPos("Highlight Range %", window) / 100.0)
            adaptive_mode = cv2.getTrackbarPos("Highlight Adapt 0/1/2", window)
            threshold_percentile = cv2.getTrackbarPos("Threshold Percentile", window)
            range_end_percentile = max(
                threshold_percentile + 1,
                cv2.getTrackbarPos("Range End Percentile", window))
            range_end_percentile = min(100, range_end_percentile)
            smoothing_seconds = cv2.getTrackbarPos("Highlight Smooth x0.1s", window) / 10.0
            adapt_config = (adaptive_mode, threshold_percentile,
                            range_end_percentile, round(smoothing_seconds * 10))
            if adaptive_mode > 0:
                if (current_index != adapt_state_index or adapt_config != adapt_state_config or
                        adapt_state_threshold is None or adapt_state_range is None):
                    source_linear = srgb_to_linear(cached_source[..., :3])
                    source_luma = (source_linear[..., 0] * 0.2126 +
                                   source_linear[..., 1] * 0.7152 +
                                   source_linear[..., 2] * 0.0722)
                    target_threshold, range_end = np.percentile(
                        source_luma, (threshold_percentile, range_end_percentile))
                    target_threshold = float(target_threshold)
                    target_range = max(0.01, float(range_end - target_threshold))
                    if (adaptive_mode == 2 and smoothing_seconds > 0.0 and
                            adapt_state_index == current_index - 1 and
                            current_index not in highlight_reset_indices and
                            current_index not in detected_cut_indices and
                            adapt_state_config == adapt_config and
                            adapt_state_threshold is not None and adapt_state_range is not None):
                        alpha = 1.0 - np.exp(-1.0 / (fps * smoothing_seconds))
                        adapt_state_threshold += alpha * (target_threshold - adapt_state_threshold)
                        adapt_state_range += alpha * (target_range - adapt_state_range)
                    else:
                        adapt_state_threshold = target_threshold
                        adapt_state_range = target_range
                    adapt_state_index = current_index
                    adapt_state_config = adapt_config
                highlight_threshold = adapt_state_threshold
                highlight_range = adapt_state_range
            front_duration = max(0.05, cv2.getTrackbarPos("Front %", window) / 100.0)
            decay_per_second = cv2.getTrackbarPos("Decay /s x0.1", window) / 10.0
            rolloff = cv2.getTrackbarPos("Rolloff %", window) / 100.0
            temperature = temperature_from_slider(cv2.getTrackbarPos("Temperature", window))
            duration = (selected_count - 1) / fps
            render_key = (current_index, spatial_mode, source_position, direction, strength, softness,
                          temperature, scatter, scatter_front_coupling, veil_gain, global_fill, radial_falloff,
                          highlight_threshold, highlight_range, adapt_config, front_duration,
                          decay_per_second, rolloff)
            if render_key != last_render_key or cached_effect is None:
                cached_effect = flash_fill(
                    cached_source, progress, mode=spatial_mode,
                    source_position=source_position, direction=direction,
                    strength=strength, softness=softness,
                    temperature=temperature, attack_duration=settings["attack_duration"],
                    peak_time=settings["peak_time"],
                    decay_rate=decay_per_second * duration, scatter_strength=scatter,
                    scatter_front_coupling=scatter_front_coupling,
                    veil_gain=veil_gain,
                    highlight_threshold=highlight_threshold,
                    highlight_range=highlight_range,
                    global_fill=global_fill, front_duration=front_duration,
                    radial_falloff=radial_falloff, highlight_rolloff=rolloff)
                last_render_key = render_key
            effected = cached_effect
            source_seconds = (start_frame + current_index) / fps
            label = (f"{source_seconds:.2f}s | effect {progress:.0%} | {spatial_mode} | "
                     f"{'PCTL' if adaptive_mode == 1 else 'EMA' if adaptive_mode == 2 else 'FIXED'} | "
                     f"{'AUTO CUT | ' if current_index in detected_cut_indices else ''}"
                     f"{'CUT RESET | ' if current_index in highlight_reset_indices else ''}"
                     f"{view_mode.upper()} | {'PLAY' if playing else 'PAUSE'} | "
                     "Space play  J/L step  W/S/A/F view  M mode  Q close")
            wipe_percent = cv2.getTrackbarPos("Wipe %", window)
            view = make_view(cached_source, effected, view_mode, wipe_percent, label)
            if view.shape[1] > 1280:
                view = cv2.resize(view, (1280, int(round(view.shape[0] * 1280 / view.shape[1]))),
                                  interpolation=cv2.INTER_AREA)
            cv2.imshow(window, view)
            key = cv2.waitKeyEx(1)
            if key in (27, ord("q"), ord("Q")):
                break
            if key == ord(" "):
                playing = not playing
                last_step = time.perf_counter()
            elif key in (ord("j"), ord("J"), 2424832):
                playing = False
                current_index = max(0, current_index - 1)
                cv2.setTrackbarPos("Frame", window, current_index)
            elif key in (ord("l"), ord("L"), 2555904):
                playing = False
                current_index = min(selected_count - 1, current_index + 1)
                cv2.setTrackbarPos("Frame", window, current_index)
            elif key in (ord("w"), ord("W")):
                view_mode = "wipe"
            elif key in (ord("s"), ord("S")):
                view_mode = "split"
            elif key in (ord("a"), ord("A")):
                view_mode = "source"
            elif key in (ord("f"), ord("F")):
                view_mode = "flash"
            elif key in (ord("m"), ord("M")):
                spatial_mode = "directional" if spatial_mode == "radial" else "radial"
            elif key in (ord("p"), ord("P")) and args.save_preset is not None:
                save_preset(args.save_preset, {
                    "mode": spatial_mode,
                    "source_position": [cv2.getTrackbarPos("Source X %", window) / 100.0,
                                        cv2.getTrackbarPos("Source Y %", window) / 100.0],
                    "direction": [
                        cv2.getTrackbarPos("Direction X %", window) / 100.0 - 1.0,
                        cv2.getTrackbarPos("Direction Y %", window) / 100.0 - 1.0,
                    ],
                    "strength": cv2.getTrackbarPos("Strength x0.1", window) / 10.0,
                    "softness": cv2.getTrackbarPos("Softness x0.01", window) / 100.0,
                    "scatter": cv2.getTrackbarPos("Scatter x0.1", window) / 10.0,
                    "scatter_front_coupling": (
                        cv2.getTrackbarPos("Scatter Front %", window) / 100.0),
                    "veil_gain": cv2.getTrackbarPos("Veil gain x1000", window) / 1000.0,
                    "global_fill": cv2.getTrackbarPos("Global Fill %", window) / 100.0,
                    "radial_falloff": cv2.getTrackbarPos("Radial Falloff x0.1", window) / 10.0,
                    "highlight_threshold": cv2.getTrackbarPos("Highlight Threshold %", window) / 100.0,
                    "highlight_range": max(0.01, cv2.getTrackbarPos("Highlight Range %", window) / 100.0),
                    "highlight_adaptive_mode": cv2.getTrackbarPos("Highlight Adapt 0/1/2", window),
                    "highlight_threshold_percentile": cv2.getTrackbarPos("Threshold Percentile", window),
                    "highlight_range_end_percentile": max(
                        cv2.getTrackbarPos("Threshold Percentile", window) + 1,
                        cv2.getTrackbarPos("Range End Percentile", window)),
                    "highlight_smoothing_seconds": (
                        cv2.getTrackbarPos("Highlight Smooth x0.1s", window) / 10.0),
                    "front_duration": cv2.getTrackbarPos("Front %", window) / 100.0,
                    "decay_rate_per_second": cv2.getTrackbarPos("Decay /s x0.1", window) / 10.0,
                    "highlight_rolloff": cv2.getTrackbarPos("Rolloff %", window) / 100.0,
                    "temperature_slider": cv2.getTrackbarPos("Temperature", window),
                    "temperature": list(temperature_from_slider(
                        cv2.getTrackbarPos("Temperature", window))),
                    "attack_duration": settings["attack_duration"],
                    "peak_time": settings["peak_time"],
                })
    finally:
        capture.release()
        cv2.destroyWindow(window)


if __name__ == "__main__":
    main()
