"""Create a radiometric comparison of geometric ghosts and aperture FrFT ghosts."""

import argparse
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw


def read_pfm(path):
    with path.open("rb") as source:
        if source.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, source.readline().split())
        scale = float(source.readline())
        payload = source.read()
    if scale >= 0:
        raise ValueError(f"expected little-endian PFM scale: {path}")
    values = struct.unpack(f"<{width * height * 3}f", payload)
    rows = [values[y * width * 3:(y + 1) * width * 3] for y in range(height)]
    if scale != -1.0:
        rows = [tuple(value * abs(scale) for value in row) for row in rows]
    return width, height, list(reversed(rows))


def display_rgb(rgb, gain, tone_map, log_contrast, log_white):
    result = []
    for value in rgb:
        value = max(0.0, value)
        if tone_map == "log":
            mapped = math.log1p(log_contrast * value / max(log_white, 1e-30)) / math.log1p(log_contrast)
        else:
            exposed = value * gain
            mapped = exposed / (1.0 + exposed)
        srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
        result.append(round(min(1.0, max(0.0, srgb)) * 255))
    return tuple(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="original HDR chart PFM")
    parser.add_argument("geometric", type=Path, help="render with --ghost-frft off")
    parser.add_argument("frft", type=Path, help="render with --ghost-frft on")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
    parser.add_argument("--gain", type=float, default=300.0)
    parser.add_argument("--difference-gain", type=float, default=1200.0)
    parser.add_argument("--tone-map", choices=("reinhard", "log"), default="reinhard",
                        help="log uses one shared white point for both flare panels")
    parser.add_argument("--log-contrast", type=float, default=1000.0)
    parser.add_argument("--white-percentile", type=float, default=99.9,
                        help="positive source-subtracted flare percentile used as log white")
    parser.add_argument("--mask-circle", type=int, nargs=3, action="append", default=[],
                        metavar=("X", "Y", "R"),
                        help="hide a bright source core from the display and comparison metrics")
    parser.add_argument("--frft-label", default="Hullin aperture FrFT",
                        help="label for the third, transformed image")
    args = parser.parse_args()

    source, geometric, frft = (read_pfm(path) for path in (args.input, args.geometric, args.frft))
    if source[:2] != geometric[:2] or source[:2] != frft[:2]:
        parser.error("PFM dimensions must match")
    for name in ("gain", "difference_gain", "log_contrast"):
        value = getattr(args, name)
        if not math.isfinite(value) or value <= 0.0:
            parser.error(f"--{name.replace('_', '-')} must be positive and finite")
    if not 0.0 < args.white_percentile <= 100.0:
        parser.error("--white-percentile must be in (0,100]")
    width, height = source[:2]
    left, top, right, bottom = args.crop or (0, 0, width, height)
    if not (0 <= left < right <= width and 0 <= top < bottom <= height):
        parser.error("crop must lie inside the input image")

    def is_masked(x, y):
        return any((x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2
                   for cx, cy, radius in args.mask_circle)

    log_white = 0.0
    if args.tone_map == "log":
        positive_flare = []
        for rendered in (geometric[2], frft[2]):
            for y in range(top, bottom):
                for x in range(left, right):
                    if is_masked(x, y):
                        continue
                    src = source[2][y][x * 3:x * 3 + 3]
                    flare = rendered[y][x * 3:x * 3 + 3]
                    positive_flare.extend(max(0.0, flare[c] - src[c]) for c in range(3))
        positive_flare = sorted(value for value in positive_flare if value > 0.0)
        if not positive_flare:
            parser.error("the selected crop has no positive flare radiance")
        percentile_index = min(len(positive_flare) - 1,
                               int((args.white_percentile / 100.0) * (len(positive_flare) - 1)))
        log_white = positive_flare[percentile_index]

    panels = [Image.new("RGB", (right - left, bottom - top)) for _ in range(3)]
    panel_pixels = [[] for _ in panels]
    geometric_luma = []
    frft_luma = []
    for y in range(top, bottom):
        for x in range(left, right):
            if is_masked(x, y):
                panel_pixels[0].append((0, 0, 0))
                panel_pixels[1].append((0, 0, 0))
                panel_pixels[2].append((0, 0, 0))
                geometric_luma.append(0.0)
                frft_luma.append(0.0)
                continue
            index = x * 3
            src = source[2][y][index:index + 3]
            geo = geometric[2][y][index:index + 3]
            fft = frft[2][y][index:index + 3]
            geo_ghost = tuple(max(0.0, geo[c] - src[c]) for c in range(3))
            frft_ghost = tuple(max(0.0, fft[c] - src[c]) for c in range(3))
            panel_pixels[0].append(display_rgb(
                geo_ghost, args.gain, args.tone_map, args.log_contrast, log_white))
            panel_pixels[1].append(display_rgb(
                frft_ghost, args.gain, args.tone_map, args.log_contrast, log_white))
            delta = sum((frft_ghost[c] - geo_ghost[c]) * weight
                        for c, weight in enumerate((0.2126, 0.7152, 0.0722)))
            magnitude = min(1.0, abs(delta) * args.difference_gain)
            color = (magnitude, 0.04 * magnitude, 0.02 * magnitude) if delta >= 0 else (
                0.02 * magnitude, 0.06 * magnitude, magnitude)
            panel_pixels[2].append(tuple(round(channel * 255) for channel in color))
            geometric_luma.append(sum(geo_ghost[c] * weight for c, weight in
                                      enumerate((0.2126, 0.7152, 0.0722))))
            frft_luma.append(sum(frft_ghost[c] * weight for c, weight in
                                 enumerate((0.2126, 0.7152, 0.0722))))
    for panel, pixels in zip(panels, panel_pixels):
        panel.putdata(pixels)

    display_label = "log" if args.tone_map == "log" else "Reinhard"
    mask_label = " | source core masked" if args.mask_circle else ""
    labels = (f"Geometric flare | {display_label}{mask_label}",
              f"{args.frft_label} | {display_label}{mask_label}",
              "Change: red adds / blue removes")
    label_height = 24
    sheet = Image.new("RGB", (panels[0].width * 3, panels[0].height + label_height), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for i, (panel, label) in enumerate(zip(panels, labels)):
        x = i * panel.width
        sheet.paste(panel, (x, label_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)

    geo_sum = sum(geometric_luma)
    frft_sum = sum(frft_luma)
    absolute_delta = sum(abs(a - b) for a, b in zip(geometric_luma, frft_luma))
    geometric_peak = max(geometric_luma, default=0.0)
    frft_peak = max(frft_luma, default=0.0)

    def centroid(values, total):
        x = sum((left + i % (right - left)) * value
                for i, value in enumerate(values)) / max(total, 1e-30)
        y = sum((top + i // (right - left)) * value
                for i, value in enumerate(values)) / max(total, 1e-30)
        return x, y

    geo_centroid = centroid(geometric_luma, geo_sum)
    frft_centroid = centroid(frft_luma, frft_sum)
    centroid_shift = ((geo_centroid[0] - frft_centroid[0]) ** 2
                      + (geo_centroid[1] - frft_centroid[1]) ** 2) ** 0.5
    print(f"saved {args.output}")
    print(f"toneMap={args.tone_map} sharedWhiteRadiance={log_white:.9g} "
          f"whitePercentile={args.white_percentile:g} logContrast={args.log_contrast:g}")
    print(f"geometricY={geo_sum:.9g} frftY={frft_sum:.9g} "
          f"fluxChange={(frft_sum / max(geo_sum, 1e-30) - 1):+.6%} "
          f"relativeL1={absolute_delta / max(geo_sum, 1e-30):.6%} "
          f"peakY={geometric_peak:.9g}->{frft_peak:.9g} "
          f"centroidShift={centroid_shift:.4f}px "
          f"geometricCentroid=({geo_centroid[0]:.4f},{geo_centroid[1]:.4f}) "
          f"frftCentroid=({frft_centroid[0]:.4f},{frft_centroid[1]:.4f})")


if __name__ == "__main__":
    main()
