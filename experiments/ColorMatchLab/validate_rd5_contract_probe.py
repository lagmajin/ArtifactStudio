"""Audit RD5 result provenance and expected mirror tolerances."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd5_contract_vector_probe"
TEST_SOURCE = ROOT.parent.parent / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
CORE_SOURCE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    result_path = CAPTURE / "result.json"
    result = json.loads(result_path.read_text(encoding="utf-8"))
    manifest = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    assert result["rd"] == "rd5-existing-contract-vector-probe"
    assert result["core_source"]["sha256"] == EXPECTED_CORE_SHA256 == sha(CORE_SOURCE)
    assert result["test_source"]["sha256"] == sha(TEST_SOURCE)
    assert result["mean_stddev_dispatch_vector"]["max_rgb_abs_error"] == 0.0
    assert result["histogram_vector"]["max_rgb_abs_error"] <= 1.0 / 255.0
    assert result["alpha_preserved_by_probe_harness"]
    assert manifest["result_sha256"] == sha(result_path)
    print("RD5 source-vector check valid: two mirror vectors within recorded tolerances")


if __name__ == "__main__":
    main()
