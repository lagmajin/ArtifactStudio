"""Build a shared-scale image and radiometric comparison for ghost FrFT cases."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


LUMA = (0.2126, 0.7152, 0.0722)


def read_pfm(path):
    with path.open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        if scale >= 0:
            raise ValueError(f"expected little-endian PFM scale: {path}")
        values = struct.unpack(f"<{width * height * 3}f", source.read())
    rows = [values[y * width * 3:(y + 1) * width * 3] for y in range(height)]
    if scale != -1.0:
        rows = [tuple(value * abs(scale) for value in row) for row in rows]
    return width, height, list(reversed(rows))


def display_rgb(rgb, gain):
    result = []
    for value in rgb:
        mapped = max(0.0, value * gain)
        mapped /= 1.0 + mapped
        srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--case", nargs=4, action="append", required=True,
                        metavar=("LABEL", "INPUT", "GEOMETRIC", "FRFT"))
    parser.add_argument("--columns", type=int, default=3)
    parser.add_argument("--panel-width", type=int, default=160)
    parser.add_argument("--panel-height", type=int, default=90)
    parser.add_argument("--gain", type=float, default=900.0)
    parser.add_argument("--difference-gain", type=float, default=1000.0)
    args = parser.parse_args()
    if args.columns <= 0 or args.panel_width <= 0 or args.panel_height <= 0:
        parser.error("columns and panel dimensions must be positive")

    cell_width = args.panel_width * 3
    heading_height = 22
    cell_height = args.panel_height + heading_height
    rows = math.ceil(len(args.case) / args.columns)
    sheet = Image.new("RGB", (cell_width * args.columns, cell_height * rows), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)

    for case_index, (label, input_name, geometric_name, frft_name) in enumerate(args.case):
        source, geometric, frft = (read_pfm(Path(name))
                                   for name in (input_name, geometric_name, frft_name))
        if source[:2] != geometric[:2] or source[:2] != frft[:2]:
            parser.error(f"PFM dimensions differ in case {label!r}")
        width, height = source[:2]
        geometric_pixels = []
        frft_pixels = []
        difference_pixels = []
        geometric_y = []
        frft_y = []
        for y in range(height):
            source_row = source[2][y]
            geometric_row = geometric[2][y]
            frft_row = frft[2][y]
            for x in range(width):
                index = x * 3
                src = source_row[index:index + 3]
                geo = tuple(max(0.0, geometric_row[index + c] - src[c]) for c in range(3))
                fft = tuple(max(0.0, frft_row[index + c] - src[c]) for c in range(3))
                geometric_pixels.append(display_rgb(geo, args.gain))
                frft_pixels.append(display_rgb(fft, args.gain))
                geo_luma = sum(geo[c] * LUMA[c] for c in range(3))
                fft_luma = sum(fft[c] * LUMA[c] for c in range(3))
                geometric_y.append(geo_luma)
                frft_y.append(fft_luma)
                delta = (fft_luma - geo_luma) * args.difference_gain
                magnitude = min(1.0, abs(delta))
                difference_pixels.append(
                    (round(255 * magnitude), round(10 * magnitude), 0) if delta >= 0 else
                    (0, round(20 * magnitude), round(255 * magnitude)))

        panels = []
        for pixels in (geometric_pixels, frft_pixels, difference_pixels):
            panel = Image.new("RGB", (width, height))
            panel.putdata(pixels)
            panels.append(panel.resize((args.panel_width, args.panel_height), Image.Resampling.LANCZOS))

        column = case_index % args.columns
        row = case_index // args.columns
        x0 = column * cell_width
        y0 = row * cell_height
        draw.text((x0 + 4, y0 + 4), label, fill=(230, 234, 242))
        for panel_index, panel in enumerate(panels):
            px = x0 + panel_index * args.panel_width
            sheet.paste(panel, (px, y0 + heading_height))
            draw.text((px + 3, y0 + heading_height + 2),
                      ("Geometry", "FrFT", "ΔY +red / -blue")[panel_index],
                      fill=(238, 240, 244))

        geo_sum = sum(geometric_y)
        frft_sum = sum(frft_y)
        absolute_delta = sum(abs(a - b) for a, b in zip(geometric_y, frft_y))

        def centroid(values, total):
            x = sum((i % width) * value for i, value in enumerate(values)) / max(total, 1e-30)
            y = sum((i // width) * value for i, value in enumerate(values)) / max(total, 1e-30)
            return x, y

        geo_center = centroid(geometric_y, geo_sum)
        frft_center = centroid(frft_y, frft_sum)
        shift = math.hypot(geo_center[0] - frft_center[0], geo_center[1] - frft_center[1])
        change = (frft_sum / geo_sum - 1.0) * 100.0 if geo_sum > 1e-30 else float("nan")
        print(f"{label}: frameY={geo_sum:.8g}->{frft_sum:.8g} change={change:+.4f}% "
              f"relativeL1={absolute_delta / max(geo_sum, 1e-30):.4%} "
              f"centroidShift={shift:.3f}px")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
