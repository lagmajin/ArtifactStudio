"""Generate a procedural lens-aperture mask animation.

The output is a black background with white aperture samples.  At low spread
the samples form a six-sided aperture; at high spread they expand into rings
of circular samples.  The result is intended as a low-cost source mask for
FFT/diffraction experiments.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def polygon_radius(angle: float, sides: int, radius: float) -> float:
    """Return the radial boundary of a regular polygon at ``angle``."""
    sector = 2.0 * math.pi / sides
    local = (angle + sector * 0.5) % sector - sector * 0.5
    return radius * math.cos(math.pi / sides) / max(1e-6, math.cos(local))


def aperture_point(angle: float, radius: float, spread: float) -> tuple[float, float]:
    hex_radius = polygon_radius(angle, 6, radius)
    circle_radius = radius
    r = lerp(hex_radius, circle_radius, spread)
    return math.cos(angle) * r, math.sin(angle) * r


def draw_frame(size: tuple[int, int], frame: int, frames: int, samples: int) -> Image.Image:
    width, height = size
    image = Image.new("L", size, 0)
    draw = ImageDraw.Draw(image)
    cx, cy = width * 0.5, height * 0.5
    phase = frame / max(1, frames - 1)
    # Smoothly move from a compact hexagonal aperture to a wide circular one.
    spread = phase * phase * (3.0 - 2.0 * phase)
    base_radius = min(width, height) * 0.16
    ring_count = 5

    for ring in range(1, ring_count + 1):
        ring_t = ring / ring_count
        ring_radius = base_radius * ring_t
        count = max(6, int(samples * ring_t))
        dot_radius = lerp(2.0, 1.0, ring_t) * lerp(1.15, 0.82, spread)
        for index in range(count):
            angle = 2.0 * math.pi * index / count + ring * 0.17
            x, y = aperture_point(angle, ring_radius, spread)
            # The outer samples travel outward as the aperture opens.
            x *= lerp(0.9, 2.8, spread)
            y *= lerp(0.9, 2.8, spread)
            px, py = cx + x, cy + y
            box = (px - dot_radius, py - dot_radius, px + dot_radius, py + dot_radius)
            draw.ellipse(box, fill=255)

    # Central aperture: a filled interpolated hexagon/circle, kept subtle so
    # the individual white samples remain visible in the diffraction mask.
    center_radius = base_radius * lerp(0.34, 0.18, spread)
    points = []
    for index in range(48):
        angle = 2.0 * math.pi * index / 48.0
        x, y = aperture_point(angle, center_radius, spread)
        points.append((cx + x, cy + y))
    draw.polygon(points, fill=255)
    return image


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("out/aperture_mask"))
    parser.add_argument("--width", type=int, default=237)
    parser.add_argument("--height", type=int, default=222)
    parser.add_argument("--frames", type=int, default=72)
    parser.add_argument("--samples", type=int, default=32)
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    frames = [
        draw_frame((args.width, args.height), i, args.frames, args.samples)
        for i in range(args.frames)
    ]
    for index, image in enumerate(frames):
        image.save(args.output / f"mask_{index:03d}.png")
    frames[0].save(
        args.output / "aperture_mask.gif",
        save_all=True,
        append_images=frames[1:],
        duration=33,
        loop=0,
        optimize=False,
    )
    print(f"generated {len(frames)} frames in {args.output}")


if __name__ == "__main__":
    main()
