#!/usr/bin/env python3
"""Repeat UI capture and pixel comparison until the approved baseline matches."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import subprocess
import sys
import time
import uuid
from pathlib import Path


def split_command(command: str) -> list[str]:
    parts = shlex.split(command, posix=(os.name != "nt"))
    if os.name == "nt":
        parts = [
            part[1:-1] if len(part) >= 2 and part[0] == part[-1] and part[0] in "\"'" else part
            for part in parts
        ]
    return parts


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--output-prefix", required=True, type=Path,
                        help="iteration artifacts are saved under this prefix")
    parser.add_argument(
        "--capture-command", required=True,
        help="capture executable command; include {actual} where screenshot path goes",
    )
    parser.add_argument(
        "--language",
        choices=("ja", "en", "zh", "zh-TW", "ko", "fr", "de", "es", "pt", "ru", "ar"),
        help="replace {language} in the capture command with this locale code",
    )
    parser.add_argument("--environment-json", type=Path)
    parser.add_argument("--region", action="append", default=[])
    parser.add_argument("--region-limit", action="append", default=[])
    parser.add_argument("--channel-tolerance", type=int, default=0)
    parser.add_argument("--max-diff-pixels", type=int, default=0)
    parser.add_argument("--max-diff-fraction", type=float, default=0.0)
    parser.add_argument("--diff-amplification", type=int, default=4)
    parser.add_argument("--max-iterations", type=int,
                        help="stop after this many captures; default repeats until pass")
    parser.add_argument(
        "--retry-delay-seconds", type=float,
        help="automatically recapture after a mismatch; omit to wait for Enter (q stops)",
    )
    parser.add_argument(
        "--compare-only", action="store_true",
        help="capture and compare once, then return the comparison result",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        capture_command = split_command(args.capture_command)
        if not capture_command or not any("{actual}" in part for part in capture_command):
            raise ValueError("--capture-command must contain the {actual} output placeholder")
        if args.language is not None and not any(
            "{language}" in part for part in capture_command
        ):
            raise ValueError("--language requires a {language} placeholder in --capture-command")
        if args.max_iterations is not None and args.max_iterations <= 0:
            raise ValueError("--max-iterations must be positive")
        if args.retry_delay_seconds is not None and args.retry_delay_seconds < 0:
            raise ValueError("--retry-delay-seconds must be non-negative")
        if args.compare_only and (args.max_iterations not in (None, 1)
                                  or args.retry_delay_seconds is not None):
            raise ValueError("--compare-only cannot be combined with loop options")
        if args.max_diff_pixels < 0 or not 0 <= args.max_diff_fraction <= 1:
            raise ValueError("diff limits must be non-negative and fraction must be in [0, 1]")
        if not 0 <= args.channel_tolerance <= 255:
            raise ValueError("--channel-tolerance must be between 0 and 255")

        reference = args.reference.resolve()
        prefix = args.output_prefix.resolve()
        prefix.parent.mkdir(parents=True, exist_ok=True)
        run_prefix = prefix.with_name(f"{prefix.name}.run-{uuid.uuid4().hex[:12]}")
        compare_tool = Path(__file__).with_name("ui_visual_compare.py").resolve()
        iteration = 1

        while args.max_iterations is None or iteration <= args.max_iterations:
            actual = run_prefix.with_name(
                f"{run_prefix.name}.iteration-{iteration:03d}.png"
            )
            output_prefix = run_prefix.with_name(
                f"{run_prefix.name}.iteration-{iteration:03d}"
            )
            invocation = [
                part.replace("{actual}", str(actual))
                    .replace("{iteration}", str(iteration))
                    .replace("{language}", args.language or "")
                for part in capture_command
            ]
            print(f"[capture {iteration}] {' '.join(invocation)}", flush=True)
            captured = subprocess.run(invocation, check=False)
            if captured.returncode != 0:
                print(f"capture command failed with exit code {captured.returncode}", file=sys.stderr)
                return captured.returncode or 1
            if not actual.is_file():
                print(f"capture command did not create {actual}", file=sys.stderr)
                return 2

            comparison = [
                sys.executable, str(compare_tool),
                "--reference", str(reference),
                "--actual", str(actual),
                "--output-prefix", str(output_prefix),
                "--channel-tolerance", str(args.channel_tolerance),
                "--max-diff-pixels", str(args.max_diff_pixels),
                "--max-diff-fraction", str(args.max_diff_fraction),
                "--diff-amplification", str(args.diff_amplification),
            ]
            if args.environment_json is not None:
                comparison.extend(["--environment-json", str(args.environment_json.resolve())])
            for region in args.region:
                comparison.extend(["--region", region])
            for region_limit in args.region_limit:
                comparison.extend(["--region-limit", region_limit])

            result = subprocess.run(comparison, check=False)
            if result.returncode == 0:
                print(f"pixel comparison passed on capture {iteration}: {output_prefix}")
                return 0
            if result.returncode != 1:
                return result.returncode

            report_path = output_prefix.with_name(output_prefix.name + ".json")
            report = json.loads(report_path.read_text(encoding="utf-8"))
            metrics = report.get("metrics", {})
            print(
                f"Mismatch: {metrics.get('changed_pixels', 'n/a')} changed pixels; "
                f"overlay={output_prefix.name}.overlay.png; diff={output_prefix.name}.diff.png",
                flush=True,
            )
            if args.max_iterations is not None and iteration >= args.max_iterations:
                return 1
            if args.compare_only:
                return 1
            if args.retry_delay_seconds is not None:
                print(
                    f"Recapturing in {args.retry_delay_seconds:g}s; "
                    "interrupt with Ctrl+C.",
                    flush=True,
                )
                time.sleep(args.retry_delay_seconds)
                iteration += 1
                continue
            try:
                answer = input("Fix the UI, then press Enter to capture again, or q to stop: ")
            except EOFError:
                return 1
            if answer.strip().lower() == "q":
                return 1
            iteration += 1
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"ui_visual_loop: {error}", file=sys.stderr)
        return 2
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
