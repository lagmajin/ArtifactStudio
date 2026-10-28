"""Ablate mapped-pupil support, amplitude, and OPD contributions to ghost PSF drift."""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from evaluate_ghost_exit_pupil_psf import (
    SENSOR_WIDTH_MM,
    SENSOR_WIDTH_PX,
    fit_exit_pupil,
    image_centered_opd,
    map_to_regular_pupil,
    read_map,
)


def sensor_psf(opd: np.ndarray, amplitude: np.ndarray, wavelength_nm: int,
               pitch_mm: float, reference_radius_mm: float, crop_radius: int,
               pixel_subsamples: int) -> np.ndarray:
    pupil = amplitude * np.exp(2j * math.pi * opd * 1e6 / wavelength_nm)
    sensor_pixel_pitch = SENSOR_WIDTH_MM / SENSOR_WIDTH_PX
    wavelength_mm = wavelength_nm * 1e-6
    offsets = (np.arange(pixel_subsamples, dtype=np.float64) + 0.5) / pixel_subsamples - 0.5
    centers = np.arange(-crop_radius, crop_radius + 1, dtype=np.float64)
    positions_mm = (centers[:, None] + offsets[None, :]).ravel() * sensor_pixel_pitch
    frequencies = positions_mm / (wavelength_mm * reference_radius_mm)
    axis = (np.arange(opd.shape[0], dtype=np.float64) + 0.5 - opd.shape[0] * 0.5) * pitch_mm
    kernel = np.exp(-2j * math.pi * np.outer(frequencies, axis))
    field = kernel @ pupil @ kernel.T
    subpixel = np.abs(field) ** 2
    side = 2 * crop_radius + 1
    pixels = subpixel.reshape(side, pixel_subsamples, side, pixel_subsamples).mean(axis=(1, 3))
    return pixels / max(float(pixels.sum()), 1e-30)


def encircled_radius(image: np.ndarray, fraction: float) -> float:
    center = image.shape[0] // 2
    y, x = np.mgrid[:image.shape[0], :image.shape[1]]
    radius = np.hypot(x - center, y - center).ravel()
    values = image.ravel()
    order = np.argsort(radius)
    index = min(int(np.searchsorted(np.cumsum(values[order]), fraction)), len(order) - 1)
    return float(radius[order[index]])


def save_plate(results: list[dict], output: Path, wavelength: int,
               opd_interpolation: str) -> None:
    tile, header, footer = 256, 28, 24
    row_height = tile + header + footer
    image = Image.new("RGB", (tile * 3, row_height * len(results)), (18, 21, 26))
    draw = ImageDraw.Draw(image)
    for row, result in enumerate(results):
        top = row * row_height
        pair = (result["low"], result["high"])
        difference = np.abs(pair[0] - pair[1])
        peak = max(float(pair[0].max()), float(pair[1].max()), 1e-30)
        diff_peak = max(float(difference.max()), 1e-30)
        for col, (label, values, scale) in enumerate((
                ("Lower grid", pair[0], peak),
                ("Higher grid", pair[1], peak),
                ("Absolute difference", difference, diff_peak))):
            left = col * tile
            draw.text((left + 6, top + 7), label, fill=(235, 238, 244))
            relative = np.maximum(values, 0.0) / scale
            shown = np.clip((np.log10(np.maximum(relative, 1e-4)) + 4.0) / 4.0,
                            0.0, 1.0)
            gray = Image.fromarray(np.clip(shown * 255 + 0.5, 0, 255).astype(np.uint8))
            image.paste(gray.resize((tile, tile), Image.Resampling.NEAREST), (left, top + header))
        draw.text((6, top + header + tile + 5),
                  f"{wavelength} nm | {opd_interpolation} OPD | {result['name']} | "
                  f"PSF L1 {result['l1']:.2f}%",
                  fill=(210, 215, 224))
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--low-grid", type=int, default=512)
    parser.add_argument("--high-grid", type=int, default=1024)
    parser.add_argument("--wavelength-nm", type=int, default=550)
    parser.add_argument("--pupil-resolution", type=int, default=512)
    parser.add_argument("--crop-radius", type=int, default=32)
    parser.add_argument("--pixel-subsamples", type=int, choices=(2, 4, 8), default=2)
    parser.add_argument("--opd-interpolation", choices=("linear", "clough-tocher"),
                        default="linear")
    parser.add_argument("--map-pattern", default="build/tools/LensFlareLab/ghost-exit-grid-{grid}.csv")
    parser.add_argument("--csv", type=Path,
                        default=Path("build/tools/LensFlareLab/ghost-pupil-ablation.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("build/tools/LensFlareLab/ghost-pupil-ablation.png"))
    args = parser.parse_args()
    if args.high_grid != 2 * args.low_grid:
        parser.error("high grid must be exactly twice low grid")

    fields = []
    for grid in (args.low_grid, args.high_grid):
        data = read_map(Path(args.map_pattern.format(grid=grid)), grid, args.wavelength_nm)
        pupil_xy, pupil_center, pupil_z, _ = fit_exit_pupil(data)
        opd, _, reference_radius, _ = image_centered_opd(
            data, args.wavelength_nm, pupil_xy, pupil_center, pupil_z)
        pupil_opd, pitch, amplitude = map_to_regular_pupil(
            data, pupil_xy, opd, args.pupil_resolution, args.opd_interpolation)
        fields.append({"opd": pupil_opd, "amplitude": amplitude,
                       "pitch": pitch, "reference_radius": reference_radius})

    low, high = fields
    low_support = low["amplitude"] > 0.0
    high_support = high["amplitude"] > 0.0
    common_support = low_support & high_support
    if int(common_support.sum()) < 16:
        parser.error("ray maps have too few common pupil samples")
    mask = common_support.astype(np.float64)
    common_amplitude = 0.5 * (low["amplitude"] + high["amplitude"]) * mask
    common_opd = 0.5 * (low["opd"] + high["opd"])
    pitch = 0.5 * (low["pitch"] + high["pitch"])
    reference_radius = 0.5 * (low["reference_radius"] + high["reference_radius"])

    conditions = (
        ("Native support, amplitude, and OPD",
         low["opd"], low["amplitude"], high["opd"], high["amplitude"]),
        ("Common support; native amplitude and OPD",
         low["opd"], low["amplitude"] * mask, high["opd"], high["amplitude"] * mask),
        ("Common support and amplitude; native OPD",
         low["opd"], common_amplitude, high["opd"], common_amplitude),
        ("Common support and OPD; native amplitude",
         common_opd, low["amplitude"] * mask, common_opd, high["amplitude"] * mask),
    )
    results = []
    for name, low_opd, low_amp, high_opd, high_amp in conditions:
        low_psf = sensor_psf(low_opd, low_amp, args.wavelength_nm, pitch,
                             reference_radius, args.crop_radius, args.pixel_subsamples)
        high_psf = sensor_psf(high_opd, high_amp, args.wavelength_nm, pitch,
                              reference_radius, args.crop_radius, args.pixel_subsamples)
        l1 = 100.0 * float(np.abs(low_psf - high_psf).sum())
        results.append({"name": name, "low": low_psf, "high": high_psf, "l1": l1,
                        "low_r50": encircled_radius(low_psf, 0.5),
                        "high_r50": encircled_radius(high_psf, 0.5),
                        "low_r90": encircled_radius(low_psf, 0.9),
                        "high_r90": encircled_radius(high_psf, 0.9)})
        print(f"{name}: PSF L1={l1:.3f}%, r50={results[-1]['low_r50']:.2f}/"
              f"{results[-1]['high_r50']:.2f}px, r90={results[-1]['low_r90']:.2f}/"
              f"{results[-1]['high_r90']:.2f}px")

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("condition", "opd_interpolation", "low_grid", "high_grid",
            "wavelength_nm", "pupil_resolution", "common_support_samples",
            "common_support_iou_percent", "normalized_psf_l1_percent",
            "low_r50_px", "high_r50_px", "low_r90_px", "high_r90_px"))
        writer.writeheader()
        common_iou = 100.0 * float(common_support.sum() /
                                   max(int((low_support | high_support).sum()), 1))
        for result in results:
            writer.writerow({"condition": result["name"], "opd_interpolation": args.opd_interpolation,
                "low_grid": args.low_grid,
                "high_grid": args.high_grid, "wavelength_nm": args.wavelength_nm,
                "pupil_resolution": args.pupil_resolution,
                "common_support_samples": int(common_support.sum()),
                "common_support_iou_percent": common_iou,
                "normalized_psf_l1_percent": result["l1"],
                "low_r50_px": result["low_r50"], "high_r50_px": result["high_r50"],
                "low_r90_px": result["low_r90"], "high_r90_px": result["high_r90"]})
    save_plate(results, args.image, args.wavelength_nm, args.opd_interpolation)
    print(f"saved {args.csv} and {args.image}; common support IoU={common_iou:.2f}%")


if __name__ == "__main__":
    main()
