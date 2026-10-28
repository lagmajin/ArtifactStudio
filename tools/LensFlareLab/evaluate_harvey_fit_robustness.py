"""Stress the measured-profile fitter with a saturated core and sparse ghosts.

Fixtures are synthetic linear images generated from the Harvey reference model;
they evaluate estimator sensitivity, not camera performance.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_harvey_scatter_reference import pixel_integrated_scatter
from fit_harvey_measurement import fit_profile, radial_profile


def contaminated_fixture(base):
    height, width = base.shape
    center_x, center_y = (width - 1) * .5, (height - 1) * .5
    yy, xx = np.mgrid[:height, :width]
    radius = np.hypot(xx - center_x, yy - center_y)
    result = base.copy()
    result[radius <= 20] = float(base.max()) * 100.0
    radial_bin = np.floor(radius + .5).astype(np.int32)
    radial_sum = np.bincount(radial_bin.ravel(), weights=base.ravel())
    radial_count = np.bincount(radial_bin.ravel())
    base_profile = radial_sum / np.maximum(radial_count, 1)
    # Isolated reflection-like spots occupy only a small fraction of their rings.
    for radius_px, angle, strength in ((62, .4, 160), (98, 1.7, 240),
                                       (142, 3.3, 220), (185, 5.1, 300)):
        gx = center_x + radius_px * math.cos(angle)
        gy = center_y + radius_px * math.sin(angle)
        local = np.exp(-((xx - gx)**2 + (yy - gy)**2) / (2 * 3.0**2))
        result += float(base_profile[radius_px]) * strength * local
    # Small residual black-level error after dark subtraction.
    result += float(base.max()) * 0.002
    return result


def analyze(image, statistic, min_radius=24, max_radius=240):
    height, width = image.shape
    center = ((width - 1) * .5, (height - 1) * .5)
    radii, angles, values, counts, valid = radial_profile(
        image, *center, 5.5, 100.0, min_radius, max_radius, statistic)
    fit = fit_profile(angles, values, valid, -1.86)
    return radii, angles, values, valid, fit


def save_image_plate(path, clean, stressed):
    size = clean.shape[0]
    panel = 360
    top = 32
    canvas = Image.new("RGB", (2 * panel, panel + top), (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    maximum = float(stressed.max())
    for index, (title, data) in enumerate((
            ("Clean synthetic capture", clean),
            ("Saturated core + ghost spots + offset", stressed))):
        log = np.log10(np.maximum(data, maximum * 1e-7))
        gray = np.clip((log - math.log10(maximum * 1e-7)) / 7 * 255, 0, 255).astype(np.uint8)
        tile = Image.fromarray(gray).resize((panel, panel), Image.Resampling.BOX)
        x, y = index * panel, 0
        canvas.paste(Image.merge("RGB", (tile, tile, tile)), (x, y + top))
        draw.text((x + 8, y + 8), title, fill=(235, 238, 244))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def save_profile_chart(path, rows):
    width, height = 900, 570
    left, right, top, bottom = 82, 30, 105, 55
    plot_w, plot_h = width - left - right, height - top - bottom
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    draw.text((left, 12), "Radial estimator response to sparse flare contamination",
              fill=(235, 238, 244))
    colors = ((90, 194, 255), (116, 220, 169),
              (255, 178, 88), (233, 112, 148))
    for i, row in enumerate(rows):
        fit = row["fit"]
        draw.text((left + 8, 36 + i * 18),
                  f"{row['case']} / {row['statistic']}: S={fit['slope']:.3f}, "
                  f"L={fit['l_mrad']:.3f} mrad, true ΔS={row['error_s_percent']:+.2f}%, "
                  f"ΔL={row['error_l_percent']:+.2f}%",
                  fill=colors[i % len(colors)])
    draw.rectangle((left, top, left + plot_w, top + plot_h), outline=(80, 88, 100))
    all_values = np.concatenate([r["values"][r["valid"]] for r in rows])
    y_min = max(float(all_values.min()), float(all_values.max()) * 1e-7)
    y_max = float(all_values.max())
    lo, hi = math.log10(y_min), math.log10(y_max)
    xmax = max(float(max(r["radii"][-1] for r in rows)), 1.0)
    for decade in range(math.floor(lo), math.ceil(hi) + 1):
        if lo <= decade <= hi:
            y = top + int((hi - decade) / (hi - lo) * plot_h)
            draw.line((left, y, left + plot_w, y), fill=(43, 48, 57))
    for i, row in enumerate(rows):
        points = []
        for r, value, valid in zip(row["radii"], row["values"], row["valid"]):
            if not valid or value <= 0:
                continue
            x = left + int(r / xmax * plot_w)
            y = top + int((hi - math.log10(value)) / (hi - lo) * plot_h)
            if top <= y <= top + plot_h:
                points.append((x, y))
        if len(points) > 1:
            draw.line(points, fill=colors[i % len(colors)], width=2)
    draw.text((left + plot_w // 2 - 40, height - 18), "radius (pixels)",
              fill=(200, 207, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-fit-robustness.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-fit-robustness-images.png"))
    parser.add_argument("--profile-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-fit-robustness-profiles.png"))
    args = parser.parse_args()
    width = height = 512
    pitch = 5.5e-3
    base, _, _, _ = pixel_integrated_scatter(
        width, height, pitch, 100.0, -1.86, .36, .00204, 8)
    clean = base
    stressed = contaminated_fixture(base)
    results = []
    for case_name, source in (("clean", clean), ("contaminated", stressed)):
        for statistic in ("median", "mean"):
            radii, angles, values, valid, fit = analyze(source, statistic)
            results.append({"case": case_name, "statistic": statistic,
                            "radii": radii, "angles": angles, "values": values,
                            "valid": valid, "fit": fit})
    baseline = next(r for r in results if r["case"] == "clean" and r["statistic"] == "median")["fit"]
    true_slope, true_l_mrad = -1.86, 2.04
    for row in results:
        row["delta_s_percent"] = 100 * (row["fit"]["slope"] / baseline["slope"] - 1)
        row["delta_l_percent"] = 100 * (row["fit"]["l_mrad"] / baseline["l_mrad"] - 1)
        row["error_s_percent"] = 100 * (row["fit"]["slope"] / true_slope - 1)
        row["error_l_percent"] = 100 * (row["fit"]["l_mrad"] / true_l_mrad - 1)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("case", "statistic", "S", "L_mrad", "image_scale",
                         "offset", "log_rmse", "r2_log", "delta_S_percent_vs_clean_median",
                         "delta_L_percent_vs_clean_median", "error_S_percent_vs_truth",
                         "error_L_percent_vs_truth"))
        for row in results:
            fit = row["fit"]
            writer.writerow((row["case"], row["statistic"], f"{fit['slope']:.9g}",
                f"{fit['l_mrad']:.9g}", f"{fit['scale']:.12g}", f"{fit['offset']:.12g}",
                f"{fit['rmse_log']:.9g}", f"{fit['r2_log']:.9g}",
                f"{row['delta_s_percent']:.9g}", f"{row['delta_l_percent']:.9g}",
                f"{row['error_s_percent']:.9g}", f"{row['error_l_percent']:.9g}"))
            print(f"{row['case']} {row['statistic']}: S={fit['slope']:.5f} "
                  f"L={fit['l_mrad']:.5f}mrad ΔS={row['delta_s_percent']:+.3f}% "
                  f"ΔL={row['delta_l_percent']:+.3f}% trueError="
                  f"({row['error_s_percent']:+.3f}%,{row['error_l_percent']:+.3f}%) "
                  f"logRMSE={fit['rmse_log']:.5g}")
    save_image_plate(args.image, clean, stressed)
    save_profile_chart(args.profile_image, results)


if __name__ == "__main__":
    main()
