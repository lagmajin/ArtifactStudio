"""Compare the apparent particle size of two captures in a local crop.

Usage: python compare_crop.py <a.png> <b.png> <x> <y> <w> <h>
Prints the covered-pixel count and the median run length inside the crop for
both images, which shows the apparent particle diameter difference between
render paths without being confused by different background colours.
"""

import sys
from pathlib import Path

from PySide6.QtGui import QImage


def crop_stats(path: Path, x: int, y: int, w: int, h: int) -> dict:
    image = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    if image.isNull():
        return {"error": "could not load"}
    background = image.pixel(0, 0)
    bg_r = background >> 16 & 0xFF
    bg_g = background >> 8 & 0xFF
    bg_b = background & 0xFF
    covered = 0
    runs = []
    total = 0
    for row in range(y, min(y + h, image.height())):
        run = 0
        for col in range(x, min(x + w, image.width())):
            pixel = image.pixel(col, row)
            r = pixel >> 16 & 0xFF
            g = pixel >> 8 & 0xFF
            b = pixel & 0xFF
            delta = max(abs(r - bg_r), abs(g - bg_g), abs(b - bg_b))
            total += 1
            if delta > 24:
                covered += 1
                run += 1
            else:
                if run:
                    runs.append(run)
                run = 0
        if run:
            runs.append(run)
    runs.sort()
    return {
        "file": path.name,
        "crop": f"{x},{y},{w}x{h}",
        "covered_pixels": covered,
        "coverage_ratio": round(covered / total, 5) if total else 0.0,
        "runs": len(runs),
        "median_run_px": runs[len(runs) // 2] if runs else 0,
        "max_run_px": runs[-1] if runs else 0,
    }


def main() -> int:
    if len(sys.argv) != 7:
        print("Usage: compare_crop.py <a.png> <b.png> <x> <y> <w> <h>")
        return 1
    x, y, w, h = (int(v) for v in sys.argv[3:7])
    for raw in sys.argv[1:3]:
        print(crop_stats(Path(raw), x, y, w, h))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
