"""Check equation mirrors against fixed vectors in the existing Core contract source."""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import apply_mean_std, apply_histogram, sha, CORE_SOURCE, EXPECTED_CORE_SHA256

TEST_SOURCE = ROOT.parent.parent / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
OUT = ROOT / "captures" / "rd5_contract_vector_probe"


def bgra_to_rgb(pixels: np.ndarray) -> np.ndarray:
    return pixels[..., (2, 1, 0)].copy()


def rgb_to_bgra(pixels: np.ndarray) -> np.ndarray:
    return pixels[..., (2, 1, 0)].copy()


def main() -> None:
    if sha(CORE_SOURCE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("Core source changed; re-audit mirrors")
    source_bgra = np.array([[[0.0, 0.0, 0.0, 0.2],
                             [1.0, 2.0, 1.0, 0.8]]], dtype=np.float64)
    reference_bgra = np.array([[[1.0, 2.0, 0.25, 0.1],
                                [3.0, 6.0, 0.75, 0.9],
                                [1.0, 2.0, 0.25, 0.0],
                                [3.0, 6.0, 0.75, 0.2]]], dtype=np.float64)
    expected_bgra = reference_bgra[:, :2, :].copy()
    source_rgb = bgra_to_rgb(source_bgra)
    reference_rgb = bgra_to_rgb(reference_bgra)

    mean_rgb = apply_mean_std(source_rgb, reference_rgb, 1.0)
    mean_bgra = source_bgra.copy()
    mean_bgra[..., :3] = rgb_to_bgra(mean_rgb)
    mean_error = float(np.max(np.abs(mean_bgra[..., :3] - expected_bgra[..., :3])))

    hist_source_bgra = np.array([[[0.0, 0.0, 0.0, 0.3],
                                  [1.0, 1.0, 1.0, 0.7]]], dtype=np.float64)
    hist_reference_bgra = np.array([[[0.25, 0.25, 0.25, 0.1],
                                     [0.75, 0.75, 0.75, 0.9]]], dtype=np.float64)
    hist_expected = hist_reference_bgra[..., :3]
    hist_rgb = apply_histogram(bgra_to_rgb(hist_source_bgra),
                               bgra_to_rgb(hist_reference_bgra), 1.0)
    hist_output_bgra = hist_source_bgra.copy()
    hist_output_bgra[..., :3] = rgb_to_bgra(hist_rgb)
    hist_error = float(np.max(np.abs(hist_output_bgra[..., :3] - hist_expected)))
    alpha_preserved = bool(np.array_equal(mean_bgra[..., 3], source_bgra[..., 3])
                           and np.array_equal(hist_output_bgra[..., 3], hist_source_bgra[..., 3]))

    assert mean_error <= 1e-6, f"MeanStddev mirror error {mean_error}"
    assert hist_error <= 1.0 / 255.0, f"Histogram mirror error {hist_error}"
    assert alpha_preserved
    output = {
        "rd": "rd5-existing-contract-vector-probe",
        "status": "offline equation-mirror consistency only; no Core test executed",
        "test_source": {"path": "tests/ArtifactCore/AutoColorMatcherContractTest.cpp",
                        "sha256": sha(TEST_SOURCE)},
        "core_source": {"path": "ArtifactCore/src/Color/AutoColorMatch.cpp",
                         "sha256": sha(CORE_SOURCE)},
        "runner_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
        "buffer_layout": "BGRA input converted to semantic RGB arrays for mirror call, then converted back for expected-value comparison",
        "mean_stddev_dispatch_vector": {"max_rgb_abs_error": mean_error, "tolerance": 1e-6,
                                         "pass": mean_error <= 1e-6,
                                         "includes_values_above_one": True},
        "histogram_vector": {"max_rgb_abs_error": hist_error, "tolerance": 1.0 / 255.0,
                             "pass": hist_error <= 1.0 / 255.0},
        "alpha_preserved_by_probe_harness": alpha_preserved,
        "scope_limits": ["does not execute or compare C++", "does not validate Reinhard",
                         "does not test actual alpha handling", "one fixed vector per method"],
    }
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "result.json"
    path.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8", newline="\n")
    output["result_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    (OUT / "manifest.json").write_text(json.dumps(output, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
