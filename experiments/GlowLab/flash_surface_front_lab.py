"""Compare a traveling flash front with local lighting on known surfaces."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from flash_shadow_projection_lab import shade_known_surfaces
from flash_surface_lab import make_surface_chart
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def read_video_frame(path: Path, index: int, max_dimension: int) -> np.ndarray:
    capture = cv2.VideoCapture(str(path))
    if not capture.isOpened():
        raise OSError(f"Could not open video: {path}")
    try:
        capture.set(cv2.CAP_PROP_POS_FRAMES, index)
        ok, bgr = capture.read()
        if not ok:
            raise OSError(f"Could not read frame {index} from {path}")
        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB).astype(np.float32) / 255.0
    finally:
        capture.release()
    scale = min(1.0, max_dimension / max(rgb.shape[:2]))
    if scale < 1.0:
        rgb = cv2.resize(rgb, (round(rgb.shape[1] * scale), round(rgb.shape[0] * scale)),
                         interpolation=cv2.INTER_AREA)
    return np.dstack((rgb, np.ones(rgb.shape[:2], np.float32)))


def fit_rgba(image: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    width, height = size
    rgb = cv2.resize(image[..., :3], size, interpolation=cv2.INTER_AREA)
    alpha = cv2.resize(image[..., 3], size, interpolation=cv2.INTER_AREA)
    return np.dstack((rgb, alpha))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--video", type=Path,
                        default=Path("tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4"))
    parser.add_argument("--frame-index", type=int, default=24)
    parser.add_argument("--progress", type=float, default=0.56)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "surface-front")
    parser.add_argument("--strength", type=float, default=0.6)
    parser.add_argument("--softness", type=float, default=0.20)
    parser.add_argument("--scatter", type=float, default=0.9)
    parser.add_argument("--depth-front-strength", type=float, default=0.18)
    parser.add_argument("--target-new-clip-fraction", type=float, default=0.08)
    parser.add_argument("--light-start", nargs=2, type=float, default=(120.0, 150.0))
    parser.add_argument("--light-end", nargs=2, type=float, default=(920.0, 120.0))
    parser.add_argument("--light-height", type=float, default=420.0)
    parser.add_argument("--max-dimension", type=int, default=720)
    args = parser.parse_args()
    if not args.video.exists():
        parser.error(f"video does not exist: {args.video}")
    if args.frame_index < 0 or args.max_dimension < 128:
        parser.error("frame-index must be nonnegative and max-dimension at least 128")
    if not (0.0 <= args.progress <= 1.0):
        parser.error("progress must be between 0 and 1")
    args.output.mkdir(parents=True, exist_ok=True)

    source_rgba, _, _ = make_surface_chart()
    h, w = source_rgba.shape[:2]
    start = np.asarray(args.light_start, np.float32)
    end = np.asarray(args.light_end, np.float32)
    light_xy = tuple((start + (end - start) * args.progress).tolist())
    source_linear = srgb_to_linear(source_rgba[..., :3])
    chart_rgba = np.dstack((source_rgba[..., :3], source_rgba[..., 3]))
    chart_depth = np.zeros((h, w), np.float32)
    chart_depth[(np.indices((h, w))[1] >= 220) & (np.indices((h, w))[1] <= 702) &
                (np.indices((h, w))[0] >= 212) & (np.indices((h, w))[0] <= 604)] = 0.38
    sphere_y, sphere_x = np.ogrid[:h, :w]
    chart_depth[((sphere_x - 918.0) ** 2 + (sphere_y - 380.0) ** 2) <= 145.0 ** 2] = 0.68
    chart_source_position = (light_xy[0] / max(w - 1, 1), light_xy[1] / max(h - 1, 1))
    front = flash_fill(
        chart_rgba, args.progress, mode="radial", source_position=chart_source_position,
        strength=args.strength, softness=args.softness, temperature=(1.0, 0.91, 0.76),
        attack_duration=0.72, peak_time=0.78, decay_rate=2.4, scatter_strength=args.scatter,
        highlight_threshold=0.25, highlight_range=0.52, global_fill=0.015,
        front_duration=0.78, highlight_rolloff=0.78, depth_map=chart_depth,
        depth_strength=0.0, depth_front_strength=args.depth_front_strength)
    flat_front = flash_fill(
        chart_rgba, args.progress, mode="radial", source_position=chart_source_position,
        strength=args.strength, softness=args.softness, temperature=(1.0, 0.91, 0.76),
        attack_duration=0.72, peak_time=0.78, decay_rate=2.4, scatter_strength=args.scatter,
        highlight_threshold=0.25, highlight_range=0.52, global_fill=0.015,
        front_duration=0.78, highlight_rolloff=0.78)
    flat_front_luma = linear_luma(srgb_to_linear(flat_front[..., :3]))
    depth_sensitivity = []
    for depth_strength in (0.0, 0.18, 0.36, 0.72):
        depth_variant = flash_fill(
            chart_rgba, args.progress, mode="radial", source_position=chart_source_position,
            strength=args.strength, softness=args.softness, temperature=(1.0, 0.91, 0.76),
            attack_duration=0.72, peak_time=0.78, decay_rate=2.4,
            scatter_strength=args.scatter, highlight_threshold=0.25, highlight_range=0.52,
            global_fill=0.015, front_duration=0.78, highlight_rolloff=0.78,
            depth_map=chart_depth, depth_strength=0.0, depth_front_strength=depth_strength)
        variant_luma = linear_luma(srgb_to_linear(depth_variant[..., :3]))
        depth_sensitivity.append({
            "depth_front_strength": depth_strength,
            "mean_abs_luma_delta_from_flat": float(np.abs(variant_luma - flat_front_luma).mean()),
            "newly_clipped_fraction": float(np.mean(
                (depth_variant[..., :3] >= 1.0) & (chart_rgba[..., :3] < 1.0))),
        })
    front_linear = srgb_to_linear(np.maximum(front[..., :3], 0.0))
    surface_lit, surface_metrics = shade_known_surfaces(
        source_linear, light_xy, strength=1.6, progress=args.progress)
    surface_linear = surface_lit[..., :3]
    front_only = np.dstack((front[..., :3], np.ones((h, w), np.float32)))
    surface_only = np.dstack((np.clip(linear_to_srgb(np.maximum(surface_linear, 0.0)), 0.0, 1.0),
                              np.ones((h, w), np.float32)))
    combined_linear = surface_linear + np.maximum(front_linear - source_linear, 0.0)
    combined = np.dstack((np.clip(linear_to_srgb(np.maximum(combined_linear, 0.0)), 0.0, 1.0),
                          np.ones((h, w), np.float32)))

    real_source = read_video_frame(args.video, args.frame_index, args.max_dimension)
    real_strengths = sorted(set((0.1, 0.2, 0.3, 0.4, args.strength, 0.8, 1.2, 1.6)))
    real_fronts: dict[float, np.ndarray] = {}
    for real_strength in sorted(set(real_strengths)):
        real_fronts[real_strength] = flash_fill(
            real_source, args.progress, mode="radial", source_position=(0.10, 0.48),
            strength=real_strength, softness=args.softness, temperature=(1.0, 0.91, 0.76),
            attack_duration=0.72, peak_time=0.78, decay_rate=2.4, scatter_strength=args.scatter,
            highlight_threshold=0.25, highlight_range=0.52, global_fill=0.015,
            front_duration=0.78, radial_falloff=2.7, highlight_rolloff=0.78)
    real_front = real_fronts[args.strength]
    real_source_luma = linear_luma(srgb_to_linear(real_source[..., :3]))
    real_strength_metrics = []
    for real_strength, candidate in real_fronts.items():
        candidate_luma = linear_luma(srgb_to_linear(candidate[..., :3]))
        real_strength_metrics.append({
            "strength": real_strength,
            "p95_linear_luma": float(np.percentile(candidate_luma, 95)),
            "newly_clipped_channel_fraction": float(np.mean(
                (candidate[..., :3] >= 1.0) & (real_source[..., :3] < 1.0))),
            "mean_linear_luma_delta": float((candidate_luma - real_source_luma).mean()),
        })
    real_front_luma = linear_luma(srgb_to_linear(real_front[..., :3]))
    eligible_real = [item for item in real_strength_metrics
                     if item["newly_clipped_channel_fraction"] <= args.target_new_clip_fraction]
    selected_real_strength = max(eligible_real, key=lambda item: item["strength"])["strength"] \
        if eligible_real else None
    selected_real_front = real_fronts.get(selected_real_strength) if selected_real_strength is not None else None

    preview_w = 480
    preview_h = round(h * preview_w / w)
    panels: list[np.ndarray] = []
    flat_only = np.dstack((flat_front[..., :3], np.ones((h, w), np.float32)))
    for name, image in (("KNOWN SURFACES / SOURCE", chart_rgba),
                        ("FLAT DEPTH FRONT", flat_only),
                        ("DEPTH MODULATED FRONT", front_only),
                        ("LOCAL SURFACE LIGHT", surface_only),
                        ("SURFACE LIGHT + FRONT", combined)):
        fitted = fit_rgba(image, (preview_w, preview_h))
        bgr = cv2.cvtColor(to_u8(fitted), cv2.COLOR_RGBA2BGR)
        header = np.full((34, preview_w, 3), (22, 27, 37), np.uint8)
        cv2.putText(header, name, (10, 23), cv2.FONT_HERSHEY_SIMPLEX, 0.52,
                    (245, 245, 245), 1, cv2.LINE_AA)
        panels.append(np.vstack((header, bgr)))
    real_w = preview_w
    real_h = round(real_source.shape[0] * real_w / real_source.shape[1])
    real_preview = fit_rgba(real_source, (real_w, real_h))
    real_effect = fit_rgba(real_front, (real_w, real_h))
    for name, image in (("CC0 FOOTAGE / SOURCE", real_preview),
                        ("CC0 FOOTAGE / RADIAL FRONT", real_effect)):
        bgr = cv2.cvtColor(to_u8(image), cv2.COLOR_RGBA2BGR)
        header = np.full((34, real_w, 3), (22, 27, 37), np.uint8)
        cv2.putText(header, name, (10, 23), cv2.FONT_HERSHEY_SIMPLEX, 0.48,
                    (245, 245, 245), 1, cv2.LINE_AA)
        panels.append(np.vstack((header, bgr)))
    if selected_real_front is not None:
        selected_preview = fit_rgba(selected_real_front, (real_w, real_h))
        selected_bgr = cv2.cvtColor(to_u8(selected_preview), cv2.COLOR_RGBA2BGR)
        selected_header = np.full((34, real_w, 3), (22, 27, 37), np.uint8)
        cv2.putText(selected_header,
                    f"CC0 / CLIP-LIMITED STRENGTH {selected_real_strength:.2f}",
                    (10, 23), cv2.FONT_HERSHEY_SIMPLEX, 0.42,
                    (245, 245, 245), 1, cv2.LINE_AA)
        panels.append(np.vstack((selected_header, selected_bgr)))
    column_left = np.vstack(panels[:5])
    column_right = np.vstack(panels[5:])
    if column_left.shape[0] > column_right.shape[0]:
        column_right = np.vstack((column_right, np.zeros(
            (column_left.shape[0] - column_right.shape[0], real_w, 3), np.uint8)))
    else:
        column_left = np.vstack((column_left, np.zeros(
            (column_right.shape[0] - column_left.shape[0], preview_w, 3), np.uint8)))
    cv2.imwrite(str(args.output / "flash-surface-front-comparison.png"),
                np.hstack((column_left, column_right)))

    source_luma = linear_luma(source_linear)
    front_delta = linear_luma(front_linear) - source_luma
    flat_delta = linear_luma(srgb_to_linear(flat_front[..., :3])) - source_luma
    surface_delta = linear_luma(surface_linear) - source_luma
    combined_delta = linear_luma(combined_linear) - source_luma
    report = {
        "experiment": "traveling front combined with known-surface direct illumination",
        "video": str(args.video.resolve()), "frame_index": args.frame_index,
        "progress": args.progress, "light_xy": [float(light_xy[0]), float(light_xy[1])],
        "light_path": [list(args.light_start), list(args.light_end)],
        "strength": args.strength, "softness": args.softness, "scatter": args.scatter,
        "depth_front_strength": args.depth_front_strength,
        "target_new_clip_fraction": args.target_new_clip_fraction,
        "depth_front_sensitivity": depth_sensitivity,
        "surface_metrics": surface_metrics,
        "chart_mean_front_luma_delta": float(front_delta.mean()),
        "chart_mean_flat_front_luma_delta": float(flat_delta.mean()),
        "chart_mean_abs_depth_front_delta": float(np.abs(front_delta - flat_delta).mean()),
        "chart_mean_surface_luma_delta": float(surface_delta.mean()),
        "chart_mean_combined_luma_delta": float(combined_delta.mean()),
        "chart_newly_clipped_fraction_front": float(np.mean(
            (front[..., :3] >= 1.0) & (chart_rgba[..., :3] < 1.0))),
        "chart_newly_clipped_fraction_combined": float(np.mean(
            (combined[..., :3] >= 1.0) & (chart_rgba[..., :3] < 1.0))),
        "real_source_p95_linear_luma": float(np.percentile(real_source_luma, 95)),
        "real_strength_sweep": real_strength_metrics,
        "real_selected_strength_under_clip_target": selected_real_strength,
        "real_front_p95_linear_luma": float(np.percentile(real_front_luma, 95)),
        "real_newly_clipped_channel_fraction": float(np.mean(
            (real_front[..., :3] >= 1.0) & (real_source[..., :3] < 1.0))),
        "real_mean_linear_luma_delta": float((real_front_luma - real_source_luma).mean()),
        "limitations": [
            "Surface shading uses known synthetic chart geometry, not estimated real-photo normals or depth.",
            "The real CC0 frame receives only an image-space radial front; it is not relit with invented geometry.",
            "The combined result adds front exposure delta over the known-surface shading reference in linear light.",
            "This is an SDR look-development probe, not calibrated illumination or production parity.",
        ],
    }
    (args.output / "flash-surface-front-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    cv2.imwrite(str(args.output / "flash-surface-front-chart.png"),
                cv2.cvtColor(to_u8(combined), cv2.COLOR_RGBA2BGRA))
    cv2.imwrite(str(args.output / "flash-surface-front-cc0.png"),
                cv2.cvtColor(to_u8(real_front), cv2.COLOR_RGBA2BGRA))
    if selected_real_front is not None:
        cv2.imwrite(str(args.output / "flash-surface-front-cc0-clip-limited.png"),
                    cv2.cvtColor(to_u8(selected_real_front), cv2.COLOR_RGBA2BGRA))
    print(f"Saved surface-front comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
