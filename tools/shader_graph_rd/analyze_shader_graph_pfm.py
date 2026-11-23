#!/usr/bin/env python3
"""Compare two RGB PFM captures from the Shader Graph R&D suite."""

from __future__ import annotations

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
from typing import Iterable


def _header_line(stream) -> bytes:
    while True:
        line = stream.readline()
        if not line:
            raise ValueError("unexpected end of PFM header")
        stripped = line.strip()
        if stripped and not stripped.startswith(b"#"):
            return stripped


def read_rgb_pfm(path: Path) -> tuple[int, int, list[float]]:
    with path.open("rb") as stream:
        magic = _header_line(stream)
        if magic != b"PF":
            raise ValueError(f"{path}: expected RGB PFM magic 'PF', got {magic!r}")
        dimensions = _header_line(stream).split()
        if len(dimensions) != 2:
            raise ValueError(f"{path}: invalid dimensions line")
        width, height = (int(value) for value in dimensions)
        if width <= 0 or height <= 0:
            raise ValueError(f"{path}: dimensions must be positive")
        scale = float(_header_line(stream))
        if not math.isfinite(scale) or scale == 0.0:
            raise ValueError(f"{path}: invalid scale/endian marker")
        payload = stream.read()

    expected_values = width * height * 3
    if array("f").itemsize != 4:
        raise ValueError("this Python runtime does not provide 32-bit float arrays")
    if len(payload) != expected_values * 4:
        raise ValueError(
            f"{path}: payload is {len(payload)} bytes; expected {expected_values * 4}"
        )
    values = array("f")
    values.frombytes(payload)
    file_is_little_endian = scale < 0.0
    if file_is_little_endian != (sys.byteorder == "little"):
        values.byteswap()
    magnitude = abs(scale)
    if magnitude != 1.0:
        values = array("f", (value * magnitude for value in values))
    return width, height, values.tolist()


def _percentile(sorted_values: list[float], fraction: float) -> float | None:
    if not sorted_values:
        return None
    position = (len(sorted_values) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(sorted_values) - 1)
    weight = position - lower
    return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight


def _regions(width: int, height: int, rois: Iterable[str]) -> list[dict[str, int | str]]:
    result: list[dict[str, int | str]] = [
        {"name": "full-frame", "x": 0, "y": 0, "width": width, "height": height}
    ]
    for index, value in enumerate(rois, start=1):
        try:
            x, y, roi_width, roi_height = (int(part.strip()) for part in value.split(","))
        except (TypeError, ValueError) as error:
            raise ValueError(f"invalid ROI {value!r}; expected x,y,width,height") from error
        if x < 0 or y < 0 or roi_width <= 0 or roi_height <= 0:
            raise ValueError(f"ROI must have nonnegative origin and positive size: {value!r}")
        if x + roi_width > width or y + roi_height > height:
            raise ValueError(f"ROI is outside {width}x{height} image: {value!r}")
        result.append({
            "name": f"roi-{index}", "x": x, "y": y,
            "width": roi_width, "height": roi_height,
        })
    return result


def _channel_samples(
    pixels: list[float], width: int, height: int,
    region: dict[str, int | str], channel: int
) -> list[float]:
    samples: list[float] = []
    x0 = int(region["x"])
    y0 = int(region["y"])
    region_width = int(region["width"])
    region_height = int(region["height"])
    for y in range(y0, y0 + region_height):
        # PFM stores rows bottom-to-top; report coordinates use top-left origin.
        source_y = y
        row_start = ((height - 1 - source_y) * width + x0) * 3
        for x_offset in range(region_width):
            samples.append(pixels[row_start + x_offset * 3 + channel])
    return samples


def _distribution(samples: list[float]) -> dict[str, float | int | None]:
    finite = [value for value in samples if math.isfinite(value)]
    if not finite:
        return {
            "sampleCount": len(samples), "nonFiniteCount": len(samples),
            "min": None, "max": None, "mean": None,
            "p50": None, "p95": None, "p99": None,
        }
    ordered = sorted(finite)
    return {
        "sampleCount": len(samples),
        "nonFiniteCount": len(samples) - len(finite),
        "min": ordered[0],
        "max": ordered[-1],
        "mean": sum(finite) / len(finite),
        "p50": _percentile(ordered, 0.50),
        "p95": _percentile(ordered, 0.95),
        "p99": _percentile(ordered, 0.99),
    }


def _histogram(samples: list[float], bins: int, minimum: float,
               maximum: float) -> dict[str, object]:
    counts = [0] * bins
    finite = [value for value in samples if math.isfinite(value)]
    if not finite:
        return {"minimum": minimum, "maximum": maximum, "counts": counts,
                "nonFiniteCount": len(samples)}
    if maximum <= minimum:
        counts[0] = len(finite)
    else:
        scale = bins / (maximum - minimum)
        for value in finite:
            index = int((value - minimum) * scale)
            counts[min(max(index, 0), bins - 1)] += 1
    return {"minimum": minimum, "maximum": maximum, "counts": counts,
            "nonFiniteCount": len(samples) - len(finite)}


def _difference(
    baseline: list[float], candidate: list[float], width: int, height: int,
    region: dict[str, int | str], threshold: float | None,
) -> dict[str, object]:
    channels: list[dict[str, object]] = []
    all_errors: list[float] = []
    pixel_max_errors: list[float] = []
    non_finite_pixel_pair_count = 0
    x0 = int(region["x"])
    y0 = int(region["y"])
    region_width = int(region["width"])
    region_height = int(region["height"])
    for channel in range(3):
        errors: list[float] = []
        for y in range(y0, y0 + region_height):
            base_row = ((height - 1 - y) * width + x0) * 3
            for x_offset in range(region_width):
                index = base_row + x_offset * 3 + channel
                left = baseline[index]
                right = candidate[index]
                if math.isfinite(left) and math.isfinite(right):
                    errors.append(abs(right - left))
        ordered = sorted(errors)
        channel_result: dict[str, object] = {
            "channel": "RGB"[channel],
            "sampleCount": len(errors),
            "nonFinitePairCount": region_width * region_height - len(errors),
            "mae": sum(errors) / len(errors) if errors else None,
            "rmse": math.sqrt(sum(value * value for value in errors) / len(errors))
            if errors else None,
            "p99AbsoluteError": _percentile(ordered, 0.99),
            "maxAbsoluteError": ordered[-1] if ordered else None,
        }
        if threshold is not None:
            channel_result["threshold"] = threshold
            channel_result["thresholdExceedanceRate"] = (
                sum(value > threshold for value in errors) / len(errors)
                if errors else None
            )
        channels.append(channel_result)
        all_errors.extend(errors)

    for y in range(y0, y0 + region_height):
        base_row = ((height - 1 - y) * width + x0) * 3
        for x_offset in range(region_width):
            index = base_row + x_offset * 3
            pair = [
                (baseline[index + channel], candidate[index + channel])
                for channel in range(3)
            ]
            if not all(math.isfinite(left) and math.isfinite(right)
                       for left, right in pair):
                non_finite_pixel_pair_count += 1
                continue
            pixel_max_errors.append(max(abs(right - left) for left, right in pair))

    all_errors.sort()
    pixel_max_errors.sort()
    result: dict[str, object] = {
        "channels": channels,
        "rgbSampleCount": len(all_errors),
        "rgbMae": sum(all_errors) / len(all_errors) if all_errors else None,
        "rgbRmse": math.sqrt(sum(value * value for value in all_errors) / len(all_errors))
        if all_errors else None,
        "rgbP99AbsoluteError": _percentile(all_errors, 0.99),
        "rgbMaxAbsoluteError": all_errors[-1] if all_errors else None,
        "pixelMaxChannelError": {
            "sampleCount": len(pixel_max_errors),
            "nonFinitePairCount": non_finite_pixel_pair_count,
            "mae": sum(pixel_max_errors) / len(pixel_max_errors)
            if pixel_max_errors else None,
            "rmse": math.sqrt(sum(value * value for value in pixel_max_errors) /
                               len(pixel_max_errors))
            if pixel_max_errors else None,
            "p99AbsoluteError": _percentile(pixel_max_errors, 0.99),
            "maxAbsoluteError": pixel_max_errors[-1] if pixel_max_errors else None,
        },
    }
    if threshold is not None:
        result["pixelMaxChannelError"]["threshold"] = threshold
        result["pixelMaxChannelError"]["thresholdExceedanceRate"] = (
            sum(value > threshold for value in pixel_max_errors) /
            len(pixel_max_errors) if pixel_max_errors else None
        )
    return result


def _write_rgb_pfm(path: Path, width: int, height: int,
                   pfm_order_pixels: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        stream.write(f"PF\n{width} {height}\n{'-1.0' if sys.byteorder == 'little' else '1.0'}\n".encode("ascii"))
        row_values = width * 3
        # Input data uses PFM's native bottom-up storage order.
        for y in range(height):
            start = y * row_values
            row = array("f", pfm_order_pixels[start:start + row_values])
            stream.write(row.tobytes())


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path, help="baseline RGB PFM capture")
    parser.add_argument("candidate", type=Path, help="candidate RGB PFM capture")
    parser.add_argument("--roi", action="append", default=[], metavar="X,Y,W,H",
                        help="add a top-left-origin ROI; may be repeated")
    parser.add_argument("--threshold", type=float,
                        help="optional absolute-error threshold for exceedance rates")
    parser.add_argument("--histogram-bins", type=int, default=32,
                        help="optional per-channel histogram bin count (0 disables; default: 32)")
    parser.add_argument("--diff-output", type=Path,
                        help="optional RGB PFM containing absolute per-pixel difference")
    parser.add_argument("--output", type=Path,
                        help="write JSON report here instead of stdout")
    args = parser.parse_args()

    if args.threshold is not None and (not math.isfinite(args.threshold) or args.threshold < 0):
        parser.error("--threshold must be a finite nonnegative value")
    if args.histogram_bins < 0 or args.histogram_bins > 4096:
        parser.error("--histogram-bins must be between 0 and 4096")
    input_paths = {args.baseline.resolve(), args.candidate.resolve()}
    output_paths = {
        path.resolve() for path in (args.output, args.diff_output)
        if path is not None
    }
    if len(output_paths) != sum(path is not None for path in (args.output, args.diff_output)):
        parser.error("--output and --diff-output must use different paths")
    if output_paths & input_paths:
        parser.error("analysis outputs must not overwrite either input capture")
    try:
        width, height, baseline = read_rgb_pfm(args.baseline)
        candidate_width, candidate_height, candidate = read_rgb_pfm(args.candidate)
        if (width, height) != (candidate_width, candidate_height):
            raise ValueError(
                f"dimension mismatch: baseline {width}x{height}, "
                f"candidate {candidate_width}x{candidate_height}"
            )
        regions = _regions(width, height, args.roi)
    except (OSError, ValueError, OverflowError) as error:
        parser.error(str(error))

    report: dict[str, object] = {
        "tool": "analyze_shader_graph_pfm.py",
        "measurementSpace": "linear RGB values stored in RGB PFM; no display transform or clamp",
        "baseline": {"path": str(args.baseline), "sha256": _sha256(args.baseline)},
        "candidate": {"path": str(args.candidate), "sha256": _sha256(args.candidate)},
        "dimensions": {"width": width, "height": height},
        "threshold": args.threshold,
        "regions": [],
        "limitations": [
            "PFM stores RGB only; alpha is not compared.",
            "No quality verdict is inferred from these statistics.",
            "Images must share scene, camera, color, alpha, and backend conditions.",
        ],
    }
    region_reports = report["regions"]
    assert isinstance(region_reports, list)
    for region in regions:
        baseline_channels = {
            "RGB"[channel]: _channel_samples(
                baseline, width, height, region, channel)
            for channel in range(3)
        }
        candidate_channels = {
            "RGB"[channel]: _channel_samples(
                candidate, width, height, region, channel)
            for channel in range(3)
        }
        baseline_distributions = {
            channel: _distribution(samples)
            for channel, samples in baseline_channels.items()
        }
        candidate_distributions = {
            channel: _distribution(samples)
            for channel, samples in candidate_channels.items()
        }
        histograms: dict[str, object] = {}
        if args.histogram_bins:
            for channel in "RGB":
                finite_values = [
                    value for samples in
                    (baseline_channels[channel], candidate_channels[channel])
                    for value in samples if math.isfinite(value)
                ]
                if finite_values:
                    minimum, maximum = min(finite_values), max(finite_values)
                    if minimum == maximum:
                        # Keep a useful, common interval for constant image pairs.
                        padding = max(abs(minimum) * 1e-6, 1e-6)
                        minimum -= padding
                        maximum += padding
                else:
                    minimum, maximum = 0.0, 1.0
                histograms[channel] = {
                    "baseline": _histogram(
                        baseline_channels[channel], args.histogram_bins,
                        minimum, maximum),
                    "candidate": _histogram(
                        candidate_channels[channel], args.histogram_bins,
                        minimum, maximum),
                }
        region_reports.append({
            "region": region,
            "baselineDistribution": baseline_distributions,
            "candidateDistribution": candidate_distributions,
            "histograms": histograms if args.histogram_bins else None,
            "difference": _difference(
                baseline, candidate, width, height, region, args.threshold),
        })

    if args.diff_output:
        difference_pixels: list[float] = []
        for left, right in zip(baseline, candidate):
            if math.isfinite(left) and math.isfinite(right):
                difference = abs(right - left)
                difference_pixels.append(
                    difference if math.isfinite(difference) else float("nan"))
            else:
                difference_pixels.append(float("nan"))
        _write_rgb_pfm(args.diff_output, width, height, difference_pixels)
        report["differencePfm"] = str(args.diff_output)

    encoded = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        sys.stdout.write(encoded)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
