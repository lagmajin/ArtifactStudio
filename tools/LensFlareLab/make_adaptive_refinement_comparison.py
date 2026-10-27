"""Compare adaptive pupil refinement and uniform ray grids against a dense PFM."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


LUMA = (0.2126, 0.7152, 0.0722)


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


def luminance(rgb):
    return sum(channel * weight for channel, weight in zip(rgb, LUMA))


def display(rgb, gain):
    result = []
    for channel in rgb:
        value = max(0.0, channel * gain)
        value /= 1.0 + value
        srgb = 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="isolated HDR source image")
    parser.add_argument("reference", type=Path, help="dense reference render")
    parser.add_argument("output", type=Path)
    parser.add_argument("--case", action="append", nargs=2, required=True,
                        metavar=("LABEL", "PFM"), help="candidate render; repeatable")
    parser.add_argument("--gain", type=float, default=300000.0)
    parser.add_argument("--difference-gain", type=float, default=1000000.0)
    parser.add_argument("--panel-width", type=int, default=192)
    parser.add_argument("--panel-height", type=int, default=108)
    args = parser.parse_args()

    source, reference = read_pfm(args.input), read_pfm(args.reference)
    if source[:2] != reference[:2]:
        parser.error("input and reference dimensions differ")
    candidates = [(label, read_pfm(path)) for label, path in args.case]
    if any(image[:2] != source[:2] for _, image in candidates):
        parser.error("candidate dimensions differ from input")

    width, height = source[:2]
    ref_y = []
    for y in range(height):
        ref_y.append([max(0.0, luminance(reference[2][y][x]) -
                          luminance(source[2][y][x])) for x in range(width)])
    ref_flux = sum(map(sum, ref_y))
    ref_peak = max(map(max, ref_y))
    ref_cx = sum(x * value for y, row in enumerate(ref_y) for x, value in enumerate(row)) / ref_flux
    ref_cy = sum(y * value for y, row in enumerate(ref_y) for value in row) / ref_flux

    columns = min(3, len(candidates))
    rows = math.ceil(len(candidates) / columns)
    heading = 22
    panel_w, panel_h = args.panel_width, args.panel_height
    cell_w, cell_h = panel_w * 2, panel_h + heading
    sheet = Image.new("RGB", (cell_w * columns, cell_h * rows), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for index, (label, image) in enumerate(candidates):
        candidate_y = []
        total_abs = 0.0
        candidate_flux = 0.0
        candidate_peak = 0.0
        candidate_cx = candidate_cy = 0.0
        for y in range(height):
            row = []
            for x in range(width):
                actual = max(0.0, luminance(image[2][y][x]) - luminance(source[2][y][x]))
                row.append(actual)
                candidate_flux += actual
                candidate_peak = max(candidate_peak, actual)
                candidate_cx += x * actual
                candidate_cy += y * actual
                total_abs += abs(actual - ref_y[y][x])
            candidate_y.append(row)
        l1 = total_abs / max(ref_flux, 1e-30)
        cx, cy = candidate_cx / max(candidate_flux, 1e-30), candidate_cy / max(candidate_flux, 1e-30)
        centroid_shift = math.hypot(cx - ref_cx, cy - ref_cy)

        render = Image.new("RGB", (width, height))
        difference = Image.new("RGB", (width, height))
        render_pixels, difference_pixels = [], []
        for y in range(height):
            for x in range(width):
                ghost_rgb = tuple(max(0.0, image[2][y][x][c] - source[2][y][x][c])
                                  for c in range(3))
                render_pixels.append(display(ghost_rgb, args.gain))
                delta = candidate_y[y][x] - ref_y[y][x]
                mag = min(1.0, abs(delta) * args.difference_gain)
                difference_pixels.append((round(255 * mag), round(10 * mag), 0) if delta >= 0
                                         else (0, round(20 * mag), round(255 * mag)))
        render.putdata(render_pixels)
        difference.putdata(difference_pixels)
        render = render.resize((panel_w, panel_h), Image.Resampling.LANCZOS)
        difference = difference.resize((panel_w, panel_h), Image.Resampling.LANCZOS)
        col, row = index % columns, index // columns
        x0, y0 = col * cell_w, row * cell_h
        draw.text((x0 + 4, y0 + 4), label, fill=(230, 234, 242))
        sheet.paste(render, (x0, y0 + heading))
        sheet.paste(difference, (x0 + panel_w, y0 + heading))
        draw.text((x0 + 3, y0 + heading + 2), "Render", fill=(238, 240, 244))
        draw.text((x0 + panel_w + 3, y0 + heading + 2), "ΔY +red / -blue", fill=(238, 240, 244))
        print(f"{label}: frameY={candidate_flux:.8g} relativeL1={l1:.4%} "
              f"peakY={candidate_peak:.8g} centroid=({cx:.4f},{cy:.4f}) "
              f"centroidShift={centroid_shift:.4f}px")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"reference: frameY={ref_flux:.8g} peakY={ref_peak:.8g} "
          f"centroid=({ref_cx:.4f},{ref_cy:.4f}); saved {args.output}")


if __name__ == "__main__":
    main()
