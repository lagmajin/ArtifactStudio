"""Measure pixel-phase sensitivity of the direct diffraction PSF."""

import argparse
import csv
import math
import subprocess
import time
from pathlib import Path

from PIL import Image, ImageDraw

from compare_diffraction_resampling import read_pfm


LUMA = (0.2126, 0.7152, 0.0722)


def integrated_gaussian_pixel(x: int, center: float, radius: float) -> float:
    """Average exp(-r²/(0.48 R²)) over one unit-width pixel."""
    sigma_scale = math.sqrt(0.48) * radius
    return (math.sqrt(math.pi) * sigma_scale * 0.5 *
            (math.erf((x + 0.5 - center) / sigma_scale) -
             math.erf((x - 0.5 - center) / sigma_scale)))


def write_source(path: Path, width: int, height: int, center_x: float,
                 center_y: float, radius: float, peak: float) -> float:
    energy = 0.0
    x0, x1 = max(0, math.floor(center_x - 20)), min(width, math.ceil(center_x + 20) + 1)
    y0, y1 = max(0, math.floor(center_y - 20)), min(height, math.ceil(center_y + 20) + 1)
    import struct
    rows = []
    for y in range(height):
        row = []
        wy = integrated_gaussian_pixel(y, center_y, radius) if y0 <= y < y1 else 0.0
        for x in range(width):
            wx = integrated_gaussian_pixel(x, center_x, radius) if x0 <= x < x1 else 0.0
            value = peak * wx * wy
            row.append((value, value, value))
            energy += value
        rows.append(row)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as output:
        output.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        for row in reversed(rows):
            for pixel in row:
                output.write(struct.pack("<3f", *pixel))
    return energy


def measure(image, expected_x: float, expected_y: float):
    width, height, rows = image
    total = cx = cy = 0.0
    radius_values = []
    for y, row in enumerate(rows):
        for x, rgb in enumerate(row):
            value = sum(channel * weight for channel, weight in zip(rgb, LUMA))
            if value <= 0.0:
                continue
            total += value
            cx += x * value
            cy += y * value
            radius_values.append((math.hypot(x - expected_x, y - expected_y), value))
    cx /= max(total, 1e-30)
    cy /= max(total, 1e-30)
    radius_values.sort(key=lambda sample: sample[0])
    def quantile(fraction):
        target, accumulated = total * fraction, 0.0
        for radius, value in radius_values:
            accumulated += value
            if accumulated >= target:
                return radius
        return radius_values[-1][0] if radius_values else 0.0
    return total, cx, cy, quantile(0.5), quantile(0.9), quantile(0.99)


def display(value: float, gain: float):
    mapped = max(0.0, value * gain)
    mapped /= 1.0 + mapped
    srgb = 12.92 * mapped if mapped <= 0.0031308 else 1.055 * mapped ** (1 / 2.4) - 0.055
    return round(min(1.0, max(0.0, srgb)) * 255)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/pixel-phase-study"))
    parser.add_argument("--csv-output", type=Path,
                        default=Path("tools/LensFlareLab/examples/diffraction-pixel-phase.csv"))
    parser.add_argument("--image-output", type=Path,
                        default=Path("tools/LensFlareLab/examples/diffraction-pixel-phase.png"))
    parser.add_argument("--phases", type=float, nargs="+",
                        default=tuple(index / 8 for index in range(8)))
    parser.add_argument("--source-radius", type=float, default=1.0)
    parser.add_argument("--light-peak", type=float, default=18432.0)
    parser.add_argument("--diffraction-resolution", type=int, default=1024)
    parser.add_argument("--resampling-modes", choices=("tent", "pixel-area"), nargs="+",
                        default=("tent", "pixel-area"))
    args = parser.parse_args()
    if any(not math.isfinite(phase) or not 0.0 <= phase < 1.0 for phase in args.phases):
        parser.error("all --phases values must lie in [0, 1)")
    if not math.isfinite(args.source_radius) or not 0.25 <= args.source_radius <= 8.0:
        parser.error("--source-radius must lie in [0.25, 8]")
    if not math.isfinite(args.light_peak) or args.light_peak <= 0.0:
        parser.error("--light-peak must be finite and positive")
    if args.diffraction_resolution not in (128, 256, 512, 1024, 2048, 4096):
        parser.error("unsupported diffraction resolution")

    width, height = 640, 360
    nominal_x, nominal_y = 319.0, 179.0
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records = []
    renders = {mode: [] for mode in args.resampling_modes}
    executable = args.executable.resolve()
    for phase in sorted(set(args.phases)):
        center_x, center_y = nominal_x + phase, nominal_y + 0.375
        tag = f"phase-{phase:.3f}".replace(".", "p")
        source_path = args.output_dir / f"{tag}-source.pfm"
        source_energy = write_source(source_path, width, height, center_x, center_y,
                                     args.source_radius, args.light_peak)
        for mode in args.resampling_modes:
            render_path = args.output_dir / f"{tag}-{mode}-render.pfm"
            command = [str(executable), str(source_path.resolve()), str(render_path.resolve()),
                       "--position", f"{center_x / (width - 1):.9g}",
                       f"{center_y / (height - 1):.9g}", "--threshold", "0", "--gain", "1",
                       "--source-samples", "1", "--ghosts", "off", "--diffraction", "on",
                       "--diffraction-resolution", str(args.diffraction_resolution),
                       "--diffraction-resampling", mode, "--aperture-blades", "6",
                       "--aperture-radius", "4.545"]
            started = time.perf_counter()
            result = subprocess.run(command, capture_output=True, text=True)
            elapsed = time.perf_counter() - started
            if result.returncode:
                raise RuntimeError(f"render failed for phase {phase:g}/{mode}:\n{result.stdout}{result.stderr}")
            rendered = read_pfm(render_path)
            total, cx, cy, r50, r90, r99 = measure(rendered, center_x, center_y)
            records.append({"resampling_mode": mode, "phase_x_px": phase,
                            "center_x_px": center_x, "center_y_px": center_y,
                            "source_radius_px": args.source_radius,
                            "source_input_luminance": source_energy,
                            "rendered_luminance": total, "throughput_ratio": total / source_energy,
                            "centroid_x_px": cx, "centroid_y_px": cy,
                            "centroid_error_x_px": cx - center_x,
                            "centroid_error_y_px": cy - center_y,
                            "r50_px": r50, "r90_px": r90, "r99_px": r99,
                            "render_seconds": elapsed,
                            "diffraction_resolution": args.diffraction_resolution})
            renders[mode].append((phase, rendered))
            print(f"mode={mode} phase={phase:g} inputY={source_energy:.8g} outputY={total:.8g} "
                  f"centroidError=({cx-center_x:+.4f},{cy-center_y:+.4f})px "
                  f"r50={r50:.3f}px r99={r99:.3f}px time={elapsed:.3f}s", flush=True)

    args.csv_output.parent.mkdir(parents=True, exist_ok=True)
    with args.csv_output.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    crop_radius = 44
    source_panel_size, scale, label_height, columns = crop_radius * 2, 2, 22, 4
    panel_size = source_panel_size * scale
    mode_rows = (len(next(iter(renders.values()))) + columns - 1) // columns
    mode_header = 24
    plate = Image.new("RGB", (columns * panel_size,
                              len(renders) * (mode_header + mode_rows * (panel_size + label_height))),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for mode_index, (mode, cases) in enumerate(renders.items()):
        mode_y = mode_index * (mode_header + mode_rows * (panel_size + label_height))
        draw.text((4, mode_y + 4), f"{mode} resampling", fill=(235, 238, 244))
        for index, (phase, image) in enumerate(cases):
            x0 = (index % columns) * panel_size
            y0 = mode_y + mode_header + (index // columns) * (panel_size + label_height)
            draw.text((x0 + 4, y0 + 3), f"x phase {phase:.3f}", fill=(225, 230, 238))
            _, _, pixels = image
            panel = Image.new("RGB", (source_panel_size, source_panel_size))
            crop_x, crop_y = int(nominal_x), int(nominal_y)
            for py in range(source_panel_size):
                for px in range(source_panel_size):
                    rgb = pixels[crop_y + py - crop_radius][crop_x + px - crop_radius]
                    panel.putpixel((px, py), tuple(display(value, 40.0) for value in rgb))
            plate.paste(panel.resize((panel_size, panel_size), Image.Resampling.NEAREST),
                        (x0, y0 + label_height))
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image_output)
    print(f"saved {args.csv_output} and {args.image_output}")


if __name__ == "__main__":
    main()
