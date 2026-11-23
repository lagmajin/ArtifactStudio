"""Audit the committed RD1 source-mirror manifest and its artifact hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd1_source_mirror"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUNNER = ROOT / "run_rd1_source_mirror.py"
MANIFEST = CAPTURE / "manifest.json"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
METHODS = ("Reinhard_source_mirror", "MeanStddev_source_mirror", "Histogram_source_mirror")
INTENSITIES = (0.25, 0.5, 1.0)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["rd"] == "rd1-source-mirror"
    assert manifest["implementation_boundary"]["source_sha256"] == EXPECTED_CORE_SHA256
    assert sha(CORE_SOURCE) == EXPECTED_CORE_SHA256, "Core source changed; re-audit before using captures"
    assert manifest["implementation_boundary"]["mirror_script_sha256"] == sha(RUNNER)

    reference = manifest["reference"]
    assert "[0,1]" in reference["input_domain"]
    source = np.load(CAPTURE / "portrait_source_linear.npy", allow_pickle=False)
    cast = np.load(CAPTURE / "portrait_reference_linear.npy", allow_pickle=False)
    assert source.shape == cast.shape == (1024, 1536, 3)
    assert np.isfinite(source).all() and np.isfinite(cast).all()
    assert source.min() >= 0 and source.max() <= 1
    assert cast.min() >= 0 and cast.max() <= 1
    assert 0.0 <= reference["unclipped_pixel_ratio"] <= 1.0
    assert 0 <= reference["saturated_pixels"]
    assert reference["saturated_pixels"] + round(
        reference["unclipped_pixel_ratio"] * 1536 * 1024
    ) == 1536 * 1024

    regions = set(manifest["conditions"]["masks"])
    regions.update(("whole_frame", "unclipped_invertible_pixels", "skin_roi_candidate_unclipped",
                    "bright_window_roi_candidate_unclipped", "saturated_by_cast"))
    neutral_roi = np.zeros((1024, 1536), dtype=bool)
    neutral_roi[640:736, 64:192] = True
    assert int(neutral_roi.sum()) == 96 * 128
    assert manifest["conditions"]["masks"]["inserted_neutral_patch"]["rect_xyxy"] == [64, 640, 192, 736]
    neutral_linear = ((0.82 + 0.055) / 1.055) ** 2.4
    cast_neutral_encoded = np.clip(0.82 * np.array([0.94, 1.015, 1.075])
                                   + np.array([0.012, 0.006, -0.008]), 0, 1)
    cast_neutral_linear = np.where(
        cast_neutral_encoded > 0.04045,
        ((cast_neutral_encoded + 0.055) / 1.055) ** 2.4,
        cast_neutral_encoded / 12.92,
    )
    assert np.allclose(source[640:736, 64:192], neutral_linear, atol=1e-6)
    assert np.allclose(cast[640:736, 64:192], cast_neutral_linear, atol=1e-6)
    expected = {f"{method}_inverse_i{intensity:.2f}"
                for method in METHODS for intensity in INTENSITIES}
    recovery = manifest["inverse_cast_recovery"]
    assert set(recovery) == expected
    for method in METHODS:
        forward = [r for r in manifest["results"] if r["method"] == method]
        assert {r["intensity"] for r in forward} == set(INTENSITIES)
        for row in forward:
            assert regions.issubset(row["regions"])
    for condition in recovery.values():
        assert regions.issubset(condition)
        for stats in condition.values():
            assert stats["pixels"] > 0
            assert stats["output_non_finite_values"] == 0

    bad = [name for name, expected_hash in manifest["files"].items()
           if not (CAPTURE / name).is_file() or sha(CAPTURE / name) != expected_hash]
    assert not bad, f"Missing or hash-mismatched artifacts: {bad}"
    print(f"RD1 manifest valid: {len(manifest['files'])} artifacts, "
          f"{len(expected)} inverse conditions, {len(regions)} required regions")


if __name__ == "__main__":
    main()
