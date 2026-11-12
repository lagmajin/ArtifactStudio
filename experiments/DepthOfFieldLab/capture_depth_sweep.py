#!/usr/bin/env python3
"""Create an offline, image-space DoF focus sweep for R&D scene review.

This is a visual reference prototype. It does not execute or represent
ArtifactStudio's GPU DepthOfFieldPass.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import cv2
import numpy as np


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_IMAGE = ROOT / "experiments/GlowLab/assets/flash-photo-plate.png"
DEFAULT_DEPTH = ROOT / "experiments/GlowLab/captures/depth-anything-portrait-rd1/depth-relative.npy"
DEFAULT_OUTPUT = ROOT / "experiments/DepthOfFieldLab/captures/depth-sweep-rd1"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_inputs(image_path: Path, depth_path: Path) -> tuple[np.ndarray, np.ndarray, dict]:
    image = cv2.imread(str(image_path), cv2.IMREAD_COLOR)
    if image is None:
        raise RuntimeError(f"Could not read source image: {image_path}")
    depth = np.load(depth_path).astype(np.float32, copy=False)
    if depth.ndim != 2 or depth.shape != image.shape[:2]:
        raise RuntimeError(
            f"Image/depth dimensions differ: image={image.shape[:2]}, depth={depth.shape}"
        )
    finite = np.isfinite(depth)
    if not np.all(finite):
        replacement = float(np.nanmedian(depth[finite])) if np.any(finite) else 0.0
        depth = np.nan_to_num(depth, nan=replacement, posinf=replacement, neginf=replacement)
    low, high = np.percentile(depth, [2.0, 98.0]).astype(float)
    if high <= low:
        raise RuntimeError("Relative depth map has no usable range")
    normalized = np.clip((depth - low) / (high - low), 0.0, 1.0)
    metadata = {
        "image_size": {"width": int(image.shape[1]), "height": int(image.shape[0])},
        "depth_normalization_percentiles": [2.0, 98.0],
        "depth_direction": "higher=near",
        "depth_percentiles_raw": {
            str(p): float(v)
            for p, v in zip([0, 10, 25, 50, 75, 90, 100],
                            np.percentile(depth, [0, 10, 25, 50, 75, 90, 100]))
        },
    }
    return image, normalized, metadata


def defocus(image: np.ndarray, depth: np.ndarray, focus: float,
            max_radius: float, levels: int = 8) -> np.ndarray:
    """Blend an interpolated Gaussian blur pyramid from relative CoC radius."""
    coc = np.clip(np.abs(depth - focus) * max_radius, 0.0, max_radius)
    position = coc * (levels - 1) / max(max_radius, 1e-6)
    lower = np.floor(position).astype(np.int32)
    upper = np.minimum(lower + 1, levels - 1)
    fraction = (position - lower).astype(np.float32)[..., None]

    sigmas = np.linspace(0.0, max_radius * 0.70, levels)
    low_image = np.empty_like(image)
    high_image = np.empty_like(image)
    for level, sigma in enumerate(sigmas):
        if level == 0:
            blurred = image
        else:
            blurred = cv2.GaussianBlur(
                image, (0, 0), sigmaX=float(sigma), sigmaY=float(sigma),
                borderType=cv2.BORDER_REFLECT101,
            )
        mask_low = lower == level
        if np.any(mask_low):
            low_image[mask_low] = blurred[mask_low]
        mask_high = upper == level
        if np.any(mask_high):
            high_image[mask_high] = blurred[mask_high]
    result = low_image.astype(np.float32) * (1.0 - fraction)
    result += high_image.astype(np.float32) * fraction
    return np.clip(result + 0.5, 0, 255).astype(np.uint8)


def label_panel(image: np.ndarray, title: str, subtitle: str) -> np.ndarray:
    panel = image.copy()
    bar_h = 66
    cv2.rectangle(panel, (0, 0), (panel.shape[1], bar_h), (18, 21, 28), -1)
    cv2.putText(panel, title, (24, 29), cv2.FONT_HERSHEY_SIMPLEX, 0.72,
                (245, 247, 250), 2, cv2.LINE_AA)
    cv2.putText(panel, subtitle, (24, 53), cv2.FONT_HERSHEY_SIMPLEX, 0.40,
                (183, 193, 208), 1, cv2.LINE_AA)
    return panel


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--depth", type=Path, default=DEFAULT_DEPTH)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--max-radius", type=float, default=18.0,
                        help="Maximum illustrative Gaussian blur radius in source pixels")
    args = parser.parse_args()
    if args.max_radius <= 0:
        raise SystemExit("--max-radius must be positive")

    image_path = args.image.resolve()
    depth_path = args.depth.resolve()
    output_dir = args.output.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    image, depth, metadata = load_inputs(image_path, depth_path)

    # Focus values use the normalized relative-depth interval. The selected
    # points target distant street, the person/car mid-plane, and near wall.
    focus_sweep = [
        ("far-focus", 0.16, "Focus: distant street"),
        ("mid-focus", 0.52, "Focus: subject face plane"),
        ("near-focus", 0.88, "Focus: foreground wall"),
    ]
    panel_size = (960, 640)
    panels = [label_panel(cv2.resize(image, panel_size), "SOURCE", "No defocus")]
    variants = []
    for name, focus, subtitle in focus_sweep:
        result = defocus(image, depth, focus, args.max_radius)
        filename = f"{name}.png"
        cv2.imwrite(str(output_dir / filename), result,
                    [cv2.IMWRITE_PNG_COMPRESSION, 3])
        panels.append(label_panel(cv2.resize(result, panel_size), name.upper(),
                                  f"{subtitle} | relative depth={focus:.2f}"))
        variants.append({
            "name": name,
            "focus_relative_depth": focus,
            "image": filename,
            "max_coc_radius_px": args.max_radius,
        })

    sheet = np.vstack([
        np.hstack(panels[:2]),
        np.hstack(panels[2:]),
    ])
    contact_path = output_dir / "dof-focus-sweep.png"
    cv2.imwrite(str(contact_path), sheet, [cv2.IMWRITE_PNG_COMPRESSION, 3])
    metadata.update({
        "capture_kind": "offline image-space R&D reference; not Artifact renderer output",
        "algorithm": "relative-depth CoC mapped to an 8-level Gaussian blur pyramid",
        "algorithm_limitations": [
            "monocular depth is relative rather than metric",
            "Gaussian blur does not model aperture-shaped bokeh",
            "blurred color may cross depth discontinuities; this is diagnostic, not a quality claim",
            "does not execute ArtifactStudio's GPU DepthOfFieldPass",
        ],
        "source_image": {
            "path": str(image_path.relative_to(ROOT)),
            "sha256": sha256(image_path),
        },
        "source_depth": {
            "path": str(depth_path.relative_to(ROOT)),
            "sha256": sha256(depth_path),
            "model": "Depth Anything V2 Small, source metadata in depth-anything-portrait-rd1",
        },
        "variants": variants,
        "contact_sheet": contact_path.name,
    })
    (output_dir / "capture-metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(contact_path)
    for variant in variants:
        print(output_dir / variant["image"])


if __name__ == "__main__":
    main()
