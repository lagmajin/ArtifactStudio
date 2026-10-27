"""Evaluate adaptive pupil refinement across representative source-field positions."""

import argparse
import array
import csv
import math
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image, ImageDraw


FIELDS = (("center", 0.50, 0.50), ("upper-left", 0.26, 0.26),
          ("top-edge", 0.50, 0.02), ("lower-right", 0.74, 0.74))


def read_pfm(path: Path):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        pixels = array.array("f")
        pixels.frombytes(stream.read())
    if len(pixels) != width * height * 3:
        raise ValueError(f"invalid PFM payload: {path}")
    if (scale < 0) != (sys.byteorder == "little"):
        pixels.byteswap()
    magnitude = abs(scale)
    if magnitude != 1.0:
        pixels = array.array("f", (value * magnitude for value in pixels))
    row = width * 3
    for y in range(height // 2):
        a, b = y * row, (height - 1 - y) * row
        first, second = pixels[a:a + row], pixels[b:b + row]
        pixels[a:a + row], pixels[b:b + row] = second, first
    return width, height, pixels


def flare(source, render):
    return array.array("f", (max(0.0, b - a) for a, b in zip(source, render)))


def metrics(values, reference, width, height):
    rgb_flux = sum(values)
    reference_flux = sum(reference)
    luma_flux = peak = xsum = ysum = 0.0
    ref_luma = ref_x = ref_y = 0.0
    for i in range(0, len(values), 3):
        x = (i // 3) % width
        y = (i // 3) // width
        lum = values[i] * .2126 + values[i + 1] * .7152 + values[i + 2] * .0722
        ref = reference[i] * .2126 + reference[i + 1] * .7152 + reference[i + 2] * .0722
        luma_flux += lum
        ref_luma += ref
        xsum += x * lum
        ysum += y * lum
        ref_x += x * ref
        ref_y += y * ref
        peak = max(peak, lum)
    rel_l1 = 100 * sum(abs(a - b) for a, b in zip(values, reference)) / max(reference_flux, 1e-30)
    shape_l1 = 100 * sum(abs(a * reference_flux / max(rgb_flux, 1e-30) - b)
                         for a, b in zip(values, reference)) / max(reference_flux, 1e-30)
    centroid_error = math.hypot(xsum / max(luma_flux, 1e-30) - ref_x / max(ref_luma, 1e-30),
                                ysum / max(luma_flux, 1e-30) - ref_y / max(ref_luma, 1e-30))
    return {"rgb_flux": rgb_flux, "luma_flux": luma_flux, "peak_luma": peak,
            "relative_rgb_l1_percent": rel_l1, "normalized_shape_l1_percent": shape_l1,
            "centroid_error_px": centroid_error}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("build/tools/LensFlareLab/adaptive-field-convergence"))
    parser.add_argument("--ray-grid", type=int, default=64, help="base grid for uniform and adaptive cases")
    parser.add_argument("--reference-grid", type=int, default=128)
    parser.add_argument("--threshold", type=float, default=0.75)
    parser.add_argument("--csv-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-field-convergence.csv"))
    parser.add_argument("--image-output", type=Path, default=Path("tools/LensFlareLab/examples/adaptive-field-convergence.png"))
    args = parser.parse_args()
    if not (8 <= args.ray_grid < args.reference_grid <= 512):
        parser.error("require 8 <= --ray-grid < --reference-grid <= 512")
    if not (math.isfinite(args.threshold) and 0.05 <= args.threshold <= 8):
        parser.error("--threshold must be finite and in [0.05, 8]")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    maker = Path(__file__).with_name("make_evaluation_chart.py")
    rows, panels = [], []
    for label, u, v in FIELDS:
        source_path = args.output_dir / f"{label}-source.pfm"
        subprocess.run([sys.executable, str(maker), "--output", str(source_path), "--source-only",
                        "--single-light-position", str(u), str(v), "--single-light-neutral",
                        "--source-radius", "1", "--light-scale", "1024"],
                       check=True, capture_output=True, text=True)
        cases = (("uniform", args.ray_grid, False), ("adaptive", args.ray_grid, True),
                 ("reference", args.reference_grid, False))
        rendered = {}
        for mode, grid, adaptive in cases:
            output = args.output_dir / f"{label}-{mode}.pfm"
            command = [str(args.executable.resolve()), str(source_path.resolve()), str(output.resolve()),
                       "--position", str(u), str(v), "--source-samples", "1", "--ray-grid", str(grid),
                       "--gain", "1", "--diffraction", "off", "--ghost-diffraction", "off",
                       "--ghost-report"]
            if adaptive:
                command += ["--adaptive-ray-refinement", "on", "--adaptive-ray-refinement-threshold",
                            str(args.threshold)]
            started = time.perf_counter()
            result = subprocess.run(command, capture_output=True, text=True)
            elapsed = time.perf_counter() - started
            if result.returncode:
                raise RuntimeError(f"render failed for {label}/{mode}:\n{result.stdout}\n{result.stderr}")
            width, height, source = read_pfm(source_path)
            out_width, out_height, image = read_pfm(output)
            if (width, height) != (out_width, out_height):
                raise ValueError(f"render dimensions changed for {output}")
            rendered[mode] = flare(source, image)
            measured = metrics(rendered[mode], rendered[mode], width, height)
            rows.append({"field": label, "source_u": u, "source_v": v, "mode": mode,
                         "ray_grid": grid, "adaptive": adaptive, "threshold_px": args.threshold if adaptive else "",
                         "render_seconds": elapsed, "rgb_flux": measured["rgb_flux"],
                         "luma_flux": measured["luma_flux"], "peak_luma": measured["peak_luma"]})
            print(f"field={label} mode={mode} grid={grid} seconds={elapsed:.3f} "
                  f"Y={rows[-1]['luma_flux']:.7g}", flush=True)
        ref = rendered["reference"]
        for row in rows[-3:]:
            comparison = metrics(rendered[row["mode"]], ref, width, height)
            row.update({f"vs_reference_{key}": value for key, value in comparison.items()
                        if key in ("relative_rgb_l1_percent", "normalized_shape_l1_percent", "centroid_error_px")})
        white = max(sorted(value for value in ref if value > 0)[int(.999 * max(0, sum(value > 0 for value in ref) - 1))], 1e-30)
        for mode in ("uniform", "adaptive", "reference"):
            reduced = Image.new("RGB", (320, 180))
            pix = []
            values = rendered[mode]
            for y in range(180):
                sy = min(height - 1, int(y * height / 180))
                for x in range(320):
                    sx = min(width - 1, int(x * width / 320))
                    off = (sy * width + sx) * 3
                    rgb = []
                    for c in range(3):
                        value = max(0.0, values[off + c] / white)
                        linear = math.log1p(1000 * value) / math.log1p(1000)
                        srgb = 12.92 * linear if linear <= .0031308 else 1.055 * linear ** (1 / 2.4) - .055
                        rgb.append(round(min(1, max(0, srgb)) * 255))
                    pix.append(tuple(rgb))
            reduced.putdata(pix)
            panels.append((label, mode, reduced))

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    sheet = Image.new("RGB", (320 * 3, 204 * len(FIELDS)), "#12161e")
    draw = ImageDraw.Draw(sheet)
    for index, (field, mode, panel) in enumerate(panels):
        x = (index % 3) * 320
        y = (index // 3) * 204
        sheet.paste(panel, (x, y + 24))
        draw.text((x + 6, y + 5), f"{field} | {mode}", fill="#e2e7ef")
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.image_output)
    print(args.csv_output)
    print(args.image_output)


if __name__ == "__main__":
    main()
