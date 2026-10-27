"""Trace prescription OPL, estimate both pupils, and validate wavefront sampling.

This is an intermediate geometric-wavefront diagnostic, not a coherent PSF.
It estimates both paraxial pupils from ray pencils, accumulates
n*distance through the prescription, applies an image-centered reference-sphere
correction, and checks the eikonal gradient against traced directions.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
import evaluate_direct_transmission_raytrace as prescription


WAVELENGTH_NM = 550.0
PUPIL_PROBE_SLOPE = 0.001
PUPIL_PROBE_SAMPLES = 9
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


def trace_stop_pencil(stop_xy, slope_x, slope_y, wavelength_nm):
    """Trace a ray emitted from an aperture-stop point through the rear group."""
    origin = np.array((stop_xy[0], stop_xy[1], prescription.IRIS_Z), dtype=np.float64)
    direction = np.array((slope_x, slope_y, 1.0), dtype=np.float64)
    direction /= np.linalg.norm(direction)
    for i, surface in enumerate(prescription.SURFACES[7:], start=7):
        point = prescription.intersect(origin, direction, surface)
        if point is None:
            return None
        n_before = 1.0 if i == 7 else prescription.refractive_index(
            prescription.SURFACES[i - 1], wavelength_nm)
        n_after = prescription.refractive_index(surface, wavelength_nm)
        direction = prescription.refract(direction, point, surface, n_before, n_after)
        if direction is None:
            return None
        origin = point
    if direction[2] <= 1e-10:
        return None
    return origin, direction


def trace_front_stop_pencil(stop_xy, slope_x, slope_y, wavelength_nm):
    """Trace a stop-point pencil backward through the front group."""
    origin = np.array((stop_xy[0], stop_xy[1], prescription.IRIS_Z), dtype=np.float64)
    direction = np.array((slope_x, slope_y, -1.0), dtype=np.float64)
    direction /= np.linalg.norm(direction)
    for i in range(6, -1, -1):
        surface = prescription.SURFACES[i]
        point = prescription.intersect(origin, direction, surface)
        if point is None:
            return None
        n_before = prescription.refractive_index(surface, wavelength_nm)
        n_after = 1.0 if i == 0 else prescription.refractive_index(
            prescription.SURFACES[i - 1], wavelength_nm)
        direction = prescription.refract(direction, point, surface, n_before, n_after)
        if direction is None:
            return None
        origin = point
    if direction[2] >= -1e-10:
        return None
    return origin, direction


def pupil_image_of_stop_point(stop_xy, pupil_z_mm, wavelength_nm,
                              trace_pencil=trace_stop_pencil):
    slopes = np.linspace(-PUPIL_PROBE_SLOPE, PUPIL_PROBE_SLOPE,
                         PUPIL_PROBE_SAMPLES)
    crossings = []
    for sy in slopes:
        for sx in slopes:
            ray = trace_pencil(stop_xy, sx, sy, wavelength_nm)
            if ray is None:
                continue
            point, direction = ray
            distance_z = pupil_z_mm - point[2]
            crossings.append(point[:2] + distance_z * direction[:2] / direction[2])
    if len(crossings) < 3:
        raise RuntimeError(f"rear-group pupil probe failed for stop point {stop_xy}")
    crossing_array = np.asarray(crossings)
    return crossing_array.mean(axis=0), float(np.sqrt(np.mean(np.sum(
        (crossing_array - crossing_array.mean(axis=0)) ** 2, axis=1))))


def estimate_exit_pupil(wavelength_nm):
    """Find the least-confusion image plane of a near-paraxial stop-center fan."""
    slopes = np.linspace(-PUPIL_PROBE_SLOPE, PUPIL_PROBE_SLOPE,
                         PUPIL_PROBE_SAMPLES)
    positions, ray_slopes = [], []
    for sy in slopes:
        for sx in slopes:
            ray = trace_stop_pencil((0.0, 0.0), sx, sy, wavelength_nm)
            if ray is None:
                continue
            point, direction = ray
            positions.append(point[:2])
            ray_slopes.append(direction[:2] / direction[2])
    positions = np.asarray(positions)
    ray_slopes = np.asarray(ray_slopes)
    z_last = prescription.SURFACES[-1][1]
    intercepts = positions - z_last * ray_slopes
    mean_slope = ray_slopes.mean(axis=0)
    mean_intercept = intercepts.mean(axis=0)
    denominator = float(np.sum((ray_slopes - mean_slope) ** 2))
    if denominator <= 1e-20:
        raise RuntimeError("rear group has no angular spread for pupil imaging")
    pupil_z = -float(np.sum((ray_slopes - mean_slope) *
                            (intercepts - mean_intercept)) / denominator)
    center, blur = pupil_image_of_stop_point((0.0, 0.0), pupil_z, wavelength_nm)
    probe = 0.25
    plus_x, _ = pupil_image_of_stop_point((probe, 0.0), pupil_z, wavelength_nm)
    minus_x, _ = pupil_image_of_stop_point((-probe, 0.0), pupil_z, wavelength_nm)
    plus_y, _ = pupil_image_of_stop_point((0.0, probe), pupil_z, wavelength_nm)
    minus_y, _ = pupil_image_of_stop_point((0.0, -probe), pupil_z, wavelength_nm)
    magnification_x = float((plus_x[0] - minus_x[0]) / (2.0 * probe))
    magnification_y = float((plus_y[1] - minus_y[1]) / (2.0 * probe))
    return pupil_z, center, blur, magnification_x, magnification_y, len(positions)


def estimate_entrance_pupil(wavelength_nm):
    """Estimate the front-group image of the stop using a reverse ray pencil."""
    slopes = np.linspace(-PUPIL_PROBE_SLOPE, PUPIL_PROBE_SLOPE,
                         PUPIL_PROBE_SAMPLES)
    positions, ray_slopes = [], []
    for sy in slopes:
        for sx in slopes:
            ray = trace_front_stop_pencil((0.0, 0.0), sx, sy, wavelength_nm)
            if ray is None:
                continue
            point, direction = ray
            positions.append(point[:2])
            ray_slopes.append(direction[:2] / direction[2])
    positions = np.asarray(positions)
    ray_slopes = np.asarray(ray_slopes)
    z_first = prescription.SURFACES[0][1]
    intercepts = positions - z_first * ray_slopes
    mean_slope = ray_slopes.mean(axis=0)
    mean_intercept = intercepts.mean(axis=0)
    denominator = float(np.sum((ray_slopes - mean_slope) ** 2))
    if denominator <= 1e-20:
        raise RuntimeError("front group has no angular spread for pupil imaging")
    pupil_z = -float(np.sum((ray_slopes - mean_slope) *
                            (intercepts - mean_intercept)) / denominator)
    center, blur = pupil_image_of_stop_point(
        (0.0, 0.0), pupil_z, wavelength_nm, trace_front_stop_pencil)
    probe = 0.25
    plus_x, _ = pupil_image_of_stop_point(
        (probe, 0.0), pupil_z, wavelength_nm, trace_front_stop_pencil)
    minus_x, _ = pupil_image_of_stop_point(
        (-probe, 0.0), pupil_z, wavelength_nm, trace_front_stop_pencil)
    plus_y, _ = pupil_image_of_stop_point(
        (0.0, probe), pupil_z, wavelength_nm, trace_front_stop_pencil)
    minus_y, _ = pupil_image_of_stop_point(
        (0.0, -probe), pupil_z, wavelength_nm, trace_front_stop_pencil)
    magnification_x = float((plus_x[0] - minus_x[0]) / (2.0 * probe))
    magnification_y = float((plus_y[1] - minus_y[1]) / (2.0 * probe))
    return pupil_z, center, blur, magnification_x, magnification_y, len(positions)


def trace_field(field_angle_deg, samples, pupil_z_mm, pupil_center_xy,
                wavelength_nm=WAVELENGTH_NM):
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
    exit_plane_xy = pupil_xy.copy()
    image_z = prescription.SENSOR_Z
    image_point = np.array((chief_xy[0] + (image_z - EXIT_PLANE_Z_MM) *
                            chief_direction[0] / chief_direction[2],
                            chief_xy[1] + (image_z - EXIT_PLANE_Z_MM) *
                            chief_direction[1] / chief_direction[2], image_z))
    # Keep the prior arbitrary-plane reference result for a direct comparison.
    exit_plane_points = np.column_stack((exit_plane_xy,
                                         np.full(len(exit_plane_xy), EXIT_PLANE_Z_MM)))
    chief_exit_point = np.array((chief_xy[0], chief_xy[1], EXIT_PLANE_Z_MM))
    opd_exit_plane = (opl + np.linalg.norm(image_point - exit_plane_points, axis=1) -
                      chief_opl - np.linalg.norm(image_point - chief_exit_point))
    # Back-project each traced ray to the paraxial image of the stop.
    back_distance = (EXIT_PLANE_Z_MM - pupil_z_mm) / directions[:, 2]
    pupil_xy = pupil_xy - back_distance[:, None] * directions[:, :2]
    pupil_opl = opl - back_distance
    chief_back_distance = (EXIT_PLANE_Z_MM - pupil_z_mm) / chief_direction[2]
    chief_pupil_xy = chief_xy - chief_back_distance * chief_direction[:2]
    chief_pupil_opl = chief_opl - chief_back_distance
    pupil_points_3d = np.column_stack((pupil_xy, np.full(len(pupil_xy), pupil_z_mm)))
    chief_point_3d = np.array((chief_pupil_xy[0], chief_pupil_xy[1], pupil_z_mm))
    reference_distance = np.linalg.norm(image_point - pupil_points_3d, axis=1)
    chief_reference_distance = float(np.linalg.norm(image_point - chief_point_3d))
    opd_mm = pupil_opl + reference_distance - chief_pupil_opl - chief_reference_distance
    sphere_center = np.array((pupil_center_xy[0], pupil_center_xy[1], pupil_z_mm))
    reference_radius = float(np.linalg.norm(image_point - sphere_center))
    return (pupil_xy, exit_plane_xy, directions, opl, opd_mm, opd_exit_plane,
            chief_xy, chief_direction, index_map, chief_pupil_xy, image_point,
            reference_radius)


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


def pupil_map_jacobian(pupil_xy, directions, index_map, launch_pitch_mm):
    """Estimate dA_exit/dA_incident and identify folds on valid grid cells."""
    p00i, p10i = index_map[:-1, :-1], index_map[:-1, 1:]
    p01i, p11i = index_map[1:, :-1], index_map[1:, 1:]
    complete = (p00i >= 0) & (p10i >= 0) & (p01i >= 0) & (p11i >= 0)
    p00i, p10i, p01i, p11i = (grid[complete] for grid in (p00i, p10i, p01i, p11i))
    a, b, c, d = (pupil_xy[index] for index in (p00i, p10i, p01i, p11i))
    jacobian_1 = ((b[:, 0] - a[:, 0]) * (d[:, 1] - a[:, 1]) -
                  (b[:, 1] - a[:, 1]) * (d[:, 0] - a[:, 0])) / launch_pitch_mm**2
    jacobian_2 = ((d[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1]) -
                  (d[:, 1] - a[:, 1]) * (c[:, 0] - a[:, 0])) / launch_pitch_mm**2
    triangles = np.vstack((np.column_stack((p00i, p10i, p11i)),
                           np.column_stack((p00i, p11i, p01i))))
    signed_jacobian = np.concatenate((jacobian_1, jacobian_2))
    finite = np.isfinite(signed_jacobian) & (np.abs(signed_jacobian) > 1e-10)
    triangles = triangles[finite]
    signed_jacobian = signed_jacobian[finite]
    if not len(signed_jacobian):
        raise RuntimeError("no complete pupil-map cells for Jacobian estimate")
    dominant_sign = math.copysign(1.0, float(np.median(signed_jacobian)))
    fold_rate = 100.0 * float(np.count_nonzero(signed_jacobian * dominant_sign < 0.0)) / \
        len(signed_jacobian)
    nodal_jacobian_sum = np.zeros(len(pupil_xy), dtype=np.float64)
    nodal_count = np.zeros(len(pupil_xy), dtype=np.int32)
    for vertex in range(3):
        np.add.at(nodal_jacobian_sum, triangles[:, vertex], np.abs(signed_jacobian))
        np.add.at(nodal_count, triangles[:, vertex], 1)
    covered = nodal_count > 0
    nodal_jacobian = np.zeros(len(pupil_xy), dtype=np.float64)
    nodal_jacobian[covered] = nodal_jacobian_sum[covered] / nodal_count[covered]
    amplitude = np.sqrt(1.0 / np.maximum(nodal_jacobian[covered] *
                                         directions[covered, 2], 1e-30))
    mapped_area = float(np.sum(np.abs(signed_jacobian)) * launch_pitch_mm**2 * 0.5)
    return {
        "valid_cells": int(len(signed_jacobian) // 2),
        "valid_triangles": int(len(signed_jacobian)),
        "foldover_triangle_percent": fold_rate,
        "mapped_pupil_area_mm2": mapped_area,
        "jacobian_p01": float(np.quantile(nodal_jacobian[covered], 0.01)),
        "jacobian_median": float(np.median(nodal_jacobian[covered])),
        "jacobian_p99": float(np.quantile(nodal_jacobian[covered], 0.99)),
        "amplitude_p01": float(np.quantile(amplitude, 0.01)),
        "amplitude_median": float(np.median(amplitude)),
        "amplitude_p99": float(np.quantile(amplitude, 0.99)),
        "nodal_jacobian": nodal_jacobian,
        "jacobian_covered": covered,
    }


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
        center = case["pupil_center_xy"]
        offsets = case["pupil_xy"] - center
        span = 22.5
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
                  f"exit-pupil plane; +/-{span:.0f} mm; +/-{limit:.3f} um",
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

    entrance_z, entrance_center, entrance_blur, entrance_mag_x, entrance_mag_y, \
        entrance_probe_rays = estimate_entrance_pupil(args.wavelength_nm)
    pupil_z, pupil_center, pupil_blur, magnification_x, magnification_y, probe_rays = \
        estimate_exit_pupil(args.wavelength_nm)
    entrance_kind = "virtual" if entrance_z > prescription.IRIS_Z else "real"
    pupil_kind = "virtual" if pupil_z < prescription.SURFACES[-1][1] else "real"
    f_number_object = prescription.EFL / (
        2.0 * prescription.STOP_RADIUS * abs(entrance_mag_x))
    print(f"paraxial exit pupil={pupil_kind} z={pupil_z:.5f} mm, "
          f"center=({pupil_center[0]:.5f},{pupil_center[1]:.5f}) mm, "
          f"magnification=({magnification_x:.5f},{magnification_y:.5f}), "
          f"probe blur={pupil_blur*1000.0:.4f} um from {probe_rays} rays")
    print(f"paraxial entrance pupil={entrance_kind} z={entrance_z:.5f} mm, "
          f"center=({entrance_center[0]:.5f},{entrance_center[1]:.5f}) mm, "
          f"magnification=({entrance_mag_x:.5f},{entrance_mag_y:.5f}), "
          f"probe blur={entrance_blur*1000.0:.4f} um from {entrance_probe_rays} rays, "
          f"object-space effective f/{f_number_object:.4f}")
    cases, records = [], []
    for name, angle in FIELD_CASES:
        (xy, exit_xy, directions, opl, opd_mm, exit_plane_opd_mm, chief_xy,
         chief_direction, index_map, chief_pupil_xy, image_point,
         reference_radius) = trace_field(
            angle, args.samples, pupil_z, pupil_center, args.wavelength_nm)
        rms_um = float(np.sqrt(np.mean(opd_mm * opd_mm)) * 1000.0)
        exit_plane_rms_um = float(np.sqrt(np.mean(exit_plane_opd_mm *
                                                  exit_plane_opd_mm)) * 1000.0)
        slope_error, slope_error_percent, pair_count = phase_slope_error(
            exit_xy, opl, directions, index_map)
        map_metrics = pupil_map_jacobian(
            xy, directions, index_map, 70.0 / args.samples)
        opd_um = opd_mm * 1000.0
        case = {"field": name, "angle_deg": angle, "pupil_xy": xy, "opd_um": opd_um,
                "pupil_center_xy": pupil_center, "opd_rms_um": rms_um}
        cases.append(case)
        chief_decenter = float(np.linalg.norm(chief_pupil_xy - pupil_center))
        f_number_image = reference_radius / (
            2.0 * prescription.STOP_RADIUS * abs(magnification_x))
        records.append({
            "field": name, "field_angle_deg": angle,
            "wavelength_nm": args.wavelength_nm, "launch_grid": args.samples,
            "paraxial_entrance_pupil_z_mm": entrance_z,
            "paraxial_entrance_pupil_type": entrance_kind,
            "entrance_pupil_center_x_mm": float(entrance_center[0]),
            "entrance_pupil_center_y_mm": float(entrance_center[1]),
            "entrance_pupil_probe_blur_um": entrance_blur * 1000.0,
            "entrance_pupil_probe_rays": entrance_probe_rays,
            "entrance_pupil_magnification_x": entrance_mag_x,
            "entrance_pupil_magnification_y": entrance_mag_y,
            "effective_f_number_object_space": f_number_object,
            "valid_rays": len(xy), "paraxial_exit_pupil_z_mm": pupil_z,
            "paraxial_exit_pupil_type": pupil_kind,
            "exit_pupil_center_x_mm": float(pupil_center[0]),
            "exit_pupil_center_y_mm": float(pupil_center[1]),
            "exit_pupil_probe_blur_um": pupil_blur * 1000.0,
            "exit_pupil_probe_rays": probe_rays,
            "exit_pupil_magnification_x": magnification_x,
            "exit_pupil_magnification_y": magnification_y,
            "chief_exit_pupil_x_mm": float(chief_pupil_xy[0]),
            "chief_exit_pupil_y_mm": float(chief_pupil_xy[1]),
            "chief_exit_pupil_decenter_mm": chief_decenter,
            "reference_sphere_radius_mm": reference_radius,
            "effective_f_number_image_space": f_number_image,
            "mapped_pupil_area_mm2": map_metrics["mapped_pupil_area_mm2"],
            "paraxial_hex_pupil_area_mm2":
                1.5 * math.sqrt(3.0) * (prescription.STOP_RADIUS *
                                          abs(magnification_x))**2,
            "pupil_map_foldover_triangle_percent":
                map_metrics["foldover_triangle_percent"],
            "pupil_map_valid_cells": map_metrics["valid_cells"],
            "pupil_map_valid_triangles": map_metrics["valid_triangles"],
            "pupil_map_jacobian_p01": map_metrics["jacobian_p01"],
            "pupil_map_jacobian_median": map_metrics["jacobian_median"],
            "pupil_map_jacobian_p99": map_metrics["jacobian_p99"],
            "pupil_amplitude_proxy_p01": map_metrics["amplitude_p01"],
            "pupil_amplitude_proxy_median": map_metrics["amplitude_median"],
            "pupil_amplitude_proxy_p99": map_metrics["amplitude_p99"],
            "entrance_exit_f_number_mismatch_percent":
                100.0 * abs(f_number_image - f_number_object) / f_number_object,
            "exit_plane_reference_opd_rms_um": exit_plane_rms_um,
            "chief_x_mm": float(chief_xy[0]), "chief_y_mm": float(chief_xy[1]),
            "chief_image_x_mm": float(image_point[0]),
            "chief_image_y_mm": float(image_point[1]),
            "chief_direction_x": float(chief_direction[0]),
            "chief_direction_y": float(chief_direction[1]),
            "opd_rms_um_exit_pupil_reference_sphere": rms_um,
            "opd_rms_waves": rms_um * 1000.0 / args.wavelength_nm,
            "opd_p99_minus_p01_um": float(np.quantile(opd_um, .99) -
                                            np.quantile(opd_um, .01)),
            "eikonal_gradient_rms_error": slope_error,
            "eikonal_gradient_rms_error_percent": slope_error_percent,
            "gradient_neighbor_pairs": pair_count,
        })
        print(f"{name:14s} rays={len(xy):5d} exit-pupil OPD RMS={rms_um:.5f} um "
              f"({records[-1]['opd_rms_waves']:.4f} waves), "
              f"chief pupil decenter={chief_decenter:.3f} mm, "
              f"pupil folds={map_metrics['foldover_triangle_percent']:.4f}%, "
              f"mapped area={map_metrics['mapped_pupil_area_mm2']:.2f} mm^2, "
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
