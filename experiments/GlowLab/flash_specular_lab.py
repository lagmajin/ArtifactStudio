"""Study moving flash specular highlights on a sphere with known normals."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from flash_surface_lab import make_surface_chart
from glow_lab import linear_to_srgb, srgb_to_linear, to_u8


def sphere_specular(width: int, height: int, light_xy: tuple[float, float],
                    roughness: float, source_radius_px: float = 0.0,
                    light_height_px: float = 320.0
                    ) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return Lambert and deterministic sampled area-source Blinn-Phong response."""
    y, x = np.mgrid[0:height, 0:width].astype(np.float32)
    center_x, center_y, radius = 918.0, 380.0, 145.0
    sx, sy = (x - center_x) / radius, (y - center_y) / radius
    r2 = sx * sx + sy * sy
    mask = r2 <= 1.0
    sz = np.sqrt(np.maximum(1.0 - r2, 0.0))
    normal = np.stack((sx, -sy, sz), axis=-1)
    view = np.array((0.0, 0.0, 1.0), np.float32)
    exponent = max(2.0, 2.0 / max(float(roughness) ** 2, 1.0e-4) - 2.0)
    if source_radius_px <= 0.0:
        samples = ((0.0, 0.0),)
    else:
        diagonal = 0.70710678 * float(source_radius_px)
        radius = float(source_radius_px)
        samples = ((0.0, 0.0), (radius, 0.0), (-radius, 0.0),
                   (0.0, radius), (0.0, -radius),
                   (diagonal, diagonal), (diagonal, -diagonal),
                   (-diagonal, diagonal), (-diagonal, -diagonal))
    diffuse = np.zeros((height, width), np.float32)
    response = np.zeros((height, width), np.float32)
    for offset_x, offset_y in samples:
        # Keep screen-space offsets and sphere normals in the same pixel-scale
        # coordinate system. Dividing by the full image dimensions made the
        # apparent light direction almost stationary across the sphere.
        light = np.array(((light_xy[0] + offset_x - center_x) / radius,
                          (center_y - (light_xy[1] + offset_y)) / radius,
                          float(light_height_px) / radius), np.float32)
        light /= max(float(np.linalg.norm(light)), 1.0e-6)
        diffuse += np.maximum(np.sum(normal * light[None, None, :], axis=-1), 0.0)
        half_vector = light + view
        half_vector /= max(float(np.linalg.norm(half_vector)), 1.0e-6)
        ndoth = np.clip(np.sum(normal * half_vector[None, None, :], axis=-1), 0.0, 1.0)
        response += np.where(mask, ndoth ** exponent, 0.0)
    sample_count = float(len(samples))
    diffuse /= sample_count
    response /= sample_count
    return diffuse.astype(np.float32), response.astype(np.float32), mask


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parent / "captures" / "specular")
    parser.add_argument("--progress", type=float, default=0.76)
    parser.add_argument("--gain", type=float, default=1.8,
                        help="additional linear-light flash specular gain")
    parser.add_argument("--roughness", nargs=3, type=float, default=(0.08, 0.18, 0.35),
                        metavar=("SMOOTH", "MEDIUM", "ROUGH"))
    parser.add_argument("--source-radii", nargs=2, type=float, default=(0.0, 180.0),
                        metavar=("POINT_PX", "AREA_PX"),
                        help="compare point and finite circular source radii")
    parser.add_argument("--sequence-frames", type=int, default=24,
                        help="write a bounded-memory source-motion sequence; set 0 to skip")
    parser.add_argument("--sequence-source-radius", type=float, default=180.0,
                        help="finite source radius for the moving-light sequence")
    parser.add_argument("--light-height-px", type=float, default=320.0,
                        help="light height above the image plane in scene pixels")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    source, _, _ = make_surface_chart()
    height, width = source.shape[:2]
    source_linear = srgb_to_linear(source[..., :3])
    light_positions = ((72.0, 150.0), (1000.0, 115.0))
    roughnesses = tuple(float(np.clip(v, 0.035, 0.8)) for v in args.roughness)
    source_radii = tuple(max(0.0, float(v)) for v in args.source_radii)
    progress = float(np.clip(args.progress, 0.0, 1.0))
    flash_envelope = float(np.clip(progress / 0.72, 0.0, 1.0))
    panels: list[np.ndarray] = []
    variants: list[dict[str, object]] = []
    sphere_roi = (slice(220, 540), slice(780, 1080))

    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    tint = np.asarray((1.0, 0.88, 0.72), np.float32)
    for light_index, light_xy in enumerate(light_positions):
        for source_radius in source_radii:
            base = source_linear.copy()
            diffuse_strength = 0.48 * flash_envelope
            diffuse, _, sphere_mask = sphere_specular(
                width, height, light_xy, roughnesses[0], source_radius, args.light_height_px)
            base[sphere_mask] *= (1.0 + diffuse_strength * diffuse[sphere_mask][:, None])
            baseline = np.clip(linear_to_srgb(np.maximum(base, 0.0)), 0.0, 1.0)
            baseline_rgba = np.dstack((baseline, source[..., 3]))
            baseline_bgr = cv2.cvtColor(to_u8(baseline_rgba), cv2.COLOR_RGBA2BGR)
            header = np.full((42, width, 3), (22, 27, 37), np.uint8)
            cv2.putText(header, f"LIGHT {light_index + 1} / RADIUS {source_radius:.0f} / DIFFUSE",
                        (18, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.60,
                        (245, 245, 245), 1, cv2.LINE_AA)
            panels.append(np.vstack((header, baseline_bgr)))

            distance = np.sqrt(((xx - light_xy[0]) / width) ** 2 +
                               ((yy - light_xy[1]) / height) ** 2)
            attenuation = 1.0 / (1.0 + 1.8 * distance)
            for roughness in roughnesses:
                _, specular, mask = sphere_specular(
                    width, height, light_xy, roughness, source_radius, args.light_height_px)
                lit = base.copy()
                energy = specular * attenuation * args.gain * flash_envelope
                lit += energy[..., None] * tint[None, None, :] * mask[..., None]
                srgb_unclamped = linear_to_srgb(np.maximum(lit, 0.0))
                srgb = np.clip(srgb_unclamped, 0.0, 1.0)
                rgba = np.dstack((srgb, source[..., 3]))
                tag = f"light-{light_index + 1}-radius-{source_radius:.0f}-roughness-{roughness:.2f}"
                cv2.imwrite(str(args.output / f"specular-{tag}.png"),
                            cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGRA))
                bgr = cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGR)
                panel_header = np.full((42, width, 3), (22, 27, 37), np.uint8)
                cv2.putText(panel_header,
                            f"LIGHT {light_index + 1} / RADIUS {source_radius:.0f} / ROUGH {roughness:.2f}",
                            (18, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.54,
                            (245, 245, 245), 1, cv2.LINE_AA)
                panels.append(np.vstack((panel_header, bgr)))
                roi = energy[sphere_roi]
                visible = roi > 0.02
                weights = roi * mask[sphere_roi]
                variants.append({
                    "light_xy": list(light_xy),
                    "source_radius_px": source_radius,
                    "roughness": roughness,
                    "blinn_phong_exponent": max(2.0, 2.0 / max(roughness ** 2, 1.0e-4) - 2.0),
                    "specular_peak_linear": float(roi.max()),
                    "specular_energy_sum": float(roi.sum()),
                    "visible_fraction_in_sphere_roi": float(visible.mean()),
                    "display_clipped_fraction_in_sphere": float(
                        np.mean(np.max(srgb_unclamped[mask], axis=1) >= 1.0)),
                    "specular_centroid_xy": [
                        float((weights * xx[sphere_roi]).sum() / max(weights.sum(), 1.0e-8)),
                        float((weights * yy[sphere_roi]).sum() / max(weights.sum(), 1.0e-8)),
                    ],
                })

    thumb_width = 640
    thumb_height = 360
    rows: list[np.ndarray] = []
    for start in range(0, len(panels), 4):
        cells = []
        for panel in panels[start:start + 4]:
            image = panel[42:]
            image = cv2.resize(image, (thumb_width, thumb_height), interpolation=cv2.INTER_AREA)
            header = cv2.resize(panel[:42], (thumb_width, 42), interpolation=cv2.INTER_AREA)
            cells.append(np.vstack((header, image)))
        rows.append(np.hstack(cells))
    cv2.imwrite(str(args.output / "flash-specular-comparison.png"), np.vstack(rows))
    sequence_report: dict[str, object] | None = None
    if args.sequence_frames > 0:
        sequence_dir = args.output / "sequence"
        sequence_dir.mkdir(parents=True, exist_ok=True)
        frame_count = max(2, args.sequence_frames)
        sample_indices = {0, frame_count // 4, frame_count // 2,
                          (3 * frame_count) // 4, frame_count - 1}
        thumbnails: list[np.ndarray] = []
        sequence_metrics: list[dict[str, object]] = []
        previous_sphere_luma: np.ndarray | None = None
        for frame_index in range(frame_count):
            progress = frame_index / (frame_count - 1)
            light_xy = (72.0 + (1000.0 - 72.0) * progress,
                        150.0 + (115.0 - 150.0) * progress)
            envelope = min(1.0, progress / 0.16) * np.exp(-max(0.0, progress - 0.24) * 2.8)
            diffuse, specular, mask = sphere_specular(
                width, height, light_xy, 0.18, max(0.0, args.sequence_source_radius),
                args.light_height_px)
            frame_linear = source_linear.copy()
            frame_linear[mask] *= 1.0 + 0.48 * envelope * diffuse[mask][:, None]
            yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
            distance = np.sqrt(((xx - light_xy[0]) / width) ** 2 +
                               ((yy - light_xy[1]) / height) ** 2)
            attenuation = 1.0 / (1.0 + 1.8 * distance)
            spec_energy = specular * attenuation * args.gain * envelope
            frame_linear += (spec_energy[..., None] *
                             np.asarray((1.0, 0.88, 0.72), np.float32)[None, None, :] *
                             mask[..., None])
            panel_mask = (xx >= 220.0) & (xx <= 702.0) & (yy >= 212.0) & (yy <= 604.0)
            panel_nx = np.clip((461.0 - xx) / 300.0, -0.48, 0.48)
            panel_ny = np.clip((408.0 - yy) / 270.0, -0.32, 0.32)
            panel_nz = np.sqrt(np.maximum(1.0 - panel_nx * panel_nx - panel_ny * panel_ny, 0.1))
            source_radius = max(0.0, args.sequence_source_radius)
            source_offsets = ((0.0, 0.0),) if source_radius <= 0.0 else (
                (0.0, 0.0), (source_radius, 0.0), (-source_radius, 0.0),
                (0.0, source_radius), (0.0, -source_radius),
                (0.70710678 * source_radius, 0.70710678 * source_radius),
                (0.70710678 * source_radius, -0.70710678 * source_radius),
                (-0.70710678 * source_radius, 0.70710678 * source_radius),
                (-0.70710678 * source_radius, -0.70710678 * source_radius))
            panel_gain = np.zeros((height, width), np.float32)
            for offset_x, offset_y in source_offsets:
                panel_light = np.array((light_xy[0] + offset_x - 461.0,
                                        408.0 - (light_xy[1] + offset_y),
                                        args.light_height_px), np.float32)
                panel_light /= max(float(np.linalg.norm(panel_light)), 1.0e-6)
                panel_ndotl = np.maximum(panel_nx * panel_light[0] + panel_ny * panel_light[1] +
                                         panel_nz * panel_light[2], 0.0)
                panel_response = 0.32 + 0.68 * panel_ndotl
                panel_distance = np.sqrt(((xx - light_xy[0] - offset_x) / width) ** 2 +
                                         ((yy - light_xy[1] - offset_y) / height) ** 2 +
                                         (args.light_height_px / max(width, height)) ** 2)
                panel_attenuation = 1.0 / (1.0 + 2.2 * panel_distance)
                panel_gain += panel_response * panel_attenuation
            panel_gain /= float(len(source_offsets))
            panel_gain *= envelope
            panel_gain[~panel_mask] = 0.0
            frame_linear += source_linear * panel_gain[..., None] * 0.9
            frame_srgb_unclamped = linear_to_srgb(np.maximum(frame_linear, 0.0))
            frame_srgb = np.clip(frame_srgb_unclamped, 0.0, 1.0)
            rgba = np.dstack((frame_srgb, source[..., 3]))
            frame_path = sequence_dir / f"flash-specular-{frame_index:04d}.png"
            cv2.imwrite(str(frame_path), cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGRA))

            sphere_weights = spec_energy * mask
            weight_sum = max(float(sphere_weights.sum()), 1.0e-8)
            sphere_roi_mask = mask[sphere_roi]
            sphere_roi_weights = spec_energy[sphere_roi] * sphere_roi_mask
            roi_weight_sum = max(float(sphere_roi_weights.sum()), 1.0e-8)
            roi_x = xx[sphere_roi]
            roi_y = yy[sphere_roi]
            panel_weights = panel_gain * panel_mask
            panel_weight_sum = max(float(panel_weights.sum()), 1.0e-8)
            sphere_linear = frame_linear[mask]
            sphere_luma = (sphere_linear[:, 0] * 0.2126 + sphere_linear[:, 1] * 0.7152 +
                           sphere_linear[:, 2] * 0.0722)
            sequence_metrics.append({
                "frame": frame_index,
                "progress": progress,
                "flash_envelope": float(envelope),
                "light_xy": [float(light_xy[0]), float(light_xy[1])],
                "source_radius_px": max(0.0, args.sequence_source_radius),
                "specular_centroid_xy": [
                    float((sphere_weights * xx).sum() / weight_sum) if envelope > 0.0 else None,
                    float((sphere_weights * yy).sum() / weight_sum) if envelope > 0.0 else None,
                ],
                "sphere_roi_specular_centroid_xy": [
                    float((sphere_roi_weights * roi_x).sum() / roi_weight_sum) if envelope > 0.0 else None,
                    float((sphere_roi_weights * roi_y).sum() / roi_weight_sum) if envelope > 0.0 else None,
                ],
                "sphere_roi_specular_peak_linear": float(spec_energy[sphere_roi].max()),
                "sphere_roi_specular_visible_fraction": float(
                    np.mean(spec_energy[sphere_roi] > 0.02)),
                "panel_light_energy_centroid_xy": [
                    float((panel_weights * xx).sum() / panel_weight_sum) if envelope > 0.0 else None,
                    float((panel_weights * yy).sum() / panel_weight_sum) if envelope > 0.0 else None,
                ],
                "panel_mean_light_gain": float(panel_gain[panel_mask].mean()),
                "sphere_mean_linear_luma": float(sphere_luma.mean()),
                "sphere_display_clipped_fraction": float(
                    np.mean(np.max(frame_srgb_unclamped[mask], axis=1) >= 1.0)),
            })
            if previous_sphere_luma is not None:
                sequence_metrics[-1]["adjacent_mean_abs_linear_luma_delta"] = float(
                    np.abs(sphere_luma - previous_sphere_luma).mean())
            previous_sphere_luma = sphere_luma
            if frame_index in sample_indices:
                thumb = cv2.resize(cv2.cvtColor(to_u8(rgba), cv2.COLOR_RGBA2BGR),
                                   (320, 180), interpolation=cv2.INTER_AREA)
                cv2.putText(thumb, f"{progress:.2f}  light=({light_xy[0]:.0f},{light_xy[1]:.0f})",
                            (8, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.42,
                            (255, 255, 255), 1, cv2.LINE_AA)
                thumbnails.append(thumb)
        while len(thumbnails) % 3:
            thumbnails.append(np.zeros_like(thumbnails[0]))
        contact_rows = [np.hstack(thumbnails[index:index + 3])
                        for index in range(0, len(thumbnails), 3)]
        cv2.imwrite(str(sequence_dir / "flash-specular-contact-sheet.png"),
                    np.vstack(contact_rows))
        sequence_report = {
            "frame_count": frame_count,
            "roughness": 0.18,
            "source_radius_px": max(0.0, args.sequence_source_radius),
            "light_height_px": args.light_height_px,
            "sequence_frames_written": frame_count,
            "bounded_memory": True,
            "tracking_summary": {},
            "frames": sequence_metrics,
        }
        active = [row for row in sequence_metrics if row["flash_envelope"] > 0.2 and
                  row["sphere_roi_specular_centroid_xy"][0] is not None]
        if active:
            centroids = np.asarray([row["sphere_roi_specular_centroid_xy"] for row in active], np.float32)
            lights = np.asarray([row["light_xy"] for row in active], np.float32)
            centroid_steps = np.linalg.norm(np.diff(centroids, axis=0), axis=1)
            light_steps = np.linalg.norm(np.diff(lights, axis=0), axis=1)
            centroid_path = float(centroid_steps.sum())
            source_path = float(light_steps.sum())
            endpoint = float(np.linalg.norm(centroids[-1] - centroids[0]))
            source_endpoint = float(np.linalg.norm(lights[-1] - lights[0]))
            sequence_report["tracking_summary"] = {
                "active_frame_count": len(active),
                "centroid_path_px": centroid_path,
                "light_path_px": source_path,
                "centroid_to_light_path_ratio": centroid_path / max(source_path, 1.0e-6),
                "centroid_endpoint_travel_px": endpoint,
                "light_endpoint_travel_px": source_endpoint,
                "endpoint_travel_ratio": endpoint / max(source_endpoint, 1.0e-6),
                "mean_centroid_to_light_distance_px": float(np.linalg.norm(centroids - lights, axis=1).mean()),
                "mean_roi_specular_peak_linear": float(np.mean(
                    [row["sphere_roi_specular_peak_linear"] for row in active])),
                "mean_roi_specular_visible_fraction": float(np.mean(
                    [row["sphere_roi_specular_visible_fraction"] for row in active])),
            }
    report = {
        "experiment": "known-normal Blinn-Phong flash specular response",
        "progress": progress,
        "flash_envelope": flash_envelope,
        "specular_gain": args.gain,
        "light_height_px": args.light_height_px,
        "light_positions": [list(point) for point in light_positions],
        "roughness_values": list(roughnesses),
        "limitations": [
            "Synthetic sphere normals and a fixed view direction are assumed.",
            "The chart's original baked highlight remains in the source plate.",
            "Roughness-to-exponent mapping is a look-development approximation, not calibrated BRDF.",
            "Clipped display PNGs do not retain HDR specular energy.",
        ],
        "variants": variants,
        "sequence": sequence_report,
    }
    with (args.output / "flash-specular-metrics.json").open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(report, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    print(f"Saved specular comparison to {args.output.resolve()}")


if __name__ == "__main__":
    main()
