"""Ablate the ray-energy to exit-pupil-area Jacobian on ghost PSF maps."""

import argparse
import csv
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_ghost_exit_pupil_psf as optics


def normalized(values: np.ndarray) -> np.ndarray:
    positive = np.maximum(np.asarray(values, dtype=np.float64), 0.0)
    return positive / max(float(positive.sum()), 1e-30)


def make_chart(cases: list[tuple[str, np.ndarray]], output: Path) -> None:
    tile, header = 256, 28
    sheet = Image.new("RGB", (tile * 3, (tile + header) * 2), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    psfs = [values for _, values in cases if not _.startswith("|Δ|")]
    differences = [values for label, values in cases if label.startswith("|Δ|")]
    psf_scale = max(float(np.percentile(np.concatenate([x.ravel() for x in psfs]), 99.9)),
                    1e-30)
    diff_scale = max(float(np.percentile(np.concatenate([x.ravel() for x in differences]), 99.9)),
                     1e-30)
    for index, (label, values) in enumerate(cases):
        row, column = divmod(index, 3)
        scale = diff_scale if label.startswith("|Δ|") else psf_scale
        shown = np.log1p(np.maximum(values, 0.0) / scale) / np.log(2.0)
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
    parser.add_argument("--pixel-subsamples", type=int, choices=(2, 4, 8), default=2)
    parser.add_argument("--detector-offset-px", type=float, nargs=2, default=(0.157536, 0.319445))
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/ghost-pupil-jacobian-ablation.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/ghost-pupil-jacobian-ablation.png"))
    args = parser.parse_args()

    fields: dict[int, dict[str, tuple[np.ndarray, np.ndarray, float]]] = {}
    for grid in (args.low_grid, args.high_grid):
        data = optics.read_map(Path(args.map_pattern.format(grid=grid)), grid,
                               args.wavelength_nm)
        pupil_xy, center, pupil_z, pupil_rms = optics.fit_exit_pupil(data)
        opd, _, reference_radius, _ = optics.image_centered_opd(
            data, args.wavelength_nm, pupil_xy, center, pupil_z)
        fields[grid] = {}
        for method in ("triangle", "launch-fd"):
            wavefront, pitch, amplitude = optics.map_to_regular_pupil(
                data, pupil_xy, opd, args.pupil_resolution, "linear", method, "delaunay")
            psf, _, _, _, _, crop_fraction, _ = optics.psf_and_pixel_integral(
                wavefront, amplitude, args.wavelength_nm, pitch, reference_radius,
                args.crop_radius, args.pixel_subsamples, *args.detector_offset_px)
            fields[grid][method] = (amplitude, psf, crop_fraction)
        print(f"grid={grid} pupil_rms={pupil_rms * 1000:.3f} um")

    rows = []
    cases = []
    for method, label in (("triangle", "Delaunay triangle Jacobian"),
                          ("launch-fd", "Structured launch-grid finite difference")):
        low_amp, low_psf, low_fraction = fields[args.low_grid][method]
        high_amp, high_psf, high_fraction = fields[args.high_grid][method]
        low_norm, high_norm = normalized(low_psf), normalized(high_psf)
        low_support, high_support = low_amp > 0.0, high_amp > 0.0
        row = {
            "method": method, "low_grid": args.low_grid, "high_grid": args.high_grid,
            "wavelength_nm": args.wavelength_nm,
            "pupil_resolution": args.pupil_resolution,
            "crop_radius_px": args.crop_radius,
            "pixel_subsamples": args.pixel_subsamples,
            "detector_offset_x_px": args.detector_offset_px[0],
            "detector_offset_y_px": args.detector_offset_px[1],
            "normalized_psf_l1_percent": 100.0 * float(np.abs(low_norm - high_norm).sum()),
            "amplitude_relative_l2_percent": 100.0 * float(
                np.linalg.norm(high_amp - low_amp) / max(np.linalg.norm(low_amp), 1e-30)),
            "support_iou_percent": 100.0 * float(
                (low_support & high_support).sum() / max((low_support | high_support).sum(), 1)),
            "low_crop_energy_fraction": low_fraction,
            "high_crop_energy_fraction": high_fraction,
        }
        rows.append(row)
        cases.extend(((f"{args.low_grid}x {label}", low_norm),
                      (f"{args.high_grid}x {label}", high_norm),
                      ("|Δ| normalized PSFs", np.abs(low_norm - high_norm))))

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    make_chart(cases, args.image)
    for row in rows:
        print(f"{row['method']}: PSF L1={row['normalized_psf_l1_percent']:.3f}%, "
              f"amplitude L2={row['amplitude_relative_l2_percent']:.3f}%, "
              f"support IoU={row['support_iou_percent']:.3f}%")
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
