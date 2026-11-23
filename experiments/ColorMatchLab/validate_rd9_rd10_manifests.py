"""Audit RD9 portrait capture and RD10 ROI-strength tradeoff provenance."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
ASSET = ROOT / "assets" / "WikimediaCC0" / "Pexels-photo-707448-1280.jpg"
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
RD9 = ROOT / "captures" / "rd9_real_skin_white_scored"
RD10 = ROOT / "captures" / "rd10_roi_strength_tradeoff"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    prep_path, rd9_path = PREP / "manifest.json", RD9 / "manifest.json"
    prep, rd9, rd10 = [json.loads(p.read_text(encoding="utf-8")) for p in
                       (prep_path, rd9_path, RD10 / "manifest.json")]
    assert prep["source"]["sha256"] == sha(ASSET)
    assert prep["preparation_script_sha256"] == sha(ROOT / "prepare_rd9_real_skin_white_scene.py")
    assert prep["manual_review"]["reviewed_before_candidate_scoring"] is True
    assert rd9["input"]["prep_manifest_sha256"] == sha(prep_path)
    assert rd9["input"]["asset_sha256"] == sha(ASSET)
    assert rd9["scoring_script_sha256"] == sha(ROOT / "run_rd9_real_skin_white_scene.py")
    assert rd9["input"]["core_source_sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float32)
    ref = np.asarray(Image.open(PREP / "synthetic_cast_reference.png").convert("RGB"), dtype=np.float32)
    assert source.shape == ref.shape == (851, 1280, 3)
    source_mask = {}
    for name, item in prep["roi"].items():
        mask = np.asarray(Image.open(PREP / item["mask"]).convert("L")) > 0
        assert int(mask.sum()) == item["pixels"] and item["sha256"] == sha(PREP / item["mask"])
        source_mask[name] = mask
    assert not np.any(source_mask["face_skin_reviewed"] & source_mask["arm_skin_reviewed"])
    assert not np.any(source_mask["skin_combined_reviewed"] & source_mask["white_blouse_light_fabric_candidate"])
    assert len(rd9["results"]) == 10
    assert {(x["method"], x["intensity"]) for x in rd9["results"]} == {
        ("unchanged_source", 0.0),
        *((method, intensity) for method in ("Reinhard", "MeanStddev", "Histogram")
          for intensity in (0.25, 0.5, 1.0)),
    }
    assert set(rd9["regions"]) == {"face_skin_reviewed", "arm_skin_reviewed",
                                     "skin_combined_reviewed", "white_blouse_light_fabric_candidate", "full_frame"}
    bad_rd9 = [n for n, h in rd9["files"].items() if not (RD9 / n).is_file() or sha(RD9 / n) != h]
    assert not bad_rd9, f"RD9 output hashes mismatch: {bad_rd9}"
    assert rd10["input"]["rd9_prep_manifest_sha256"] == sha(prep_path)
    assert rd10["input"]["rd9_scored_manifest_sha256"] == sha(rd9_path)
    assert rd10["scoring_script_sha256"] == sha(ROOT / "run_rd10_roi_strength_tradeoff.py")
    assert rd10["input"]["core_source_sha256"] == EXPECTED_CORE_SHA256
    assert rd10["combination_count"] == len(rd10["results"]) == 27
    expected_grid = {(m, sf, bf) for m in ("Reinhard", "MeanStddev", "Histogram")
                     for sf in (0.0, 0.5, 1.0) for bf in (0.0, 0.5, 1.0)}
    assert {(x["method"], x["skin_factor"], x["blouse_factor"]) for x in rd10["results"]} == expected_grid
    rd9_map = {(x["method"], x["intensity"]): x for x in rd9["results"]}
    for row in rd10["results"]:
        for name, region in row["regions"].items():
            assert region["pixels"] == rd9["regions"][name]["pixels"]
            assert region["non_finite_values"] == 0
        if row["skin_factor"] == 1.0 and row["blouse_factor"] == 1.0:
            base = rd9_map[(row["method"], 1.0)]
            for name in ("skin_combined_reviewed", "white_blouse_light_fabric_candidate", "full_frame"):
                err = row["regions"][name]["deltaE76_mean_to_synthetic_reference"]
                expected = base["regions"][name]["deltaE76_mean_to_synthetic_reference"]
                assert abs(err - expected) < 1e-5
        if row["skin_factor"] == 0.0:
            assert row["regions"]["skin_combined_reviewed"]["encoded_sRGB_MAE_to_input"] == 0.0
        if row["blouse_factor"] == 0.0:
            assert row["regions"]["white_blouse_light_fabric_candidate"]["encoded_sRGB_MAE_to_input"] == 0.0
    print("RD9 valid: 10 conditions, five regions, disjoint ROI masks and all file hashes pass")
    print("RD10 valid: 27 method/skin/blouse settings, source-preserve and global-match endpoints pass")


if __name__ == "__main__":
    main()
