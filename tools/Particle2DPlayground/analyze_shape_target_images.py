#!/usr/bin/env python3
"""Summarize RGBA distributions and black/white composites of headless captures."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageStat


MODES = ("baseline_disabled_default", "shape_ring", "text_A8")
FRAMES = (30, 60, 90, 180, 240)


def channel_summary(values: list[int]) -> dict[str, float | int]:
    ordered = sorted(values)

    def percentile(fraction: float) -> int:
        return ordered[round((len(ordered) - 1) * fraction)]

    return {
        "min": ordered[0],
        "p50": percentile(0.50),
        "p95": percentile(0.95),
        "p99": percentile(0.99),
        "mean": sum(ordered) / len(ordered),
        "max": ordered[-1],
    }


def summarize_image(path: Path) -> dict:
    with Image.open(path) as source:
        image = source.convert("RGBA")
    channels = list(image.split())
    pixel_count = image.width * image.height
    alpha_values = list(channels[3].get_flattened_data())
    alpha_counts = {
        "zero": sum(value == 0 for value in alpha_values),
        "partial": sum(0 < value < 255 for value in alpha_values),
        "full": sum(value == 255 for value in alpha_values),
    }
    rgb = {
        name: channel_summary(list(channel.get_flattened_data()))
        for name, channel in zip(("red", "green", "blue", "alpha"), channels)
    }
    black = Image.new("RGBA", image.size, (0, 0, 0, 255))
    white = Image.new("RGBA", image.size, (255, 255, 255, 255))
    black.alpha_composite(image)
    white.alpha_composite(image)
    return {
        "size": [image.width, image.height],
        "pixel_count": pixel_count,
        "alpha_counts": alpha_counts,
        "channels": rgb,
        "black_composite_rgb_mean": ImageStat.Stat(black.convert("RGB")).mean,
        "white_composite_rgb_mean": ImageStat.Stat(white.convert("RGB")).mean,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--d3d12", required=True, type=Path)
    parser.add_argument("--vulkan", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = {}
    for backend, directory in (("d3d12", args.d3d12), ("vulkan", args.vulkan)):
        result[backend] = {}
        for mode in MODES:
            result[backend][mode] = {}
            for frame in FRAMES:
                report_path = directory / "shape_target_report.json"
                report = json.loads(report_path.read_text(encoding="utf-8"))
                image_name = report["captures"][mode][str(frame)]["image"]
                result[backend][mode][str(frame)] = summarize_image(directory / image_name)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    for backend in ("d3d12", "vulkan"):
        for mode in MODES:
            frame90 = result[backend][mode]["90"]
            alpha = frame90["alpha_counts"]
            print(f"{backend} {mode} f90: alpha 0/{alpha['zero']} partial/{alpha['partial']} full/{alpha['full']}; "
                  f"white composite RGB mean={tuple(round(x, 2) for x in frame90['white_composite_rgb_mean'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
