"""Analyse particle blob profiles inside a region of a PNG.

Usage: python profile_region.py <png> <x> <y> <w> <h> [max-blobs]
For every isolated bright blob found in the region, print the alpha profile
across the blob and classify it as:
  solid   - the alpha peaks in the middle and falls off outward (correct
            radial gradient sprite)
  hollow  - the centre is much darker than the rim (defective falloff)
  flat    - the alpha barely varies (saturated core)
The classification tells whether an apparent "ring" in a dense capture comes
from the per-particle sprite or from overlapping particles saturating.
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
    results = []
    for cy in range(y0, min(y0 + h, image.height())):
        for cx in range(x0, min(x0 + w, image.width())):
            pixel = image.pixel(cx, cy)
            alpha = pixel >> 24 & 0xFF
            if alpha < 40:
                continue
            value = luma(pixel)
            neighbours = [luma(image.pixel(cx + dx, cy + dy))
                          for dy in (-2, 2) for dx in (-2, 0, 2)]
            if value - sum(neighbours) / len(neighbours) <= 25.0:
                continue
            alphas = []
            for x in range(max(0, cx - 24), min(image.width(), cx + 25)):
                alphas.append(image.pixel(x, cy) >> 24 & 0xFF)
            peak = max(alphas)
            peak_index = alphas.index(peak)
            centre = alphas[len(alphas) // 2]
            # The blob spans the contiguous non-zero run around the peak.
            start = peak_index
            while start > 0 and alphas[start - 1] > 0:
                start -= 1
            end = peak_index
            while end + 1 < len(alphas) and alphas[end + 1] > 0:
                end += 1
            span = alphas[start:end + 1]
            if len(span) < 5:
                continue
            inner = span[len(span) // 4: max(len(span) // 4 + 1, 3 * len(span) // 4)]
            rim = span[:max(1, len(span) // 4)] + span[max(len(span) - len(span) // 4, 1):]
            inner_mean = sum(inner) / len(inner)
            rim_mean = sum(rim) / len(rim) if rim else 0
            if inner_mean < rim_mean * 0.75:
                kind = "hollow"
            elif peak >= 250 and rim_mean >= peak * 0.8:
                kind = "flat"
            else:
                kind = "solid"
            results.append({
                "center": [cx, cy],
                "span_px": len(span),
                "peak_alpha": peak,
                "inner_mean_alpha": round(inner_mean, 1),
                "rim_mean_alpha": round(rim_mean, 1),
                "kind": kind,
            })
            if len(results) >= max_blobs:
                return {"region": [x0, y0, w, h], "blobs": results}
    return {"region": [x0, y0, w, h], "blobs": results}


def main() -> int:
    if len(sys.argv) < 6:
        print("Usage: profile_region.py <png> <x> <y> <w> <h> [max-blobs]")
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
