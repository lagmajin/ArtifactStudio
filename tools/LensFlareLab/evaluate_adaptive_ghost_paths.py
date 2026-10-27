"""Compare uniform and adaptive pupil sampling on isolated ghost paths and fields."""

import argparse
import csv
import math
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image, ImageDraw

import evaluate_adaptive_field_convergence as shared


FIELDS = shared.FIELDS
GHOST_PAIRS = ((13, 9), (6, 5), (5, 0), (12, 9), (10, 7))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/adaptive-paths"))
    parser.add_argument("--ray-grid", type=int, default=64)
    parser.add_argument("--reference-grid", type=int, default=128)
    parser.add_argument("--threshold", type=float, default=0.75)
    parser.add_argument("--fields", nargs="+", choices=tuple(field[0] for field in FIELDS),
                        default=tuple(field[0] for field in FIELDS))
    parser.add_argument("--ghost-pair", nargs=2, type=int, action="append", metavar=("FIRST", "SECOND"),
                        help="repeat to select isolated surface-pair paths")
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-path-convergence.csv"))
    parser.add_argument("--image-dir", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-path-convergence"))
    args = parser.parse_args()
    if not (8 <= args.ray_grid < args.reference_grid <= 512):
        parser.error("require 8 <= --ray-grid < --reference-grid <= 512")
    if not (math.isfinite(args.threshold) and 0.05 <= args.threshold <= 8):
        parser.error("--threshold must be finite and in [0.05, 8]")
    fields = tuple(item for item in FIELDS if item[0] in args.fields)
    pairs = tuple(tuple(pair) for pair in args.ghost_pair) if args.ghost_pair else GHOST_PAIRS
    if any(not (0 <= second < first < 14) for first, second in pairs):
        parser.error("each ghost pair must satisfy 0 <= SECOND < FIRST < 14")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    rows = []
    panels = {pair: [] for pair in pairs}
    for field, u, v in fields:
        source_path = args.output_dir / f"{field}-source.pfm"
        subprocess.run([sys.executable, str(maker), "--output", str(source_path), "--source-only",
                        "--single-light-position", str(u), str(v), "--single-light-neutral",
                        "--source-radius", "1", "--light-scale", "1024"],
                       check=True, capture_output=True, text=True)
        width, height, source_pixels = shared.read_pfm(source_path)
        for first, second in pairs:
            pair_name = f"{first}-{second}"
            case_values = {}
            for mode, grid, adaptive in (("uniform", args.ray_grid, False),
                                         ("adaptive", args.ray_grid, True),
                                         ("reference", args.reference_grid, False)):
                output = args.output_dir / f"{field}-{pair_name}-{mode}.pfm"
                command = [str(args.executable.resolve()), str(source_path.resolve()), str(output.resolve()),
                           "--position", str(u), str(v), "--source-samples", "1", "--ray-grid", str(grid),
                           "--gain", "1", "--diffraction", "off", "--ghost-diffraction", "off",
                           "--ghost-pair", str(first), str(second), "--ghost-report"]
                if adaptive:
                    command += ["--adaptive-ray-refinement", "on", "--adaptive-ray-refinement-threshold",
                                str(args.threshold)]
                started = time.perf_counter()
                result = subprocess.run(command, capture_output=True, text=True)
                elapsed = time.perf_counter() - started
                if result.returncode:
                    raise RuntimeError(f"render failed for {field}/{pair_name}/{mode}:\n{result.stdout}\n{result.stderr}")
                rw, rh, render_pixels = shared.read_pfm(output)
                if (rw, rh) != (width, height):
                    raise ValueError(f"render dimensions changed for {output}")
                case_values[mode] = shared.flare(source_pixels, render_pixels)
                summary = shared.metrics(case_values[mode], case_values[mode], width, height)
                rows.append({"field": field, "source_u": u, "source_v": v,
                             "first_surface": first, "second_surface": second, "mode": mode,
                             "ray_grid": grid, "adaptive": adaptive,
                             "threshold_px": args.threshold if adaptive else "", "render_seconds": elapsed,
                             "rgb_flux": summary["rgb_flux"], "luma_flux": summary["luma_flux"],
                             "peak_luma": summary["peak_luma"]})
                print(f"field={field} path={first}->{second} mode={mode} seconds={elapsed:.3f} "
                      f"Y={summary['luma_flux']:.7g}", flush=True)

            reference = case_values["reference"]
            for row in rows[-3:]:
                comparison = shared.metrics(case_values[row["mode"]], reference, width, height)
                row.update({f"vs_reference_{key}": comparison[key]
                            for key in ("relative_rgb_l1_percent", "normalized_shape_l1_percent",
                                        "centroid_error_px")})
            positive = sorted(value for value in reference if value > 0)
            white = positive[min(len(positive) - 1, int(.999 * (len(positive) - 1)))] if positive else 1.0
            for mode in ("uniform", "adaptive", "reference"):
                values = case_values[mode]
                panel = Image.new("RGB", (320, 180))
                pixels = []
                for y in range(180):
                    sy = min(height - 1, int(y * height / 180))
                    for x in range(320):
                        sx = min(width - 1, int(x * width / 320))
                        off = (sy * width + sx) * 3
                        rgb = []
                        for c in range(3):
                            linear = math.log1p(1000 * max(0.0, values[off + c]) / white) / math.log1p(1000)
                            srgb = 12.92 * linear if linear <= .0031308 else 1.055 * linear ** (1 / 2.4) - .055
                            rgb.append(round(min(1.0, max(0.0, srgb)) * 255))
                        pixels.append(tuple(rgb))
                panel.putdata(pixels)
                panels[(first, second)].append((field, mode, panel))
            differences = {}
            for mode in ("uniform", "adaptive"):
                values = case_values[mode]
                delta = []
                for offset in range(0, len(values), 3):
                    sample_luma = values[offset] * .2126 + values[offset + 1] * .7152 + values[offset + 2] * .0722
                    ref_luma = reference[offset] * .2126 + reference[offset + 1] * .7152 + reference[offset + 2] * .0722
                    delta.append(sample_luma - ref_luma)
                differences[mode] = delta
            diff_white = sorted(abs(value) for values in differences.values() for value in values)
            diff_white = max(diff_white[min(len(diff_white) - 1, int(.999 * (len(diff_white) - 1)))], 1e-30)
            for mode in ("uniform", "adaptive"):
                panel = Image.new("RGB", (320, 180))
                pixels = []
                delta = differences[mode]
                for y in range(180):
                    sy = min(height - 1, int(y * height / 180))
                    for x in range(320):
                        sx = min(width - 1, int(x * width / 320))
                        value = delta[sy * width + sx] / diff_white
                        amount = round(min(1.0, abs(value)) ** .6 * 255)
                        pixels.append((amount, int(amount * .035), int(amount * .01)) if value >= 0
                                      else (int(amount * .01), int(amount * .05), amount))
                panel.putdata(pixels)
                panels[(first, second)].append((field, f"{mode} ΔY", panel))

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    args.image_dir.mkdir(parents=True, exist_ok=True)
    for (first, second), entries in panels.items():
        sheet = Image.new("RGB", (1600, 204 * len(fields)), "#12161e")
        draw = ImageDraw.Draw(sheet)
        for index, (field, mode, panel) in enumerate(entries):
            x, y = (index % 5) * 320, (index // 5) * 204
            sheet.paste(panel, (x, y + 24))
            draw.text((x + 6, y + 5), f"{field} | {mode}", fill="#e2e7ef")
        image_path = args.image_dir / f"ghost-{first}-{second}.png"
        sheet.save(image_path)
        print(image_path)
    print(args.csv_output)


if __name__ == "__main__":
    main()
