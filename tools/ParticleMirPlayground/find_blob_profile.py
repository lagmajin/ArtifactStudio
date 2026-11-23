"""Find an isolated bright blob in a PNG and print its radial alpha profile.

Usage: python find_blob_profile.py <png> [search-region]
Finds the brightest isolated pixel, then walks outward along its row to
report the alpha/luminance profile across the blob. This distinguishes a
solid radial-gradient core (normal) from a hollow ring with a washed-out
centre (defect) without hand-picking coordinates.
"""

import sys
from pathlib import Path

from PySide6.QtGui import QImage


def find_isolated_blob(image: QImage):
    """Brightest pixel whose 8-neighbourhood is much darker (isolated)."""
    width, height = image.width(), image.height()
    best = None
    best_score = -1.0
    for y in range(2, height - 2):
        for x in range(2, width - 2):
            pixel = image.pixel(x, y)
            alpha = pixel >> 24 & 0xFF
            if alpha < 40:
                continue
            r = pixel >> 16 & 0xFF
            g = pixel >> 8 & 0xFF
            b = pixel & 0xFF
            luma = r * 0.2126 + g * 0.7152 + b * 0.0722
            if luma <= best_score:
                continue
            neighbours = []
            for dy in (-2, 2):
                for dx in (-2, 0, 2):
                    n = image.pixel(x + dx, y + dy)
                    neighbours.append((n >> 16 & 0xFF) * 0.2126 +
                                      (n >> 8 & 0xFF) * 0.7152 +
                                      (n & 0xFF) * 0.0722)
            avg = sum(neighbours) / len(neighbours)
            if luma - avg > 25.0:
                best_score = luma
                best = (x, y)
    return best, best_score


def row_profile(image: QImage, cx: int, cy: int, radius: int):
    alphas = []
    lums = []
    for x in range(max(0, cx - radius), min(image.width(), cx + radius + 1)):
        pixel = image.pixel(x, cy)
        alphas.append(pixel >> 24 & 0xFF)
        lums.append(round((pixel >> 16 & 0xFF) * 0.2126 +
                           (pixel >> 8 & 0xFF) * 0.7152 +
                           (pixel & 0xFF) * 0.0722, 1))
    return alphas, lums


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: find_blob_profile.py <png> [radius]")
        return 1
    radius = int(sys.argv[2]) if len(sys.argv) > 2 else 24
    path = Path(sys.argv[1])
    image = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    if image.isNull():
        print({"error": "could not load"})
        return 1
    (cx, cy), score = find_isolated_blob(image)
    if cx is None:
        print({"file": path.name, "error": "no isolated bright blob found"})
        return 1
    alphas, lums = row_profile(image, cx, cy, radius)
    peak = max(alphas)
    peak_index = alphas.index(peak)
    centre = alphas[len(alphas) // 2]
    edges = max(alphas[0], alphas[-1])
    print({
        "file": path.name,
        "blob_center": [cx, cy],
        "peak_luma": round(score, 1),
        "alpha_row": alphas,
        "luma_row": lums,
        "peak_alpha": peak,
        "peak_offset_px": peak_index - len(alphas) // 2,
        "centre_alpha": centre,
        "edge_alpha": edges,
        "hollow": bool(centre < peak * 0.6 and edges > centre),
    })
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
