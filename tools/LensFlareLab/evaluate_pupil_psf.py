"""Evaluate ideal pupil diffraction PSF energy, symmetry, and grid convergence.

This isolates the Fraunhofer stop model from lens aberrations, sensor response,
and the separate Fresnel ghost approximation.
"""

import argparse
import csv
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        data = np.frombuffer(stream.read(), dtype="<f4")
    if data.size != width * height * 3 or not math.isfinite(scale) or scale == 0:
        raise ValueError(f"invalid PFM payload: {path}")
    return np.flipud(data.reshape(height, width, 3)).astype(np.float64) * abs(scale)


def write_pfm(path: Path, image: np.ndarray) -> None:
    height, width, _ = image.shape
    with path.open("wb") as stream:
        stream.write(f"PF\n{width} {height}\n-1.0\n".encode("ascii"))
        np.flipud(image.astype("<f4", copy=False)).tofile(stream)


def removed_source(source: np.ndarray, threshold: float) -> np.ndarray:
    source_y = source @ np.array((0.2126, 0.7152, 0.0722))
    excess = np.maximum(source_y - threshold, 0.0)
    fraction = excess / np.maximum(source_y, 1e-30)
    return source * fraction[:, :, None]


def psf_from_render(source: np.ndarray, rendered: np.ndarray,
                    threshold: float) -> np.ndarray:
    # The rendered source retains its thresholded remainder; restore only the
    # amount removed at the requested threshold before isolating the PSF.
    return rendered - source + removed_source(source, threshold)


def metrics(psf: np.ndarray, source_y_energy: float, source_rgb_energy: np.ndarray,
            reference: np.ndarray | None) -> dict[str, float | str]:
    height, width, _ = psf.shape
    center_x, center_y = (width - 1) * 0.5, (height - 1) * 0.5
    yy, xx = np.mgrid[:height, :width]
    weights = np.array((0.2126, 0.7152, 0.0722))
    output_rgb_energy = psf.sum(axis=(0, 1))
    output_y_energy = float(output_rgb_energy @ weights)
    y = psf @ weights
    cx = float((y * xx).sum() / max(output_y_energy, 1e-30))
    cy = float((y * yy).sum() / max(output_y_energy, 1e-30))
    mirror_l1 = float(np.abs(psf - np.flip(psf, axis=1)).sum()) / max(float(np.abs(psf).sum()), 1e-30)
    radial = np.hypot(xx - round(center_x), yy - round(center_y)).ravel()
    positive_y = np.maximum(y, 0).ravel()
    order = np.argsort(radial)
    cumulative = np.cumsum(positive_y[order])

    def radius(fraction: float) -> float:
        index = min(int(np.searchsorted(cumulative, cumulative[-1] * fraction)), len(order) - 1)
        return float(radial[order[index]])

    def core_radius(fraction: float) -> float:
        positive_pixels = np.maximum(y, 0).ravel()
        center_mask = radial <= 2.0
        core_total = float(positive_pixels[center_mask].sum())
        core_order = np.argsort(radial[center_mask])
        core_radii = radial[center_mask][core_order]
        core_values = positive_pixels[center_mask][core_order]
        core_cumulative = np.cumsum(core_values)
        index = min(int(np.searchsorted(core_cumulative, core_total * fraction)), len(core_order) - 1)
        return float(core_radii[index])

    l1 = "" if reference is None else 100.0 * float(np.abs(psf - reference).sum()) / max(
        float(np.abs(reference).sum()), 1e-30)
    return {
        "throughput_ratio": output_y_energy / max(source_y_energy, 1e-30),
        "channel_throughput_r": float(output_rgb_energy[0] / max(source_rgb_energy[0], 1e-30)),
        "channel_throughput_g": float(output_rgb_energy[1] / max(source_rgb_energy[1], 1e-30)),
        "channel_throughput_b": float(output_rgb_energy[2] / max(source_rgb_energy[2], 1e-30)),
        "centroid_error_px": math.hypot(cx - center_x, cy - center_y),
        "horizontal_mirror_l1_percent": mirror_l1 * 100.0,
        "r50_luminance_px": radius(0.5),
        "r50_core_2px_luminance_px": core_radius(0.5),
        "r99_luminance_px": radius(0.99),
        "rgb_l1_vs_dense_percent": l1,
    }


def to_display(image: np.ndarray, gain: float) -> np.ndarray:
    values = np.maximum(image, 0.0) * gain
    values = np.log1p(values) / np.log1p(1024.0)
    srgb = np.where(values <= .0031308, 12.92 * values,
                    1.055 * np.power(values, 1 / 2.4) - .055)
    return np.clip(srgb * 255 + .5, 0, 255).astype(np.uint8)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("renderer", type=Path)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("build/tools/LensFlareLab/pupil-psf-study"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/pupil-psf-diagnostic.png"))
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/pupil-psf-diagnostic.csv"))
    parser.add_argument("--quick", action="store_true",
                        help="use 256/512/1024 grids instead of the full 512/1024/2048/4096 sweep")
    args = parser.parse_args()
    if not args.renderer.is_file():
        parser.error(f"renderer does not exist: {args.renderer}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    width, height = 640, 360
    y, x = np.mgrid[:height, :width]
    source = np.empty((height, width, 3), dtype=np.float64)
    source_y = 1024.0 * np.exp(-((x - 319.5) ** 2 + (y - 179.5) ** 2) / (2.0 * .7 ** 2))
    source[:] = source_y[:, :, None]
    luma = np.array((0.2126, 0.7152, 0.0722))
    source_rgb_energy = source.sum(axis=(0, 1))
    source_y_energy = float(source_rgb_energy @ luma)
    # Threshold zero extracts and replaces the complete Gaussian source.
    source_path = args.output_dir / "centered-neutral-source.pfm"
    write_pfm(source_path, source)

    resolutions = (256, 512, 1024) if args.quick else (512, 1024, 2048, 4096)
    cases = ((5, 0.0), (6, 0.0), (8, 0.0), (10, 0.0), (6, 0.30))
    raw: dict[tuple[int, float, int], np.ndarray] = {}
    records: list[dict[str, float | str | int]] = []
    renderer = str(args.renderer.resolve())
    for blades, obstruction in cases:
        for resolution in resolutions:
            output = args.output_dir / f"{blades}-blade-obstruction-{obstruction:.2f}-{resolution}.pfm"
            command = [renderer, str(source_path.resolve()), str(output.resolve()),
                       "--position", str(319.5 / 639), str(179.5 / 359),
                       "--threshold", "0", "--gain", "1", "--source-samples", "1",
                       "--ghosts", "off", "--diffraction", "on",
                       "--diffraction-resolution", str(resolution),
                       "--diffraction-resampling", "pixel-area", "--spectral-samples", "3",
                       "--aperture-blades", str(blades),
                       "--central-obstruction-ratio", str(obstruction)]
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
            flare = psf_from_render(source, read_pfm(output), 0.0)
            raw[blades, obstruction, resolution] = flare
            values = metrics(flare, source_y_energy, source_rgb_energy, None)
            values.update({"blades": blades, "obstruction_ratio": obstruction,
                           "resolution": resolution})
            records.append(values)
            print(f"{blades:2d} blades ε={obstruction:.2f} FFT={resolution:4d}: "
                  f"throughput={values['throughput_ratio']:.8f} "
                  f"mirrorL1={values['horizontal_mirror_l1_percent']:.6f}% "
                  f"r99={values['r99_luminance_px']:.3f}px", flush=True)

    dense = resolutions[-1]
    for row in records:
        key = int(row["blades"]), float(row["obstruction_ratio"]), int(row["resolution"])
        row["rgb_l1_vs_dense_percent"] = metrics(
            raw[key], source_y_energy, source_rgb_energy, raw[key[0], key[1], dense]
        )["rgb_l1_vs_dense_percent"]
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

    tile, label_height, gap = 128, 20, 5
    plate = Image.new("RGB", (len(resolutions) * tile + (len(resolutions) - 1) * gap,
                              len(cases) * (tile + label_height + gap)), (18, 21, 26))
    draw = ImageDraw.Draw(plate)
    for row, (blades, obstruction) in enumerate(cases):
        gain = 1.0e4
        for column, resolution in enumerate(resolutions):
            left, top = column * (tile + gap), row * (tile + label_height + gap)
            flare = raw[blades, obstruction, resolution]
            crop = flare[115:244, 255:384]
            image = Image.fromarray(to_display(crop, gain))
            display_scale = 1.0
            display_size = max(1, min(tile, int(round(tile * display_scale))))
            image = image.resize((display_size, display_size), Image.Resampling.BILINEAR)
            image_left = left + (tile - display_size) // 2
            image_top = top + label_height + (tile - display_size) // 2
            plate.paste(image, (image_left, image_top))
            draw.text((left + 3, top + 2), f"{blades} blades ε={obstruction:.2f} N={resolution}",
                      fill=(225, 230, 238))
    args.image.parent.mkdir(parents=True, exist_ok=True)
    plate.save(args.image)
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
