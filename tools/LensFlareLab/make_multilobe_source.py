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
    args = parser.parse_args()
    if not math.isfinite(args.peak) or args.peak <= 0.0:
        parser.error("--peak must be finite and greater than zero")

    width, height = 640, 360
    center_x, center_y = round(0.383 * (width - 1)), round(0.384 * (height - 1))
    # Three overlapping lobes form an asymmetric finite emitter. The unequal
    # radiance and offsets exercise both marginal partitions and the 2D bins.
    lobes = ((-7.0, -2.0, 3.8, 1.0), (2.0, -4.0, 2.8, 0.68),
             (6.0, 4.0, 2.1, 0.42))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("wb") as image:
        image.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        for y in range(height - 1, -1, -1):
            for x in range(width):
                radiance = 0.0
                for offset_x, offset_y, sigma, strength in lobes:
                    dx, dy = x - center_x - offset_x, y - center_y - offset_y
                    radiance += args.peak * strength * math.exp(
                        -(dx * dx + dy * dy) / (2.0 * sigma * sigma))
                image.write(struct.pack("<3f", radiance, radiance, radiance))
    print(args.output)


if __name__ == "__main__":
    main()
