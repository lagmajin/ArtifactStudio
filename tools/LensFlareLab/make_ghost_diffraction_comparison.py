"""Create a cropped PNG comparison of geometric and Fresnel-propagated ghosts."""

import argparse
import math
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


def display_rgb(pixel, gain, tone_map, log_contrast, log_white):
    result = []
    for value in pixel:
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
    parser.add_argument("geometric", type=Path, help="render with ghost diffraction off")
    parser.add_argument("fresnel", type=Path, help="render with ghost diffraction on")
    parser.add_argument("output", type=Path, help="comparison PNG")
    parser.add_argument("--crop", type=int, nargs=4, metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                        default=(320, 100, 620, 359))
    parser.add_argument("--gain", type=float, default=120.0,
                        help="Reinhard display gain; log mapping uses its measured shared white point")
    parser.add_argument("--difference-gain", type=float,
                        help="linear gain for the signed difference panel (defaults to --gain)")
    parser.add_argument("--tone-map", choices=("reinhard", "log"), default="reinhard",
                        help="Reinhard is the legacy default; log uses a shared crop white point")
    parser.add_argument("--log-contrast", type=float, default=1000.0,
                        help="log mapping contrast from zero to the shared white point")
    parser.add_argument("--white-percentile", type=float, default=99.9,
                        help="shared positive channel-radiance percentile used as log white")
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
    if not math.isfinite(args.gain) or args.gain <= 0:
        parser.error("--gain must be positive and finite")
    if args.difference_gain is None:
        args.difference_gain = args.gain
    if not math.isfinite(args.difference_gain) or args.difference_gain <= 0:
        parser.error("--difference-gain must be positive and finite")
    if not math.isfinite(args.log_contrast) or args.log_contrast <= 0:
        parser.error("--log-contrast must be positive and finite")
    if not 0.0 < args.white_percentile <= 100.0:
        parser.error("--white-percentile must be in (0,100]")

    log_white = 0.0
    if args.tone_map == "log":
        crop_radiance = []
        for rendered in (geometric[2], fresnel[2]):
            for y in range(top, bottom):
                for x in range(left, right):
                    if any((x - cx) ** 2 + (y - cy) ** 2 <= radius ** 2
                           for cx, cy, radius in args.mask_circle):
                        continue
                    source, flare = base[2][y][x], rendered[y][x]
                    crop_radiance.extend(max(0.0, flare[c] - source[c]) for c in range(3))
        positive_radiance = sorted(value for value in crop_radiance if value > 0.0)
        if not positive_radiance:
            parser.error("the selected crop has no positive flare radiance")
        percentile_index = min(len(positive_radiance) - 1,
                               int((args.white_percentile / 100.0) * (len(positive_radiance) - 1)))
        log_white = positive_radiance[percentile_index]

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
                pixels.append(display_rgb(tuple(max(0.0, flare[c] - source[c]) for c in range(3)),
                                          args.gain, args.tone_map, args.log_contrast, log_white))
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
            value = min(1.0, abs(dg) * args.difference_gain)
            color = (value, 0.05 * value, 0.02 * value) if dg >= 0 else (0.02 * value, 0.08 * value, value)
            delta_pixels.append(tuple(round(channel * 255) for channel in color))
    delta_panel.putdata(delta_pixels)

    label_height = 24
    sheet = Image.new("RGB", (panels[0].width * 3, panels[0].height + label_height), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for index, (panel, label) in enumerate(zip(
        (*panels, delta_panel), ("Geometric ghost", "Fresnel propagated",
                                 f"Change: red adds / blue removes (linear ×{args.difference_gain:g})")
    )):
        x = index * panel.width
        sheet.paste(panel, (x, label_height))
        draw.text((x + 6, 5), label, fill=(225, 230, 238))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f"saved {args.output}")
    print(f"toneMap={args.tone_map} sharedWhiteRadiance={log_white:.9g} "
          f"whitePercentile={args.white_percentile:g} logContrast={args.log_contrast:g}")
    print(f"crop luma delta signed={signed_sum:.9g} abs={absolute_sum:.9g} "
          f"positivePeak={peak_positive:.9g} negativePeak={peak_negative:.9g}")


if __name__ == "__main__":
    main()
