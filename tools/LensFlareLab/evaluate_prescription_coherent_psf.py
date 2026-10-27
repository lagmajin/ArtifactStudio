"""Evaluate a 550 nm coherent PSF from the prescription-derived exit pupil.

Ray-mapped OPD and geometric amplitude are interpolated only across locally
connected launch-grid triangles, then propagated with a scalar Fraunhofer FFT.
This is a monochromatic image-quality study, not a calibrated camera model.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.spatial import Delaunay

import evaluate_direct_transmission_raytrace as prescription
import evaluate_prescription_wavefront as wavefront
import evaluate_wavefront_aberration_psf as psf_reference


WAVELENGTH_NM = 550.0
FOCAL_LENGTH_MM = prescription.EFL
SENSOR_WIDTH_MM = 53.96
SENSOR_WIDTH_PX = 640
FIELD_CASES = wavefront.FIELD_CASES
OUTPUT_RADIUS_PX = 12
LOG_DISPLAY_GAIN = 1.0e6


def map_to_regular_pupil(xy, opd_mm, directions, index_map, jacobian,
                         pupil_center, pupil_radius, pupil_grid_size):
    """Interpolate ray data without triangulating across missing ray-grid cells."""
    launch_locations = np.argwhere(index_map >= 0)
    xy_relative = xy - pupil_center
    triangulation = Delaunay(xy_relative)
    simplex_vertices = triangulation.simplices
    grid_vertices = launch_locations[simplex_vertices]
    local = ((np.ptp(grid_vertices[:, :, 0], axis=1) <= 1) &
             (np.ptp(grid_vertices[:, :, 1], axis=1) <= 1))
    jacobian_valid = np.isfinite(jacobian) & (jacobian > 0.0)
    local &= np.all(jacobian_valid[simplex_vertices], axis=1)
    if not np.any(local):
        raise RuntimeError("ray map contains no locally connected pupil triangles")

    amplitude = np.sqrt(1.0 / np.maximum(jacobian * directions[:, 2], 1e-30))
    half_span = 1.30 * pupil_radius
    pitch = 2.0 * half_span / pupil_grid_size
    axis = (np.arange(pupil_grid_size, dtype=np.float64) + 0.5) * pitch - half_span
    pupil_opd = np.zeros((pupil_grid_size, pupil_grid_size), dtype=np.float64)
    pupil_amplitude = np.zeros_like(pupil_opd)
    valid_pupil = np.zeros_like(pupil_opd, dtype=bool)

    # Row chunks keep the query, barycentric weights, and temporary arrays bounded.
    for row, y in enumerate(axis):
        query = np.column_stack((axis, np.full(pupil_grid_size, y)))
        simplex = triangulation.find_simplex(query)
        inside = simplex >= 0
        safe_simplex = np.maximum(simplex, 0)
        inside &= local[safe_simplex]
        if not np.any(inside):
            continue
        selected_simplex = safe_simplex[inside]
        transform = triangulation.transform[selected_simplex]
        delta = query[inside] - transform[:, 2]
        barycentric_01 = np.einsum("nij,nj->ni", transform[:, :2], delta)
        barycentric = np.column_stack((barycentric_01,
                                       1.0 - barycentric_01.sum(axis=1)))
        vertices = simplex_vertices[selected_simplex]
        target_x = np.flatnonzero(inside)
        pupil_opd[row, target_x] = np.sum(opd_mm[vertices] * barycentric, axis=1)
        pupil_amplitude[row, target_x] = np.sum(amplitude[vertices] * barycentric, axis=1)
        valid_pupil[row, target_x] = True

    if not np.any(valid_pupil):
        raise RuntimeError("ray map did not cover the regular exit-pupil grid")
    return axis, pitch, pupil_opd, pupil_amplitude, valid_pupil


def vectorized_hexagon(axis, radius):
    x, y = np.meshgrid(axis, axis)
    angle = np.arctan2(y, x)
    local = np.remainder(angle + math.pi / 6.0, math.pi / 3.0) - math.pi / 6.0
    return np.hypot(x, y) * np.cos(local) <= radius * math.cos(math.pi / 6.0)


def integrate_pixel_areas(intensity, wavelength_nm, pitch_mm, output_radius):
    size = intensity.shape[0]
    pixel_pitch_mm = SENSOR_WIDTH_MM / SENSOR_WIDTH_PX
    pixel_scale = wavelength_nm * 1e-6 * FOCAL_LENGTH_MM / (
        size * pitch_mm * pixel_pitch_mm)
    coordinates = (np.arange(size, dtype=np.float64) - size // 2) * pixel_scale
    base = np.floor(coordinates + 0.5).astype(np.int32)
    left = np.maximum(0.0, (base - 0.5) - (coordinates - 0.5 * pixel_scale)) / pixel_scale
    right = np.maximum(0.0, (coordinates + 0.5 * pixel_scale) - (base + 0.5)) / pixel_scale
    edge_index = np.where(left > 0.0, base - 1, base + 1)
    center_weight = 1.0 - left - right
    edge_weight = left + right
    output = np.zeros((2 * output_radius + 1, 2 * output_radius + 1), dtype=np.float64)
    for row_indices, row_weights in ((base, center_weight), (edge_index, edge_weight)):
        valid_rows = (row_indices >= -output_radius) & (row_indices <= output_radius)
        out_rows = row_indices[valid_rows] + output_radius
        for col_indices, col_weights in ((base, center_weight), (edge_index, edge_weight)):
            valid_cols = (col_indices >= -output_radius) & (col_indices <= output_radius)
            out_cols = col_indices[valid_cols] + output_radius
            contribution = (intensity[np.ix_(valid_rows, valid_cols)] *
                            row_weights[valid_rows, None] * col_weights[None, valid_cols])
            np.add.at(output, (out_rows[:, None], out_cols[None, :]), contribution)
    return output, pixel_scale


def propagate(pupil, pitch_mm, wavelength_nm, crop_radius):
    field = np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(pupil)))
    intensity = np.abs(field) ** 2
    intensity /= max(float(intensity.sum()), 1e-30)
    pixel_image, pixel_scale = integrate_pixel_areas(
        intensity, wavelength_nm, pitch_mm, crop_radius)
    return pixel_image, pixel_scale


def image_metrics(image, reference):
    center = image.shape[0] // 2
    y, x = np.mgrid[:image.shape[0], :image.shape[1]]
    energy = float(image.sum())
    reference_energy = float(reference.sum())
    cx = float((image * x).sum() / max(energy, 1e-30))
    cy = float((image * y).sum() / max(energy, 1e-30))
    normalized = image / max(energy, 1e-30)
    normalized_reference = reference / max(reference_energy, 1e-30)
    return {
        "crop_energy_fraction": energy,
        "crop_energy_difference_vs_ideal_percent":
            100.0 * abs(energy - reference_energy) / max(reference_energy, 1e-30),
        "centroid_offset_px": math.hypot(cx - center, cy - center),
        "pixel_peak_ratio_vs_ideal": float(image.max() / max(reference.max(), 1e-30)),
        "shape_l1_vs_ideal_percent":
            100.0 * float(np.abs(normalized - normalized_reference).sum()),
        "r50_px": psf_reference.encircled_radius(image, 0.50),
        "r90_px": psf_reference.encircled_radius(image, 0.90),
        "r99_px": psf_reference.encircled_radius(image, 0.99),
    }


def make_chart(cases, output_path):
    panel = 280
    rows = (len(cases) + 1) // 2
    image = Image.new("RGB", (2 * panel, rows * (panel + 34)), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    for index, case in enumerate(cases):
        x0 = (index % 2) * panel
        y0 = (index // 2) * (panel + 34)
        pixels = np.maximum(case["psf"], 0.0)
        display = np.log1p(pixels * LOG_DISPLAY_GAIN)
        display /= max(float(display.max()), 1e-30)
        gray = np.clip(display * 255.0 + 0.5, 0, 255).astype(np.uint8)
        padded = np.zeros((2 * OUTPUT_RADIUS_PX + 1,
                           2 * OUTPUT_RADIUS_PX + 1), dtype=np.uint8)
        offset = OUTPUT_RADIUS_PX - case["crop_radius"]
        padded[offset:offset + gray.shape[0], offset:offset + gray.shape[1]] = gray
        tile = Image.fromarray(padded).resize((panel, panel), Image.Resampling.NEAREST)
        image.paste(tile.convert("RGB"), (x0, y0 + 30))
        draw.text((x0 + 6, y0 + 5),
                  f"{case['field']}  RMS {case['opd_rms_um']:.3f} um; "
                  f"L1 {case['metrics']['shape_l1_vs_ideal_percent']:.1f}% "
                  f"(crop +/-{case['crop_radius']} px)",
                  fill=(235, 238, 244))
        draw.rectangle((x0, y0 + 30, x0 + panel - 1, y0 + 30 + panel - 1),
                       outline=(62, 68, 78))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ray-samples", type=int, default=256)
    parser.add_argument("--pupil-samples", type=int, default=1024)
    parser.add_argument("--field", action="append", choices=[name for name, _ in FIELD_CASES],
                        help="evaluate one or more fields; default evaluates all fields")
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-coherent-psf.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-coherent-psf.png"))
    parser.add_argument("--pixels-csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-coherent-psf-pixels.csv"))
    args = parser.parse_args()
    if args.ray_samples < 64 or args.pupil_samples < 256 or args.pupil_samples % 2:
        parser.error("ray-samples must be >=64; pupil-samples must be even and >=256")

    exit_z, pupil_center, _, magnification_x, _, _ = \
        wavefront.estimate_exit_pupil(WAVELENGTH_NM)
    pupil_radius = prescription.STOP_RADIUS * abs(magnification_x)
    pupil_hex_area = 1.5 * math.sqrt(3.0) * pupil_radius**2
    pupil_pitch = 2.0 * 1.30 * pupil_radius / args.pupil_samples
    support_radius_px = (WAVELENGTH_NM * 1e-6 * FOCAL_LENGTH_MM /
                         (2.0 * pupil_pitch * (SENSOR_WIDTH_MM / SENSOR_WIDTH_PX)))
    crop_radius = min(OUTPUT_RADIUS_PX, math.floor(support_radius_px - 0.5))
    if crop_radius < 1:
        parser.error("pupil-samples is too small to resolve a one-pixel PSF crop")
    records, cases, pixel_records = [], [], []
    selected_fields = [case for case in FIELD_CASES
                       if args.field is None or case[0] in args.field]
    for field_name, angle in selected_fields:
        (pupil_xy, _, directions, _, opd_mm, _, _, _, index_map, _, _, _) = \
            wavefront.trace_field(angle, args.ray_samples, exit_z, pupil_center,
                                  WAVELENGTH_NM)
        map_info = wavefront.pupil_map_jacobian(
            pupil_xy, directions, index_map, 70.0 / args.ray_samples)
        axis, pupil_pitch, opd_grid, amplitude_grid, valid = map_to_regular_pupil(
            pupil_xy, opd_mm, directions, index_map, map_info["nodal_jacobian"],
            pupil_center, pupil_radius, args.pupil_samples)

        prescription_pupil = np.where(valid, amplitude_grid, 0.0) * \
            np.exp(2j * math.pi * opd_grid * 1e6 / WAVELENGTH_NM)
        ideal_mask = vectorized_hexagon(axis, pupil_radius)
        ideal_pupil = ideal_mask.astype(np.complex128)
        psf_image, pixel_scale = propagate(
            prescription_pupil, pupil_pitch, WAVELENGTH_NM, crop_radius)
        ideal_image, _ = propagate(ideal_pupil, pupil_pitch, WAVELENGTH_NM, crop_radius)
        metrics = image_metrics(psf_image, ideal_image)
        for pixel_y in range(-crop_radius, crop_radius + 1):
            for pixel_x in range(-crop_radius, crop_radius + 1):
                pixel_records.append({
                    "field": field_name, "x_px": pixel_x, "y_px": pixel_y,
                    "prescription_psf_energy": float(
                        psf_image[pixel_y + crop_radius, pixel_x + crop_radius]),
                    "ideal_psf_energy": float(
                        ideal_image[pixel_y + crop_radius, pixel_x + crop_radius]),
                })
        opd_rms_um = float(np.sqrt(np.mean(opd_mm * opd_mm)) * 1000.0)
        case = {"field": field_name, "angle_deg": angle, "psf": psf_image,
                "opd_rms_um": opd_rms_um, "metrics": metrics,
                "crop_radius": crop_radius}
        cases.append(case)
        records.append({
            "field": field_name, "field_angle_deg": angle,
            "wavelength_nm": WAVELENGTH_NM, "ray_grid": args.ray_samples,
            "pupil_grid": args.pupil_samples, "valid_rays": len(pupil_xy),
            "exit_pupil_area_mm2_from_ray_map": map_info["mapped_pupil_area_mm2"],
            "paraxial_hex_pupil_area_mm2": pupil_hex_area,
            "pupil_area_fraction_vs_paraxial": map_info["mapped_pupil_area_mm2"] /
                                                pupil_hex_area,
            "foldover_triangle_percent": map_info["foldover_triangle_percent"],
            "pupil_jacobian_p01": map_info["jacobian_p01"],
            "pupil_jacobian_median": map_info["jacobian_median"],
            "pupil_jacobian_p99": map_info["jacobian_p99"],
            "geometric_amplitude_p01": map_info["amplitude_p01"],
            "geometric_amplitude_median": map_info["amplitude_median"],
            "geometric_amplitude_p99": map_info["amplitude_p99"],
            "opd_rms_um": opd_rms_um,
            "opd_rms_waves": opd_rms_um * 1000.0 / WAVELENGTH_NM,
            "regular_pupil_pitch_mm": pupil_pitch,
            "fft_sample_pitch_in_sensor_pixels": pixel_scale,
            "fft_angular_support_radius_px": support_radius_px,
            "interpolated_open_pupil_samples": int(np.count_nonzero(valid)),
            **metrics,
        })
        print(f"{field_name:14s} rays={len(pupil_xy):5d} pupil={map_info['mapped_pupil_area_mm2']:.1f} mm^2 "
              f"OPD RMS={opd_rms_um:.4f} um, crop energy={metrics['crop_energy_fraction']:.6f}, "
              f"L1 vs ideal={metrics['shape_l1_vs_ideal_percent']:.2f}%, "
              f"FFT support=+/-{support_radius_px:.1f} px")

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    args.pixels_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.pixels_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(pixel_records[0]))
        writer.writeheader()
        writer.writerows(pixel_records)
    make_chart(cases, args.image)
    print(f"saved {args.csv}, {args.pixels_csv}, and {args.image}")


if __name__ == "__main__":
    main()
