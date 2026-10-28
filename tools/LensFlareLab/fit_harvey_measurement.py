"""Fit a radial Harvey-Shack scatter profile from a linearized PFM capture.

Use a dark-subtracted, linear radiance image of a small bright point source.
The fit estimates shape (S and L) and an image-domain scale only; absolute B
requires calibrated incident irradiance, sensor response, and optical throughput.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.optimize import least_squares


def read_pfm(path):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM input: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        payload = np.frombuffer(stream.read(), dtype="<f4" if scale < 0 else ">f4")
    if width <= 0 or height <= 0 or not math.isfinite(scale) or scale == 0:
        raise ValueError(f"invalid PFM header: {path}")
    if payload.size != width * height * 3:
        raise ValueError(f"invalid PFM pixel count: {path}")
    return np.flipud(payload.reshape(height, width, 3)).astype(np.float64) * abs(scale)


def radial_profile(image, center_x, center_y, pixel_pitch_um, focal_length_mm,
                   min_radius_px, max_radius_px, statistic):
    height, width = image.shape
    yy, xx = np.mgrid[:height, :width]
    radii = np.hypot(xx - center_x, yy - center_y)
    radial_bin = np.floor(radii + .5).astype(np.int32)
    bins = np.arange(max_radius_px + 1)
    values, counts = [], []
    for radius in bins:
        selected = image[radial_bin == radius]
        counts.append(selected.size)
        if selected.size:
            values.append(float(np.mean(selected) if statistic == "mean" else np.median(selected)))
        else:
            values.append(math.nan)
    angles = np.arctan2(bins * pixel_pitch_um * 1e-3, focal_length_mm)
    valid = ((bins >= min_radius_px) & (bins <= max_radius_px) &
             np.isfinite(values) & (np.asarray(values) > 0))
    return bins, angles, np.asarray(values), np.asarray(counts), valid


def fit_profile(angles, values, valid, initial_slope):
    theta = angles[valid]
    measured = values[valid]
    if measured.size < 24:
        raise ValueError(f"only {measured.size} positive annuli available; need at least 24")
    floor = max(float(np.percentile(measured, 10)) * .05, np.finfo(float).tiny)
    observed = np.log(measured)

    def residual(params):
        log_scale, slope, log_l, log_offset = params
        scale, l_rad, offset = math.exp(log_scale), math.exp(log_l), math.exp(log_offset)
        model = scale * (1.0 + (np.sin(theta) / l_rad) ** 2) ** (slope * .5) + offset
        return np.log(np.maximum(model, floor)) - observed

    y = measured
    initial_scale = max(float(np.max(y)), floor)
    initial_offset = max(float(np.percentile(y, 10)) * .1, floor)
    results = []
    for initial_l_mrad in (.25, .5, 1.0, 2.0, 4.0, 8.0, 16.0):
        initial_l = initial_l_mrad * 1e-3
        results.append(least_squares(residual,
            (math.log(initial_scale), initial_slope, math.log(initial_l),
             math.log(initial_offset)),
            bounds=((-100.0, -4.0, math.log(5e-5), math.log(floor)),
                    (100.0, -0.05, math.log(.25), math.log(max(y.max(), floor)))),
            loss="soft_l1", f_scale=.05, max_nfev=4000, x_scale="jac"))
    result = min(results, key=lambda candidate: candidate.cost)
    log_scale, slope, log_l, log_offset = result.x
    predicted = np.full_like(values, np.nan)
    predicted[:] = math.exp(log_scale) * (
        1.0 + (np.sin(angles) / math.exp(log_l)) ** 2) ** (slope * .5) + math.exp(log_offset)
    log_residual = np.log(np.maximum(predicted[valid], floor)) - observed
    rmse = float(np.sqrt(np.mean(log_residual**2)))
    denominator = float(np.sum((observed - observed.mean())**2))
    r2_log = 1.0 - float(np.sum(log_residual**2)) / max(denominator, 1e-30)
    return {"scale": math.exp(log_scale), "slope": slope,
            "l_mrad": math.exp(log_l) * 1000.0,
            "offset": math.exp(log_offset), "rmse_log": rmse,
            "r2_log": r2_log, "predicted": predicted, "success": result.success,
            "message": result.message}


def save_profile_chart(path, radius_px, measured, fitted, valid, fit):
    width, height = 900, 560
    left, right, top, bottom = 88, 30, 86, 58
    plot_w, plot_h = width - left - right, height - top - bottom
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    draw.text((left, 12), "Measured point-source scatter profile vs Harvey-Shack fit",
              fill=(235, 238, 244))
    draw.text((left, 37),
              f"S={fit['slope']:.4f}  L={fit['l_mrad']:.4f} mrad  "
              f"image scale={fit['scale']:.5g}  log-RMSE={fit['rmse_log']:.4g}  "
              f"R²(log)={fit['r2_log']:.5f}", fill=(134, 207, 255))
    draw.rectangle((left, top, left + plot_w, top + plot_h), outline=(80, 88, 100))
    positive = np.concatenate((measured[np.isfinite(measured) & (measured > 0)],
                               fitted[np.isfinite(fitted) & (fitted > 0)]))
    y_min = max(float(positive.min()), float(positive.max()) * 1e-7)
    y_max = float(positive.max())
    log_min, log_max = math.log10(y_min), math.log10(y_max)
    max_x = max(float(radius_px[-1]), 1.0)
    for decade in range(math.floor(log_min), math.ceil(log_max) + 1):
        if not log_min <= decade <= log_max:
            continue
        y = top + int((log_max - decade) / (log_max - log_min) * plot_h)
        draw.line((left, y, left + plot_w, y), fill=(43, 48, 57))
        draw.text((12, y - 6), f"1e{decade}", fill=(150, 158, 170))
    measured_points, fitted_points = [], []
    for index, radius in enumerate(radius_px):
        x = left + int(radius / max_x * plot_w)
        if valid[index] and measured[index] > 0:
            y = top + int((log_max - math.log10(measured[index])) /
                          (log_max - log_min) * plot_h)
            if top <= y <= top + plot_h:
                measured_points.append((x, y))
        if math.isfinite(fitted[index]) and fitted[index] > 0:
            y = top + int((log_max - math.log10(fitted[index])) /
                          (log_max - log_min) * plot_h)
            if top <= y <= top + plot_h:
                fitted_points.append((x, y))
    for x, y in measured_points:
        draw.ellipse((x - 2, y - 2, x + 2, y + 2), fill=(245, 190, 94))
    if len(fitted_points) > 1:
        draw.line(fitted_points, fill=(89, 197, 255), width=2)
    draw.text((left + plot_w // 2 - 40, height - 18), "radius (pixels)",
              fill=(200, 207, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("measurement", type=Path, help="dark-subtracted linear RGB PFM")
    parser.add_argument("--dark", type=Path, help="matching linear RGB dark-frame PFM")
    parser.add_argument("--center", type=float, nargs=2, metavar=("X", "Y"),
                        help="source center in pixel coordinates; default is image center")
    parser.add_argument("--pixel-pitch-um", type=float, required=True)
    parser.add_argument("--focal-length-mm", type=float, required=True)
    parser.add_argument("--core-radius-px", type=int, default=8,
                        help="exclude saturated/specular core and central diffraction")
    parser.add_argument("--max-radius-px", type=int)
    parser.add_argument("--statistic", choices=("median", "mean"), default="median")
    parser.add_argument("--initial-slope", type=float, default=-1.86)
    parser.add_argument("--csv", type=Path, default=Path("build/LensFlareLab/harvey-measurement-fit.csv"))
    parser.add_argument("--image", type=Path, default=Path("build/LensFlareLab/harvey-measurement-fit.png"))
    args = parser.parse_args()
    if args.pixel_pitch_um <= 0 or args.focal_length_mm <= 0 or args.core_radius_px < 0:
        parser.error("pixel pitch and focal length must be positive; core radius must be nonnegative")
    measurement = read_pfm(args.measurement)
    if args.dark:
        dark = read_pfm(args.dark)
        if dark.shape != measurement.shape:
            parser.error("dark frame dimensions must match measurement")
        measurement = measurement - dark
    height, width, _ = measurement.shape
    center_x, center_y = args.center or ((width - 1) * .5, (height - 1) * .5)
    max_radius = args.max_radius_px or int(min(center_x, center_y, width - 1 - center_x,
                                               height - 1 - center_y))
    if max_radius <= args.core_radius_px + 24:
        parser.error("fitting annulus must span at least 24 pixels beyond the excluded core")
    luminance = np.maximum(0.0, measurement @ np.array((.2126, .7152, .0722)))
    radii, angles, values, counts, positive = radial_profile(
        luminance, center_x, center_y, args.pixel_pitch_um, args.focal_length_mm,
        args.core_radius_px, max_radius, args.statistic)
    fit = fit_profile(angles, values, positive, args.initial_slope)
    csv_path = args.csv
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("radius_px", "angle_mrad", "sample_count", "measured_profile",
                         "fitted_profile", "relative_residual", "included_in_fit"))
        for index in range(len(radii)):
            residual = (values[index] / fit["predicted"][index] - 1.0
                        if values[index] > 0 and math.isfinite(fit["predicted"][index]) else math.nan)
            writer.writerow((radii[index], f"{angles[index]*1000:.9g}", counts[index],
                f"{values[index]:.12g}" if math.isfinite(values[index]) else "",
                f"{fit['predicted'][index]:.12g}" if math.isfinite(fit["predicted"][index]) else "",
                f"{residual:.9g}" if math.isfinite(residual) else "",
                int(positive[index])))
    save_profile_chart(args.image, radii, values, fit["predicted"], positive, fit)
    print(f"fit_success={fit['success']} S={fit['slope']:.6g} "
          f"L={fit['l_mrad']:.6g} mrad image_scale={fit['scale']:.6g} "
          f"offset={fit['offset']:.6g} log_RMSE={fit['rmse_log']:.6g} "
          f"R2_log={fit['r2_log']:.6g} annuli={positive.sum()}")
    print("B/B0 are not reported as physical BSDF coefficients: absolute calibration "
          "of source irradiance, sensor response, and system throughput is required.")


if __name__ == "__main__":
    main()
