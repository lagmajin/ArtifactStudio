"""Evaluate wavelength-scaled Harvey scatter on traced prescription surfaces.

This is an exploratory model: it re-traces the Angenieux prescription per
RGB band, applies the limited-band Harvey wavelength law, then sums Peterson
Eq. 8 surface contributions. Lens-specific measured BSDFs are unavailable.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_prescription_surface_scatter as prescription
from evaluate_harvey_spectral_scaling import RGB_BANDS


def write_pfm(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    height, width, _ = image.shape
    with path.open("wb") as stream:
        stream.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        stream.write(np.flipud(image).astype("<f4").tobytes())


def scale_parameters(wavelength_nm, reference_nm, slope, b_ref, l_ref_mrad):
    ratio = wavelength_nm / reference_nm
    return b_ref * ratio ** (-(4.0 + slope)), l_ref_mrad * ratio


def render_band(case, wavelength_nm, slope, b_ref, l_ref_mrad,
                reference_nm, transmission, na, rho, pixel_area_mm2,
                radius_index=3):
    b, l_mrad = scale_parameters(wavelength_nm, reference_nm, slope,
                                 b_ref, l_ref_mrad)
    l_rad = l_mrad * 1e-3
    b0 = b * (100.0 * l_rad) ** slope
    rows = case["rows"]
    entrance_radius = rows[0][radius_index]
    accepted_fraction = case["accepted"] / case["launched"]
    field_transmission = transmission * accepted_fraction
    image = np.zeros_like(rho)
    surface_contributions = []
    for row in rows:
        a_j = row[radius_index]
        if not math.isfinite(a_j) or a_j <= 1e-8 or not math.isfinite(entrance_radius):
            surface_contributions.append(0.0)
            continue
        scatter_angle = na * rho / (l_rad * a_j)
        surface_image = (
            math.pi * field_transmission * na**2 * (entrance_radius / a_j)**2 *
            b0 * (1.0 + scatter_angle**2) ** (slope * .5))
        image += surface_image
        surface_contributions.append(float(surface_image.sum()) * pixel_area_mm2 /
                                     (math.pi * entrance_radius**2))
    return {"b": b, "l_mrad": l_mrad,
            "surface_contributions": surface_contributions,
            "image": image,
            "entrance_radius": entrance_radius,
            "field_transmission": field_transmission}


def make_plate(path, cases, bands, pixel_pitch_um):
    panel, header = 480, 42
    canvas = Image.new("RGB", (2 * panel, 2 * (panel + header)), (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    floor = np.finfo(float).tiny
    for index, case in enumerate(cases):
        field = case["field_deg"]
        rgb = np.stack([bands[(field, name)]["image"] for name, _ in RGB_BANDS], axis=2)
        # Peak-normalize each band to show chromatic shape changes. This is a
        # false-color diagnostic, not radiometric RGB; report absolute relative
        # crop energies beside the panel instead.
        channel_peaks = np.maximum(np.max(rgb, axis=(0, 1)), floor)
        normalized = rgb / channel_peaks[None, None, :]
        log = np.log10(np.maximum(normalized, 1e-3))
        display = np.clip((log + 3.0) / 3.0, 0, 1)
        display = np.power(display, 1.0 / 2.2)
        pixels = np.clip(display * 255.0 + .5, 0, 255).astype(np.uint8)
        tile = Image.fromarray(pixels).resize((panel, panel), Image.Resampling.BOX)
        x, y = (index % 2) * panel, (index // 2) * (panel + header)
        canvas.paste(tile, (x, y + header))
        pixel_area_mm2 = (pixel_pitch_um * 1e-3) ** 2
        crop_fraction = [
            float(bands[(field, name)]["image"].sum()) * pixel_area_mm2 /
            (math.pi * bands[(field, name)]["entrance_radius"]**2)
            for name, _ in RGB_BANDS]
        draw.text((x + 8, y + 6),
            f"{field:g}° field  R/G/B crop=" + "/".join(
                f"{value*100:.3f}%" for value in crop_fraction),
            fill=(235, 238, 244))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def make_profile_plate(path, cases, bands, pixel_pitch_um,
                       scatter_width, scatter_height):
    panel_w, panel_h, header = 500, 390, 32
    canvas = Image.new("RGB", (2 * panel_w, 2 * (panel_h + header)),
                       (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    pitch_mm = pixel_pitch_um * 1e-3
    max_radius = int((min(scatter_width, scatter_height) - 1) * .5)
    radius_extent_mm = max_radius * pitch_mm
    left, right, top, bottom = 55, panel_w - 16, 42, panel_h - 42
    colors = ((255, 110, 104), (111, 223, 160), (110, 174, 255))
    for case_index, case in enumerate(cases):
        field = case["field_deg"]
        x0 = (case_index % 2) * panel_w
        y0 = (case_index // 2) * (panel_h + header)
        draw.text((x0 + 8, y0 + 7),
                  f"{field:g}° field  radial mean / first annulus",
                  fill=(235, 238, 244))
        plot = (x0 + left, y0 + top, x0 + right, y0 + bottom)
        draw.rectangle(plot, outline=(80, 88, 100))
        for decade in (-2, -1, 0):
            y = plot[3] - int((decade + 2) / 2 * (plot[3] - plot[1]))
            draw.line((plot[0], y, plot[2], y), fill=(43, 48, 57))
            draw.text((x0 + 5, y - 6), f"1e{decade}", fill=(150, 158, 170))
        for tick in np.linspace(0.0, radius_extent_mm, 5):
            x = plot[0] + int(tick / radius_extent_mm * (plot[2] - plot[0]))
            draw.line((x, plot[1], x, plot[3]), fill=(37, 42, 50))
            draw.text((x - 8, plot[3] + 5), f"{tick:.2g}", fill=(150, 158, 170))
        for band_index, (name, _) in enumerate(RGB_BANDS):
            image = bands[(field, name)]["image"]
            yy, xx = np.mgrid[:image.shape[0], :image.shape[1]]
            radius_bins = np.floor(np.hypot(xx - (image.shape[1] - 1) * .5,
                                            yy - (image.shape[0] - 1) * .5) + .5).astype(np.int32)
            energy = np.bincount(radius_bins.ravel(), weights=image.ravel())
            counts = np.bincount(radius_bins.ravel())
            profile = energy / np.maximum(counts, 1)
            valid_positive = np.flatnonzero(profile[:max_radius + 1] > 0)
            if not len(valid_positive):
                continue
            peak = float(profile[valid_positive[0]])
            points = []
            for radius_px in range(1, min(max_radius, len(profile) - 1) + 1):
                value = float(profile[radius_px]) / peak
                if value <= 0:
                    continue
                log_value = float(np.clip(math.log10(value), -2.0, 0.0))
                x = plot[0] + int(radius_px * pitch_mm / radius_extent_mm *
                                  (plot[2] - plot[0]))
                y = plot[3] - int((log_value + 2.0) / 2.0 *
                                  (plot[3] - plot[1]))
                points.append((x, y))
            if len(points) > 1:
                draw.line(points, fill=colors[band_index], width=2)
            draw.text((plot[0] + 8 + band_index * 86, plot[1] + 7),
                      name, fill=colors[band_index])
        draw.text((plot[0], y0 + panel_h - 14),
                  "sensor radius (mm)", fill=(200, 207, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def make_contribution_plate(path, cases, bands):
    panel_w, panel_h, header = 500, 390, 32
    canvas = Image.new("RGB", (2 * panel_w, 2 * (panel_h + header)),
                       (18, 21, 26))
    draw = ImageDraw.Draw(canvas)
    left, right, top, bottom = 54, panel_w - 16, 42, panel_h - 42
    colors = ((255, 110, 104), (111, 223, 160), (110, 174, 255))
    surface_count = len(next(iter(bands.values()))["surface_contributions"])
    for case_index, case in enumerate(cases):
        field = case["field_deg"]
        x0 = (case_index % 2) * panel_w
        y0 = (case_index // 2) * (panel_h + header)
        per_band = {}
        for name, _ in RGB_BANDS:
            result = bands[(field, name)]
            fractions = np.asarray(result["surface_contributions"])
            per_band[name] = 100.0 * fractions / max(float(fractions.sum()), 1e-300)
        top5 = [float(np.sort(per_band[name])[-5:].sum()) for name, _ in RGB_BANDS]
        draw.text((x0 + 8, y0 + 7),
            f"{field:g}° field  top-5 R/G/B=" + "/".join(f"{value:.1f}%" for value in top5),
            fill=(235, 238, 244))
        plot = (x0 + left, y0 + top, x0 + right, y0 + bottom)
        draw.rectangle(plot, outline=(80, 88, 100))
        for tick in (0, 5, 10):
            y = plot[3] - int(tick / 10 * (plot[3] - plot[1]))
            draw.line((plot[0], y, plot[2], y), fill=(43, 48, 57))
            draw.text((x0 + 14, y - 6), f"{tick}%", fill=(150, 158, 170))
        baseline = 100.0 / surface_count
        y = plot[3] - int(baseline / 10 * (plot[3] - plot[1]))
        for x in range(plot[0], plot[2], 10):
            draw.line((x, y, min(x + 5, plot[2]), y), fill=(105, 111, 122))
        for band_index, (name, _) in enumerate(RGB_BANDS):
            points = []
            for surface, share in enumerate(per_band[name]):
                x = plot[0] + int(surface / max(surface_count - 1, 1) *
                                  (plot[2] - plot[0]))
                y = plot[3] - int(share / 10 * (plot[3] - plot[1]))
                points.append((x, y))
            if len(points) > 1:
                draw.line(points, fill=colors[band_index], width=2)
            lx = plot[0] + 10 + band_index * 86
            draw.text((lx, plot[1] + 5), name, fill=colors[band_index])
        for surface in range(surface_count):
            x = plot[0] + int(surface / max(surface_count - 1, 1) *
                              (plot[2] - plot[0]))
            if surface in (0, surface_count - 1) or surface % 2 == 0:
                draw.text((x - 3, plot[3] + 4), str(surface),
                          fill=(150, 158, 170))
        draw.text((plot[0], y0 + panel_h - 14), "surface index",
                  fill=(200, 207, 218))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fields", type=float, nargs="+", default=[0, 5, 10, 12])
    parser.add_argument("--grid", type=int, default=32)
    parser.add_argument("--scatter-width", type=int, default=720)
    parser.add_argument("--scatter-height", type=int, default=720)
    parser.add_argument("--pixel-pitch-um", type=float, default=5.5)
    parser.add_argument("--focal-length-mm", type=float, default=100.0)
    parser.add_argument("--reference-wavelength-nm", type=float, default=550.0)
    parser.add_argument("--slope", type=float, default=-1.86)
    parser.add_argument("--b-reference", type=float, default=.36)
    parser.add_argument("--l-reference-mrad", type=float, default=2.04)
    parser.add_argument("--transmission", type=float, default=.8)
    parser.add_argument("--na", type=float, default=1 / 44)
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-spectral-surface-scatter.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-spectral-surface-scatter.png"))
    parser.add_argument("--profile-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-spectral-surface-scatter-profile.png"))
    parser.add_argument("--contribution-image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-surface-scatter-contributions.png"))
    parser.add_argument("--pfm-dir", type=Path,
                        help="optional directory for one linear RGB PFM per field")
    args = parser.parse_args()
    if (args.grid < 4 or args.grid > 256 or
            min(args.scatter_width, args.scatter_height) < 16 or
            min(args.pixel_pitch_um, args.focal_length_mm,
                args.reference_wavelength_nm, args.b_reference,
                args.l_reference_mrad, args.na) <= 0 or
            args.na >= 1 or not 0 < args.transmission <= 1 or
            args.slope >= 0 or abs(args.slope + 2) < 1e-12):
        parser.error("grid/optical parameters must be positive, slope negative and not -2, T in 0..1")

    bundles = {}
    for wavelength_name, wavelength_nm in RGB_BANDS:
        for field in args.fields:
            accepted, rows = prescription.surface_bundle(field, wavelength_nm, args.grid)
            bundles[(field, wavelength_name)] = {
                "field_deg": field, "wavelength_nm": wavelength_nm,
                "accepted": accepted, "launched": args.grid**2, "rows": rows}
    pitch_mm = args.pixel_pitch_um * 1e-3
    axis = (np.arange(args.scatter_width) + .5 - args.scatter_width / 2) * pitch_mm
    xx, yy = np.meshgrid(axis, axis)
    rho = np.hypot(xx, yy)
    bands = {}
    pixel_area = pitch_mm**2
    for field in args.fields:
        for name, wavelength_nm in RGB_BANDS:
            result = render_band(bundles[(field, name)], wavelength_nm,
                args.slope, args.b_reference, args.l_reference_mrad,
                args.reference_wavelength_nm, args.transmission, args.na, rho,
                pixel_area)
            bands[(field, name)] = result

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(("field_deg", "channel", "wavelength_nm", "accepted_rays",
            "launched_rays", "surface_index", "surface_rays", "r50_mm", "r90_mm",
            "r99_mm", "rms_mm", "S", "B_sr-1", "L_mrad", "Peterson_center",
            "surface_crop_fraction", "summed_crop_fraction_all_surfaces",
            "reference_wavelength_nm", "grid", "scatter_width_px",
            "scatter_height_px", "pixel_pitch_um", "focal_length_mm",
            "transmission_assumption", "NA_assumption"))
        for field in args.fields:
            for name, wavelength_nm in RGB_BANDS:
                bundle = bundles[(field, name)]
                result = bands[(field, name)]
                total_crop = float(sum(result["surface_contributions"]))
                b0 = result["b"] * (100 * result["l_mrad"] * 1e-3) ** args.slope
                for index, row in enumerate(bundle["rows"]):
                    surface_crop = result["surface_contributions"][index]
                    a_j = row[3]
                    coefficient = (math.pi * result["field_transmission"] * args.na**2 *
                        (result["entrance_radius"] / a_j)**2 * b0
                        if math.isfinite(a_j) and a_j > 1e-8 else math.nan)
                    writer.writerow((field, name, wavelength_nm, bundle["accepted"],
                        bundle["launched"], row[0], row[1], row[2], row[3], row[4],
                        row[5], args.slope, result["b"], result["l_mrad"],
                        coefficient, surface_crop, total_crop,
                        args.reference_wavelength_nm, args.grid, args.scatter_width,
                        args.scatter_height, args.pixel_pitch_um, args.focal_length_mm,
                        args.transmission, args.na))
                print(f"field={field:g} {name} {wavelength_nm:g}nm "
                      f"accepted={bundle['accepted']}/{bundle['launched']} "
                      f"B={result['b']:.6g} L={result['l_mrad']:.5g}mrad "
                      f"crop={total_crop:.6g}")
            if args.pfm_dir:
                write_pfm(args.pfm_dir / f"field-{field:g}deg.pfm",
                          np.stack([bands[(field, channel)]["image"]
                                    for channel, _ in RGB_BANDS], axis=2))
    make_plate(args.image, [{"field_deg": field} for field in args.fields],
               bands, args.pixel_pitch_um)
    make_profile_plate(args.profile_image,
                       [{"field_deg": field} for field in args.fields],
                       bands, args.pixel_pitch_um, args.scatter_width,
                       args.scatter_height)
    make_contribution_plate(args.contribution_image,
                            [{"field_deg": field} for field in args.fields], bands)
    print(f"wrote {args.csv}, {args.image}, {args.profile_image}, "
          f"and {args.contribution_image}; "
          "spectral display uses log compression")
    print("RGB mapping is illustrative; input spectrum, sensor response, measured per-surface BSDF, "
          "post-scatter clipping, and housing scatter are not calibrated/modelled.")


if __name__ == "__main__":
    main()
