"""Create channel-resolved images and metrics for two dispersion models."""

import argparse
from pathlib import Path

from PIL import Image, ImageDraw

from make_ghost_frft_comparison import display_rgb, read_pfm


CHANNEL_NAMES = ("R", "G", "B")
WEIGHTS = ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0))
COMPLEMENTS = ((0.0, 1.0, 1.0), (1.0, 0.0, 1.0), (1.0, 1.0, 0.0))


def ghost_pixels(source, rendered):
    width, height = source[:2]
    result = []
    for y in range(height):
        row = []
        for x in range(width):
            offset = x * 3
            row.append(tuple(max(0.0, rendered[2][y][offset + c] - source[2][y][offset + c])
                             for c in range(3)))
        result.append(row)
    return result


def channel_metrics(pixels):
    height, width = len(pixels), len(pixels[0])
    sums = [0.0, 0.0, 0.0]
    peaks = [0.0, 0.0, 0.0]
    x_sums = [0.0, 0.0, 0.0]
    y_sums = [0.0, 0.0, 0.0]
    for y, row in enumerate(pixels):
        for x, rgb in enumerate(row):
            for c, value in enumerate(rgb):
                sums[c] += value
                peaks[c] = max(peaks[c], value)
                x_sums[c] += x * value
                y_sums[c] += y * value
    centroids = [(x_sums[c] / max(sums[c], 1e-30),
                  y_sums[c] / max(sums[c], 1e-30)) for c in range(3)]
    separations = []
    for a, b in ((0, 1), (1, 2), (0, 2)):
        dx = centroids[a][0] - centroids[b][0]
        dy = centroids[a][1] - centroids[b][1]
        separations.append((dx * dx + dy * dy) ** 0.5)
    return sums, peaks, centroids, max(separations)


def make_panels(source, cauchy, linear, gain, difference_gain):
    width, height = source[:2]
    cauchy_pixels = ghost_pixels(source, cauchy)
    linear_pixels = ghost_pixels(source, linear)
    cauchy_image = Image.new("RGB", (width, height))
    linear_image = Image.new("RGB", (width, height))
    difference_image = Image.new("RGB", (width, height))
    cauchy_display = []
    linear_display = []
    differences = []
    for y in range(height):
        for x in range(width):
            a, b = cauchy_pixels[y][x], linear_pixels[y][x]
            cauchy_display.append(display_rgb(a, gain))
            linear_display.append(display_rgb(b, gain))
            diff = tuple(a[c] - b[c] for c in range(3))
            color = [0.0, 0.0, 0.0]
            for c, value in enumerate(diff):
                strength = min(1.0, abs(value) * difference_gain)
                tint = WEIGHTS[c] if value >= 0.0 else COMPLEMENTS[c]
                for channel in range(3):
                    color[channel] = min(1.0, color[channel] + tint[channel] * strength)
            differences.append(tuple(round(channel * 255) for channel in color))
    cauchy_image.putdata(cauchy_display)
    linear_image.putdata(linear_display)
    difference_image.putdata(differences)
    return cauchy_image, linear_image, difference_image, cauchy_pixels, linear_pixels


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--case", action="append", nargs=4, required=True,
                        metavar=("LABEL", "INPUT", "CAUCHY", "LINEAR"),
                        help="repeat for each source position or ghost path")
    parser.add_argument("--gain", type=float, default=2400.0,
                        help="shared Reinhard display gain for ghost radiance")
    parser.add_argument("--difference-gain", type=float, default=12000.0,
                        help="display amplification for signed per-channel differences")
    parser.add_argument("--model-labels", nargs=2, default=("Cauchy", "Linear Abbe"),
                        metavar=("FIRST", "SECOND"),
                        help="labels for the two render inputs")
    args = parser.parse_args()

    loaded = []
    for label, input_path, cauchy_path, linear_path in args.case:
        source, cauchy, linear = (read_pfm(Path(path))
                                  for path in (input_path, cauchy_path, linear_path))
        if source[:2] != cauchy[:2] or source[:2] != linear[:2]:
            parser.error(f"PFM dimensions do not match for case {label}")
        loaded.append((label, source, cauchy, linear))

    width, height = loaded[0][1][:2]
    label_height = 24
    sheet = Image.new("RGB", (width * 3, (height + label_height) * len(loaded)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for case_index, (label, source, cauchy, linear) in enumerate(loaded):
        panels = make_panels(source, cauchy, linear, args.gain, args.difference_gain)
        cauchy_image, linear_image, difference_image, cauchy_pixels, linear_pixels = panels
        y = case_index * (height + label_height)
        for column, (panel, title) in enumerate(zip(
                (cauchy_image, linear_image, difference_image),
                (args.model_labels[0], args.model_labels[1],
                f"{args.model_labels[0]} minus {args.model_labels[1]} (signed RGB)"))):
            x = column * width
            sheet.paste(panel, (x, y + label_height))
            draw.text((x + 6, y + 5), f"{label} | {title}", fill=(225, 230, 238))

        model_metrics = []
        for model_name, pixels in zip(args.model_labels, (cauchy_pixels, linear_pixels)):
            sums, peaks, centroids, separation = channel_metrics(pixels)
            model_metrics.append((model_name, sums, peaks, centroids, separation))
            formatted = " ".join(
                f"{CHANNEL_NAMES[c]} Y={sums[c]:.8g} peak={peaks[c]:.8g} "
                f"centroid=({centroids[c][0]:.3f},{centroids[c][1]:.3f})"
                for c in range(3))
            print(f"{label} {model_name}: {formatted}; maxRGBSeparation={separation:.4f}px")
        a, b = model_metrics
        print(f"{label}: channelFluxChange({args.model_labels[1]}/{args.model_labels[0]}-1)=" + ", ".join(
            f"{CHANNEL_NAMES[c]}={(b[1][c] / max(a[1][c], 1e-30) - 1):+.5%}"
            for c in range(3)))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"saved {args.output}")


if __name__ == "__main__":
    main()
