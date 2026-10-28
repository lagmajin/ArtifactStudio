"""Evaluate the full-frame Fraunhofer PSF of an ideal regular hexagonal pupil.

Unlike an FFT of a sampled pupil, the polygon boundary integral evaluates the
continuous aperture transform at arbitrary image-plane frequencies. This is a
reference for ideal-pupil diffraction wings, not an aberrated lens PSF.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_direct_transmission_raytrace as prescription
import evaluate_prescription_wavefront as wavefront


def polygon_transform(qx, qy, radius):
    """Continuous Fourier transform of a regular hexagon, q in cycles/mm."""
    angles = np.arange(6, dtype=np.float64) * (math.pi / 3.0) + math.pi / 6.0
    vertices = np.stack((radius * np.cos(angles), radius * np.sin(angles)), axis=1)
    area = 1.5 * math.sqrt(3.0) * radius * radius
    q2 = qx * qx + qy * qy
    result = np.zeros(np.broadcast_shapes(qx.shape, qy.shape), dtype=np.complex128)
    safe = q2 > 1e-20
    if not np.any(safe):
        result[...] = area
        return result
    qxs, qys, q2s = qx[safe], qy[safe], q2[safe]
    total = np.zeros(qxs.shape, dtype=np.complex128)
    for index in range(6):
        start = vertices[index]
        end = vertices[(index + 1) % 6]
        edge = end - start
        length = float(np.linalg.norm(edge))
        normal = np.array((edge[1], -edge[0])) / length
        projection = qxs * edge[0] + qys * edge[1]
        phase = np.exp(-2j * math.pi * (qxs * start[0] + qys * start[1]))
        edge_integral = phase * np.exp(-1j * math.pi * projection) * np.sinc(projection)
        total += (qxs * normal[0] + qys * normal[1]) * length * edge_integral
    result[safe] = total / (-2j * math.pi * q2s)
    result[~safe] = area
    return result


def direct_triangle_quadrature(qx, qy, radius, order):
    """Independent area integral over six center-to-edge triangles."""
    nodes, weights = np.polynomial.legendre.leggauss(order)
    nodes = (nodes + 1.0) * 0.5
    weights = weights * 0.5
    radial, edge_t = np.meshgrid(nodes, nodes, indexing="ij")
    quadrature_weights = weights[:, None] * weights[None, :] * radial
    angles = np.arange(6, dtype=np.float64) * (math.pi / 3.0) + math.pi / 6.0
    vertices = np.stack((radius * np.cos(angles), radius * np.sin(angles)), axis=1)
    x_parts, y_parts, w_parts = [], [], []
    for index in range(6):
        start, end = vertices[index], vertices[(index + 1) % 6]
        determinant = abs(start[0] * end[1] - start[1] * end[0])
        x = radial * ((1.0 - edge_t) * start[0] + edge_t * end[0])
        y = radial * ((1.0 - edge_t) * start[1] + edge_t * end[1])
        x_parts.append(x.ravel())
        y_parts.append(y.ravel())
        w_parts.append((quadrature_weights * determinant).ravel())
    x = np.concatenate(x_parts)
    y = np.concatenate(y_parts)
    area_weights = np.concatenate(w_parts)
    phase = np.exp(-2j * math.pi * (qx[:, None] * x[None, :] +
                                      qy[:, None] * y[None, :]))
    return phase @ area_weights


def validate_transform(radius, order, output_path):
    normalized_radius = (0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 12.0, 16.0)
    angles_deg = (0.0, 11.0, 30.0, 47.0, 73.0)
    rho = np.repeat(normalized_radius, len(angles_deg))
    theta = np.tile(np.deg2rad(angles_deg), len(normalized_radius))
    qx, qy = rho * np.cos(theta) / radius, rho * np.sin(theta) / radius
    analytic = polygon_transform(qx, qy, radius)
    numerical = direct_triangle_quadrature(qx, qy, radius, order)
    area = 1.5 * math.sqrt(3.0) * radius * radius
    abs_error = np.abs(analytic - numerical) / area
    rel_error = abs_error / np.maximum(np.abs(numerical) / area, 1e-12)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("frequency_radius_over_pupil_radius", "angle_deg",
                         "analytic_real_over_area", "quadrature_real_over_area",
                         "analytic_imag_over_area", "quadrature_imag_over_area",
                         "absolute_complex_error_over_area", "relative_complex_error"))
        for index in range(len(rho)):
            writer.writerow((f"{rho[index]:.6g}", f"{math.degrees(theta[index]):.6g}",
                             f"{analytic[index].real / area:.12g}",
                             f"{numerical[index].real / area:.12g}",
                             f"{analytic[index].imag / area:.12g}",
                             f"{numerical[index].imag / area:.12g}",
                             f"{abs_error[index]:.12g}", f"{rel_error[index]:.12g}"))
    # Rotational symmetry exercises the analytic expression out to the far
    # corners of the rendered sensor, where direct area quadrature would need
    # thousands of nodes per edge to resolve the oscillations.
    high_rho = np.array((32.0, 256.0, 2048.0, 8192.0, 12000.0))
    high_theta = np.linspace(0.0, math.pi / 3.0, 257, endpoint=False)
    symmetry_error = 0.0
    for value in high_rho:
        base = polygon_transform(value * np.cos(high_theta) / radius,
                                 value * np.sin(high_theta) / radius, radius)
        rotated = polygon_transform(value * np.cos(high_theta + math.pi / 3.0) / radius,
                                    value * np.sin(high_theta + math.pi / 3.0) / radius,
                                    radius)
        symmetry_error = max(symmetry_error,
                             float(np.max(np.abs(base - rotated)) / area))
    return (float(abs_error.max()), float(np.quantile(abs_error, 0.95)),
            symmetry_error)


def evaluate_pixel_centers(width, height, pixel_pitch_mm, wavelength_nm, efl_mm,
                           pupil_radius_mm):
    x_centers = (np.arange(width, dtype=np.float64) - (width - 1) / 2) * pixel_pitch_mm
    y_centers = (np.arange(height, dtype=np.float64) - (height - 1) / 2) * pixel_pitch_mm
    wavelength_mm = wavelength_nm * 1e-6
    area = 1.5 * math.sqrt(3.0) * pupil_radius_mm**2
    x = x_centers[None, :] / (wavelength_mm * efl_mm)
    y = y_centers[:, None] / (wavelength_mm * efl_mm)
    amplitude = polygon_transform(np.broadcast_to(x, (height, width)),
                                  np.broadcast_to(y, (height, width)),
                                  pupil_radius_mm)
    return (amplitude.real**2 + amplitude.imag**2) / (
        (wavelength_mm * efl_mm)**2 * area)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--sensor-width-mm", type=float, default=53.96)
    parser.add_argument("--wavelength-nm", type=float, default=550.0)
    parser.add_argument("--validation-order", type=int, default=96)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/hex-pupil-full-frame.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/hex-pupil-full-frame.png"))
    parser.add_argument("--validation-csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/hex-pupil-transform-validation.csv"))
    args = parser.parse_args()
    if min(args.width, args.height, args.validation_order) < 1:
        parser.error("width, height, and validation-order must be positive")
    pixel_pitch = args.sensor_width_mm / args.width
    _, _, _, magnification_x, _, _ = wavefront.estimate_exit_pupil(
        args.wavelength_nm)
    pupil_radius = prescription.STOP_RADIUS * abs(magnification_x)
    irradiance = evaluate_pixel_centers(args.width, args.height, pixel_pitch,
                                        args.wavelength_nm, prescription.EFL,
                                        pupil_radius)

    # The ordinary pupil FFT's unaliased extent is intentionally reported as
    # a sampling comparison; this analytic transform remains valid beyond it.
    fft_grid = 2048
    pupil_pitch = 2.0 * 1.30 * pupil_radius / fft_grid
    support_px = (args.wavelength_nm * 1e-6 * prescription.EFL /
                  (2.0 * pupil_pitch * pixel_pitch))
    max_error, p95_error, symmetry_error = validate_transform(
        pupil_radius, args.validation_order, args.validation_csv)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("x_px", "y_px", "normalized_irradiance_per_mm2"))
        for y in range(args.height):
            for x in range(args.width):
                writer.writerow((x - args.width // 2, y - args.height // 2,
                                 f"{irradiance[y, x]:.12g}"))

    display = np.log10(np.maximum(irradiance, 1e-12))
    low, high = -8.0, float(display.max())
    gray = np.clip((display - low) / max(high - low, 1e-12) * 255.0, 0, 255).astype(np.uint8)
    rgb = np.stack((gray, gray, gray), axis=2)
    rgb = np.asarray(Image.fromarray(rgb).resize((args.width, args.height), Image.Resampling.NEAREST))
    output = Image.new("RGB", (args.width, args.height + 24), (18, 21, 26))
    output.paste(Image.fromarray(rgb), (0, 24))
    draw = ImageDraw.Draw(output)
    draw.text((5, 5), f"Ideal hex pupil | {args.wavelength_nm:g} nm | log10 normalized irradiance",
              fill=(235, 238, 244))
    draw.rectangle((args.width // 2 - 1, args.height // 2 + 23,
                    args.width // 2 + 1, args.height // 2 + 25), fill=(255, 190, 45))
    output.save(args.image)
    print(f"pixel_pitch_um={pixel_pitch * 1000:.4f}")
    print(f"pupil_radius_mm={pupil_radius:.6f}")
    print(f"fft_support_radius_px={support_px:.3f}")
    print(f"transform_validation_order={args.validation_order}")
    print(f"transform_validation_max_abs_error_over_area={max_error:.3e}")
    print(f"transform_validation_p95_abs_error_over_area={p95_error:.3e}")
    print(f"high_frequency_hex_symmetry_max_error_over_area={symmetry_error:.3e}")
    print("sampling=pointwise irradiance; values are not pixel-area integrated")
    print(f"validation_csv={args.validation_csv}")
    print(f"csv={args.csv}")
    print(f"image={args.image}")


if __name__ == "__main__":
    main()
