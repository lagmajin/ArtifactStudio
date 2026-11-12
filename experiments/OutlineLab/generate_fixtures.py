#!/usr/bin/env python3
"""Generate deterministic outline R&D fixtures.

This script creates synthetic RGBA and depth fixtures for Phase 1 of the
Outline Lab. It does not read or modify ArtifactStudio product data.

Outputs:
- assets/generated/alpha-shapes-2d-rd1/{straight,premultiplied}/
- assets/generated/depth-planes-3d-rd1/straight/
- assets/generated/index.json

PNG is used for review convenience. Raw float32 .npy files are the measurement
inputs; see fixture_manifest.json for the color and depth contracts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image, ImageDraw

from outline_metrics import boundary_ring


ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets" / "generated"
SEED = 20261112
WIDTH = 512
HEIGHT = 512

# Rectangle regions of the synthetic depth field, as fractions of the frame.
DEPTH_RECTS = (
    (0.48, 0.72, 0.18, 0.78),
    (0.12, 0.34, 0.56, 0.92),
)


def srgb_from_linear(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, x * 12.92, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)


def write_png(path: Path, rgba: np.ndarray, alpha_mode: str) -> None:
    """Write an 8-bit PNG preview without changing the measurement array.

    Straight RGB is display-encoded. Premultiplied RGB is temporarily
    unpremultiplied for preview only; the .npy file remains premultiplied.
    """
    rgb = rgba[..., :3].astype(np.float32)
    alpha = rgba[..., 3].astype(np.float32)
    if alpha_mode == "premultiplied":
        safe_alpha = np.maximum(alpha, 1e-6)
        rgb = np.where(alpha[..., None] > 1e-6, rgb / safe_alpha[..., None], 0.0)
    rgb = srgb_from_linear(rgb)
    rgba8 = np.empty((rgba.shape[0], rgba.shape[1], 4), dtype=np.uint8)
    rgba8[..., :3] = np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8)
    rgba8[..., 3] = np.clip(alpha * 255.0 + 0.5, 0, 255).astype(np.uint8)
    Image.fromarray(rgba8, mode="RGBA").save(path)


def shape_alpha_mask(width: int, height: int) -> np.ndarray:
    # Draw at 4x and box-downsample for deterministic antialiasing.
    scale = 4
    big = Image.new("L", (width * scale, height * scale), 0)
    d = ImageDraw.Draw(big)
    s = scale
    d.ellipse((40 * s, 48 * s, 210 * s, 218 * s), fill=255)
    d.rectangle((230 * s, 70 * s, 410 * s, 230 * s), fill=255)
    d.ellipse((300 * s, 210 * s, 470 * s, 380 * s), fill=255)
    star = []
    cx, cy, outer, inner = 150.0, 360.0, 90.0, 38.0
    for i in range(10):
        r = outer if i % 2 == 0 else inner
        a = -math.pi / 2 + i * math.pi / 5
        star.append(((cx + r * math.cos(a)) * s, (cy + r * math.sin(a)) * s))
    d.polygon(star, fill=255)
    d.line((30 * s, 300 * s, 480 * s, 300 * s), fill=180, width=3 * s)
    d.line((30 * s, 440 * s, 480 * s, 440 * s), fill=120, width=1 * s)
    return np.asarray(big.resize((width, height), Image.Resampling.BOX), dtype=np.float32) / 255.0


def depth_mask(width: int, height: int) -> np.ndarray:
    yy, xx = np.mgrid[0:height, 0:width].astype(np.float32)
    depth = 0.65 + 0.15 * np.sin(xx / width * np.pi * 2.0)
    # Distinct plateaus give one clearly measurable discontinuity per rectangle.
    plateau = (0.22, 0.95)
    for (y0, y1, x0, x1), value in zip(DEPTH_RECTS, plateau):
        depth[int(height * y0):int(height * y1), int(width * x0):int(width * x1)] = value
    return depth


def depth_truth_boundary(width: int, height: int) -> np.ndarray:
    """Truth ring from the rectangle geometry, not from the rendered field."""
    truth = np.zeros((height, width), dtype=bool)
    for y0, y1, x0, x1 in DEPTH_RECTS:
        rect = np.zeros((height, width), dtype=bool)
        rect[int(height * y0):int(height * y1), int(width * x0):int(width * x1)] = True
        truth |= boundary_ring(rect)
    return truth


def build_rgba(alpha: np.ndarray, seed: int) -> tuple[np.ndarray, np.ndarray]:
    rng = np.random.default_rng(seed)
    h, w = alpha.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    base = np.empty((h, w, 3), dtype=np.float32)
    base[..., 0] = 0.18 + 0.22 * (xx / max(w - 1, 1))
    base[..., 1] = 0.24 + 0.18 * (yy / max(h - 1, 1))
    base[..., 2] = 0.42
    # Small deterministic color variation makes flat interior false edges visible.
    base += (rng.random((h, w, 1), dtype=np.float32) - 0.5) * 0.015
    base = np.clip(base, 0.0, 1.0)
    # Include an alpha-zero RGB stress color. Straight output preserves it as
    # hidden RGB; premultiplied output must zero it.
    hidden = np.array([0.9, 0.05, 0.8], dtype=np.float32)
    rgb = np.where(alpha[..., None] > 0.001, base, hidden)
    return rgb, alpha


def premultiply(rgb: np.ndarray, alpha: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    return rgb * alpha[..., None], alpha


def save_case(
    out_dir: Path,
    name: str,
    rgb: np.ndarray,
    alpha: np.ndarray,
    depth: np.ndarray | None,
    alpha_mode: str,
    depth_truth: np.ndarray | None = None,
) -> dict[str, Any]:
    out_dir.mkdir(parents=True, exist_ok=True)
    rgba = np.concatenate([rgb, alpha[..., None]], axis=-1).astype(np.float32)
    np.save(out_dir / f"{name}.npy", rgba)
    if depth is not None:
        np.save(out_dir / f"{name}.depth.npy", depth.astype(np.float32))
    alpha_truth = boundary_ring(alpha >= 0.5)
    np.save(out_dir / f"{name}.truth-alpha.npy", alpha_truth)
    entry: dict[str, Any] = {
        "name": name,
        "width": int(rgba.shape[1]),
        "height": int(rgba.shape[0]),
        "alpha_mode": alpha_mode,
        "has_depth": depth is not None,
        "sha256": hashlib.sha256(rgba.tobytes()).hexdigest(),
        "preview": str((out_dir / f"{name}.preview.png").relative_to(ROOT)),
        "truth_alpha": str((out_dir / f"{name}.truth-alpha.npy").relative_to(ROOT)),
        "truth_depth": None,
    }
    if depth is not None and depth_truth is not None:
        np.save(out_dir / f"{name}.truth-depth.npy", depth_truth)
        entry["truth_depth"] = str((out_dir / f"{name}.truth-depth.npy").relative_to(ROOT))
    write_png(out_dir / f"{name}.preview.png", rgba, alpha_mode)
    return entry


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()

    alpha = shape_alpha_mask(WIDTH, HEIGHT)
    rgb, _ = build_rgba(alpha, SEED)
    depth = depth_mask(WIDTH, HEIGHT)
    entries: list[dict[str, Any]] = []

    for mode in ("straight", "premultiplied"):
        out_dir = ASSETS / "alpha-shapes-2d-rd1" / mode
        if mode == "straight":
            entries.append(save_case(out_dir, "alpha-shapes", rgb, alpha, None, mode))
        else:
            prgb, palpha = premultiply(rgb, alpha)
            entries.append(save_case(out_dir, "alpha-shapes", prgb, palpha, None, mode))

    # The depth fixture keeps alpha fully opaque so the alpha boundary term is
    # constant. Only depth discontinuity can produce a line there, which keeps
    # the two feature sources separable in Phase 1.
    opaque = np.ones_like(alpha, dtype=np.float32)
    depth_rgb, _ = build_rgba(opaque, SEED + 1)
    out_dir = ASSETS / "depth-planes-3d-rd1" / "straight"
    depth_truth = depth_truth_boundary(WIDTH, HEIGHT)
    entries.append(
        save_case(out_dir, "depth-planes", depth_rgb, opaque, depth, "straight", depth_truth)
    )

    index = {
        "lab": "OutlineLab",
        "created": "2026-11-12",
        "seed": SEED,
        "width": WIDTH,
        "height": HEIGHT,
        "fixtures": entries,
        "notes": [
            "PNG previews are review-only; .npy files are the measurement inputs.",
            "Straight fixtures preserve hidden RGB where alpha is zero.",
            "Premultiplied fixtures store RGB multiplied by alpha.",
            "Depth is a synthetic normalized field and is not metric depth.",
            "truth-alpha is a boundary ring of alpha>=0.5; truth-depth comes from the rectangle geometry.",
        ],
    }
    (ASSETS / "index.json").write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(entries)} fixtures to {ASSETS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())