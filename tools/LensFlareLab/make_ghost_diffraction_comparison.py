"""Create a cropped PNG comparison of geometric and Fresnel-propagated ghosts."""

import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw


def read_pfm(path: Path):
    with path.open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        payload = source.read()
    if scale >= 0:
        raise ValueError(f"expected little-endian PFM scale: {path}")
    values = tuple(value * abs(scale) for value in struct.unpack(f"<{width * height * 3}f", payload))
    pixels = [values[i:i + 3] for i in range(0, len(values), 3)]
    rows = [pixels[y * width:(y + 1) * width] for y in range(height)]
    return width, height, list(reversed(rows))


def display_rgb(pixel, gain):
    result = []
    for value in pixel:
        mapped = max(0.0, value * gain) / (1.0 + max(0.0, value * gain))
        srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR chart PFM")
    parser.add_argument("geometric", type=Path, help="render with ghost diffraction off")
    parser.add_argument("fresnel", type=Path, help="render with ghost diffraction on")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                        default=(320, 100, 620, 359))
    parser.add_argument("--gain", type=float, default=120.0,
                        help="display gain for isolated ghost radiance")
    parser.add_argument("--mask-circle", type=int, nargs=3, action="append",
                        metavar=("X", "Y", "R"), default=[],
                        help="hide a source core from ghost-only display panels")
    args = parser.parse_args()

    base = read_pfm(args.input)
    geometric = read_pfm(args.geometric)
    fresnel = read_pfm(args.fresnel)
    if base[:2] != geometric[:2] or base[:2] != fresnel[:2]:
        parser.error("PFM dimensions must match")
    width, height = base[:2]
    left, top, right, bottom = args.crop
    if not (0 <= left < right < width and 0 <= top < bottom < height):
        parser.error("crop must lie inside the input image")

    panels = []
    for rendered in (geometric[2], fresnel[2]):
        panel = Image.new("RGB", (right - left, bottom - top))
        pixels = []
        for y in range(top, bottom):
            for x in range(left, right):
                if any((x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2
                       for cx, cy, radius in args.mask_circle):
                    pixels.append((0, 0, 0))
                    continue
                source = base[2][y][x]
                flare = rendered[y][x]
                pixels.append(display_rgb(tuple(max(0.0, flare[c] - source[c]) for c in range(3)), args.gain))
        panel.putdata(pixels)
        panels.append(panel)

    delta_panel = Image.new("RGB", panels[0].size)
    delta_pixels = []
    signed_sum = 0.0
    absolute_sum = 0.0
    peak_positive = 0.0
    peak_negative = 0.0
    for y in range(top, bottom):
        for x in range(left, right):
            if any((x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2
                   for cx, cy, radius in args.mask_circle):
                delta_pixels.append((0, 0, 0))
                continue
            g = geometric[2][y][x]
            f = fresnel[2][y][x]
            dg = (f[0] - g[0]) * 0.2126 + (f[1] - g[1]) * 0.7152 + (f[2] - g[2]) * 0.0722
            signed_sum += dg
            absolute_sum += abs(dg)
            peak_positive = max(peak_positive, dg)
            peak_negative = min(peak_negative, dg)
            value = min(1.0, abs(dg) * args.gain)
            color = (value, 0.05 * value, 0.02 * value) if dg >= 0 else (0.02 * value, 0.08 * value, value)
            delta_pixels.append(tuple(round(channel * 255) for channel in color))
    delta_panel.putdata(delta_pixels)

    label_height = 24
    sheet = Image.new("RGB", (panels[0].width * 3, panels[0].height + label_height), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for index, (panel, label) in enumerate(zip(
        (*panels, delta_panel), ("Geometric ghost", "Fresnel propagated", "Change: red adds / blue removes")
    )):
        x = index * panel.width
        sheet.paste(panel, (x, label_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"saved {args.output}")
    print(f"crop luma delta signed={signed_sum:.9g} abs={absolute_sum:.9g} "
          f"positivePeak={peak_positive:.9g} negativePeak={peak_negative:.9g}")


if __name__ == "__main__":
    main()
