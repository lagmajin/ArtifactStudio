"""Audit RD12 cast strengths, complete conditions, source pins and artifacts."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd12_cast_strength_sensitivity"
MANIFEST = OUT / "manifest.json"
SCRIPT = ROOT / "run_rd12_cast_strength_sensitivity.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
EXPECTED_CASTS = {
    "cool": ([0.94, 1.015, 1.075], [0.012, 0.006, -0.008]),
    "warm": ([1.06, 1.0, 0.94], [-0.008, 0.006, 0.012]),
    "green": ([0.98, 1.05, 0.98], [0.006, -0.006, 0.006]),
}
EXPECTED_STRENGTHS = (0.5, 1.0, 1.5)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    prep = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    assert data["script_sha256"] == sha(SCRIPT)
    assert data["visual_review"]["generator_sha256"] == sha(ROOT / "make_rd12_visual_sheets.py")
    assert len(data["visual_review"]["sheets"]) == 4
    assert data["input"]["prep_manifest_sha256"] == sha(PREP / "manifest.json")
    assert data["input"]["source_sha256"] == sha(PREP / "source.png")
    assert data["input"]["core_source_sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert data["input"]["mirror_runner_sha256"] == sha(ROOT / "run_rd1_source_mirror.py")
    assert data["cast_model"] == "clip(source_sRGB * direction_scale + direction_offset * strength, 0, 1)"
    assert data["strengths"] == list(EXPECTED_STRENGTHS)
    assert data["intensities"] == [0.5, 1.0]
    assert data["regions"] == [*prep["roi"], "full_frame"]
    assert data["condition_count"] == len(data["results"]) == 63
    assert set(data["constructed_target_metrics"]) == {
        f"{direction}_s{strength:.1f}"
        for direction in EXPECTED_CASTS for strength in EXPECTED_STRENGTHS
    }

    expected_methods = {
        ("unchanged_source", 0.0),
        *((method, intensity)
          for method in ("Reinhard", "MeanStddev", "Histogram")
          for intensity in (0.5, 1.0)),
    }
    for direction, (scale, offset) in EXPECTED_CASTS.items():
        for strength in EXPECTED_STRENGTHS:
            cast_id = f"{direction}_s{strength:.1f}"
            rows = [row for row in data["results"]
                    if row["direction"] == direction and row["strength"] == strength]
            assert len(rows) == 7
            assert {(row["method"], row["intensity"]) for row in rows} == expected_methods
            assert all(row["scale"] == scale and row["offset"] == offset
                       and row["offset_multiplier"] == strength for row in rows)
            target = np.load(OUT / f"{cast_id}_target.npy", allow_pickle=False)
            assert target.shape == (851, 1280, 3) and np.isfinite(target).all()
            assert target.min() >= 0 and target.max() <= 1
            assert (OUT / f"{cast_id}_target.png").is_file()
            target_regions = data["constructed_target_metrics"][cast_id]
            assert set(target_regions) == set(data["regions"])
            for metrics in target_regions.values():
                assert np.isfinite([*metrics["Lab_mean"], metrics["Cstar_from_mean_ab"],
                                    metrics["hue_circular_mean_deg"]]).all()

    for row in data["results"]:
        assert set(row["regions"]) == set(data["regions"])
        for region in row["regions"].values():
            assert region["non_finite_values"] == 0
            assert np.isfinite([
                region["deltaE76_mean_to_source"], region["deltaE76_p95_to_source"],
                region["deltaE76_mean_to_constructed_target"],
                region["deltaE76_p95_to_constructed_target"],
                region["encoded_sRGB_MAE_to_constructed_target"],
                region["Cstar_from_mean_ab"], region["hue_circular_mean_deg"],
            ]).all()

    bad = [name for name, digest in data["files"].items()
           if not (OUT / name).is_file() or sha(OUT / name) != digest]
    assert not bad, f"Missing or hash-mismatched files: {bad}"
    expected_artifacts = 9 * (1 + 3 * 2) * 2 + 4
    assert len(data["files"]) == expected_artifacts == 130
    print("RD12 valid: 9 cast targets × 7 conditions, five regions, 130 hashed artifacts")


if __name__ == "__main__":
    main()
