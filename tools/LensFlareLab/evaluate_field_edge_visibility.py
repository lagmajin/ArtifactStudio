"""Survey all 42 ghost paths for visibility and sensor-edge contact at four image edges."""

import argparse
import csv
import math
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


SURFACES = 14
GHOST_PAIRS = tuple((first, second) for first in range(1, SURFACES)
                    for second in range(first) if not (first >= 7 and second <= 6))
POSITIONS = {
    "top": [(u, 0.02) for u in (0.10, 0.30, 0.50, 0.70, 0.90)],
    "right": [(0.98, v) for v in (0.10, 0.30, 0.50, 0.70, 0.90)],
    "bottom": [(u, 0.98) for u in (0.10, 0.30, 0.50, 0.70, 0.90)],
    "left": [(0.02, v) for v in (0.10, 0.30, 0.50, 0.70, 0.90)],
}
EDGE_NAMES = (("left", 1), ("top", 2), ("right", 4), ("bottom", 8))
COLORS = {0: (57, 120, 174), 1: (242, 190, 75), 2: (242, 116, 80),
          4: (193, 122, 240), 8: (78, 194, 151)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/edge-visibility"))
    parser.add_argument("--ray-grid", type=int, default=96)
    parser.add_argument("--source-radius", type=float, default=1.0)
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/field-edge-visibility.csv"))
    parser.add_argument("--image-output", type=Path, default=Path("tools/LensFlareLab/examples/field-edge-visibility.png"))
    args = parser.parse_args()
    exe = args.executable.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    pair_rows = {}
    test_columns = []
    for side, positions in POSITIONS.items():
        for sample_index, (u, v) in enumerate(positions):
            key = f"{side}-{sample_index}"
            test_columns.append((key, side, u, v))
            input_path = (args.output_dir / f"{key}-input.pfm").resolve()
            output_path = (args.output_dir / f"{key}-render.pfm").resolve()
            report_path = (args.output_dir / f"{key}-paths.csv").resolve()
            subprocess.run([sys.executable, str(maker), "--output", str(input_path),
                            "--source-only", "--single-light-position", f"{u:.6f}", f"{v:.6f}",
                            "--single-light-neutral", "--source-radius", str(args.source_radius),
                            "--light-scale", "1024"], check=True, capture_output=True, text=True)
            command = [str(exe), str(input_path), str(output_path), "--position", f"{u:.6f}", f"{v:.6f}",
                       "--source-samples", "1", "--ray-grid", str(args.ray_grid), "--gain", "1",
                       "--diffraction", "off", "--ghost-report-csv", str(report_path)]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"render failed at {side} field ({u:.3f},{v:.3f}):\n{result.stderr}\n{result.stdout}")
            with report_path.open(newline="", encoding="utf-8") as stream:
                visible = list(csv.DictReader(stream))
            for row in visible:
                first, second = int(row["first_surface"]), int(row["second_surface"])
                min_x, min_y = float(row["min_x"]), float(row["min_y"])
                max_x, max_y = float(row["max_x"]), float(row["max_y"])
                mask = ((1 if min_x <= 1 else 0) | (2 if min_y <= 1 else 0) |
                        (4 if max_x >= 638 else 0) | (8 if max_y >= 358 else 0))
                pair_rows[(first, second, key)] = {
                    "side": side, "source_u": u, "source_v": v, "visible": True,
                    "boundary_mask": mask, "integrated_luminance": float(row["integrated_luminance"]),
                    "centroid_x": float(row["centroid_x"]), "centroid_y": float(row["centroid_y"]),
                    "min_x": min_x, "min_y": min_y, "max_x": max_x, "max_y": max_y,
                }
            boundary_count = sum(1 for row in visible if min(float(row["min_x"]), float(row["min_y"])) <= 1 or
                                 float(row["max_x"]) >= 638 or float(row["max_y"]) >= 358)
        print(f"edge={side} source=({u:.2f},{v:.2f}) visible={len(visible)}/42 boundary_near={boundary_count}")

    output_rows = []
    for first, second in GHOST_PAIRS:
        for key, side, u, v in test_columns:
            value = pair_rows.get((first, second, key), {})
            output_rows.append({"side": side, "source_u": u, "source_v": v,
                                "first_surface": first, "second_surface": second,
                                "visible": value.get("visible", False),
                                "boundary_mask": value.get("boundary_mask", 0),
                                "integrated_luminance": value.get("integrated_luminance", 0.0),
                                "centroid_x": value.get("centroid_x", 0.0),
                                "centroid_y": value.get("centroid_y", 0.0),
                                "min_x": value.get("min_x", ""), "min_y": value.get("min_y", ""),
                                "max_x": value.get("max_x", ""), "max_y": value.get("max_y", "")})

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(output_rows[0]))
        writer.writeheader()
        writer.writerows(output_rows)

    cell_w, cell_h, left_w, header_h, top_h = 32, 13, 60, 34, 26
    image = Image.new("RGB", (left_w + len(test_columns) * cell_w + 12,
                               header_h + top_h + 42 * cell_h + 20), "#11151d")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    for column, (key, side, u, v) in enumerate(test_columns):
        x = left_w + column * cell_w
        if column == 0 or test_columns[column - 1][1] != side:
            draw.text((x + 2, 5), side, fill="#f0f3f8", font=font)
        draw.text((x + 3, header_h), f"{u:.1f}" if side in ("top", "bottom") else f"{v:.1f}",
                  fill="#aeb8c7", font=font)
    max_log_luminance = max((math.log1p(value["integrated_luminance"]) for value in pair_rows.values()),
                            default=1.0)
    row_index = 0
    for first, second in GHOST_PAIRS:
        y = header_h + top_h + row_index * cell_h
        draw.text((2, y + 2), f"{first:02d}-{second:02d}", fill="#c4ccd8", font=font)
        for column, (key, side, u, v) in enumerate(test_columns):
            value = pair_rows.get((first, second, key))
            color = COLORS.get(value["boundary_mask"], (238, 238, 238)) if value else (36, 41, 51)
            x = left_w + column * cell_w
            draw.rectangle((x, y, x + cell_w - 2, y + cell_h - 2), fill=color)
            if value and value["boundary_mask"] == 0:
                luma = math.log1p(value["integrated_luminance"])
                bar_width = max(1, int((cell_w - 5) * luma / max_log_luminance))
                draw.rectangle((x + 2, y + 3, x + 2 + bar_width, y + cell_h - 4), fill=(205, 230, 247))
        row_index += 1
    draw.text((left_w, image.height - 14), "Blue: visible interior (bar=log luminance) | Yellow/Red/Purple/Green: near left/top/right/bottom edge", fill="#b7c0cd", font=font)
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.image_output)
    print(args.csv_output)
    print(args.image_output)


if __name__ == "__main__":
    main()
