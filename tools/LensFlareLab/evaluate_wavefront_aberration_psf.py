"""Evaluate polychromatic aperture PSFs with controlled pupil wavefront error.

The pupil function is the ideal six-blade stop multiplied by exp(i 2πW/λ).
Wavefront modes are controlled synthetic fixtures, not fitted lens data.
"""

import argparse
import csv
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.signal import fftconvolve


WIDTH, HEIGHT = 640, 360
SENSOR_WIDTH_MM = 53.96
FOCAL_LENGTH_MM = 100.0
PUPIL_RADIUS_MM = 4.545
WAVELENGTHS_NM = (610.0, 550.0, 460.0)
CHANNELS = (0, 1, 2)
MODES = (
    ("Ideal", {}),
    ("Defocus", {"defocus": 0.25}),
    ("Astigmatism", {"astigmatism": 0.25}),
    ("Coma", {"coma": 0.25}),
    ("Spherical", {"spherical": 0.25}),
    ("Mixed", {"defocus": 0.08, "astigmatism": 0.10,
                "coma": 0.12, "spherical": 0.08}),
)


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        data = np.frombuffer(stream.read(), dtype="<f4")
    if data.size != width * height * 3 or not math.isfinite(scale) or scale == 0:
        raise ValueError(f"invalid PFM payload: {path}")
    return np.flipud(data.reshape(height, width, 3)).astype(np.float64) * abs(scale)


def write_pfm(path: Path, image: np.ndarray) -> None:
    height, width, _ = image.shape
    with path.open("wb") as stream:
        stream.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        np.flipud(image.astype("<f4", copy=False)).tofile(stream)


def aperture_mask(size: int, blades: int = 6) -> np.ndarray:
    coordinates = ((np.arange(size, dtype=np.float64) + 0.5) * 2.0 / size) - 1.0
    x, y = np.meshgrid(coordinates, coordinates)
    sector = 2.0 * math.pi / blades
    angle = np.remainder(np.arctan2(y, x) + 0.5 * sector, sector) - 0.5 * sector
    apothem_ratio = math.cos(0.5 * sector)
    radius = np.hypot(x, y)
    boundary = apothem_ratio / np.cos(angle)
    return (radius <= boundary).astype(np.float64)


def wavefront_modes(size: int) -> dict[str, np.ndarray]:
    coordinates = ((np.arange(size, dtype=np.float64) + 0.5) * 2.0 / size) - 1.0
    x, y = np.meshgrid(coordinates, coordinates)
    radius = np.hypot(x, y)
    theta = np.arctan2(y, x)
    r2 = x * x + y * y
    # Primary Noll-index Zernike shapes: j=4 defocus, j=5 astigmatism,
    # j=8 coma, and j=11 primary spherical aberration.
    return {
        "defocus": math.sqrt(3.0) * (2.0 * r2 - 1.0),
        "astigmatism": math.sqrt(6.0) * r2 * np.sin(2.0 * theta),
        "coma": math.sqrt(8.0) * (3.0 * radius**3 - 2.0 * radius) * np.cos(theta),
        "spherical": math.sqrt(5.0) * (6.0 * r2 * r2 - 6.0 * r2 + 1.0),
    }


def pupil_phase(mask: np.ndarray, modes: dict[str, np.ndarray], coefficients: dict[str, float],
                reference_wavelength_nm: float = 550.0) -> np.ndarray:
    phase_opd_waves = np.zeros(mask.shape, dtype=np.float64)
    weights = mask
    for name, coefficient in coefficients.items():
        mode = modes[name]
        mean = float(np.average(mode, weights=weights))
        centered = mode - mean
        rms = math.sqrt(float(np.average(centered * centered, weights=weights)))
        if rms <= 1e-12:
            continue
        phase_opd_waves += coefficient * centered / rms
    return phase_opd_waves * (reference_wavelength_nm * 1e-6)


def wavefront_rms_waves(mask: np.ndarray, opd_mm: np.ndarray,
                        reference_wavelength_nm: float = 550.0) -> float:
    open_pupil = mask > 0.0
    opd = opd_mm[open_pupil]
    opd = opd - float(opd.mean())
    return float(np.sqrt(np.mean(opd * opd)) * 1e6 / reference_wavelength_nm)


def integrate_detector_pixels(intensity: np.ndarray, wavelength_nm: float,
                              fft_size: int, output_radius: int) -> np.ndarray:
    pixel_scale = (wavelength_nm * 1e-6 * FOCAL_LENGTH_MM * WIDTH /
                   (2.0 * PUPIL_RADIUS_MM * SENSOR_WIDTH_MM))
    x = (np.arange(fft_size, dtype=np.float64) - fft_size // 2) * pixel_scale
    base_x = np.floor(x + 0.5).astype(np.int32)
    left_x = np.maximum(0.0, (base_x - 0.5) - (x - 0.5 * pixel_scale)) / pixel_scale
    right_x = np.maximum(0.0, (x + 0.5 * pixel_scale) - (base_x + 0.5)) / pixel_scale
    extra_x = np.where(left_x > 0.0, base_x - 1, base_x + 1)
    center_x = 1.0 - left_x - right_x
    edge_x = left_x + right_x
    x_edge_index = extra_x
    center_x_index = base_x

    base_y = base_x.copy()
    left_y, right_y = left_x.copy(), right_x.copy()
    extra_y = extra_x.copy()
    center_y, edge_y = center_x.copy(), edge_x.copy()
    y_edge_index = extra_y
    center_y_index = base_y

    target_size = output_radius * 2 + 1
    output = np.zeros((target_size, target_size), dtype=np.float64)
    rows = ((center_y_index, center_y), (y_edge_index, edge_y))
    columns = ((center_x_index, center_x), (x_edge_index, edge_x))
    for row_indices, row_fractions in rows:
        valid_rows = (row_indices >= -output_radius) & (row_indices <= output_radius)
        selected_rows = row_indices[valid_rows] + output_radius
        for column_indices, column_fractions in columns:
            valid_columns = (column_indices >= -output_radius) & (
                column_indices <= output_radius)
            selected_columns = column_indices[valid_columns] + output_radius
            contribution = intensity[np.ix_(valid_rows, valid_columns)] * \
                row_fractions[valid_rows, None] * column_fractions[None, valid_columns]
            np.add.at(output, (selected_rows[:, None], selected_columns[None, :]), contribution)
    return output


def make_psf(mask: np.ndarray, opd_mm: np.ndarray, wavelength_nm: float,
             output_radius: int) -> np.ndarray:
    phase = 2.0 * math.pi * opd_mm * 1e6 / wavelength_nm
    pupil = mask * np.exp(1j * phase)
    field = np.fft.fftshift(np.fft.fft2(pupil))
    intensity = np.abs(field) ** 2
    intensity /= max(float(intensity.sum()), 1e-30)
    return integrate_detector_pixels(intensity, wavelength_nm, mask.shape[0], output_radius)


def encircled_radius(image: np.ndarray, fraction: float) -> float:
    center = image.shape[0] // 2
    y, x = np.mgrid[:image.shape[0], :image.shape[1]]
    radii = np.hypot(x - center, y - center).ravel()
    values = np.maximum(image, 0.0).ravel()
    order = np.argsort(radii)
    cumulative = np.cumsum(values[order])
    if cumulative.size == 0 or cumulative[-1] <= 0.0:
        return 0.0
    index = min(int(np.searchsorted(cumulative, fraction * cumulative[-1])), len(order) - 1)
    return float(radii[order[index]])


def metrics(image: np.ndarray, reference: np.ndarray) -> dict[str, float]:
    center = image.shape[0] // 2
    y, x = np.mgrid[:image.shape[0], :image.shape[1]]
    energy = float(image.sum())
    cx = float((image * x).sum() / max(energy, 1e-30))
    cy = float((image * y).sum() / max(energy, 1e-30))
    return {
        "energy_in_crop": energy,
        "centroid_error_px": math.hypot(cx - center, cy - center),
        "r50_px": encircled_radius(image, 0.50),
        "r90_px": encircled_radius(image, 0.90),
        "r99_px": encircled_radius(image, 0.99),
        "shape_l1_vs_ideal_percent": 100.0 * float(np.abs(image - reference).sum()) /
                                     max(float(reference.sum()), 1e-30),
        "detector_peak_ratio_vs_ideal": float(image.max() / max(reference.max(), 1e-30)),
    }


def display(image: np.ndarray, gain: float) -> np.ndarray:
    value = np.maximum(image, 0.0) * gain
    value = np.log1p(value) / math.log1p(4096.0)
    srgb = np.where(value <= 0.0031308, 12.92 * value,
                    1.055 * np.power(value, 1.0 / 2.4) - 0.055)
    return np.clip(srgb * 255.0 + 0.5, 0, 255).astype(np.uint8)


def run_renderer_reference(renderer: Path, output_dir: Path,
                           source: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    source_path = output_dir / "aberration-reference-source.pfm"
    output_path = output_dir / "aberration-reference-render.pfm"
    write_pfm(source_path, source)
    command = [str(renderer.resolve()), str(source_path.resolve()), str(output_path.resolve()),
               "--position", str(319.5 / 639), str(179.5 / 359), "--threshold", "0",
               "--gain", "1", "--source-samples", "1", "--ghosts", "off",
               "--diffraction", "on", "--diffraction-resolution", "1024",
               "--diffraction-resampling", "pixel-area", "--spectral-samples", "3",
               "--aperture-blades", "6", "--aperture-radius", "4.545"]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    return read_pfm(source_path), read_pfm(output_path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path, help="existing LensFlareLab executable")
    parser.add_argument("--resolution", type=int, default=1024,
                        choices=(256, 512, 1024, 2048))
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/wavefront-aberration"))
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/wavefront-aberration-psf.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/wavefront-aberration-psf.png"))
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    resolution = args.resolution
    mask = aperture_mask(resolution)
    zernikes = wavefront_modes(resolution)
    y, x = np.mgrid[:HEIGHT, :WIDTH]
    source_y = 8192.0 * np.exp(-((x - 319.5) ** 2 + (y - 179.5) ** 2) /
                               (2.0 * 0.7**2))
    source = np.repeat(source_y[:, :, None], 3, axis=2)
    source, renderer_reference = run_renderer_reference(args.renderer, args.output_dir, source)
    crop_radius = max(48, math.ceil(0.5 * resolution *
                                   (WAVELENGTHS_NM[0] * 1e-6 * FOCAL_LENGTH_MM * WIDTH /
                                    (2.0 * PUPIL_RADIUS_MM * SENSOR_WIDTH_MM))) + 2)

    records: list[dict[str, float | str | int]] = []
    plate_rows: list[tuple[str, np.ndarray]] = []
    ideal_by_channel: dict[int, np.ndarray] = {}
    psfs_by_mode: dict[str, dict[int, np.ndarray]] = {}
    rms_by_mode: dict[str, float] = {}
    for label, coefficients in MODES:
        opd_mm = pupil_phase(mask, zernikes, coefficients)
        rms_by_mode[label] = wavefront_rms_waves(mask, opd_mm)
        channel_psfs: dict[int, np.ndarray] = {}
        combined = np.zeros((crop_radius * 2 + 1, crop_radius * 2 + 1, 3),
                            dtype=np.float64)
        for wavelength_nm, channel in zip(WAVELENGTHS_NM, CHANNELS):
            psf = make_psf(mask, opd_mm, wavelength_nm, crop_radius)
            channel_psfs[channel] = psf
            combined[:, :, channel] = psf
            if label == MODES[0][0]:
                ideal_by_channel[channel] = psf
        psfs_by_mode[label] = channel_psfs
        plate_rows.append((label, combined))
        case_index = len(plate_rows) - 1
        write_pfm(args.output_dir / f"psf-{case_index:02d}.pfm", combined)

    for label, coefficients in MODES:
        channel_psfs = psfs_by_mode[label]
        for wavelength_nm, channel in zip(WAVELENGTHS_NM, CHANNELS):
            values = metrics(channel_psfs[channel], ideal_by_channel[channel])
            records.append({"case": label, "pupil_fft_resolution": resolution,
                            "wavefront_rms_waves_at_550nm": rms_by_mode[label],
                            "wavelength_nm": wavelength_nm,
                            "mode_coefficients_rms_waves_at_550nm": str(coefficients), **values})

    # Verify the independent pupil FFT against the existing renderer at zero
    # aberration by convolving the same pixel-integrated PSF with its Gaussian source.
    reference_resolution = 1024
    reference_mask = aperture_mask(reference_resolution)
    reference_modes = wavefront_modes(reference_resolution)
    reference_opd = pupil_phase(reference_mask, reference_modes, {})
    reference_crop_radius = max(48, math.ceil(0.5 * reference_resolution *
        (WAVELENGTHS_NM[0] * 1e-6 * FOCAL_LENGTH_MM * WIDTH /
         (2.0 * PUPIL_RADIUS_MM * SENSOR_WIDTH_MM))) + 2)
    ideal_rgb = np.stack([
        make_psf(reference_mask, reference_opd, wavelength, reference_crop_radius)
        for wavelength in WAVELENGTHS_NM], axis=2)
    expected = np.stack([fftconvolve(source[:, :, channel], ideal_rgb[:, :, channel],
                                     mode="same") for channel in CHANNELS], axis=2)
    difference = renderer_reference - expected
    l1_percent = 100.0 * float(np.abs(difference).sum()) / max(float(np.abs(expected).sum()), 1e-30)
    peak_relative = float(np.max(np.abs(difference))) / max(float(expected.max()), 1e-30)
    if l1_percent > 1.0 or peak_relative > 0.10:
        raise RuntimeError(f"zero-aberration renderer agreement failed: L1={l1_percent:.3f}%, "
                           f"peak-relative={peak_relative:.3f}")

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    tile = 168
    label_height = 22
    panel_header = 14
    gap = 5
    plate = Image.new("RGB", (tile * 3 + gap * 2,
                               len(plate_rows) * (tile + label_height + panel_header + gap)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for row, (label, image) in enumerate(plate_rows):
        top = row * (tile + label_height + panel_header + gap)
        label_text = f"{label} - {rms_by_mode[label]:.3f} waves RMS"
        draw.text((4, top + 3), label_text, fill=(235, 238, 244))
        ideal = plate_rows[0][1]
        core = slice(crop_radius - 8, crop_radius + 9)
        panels = (image, image[core, core, :], np.abs(image[core, core, :] - ideal[core, core, :]))
        panel_titles = ("Full PSF", "Core zoom", "Abs. delta")
        for column, panel in enumerate(panels):
            draw.text((column * (tile + gap) + 4, top + label_height - 1),
                      panel_titles[column], fill=(180, 190, 202))
            if column == 2:
                channel_images = np.stack([display(panel[:, :, c], 50000.0)
                                           for c in range(3)], axis=2)
            else:
                channel_images = np.stack([display(panel[:, :, c], 16000.0)
                                           for c in range(3)], axis=2)
            panel_image = Image.fromarray(channel_images).resize(
                (tile, tile), Image.Resampling.NEAREST)
            plate.paste(panel_image, (column * (tile + gap),
                                      top + label_height + panel_header))
    args.image.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image)

    print(f"zero-aberration agreement against LensFlareLab: RGB L1={l1_percent:.5f}%, "
          f"peak-relative={peak_relative:.5f}%")
    for label, _ in MODES:
        values = [row for row in records if row["case"] == label]
        print(f"{label}: mean detector peak ratio="
              f"{np.mean([row['detector_peak_ratio_vs_ideal'] for row in values]):.4f}, "
              f"mean RGB shape L1={np.mean([row['shape_l1_vs_ideal_percent'] for row in values]):.3f}%")
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
