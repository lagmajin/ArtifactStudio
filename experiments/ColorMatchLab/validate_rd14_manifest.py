"""Audit RD14 transparent-area dose grid, inputs, metrics and output hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd14_transparent_area_dose"
MANIFEST = OUT / "manifest.json"
SCRIPT = ROOT / "run_rd14_transparent_area_dose.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
FRACTIONS = (0.0, 0.01, 0.05, 0.10, 0.20)
METHODS = {"Reinhard", "MeanStddev", "Histogram"}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    prep = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    assert data["script_sha256"] == sha(SCRIPT)
    assert data["source"]["prep_manifest_sha256"] == sha(PREP / "manifest.json")
    assert data["source"]["source_sha256"] == sha(PREP / "source.png")
    assert data["method"]["core_source_sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert data["method"]["mirror_source_sha256"] == sha(ROOT / "run_rd1_source_mirror.py")
    assert data["target"]["sha256"] == sha(PREP / "synthetic_cast_reference.png")
    assert data["alpha"]["fractions_requested"] == list(FRACTIONS)
    assert data["method"]["intensity"] == 1.0
    assert data["condition_pair_count"] == len(data["results"]) == 15
    assert len(data["visual_review"]["sheets"]) == 3
    assert data["alpha"]["hidden_rgb_variants"] == {
        "black": [0.0, 0.0, 0.0], "magenta": [1.0, 0.0, 1.0]}

    expected_grid = {(method, fraction) for method in METHODS for fraction in FRACTIONS}
    assert {(row["method"], row["transparent_fraction_requested"])
            for row in data["results"]} == expected_grid
    for fraction in FRACTIONS:
        alpha = np.load(OUT / f"alpha_{fraction:.2f}.npy", allow_pickle=False)
        assert alpha.shape == (851, 1280)
        expected_columns = int(np.floor(1280 * fraction))
        assert np.count_nonzero(alpha == 0) == expected_columns * 851
        assert set(np.unique(alpha)) == {0.0, 1.0} if expected_columns else set(np.unique(alpha)) == {1.0}

    for row in data["results"]:
        fraction = row["transparent_fraction_requested"]
        assert row["hidden_variant_visible_input_identical"] is True
        assert row["transparent_columns"] == int(np.floor(1280 * fraction))
        assert np.isclose(row["transparent_pixel_fraction_actual"],
                          row["transparent_columns"] / 1280.0)
        assert set(row["regions"]) == {"visible_pixels", *prep["roi"].keys()}
        assert set(row["variant_output_metrics"]) == {"black", "magenta"}
        for region in row["regions"].values():
            assert region["variant_a_non_finite_values"] == 0
            assert region["variant_b_non_finite_values"] == 0
            numeric = [value for key, value in region.items()
                       if key not in ("pixels", "variant_a_range", "variant_b_range")]
            assert np.isfinite(numeric).all()
        for variant_regions in row["variant_output_metrics"].values():
            assert set(variant_regions) == set(row["regions"])
            for region in variant_regions.values():
                assert region["non_finite_values"] == 0
                assert np.isfinite([
                    region["deltaE76_mean_to_source"], region["deltaE76_p95_to_source"],
                    region["deltaE76_mean_to_target"], region["deltaE76_p95_to_target"],
                    region["encoded_sRGB_MAE_to_target"], region["Cstar_from_mean_ab"],
                    region["hue_circular_mean_deg"],
                ]).all()
        if fraction == 0.0:
            for region in row["regions"].values():
                assert region["encoded_sRGB_MAE_between_variants"] == 0.0
                assert region["DeltaE76_mean_between_variants"] == 0.0
                assert region["DeltaE76_max_between_variants"] == 0.0

    bad = [name for name, digest in data["files"].items()
           if not (OUT / name).is_file() or sha(OUT / name) != digest]
    assert not bad, f"Missing or hash-mismatched artifacts: {bad}"
    assert len(data["files"]) == 68
    print("RD14 valid: 5 transparent fractions × 3 methods, zero-area controls and 68 hashed artifacts")


if __name__ == "__main__":
    main()
