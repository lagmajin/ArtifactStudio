"""Validate RD26 Core premultiplied-alpha metrics and provenance hashes."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd26_core_premultiplied"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    provenance = manifest["provenance"]
    assert provenance["test_source_sha256"] == sha(
        REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp")
    assert provenance["core_source_sha256"] == sha(
        REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp")
    log_path = REPO / provenance["test_log"]
    assert provenance["test_log_sha256"] == sha(log_path)
    assert "RD26_CORE_PREMULT," in log_path.read_text(
        encoding="utf-8", errors="replace")
    metrics_path = CAPTURE / provenance["metrics_csv"]
    assert provenance["metrics_sha256"] == sha(metrics_path)
    with metrics_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    expected = {(method, intensity) for method in ("Reinhard", "MeanStddev", "Histogram")
                for intensity in (0.5, 1.0)}
    assert {(row["method"], float(row["intensity"])) for row in rows} == expected
    assert len(rows) == 6
    for row in rows:
        assert int(row["pixels"]) == 4096
        assert row["finite"] == "True" and row["alpha_preserved"] == "True"
        assert int(row["rgb_channels_below_zero"]) >= 0
        assert int(row["rgb_channels_above_alpha"]) >= 0
        assert int(row["transparent_nonzero_rgb_channels"]) >= 0
        assert math.isfinite(float(row["max_excess_over_alpha"]))
        assert float(row["max_excess_over_alpha"]) >= 0.0
    assert manifest["measurement"]["acceptance"] is False
    assert manifest["fixture"]["width"] == 64 and manifest["fixture"]["height"] == 64
    print("RD26 valid: six actual-Core premultiplied-alpha conditions, finite outputs, alpha preserved, hashes verified; exploratory only")


if __name__ == "__main__":
    main()
