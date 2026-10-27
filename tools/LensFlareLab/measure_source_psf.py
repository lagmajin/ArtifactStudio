"""Measure direct-source PSF energy and encircled radii from paired PFM images."""

import argparse
import math
import struct
from pathlib import Path


LUMA = (0.2126, 0.7152, 0.0722)
CHANNELS = ("R", "G", "B")


def read_pfm(path: Path):
    with path.open("rb") as image:
        if image.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, image.readline().split())
        scale = float(image.readline())
        if not math.isfinite(scale) or scale == 0.0:
            raise ValueError(f"invalid PFM scale: {path}")
        payload = image.read()
    count = width * height * 3
    if len(payload) != count * 4:
        raise ValueError(f"unexpected PFM payload size: {path}")
    endian = "<" if scale < 0 else ">"
    values = struct.unpack(f"{endian}{count}f", payload)
    factor = abs(scale)
    rows = []
    for y in range(height):
        file_y = height - 1 - y
        start = file_y * width * 3
        rows.append([tuple(values[i + c] * factor for c in range(3))
                     for i in range(start, start + width * 3, 3)])
    return width, height, rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original source PFM")
    parser.add_argument("render", type=Path, help="diffraction-on render PFM")
    parser.add_argument("--threshold", type=float, default=0.85)
    parser.add_argument("--center", type=int, nargs=2, metavar=("X", "Y"),
                        help="PSF center; default is the brightest source pixel")
    args = parser.parse_args()
    source = read_pfm(args.input)
    rendered = read_pfm(args.render)
    if source[:2] != rendered[:2]:
        parser.error("input and rendered dimensions differ")
    width, height = source[:2]
    if args.center:
        center_x, center_y = args.center
    else:
        center_x, center_y, peak = 0, 0, -1.0
        for y, row in enumerate(source[2]):
            for x, pixel in enumerate(row):
                y_value = sum(c * w for c, w in zip(pixel, LUMA))
                if y_value > peak:
                    center_x, center_y, peak = x, y, y_value
    if not (0 <= center_x < width and 0 <= center_y < height):
        parser.error("PSF center must lie inside the image")
    if not math.isfinite(args.threshold) or args.threshold < 0.0:
        parser.error("threshold must be finite and non-negative")

    for channel, label in enumerate(CHANNELS):
        samples = []
        input_energy = 0.0
        for y in range(height):
            for x in range(width):
                original = source[2][y][x]
                luminance = sum(c * w for c, w in zip(original, LUMA))
                excess_fraction = max(0.0, luminance - args.threshold) / max(luminance, 1e-30)
                remaining = original[channel] * (1.0 - excess_fraction)
                input_energy += original[channel] * excess_fraction
                psf = max(0.0, rendered[2][y][x][channel] - remaining)
                if psf <= 0.0:
                    continue
                radius = math.hypot(x - center_x, y - center_y)
                samples.append((radius, psf))
        energy = sum(value for _, value in samples)
        samples.sort(key=lambda item: item[0])
        quantiles = []
        for fraction in (0.5, 0.8, 0.9, 0.99, 0.999):
            target = energy * fraction
            cumulative = 0.0
            radius = 0.0
            for radius, value in samples:
                cumulative += value
                if cumulative >= target:
                    break
            quantiles.append(radius)
        print(f"{label}: inputEnergy={input_energy:.9g} psfEnergy={energy:.9g} "
              f"retained={energy / max(input_energy, 1e-30):.8%} "
              f"r50={quantiles[0]:.3f}px r80={quantiles[1]:.3f}px "
              f"r90={quantiles[2]:.3f}px r99={quantiles[3]:.3f}px "
              f"r99.9={quantiles[4]:.3f}px")


if __name__ == "__main__":
    main()
