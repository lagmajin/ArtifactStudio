#!/usr/bin/env python3
"""Generate standalone PNG test patterns without launching ArtifactStudio."""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw


def color_bars(width: int, height: int) -> Image.Image:
    colors = [
        (192, 192, 192), (192, 192, 0), (0, 192, 192), (0, 192, 0),
        (192, 0, 192), (192, 0, 0), (0, 0, 192),
    ]
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for x in range(width):
        color = colors[min(len(colors) - 1, x * len(colors) // width)]
        for y in range(height):
            pixels[x, y] = color
    return image


def color_wheel(width: int, height: int) -> Image.Image:
    image = Image.new("RGB", (width, height), (32, 32, 32))
    pixels = image.load()
    cx, cy = (width - 1) / 2, (height - 1) / 2
    radius = min(width, height) * 0.48
    for y in range(height):
        for x in range(width):
            dx, dy = x - cx, cy - y
            distance = math.hypot(dx, dy) / radius
            if distance <= 1:
                hue = (math.degrees(math.atan2(dy, dx)) % 360) / 60
                chroma = distance
                sector = int(hue)
                fraction = hue - sector
                p, q, t = 1 - chroma, 1 - chroma * fraction, 1 - chroma * (1 - fraction)
                rgb = [(1, t, p), (q, 1, p), (p, 1, t), (p, q, 1), (t, p, 1), (1, p, q)][sector]
                pixels[x, y] = tuple(round(channel * 255) for channel in rgb)
    return image


def gradient(width: int, height: int) -> Image.Image:
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for x in range(width):
        value = round(255 * x / max(1, width - 1))
        for y in range(height):
            pixels[x, y] = (value, value, value)
    return image


def checkerboard(width: int, height: int, cell: int) -> Image.Image:
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for y in range(height):
        for x in range(width):
            value = 224 if (x // cell + y // cell) % 2 == 0 else 48
            pixels[x, y] = (value, value, value)
    return image


PATTERNS = {
    "bars": color_bars,
    "wheel": color_wheel,
    "gradient": gradient,
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pattern", choices=[*PATTERNS, "checkerboard", "all"], default="all")
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--cell-size", type=int, default=48, help="Checkerboard cell size")
    parser.add_argument("--output-dir", type=Path, default=Path("test-images"))
    args = parser.parse_args()

    if args.width < 1 or args.height < 1 or args.cell_size < 1:
        parser.error("width, height, and cell-size must be positive")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    patterns = [*PATTERNS, "checkerboard"] if args.pattern == "all" else [args.pattern]
    for name in patterns:
        if name == "checkerboard":
            image = checkerboard(args.width, args.height, args.cell_size)
        else:
            image = PATTERNS[name](args.width, args.height)
        destination = args.output_dir / f"{name}_{args.width}x{args.height}.png"
        image.save(destination, format="PNG")
        print(destination.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
