"""Validate RD24 actual-Core ColorChecker provenance and measurements."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd24_colorchecker_core"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    source = REPO / manifest["source_pair"]["source"]["path"]
    reference = REPO / manifest["source_pair"]["reference"]["path"]
    assert sha(source) == manifest["source_pair"]["source"]["sha256"]
    assert sha(reference) == manifest["source_pair"]["reference"]["sha256"]
    assert manifest["provenance"]["test_source_sha256"] == sha(
        REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp")
    assert manifest["provenance"]["core_source_sha256"] == sha(
        REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp")
    log = REPO / manifest["provenance"]["test_log"]
    assert sha(log) == manifest["provenance"]["test_log_sha256"]
    log_text = log.read_text(encoding="utf-8", errors="replace")
    assert "Running 12 tests from 1 test suite." in log_text
    assert "[  PASSED  ] 12 tests." in log_text
    descriptors = manifest["provenance"]["loaded_image_descriptors"]
    assert len(descriptors) == 2
    assert {line.split(",")[1] for line in descriptors} == {"source", "reference"}
    for line in descriptors:
        fields = dict(field.split("=", 1) for field in line.split(",")[2:])
        assert fields["transfer_known"] in {"0", "1"}
        assert fields["transfer_known"] == "0"
        assert fields["primaries"] == "0"

    csv_path = CAPTURE / manifest["provenance"]["metrics_csv"]
    assert sha(csv_path) == manifest["provenance"]["metrics_sha256"]
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 27
    expected = {(m, i, r) for m in ("Reinhard", "MeanStddev", "Histogram")
                for i in ("0.25", "0.5", "1")
                for r in ("skin_tone_proxy", "white_patch", "neutral_gray")}
    assert {(r["method"], r["intensity"], r["region"]) for r in rows} == expected
    pixels = {"skin_tone_proxy": 7200, "white_patch": 3600, "neutral_gray": 18000}
    numeric = ("intensity", "pixels", "source_mean_delta_e76",
               "source_p95_delta_e76", "source_max_delta_e76",
               "source_pct_over_2", "wb2_mean_delta_e76",
               "wb2_p95_delta_e76", "wb2_max_delta_e76", "output_mean_L",
               "output_mean_a", "output_mean_b", "output_mean_Cstar",
               "source_mean_L", "source_mean_a", "source_mean_b",
               "source_mean_Cstar", "wb2_mean_L", "wb2_mean_a",
               "wb2_mean_b", "wb2_mean_Cstar")
    for row in rows:
        assert int(row["pixels"]) == pixels[row["region"]]
        assert all(math.isfinite(float(row[field])) for field in numeric)
    for name, digest in manifest["outputs"].items():
        assert sha(CAPTURE / name) == digest
    assert sha(CAPTURE / manifest["provenance"]["visual_sheet"]) == manifest["provenance"]["visual_sheet_sha256"]
    assert manifest["source_pair"]["reference"]["role"].endswith("not calibrated/colorimetric ground truth")
    assert manifest["provenance"]["acceptance"] is False

    summary_path = CAPTURE / "tradeoff_summary.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    assert summary["input_metrics_sha256"] == sha(csv_path)
    assert summary["input_manifest_sha256"] == sha(CAPTURE / "manifest.json")
    assert set(summary["regions"]) == {"skin_tone_proxy", "white_patch", "neutral_gray"}
    for region in summary["regions"].values():
        candidates = region["candidates"]
        frontier = region["pareto_frontier"]
        assert len(candidates) == 9 and 1 <= len(frontier) <= 9
        candidate_ids = {(item["method"], item["intensity"]) for item in candidates}
        frontier_ids = {(item["method"], item["intensity"]) for item in frontier}
        assert frontier_ids <= candidate_ids
        for point in frontier:
            assert not any(
                other["source_change_mean_de76"] <= point["source_change_mean_de76"]
                and other["camera_auto_wb_distance_mean_de76"] <= point["camera_auto_wb_distance_mean_de76"]
                and (other["source_change_mean_de76"] < point["source_change_mean_de76"]
                     or other["camera_auto_wb_distance_mean_de76"] < point["camera_auto_wb_distance_mean_de76"])
                for other in candidates if other is not point
            )
    chroma_path = CAPTURE / "chroma_summary.json"
    chroma = json.loads(chroma_path.read_text(encoding="utf-8"))
    assert chroma["metrics_sha256"] == sha(csv_path)
    assert chroma["source_png_sha256"] == sha(CAPTURE / "source_wb1.png")
    assert chroma["reference_png_sha256"] == sha(CAPTURE / "reference_wb2.png")
    assert len(chroma["rows"]) == 27
    expected_pixels = {"skin_tone_proxy": 7200, "white_patch": 3600, "neutral_gray": 18000}
    for region, baseline in chroma["roi_baselines"].items():
        assert baseline["source"]["pixels"] == expected_pixels[region]
        assert baseline["camera_auto_wb"]["pixels"] == expected_pixels[region]
    for row in chroma["rows"]:
        assert row["pixels"] == expected_pixels[row["region"]]
        assert all(math.isfinite(row[field]) for field in (
            "source_mean_a", "output_mean_a", "wb2_mean_a", "delta_a_from_source",
            "source_mean_b", "output_mean_b", "wb2_mean_b", "delta_b_from_source",
            "source_mean_Cstar", "output_mean_Cstar", "wb2_mean_Cstar",
            "delta_Cstar_from_source"))
    print("RD24 valid: 27 actual-Core chart measurements, full 12/12 suite log, hashes and exploratory limits verified")


if __name__ == "__main__":
    main()
