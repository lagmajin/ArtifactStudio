"""Compare reflected-ghost images rendered at different pupil ray-grid sizes."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


def read_pfm(path):
    with Path(path).open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        if scale >= 0:
            raise ValueError(f"expected little-endian PFM scale: {path}")
        values = struct.unpack(f"<{width * height * 3}f", source.read())
    rows = []
    for y in range(height):
        start = (height - y - 1) * width * 3
        rows.append([tuple(value * abs(scale) for value in values[i:i + 3])
                     for i in range(start, start + width * 3, 3)])
    return width, height, rows


def luma(rgb):
    return rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722


def display(rgb, gain):
    result = []
    for channel in rgb:
        value = max(0.0, channel * gain)
        value /= 1.0 + value
        srgb = 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def parse_source_samples(value):
    try:
        count = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected an integer sample count") from error
    axis = math.isqrt(count) if count > 0 else 0
    if count == 1 or (9 <= count <= 225 and axis % 2 == 1 and axis * axis == count):
        return count
    raise argparse.ArgumentTypeError("expected 1 or an odd square sample count from 9 to 225")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("grid64", type=Path)
    parser.add_argument("grid96", type=Path)
    parser.add_argument("grid128", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                        default=(0, 0, 640, 360))
    parser.add_argument("--gain", type=float, default=256.0)
    parser.add_argument("--difference-gain", type=float, default=256.0,
                        help="display gain used only for signed image differences")
    parser.add_argument("--source-samples", type=parse_source_samples, default=9,
                        help="source-angle bin count used for all three renders")
    args = parser.parse_args()

    base = read_pfm(args.input)
    renders = [read_pfm(path) for path in (args.grid64, args.grid96, args.grid128)]
    if any(render[:2] != base[:2] for render in renders):
        parser.error("all PFM dimensions must match")
    width, height = base[:2]
    left, top, right, bottom = args.crop
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the images")

    suffix = f"{args.source_samples} source directions"
    labels = (f"Pupil ray grid 64 ({suffix})", f"Pupil ray grid 96 ({suffix})",
              f"Pupil ray grid 128 ({suffix})", "64 vs 128", "96 vs 128")
    panel_width, panel_height = right - left, bottom - top
    panels = [Image.new("RGB", (panel_width, panel_height)) for _ in labels]
    flux = [0.0, 0.0, 0.0]
    peak = [0.0, 0.0, 0.0]
    cx = [0.0, 0.0, 0.0]
    cy = [0.0, 0.0, 0.0]
    absolute_difference = [0.0, 0.0]
    reference_energy = 0.0

    for y in range(top, bottom):
        for x in range(left, right):
            source = base[2][y][x]
            flare = [tuple(max(0.0, render[2][y][x][c] - source[c]) for c in range(3))
                     for render in renders]
            values = [luma(pixel) for pixel in flare]
            for index, rgb in enumerate(flare):
                value = values[index]
                flux[index] += value
                peak[index] = max(peak[index], value)
                cx[index] += value * x
                cy[index] += value * y
                panels[index].putpixel((x - left, y - top), display(rgb, args.gain))
            reference_energy += values[2]
            for index, source_index in enumerate((0, 1)):
                delta = values[source_index] - values[2]
                absolute_difference[index] += abs(delta)
                strength = min(1.0, abs(delta) * args.difference_gain)
                value = round(strength * 255)
                panels[3 + index].putpixel(
                    (x - left, y - top),
                    (value, round(value * 0.04), round(value * 0.02)) if delta >= 0
                    else (round(value * 0.02), round(value * 0.06), value))

    label_height = 24
    sheet = Image.new("RGB", (panel_width * len(panels), panel_height + label_height), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for index, (panel, label) in enumerate(zip(panels, labels)):
        x = index * panel_width
        sheet.paste(panel, (x, label_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)

    for index, grid in enumerate((64, 96, 128)):
        print(f"grid={grid} integratedY={flux[index]:.9g} peakY={peak[index]:.9g} "
              f"centroid=({cx[index] / flux[index]:.5f},{cy[index] / flux[index]:.5f})")
    print(f"grid64-vs-128 relativeL1={absolute_difference[0] / reference_energy:.6%}; "
          f"grid96-vs-128 relativeL1={absolute_difference[1] / reference_energy:.6%}; "
          f"saved {args.output}")


if __name__ == "__main__":
    main()
