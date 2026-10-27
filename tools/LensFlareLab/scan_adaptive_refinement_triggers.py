"""Map which adaptive pupil-refinement criteria trigger across ghost paths and fields."""

import argparse
import csv
import re
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SURFACE_COUNT = 14
FIELDS = (("center", 0.50, 0.50), ("upper-left", 0.26, 0.26),
          ("top-edge", 0.50, 0.02), ("lower-right", 0.74, 0.74))
REPORT = re.compile(r"centerRays=(\d+) edgeRays=(\d+) refinedCells=(\d+).*?maxMappingError=([\deE.+-]+)px maxEnergyRelativeError=([\deE.+-]+)% triggers\(mapping=(\d+), energy=(\d+), validity=(\d+), foldover=(\d+)\)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/adaptive-trigger-scan"))
    parser.add_argument("--ray-grid", type=int, default=64)
    parser.add_argument("--screen-threshold", type=float, default=0.75)
    parser.add_argument("--energy-threshold", type=float, default=0.25)
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-trigger-map.csv"))
    parser.add_argument("--image-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-trigger-map.png"))
    args = parser.parse_args()
    if not (8 <= args.ray_grid <= 512):
        parser.error("--ray-grid must be in [8, 512]")
    exe = args.executable.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    rows = []
    paths = [(first, second) for first in range(1, SURFACE_COUNT)
             for second in range(first) if not (first >= 7 and second <= 6)]
    for field, u, v in FIELDS:
        source = args.output_dir / f"{field}-source.pfm"
        subprocess.run([sys.executable, str(maker), "--output", str(source), "--source-only",
                        "--single-light-position", str(u), str(v), "--single-light-neutral",
                        "--source-radius", "1", "--light-scale", "1024"],
                       check=True, capture_output=True, text=True)
        for first, second in paths:
            output = args.output_dir / f"{field}-{first}-{second}.pfm"
            command = [str(exe), str(source.resolve()), str(output.resolve()), "--position", str(u), str(v),
                       "--source-samples", "1", "--ray-grid", str(args.ray_grid), "--gain", "1",
                       "--diffraction", "off", "--ghost-diffraction", "off", "--ghost-pair",
                       str(first), str(second), "--adaptive-ray-refinement", "on",
                       "--adaptive-ray-refinement-threshold", str(args.screen_threshold),
                       "--adaptive-ray-refinement-energy-threshold", str(args.energy_threshold),
                       "--ghost-report"]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"render failed for {field} {first}->{second}:\n{result.stdout}\n{result.stderr}")
            match = REPORT.search(result.stdout)
            if not match:
                raise RuntimeError(f"refinement diagnostic missing for {field} {first}->{second}:\n{result.stdout}")
            values = [int(value) for value in match.groups()[:3]]
            triggers = [int(value) for value in match.groups()[5:]]
            row = {"field": field, "source_u": u, "source_v": v,
                   "first_surface": first, "second_surface": second,
                   "ray_grid": args.ray_grid, "screen_threshold_px": args.screen_threshold,
                   "energy_threshold_fraction": args.energy_threshold,
                   "center_rays": values[0], "edge_rays": values[1], "refined_cells": values[2],
                   "max_mapping_error_px": float(match.group(4)),
                   "max_energy_relative_error_percent": float(match.group(5)),
                   "mapping_triggers": triggers[0], "energy_triggers": triggers[1],
                   "validity_boundary_triggers": triggers[2], "foldover_triggers": triggers[3]}
            rows.append(row)
            if row["refined_cells"]:
                print(f"{field} {first}->{second}: refined={row['refined_cells']} "
                      f"map={row['mapping_triggers']} energy={row['energy_triggers']} "
                      f"validity={row['validity_boundary_triggers']} fold={row['foldover_triggers']}", flush=True)

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    cell_w, cell_h, left, header = 88, 17, 82, 30
    image = Image.new("RGB", (left + cell_w * len(FIELDS) + 4, header + cell_h * len(paths) + 20), "#11151d")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    lookup = {(r["first_surface"], r["second_surface"], r["field"]): r for r in rows}
    for col, (field, _, _) in enumerate(FIELDS):
        draw.text((left + col * cell_w + 3, 8), field, fill="#e7edf5", font=font)
    for row_index, (first, second) in enumerate(paths):
        y = header + row_index * cell_h
        draw.text((2, y + 3), f"{first:02d}→{second:02d}", fill="#c5ceda", font=font)
        for col, (field, _, _) in enumerate(FIELDS):
            record = lookup[(first, second, field)]
            total = record["refined_cells"]
            triggers = (record["mapping_triggers"], record["energy_triggers"],
                        record["validity_boundary_triggers"], record["foldover_triggers"])
            colors = ((182, 119, 235), (246, 176, 79), (76, 155, 220), (239, 91, 98))
            if total == 0:
                color = (32, 38, 47)
            else:
                active = [colors[i] for i, count in enumerate(triggers) if count]
                color = tuple(sum(c[k] for c in active) // len(active) for k in range(3))
                strength = min(1.0, total / 280.0)
                color = tuple(int(32 + (value - 32) * strength) for value in color)
            x = left + col * cell_w
            draw.rectangle((x, y, x + cell_w - 2, y + cell_h - 2), fill=color)
            if total:
                draw.text((x + 3, y + 3), f"{total:4d}", fill="#f4f6fa", font=font)
    draw.text((left, image.height - 14), "Purple: mapping | amber: energy | blue: validity boundary | red: foldover", fill="#b7c0cd", font=font)
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.image_output)
    nonzero = sum(1 for row in rows if row["refined_cells"])
    print(f"nonzero path-field maps={nonzero}/{len(rows)}")
    print(args.csv_output)
    print(args.image_output)


if __name__ == "__main__":
    main()
