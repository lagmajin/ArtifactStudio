"""Measure RGB-basis diffraction color and clipping across source fields."""

import argparse
import csv
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_pupil_psf import read_pfm, to_display, write_pfm


WIDTH, HEIGHT = 640, 360
LUMA = np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float64)
COLORS = {"red": (1.0, 0.0, 0.0), "green": (0.0, 1.0, 0.0),
          "blue": (0.0, 0.0, 1.0)}
SAMPLES = (3, 15, 61)


def source_fixture(center_x: float, center_y: float, sigma: float,
                   color: tuple[float, float, float]) -> np.ndarray:
    y, x = np.mgrid[:HEIGHT, :WIDTH]
    profile = 4096.0 * np.exp(-((x - center_x) ** 2 + (y - center_y) ** 2) /
                              (2.0 * sigma * sigma))
    return profile[:, :, None] * np.asarray(color, dtype=np.float64)[None, None, :]


def full_discrete_source_energy(sigma: float) -> float:
    extent = int(math.ceil(8.0 * sigma))
    offsets = np.arange(-extent, extent + 1, dtype=np.float64)
    one_dimensional = np.exp(-(offsets * offsets) / (2.0 * sigma * sigma)).sum()
    return 4096.0 * one_dimensional * one_dimensional


def render(executable: Path, source: np.ndarray, output: Path,
           position: tuple[float, float], samples: int, resolution: int) -> np.ndarray:
    input_path = output.with_name(output.stem + "-input.pfm")
    write_pfm(input_path, source)
    subprocess.run([str(executable), str(input_path.resolve()), str(output.resolve()),
                    "--position", f"{position[0]:.9g}", f"{position[1]:.9g}",
                    "--threshold", "0", "--gain", "1", "--source-samples", "1",
                    "--ghosts", "off", "--diffraction", "on",
                    "--diffraction-resolution", str(resolution),
                    "--diffraction-resampling", "pixel-area",
                    "--spectral-samples", str(samples), "--aperture-blades", "6"],
                   check=True, stdout=subprocess.DEVNULL)
    return read_pfm(output)


def image_metrics(image: np.ndarray, source: np.ndarray,
                  center: tuple[float, float], reference: np.ndarray | None) -> dict:
    positive = np.maximum(image, 0.0)
    total_input_energy = float(np.maximum(source, 0.0).sum())
    total_output_energy = float(positive.sum())
    height, width, _ = image.shape
    yy, xx = np.mgrid[:height, :width]
    cx, cy = center
    radius = np.hypot(xx - cx, yy - cy)
    result = {}
    active = []
    for channel, name in enumerate(("r", "g", "b")):
        input_energy = float(source[:, :, channel].sum())
        values = positive[:, :, channel]
        energy = float(values.sum())
        signed_energy = float(image[:, :, channel].sum())
        negative_energy = float(np.maximum(-image[:, :, channel], 0.0).sum())
        result[f"{name}_visible_input_energy"] = input_energy
        result[f"{name}_output_energy"] = energy
        result[f"{name}_signed_output_fraction_of_total_input"] = (
            signed_energy / max(total_input_energy, 1e-30))
        result[f"{name}_negative_mass_fraction_of_total_input"] = (
            negative_energy / max(total_input_energy, 1e-30))
        result[f"{name}_output_fraction_of_total_input"] = energy / max(total_input_energy, 1e-30)
        result[f"{name}_output_fraction_of_total_output"] = energy / max(total_output_energy, 1e-30)
        result[f"{name}_throughput_to_visible_input"] = (
            energy / input_energy if input_energy > total_input_energy * 1e-8 else math.nan)
        if energy <= max(total_output_energy * 1e-5, 1e-30):
            result[f"{name}_centroid_x_px"] = math.nan
            result[f"{name}_centroid_y_px"] = math.nan
            result[f"{name}_r50_px"] = math.nan
            result[f"{name}_r90_px"] = math.nan
            result[f"{name}_r99_px"] = math.nan
            if reference is not None:
                result[f"{name}_shape_l1_vs_61_percent"] = math.nan
            continue
        active.append((name, float((values * xx).sum() / energy),
                       float((values * yy).sum() / energy)))
        ordered = np.argsort(radius.ravel())
        cumulative = np.cumsum(values.ravel()[ordered])
        for fraction, label in ((0.50, "r50"), (0.90, "r90"), (0.99, "r99")):
            index = min(int(np.searchsorted(cumulative, cumulative[-1] * fraction)),
                        len(ordered) - 1)
            result[f"{name}_{label}_px"] = float(radius.ravel()[ordered[index]])
        result[f"{name}_centroid_x_px"] = active[-1][1]
        result[f"{name}_centroid_y_px"] = active[-1][2]
        if reference is not None:
            ref = np.maximum(reference[:, :, channel], 0.0)
            candidate_norm = values / max(energy, 1e-30)
            ref_norm = ref / max(float(ref.sum()), 1e-30)
            result[f"{name}_shape_l1_vs_61_percent"] = 100.0 * float(
                np.abs(candidate_norm - ref_norm).sum())
    for first_index, first in enumerate(active):
        for second in active[first_index + 1:]:
            result[f"centroid_separation_{first[0]}_{second[0]}_px"] = math.hypot(
                first[1] - second[1], first[2] - second[2])
    result["active_output_channels"] = ";".join(item[0] for item in active)
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/spectral-field-psf"))
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/spectral-field-psf.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/spectral-field-psf.png"))
    parser.add_argument("--resolution", type=int, default=512,
                        choices=(128, 256, 512, 1024, 2048, 4096))
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    executable = args.renderer.resolve()
    fields = (("center", 320.0, 180.0), ("left-edge", 4.0, 180.0))
    emitters = (("near-point", 0.7), ("extended", 7.0))
    records, plates = [], []
    for color_name, color in COLORS.items():
        for field, cx, cy in fields:
            for emitter, sigma in emitters:
                source = source_fixture(cx, cy, sigma, color)
                position = (cx / (WIDTH - 1), cy / (HEIGHT - 1))
                renders = {}
                for samples in SAMPLES:
                    output = args.output_dir / f"{color_name}-{field}-{emitter}-{samples}.pfm"
                    renders[samples] = render(executable, source, output, position,
                                              samples, args.resolution)
                reference = renders[61]
                for samples in SAMPLES:
                    metrics = image_metrics(renders[samples], source, (cx, cy),
                                            reference if samples != 61 else None)
                    record = {"color": color_name, "field": field, "emitter": emitter,
                              "sigma_px": sigma, "samples": samples,
                              "diffraction_resolution": args.resolution,
                              "visible_source_fraction": (
                                  float(source.sum()) / full_discrete_source_energy(sigma)),
                              **metrics}
                    records.append(record)
                    print(f"{color_name:5s} {field:9s} {emitter:11s} {samples:2d} bands "
                          f"R/G/B output share=" + "/".join(
                              f"{metrics[f'{ch}_output_fraction_of_total_output']:.3f}"
                              for ch in "rgb"), flush=True)
                plates.append((color_name, field, emitter, source, renders))

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as output:
        fieldnames = list(records[0])
        for record in records[1:]:
            fieldnames.extend(key for key in record if key not in fieldnames)
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(records)

    tile, header, label_height, gap = 104, 17, 14, 4
    columns = 4
    headers = ("Input", "3 bands", "61 bands", "|3−61| scaled per row")
    plate = Image.new("RGB", (columns * tile + (columns - 1) * gap,
                              header + len(plates) * (tile + label_height + gap)),
                      (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for column, label in enumerate(headers):
        draw.text((column * (tile + gap) + 2, 2), label, fill=(235, 238, 244))
    for row, (color_name, field, emitter, source, renders) in enumerate(plates):
        label = f"{color_name} / {field} / {emitter}"
        top = header + row * (tile + label_height + gap)
        draw.text((2, top + 1), label, fill=(230, 234, 240))
        center_x = 320.0 if field == "center" else 4.0
        crop_left = int(round(center_x)) - tile // 2
        crop_top = 180 - tile // 2
        difference = np.abs(renders[3] - renders[61])
        panels = (source, renders[3], renders[61], difference)
        for column, panel in enumerate(panels):
            x0, y0 = max(0, crop_left), max(0, crop_top)
            crop = panel[y0:y0 + tile, x0:x0 + tile]
            if crop.shape[:2] != (tile, tile):
                padded = np.zeros((tile, tile, 3), dtype=np.float64)
                padded[:crop.shape[0], :crop.shape[1]] = crop
                crop = padded
            gain = (1.0 / 4096.0) if column == 0 else 120.0
            if column == 3:
                reference_level = float(np.quantile(difference, 0.999))
                gain = 1024.0 / max(reference_level, 1e-30)
            tile_image = Image.fromarray(to_display(crop, gain))
            left = column * (tile + gap)
            plate.paste(tile_image, (left, top + label_height))
    args.image.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
