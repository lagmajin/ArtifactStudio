"""Measure selected ghost-path centroid motion around the two Fig. 10 fields."""

import argparse
import csv
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


CASES = (
    ("A-X bright", 0.246, 0.356, 13, 9, "x", "fig10"),
    ("B-X bright", 0.390, 0.693, 11, 9, "x", "fig10"),
    ("A-Y weak", 0.246, 0.356, 5, 0, "y", "fig10"),
    ("C-Y edge", 0.060, 0.060, 6, 5, "y", "corner"),
)
OFFSETS = (-1.0, -0.5, 0.0, 0.5, 1.0)
COLORS = ((84, 190, 255), (74, 222, 170), (255, 255, 255), (255, 176, 84), (255, 103, 131))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/field-continuity"))
    parser.add_argument("--ray-grid", type=int, default=96)
    parser.add_argument("--offset-pixels", type=float, default=6.0)
    parser.add_argument("--edge-offset-pixels", type=float, default=18.0)
    parser.add_argument("--csv-output", type=Path)
    parser.add_argument("--image-output", type=Path)
    args = parser.parse_args()
    if args.offset_pixels <= 0 or args.edge_offset_pixels <= 0:
        parser.error("offset pixel ranges must be positive")
    executable, source = args.executable.resolve(), args.input.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    rows = []
    corner_source = args.output_dir / "corner-source.pfm"
    subprocess.run([sys.executable, str(Path(__file__).with_name("make_evaluation_chart.py")),
                    "--output", str(corner_source), "--source-only", "--single-light-position",
                    "0.06", "0.06", "--single-light-neutral", "--source-radius", "1",
                    "--light-scale", "1024"], check=True, capture_output=True, text=True)
    for label, center_u, center_v, first, second, axis, fixture in CASES:
        case_source = source if fixture == "fig10" else corner_source
        offset_range = args.edge_offset_pixels if fixture == "corner" else args.offset_pixels
        for index, multiplier in enumerate(OFFSETS):
            delta = multiplier * offset_range
            u = center_u + delta / 639.0 if axis == "x" else center_u
            v = center_v + delta / 359.0 if axis == "y" else center_v
            csv_path = args.output_dir / f"{label}-{index}.csv"
            render_path = args.output_dir / f"{label}-{index}.pfm"
            command = [str(executable), str(case_source), str(render_path),
                       "--position", f"{u:.8f}", f"{v:.8f}",
                       "--source-samples", "1", "--ray-grid", str(args.ray_grid),
                       "--gain", "1", "--diffraction", "off", "--ghost-pair",
                       str(first), str(second), "--ghost-report-csv", str(csv_path)]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(f"render failed for {label} delta={delta:g}px:\n{result.stderr}\n{result.stdout}")
            with csv_path.open(newline="", encoding="utf-8") as stream:
                path_rows = list(csv.DictReader(stream))
            item = {"field": label, "first_surface": first, "second_surface": second,
                    "source_axis": axis, "source_delta_px": delta, "source_u": u, "source_v": v,
                    "visible": bool(path_rows)}
            if path_rows:
                path = path_rows[0]
                item.update({key: float(path[key]) for key in
                             ("integrated_luminance", "centroid_x", "centroid_y", "min_x", "min_y", "max_x", "max_y")})
            else:
                item.update({key: 0.0 for key in
                             ("integrated_luminance", "centroid_x", "centroid_y", "min_x", "min_y", "max_x", "max_y")})
            rows.append(item)
            print(f"field={label} delta={delta:+.1f}px visible={item['visible']} "
                  f"Y={item['integrated_luminance']:.7g} centroid=({item['centroid_x']:.3f},{item['centroid_y']:.3f})")

    csv_output = args.csv_output or (args.output_dir / "centroid-continuity.csv")
    csv_output.parent.mkdir(parents=True, exist_ok=True)
    columns = list(rows[0])
    with csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)

    panel_w, panel_h, margin, header = 640, 360, 28, 50
    sheet = Image.new("RGB", (len(CASES) * panel_w + (len(CASES) + 1) * margin,
                               panel_h + header + margin), "#11151d")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for field_index, (label, center_u, center_v, first, second, axis, fixture) in enumerate(CASES):
        x0, y0 = margin + field_index * (panel_w + margin), header
        draw.text((x0, 14), f"{label}: ghost {first}→{second} | centroid {axis.upper()} vs source {axis.upper()} shift", fill="#eef2f8", font=font)
        left, top, right, bottom = x0 + 42, y0 + 12, x0 + panel_w - 14, y0 + panel_h - 28
        draw.rectangle((left, top, right, bottom), outline="#657184")
        subset = [row for row in rows if row["field"] == label and row["visible"]]
        if not subset:
            continue
        offset_range = max(abs(row["source_delta_px"]) for row in subset)
        centroid_key = f"centroid_{axis}"
        values = [row[centroid_key] for row in subset]
        low, high = min(values), max(values)
        padding = max(1.0, (high - low) * 0.15)
        low, high = low - padding, high + padding
        draw.text((x0, top), f"{high:.1f}", fill="#aeb8c7", font=font)
        draw.text((x0, bottom - 8), f"{low:.1f}", fill="#aeb8c7", font=font)
        draw.text((left, bottom + 8), f"-{offset_range:g}px", fill="#aeb8c7", font=font)
        draw.text((right - 22, bottom + 8), f"+{offset_range:g}px", fill="#aeb8c7", font=font)
        points = []
        for row in subset:
            tx = (row["source_delta_px"] + offset_range) / (2 * offset_range)
            ty = (row[centroid_key] - low) / (high - low)
            points.append((left + tx * (right - left), bottom - ty * (bottom - top)))
        if len(points) > 1:
            draw.line(points, fill="#667080", width=2)
        for point, row, color in zip(points, subset, COLORS):
            px, py = point
            draw.ellipse((px - 4, py - 4, px + 4, py + 4), fill=color, outline="#0b0d12")
            draw.text((px - 7, py - 16), f"{row['source_delta_px']:+.0f}", fill=color, font=font)
    output = args.image_output or (args.output_dir / "centroid-continuity.png")
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    print(csv_output)
    print(output)


if __name__ == "__main__":
    main()
