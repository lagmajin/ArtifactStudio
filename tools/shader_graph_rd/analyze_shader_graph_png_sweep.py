#!/usr/bin/env python3
"""Summarize multi-value Shader Graph response captures from 8-bit PNGs."""

from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import zlib

from analyze_shader_graph_png import (
    _crop_pixels,
    _measure,
    _parse_roi,
    _rgb_bytes,
    _rgb_difference,
    _sha256,
    read_png,
)


def _parse_sample(value: str) -> tuple[str, float, Path]:
    parameter_and_value, separator, path_value = value.partition(":")
    if not separator or "=" not in parameter_and_value or not path_value:
        raise ValueError(
            f"invalid sample {value!r}; expected PARAMETER=VALUE:PNG_PATH"
        )
    parameter, raw_value = parameter_and_value.split("=", 1)
    if not parameter.strip():
        raise ValueError(f"sample parameter name is empty: {value!r}")
    try:
        numeric_value = float(raw_value)
    except ValueError as error:
        raise ValueError(f"invalid sample value in {value!r}") from error
    if not math.isfinite(numeric_value):
        raise ValueError(f"sample value must be finite: {value!r}")
    return parameter.strip(), numeric_value, Path(path_value)


def _samples_from_manifest(
    manifest_path: Path, route_name: str, capture_dir: Path,
) -> tuple[list[str], dict[tuple[str, float, Path], dict[str, object]], dict[str, object]]:
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes.decode("utf-8"))
    routes = manifest.get("captureRoutes")
    route = routes.get(route_name) if isinstance(routes, dict) else None
    if not isinstance(route, dict):
        raise ValueError(f"capture route not found in manifest: {route_name}")
    parameter_samples = route.get("parameterSamples")
    fixtures = manifest.get("fixtures")
    if not isinstance(parameter_samples, dict) or not isinstance(fixtures, list):
        raise ValueError(f"capture route has no structured parameter samples: {route_name}")
    fixture_entries = {
        entry.get("file"): entry for entry in fixtures
        if isinstance(entry, dict) and isinstance(entry.get("file"), str)
    }
    fixture_root = manifest_path.resolve().parent
    parameter_sockets = {
        "metallic": "Metallic",
        "roughness": "Roughness",
        "emissionStrength": "EmissionStrength",
    }
    if set(parameter_samples) != set(parameter_sockets):
        raise ValueError("manifest route parameter sample set is incomplete or unexpected")
    capture_names_value = route.get("captureNames")
    if not isinstance(capture_names_value, list) or any(
        not isinstance(name, str) for name in capture_names_value
    ):
        raise ValueError("manifest route captureNames must be an array of strings")
    capture_names = set(capture_names_value)
    samples: list[str] = []
    provenance: dict[tuple[str, float, Path], dict[str, object]] = {}
    for parameter, socket in parameter_sockets.items():
        points = parameter_samples.get(parameter)
        if not isinstance(points, list) or len(points) < 2:
            raise ValueError(f"route parameter {parameter} must have at least two samples")
        for point in points:
            if not isinstance(point, dict):
                raise ValueError(f"route sample for {parameter} must be an object")
            value = point.get("value")
            fixture_name = point.get("fixture")
            capture_name = point.get("captureName")
            if isinstance(value, bool) or not isinstance(value, (int, float)) or \
                    not math.isfinite(float(value)):
                raise ValueError(f"route sample value for {parameter} must be finite numeric")
            if not isinstance(fixture_name, str) or not isinstance(capture_name, str):
                raise ValueError(f"route sample for {parameter} needs fixture and captureName")
            if capture_name not in capture_names:
                raise ValueError(f"capture name is not declared in route: {capture_name}")
            fixture_entry = fixture_entries.get(fixture_name)
            if not isinstance(fixture_entry, dict):
                raise ValueError(f"fixture is not declared in manifest: {fixture_name}")
            fixture_path = (fixture_root / fixture_name).resolve()
            try:
                fixture_path.relative_to(fixture_root)
            except ValueError as error:
                raise ValueError(f"fixture path escapes manifest directory: {fixture_name}") from error
            fixture_bytes = fixture_path.read_bytes()
            fixture_sha = hashlib.sha256(fixture_bytes).hexdigest().upper()
            expected_sha = fixture_entry.get("sha256")
            if not isinstance(expected_sha, str) or fixture_sha != expected_sha.upper():
                raise ValueError(f"fixture SHA-256 differs from manifest: {fixture_name}")
            expected_surface_input = fixture_entry.get("expectedRasterSurfaceInput")
            if not isinstance(expected_surface_input, dict):
                raise ValueError(f"fixture has no expected Raster input metadata: {fixture_name}")
            fixture_payload = json.loads(fixture_bytes.decode("utf-8"))
            if not isinstance(fixture_payload, dict):
                raise ValueError(f"fixture root must be an object: {fixture_name}")
            nodes = fixture_payload.get("nodes")
            links = fixture_payload.get("links")
            if not isinstance(nodes, list) or not isinstance(links, list):
                raise ValueError(f"fixture nodes/links are invalid: {fixture_name}")
            nodes_by_id = {
                node.get("id"): node for node in nodes
                if isinstance(node, dict) and isinstance(node.get("id"), str)
            }
            parameter_link = next((link for link in links if
                isinstance(link, dict) and link.get("to") == "surface" and
                link.get("in") == socket and link.get("from") == "parameter_value" and
                link.get("out") == "Value"), None)
            parameter_node = nodes_by_id.get("parameter_value")
            if parameter_link is None or not isinstance(parameter_node, dict) or \
                    parameter_node.get("type") != "Value" or \
                    not isinstance(parameter_node.get("params"), dict) or \
                    parameter_node["params"].get("value") != value:
                raise ValueError(
                    f"fixture socket value does not match route sample {parameter}={value}: {fixture_name}"
                )
            capture_path = capture_dir / f"{capture_name}.png"
            sample_spec = f"{parameter}={float(value):g}:{capture_path}"
            parsed_parameter, parsed_value, parsed_path = _parse_sample(sample_spec)
            provenance[(parsed_parameter, parsed_value, parsed_path.resolve())] = {
                "fixture": fixture_name,
                "fixturePath": str(fixture_path),
                "fixtureSha256": fixture_sha,
                "captureName": capture_name,
                "expectedRasterSurfaceInput": expected_surface_input,
            }
            samples.append(sample_spec)
    return samples, provenance, {
        "manifestPath": str(manifest_path.resolve()),
        "manifestSha256": hashlib.sha256(manifest_bytes).hexdigest().upper(),
        "captureRoute": route_name,
        "fixturePaths": sorted({
            str(item["fixturePath"])
            for item in provenance.values()
        }),
    }


def _summary(width: int, height: int, channels: int,
             pixels: bytes) -> dict[str, object]:
    measured = _measure(width, height, channels, pixels)
    rgb = measured["displayReferredRGB"]
    summarized_rgb = {
        channel: {key: value for key, value in stats.items()
                  if key != "histogram256"}
        for channel, stats in rgb.items()
    }
    alpha = measured.get("alpha")
    if isinstance(alpha, dict):
        alpha = {key: value for key, value in alpha.items()
                 if key != "histogram256"}
    return {
        "displayReferredRGB": summarized_rgb,
        "anyRGBChannelAtEndpoint": measured["anyRGBChannelAtEndpoint"],
        "alpha": alpha,
        "alphaCoverage": measured.get("alphaCoverage"),
    }


def _difference(left: bytes, right: bytes, channels: int) -> dict[str, object]:
    result: dict[str, object] = {
        "storedRGB": _rgb_difference(_rgb_bytes(left, channels),
                                     _rgb_bytes(right, channels)),
    }
    if channels == 4:
        alpha_left = left[3::4]
        alpha_right = right[3::4]
        absolute = [abs(a - b) for a, b in zip(alpha_left, alpha_right)]
        result["storedAlpha"] = {
            "sampleCount": len(absolute),
            "mae": sum(absolute) / (255.0 * len(absolute)),
            "maxAbsoluteError": max(absolute) / 255.0,
            "changedPixelRate": sum(error != 0 for error in absolute) / len(absolute),
        }
    return result


def _write_png(path: Path, width: int, height: int, channels: int,
               pixels: bytes) -> str:
    if channels not in (3, 4) or len(pixels) != width * height * channels:
        raise ValueError("crop pixel payload does not match PNG dimensions/channels")
    color_type = 2 if channels == 3 else 6
    row_bytes = width * channels
    raw = bytearray(height * (row_bytes + 1))
    for row in range(height):
        target = row * (row_bytes + 1)
        source = row * row_bytes
        raw[target] = 0
        raw[target + 1:target + 1 + row_bytes] = pixels[source:source + row_bytes]

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (struct.pack(">I", len(payload)) + kind + payload +
                struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    encoded = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
               chunk(b"IDAT", zlib.compress(bytes(raw), level=6)) +
               chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)
    return _sha256(path)


def _safe_component(value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9_-]+", "_", value).strip("_")
    return normalized or "region"


def analyze(samples: list[str], roi_values: list[str],
            crop_output_dir: Path | None = None,
            report_output: Path | None = None,
            sample_provenance: dict[tuple[str, float, Path], dict[str, object]] | None = None,
            analysis_provenance: dict[str, object] | None = None) -> dict[str, object]:
    curves: dict[str, list[tuple[float, Path]]] = defaultdict(list)
    input_paths: set[Path] = set()
    for value in samples:
        parameter, numeric_value, path = _parse_sample(value)
        resolved_path = path.resolve()
        if resolved_path in input_paths:
            raise ValueError(f"PNG input is repeated: {path}")
        input_paths.add(resolved_path)
        curves[parameter].append((numeric_value, path))
    if not curves:
        raise ValueError("at least two --sample inputs are required")

    decoded: dict[Path, tuple[int, int, int, bytes]] = {}
    dimensions: tuple[int, int, int] | None = None
    for curve_samples in curves.values():
        if len(curve_samples) < 2:
            raise ValueError("each parameter curve requires at least two samples")
        curve_samples.sort(key=lambda item: item[0])
        values = [value for value, _ in curve_samples]
        if len(values) != len(set(values)):
            raise ValueError("parameter values must be unique within each curve")
        for _, path in curve_samples:
            if path in decoded:
                continue
            width, height, channels, pixels = read_png(path)
            candidate_dimensions = (width, height, channels)
            if dimensions is None:
                dimensions = candidate_dimensions
            elif dimensions != candidate_dimensions:
                raise ValueError("all sweep captures must share dimensions and RGB/RGBA format")
            decoded[path] = (width, height, channels, pixels)
    assert dimensions is not None
    width, height, channels = dimensions

    regions: list[dict[str, int | str]] = [
        {"name": "full-frame", "x": 0, "y": 0,
         "width": width, "height": height}
    ]
    region_names = {"full-frame"}
    for roi_value in roi_values:
        region = _parse_roi(roi_value, width, height)
        name = str(region["name"])
        if name in region_names:
            raise ValueError(f"duplicate ROI name: {name}")
        region_names.add(name)
        regions.append(region)

    planned_crop_paths: dict[tuple[str, int, str], Path] = {}
    if crop_output_dir is not None:
        output_paths: set[Path] = set()
        for parameter, curve_samples in sorted(curves.items()):
            for sample_index, _ in enumerate(curve_samples, start=1):
                for region in regions:
                    name = str(region["name"])
                    if name == "full-frame":
                        continue
                    output_name = (
                        f"{_safe_component(parameter)}-{sample_index:02d}-"
                        f"{_safe_component(name)}.png"
                    )
                    crop_path = (crop_output_dir / output_name).resolve()
                    if crop_path in input_paths:
                        raise ValueError(f"crop output would overwrite an input PNG: {crop_path}")
                    if report_output is not None and crop_path == report_output.resolve():
                        raise ValueError(f"crop output would overwrite the JSON report: {crop_path}")
                    if crop_path in output_paths:
                        raise ValueError(f"crop output name collision: {crop_path}")
                    if crop_path.exists():
                        raise ValueError(f"crop output already exists: {crop_path}")
                    output_paths.add(crop_path)
                    planned_crop_paths[(parameter, sample_index, name)] = crop_path

    crop_paths: set[Path] = set()
    crop_artifacts: list[dict[str, str]] = []
    curve_reports: dict[str, object] = {}
    for parameter, curve_samples in sorted(curves.items()):
        image_reports: list[dict[str, object]] = []
        for sample_index, (numeric_value, path) in enumerate(curve_samples, start=1):
            _, _, _, pixels = decoded[path]
            image_report: dict[str, object] = {
                "value": numeric_value,
                "path": str(path),
                "sha256": _sha256(path),
                "regions": {},
            }
            if sample_provenance is not None:
                fixture_metadata = sample_provenance.get(
                    (parameter, numeric_value, path.resolve())
                )
                if fixture_metadata is None:
                    raise ValueError(
                        f"missing graph-fixture provenance for {parameter}={numeric_value}: {path}"
                    )
                image_report["graphFixture"] = fixture_metadata
            for region in regions:
                name = str(region["name"])
                cropped = _crop_pixels(width, height, channels, pixels, region)
                region_result: dict[str, object] = {
                    "bounds": {key: region[key]
                               for key in ("x", "y", "width", "height")},
                    "statistics": _summary(int(region["width"]),
                                           int(region["height"]),
                                           channels, cropped),
                }
                if crop_output_dir is not None and name != "full-frame":
                    crop_path = planned_crop_paths[(parameter, sample_index, name)]
                    crop_paths.add(crop_path)
                    crop_hash = _write_png(
                        crop_path, int(region["width"]), int(region["height"]),
                        channels, cropped
                    )
                    region_result["cropArtifact"] = {
                        "path": str(crop_path),
                        "sha256": crop_hash,
                        "encoding": "lossless 8-bit RGB/RGBA PNG crop of source bytes",
                    }
                    crop_artifacts.append({"path": str(crop_path), "sha256": crop_hash})
                image_report["regions"][name] = region_result
            image_reports.append(image_report)

        adjacent_reports: list[dict[str, object]] = []
        for index in range(len(curve_samples) - 1):
            left_value, left_path = curve_samples[index]
            right_value, right_path = curve_samples[index + 1]
            left_pixels = decoded[left_path][3]
            right_pixels = decoded[right_path][3]
            comparisons: dict[str, object] = {}
            for region in regions:
                name = str(region["name"])
                left_crop = _crop_pixels(width, height, channels, left_pixels, region)
                right_crop = _crop_pixels(width, height, channels, right_pixels, region)
                comparisons[name] = _difference(left_crop, right_crop, channels)
            adjacent_reports.append({
                "fromValue": left_value,
                "toValue": right_value,
                "regions": comparisons,
            })
        curve_reports[parameter] = {
            "samples": image_reports,
            "adjacentDifferences": adjacent_reports,
        }

    return {
        "tool": "analyze_shader_graph_png_sweep.py",
        "measurementSpace": (
            "8-bit encoded display-referred RGB and stored PNG alpha; "
            "not scene-linear and not HDR"
        ),
        "dimensions": {"width": width, "height": height, "channels": channels},
        "cropOutputDirectory": (
            str(crop_output_dir.resolve()) if crop_output_dir is not None else None
        ),
        "cropArtifacts": crop_artifacts,
        "captureProvenance": analysis_provenance,
        "curveInterpretation": (
            "sample statistics and adjacent differences only; no monotonicity, "
            "quality verdict, tolerance, or physical correctness is inferred"
        ),
        "curves": curve_reports,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument(
        "--sample", action="append",
        metavar="PARAMETER=VALUE:PNG_PATH",
        help="add a sample; repeat for each point and parameter",
    )
    inputs.add_argument("--manifest", type=Path,
                        help="derive sample fixtures and capture names from a manifest route")
    parser.add_argument("--route", default="productionNumericParameterSweep",
                        help="captureRoutes entry to read (default: %(default)s)")
    parser.add_argument("--capture-dir", type=Path,
                        help="directory containing <captureName>.png files for --manifest")
    parser.add_argument(
        "--roi", action="append", default=[], metavar="NAME:X,Y,W,H",
        help="add a top-left-origin region for per-sample statistics and differences",
    )
    parser.add_argument(
        "--crop-output-dir", type=Path,
        help="write lossless PNG crops for each named ROI and sample",
    )
    parser.add_argument("--output", type=Path, help="write JSON report here")
    args = parser.parse_args()
    if args.manifest:
        if args.capture_dir is None:
            parser.error("--capture-dir is required with --manifest")
        try:
            samples, sample_provenance, analysis_provenance = _samples_from_manifest(
                args.manifest, args.route, args.capture_dir
            )
        except (OSError, ValueError, TypeError, KeyError) as error:
            parser.error(str(error))
    else:
        if args.capture_dir is not None:
            parser.error("--capture-dir requires --manifest")
        samples = args.sample
        sample_provenance = None
        analysis_provenance = None
    if analysis_provenance is not None and args.output is not None:
        protected_paths = {
            Path(str(analysis_provenance["manifestPath"])).resolve(),
            *(Path(str(path)).resolve()
              for path in analysis_provenance.get("fixturePaths", [])),
        }
        if args.output.resolve() in protected_paths:
            parser.error("JSON report output would overwrite the manifest or a graph fixture")
    if args.output:
        try:
            input_paths = {_parse_sample(sample)[2].resolve()
                           for sample in samples}
        except ValueError as error:
            parser.error(str(error))
        if args.output.resolve() in input_paths:
            parser.error("--output must not overwrite a sample PNG")
    try:
        report = analyze(samples, args.roi, args.crop_output_dir, args.output,
                         sample_provenance, analysis_provenance)
    except (OSError, ValueError, OverflowError) as error:
        parser.error(str(error))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + "\n",
                               encoding="utf-8")
    else:
        print(json.dumps(report, indent=2, allow_nan=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
