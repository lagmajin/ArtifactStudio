"""Compare generic and published measured Harvey parameters on the same pupil cones.

The measured Fr3 coefficients belong to Ritt et al.'s achromatic doublet lens
#8, not to the Angenieux prescription bundled with LensFlareLab. This is a
transfer/sensitivity comparison, not a claim that the two lenses share coatings.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_harvey_scatter_reference import calculate_tis


PRESETS = (
    {"name": "Generic Double-Gauss", "slope": -1.86, "b": .36,
     "b0": 6.92, "l_mrad": 2.04, "color": (91, 184, 255)},
    {"name": "Measured achromat Fr3 #8", "slope": -2.22, "b": 1.09,
     "b0": 16.14, "l_mrad": 3.02, "color": (255, 158, 93)},
)


def load_axis_radii(path):
    radii = []
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if float(row["field_deg"]) == 0.0:
                radii.append(float(row["bundle_radius_r90_mm"]))
    if len(radii) != 14 or any(not math.isfinite(value) or value <= 0 for value in radii):
        raise ValueError(f"expected 14 valid on-axis radii in {path}; run the prescription evaluator first")
    return radii


def evaluate_preset(radii, preset, size, pitch_um, transmission, na):
    pitch_mm = pitch_um * 1e-3
    axis = (np.arange(size) + .5 - size / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    a_ent = radii[0]
    irradiance = np.zeros((size, size), dtype=np.float64)
    for a_j in radii:
        angular_coordinate = na * rho / (preset["l_mrad"] * 1e-3 * a_j)
        irradiance += (math.pi * transmission * na**2 * (a_ent / a_j)**2 *
            preset["b0"] * (1.0 + angular_coordinate**2)**(preset["slope"] * .5))
    tis_integral, tis_eq6 = calculate_tis(
        preset["slope"], preset["b"], preset["l_mrad"] * 1e-3, preset["b0"])
    captured = float(irradiance.sum()) * pitch_mm**2 / (math.pi * a_ent**2)
    full_plane = len(radii) * transmission * tis_eq6
    radial_index = np.floor(rho / pitch_mm + .5).astype(np.int32)
    radial_count = np.bincount(radial_index.ravel())
    profile = (np.bincount(radial_index.ravel(), weights=irradiance.ravel()) /
               np.maximum(radial_count, 1))
    return {"preset": preset, "image": irradiance, "profile": profile,
            "tis": tis_integral, "tis_eq6": tis_eq6, "captured": captured,
            "full_plane": full_plane, "captured_ratio": captured / full_plane}


def save_plates(results, image_path, profile_path, pitch_um):
    size = results[0]["image"].shape[0]
    pitch_mm = pitch_um * 1e-3
    panel = 420
    plate = Image.new("RGB", (2 * panel, panel + 34), (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    global_max = max(float(result["image"].max()) for result in results)
    for index, result in enumerate(results):
        values = result["image"]
        log = np.log10(np.maximum(values, global_max * 1e-5))
        gray = np.clip((log - math.log10(global_max * 1e-5)) / 5 * 255,
                       0, 255).astype(np.uint8)
        tile = Image.fromarray(gray).resize((panel, panel), Image.Resampling.BOX)
        x = index * panel
        plate.paste(Image.merge("RGB", (tile, tile, tile)), (x, 34))
        draw.text((x + 8, 8),
            f"{result['preset']['name']}  3.96mm crop={result['captured']*100:.3g}%",
            fill=result["preset"]["color"])
    image_path.parent.mkdir(parents=True, exist_ok=True)
    plate.save(image_path)

    width, height = 920, 600
    left, right, top, bottom = 82, 30, 95, 65
    chart = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    plot_w, plot_h = width - left - right, height - top - bottom
    draw.text((left, 13), "Published Harvey parameter sets on Angenieux pupil cones",
              fill=(235, 238, 244))
    for i, result in enumerate(results):
        preset = result["preset"]
        draw.text((left + 8, 38 + i * 18),
            f"{preset['name']}: S={preset['slope']}, B={preset['b']}, "
            f"B0={preset['b0']}, L={preset['l_mrad']} mrad",
            fill=preset["color"])
    draw.rectangle((left, top, left + plot_w, top + plot_h), outline=(80, 88, 100))
    for decade in range(-7, 1):
        y = top + int(-decade / 7 * plot_h)
        draw.line((left, y, left + plot_w, y), fill=(43, 48, 57))
        draw.text((22, y - 6), f"1e{decade}", fill=(150, 158, 170))
    xmax = size * pitch_mm * .5
    for result in results:
        profile = result["profile"]
        peak = max(float(profile.max()), 1e-300)
        points = []
        for radius_px in range(1, len(profile)):
            radius_mm = radius_px * pitch_mm
            if radius_mm > xmax:
                break
            relative = max(float(profile[radius_px]) / peak, 1e-7)
            x = left + int(radius_mm / xmax * plot_w)
            y = top + int(-math.log10(relative) / 7 * plot_h)
            if top <= y <= top + plot_h:
                points.append((x, y))
        if len(points) > 1:
            draw.line(points, fill=result["preset"]["color"], width=2)
    draw.text((left + plot_w // 2 - 90, height - 18),
              "radius (mm), normalized irradiance", fill=(200, 207, 218))
    profile_path.parent.mkdir(parents=True, exist_ok=True)
    chart.save(profile_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cone-csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/scatter-parameter-sets.png"))
    parser.add_argument("--profile-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/scatter-parameter-profiles.png"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/scatter-parameter-comparison.csv"))
    parser.add_argument("--size", type=int, default=720)
    parser.add_argument("--pixel-pitch-um", type=float, default=5.5)
    parser.add_argument("--transmission", type=float, default=.8)
    parser.add_argument("--na", type=float, default=1 / 44)
    args = parser.parse_args()
    if min(args.size, args.pixel_pitch_um, args.na) <= 0 or not 0 <= args.transmission <= 1:
        parser.error("size, pitch and NA must be positive; transmission must be 0..1")
    radii = load_axis_radii(args.cone_csv)
    results = [evaluate_preset(radii, preset, args.size, args.pixel_pitch_um,
                               args.transmission, args.na)
               for preset in PRESETS]
    save_plates(results, args.image, args.profile_image, args.pixel_pitch_um)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("parameter_set", "S", "B_sr-1", "B0_sr-1", "L_mrad",
                         "radius_px", "radius_mm", "mean_Es_over_Eent",
                         "surface_TIS_integral", "surface_TIS_Eq6",
                         "captured_3p96mm_crop_fraction_of_incident",
                         "full_plane_14_surface_Eq6_prediction", "crop_fraction_of_full_plane"))
        for result in results:
            preset = result["preset"]
            for radius_px, value in enumerate(result["profile"]):
                writer.writerow((preset["name"], preset["slope"], preset["b"],
                    preset["b0"], preset["l_mrad"], radius_px,
                    f"{radius_px * args.pixel_pitch_um * 1e-3:.9g}", f"{value:.12g}",
                    f"{result['tis']:.12g}", f"{result['tis_eq6']:.12g}",
                    f"{result['captured']:.12g}", f"{result['full_plane']:.12g}",
                    f"{result['captured_ratio']:.12g}"))
            preset = result["preset"]
            print(f"{preset['name']}: S={preset['slope']} B={preset['b']} "
                  f"B0={preset['b0']} L={preset['l_mrad']}mrad TIS={result['tis']:.6g} "
                  f"crop={result['captured']:.6g} ({result['captured_ratio']:.3%} of Eq6 full-plane)")


if __name__ == "__main__":
    main()
