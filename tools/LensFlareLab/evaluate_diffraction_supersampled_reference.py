"""Compare pixel-convolved diffraction with an oversampled sensor reference."""

import argparse
import csv
import math
import subprocess
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.signal import fftconvolve

from compare_diffraction_resampling import read_pfm
from evaluate_diffraction_pixel_phase import display, measure, write_source


def aperture_mask(size: int, radius_mm: float, blades: int) -> np.ndarray:
    coordinates = ((np.arange(size, dtype=np.float64) + 0.5) / size * 2.0 - 1.0) * radius_mm
    x, y = np.meshgrid(coordinates, coordinates)
    angle = np.arctan2(y, x)
    sector = 2.0 * math.pi / blades
    wrapped = np.remainder(angle + sector * 0.5, sector) - sector * 0.5
    boundary = radius_mm * math.cos(sector * 0.5) / np.cos(wrapped)
    return (np.hypot(x, y) <= boundary).astype(np.float64)


def rebin_axis(coordinates: np.ndarray, cell_width: float, samples_per_pixel: int):
    left = (coordinates - cell_width * 0.5) * samples_per_pixel
    right = (coordinates + cell_width * 0.5) * samples_per_pixel
    first = np.floor(left + 0.5).astype(np.int32)
    width = cell_width * samples_per_pixel
    first_weight = np.clip((first + 0.5 - left) / width, 0.0, 1.0)
    return first, first_weight, 1.0 - first_weight


def oversampled_psf_kernels(pupil_samples: int, fft_size: int,
                            samples_per_pixel: int, sensor_width_mm: float,
                            image_width: int, focal_length_mm: float,
                            aperture_radius_mm: float, blades: int,
                            max_support_px: float):
    pupil = aperture_mask(pupil_samples, aperture_radius_mm, blades)
    padded = np.zeros((fft_size, fft_size), dtype=np.float64)
    offset = (fft_size - pupil_samples) // 2
    padded[offset:offset + pupil_samples, offset:offset + pupil_samples] = pupil
    field = np.fft.fft2(np.fft.ifftshift(padded))
    intensity = np.abs(np.fft.fftshift(field)) ** 2
    intensity /= intensity.sum()

    subgrid_size = 2048
    center = subgrid_size // 2
    highres_kernels = {}
    source_indices = np.arange(fft_size, dtype=np.float64) - fft_size // 2
    for wavelength_nm in (610.0, 550.0, 460.0):
        pixel_scale = (wavelength_nm * 1e-6 * focal_length_mm * image_width /
                       (2.0 * aperture_radius_mm * sensor_width_mm))
        cell_width = pixel_scale * pupil_samples / fft_size
        coordinates = source_indices * cell_width
        x_index, x_weight_0, x_weight_1 = rebin_axis(
            coordinates, cell_width, samples_per_pixel)
        y_index, y_weight_0, y_weight_1 = rebin_axis(
            coordinates, cell_width, samples_per_pixel)
        keep_x = np.abs(x_index) <= max_support_px * samples_per_pixel + 2
        keep_y = np.abs(y_index) <= max_support_px * samples_per_pixel + 2
        x_index, x_weight_0, x_weight_1 = (
            array[keep_x] for array in (x_index, x_weight_0, x_weight_1))
        y_index, y_weight_0, y_weight_1 = (
            array[keep_y] for array in (y_index, y_weight_0, y_weight_1))
        source = intensity[np.ix_(keep_y, keep_x)]
        flat_kernel = np.zeros(subgrid_size * subgrid_size, dtype=np.float64)
        for yi, yw in ((y_index, y_weight_0), (y_index + 1, y_weight_1)):
            row_indices = (center + yi)[:, None] * subgrid_size
            for xi, xw in ((x_index, x_weight_0), (x_index + 1, x_weight_1)):
                flat_indices = row_indices + (center + xi)[None, :]
                weights = source * yw[:, None] * xw[None, :]
                flat_kernel += np.bincount(flat_indices.ravel(), weights=weights.ravel(),
                                           minlength=flat_kernel.size)
        kernel = flat_kernel.reshape((subgrid_size, subgrid_size))
        kernel /= max(kernel.sum(), 1e-30)
        highres_kernels[wavelength_nm] = kernel
    return highres_kernels


def discrete_pupil_kernel(pupil_samples: int, wavelength_nm: float,
                          image_width: int, sensor_width_mm: float,
                          focal_length_mm: float, aperture_radius_mm: float,
                          blades: int, resampling: str) -> np.ndarray:
    """Independent NumPy version of LensFlareLab's sampled PSF accumulator."""
    pupil = aperture_mask(pupil_samples, aperture_radius_mm, blades)
    intensity = np.abs(np.fft.fftshift(np.fft.fft2(pupil))) ** 2
    intensity /= intensity.sum()
    pixel_scale = (wavelength_nm * 1e-6 * focal_length_mm * image_width /
                   (2.0 * aperture_radius_mm * sensor_width_mm))
    support = math.ceil(pupil_samples * 0.5 * pixel_scale) + 1
    size = 2 * support + 1
    center = size // 2
    indices = np.arange(pupil_samples, dtype=np.float64) - pupil_samples // 2
    positions = indices * pixel_scale
    if resampling == "pixel-area":
        first_x, weight_x0, weight_x1 = rebin_axis(positions, pixel_scale, 1)
        first_y, weight_y0, weight_y1 = rebin_axis(positions, pixel_scale, 1)
    else:
        first_x = np.floor(positions).astype(np.int32)
        fraction_x = positions - first_x
        weight_x0, weight_x1 = 1.0 - fraction_x, fraction_x
        first_y = first_x.copy()
        weight_y0, weight_y1 = weight_x0.copy(), weight_x1.copy()
    flat = np.zeros(size * size, dtype=np.float64)
    for iy, wy in ((first_y, weight_y0), (first_y + 1, weight_y1)):
        target_y = center + iy
        for ix, wx in ((first_x, weight_x0), (first_x + 1, weight_x1)):
            target_x = center + ix
            valid_y = (target_y >= 0) & (target_y < size)
            valid_x = (target_x >= 0) & (target_x < size)
            sample = intensity[np.ix_(valid_y, valid_x)] * wy[valid_y, None] * wx[None, valid_x]
            flat_indices = target_y[valid_y, None] * size + target_x[None, valid_x]
            flat += np.bincount(flat_indices.ravel(), weights=sample.ravel(),
                                minlength=flat.size)
    kernel = flat.reshape((size, size))
    kernel /= max(kernel.sum(), 1e-30)
    return kernel


def discrete_source_reference(source_path: Path, wavelength_nm: float, mode: str,
                              pupil_samples: int, nominal_x: int = 319,
                              nominal_y: int = 179) -> np.ndarray:
    width, height, rows = read_pfm(source_path)
    source = np.asarray(rows, dtype=np.float64)
    kernel = discrete_pupil_kernel(pupil_samples, wavelength_nm, width, 53.96,
                                   100.0, 4.545, 6, mode)
    rendered = fftconvolve(source[:, :, 0], kernel, mode="same")
    radius = 44
    return rendered[nominal_y-radius:nominal_y+radius,
                    nominal_x-radius:nominal_x+radius]


def continuous_source(size: int, center_x_phase: float, center_y_phase: float,
                      radius_px: float) -> np.ndarray:
    center = size // 2
    q = 9
    radius = int(math.ceil(20 * q))
    offsets = np.arange(-radius, radius + 1, dtype=np.float64) / q
    x = offsets - center_x_phase
    y = offsets - center_y_phase
    values = np.exp(-(x[None, :] ** 2 + y[:, None] ** 2) /
                    (0.48 * radius_px * radius_px))
    source = np.zeros((size, size), dtype=np.float64)
    source[center - radius:center + radius + 1,
           center - radius:center + radius + 1] = values
    source /= max(source.sum(), 1e-30)
    return source


def convolve_subpixel(source: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    source_origin = np.fft.ifftshift(source)
    kernel_origin = np.fft.ifftshift(kernel)
    result = np.fft.fftshift(np.fft.ifft2(
        np.fft.fft2(source_origin) * np.fft.fft2(kernel_origin)).real)
    return np.maximum(result, 0.0)


def sensor_crop(subpixel_image: np.ndarray, source_energy: float,
                radius: int = 44, samples_per_pixel: int = 9):
    center = subpixel_image.shape[0] // 2
    first_x = center - radius * samples_per_pixel - samples_per_pixel // 2
    first_y = center - radius * samples_per_pixel - samples_per_pixel // 2
    image = np.zeros((radius * 2, radius * 2), dtype=np.float64)
    for y in range(image.shape[0]):
        source_y = first_y + y * samples_per_pixel
        for x in range(image.shape[1]):
            source_x = first_x + x * samples_per_pixel
            image[y, x] = subpixel_image[
                source_y:source_y + samples_per_pixel,
                source_x:source_x + samples_per_pixel].sum()
    image *= source_energy
    return image


def read_crop(path: Path, center_x: int = 319, center_y: int = 179, radius: int = 44):
    width, height, rows = read_pfm(path)
    left, top = center_x - radius, center_y - radius
    crop = np.asarray(rows[top:top + radius * 2], dtype=np.float64)
    return crop[:, left:left + radius * 2]


def compare(reference: np.ndarray, candidate: np.ndarray,
            center_x: float, center_y: float):
    metrics = []
    for channel in range(3):
        ref = reference[:, :, channel]
        got = candidate[:, :, channel]
        ref_energy, got_energy = float(ref.sum()), float(got.sum())
        l1 = float(np.abs(ref - got).sum() / max(ref_energy, got_energy, 1e-30))
        y, x = np.indices(ref.shape)
        ref_cx = float((ref * x).sum() / max(ref_energy, 1e-30))
        ref_cy = float((ref * y).sum() / max(ref_energy, 1e-30))
        got_cx = float((got * x).sum() / max(got_energy, 1e-30))
        got_cy = float((got * y).sum() / max(got_energy, 1e-30))
        metrics.append((l1, (got_energy - ref_energy) / max(ref_energy, 1e-30),
                        math.hypot(got_cx - ref_cx, got_cy - ref_cy)))
    luma_ref = reference @ np.asarray((0.2126, 0.7152, 0.0722))
    luma_got = candidate @ np.asarray((0.2126, 0.7152, 0.0722))
    _, _, _, r50_ref, r90_ref, r99_ref = measure(
        (reference.shape[1], reference.shape[0],
         [[(float(pixel[0]), float(pixel[1]), float(pixel[2])) for pixel in row]
          for row in reference]), center_x, center_y)
    _, _, _, r50_got, r90_got, r99_got = measure(
        (candidate.shape[1], candidate.shape[0],
         [[(float(pixel[0]), float(pixel[1]), float(pixel[2])) for pixel in row]
          for row in candidate]), center_x, center_y)
    luma_l1 = float(np.abs(luma_ref - luma_got).sum() /
                    max(float(luma_ref.sum()), float(luma_got.sum()), 1e-30))
    return {"rgb_l1_percent": 100 * sum(item[0] for item in metrics) / 3,
            "luma_l1_percent": 100 * luma_l1,
            "max_channel_energy_delta_percent": 100 * max(abs(item[1]) for item in metrics),
            "max_channel_centroid_error_px": max(item[2] for item in metrics),
            "reference_r50_px": r50_ref, "render_r50_px": r50_got,
            "reference_r90_px": r90_ref, "render_r90_px": r90_got,
            "reference_r99_px": r99_ref, "render_r99_px": r99_got}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/supersampled-reference"))
    parser.add_argument("--csv-output", type=Path,
                        default=Path("tools/LensFlareLab/examples/diffraction-supersampled-reference.csv"))
    parser.add_argument("--image-output", type=Path,
                        default=Path("tools/LensFlareLab/examples/diffraction-supersampled-reference.png"))
    parser.add_argument("--phases", type=float, nargs="+", default=(0.0, 0.25, 0.5, 0.75))
    parser.add_argument("--pupil-samples", type=int, default=1024)
    parser.add_argument("--fft-size", type=int, default=4096)
    parser.add_argument("--subpixel-samples", type=int, default=9)
    parser.add_argument("--source-radius", type=float, default=1.0)
    parser.add_argument("--light-peak", type=float, default=18432.0)
    args = parser.parse_args()
    if args.pupil_samples < 128 or args.fft_size < args.pupil_samples or args.fft_size & (args.fft_size - 1):
        parser.error("require pupil samples >=128 and power-of-two FFT size >= pupil samples")
    if args.subpixel_samples != 9:
        parser.error("this reference currently uses exactly 9 subpixel samples per detector pixel")
    if any(not math.isfinite(phase) or not 0 <= phase < 1 for phase in args.phases):
        parser.error("all phases must lie in [0,1)")

    width, height = 640, 360
    nominal_x, nominal_y = 319, 179
    args.output_dir.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    kernels = oversampled_psf_kernels(args.pupil_samples, args.fft_size,
                                      args.subpixel_samples, 53.96, width, 100.0,
                                      4.545, 6, 44.0)
    print(f"built oversampled aperture reference in {time.perf_counter()-started:.2f}s",
          flush=True)
    records, plates = [], []
    executable = args.executable.resolve()
    for phase in sorted(set(args.phases)):
        center_x, center_y = nominal_x + phase, nominal_y + 0.375
        tag = f"phase-{phase:.3f}".replace(".", "p")
        source_path = args.output_dir / f"{tag}-source.pfm"
        source_energy = write_source(source_path, width, height, center_x, center_y,
                                     args.source_radius, args.light_peak)
        continuous = continuous_source(2048, phase, 0.375, args.source_radius)
        reference = np.zeros((88, 88, 3), dtype=np.float64)
        for channel, wavelength in enumerate((610.0, 550.0, 460.0)):
            output = convolve_subpixel(continuous, kernels[wavelength])
            reference[:, :, channel] = sensor_crop(output, source_energy)

        for mode in ("tent", "pixel-area"):
            render_path = args.output_dir / f"{tag}-{mode}.pfm"
            command = [str(executable), str(source_path.resolve()), str(render_path.resolve()),
                       "--position", f"{center_x / (width - 1):.9g}",
                       f"{center_y / (height - 1):.9g}", "--threshold", "0", "--gain", "1",
                       "--source-samples", "1", "--ghosts", "off", "--diffraction", "on",
                       "--diffraction-resolution", str(args.pupil_samples),
                       "--diffraction-resampling", mode, "--aperture-blades", "6",
                       "--aperture-radius", "4.545"]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"LensFlareLab failed for phase {phase:g}/{mode}: "
                                   f"{result.stdout}{result.stderr}")
            candidate = read_crop(render_path)
            fields = compare(reference, candidate, 44.0 + phase, 44.375)
            discrete_channels = [discrete_source_reference(
                source_path, wavelength, mode, args.pupil_samples)
                for wavelength in (610.0, 550.0, 460.0)]
            discrete = np.stack(discrete_channels, axis=2)
            discrete_vs_continuous = compare(reference, discrete, 44.0 + phase, 44.375)
            app_vs_discrete = compare(discrete, candidate, 44.0 + phase, 44.375)
            record = {"phase_x_px": phase, "resampling_mode": mode,
                      "pupil_samples": args.pupil_samples, "reference_fft_size": args.fft_size,
                      "subpixel_samples_per_pixel": args.subpixel_samples,
                      "source_radius_px": args.source_radius,
                      "rgb_l1_percent": fields["rgb_l1_percent"],
                      "luma_l1_percent": fields["luma_l1_percent"],
                      "discrete_source_vs_continuous_rgb_l1_percent":
                          discrete_vs_continuous["rgb_l1_percent"],
                      "app_vs_independent_discrete_rgb_l1_percent":
                          app_vs_discrete["rgb_l1_percent"],
                      "max_channel_energy_delta_percent": fields["max_channel_energy_delta_percent"],
                      "max_channel_centroid_error_px": fields["max_channel_centroid_error_px"],
                      "reference_r50_px": fields["reference_r50_px"],
                      "render_r50_px": fields["render_r50_px"],
                      "reference_r90_px": fields["reference_r90_px"],
                      "render_r90_px": fields["render_r90_px"],
                      "reference_r99_px": fields["reference_r99_px"],
                      "render_r99_px": fields["render_r99_px"]}
            records.append(record)
            plates.append((phase, mode, reference.copy(), candidate.copy()))
            print(f"phase={phase:g} mode={mode} RGB-L1={fields['rgb_l1_percent']:.3f}% "
                  f"discrete-vs-continuous={discrete_vs_continuous['rgb_l1_percent']:.3f}% "
                  f"app-vs-discrete={app_vs_discrete['rgb_l1_percent']:.6f}% "
                  f"Y-L1={fields['luma_l1_percent']:.3f}% "
                  f"maxCentroidError={fields['max_channel_centroid_error_px']:.4f}px "
                  f"r50={fields['reference_r50_px']:.3f}/{fields['render_r50_px']:.3f}px",
                  flush=True)

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    panel_px, scale, columns = 88, 2, 4
    panel_size, title_height, row_label_height = panel_px * scale, 38, 18
    tile_height = title_height + panel_size * 2 + row_label_height
    rows = (len(plates) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * panel_size, rows * tile_height), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for index, (phase, mode, reference, candidate) in enumerate(plates):
        x = (index % columns) * panel_size
        y = (index // columns) * tile_height
        draw.text((x + 4, y + 2), f"phase {phase:g}  {mode}", fill=(235, 238, 244))
        draw.text((x + 4, y + 20), "LensFlareLab", fill=(205, 211, 220))
        for values, panel_y in ((candidate, y + title_height),
                                (reference, y + title_height + panel_size + row_label_height)):
            panel = Image.new("RGB", (panel_px, panel_px))
            for py in range(panel_px):
                for px in range(panel_px):
                    pixel = values[py, px]
                    panel.putpixel((px, py), tuple(display(float(value), 40.0) for value in pixel))
            sheet.paste(panel.resize((panel_size, panel_size), Image.Resampling.NEAREST),
                        (x, panel_y))
        draw.text((x + 4, y + title_height + panel_size + 1),
                  "oversampled 9x-pixel reference", fill=(205, 211, 220))
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.image_output)
    print(f"saved {args.csv_output} and {args.image_output}")


if __name__ == "__main__":
    main()
