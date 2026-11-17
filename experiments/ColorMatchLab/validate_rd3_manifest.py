"""Audit RD3 sweep coverage, boundary diagnostics, and artifact hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd3_histogram_sweep"
MANIFEST = CAPTURE / "manifest.json"
SCRIPT = ROOT / "histogram_response_sweep.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUNNER = ROOT / "run_rd1_source_mirror.py"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    rows = json.loads((CAPTURE / "response.json").read_text(encoding="utf-8"))
    assert manifest["rd"] == "rd3-histogram-neutral-level-sweep"
    assert len(rows) == 141
    assert abs(rows[0]["target_linear"] - 0.84) < 1e-12
    assert abs(rows[-1]["target_linear"] - 0.98) < 1e-12
    assert manifest["source_hashes"]["core"] == EXPECTED_CORE_SHA256
    assert sha(CORE) == EXPECTED_CORE_SHA256
    assert manifest["source_hashes"]["runner"] == sha(RUNNER)
    assert manifest["source_hashes"]["sweep_script"] == sha(SCRIPT)

    before = next(row for row in rows if abs(row["target_linear"] - 0.891) < 1e-12)
    after = next(row for row in rows if abs(row["target_linear"] - 0.892) < 1e-12)
    blue_before, blue_after = before["mapping"]["B"], after["mapping"]["B"]
    assert (blue_before["source_patch_bin"], blue_after["source_patch_bin"]) == (227, 228)
    assert abs(blue_before["source_patch_cdf"] - 0.9186279296875) < 1e-12
    assert blue_after["source_patch_cdf"] == 1.0
    assert (blue_before["mapped_reference_bin"], blue_after["mapped_reference_bin"]) == (235, 242)
    assert abs(before["mean_deltaE76"] - 7.5812274226372995) < 1e-9
    assert abs(after["mean_deltaE76"] - 0.36659912968159336) < 1e-9

    bad = [name for name, expected in manifest["files"].items()
           if not (CAPTURE / name).is_file() or sha(CAPTURE / name) != expected]
    assert not bad, f"Missing or hash-mismatched files: {bad}"
    print(f"RD3 manifest valid: {len(rows)} sweep points, CDF boundary reproduced, "
          f"{len(manifest['files'])} artifact hashes valid")


if __name__ == "__main__":
    main()
