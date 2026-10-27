"""Compare transport-mask FrFT texture resolutions against a dense reference."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR chart PFM")
    parser.add_argument("image64", type=Path)
    parser.add_argument("image128", type=Path)
    parser.add_argument("image256", type=Path)
    parser.add_argument("image512", type=Path, help="dense image reference")
    parser.add_argument("output", type=Path)
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                        default=(360, 165, 560, 345))
    parser.add_argument("--gain", type=float, default=300000.0)
    parser.add_argument("--difference-gain", type=float, default=1000000.0)
    args = parser.parse_args()

    source = read_pfm(args.input)
    images = [read_pfm(path) for path in
              (args.image64, args.image128, args.image256, args.image512)]
    if any(image[:2] != source[:2] for image in images):
        parser.error("PFM dimensions must match")
    width, height = source[:2]
    left, top, right, bottom = args.crop
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the images")

    panel_width, panel_height = right - left, bottom - top
    label_height, gap = 26, 4
    chart = Image.new("RGB", (panel_width * 4 + gap * 5,
                               (panel_height + label_height) * 2 + gap * 3),
                      (18, 21, 26))
    draw = ImageDraw.Draw(chart)
    weights = (0.2126, 0.7152, 0.0722)
    luminance = [[] for _ in images]
    panels = []
    stats = []
    for index, resolution in enumerate((64, 128, 256, 512)):
        colors = []
        total = peak = sum_x = sum_y = 0.0
        for y in range(top, bottom):
            for x in range(left, right):
                offset = x * 3
                base = source[2][y][offset:offset + 3]
                rgb = tuple(max(0.0, images[index][2][y][offset + c] - base[c])
                            for c in range(3))
                value = sum(rgb[c] * weights[c] for c in range(3))
                luminance[index].append(value)
                total += value
                peak = max(peak, value)
                sum_x += x * value
                sum_y += y * value
                colors.append(display_rgb(rgb, args.gain))
        panel = Image.new("RGB", (panel_width, panel_height))
        panel.putdata(colors)
        panels.append(panel)
        stats.append((total, peak, sum_x, sum_y))

    for index, resolution in enumerate((64, 128, 256, 512)):
        relative_l1 = (sum(abs(a - b) for a, b in zip(luminance[index], luminance[3])) /
                       max(sum(luminance[3]), 1e-30)) if index < 3 else 0.0
        x = gap + index * (panel_width + gap)
        y = gap
        chart.paste(panels[index], (x, y + label_height))
        suffix = f"Delta {relative_l1:.2%}" if index < 3 else "reference"
        draw.text((x + 4, y + 5), f"{resolution}² | {suffix}", fill=(225, 230, 238))
        total, peak, sum_x, sum_y = stats[index]
        print(f"texture={resolution} Y={total:.9g} peak={peak:.9g} "
              f"centroid=({sum_x / max(total, 1e-30):.3f},{sum_y / max(total, 1e-30):.3f}) "
              f"relativeL1vs512={relative_l1:.6%}")

    for index, resolution in enumerate((64, 128, 256)):
        pixels = []
        for a, b in zip(luminance[index], luminance[3]):
            delta = a - b
            strength = min(1.0, abs(delta) * args.difference_gain)
            value = round(strength * 255)
            pixels.append((value, round(value * 0.04), round(value * 0.02)) if delta >= 0
                          else (round(value * 0.02), round(value * 0.06), value))
        panel = Image.new("RGB", (panel_width, panel_height))
        panel.putdata(pixels)
        x = gap + index * (panel_width + gap)
        y = panel_height + label_height + gap * 2
        chart.paste(panel, (x, y + label_height))
        draw.text((x + 4, y + 5), f"{resolution}² - 512² (red + / blue -)",
                  fill=(225, 230, 238))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    chart.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
