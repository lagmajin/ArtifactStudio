#!/usr/bin/env python3
"""Generate deterministic CPU-only RGBA32F and display images for the Ramp fixture."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

from generate_dynamic_ramp_golden import evaluate, f32


WIDTH = 256
HEIGHT = 256
SWATCH_FACTORS = (0.0, 0.4999, 0.5, 0.5001, 1.0)
OUTPUT_DIRECTORY = Path("tests/fixtures/shader-graph-rd/dynamic-ramp-image-oracle")


def png_chunk(kind: bytes, data: bytes) -> bytes:
    payload = kind + data
    return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF)


def deterministic_zlib_store(data: bytes) -> bytes:
    """Use stored DEFLATE blocks so output bytes do not depend on zlib compression heuristics."""
    result = bytearray(b"\x78\x01")
    for offset in range(0, len(data), 65535):
        block = data[offset:offset + 65535]
        final = 1 if offset + len(block) == len(data) else 0
        result.extend(struct.pack("<BHH", final, len(block), 0xFFFF ^ len(block)))
        result.extend(block)
    result.extend(struct.pack(">I", zlib.adler32(data) & 0xFFFFFFFF))
    return bytes(result)


def linear_to_srgb(value: float) -> float:
    value = min(max(value, 0.0), 1.0)
    return 12.92 * value if value <= 0.0031308 else 1.055 * (value ** (1.0 / 2.4)) - 0.055


def make_png(width: int, height: int, pixels: list[tuple[float, float, float, float]], background: float) -> bytes:
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter: None
        for red, green, blue, alpha in pixels[y * width:(y + 1) * width]:
            for channel in (red * alpha + background * (1.0 - alpha),
                            green * alpha + background * (1.0 - alpha),
                            blue * alpha + background * (1.0 - alpha)):
                rows.append(round(linear_to_srgb(channel) * 255.0))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header) +
            png_chunk(b"IDAT", deterministic_zlib_store(bytes(rows))) + png_chunk(b"IEND", b""))


def channel_statistics(pixels: list[tuple[float, float, float, float]]) -> dict[str, object]:
    channels: dict[str, object] = {}
    for index, name in enumerate(("r", "g", "b", "a")):
        values = sorted(pixel[index] for pixel in pixels)
        finite_values = [value for value in values if math.isfinite(value)]

        def percentile(fraction: float) -> float | None:
            if not finite_values:
                return None
            return finite_values[round((len(finite_values) - 1) * fraction)]

        channels[name] = {
            "minimum": min(finite_values) if finite_values else None,
            "maximum": max(finite_values) if finite_values else None,
            "mean": sum(finite_values) / len(finite_values) if finite_values else None,
            "p50": percentile(0.50), "p95": percentile(0.95), "p99": percentile(0.99),
            "nonFiniteCount": len(values) - len(finite_values),
            "outsideUnitRangeCount": sum(value < 0.0 or value > 1.0 for value in finite_values),
        }
    return channels


def write_artifact(repo_root: Path, output_directory: Path, name: str,
                   pixels: list[tuple[float, float, float, float]]) -> dict[str, object]:
    raw = b"".join(struct.pack("<4f", *pixel) for pixel in pixels)
    raw_path = output_directory / f"{name}.rgba32f"
    raw_path.write_bytes(raw)
    display_paths = {}
    for background_name, background in (("black", 0.0), ("white", 1.0)):
        png_path = output_directory / f"{name}-over-{background_name}.png"
        png_path.write_bytes(make_png(WIDTH, HEIGHT, pixels, background))
        display_paths[background_name] = {
            "path": png_path.relative_to(repo_root).as_posix(),
            "sha256": hashlib.sha256(png_path.read_bytes()).hexdigest().upper(),
        }
    return {
        "rawPath": raw_path.relative_to(repo_root).as_posix(),
        "rawSha256": hashlib.sha256(raw).hexdigest().upper(),
        "rawBytes": len(raw),
        "width": WIDTH,
        "height": HEIGHT,
        "channelOrder": "RGBA",
        "dtype": "IEEE-754 binary32 little-endian",
        "statistics": channel_statistics(pixels),
        "displayPngs": display_paths,
    }


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    manifest_path = root / "tests/fixtures/shader-graph-rd/evaluation_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    contract = manifest["coreTypedOperationSchemaProposal"]["dynamicGraphPrototype"]
    plan = contract["rampImageEvaluationPlan"]
    document_path = root / plan["fixture"]
    document_bytes = document_path.read_bytes()
    document = json.loads(document_bytes.decode("utf-8"))
    stops = next(node["parameters"]["stops"] for node in document["nodes"] if node["nodeId"] == "ramp")
    pixels = [evaluate(stops, f32(x / (WIDTH - 1)))[0] for y in range(HEIGHT) for x in range(WIDTH)]
    gradient = [tuple(pixel) for pixel in pixels]
    atlas_factors = [SWATCH_FACTORS[min((x * len(SWATCH_FACTORS)) // WIDTH, len(SWATCH_FACTORS) - 1)]
                     for y in range(HEIGHT) for x in range(WIDTH)]
    atlas = [tuple(evaluate(stops, factor)[0]) for factor in atlas_factors]
    output_directory = root / OUTPUT_DIRECTORY
    output_directory.mkdir(parents=True, exist_ok=True)
    artifacts = {
        "gradient": write_artifact(root, output_directory, "gradient", gradient),
        "swatchAtlas": write_artifact(root, output_directory, "swatch-atlas", atlas),
    }
    seam_left = gradient[HEIGHT // 2 * WIDTH + (WIDTH // 2 - 1)]
    seam_right = gradient[HEIGHT // 2 * WIDTH + (WIDTH // 2)]
    swatch_samples = []
    for index, factor in enumerate(SWATCH_FACTORS):
        start_x = index * WIDTH // len(SWATCH_FACTORS)
        end_x = (index + 1) * WIDTH // len(SWATCH_FACTORS)
        center_x = (start_x + end_x) // 2
        color = atlas[HEIGHT // 2 * WIDTH + center_x]
        swatch_samples.append({"factor": factor, "rgba": list(color),
                               "selection": evaluate(stops, factor)[1]})
    report = {
        "schemaVersion": 1,
        "kind": "artifact-shader-graph-ramp-cpu-image-oracle",
        "status": "cpu-formula-image-only-no-core-hlsl-runtime-or-backend-capture",
        "sourceDocument": plan["fixture"],
        "sourceDocumentSha256": hashlib.sha256(document_bytes).hexdigest().upper(),
        "generator": "tools/shader_graph_rd/generate_dynamic_ramp_image_oracle.py",
        "generatorFormula": "tools/shader_graph_rd/generate_dynamic_ramp_golden.py",
        "width": WIDTH,
        "height": HEIGHT,
        "gradientMapping": "UV0.x = x / (width - 1), repeated across rows",
        "swatchFactors": list(SWATCH_FACTORS),
        "alphaDisplay": "linear RGB composited over black and white, then sRGB encoded for PNG only",
        "artifacts": artifacts,
        "swatchCenterSamples": swatch_samples,
        "gradientCenterSeamAdjacentDelta": [f32(seam_right[index] - seam_left[index]) for index in range(4)],
        "limitations": [
            "CPU formula output is not an HLSL compiler, GPU, Artifact renderer, legacy ColorRamp, or Blender capture",
            "PNG output is a display view; raw RGBA32F files are authoritative numeric data",
        ],
    }
    report_bytes = (json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8")
    report_path = output_directory / "report.json"
    report_path.write_bytes(report_bytes)
    plan["status"] = "cpu-formula-image-generated-runtime-unverified"
    plan["captureRoute"] = "cpu-formula-image-oracle-only-core-hlsl-runtime-unimplemented"
    plan["generatedOracle"] = {
        "report": report_path.relative_to(root).as_posix(),
        "reportSha256": hashlib.sha256(report_bytes).hexdigest().upper(),
        "generator": "tools/shader_graph_rd/generate_dynamic_ramp_image_oracle.py",
        "sourceDocumentSha256": report["sourceDocumentSha256"],
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    print(f"Updated {plan['generatedOracle']['report']} ({plan['generatedOracle']['reportSha256']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
