"""Generate a deterministic asymmetric multi-lobe HDR source for sampling studies."""

import argparse
import math
import struct
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path("build/tools/LensFlareLab/multilobe-source.pfm"))
    parser.add_argument("--peak", type=float, default=1024.0,
                        help="radiance scale (default: 1024)")
    parser.add_argument("--lobe-spacing-scale", type=float, default=1.0,
                        help="scale lobe offsets from their center (0..2.5, default: 1)")
    parser.add_argument("--color-separated", action="store_true",
                        help="assign warm, blue, and red spectra to the three lobes")
    parser.add_argument("--position", type=float, nargs=2, default=(0.383, 0.384),
                        metavar=("U", "V"), help="source center in normalized image coordinates")
    args = parser.parse_args()
    if not math.isfinite(args.peak) or args.peak <= 0.0:
        parser.error("--peak must be finite and greater than zero")
    if not math.isfinite(args.lobe_spacing_scale) or not 0.0 <= args.lobe_spacing_scale <= 2.5:
        parser.error("--lobe-spacing-scale must be in the range 0..2.5")
    if any(not math.isfinite(value) or not 0.0 <= value <= 1.0
           for value in args.position):
        parser.error("--position coordinates must be in the range 0..1")

    width, height = 640, 360
    center_x = round(args.position[0] * (width - 1))
    center_y = round(args.position[1] * (height - 1))
    # Three overlapping lobes form an asymmetric finite emitter. The unequal
    # radiance and offsets exercise both marginal partitions and the 2D bins.
    lobes = ((-7.0, -2.0, 3.8, 1.0, (1.0, 0.66, 0.30)),
             (2.0, -4.0, 2.8, 0.68, (0.12, 0.56, 1.0)),
             (6.0, 4.0, 2.1, 0.42, (1.0, 0.16, 0.07)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("wb") as image:
        image.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        for y in range(height - 1, -1, -1):
            for x in range(width):
                radiance = [0.0, 0.0, 0.0]
                for offset_x, offset_y, sigma, strength, color in lobes:
                    dx = x - center_x - offset_x * args.lobe_spacing_scale
                    dy = y - center_y - offset_y * args.lobe_spacing_scale
                    intensity = args.peak * strength * math.exp(
                        -(dx * dx + dy * dy) / (2.0 * sigma * sigma))
                    spectrum = color if args.color_separated else (1.0, 1.0, 1.0)
                    for channel in range(3):
                        radiance[channel] += intensity * spectrum[channel]
                image.write(struct.pack("<3f", *radiance))
    print(args.output)


if __name__ == "__main__":
    main()
