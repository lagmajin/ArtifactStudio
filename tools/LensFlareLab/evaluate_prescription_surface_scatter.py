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
from evaluate_harvey_scatter_reference import bsdf, calculate_tis


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


def make_scatter_plate(cases, path, pitch_um, focal_mm, slope, b, l_mrad, subpixels):
    """Render the simple summed surface kernels for qualitative field comparison."""
    size = 720
    pitch_mm = pitch_um * 1e-3
    axis = (np.arange(size) + .5 - size / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    cos_theta = focal_mm / np.hypot(focal_mm, rho)
    sin_theta = rho / np.hypot(focal_mm, rho)
    images = []
    tis, _ = calculate_tis(slope, b, l_mrad * 1e-3,
                           b * (100 * l_mrad * 1e-3) ** slope)
    for case in cases:
        acc = np.zeros((size, size), dtype=np.float64)
        # Each surface contributes one equal-weight scatter event here; this
        # deliberately isolates the pupil-cone geometry from unknown coatings.
        for row in case["rows"]:
            radius_mm = row[3]
            if not math.isfinite(radius_mm) or radius_mm <= 1e-8:
                continue
            corner = l_mrad * 1e-3 * math.sqrt(max(radius_mm, 1e-6) /
                                                max(case["rows"][0][3], 1e-6))
            amp = b * (100 * corner) ** slope
            angular = bsdf(np.arctan2(sin_theta, cos_theta), slope, amp, corner)
            acc += angular * cos_theta**3 / focal_mm**2 / max(tis, 1e-12)
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fields", type=float, nargs="+", default=[0, 5, 10, 12])
    parser.add_argument("--wavelength-nm", type=float, default=550)
    parser.add_argument("--grid", type=int, default=48)
    parser.add_argument("--slope", type=float, default=-1.86)
    parser.add_argument("--b", type=float, default=.36)
    parser.add_argument("--l-mrad", type=float, default=2.04)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-bundle-radii.png"))
    parser.add_argument("--scatter-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter-fields.png"))
    args = parser.parse_args()
    if args.grid < 4 or args.grid > 256 or args.wavelength_nm <= 0 or args.l_mrad <= 0:
        parser.error("grid must be 4..256; wavelength and L must be positive")
    cases = []
    for field in args.fields:
        accepted, rows = surface_bundle(field, args.wavelength_nm, args.grid)
        cases.append({"field_deg": field, "accepted": accepted, "rows": rows})
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("field_deg", "accepted_full_path_rays", "surface_index",
                         "surface_ray_count", "bundle_radius_r50_mm", "bundle_radius_r90_mm",
                         "bundle_radius_r99_mm", "bundle_radius_rms_mm"))
        for case in cases:
            for row in case["rows"]:
                writer.writerow((case["field_deg"], case["accepted"], *row))
    make_plate(cases, args.image)
    make_scatter_plate(cases, args.scatter_image, 5.5, 100.0, args.slope,
                       args.b, args.l_mrad, 4)
    for case in cases:
        print(f"field={case['field_deg']:g} deg accepted={case['accepted']}/{args.grid**2} "
              f"surface-0 r90={case['rows'][0][3]:.4f} mm "
              f"surface-13 r90={case['rows'][-1][3]:.4f} mm")
    print("Scatter image uses equal per-surface event weights; no calibrated coating, "
          "contamination, or surface-specific TIS data are available.")


if __name__ == "__main__":
    main()
