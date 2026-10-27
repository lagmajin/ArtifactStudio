"""Trace prescription OPL to an exit-side reference plane and validate its phase slope.

This is an intermediate geometric-wavefront diagnostic, not an exit-pupil PSF.
It preserves equal phase on the incident plane, accumulates n*distance through
the prescription, applies an image-centered reference-sphere path correction,
and checks the plane eikonal gradient against traced outgoing directions.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
import evaluate_direct_transmission_raytrace as prescription


WAVELENGTH_NM = 550.0
FIELD_CASES = (("on_axis", 0.0), ("field_5deg", 5.0),
               ("field_10deg", 10.0), ("field_12deg", 12.0))
EXIT_PLANE_Z_MM = prescription.SURFACES[-1][1] + 1.0


def trace_to_exit_plane(launch_origin, direction, wavelength_nm):
    """Trace one ray and accumulate optical path length to a shared z plane."""
    origin = np.asarray(launch_origin, dtype=np.float64).copy()
    direction = np.asarray(direction, dtype=np.float64).copy()
    opl_mm = 0.0
    for i, surface in enumerate(prescription.SURFACES):
        if i == 7:
            distance = (prescription.IRIS_Z - origin[2]) / direction[2]
            iris = origin + distance * direction
            if distance <= 0.0 or not prescription.in_hexagon(
                    iris[0], iris[1], prescription.STOP_RADIUS):
                return None
            n_before = prescription.refractive_index(
                prescription.SURFACES[i - 1], wavelength_nm)
            opl_mm += n_before * distance
            origin = iris
        point = prescription.intersect(origin, direction, surface)
        if point is None:
            return None
        n_before = 1.0 if i == 0 else prescription.refractive_index(
            prescription.SURFACES[i - 1], wavelength_nm)
        opl_mm += n_before * float(np.linalg.norm(point - origin))
        n_after = prescription.refractive_index(surface, wavelength_nm)
        direction = prescription.refract(direction, point, surface, n_before, n_after)
        if direction is None:
            return None
        origin = point
    if direction[2] <= 1e-10:
        return None
    distance = (EXIT_PLANE_Z_MM - origin[2]) / direction[2]
    if distance <= 0.0:
        return None
    opl_mm += distance  # Air propagation to the common exit-side plane.
    point = origin + distance * direction
    return point[:2], direction, opl_mm


def trace_field(field_angle_deg, samples, wavelength_nm=WAVELENGTH_NM):
    angle = math.radians(field_angle_deg)
    direction = np.array((math.sin(angle), 0.0, math.cos(angle)))
    chief_origin, _ = prescription.chief_ray(angle, wavelength_nm)
    chief = trace_to_exit_plane(chief_origin, direction, wavelength_nm)
    if chief is None:
        raise RuntimeError(f"chief ray is blocked at {field_angle_deg} deg")
    chief_xy, chief_direction, chief_opl = chief
    transverse_x = np.array((math.cos(angle), 0.0, -math.sin(angle)))
    transverse_y = np.array((0.0, 1.0, 0.0))
    extent = 35.0
    grid = np.linspace(-extent, extent, samples, endpoint=False) + extent / samples
    pupil_xy, directions, opl = [], [], []
    index_map = np.full((samples, samples), -1, dtype=np.int64)
    for iy, y in enumerate(grid):
        for ix, x in enumerate(grid):
            launch = chief_origin + x * transverse_x + y * transverse_y
            result = trace_to_exit_plane(launch, direction, wavelength_nm)
            if result is None:
                continue
            point, ray_direction, ray_opl = result
            pupil_xy.append(point)
            directions.append(ray_direction)
            opl.append(ray_opl)
            index_map[iy, ix] = len(pupil_xy) - 1
    pupil_xy = np.asarray(pupil_xy, dtype=np.float64)
    directions = np.asarray(directions, dtype=np.float64)
    opl = np.asarray(opl, dtype=np.float64)
    # Compare the traced phase with a reference sphere centered on the chief
    # image point. The unmodified plane OPL is retained for gradient checking.
    image_z = prescription.SENSOR_Z
    image_point = np.array((chief_xy[0] + (image_z - EXIT_PLANE_Z_MM) *
                            chief_direction[0] / chief_direction[2],
                            chief_xy[1] + (image_z - EXIT_PLANE_Z_MM) *
                            chief_direction[1] / chief_direction[2], image_z))
    pupil_points_3d = np.column_stack((pupil_xy, np.full(len(pupil_xy), EXIT_PLANE_Z_MM)))
    chief_point_3d = np.array((chief_xy[0], chief_xy[1], EXIT_PLANE_Z_MM))
    reference_distance = np.linalg.norm(image_point - pupil_points_3d, axis=1)
    chief_reference_distance = float(np.linalg.norm(image_point - chief_point_3d))
    opd_mm = opl + reference_distance - chief_opl - chief_reference_distance
    return pupil_xy, directions, opl, opd_mm, chief_xy, chief_direction, index_map


def phase_slope_error(pupil_xy, opl, directions, index_map):
    """Check local OPL increments against the eikonal ray-direction relation."""
    first, second = [], []
    for a_grid, b_grid in ((index_map[:, :-1], index_map[:, 1:]),
                           (index_map[:-1, :], index_map[1:, :])):
        valid = (a_grid >= 0) & (b_grid >= 0)
        first.append(a_grid[valid])
        second.append(b_grid[valid])
    first = np.concatenate(first)
    second = np.concatenate(second)
    delta_position = pupil_xy[second] - pupil_xy[first]
    predicted_delta_opl = np.sum(
        0.5 * (directions[first, :2] + directions[second, :2]) * delta_position,
        axis=1)
    actual_delta_opl = opl[second] - opl[first]
    error = actual_delta_opl - predicted_delta_opl
    rms_error = math.sqrt(float(np.mean(error * error)))
    rms_signal = math.sqrt(float(np.mean(predicted_delta_opl * predicted_delta_opl)))
    return rms_error, 100.0 * rms_error / max(rms_signal, 1e-15), len(first)


def color_opd(value, limit):
    t = float(np.clip(value / max(limit, 1e-12), -1.0, 1.0))
    if t < 0.0:
        q = t + 1.0
        return (int(40 + 210 * q), int(80 + 170 * q), 255)
    q = 1.0 - t
    return (255, int(75 + 175 * q), int(45 + 210 * q))


def make_chart(cases, output):
    width, height, panel = 2 * 470, 2 * 490, 440
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    pooled = np.concatenate([case["opd_um"] for case in cases])
    limit = max(float(np.quantile(np.abs(pooled), 0.99)), 1e-6)
    for index, case in enumerate(cases):
        left = (index % 2) * 470
        top = (index // 2) * 490
        draw.text((left + 8, top + 5),
                  f"{case['field']} ({case['angle_deg']:.0f} deg)  "
                  f"OPD RMS {case['opd_rms_um']:.3f} um", fill=(235, 238, 244))
        center = case["chief_xy"]
        offsets = case["pupil_xy"] - center
        span = 5.0
        scale = 0.46 * panel / (2.0 * span)
        for (x, y), value in zip(offsets, case["opd_um"]):
            if abs(x) > span or abs(y) > span:
                continue
            sx = left + panel / 2 + x * scale
            sy = top + 34 + panel / 2 - y * scale
            draw.point((sx, sy), fill=color_opd(value, limit))
        draw.rectangle((left + 15, top + 34, left + panel - 15, top + 34 + panel),
                       outline=(62, 68, 78))
        draw.text((left + 14, top + 468),
                  f"image reference sphere; +/-{span:.0f} mm; +/-{limit:.3f} um",
                  fill=(185, 193, 204))
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--samples", type=int, default=256)
    parser.add_argument("--wavelength-nm", type=float, default=WAVELENGTH_NM)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-wavefront.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-wavefront.png"))
    args = parser.parse_args()
    if args.samples < 32 or args.wavelength_nm <= 0.0:
        parser.error("samples must be >=32 and wavelength must be positive")

    cases, records = [], []
    for name, angle in FIELD_CASES:
        xy, directions, opl, opd_mm, chief_xy, chief_direction, index_map = trace_field(
            angle, args.samples, args.wavelength_nm)
        rms_um = float(np.sqrt(np.mean(opd_mm * opd_mm)) * 1000.0)
        slope_error, slope_error_percent, pair_count = phase_slope_error(
            xy, opl, directions, index_map)
        opd_um = opd_mm * 1000.0
        image_point_x = chief_xy[0] + (prescription.SENSOR_Z - EXIT_PLANE_Z_MM) * \
            chief_direction[0] / chief_direction[2]
        image_point_y = chief_xy[1] + (prescription.SENSOR_Z - EXIT_PLANE_Z_MM) * \
            chief_direction[1] / chief_direction[2]
        case = {"field": name, "angle_deg": angle, "pupil_xy": xy, "opd_um": opd_um,
                "chief_xy": chief_xy, "opd_rms_um": rms_um}
        cases.append(case)
        records.append({
            "field": name, "field_angle_deg": angle,
            "wavelength_nm": args.wavelength_nm, "launch_grid": args.samples,
            "valid_rays": len(xy), "exit_plane_z_mm": EXIT_PLANE_Z_MM,
            "chief_x_mm": float(chief_xy[0]), "chief_y_mm": float(chief_xy[1]),
            "chief_image_x_mm": float(image_point_x),
            "chief_image_y_mm": float(image_point_y),
            "reference_sphere_radius_mm": float(prescription.SENSOR_Z - EXIT_PLANE_Z_MM),
            "chief_direction_x": float(chief_direction[0]),
            "chief_direction_y": float(chief_direction[1]),
            "opd_rms_um_reference_sphere": rms_um,
            "opd_rms_waves": rms_um * 1000.0 / args.wavelength_nm,
            "opd_p99_minus_p01_um": float(np.quantile(opd_um, .99) -
                                            np.quantile(opd_um, .01)),
            "eikonal_gradient_rms_error": slope_error,
            "eikonal_gradient_rms_error_percent": slope_error_percent,
            "gradient_neighbor_pairs": pair_count,
        })
        print(f"{name:14s} rays={len(xy):5d} OPD RMS={rms_um:.5f} um "
              f"({records[-1]['opd_rms_waves']:.4f} waves), "
              f"eikonal slope error={slope_error_percent:.6f}% "
              f"({pair_count} neighbor pairs)")

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    make_chart(cases, args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
