#!/usr/bin/env python3
"""Run the headless GPU motion-blur fixture and recognize its output image."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-exe", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path,
                        help="Keep generated input/output PPM images here")
    args = parser.parse_args()
    executable = args.test_exe.resolve()
    if not executable.is_file():
        parser.error(f"test executable does not exist: {executable}")

    owned_temp = None
    if args.output_dir:
        output_dir = args.output_dir.resolve()
        output_dir.mkdir(parents=True, exist_ok=True)
    else:
        owned_temp = tempfile.TemporaryDirectory(prefix="artifact-motion-blur-")
        output_dir = Path(owned_temp.name)

    environment = os.environ.copy()
    environment["ARTIFACT_MOTION_BLUR_IMAGE_DIR"] = str(output_dir)

    def run_fixture(name: str) -> int:
        return subprocess.run(
            [str(executable),
             f"--gtest_filter=PostProcessPassGpuContractTest.{name}"],
            env=environment,
            check=False,
        ).returncode

    for fixture in (
        "MotionBlurPreservesStaticLayerAndBlursMultipleMovingLayerRegions",
        "MotionBlurRotationFixtureWritesImagesForChecker",
    ):
        code = run_fixture(fixture)
        if code:
            return code

    input_path = output_dir / "motion-blur-input.ppm"
    output_path = output_dir / "motion-blur-output.ppm"
    if not input_path.is_file() or not output_path.is_file():
        raise RuntimeError(
            "The GPU test exited without writing images. Check that its Vulkan "
            "device initialized and that the selected test actually ran."
        )
    width, height, before = read_ppm(input_path)
    out_width, out_height, after = read_ppm(output_path)
    if (width, height) != (9, 9) or (out_width, out_height) != (width, height):
        raise ValueError("Unexpected motion-blur fixture dimensions")

    def px(image: list[tuple[int, int, int]], x: int, y: int) -> tuple[int, int, int]:
        return image[y * width + x]

    def red_strength(image: list[tuple[int, int, int]], x: int, y: int) -> int:
        red, green, blue = px(image, x, y)
        return red if red > 8 and red > green + 8 else 0

    def yellow_strength(image: list[tuple[int, int, int]], x: int, y: int) -> int:
        red, green, blue = px(image, x, y)
        return min(red, green) if min(red, green) > 8 and abs(red - green) < 8 else 0

    checks = {
        "static green mark preserved": px(after, 2, 2)[1] > 240,
        "horizontal red trail detected": red_strength(after, 3, 4) > 0,
        "horizontal red trail reaches the opposite side": red_strength(after, 5, 4) > 0,
        "horizontal trail remains centered": red_strength(after, 4, 4) > 0,
        "vertical yellow trail detected": yellow_strength(after, 6, 5) > 0,
        "vertical yellow trail reaches the opposite side": yellow_strength(after, 6, 7) > 0,
        "vertical trail remains centered": yellow_strength(after, 6, 6) > 0,
        "horizontal motion stays out of the neighboring row": red_strength(after, 4, 3) == 0,
        "vertical motion stays out of the neighboring column": yellow_strength(after, 5, 6) == 0,
        "input fixture contains no horizontal trail": red_strength(before, 3, 4) == 0,
    }

    rotation_input_path = output_dir / "motion-blur-rotation-input.ppm"
    rotation_output_path = output_dir / "motion-blur-rotation-output.ppm"
    if not rotation_input_path.is_file() or not rotation_output_path.is_file():
        raise RuntimeError(
            "The rotation fixture did not write its images. Check that its "
            "GoogleTest case ran on this host."
        )
    rotation_width, rotation_height, rotation_before = read_ppm(
        rotation_input_path
    )
    out_rotation_width, out_rotation_height, rotation_after = read_ppm(
        rotation_output_path
    )
    if (rotation_width, rotation_height) != (9, 9) or (
        out_rotation_width, out_rotation_height
    ) != (rotation_width, rotation_height):
        raise ValueError("Unexpected rotation fixture dimensions")

    checks.update(
        {
            "rotation centre stays sharp": px(rotation_after, 4, 4)[1] > 240,
            "rotation trail appears above the marker": red_strength(
                rotation_after, 2, 3
            )
            > 0,
            "rotation trail appears below the marker": red_strength(
                rotation_after, 2, 5
            )
            > 0,
            "rotation leaves no left horizontal trail": red_strength(
                rotation_after, 1, 4
            )
            == 0,
            "rotation leaves no right horizontal trail": red_strength(
                rotation_after, 3, 4
            )
            == 0,
            "rotation input contains no vertical trail": red_strength(
                rotation_before, 2, 3
            )
            == 0,
        }
    )

    for label, passed in checks.items():
        print(f"{'PASS' if passed else 'FAIL'}: {label}")
    print(f"Recognized image: {output_path}")
    print(f"Input reference:  {input_path}")
    print(f"Rotation image:   {rotation_output_path}")
    if owned_temp:
        print("Images are temporary; pass --output-dir to retain them.")
        owned_temp.cleanup()
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
