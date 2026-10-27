"""Measure adaptive-refinement threshold sensitivity for one isolated ghost."""

import argparse
import csv
import math
import re
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image, ImageDraw

import evaluate_adaptive_field_convergence as shared


COUNTS = re.compile(r"centerRays=(\d+) edgeRays=(\d+) refinedCells=(\d+).*?screenErrorThreshold=([\d.]+)px, relativeEnergyThreshold=([\d.]+)%")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--field", type=float, nargs=2, metavar=("U", "V"), default=(0.26, 0.26))
    parser.add_argument("--ghost-pair", type=int, nargs=2, default=(6, 5), metavar=("FIRST", "SECOND"))
    parser.add_argument("--ray-grid", type=int, default=64)
    parser.add_argument("--reference-grid", type=int, default=128)
    parser.add_argument("--thresholds", type=float, nargs="+", default=(0.05, 0.375, 0.75, 2.0, 4.0, 8.0))
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/adaptive-threshold-study"))
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-threshold-sensitivity.csv"))
    parser.add_argument("--image-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-threshold-sensitivity.png"))
    args = parser.parse_args()
    if not (8 <= args.ray_grid < args.reference_grid <= 512):
        parser.error("require 8 <= --ray-grid < --reference-grid <= 512")
    if any(not (math.isfinite(t) and 0.05 <= t <= 8) for t in args.thresholds):
        parser.error("all --thresholds values must be finite and in [0.05, 8]")
    u, v = args.field
    first, second = args.ghost_pair
    if not (0 <= u <= 1 and 0 <= v <= 1 and 0 <= second < first < 14):
        parser.error("field coordinates must be in [0,1] and pair must satisfy 0 <= SECOND < FIRST < 14")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    source_path = args.output_dir / "source.pfm"
    subprocess.run([sys.executable, str(maker), "--output", str(source_path), "--source-only",
                    "--single-light-position", str(u), str(v), "--single-light-neutral",
                    "--source-radius", "1", "--light-scale", "1024"],
                   check=True, capture_output=True, text=True)
    width, height, source = shared.read_pfm(source_path)

    cases = [("uniform", args.ray_grid, None), ("reference", args.reference_grid, None)]
    cases.extend((f"adaptive-{threshold:g}px", args.ray_grid, threshold)
                 for threshold in sorted(set(args.thresholds)))
    records, rows = {}, []
    for label, grid, threshold in cases:
        output = args.output_dir / f"{label}.pfm"
        command = [str(args.executable.resolve()), str(source_path.resolve()), str(output.resolve()),
                   "--position", str(u), str(v), "--source-samples", "1", "--ray-grid", str(grid),
                   "--gain", "1", "--diffraction", "off", "--ghost-diffraction", "off",
                   "--ghost-pair", str(first), str(second), "--ghost-report"]
        if threshold is not None:
            command += ["--adaptive-ray-refinement", "on", "--adaptive-ray-refinement-threshold", str(threshold)]
        started = time.perf_counter()
        result = subprocess.run(command, capture_output=True, text=True)
        elapsed = time.perf_counter() - started
        if result.returncode:
            raise RuntimeError(f"render failed for {label}:\n{result.stdout}\n{result.stderr}")
        rw, rh, rendered = shared.read_pfm(output)
        if (rw, rh) != (width, height):
            raise ValueError(f"render dimensions changed for {output}")
        values = shared.flare(source, rendered)
        records[label] = values
        row = {"field_u": u, "field_v": v, "first_surface": first, "second_surface": second,
               "mode": label, "ray_grid": grid, "screen_error_threshold_px": threshold or "",
               "render_seconds": elapsed}
        row.update(shared.metrics(values, values, width, height))
        if threshold is not None:
            match = COUNTS.search(result.stdout)
            if not match:
                raise RuntimeError(f"adaptive diagnostic counts missing for {label}:\n{result.stdout}")
            row.update({"center_rays": int(match.group(1)), "edge_rays": int(match.group(2)),
                        "refined_cells": int(match.group(3)), "reported_screen_threshold_px": float(match.group(4)),
                        "fixed_relative_energy_threshold_percent": float(match.group(5))})
        rows.append(row)
        print(f"mode={label} seconds={elapsed:.3f} Y={row['luma_flux']:.7g} "
              f"refined={row.get('refined_cells', '')}", flush=True)

    reference = records["reference"]
    for row in rows:
        compare = shared.metrics(records[row["mode"]], reference, width, height)
        row.update({f"vs_reference_{key}": compare[key]
                    for key in ("relative_rgb_l1_percent", "normalized_shape_l1_percent", "centroid_error_px")})
    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        fieldnames = list(dict.fromkeys(key for row in rows for key in row))
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    positive = sorted(value for value in reference if value > 0)
    white = positive[min(len(positive) - 1, int(.999 * (len(positive) - 1)))] if positive else 1.0
    labels = [row["mode"] for row in rows]
    sheet = Image.new("RGB", (320 * len(labels), 204), "#12161e")
    draw = ImageDraw.Draw(sheet)
    for i, label in enumerate(labels):
        panel = Image.new("RGB", (320, 180))
        pixels = []
        for y in range(180):
            sy = min(height - 1, int(y * height / 180))
            for x in range(320):
                sx = min(width - 1, int(x * width / 320))
                off = (sy * width + sx) * 3
                rgb = []
                for c in range(3):
                    linear = math.log1p(1000 * max(0.0, records[label][off + c]) / white) / math.log1p(1000)
                    srgb = 12.92 * linear if linear <= .0031308 else 1.055 * linear ** (1 / 2.4) - .055
                    rgb.append(round(min(1.0, max(0.0, srgb)) * 255))
                pixels.append(tuple(rgb))
        panel.putdata(pixels)
        sheet.paste(panel, (i * 320, 24))
        draw.text((i * 320 + 6, 5), label, fill="#e2e7ef")
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.image_output)
    print(args.csv_output)
    print(args.image_output)


if __name__ == "__main__":
    main()
