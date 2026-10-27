"""Generate a deterministic multi-light HDR input for LensFlareLab."""

import argparse
import math
import struct
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="PFM output path")
    parser.add_argument("--light-scale", type=float, default=1.0,
                        help="multiply light-source radiance only (default: 1)")
    parser.add_argument("--source-only", action="store_true",
                        help="omit the constant background for isolated source-response studies")
    parser.add_argument("--single-light-position", type=float, nargs=2, metavar=("U", "V"),
                        help="replace the chart lights with one warm source at normalized image coordinates")
    parser.add_argument("--source-radius", type=float, default=7.0,
                        help="Gaussian scale parameter in pixels for the single-light source (1..8, default 7)")
    parser.add_argument("--single-light-neutral", action="store_true",
                        help="use an equal-energy RGB source for spectral dispersion evaluation")
    args = parser.parse_args()
    if not math.isfinite(args.light_scale) or args.light_scale <= 0:
        parser.error("--light-scale must be finite and greater than zero")
    if args.single_light_position is not None and any(
        not math.isfinite(value) or value < 0.0 or value > 1.0
        for value in args.single_light_position
    ):
        parser.error("--single-light-position coordinates must be in the range 0..1")
    if not math.isfinite(args.source_radius) or not 1.0 <= args.source_radius <= 8.0:
        parser.error("--source-radius must be in the range 1..8 pixels")

    width, height = 640, 360
    pixels = [[0.0, 0.0, 0.0] for _ in range(width * height)]
    if not args.source_only:
        for y in range(height):
            for x in range(width):
                u = (x - (width - 1) * 0.5) / ((width - 1) * 0.5)
                v = (y - (height - 1) * 0.5) / ((height - 1) * 0.5)
                vignette = max(0.78, 1.0 - 0.18 * (u * u + v * v))
                pixels[y * width + x] = [
                    vignette * 0.012,
                    vignette * 0.016,
                    vignette * 0.022,
                ]

    lights = (
        (245, 138, 7, (1.0, 0.83, 0.58), 4.5),
        (118, 95, 5, (0.46, 0.72, 1.0), 3.5),
        (520, 250, 6, (1.0, 0.36, 0.20), 3.8),
        (420, 78, 4, (0.55, 1.0, 0.65), 3.1),
    )
    if args.single_light_position is not None:
        u, v = args.single_light_position
        lights = ((round(u * (width - 1)), round(v * (height - 1)), args.source_radius,
                   (1.0, 1.0, 1.0) if args.single_light_neutral else (1.0, 0.83, 0.58), 4.5),)
    elif args.single_light_neutral:
        parser.error("--single-light-neutral requires --single-light-position")
    for cx, cy, radius, color, peak in lights:
        for y in range(max(0, cy - 20), min(height, cy + 21)):
            for x in range(max(0, cx - 20), min(width, cx + 21)):
                distance2 = (x - cx) ** 2 + (y - cy) ** 2
                if distance2 > 400:
                    continue
                contribution = args.light_scale * peak * math.exp(
                    -distance2 / (radius * radius * 0.48))
                pixel = pixels[y * width + x]
                for channel in range(3):
                    pixel[channel] += color[channel] * contribution

    output = args.output or Path(__file__).resolve().parent / "evaluation-chart.pfm"
    with output.open("wb") as image:
        image.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        for y in range(height - 1, -1, -1):
            for pixel in pixels[y * width : (y + 1) * width]:
                image.write(struct.pack("<3f", *pixel))
    print(output)


if __name__ == "__main__":
    main()
