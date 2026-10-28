"""Build a path-specific exit-pupil wavefront and sensor PSF from ghost rays.

The input map must include the shared image-side exit plane, ray direction,
exit-plane OPL, sensor intercept, and sensor-plane z written by LensFlareLab.
The evaluator back-projects the ray bundle to its least-squares pupil plane,
applies an image-centered reference sphere, regrids the physical pupil, and
integrates a scalar Fraunhofer PSF over sensor-pixel areas. It is still a
monochromatic scalar diagnostic, not a calibrated full-frame flare renderer.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.spatial import Delaunay


SENSOR_WIDTH_MM = 53.96
SENSOR_WIDTH_PX = 640
SENSOR_HEIGHT_PX = 360
DEFAULT_WAVELENGTHS = (460, 550, 610)


def read_map(path: Path, grid: int, wavelength: int) -> dict[str, np.ndarray]:
    count = grid * grid
    names = ("u", "v", "valid", "energy", "exit_x", "exit_y", "exit_z",
             "dir_x", "dir_y", "dir_z", "exit_opl", "sensor_x", "sensor_y",
             "sensor_z")
    arrays = {name: np.empty(count, dtype=bool if name == "valid" else np.float64)
              for name in names}
    index = 0
    required = {"exit_plane_x_mm", "exit_plane_y_mm", "exit_plane_z_mm",
                "exit_dir_x", "exit_dir_y", "exit_dir_z", "exit_opl_mm", "sensor_z_mm"}
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if not required.issubset(set(reader.fieldnames or ())):
            raise ValueError(f"{path} lacks exit-plane ray data; regenerate with the current LensFlareLab")
        for row in reader:
            if int(round(float(row["wavelength_nm"]))) != wavelength:
                continue
            if index >= count:
                raise ValueError(f"too many {wavelength} nm records in {path}")
            values = {
                "u": float(row["pupil_u"]), "v": float(row["pupil_v"]),
                "valid": int(row["valid"]) != 0, "energy": float(row["energy"]),
                "exit_x": float(row["exit_plane_x_mm"]),
                "exit_y": float(row["exit_plane_y_mm"]),
                "exit_z": float(row["exit_plane_z_mm"]),
                "dir_x": float(row["exit_dir_x"]), "dir_y": float(row["exit_dir_y"]),
                "dir_z": float(row["exit_dir_z"]),
                "exit_opl": float(row["exit_opl_mm"]),
                "sensor_x": float(row["sensor_x_mm"]),
                "sensor_y": float(row["sensor_y_mm"]),
                "sensor_z": float(row["sensor_z_mm"]),
            }
            for name, value in values.items():
                arrays[name][index] = value
            index += 1
    if index != count:
        raise ValueError(f"expected {count} {wavelength} nm records in {path}, got {index}")
    return {name: value.reshape(grid, grid) for name, value in arrays.items()}


def fit_exit_pupil(data: dict[str, np.ndarray]) -> tuple[np.ndarray, np.ndarray, float, float]:
    valid = data["valid"]
    z_values = data["exit_z"][valid]
    if valid.sum() < 16:
        raise ValueError("too few valid rays to estimate the ghost exit pupil")
    exit_xy = np.column_stack((data["exit_x"][valid], data["exit_y"][valid]))
    direction = np.column_stack((data["dir_x"][valid], data["dir_y"][valid],
                                 data["dir_z"][valid]))
    if np.any(direction[:, 2] <= 1e-6):
        raise ValueError("exit-pupil fit requires forward-going image-side rays")
    slopes = direction[:, :2] / direction[:, 2, None]
    exit_center = exit_xy.mean(axis=0)
    slope_center = slopes.mean(axis=0)
    centered_xy = exit_xy - exit_center
    centered_slopes = slopes - slope_center
    denominator = float(np.sum(centered_slopes * centered_slopes))
    if denominator <= 1e-16:
        raise ValueError("image-side ray slopes do not determine a finite pupil plane")
    back_projection_mm = float(np.sum(centered_xy * centered_slopes) / denominator)
    pupil_xy = exit_xy - back_projection_mm * slopes
    pupil_center = pupil_xy.mean(axis=0)
    radial_error = np.linalg.norm(pupil_xy - pupil_center, axis=1)
    rms = math.sqrt(float(np.mean(radial_error * radial_error)))
    pupil_z = float(np.mean(z_values) - back_projection_mm)
    return pupil_xy, pupil_center, pupil_z, rms


def image_centered_opd(data: dict[str, np.ndarray], wavelength: int,
                        pupil_xy: np.ndarray, pupil_center: np.ndarray,
                        pupil_z: float) -> tuple[np.ndarray, np.ndarray, float, np.ndarray]:
    valid = data["valid"]
    u, v = data["u"][valid], data["v"][valid]
    center_index = int(np.argmin(u * u + v * v))
    chief_exit = np.array((data["exit_x"][valid][center_index],
                           data["exit_y"][valid][center_index],
                           data["exit_z"][valid][center_index]))
    chief_direction = np.array((data["dir_x"][valid][center_index],
                                 data["dir_y"][valid][center_index],
                                 data["dir_z"][valid][center_index]))
    sensor_z_values = data["sensor_z"][valid]
    sensor_z = float(np.median(sensor_z_values))
    sensor_z_error = float(np.max(np.abs(sensor_z_values - sensor_z)))
    if sensor_z_error > 1e-4:
        raise ValueError("sensor plane z varies across the optical-path map")
    image_point = np.array((data["sensor_x"][valid][center_index],
                            data["sensor_y"][valid][center_index], sensor_z))

    back_projection = (float(np.mean(data["exit_z"][valid])) - pupil_z)
    distance = back_projection / data["dir_z"][valid]
    pupil_opl = data["exit_opl"][valid] - distance
    chief_pupil = chief_exit.copy()
    chief_back = back_projection / chief_direction[2]
    chief_pupil[:2] -= chief_back * chief_direction[:2]
    chief_pupil[2] = pupil_z
    chief_pupil_opl = data["exit_opl"][valid][center_index] - chief_back

    pupil_points = np.column_stack((pupil_xy, np.full(len(pupil_xy), pupil_z)))
    reference_distance = np.linalg.norm(image_point[None, :] - pupil_points, axis=1)
    chief_reference_distance = float(np.linalg.norm(image_point - chief_pupil))
    opd = pupil_opl + reference_distance - chief_pupil_opl - chief_reference_distance
    pupil_center_3d = np.array((pupil_center[0], pupil_center[1], pupil_z))
    reference_radius = float(np.linalg.norm(image_point - pupil_center_3d))
    return opd, image_point, reference_radius, chief_pupil


def map_to_regular_pupil(data: dict[str, np.ndarray], pupil_xy: np.ndarray,
                         opd_mm: np.ndarray, resolution: int) -> tuple[np.ndarray, float, np.ndarray]:
    grid = data["u"].shape[0]
    valid = data["valid"]
    launch_grid = np.argwhere(valid)
    launch_uv = np.column_stack((data["u"][valid], data["v"][valid]))
    triangulation = Delaunay(pupil_xy)
    triangles = triangulation.simplices
    triangle_launch = launch_grid[triangles]
    local = (np.ptp(triangle_launch[:, :, 0], axis=1) <= 1) & \
            (np.ptp(triangle_launch[:, :, 1], axis=1) <= 1)
    pupil_triangle = pupil_xy[triangles]
    uv_triangle = launch_uv[triangles]
    pupil_cross = ((pupil_triangle[:, 1, 0] - pupil_triangle[:, 0, 0]) *
                   (pupil_triangle[:, 2, 1] - pupil_triangle[:, 0, 1]) -
                   (pupil_triangle[:, 1, 1] - pupil_triangle[:, 0, 1]) *
                   (pupil_triangle[:, 2, 0] - pupil_triangle[:, 0, 0]))
    uv_cross = ((uv_triangle[:, 1, 0] - uv_triangle[:, 0, 0]) *
                (uv_triangle[:, 2, 1] - uv_triangle[:, 0, 1]) -
                (uv_triangle[:, 1, 1] - uv_triangle[:, 0, 1]) *
                (uv_triangle[:, 2, 0] - uv_triangle[:, 0, 0]))
    jacobian = pupil_cross / np.maximum(np.abs(uv_cross), 1e-30) * np.sign(uv_cross)
    local &= np.isfinite(jacobian) & (jacobian > 1e-12)
    if not np.any(local):
        raise ValueError("ghost ray map has no local, positive-Jacobian pupil triangles")

    nodal_jacobian = np.zeros(len(pupil_xy), dtype=np.float64)
    nodal_count = np.zeros(len(pupil_xy), dtype=np.float64)
    for corner in range(3):
        np.add.at(nodal_jacobian, triangles[local, corner], jacobian[local])
        np.add.at(nodal_count, triangles[local, corner], 1.0)
    nodal_jacobian /= np.maximum(nodal_count, 1.0)
    nodal_energy = data["energy"][valid]
    amplitude = np.sqrt(nodal_energy / np.maximum(nodal_jacobian, 1e-30))

    center = pupil_xy.mean(axis=0)
    half_span = 0.5 * max(float(np.ptp(pupil_xy[:, 0])), float(np.ptp(pupil_xy[:, 1])))
    pitch = 2.0 * half_span / resolution
    axis = (np.arange(resolution, dtype=np.float64) + 0.5 - resolution * 0.5) * pitch
    pupil_opd = np.zeros((resolution, resolution), dtype=np.float64)
    pupil_amplitude = np.zeros_like(pupil_opd)
    pupil_valid = np.zeros_like(pupil_opd, dtype=bool)

    # Local-triangle interpolation avoids bridging clipped or disconnected parts
    # of the ghost pupil.
    chunk = max(1, min(32, resolution))
    for y0 in range(0, resolution, chunk):
        y1 = min(resolution, y0 + chunk)
        xx, yy = np.meshgrid(center[0] + axis,
                             center[1] + axis[y0:y1])
        query = np.column_stack((xx.ravel(), yy.ravel()))
        simplex = triangulation.find_simplex(query)
        inside = simplex >= 0
        safe = np.maximum(simplex, 0)
        inside &= local[safe]
        if not np.any(inside):
            continue
        selected = safe[inside]
        transform = triangulation.transform[selected]
        delta = query[inside] - transform[:, 2]
        bary01 = np.einsum("nij,nj->ni", transform[:, :2], delta)
        bary = np.column_stack((bary01, 1.0 - bary01.sum(axis=1)))
        vertices = triangles[selected]
        target = np.flatnonzero(inside)
        flat_opd = pupil_opd[y0:y1].ravel()
        flat_amp = pupil_amplitude[y0:y1].ravel()
        flat_valid = pupil_valid[y0:y1].ravel()
        flat_opd[target] = np.sum(opd_mm[vertices] * bary, axis=1)
        flat_amp[target] = np.sum(amplitude[vertices] * bary, axis=1)
        flat_valid[target] = True

    if not np.any(pupil_valid):
        raise ValueError("ghost pupil did not cover a regular physical pupil grid")
    return pupil_opd, pitch, np.where(pupil_valid, pupil_amplitude, 0.0)


def psf_and_pixel_integral(opd_mm: np.ndarray, amplitude: np.ndarray, wavelength: int,
                           pitch_mm: float, reference_radius_mm: float,
                           crop_radius: int) -> tuple[np.ndarray, np.ndarray, float]:
    pupil_phase = opd_mm * 1e6 / wavelength
    pupil = amplitude * np.exp(2j * math.pi * pupil_phase)
    field = np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(pupil)))
    intensity = np.abs(field) ** 2
    intensity /= max(float(intensity.sum()), 1e-30)
    flat_field = np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(amplitude.astype(np.complex128))))
    flat_intensity = np.abs(flat_field) ** 2
    flat_intensity /= max(float(flat_intensity.sum()), 1e-30)

    sensor_pixel_pitch = SENSOR_WIDTH_MM / SENSOR_WIDTH_PX
    sample_pitch_px = (wavelength * 1e-6 * reference_radius_mm /
                       (intensity.shape[0] * pitch_mm * sensor_pixel_pitch))
    coordinates = (np.arange(intensity.shape[0], dtype=np.float64) -
                   intensity.shape[0] // 2) * sample_pitch_px
    base = np.floor(coordinates + 0.5).astype(np.int32)
    left = np.maximum(0.0, (base - 0.5) - (coordinates - 0.5 * sample_pitch_px)) / sample_pitch_px
    right = np.maximum(0.0, (coordinates + 0.5 * sample_pitch_px) - (base + 0.5)) / sample_pitch_px
    edge_index = np.where(left > 0.0, base - 1, base + 1)
    center_weight = 1.0 - left - right
    edge_weight = left + right

    def reduce_to_sensor(source: np.ndarray) -> np.ndarray:
        output = np.zeros((2 * crop_radius + 1, 2 * crop_radius + 1), dtype=np.float64)
        rows = ((base, center_weight), (edge_index, edge_weight))
        columns = ((base, center_weight), (edge_index, edge_weight))
        for row_indices, row_weights in rows:
            valid_rows = (row_indices >= -crop_radius) & (row_indices <= crop_radius)
            out_rows = row_indices[valid_rows] + crop_radius
            for col_indices, col_weights in columns:
                valid_cols = (col_indices >= -crop_radius) & (col_indices <= crop_radius)
                out_cols = col_indices[valid_cols] + crop_radius
                contribution = (source[np.ix_(valid_rows, valid_cols)] *
                                row_weights[valid_rows, None] * col_weights[None, valid_cols])
                np.add.at(output, (out_rows[:, None], out_cols[None, :]), contribution)
        return output

    return reduce_to_sensor(intensity), reduce_to_sensor(flat_intensity), sample_pitch_px


def encircled_radius(image: np.ndarray, fraction: float) -> float:
    center = image.shape[0] // 2
    y, x = np.mgrid[:image.shape[0], :image.shape[1]]
    radii = np.hypot(x - center, y - center).ravel()
    values = np.maximum(image, 0.0).ravel()
    order = np.argsort(radii)
    cumulative = np.cumsum(values[order])
    if cumulative.size == 0 or cumulative[-1] <= 0.0:
        return 0.0
    index = min(int(np.searchsorted(cumulative, fraction * cumulative[-1])), len(order) - 1)
    return float(radii[order[index]])


def make_chart(cases: list[dict], output: Path) -> None:
    tile, header_height, footer_height = 256, 26, 24
    row_height = header_height + tile + footer_height
    image = Image.new("RGB", (tile * 3, row_height * len(cases)), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    for row_index, case in enumerate(cases):
        top = row_index * row_height
        for column, (key, title) in enumerate((
                ("phase", "Reference-sphere OPL PSF"),
                ("flat", "Same pupil, flat phase"),
                ("difference", "Absolute difference"))):
            left = column * tile
            draw.text((left + 6, top + 6), title, fill=(235, 238, 244))
            data = case[key]
            shown = np.log1p(np.maximum(data, 0.0) * 1e6)
            shown /= max(float(shown.max()), 1e-30)
            gray = Image.fromarray(np.clip(shown * 255 + 0.5, 0, 255).astype(np.uint8))
            image.paste(gray.resize((tile, tile), Image.Resampling.NEAREST),
                        (left, top + header_height))
            draw.rectangle((left, top + header_height, left + tile - 1,
                            top + header_height + tile - 1), outline=(62, 68, 78))
        draw.text((8, top + header_height + tile + 5),
                  f"{case['wavelength_nm']} nm, {case['grid']}² ray map; pixel-integrated",
                  fill=(210, 215, 224))
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grid", type=int, action="append", choices=(64, 128, 256, 512, 1024),
                        help="optical-path-map grids; repeat for convergence")
    parser.add_argument("--map-pattern", default="build/tools/LensFlareLab/ghost-exit-grid-{grid}.csv")
    parser.add_argument("--wavelengths-nm", type=int, nargs="+", default=DEFAULT_WAVELENGTHS)
    parser.add_argument("--crop-radius", type=int, default=32)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/ghost-exit-pupil-psf-20261028.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/ghost-exit-pupil-psf-20261028.png"))
    args = parser.parse_args()
    grids = sorted(set(args.grid or [256, 512]))
    records, cases = [], []
    previous_psfs: dict[int, np.ndarray] = {}
    for grid in grids:
        path = Path(args.map_pattern.format(grid=grid))
        if not path.is_file():
            parser.error(f"missing optical-path map: {path}")
        for wavelength in args.wavelengths_nm:
            data = read_map(path, grid, wavelength)
            pupil_xy, pupil_center, pupil_z, plane_rms = fit_exit_pupil(data)
            opd, image_point, reference_radius, chief_pupil = image_centered_opd(
                data, wavelength, pupil_xy, pupil_center, pupil_z)
            opd_full = np.zeros((grid, grid), dtype=np.float64)
            opd_full[data["valid"]] = opd
            regular_opd, pupil_pitch, amplitude = map_to_regular_pupil(
                data, pupil_xy, opd, grid)
            phase_psf, flat_psf, pixel_scale = psf_and_pixel_integral(
                regular_opd, amplitude, wavelength, pupil_pitch, reference_radius,
                args.crop_radius)
            total = float(phase_psf.sum())
            flat_total = float(flat_psf.sum())
            normalized_phase = phase_psf / max(total, 1e-30)
            normalized_flat = flat_psf / max(flat_total, 1e-30)
            l1 = 100.0 * float(np.abs(normalized_phase - normalized_flat).sum())
            previous = previous_psfs.get(wavelength)
            grid_l1 = (float("nan") if previous is None else
                       100.0 * float(np.abs(normalized_phase - previous).sum()))
            previous_psfs[wavelength] = normalized_phase
            record = {
                "grid": grid, "wavelength_nm": wavelength,
                "valid_stop_ray_fraction": float(data["valid"].mean()),
                "exit_pupil_z_mm": pupil_z,
                "exit_pupil_center_x_mm": float(pupil_center[0]),
                "exit_pupil_center_y_mm": float(pupil_center[1]),
                "exit_pupil_back_projection_mm": float(np.mean(data["exit_z"][data["valid"]]) - pupil_z),
                "exit_pupil_plane_fit_rms_mm": plane_rms,
                "reference_sphere_radius_mm": reference_radius,
                "chief_image_x_mm": float(image_point[0]),
                "chief_image_y_mm": float(image_point[1]),
                "chief_pupil_x_mm": float(chief_pupil[0]),
                "chief_pupil_y_mm": float(chief_pupil[1]),
                "physical_pupil_grid_pitch_mm": pupil_pitch,
                "fft_sample_pitch_sensor_px": pixel_scale,
                "reference_sphere_opd_rms_um": float(np.sqrt(np.mean(opd * opd)) * 1000.0),
                "phase_psf_crop_energy_fraction": total,
                "phase_psf_r50_px": encircled_radius(phase_psf, 0.5),
                "phase_psf_r90_px": encircled_radius(phase_psf, 0.9),
                "flat_phase_r50_px": encircled_radius(flat_psf, 0.5),
                "phase_to_flat_normalized_l1_percent": l1,
                "phase_psf_l1_to_previous_grid_percent": grid_l1,
            }
            records.append(record)
            print(f"{wavelength} nm grid={grid} exit pupil z={pupil_z:.4f} mm "
                  f"fit RMS={plane_rms:.5f} mm, OPD RMS="
                  f"{record['reference_sphere_opd_rms_um']:.4f} um, "
                  f"Rref={reference_radius:.3f} mm, phase/flat L1={l1:.2f}%, "
                  f"grid L1={grid_l1:.2f}%")
            if grid == grids[-1]:
                cases.append({"grid": grid, "wavelength_nm": wavelength,
                              "phase": normalized_phase, "flat": normalized_flat,
                              "difference": np.abs(normalized_phase - normalized_flat)})

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    make_chart(cases, args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
