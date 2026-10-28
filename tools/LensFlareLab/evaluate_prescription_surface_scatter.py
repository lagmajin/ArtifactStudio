"""Map a Harvey-Shack/Peterson scatter model onto traced prescription pupils.

The output is an exploratory, monochromatic surface-by-surface comparison,
not a calibrated lens prediction. Prescription data and geometric ray routines
are shared with evaluate_direct_transmission_raytrace.py.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_direct_transmission_raytrace as raytrace
from evaluate_harvey_scatter_reference import calculate_tis


def surface_bundle(field_deg, wavelength_nm, grid_size):
    field = math.radians(field_deg)
    direction = np.array((math.sin(field), 0.0, math.cos(field)))
    chief, _ = raytrace.chief_ray(field, wavelength_nm)
    transverse_x = np.array((math.cos(field), 0.0, -math.sin(field)))
    transverse_y = np.array((0.0, 1.0, 0.0))
    extent = raytrace.STOP_RADIUS
    coords = (np.arange(grid_size) + 0.5) * (2 * extent / grid_size) - extent
    hits = [[] for _ in raytrace.SURFACES]
    accepted = 0
    for y in coords:
        for x in coords:
            origin = chief + x * transverse_x + y * transverse_y
            d = direction.copy()
            valid = True
            for i, surface in enumerate(raytrace.SURFACES):
                if i == 7:
                    t = (raytrace.IRIS_Z - origin[2]) / d[2]
                    if t <= 0:
                        valid = False
                        break
                    iris = origin + t * d
                    if not raytrace.in_hexagon(iris[0], iris[1], raytrace.STOP_RADIUS):
                        valid = False
                        break
                    origin = iris
                point = raytrace.intersect(origin, d, surface)
                if point is None:
                    valid = False
                    break
                hits[i].append(point[:2].copy())
                n1 = 1.0 if i == 0 else raytrace.refractive_index(
                    raytrace.SURFACES[i - 1], wavelength_nm)
                n2 = raytrace.refractive_index(surface, wavelength_nm)
                d = raytrace.refract(d, point, surface, n1, n2)
                if d is None:
                    valid = False
                    break
                origin = point
            accepted += int(valid)
    stats = []
    for i, points in enumerate(hits):
        values = np.asarray(points, dtype=np.float64).reshape(-1, 2)
        if not len(values):
            stats.append((i, 0, math.nan, math.nan, math.nan, math.nan))
            continue
        center = np.median(values, axis=0)
        radii = np.linalg.norm(values - center, axis=1)
        stats.append((i, len(values), float(np.quantile(radii, .5)),
                      float(np.quantile(radii, .9)), float(np.quantile(radii, .99)),
                      float(np.sqrt(np.mean(radii * radii)))))
    return accepted, stats


def make_plate(cases, path):
    width, height = 720, 440
    margin = 68
    image = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    draw.text((12, 12), "Prescription surface scatter: field-dependent pupil radius",
              fill=(235, 238, 244))
    plot = (margin, 50, width - 24, height - 48)
    draw.rectangle(plot, outline=(80, 88, 100))
    colors = ((95, 184, 255), (255, 193, 87), (255, 115, 147), (140, 220, 168))
    all_values = [row[3] for case in cases for row in case["rows"] if math.isfinite(row[3])]
    ymax = max(all_values) * 1.08
    n = len(raytrace.SURFACES)
    for case_i, case in enumerate(cases):
        pts = []
        for row in case["rows"]:
            if math.isfinite(row[3]):
                x = plot[0] + row[0] * (plot[2] - plot[0]) / (n - 1)
                y = plot[3] - row[3] / ymax * (plot[3] - plot[1])
                pts.append((x, y))
        if len(pts) > 1:
            draw.line(pts, fill=colors[case_i % len(colors)], width=2)
        draw.text((plot[0] + 8, plot[1] + 8 + case_i * 17),
                  f"{case['field_deg']:g} deg field", fill=colors[case_i % len(colors)])
    for i in range(n):
        x = plot[0] + i * (plot[2] - plot[0]) / (n - 1)
        draw.line((x, plot[3], x, plot[3] + 4), fill=(150, 158, 170))
        draw.text((x - 4, plot[3] + 7), str(i), fill=(180, 187, 198))
    draw.text((plot[0], height - 20), "surface index", fill=(200, 207, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def make_scatter_plate(cases, path, pitch_um, focal_mm, slope, b, l_mrad,
                       transmission, na):
    """Render the Peterson per-surface irradiance model with traced cone radii."""
    size = 720
    pitch_mm = pitch_um * 1e-3
    axis = (np.arange(size) + .5 - size / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    images = []
    b0 = b * (100.0 * l_mrad * 1e-3) ** slope
    for case in cases:
        acc = np.zeros((size, size), dtype=np.float64)
        a_ent = case["rows"][0][3]
        field_transmission = transmission * case["accepted"] / case["launched"]
        for row in case["rows"]:
            a_j = row[3]
            if not math.isfinite(a_j) or a_j <= 1e-8:
                continue
            # Peterson Eq. 8. r is sensor-plane distance; a_ent/a_j are the
            # traced 90%-energy pupil-cone radii (an operational proxy).
            scatter_angle = na * rho / (l_mrad * 1e-3 * a_j)
            acc += (math.pi * field_transmission * na**2 * (a_ent / a_j) ** 2 * b0 *
                    (1.0 + scatter_angle**2) ** (slope * .5))
        images.append(acc)
    maximum = max(float(im.max()) for im in images)
    panel = 360
    canvas = Image.new("RGB", (2 * panel, 2 * (panel + 28)), (18, 21, 26))
    for i, (case, data) in enumerate(zip(cases, images)):
        log = np.log10(np.maximum(data, maximum * 1e-5))
        gray = np.clip((log - math.log10(maximum * 1e-5)) / 5 * 255, 0, 255).astype(np.uint8)
        tile = Image.fromarray(gray).resize((panel, panel), Image.Resampling.BOX)
        x, y = (i % 2) * panel, (i // 2) * (panel + 28)
        canvas.paste(Image.merge("RGB", (tile, tile, tile)), (x, y + 28))
    draw = ImageDraw.Draw(canvas)
    for i, case in enumerate(cases):
        x, y = (i % 2) * panel, (i // 2) * (panel + 28)
        draw.text((x + 8, y + 8), f"{case['field_deg']:g} deg field, summed surface scatter",
                  fill=(235, 238, 244))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def make_l_sweep(case, image_path, profile_path, csv_path, pitch_um, focal_mm,
                 slope, b, l_values, transmission, na):
    """Compare L roll-off values and integrate Eq. 8 over the finite sensor."""
    size = 720
    pitch_mm = pitch_um * 1e-3
    axis = (np.arange(size) + .5 - size / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    radial_bins = np.floor(rho / pitch_mm + .5).astype(np.int32)
    max_bin = int(radial_bins.max()) + 1
    counts = np.bincount(radial_bins.ravel(), minlength=max_bin)
    a_ent = case["rows"][0][3]
    field_transmission = transmission * case["accepted"] / case["launched"]
    profiles = []
    records = []
    for l_mrad in l_values:
        l_rad = l_mrad * 1e-3
        b0 = b * (100.0 * l_rad) ** slope
        irradiance = np.zeros((size, size), dtype=np.float64)
        for row in case["rows"]:
            a_j = row[3]
            if not math.isfinite(a_j) or a_j <= 1e-8:
                continue
            angle_term = na * rho / (l_rad * a_j)
            irradiance += (math.pi * field_transmission * na**2 *
                (a_ent / a_j)**2 * b0 * (1.0 + angle_term**2)**(slope * .5))
        profile = np.bincount(radial_bins.ravel(), weights=irradiance.ravel(),
                              minlength=max_bin) / np.maximum(counts, 1)
        tis_integral, tis_eq6 = calculate_tis(
            slope, b, l_rad, b0)
        captured_fraction = (float(irradiance.sum()) * pitch_mm**2 /
                             (math.pi * a_ent**2))
        predicted_infinite_fraction = (len(case["rows"]) * field_transmission * tis_eq6)
        profiles.append((l_mrad, profile, irradiance))
        records.append((l_mrad, b0, tis_integral, tis_eq6,
                        captured_fraction, predicted_infinite_fraction))

    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("L_mrad", "radius_px", "radius_mm", "mean_Es_over_Eent",
                         "surface_TIS_integral", "surface_TIS_Eq6",
                         "captured_sensor_fraction_of_incident", "infinite_sensor_Eq6_prediction",
                         "captured_fraction_of_infinite_prediction"))
        for l_value, profile, _ in profiles:
            record = next(r for r in records if r[0] == l_value)
            captured, predicted = record[4], record[5]
            for radius_px, value in enumerate(profile):
                writer.writerow((f"{l_value:.6g}", radius_px,
                    f"{radius_px * pitch_mm:.9g}", f"{value:.12g}",
                    f"{record[2]:.12g}", f"{record[3]:.12g}",
                    f"{captured:.12g}", f"{predicted:.12g}",
                    f"{captured / predicted:.12g}" if predicted > 0 else "nan"))

    # Draw radial irradiance normalized per curve; this compares shape without
    # hiding that the absolute center coefficient also changes with L.
    chart_w, chart_h = 920, 600
    left, right, top, bottom = 80, 30, 110, 65
    chart = Image.new("RGB", (chart_w, chart_h), (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    plot_w, plot_h = chart_w - left - right, chart_h - top - bottom
    draw.text((left, 14), "Peterson Eq. 8 radial scatter profile (axis field)",
              fill=(235, 238, 244))
    draw.rectangle((left, top, left + plot_w, top + plot_h), outline=(80, 88, 100))
    colors = ((91, 184, 255), (109, 229, 176), (255, 196, 90), (255, 119, 139))
    xmax = max(1.0, (size // 2) * pitch_mm)
    ymin, ymax = 1e-7, 1.0
    for decade in range(-7, 1):
        y = top + int((math.log10(ymax) - decade) / 7 * plot_h)
        draw.line((left, y, left + plot_w, y), fill=(43, 48, 57))
        draw.text((20, y - 6), f"1e{decade}", fill=(150, 158, 170))
    for index, (l_value, profile, _) in enumerate(profiles):
        peak = max(float(profile.max()), 1e-300)
        points = []
        for r in range(1, len(profile)):
            radius_mm = r * pitch_mm
            if radius_mm > xmax:
                break
            normalized_value = max(float(profile[r]) / peak, ymin)
            x = left + int(radius_mm / xmax * plot_w)
            y = top + int(-math.log10(normalized_value) / 7 * plot_h)
            if top <= y <= top + plot_h:
                points.append((x, y))
        if len(points) > 1:
            draw.line(points, fill=colors[index % len(colors)], width=2)
        rec = records[index]
        draw.text((left + 10, 31 + index * 17),
                  f"L={l_value:g} mrad  frame={rec[4]*100:.3g}% of incident",
                  fill=colors[index % len(colors)])
    draw.text((left + plot_w // 2 - 80, chart_h - 19),
              "sensor radius (mm), log irradiance / center", fill=(200, 207, 218))
    profile_path.parent.mkdir(parents=True, exist_ok=True)
    chart.save(profile_path)

    panel = 360
    plate = Image.new("RGB", (2 * panel, 2 * (panel + 28)), (18, 21, 26))
    maximum = max(float(entry[2].max()) for entry in profiles)
    draw = ImageDraw.Draw(plate)
    for i, (l_value, _, irradiance) in enumerate(profiles):
        log = np.log10(np.maximum(irradiance, maximum * 1e-5))
        gray = np.clip((log - math.log10(maximum * 1e-5)) / 5 * 255, 0, 255).astype(np.uint8)
        tile = Image.fromarray(gray).resize((panel, panel), Image.Resampling.BOX)
        x, y = (i % 2) * panel, (i // 2) * (panel + 28)
        plate.paste(Image.merge("RGB", (tile, tile, tile)), (x, y + 28))
        rec = records[i]
        draw.text((x + 8, y + 8),
                  f"L={l_value:g} mrad  captured={rec[4]*100:.3g}%",
                  fill=(235, 238, 244))
    image_path.parent.mkdir(parents=True, exist_ok=True)
    plate.save(image_path)
    for l_value, b0, tis_integral, tis_eq6, captured, predicted in records:
        print(f"L={l_value:g} mrad b0={b0:.6g} TIS={tis_integral:.6g} "
              f"(Eq6 {tis_eq6:.6g}) frame={captured:.6g} "
              f"({captured / predicted:.3%} of 14-surface Eq6 integral)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fields", type=float, nargs="+", default=[0, 5, 10, 12])
    parser.add_argument("--wavelength-nm", type=float, default=550)
    parser.add_argument("--grid", type=int, default=48)
    parser.add_argument("--slope", type=float, default=-1.86)
    parser.add_argument("--b", type=float, default=.36)
    parser.add_argument("--l-mrad", type=float, default=2.04)
    parser.add_argument("--l-sweep-mrad", type=float, nargs="+", default=[.5, 1, 2.04, 4])
    parser.add_argument("--transmission", type=float, default=.8,
                        help="post-scatter system transmission assumption (0..1)")
    parser.add_argument("--na", type=float, default=1.0 / 44.0,
                        help="image-side numerical aperture; default is f/22")
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-bundle-radii.png"))
    parser.add_argument("--scatter-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter-fields.png"))
    parser.add_argument("--l-sweep-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter-l-sweep.png"))
    parser.add_argument("--l-profile-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter-profile.png"))
    parser.add_argument("--l-profile-csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter-profile.csv"))
    args = parser.parse_args()
    if (args.grid < 4 or args.grid > 256 or args.wavelength_nm <= 0 or
            args.l_mrad <= 0 or args.transmission < 0 or args.transmission > 1 or
            args.na <= 0 or args.na >= 1 or args.b <= 0 or args.slope >= 0 or
            not args.l_sweep_mrad or any(value <= 0 for value in args.l_sweep_mrad)):
        parser.error("grid/wavelength/L/b/NA must be positive, slope negative, and T in 0..1")
    cases = []
    for field in args.fields:
        accepted, rows = surface_bundle(field, args.wavelength_nm, args.grid)
        cases.append({"field_deg": field, "accepted": accepted,
                      "launched": args.grid**2, "rows": rows})
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("field_deg", "accepted_full_path_rays", "surface_index",
                         "surface_ray_count", "bundle_radius_r50_mm", "bundle_radius_r90_mm",
                         "bundle_radius_r99_mm", "bundle_radius_rms_mm",
                         "peterson_eq8_center_coefficient"))
        for case in cases:
            a_ent = case["rows"][0][3]
            t_field = args.transmission * case["accepted"] / case["launched"]
            b0 = args.b * (100.0 * args.l_mrad * 1e-3) ** args.slope
            for row in case["rows"]:
                a_j = row[3]
                center_coefficient = (math.pi * t_field * args.na**2 *
                    (a_ent / a_j) ** 2 * b0) if math.isfinite(a_j) and a_j > 1e-8 else math.nan
                writer.writerow((case["field_deg"], case["accepted"], *row,
                                 f"{center_coefficient:.12g}"))
    make_plate(cases, args.image)
    make_scatter_plate(cases, args.scatter_image, 5.5, 100.0, args.slope,
                       args.b, args.l_mrad, args.transmission, args.na)
    axis_case = next((case for case in cases if abs(case["field_deg"]) < 1e-9), cases[0])
    make_l_sweep(axis_case, args.l_sweep_image, args.l_profile_image,
                 args.l_profile_csv, 5.5, 100.0, args.slope, args.b,
                 args.l_sweep_mrad, args.transmission, args.na)
    for case in cases:
        print(f"field={case['field_deg']:g} deg accepted={case['accepted']}/{args.grid**2} "
              f"surface-0 r90={case['rows'][0][3]:.4f} mm "
              f"surface-13 r90={case['rows'][-1][3]:.4f} mm")
    print(f"Scatter image uses Peterson Eq. 8 with B={args.b:g}, S={args.slope:g}, "
          f"L={args.l_mrad:g} mrad, NA={args.na:g}, T={args.transmission:g}; "
          "surface a_j is traced r90 and calibrated per-surface BSDF/TIS is unavailable.")


if __name__ == "__main__":
    main()
