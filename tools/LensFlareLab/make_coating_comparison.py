"""Compare coated and uncoated LensFlareLab ghost PFM renders."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="unprocessed source PFM")
    parser.add_argument("uncoated", type=Path, help="render made with --coating off")
    parser.add_argument("coated", type=Path, help="render made with --coating on")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--gain", type=float, default=2400.0)
    parser.add_argument("--difference-gain", type=float, default=12000.0)
    parser.add_argument("--labels", nargs=2, default=("Uncoated", "Ideal MgF2-like single layer"))
    args = parser.parse_args()

    source, uncoated, coated = (read_pfm(path) for path in
                                (args.input, args.uncoated, args.coated))
    if source[:2] != uncoated[:2] or source[:2] != coated[:2]:
        parser.error("PFM dimensions must match")
    width, height = source[:2]
    title_height = 24
    sheet = Image.new("RGB", (width * 3, height + title_height), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    images = [Image.new("RGB", (width, height)) for _ in range(3)]
    panels = [[], [], []]
    abs_error = [0.0, 0.0, 0.0]
    baseline = [0.0, 0.0, 0.0]
    for y in range(height):
        for x in range(width):
            i = x * 3
            src = source[2][y][i:i + 3]
            a = tuple(max(0.0, uncoated[2][y][i + c] - src[c]) for c in range(3))
            b = tuple(max(0.0, coated[2][y][i + c] - src[c]) for c in range(3))
            panels[0].append(display_rgb(a, args.gain))
            panels[1].append(display_rgb(b, args.gain))
            diff = []
            for c in range(3):
                delta = b[c] - a[c]
                abs_error[c] += abs(delta)
                baseline[c] += abs(a[c])
                strength = min(1.0, abs(delta) * args.difference_gain)
                tint = (1.0, 0.0, 0.0) if delta >= 0 else (0.0, 0.7, 1.0)
                diff.append(tuple(round(strength * v * 255) for v in tint))
            panels[2].append(tuple(max(pixel[c] for pixel in diff) for c in range(3)))
    for index, image in enumerate(images):
        image.putdata(panels[index])
    labels = (args.labels[0], args.labels[1], f"{args.labels[1]} − {args.labels[0]} (signed RGB)")
    for column, (image, label) in enumerate(zip(images, labels)):
        x = column * width
        sheet.paste(image, (x, title_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    ratios = [abs_error[c] / max(baseline[c], 1e-30) for c in range(3)]
    print(f"saved {args.output}")
    print("coated-vs-uncoated flare absolute RGB change relative to uncoated: " +
          ", ".join(f"{name}={ratio:.6%}" for name, ratio in zip("RGB", ratios)))


if __name__ == "__main__":
    main()
