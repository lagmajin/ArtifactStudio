"""Validate RD25 Core alpha/hidden-RGB measurements and provenance pins."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd25_core_alpha"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest_path = CAPTURE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    rd13_manifest = ROOT / "captures" / "rd13_transparent_hidden_rgb" / "manifest.json"
    assert manifest["fixture"]["source_manifest_sha256"] == sha(rd13_manifest)
    assert manifest["fixture"]["source_black_hidden_sha256"] == sha(
        rd13_manifest.parent / "source_black_hidden_rgba.npy")
    assert manifest["fixture"]["source_magenta_hidden_sha256"] == sha(
        rd13_manifest.parent / "source_magenta_hidden_rgba.npy")
    assert manifest["fixture"]["constructed_target_sha256"] == sha(
        rd13_manifest.parent / "constructed_target_rgb.npy")
    provenance = manifest["provenance"]
    assert provenance["test_source_sha256"] == sha(
        REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp")
    assert provenance["core_source_sha256"] == sha(
        REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp")
    log_path = REPO / provenance["test_log"]
    assert provenance["test_log_sha256"] == sha(log_path)
    log = log_path.read_text(encoding="utf-8", errors="replace")
    assert "RD25_CORE_ALPHA," in log
    metrics_path = CAPTURE / provenance["metrics_csv"]
    assert provenance["metrics_sha256"] == sha(metrics_path)
    with metrics_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    expected = {(method, intensity) for method in ("Reinhard", "MeanStddev", "Histogram")
                for intensity in (0.5, 1.0)}
    assert {(row["method"], float(row["intensity"])) for row in rows} == expected
    assert len(rows) == 6
    for row in rows:
        assert int(row["visible_pixels"]) == 871424
        assert row["finite"] == "True" and row["alpha_preserved"] == "True"
        for field in ("paired_rgb_mae", "paired_rgb_p95", "paired_rgb_max",
                      "paired_delta_e76_mean", "paired_delta_e76_p95",
                      "paired_delta_e76_max"):
            assert math.isfinite(float(row[field])) and float(row[field]) >= 0.0
    assert manifest["measurement"]["acceptance"] is False
    assert manifest["fixture"]["visible_rgb_and_alpha_identical"] is True
    print("RD25 valid: six actual-Core alpha conditions, finite outputs, alpha preserved, hashes verified; exploratory only")


if __name__ == "__main__":
    main()
