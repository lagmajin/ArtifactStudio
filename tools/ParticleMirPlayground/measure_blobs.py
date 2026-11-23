"""Measure apparent particle blob size in a PNG to compare render paths.

Usage: python measure_blobs.py <png> [<png> ...]
Reports coverage ratio, connected-blob count approximation via row-run
analysis, and the median vertical run length (an approximation of the
apparent particle diameter in pixels).
"""

import sys
from pathlib import Path

from PySide6.QtGui import QImage


def measure(path: Path) -> dict:
    image = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    if image.isNull():
        return {"error": "could not load"}
    width, height = image.width(), image.height()
    opaque = 0
    run_lengths = []
    for y in range(height):
        run = 0
        for x in range(width):
            alpha = image.pixel(x, y) >> 24
            if alpha > 32:
                opaque += 1
                run += 1
            else:
                if run:
                    run_lengths.append(run)
                run = 0
        if run:
            run_lengths.append(run)
    total = width * height
    run_lengths.sort()
    median = run_lengths[len(run_lengths) // 2] if run_lengths else 0
    return {
        "file": path.name,
        "size": f"{width}x{height}",
        "opaque_pixels": opaque,
        "coverage_ratio": round(opaque / total, 6) if total else 0.0,
        "opaque_runs": len(run_lengths),
        "median_run_px": median,
        "max_run_px": run_lengths[-1] if run_lengths else 0,
    }


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: measure_blobs.py <png> [<png> ...]")
        return 1
    for raw in sys.argv[1:]:
        path = Path(raw)
        print(measure(path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
