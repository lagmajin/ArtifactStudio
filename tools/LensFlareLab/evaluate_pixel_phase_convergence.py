"""Measure source pixel-phase sensitivity across aperture FFT resolutions."""

import argparse
import csv
import math
import subprocess
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_diffraction_pixel_phase import display, measure, read_pfm, write_source


WIDTH, HEIGHT = 640, 360
NOMINAL_X, NOMINAL_Y = 319.0, 179.0
PHASES = tuple(index / 8.0 for index in range(8))
RESOLUTIONS = (128, 256, 512, 1024, 4096)


def crop_rgb(image, center_x: float, center_y: float, radius: int = 44) -> np.ndarray:
    width, height, rows = image
    x0, y0 = int(center_x) - radius, int(center_y) - radius
    return np.asarray(rows[y0:y0 + 2 * radius], dtype=np.float64)[:, x0:x0 + 2 * radius, :]


def normalized_shape_error(candidate: np.ndarray, reference: np.ndarray) -> float:
    candidate = np.maximum(candidate, 0.0)
    reference = np.maximum(reference, 0.0)
    candidate /= max(float(candidate.sum()), 1e-30)
    reference /= max(float(reference.sum()), 1e-30)
    return 100.0 * float(np.abs(candidate - reference).sum())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/pixel-phase-convergence"))
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/diffraction-pixel-phase-convergence.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/diffraction-pixel-phase-convergence.png"))
    parser.add_argument("--resolutions", type=int, nargs="+", default=RESOLUTIONS,
                        choices=(128, 256, 512, 1024, 2048, 4096))
    parser.add_argument("--phases", type=float, nargs="+", default=PHASES)
    parser.add_argument("--source-radius", type=float, default=1.0)
    parser.add_argument("--light-peak", type=float, default=18432.0)
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    if any(not math.isfinite(phase) or not 0.0 <= phase < 1.0 for phase in args.phases):
        parser.error("all --phases values must lie in [0,1)")
    if len(set(args.resolutions)) != len(args.resolutions):
        parser.error("--resolutions values must be unique")
    executable = args.renderer.resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    phases = tuple(sorted(set(args.phases)))
    resolutions = tuple(args.resolutions)
    inputs, source_energies = {}, {}
    rendered_images, rendered_crops = {}, {}
    for phase in phases:
        center_x, center_y = NOMINAL_X + phase, NOMINAL_Y + 0.375
        tag = f"phase-{phase:.3f}".replace(".", "p")
        source_path = args.output_dir / f"{tag}-source.pfm"
        source_energies[phase] = write_source(source_path, WIDTH, HEIGHT,
                                               center_x, center_y,
                                               args.source_radius, args.light_peak)
        inputs[phase] = source_path
        for resolution in resolutions:
            render_path = args.output_dir / f"{tag}-fft-{resolution}.pfm"
            command = [str(executable), str(source_path.resolve()), str(render_path.resolve()),
                       "--position", f"{center_x / (WIDTH - 1):.9g}",
                       f"{center_y / (HEIGHT - 1):.9g}", "--threshold", "0", "--gain", "1",
                       "--source-samples", "1", "--ghosts", "off", "--diffraction", "on",
                       "--diffraction-resolution", str(resolution),
                       "--diffraction-resampling", "pixel-area", "--aperture-blades", "6",
                       "--aperture-radius", "4.545", "--spectral-samples", "3"]
            started = time.perf_counter()
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
            elapsed = time.perf_counter() - started
            image = read_pfm(render_path)
            total, cx, cy, r50, r90, r99 = measure(image, center_x, center_y)
            rendered_images[phase, resolution] = image
            rendered_crops[phase, resolution] = crop_rgb(image, center_x, center_y)
            rendered_images[phase, resolution, "metrics"] = {
                "source_energy": source_energies[phase],
                "rendered_luminance": total,
                "throughput_ratio": total / max(source_energies[phase], 1e-30),
                "centroid_error_x_px": cx - center_x,
                "centroid_error_y_px": cy - center_y,
                "centroid_error_px": math.hypot(cx - center_x, cy - center_y),
                "r50_px": r50, "r90_px": r90, "r99_px": r99,
                "render_seconds": elapsed,
            }
            print(f"FFT={resolution:4d} phase={phase:.3f} "
                  f"flux={total / max(source_energies[phase],1e-30):.7f} "
                  f"centroid={math.hypot(cx-center_x,cy-center_y):.4f}px "
                  f"r99={r99:.3f}px time={elapsed:.3f}s", flush=True)

    records = []
    reference_resolution = 4096 if 4096 in resolutions else (
        1024 if 1024 in resolutions else max(resolutions))
    for resolution in resolutions:
        for phase in phases:
            metrics = dict(rendered_images[phase, resolution, "metrics"])
            metrics.update({"phase_x_px": phase, "center_x_px": NOMINAL_X + phase,
                            "center_y_px": NOMINAL_Y + 0.375,
                            "source_radius_px": args.source_radius,
                            "diffraction_resolution": resolution,
                            "shape_l1_vs_same_phase_reference_percent": normalized_shape_error(
                                rendered_crops[phase, resolution],
                                rendered_crops[phase, reference_resolution])})
            records.append(metrics)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    panel, label_height, gap, row_header = 128, 18, 4, 20
    plate = Image.new("RGB", (len(phases) * (panel + gap) - gap,
                              len(resolutions) * (panel + row_header + gap) - gap),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for row, resolution in enumerate(resolutions):
        top = row * (panel + row_header + gap)
        draw.text((3, top + 2), f"pixel-area · aperture FFT {resolution}²",
                  fill=(235, 238, 244))
        for column, phase in enumerate(phases):
            x = column * (panel + gap)
            y = top + row_header
            draw.text((x + 3, y + 2), f"x phase {phase:.3f}", fill=(225, 230, 238))
            crop = rendered_crops[phase, resolution]
            rgb = np.empty((crop.shape[0], crop.shape[1], 3), dtype=np.uint8)
            for channel in range(3):
                rgb[:, :, channel] = np.asarray(
                    [[display(value[channel], 40.0) for value in line]
                     for line in crop], dtype=np.uint8)
            panel_image = Image.fromarray(rgb).resize(
                (panel, panel), Image.Resampling.NEAREST)
            plate.paste(panel_image, (x, y + label_height))
    args.image.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image)

    for resolution in resolutions:
        rows = [row for row in records if row["diffraction_resolution"] == resolution]
        throughput = [row["throughput_ratio"] for row in rows]
        r99 = [row["r99_px"] for row in rows]
        centroid = [row["centroid_error_px"] for row in rows]
        shape = [row["shape_l1_vs_same_phase_reference_percent"] for row in rows]
        print(f"summary FFT={resolution:4d}: throughput span="
              f"{max(throughput)-min(throughput):.7f}, r99 span={max(r99)-min(r99):.4f}px, "
              f"max centroid={max(centroid):.4f}px, max shape L1={max(shape):.4f}%",
              flush=True)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
