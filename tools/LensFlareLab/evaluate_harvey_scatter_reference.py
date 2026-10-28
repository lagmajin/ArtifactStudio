"""Render a pixel-integrated Harvey-Shack surface-scatter reference PSF.

This evaluates the rotationally symmetric angular BSDF reported for generic
commercial Double-Gauss lenses. It is a scatter-only reference component; it
does not include the lens-specific beam-cone factors in Peterson's full model.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


DEFAULT_L_MRAD = (0.5, 1.0, 2.04, 4.0)
COLORS = ((91, 184, 255), (109, 229, 176), (255, 196, 90), (255, 119, 139))


def bsdf(theta_rad, slope, amplitude, corner_rad):
    sine = np.sin(theta_rad)
    return amplitude * (1.0 + (sine / corner_rad) ** 2) ** (slope * 0.5)


def calculate_tis(slope, b, corner_rad, amplitude, order=256):
    """Integrate BSDF over the forward hemisphere and return article Eq. 6 too."""
    nodes, weights = np.polynomial.legendre.leggauss(order)
    cos_theta = (nodes + 1.0) * 0.5
    weights = weights * 0.5
    theta = np.arccos(cos_theta)
    integrated = 2.0 * math.pi * float(np.sum(
        bsdf(theta, slope, amplitude, corner_rad) * weights))
    exponent = 0.5 * (slope + 2.0)
    article = (2.0 * math.pi * b * 100.0**slope / (slope + 2.0) *
               ((1.0 + corner_rad**2) ** exponent - corner_rad ** (slope + 2.0)))
    return integrated, article


def pixel_integrated_scatter(width, height, pixel_pitch_mm, focal_length_mm,
                             slope, b, corner_rad, subpixels):
    amplitude = b * (100.0 * corner_rad) ** slope
    tis, article_tis = calculate_tis(slope, b, corner_rad, amplitude)
    total = np.zeros((height, width), dtype=np.float64)
    offsets = (np.arange(subpixels, dtype=np.float64) + 0.5) / subpixels - 0.5
    x_centers = (np.arange(width) + 0.5 - width / 2.0) * pixel_pitch_mm
    y_centers = (np.arange(height) + 0.5 - height / 2.0) * pixel_pitch_mm
    for offset_y in offsets:
        y = y_centers[:, None] + offset_y * pixel_pitch_mm
        for offset_x in offsets:
            x = x_centers[None, :] + offset_x * pixel_pitch_mm
            radius = np.hypot(x, y)
            cos_theta = focal_length_mm / np.hypot(focal_length_mm, radius)
            sin_theta = radius / np.hypot(focal_length_mm, radius)
            angular = amplitude * (1.0 + (sin_theta / corner_rad) ** 2) ** (slope * 0.5)
            # x=f tan(theta), so dOmega/dA = cos(theta)^3/f^2.
            # Use the paper's Eq. 6 TIS; scale the exact hemisphere BSDF
            # integral to that small-angle truncation convention.
            irradiance_density = (angular * cos_theta**3 / (focal_length_mm**2 * tis) *
                                  article_tis)
            total += irradiance_density * (pixel_pitch_mm / subpixels) ** 2
    return total, amplitude, tis, article_tis


def make_image_plate(cases, output_path, width, height):
    panel_w, panel_h, top = 420, 420, 34
    columns, rows = 2, 2
    image = Image.new("RGB", (columns * panel_w, rows * (panel_h + top)), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    max_value = max(float(case["image"].max()) for case in cases)
    minimum = max_value * 1e-7
    for index, case in enumerate(cases):
        column, row = index % columns, index // columns
        x0, y0 = column * panel_w, row * (panel_h + top)
        label = (f"L={case['l_mrad']:.2f} mrad  TIS={case['tis'] * 100:.3f}% "
                 f"(Eq.6 {case['article_tis'] * 100:.3f}%)")
        draw.text((x0 + 6, y0 + 5), label, fill=(235, 238, 244))
        irradiance = case["image"]
        log_image = ((np.log10(np.maximum(irradiance, minimum)) - math.log10(minimum)) /
                     (math.log10(max_value) - math.log10(minimum)))
        gray = np.clip(log_image * 255.0 + 0.5, 0, 255).astype(np.uint8)
        tile = Image.fromarray(gray).resize((panel_w, panel_h), Image.Resampling.BOX)
        image.paste(Image.merge("RGB", (tile, tile, tile)), (x0, y0 + top))
        cx, cy = x0 + panel_w // 2, y0 + top + panel_h // 2
        draw.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill=(255, 195, 60))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)


def make_profile_chart(cases, output_path, width, height, pixel_pitch_mm):
    canvas_w, canvas_h = 900, 580
    left, right, top, bottom = 82, 30, 52, 62
    plot_w, plot_h = canvas_w - left - right, canvas_h - top - bottom
    image = Image.new("RGB", (canvas_w, canvas_h), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    draw.text((left, 14), "Harvey-Shack rough-surface scatter: radial pixel irradiance",
              fill=(235, 238, 244))
    x_min, x_max = 1.0, min(width, height) * 0.5
    y_values = np.concatenate([case["profile_norm"] for case in cases])
    positive = y_values[y_values > 0]
    y_min = max(float(positive.min()), 1e-8)
    y_max = 1.0
    for decade in range(math.floor(math.log10(y_min)), 1):
        y = top + int((math.log10(y_max) - decade) /
                      (math.log10(y_max) - math.log10(y_min)) * plot_h)
        if top <= y <= top + plot_h:
            draw.line((left, y, left + plot_w, y), fill=(46, 51, 60))
            draw.text((8, y - 7), f"1e{decade}", fill=(159, 166, 178))
    for tick in (1, 2, 5, 10, 20, 50, 100, 200, 500):
        if tick < x_min or tick > x_max:
            continue
        px = left + int(math.log(tick / x_min) / math.log(x_max / x_min) * plot_w)
        draw.line((px, top, px, top + plot_h), fill=(37, 42, 50))
        draw.text((px - 8, top + plot_h + 9), str(tick), fill=(159, 166, 178))
    draw.rectangle((left, top, left + plot_w, top + plot_h), outline=(100, 108, 120))
    for index, case in enumerate(cases):
        profile = case["profile_norm"]
        radius = np.arange(len(profile), dtype=np.float64)
        valid = (radius >= 1.0) & (radius <= x_max) & (profile > 0.0)
        x = left + np.log(radius[valid] / x_min) / math.log(x_max / x_min) * plot_w
        y = (top + (math.log10(y_max) - np.log10(profile[valid])) /
             (math.log10(y_max) - math.log10(y_min)) * plot_h)
        points = list(zip(x.astype(int), y.astype(int)))
        if len(points) > 1:
            draw.line(points, fill=COLORS[index], width=2)
        draw.text((left + 10 + index * 190, canvas_h - 30),
                  f"L={case['l_mrad']:.2f} mrad", fill=COLORS[index])
    draw.text((left + plot_w // 2 - 48, canvas_h - 14), "radius (pixels)",
              fill=(205, 212, 222))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=1000)
    parser.add_argument("--height", type=int, default=1000)
    parser.add_argument("--pixel-pitch-um", type=float, default=5.5)
    parser.add_argument("--focal-length-mm", type=float, default=35.0)
    parser.add_argument("--slope", type=float, default=-1.86)
    parser.add_argument("--b", type=float, default=0.36,
                        help="Harvey scale coefficient in sr^-1")
    parser.add_argument("--l-mrad", type=float, nargs="+", default=list(DEFAULT_L_MRAD))
    parser.add_argument("--subpixels", type=int, default=4)
    parser.add_argument("--reference-subpixels", type=int, default=8)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-scatter-reference.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-scatter-reference.png"))
    parser.add_argument("--profile-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-scatter-profile.png"))
    args = parser.parse_args()
    if (min(args.width, args.height, args.subpixels, args.reference_subpixels) < 1 or
            args.reference_subpixels <= args.subpixels or args.pixel_pitch_um <= 0.0):
        parser.error("dimensions/pitch must be positive and reference-subpixels must exceed subpixels")
    if args.focal_length_mm <= 0.0 or args.b <= 0.0 or abs(args.slope + 2.0) < 1e-12:
        parser.error("focal length and b must be positive; slope must differ from -2 for Eq. 6")
    if not args.l_mrad or any(value <= 0.0 for value in args.l_mrad):
        parser.error("all l-mrad values must be positive")

    pixel_pitch_mm = args.pixel_pitch_um * 1e-3
    radius = np.hypot(
        (np.arange(args.width) + 0.5 - args.width / 2.0)[None, :],
        (np.arange(args.height) + 0.5 - args.height / 2.0)[:, None])
    radial_bins = np.floor(radius + 0.5).astype(np.int32)
    cases = []
    for l_mrad in args.l_mrad:
        corner_rad = l_mrad * 1e-3
        image, amplitude, tis, article_tis = pixel_integrated_scatter(
            args.width, args.height, pixel_pitch_mm, args.focal_length_mm,
            args.slope, args.b, corner_rad, args.subpixels)
        reference_image, _, _, _ = pixel_integrated_scatter(
            args.width, args.height, pixel_pitch_mm, args.focal_length_mm,
            args.slope, args.b, corner_rad, args.reference_subpixels)
        image_shape = image / max(float(image.sum()), 1e-300)
        reference_shape = reference_image / max(float(reference_image.sum()), 1e-300)
        quadrature_l1 = float(np.abs(image_shape - reference_shape).sum())
        bin_energy = np.bincount(radial_bins.ravel(), weights=image.ravel())
        bin_count = np.bincount(radial_bins.ravel())
        profile = bin_energy / np.maximum(bin_count, 1)
        nonempty = np.flatnonzero(bin_count > 0)
        peak = max(float(profile[nonempty[0]]) if len(nonempty) else 0.0, 1e-300)
        cases.append({"l_mrad": l_mrad, "image": image, "amplitude": amplitude,
                      "tis": tis, "article_tis": article_tis,
                      "profile": profile, "profile_norm": profile / peak,
                      "frame_energy": float(image.sum()),
                      "quadrature_l1": quadrature_l1,
                      "reference_frame_energy": float(reference_image.sum())})

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("l_mrad", "radius_px", "angle_mrad", "mean_pixel_irradiance",
                         "irradiance_over_center", "surface_TIS_BSDF_integral",
                         "surface_TIS_article_eq6", "captured_sensor_fraction",
                         "pixel_quadrature_l1_vs_reference"))
        pixel_pitch_mm = args.pixel_pitch_um * 1e-3
        for case in cases:
            for radius_px, value in enumerate(case["profile"]):
                angle_mrad = math.atan(radius_px * pixel_pitch_mm /
                                       args.focal_length_mm) * 1000.0
                writer.writerow((f"{case['l_mrad']:.6g}", radius_px,
                                 f"{angle_mrad:.9g}", f"{value:.12g}",
                                 f"{case['profile_norm'][radius_px]:.12g}",
                                 f"{case['tis']:.12g}", f"{case['article_tis']:.12g}",
                                 f"{case['frame_energy']:.12g}",
                                 f"{case['quadrature_l1']:.12g}"))
    make_image_plate(cases, args.image, args.width, args.height)
    make_profile_chart(cases, args.profile_image, args.width, args.height, pixel_pitch_mm)
    for case in cases:
        print(f"l_mrad={case['l_mrad']:.3f} b0_sr-1={case['amplitude']:.6f} "
              f"TIS_hemisphere={case['tis']:.8f} TIS_Eq6={case['article_tis']:.8f} "
              f"captured_frame_fraction={case['frame_energy']:.8f} "
              f"pixel_quadrature_L1={case['quadrature_l1']:.6%}")
    print(f"bsdf_slope={args.slope:.6f} b_sr-1={args.b:.6f} "
          f"f_mm={args.focal_length_mm:.6f} pixel_um={args.pixel_pitch_um:.6f}")
    print(f"csv={args.csv}")
    print(f"image={args.image}")
    print(f"profile_image={args.profile_image}")


if __name__ == "__main__":
    main()
