"""Compare direct-source diffraction wavelength sampling to a 61-band reference."""

import argparse
import csv
import math
import statistics
import subprocess
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


SAMPLES = (3, 7, 15, 31, 61)
COLORS = {
    "neutral": (1.0, 1.0, 1.0),
    "red": (1.0, 0.0, 0.0),
    "green": (0.0, 1.0, 0.0),
    "blue": (0.0, 0.0, 1.0),
}
LUMA = np.array((0.2126, 0.7152, 0.0722), dtype=np.float64)


def write_pfm(path: Path, image: np.ndarray) -> None:
    height, width, _ = image.shape
    with path.open("wb") as output:
        output.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        np.flipud(image.astype("<f4", copy=False)).tofile(output)


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        if not math.isfinite(scale) or scale == 0.0:
            raise ValueError(f"invalid PFM scale: {path}")
        pixels = np.frombuffer(source.read(), dtype="<f4")
    if pixels.size != width * height * 3:
        raise ValueError(f"unexpected PFM payload size: {path}")
    return np.flipud(pixels.reshape(height, width, 3)).astype(np.float64) * abs(scale)


def source_image(color: tuple[float, float, float], width: int = 640,
                 height: int = 360) -> np.ndarray:
    y, x = np.mgrid[:height, :width]
    dx, dy = x - (width - 1) * 0.5, y - (height - 1) * 0.5
    profile = 1024.0 * np.exp(-(dx * dx + dy * dy) / (2.0 * 1.25**2))
    return profile[:, :, None] * np.asarray(color, dtype=np.float64)[None, None, :]


def extract_flare(source: np.ndarray, rendered: np.ndarray,
                  threshold: float = 0.85) -> np.ndarray:
    luminance = source @ LUMA
    removed = np.clip((luminance - threshold) / np.maximum(luminance, 1e-30), 0.0, 1.0)
    remaining = source * (1.0 - removed[:, :, None])
    return rendered - remaining


def image_metrics(candidate: np.ndarray, reference: np.ndarray,
                  source_energy: np.ndarray) -> dict[str, float]:
    reference_energy = reference.sum(axis=(0, 1))
    candidate_energy = candidate.sum(axis=(0, 1))
    positive_reference = np.maximum(reference, 0.0)
    positive_candidate = np.maximum(candidate, 0.0)
    positive_reference_energy = positive_reference.sum(axis=(0, 1))
    positive_candidate_energy = positive_candidate.sum(axis=(0, 1))
    channel_l1 = np.abs(candidate - reference).sum(axis=(0, 1))
    normalized_l1 = np.abs(
        positive_candidate / np.maximum(positive_candidate_energy, 1e-30)[None, None, :] -
        positive_reference / np.maximum(positive_reference_energy, 1e-30)[None, None, :]
    ).sum(axis=(0, 1))
    energy_threshold = 1e-6 * max(float(positive_reference_energy.sum()), 1e-30)
    active = (positive_candidate_energy > energy_threshold) & (
        positive_reference_energy > energy_threshold)
    support_mismatch = (positive_candidate_energy > energy_threshold) ^ (
        positive_reference_energy > energy_threshold)
    channel_shape_error = np.where(active, normalized_l1, np.where(support_mismatch, 1.0, 0.0))
    height, width, _ = candidate.shape
    yy, xx = np.mgrid[:height, :width]
    centroid_error = []
    for channel in np.flatnonzero(active):
        c = (float((positive_candidate[:, :, channel] * xx).sum() /
                   max(positive_candidate_energy[channel], 1e-30)),
             float((positive_candidate[:, :, channel] * yy).sum() /
                   max(positive_candidate_energy[channel], 1e-30)))
        r = (float((positive_reference[:, :, channel] * xx).sum() /
                   max(positive_reference_energy[channel], 1e-30)),
             float((positive_reference[:, :, channel] * yy).sum() /
                   max(positive_reference_energy[channel], 1e-30)))
        centroid_error.append(math.hypot(c[0] - r[0], c[1] - r[1]))
    denom = max(float(reference_energy.sum()), 1e-30)
    source_denom = max(float(source_energy.sum()), 1e-30)
    return {
        "rgb_l1_percent": 100.0 * float(channel_l1.sum()) / denom,
        "max_channel_shape_l1_percent": 100.0 * float(channel_shape_error.max()),
        "channel_support_mismatch_count": int(support_mismatch.sum()),
        "max_common_channel_centroid_error_px": max(centroid_error, default=0.0),
        "rgb_energy_ratio": float(candidate_energy.sum() / denom),
        "rgb_channel_energy_error_percent": 100.0 * float(
            np.abs(candidate_energy - source_energy).sum()) / source_denom,
        "negative_rgb_mass_percent": 100.0 * float(np.maximum(-candidate, 0.0).sum()) /
            source_denom,
    }


def to_display(values: np.ndarray, exposure: float) -> np.ndarray:
    mapped = np.maximum(values, 0.0) * exposure
    mapped = mapped / (1.0 + mapped)
    srgb = np.where(mapped <= 0.0031308, 12.92 * mapped,
                    1.055 * np.power(mapped, 1.0 / 2.4) - 0.055)
    return np.clip(srgb * 255.0 + 0.5, 0, 255).astype(np.uint8)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path, help="compiled LensFlareLab executable")
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/direct-diffraction-spectral"))
    parser.add_argument("--resolution", type=int, default=512,
                        choices=(128, 256, 512, 1024, 2048, 4096))
    parser.add_argument("--timing-repeats", type=int, default=3,
                        help="warm render repetitions; report median milliseconds")
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    if args.timing_repeats < 1:
        parser.error("--timing-repeats must be at least one")
    args.output_dir.mkdir(parents=True, exist_ok=True)

    inputs: dict[str, np.ndarray] = {}
    source_energies: dict[str, np.ndarray] = {}
    flares: dict[tuple[str, int], np.ndarray] = {}
    render_milliseconds: dict[tuple[str, int], float] = {}
    for color_name, color in COLORS.items():
        source = source_image(color)
        inputs[color_name] = source
        source_luminance = source @ LUMA
        source_removed = source * np.clip(
            (source_luminance - 0.85) / np.maximum(source_luminance, 1e-30),
            0.0, 1.0)[:, :, None]
        source_energy = source_removed.sum(axis=(0, 1))
        source_energies[color_name] = source_energy
        input_path = args.output_dir / f"{color_name}-input.pfm"
        write_pfm(input_path, source)
        for sample_count in SAMPLES:
            render_path = args.output_dir / f"{color_name}-{sample_count}-band.pfm"
            command = [str(args.renderer), str(input_path), str(render_path),
                       "--position", "0.5", "0.5", "--threshold", "0.85",
                       "--gain", "1", "--ghosts", "off", "--diffraction", "on",
                       "--diffraction-resolution", str(args.resolution),
                       "--diffraction-resampling", "pixel-area",
                       "--spectral-samples", str(sample_count)]
            timings = []
            for _ in range(args.timing_repeats):
                start_time = time.perf_counter()
                subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
                timings.append((time.perf_counter() - start_time) * 1000.0)
            render_milliseconds[color_name, sample_count] = statistics.median(timings)
            flares[color_name, sample_count] = extract_flare(source, read_pfm(render_path))

    csv_path = args.output_dir / "metrics.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=("source", "samples", "render_ms", *image_metrics(
            flares["neutral", 3], flares["neutral", 61],
            source_energies["neutral"]).keys()))
        writer.writeheader()
        for color_name in COLORS:
            reference = flares[color_name, 61]
            for sample_count in SAMPLES:
                writer.writerow({"source": color_name, "samples": sample_count,
                                 "render_ms": f"{render_milliseconds[color_name, sample_count]:.3f}",
                             **image_metrics(flares[color_name, sample_count], reference,
                                             source_energies[color_name])})

    width, height = 124, 124
    crop_radius = 42
    center_x, center_y = 319, 179
    labels = ("input", "3 band", "7 band", "15 band", "31 band", "61 band", "|3−61|")
    columns, rows = len(labels), len(COLORS)
    gap, header = 5, 18
    plate = Image.new("RGB", (columns * width + (columns - 1) * gap,
                              rows * (height + header) + (rows - 1) * gap), (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for row, color_name in enumerate(COLORS):
        reference = flares[color_name, 61]
        exposures = max(float(np.quantile(reference, 0.9999)), 1e-8)
        panels = [inputs[color_name]] + [flares[color_name, count] for count in SAMPLES]
        panels.append(np.abs(flares[color_name, 3] - reference) * 8.0)
        for column, (label, panel) in enumerate(zip(labels, panels)):
            left = max(0, center_x - crop_radius)
            top = max(0, center_y - crop_radius)
            crop = panel[top:top + 2 * crop_radius, left:left + 2 * crop_radius]
            rgb = to_display(crop, 1.0 / exposures if column != 0 else 1.0 / 1024.0)
            tile = Image.fromarray(rgb).resize((width, height), Image.Resampling.NEAREST)
            x = column * (width + gap)
            y = row * (height + header + gap)
            draw.text((x + 3, y + 2), f"{color_name}: {label}", fill=(225, 230, 238))
            plate.paste(tile, (x, y + header))

    image_path = args.output_dir / "spectral-convergence.png"
    plate.save(image_path)
    print(f"saved {csv_path} and {image_path}")
    with csv_path.open(encoding="utf-8", newline="") as source:
        for row in csv.DictReader(source):
            if row["samples"] in ("3", "7", "15", "31", "61"):
                print(f"{row['source']:7s} {row['samples']:>2s} bands "
                      f"RGB-L1={float(row['rgb_l1_percent']):.3f}% "
                      f"shape-L1={float(row['max_channel_shape_l1_percent']):.3f}% "
                      f"centroid={float(row['max_common_channel_centroid_error_px']):.4f}px "
                      f"energy-error={float(row['rgb_channel_energy_error_percent']):.4f}% "
                      f"negative-mass={float(row['negative_rgb_mass_percent']):.4f}% "
                      f"render={float(row['render_ms']):.0f}ms")


if __name__ == "__main__":
    main()
