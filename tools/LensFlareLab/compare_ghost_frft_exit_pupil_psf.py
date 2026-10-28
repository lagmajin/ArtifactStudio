"""Compare a ghost render crop with a ray-mapped scalar Fraunhofer reference PSF."""

import argparse
import csv
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import evaluate_ghost_exit_pupil_psf as optics


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"expected RGB PFM: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        if scale >= 0.0:
            raise ValueError(f"expected little-endian PFM: {path}")
        pixels = np.frombuffer(stream.read(), dtype="<f4")
    if pixels.size != width * height * 3:
        raise ValueError(f"invalid RGB payload in {path}")
    return pixels.reshape(height, width, 3)[::-1].copy() * abs(scale)


def normalized(image: np.ndarray) -> np.ndarray:
    data = np.maximum(np.asarray(image, dtype=np.float64), 0.0)
    return data / max(float(data.sum()), 1e-30)


def centroid(image: np.ndarray) -> tuple[float, float]:
    values = normalized(image)
    yy, xx = np.mgrid[:values.shape[0], :values.shape[1]]
    return (float(np.sum(values * (xx - values.shape[1] // 2))),
            float(np.sum(values * (yy - values.shape[0] // 2))))


def make_chart(cases: list[tuple[str, np.ndarray]], output: Path) -> None:
    tile = 256
    header = 30
    sheet = Image.new("RGB", (tile * len(cases), tile + header), (18, 21, 26))
    draw = ImageDraw.Draw(sheet)
    for index, (label, values) in enumerate(cases):
        data = np.maximum(values, 0.0)
        positive = data[data > 0.0]
        white = float(np.percentile(positive, 99.9)) if positive.size else 1.0
        shown = np.log1p(data / max(white, 1e-30)) / math.log(2.0)
        gray = Image.fromarray(np.clip(shown * 255.0 + 0.5, 0, 255).astype(np.uint8))
        gray = gray.resize((tile, tile), Image.Resampling.NEAREST)
        left = index * tile
        sheet.paste(gray, (left, header))
        draw.text((left + 6, 8), label, fill=(235, 238, 244))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="source HDR PFM")
    parser.add_argument("geometric", type=Path, help="ghost render with FrFT off")
    parser.add_argument("frft", type=Path, help="matching ghost render with FrFT on")
    parser.add_argument("map", type=Path, help="matching optical-path-map CSV")
    parser.add_argument("--grid", type=int, required=True)
    parser.add_argument("--wavelength-nm", type=int, default=550)
    parser.add_argument("--pupil-resolution", type=int, choices=(128, 256, 512, 1024, 2048),
                        default=1024)
    parser.add_argument("--triangulation-method", choices=("delaunay", "launch-grid"),
                        default="launch-grid")
    parser.add_argument("--crop-radius", type=int, default=32)
    parser.add_argument("--pixel-subsamples", type=int, choices=(2, 4, 8), default=4)
    parser.add_argument("--csv", type=Path,
                        default=Path("tools/LensFlareLab/examples/ghost-frft-wavefront-compare.csv"))
    parser.add_argument("--image", type=Path,
                        default=Path("tools/LensFlareLab/examples/ghost-frft-wavefront-compare.png"))
    args = parser.parse_args()

    source, geometric, frft = map(read_pfm, (args.input, args.geometric, args.frft))
    if source.shape != geometric.shape or source.shape != frft.shape:
        parser.error("input and render PFM dimensions must match")
    height, width, _ = source.shape
    data = optics.read_map(args.map, args.grid, args.wavelength_nm)
    pupil_xy, pupil_center, pupil_z, pupil_rms = optics.fit_exit_pupil(data)
    opd, image_point, reference_radius, _ = optics.image_centered_opd(
        data, args.wavelength_nm, pupil_xy, pupil_center, pupil_z)
    regular_opd, pitch, amplitude = optics.map_to_regular_pupil(
        data, pupil_xy, opd, args.pupil_resolution, "linear", "triangle",
        args.triangulation_method)

    # Align the pixel-integrated reference with the nearest detector-pixel center.
    center_x = (float(image_point[0]) / optics.SENSOR_WIDTH_MM + 0.5) * width
    center_y = (0.5 - float(image_point[1]) / optics.SENSOR_WIDTH_MM) * width
    integer_x, integer_y = int(math.floor(center_x + 0.5)), int(math.floor(center_y + 0.5))
    offset_x, offset_y = integer_x - center_x, integer_y - center_y
    radius = args.crop_radius
    if (integer_x - radius < 0 or integer_x + radius >= width or
            integer_y - radius < 0 or integer_y + radius >= height):
        parser.error("the chief-ray-centered crop extends beyond the rendered image")
    phase_psf, flat_psf, sample_pitch, _, _, phase_fraction, flat_fraction = (
        optics.psf_and_pixel_integral(
            regular_opd, amplitude, args.wavelength_nm, pitch, reference_radius,
            radius, args.pixel_subsamples, offset_x, offset_y))
    phase_crop = normalized(phase_psf)
    flat_crop = normalized(flat_psf)

    y0, y1 = integer_y - radius, integer_y + radius + 1
    x0, x1 = integer_x - radius, integer_x + radius + 1
    source_green = source[y0:y1, x0:x1, 1]
    geometric_crop = np.maximum(geometric[y0:y1, x0:x1, 1] - source_green, 0.0)
    frft_crop = np.maximum(frft[y0:y1, x0:x1, 1] - source_green, 0.0)
    geometric_shape, frft_shape = normalized(geometric_crop), normalized(frft_crop)
    l1_geometric_phase = 100.0 * float(np.abs(geometric_shape - phase_crop).sum())
    l1_frft_phase = 100.0 * float(np.abs(frft_shape - phase_crop).sum())
    l1_geometric_flat = 100.0 * float(np.abs(geometric_shape - flat_crop).sum())
    l1_frft_flat = 100.0 * float(np.abs(frft_shape - flat_crop).sum())

    record = {
        "wavelength_nm": args.wavelength_nm, "ray_grid": args.grid,
        "pupil_resolution": args.pupil_resolution,
        "triangulation_method": args.triangulation_method,
        "detector_pixel_subsamples": args.pixel_subsamples,
        "chief_image_x_px": center_x, "chief_image_y_px": center_y,
        "detector_offset_x_px": offset_x, "detector_offset_y_px": offset_y,
        "exit_pupil_z_mm": pupil_z, "exit_pupil_rms_radius_mm": pupil_rms,
        "reference_sphere_radius_mm": reference_radius,
        "fraunhofer_native_bin_pitch_px": sample_pitch,
        "direct_psf_r50_px": optics.encircled_radius(phase_crop, 0.5),
        "direct_psf_r90_px": optics.encircled_radius(phase_crop, 0.9),
        "phase_crop_energy_fraction": phase_fraction,
        "flat_crop_energy_fraction": flat_fraction,
        "geometric_crop_flux_green": float(geometric_crop.sum()),
        "frft_crop_flux_green": float(frft_crop.sum()),
        "geometric_centroid_dx_px": centroid(geometric_shape)[0],
        "geometric_centroid_dy_px": centroid(geometric_shape)[1],
        "frft_centroid_dx_px": centroid(frft_shape)[0],
        "frft_centroid_dy_px": centroid(frft_shape)[1],
        "geometric_r50_px": optics.encircled_radius(geometric_shape, 0.5),
        "frft_r50_px": optics.encircled_radius(frft_shape, 0.5),
        "geometric_l1_vs_phase_psf_percent": l1_geometric_phase,
        "frft_l1_vs_phase_psf_percent": l1_frft_phase,
        "geometric_l1_vs_flat_psf_percent": l1_geometric_flat,
        "frft_l1_vs_flat_psf_percent": l1_frft_flat,
    }
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(record))
        writer.writeheader()
        writer.writerow(record)
    make_chart([
        ("Fraunhofer pupil OPL", phase_crop),
        ("Same pupil, flat phase", flat_crop),
        ("Geometric render, green channel", geometric_shape),
        ("FrFT render, green channel", frft_shape),
        ("|Geometric − OPL PSF|", np.abs(geometric_shape - phase_crop)),
        ("|FrFT − OPL PSF|", np.abs(frft_shape - phase_crop)),
    ], args.image)
    print(f"{args.wavelength_nm} nm reference center=({center_x:.3f},{center_y:.3f}) px; "
          f"native-bin pitch={sample_pitch:.3f} px; exit-pupil rms={pupil_rms*1000:.2f} µm")
    print(f"phase/flat PSF L1={100*np.abs(phase_crop-flat_crop).sum():.2f}%; "
          f"crop fractions phase/flat={phase_fraction:.5f}/{flat_fraction:.5f}")
    print(f"geometric vs phase/flat PSF L1={l1_geometric_phase:.2f}%/{l1_geometric_flat:.2f}%; "
          f"FrFT vs phase/flat PSF L1={l1_frft_phase:.2f}%/{l1_frft_flat:.2f}%")
    print(f"saved {args.csv} and {args.image}")


if __name__ == "__main__":
    main()
