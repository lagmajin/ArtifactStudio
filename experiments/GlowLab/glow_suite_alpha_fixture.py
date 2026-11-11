"""Derive a transparent neon plate with smooth, brightness-linked alpha."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import cv2
import numpy as np


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--alpha-threshold", type=float, default=0.28)
    parser.add_argument("--feather-radius", type=float, default=3.0)
    args = parser.parse_args()
    source_path, output_dir = args.input.resolve(), args.output_dir.resolve()
    if not 0.0 <= args.alpha_threshold <= 1.0 or args.feather_radius <= 0.0:
        raise ValueError("require 0 <= alpha-threshold <= 1 and feather-radius > 0")
    image = cv2.imread(str(source_path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise RuntimeError(f"could not load source image: {source_path}")
    if image.ndim != 3 or image.shape[2] not in (3, 4):
        raise RuntimeError("source must be an RGB or RGBA PNG")
    if image.shape[2] == 4:
        bgr = image[..., :3]
    else:
        bgr = image
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    rgb_float = rgb.astype(np.float32) / 255.0
    brightness = np.max(rgb_float, axis=2)
    foreground = (brightness >= args.alpha_threshold).astype(np.float32)
    alpha_float = cv2.GaussianBlur(foreground, (0, 0), args.feather_radius,
                                   borderType=cv2.BORDER_CONSTANT)
    alpha = np.rint(np.clip(alpha_float, 0.0, 1.0) * 255.0).astype(np.uint8)
    rgba = np.dstack((rgb, alpha))

    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / "transparent-neon-smooth-alpha.png"
    if not cv2.imwrite(str(output_path), cv2.cvtColor(rgba, cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"could not write {output_path}")
    metadata = {
        "purpose": "Glow suite alpha-boundary product capture fixture",
        "source": str(source_path),
        "source_sha256": digest(source_path),
        "output": output_path.name,
        "output_sha256": digest(output_path),
        "dimensions": [int(rgba.shape[1]), int(rgba.shape[0])],
        "alpha_generation": "Gaussian feather of max(encoded RGB) threshold mask",
        "alpha_threshold": args.alpha_threshold,
        "feather_radius_sigma_px": args.feather_radius,
        "alpha_histogram_bins_16": np.histogram(alpha, bins=16, range=(0, 256))[0].tolist(),
        "transparent_pixel_fraction": float(np.mean(alpha == 0)),
        "partial_alpha_fraction": float(np.mean((alpha > 0) & (alpha < 255))),
        "opaque_pixel_fraction": float(np.mean(alpha == 255)),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
    }
    (output_dir / "fixture-metadata.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"Saved {output_path}")


if __name__ == "__main__":
    main()
