"""Mir particle capture: cross-backend (D3D12 vs Vulkan) image comparison.

Usage: python compare_backends.py <d3d12-dir> <vulkan-dir> <output-json>
Reads PNGs with PySide6/QImage (no numpy dependency) and reports per-pair
pixel differences so the environment-to-environment tolerance in
EVALUATION_PLAN.md can be recorded with evidence.
"""

import json
import sys
from pathlib import Path

from PySide6.QtGui import QImage


def compare(left_path: Path, right_path: Path) -> dict:
    left = QImage(str(left_path)).convertToFormat(QImage.Format_ARGB32)
    right = QImage(str(right_path)).convertToFormat(QImage.Format_ARGB32)
    if left.isNull() or right.isNull():
        return {"error": "image could not be loaded"}
    if left.size() != right.size():
        return {"error": f"size mismatch {left.size()} vs {right.size()}"}

    different = 0
    max_delta = 0
    delta_histogram = {}
    for y in range(left.height()):
        for x in range(left.width()):
            lp = left.pixel(x, y)
            rp = right.pixel(x, y)
            delta = max(
                abs((lp >> 16 & 0xFF) - (rp >> 16 & 0xFF)),
                abs((lp >> 8 & 0xFF) - (rp >> 8 & 0xFF)),
                abs((lp & 0xFF) - (rp & 0xFF)),
                abs((lp >> 24 & 0xFF) - (rp >> 24 & 0xFF)),
            )
            if delta:
                different += 1
            max_delta = max(max_delta, delta)
            bucket = delta // 8 * 8
            delta_histogram[str(bucket)] = delta_histogram.get(str(bucket), 0) + 1

    total = left.width() * left.height()
    return {
        "size": f"{left.width()}x{left.height()}",
        "total_pixels": total,
        "different_pixels": different,
        "different_ratio": round(different / total, 8) if total else 0.0,
        "max_channel_delta": max_delta,
        "delta_histogram_bucket8": delta_histogram,
    }


def main() -> int:
    if len(sys.argv) != 4:
        print("Usage: compare_backends.py <d3d12-dir> <vulkan-dir> <output-json>")
        return 1

    d3d12_dir = Path(sys.argv[1])
    vulkan_dir = Path(sys.argv[2])
    output_path = Path(sys.argv[3])

    result = {}
    for name in (
        "mir_2d_frame_45.png",
        "mir_2d_frame_60.png",
        "mir_2d_direct_seek_frame_45.png",
        "mir_3d_front_frame_45.png",
        "mir_3d_orbit_frame_45.png",
        "mir_3d_frame_60.png",
    ):
        left = d3d12_dir / name
        right = vulkan_dir / name
        if not left.exists() or not right.exists():
            result[name] = {"error": "missing capture"}
            continue
        result[name] = compare(left, right)

    output_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
