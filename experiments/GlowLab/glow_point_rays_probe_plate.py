"""Generate a deterministic no-ray night plate for point-ray effect captures."""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np


def make_plate() -> np.ndarray:
    width, height = 960, 540
    yy, xx = np.mgrid[0:height, 0:width]
    t = yy.astype(np.float32) / max(1, height - 1)
    # Dark navy upper field fades into a muted blue city horizon.
    top = np.asarray((10.0, 7.0, 5.0), dtype=np.float32)  # BGR
    bottom = np.asarray((42.0, 26.0, 16.0), dtype=np.float32)
    image = top[None, None, :] * (1.0 - t[..., None]) + bottom[None, None, :] * t[..., None]
    # Quiet city silhouettes and low-luminance windows supply context without baked light rays.
    cv2.rectangle(image, (0, 310), (959, 539), (15, 13, 12), -1)
    buildings = [
        (0, 214, 120, 442), (145, 263, 258, 449), (274, 194, 410, 449),
        (430, 287, 545, 449), (568, 230, 690, 449), (716, 268, 830, 449),
        (852, 207, 959, 449),
    ]
    for x0, y0, x1, y1 in buildings:
        shade = 19 + (x0 * 7 % 7)
        cv2.rectangle(image, (x0, y0), (x1, y1), (shade, shade + 1, shade + 3), -1)
        for wy in range(y0 + 18, y1 - 12, 27):
            for wx in range(x0 + 13, x1 - 8, 24):
                # Dim isolated windows remain below typical glow thresholds.
                value = 22 + ((wx * 13 + wy * 7) % 18)
                cv2.rectangle(image, (wx, wy), (wx + 4, wy + 8),
                              (value, value, value + 2), -1)
    # A low-contrast lamp post gives the main point a physical anchor, with no bright streaks.
    cv2.line(image, (326, 190), (326, 505), (26, 23, 22), 3, cv2.LINE_AA)
    cv2.line(image, (276, 192), (376, 192), (27, 24, 23), 3, cv2.LINE_AA)
    # Small cores at known positions; all bloom, rays, and star structure must come from the effect.
    cv2.circle(image, (326, 188), 5, (244, 242, 232), -1, cv2.LINE_AA)
    cv2.circle(image, (704, 248), 3, (52, 150, 244), -1, cv2.LINE_AA)
    cv2.circle(image, (158, 286), 3, (90, 184, 255), -1, cv2.LINE_AA)
    cv2.circle(image, (836, 220), 3, (188, 208, 255), -1, cv2.LINE_AA)
    return np.clip(image, 0, 255).astype(np.uint8)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not cv2.imwrite(str(args.output), make_plate()):
        raise RuntimeError(f"Could not write {args.output}")
    print(f"Saved {args.output}")


if __name__ == "__main__":
    main()
