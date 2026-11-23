"""Profile the alpha/radii profile of isolated blobs in a PNG.

Usage: python profile_blob.py <png> <x> <y> [radius]
Prints the alpha (and luminance) profile along the horizontal line through
(x, y), which shows whether a rendered particle is a solid gradient core
(normal) or a hollow ring with a washed-out centre (defect).
"""

import sys
from pathlib import Path

from PySide6.QtGui import QImage


def profile(path: Path, cx: int, cy: int, radius: int) -> dict:
    image = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    if image.isNull():
        return {"error": "could not load"}
    alphas = []
    lums = []
    for x in range(max(0, cx - radius), min(image.width(), cx + radius + 1)):
        pixel = image.pixel(x, cy)
        a = pixel >> 24 & 0xFF
        r = pixel >> 16 & 0xFF
        g = pixel >> 8 & 0xFF
        b = pixel & 0xFF
        alphas.append(a)
        lums.append(round((r * 0.2126 + g * 0.7152 + b * 0.0722), 1))
    peak = max(alphas) if alphas else 0
    peak_index = alphas.index(peak) if peak else -1
    # A hollow ring shows a local minimum in the middle with higher values at
    # both sides; a solid blob shows the maximum in the middle.
    centre = alphas[len(alphas) // 2] if alphas else 0
    edges = max(alphas[0], alphas[-1]) if alphas else 0
    return {
        "file": path.name,
        "center": [cx, cy],
        "alpha_row": alphas,
        "luma_row": lums,
        "peak_alpha": peak,
        "peak_offset_px": peak_index - len(alphas) // 2,
        "centre_alpha": centre,
        "edge_alpha": edges,
        "hollow": bool(centre < peak * 0.6 and edges > centre),
    }


def main() -> int:
    if len(sys.argv) < 4:
        print("Usage: profile_blob.py <png> <x> <y> [radius]")
        return 1
    radius = int(sys.argv[4]) if len(sys.argv) > 4 else 30
    print(profile(Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]), radius))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
