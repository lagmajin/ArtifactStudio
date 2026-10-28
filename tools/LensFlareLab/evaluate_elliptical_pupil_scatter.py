"""Compare circular and footprint-aware elliptical pupil-cone scatter images.

The ellipse is a geometric sensitivity model derived from traced surface hit
points. It extends the circular pupil assumption in Peterson Eq. 8 while
preserving each 90%-energy footprint's area and orientation. It is not a
validated anisotropic BSDF or a replacement for non-sequential ray tracing.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_prescription_surface_scatter as prescription


PITCH_UM = 5.5
SLOPE = -1.86
B = 0.36
L_MRAD = 2.04
TRANSMISSION = 0.8
NA = 1.0 / 44.0


def footprint_ellipse(points):
    """Return median center, 90%-coverage covariance ellipse, and source stats."""
    if len(points) < 3:
        return None
    center = np.median(points, axis=0)
    centered = points - center
    covariance = np.cov(centered, rowvar=False)
    eigenvalues, eigenvectors = np.linalg.eigh(covariance)
    eigenvalues = np.maximum(eigenvalues, 1e-18)
    local = centered @ eigenvectors
    mahalanobis = np.sqrt(np.sum(local * local / eigenvalues, axis=1))
    q90 = float(np.quantile(mahalanobis, .9))
    axes = q90 * np.sqrt(eigenvalues)
    radii = np.linalg.norm(centered, axis=1)
    area_radius = math.sqrt(float(axes[0] * axes[1]))
    return {
        "center": center,
        "axes": axes,
        "vectors": eigenvectors,
        "area_radius": area_radius,
        "r90": float(np.quantile(radii, .9)),
        "aspect": float(axes[1] / max(axes[0], 1e-12)),
        "angle_deg": math.degrees(math.atan2(eigenvectors[1, 1],
                                             eigenvectors[0, 1])),
        "points": len(points),
    }


def render_cases(fields, csv_path, image_path, grid):
    size = 720
    pitch_mm = PITCH_UM * 1e-3
    axis = (np.arange(size) + .5 - size / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    phi = np.arctan2(yy, xx)
    b0 = B * (100.0 * L_MRAD * 1e-3) ** SLOPE
    cases, surface_rows = [], []
    for field in fields:
        accepted, stats, hit_points = prescription.surface_bundle(
            field, 550.0, grid, include_hit_points=True)
        ellipses = [footprint_ellipse(points) for points in hit_points]
        valid_ellipses = [item for item in ellipses if item is not None]
        entrance = ellipses[0]
        field_transmission = TRANSMISSION * accepted / grid**2
        circular = np.zeros_like(rho)
        elliptical = np.zeros_like(rho)
        for index, (stat, ellipse) in enumerate(zip(stats, ellipses)):
            if ellipse is None:
                continue
            a_circ = stat[3]
            if not math.isfinite(a_circ) or a_circ <= 1e-8:
                continue
            circular += (math.pi * field_transmission * NA**2 *
                (stats[0][3] / a_circ)**2 * b0 *
                (1.0 + (NA * rho / (L_MRAD * 1e-3 * a_circ))**2)**(SLOPE * .5))

            major_axis = ellipse["vectors"][:, 1]
            angle = phi - math.atan2(major_axis[1], major_axis[0])
            major, minor = ellipse["axes"][1], ellipse["axes"][0]
            directional_radius = 1.0 / np.sqrt(
                np.cos(angle)**2 / major**2 + np.sin(angle)**2 / minor**2)
            a_area = ellipse["area_radius"]
            entrance_area = entrance["area_radius"]
            elliptical += (math.pi * field_transmission * NA**2 *
                (entrance_area / a_area)**2 * b0 *
                (1.0 + (NA * rho / (L_MRAD * 1e-3 * directional_radius))**2)**(SLOPE * .5))
            surface_rows.append({
                "field_deg": field, "surface_index": index,
                "hit_rays": ellipse["points"], "r90_mm": ellipse["r90"],
                "ellipse_minor_mm": minor, "ellipse_major_mm": major,
                "area_equivalent_radius_mm": a_area,
                "major_to_minor_ratio": ellipse["aspect"],
                "major_axis_angle_deg": ellipse["angle_deg"],
                "area_radius_vs_r90_percent": 100.0 * (a_area / ellipse["r90"] - 1.0),
            })
        circ_crop = float(circular.sum() * pitch_mm**2 /
                          (math.pi * stats[0][3]**2))
        ellip_crop = float(elliptical.sum() * pitch_mm**2 /
                           (math.pi * entrance["area_radius"]**2))
        cases.append({"field": field, "accepted": accepted, "circular": circular,
                      "elliptical": elliptical, "circ_crop": circ_crop,
                      "ellip_crop": ellip_crop,
                      "ellipse_stats": valid_ellipses})
        print(f"field={field:g} accepted={accepted}/{grid**2} "
              f"crop circular={circ_crop:.8g} elliptical={ellip_crop:.8g} "
              f"delta={100*(ellip_crop/circ_crop-1):+.3f}% "
              f"median axis ratio={np.median([e['aspect'] for e in valid_ellipses]):.3f}")

    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("field_deg", "accepted_rays", "launched_rays", "grid",
            "wavelength_nm", "pixel_pitch_um", "Harvey_S", "Harvey_B_sr-1",
            "Harvey_L_mrad", "transmission_assumption", "NA_assumption",
            "circular_r90_crop_fraction", "elliptical_crop_fraction",
            "elliptical_delta_vs_circular_percent", "median_axis_ratio",
            "max_axis_ratio", "surfaces_axis_ratio_gt_1p1"))
        for case in cases:
            ratios = [e["aspect"] for e in case["ellipse_stats"]]
            writer.writerow((case["field"], case["accepted"], grid**2, grid,
                550.0, PITCH_UM, SLOPE, B, L_MRAD, TRANSMISSION, NA,
                f"{case['circ_crop']:.12g}", f"{case['ellip_crop']:.12g}",
                f"{100*(case['ellip_crop']/case['circ_crop']-1):.9g}",
                f"{np.median(ratios):.9g}", f"{max(ratios):.9g}",
                sum(ratio > 1.1 for ratio in ratios)))
    surface_csv = csv_path.with_name(csv_path.stem + "-surfaces.csv")
    with surface_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(surface_rows[0]))
        writer.writeheader()
        writer.writerows(surface_rows)

    make_plate(cases, image_path)
    print(f"wrote {csv_path}, {surface_csv}, and {image_path}")


def make_plate(cases, path):
    tile, label_h, gap = 280, 20, 4
    width, height = 3 * tile + 2 * gap, len(cases) * (tile + label_h + gap)
    canvas = Image.new("RGB", (width, height), (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    shared_peak = max(float(max(case["circular"].max(),
                                case["elliptical"].max())) for case in cases)
    for row_index, case in enumerate(cases):
        crop_left = 220
        crop_top = 220
        for column, key, label in ((0, "circular", "circular r90"),
                                   (1, "elliptical", "90% covariance ellipse")):
            x = column * (tile + gap)
            y = row_index * (tile + label_h + gap)
            image = case[key]
            crop = image[crop_top:crop_top + tile, crop_left:crop_left + tile]
            log = np.log10(np.maximum(crop, shared_peak * 1e-3))
            gray = np.clip((log - math.log10(shared_peak * 1e-3)) / 3 * 255,
                           0, 255).astype(np.uint8)
            canvas.paste(Image.merge("RGB", (Image.fromarray(gray),) * 3),
                         (x, y + label_h))
            draw.text((x + 4, y + 3), f"{case['field']:g}° {label}",
                      fill=(235, 238, 244))
        x = 2 * (tile + gap)
        y = row_index * (tile + label_h + gap)
        difference = case["elliptical"][crop_top:crop_top + tile,
                                          crop_left:crop_left + tile] - \
                     case["circular"][crop_top:crop_top + tile,
                                       crop_left:crop_left + tile]
        limit = max(float(np.quantile(np.abs(difference), .9999)), 1e-300)
        normalized = np.clip(difference / limit, -1, 1)
        rgb = np.zeros((tile, tile, 3), dtype=np.uint8)
        rgb[:, :, 0] = np.where(normalized > 0, normalized * 255, 0).astype(np.uint8)
        rgb[:, :, 2] = np.where(normalized < 0, -normalized * 255, 0).astype(np.uint8)
        canvas.paste(Image.fromarray(rgb), (x, y + label_h))
        draw.text((x + 4, y + 3), f"{case['field']:g}° difference, ±99.99% range",
                  fill=(235, 238, 244))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fields", type=float, nargs="+", default=(0, 5, 10, 12))
    parser.add_argument("--grid", type=int, default=64)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/elliptical-pupil-scatter.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/elliptical-pupil-scatter.png"))
    args = parser.parse_args()
    if args.grid < 4 or args.grid > 256:
        parser.error("grid must be in 4..256")
    render_cases(args.fields, args.csv, args.image, args.grid)


if __name__ == "__main__":
    main()
