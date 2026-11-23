"""Verify RD8 metric decomposition provenance and coverage."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd8_skin_white_chroma"
METRICS = CAPTURE / "metrics.json"
MANIFEST = CAPTURE / "manifest.json"
SCRIPT = ROOT / "run_rd8_skin_white_chroma.py"
RD6_MANIFEST = ROOT / "captures" / "rd6_colorchecker_pair" / "manifest.json"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    data = json.loads(METRICS.read_text(encoding="utf-8"))
    assert manifest["script_sha256"] == sha(SCRIPT)
    assert manifest["metrics_sha256"] == sha(METRICS)
    assert manifest["rd6_manifest_sha256"] == sha(RD6_MANIFEST)
    assert data["inputs"]["rd6_manifest_sha256"] == sha(RD6_MANIFEST)
    assert data["inputs"]["core_source_sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert len(data["conditions"]) == manifest["conditions"] == 10
    assert len({row["condition"] for row in data["conditions"]}) == 10
    assert set(data["regions"]) == set(manifest["regions"])
    expected_pixels = {"skin_dark_proxy_patch_r0_c0": 3600,
                       "skin_light_proxy_patch_r0_c1": 3600,
                       "skin_proxy_combined": 7200,
                       "white_patch_r3_c0": 3600}
    for name, pixels in expected_pixels.items():
        assert data["regions"][name]["pixels"] == pixels
    for row in data["conditions"]:
        assert set(row["regions"]) == set(expected_pixels)
        for name, region in row["regions"].items():
            assert region["pixels"] == expected_pixels[name]
            numeric = [region["Cstar_from_mean_ab"], region["Cstar_pixel_mean"],
                       region["hue_circular_mean_deg"], region["deltaE76_of_mean_Lab_to_reference"]]
            assert np.isfinite(numeric).all()
            assert 0 <= region["hue_circular_mean_deg"] < 360
    print("RD8 metrics valid: 10 conditions × 4 ROIs; source pins and finite metrics verified")


if __name__ == "__main__":
    main()
