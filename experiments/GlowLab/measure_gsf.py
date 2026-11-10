"""Extract a radial glare-spread profile from a point-source camera capture."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np


def to_linear_luminance(image: np.ndarray) -> np.ndarray:
    if image.ndim == 2:
        rgb = np.repeat(image[..., None], 3, axis=2)
    elif image.shape[2] >= 3:
        rgb = cv2.cvtColor(image[..., :3], cv2.COLOR_BGR2RGB)
    else:
        raise ValueError(f"Unsupported image layout: {image.shape}")
    if np.issubdtype(image.dtype, np.integer):
        maximum = float(np.iinfo(image.dtype).max)
        rgb = rgb.astype(np.float32) / maximum
        rgb = np.where(rgb <= 0.04045, rgb / 12.92,
                       ((rgb + 0.055) / 1.055) ** 2.4)
    else:
        # Float captures are treated as linear sensor/image values.
        rgb = rgb.astype(np.float32)
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def estimate_center(luminance: np.ndarray) -> tuple[float, float]:
    h, w = luminance.shape
    border = np.concatenate((luminance[0, :], luminance[-1, :],
                             luminance[:, 0], luminance[:, -1]))
    background = float(np.median(border))
    signal = np.maximum(luminance - background, 0.0)
    threshold = float(np.quantile(signal, 0.999))
    weights = np.where(signal >= threshold, signal, 0.0)
    total = float(weights.sum())
    if total <= 1.0e-12:
        raise ValueError("No bright point source found; supply a capture with a clear bright source")
    y, x = np.mgrid[0:h, 0:w]
    return float((x * weights).sum() / total), float((y * weights).sum() / total)


def radial_profile(luminance: np.ndarray, center: tuple[float, float] | None = None
                   ) -> tuple[np.ndarray, np.ndarray, tuple[float, float]]:
    h, w = luminance.shape
    cx, cy = center if center is not None else estimate_center(luminance)
    max_radius = min(cx, cy, (w - 1) - cx, (h - 1) - cy)
    if max_radius < 2:
        raise ValueError("Bright source must be far enough from the image edges")
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    radius = np.hypot(x - cx, y - cy)
    bins = np.floor(radius).astype(np.int32)
    valid = bins <= int(max_radius)
    background = float(np.median(np.concatenate((luminance[0, :], luminance[-1, :],
                                                  luminance[:, 0], luminance[:, -1]))))
    signal = np.maximum(luminance - background, 0.0)
    sums = np.bincount(bins[valid], weights=signal[valid], minlength=int(max_radius) + 1)
    counts = np.bincount(bins[valid], minlength=int(max_radius) + 1)
    means = sums / np.maximum(counts, 1)
    peak = float(means.max())
    if peak <= 1.0e-12:
        raise ValueError("No measurable source energy after background subtraction")
    return np.arange(len(means), dtype=np.float32), (means / peak).astype(np.float32), (cx, cy)


def write_plot(path: Path, radii: np.ndarray, profile: np.ndarray) -> None:
    width, height = 1000, 560
    left, right, top, bottom = 90, width - 32, 32, height - 68
    chart = np.full((height, width, 3), (18, 20, 26), dtype=np.uint8)
    cv2.rectangle(chart, (left, top), (right, bottom), (55, 58, 66), 1)
    cv2.putText(chart, "Measured glare spread profile (radial mean, peak-normalized)",
                (left, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.58, (225, 230, 238), 1, cv2.LINE_AA)
    max_radius = max(float(radii[-1]), 1.0)
    floor = 1.0e-6
    points = []
    for radius, value in zip(radii, profile):
        px = int(left + (right - left) * float(radius) / max_radius)
        log_value = np.log10(max(float(value), floor))
        py = int(bottom - (bottom - top) * (log_value + 6.0) / 6.0)
        points.append((px, int(np.clip(py, top, bottom))))
    if len(points) > 1:
        cv2.polylines(chart, [np.asarray(points, dtype=np.int32)], False,
                      (80, 210, 255), 2, cv2.LINE_AA)
    for decade in range(7):
        y = int(bottom - (bottom - top) * decade / 6.0)
        cv2.line(chart, (left, y), (right, y), (39, 42, 48), 1)
        cv2.putText(chart, f"1e-{6 - decade}", (20, y + 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.42, (165, 170, 180), 1, cv2.LINE_AA)
    cv2.putText(chart, "radius (pixels)", (width // 2 - 60, height - 18),
                cv2.FONT_HERSHEY_SIMPLEX, 0.48, (190, 196, 206), 1, cv2.LINE_AA)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(path), chart):
        raise OSError(f"Could not write profile plot: {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="camera capture of a bright point on a dark field")
    parser.add_argument("--csv", type=Path, default=Path("gsf-profile.csv"),
                        help="output CSV path")
    parser.add_argument("--plot", type=Path, default=Path("gsf-profile.png"),
                        help="output log-scale radial profile plot")
    parser.add_argument("--center", nargs=2, type=float, metavar=("X", "Y"),
                        help="optional source center in pixel coordinates")
    args = parser.parse_args()
    image = cv2.imread(str(args.image), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise ValueError(f"Could not read image: {args.image}")
    luminance = to_linear_luminance(image)
    center = tuple(args.center) if args.center else None
    radii, profile, measured_center = radial_profile(luminance, center)
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(f"# width={image.shape[1]} height={image.shape[0]} "
                     f"center_x={measured_center[0]:.6f} center_y={measured_center[1]:.6f}\n")
        np.savetxt(stream, np.column_stack((radii, profile)), delimiter=",",
                   header="radius_px,relative_linear_luminance", comments="")
    write_plot(args.plot, radii, profile)
    print(f"Source center: ({measured_center[0]:.2f}, {measured_center[1]:.2f}) px")
    print(f"Saved radial GSF profile: {args.csv.resolve()}")
    print(f"Saved profile plot: {args.plot.resolve()}")


if __name__ == "__main__":
    main()
