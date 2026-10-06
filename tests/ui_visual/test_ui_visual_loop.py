"""End-to-end regression tests for the repeatable UI capture runner.

Run directly with:
    python -m unittest discover -s tests/ui_visual -v
"""

from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LOOP_PATH = REPOSITORY_ROOT / "tools" / "ui_visual_loop.py"


class UiVisualLoopTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory(prefix="artifact-ui-loop-")
        self.root = Path(self.temporary_directory.name)
        self.fixture_directory = self.root / "capture fixture with spaces"
        self.fixture_directory.mkdir()
        self.reference_path = self.fixture_directory / "reference image.png"
        self.capture_path = self.fixture_directory / "fake capture.py"
        self.counter_path = self.root / "capture-count.txt"
        self.output_prefix = self.root / "review output"
        Image.new("RGBA", (4, 3), (20, 40, 60, 255)).save(self.reference_path)
        self.capture_path.write_text(
            """from __future__ import annotations
import argparse
from pathlib import Path
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--counter', required=True, type=Path)
parser.add_argument('--iteration', type=str)
parser.add_argument('--iteration-output', type=Path)
parser.add_argument('--mismatch-captures', type=int, default=0)
parser.add_argument('--exit-code', type=int, default=0)
parser.add_argument('--omit-output', action='store_true')
args = parser.parse_args()
if args.exit_code:
    raise SystemExit(args.exit_code)
if args.omit_output:
    raise SystemExit(0)
if args.iteration_output is not None:
    args.iteration_output.write_text(args.iteration or '')
count = int(args.counter.read_text() if args.counter.exists() else '0') + 1
args.counter.write_text(str(count))
image = Image.new('RGBA', (4, 3), (20, 40, 60, 255))
if count <= args.mismatch_captures:
    image.putpixel((0, 0), (255, 0, 0, 255))
args.output.parent.mkdir(parents=True, exist_ok=True)
image.save(args.output)
""",
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def command(self, *extra: str) -> str:
        arguments = [
            sys.executable,
            str(self.capture_path),
            "--output",
            "{actual}",
            "--counter",
            str(self.counter_path),
            *extra,
        ]
        return subprocess.list2cmdline(arguments) if os.name == "nt" else shlex.join(arguments)

    def run_loop(
        self,
        *options: str,
        capture_extra: tuple[str, ...] = (),
        input_text: str | None = None,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                sys.executable,
                str(LOOP_PATH),
                "--reference",
                str(self.reference_path),
                "--output-prefix",
                str(self.output_prefix),
                "--capture-command",
                self.command(*capture_extra),
                *options,
            ],
            cwd=REPOSITORY_ROOT,
            check=False,
            capture_output=True,
            text=True,
            input=input_text,
            timeout=30,
        )

    def artifacts(self, suffix: str) -> list[Path]:
        return sorted(self.root.glob(f"review output.run-*.{suffix}"))

    def test_compare_only_captures_once_and_passes(self) -> None:
        result = self.run_loop("--compare-only")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.counter_path.read_text(encoding="utf-8"), "1")
        reports = self.artifacts("json")
        self.assertEqual(len(reports), 1)
        self.assertTrue(json.loads(reports[0].read_text(encoding="utf-8"))["passed"])
        self.assertEqual(len(self.artifacts("iteration-001.png")), 1)

    def test_each_run_keeps_artifacts_under_a_unique_prefix(self) -> None:
        first = self.run_loop("--compare-only")
        second = self.run_loop("--compare-only")

        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(second.returncode, 0, second.stderr)
        actuals = self.artifacts("iteration-001.png")
        self.assertEqual(len(actuals), 2)
        self.assertNotEqual(actuals[0].name, actuals[1].name)

    def test_iteration_placeholder_is_expanded_for_capture_command(self) -> None:
        iteration_path = self.root / "captured iteration.txt"
        result = self.run_loop(
            "--compare-only",
            capture_extra=("--iteration", "{iteration}", "--iteration-output", str(iteration_path)),
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(iteration_path.read_text(encoding="utf-8"), "1")

    def test_environment_manifest_reaches_comparison_report(self) -> None:
        environment_path = self.root / "environment.json"
        manifest = {"dpi": 144, "theme": "dark", "locale": "ja-JP"}
        environment_path.write_text(json.dumps(manifest), encoding="utf-8")

        result = self.run_loop("--compare-only", "--environment-json", str(environment_path))

        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(self.artifacts("json")[0].read_text(encoding="utf-8"))
        self.assertEqual(report["environment"], manifest)

    def test_automatic_retry_reaches_match_and_forwards_region_gate(self) -> None:
        result = self.run_loop(
            "--retry-delay-seconds",
            "0",
            "--max-iterations",
            "2",
            "--region",
            "corner=0,0,1,1",
            "--region-limit",
            "corner,0,0",
            capture_extra=("--mismatch-captures", "1"),
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.counter_path.read_text(encoding="utf-8"), "2")
        reports = self.artifacts("json")
        self.assertEqual(len(reports), 2)
        first, second = [json.loads(path.read_text(encoding="utf-8")) for path in reports]
        self.assertFalse(first["passed"])
        self.assertEqual(first["regions"]["corner"]["changed_pixels"], 1)
        self.assertTrue(second["passed"])

    def test_mismatch_at_iteration_limit_returns_failure_and_keeps_diagnostics(self) -> None:
        result = self.run_loop(
            "--retry-delay-seconds",
            "0",
            "--max-iterations",
            "2",
            capture_extra=("--mismatch-captures", "5"),
        )

        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(self.counter_path.read_text(encoding="utf-8"), "2")
        self.assertEqual(len(self.artifacts("json")), 2)
        self.assertEqual(len(self.artifacts("iteration-002.diff.png")), 1)
        self.assertEqual(len(self.artifacts("iteration-002.overlay.png")), 1)

    def test_enter_after_mismatch_captures_again_and_passes(self) -> None:
        result = self.run_loop(
            "--max-iterations",
            "2",
            capture_extra=("--mismatch-captures", "1"),
            input_text="\n",
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("press Enter to capture again", result.stdout)
        self.assertEqual(self.counter_path.read_text(encoding="utf-8"), "2")
        self.assertEqual(len(self.artifacts("json")), 2)

    def test_q_after_mismatch_stops_without_another_capture(self) -> None:
        result = self.run_loop(
            capture_extra=("--mismatch-captures", "5"),
            input_text="q\n",
        )

        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(self.counter_path.read_text(encoding="utf-8"), "1")
        self.assertEqual(len(self.artifacts("json")), 1)

    def test_capture_failure_propagates_exit_code(self) -> None:
        result = self.run_loop("--compare-only", capture_extra=("--exit-code", "7"))

        self.assertEqual(result.returncode, 7)
        self.assertIn("capture command failed", result.stderr)
        self.assertEqual(self.artifacts("json"), [])

    def test_capture_without_png_fails_clearly(self) -> None:
        result = self.run_loop("--compare-only", capture_extra=("--omit-output",))

        self.assertEqual(result.returncode, 2)
        self.assertIn("did not create", result.stderr)
        self.assertEqual(self.artifacts("json"), [])

    def test_missing_output_placeholder_is_rejected_before_capture(self) -> None:
        result = subprocess.run(
            [
                sys.executable,
                str(LOOP_PATH),
                "--reference",
                str(self.reference_path),
                "--output-prefix",
                str(self.output_prefix),
                "--capture-command",
                self.command().replace("{actual}", "unused"),
                "--compare-only",
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("must contain the {actual}", result.stderr)
        self.assertFalse(self.counter_path.exists())


if __name__ == "__main__":
    unittest.main()
