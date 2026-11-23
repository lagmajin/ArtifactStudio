"""Create a deterministic linear-exposure variant for Glow suite stress captures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def srgb_to_linear(value: np.ndarray) -> np.ndarray:
    return np.where(value <= 0.04045, value / 12.92,
                    ((value + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(value: np.ndarray) -> np.ndarray:
    return np.where(value <= 0.0031308, value * 12.92,
                    1.055 * np.power(np.maximum(value, 0.0), 1.0 / 2.4) - 0.055)


def run(input_path: Path, output_path: Path, exposure_stops: float) -> None:
    if not np.isfinite(exposure_stops):
        raise ValueError("exposure stops must be finite")
    raw = cv2.imread(str(input_path), cv2.IMREAD_UNCHANGED)
    if raw is None:
        raise RuntimeError(f"Could not read input: {input_path}")
    if raw.ndim == 2:
        rgb = cv2.cvtColor(raw, cv2.COLOR_GRAY2RGB)
        alpha = np.full(raw.shape, 255, dtype=np.uint8)
    elif raw.shape[2] == 4:
        rgba = cv2.cvtColor(raw, cv2.COLOR_BGRA2RGBA)
        rgb, alpha = rgba[..., :3], rgba[..., 3]
    else:
        rgb = cv2.cvtColor(raw, cv2.COLOR_BGR2RGB)
        alpha = np.full(raw.shape[:2], 255, dtype=np.uint8)

    source = rgb.astype(np.float32) / 255.0
    source_linear = srgb_to_linear(source)
    exposed_linear = source_linear * float(2.0 ** exposure_stops)
    clipped_linear = np.clip(exposed_linear, 0.0, 1.0)
    exposed_srgb = np.clip(linear_to_srgb(clipped_linear), 0.0, 1.0)
    result_rgb = np.rint(exposed_srgb * 255.0).astype(np.uint8)
    out = cv2.cvtColor(np.dstack((result_rgb, alpha)), cv2.COLOR_RGBA2BGRA)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(output_path), out):
        raise RuntimeError(f"Could not write output: {output_path}")
    result_linear = srgb_to_linear(result_rgb.astype(np.float32) / 255.0)
    source_clip = (source >= 254.0 / 255.0)
    output_clip = (result_rgb >= 254)
    source_luma = source_linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
    output_luma = result_linear @ np.asarray((0.2126, 0.7152, 0.0722), dtype=np.float32)
    report = {
        "model": "deterministic linear-light exposure scaling of an existing portrait still",
        "not_a_new_photograph": True,
        "input": str(input_path),
        "input_sha256": digest(input_path),
        "output": str(output_path),
        "output_sha256": digest(output_path),
        "dimensions": [int(rgb.shape[1]), int(rgb.shape[0])],
        "exposure_stops": float(exposure_stops),
        "linear_gain": float(2.0 ** exposure_stops),
        "source_clipped_channel_fraction_ge_254": float(source_clip.mean()),
        "output_clipped_channel_fraction_ge_254": float(output_clip.mean()),
        "output_pixels_clipped_by_linear_exposure": int(np.count_nonzero(exposed_linear > 1.0)),
        "source_linear_luma_mean": float(source_luma.mean()),
        "output_linear_luma_mean": float(output_luma.mean()),
        "limitations": [
            "8-bit SDR sRGB output clamps linear values above 1.0",
            "does not model camera exposure, sensor noise, or HDR display mapping",
        ],
    }
    # Compare the written RGBA alpha explicitly after re-reading for the evidence record.
    written = cv2.imread(str(output_path), cv2.IMREAD_UNCHANGED)
    report["alpha_preserved_exactly"] = bool(
        written is not None and written.ndim == 3 and written.shape[2] == 4 and
        np.array_equal(alpha, written[..., 3]))
    sidecar = Path(str(output_path) + ".json")
    sidecar.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {output_path}")
    print(f"Saved {sidecar}")
    print(f"linear gain={report['linear_gain']:.3f}, "
          f"output clipped channels={report['output_clipped_channel_fraction_ge_254']:.3%}, "
          f"alpha preserved={report['alpha_preserved_exactly']}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stops", type=float, default=1.0)
    args = parser.parse_args()
    run(args.input, args.output, args.stops)


if __name__ == "__main__":
    main()
