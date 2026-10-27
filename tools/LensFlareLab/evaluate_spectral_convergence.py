"""Sweep reflected-ghost spectral integration against a dense RGB-basis reference.

Example:
  python tools/LensFlareLab/evaluate_spectral_convergence.py \
      tools/LensFlareLab/LensFlareLab.exe --output-dir build/spectral-study

The study uses synthetic neutral point sources. Its dense reference is a
numerical reference for LensFlareLab's RGB-basis reconstruction, not measured
spectral ground truth.
"""

import argparse
import csv
import math
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw


SAMPLES = (3, 7, 15, 31, 61)
POSITIONS = (("center", 0.5, 0.5), ("midfield", 0.4, 0.5), ("offaxis", 0.2, 0.5))
PATHS = (("9-8", 9, 8), ("10-9", 10, 9), ("12-11", 12, 11))
COATINGS = ("off", "on")
WIDTH, HEIGHT = 640, 360
LABEL_HEIGHT = 24


def read_pfm(path):
    with path.open("rb") as image:
        if image.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, image.readline().split())
        scale = float(image.readline())
        payload = image.read()
    count = width * height * 3
    if len(payload) != count * 4 or scale == 0 or not math.isfinite(scale):
        raise ValueError(f"invalid PFM payload: {path}")
    values = struct.unpack(("<" if scale < 0 else ">") + f"{count}f", payload)
    factor = abs(scale)
    # PFM stores rows bottom-to-top.
    return width, height, [tuple(v * factor for v in values[y * width * 3:(y + 1) * width * 3])
                           for y in reversed(range(height))]


def ghost_metrics(source, rendered):
    width, height = source[:2]
    flux = [0.0, 0.0, 0.0]
    x_moment = [0.0, 0.0, 0.0]
    y_moment = [0.0, 0.0, 0.0]
    pixels = []
    for y in range(height):
        row = []
        for x in range(width):
            offset = x * 3
            rgb = tuple(max(0.0, rendered[2][y][offset + c] - source[2][y][offset + c])
                        for c in range(3))
            row.append(rgb)
            for c, value in enumerate(rgb):
                flux[c] += value
                x_moment[c] += x * value
                y_moment[c] += y * value
        pixels.append(row)
    centroid = tuple((x_moment[c] / flux[c], y_moment[c] / flux[c]) if flux[c] > 1e-30
                     else (0.0, 0.0) for c in range(3))
    separation = max(math.hypot(centroid[a][0] - centroid[b][0],
                                centroid[a][1] - centroid[b][1])
                     for a, b in ((0, 1), (1, 2), (0, 2)))
    return flux, separation, pixels


def rgb_l1_percent(candidate, reference):
    denominator = max(sum(reference), 1e-30)
    return 100.0 * sum(abs(candidate[c] - reference[c]) for c in range(3)) / denominator


def display(value, gain):
    mapped = max(0.0, value * gain)
    mapped /= 1.0 + mapped
    srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1.0 / 2.4) - 0.055
    return round(min(1.0, max(0.0, srgb)) * 255)


def save_outputs(output_dir, results, display_gain):
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "spectral-convergence.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("position", "field_u", "field_v", "path", "coating", "bands",
                         "flux_r", "flux_g", "flux_b", "rgb_l1_percent_vs_61",
                         "max_rgb_centroid_separation_px"))
        for key, samples in results.items():
            position, u, v, path, coating = key
            reference = samples[61][0]
            for count in SAMPLES:
                flux, separation, _ = samples[count]
                writer.writerow((position, u, v, path, coating, count, *flux,
                                 rgb_l1_percent(flux, reference), separation))

    row_count = len(results)
    sheet = Image.new("RGB", (WIDTH * len(SAMPLES),
                               row_count * (HEIGHT + LABEL_HEIGHT)), (15, 18, 23))
    draw = ImageDraw.Draw(sheet)
    for row_index, (key, samples) in enumerate(results.items()):
        position, _, _, path, coating = key
        reference = samples[61][0]
        top = row_index * (HEIGHT + LABEL_HEIGHT)
        for column, count in enumerate(SAMPLES):
            flux, _, pixels = samples[count]
            panel = Image.new("RGB", (WIDTH, HEIGHT))
            panel.putdata([tuple(display(value, display_gain) for value in rgb)
                           for pixel_row in pixels for rgb in pixel_row])
            left = column * WIDTH
            sheet.paste(panel, (left, top + LABEL_HEIGHT))
            label = f"{position} {path} {coating} | {count} bands"
            if count != 61:
                label += f" | L1 {rgb_l1_percent(flux, reference):.2f}%"
            else:
                label += " | reference"
            draw.text((left + 5, top + 6), label, fill=(225, 230, 238))
    image_path = output_dir / "spectral-convergence.png"
    sheet.save(image_path)
    print(f"wrote {csv_path}")
    print(f"wrote {image_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="compiled standalone LensFlareLab executable")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--ray-grid", type=int, default=64)
    parser.add_argument("--display-gain", type=float, default=200000.0)
    args = parser.parse_args()
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"LensFlareLab executable not found: {executable}")
    if args.ray_grid < 8 or args.ray_grid > 512 or not math.isfinite(args.display_gain) or args.display_gain <= 0:
        parser.error("ray-grid must be 8..512 and display-gain must be positive")

    lab_dir = Path(__file__).resolve().parent
    fixture_script = lab_dir / "make_evaluation_chart.py"
    results = {}
    with tempfile.TemporaryDirectory(prefix="lensflare-spectrum-") as temporary:
        temporary_dir = Path(temporary)
        for position, u, v in POSITIONS:
            source_path = temporary_dir / f"{position}-source.pfm"
            subprocess.run((sys.executable, str(fixture_script), "--output", str(source_path),
                            "--source-only", "--single-light-position", str(u), str(v),
                            "--single-light-neutral", "--source-radius", "1", "--light-scale", "1024"),
                           check=True, stdout=subprocess.DEVNULL)
            source = read_pfm(source_path)
            for path_name, first, second in PATHS:
                for coating in COATINGS:
                    key = (position, u, v, path_name, coating)
                    samples = {}
                    for count in SAMPLES:
                        output_path = temporary_dir / f"{position}-{path_name}-{coating}-{count}.pfm"
                        subprocess.run((str(executable), str(source_path), str(output_path),
                                        "--position", str(u), str(v), "--ray-grid", str(args.ray_grid),
                                        "--source-samples", "1", "--diffraction", "off",
                                        "--ghost-pair", str(first), str(second), "--coating", coating,
                                        "--spectral-samples", str(count)),
                                       check=True, stdout=subprocess.DEVNULL)
                        samples[count] = ghost_metrics(source, read_pfm(output_path))
                    results[key] = samples

    save_outputs(args.output_dir, results, args.display_gain)
    print("position/path/coating | 7-band RGB L1 | 15-band RGB L1 | 31-band RGB L1")
    for key, samples in results.items():
        reference = samples[61][0]
        errors = [rgb_l1_percent(samples[count][0], reference) for count in (7, 15, 31)]
        print(f"{'/'.join((key[0], key[3], key[4]))}: " +
              " / ".join(f"{error:.3f}%" for error in errors))


if __name__ == "__main__":
    main()
