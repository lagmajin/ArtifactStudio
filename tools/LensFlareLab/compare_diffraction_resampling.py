"""Compare reconstructed direct-source PSFs from two resampling modes."""

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


def reconstruct(source, rendered, threshold):
    result = []
    for source_row, render_row in zip(source[2], rendered[2]):
        row = []
        for original, output in zip(source_row, render_row):
            luminance = sum(c * w for c, w in zip(original, LUMA))
            remaining_fraction = min(1.0, threshold / max(luminance, 1e-30))
            row.append(tuple(max(0.0, output[c] - original[c] * remaining_fraction)
                             for c in range(3)))
        result.append(row)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original source PFM")
    parser.add_argument("tent", type=Path, help="tent-resampled render PFM")
    parser.add_argument("pixel_area", type=Path, help="pixel-area render PFM")
    parser.add_argument("--threshold", type=float, default=0.85)
    parser.add_argument("--center", type=int, nargs=2, metavar=("X", "Y"),
                        help="PSF center; default is the brightest source pixel")
    args = parser.parse_args()
    source, tent_render, area_render = (read_pfm(p) for p in
                                         (args.input, args.tent, args.pixel_area))
    if source[:2] != tent_render[:2] or source[:2] != area_render[:2]:
        parser.error("input and rendered dimensions differ")
    width, height = source[:2]
    if args.center:
        cx, cy = args.center
    else:
        cx, cy, peak = 0, 0, -1.0
        for y, row in enumerate(source[2]):
            for x, pixel in enumerate(row):
                value = sum(c * w for c, w in zip(pixel, LUMA))
                if value > peak:
                    cx, cy, peak = x, y, value
    if not (0 <= cx < width and 0 <= cy < height):
        parser.error("PSF center must lie inside the image")
    tent = reconstruct(source, tent_render, args.threshold)
    area = reconstruct(source, area_render, args.threshold)

    for c, label in enumerate(CHANNELS):
        t = [row[x][c] for row in tent for x in range(width)]
        a = [row[x][c] for row in area for x in range(width)]
        energy_t, energy_a = sum(t), sum(a)
        diff = sum(abs(x - y) for x, y in zip(t, a))
        radius_t, radius_a = [], []
        for y in range(height):
            for x in range(width):
                radius = math.hypot(x - cx, y - cy)
                if tent[y][x][c] > 0:
                    radius_t.append((radius, tent[y][x][c]))
                if area[y][x][c] > 0:
                    radius_a.append((radius, area[y][x][c]))
        def quantile(samples, fraction):
            samples.sort(key=lambda item: item[0])
            target, total = sum(v for _, v in samples) * fraction, 0.0
            for radius, value in samples:
                total += value
                if total >= target:
                    return radius
            return 0.0
        r99_t, r99_a = quantile(radius_t, .99), quantile(radius_a, .99)
        print(f"{label}: tentEnergy={energy_t:.9g} pixelAreaEnergy={energy_a:.9g} "
              f"energyDelta={(energy_a-energy_t)/max(energy_t,1e-30):+.6%} "
              f"PSF_L1={diff/max(energy_t,energy_a,1e-30):.6%} "
              f"r99_tent={r99_t:.3f}px r99_pixelArea={r99_a:.3f}px")


if __name__ == "__main__":
    main()
