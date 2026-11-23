"""Build a deterministic textured emissive plate for Glow-family role captures."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np

from glow_lab import make_fixture, to_u8


def make_material_fixture() -> np.ndarray:
    """Add dim wall and wet-ground texture without baking in visible bloom."""
    rgba = to_u8(make_fixture()).astype(np.int16)
    height, width = rgba.shape[:2]
    yy, xx = np.mgrid[0:height, 0:width]
    rng = np.random.default_rng(20261111)

    # Quiet brick texture occupies the left field, below the glow threshold.
    wall = np.zeros((height, width, 3), dtype=np.int16)
    wall[:] = (13, 10, 17)
    wall_noise = rng.integers(-2, 3, size=(height, width, 1), dtype=np.int16)
    wall += wall_noise
    brick_x = xx % 56
    brick_y = (yy + ((xx // 56) % 2) * 16) % 32
    mortar = (brick_x < 2) | (brick_y < 2)
    wall[mortar] += 5
    wall_zone = (xx < 330) & (yy < 360)

    # Wet pavement uses smooth, warped horizontal ridges and a dim blue gradient.
    road = np.zeros((height, width, 3), dtype=np.int16)
    road[:] = (11, 13, 22)
    road_gradient = np.clip((yy - 330) / 210.0, 0.0, 1.0)
    road += np.round(road_gradient[..., None] * np.asarray((9, 12, 20))).astype(np.int16)
    phase = yy * 0.16 + np.sin(xx * 0.018) * 2.4 + np.sin(xx * 0.047) * 0.8
    ripple = np.maximum(0.0, np.cos(phase)) ** 20
    ripple_gain = (yy >= 340) * (2.0 + 9.0 * np.clip((yy - 340) / 200.0, 0.0, 1.0))
    road += np.round((ripple * ripple_gain)[..., None] * np.asarray((0.65, 0.8, 1.0))).astype(np.int16)
    road_noise = rng.integers(-1, 2, size=(height, width, 1), dtype=np.int16)
    road += road_noise
    road_zone = yy >= 350

    texture = np.zeros_like(rgba[..., :3], dtype=np.int16)
    texture[wall_zone] = wall[wall_zone]
    texture[road_zone] = road[road_zone]
    # Preserve the fixture's emitting shapes and dim text; texture fills only its
    # nearly-black negative space, leaving all practical cores and strokes intact.
    source_luma = rgba[..., :3].astype(np.float32).mean(axis=2)
    negative_space = source_luma < 36.0
    replace = negative_space & (wall_zone | road_zone)
    rgba[..., :3][replace] = np.clip(texture[replace], 0, 255).astype(np.uint8)
    return np.clip(rgba, 0, 255).astype(np.uint8)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image = make_material_fixture()
    if not cv2.imwrite(str(args.output), cv2.cvtColor(image, cv2.COLOR_RGBA2BGRA)):
        raise RuntimeError(f"Could not write {args.output}")
    print(f"Saved deterministic material fixture: {args.output.resolve()}")


if __name__ == "__main__":
    main()
