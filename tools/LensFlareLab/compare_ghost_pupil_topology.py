"""Compare Delaunay and launch-cell-preserving ghost pupil reconstruction."""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_ghost_exit_pupil_psf as optics


def normalized(image: np.ndarray) -> np.ndarray:
    positive = np.maximum(np.asarray(image, dtype=np.float64), 0.0)
    return positive / max(float(positive.sum()), 1e-30)


def make_chart(cases: list[tuple[str, np.ndarray]], output: Path) -> None:
    tile, header = 256, 28
    sheet = Image.new("RGB", (tile * 3, (tile + header) * (len(cases) // 3)), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    psfs = [values for label, values in cases
            if not label.startswith(("abs delta", "support", "mask delta"))]
    differences = [values for label, values in cases if label.startswith("abs delta PSF")]
    psf_scale = max(float(np.percentile(np.concatenate([x.ravel() for x in psfs]), 99.9)), 1e-30)
    diff_scale = max(float(np.percentile(np.concatenate([x.ravel() for x in differences]), 99.9)), 1e-30)
    for index, (label, values) in enumerate(cases):
        row, column = divmod(index, 3)
        if label.startswith(("support", "mask delta")):
            shown = np.maximum(values, 0.0)
        elif label.startswith("abs delta PSF"):
            shown = np.log1p(np.maximum(values, 0.0) / diff_scale) / np.log(2.0)
        else:
            shown = np.log1p(np.maximum(values, 0.0) / psf_scale) / np.log(2.0)
        gray = Image.fromarray(np.clip(shown * 255.0 + 0.5, 0, 255).astype(np.uint8))
        gray = gray.resize((tile, tile), Image.Resampling.NEAREST)
        left, top = column * tile, row * (tile + header)
        draw.text((left + 6, top + 7), label, fill=(235, 238, 244))
        sheet.paste(gray, (left, top + header))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--low-grid", type=int, default=512)
    parser.add_argument("--high-grid", type=int, default=1024)
    parser.add_argument("--wavelength-nm", type=int, default=550)
    parser.add_argument("--map-pattern", default="build/tools/LensFlareLab/psf-reference-550-center-grid{grid}.csv")
    parser.add_argument("--pupil-resolution", type=int, default=512)
    parser.add_argument("--crop-radius", type=int, default=12)
    parser.add_argument("--pixel-subsamples", type=int, choices=(2, 4, 8), default=4)
    parser.add_argument("--detector-offset-px", type=float, nargs=2, default=None,
                        help="override chief-ray subpixel offset; default derives it from the low-grid map")
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/ghost-pupil-topology-ablation.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/ghost-pupil-topology-ablation.png"))
    args = parser.parse_args()

    mapped: dict[int, dict[str, tuple[np.ndarray, np.ndarray, float]]] = {}
    methods = (("delaunay", "Delaunay pupil mesh"),
               ("launch-grid", "TL-BR launch-cell mesh"),
               ("launch-grid-alt", "TR-BL launch-cell mesh"))
    for grid in (args.low_grid, args.high_grid):
        data = optics.read_map(Path(args.map_pattern.format(grid=grid)), grid,
                               args.wavelength_nm)
        pupil_xy, center, pupil_z, pupil_rms = optics.fit_exit_pupil(data)
        opd, image_point, reference_radius, _ = optics.image_centered_opd(
            data, args.wavelength_nm, pupil_xy, center, pupil_z)
        if grid == args.low_grid:
            center_x = (image_point[0] / optics.SENSOR_WIDTH_MM + 0.5) * optics.SENSOR_WIDTH_PX
            center_y = (0.5 - image_point[1] / optics.SENSOR_WIDTH_MM) * optics.SENSOR_WIDTH_PX
            offset_x = math.floor(center_x + 0.5) - center_x
            offset_y = math.floor(center_y + 0.5) - center_y
            detector_offset = (tuple(args.detector_offset_px) if args.detector_offset_px
                               else (offset_x, offset_y))
        mapped[grid] = {}
        for method, _ in methods:
            wavefront, pitch, amplitude = optics.map_to_regular_pupil(
                data, pupil_xy, opd, args.pupil_resolution, "linear", "triangle", method)
            psf, _, _, _, _, crop_fraction, _ = optics.psf_and_pixel_integral(
                wavefront, amplitude, args.wavelength_nm, pitch, reference_radius,
                args.crop_radius, args.pixel_subsamples, *detector_offset)
            mapped[grid][method] = (amplitude, psf, crop_fraction)
        print(f"grid={grid} pupil_rms={pupil_rms * 1000:.3f} um")

    cases = []
    rows = []
    for method, label in methods:
        low_amp, low_psf, low_fraction = mapped[args.low_grid][method]
        high_amp, high_psf, high_fraction = mapped[args.high_grid][method]
        low_norm, high_norm = normalized(low_psf), normalized(high_psf)
        low_support, high_support = low_amp > 0.0, high_amp > 0.0
        row = {
            "method": method, "low_grid": args.low_grid, "high_grid": args.high_grid,
            "wavelength_nm": args.wavelength_nm,
            "pupil_resolution": args.pupil_resolution,
            "crop_radius_px": args.crop_radius,
            "pixel_subsamples": args.pixel_subsamples,
            "detector_offset_x_px": detector_offset[0],
            "detector_offset_y_px": detector_offset[1],
            "normalized_psf_l1_percent": 100.0 * float(np.abs(low_norm - high_norm).sum()),
            "amplitude_relative_l2_percent": 100.0 * float(
                np.linalg.norm(high_amp - low_amp) / max(np.linalg.norm(low_amp), 1e-30)),
            "support_iou_percent": 100.0 * float(
                (low_support & high_support).sum() / max((low_support | high_support).sum(), 1)),
            "low_support_pixels": int(low_support.sum()),
            "high_support_pixels": int(high_support.sum()),
            "low_crop_energy_fraction": low_fraction,
            "high_crop_energy_fraction": high_fraction,
        }
        rows.append(row)
        cases.extend(((f"{args.low_grid}x {label} PSF", low_norm),
                      (f"{args.high_grid}x {label} PSF", high_norm),
                      ("abs delta PSF", np.abs(low_norm - high_norm)),
                      (f"support {args.low_grid}x {label}", low_support.astype(np.float64)),
                      (f"support {args.high_grid}x {label}", high_support.astype(np.float64)),
                      ("mask delta support", np.abs(low_support.astype(np.float64) -
                                                    high_support.astype(np.float64)))))

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    make_chart(cases, args.image)
    for row in rows:
        print(f"{row['method']}: PSF L1={row['normalized_psf_l1_percent']:.3f}%, "
              f"amplitude L2={row['amplitude_relative_l2_percent']:.3f}%, "
              f"support IoU={row['support_iou_percent']:.3f}% "
              f"({row['low_support_pixels']}->{row['high_support_pixels']} pixels)")
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
