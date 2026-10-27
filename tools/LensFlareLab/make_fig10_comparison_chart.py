"""Generate a two-point-light input approximating the Angenieux markers in paper Fig. 10."""

import argparse
import math
import struct
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path,
        default=Path("build/tools/LensFlareLab/fig10-angenieux-two-light-input.pfm"),
        help="output little-endian RGB PFM path",
    )
    parser.add_argument("--peak", type=float, default=1024.0,
                        help="radiance peak for each Gaussian light")
    args = parser.parse_args()
    if not math.isfinite(args.peak) or args.peak <= 0.0:
        parser.error("--peak must be finite and greater than zero")

    width, height = 640, 360
    # Estimated from the red and green source markers in Fig. 10's Angenieux
    # panel. They are geometric comparison points, not authors' published data.
    lights = ((0.246, 0.356, 2.2), (0.390, 0.693, 2.2))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("wb") as image:
        image.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        for y in range(height - 1, -1, -1):
            for x in range(width):
                radiance = 0.0
                for u, v, sigma in lights:
                    dx = x - u * (width - 1)
                    dy = y - v * (height - 1)
                    radiance += args.peak * math.exp(
                        -(dx * dx + dy * dy) / (2.0 * sigma * sigma)
                    )
                image.write(struct.pack("<3f", radiance, radiance, radiance))
    print(args.output)


if __name__ == "__main__":
    main()
