"""Compare polygon-only and per-path transport-mask ghost FrFT images."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR chart PFM")
    parser.add_argument("polygon", type=Path, help="polygon-only FrFT output")
    parser.add_argument("transport", type=Path, help="transport-mask FrFT output")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
    parser.add_argument("--gain", type=float, default=300.0)
    parser.add_argument("--difference-gain", type=float, default=1200.0)
    args = parser.parse_args()

    source, polygon, transport = (read_pfm(path)
                                  for path in (args.input, args.polygon, args.transport))
    if source[:2] != polygon[:2] or source[:2] != transport[:2]:
        parser.error("PFM dimensions must match")
    width, height = source[:2]
    left, top, right, bottom = args.crop or (0, 0, width, height)
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the input image")

    panel_width, panel_height = right - left, bottom - top
    images = [Image.new("RGB", (panel_width, panel_height)) for _ in range(3)]
    pixels = [[] for _ in images]
    polygon_luma, transport_luma = [], []
    weights = (0.2126, 0.7152, 0.0722)
    for y in range(top, bottom):
        for x in range(left, right):
            index = x * 3
            base = source[2][y][index:index + 3]
            poly_rgb = tuple(max(0.0, polygon[2][y][index + c] - base[c]) for c in range(3))
            transport_rgb = tuple(max(0.0, transport[2][y][index + c] - base[c])
                                  for c in range(3))
            pixels[0].append(display_rgb(poly_rgb, args.gain))
            pixels[1].append(display_rgb(transport_rgb, args.gain))
            delta = sum((transport_rgb[c] - poly_rgb[c]) * weights[c] for c in range(3))
            magnitude = min(1.0, abs(delta) * args.difference_gain)
            pixels[2].append((round(255 * magnitude), round(10 * magnitude), 0)
                             if delta >= 0 else (0, round(15 * magnitude), round(255 * magnitude)))
            polygon_luma.append(sum(poly_rgb[c] * weights[c] for c in range(3)))
            transport_luma.append(sum(transport_rgb[c] * weights[c] for c in range(3)))
    for image, colors in zip(images, pixels):
        image.putdata(colors)

    labels = ("Polygon-only", "Per-path transport", "Delta: red + / blue -")
    label_height = 24
    chart = Image.new("RGB", (panel_width * 3, panel_height + label_height), (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    for column, (image, label) in enumerate(zip(images, labels)):
        x = column * panel_width
        chart.paste(image, (x, label_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)

    polygon_sum, transport_sum = sum(polygon_luma), sum(transport_luma)
    difference = sum(abs(a - b) for a, b in zip(polygon_luma, transport_luma))
    print(f"saved {args.output}; polygonY={polygon_sum:.9g} transportY={transport_sum:.9g} "
          f"relativeL1={difference / max(polygon_sum, 1e-30):.6%}")


if __name__ == "__main__":
    main()
