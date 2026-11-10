"""Sweep flash-front timing and cast-shadow geometry on known surfaces."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_fill_lab import flash_fill
from flash_shadow_projection_lab import projected_shadow, shade_known_surfaces
from flash_surface_lab import make_surface_chart
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def linear_luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def measure_shadow(mask: np.ndarray, ground_roi: np.ndarray) -> dict[str, float]:
    """Measure the blurred cast mask in one fixed ground-plane region."""
    roi_mask = np.where(ground_roi, np.clip(mask, 0.0, 1.0), 0.0)
    profile = roi_mask.sum(axis=1)
    total = float(profile.sum())
    if total <= 1.0e-8:
        return {
            "area_px": 0.0, "centroid_x_px": 0.0, "centroid_y_px": 0.0,
            "length_50_percent_px": 0.0, "trailing_edge_10_90_width_px": 0.0,
            "mean_opacity": 0.0,
        }
    yy, xx = np.indices(mask.shape, dtype=np.float32)
    centroid_x = float((roi_mask * xx).sum() / total)
    centroid_y = float((roi_mask * yy).sum() / total)
    peak = float(profile.max())
    active_rows = np.flatnonzero(profile >= peak * 0.5)
    peak_row = int(np.argmax(profile))
    normalized_tail = profile[peak_row:] / max(peak, 1.0e-8)
    below_90 = np.flatnonzero(normalized_tail <= 0.9)
    below_10 = np.flatnonzero(normalized_tail <= 0.1)
    edge_width = (float(below_10[0] - below_90[0])
                  if below_90.size and below_10.size and below_10[0] >= below_90[0]
                  else 0.0)
    binary = (roi_mask >= 0.5) & ground_roi
    return {
        "area_px": float(binary.sum()),
        "centroid_x_px": centroid_x,
        "centroid_y_px": centroid_y,
        "length_50_percent_px": float(active_rows[-1] - active_rows[0] + 1)
            if active_rows.size else 0.0,
        "trailing_edge_10_90_width_px": edge_width,
        "mean_opacity": float(roi_mask[ground_roi].mean()),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "shadow-animation")
    parser.add_argument("--samples", type=int, default=7)
    parser.add_argument("--strength", type=float, default=0.8)
    parser.add_argument("--surface-strength", type=float, default=1.2)
    parser.add_argument("--depth-front-strength", type=float, default=0.36)
    parser.add_argument("--front-durations", nargs="+", type=float, default=(0.45, 0.78, 1.10))
    parser.add_argument("--shadow-heights", nargs="+", type=float, default=(16.0, 32.0, 52.0))
    parser.add_argument("--shadow-softnesses", nargs="+", type=float, default=(6.0, 14.0, 24.0))
    parser.add_argument("--shadow-reference-height", type=float, default=28.0)
    parser.add_argument("--shadow-reference-softness", type=float, default=14.0)
    parser.add_argument("--shadow-opacity", type=float, default=0.56)
    args = parser.parse_args()
    if args.samples < 3 or args.samples > 13:
        parser.error("samples must be between 3 and 13")
    if min(args.strength, args.surface_strength, args.depth_front_strength,
           args.shadow_reference_height, args.shadow_reference_softness,
           args.shadow_opacity, *args.front_durations, *args.shadow_heights,
           *args.shadow_softnesses) < 0.0:
        parser.error("strength, depth, and shadow controls must be nonnegative")
    front_durations = sorted(set(float(value) for value in args.front_durations))
    shadow_heights = sorted(set(float(value) for value in args.shadow_heights))
    shadow_softnesses = sorted(set(float(value) for value in args.shadow_softnesses))
    if not front_durations or not shadow_heights or not shadow_softnesses or min(front_durations) <= 0.0:
        parser.error("front durations must be positive and each sweep needs at least one value")
    if args.shadow_opacity > 1.0:
        parser.error("shadow-opacity must be at most 1.0")
    args.output.mkdir(parents=True, exist_ok=True)

    source_rgba, depth_map, _ = make_surface_chart()
    height, width = source_rgba.shape[:2]
    source_srgb = source_rgba[..., :3]
    source_linear = srgb_to_linear(source_srgb)
    start = np.asarray((72.0, 150.0), np.float32)
    end = np.asarray((1000.0, 115.0), np.float32)
    progress_values = np.linspace(0.12, 0.92, args.samples, dtype=np.float32)
    preview_width = 480
    preview_height = round(height * preview_width / width)
    speed_rows: dict[str, list[np.ndarray]] = {f"FRONT {value:g}s": []
                                               for value in front_durations}
    shadow_configs = [
        {"axis": "height", "label": f"HEIGHT SWEEP {value:g}px / SOFT {args.shadow_reference_softness:g}px",
         "height": value, "softness": args.shadow_reference_softness}
        for value in shadow_heights
    ] + [
        {"axis": "softness", "label": f"SOFTNESS SWEEP {value:g}px / HEIGHT {args.shadow_reference_height:g}px",
         "height": args.shadow_reference_height, "softness": value}
        for value in shadow_softnesses
    ]
    shadow_rows: dict[str, list[np.ndarray]] = {config["label"]: [] for config in shadow_configs}
    mask_rows: dict[str, list[np.ndarray]] = {config["label"]: [] for config in shadow_configs}
    front_metrics: dict[float, list[dict[str, float]]] = {value: [] for value in front_durations}
    shadow_metrics: dict[str, list[dict[str, float]]] = {key: [] for key in shadow_rows}
    yy, xx = np.mgrid[0:height, 0:width]
    ground_roi = (yy >= 604) & (yy < height) & (xx >= 130) & (xx < 850)
    for progress in progress_values:
        light_xy = tuple((start + (end - start) * progress).tolist())
        source_position = (light_xy[0] / (width - 1), light_xy[1] / (height - 1))
        fronts: dict[float, np.ndarray] = {}
        for front_duration in front_durations:
            fronts[front_duration] = flash_fill(
                source_rgba, float(progress), mode="radial", source_position=source_position,
                strength=args.strength, softness=0.16, temperature=(1.0, 0.91, 0.78),
                attack_duration=0.72, peak_time=0.78, decay_rate=2.4,
                scatter_strength=0.55, highlight_threshold=0.25, highlight_range=0.52,
                global_fill=0.015, front_duration=front_duration, radial_falloff=1.8,
                depth_map=depth_map, depth_front_strength=args.depth_front_strength,
                highlight_rolloff=0.82)
        reference_duration = front_durations[len(front_durations) // 2]
        reference_front = fronts[reference_duration]

        # Add a lower-energy image-space front to local diffuse illumination, then
        # attenuate only the flash contribution by the moving synthetic shadow.
        surface_lit, _surface_metrics = shade_known_surfaces(
            source_linear, light_xy, args.surface_strength, float(progress))
        front_linear = srgb_to_linear(reference_front[..., :3])
        front_delta = np.maximum(front_linear - source_linear, 0.0) * 0.45
        surface_delta = np.maximum(surface_lit - source_linear, 0.0) * 0.55
        source_luma = linear_luma(source_linear)
        for front_duration, front in fronts.items():
            front_luma = linear_luma(srgb_to_linear(front[..., :3]))
            front_metrics[front_duration].append({
                "progress": float(progress),
                "mean_linear_luma_delta": float((front_luma - source_luma).mean()),
                "newly_clipped_channel_fraction": float(np.mean(
                    (front[..., :3] >= 1.0) & (source_srgb < 1.0))),
                "front_coverage_above_0p01_delta": float(np.mean(
                    (front_luma - source_luma) > 0.01)),
            })
            label = f"FRONT {front_duration:g}s"
            thumb = cv2.resize(cv2.cvtColor(to_u8(front), cv2.COLOR_RGBA2BGR),
                               (preview_width, preview_height), interpolation=cv2.INTER_AREA)
            header = np.full((30, preview_width, 3), (22, 27, 37), np.uint8)
            cv2.putText(header, f"{label} / t={progress:.2f}", (8, 21),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, (245, 245, 245), 1, cv2.LINE_AA)
            speed_rows[label].append(np.vstack((header, thumb)))

        for config in shadow_configs:
            label = str(config["label"])
            shadow_height = float(config["height"])
            shadow_softness = float(config["softness"])
            shadow = projected_shadow(width, height, light_xy, shadow_height, shadow_softness)
            shadow[:604, :] = 0.0
            shadow[:, :130] = 0.0
            shadow[:, 850:] = 0.0
            cast_factor = np.clip(shadow * args.shadow_opacity, 0.0, 0.90)
            geometry_linear = source_linear + (front_delta + surface_delta) * (1.0 - cast_factor[..., None])
            geometry_srgb = np.clip(linear_to_srgb(np.maximum(geometry_linear, 0.0)), 0.0, 1.0)
            geometry = np.dstack((geometry_srgb, source_rgba[..., 3]))
            geometry_luma = linear_luma(geometry_linear)
            shadow_region = (shadow > 0.2) & ground_roi
            shadow_core = (shadow > 0.2).astype(np.uint8)
            neighbor_ring = cv2.dilate(shadow_core, np.ones((13, 13), np.uint8)) > 0
            adjacent_region = neighbor_ring & (shadow < 0.02) & ground_roi
            geometry_delta = geometry_luma - source_luma
            shadow_metrics[label].append({
                "progress": float(progress),
                "height_px": float(shadow_height),
                "softness_px": float(shadow_softness),
                **measure_shadow(shadow, ground_roi),
                "shadow_region_flash_delta": float(geometry_delta[shadow_region].mean())
                    if np.any(shadow_region) else 0.0,
                "adjacent_region_flash_delta": float(geometry_delta[adjacent_region].mean())
                    if np.any(adjacent_region) else 0.0,
                "newly_clipped_channel_fraction": float(np.mean(
                    (geometry[..., :3] >= 1.0) & (source_srgb < 1.0))),
            })
            thumb = cv2.resize(cv2.cvtColor(to_u8(geometry), cv2.COLOR_RGBA2BGR),
                               (preview_width, preview_height), interpolation=cv2.INTER_AREA)
            header = np.full((30, preview_width, 3), (22, 27, 37), np.uint8)
            cv2.putText(header, f"{label} / t={progress:.2f}", (8, 21),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.40, (245, 245, 245), 1, cv2.LINE_AA)
            shadow_rows[label].append(np.vstack((header, thumb)))
            ground_crop = shadow[604:height, 130:850]
            mask_u8 = np.clip(ground_crop * 255.0, 0.0, 255.0).astype(np.uint8)
            mask_color = cv2.applyColorMap(mask_u8, cv2.COLORMAP_TURBO)
            mask_thumb = cv2.resize(mask_color, (preview_width, max(1, round(
                ground_crop.shape[0] * preview_width / ground_crop.shape[1]))),
                interpolation=cv2.INTER_AREA)
            mask_header = np.full((26, preview_width, 3), (22, 27, 37), np.uint8)
            cv2.putText(mask_header, f"{label} / MATTE t={progress:.2f}", (8, 19),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.36, (245, 245, 245), 1, cv2.LINE_AA)
            mask_rows[label].append(np.vstack((mask_header, mask_thumb)))

    cv2.imwrite(str(args.output / "flash-front-speed-sweep-contact-sheet.png"),
                np.vstack([np.hstack(row) for row in speed_rows.values()]))
    cv2.imwrite(str(args.output / "flash-shadow-parameter-sweep-contact-sheet.png"),
                np.vstack([np.hstack(row) for row in shadow_rows.values()]))
    cv2.imwrite(str(args.output / "flash-shadow-mask-parameter-sweep-contact-sheet.png"),
                np.vstack([np.hstack(row) for row in mask_rows.values()]))
    report = {
        "experiment": "moving flash front with known-surface diffuse response and cast shadow",
        "samples": int(args.samples),
        "light_path_xy": [start.tolist(), end.tolist()],
        "strength": args.strength,
        "surface_strength": args.surface_strength,
        "depth_front_strength": args.depth_front_strength,
        "front_durations_seconds": front_durations,
        "reference_front_duration_seconds": reference_duration,
        "front_duration_sweep": [
            {"front_duration_seconds": key, "samples": value} for key, value in front_metrics.items()
        ],
        "shadow_height_sweep": [
            {"height_px": config["height"], "reference_softness_px": config["softness"],
             "samples": shadow_metrics[str(config["label"])]}
            for config in shadow_configs if config["axis"] == "height"
        ],
        "shadow_softness_sweep": [
            {"reference_height_px": config["height"], "softness_px": config["softness"],
             "samples": shadow_metrics[str(config["label"])]}
            for config in shadow_configs if config["axis"] == "softness"
        ],
        "shadow_opacity": args.shadow_opacity,
        "limitations": [
            "Depth, panel, sphere, and shadow caster are authored for the synthetic chart.",
            "The translated Gaussian shadow is a 2.5D image-space approximation, not ray-traced geometry.",
            "The combined variant is a look-development blend; its two flash contributions are not energy calibrated.",
            "No geometry is inferred or applied to the real CC0 footage in this experiment.",
        ],
    }
    (args.output / "flash-shadow-parameter-sweep-metrics.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Saved moving shadow comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
