"""Validate RD23 source, ROI, Core result and review artifact provenance."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd23_shadowed_portrait"
OUTPUTS = CAPTURE / "core_outputs"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    if manifest["status"] != "exploratory Core synthetic-cast stress measurement completed; not acceptance":
        raise RuntimeError("Unexpected RD23 status")
    source = REPO / manifest["source"]["path"]
    if sha(source) != manifest["source"]["sha256"]:
        raise RuntimeError("RD23 source image hash mismatch")
    for name, expected in manifest["artifacts"].items():
        if sha(CAPTURE / name) != expected:
            raise RuntimeError(f"RD23 preparation artifact hash mismatch: {name}")

    test_source = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
    if sha(test_source) != manifest["core_test"]["test_source_sha256"]:
        raise RuntimeError("RD23 Core test source hash mismatch")
    metrics_path = CAPTURE / manifest["core_test"]["metrics_csv"]
    if sha(metrics_path) != manifest["metrics"]["sha256"]:
        raise RuntimeError("RD23 CSV hash mismatch")
    with metrics_path.open(newline="", encoding="utf-8") as stream:
        csv_rows = list(csv.DictReader(stream))
    if len(csv_rows) != 9:
        raise RuntimeError(f"Expected 9 Core metric rows, found {len(csv_rows)}")
    expected = {(method, intensity) for method in
                ("Reinhard", "MeanStddev", "Histogram")
                for intensity in ("0", "0.5", "1")}
    actual = {(row["method"], row["intensity"]) for row in csv_rows}
    if actual != expected:
        raise RuntimeError("RD23 Core method/intensity matrix mismatch")
    numeric_fields = list(csv_rows[0])[1:]
    for row in csv_rows:
        if int(row["roi_pixels"]) != manifest["core_test"]["roi_pixels"]:
            raise RuntimeError("RD23 metric ROI count mismatch")
        for field in numeric_fields:
            if not math.isfinite(float(row[field])):
                raise RuntimeError(f"Non-finite RD23 metric {field}")

    current_pngs = {path.name: sha(path) for path in sorted(OUTPUTS.glob("*.png"))}
    if current_pngs != manifest["core_png_outputs"] or len(current_pngs) != 9:
        raise RuntimeError("RD23 Core output PNG hash/name mismatch")
    visual = CAPTURE / manifest["core_visual_review"]["file"]
    if sha(visual) != manifest["core_visual_review"]["sha256"]:
        raise RuntimeError("RD23 visual review hash mismatch")

    suite_log = REPO / manifest["core_test"]["full_suite_run_log"]
    if sha(suite_log) != manifest["core_test"]["full_suite_run_log_sha256"]:
        raise RuntimeError("RD23 full suite log hash mismatch")
    suite_text = suite_log.read_text(encoding="utf-8", errors="replace")
    if "Running 12 tests from 1 test suite." not in suite_text or "[  PASSED  ] 12 tests." not in suite_text:
        raise RuntimeError("RD23 full suite log does not prove 12/12 pass")
    if manifest["core_visual_review"]["acceptance"] is not False:
        raise RuntimeError("RD23 visual review must remain exploratory")

    performance = manifest["performance"]
    performance_path = CAPTURE / performance["csv"]
    if sha(performance_path) != performance["sha256"]:
        raise RuntimeError("RD23 performance CSV hash mismatch")
    with performance_path.open(newline="", encoding="utf-8") as stream:
        performance_rows = list(csv.DictReader(stream))
    expected_performance = {(name, method) for name in
                            ("portrait_1280x851", "portrait_1536x1024")
                            for method in ("Reinhard", "MeanStddev", "Histogram")}
    if len(performance_rows) != 6 or {
            (row["image"], row["method"]) for row in performance_rows
    } != expected_performance:
        raise RuntimeError("RD23 performance matrix mismatch")
    for row in performance_rows:
        for field in ("width", "height", "intensity", "samples",
                      "hardware_threads", "min_ms", "median_ms", "max_ms"):
            if not math.isfinite(float(row[field])):
                raise RuntimeError(f"Non-finite RD23 performance value {field}")

    print("RD23 Core capture valid: 9 quality conditions, 6 timing conditions, ROI, logs and hashes verified; exploratory only")


if __name__ == "__main__":
    main()
