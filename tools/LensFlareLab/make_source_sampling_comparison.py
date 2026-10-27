"""Make a radiance and image-difference plate for 1/9/25 ghost-source bins."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


def read_pfm(path: Path):
    with path.open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        if scale >= 0:
            raise ValueError(f"expected little-endian PFM scale: {path}")
        values = struct.unpack(f"<{width * height * 3}f", source.read())
    scale = abs(scale)
    rows = []
    for y in range(height):
        source_y = height - 1 - y
        start = source_y * width * 3
        rows.append([tuple(channel * scale for channel in values[i:i + 3])
                     for i in range(start, start + width * 3, 3)])
    return width, height, rows


def luminance(rgb):
    return rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722


def display(rgb, gain):
    mapped = []
    for value in rgb:
        value = max(0.0, value * gain)
        value = value / (1.0 + value)
        mapped.append(round((12.92 * value if value <= 0.0031308
                             else 1.055 * value ** (1.0 / 2.4) - 0.055) * 255))
    return tuple(mapped)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="input chart PFM")
    parser.add_argument("point", type=Path, help="render with --source-samples 1")
    parser.add_argument("nine", type=Path, help="render with --source-samples 9")
    parser.add_argument("twenty_five", type=Path, help="render with --source-samples 25")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                        default=(0, 0, 640, 360))
    parser.add_argument("--gain", type=float, default=256.0)
    args = parser.parse_args()

    base = read_pfm(args.input)
    renders = [read_pfm(path) for path in (args.point, args.nine, args.twenty_five)]
    if any(render[:2] != base[:2] for render in renders):
        parser.error("all PFM dimensions must match")
    width, height = base[:2]
    left, top, right, bottom = args.crop
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the images")

    labels = ("Point source: 1 direction", "Finite source: 3x3", "Finite source: 5x5",
              "Change: 3x3 to 5x5")
    panels = [Image.new("RGB", (right - left, bottom - top)) for _ in labels]
    flux = [0.0, 0.0, 0.0]
    peak = [0.0, 0.0, 0.0]
    centroid_x = [0.0, 0.0, 0.0]
    centroid_y = [0.0, 0.0, 0.0]
    abs_delta = 0.0
    reference_energy = 0.0
    for y in range(top, bottom):
        for x in range(left, right):
            src = base[2][y][x]
            flares = [tuple(max(0.0, render[2][y][x][c] - src[c]) for c in range(3))
                      for render in renders]
            for i, flare in enumerate(flares):
                value = luminance(flare)
                flux[i] += value
                peak[i] = max(peak[i], value)
                centroid_x[i] += value * x
                centroid_y[i] += value * y
                panels[i].putpixel((x - left, y - top), display(flare, args.gain))
            delta = luminance(flares[2]) - luminance(flares[1])
            abs_delta += abs(delta)
            reference_energy += luminance(flares[2])
            strength = min(1.0, abs(delta) * args.gain)
            value = round(strength * 255)
            panels[3].putpixel((x - left, y - top),
                               (value, round(value * 0.04), round(value * 0.02)) if delta >= 0
                               else (round(value * 0.02), round(value * 0.06), value))

    label_height = 24
    sheet = Image.new("RGB", ((right - left) * len(panels), bottom - top + label_height),
                      (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for i, (panel, label) in enumerate(zip(panels, labels)):
        x = i * panel.width
        sheet.paste(panel, (x, label_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    for count in range(3):
        print(f"samples={1 if count == 0 else 9 if count == 1 else 25} "
              f"integratedY={flux[count]:.9g} peakY={peak[count]:.9g} "
              f"centroid=({centroid_x[count] / flux[count]:.5f},"
              f"{centroid_y[count] / flux[count]:.5f})")
    print(f"5x5-vs-3x3 relativeL1={abs_delta / reference_energy:.6%} "
          f"absoluteL1={abs_delta:.9g}; saved {args.output}")


if __name__ == "__main__":
    main()
