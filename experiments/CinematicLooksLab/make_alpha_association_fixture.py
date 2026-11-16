#!/usr/bin/env python3
"""Generate a deterministic straight-alpha PNG for LUT association review."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
ASSET = ROOT / "assets" / "alpha-association-chart.png"
MANIFEST = ROOT / "assets" / "alpha-association-chart.json"
WIDTH = 1280
ROW_HEIGHT = 96
COLORS = (
    ("neutral_mid", (128, 128, 128)),
    ("skin_approximation", (199, 122, 89)),
    ("red", (220, 32, 28)),
    ("green", (24, 184, 58)),
    ("blue", (32, 72, 220)),
    ("teal", (20, 176, 180)),
    ("amber", (242, 166, 38)),
    ("dark_neutral", (26, 26, 26)),
)
FIXED_ALPHA = (0, 26, 64, 128, 192, 255)


def main() -> None:
    image = np.zeros((ROW_HEIGHT * len(COLORS), WIDTH, 4), dtype=np.uint8)
    fixed_width = 72
    ramp_start = fixed_width * len(FIXED_ALPHA) + 32
    ramp = np.rint(np.linspace(0, 255, WIDTH - ramp_start)).astype(np.uint8)
    for row, (_, color) in enumerate(COLORS):
        y0 = row * ROW_HEIGHT
        for index, alpha in enumerate(FIXED_ALPHA):
            x0 = index * fixed_width
            image[y0:y0 + ROW_HEIGHT, x0:x0 + fixed_width, :3] = color
            image[y0:y0 + ROW_HEIGHT, x0:x0 + fixed_width, 3] = alpha
        image[y0:y0 + ROW_HEIGHT, ramp_start:, :3] = color
        image[y0:y0 + ROW_HEIGHT, ramp_start:, 3] = ramp[None, :]

    ASSET.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(image, mode="RGBA").save(ASSET, format="PNG", optimize=False)
    digest = hashlib.sha256(ASSET.read_bytes()).hexdigest()
    manifest = {
        "kind": "synthetic evaluation fixture; not a ground-truth look image",
        "alpha_association": "straight/unassociated RGB values encoded in PNG",
        "color_interpretation": "RGB byte code values intended as sRGB; no embedded ICC profile",
        "size": [WIDTH, ROW_HEIGHT * len(COLORS)],
        "sha256": digest,
        "fixed_alpha_bytes": list(FIXED_ALPHA),
        "fixed_patch_width": fixed_width,
        "ramp_start_x": ramp_start,
        "ramp": "each row uses a monotonic horizontal 0..255 alpha ramp with constant RGB",
        "rows_top_to_bottom": [
            {"id": name, "rgb8": list(color)} for name, color in COLORS
        ],
        "generation_script": "experiments/CinematicLooksLab/make_alpha_association_fixture.py",
    }
    with MANIFEST.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(manifest, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(f"Wrote {ASSET} ({WIDTH}x{ROW_HEIGHT * len(COLORS)}, sha256={digest})")


if __name__ == "__main__":
    main()
