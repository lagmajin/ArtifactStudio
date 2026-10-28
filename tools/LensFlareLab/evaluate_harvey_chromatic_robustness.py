"""Stress per-channel Harvey fits with photon/read noise and colored ghosts.

This deterministic linear-image experiment evaluates estimator behavior; it
does not represent a measured camera or calibrated sensor noise model.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_harvey_chromatic_fit import CASES
from evaluate_harvey_scatter_reference import pixel_integrated_scatter
from fit_harvey_measurement import fit_profile, radial_profile, save_spatial_residual


def make_contaminated(clean):
    height, width, _ = clean.shape
    center_x, center_y = (width - 1) * .5, (height - 1) * .5
    yy, xx = np.mgrid[:height, :width]
    radius = np.hypot(xx - center_x, yy - center_y)
    result = clean.copy()
    peak = float(clean.max())
    result[radius <= 20] = peak * 100.0
    spots = ((62, .4, (1.0, .85, .85)),
             (98, 1.7, (1.0, .12, .06)),
             (142, 3.3, (.06, .8, .16)),
             (185, 5.1, (.08, .22, 1.0)))
    for radius_px, angle, color in spots:
        gx = center_x + radius_px * math.cos(angle)
        gy = center_y + radius_px * math.sin(angle)
        local = np.exp(-((xx - gx)**2 + (yy - gy)**2) / (2 * 3.0**2))
        annulus = int(round(radius_px))
        for channel in range(3):
            ring = clean[np.floor(radius + .5).astype(np.int32) == annulus, channel]
            baseline = float(np.median(ring))
            result[:, :, channel] += baseline * 180.0 * color[channel] * local

    # A fixed-seed Poisson shot-noise approximation plus 3-count read noise.
    rng = np.random.default_rng(20261028)
    photon_scale = 1.0e8 / peak
    expected_counts = np.maximum(result, 0.0) * photon_scale
    noisy_counts = rng.poisson(expected_counts).astype(np.float64)
    noisy_counts += rng.normal(0.0, 3.0, noisy_counts.shape)
    result = noisy_counts / photon_scale
    # A residual dark-subtraction error, deliberately kept positive here.
    result += peak * .0002
    return np.maximum(result, 0.0)


def make_plate(path, clean, contaminated, residuals):
    size = 320
    labels = ("clean synthetic", "noisy + colored ghost spots", "R / G / B fit residuals")
    canvas = Image.new("RGB", (size * 3, size + 30), (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    for index, label in enumerate(labels):
        draw.text((index * size + 7, 8), label, fill=(235, 238, 244))
    peak = float(contaminated.max())
    floor = peak * 1e-8
    display = np.zeros((clean.shape[0], clean.shape[1], 3), dtype=np.uint8)
    for channel in range(3):
        log = np.log10(np.maximum(contaminated[:, :, channel], floor))
        display[:, :, channel] = np.clip((log - math.log10(floor)) / 8 * 255,
                                         0, 255).astype(np.uint8)
    clean_disp = np.zeros_like(display)
    for channel in range(3):
        log = np.log10(np.maximum(clean[:, :, channel], floor))
        clean_disp[:, :, channel] = np.clip((log - math.log10(floor)) / 8 * 255,
                                            0, 255).astype(np.uint8)
    tiles = [Image.fromarray(clean_disp), Image.fromarray(display),
             Image.fromarray(residuals)]
    for index, tile in enumerate(tiles):
        canvas.paste(tile.resize((size, size), Image.Resampling.BOX),
                     (index * size, 30))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-chromatic-robustness.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-chromatic-robustness.png"))
    parser.add_argument("--residual-dir", type=Path, default=Path(
        "tools/LensFlareLab/examples/harvey-chromatic-robustness-residuals"))
    args = parser.parse_args()
    width = height = 512
    center_x = center_y = (width - 1) * .5
    pitch_mm = 5.5e-3
    clean = np.zeros((height, width, 3), dtype=np.float64)
    for channel, (_, slope, b, l_rad) in enumerate(CASES):
        clean[:, :, channel], _, _, _ = pixel_integrated_scatter(
            width, height, pitch_mm, 100.0, slope, b, l_rad, 8)
    stressed = make_contaminated(clean)
    yy, xx = np.mgrid[:height, :width]
    radius = np.hypot(xx - center_x, yy - center_y)
    radial_bin = np.floor(radius + .5).astype(np.int32)
    out_rows = []
    residual_rgb = np.zeros((height, width, 3), dtype=np.uint8)
    for channel, (name, true_s, _, true_l_rad) in enumerate(CASES):
        for case_name, source in (("clean", clean), ("stressed", stressed)):
            plane = source[:, :, channel]
            radii, angles, values, counts, valid = radial_profile(
                plane, center_x, center_y, 5.5, 100.0, 24, 240, "median")
            fit = fit_profile(angles, values, valid, -1.86)
            save_spatial_residual(
                args.residual_dir / f"{name}-{case_name}.png", plane,
                center_x, center_y, 24, 240, fit["predicted"], fit)
            if case_name == "stressed":
                # Compose each signed residual channel's magnitude into RGB.
                floor = max(float(np.max(plane)) * 1e-9,
                            fit["scale"] * 1e-8, np.finfo(float).tiny)
                expected = fit["predicted"][np.clip(radial_bin, 0,
                                                    len(fit["predicted"])-1)]
                log_resid = np.log10(np.maximum(plane, floor) /
                                     np.maximum(expected, floor))
                valid_px = (radius > 24) & (radius <= 240)
                magnitude = np.clip(np.abs(log_resid) / 1.5, 0, 1)
                residual_rgb[:, :, channel] = np.where(valid_px,
                    (50 + 205 * magnitude).astype(np.uint8), 18)
            out_rows.append((name, case_name, true_s, fit["slope"],
                100 * (fit["slope"] / true_s - 1.0), true_l_rad * 1000,
                fit["l_mrad"], 100 * (fit["l_mrad"] / (true_l_rad * 1000) - 1.0),
                fit["rmse_log"], fit["r2_log"]))

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("channel", "case", "true_S", "fit_S", "S_error_percent",
                         "true_L_mrad", "fit_L_mrad", "L_error_percent",
                         "log_rmse", "r2_log"))
        writer.writerows(out_rows)
    make_plate(args.image, clean, stressed, residual_rgb)
    for row in out_rows:
        name, case_name, _, fitted_s, error_s, _, fitted_l, error_l, rmse, _ = row
        print(f"{name} {case_name}: S={fitted_s:.5f} ({error_s:+.2f}%) "
              f"L={fitted_l:.4f}mrad ({error_l:+.2f}%) logRMSE={rmse:.5g}")


if __name__ == "__main__":
    main()
