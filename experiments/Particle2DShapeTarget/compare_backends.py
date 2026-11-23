#!/usr/bin/env python3
"""Compare deterministic 2D particle target captures and refresh image stats."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


BACKENDS = ("vulkan", "d3d12")
MODES = ("baseline_disabled_default", "shape_ring", "text_A8")
FRAMES = ("30", "60", "90", "180", "240")
STAT_FRAMES = ("90", "240")


def read_report(root: Path, backend: str) -> dict:
    path = root / backend / "shape_target_report.json"
    report = json.loads(path.read_text(encoding="utf-8"))
    if report.get("backend", "").lower() != backend:
        raise ValueError(f"Expected backend {backend!r} in {path}")
    return report


def read_rgba(root: Path, backend: str, name: str) -> np.ndarray:
    image = np.asarray(Image.open(root / backend / name).convert("RGBA"), dtype=np.int16)
    if image.shape != (180, 320, 4):
        raise ValueError(f"Unexpected image shape {image.shape} for {backend}/{name}")
    return image


def difference_stats(left: np.ndarray, right: np.ndarray) -> dict:
    delta = np.abs(left - right)
    per_pixel_max = delta.max(axis=2)
    changed = per_pixel_max[per_pixel_max > 0]
    alpha = delta[:, :, 3]
    changed_alpha = alpha[alpha > 0]
    return {
        "differing_pixels": int(changed.size),
        "differing_pixel_percent": round(100.0 * changed.size / per_pixel_max.size, 5),
        "max_channel_delta": int(delta.max()),
        "full_frame_p95_per_pixel_max_delta": round(float(np.percentile(per_pixel_max, 95)), 3),
        "changed_pixel_p95_max_delta": round(float(np.percentile(changed, 95)), 3) if changed.size else 0,
        "alpha_differing_pixels": int(changed_alpha.size),
        "alpha_max_delta": int(alpha.max()),
        "alpha_full_frame_p95": round(float(np.percentile(alpha, 95)), 3),
        "alpha_changed_p95": round(float(np.percentile(changed_alpha, 95)), 3) if changed_alpha.size else 0,
    }


def refresh_vulkan_statistics(root: Path, report: dict) -> None:
    path = root / "vulkan" / "image_statistics.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    for mode in MODES:
        for frame in STAT_FRAMES:
            entry = data["images"][mode][frame]
            rgba = np.asarray(
                Image.open(root / "vulkan" / entry["file"]).convert("RGBA"), dtype=np.uint8
            )
            rgb = rgba[:, :, :3].astype(np.float64)
            alpha = rgba[:, :, 3].astype(np.float64)
            entry["size"] = [int(rgba.shape[1]), int(rgba.shape[0])]
            entry["rgb_min_max"] = [rgb.min((0, 1)).tolist(), rgb.max((0, 1)).tolist()]
            entry["rgb_mean"] = np.round(rgb.mean((0, 1)), 4).tolist()
            entry["rgb_p50_p95_p99"] = np.round(
                np.percentile(rgb, [50, 95, 99], axis=(0, 1)), 2
            ).tolist()
            entry["alpha_min_max"] = [int(alpha.min()), int(alpha.max())]
            entry["alpha_mean"] = round(float(alpha.mean()), 4)
            entry["alpha_p50_p95_p99"] = np.round(
                np.percentile(alpha, [50, 95, 99]), 2
            ).tolist()
            entry["alpha_counts"] = {
                "zero": int(np.count_nonzero(alpha == 0)),
                "partial_1_254": int(np.count_nonzero((alpha > 0) & (alpha < 255))),
                "full_255": int(np.count_nonzero(alpha == 255)),
            }
            entry["alpha_bands_0_15_16_63_64_127_128_254_255"] = [
                int(np.count_nonzero(alpha <= 15)),
                int(np.count_nonzero((alpha >= 16) & (alpha <= 63))),
                int(np.count_nonzero((alpha >= 64) & (alpha <= 127))),
                int(np.count_nonzero((alpha >= 128) & (alpha <= 254))),
                int(np.count_nonzero(alpha == 255)),
            ]
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("output/particle-shape-target"))
    args = parser.parse_args()
    root = args.root.resolve()
    reports = {backend: read_report(root, backend) for backend in BACKENDS}
    if reports["vulkan"].get("seed") != reports["d3d12"].get("seed"):
        raise ValueError("Backend captures use different simulation seeds")
    vulkan_ids = reports["vulkan"].get("fixture_layer_ids")
    if not vulkan_ids or vulkan_ids != reports["d3d12"].get("fixture_layer_ids"):
        raise ValueError("Backend captures use different or missing fixture LayerIDs")

    result = {
        "resolution": [320, 180],
        "format": "8-bit RGBA",
        "seed": reports["vulkan"]["seed"],
        "fixture_layer_ids": vulkan_ids,
        "same_seed_and_fixed_layer_ids": True,
        "modes": {},
        "performance": {},
    }
    for mode in MODES:
        result["modes"][mode] = {}
        for frame in FRAMES:
            images = [
                read_rgba(root, backend, reports[backend]["captures"][mode][frame]["image"])
                for backend in BACKENDS
            ]
            result["modes"][mode][frame] = difference_stats(*images)

        direct_images = [
            read_rgba(
                root,
                backend,
                reports[backend]["captures"][mode]["direct_seek_image"],
            )
            for backend in BACKENDS
        ]
        result["modes"][mode]["direct_seek_frame_60"] = difference_stats(*direct_images)

        for frame in STAT_FRAMES:
            centers = [
                reports[backend]["captures"][mode][frame]["particle_centers"]
                for backend in BACKENDS
            ]
            xy = [
                np.asarray([[item["x"], item["y"]] for item in values], dtype=np.float64)
                for values in centers
            ]
            common = min(len(xy[0]), len(xy[1]))
            deltas = np.linalg.norm(xy[0][:common] - xy[1][:common], axis=1)
            result["modes"][mode].setdefault("particle_position_parity", {})[frame] = {
                "count_vulkan": len(xy[0]),
                "count_d3d12": len(xy[1]),
                "mismatches_over_1e-5_px": int(np.count_nonzero(deltas > 1e-5)),
                "mean_delta_px": round(float(deltas.mean()), 7) if common else None,
                "p95_delta_px": round(float(np.percentile(deltas, 95)), 7) if common else None,
                "max_delta_px": round(float(deltas.max()), 7) if common else None,
            }

        result["performance"][mode] = {
            backend: reports[backend]["captures"][mode]["performance"]
            for backend in BACKENDS
        }

    (root / "backend_comparison.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    refresh_vulkan_statistics(root, reports["vulkan"])
    for mode in MODES:
        for frame in ("90", "240"):
            delta = result["modes"][mode][frame]
            position = result["modes"][mode]["particle_position_parity"][frame]
            print(
                f"{mode} frame {frame}: pixel diff={delta['differing_pixels']}, "
                f"max={delta['max_channel_delta']}; particle max="
                f"{position['max_delta_px']} px"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
