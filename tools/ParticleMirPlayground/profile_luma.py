"""Classify particle blob shapes by luminance profile in an opaque render.

Usage: python profile_luma.py <png> <x> <y> <w> <h> [max-blobs]
The GPU render path writes an opaque background, so alpha carries no
information. This variant measures the luminance profile across each isolated
bright blob instead, and classifies the shape:
  solid  - luminance peaks in the middle and falls off outward
  hollow - the centre is much darker than the rim (defective falloff)
  flat   - luminance barely varies across the blob (saturated core)
"""

import sys
from pathlib import Path

from PySide6.QtGui import QImage


def luma(pixel: int) -> float:
    r = pixel >> 16 & 0xFF
    g = pixel >> 8 & 0xFF
    b = pixel & 0xFF
    return r * 0.2126 + g * 0.7152 + b * 0.0722


def analyse(path: Path, x0: int, y0: int, w: int, h: int, max_blobs: int):
    image = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    if image.isNull():
        return {"error": "could not load"}
    background = luma(image.pixel(0, 0))
    results = []
    for cy in range(max(1, y0), min(y0 + h, image.height()) - 1):
        for cx in range(max(1, x0), min(x0 + w, image.width()) - 1):
            value = luma(image.pixel(cx, cy))
            if value - background <= 30.0:
                continue
            neighbours = [luma(image.pixel(cx + dx, cy + dy))
                          for dy in (-2, 2) for dx in (-2, 0, 2)]
            if value - sum(neighbours) / len(neighbours) <= 25.0:
                continue
            lumas = []
            for x in range(max(0, cx - 24), min(image.width(), cx + 25)):
                lumas.append(luma(image.pixel(x, cy)))
            peak = max(lumas)
            peak_index = lumas.index(peak)
            start = peak_index
            while start > 0 and lumas[start - 1] > background + 6:
                start -= 1
            end = peak_index
            while end + 1 < len(lumas) and lumas[end + 1] > background + 6:
                end += 1
            span = lumas[start:end + 1]
            if len(span) < 5:
                continue
            quarter = max(1, len(span) // 4)
            inner = span[quarter: max(quarter + 1, 3 * quarter)]
            rim = span[:quarter] + span[max(len(span) - quarter, 1):]
            inner_mean = sum(inner) / len(inner)
            rim_mean = sum(rim) / len(rim) if rim else 0
            if inner_mean < rim_mean * 0.75:
                kind = "hollow"
            elif (peak - background) < 40:
                kind = "flat"
            else:
                kind = "solid"
            results.append({
                "center": [cx, cy],
                "span_px": len(span),
                "peak_luma": round(peak, 1),
                "inner_mean_luma": round(inner_mean, 1),
                "rim_mean_luma": round(rim_mean, 1),
                "kind": kind,
            })
            if len(results) >= max_blobs:
                return {"region": [x0, y0, w, h], "blobs": results}
    return {"region": [x0, y0, w, h], "blobs": results}


def main() -> int:
    if len(sys.argv) < 6:
        print("Usage: profile_luma.py <png> <x> <y> <w> <h> [max-blobs]")
        return 1
    max_blobs = int(sys.argv[6]) if len(sys.argv) > 6 else 12
    out = analyse(Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]),
                  int(sys.argv[4]), int(sys.argv[5]), max_blobs)
    kinds = {}
    for blob in out.get("blobs", []):
        kinds[blob["kind"]] = kinds.get(blob["kind"], 0) + 1
    out["kind_histogram"] = kinds
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
