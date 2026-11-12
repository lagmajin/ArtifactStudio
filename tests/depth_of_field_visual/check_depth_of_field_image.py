#!/usr/bin/env python3
"""Capture the headless GPU DoF fixture and verify focus-plane sharpness."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib


def read_ppm(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    tokens: list[bytes] = []
    cursor = 0
    while len(tokens) < 4:
        while cursor < len(data) and data[cursor] in b" \t\r\n":
            cursor += 1
        if cursor < len(data) and data[cursor] == ord("#"):
            while cursor < len(data) and data[cursor] not in b"\r\n":
                cursor += 1
            continue
        start = cursor
        while cursor < len(data) and data[cursor] not in b" \t\r\n":
            cursor += 1
        tokens.append(data[start:cursor])
    if tokens[0] != b"P6" or int(tokens[3]) != 255:
        raise ValueError(f"Unsupported PPM format in {path}")
    if cursor >= len(data) or data[cursor] not in b" \t\r\n":
        raise ValueError(f"Missing PPM header separator in {path}")
    separator = data[cursor]
    cursor += 1
    if separator == ord("\r") and cursor < len(data) and data[cursor] == ord("\n"):
        cursor += 1
    width, height = int(tokens[1]), int(tokens[2])
    payload = data[cursor:]
    if len(payload) != width * height * 3:
        raise ValueError(f"Invalid PPM pixel payload in {path}")
    pixels = [tuple(payload[i : i + 3]) for i in range(0, len(payload), 3)]
    return width, height, pixels


def write_png(path: Path, width: int, height: int,
              pixels: list[tuple[int, int, int]]) -> None:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))

    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for red, green, blue in pixels[y * width : (y + 1) * width]:
            rows.extend((red, green, blue))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), level=6))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def contrast(pixels: list[tuple[int, int, int]], width: int, y: int) -> float:
    bright: list[float] = []
    dark: list[float] = []
    for x in range(32, width - 32):
        red, green, blue = pixels[y * width + x]
        luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue
        (bright if (x // 4) % 2 == 0 else dark).append(luminance)
    return abs(sum(bright) / len(bright) - sum(dark) / len(dark))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-exe", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    executable = args.test_exe.resolve()
    if not executable.is_file():
        parser.error(f"test executable does not exist: {executable}")

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    environment["ARTIFACT_DOF_IMAGE_DIR"] = str(output_dir)
    run = subprocess.run(
        [str(executable), "--gtest_filter=PostProcessPassGpuContractTest.DepthOfFieldWritesLayeredVisualCapture"],
        env=environment,
        check=False,
    )
    if run.returncode:
        return run.returncode

    input_path = output_dir / "dof-input.ppm"
    output_path = output_dir / "dof-output.ppm"
    if not input_path.is_file() or not output_path.is_file():
        raise RuntimeError(
            "The selected GPU test exited without writing images. Check whether "
            "its Vulkan device initialized and the capture test ran."
        )
    width, height, source = read_ppm(input_path)
    output_width, output_height, rendered = read_ppm(output_path)
    if (width, height) != (160, 96) or (output_width, output_height) != (width, height):
        raise ValueError("Unexpected DoF visual fixture dimensions")

    focus_y, near_y, far_y = height // 2, height // 6, height * 5 // 6
    input_contrast = {
        "near": contrast(source, width, near_y),
        "focus": contrast(source, width, focus_y),
        "far": contrast(source, width, far_y),
    }
    output_contrast = {
        "near": contrast(rendered, width, near_y),
        "focus": contrast(rendered, width, focus_y),
        "far": contrast(rendered, width, far_y),
    }
    retention = {
        plane: output_contrast[plane] / max(input_contrast[plane], 1e-6)
        for plane in ("near", "focus", "far")
    }
    changed_pixels = sum(a != b for a, b in zip(source, rendered))
    checks = {
        "GPU output differs from the input": changed_pixels > 0,
        "known focus plane retains more chart contrast than near plane":
            retention["focus"] > retention["near"],
        "known focus plane retains more chart contrast than far plane":
            retention["focus"] > retention["far"],
    }

    sheet_width = width * 2
    sheet = [(0, 0, 0)] * (sheet_width * height)
    for y in range(height):
        source_row = source[y * width : (y + 1) * width]
        output_row = rendered[y * width : (y + 1) * width]
        row_start = y * sheet_width
        sheet[row_start : row_start + width] = source_row
        sheet[row_start + width : row_start + sheet_width] = output_row
    contact_path = output_dir / "dof-gpu-comparison.png"
    write_png(contact_path, sheet_width, height, sheet)
    metrics = {
        "capture_kind": "headless Vulkan GPU execution of Artifact DepthOfFieldPass",
        "gpu": "NVIDIA GeForce RTX 4070 Ti",
        "fixture": "three horizontal metric-depth planes with alternating chart bars",
        "width": width,
        "height": height,
        "focus_distance": 10.0,
        "near_clip": 1.0,
        "far_clip": 100.0,
        "max_coc_radius_px": 12.0,
        "sample_count": 64,
        "planes": {"near": 4.0, "focus": 10.0, "far": 30.0},
        "input_contrast": input_contrast,
        "output_contrast": output_contrast,
        "contrast_retention": retention,
        "changed_pixels": changed_pixels,
        "checks": checks,
        "comparison_png": contact_path.name,
        "layout": "left=input, right=GPU DoF output",
        "limitations": [
            "synthetic flat-depth fixture, not a production 3D camera render",
            "relative chart contrast is an aliasing-sensitive diagnostic, not a perceptual sharpness score",
            "does not evaluate aperture-shaped bokeh, transparency, temporal stability, or GPU timing",
            "PPM capture is clamped to display range; HDR headroom is not represented",
        ],
    }
    metrics_path = output_dir / "dof-gpu-metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    for label, passed in checks.items():
        print(f"{'PASS' if passed else 'FAIL'}: {label}")
    print(f"Focus contrast retention: {retention['focus']:.4f}")
    print(f"Near contrast retention:  {retention['near']:.4f}")
    print(f"Far contrast retention:   {retention['far']:.4f}")
    print(f"GPU comparison image: {contact_path}")
    print(f"Metrics: {metrics_path}")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
