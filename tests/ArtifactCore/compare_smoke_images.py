#!/usr/bin/env python3
"""Compare two smoke emitter PNGs and report image-space regression metrics."""

import argparse
import json
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--max-mean-absolute-error", type=float, default=0.03)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    reference = Image.open(args.reference).convert("RGBA")
    candidate = Image.open(args.candidate).convert("RGBA")
    if reference.size != candidate.size:
        raise SystemExit(
            f"image dimensions differ: {reference.size} != {candidate.size}"
        )
    difference = ImageChops.difference(reference, candidate)
    channels = ImageStat.Stat(difference).mean
    mean_absolute_error = sum(channels[:3]) / (3.0 * 255.0)
    threshold = 24
    changed_pixels = 0
    for y in range(difference.height):
        for x in range(difference.width):
            if max(difference.getpixel((x, y))[:3]) > threshold:
                changed_pixels += 1
    report = {
        "reference": str(args.reference),
        "candidate": str(args.candidate),
        "width": reference.width,
        "height": reference.height,
        "mean_absolute_error": mean_absolute_error,
        "changed_pixel_ratio": changed_pixels / (reference.width * reference.height),
        "max_mean_absolute_error": args.max_mean_absolute_error,
        "passed": mean_absolute_error <= args.max_mean_absolute_error,
    }
    encoded = json.dumps(report, indent=2)
    if args.report:
        args.report.write_text(encoded + "\n", encoding="utf-8")
    print(encoded)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
