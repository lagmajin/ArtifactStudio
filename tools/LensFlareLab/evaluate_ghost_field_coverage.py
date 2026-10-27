"""Map visibility of all eligible ghost paths over a two-dimensional source-field grid."""

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
DEFAULT_POSITIONS = (0.02, 0.26, 0.50, 0.74, 0.98)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/field-coverage"))
    parser.add_argument("--ray-grid", type=int, default=96)
    parser.add_argument("--positions", type=float, nargs="+", default=DEFAULT_POSITIONS)
    parser.add_argument("--source-radius", type=float, default=1.0)
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/ghost-field-coverage.csv"))
    parser.add_argument("--image-output", type=Path, default=Path("tools/LensFlareLab/examples/ghost-field-coverage.png"))
    args = parser.parse_args()
    positions = tuple(sorted(set(args.positions)))
    if len(positions) < 2 or any(value < 0 or value > 1 for value in positions):
        parser.error("--positions requires at least two unique normalized coordinates in [0, 1]")
    exe = args.executable.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    visible_paths = {}
    fields = []
    for row_index, v in enumerate(positions):
        for column_index, u in enumerate(positions):
            key = f"v{row_index}-u{column_index}"
            fields.append((key, u, v))
            input_path = (args.output_dir / f"{key}-input.pfm").resolve()
            render_path = (args.output_dir / f"{key}-render.pfm").resolve()
            report_path = (args.output_dir / f"{key}-paths.csv").resolve()
            subprocess.run([sys.executable, str(maker), "--output", str(input_path),
                            "--source-only", "--single-light-position", f"{u:.6f}", f"{v:.6f}",
                            "--single-light-neutral", "--source-radius", str(args.source_radius),
                            "--light-scale", "1024"], check=True, capture_output=True, text=True)
            command = [str(exe), str(input_path), str(render_path), "--position", f"{u:.6f}", f"{v:.6f}",
                       "--source-samples", "1", "--ray-grid", str(args.ray_grid), "--gain", "1",
                       "--diffraction", "off", "--ghost-report-csv", str(report_path)]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"render failed at source field ({u:.3f},{v:.3f}):\n{result.stderr}\n{result.stdout}")
            with report_path.open(newline="", encoding="utf-8") as stream:
                report = list(csv.DictReader(stream))
            for item in report:
                first, second = int(item["first_surface"]), int(item["second_surface"])
                bounds = {name: float(item[name]) for name in ("min_x", "min_y", "max_x", "max_y")}
                mask = ((1 if bounds["min_x"] <= 1 else 0) | (2 if bounds["min_y"] <= 1 else 0) |
                        (4 if bounds["max_x"] >= 638 else 0) | (8 if bounds["max_y"] >= 358 else 0))
                visible_paths[(first, second, key)] = {
                    "boundary_mask": mask, "integrated_luminance": float(item["integrated_luminance"]),
                    "centroid_x": float(item["centroid_x"]), "centroid_y": float(item["centroid_y"]), **bounds}
            print(f"field=({u:.2f},{v:.2f}) visible={len(report)}/42 "
                  f"near_edge={sum(1 for item in report if min(float(item['min_x']), float(item['min_y'])) <= 1 or float(item['max_x']) >= 638 or float(item['max_y']) >= 358)}")

    output_rows = []
    for first, second in GHOST_PAIRS:
        count = sum((first, second, key) in visible_paths for key, _, _ in fields)
        fraction = count / len(fields)
        for key, u, v in fields:
            data = visible_paths.get((first, second, key), {})
            output_rows.append({"first_surface": first, "second_surface": second,
                                "visible_fields": count, "visibility_fraction": fraction,
                                "source_u": u, "source_v": v,
                                "visible": bool(data), "boundary_mask": data.get("boundary_mask", 0),
                                "integrated_luminance": data.get("integrated_luminance", 0.0),
                                "centroid_x": data.get("centroid_x", 0.0),
                                "centroid_y": data.get("centroid_y", 0.0),
                                "min_x": data.get("min_x", ""), "min_y": data.get("min_y", ""),
                                "max_x": data.get("max_x", ""), "max_y": data.get("max_y", "")})
        print(f"path={first}->{second} visible_fields={count}/{len(fields)}")

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(output_rows[0]))
        writer.writeheader()
        writer.writerows(output_rows)

    cell_w, cell_h, left_w, header_h, top_h = 30, 13, 60, 34, 25
    image = Image.new("RGB", (left_w + len(fields) * cell_w + 12,
                               header_h + top_h + len(GHOST_PAIRS) * cell_h + 22), "#11151d")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    for col, (key, u, v) in enumerate(fields):
        x = left_w + col * cell_w
        if col == 0 or fields[col - 1][2] != v:
            draw.text((x + 2, 5), f"v={v:.2f}", fill="#eef2f8", font=font)
        draw.text((x + 2, header_h), f"{u:.2f}", fill="#aeb8c7", font=font)
    max_log = max((math.log1p(value["integrated_luminance"]) for value in visible_paths.values()), default=1.0)
    for row_index, (first, second) in enumerate(GHOST_PAIRS):
        y = header_h + top_h + row_index * cell_h
        count = sum((first, second, key) in visible_paths for key, _, _ in fields)
        draw.text((2, y + 2), f"{first:02d}-{second:02d} {count:02d}", fill="#c4ccd8", font=font)
        for column, (key, u, v) in enumerate(fields):
            value = visible_paths.get((first, second, key))
            x = left_w + column * cell_w
            if not value:
                color = (36, 41, 51)
            else:
                color = {0: (57, 120, 174), 1: (242, 190, 75), 2: (242, 116, 80),
                         4: (193, 122, 240), 8: (78, 194, 151)}.get(value["boundary_mask"], (240, 240, 240))
            draw.rectangle((x, y, x + cell_w - 2, y + cell_h - 2), fill=color)
            if value and value["boundary_mask"] == 0:
                bar = max(1, int((cell_w - 5) * math.log1p(value["integrated_luminance"]) / max_log))
                draw.rectangle((x + 2, y + 3, x + 2 + bar, y + cell_h - 4), fill=(205, 230, 247))
    draw.text((left_w, image.height - 14), "Dark: no in-frame flux | Blue: visible interior (bar=log Y) | other colors: within 2px of edge", fill="#b7c0cd", font=font)
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.image_output)
    print(args.csv_output)
    print(args.image_output)


if __name__ == "__main__":
    main()
