#!/usr/bin/env python3
"""Generate deterministic source-crop coordinate fixtures and expected geometry.

This is a fixture generator only. It does not execute Artifact or claim runtime
parity. PNG pixels encode source coordinates so crop/pan/zoom results can be
measured against the adjacent JSON manifest.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def build_fixture(width: int, height: int, cell: int) -> tuple[Image.Image, dict]:
    image = Image.new("RGB", (width, height), "#101820")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    # Asymmetric quadrants make sign flips and axis swaps immediately visible.
    colors = {
        "TL": "#e63946",
        "TR": "#2a9d8f",
        "BL": "#457b9d",
        "BR": "#f4a261",
    }
    for y in range(height):
        for x in range(width):
            q = ("T" if y < height / 2 else "B") + ("L" if x < width / 2 else "R")
            # Low-amplitude coordinate encoding survives common display transforms.
            image.putpixel((x, y), tuple(
                min(255, int(colors[q][i:i + 2], 16) + ((x // 16 + y // 16) % 3) * 8)
                for i in (1, 3, 5)
            ))
    # Pixel-coordinate grid; axes and nonuniform tick intervals reveal scale.
    for x in range(0, width, cell):
        major = (x // cell) % 4 == 0
        color = "#ffffff" if major else "#a8b3bf"
        draw.line((x, 0, x, height - 1), fill=color, width=2 if major else 1)
        if major and x + 42 < width:
            draw.text((x + 3, 4), f"x={x}", fill="white", font=font,
                      stroke_width=1, stroke_fill="#101820")
    for y in range(0, height, cell):
        major = (y // cell) % 4 == 0
        color = "#ffffff" if major else "#a8b3bf"
        draw.line((0, y, width - 1, y), fill=color, width=2 if major else 1)
        if major and y + 12 < height:
            draw.text((4, y + 2), f"y={y}", fill="white", font=font,
                      stroke_width=1, stroke_fill="#101820")
    draw.rectangle((1, 1, width - 2, height - 2), outline="#ffffff", width=3)
    draw.text((width // 2 - 48, height // 2 - 8), "SOURCE CENTER",
              fill="white", font=font, stroke_width=2, stroke_fill="#101820")
    manifest = {
        "fixture": "source_crop_coordinate_grid",
        "dimensions_px": [width, height],
        "cell_spacing_px": cell,
        "coordinate_contract": "cropRect and pan are source pixels; zoom=1 is neutral",
        "quadrants": colors,
        "scenarios": [
            {"id": "neutral", "cropRect": [0, 0, width, height], "pan": [0, 0], "zoom": 1,
             "expected_effectiveRect": [0, 0, width, height]},
            {"id": "center_half", "cropRect": [0, 0, width, height], "pan": [0, 0], "zoom": 2,
             "expected_effectiveRect": [width / 4, height / 4, width / 2, height / 2]},
            {"id": "pan_right_down", "cropRect": [0, 0, width, height], "pan": [32, 24], "zoom": 2,
             "expected_effectiveRect": [width / 4 + 32, height / 4 + 24, width / 2, height / 2]},
            {"id": "pan_left_up", "cropRect": [0, 0, width, height], "pan": [-32, -24], "zoom": 2,
             "expected_effectiveRect": [width / 4 - 32, height / 4 - 24, width / 2, height / 2]},
            {"id": "explicit_crop", "cropRect": [64, 48, width / 2, height / 2], "pan": [0, 0], "zoom": 1,
             "expected_effectiveRect": [64, 48, width / 2, height / 2]},
        ],
        "notes": [
            "Expected rectangles are source pixel coordinates [x,y,width,height].",
            "With preserveAspect=true and matching source/output aspect, expected output fills the original layer bounds.",
            "The application output still must be captured and measured; generated fixture alone does not prove runtime behavior."
        ]
    }
    return image, manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, default=Path("tests/fixtures/source_crop"))
    parser.add_argument("--width", type=int, default=512)
    parser.add_argument("--height", type=int, default=384)
    parser.add_argument("--cell", type=int, default=16)
    args = parser.parse_args()
    if args.width <= 0 or args.height <= 0 or args.cell <= 0:
        parser.error("width, height, and cell must be positive")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    image, manifest = build_fixture(args.width, args.height, args.cell)
    image.save(args.out_dir / "source_crop_coordinate_grid.png")
    (args.out_dir / "source_crop_coordinate_grid.expected.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    print(f"Wrote fixture and manifest to {args.out_dir}")


if __name__ == "__main__":
    main()
