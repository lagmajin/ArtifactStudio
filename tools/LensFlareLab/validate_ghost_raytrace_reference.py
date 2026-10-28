"""Compare LensFlareLab ghost rays with an independent float64 prescription trace."""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


EFL_MM = 100.0
STOP_RADIUS_MM = EFL_MM / 22.0
SENSOR_WIDTH_MM = 53.96
SURFACE_DATA = (
    (164.13, 10.99, 1.6751, 32.2, 27.0),
    (559.20, 0.23, 1.0000, 0.0, 27.0),
    (100.12, 11.45, 1.6689, 47.1, 25.5),
    (213.54, 0.23, 1.0000, 0.0, 25.5),
    (58.04, 22.95, 1.6913, 54.7, 20.5),
    (2551.10, 2.58, 1.6751, 32.2, 20.5),
    (32.39, 15.66, 1.0000, 0.0, 13.5),
    (-40.42, 2.74, 1.6992, 30.1, 12.5),
    (192.98, 27.92, 1.6204, 60.3, 18.0),
    (-55.53, 0.23, 1.0000, 0.0, 18.0),
    (192.98, 7.98, 1.6913, 54.7, 17.5),
    (-225.30, 0.23, 1.0000, 0.0, 17.5),
    (175.08, 8.48, 1.6913, 54.7, 17.5),
    (-203.55, 55.742, 1.0000, 0.0, 17.5),
)


def surfaces_with_z():
    result = []
    z = 0.0
    for index, (radius, thickness, index_d, abbe, semi_aperture) in enumerate(SURFACE_DATA):
        if index == 7:
            z += 15.0  # Prescription table's 15 mm iris thickness / rear-group spacing.
        result.append((radius, z, thickness, index_d, abbe, semi_aperture))
        z += thickness
    return result


SURFACES = surfaces_with_z()
IRIS_Z = SURFACES[6][1] + SURFACES[6][2]
SENSOR_Z = SURFACES[-1][1] + SURFACES[-1][2]
EXIT_Z = SURFACES[-1][1] + 1.0


def refractive_index(surface, wavelength_nm):
    _, _, _, index_d, abbe, _ = surface
    if abbe <= 0.0:
        return 1.0
    slope = ((index_d - 1.0) / abbe) * 0.52345
    offset = index_d - slope * 2.897
    wavelength_um = wavelength_nm * 1e-3
    return offset + slope / (wavelength_um * wavelength_um)


def surface_normal(surface, point):
    radius, z, *_ = surface
    if abs(radius) > 1e6:
        return np.array((0.0, 0.0, -1.0))
    center = np.array((0.0, 0.0, z + radius))
    normal = point - center
    return normal / np.linalg.norm(normal)


def intersect(point, direction, surface):
    radius, z, _, _, _, semi_aperture = surface
    if abs(radius) > 1e6:
        if abs(direction[2]) < 1e-7:
            return None
        distance = (z - point[2]) / direction[2]
    else:
        center = np.array((0.0, 0.0, z + radius))
        offset = point - center
        b = float(np.dot(offset, direction))
        c = float(np.dot(offset, offset) - radius * radius)
        discriminant = b * b - c
        if discriminant < 0.0:
            return None
        root = math.sqrt(discriminant)
        roots = [value for value in (-b - root, -b + root) if value > 1e-5]
        if not roots:
            return None
        distance = min(roots)
    if not distance > 1e-5:
        return None
    hit = point + direction * distance
    if math.hypot(hit[0], hit[1]) > semi_aperture:
        return None
    return hit


def refract(direction, normal, n1, n2):
    if float(np.dot(direction, normal)) > 0.0:
        normal = -normal
    cos1 = min(max(float(-np.dot(normal, direction)), 0.0), 1.0)
    eta = n1 / n2
    k = 1.0 - eta * eta * (1.0 - cos1 * cos1)
    if k < 0.0:
        return None
    result = direction * eta + normal * (eta * cos1 - math.sqrt(k))
    norm = float(np.linalg.norm(result))
    result /= norm if norm > 1e-12 else 1.0
    return result if abs(result[2]) > 1e-6 else None


def polygon_stop_contains(x, y):
    sector = 2.0 * math.pi / 6.0
    sector_angle = math.remainder(math.atan2(y, x), sector)
    boundary = STOP_RADIUS_MM * math.cos(0.5 * sector) / math.cos(sector_angle)
    return math.hypot(x, y) <= boundary


def trace_front_to_iris(origin, direction, wavelength_nm):
    origin = origin.copy()
    direction = direction.copy()
    for index, surface in enumerate(SURFACES[:7]):
        origin = intersect(origin, direction, surface)
        if origin is None:
            return None
        n_before = 1.0 if index == 0 else refractive_index(SURFACES[index - 1], wavelength_nm)
        n_after = refractive_index(surface, wavelength_nm)
        direction = refract(direction, surface_normal(surface, origin), n_before, n_after)
        if direction is None:
            return None
    distance = (IRIS_Z - origin[2]) / direction[2]
    if distance <= 0.0:
        return None
    return origin + direction * distance


def trace_reference(u, v, wavelength_nm, source_u, source_v, image_aspect):
    """Trace the 9→8 ghost using independent float64 scalar equations."""
    stop_x, stop_y = u * STOP_RADIUS_MM, v * STOP_RADIUS_MM
    origin = np.array((stop_x, stop_y, IRIS_Z))
    theta_x = math.atan((source_u - 0.5) * SENSOR_WIDTH_MM / EFL_MM)
    theta_y = math.atan((0.5 - source_v) * SENSOR_WIDTH_MM /
                        (image_aspect * EFL_MM))
    incident = np.array((math.sin(theta_x), math.sin(theta_y),
                         math.cos(theta_x) * math.cos(theta_y)))
    incident /= np.linalg.norm(incident)
    direction = -incident
    for index in range(6, -1, -1):
        surface = SURFACES[index]
        origin = intersect(origin, direction, surface)
        if origin is None:
            return None
        n_forward = refractive_index(surface, wavelength_nm)
        n_previous = 1.0 if index == 0 else refractive_index(SURFACES[index - 1], wavelength_nm)
        direction = refract(direction, surface_normal(surface, origin), n_forward, n_previous)
        if direction is None:
            return None

    backtraced_forward = -direction
    origin = origin - backtraced_forward * 0.05
    entrance_origin_point = origin.copy()
    direction = incident.copy()
    finite_difference_step = 1e-4
    stop_tolerance = 1e-7
    entrance_stop_point = None
    for _ in range(12):
        entrance_stop_point = trace_front_to_iris(origin, direction, wavelength_nm)
        if entrance_stop_point is None:
            return None
        error = entrance_stop_point[:2] - (stop_x, stop_y)
        if float(np.linalg.norm(error)) <= stop_tolerance:
            break
        hit_x = trace_front_to_iris(origin + (finite_difference_step, 0.0, 0.0),
                                    direction, wavelength_nm)
        hit_y = trace_front_to_iris(origin + (0.0, finite_difference_step, 0.0),
                                    direction, wavelength_nm)
        if hit_x is None or hit_y is None:
            return None
        jacobian = np.column_stack(((hit_x[:2] - entrance_stop_point[:2]) / finite_difference_step,
                                    (hit_y[:2] - entrance_stop_point[:2]) / finite_difference_step))
        try:
            delta = np.linalg.solve(jacobian, error)
        except np.linalg.LinAlgError:
            return None
        if not np.all(np.isfinite(delta)) or float(np.linalg.norm(delta)) > 20.0:
            return None
        origin[:2] -= delta
    entrance_stop_point = trace_front_to_iris(origin, direction, wavelength_nm)
    if entrance_stop_point is None:
        return None
    stop_miss = float(np.linalg.norm(entrance_stop_point[:2] - (stop_x, stop_y)))
    if stop_miss > stop_tolerance:
        return None
    previous = origin.copy()
    segment_index = 1.0
    opl = 0.0

    def segment_to(point, current_index):
        nonlocal previous, opl
        opl += current_index * float(np.linalg.norm(point - previous))
        previous = point.copy()

    for index in range(10):
        if index == 7:
            distance = (IRIS_Z - origin[2]) / direction[2]
            if not distance > 1e-5:
                return None
            origin = origin + direction * distance
            if not polygon_stop_contains(origin[0], origin[1]):
                return None
            segment_to(origin, segment_index)
        surface = SURFACES[index]
        origin = intersect(origin, direction, surface)
        if origin is None:
            return None
        segment_to(origin, segment_index)
        n_before = 1.0 if index == 0 else refractive_index(SURFACES[index - 1], wavelength_nm)
        n_after = refractive_index(surface, wavelength_nm)
        if index == 9:
            normal = surface_normal(surface, origin)
            direction = direction - 2.0 * float(np.dot(direction, normal)) * normal
            direction /= np.linalg.norm(direction)
            segment_index = n_before
        else:
            direction = refract(direction, surface_normal(surface, origin), n_before, n_after)
            if direction is None:
                return None
            segment_index = n_after

    for index in range(8, 7, -1):
        surface = SURFACES[index]
        origin = intersect(origin, direction, surface)
        if origin is None:
            return None
        segment_to(origin, segment_index)
        n_from = refractive_index(surface, wavelength_nm)
        n_to = refractive_index(SURFACES[index - 1], wavelength_nm)
        normal = surface_normal(surface, origin)
        direction = direction - 2.0 * float(np.dot(direction, normal)) * normal
        direction /= np.linalg.norm(direction)
        segment_index = n_from

    for index in range(9, len(SURFACES)):
        surface = SURFACES[index]
        origin = intersect(origin, direction, surface)
        if origin is None:
            return None
        segment_to(origin, segment_index)
        n_before = 1.0 if index == 0 else refractive_index(SURFACES[index - 1], wavelength_nm)
        n_after = refractive_index(surface, wavelength_nm)
        direction = refract(direction, surface_normal(surface, origin), n_before, n_after)
        if direction is None:
            return None
        segment_index = n_after

    exit_point = origin + direction * ((EXIT_Z - origin[2]) / direction[2])
    segment_to(exit_point, segment_index)
    incident_plane_path = float(np.dot(entrance_origin_point, incident) +
                                 incident[2] * 100.0)
    exit_opl = opl + incident_plane_path
    sensor_point = origin + direction * ((SENSOR_Z - origin[2]) / direction[2])
    return {"exit_x": exit_point[0], "exit_y": exit_point[1], "exit_z": exit_point[2],
            "dir_x": direction[0], "dir_y": direction[1], "dir_z": direction[2],
            "exit_opl": exit_opl, "sensor_x": sensor_point[0], "sensor_y": sensor_point[1],
            "backprojected_stop_miss_mm": stop_miss}


def load_rows(path: Path, grid: int, wavelength: int):
    rows = []
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        for row in reader:
            if int(round(float(row["wavelength_nm"]))) == wavelength:
                rows.append(row)
    if len(rows) != grid * grid:
        raise ValueError(f"expected {grid*grid} rows for {wavelength} nm; got {len(rows)}")
    return rows


def make_error_image(samples, residual_um, output: Path, wavelength: int, grid: int):
    size = 512
    image = Image.new("RGB", (size + 130, size + 80), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    finite = np.isfinite(residual_um)
    limit = max(float(np.percentile(np.abs(residual_um[finite]), 98.0)), 1e-6)
    for sample, value in zip(samples, residual_um):
        if not math.isfinite(float(value)):
            continue
        u, v = sample["u"], sample["v"]
        x = int(round((u + 1.0) * 0.5 * (size - 1)))
        y = int(round((1.0 - v) * 0.5 * (size - 1)))
        strength = min(abs(float(value)) / limit, 1.0)
        color = ((int(255 * strength), int(190 * (1 - strength)), 50) if value >= 0 else
                 (50, int(190 * (1 - strength)), int(255 * strength)))
        draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=color)
    draw.rectangle((0, 0, size - 1, size - 1), outline=(90, 96, 106))
    draw.text((size + 10, 16), f"{wavelength} nm", fill=(235, 238, 244))
    draw.text((size + 10, 42), f"grid {grid}²", fill=(210, 215, 224))
    draw.text((size + 10, 68), f"samples {len(samples)}", fill=(210, 215, 224))
    draw.text((size + 10, 96), f"±{limit:.4f} µm", fill=(210, 215, 224))
    draw.text((size + 10, 124), "red: + OPD", fill=(255, 120, 90))
    draw.text((size + 10, 150), "blue: - OPD", fill=(100, 160, 255))
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map", type=Path)
    parser.add_argument("--grid", type=int, required=True)
    parser.add_argument("--wavelength-nm", type=int, default=550)
    parser.add_argument("--samples", type=int, default=512)
    parser.add_argument("--source-u", type=float, default=32.0 / 63.0)
    parser.add_argument("--source-v", type=float, default=32.0 / 63.0)
    parser.add_argument("--image-aspect", type=float, default=1.0,
                        help="source image width divided by height")
    parser.add_argument("--csv", type=Path,
                        default=Path("build/tools/LensFlareLab/ghost-raytrace-reference.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("build/tools/LensFlareLab/ghost-raytrace-reference.png"))
    args = parser.parse_args()
    rows = load_rows(args.map, args.grid, args.wavelength_nm)
    valid_indices = np.array([i for i, row in enumerate(rows) if int(row["valid"]) != 0])
    if valid_indices.size == 0:
        parser.error("map has no valid rays")
    sample_count = min(args.samples, valid_indices.size)
    chosen = valid_indices[np.linspace(0, valid_indices.size - 1, sample_count, dtype=int)]
    measured, errors = [], []
    fields = ("exit_x", "exit_y", "exit_z", "dir_x", "dir_y", "dir_z",
              "exit_opl", "sensor_x", "sensor_y")
    for index in chosen:
        row = rows[int(index)]
        sample = {"u": float(row["pupil_u"]), "v": float(row["pupil_v"])}
        reference = trace_reference(sample["u"], sample["v"], args.wavelength_nm,
                                    args.source_u, args.source_v, args.image_aspect)
        if reference is None:
            continue
        map_columns = {
            "exit_x": "exit_plane_x_mm", "exit_y": "exit_plane_y_mm",
            "exit_z": "exit_plane_z_mm", "dir_x": "exit_dir_x",
            "dir_y": "exit_dir_y", "dir_z": "exit_dir_z",
            "exit_opl": "exit_opl_mm", "sensor_x": "sensor_x_mm",
            "sensor_y": "sensor_y_mm"}
        sample.update({f"map_{name}": float(row[map_columns[name]]) for name in fields})
        sample.update({f"ref_{name}": float(reference[name]) for name in fields})
        sample["backprojected_stop_miss_um"] = reference["backprojected_stop_miss_mm"] * 1000.0
        for name in fields:
            sample[f"error_{name}"] = sample[f"map_{name}"] - sample[f"ref_{name}"]
        measured.append(sample)

    if len(measured) < 16:
        parser.error(f"only {len(measured)} samples reproduced as valid in the reference trace")
    stop_xy = np.array([(sample["u"] * STOP_RADIUS_MM,
                         sample["v"] * STOP_RADIUS_MM) for sample in measured])
    opl_error = np.array([sample["error_exit_opl"] for sample in measured])
    design = np.column_stack((np.ones(len(measured)), stop_xy))
    coefficients, *_ = np.linalg.lstsq(design, opl_error, rcond=None)
    residual_um = (opl_error - design @ coefficients) * 1000.0
    for i, sample in enumerate(measured):
        sample["opl_residual_after_piston_tilt_um"] = float(residual_um[i])

    summary = []
    for field in fields:
        values = np.array([sample[f"error_{field}"] for sample in measured])
        if field == "exit_opl":
            values = values * 1000.0
            unit = "um"
        elif field.startswith("dir_"):
            values = values * 1e6
            unit = "ppm"
        else:
            values = values * 1000.0
            unit = "um"
        summary.append((field, float(np.sqrt(np.mean(values * values))),
                        float(np.percentile(np.abs(values), 95.0)), unit))
    miss = np.array([sample["backprojected_stop_miss_um"] for sample in measured])
    print(f"re-forwarded stop miss: RMS={np.sqrt(np.mean(miss**2)):.3f} um, "
          f"p95={np.percentile(miss, 95):.3f} um, max={np.max(miss):.3f} um")
    print(f"float64 reference valid rays={len(measured)}/{sample_count}; "
          f"OPL residual after piston/tilt RMS={np.sqrt(np.mean(residual_um**2)):.5f} um")
    for name, rms, p95, unit in summary:
        print(f"{name}: RMS={rms:.6g} {unit}, abs-p95={p95:.6g} {unit}")

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(measured[0]))
        writer.writeheader()
        writer.writerows(measured)
    make_error_image(measured, residual_um, args.image, args.wavelength_nm, args.grid)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
