"""Create a crop plate comparing two PFM renders around the source or a crop center."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_source_diffraction_comparison import display, luminance, read_pfm


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="source PFM")
    parser.add_argument("tent", type=Path, help="tent-resampled render PFM")
    parser.add_argument("pixel_area", type=Path, help="area-integrated render PFM")
    parser.add_argument("output", type=Path)
    parser.add_argument("--radius", type=int, default=32, help="crop radius in pixels")
    parser.add_argument("--gain", type=float, default=80.0)
    parser.add_argument("--difference-gain", type=float, default=160.0)
    parser.add_argument("--scale", type=int, default=3, help="nearest-neighbor display scale")
    parser.add_argument("--tent-label", default="Tent")
    parser.add_argument("--pixel-area-label", default="Pixel-area")
    parser.add_argument("--center", type=int, nargs=2, metavar=("X", "Y"),
                        help="crop center; default is the brightest input pixel")
    parser.add_argument("--subtract-source", action="store_true",
                        help="display positive flare above the unprocessed input")
    args = parser.parse_args()
    if args.radius < 2 or args.scale < 1:
        parser.error("radius must be >= 2 and scale must be >= 1")

    source, tent, area = (read_pfm(path) for path in
                          (args.input, args.tent, args.pixel_area))
    if source[:2] != tent[:2] or source[:2] != area[:2]:
        parser.error("all PFM dimensions must match")
    width, height = source[:2]
    if args.center:
        cx, cy = args.center
        if not (0 <= cx < width and 0 <= cy < height):
            parser.error("crop center must lie inside the image")
    else:
        cx, cy, peak = 0, 0, -1.0
        for y, row in enumerate(source[2]):
            for x, pixel in enumerate(row):
                value = luminance(pixel)
                if value > peak:
                    cx, cy, peak = x, y, value
    left, top = max(0, cx - args.radius), max(0, cy - args.radius)
    right, bottom = min(width, cx + args.radius), min(height, cy + args.radius)
    crop_w, crop_h = right - left, bottom - top
    labels = ("Source", args.tent_label, args.pixel_area_label, "Abs diff")
    header, gap = 24, 8
    plate = Image.new("RGB", (crop_w * 4 + gap * 3, crop_h + header), (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for index, label in enumerate(labels):
        x0 = index * (crop_w + gap)
        draw.text((x0 + 3, 5), label, fill=(225, 230, 238))
    for y in range(top, bottom):
        for x in range(left, right):
            source_rgb = source[2][y][x]
            tent_rgb = tent[2][y][x]
            area_rgb = area[2][y][x]
            if args.subtract_source:
                tent_rgb = tuple(max(0.0, tent_rgb[c] - source_rgb[c]) for c in range(3))
                area_rgb = tuple(max(0.0, area_rgb[c] - source_rgb[c]) for c in range(3))
            difference = tuple(abs(area_rgb[c] - tent_rgb[c]) for c in range(3))
            pixels = (display(source_rgb, args.gain), display(tent_rgb, args.gain),
                      display(area_rgb, args.gain),
                      display(difference, args.difference_gain))
            for index, rgb in enumerate(pixels):
                plate.putpixel((index * (crop_w + gap) + x - left,
                                header + y - top), rgb)

    if args.scale > 1:
        plate = plate.resize((plate.width * args.scale, plate.height * args.scale),
                             Image.Resampling.NEAREST)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.output)
    for name, image in (("source", source), ("tent", tent), ("pixel-area", area)):
        frame_y = sum(luminance(pixel) for row in image[2] for pixel in row)
        crop_y = sum(luminance(image[2][y][x]) for y in range(top, bottom)
                     for x in range(left, right))
        print(f"{name}: cropY={crop_y:.9g} frameY={frame_y:.9g}")
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
