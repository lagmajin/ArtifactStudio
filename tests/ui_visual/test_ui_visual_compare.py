"""Regression tests for the strict UI screenshot comparison gate.

Run directly with:
    python tests/ui_visual/test_ui_visual_compare.py
"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path

from PIL import Image


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
COMPARATOR_PATH = REPOSITORY_ROOT / "tools" / "ui_visual_compare.py"
SPEC = importlib.util.spec_from_file_location("ui_visual_compare", COMPARATOR_PATH)
assert SPEC is not None and SPEC.loader is not None
ui_visual_compare = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ui_visual_compare)


class UiVisualCompareTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory(prefix="artifact-ui-visual-")
        self.root = Path(self.temporary_directory.name)
        self.reference_path = self.root / "reference.png"
        self.actual_path = self.root / "actual.png"
        Image.new("RGBA", (4, 4), (0, 0, 0, 0)).save(self.reference_path)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def compare(self, *, prefix: str = "result", **overrides: object) -> tuple[int, dict[str, object]]:
        arguments = {
            "reference": self.reference_path,
            "actual": self.actual_path,
            "output_prefix": self.root / prefix,
            "channel_tolerance": 0,
            "max_diff_pixels": 0,
            "max_diff_fraction": 0.0,
            "diff_amplification": 4,
            "environment_json": None,
            "regions": [],
            "region_limits": [],
        }
        arguments.update(overrides)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            exit_code = ui_visual_compare.compare(ui_visual_compare.argparse.Namespace(**arguments))
        report = ui_visual_compare.json.loads(
            (self.root / f"{prefix}.json").read_text(encoding="utf-8")
        )
        return exit_code, report

    def test_region_limit_fails_even_when_global_budget_passes(self) -> None:
        actual = Image.new("RGBA", (4, 4), (0, 0, 0, 0))
        actual.putpixel((0, 0), (255, 0, 0, 255))
        actual.save(self.actual_path)

        exit_code, report = self.compare(
            max_diff_pixels=1,
            max_diff_fraction=0.1,
            regions=[("ruler", (0, 0, 2, 2))],
            region_limits=[("ruler", 0, 0.0)],
        )

        self.assertEqual(exit_code, 1)
        self.assertEqual(report["metrics"]["changed_pixels"], 1)
        self.assertEqual(report["regions"]["ruler"]["changed_pixels"], 1)
        self.assertFalse(report["passed"])

    def test_exact_region_match_passes_and_emits_overlay_and_diff(self) -> None:
        Image.new("RGBA", (4, 4), (0, 0, 0, 0)).save(self.actual_path)

        exit_code, report = self.compare(
            regions=[("tracks", (0, 0, 4, 4))],
            region_limits=[("tracks", 0, 0.0)],
        )

        self.assertEqual(exit_code, 0)
        self.assertTrue(report["passed"])
        self.assertEqual(report["regions"]["tracks"]["changed_pixels"], 0)
        self.assertTrue((self.root / "result.overlay.png").is_file())
        self.assertTrue((self.root / "result.diff.png").is_file())

    def test_channel_tolerance_is_applied_per_channel_before_pixel_count(self) -> None:
        actual = Image.new("RGBA", (4, 4), (0, 0, 0, 0))
        actual.putpixel((2, 2), (1, 0, 0, 0))
        actual.save(self.actual_path)

        exit_code, report = self.compare(channel_tolerance=1)

        self.assertEqual(exit_code, 0)
        self.assertEqual(report["metrics"]["changed_pixels"], 0)
        self.assertEqual(report["metrics"]["maximum_channel_difference"], 1)

    def test_dimension_mismatch_fails_and_reports_region_metrics(self) -> None:
        Image.new("RGBA", (5, 4), (0, 0, 0, 0)).save(self.actual_path)

        exit_code, report = self.compare(
            regions=[("full", (0, 0, 5, 4))],
            region_limits=[("full", 0, 0.0)],
        )

        self.assertEqual(exit_code, 1)
        self.assertFalse(report["passed"])
        self.assertEqual(report["failure"], "image dimensions differ")
        self.assertEqual(report["regions"]["full"]["total_pixels"], 20)
        self.assertTrue(report["region_limits"]["full"]["passed"])
        self.assertTrue((self.root / "result.overlay.png").is_file())
        self.assertTrue((self.root / "result.diff.png").is_file())

    def test_unknown_region_limit_is_rejected(self) -> None:
        Image.new("RGBA", (4, 4), (0, 0, 0, 0)).save(self.actual_path)
        arguments = ui_visual_compare.argparse.Namespace(
            reference=self.reference_path,
            actual=self.actual_path,
            output_prefix=self.root / "invalid",
            channel_tolerance=0,
            max_diff_pixels=0,
            max_diff_fraction=0.0,
            diff_amplification=4,
            environment_json=None,
            regions=[],
            region_limits=[("missing", 0, 0.0)],
        )

        with self.assertRaisesRegex(ValueError, "unknown region"):
            ui_visual_compare.compare(arguments)

    def test_dimension_mismatch_still_rejects_unknown_region_limit(self) -> None:
        Image.new("RGBA", (5, 4), (0, 0, 0, 0)).save(self.actual_path)
        arguments = ui_visual_compare.argparse.Namespace(
            reference=self.reference_path,
            actual=self.actual_path,
            output_prefix=self.root / "invalid-dimensions",
            channel_tolerance=0,
            max_diff_pixels=0,
            max_diff_fraction=0.0,
            diff_amplification=4,
            environment_json=None,
            regions=[("full", (0, 0, 5, 4))],
            region_limits=[("missing", 0, 0.0)],
        )

        with self.assertRaisesRegex(ValueError, "unknown region"):
            ui_visual_compare.compare(arguments)


if __name__ == "__main__":
    unittest.main()
