"""Compare direct-source PSF shape across emitter size and sensor position.

Outputs are judged against a numerical convolution reference formed from the
same measured point-source PSF and the same finite source footprint.
"""

import argparse
import csv
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_pupil_psf import read_pfm, write_pfm, to_display


WIDTH, HEIGHT = 640, 360
LUMA = np.array((0.2126, 0.7152, 0.0722))


def source_fixture(center_x: float, center_y: float, sigma: float) -> np.ndarray:
    y, x = np.mgrid[:HEIGHT, :WIDTH]
    if sigma <= 0.0:
        value = np.zeros((HEIGHT, WIDTH), dtype=np.float64)
        value[int(round(center_y)), int(round(center_x))] = 4096.0
        return np.repeat(value[:, :, None], 3, axis=2)
    value = 4096.0 * np.exp(-((x - center_x) ** 2 + (y - center_y) ** 2) /
                            (2.0 * sigma ** 2))
    return np.repeat(value[:, :, None], 3, axis=2)


def full_discrete_source_energy(sigma: float) -> float:
    extent = int(math.ceil(8.0 * sigma))
    offsets = np.arange(-extent, extent + 1, dtype=np.float64)
    one_dimensional = np.exp(-(offsets * offsets) / (2.0 * sigma * sigma)).sum()
    return 4096.0 * one_dimensional * one_dimensional


def render(executable: Path, source: np.ndarray, path: Path, position: tuple[float, float],
           threshold: float, resolution: int, samples: int = 1) -> np.ndarray:
    input_path = path.with_name(path.stem + "-input.pfm")
    write_pfm(input_path, source)
    command = [str(executable), str(input_path.resolve()), str(path.resolve()),
               "--position", f"{position[0]:.9g}", f"{position[1]:.9g}",
               "--threshold", f"{threshold:.9g}", "--gain", "1",
               "--source-samples", str(samples), "--ghosts", "off", "--diffraction", "on",
               "--diffraction-resolution", str(resolution), "--diffraction-resampling", "pixel-area",
               "--spectral-samples", "3", "--aperture-blades", "6"]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    rendered = read_pfm(path)
    source_y = source @ LUMA
    excess = np.maximum(source_y - threshold, 0.0)
    fraction = excess / np.maximum(source_y, 1e-30)
    return rendered - source + source * fraction[:, :, None]


def metrics(image: np.ndarray, reference: np.ndarray, source_energy: float,
            expected_center: tuple[float, float], visible_source_energy: float) -> dict[str, float]:
    y = image @ LUMA
    ref_y = reference @ LUMA
    energy = float(y.sum())
    ref_energy = float(ref_y.sum())
    yy, xx = np.mgrid[:HEIGHT, :WIDTH]
    cx = float((y * xx).sum() / max(energy, 1e-30))
    cy = float((y * yy).sum() / max(energy, 1e-30))
    reference_cx = float((ref_y * xx).sum() / max(ref_energy, 1e-30))
    reference_cy = float((ref_y * yy).sum() / max(ref_energy, 1e-30))
    center_x, center_y = expected_center
    radius = np.hypot(xx - center_x, yy - center_y).ravel()
    weights = np.maximum(y, 0.0).ravel()
    order = np.argsort(radius)
    cumulative = np.cumsum(weights[order])

    def encircled(fraction: float) -> float:
        index = min(int(np.searchsorted(cumulative, cumulative[-1] * fraction)), len(order) - 1)
        return float(radius[order[index]])

    return {
        "visible_source_fraction": visible_source_energy / max(source_energy, 1e-30),
        "psf_throughput_to_full_source": energy / max(source_energy, 1e-30),
        "psf_throughput_to_visible_source": energy / max(visible_source_energy, 1e-30),
        "centroid_x_px": cx,
        "centroid_y_px": cy,
        "centroid_shift_from_reference_px": math.hypot(cx - reference_cx, cy - reference_cy),
        "centroid_offset_from_source_px": math.hypot(cx - center_x, cy - center_y),
        "r50_from_source_px": encircled(.50),
        "r90_from_source_px": encircled(.90),
        "r99_from_source_px": encircled(.99),
        "rgb_l1_vs_convolution_percent": 100.0 * float(np.abs(image - reference).sum()) /
            max(float(np.abs(reference).sum()), 1e-30),
    }


def convolve_reference(source: np.ndarray, point_kernel: np.ndarray) -> np.ndarray:
    from scipy.signal import fftconvolve

    result = np.empty_like(source)
    for channel in range(3):
        result[:, :, channel] = fftconvolve(source[:, :, channel], point_kernel[:, :, channel],
                                            mode="same")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/source-field-psf"))
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/source-field-psf.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/source-field-psf.png"))
    parser.add_argument("--diffraction-resolution", type=int, default=1024,
                        choices=(512, 1024, 2048, 4096))
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    executable = args.renderer.resolve()
    cases = (("center", 320.0, 180.0), ("left-edge", 4.0, 180.0),
             ("right-edge", 635.0, 180.0))
    emitters = (("near-point", 0.7), ("extended-3px", 3.0), ("extended-7px", 7.0))
    records = []
    plates = []
    for field, cx, cy in cases:
        point = source_fixture(320.0, 180.0, 0.0)
        point_output = args.output_dir / "reference-point-render.pfm"
        point_psf = render(executable, point, point_output, (320 / 639, 180 / 359), 0.0,
                           args.diffraction_resolution)
        point_kernel = point_psf[120:241, 260:381] / 4096.0
        for name, sigma in emitters:
            source = source_fixture(cx, cy, sigma)
            threshold = 0.0
            output = args.output_dir / f"{field}-{name}-render.pfm"
            position = (cx / (WIDTH - 1), cy / (HEIGHT - 1))
            image = render(executable, source, output, position, threshold,
                           args.diffraction_resolution)
            visible_source_energy = float((source @ LUMA).sum())
            source_energy = full_discrete_source_energy(sigma)
            source_excess = source_fixture(cx, cy, sigma)
            reference = convolve_reference(source_excess, point_kernel)
            # Crop to the detector after convolution; do not renormalize energy
            # lost outside the image boundary.
            values = metrics(image, reference, source_energy, (cx, cy), visible_source_energy)
            values.update({"field": field, "emitter": name, "sigma_px": sigma,
                           "diffraction_resolution": args.diffraction_resolution})
            records.append(values)
            print(f"{field:10s} {name:14s} PSF={values['psf_throughput_to_full_source']:.6f} "
                  f"centroidShift={values['centroid_shift_from_reference_px']:.4f}px "
                  f"r90={values['r90_from_source_px']:.2f}px "
                  f"L1={values['rgb_l1_vs_convolution_percent']:.3f}%", flush=True)
            plates.append((field, name, sigma, image, reference, (cx, cy)))

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    tile, header, row_header, gap = 128, 18, 16, 5
    plate = Image.new("RGB", (4 * tile + 3 * gap,
                              header + len(plates) * (tile + row_header + gap)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for column, label in enumerate(("Input", "LensFlareLab", "Linear convolution", "Difference ×1000")):
        draw.text((column * (tile + gap) + 4, 3), label, fill=(235, 238, 244))
    for row, (field, emitter, sigma, image, reference, center) in enumerate(plates):
        crop_left = int(round(center[0])) - 64
        crop_top = int(round(center[1])) - 64
        top = header + row * (tile + row_header + gap)
        draw.text((4, top + 2), f"{field} / {emitter} (σ={sigma:g}px)", fill=(225, 230, 238))
        for column, values in enumerate((image, reference, np.abs(image - reference) * 1000.0)):
            left = column * (tile + gap)
            x0, y0 = max(0, crop_left), max(0, crop_top)
            crop = values[y0:y0 + 128, x0:x0 + 128]
            if crop.shape[:2] != (128, 128):
                padded = np.zeros((128, 128, 3), dtype=np.float64)
                padded[:crop.shape[0], :crop.shape[1]] = crop
                crop = padded
            gain = 80.0 if column == 0 else (1.0 / 4096.0 if column == 1 else 1.0e3)
            tile_image = Image.fromarray(to_display(crop, gain))
            plate.paste(tile_image, (left, top + row_header))
        left = 3 * (tile + gap)
        source = source_fixture(*center, sigma)
        crop = source[max(0, int(center[1]) - 64):max(0, int(center[1]) - 64) + 128,
                      max(0, int(center[0]) - 64):max(0, int(center[0]) - 64) + 128]
        if crop.shape[:2] != (128, 128):
            padded = np.zeros((128, 128, 3), dtype=np.float64)
            padded[:crop.shape[0], :crop.shape[1]] = crop
            crop = padded
        plate.paste(Image.fromarray(to_display(crop, 1.0 / 4096.0)), (left, top + row_header))
    args.image.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
