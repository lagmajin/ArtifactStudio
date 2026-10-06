#!/usr/bin/env python3
"""Compare a UI screenshot against an approved reference image.

Writes a 50/50 overlay, an amplified grayscale difference image, and a JSON
report. Pixel-exact comparison is the default; tolerance must be requested
explicitly so platform variance cannot silently weaken a visual gate.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops, ImageStat, __version__ as PILLOW_VERSION


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def parse_region(value: str) -> tuple[str, tuple[int, int, int, int]]:
    try:
        name, coordinates = value.split("=", 1)
        x, y, width, height = (int(part) for part in coordinates.split(","))
    except (ValueError, TypeError) as error:
        raise argparse.ArgumentTypeError(
            "region must use NAME=X,Y,WIDTH,HEIGHT"
        ) from error
    if not name.strip() or width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError(
            "region name must be non-empty and width/height must be positive"
        )
    return name.strip(), (x, y, width, height)


def parse_region_limit(value: str) -> tuple[str, int, float]:
    try:
        name, max_pixels_text, max_fraction_text = value.split(",", 2)
        max_pixels = int(max_pixels_text)
        max_fraction = float(max_fraction_text)
    except (ValueError, TypeError) as error:
        raise argparse.ArgumentTypeError(
            "region limit must use NAME,MAX_DIFF_PIXELS,MAX_DIFF_FRACTION"
        ) from error
    if not name.strip() or max_pixels < 0 or not 0.0 <= max_fraction <= 1.0:
        raise argparse.ArgumentTypeError(
            "region limit requires a name, non-negative pixel limit, and fraction in [0, 1]"
        )
    return name.strip(), max_pixels, max_fraction


def measure_difference(
    difference: Image.Image, tolerance: int, box: tuple[int, int, int, int] | None = None
) -> dict[str, int | float]:
    measured = difference.crop(box) if box is not None else difference
    channels = measured.split()
    changed_mask = Image.new("L", measured.size, 0)
    for channel in channels:
        changed_mask = ImageChops.lighter(
            changed_mask,
            channel.point(lambda value: 255 if value > tolerance else 0),
        )
    changed_pixels = sum(changed_mask.histogram()[1:])
    total_pixels = measured.width * measured.height
    return {
        "total_pixels": total_pixels,
        "changed_pixels": changed_pixels,
        "changed_fraction": changed_pixels / total_pixels if total_pixels else 0.0,
        "maximum_channel_difference": max(channel.getextrema()[1] for channel in channels),
        "mean_absolute_channel_difference":
            sum(ImageStat.Stat(measured).mean) / len(channels),
    }


def compare(args: argparse.Namespace) -> int:
    reference_path = args.reference.resolve()
    actual_path = args.actual.resolve()
    prefix = args.output_prefix.resolve()
    prefix.parent.mkdir(parents=True, exist_ok=True)

    overlay_path = prefix.with_name(prefix.name + ".overlay.png")
    diff_path = prefix.with_name(prefix.name + ".diff.png")
    report_path = prefix.with_name(prefix.name + ".json")

    report: dict[str, Any] = {
        "schema_version": 1,
        "reference": {"path": str(reference_path), "sha256": sha256(reference_path)},
        "actual": {"path": str(actual_path), "sha256": sha256(actual_path)},
        "comparison": {
            "channel_tolerance": args.channel_tolerance,
            "max_diff_pixels": args.max_diff_pixels,
            "max_diff_fraction": args.max_diff_fraction,
            "diff_amplification": args.diff_amplification,
            "channels": "RGBA",
        },
        "tool": {"python": sys.version.split()[0], "pillow": PILLOW_VERSION},
        "artifacts": {
            "overlay": str(overlay_path),
            "diff": str(diff_path),
            "report": str(report_path),
        },
    }

    if args.environment_json is not None:
        report["environment"] = json.loads(
            args.environment_json.resolve().read_text(encoding="utf-8")
        )

    with Image.open(reference_path) as source:
        reference = source.convert("RGBA")
    with Image.open(actual_path) as source:
        actual = source.convert("RGBA")

    report["reference"]["size"] = list(reference.size)
    report["actual"]["size"] = list(actual.size)
    if reference.size != actual.size:
        comparison_size = (
            max(reference.width, actual.width),
            max(reference.height, actual.height),
        )
        aligned_reference = Image.new("RGBA", comparison_size, (0, 0, 0, 0))
        aligned_actual = Image.new("RGBA", comparison_size, (0, 0, 0, 0))
        aligned_reference.paste(reference, (0, 0))
        aligned_actual.paste(actual, (0, 0))
        difference = ImageChops.difference(aligned_reference, aligned_actual)
        difference_channels = difference.split()
        heat = difference_channels[0]
        for channel in difference_channels[1:]:
            heat = ImageChops.lighter(heat, channel)
        heat = heat.point(lambda value: min(255, value * args.diff_amplification))
        Image.blend(aligned_reference, aligned_actual, 0.5).save(overlay_path)
        Image.merge("RGB", (heat, heat, heat)).save(diff_path)

        regions: dict[str, dict[str, int | float]] = {}
        seen_regions: set[str] = set()
        for name, (x, y, width, height) in args.regions:
            if name in seen_regions:
                raise ValueError(f"duplicate region name: {name}")
            seen_regions.add(name)
            if x < 0 or y < 0 or x + width > comparison_size[0] or y + height > comparison_size[1]:
                raise ValueError(f"region {name!r} is outside the comparison canvas")
            regions[name] = measure_difference(
                difference, args.channel_tolerance, (x, y, x + width, y + height)
            )

        report["passed"] = False
        report["failure"] = "image dimensions differ"
        report["comparison_canvas_size"] = list(comparison_size)
        report["metrics"] = measure_difference(difference, args.channel_tolerance)
        if regions:
            report["regions"] = regions
        region_limits = {
            name: {"max_diff_pixels": max_pixels, "max_diff_fraction": max_fraction}
            for name, max_pixels, max_fraction in args.region_limits
        }
        for name, limits in region_limits.items():
            if name not in regions:
                raise ValueError(f"region limit references unknown region: {name}")
            region_metrics = regions[name]
            limits["passed"] = (
                int(region_metrics["changed_pixels"]) <= limits["max_diff_pixels"]
                and float(region_metrics["changed_fraction"]) <= limits["max_diff_fraction"]
            )
        if region_limits:
            report["region_limits"] = region_limits
        write_json(report_path, report)
        print(json.dumps(report, indent=2, sort_keys=True))
        return 1

    difference = ImageChops.difference(reference, actual)
    channels = difference.split()
    metrics = measure_difference(difference, args.channel_tolerance)
    changed_pixels = int(metrics["changed_pixels"])
    changed_fraction = float(metrics["changed_fraction"])

    regions: dict[str, dict[str, int | float]] = {}
    seen_regions: set[str] = set()
    for name, (x, y, width, height) in args.regions:
        if name in seen_regions:
            raise ValueError(f"duplicate region name: {name}")
        seen_regions.add(name)
        if x < 0 or y < 0 or x + width > reference.width or y + height > reference.height:
            raise ValueError(f"region {name!r} is outside the screenshot dimensions")
        regions[name] = measure_difference(
            difference, args.channel_tolerance, (x, y, x + width, y + height)
        )

    overlay = Image.blend(reference, actual, 0.5)
    overlay.save(overlay_path)

    heat = channels[0]
    for channel in channels[1:]:
        heat = ImageChops.lighter(heat, channel)
    heat = heat.point(lambda value: min(255, value * args.diff_amplification))
    Image.merge("RGB", (heat, heat, heat)).save(diff_path)

    passed = (
        changed_pixels <= args.max_diff_pixels
        and changed_fraction <= args.max_diff_fraction
    )
    region_limits = {
        name: {
            "max_diff_pixels": max_pixels,
            "max_diff_fraction": max_fraction,
            "passed": False,
        }
        for name, max_pixels, max_fraction in args.region_limits
    }
    for name, limits in region_limits.items():
        if name not in regions:
            raise ValueError(f"region limit references unknown region: {name}")
        metrics_for_region = regions[name]
        limits["passed"] = (
            int(metrics_for_region["changed_pixels"]) <= limits["max_diff_pixels"]
            and float(metrics_for_region["changed_fraction"]) <= limits["max_diff_fraction"]
        )
        passed = passed and bool(limits["passed"])
    report["metrics"] = metrics
    if regions:
        report["regions"] = regions
    if region_limits:
        report["region_limits"] = region_limits
    report["passed"] = passed
    write_json(report_path, report)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if passed else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, type=Path, help="approved baseline image")
    parser.add_argument("--actual", required=True, type=Path, help="captured UI screenshot")
    parser.add_argument(
        "--output-prefix", required=True, type=Path,
        help="prefix for .overlay.png, .diff.png, and .json outputs",
    )
    parser.add_argument(
        "--channel-tolerance", type=int, default=0, choices=range(256), metavar="0..255",
        help="per-channel difference ignored when counting changed pixels (default: 0)",
    )
    parser.add_argument(
        "--max-diff-pixels", type=int, default=0,
        help="maximum changed pixel count allowed (default: 0)",
    )
    parser.add_argument(
        "--max-diff-fraction", type=float, default=0.0,
        help="maximum changed pixel fraction allowed (default: 0)",
    )
    parser.add_argument(
        "--diff-amplification", type=int, default=4, choices=range(1, 17), metavar="1..16",
        help="amplify difference image brightness (default: 4)",
    )
    parser.add_argument(
        "--environment-json", type=Path,
        help="runner manifest containing DPI, Qt, theme, font, locale, and window state",
    )
    parser.add_argument(
        "--region", dest="regions", action="append", type=parse_region, default=[],
        help="named pixel region to report as NAME=X,Y,WIDTH,HEIGHT; repeat as needed",
    )
    parser.add_argument(
        "--region-limit", dest="region_limits", action="append",
        type=parse_region_limit, default=[],
        help="region gate as NAME,MAX_DIFF_PIXELS,MAX_DIFF_FRACTION; repeat as needed",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.max_diff_pixels < 0:
        raise SystemExit("--max-diff-pixels must be non-negative")
    if not 0.0 <= args.max_diff_fraction <= 1.0:
        raise SystemExit("--max-diff-fraction must be between 0 and 1")
    limit_names = [name for name, _, _ in args.region_limits]
    if len(limit_names) != len(set(limit_names)):
        raise SystemExit("--region-limit names must be unique")
    try:
        return compare(args)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ui_visual_compare: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
