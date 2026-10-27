"""Visualize direct-source PSF convergence across aperture FFT resolutions."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from compare_diffraction_resampling import read_pfm, reconstruct
from make_source_diffraction_comparison import display, luminance


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR source PFM")
    parser.add_argument("output", type=Path)
    parser.add_argument("--case", action="append", nargs=2, required=True,
                        metavar=("LABEL", "PFM"))
    parser.add_argument("--threshold", type=float, default=0.85)
    parser.add_argument("--difference-case", default="512",
                        help="resolution label to subtract from the final reference")
    parser.add_argument("--radius", type=int, default=32, help="crop radius in pixels")
    parser.add_argument("--gain", type=float, default=80.0)
    parser.add_argument("--difference-gain", type=float, default=240.0)
    parser.add_argument("--scale", type=int, default=3)
    args = parser.parse_args()
    if args.radius < 2 or args.scale < 1:
        parser.error("radius must be >= 2 and scale must be >= 1")

    source = read_pfm(args.input)
    cases = [(label, read_pfm(Path(path))) for label, path in args.case]
    if any(image[:2] != source[:2] for _, image in cases):
        parser.error("all PFM dimensions must match")
    width, height = source[:2]
    cx, cy, peak = 0, 0, -1.0
    for y, row in enumerate(source[2]):
        for x, pixel in enumerate(row):
            value = luminance(pixel)
            if value > peak:
                cx, cy, peak = x, y, value
    left, top = max(0, cx - args.radius), max(0, cy - args.radius)
    right, bottom = min(width, cx + args.radius), min(height, cy + args.radius)
    panel_width, panel_height = right - left, bottom - top
    reference_label, reference_render = cases[-1]
    case_labels = [label for label, _ in cases]
    if args.difference_case not in case_labels:
        parser.error(f"difference case {args.difference_case!r} is not present")
    difference_case_index = case_labels.index(args.difference_case) + 1
    reference = reconstruct(source, reference_render, args.threshold)
    labels = ["Source"] + case_labels + ["Abs diff"]
    panels = [source[2]] + [reconstruct(source, image, args.threshold)
                            for _, image in cases]
    difference = [[[
        abs(panels[difference_case_index][y][x][c] - reference[y][x][c])
        for c in range(3)] for x in range(width)] for y in range(height)]
    panels.append(difference)

    count = len(panels)
    gap, header = 6, 22
    plate = Image.new("RGB", (panel_width * count + gap * (count - 1),
                              panel_height + header), (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for index, label in enumerate(labels):
        draw.text((index * (panel_width + gap) + 3, 4), label,
                  fill=(225, 230, 238))
        for y in range(top, bottom):
            for x in range(left, right):
                gain = args.difference_gain if index == count - 1 else args.gain
                rgb = display(panels[index][y][x], gain)
                plate.putpixel((index * (panel_width + gap) + x - left,
                                header + y - top), rgb)
    if args.scale > 1:
        plate = plate.resize((plate.width * args.scale, plate.height * args.scale),
                             Image.Resampling.NEAREST)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.output)
    for label, image in [("source", source[2])] + list(zip(labels[1:-1], panels[1:-1])):
        crop_y = sum(luminance(image[y][x]) for y in range(top, bottom)
                     for x in range(left, right))
        print(f"{label}: cropY={crop_y:.9g}")
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
