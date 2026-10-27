"""Compare the two-source Angenieux Fig. 10 render at ray grids 192 and 512."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


def read_pfm(path: Path):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        if scale >= 0:
            raise ValueError("expected little-endian PFM")
        values = struct.unpack(f"<{width * height * 3}f", stream.read())
    pixels = []
    for y in range(height):
        row = []
        source_y = height - y - 1
        for x in range(width):
            offset = (source_y * width + x) * 3
            row.append(tuple(values[offset + c] * abs(scale) for c in range(3)))
        pixels.append(row)
    return width, height, pixels


def luma(rgb):
    return 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("grid192", type=Path)
    parser.add_argument("grid512", type=Path)
    parser.add_argument("--output", type=Path, default=Path("tools/LensFlareLab/examples/fig10-grid192-vs-512.png"))
    args = parser.parse_args()
    source = read_pfm(args.source)
    renders = [read_pfm(args.grid192), read_pfm(args.grid512)]
    if any(render[:2] != source[:2] for render in renders):
        parser.error("PFM dimensions must match")
    width, height, source_pixels = source
    flares = [[[
        tuple(max(0.0, render[2][y][x][c] - source_pixels[y][x][c]) for c in range(3))
        for x in range(width)] for y in range(height)] for render in renders]
    luminances = [[luma(pixel) for row in flare for pixel in row] for flare in flares]
    sorted_values = sorted(value for values in luminances for value in values if value > 0.0)
    white = sorted_values[min(len(sorted_values) - 1, int(0.999 * (len(sorted_values) - 1)))]
    contrast = 24.0

    def view(rgb):
        channels = []
        for value in rgb:
            mapped = math.log1p(max(0.0, value) * contrast) / math.log1p(white * contrast)
            mapped = min(1.0, mapped)
            srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
            channels.append(round(255 * min(1.0, max(0.0, srgb))))
        return tuple(channels)

    panels = [Image.new("RGB", (width, height)) for _ in range(3)]
    flux = [sum(values) for values in luminances]
    centroids = []
    for index, flare in enumerate(flares):
        cx = sum(luminances[index][y * width + x] * x for y in range(height) for x in range(width)) / flux[index]
        cy = sum(luminances[index][y * width + x] * y for y in range(height) for x in range(width)) / flux[index]
        centroids.append((cx, cy))
        for y in range(height):
            for x in range(width):
                panels[index].putpixel((x, y), view(flare[y][x]))
    denom = flux[1]
    l1 = sum(abs(a - b) for a, b in zip(luminances[0], luminances[1])) / denom
    for y in range(height):
        for x in range(width):
            delta = luminances[0][y * width + x] - luminances[1][y * width + x]
            strength = min(1.0, abs(delta) / white)
            v = round(255 * strength)
            panels[2].putpixel((x, y), (v, 0, 0) if delta >= 0 else (0, 0, v))

    labels = ("Grid 192", "Grid 512", f"Signed difference | Y L1={l1:.2%}")
    header = 26
    sheet = Image.new("RGB", (width * 3, height + header), (16, 20, 27))
    draw = ImageDraw.Draw(sheet)
    for i, (panel, label) in enumerate(zip(panels, labels)):
        sheet.paste(panel, (i * width, header))
        draw.text((i * width + 8, 7), label, fill=(230, 235, 243))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    for grid, values, center in zip((192, 512), luminances, centroids):
        print(f"grid={grid} sumY={sum(values):.9g} peakY={max(values):.9g} centroid=({center[0]:.3f},{center[1]:.3f})")
    print(f"relativeY_L1={l1:.6%} commonLogWhite={white:.9g} output={args.output}")


if __name__ == "__main__":
    main()
