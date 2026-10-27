"""Compare direct-source Fraunhofer diffraction with the original HDR source."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


LUMA = (0.2126, 0.7152, 0.0722)


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
    multiplier = abs(scale)
    rows = []
    for y in range(height):
        file_y = height - 1 - y
        start = file_y * width * 3
        rows.append([tuple(values[i + c] * multiplier for c in range(3))
                     for i in range(start, start + width * 3, 3)])
    return width, height, rows


def display(rgb, gain):
    result = []
    for value in rgb:
        mapped = max(0.0, value * gain)
        mapped /= 1.0 + mapped
        srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def luminance(rgb):
    return sum(value * weight for value, weight in zip(rgb, LUMA))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("diffraction_off", type=Path)
    parser.add_argument("diffraction_on", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--crop", type=int, nargs=4, default=(0, 0, 640, 360),
                        metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
    parser.add_argument("--gain", type=float, default=80.0)
    parser.add_argument("--difference-gain", type=float, default=200.0)
    args = parser.parse_args()

    source = read_pfm(args.input)
    off = read_pfm(args.diffraction_off)
    on = read_pfm(args.diffraction_on)
    if source[:2] != off[:2] or source[:2] != on[:2]:
        parser.error("all PFM dimensions must match")
    width, height = source[:2]
    left, top, right, bottom = args.crop
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the images")

    labels = ("Original HDR source", "Direct diffraction on", "On minus off")
    panels = [Image.new("RGB", (right - left, bottom - top)) for _ in labels]
    frame_y = [0.0, 0.0]
    crop_y = [0.0, 0.0]
    frame_abs_delta = 0.0
    crop_abs_delta = 0.0
    for y in range(height):
        for x in range(width):
            original = source[2][y][x]
            off_rgb = off[2][y][x]
            on_rgb = on[2][y][x]
            delta = tuple(on_rgb[c] - off_rgb[c] for c in range(3))
            frame_y[0] += luminance(off_rgb)
            frame_y[1] += luminance(on_rgb)
            frame_abs_delta += abs(luminance(delta))
            if left <= x < right and top <= y < bottom:
                crop_y[0] += luminance(off_rgb)
                crop_y[1] += luminance(on_rgb)
                crop_abs_delta += abs(luminance(delta))
                px, py = x - left, y - top
                panels[0].putpixel((px, py), display(original, args.gain))
                panels[1].putpixel((px, py), display(on_rgb, args.gain))
                strength = min(1.0, abs(luminance(delta)) * args.difference_gain)
                value = round(strength * 255)
                panels[2].putpixel((px, py),
                    (value, round(value * 0.04), round(value * 0.02)) if luminance(delta) >= 0
                    else (round(value * 0.02), round(value * 0.05), value))

    header = 24
    plate = Image.new("RGB", ((right - left) * len(panels), bottom - top + header),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for index, (panel, label) in enumerate(zip(panels, labels)):
        x = index * panel.width
        plate.paste(panel, (x, header))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.output)
    print(f"frameY off={frame_y[0]:.9g} on={frame_y[1]:.9g} "
          f"signedChange={(frame_y[1] - frame_y[0]):.9g} "
          f"absDelta={frame_abs_delta:.9g}")
    print(f"cropY off={crop_y[0]:.9g} on={crop_y[1]:.9g} "
          f"signedChange={(crop_y[1] - crop_y[0]):.9g} "
          f"absDelta={crop_abs_delta:.9g}; saved {args.output}")


if __name__ == "__main__":
    main()
