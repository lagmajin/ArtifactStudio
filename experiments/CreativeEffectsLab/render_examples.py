"""Generate deterministic CPU reference images for visual review."""

import hashlib
import json
import platform
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from reference import neon_ignition, paper_cutout, time_mosaic


def render(size, destination):
    height, width = size
    scale = width / 640
    destination.mkdir(parents=True, exist_ok=True)
    records = {}

    def save(name, rgba):
        rgb, alpha = rgba[..., :3], rgba[..., 3:]
        record = {
            "sha256_float_rgba": hashlib.sha256(rgba.tobytes()).hexdigest(),
            "nonfinite": int(np.count_nonzero(~np.isfinite(rgba))),
            "min_rgba": rgba.min(axis=(0, 1)).tolist(),
            "max_rgba": rgba.max(axis=(0, 1)).tolist(),
        }
        records[name] = record
        for suffix, background in (("dark", 0.015), ("white", 1.0)):
            display = np.clip(rgb * alpha + background * (1 - alpha), 0, 1)
            srgb = np.where(display <= 0.0031308, display * 12.92,
                            1.055 * display ** (1 / 2.4) - 0.055)
            Image.fromarray(np.rint(srgb * 255).astype(np.uint8)).save(
                destination / f"{name}-{suffix}.png")

    artwork = Image.new("RGBA", (width, height))
    draw = ImageDraw.Draw(artwork)
    draw.ellipse(tuple(round(n * scale) for n in (85, 35, 550, 321)),
                 fill=(255, 125, 61, 255))
    draw.polygon([(round(x * scale), round(y * scale))
                  for x, y in [(120, 254), (320, 72), (511, 250)]],
                 fill=(36, 112, 105, 255))
    draw.line((150 * scale, 250 * scale, 490 * scale, 250 * scale),
              fill=(255, 234, 188, 190), width=1)
    paper_source = np.asarray(artwork, dtype=np.float32) / 255
    palette = np.array([[1, .49, .24], [.14, .44, .41], [1, .92, .74]])
    save("paper-input", paper_source)
    save("paper-flat", paper_cutout(paper_source, palette, thickness=0))
    save("paper-cutout", paper_cutout(paper_source, palette, round(4 * scale),
                                     (round(7 * scale), round(8 * scale))))

    curves = np.array([[[56, 220], [100, 220], [148, 220], [190, 220]],
                       [[190, 220], [245, 40], [330, 40], [366, 220]],
                       [[366, 220], [405, 350], [495, 84], [580, 173]]]) * scale
    for progress in (0, .25, .5, .75, 1):
        save(f"neon-{progress}", neon_ignition(size, curves, progress,
                                               8 * scale, [.12, .7, 1], 1.4))

    def frame(number):
        card = Image.new("RGBA", (width, height), (20, 28, 42, 255))
        draw = ImageDraw.Draw(card)
        for r in range(3):
            for c in range(3):
                x = c * width // 3 + round((20 + number * 5) * scale)
                y = r * height // 3 + round(32 * scale)
                radius = round(15 * scale)
                draw.ellipse((x-radius, y-radius, x+radius, y+radius),
                             fill=(40 + number * 8, 200, 150, 255))
                draw.text((c * width // 3 + 10, y + radius + 10),
                          f"Frame {number:02d}", fill="white")
        return np.asarray(card, dtype=np.float32) / 255

    save("mosaic-input-23", frame(23))
    for order in ("row", "column", "snake"):
        save(f"mosaic-{order}", time_mosaic(frame, 23, 0, delay=2, order=order))
    save("mosaic-start", time_mosaic(frame, 3, 0, delay=2))
    manifest = {"size": [width, height], "python": platform.python_version(),
                "numpy": np.__version__, "images": records}
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2),
                                               encoding="utf-8")


if __name__ == "__main__":
    root = Path(__file__).resolve().parent / "captures"
    for width in (640, 1280):
        render((width * 9 // 16, width), root / str(width))
