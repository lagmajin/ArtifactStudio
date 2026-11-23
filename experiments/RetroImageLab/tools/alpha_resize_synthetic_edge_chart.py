"""Generate a deterministic RGBA chart to probe alpha resize footprints."""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "fixtures" / "alpha-resize-edge-chart.png"


def main() -> dict[str, object]:
    width, height, supersample = 768, 512, 4
    image = Image.new("RGBA", (width * supersample, height * supersample), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    # Four bands isolate hard edges, antialiased curves, fine strands, and partial alpha.
    draw.rectangle((24, 24, 360, 104), fill=(224, 58, 42, 255))
    draw.rectangle((400, 24, 744, 104), fill=(42, 118, 232, 255))
    draw.ellipse((48, 136, 248, 312), fill=(44, 206, 122, 255))
    draw.polygon([(310, 310), (500, 132), (706, 312)], fill=(238, 194, 56, 255))
    # Thin strands of several orientations and widths.
    for i, width_px in enumerate((1, 2, 3, 5, 8)):
        x = 28 + i * 37
        draw.line((x * supersample, 360 * supersample, (x + 180) * supersample, 480 * supersample),
                  fill=(255, 255, 255, 255), width=width_px * supersample)
    for i, width_px in enumerate((1, 2, 4, 7)):
        x = 260 + i * 45
        draw.line((x * supersample, 480 * supersample, (x + 150) * supersample, 350 * supersample),
                  fill=(160, 50, 210, 180), width=width_px * supersample)
    # Narrow gaps between unlike solids and multiple alpha levels.
    draw.rectangle((570 * supersample, 348 * supersample, 630 * supersample, 480 * supersample), fill=(245, 80, 45, 255))
    draw.rectangle((632 * supersample, 348 * supersample, 700 * supersample, 480 * supersample), fill=(35, 190, 235, 255))
    for gap in (1, 2, 4, 8):
        x = 568 + gap * 10
        draw.rectangle((x * supersample, 350 * supersample, (x + gap) * supersample, 476 * supersample), fill=(0, 0, 0, 0))
    for i, alpha in enumerate((48, 96, 160, 224)):
        x0 = 24 + i * 86
        draw.rounded_rectangle((x0 * supersample, 324 * supersample, (x0 + 72) * supersample, 350 * supersample),
                               radius=8 * supersample, fill=(30, 220, 185, alpha))
    # Controlled hidden RGB variants: identical alpha/visible samples, different alpha-zero RGB.
    rgba = np.asarray(image.resize((width, height), Image.Resampling.LANCZOS), dtype=np.uint8).copy()
    invisible = rgba[:, :, 3] == 0
    rgba[invisible, :3] = (9, 240, 17)
    FIXTURE.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, "RGBA").save(FIXTURE)
    return {
        "fixture": str(FIXTURE.relative_to(ROOT)), "dimensions": [width, height],
        "construction": "4x supersampled deterministic geometric chart, Lanczos downsampled; RGB hidden under exact zero alpha set to (9,240,17)",
        "regions": {
            "hard colored rectangles": [24, 24, 744, 105],
            "curved/diagonal edges": [40, 128, 712, 320],
            "partial alpha and strands": [20, 320, 480, 492],
            "1/2/4/8 px gaps and unlike colors": [560, 340, 710, 492],
        },
        "notes": "Synthetic control, not a photographic proxy or product-quality target. Hidden RGB is intentionally assigned only at alpha zero after antialiasing.",
    }


if __name__ == "__main__":
    print(json.dumps(main(), indent=2))
