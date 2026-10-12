#!/usr/bin/env python3
"""Run the layered mask contract executable and assemble its PNG planes."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=pathlib.Path, help="MaskVisualTest executable")
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("mask-visual-output"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(args.executable.resolve()), str(args.output.resolve())], check=False)
    if result.returncode:
        return result.returncode

    try:
        from PIL import Image, ImageDraw, ImageFont
    except ImportError:
        print("Pillow is required to write the PNG atlas", file=sys.stderr)
        return 2

    cases = [
        "01_single_base", "02_add_opacity", "03_subtract", "04_intersect",
        "05_difference", "06_expansion", "07_contraction", "08_feather",
        "09_inverted", "10_stack_parameter_matrix",
    ]
    cell_w, cell_h, title_h = 320, 240, 28
    columns = 3
    rows = (len(cases) + columns - 1) // columns
    atlas = Image.new("RGB", (columns * cell_w, rows * (cell_h + title_h)), "#282a30")
    draw = ImageDraw.Draw(atlas)
    font = ImageFont.load_default()
    for index, name in enumerate(cases):
        image_path = args.output / f"{name}.png"
        with Image.open(image_path) as source:
            image = source.convert("RGB")
        image.thumbnail((cell_w - 16, cell_h - 16), Image.Resampling.NEAREST)
        x = (index % columns) * cell_w
        y = (index // columns) * (cell_h + title_h)
        draw.text((x + 8, y + 7), name, font=font, fill="white")
        atlas.paste(image, (x + (cell_w - image.width) // 2, y + title_h + (cell_h - image.height) // 2))
    output = args.output / "mask-visual-atlas.png"
    atlas.save(output)
    print(f"Wrote {output.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
