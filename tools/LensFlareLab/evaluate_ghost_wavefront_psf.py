"""Render a prescription-OPL coherent pupil PSF for one traced ghost path.

This is a monochromatic scalar Fourier-optics diagnostic. It removes piston,
tilt, and isotropic quadratic carrier from the sampled OPL; non-isotropic
quadratic aberration and higher orders remain in the pupil phase. It is not a
calibrated ghost image or a production renderer.
"""

import argparse
import csv
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def read_maps(path: Path, expected_size: int,
              wavelengths_nm: tuple[int, ...]) -> dict[int, tuple[np.ndarray, ...]]:
    count = expected_size * expected_size
    arrays = {
        wavelength: [np.empty(count, dtype=np.float64),
                     np.empty(count, dtype=np.float64),
                     np.empty(count, dtype=bool),
                     np.empty(count, dtype=np.float64),
                     np.empty(count, dtype=np.float64)]
        for wavelength in wavelengths_nm
    }
    indices = {wavelength: 0 for wavelength in wavelengths_nm}
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            wavelength = int(round(float(row["wavelength_nm"])))
            if wavelength not in arrays:
                continue
            index = indices[wavelength]
            if index >= count:
                raise ValueError(f"too many {wavelength} nm samples in {path}")
            columns = arrays[wavelength]
            columns[0][index] = float(row["pupil_u"])
            columns[1][index] = float(row["pupil_v"])
            columns[2][index] = int(row["valid"]) != 0
            columns[3][index] = float(row["phase_cycles"])
            columns[4][index] = float(row["energy"])
            indices[wavelength] += 1
    shape = (expected_size, expected_size)
    maps = {}
    for wavelength in wavelengths_nm:
        if indices[wavelength] != count:
            raise ValueError(f"expected {count} {wavelength} nm samples in {path}, "
                             f"got {indices[wavelength]}")
        maps[wavelength] = tuple(column.reshape(shape) for column in arrays[wavelength])
    return maps


def remove_reference(u: np.ndarray, v: np.ndarray, valid: np.ndarray,
                     phase: np.ndarray) -> tuple[np.ndarray, np.ndarray, float]:
    """Fit piston/tilt/full quadratic, remove only its isotropic curvature."""
    x, y = u[valid], v[valid]
    basis = np.column_stack((np.ones(x.size), x, y, x * x, x * y, y * y))
    coefficients, *_ = np.linalg.lstsq(basis, phase[valid], rcond=None)
    piston, tilt_x, tilt_y, qx, qxy, qy = coefficients
    isotropic = 0.5 * (qx + qy)
    residual = phase - (piston + tilt_x * u + tilt_y * v + isotropic * (u * u + v * v))
    residual[~valid] = 0.0
    # Return the coefficient vector to make the fitted curvature visible in metrics.
    return residual, coefficients, isotropic


def make_pupil(valid: np.ndarray, energy: np.ndarray,
               residual_phase_cycles: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    amplitude = np.where(valid, np.sqrt(np.maximum(energy, 0.0)), 0.0)
    mean = float(np.mean(amplitude[valid])) if np.any(valid) else 0.0
    if mean <= 0.0:
        raise ValueError("pupil has no positive ray energy")
    amplitude /= mean
    phased = amplitude * np.exp(2j * math.pi * residual_phase_cycles)
    return phased, amplitude.astype(np.complex128)


def psf(pupil: np.ndarray) -> np.ndarray:
    field = np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(pupil)))
    result = np.abs(field) ** 2
    result /= max(float(result.sum()), 1e-30)
    return result


def common_crop(image: np.ndarray, half_size: int) -> np.ndarray:
    center = image.shape[0] // 2
    return image[center - half_size:center + half_size + 1,
                 center - half_size:center + half_size + 1]


def metrics(image: np.ndarray, reference: np.ndarray) -> dict[str, float]:
    y, x = np.mgrid[:image.shape[0], :image.shape[1]]
    total = max(float(image.sum()), 1e-30)
    center = (image.shape[0] - 1) * 0.5
    cx, cy = float((image * x).sum() / total), float((image * y).sum() / total)
    comparison_total = max(float(reference.sum()), 1e-30)
    a, b = image / total, reference / comparison_total
    return {
        "energy_fraction_in_crop": total,
        "centroid_offset_bins": math.hypot(cx - center, cy - center),
        "normalized_l1_vs_flat_phase_percent": 100.0 * float(np.abs(a - b).sum()),
    }


def display(image: np.ndarray, gain: float) -> np.ndarray:
    value = np.log1p(np.maximum(image, 0.0) * gain)
    value /= max(float(value.max()), 1e-30)
    return np.clip(value * 255.0 + 0.5, 0, 255).astype(np.uint8)


def render_plate(cases: list[dict], output: Path) -> None:
    tile_size, label_height = 320, 30
    width = tile_size * 5
    height = (tile_size + label_height) * len(cases)
    plate = Image.new("RGB", (width, height), (20, 23, 29))
    draw = ImageDraw.Draw(plate)
    for row_index, case in enumerate(cases):
        top = row_index * (tile_size + label_height)
        panels = (case["low_phase"], case["phase"], case["grid_difference"],
                  case["flat"], case["pupil"])
        labels = (f"OPL PSF {case['low_grid']}²", f"OPL PSF {case['grid']}²",
                  "Absolute grid difference", "Same amplitude, flat phase",
                  "Stop-grid residual OPL phase")
        for column, (data, label) in enumerate(zip(panels, labels)):
            left = column * tile_size
            if column == 4:
                phase = np.angle(data) / (2.0 * math.pi)
                hue = np.mod(phase, 1.0)
                hsv = np.empty((*hue.shape, 3), dtype=np.uint8)
                hsv[:, :, 0] = np.clip(hue * 255.0 + 0.5, 0, 255).astype(np.uint8)
                hsv[:, :, 1] = 230
                hsv[:, :, 2] = 242
                rgb = np.asarray(Image.fromarray(hsv, "HSV").convert("RGB")).copy()
                rgb[np.abs(data) <= 0.0] = 0
                gray = Image.fromarray(rgb)
            else:
                gain = 2e6 if column == 2 else 5e5
                gray = Image.fromarray(display(data, gain), "L").convert("RGB")
            gray = gray.resize((tile_size, tile_size), Image.Resampling.NEAREST)
            plate.paste(gray, (left, top + label_height))
            draw.text((left + 7, top + 8), label, fill=(238, 241, 246))
            draw.rectangle((left, top + label_height, left + tile_size - 1,
                            top + label_height + tile_size - 1), outline=(66, 72, 83))
        draw.text((8, top + tile_size + label_height - 18),
                  f"{case['wavelength_nm']} nm, grid {case['grid']}²; pupil/PSF independently normalized",
                  fill=(210, 215, 224))
    output.parent.mkdir(parents=True, exist_ok=True)
    plate.save(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grid", type=int, choices=(256, 512, 1024, 2048), action="append",
                        help="input ray grids; repeat for convergence (default: 256 and 512)")
    parser.add_argument("--map-pattern", default=(
        "build/tools/LensFlareLab/opl-fixed-grid-{grid}.csv"))
    parser.add_argument("--wavelengths-nm", type=int, nargs="+", default=(460, 550, 610))
    parser.add_argument("--crop", type=int, default=96,
                        help="half-width in normalized Fourier bins for metrics")
    parser.add_argument("--csv", type=Path, default=Path(
        "tools/LensFlareLab/examples/ghost-wavefront-psf-20261028.csv"))
    parser.add_argument("--image", type=Path, default=Path(
        "tools/LensFlareLab/examples/ghost-wavefront-psf-20261028.png"))
    args = parser.parse_args()
    grids = args.grid or [256, 512]
    if len(set(grids)) != len(grids) or args.crop < 1:
        parser.error("grid values must be unique and crop must be positive")

    records, cases = [], []
    maps_by_grid = {}
    for grid in grids:
        path = Path(args.map_pattern.format(grid=grid))
        if not path.is_file():
            parser.error(f"missing optical-path map: {path}")
        maps_by_grid[grid] = read_maps(path, grid, tuple(args.wavelengths_nm))

    for wavelength in args.wavelengths_nm:
        per_grid = {}
        for grid in grids:
            u, v, valid, phase, energy = maps_by_grid[grid][wavelength]
            size = grid
            residual, coefficients, isotropic = remove_reference(u, v, valid, phase)
            coherent_pupil, flat_pupil = make_pupil(valid, energy, residual)
            phased_psf, flat_psf = psf(coherent_pupil), psf(flat_pupil)
            half = min(args.crop, size // 2 - 1)
            phased_crop = common_crop(phased_psf, half)
            flat_crop = common_crop(flat_psf, half)
            open_mask = valid
            residual_rms = float(np.sqrt(np.mean(residual[open_mask] ** 2)))
            record = {
                "grid": grid, "wavelength_nm": wavelength,
                "valid_pupil_fraction": float(valid.mean()),
                "residual_opd_rms_waves": residual_rms,
                "fit_piston_cycles": float(coefficients[0]),
                "fit_tilt_u_cycles": float(coefficients[1]),
                "fit_tilt_v_cycles": float(coefficients[2]),
                "fit_quadratic_u_cycles": float(coefficients[3]),
                "fit_quadratic_uv_cycles": float(coefficients[4]),
                "fit_quadratic_v_cycles": float(coefficients[5]),
                "isotropic_quadratic_cycles": float(isotropic),
                "phase_psf_peak_ratio_vs_flat": float(
                    phased_psf.max() / max(float(flat_psf.max()), 1e-30)),
                **metrics(phased_crop, flat_crop),
            }
            records.append(record)
            per_grid[grid] = {"phase": phased_psf, "flat": flat_psf,
                              "residual": residual, "pupil": coherent_pupil}
            print(f"{wavelength} nm grid={grid} valid={valid.mean():.3%} "
                  f"residual RMS={residual_rms:.4f} waves "
                  f"PSF peak ratio={record['phase_psf_peak_ratio_vs_flat']:.4f} "
                  f"L1 vs flat={record['normalized_l1_vs_flat_phase_percent']:.2f}%")

        ordered_grids = sorted(grids)
        for low, high in zip(ordered_grids, ordered_grids[1:]):
            limit = min(args.crop, low // 2 - 1)
            low_image = common_crop(per_grid[low]["phase"], limit)
            high_image = common_crop(per_grid[high]["phase"], limit)
            convergence = metrics(low_image, high_image)
            records.append({"grid": f"{low}-to-{high}", "wavelength_nm": wavelength,
                            "convergence_normalized_l1_percent":
                                convergence["normalized_l1_vs_flat_phase_percent"],
                            "convergence_centroid_offset_bins":
                                convergence["centroid_offset_bins"]})
            print(f"{wavelength} nm {low}²→{high}² common crop normalized L1="
                  f"{convergence['normalized_l1_vs_flat_phase_percent']:.3f}%")

        grid = max(grids)
        phase_image = per_grid[grid]["phase"]
        flat_image = per_grid[grid]["flat"]
        half = min(args.crop, grid // 2 - 1)
        previous_grid = max((value for value in grids if value < grid), default=grid)
        phase_crop = common_crop(phase_image, half)
        flat_crop = common_crop(flat_image, half)
        previous_crop = common_crop(per_grid[previous_grid]["phase"], half)
        phase_normalized = phase_crop / max(float(phase_crop.sum()), 1e-30)
        previous_normalized = previous_crop / max(float(previous_crop.sum()), 1e-30)
        cases.append({"wavelength_nm": wavelength, "grid": grid,
                      "low_grid": previous_grid, "low_phase": previous_crop,
                      "phase": phase_crop,
                      "grid_difference": np.abs(phase_normalized - previous_normalized),
                      "flat": flat_crop,
                      "pupil": per_grid[grid]["pupil"]})

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        fieldnames = list(dict.fromkeys(key for record in records for key in record))
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(records)
    render_plate(cases, args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
