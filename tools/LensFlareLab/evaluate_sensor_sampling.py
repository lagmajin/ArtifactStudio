"""Compare direct diffraction across sensor pixel pitches and check a NumPy reference."""

import argparse
import csv
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_diffraction_supersampled_reference import discrete_pupil_kernel
from evaluate_pupil_psf import read_pfm, to_display, write_pfm


SENSOR_WIDTH_MM = 53.96
SIZES = ((320, 180), (640, 360), (1280, 720), (2560, 1440))
LUMA = np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float64)


def impulse_source(width: int, height: int) -> np.ndarray:
    source = np.zeros((height, width, 3), dtype=np.float64)
    source[height // 2, width // 2] = (4096.0, 4096.0, 4096.0)
    return source


def render(executable: Path, source: np.ndarray, output: Path,
           diffraction_resolution: int) -> np.ndarray:
    height, width, _ = source.shape
    input_path = output.with_name(output.stem + "-input.pfm")
    write_pfm(input_path, source)
    subprocess.run([str(executable), str(input_path.resolve()), str(output.resolve()),
                    "--position", "0.5", "0.5", "--threshold", "0", "--gain", "1",
                    "--source-samples", "1", "--ghosts", "off", "--diffraction", "on",
                    "--diffraction-resolution", str(diffraction_resolution),
                    "--diffraction-resampling", "pixel-area", "--spectral-samples", "3",
                    "--aperture-blades", "6"], check=True, stdout=subprocess.DEVNULL)
    result = read_pfm(output)
    if result.shape != (height, width, 3):
        raise ValueError(f"unexpected render size {result.shape}; expected {(height, width, 3)}")
    return result


def radial_metrics(image: np.ndarray, pitch_mm: float,
                   center: tuple[float, float]) -> dict[str, float]:
    luma = np.maximum(image @ LUMA, 0.0)
    height, width = luma.shape
    yy, xx = np.mgrid[:height, :width]
    cx, cy = center
    weights = luma.ravel()
    energy = float(weights.sum())
    radius_px = np.hypot(xx - cx, yy - cy).ravel()
    order = np.argsort(radius_px)
    cumulative = np.cumsum(weights[order])
    result = {"luma_energy": energy}
    nearest_x, nearest_y = int(round(cx)), int(round(cy))
    center_pixels = luma[max(0, nearest_y - 1):nearest_y + 1,
                         max(0, nearest_x - 1):nearest_x + 1]
    result["central_four_pixel_energy_fraction"] = float(center_pixels.sum() / max(energy, 1e-30))
    for fraction, name in ((0.50, "r50"), (0.90, "r90"), (0.99, "r99"),
                           (0.999, "r999")):
        index = min(int(np.searchsorted(cumulative, energy * fraction)), len(order) - 1)
        result[f"{name}_px"] = float(radius_px[order[index]])
        result[f"{name}_sensor_mm"] = result[f"{name}_px"] * pitch_mm
    centroid_x = float((weights * xx.ravel()).sum() / max(energy, 1e-30))
    centroid_y = float((weights * yy.ravel()).sum() / max(energy, 1e-30))
    result["centroid_offset_sensor_mm"] = math.hypot(
        centroid_x - cx, centroid_y - cy) * pitch_mm
    return result


def compare(native: np.ndarray, reference: np.ndarray) -> dict[str, float]:
    result = {}
    ref_energy = reference.sum(axis=(0, 1))
    native_energy = native.sum(axis=(0, 1))
    for channel, name in enumerate(("r", "g", "b")):
        ref = reference[:, :, channel]
        got = native[:, :, channel]
        result[f"{name}_l1_vs_independent_numpy_percent"] = (
            100.0 * float(np.abs(got - ref).sum()) /
            max(float(np.abs(ref).sum()), 1e-30))
        result[f"{name}_energy_delta_vs_numpy_percent"] = (
            100.0 * (native_energy[channel] - ref_energy[channel]) /
            max(float(ref_energy[channel]), 1e-30))
    result["rgb_l1_vs_independent_numpy_percent"] = (
        100.0 * float(np.abs(native - reference).sum()) /
        max(float(np.abs(reference).sum()), 1e-30))
    result["rgb_energy_delta_vs_numpy_percent"] = (
        100.0 * float(native_energy.sum() - ref_energy.sum()) /
        max(float(ref_energy.sum()), 1e-30))
    return result


def independent_kernel_reference(width: int, height: int,
                                  pupil_samples: int) -> np.ndarray:
    reference = np.zeros((height, width, 3), dtype=np.float64)
    wavelengths = (610.0, 550.0, 460.0)
    center_x, center_y = width // 2, height // 2
    for channel, wavelength in enumerate(wavelengths):
        kernel = discrete_pupil_kernel(pupil_samples, wavelength, width,
                                       SENSOR_WIDTH_MM, 100.0, 4.545, 6,
                                       "pixel-area")
        half_y, half_x = kernel.shape[0] // 2, kernel.shape[1] // 2
        left, top = center_x - half_x, center_y - half_y
        reference[top:top + kernel.shape[0], left:left + kernel.shape[1], channel] = (
            kernel * 4096.0)
    return reference


def common_physical_crop(image: np.ndarray, center: tuple[int, int],
                         pitch_mm: float, half_extent_mm: float) -> np.ndarray:
    radius = int(math.ceil(half_extent_mm / pitch_mm))
    cx, cy = (int(round(center[0])), int(round(center[1])))
    left, top = cx - radius, cy - radius
    crop = np.zeros((2 * radius, 2 * radius, 3), dtype=np.float64)
    x0, y0 = max(0, left), max(0, top)
    x1, y1 = min(image.shape[1], left + crop.shape[1]), min(image.shape[0], top + crop.shape[0])
    crop[y0 - top:y1 - top, x0 - left:x1 - left] = image[y0:y1, x0:x1]
    return crop


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/sensor-sampling"))
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/sensor-sampling.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/sensor-sampling.png"))
    parser.add_argument("--diffraction-resolution", type=int, default=1024,
                        choices=(128, 256, 512, 1024, 2048, 4096))
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    images, records = {}, []
    executable = args.renderer.resolve()
    for width, height in SIZES:
        image = render(executable, impulse_source(width, height),
                       args.output_dir / f"psf-{width}x{height}.pfm",
                       args.diffraction_resolution)
        images[width] = image
        reference = independent_kernel_reference(width, height,
                                                 args.diffraction_resolution)
        pitch = SENSOR_WIDTH_MM / width
        center = (width // 2, height // 2)
        row = {"width_px": width, "height_px": height,
               "sensor_width_mm": SENSOR_WIDTH_MM, "pixel_pitch_um": pitch * 1000.0,
               "diffraction_resolution": args.diffraction_resolution,
               **radial_metrics(image, pitch, center),
               **compare(image, reference)}
        records.append(row)
        print(f"{width:4d}x{height:<4d} pitch={pitch * 1000:.2f}um "
              f"r50/r90/r99={row['r50_sensor_mm']:.5f}/"
              f"{row['r90_sensor_mm']:.5f}/{row['r99_sensor_mm']:.5f}mm "
              f"NumPy RGB-L1={row['rgb_l1_vs_independent_numpy_percent']:.4f}%",
              flush=True)

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    tile, label_h, gap = 128, 17, 5
    headers = ("320 × 180", "640 × 360", "1280 × 720", "2560 × 1440")
    plate = Image.new("RGB", (4 * tile + 3 * gap, tile + label_h + gap),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for column, label in enumerate(headers):
        draw.text((column * (tile + gap) + 3, 2), label, fill=(235, 238, 244))
    half_extent = 2.7
    crops = []
    for width, height in SIZES:
        pitch = SENSOR_WIDTH_MM / width
        crop = common_physical_crop(images[width], (width // 2, height // 2),
                                    pitch, half_extent)
        crops.append(Image.fromarray(to_display(crop, 100.0)).resize(
            (tile, tile), Image.Resampling.BOX))
    panels = tuple(crops)
    for column, panel in enumerate(panels):
        plate.paste(panel, (column * (tile + gap), label_h + gap))
    args.image.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
