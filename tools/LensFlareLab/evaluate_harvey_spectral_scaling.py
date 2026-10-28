"""Render a Harvey-Shack rough-surface spectrum using wavelength scaling.

This applies the limited-band smooth-surface scaling law to one reference
parameter set. It is a model study, not measured lens or sensor data.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_harvey_scatter_reference import calculate_tis, pixel_integrated_scatter


RGB_BANDS = (("red", 610.0), ("green", 550.0), ("blue", 460.0))


def write_pfm(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    height, width, _ = image.shape
    with path.open("wb") as stream:
        stream.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        stream.write(np.flipud(image).astype("<f4").tobytes())


def make_plate(path, cases):
    panel = 340
    header = 54
    plot_height = 340
    canvas = Image.new("RGB", (panel * 3, panel + header + plot_height),
                       (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    for index, case in enumerate(cases):
        x = index * panel
        draw.text((x + 7, 6), f"{case['name']}  {case['wavelength_nm']:.0f} nm",
                  fill=(235, 238, 244))
        draw.text((x + 7, 24),
                  f"B={case['b']:.3g}  L={case['l_mrad']:.3f} mrad  "
                  f"crop={case['captured'] * 100:.3g}%", fill=(134, 207, 255))
        data = case["image"]
        normalized = data / max(float(data.max()), np.finfo(float).tiny)
        log = np.log10(np.maximum(normalized, 1e-8))
        gray = np.clip((log + 8.0) * (255.0 / 8.0), 0, 255).astype(np.uint8)
        tile = Image.fromarray(gray).resize((panel, panel), Image.Resampling.BOX)
        canvas.paste(Image.merge("RGB", (tile, tile, tile)), (x, header))
    plot_top = header + panel + 24
    plot_left, plot_right = 82, panel * 3 - 24
    plot_bottom = canvas.height - 44
    draw.text((plot_left, header + panel + 3),
              "Radial pixel irradiance / center (log scale)",
              fill=(235, 238, 244))
    draw.rectangle((plot_left, plot_top, plot_right, plot_bottom),
                   outline=(80, 88, 100))
    colors = ((255, 110, 104), (111, 223, 160), (110, 174, 255))
    y_min, y_max = -8.0, 0.0
    radius_max = max(len(case["profile"]) - 1 for case in cases)
    for decade in range(-8, 1, 2):
        y = plot_top + int((y_max - decade) / (y_max - y_min) *
                           (plot_bottom - plot_top))
        draw.line((plot_left, y, plot_right, y), fill=(43, 48, 57))
        draw.text((22, y - 6), f"1e{decade}", fill=(150, 158, 170))
    for index, case in enumerate(cases):
        profile = case["profile"]
        positive_profile = profile[profile > 0]
        peak = max(float(positive_profile[0]) if len(positive_profile) else 0.0,
                   np.finfo(float).tiny)
        points = []
        for radius_px, value in enumerate(profile):
            if value <= 0:
                continue
            x = plot_left + int(radius_px / radius_max *
                                (plot_right - plot_left))
            log_value = float(np.clip(math.log10(float(value) / peak),
                                      y_min, y_max))
            y = plot_top + int((y_max - log_value) / (y_max - y_min) *
                               (plot_bottom - plot_top))
            if plot_top <= y <= plot_bottom:
                points.append((x, y))
        if len(points) > 1:
            draw.line(points, fill=colors[index], width=2)
    draw.text((plot_left, plot_bottom + 9), "0", fill=(170, 177, 188))
    draw.text((plot_right - 30, plot_bottom + 9), f"{radius_max}px",
              fill=(170, 177, 188))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=512)
    parser.add_argument("--height", type=int, default=512)
    parser.add_argument("--pixel-pitch-um", type=float, default=5.5)
    parser.add_argument("--focal-length-mm", type=float, default=100.0)
    parser.add_argument("--reference-wavelength-nm", type=float, default=550.0)
    parser.add_argument("--slope", type=float, default=-1.86)
    parser.add_argument("--b-reference", type=float, default=.36)
    parser.add_argument("--l-reference-mrad", type=float, default=2.04)
    parser.add_argument("--subpixels", type=int, default=8)
    parser.add_argument("--pfm", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-spectral-scaling-synthetic.pfm"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-spectral-scaling.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-spectral-scaling.png"))
    args = parser.parse_args()
    if min(args.width, args.height, args.subpixels) < 1 or min(
            args.pixel_pitch_um, args.focal_length_mm,
            args.reference_wavelength_nm, args.b_reference,
            args.l_reference_mrad) <= 0:
        parser.error("dimensions, sampling, and optical parameters must be positive")
    if abs(args.slope + 2.0) < 1e-12:
        parser.error("slope must differ from -2 for the integrated-scatter equation")

    pitch_mm = args.pixel_pitch_um * 1e-3
    l0_rad = args.l_reference_mrad * 1e-3
    cases = []
    for name, wavelength_nm in RGB_BANDS:
        ratio = wavelength_nm / args.reference_wavelength_nm
        b = args.b_reference * ratio ** (-(4.0 + args.slope))
        l_rad = l0_rad * ratio
        image, amplitude, _, article_tis = pixel_integrated_scatter(
            args.width, args.height, pitch_mm, args.focal_length_mm,
            args.slope, b, l_rad, args.subpixels)
        tis_integral, _ = calculate_tis(args.slope, b, l_rad, amplitude)
        # pixel_integrated_scatter already multiplies each sample by pixel
        # area; summing the returned pixels yields captured energy fraction.
        captured = float(image.sum())
        radii = np.hypot(
            (np.arange(args.width) + .5 - args.width / 2.0)[None, :],
            (np.arange(args.height) + .5 - args.height / 2.0)[:, None])
        radial_bins = np.floor(radii + .5).astype(np.int32)
        energy = np.bincount(radial_bins.ravel(), weights=image.ravel())
        count = np.bincount(radial_bins.ravel())
        profile = energy / np.maximum(count, 1)
        cases.append({"name": name, "wavelength_nm": wavelength_nm, "b": b,
            "l_mrad": l_rad * 1000.0, "image": image, "amplitude": amplitude,
            "tis_integral": tis_integral, "article_tis": article_tis,
            "captured": captured, "profile": profile})

    rgb = np.stack([case["image"] for case in cases], axis=2)
    write_pfm(args.pfm, rgb)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("channel", "wavelength_nm", "S", "B_sr-1",
                         "L_mrad", "surface_TIS_integral", "surface_TIS_Eq6",
                         "captured_crop_fraction", "radius_px",
                         "radial_mean_pixel_irradiance", "reference_wavelength_nm",
                         "width_px", "height_px", "pixel_pitch_um",
                         "focal_length_mm", "subpixels"))
        for case in cases:
            for radius_px, value in enumerate(case["profile"]):
                writer.writerow((case["name"], case["wavelength_nm"], args.slope,
                    f"{case['b']:.12g}", f"{case['l_mrad']:.9g}",
                    f"{case['tis_integral']:.12g}", f"{case['article_tis']:.12g}",
                    f"{case['captured']:.12g}", radius_px, f"{value:.12g}",
                    args.reference_wavelength_nm, args.width, args.height,
                    args.pixel_pitch_um, args.focal_length_mm, args.subpixels))
    make_plate(args.image, cases)
    print(f"wrote {args.pfm}, {args.csv}, {args.image}")
    for case in cases:
        print(f"{case['name']} {case['wavelength_nm']:.0f}nm: "
              f"S={args.slope:.4f} B={case['b']:.6g}sr^-1 "
              f"L={case['l_mrad']:.4f}mrad Eq6TIS={case['article_tis']:.6g} "
              f"captured={case['captured']:.6g}")


if __name__ == "__main__":
    main()
