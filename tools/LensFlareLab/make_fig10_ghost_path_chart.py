"""Visualize per-path sensor bounds from the Fig. 10 Angenieux diagnostic CSV."""

import argparse
import csv
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, nargs="?", default=Path("tools/LensFlareLab/examples/fig10-ghost-paths.csv"))
    parser.add_argument("--output", type=Path, default=Path("tools/LensFlareLab/examples/fig10-ghost-path-layout.png"))
    args = parser.parse_args()
    with args.csv.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    sources = sorted({(row["source_u"], row["source_v"]) for row in rows})
    if not rows or not sources:
        parser.error("CSV contains no ghost path rows")

    panel_w, panel_h, margin, header = 640, 360, 30, 52
    image = Image.new("RGB", (margin * 2 + panel_w * len(sources), header + panel_h + margin), "#10141c")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    colors = ((73, 180, 255), (255, 174, 74), (175, 122, 255), (96, 222, 173))
    for panel, source in enumerate(sources):
        x0, y0 = margin + panel * panel_w, header
        draw.text((x0, 14), f"Source ({float(source[0]):.3f}, {float(source[1]):.3f})  |  {sum((r['source_u'], r['source_v']) == source for r in rows)} visible paths", fill="#f1f4fa", font=font)
        draw.rectangle((x0, y0, x0 + panel_w - 1, y0 + panel_h - 1), outline="#687384")
        group = [r for r in rows if (r["source_u"], r["source_v"]) == source]
        group.sort(key=lambda r: float(r["integrated_luminance"]), reverse=True)
        for rank, row in enumerate(group):
            color = colors[(int(row["first_surface"]) + int(row["second_surface"])) % len(colors)]
            alpha = 255 if rank < 8 else 62
            color = tuple(int(c * alpha / 255 + 16 * (1 - alpha / 255)) for c in color)
            left, top, right, bottom = (float(row[key]) for key in ("min_x", "min_y", "max_x", "max_y"))
            box = (x0 + left, y0 + top, x0 + right, y0 + bottom)
            draw.rectangle(box, outline=color, width=2 if rank < 8 else 1)
            if rank < 6:
                pair = f"{row['first_surface']}→{row['second_surface']}"
                draw.text((x0 + left + 2, y0 + top + 2), pair, fill=color, font=font)
    draw.text((margin, image.height - 16), "Bounds are sensor-space raster extents; labels show the six brightest paths per source by integrated luminance.", fill="#aeb8c7", font=font)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)
    print(args.output)


if __name__ == "__main__":
    main()
