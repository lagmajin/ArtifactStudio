"""Compare direct Fraunhofer PSFs for different aperture-blade counts."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_source_diffraction_comparison import display, luminance, read_pfm


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR source PFM")
    parser.add_argument("output", type=Path)
    parser.add_argument("--case", action="append", nargs=2, required=True,
                        metavar=("LABEL", "PFM"))
    parser.add_argument("--crop", type=int, nargs=4, default=(280, 140, 360, 220),
                        metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
    parser.add_argument("--gain", type=float, default=80.0)
    args = parser.parse_args()

    source = read_pfm(args.input)
    cases = [(label, read_pfm(Path(path))) for label, path in args.case]
    if any(image[:2] != source[:2] for _, image in cases):
        parser.error("all PFM dimensions must match")
    width, height = source[:2]
    left, top, right, bottom = args.crop
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the images")

    labels = ["Original source"] + [label for label, _ in cases]
    renders = [source] + [image for _, image in cases]
    panel_width, panel_height = right - left, bottom - top
    header = 24
    plate = Image.new("RGB", (panel_width * len(renders), panel_height + header),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for index, (label, image) in enumerate(zip(labels, renders)):
        x0 = index * panel_width
        panel = Image.new("RGB", (panel_width, panel_height))
        for y in range(top, bottom):
            for x in range(left, right):
                panel.putpixel((x - left, y - top), display(image[2][y][x], args.gain))
        plate.paste(panel, (x0, header))
        draw.text((x0 + 5, 5), label, fill=(225, 230, 238))
        crop_y = sum(luminance(image[2][y][x])
                     for y in range(top, bottom) for x in range(left, right))
        frame_y = sum(luminance(pixel) for row in image[2] for pixel in row)
        print(f"{label}: cropY={crop_y:.9g} frameY={frame_y:.9g}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
