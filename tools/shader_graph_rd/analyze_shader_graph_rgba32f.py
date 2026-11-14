#!/usr/bin/env python3
"""Analyze repeated RGBA32F Shader Graph captures and candidate error."""

from __future__ import annotations

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys
from typing import Sequence


CHANNELS = ("R", "G", "B", "A")


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _read_rgba32f(path: Path, width: int, height: int,
                  byte_order: str) -> array:
    expected_bytes = width * height * 4 * 4
    payload = path.read_bytes()
    if len(payload) != expected_bytes:
        raise ValueError(
            f"{path}: {len(payload)} bytes; expected {expected_bytes} for "
            f"{width}x{height} RGBA32F"
        )
    values = array("f")
    if values.itemsize != 4:
        raise ValueError("this Python runtime does not provide 32-bit float arrays")
    values.frombytes(payload)
    file_is_little_endian = (
        sys.byteorder == "little" if byte_order == "native"
        else byte_order == "little"
    )
    if file_is_little_endian != (sys.byteorder == "little"):
        values.byteswap()
    return values


def _parse_roi(value: str, width: int, height: int) -> dict[str, int | str]:
    name, separator, bounds = value.partition(":")
    if not separator or not name.strip():
        raise ValueError(f"invalid ROI {value!r}; expected name:x,y,width,height")
    try:
        x, y, roi_width, roi_height = (int(part.strip()) for part in bounds.split(","))
    except (TypeError, ValueError) as error:
        raise ValueError(f"invalid ROI {value!r}; expected name:x,y,width,height") from error
    if x < 0 or y < 0 or roi_width <= 0 or roi_height <= 0:
        raise ValueError(f"ROI must have nonnegative origin and positive size: {value!r}")
    if x + roi_width > width or y + roi_height > height:
        raise ValueError(f"ROI is outside {width}x{height}: {value!r}")
    return {"name": name.strip(), "x": x, "y": y,
            "width": roi_width, "height": roi_height}


def _region_indices(region: dict[str, int | str], width: int):
    x0 = int(region["x"])
    y0 = int(region["y"])
    roi_width = int(region["width"])
    roi_height = int(region["height"])
    for y in range(y0, y0 + roi_height):
        pixel_start = y * width + x0
        for x_offset in range(roi_width):
            yield pixel_start + x_offset


def _percentile(sorted_values: Sequence[float], fraction: float) -> float | None:
    if not sorted_values:
        return None
    position = (len(sorted_values) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(sorted_values) - 1)
    weight = position - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def _distribution(values: Sequence[float]) -> dict[str, int | float | None]:
    finite = [value for value in values if math.isfinite(value)]
    ordered = sorted(finite)
    outside_unit = sum(value < 0.0 or value > 1.0 for value in finite)
    return {
        "sampleCount": len(values),
        "nonFiniteCount": len(values) - len(finite),
        "outsideUnitRangeCount": outside_unit,
        "min": ordered[0] if ordered else None,
        "max": ordered[-1] if ordered else None,
        "mean": statistics.fmean(finite) if finite else None,
        "p50": _percentile(ordered, 0.50),
        "p95": _percentile(ordered, 0.95),
        "p99": _percentile(ordered, 0.99),
    }


def _channel_values(pixels: Sequence[float], width: int,
                    region: dict[str, int | str], channel: int) -> list[float]:
    return [pixels[pixel * 4 + channel]
            for pixel in _region_indices(region, width)]


def _image_distributions(pixels: Sequence[float], width: int,
                         region: dict[str, int | str]) -> dict[str, object]:
    return {CHANNELS[channel]: _distribution(
        _channel_values(pixels, width, region, channel)) for channel in range(4)}


def _repeat_noise(repeats: Sequence[array], width: int,
                  region: dict[str, int | str]) -> dict[str, object]:
    result: dict[str, object] = {}
    for channel, name in enumerate(CHANNELS):
        ranges = []
        for pixel in _region_indices(region, width):
            samples = [capture[pixel * 4 + channel] for capture in repeats]
            if all(math.isfinite(value) for value in samples):
                ranges.append(max(samples) - min(samples))
        result[name] = _distribution(ranges)
    return result


def _candidate_error(baseline_mean: Sequence[float], candidate: Sequence[float],
                     width: int, region: dict[str, int | str],
                     relative_epsilon: float, threshold: float | None
                     ) -> dict[str, object]:
    result: dict[str, object] = {}
    for channel, name in enumerate(CHANNELS):
        absolute: list[float] = []
        relative: list[float] = []
        non_finite = 0
        for pixel in _region_indices(region, width):
            reference = baseline_mean[pixel * 4 + channel]
            value = candidate[pixel * 4 + channel]
            if not math.isfinite(reference) or not math.isfinite(value):
                non_finite += 1
                continue
            error = abs(value - reference)
            absolute.append(error)
            relative.append(error / max(abs(reference), relative_epsilon))
        absolute.sort()
        relative.sort()
        channel_result: dict[str, object] = {
            "sampleCount": len(absolute),
            "nonFinitePairCount": non_finite,
            "absolute": {
                "mae": statistics.fmean(absolute) if absolute else None,
                "rmse": math.sqrt(statistics.fmean(
                    value * value for value in absolute)) if absolute else None,
                "p50": _percentile(absolute, 0.50),
                "p95": _percentile(absolute, 0.95),
                "p99": _percentile(absolute, 0.99),
                "max": absolute[-1] if absolute else None,
            },
            "relative": {
                "epsilon": relative_epsilon,
                "p50": _percentile(relative, 0.50),
                "p95": _percentile(relative, 0.95),
                "p99": _percentile(relative, 0.99),
                "max": relative[-1] if relative else None,
            },
        }
        if threshold is not None:
            channel_result["absolute"]["threshold"] = threshold
            channel_result["absolute"]["thresholdExceedanceRate"] = (
                sum(value > threshold for value in absolute) / len(absolute)
                if absolute else None
            )
        result[name] = channel_result
    return result


def _write_difference(path: Path, baseline_mean: Sequence[float],
                      candidate: Sequence[float], byte_order: str) -> str:
    difference = array("f", (
        abs(value - reference)
        for reference, value in zip(baseline_mean, candidate)
    ))
    if byte_order != "native" and ((byte_order == "little") != (sys.byteorder == "little")):
        difference.byteswap()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        difference.tofile(stream)
    return _sha256(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, action="append", required=True,
                        help="legacy RGBA32F capture; repeat for noise characterization")
    parser.add_argument("--candidate", type=Path, required=True,
                        help="candidate RGBA32F capture")
    parser.add_argument("--width", type=int, default=256)
    parser.add_argument("--height", type=int, default=256)
    parser.add_argument("--byte-order", choices=("native", "little", "big"),
                        default="native")
    parser.add_argument("--roi", action="append", default=[], metavar="NAME:X,Y,W,H",
                        help="top-left-origin ROI; may be repeated")
    parser.add_argument("--relative-epsilon", type=float, default=1.0e-8)
    parser.add_argument("--threshold", type=float,
                        help="optional absolute-error threshold fixed before comparison")
    parser.add_argument("--diff-output", type=Path,
                        help="write per-channel absolute error RGBA32F")
    parser.add_argument("--output", type=Path,
                        help="write JSON report here instead of stdout")
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        parser.error("width and height must be positive")
    if not math.isfinite(args.relative_epsilon) or args.relative_epsilon <= 0.0:
        parser.error("--relative-epsilon must be finite and positive")
    if args.threshold is not None and (
        not math.isfinite(args.threshold) or args.threshold < 0.0
    ):
        parser.error("--threshold must be finite and nonnegative")

    input_paths = [*args.baseline, args.candidate]
    output_paths = [path for path in (args.diff_output, args.output) if path is not None]
    if len({path.resolve() for path in output_paths}) != len(output_paths):
        parser.error("--output and --diff-output must use different paths")
    if {path.resolve() for path in input_paths} & {path.resolve() for path in output_paths}:
        parser.error("analysis outputs must not overwrite capture inputs")

    try:
        if len({path.resolve() for path in args.baseline}) != len(args.baseline):
            raise ValueError("baseline capture paths must be unique")
        baselines = [_read_rgba32f(path, args.width, args.height, args.byte_order)
                     for path in args.baseline]
        candidate = _read_rgba32f(args.candidate, args.width, args.height,
                                  args.byte_order)
        regions: list[dict[str, int | str]] = [{
            "name": "full-frame", "x": 0, "y": 0,
            "width": args.width, "height": args.height,
        }]
        regions.extend(_parse_roi(value, args.width, args.height) for value in args.roi)
        names = [str(region["name"]) for region in regions]
        if len(set(names)) != len(names):
            raise ValueError("ROI names must be unique and cannot repeat 'full-frame'")

        baseline_mean = array("f", [0.0]) * (args.width * args.height * 4)
        for index in range(len(baseline_mean)):
            samples = [capture[index] for capture in baselines]
            baseline_mean[index] = statistics.fmean(samples)

        report: dict[str, object] = {
            "kind": "artifact-shader-graph-rgba32f-analysis",
            "status": "analysis-generated-no-quality-decision",
            "width": args.width,
            "height": args.height,
            "channelOrder": "RGBA",
            "byteOrder": args.byte_order,
            "alpha": "straight sample preserved from capture; no premultiply or composite",
            "inputs": {
                "baseline": [
                    {"path": path.as_posix(), "sha256": _sha256(path)}
                    for path in args.baseline
                ],
                "candidate": {
                    "path": args.candidate.as_posix(),
                    "sha256": _sha256(args.candidate),
                },
            },
            "relativeErrorEpsilon": args.relative_epsilon,
            "absoluteErrorThreshold": args.threshold,
            "regions": [],
        }
        for region in regions:
            report["regions"].append({
                "boundsTopLeftOrigin": region,
                "baselineRepeatNoiseRange": _repeat_noise(
                    baselines, args.width, region),
                "baselineMeanDistribution": _image_distributions(
                    baseline_mean, args.width, region),
                "candidateDistribution": _image_distributions(
                    candidate, args.width, region),
                "candidateVsBaselineMeanError": _candidate_error(
                    baseline_mean, candidate, args.width, region,
                    args.relative_epsilon, args.threshold),
            })

        if args.diff_output is not None:
            report["differenceCapture"] = {
                "path": args.diff_output.as_posix(),
                "sha256": _write_difference(
                    args.diff_output, baseline_mean, candidate, args.byte_order),
                "formula": "per-channel abs(candidate - mean(baseline repeats)); RGBA32F",
            }
        encoded = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.output is None:
            sys.stdout.write(encoded)
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(encoded, encoding="utf-8", newline="\n")
    except (OSError, UnicodeError, ValueError, OverflowError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
