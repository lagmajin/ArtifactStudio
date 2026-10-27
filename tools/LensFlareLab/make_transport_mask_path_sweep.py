"""Compare polygon-only and transport-mask FrFT over several ghost paths."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR chart PFM")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--case", action="append", nargs=7, required=True,
                        metavar=("LABEL", "POLYGON", "TRANSPORT", "L", "T", "R", "B"),
                        help="repeat with one selected path and its crop bounds")
    parser.add_argument("--gain", type=float, default=300000.0)
    parser.add_argument("--difference-gain", type=float, default=1000000.0)
    args = parser.parse_args()

    source = read_pfm(args.input)
    cases = []
    for label, polygon_path, transport_path, *bounds in args.case:
        polygon = read_pfm(Path(polygon_path))
        transport = read_pfm(Path(transport_path))
        if polygon[:2] != source[:2] or transport[:2] != source[:2]:
            parser.error(f"PFM dimensions do not match for {label}")
        left, top, right, bottom = map(int, bounds)
        if not (0 <= left < right <= source[0] and 0 <= top < bottom <= source[1]):
            parser.error(f"crop is outside input for {label}")
        cases.append((label, polygon, transport, (left, top, right, bottom)))

    cell_width, cell_height, header, row_gap = 230, 148, 24, 8
    columns, rows = 3, len(cases)
    chart = Image.new("RGB", (columns * cell_width, rows * (cell_height + header + row_gap)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    weights = (0.2126, 0.7152, 0.0722)

    for row, (label, polygon, transport, bounds) in enumerate(cases):
        left, top, right, bottom = bounds
        crop_width, crop_height = right - left, bottom - top
        scale = min(cell_width / crop_width, cell_height / crop_height)
        display_width = max(1, round(crop_width * scale))
        display_height = max(1, round(crop_height * scale))
        mode_pixels = [[] for _ in range(3)]
        polygon_luma, transport_luma = [], []
        for y in range(top, bottom):
            for x in range(left, right):
                offset = x * 3
                base = source[2][y][offset:offset + 3]
                poly_rgb = tuple(max(0.0, polygon[2][y][offset + c] - base[c])
                                 for c in range(3))
                transport_rgb = tuple(max(0.0, transport[2][y][offset + c] - base[c])
                                      for c in range(3))
                mode_pixels[0].append(display_rgb(poly_rgb, args.gain))
                mode_pixels[1].append(display_rgb(transport_rgb, args.gain))
                delta = sum((transport_rgb[c] - poly_rgb[c]) * weights[c]
                            for c in range(3))
                strength = min(1.0, abs(delta) * args.difference_gain)
                value = round(strength * 255)
                mode_pixels[2].append((value, round(value * 0.04), round(value * 0.02))
                                      if delta >= 0 else
                                      (round(value * 0.02), round(value * 0.06), value))
                polygon_luma.append(sum(poly_rgb[c] * weights[c] for c in range(3)))
                transport_luma.append(sum(transport_rgb[c] * weights[c] for c in range(3)))
        mode_panels = []
        for pixels in mode_pixels:
            panel = Image.new("RGB", (crop_width, crop_height))
            panel.putdata(pixels)
            mode_panels.append(panel.resize((display_width, display_height),
                                            Image.Resampling.LANCZOS))
        relative_l1 = sum(abs(a - b) for a, b in zip(polygon_luma, transport_luma)) / max(
            sum(polygon_luma), 1e-30)
        transport_ratio = sum(transport_luma) / max(sum(polygon_luma), 1e-30)
        labels = (f"{label} | polygon", f"{label} | transport",
                  f"L1 {relative_l1:.1%} | Y x{transport_ratio:.3f}")
        y = row * (cell_height + header + row_gap)
        for column, (panel, panel_label) in enumerate(zip(mode_panels, labels)):
            x = column * cell_width
            draw.text((x + 5, y + 4), panel_label, fill=(225, 230, 238))
            chart.paste(panel, (x + (cell_width - display_width) // 2, y + header))
        print(f"{label}: cropY polygon={sum(polygon_luma):.8g} "
              f"transport={sum(transport_luma):.8g} relativeL1={relative_l1:.6%} "
              f"transportYratio={transport_ratio:.6f}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
