"""Study clip-limited flash strength on a continuous natural video excerpt."""

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


def render(source: np.ndarray, progress: float, strength: float,
           highlight_rolloff: float = 0.78) -> np.ndarray:
    return flash_fill(
        source, progress, mode="radial", source_position=(0.10, 0.48),
        strength=strength, softness=0.20, temperature=(1.0, 0.91, 0.76),
        attack_duration=0.72, peak_time=0.78, decay_rate=2.4,
        scatter_strength=0.9, highlight_threshold=0.25, highlight_range=0.52,
        global_fill=0.015, front_duration=0.78, radial_falloff=2.7,
        highlight_rolloff=highlight_rolloff)


def new_clip_fraction(result: np.ndarray, source: np.ndarray) -> float:
    return float(np.mean((result[..., :3] >= 1.0) & (source[..., :3] < 1.0)))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "exposure-video-adaptation")
    parser.add_argument("--start-seconds", type=float, default=0.0)
    parser.add_argument("--end-seconds", type=float, default=4.0)
    parser.add_argument("--progress-start", type=float, default=0.38)
    parser.add_argument("--progress-end", type=float, default=0.70)
    parser.add_argument("--synthetic-exposure-stops", type=float, default=0.0,
                        help="uniform linear-light source exposure offset; not a sensor-noise simulation")
    parser.add_argument("--target-new-clip-fraction", type=float, default=0.08)
    parser.add_argument("--adaptation-safety-margin", type=float, default=0.02)
    parser.add_argument("--asymmetric-rise-tau", type=float, default=0.25)
    parser.add_argument("--asymmetric-fall-tau", type=float, default=0.05)
    parser.add_argument("--strength-search-iterations", type=int, default=5)
    parser.add_argument("--rolloffs", nargs="+", type=float, default=(0.78, 0.85, 0.90, 0.95, 1.0))
    parser.add_argument("--rolloff-safety-margin", type=float, default=0.02)
    parser.add_argument("--rolloff-search-iterations", type=int, default=5)
    parser.add_argument("--rolloff-rise-tau", type=float, default=0.08)
    parser.add_argument("--rolloff-fall-tau", type=float, default=0.25)
    parser.add_argument("--strengths", nargs="+", type=float,
                        default=(0.0, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.40,
                                 0.60, 0.80, 1.0, 1.2, 1.4, 1.6, 2.0, 2.4, 3.0,
                                 3.5, 4.0, 4.5, 5.0))
    parser.add_argument("--ema-taus", nargs="+", type=float, default=(0.10, 0.25))
    parser.add_argument("--max-dimension", type=int, default=240)
    args = parser.parse_args()
    if not args.input.exists():
        parser.error(f"input video does not exist: {args.input}")
    if args.end_seconds <= args.start_seconds or args.start_seconds < 0.0:
        parser.error("video range must be nonnegative and end after start")
    if not (0.0 <= args.progress_start <= 1.0 and 0.0 <= args.progress_end <= 1.0):
        parser.error("progress values must be in [0, 1]")
    if args.max_dimension < 64 or not (0.0 <= args.target_new_clip_fraction <= 1.0):
        parser.error("invalid max-dimension or clip target")
    if not (0.0 <= args.adaptation_safety_margin < args.target_new_clip_fraction):
        parser.error("adaptation safety margin must be smaller than the clip target")
    if args.asymmetric_rise_tau <= 0.0 or args.asymmetric_fall_tau <= 0.0:
        parser.error("asymmetric EMA time constants must be positive")
    if args.strength_search_iterations < 0:
        parser.error("strength-search-iterations must be nonnegative")
    if not (0.0 <= args.rolloff_safety_margin < args.target_new_clip_fraction):
        parser.error("rolloff safety margin must be smaller than clip target")
    if args.rolloff_search_iterations < 0 or args.rolloff_rise_tau <= 0.0 or args.rolloff_fall_tau <= 0.0:
        parser.error("invalid rolloff search or smoothing parameters")
    args.output.mkdir(parents=True, exist_ok=True)

    capture = cv2.VideoCapture(str(args.input))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {args.input}")
    fps = float(capture.get(cv2.CAP_PROP_FPS)) or 24.0
    declared_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    start_frame = max(0, int(round(args.start_seconds * fps)))
    end_frame = min(declared_count - 1, int(round(args.end_seconds * fps)))
    frame_count = end_frame - start_frame + 1
    if frame_count < 2:
        capture.release()
        parser.error("selected range must contain at least two frames")
    source_width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    source_height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    scale = min(1.0, args.max_dimension / max(source_width, source_height))
    width = max(1, round(source_width * scale))
    height = max(1, round(source_height * scale))
    strengths = sorted(set(max(0.0, float(value)) for value in args.strengths))
    taus = sorted(set(max(0.0, float(value)) for value in args.ema_taus))
    rolloffs = sorted(set(float(np.clip(value, 0.0, 1.0)) for value in args.rolloffs))
    if not strengths or not taus or not rolloffs:
        capture.release()
        parser.error("strengths, ema-taus, and rolloffs must not be empty")
    policies: dict[str, dict[str, object]] = {
        "framewise_clip_limited": {"previous_strength": None, "frames": [], "thumbnails": []},
        "fixed_from_first_frame": {"previous_strength": None, "frames": [], "thumbnails": []},
        "fixed_strength_adaptive_rolloff": {"previous_strength": None, "frames": [], "thumbnails": []},
        "fixed_strength_smoothed_rolloff": {"previous_strength": None, "frames": [], "thumbnails": []},
        "framewise_strength_adaptive_rolloff": {"previous_strength": None, "frames": [], "thumbnails": []},
        "framewise_strength_smoothed_rolloff": {"previous_strength": None, "frames": [], "thumbnails": []},
    }
    for tau in taus:
        policies[f"ema_tau_{tau:g}s"] = {"previous_strength": None, "frames": [], "thumbnails": []}
    policies["asymmetric_safe_ema"] = {"previous_strength": None, "frames": [], "thumbnails": []}
    sample_indices = {0, frame_count // 4, frame_count // 2,
                      (3 * frame_count) // 4, frame_count - 1}
    capture.set(cv2.CAP_PROP_POS_FRAMES, start_frame)
    try:
        for index in range(frame_count):
            ok, bgr = capture.read()
            if not ok:
                frame_count = index
                break
            if bgr.shape[1] != width or bgr.shape[0] != height:
                bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
            if args.synthetic_exposure_stops != 0.0:
                gain = 2.0 ** args.synthetic_exposure_stops
                rgb = np.clip(linear_to_srgb(srgb_to_linear(rgb) * gain), 0.0, 1.0)
            source = np.dstack((rgb, np.ones((height, width), np.float32)))
            progress = float(args.progress_start +
                             (args.progress_end - args.progress_start) * index / max(1, frame_count - 1))
            source_luma = linear_luma(rgb)
            candidate_outputs: dict[float, np.ndarray] = {}
            candidate_stats: list[dict[str, float | bool]] = []
            for strength in strengths:
                result = render(source, progress, strength)
                clip_fraction = new_clip_fraction(result, source)
                candidate_outputs[strength] = result
                candidate_stats.append({
                    "strength": strength,
                    "newly_clipped_channel_fraction": clip_fraction,
                    "mean_linear_luma_delta": float((linear_luma(result[..., :3]) - source_luma).mean()),
                    "within_clip_target": clip_fraction <= args.target_new_clip_fraction,
                })
            def choose_strength(target_fraction: float) -> tuple[float, np.ndarray]:
                eligible = [item for item in candidate_stats
                            if float(item["newly_clipped_channel_fraction"]) <= target_fraction]
                if eligible:
                    low = float(max(eligible, key=lambda item: float(item["strength"]))["strength"])
                    best = candidate_outputs[low]
                    higher = [float(item["strength"]) for item in candidate_stats
                              if float(item["strength"]) > low and
                              float(item["newly_clipped_channel_fraction"]) > target_fraction]
                    high = min(higher) if higher else low
                    if high <= low:
                        return low, best
                else:
                    low = strengths[0]
                    best = candidate_outputs[low]
                    high = strengths[1] if len(strengths) > 1 else low
                    if high <= low:
                        return low, best
                for _ in range(args.strength_search_iterations):
                    middle = 0.5 * (low + high)
                    candidate = render(source, progress, middle)
                    if new_clip_fraction(candidate, source) <= target_fraction:
                        low, best = middle, candidate
                    else:
                        high = middle
                return low, best

            selected_strength, selected_output = choose_strength(args.target_new_clip_fraction)
            safe_target_fraction = args.target_new_clip_fraction - args.adaptation_safety_margin
            safe_strength, safe_output = choose_strength(safe_target_fraction)

            frame_rolloff_outputs: dict[float, np.ndarray] = {}
            frame_rolloff_stats: list[tuple[float, float]] = []
            for rolloff in rolloffs:
                result = render(source, progress, selected_strength, rolloff)
                frame_rolloff_outputs[rolloff] = result
                frame_rolloff_stats.append((rolloff, new_clip_fraction(result, source)))
            eligible_frame_rolloffs = [item for item in frame_rolloff_stats
                                       if item[1] <= args.target_new_clip_fraction]
            frame_adaptive_rolloff = (min(eligible_frame_rolloffs, key=lambda item: item[0])[0]
                                      if eligible_frame_rolloffs else rolloffs[-1])
            frame_safe_target = args.target_new_clip_fraction - args.rolloff_safety_margin
            frame_low, frame_high = rolloffs[0], rolloffs[-1]
            if frame_rolloff_stats[0][1] <= frame_safe_target:
                frame_safe_rolloff_target = frame_low
            elif frame_rolloff_stats[-1][1] > frame_safe_target:
                frame_safe_rolloff_target = frame_high
            else:
                low, high = frame_low, frame_high
                for _ in range(args.rolloff_search_iterations):
                    middle = 0.5 * (low + high)
                    candidate = render(source, progress, selected_strength, middle)
                    if new_clip_fraction(candidate, source) <= frame_safe_target:
                        high = middle
                    else:
                        low = middle
                frame_safe_rolloff_target = high
            frame_smooth_state = policies["framewise_strength_smoothed_rolloff"]
            frame_previous_rolloff = frame_smooth_state["previous_strength"]
            if frame_previous_rolloff is None:
                frame_smooth_rolloff = frame_safe_rolloff_target
            else:
                tau = (args.rolloff_rise_tau if frame_safe_rolloff_target > float(frame_previous_rolloff)
                       else args.rolloff_fall_tau)
                alpha = 1.0 - np.exp(-1.0 / (fps * tau))
                frame_smooth_rolloff = float(frame_previous_rolloff) + alpha * (
                    frame_safe_rolloff_target - float(frame_previous_rolloff))
            frame_smooth_state["previous_strength"] = frame_smooth_rolloff
            frame_smooth_output = render(source, progress, selected_strength, frame_smooth_rolloff)
            fixed_state = policies["fixed_from_first_frame"]
            if fixed_state["previous_strength"] is None:
                fixed_state["previous_strength"] = selected_strength
            fixed_strength = float(fixed_state["previous_strength"])
            rolloff_outputs: dict[float, np.ndarray] = {}
            rolloff_stats: list[tuple[float, float]] = []
            for rolloff in rolloffs:
                result = render(source, progress, fixed_strength, rolloff)
                rolloff_outputs[rolloff] = result
                rolloff_stats.append((rolloff, new_clip_fraction(result, source)))
            eligible_rolloffs = [item for item in rolloff_stats
                                 if item[1] <= args.target_new_clip_fraction]
            adaptive_rolloff = (min(eligible_rolloffs, key=lambda item: item[0])[0]
                                if eligible_rolloffs else rolloffs[-1])
            safe_rolloff_target = args.target_new_clip_fraction - args.rolloff_safety_margin
            low_rolloff, high_rolloff = rolloffs[0], rolloffs[-1]
            low_result = rolloff_outputs[low_rolloff]
            high_result = rolloff_outputs[high_rolloff]
            if new_clip_fraction(low_result, source) <= safe_rolloff_target:
                safe_rolloff_target_value = low_rolloff
            elif new_clip_fraction(high_result, source) > safe_rolloff_target:
                safe_rolloff_target_value = high_rolloff
            else:
                low, high = low_rolloff, high_rolloff
                for _ in range(args.rolloff_search_iterations):
                    middle = 0.5 * (low + high)
                    middle_result = render(source, progress, fixed_strength, middle)
                    if new_clip_fraction(middle_result, source) <= safe_rolloff_target:
                        high = middle
                    else:
                        low = middle
                safe_rolloff_target_value = high
            smoothed_rolloff_state = policies["fixed_strength_smoothed_rolloff"]
            previous_rolloff = smoothed_rolloff_state["previous_strength"]
            if previous_rolloff is None:
                smoothed_rolloff = safe_rolloff_target_value
            else:
                tau = (args.rolloff_rise_tau if safe_rolloff_target_value > float(previous_rolloff)
                       else args.rolloff_fall_tau)
                alpha = 1.0 - np.exp(-1.0 / (fps * tau))
                smoothed_rolloff = float(previous_rolloff) + alpha * (
                    safe_rolloff_target_value - float(previous_rolloff))
            smoothed_rolloff_state["previous_strength"] = smoothed_rolloff
            smoothed_rolloff_output = render(source, progress, fixed_strength, smoothed_rolloff)
            policy_strengths = {
                "framewise_clip_limited": selected_strength,
                "fixed_from_first_frame": fixed_strength,
                "fixed_strength_adaptive_rolloff": fixed_strength,
                "fixed_strength_smoothed_rolloff": fixed_strength,
                "framewise_strength_adaptive_rolloff": selected_strength,
                "framewise_strength_smoothed_rolloff": selected_strength,
            }
            policy_targets = {
                "framewise_clip_limited": selected_strength,
                "fixed_from_first_frame": selected_strength,
                "fixed_strength_adaptive_rolloff": selected_strength,
                "fixed_strength_smoothed_rolloff": selected_strength,
                "framewise_strength_adaptive_rolloff": selected_strength,
                "framewise_strength_smoothed_rolloff": selected_strength,
            }
            policy_rolloffs = {name: 0.78 for name in policies}
            policy_rolloffs["fixed_strength_adaptive_rolloff"] = adaptive_rolloff
            policy_rolloffs["fixed_strength_smoothed_rolloff"] = smoothed_rolloff
            policy_rolloffs["framewise_strength_adaptive_rolloff"] = frame_adaptive_rolloff
            policy_rolloffs["framewise_strength_smoothed_rolloff"] = frame_smooth_rolloff
            for tau in taus:
                name = f"ema_tau_{tau:g}s"
                state = policies[name]
                previous = state["previous_strength"]
                if previous is None or tau <= 0.0:
                    effective = selected_strength
                else:
                    alpha = 1.0 - np.exp(-1.0 / (fps * tau))
                    effective = float(previous) + alpha * (selected_strength - float(previous))
                state["previous_strength"] = effective
                policy_strengths[name] = effective
                policy_targets[name] = selected_strength

            asymmetric_state = policies["asymmetric_safe_ema"]
            asymmetric_previous = asymmetric_state["previous_strength"]
            if asymmetric_previous is None:
                asymmetric_effective = safe_strength
            else:
                tau = (args.asymmetric_fall_tau if safe_strength < float(asymmetric_previous)
                       else args.asymmetric_rise_tau)
                alpha = 1.0 - np.exp(-1.0 / (fps * tau))
                asymmetric_effective = float(asymmetric_previous) + alpha * (
                    safe_strength - float(asymmetric_previous))
            asymmetric_state["previous_strength"] = asymmetric_effective
            policy_strengths["asymmetric_safe_ema"] = asymmetric_effective
            policy_targets["asymmetric_safe_ema"] = safe_strength
            policy_rolloffs["asymmetric_safe_ema"] = 0.78
            candidate_outputs[selected_strength] = selected_output
            candidate_outputs[safe_strength] = safe_output

            for name, state in policies.items():
                strength = policy_strengths[name]
                rolloff = policy_rolloffs[name]
                if name == "fixed_strength_adaptive_rolloff":
                    output = rolloff_outputs[rolloff]
                elif name == "fixed_strength_smoothed_rolloff":
                    output = smoothed_rolloff_output
                elif name == "framewise_strength_adaptive_rolloff":
                    output = frame_rolloff_outputs[rolloff]
                elif name == "framewise_strength_smoothed_rolloff":
                    output = frame_smooth_output
                else:
                    output = candidate_outputs.get(strength)
                    if output is None:
                        output = render(source, progress, strength, rolloff)
                output_luma = linear_luma(output[..., :3])
                clip_fraction = new_clip_fraction(output, source)
                rows = state["frames"]
                assert isinstance(rows, list)
                row: dict[str, float | int] = {
                    "frame": index, "source_frame": start_frame + index,
                    "time_seconds": index / fps, "progress": progress,
                    "source_p95_linear_luma": float(np.percentile(source_luma, 95)),
                    "target_strength": policy_targets[name], "effective_strength": strength,
                    "highlight_rolloff": rolloff,
                    "newly_clipped_channel_fraction": clip_fraction,
                    "mean_linear_luma_delta": float((output_luma - source_luma).mean()),
                }
                if name in ("fixed_strength_adaptive_rolloff", "fixed_strength_smoothed_rolloff"):
                    row["rolloff_candidate_clip_fractions"] = [
                        {"rolloff": candidate_rolloff, "newly_clipped_channel_fraction": candidate_clip}
                        for candidate_rolloff, candidate_clip in rolloff_stats
                    ]
                elif name in ("framewise_strength_adaptive_rolloff", "framewise_strength_smoothed_rolloff"):
                    row["rolloff_candidate_clip_fractions"] = [
                        {"rolloff": candidate_rolloff, "newly_clipped_channel_fraction": candidate_clip}
                        for candidate_rolloff, candidate_clip in frame_rolloff_stats
                    ]
                if rows:
                    previous_row = rows[-1]
                    row["adjacent_strength_delta"] = abs(strength - float(previous_row["effective_strength"]))
                    row["adjacent_rolloff_delta"] = abs(
                        rolloff - float(previous_row["highlight_rolloff"]))
                    row["adjacent_flash_contribution_delta"] = abs(
                        row["mean_linear_luma_delta"] - float(previous_row["mean_linear_luma_delta"]))
                rows.append(row)
                if index in sample_indices:
                    thumb = cv2.resize(cv2.cvtColor(to_u8(output), cv2.COLOR_RGBA2BGR),
                                       (240, max(1, round(height * 240 / width))),
                                       interpolation=cv2.INTER_AREA)
                    cv2.putText(thumb, f"t={index / fps:.2f}s s={strength:.2f} r={rolloff:.2f}",
                                (5, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.34,
                                (255, 255, 255), 1, cv2.LINE_AA)
                    thumbnails = state["thumbnails"]
                    assert isinstance(thumbnails, list)
                    thumbnails.append(thumb)
    finally:
        capture.release()

    summaries = []
    contact_rows = []
    for name, state in policies.items():
        rows = state["frames"]
        thumbnails = state["thumbnails"]
        assert isinstance(rows, list) and rows
        strengths_used = [float(row["effective_strength"]) for row in rows]
        adjacent_strength = [float(row["adjacent_strength_delta"]) for row in rows
                             if "adjacent_strength_delta" in row]
        adjacent_rolloff = [float(row["adjacent_rolloff_delta"]) for row in rows
                            if "adjacent_rolloff_delta" in row]
        rolloff_values = [float(row["highlight_rolloff"]) for row in rows]
        clip_values = [float(row["newly_clipped_channel_fraction"]) for row in rows]
        summaries.append({
            "policy": name,
            "mean_strength": float(np.mean(strengths_used)),
            "strength_min": float(np.min(strengths_used)),
            "strength_max": float(np.max(strengths_used)),
            "mean_adjacent_strength_delta": float(np.mean(adjacent_strength)) if adjacent_strength else 0.0,
            "max_adjacent_strength_delta": float(np.max(adjacent_strength)) if adjacent_strength else 0.0,
            "mean_highlight_rolloff": float(np.mean(rolloff_values)),
            "max_adjacent_highlight_rolloff_delta": float(np.max(adjacent_rolloff))
                if adjacent_rolloff else 0.0,
            "mean_newly_clipped_channel_fraction": float(np.mean(clip_values)),
            "max_newly_clipped_channel_fraction": float(np.max(clip_values)),
            "frames_over_clip_target": int(sum(value > args.target_new_clip_fraction for value in clip_values)),
            "mean_flash_luma_delta": float(np.mean([float(row["mean_linear_luma_delta"]) for row in rows])),
            "frames": rows,
        })
        while len(thumbnails) < len(sample_indices):
            thumbnails.append(np.zeros_like(thumbnails[0]))
        header = np.full((32, 240 * len(thumbnails), 3), (22, 27, 37), np.uint8)
        cv2.putText(header, name, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.48,
                    (245, 245, 245), 1, cv2.LINE_AA)
        contact_rows.append(np.vstack((header, np.hstack(thumbnails))))
    report = {
        "experiment": "clip-limited strength adaptation on natural continuous footage",
        "input_video": str(args.input.resolve()),
        "source_frame_range": [start_frame, start_frame + frame_count - 1],
        "frame_count": frame_count, "fps": fps,
        "progress_range": [args.progress_start, args.progress_end],
        "synthetic_exposure_stops": args.synthetic_exposure_stops,
        "strength_candidates": strengths, "target_new_clip_fraction": args.target_new_clip_fraction,
        "ema_taus_seconds": taus,
        "strength_search_iterations": args.strength_search_iterations,
        "rolloff_candidates": rolloffs,
        "rolloff_safety_margin": args.rolloff_safety_margin,
        "safe_rolloff_clip_target": args.target_new_clip_fraction - args.rolloff_safety_margin,
        "rolloff_search_iterations": args.rolloff_search_iterations,
        "rolloff_smoothing_taus_seconds": {
            "rise": args.rolloff_rise_tau,
            "fall": args.rolloff_fall_tau,
        },
        "adaptation_safety_margin": args.adaptation_safety_margin,
        "safe_adaptation_clip_target": safe_target_fraction,
        "asymmetric_ema_taus_seconds": {
            "rise": args.asymmetric_rise_tau,
            "fall": args.asymmetric_fall_tau,
        },
        "limitations": [
            "The selected range is a single continuous snow/bridge shot with no cut reset test.",
            "Per-frame candidate selection is a clip-limit heuristic, not an aesthetic optimum.",
            "Uncompensated temporal luma deltas include camera/object motion and are descriptive only.",
            "The analysis is SDR at reduced resolution; HDR and full-resolution appearance remain untested.",
        ],
        "policies": summaries,
    }
    (args.output / "flash-exposure-video-adaptation-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    cv2.imwrite(str(args.output / "flash-exposure-video-adaptation-contact-sheet.png"),
                np.vstack(contact_rows))
    print(f"Saved continuous exposure adaptation study to {args.output.resolve()}")


if __name__ == "__main__":
    main()
