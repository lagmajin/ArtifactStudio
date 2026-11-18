#!/usr/bin/env python3
"""Compare matching 2D particle Shape/Text headless captures."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from PIL import Image, ImageChops
except ImportError as exc:
    raise SystemExit("Pillow is required to compare capture PNGs") from exc


FRAMES = (30, 60, 90, 180, 240)
MODES = ("baseline_disabled_default", "shape_ring", "text_A8")


def compare_images(left_path: Path, right_path: Path) -> dict[str, int | list[int]]:
    with Image.open(left_path) as left_source, Image.open(right_path) as right_source:
        left = left_source.convert("RGBA")
        right = right_source.convert("RGBA")
    if left.size != right.size:
        raise ValueError(f"image dimensions differ: {left_path} {right.size} != {left.size}")
    difference = ImageChops.difference(left, right)
    extrema = difference.getextrema()
    maximum = max(channel[1] for channel in extrema)
    channel_deltas = [max(pixel) for pixel in difference.get_flattened_data()]
    ordered_deltas = sorted(channel_deltas)
    alpha_maximum = extrema[3][1]
    alpha_diff_pixels = sum(1 for pixel in difference.getchannel("A").get_flattened_data() if pixel)
    return {
        "different_pixels": sum(1 for delta in channel_deltas if delta),
        "max_channel_delta": maximum,
        "p95_max_channel_delta": ordered_deltas[int((len(ordered_deltas) - 1) * 0.95)],
        "alpha_different_pixels": alpha_diff_pixels,
        "max_alpha_delta": alpha_maximum,
        "size": list(left.size),
    }


def compare_centers(left: list[dict], right: list[dict]) -> dict[str, int | float]:
    if len(left) != len(right):
        raise ValueError(f"particle counts differ: {len(left)} != {len(right)}")
    mismatches = 0
    max_delta = 0.0
    for a, b in zip(left, right):
        if a.get("index") != b.get("index"):
            mismatches += 1
            continue
        dx = float(a["x"]) - float(b["x"])
        dy = float(a["y"]) - float(b["y"])
        max_delta = max(max_delta, (dx * dx + dy * dy) ** 0.5)
    return {"particle_id_mismatches": mismatches, "max_position_delta_px": max_delta}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--d3d12", required=True, type=Path, help="D3D12 capture directory")
    parser.add_argument("--vulkan", required=True, type=Path, help="Vulkan capture directory")
    parser.add_argument("--output", required=True, type=Path, help="JSON report path")
    args = parser.parse_args()

    reports = {
        "d3d12": json.loads((args.d3d12 / "shape_target_report.json").read_text(encoding="utf-8")),
        "vulkan": json.loads((args.vulkan / "shape_target_report.json").read_text(encoding="utf-8")),
    }
    for backend, report in reports.items():
        if report.get("backend") != backend:
            raise ValueError(f"expected {backend} report, got {report.get('backend')!r}")
    metadata_keys = (
        "width", "height", "fps", "seed", "fixture_layer_ids", "requested_font_family",
        "resolved_font_family", "font_weight", "qt_version", "font_exact_match",
    )
    metadata_match = all(reports["d3d12"].get(key) == reports["vulkan"].get(key) for key in metadata_keys)
    if not metadata_match:
        raise ValueError("capture metadata differs between backends")

    comparisons: dict[str, dict] = {}
    for mode in MODES:
        comparisons[mode] = {}
        for frame in FRAMES:
            key = str(frame)
            d3d_frame = reports["d3d12"]["captures"][mode][key]
            vk_frame = reports["vulkan"]["captures"][mode][key]
            image_result = compare_images(args.d3d12 / d3d_frame["image"], args.vulkan / vk_frame["image"])
            result = {"image": image_result}
            if "particle_centers" in d3d_frame or "particle_centers" in vk_frame:
                if "particle_centers" not in d3d_frame or "particle_centers" not in vk_frame:
                    raise ValueError(f"particle-center data missing for {mode} frame {frame}")
                result["particles"] = compare_centers(
                    d3d_frame["particle_centers"], vk_frame["particle_centers"]
                )
            comparisons[mode][key] = result

    report = {
        "metadata_match": metadata_match,
        "metadata": {key: reports["d3d12"].get(key) for key in metadata_keys},
        "comparisons": comparisons,
        "no_op_targets": {
            backend: report["no_op_targets"] for backend, report in reports.items()
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for mode, frames in comparisons.items():
        summary = ", ".join(
            f"f{frame}: {frames[str(frame)]['image']['different_pixels']} px"
            + (f" / {frames[str(frame)]['particles']['max_position_delta_px']:.3g}px"
               if "particles" in frames[str(frame)] else "")
            for frame in FRAMES
        )
        print(f"{mode}: {summary}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, KeyError, TypeError, ValueError) as error:
        print(f"comparison failed: {error}", file=sys.stderr)
        raise SystemExit(1)
