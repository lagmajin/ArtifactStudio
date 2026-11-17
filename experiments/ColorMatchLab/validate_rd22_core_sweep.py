"""Validate the captured RD22 Core portrait strength/tail sweep."""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd20_core_skin_white"
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
TEST = ROOT.parent.parent / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
ASSET = ROOT / "assets" / "WikimediaCC0" / "Pexels-photo-707448-1280.jpg"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    metrics = json.loads((CAPTURE / "metrics.json").read_text(encoding="utf-8"))
    assert manifest["rd"] == metrics["rd"] == "rd22-core-portrait-synthetic-cast-strength-and-tail-sweep"
    assert manifest["inputs"]["core_source_sha256"] == sha(CORE)
    assert manifest["inputs"]["test_source_sha256"] == sha(TEST)
    assert manifest["source"]["sha256"] == sha(ASSET)
    assert manifest["inputs"]["prep_manifest_sha256"] == sha(PREP / "manifest.json")
    for name, digest in manifest["core_png_outputs"].items():
        assert sha(ROOT / "captures" / "rd22_core_outputs" / name) == digest, name
    for name, digest in manifest["files"].items():
        assert sha(CAPTURE / name) == digest, name
    assert sha(CAPTURE / "metrics.json") == manifest["metrics_sha256"]

    rows = metrics["data"]
    methods = {"Reinhard", "MeanStddev", "Histogram"}
    intensities = {round(i / 10, 1) for i in range(11)}
    regions = {"face_skin", "arm_skin", "skin_combined", "light_blouse_candidate", "full_frame"}
    assert len(rows) == 165
    assert {(r["method"], round(r["intensity"], 1), r["roi"]) for r in rows} == {
        (method, intensity, region)
        for method in methods for intensity in intensities for region in regions
    }
    fields = (
        "mean_deltaE76_to_input", "mean_deltaE76_to_synthetic_reference",
        "p95_deltaE76_to_input", "max_deltaE76_to_input",
        "percent_pixels_deltaE76_gt2_to_input", "p95_deltaE76_to_synthetic_reference",
        "max_deltaE76_to_synthetic_reference",
        "percent_pixels_deltaE76_gt2_to_synthetic_reference",
        "mean_vector_Cstar_output", "pixel_mean_Cstar_output",
        "output_hue_circular_mean_degrees",
    )
    assert all(math.isfinite(r[key]) for r in rows for key in fields)
    assert all(0 <= r["percent_pixels_deltaE76_gt2_to_input"] <= 100 for r in rows)
    assert all(0 <= r["percent_pixels_deltaE76_gt2_to_synthetic_reference"] <= 100 for r in rows)
    assert metrics["conditions"]["roi_measurement_count"] == 165
    print("RD22 Core sweep valid: 33 conditions × 5 regions, provenance and artifact hashes verified")


if __name__ == "__main__":
    main()
