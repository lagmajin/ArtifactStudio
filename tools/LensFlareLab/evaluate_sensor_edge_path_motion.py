"""Track all ghost paths found near the top/bottom sensor edges as sources move inward."""

import argparse
import csv
import math
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PAIRS = ((6, 5), (11, 10), (13, 7), (13, 12), (10, 8))
FIELDS = {
    "top": ((0.50, 0.01), (0.50, 0.02), (0.50, 0.04), (0.50, 0.06), (0.50, 0.10)),
    "bottom": ((0.50, 0.90), (0.50, 0.94), (0.50, 0.96), (0.50, 0.98), (0.50, 0.99)),
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/edge-path-motion"))
    parser.add_argument("--ray-grid", type=int, default=96)
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/sensor-edge-path-motion.csv"))
    parser.add_argument("--image-output", type=Path, default=Path("tools/LensFlareLab/examples/sensor-edge-path-motion.png"))
    args = parser.parse_args()
    exe = args.executable.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    rows = []
    for side, positions in FIELDS.items():
        for sample_index, (u, v) in enumerate(positions):
            input_path = (args.output_dir / f"{side}-{sample_index}-input.pfm").resolve()
            subprocess.run([sys.executable, str(maker), "--output", str(input_path),
                            "--source-only", "--single-light-position", f"{u:.6f}", f"{v:.6f}",
                            "--single-light-neutral", "--source-radius", "1", "--light-scale", "1024"],
                           check=True, capture_output=True, text=True)
            for first, second in PAIRS:
                output_path = (args.output_dir / f"{side}-{sample_index}-{first}-{second}.pfm").resolve()
                report_path = (args.output_dir / f"{side}-{sample_index}-{first}-{second}.csv").resolve()
                command = [str(exe), str(input_path), str(output_path), "--position", f"{u:.6f}", f"{v:.6f}",
                           "--source-samples", "1", "--ray-grid", str(args.ray_grid), "--gain", "1",
                           "--diffraction", "off", "--ghost-pair", str(first), str(second),
                           "--ghost-report-csv", str(report_path)]
                result = subprocess.run(command, capture_output=True, text=True)
                if result.returncode:
                    raise RuntimeError(f"render failed at {side} ({u:.3f},{v:.3f}) path {first}->{second}:\n{result.stderr}\n{result.stdout}")
                with report_path.open(newline="", encoding="utf-8") as stream:
                    detected = list(csv.DictReader(stream))
                item = {"edge": side, "source_u": u, "source_v": v,
                        "first_surface": first, "second_surface": second,
                        "visible": bool(detected), "boundary_mask": 0,
                        "integrated_luminance": 0.0, "centroid_x": 0.0, "centroid_y": 0.0,
                        "min_x": "", "min_y": "", "max_x": "", "max_y": ""}
                if detected:
                    path = detected[0]
                    bounds = {key: float(path[key]) for key in ("min_x", "min_y", "max_x", "max_y")}
                    item.update(bounds)
                    item["integrated_luminance"] = float(path["integrated_luminance"])
                    item["centroid_x"] = float(path["centroid_x"])
                    item["centroid_y"] = float(path["centroid_y"])
                    item["boundary_mask"] = ((1 if bounds["min_x"] <= 1 else 0) |
                                              (2 if bounds["min_y"] <= 1 else 0) |
                                              (4 if bounds["max_x"] >= 638 else 0) |
                                              (8 if bounds["max_y"] >= 358 else 0))
                rows.append(item)
                print(f"edge={side} sourceV={v:.2f} path={first}->{second} visible={item['visible']} "
                      f"mask={item['boundary_mask']} Y={item['integrated_luminance']:.7g} "
                      f"centroidY={item['centroid_y']:.3f}")

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    cell_w, cell_h, left_w, top_h, panel_gap = 58, 24, 84, 38, 26
    bottom_top = top_h + 5 * cell_h + panel_gap
    image = Image.new("RGB", (left_w + 5 * cell_w + 10, bottom_top + 5 * cell_h + 22), "#11151d")
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    for col, v in enumerate((0.01, 0.02, 0.04, 0.06, 0.10)):
        draw.text((left_w + col * cell_w + 5, 8), f"top {v:.2f}", fill="#cbd3df", font=font)
    for col, v in enumerate((0.90, 0.94, 0.96, 0.98, 0.99)):
        draw.text((left_w + col * cell_w + 5, top_h + 5 * cell_h + 6), f"{v:.2f}", fill="#cbd3df", font=font)
    color_for_mask = {0: (61, 116, 158), 1: (238, 191, 83), 2: (239, 112, 77),
                      4: (190, 123, 234), 8: (76, 193, 148)}
    visible_luma = [math.log1p(row["integrated_luminance"]) for row in rows if row["visible"]]
    max_luma = max(visible_luma, default=1.0)
    for edge_index, side in enumerate(("top", "bottom")):
        base_y = top_h if side == "top" else bottom_top
        for path_index, (first, second) in enumerate(PAIRS):
            label = f"{side} {first:02d}-{second:02d}" if path_index == 0 else f"       {first:02d}-{second:02d}"
            draw.text((3, base_y + path_index * cell_h + 6), label, fill="#cbd3df", font=font)
    for row in rows:
        edge_index = 0 if row["edge"] == "top" else 1
        position_index = next(i for i, pair in enumerate(FIELDS[row["edge"]])
                              if pair == (row["source_u"], row["source_v"]))
        x = left_w + position_index * cell_w
        base_y = top_h if edge_index == 0 else bottom_top
        y = base_y + PAIRS.index((row["first_surface"], row["second_surface"])) * cell_h
        color = color_for_mask.get(row["boundary_mask"], (240, 240, 240)) if row["visible"] else (36, 41, 51)
        draw.rectangle((x, y, x + cell_w - 3, y + cell_h - 3), fill=color)
        if row["visible"] and row["boundary_mask"] == 0:
            bar = max(1, int((cell_w - 7) * math.log1p(row["integrated_luminance"]) / max_luma))
            draw.rectangle((x + 2, y + 5, x + 2 + bar, y + cell_h - 7), fill=(211, 232, 247))
    draw.text((left_w, image.height - 14), "Colored cell: edge proximity mask; blue: visible interior (bar=log Y); dark: no visible flux", fill="#b7c0cd", font=font)
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.image_output)
    print(args.csv_output)
    print(args.image_output)


if __name__ == "__main__":
    main()
