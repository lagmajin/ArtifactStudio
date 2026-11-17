"""Audit RD13 hidden-RGB paired fixture, visible metrics, and artifact hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd13_transparent_hidden_rgb"
MANIFEST = OUT / "manifest.json"
SCRIPT = ROOT / "run_rd13_transparent_hidden_rgb.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
EXPECTED_METHODS = {"Reinhard", "MeanStddev", "Histogram"}
EXPECTED_INTENSITIES = {0.5, 1.0}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    prep = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    implementation = data["implementation_boundary"]
    assert implementation["script_sha256"] == sha(SCRIPT)
    assert implementation["core_source_sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert implementation["mirror_source_sha256"] == sha(ROOT / "run_rd1_source_mirror.py")
    assert data["source"]["prep_manifest_sha256"] == sha(PREP / "manifest.json")
    assert data["source"]["source_sha256"] == sha(PREP / "source.png")
    assert data["target"]["sha256"] == sha(PREP / "synthetic_cast_reference.png")
    assert data["alpha"]["model"] == "straight alpha for exploratory preview composites; alpha is binary here"
    assert data["alpha"]["variants_have_identical_alpha"] is True
    assert data["alpha"]["variants_have_identical_visible_rgb"] is True
    assert data["hidden_rgb_variants"] == {"black": [0.0, 0.0, 0.0], "magenta": [1.0, 0.0, 1.0]}
    assert data["methods"] == ["Reinhard", "MeanStddev", "Histogram"]
    assert data["intensities"] == [0.5, 1.0]
    assert data["condition_pair_count"] == len(data["paired_results"]) == 6
    assert len(data["visual_review"]["sheets"]) == 6

    rgba_a = np.load(OUT / "source_black_hidden_rgba.npy", allow_pickle=False)
    rgba_b = np.load(OUT / "source_magenta_hidden_rgba.npy", allow_pickle=False)
    alpha = np.load(OUT / "shared_alpha.npy", allow_pickle=False)
    target = np.load(OUT / "constructed_target_rgb.npy", allow_pickle=False)
    assert rgba_a.shape == rgba_b.shape == target.shape[:2] + (4,)
    assert alpha.shape == target.shape[:2]
    visible = alpha > 0
    assert np.array_equal(rgba_a[..., 3], rgba_b[..., 3])
    assert np.array_equal(rgba_a[..., 3], alpha)
    assert np.array_equal(rgba_a[visible, :3], rgba_b[visible, :3])
    assert np.all(rgba_a[~visible, :3] == 0.0)
    assert np.all(rgba_b[~visible, :3] == [1.0, 0.0, 1.0])
    assert int(np.count_nonzero(~visible)) == data["alpha"]["transparent_pixel_count"]
    assert int(np.count_nonzero(visible)) == data["alpha"]["visible_pixel_count"]
    assert target.min() >= 0 and target.max() <= 1 and np.isfinite(target).all()

    expected_grid = {(method, intensity)
                     for method in EXPECTED_METHODS for intensity in EXPECTED_INTENSITIES}
    assert {(row["method"], row["intensity"]) for row in data["paired_results"]} == expected_grid
    for row in data["paired_results"]:
        assert row["alpha_preserved_exactly"] is True
        assert row["regions"]["visible_pixels"]["pixels"] == int(visible.sum())
        assert row["regions"]["skin_combined_reviewed"]["pixels"] > 0
        assert set(row["variant_output_metrics"]) == {"black", "magenta"}
        for region in row["regions"].values():
            numeric = [value for key, value in region.items()
                       if key not in ("pixels", "variant_a_range", "variant_b_range")]
            assert np.isfinite(numeric).all()
            assert region["variant_a_non_finite_values"] == 0
            assert region["variant_b_non_finite_values"] == 0
        for variant_regions in row["variant_output_metrics"].values():
            assert set(variant_regions) == set(row["regions"])
            for region in variant_regions.values():
                assert region["non_finite_values"] == 0
                assert np.isfinite([
                    region["deltaE76_mean_to_source"],
                    region["deltaE76_p95_to_source"],
                    region["deltaE76_mean_to_target"],
                    region["deltaE76_p95_to_target"],
                    region["encoded_sRGB_MAE_to_target"],
                    region["Cstar_from_mean_ab"],
                    region["hue_circular_mean_deg"],
                ]).all()
    bad = [name for name, digest in data["files"].items()
           if not (OUT / name).is_file() or sha(OUT / name) != digest]
    assert not bad, f"Missing or hash-mismatched artifacts: {bad}"
    assert len(data["files"]) == 36
    print("RD13 valid: 6 paired conditions, identical visible source/alpha, 36 hashed artifacts")


if __name__ == "__main__":
    main()
