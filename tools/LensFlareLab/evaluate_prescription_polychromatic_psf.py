"""Evaluate wavelength-dependent prescription PSFs in diagnostic RGB bands.

Each band independently traces its dispersive pupil, reference-sphere OPD,
geometric area amplitude, and chief-image location. RGB is a display basis only;
it is not a calibrated source spectrum or camera response.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_direct_transmission_raytrace as prescription
import evaluate_prescription_coherent_psf as coherent
import evaluate_prescription_wavefront as wavefront


FIELD_CASES = wavefront.FIELD_CASES
DEFAULT_WAVELENGTHS_NM = (460.0, 550.0, 610.0)
CHANNELS = {460.0: 2, 550.0: 1, 610.0: 0}


def make_chart(cases, wavelengths, fields, output_path):
    panel = 240
    label_height = 32
    field_width = panel + 8
    image = Image.new("RGB", (len(wavelengths) * field_width,
                               len(fields) * (panel + label_height)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(image)
    for field_index, (field_name, _) in enumerate(fields):
        for wave_index, wavelength_nm in enumerate(wavelengths):
            case = cases[(field_name, wavelength_nm)]
            x0 = wave_index * field_width
            y0 = field_index * (panel + label_height)
            data = np.maximum(case["psf"], 0.0)
            display = np.log1p(data * coherent.LOG_DISPLAY_GAIN)
            display /= max(float(display.max()), 1e-30)
            gray = np.clip(display * 255.0 + 0.5, 0, 255).astype(np.uint8)
            padded = np.zeros((2 * coherent.OUTPUT_RADIUS_PX + 1,
                               2 * coherent.OUTPUT_RADIUS_PX + 1), dtype=np.uint8)
            offset = coherent.OUTPUT_RADIUS_PX - case["crop_radius"]
            padded[offset:offset + gray.shape[0], offset:offset + gray.shape[1]] = gray
            tile = Image.fromarray(padded).resize(
                (panel, panel), Image.Resampling.NEAREST)
            rgb = np.zeros((panel, panel, 3), dtype=np.uint8)
            if wavelength_nm in CHANNELS:
                channel = CHANNELS[wavelength_nm]
            elif len(wavelengths) > 1:
                channel = 2 if wavelength_nm == min(wavelengths) else (
                    0 if wavelength_nm == max(wavelengths) else 1)
            else:
                channel = 1
            rgb[:, :, channel] = np.asarray(tile)
            image.paste(Image.fromarray(rgb), (x0, y0 + label_height))
            angle = next(value for name, value in fields if name == field_name)
            draw.text((x0 + 4, y0 + 4),
                      f"{angle:g} deg  {wavelength_nm:.0f} nm "
                      f"L1 {case['metrics']['shape_l1_vs_ideal_percent']:.2f}%",
                      fill=(235, 238, 244))
            draw.rectangle((x0, y0 + label_height, x0 + panel - 1,
                            y0 + label_height + panel - 1), outline=(62, 68, 78))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ray-samples", type=int, default=256)
    parser.add_argument("--pupil-samples", type=int, default=1024)
    parser.add_argument("--wavelengths-nm", type=float, nargs="+",
                        default=list(DEFAULT_WAVELENGTHS_NM))
    parser.add_argument("--field", action="append",
                        choices=[field for field, _ in FIELD_CASES])
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-polychromatic-psf.csv"))
    parser.add_argument("--pixels-csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-polychromatic-psf-pixels.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/prescription-polychromatic-psf.png"))
    args = parser.parse_args()
    if args.ray_samples < 64 or args.pupil_samples < 256 or args.pupil_samples % 2:
        parser.error("ray-samples must be >=64; pupil-samples must be even and >=256")
    if (not args.wavelengths_nm or any(w <= 0.0 for w in args.wavelengths_nm) or
            len(set(args.wavelengths_nm)) != len(args.wavelengths_nm)):
        parser.error("wavelengths must be positive and unique")

    wavelengths = tuple(args.wavelengths_nm)
    fields = tuple(case for case in FIELD_CASES
                   if args.field is None or case[0] in args.field)
    records, pixel_records, cases = [], [], {}
    for wavelength_nm in wavelengths:
        exit_z, pupil_center, _, magnification_x, _, _ = \
            wavefront.estimate_exit_pupil(wavelength_nm)
        pupil_radius = prescription.STOP_RADIUS * abs(magnification_x)
        pupil_hex_area = 1.5 * math.sqrt(3.0) * pupil_radius**2
        pupil_pitch = 2.60 * pupil_radius / args.pupil_samples
        support_radius_px = (wavelength_nm * 1e-6 * prescription.EFL /
                             (2.0 * pupil_pitch *
                              (coherent.SENSOR_WIDTH_MM / coherent.SENSOR_WIDTH_PX)))
        crop_radius = min(coherent.OUTPUT_RADIUS_PX,
                          math.floor(support_radius_px - 0.5))
        if crop_radius < 1:
            parser.error("pupil-samples is too small to resolve a one-pixel PSF crop")

        for field_name, angle in fields:
            traced = wavefront.trace_field(angle, args.ray_samples,
                                           exit_z, pupil_center, wavelength_nm)
            (pupil_xy, _, directions, _, opd_mm, _, _, _, index_map,
             _, image_point, _) = traced
            map_info = wavefront.pupil_map_jacobian(
                pupil_xy, directions, index_map, 70.0 / args.ray_samples)
            axis, actual_pitch, opd_grid, amplitude_grid, valid = \
                coherent.map_to_regular_pupil(
                    pupil_xy, opd_mm, directions, index_map,
                    map_info["nodal_jacobian"], pupil_center, pupil_radius,
                    args.pupil_samples)
            complex_pupil = np.where(valid, amplitude_grid, 0.0) * \
                np.exp(2j * math.pi * opd_grid * 1e6 / wavelength_nm)
            ideal_pupil = coherent.vectorized_hexagon(axis, pupil_radius).astype(np.complex128)
            image, pixel_scale = coherent.propagate(
                complex_pupil, actual_pitch, wavelength_nm, crop_radius)
            ideal_image, _ = coherent.propagate(
                ideal_pupil, actual_pitch, wavelength_nm, crop_radius)
            metrics = coherent.image_metrics(image, ideal_image)
            opd_rms_um = float(np.sqrt(np.mean(opd_mm * opd_mm)) * 1000.0)
            case = {"field": field_name, "wavelength_nm": wavelength_nm,
                    "psf": image, "metrics": metrics, "crop_radius": crop_radius,
                    "opd_rms_um": opd_rms_um}
            cases[(field_name, wavelength_nm)] = case
            for pixel_y in range(-crop_radius, crop_radius + 1):
                for pixel_x in range(-crop_radius, crop_radius + 1):
                    pixel_records.append({
                        "field": field_name, "wavelength_nm": wavelength_nm,
                        "x_px_from_chief": pixel_x, "y_px_from_chief": pixel_y,
                        "psf_energy": float(image[pixel_y + crop_radius,
                                                  pixel_x + crop_radius]),
                    })
            records.append({
                "field": field_name, "field_angle_deg": angle,
                "wavelength_nm": wavelength_nm, "ray_grid": args.ray_samples,
                "pupil_grid": args.pupil_samples, "valid_rays": len(pupil_xy),
                "chief_image_x_mm": float(image_point[0]),
                "chief_image_y_mm": float(image_point[1]),
                "exit_pupil_z_mm": exit_z,
                "exit_pupil_magnification_x": magnification_x,
                "exit_pupil_area_mm2": map_info["mapped_pupil_area_mm2"],
                "pupil_area_fraction_vs_paraxial": map_info["mapped_pupil_area_mm2"] /
                                                    pupil_hex_area,
                "pupil_foldover_triangle_percent": map_info["foldover_triangle_percent"],
                "pupil_amplitude_median": map_info["amplitude_median"],
                "opd_rms_um": opd_rms_um,
                "opd_rms_waves": opd_rms_um * 1000.0 / wavelength_nm,
                "pixel_scale_in_fft_samples": pixel_scale,
                "fft_angular_support_radius_px": support_radius_px,
                "interpolated_pupil_samples": int(valid.sum()),
                **metrics,
            })
            print(f"{field_name:14s} {wavelength_nm:5.0f} nm "
                  f"rays={len(pupil_xy):5d} area={map_info['mapped_pupil_area_mm2']:.1f} mm^2 "
                  f"chief=({image_point[0]:.3f},{image_point[1]:.3f}) mm "
                  f"OPD RMS={opd_rms_um:.4f} um, "
                  f"L1={metrics['shape_l1_vs_ideal_percent']:.3f}%")

    if set(wavelengths) >= {460.0, 550.0, 610.0}:
        for field_name, _ in fields:
            locations = {row["wavelength_nm"]: (row["chief_image_x_mm"],
                                                 row["chief_image_y_mm"])
                         for row in records if row["field"] == field_name}
            reference = np.asarray(locations[550.0])
            shifts = {f"chief_shift_{int(wavelength)}nm_px":
                      float(np.linalg.norm(np.asarray(locations[wavelength]) - reference) /
                            (coherent.SENSOR_WIDTH_MM / coherent.SENSOR_WIDTH_PX))
                      for wavelength in (460.0, 610.0)}
            for row in records:
                if row["field"] == field_name:
                    row.update(shifts)

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    args.pixels_csv.parent.mkdir(parents=True, exist_ok=True)
    with args.pixels_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(pixel_records[0]))
        writer.writeheader()
        writer.writerows(pixel_records)
    make_chart(cases, wavelengths, fields, args.image)
    print(f"saved {args.csv}, {args.pixels_csv}, and {args.image}")


if __name__ == "__main__":
    main()
