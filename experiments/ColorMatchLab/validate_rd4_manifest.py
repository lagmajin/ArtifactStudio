"""Audit RD4 fixed/distributed histogram area control."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd4_histogram_area_control"
MANIFEST = CAPTURE / "manifest.json"
SCRIPT = ROOT / "histogram_patch_area_control.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUNNER = ROOT / "run_rd1_source_mirror.py"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    rows = json.loads((CAPTURE / "results.json").read_text(encoding="utf-8"))
    assert manifest["rd"] == "rd4-histogram-patch-area-control"
    assert len(rows) == 20
    assert manifest["source_hashes"]["core"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert manifest["source_hashes"]["runner"] == sha(RUNNER)
    assert manifest["source_hashes"]["control_script"] == sha(SCRIPT)

    for level in manifest["levels_linear"]:
        fixed = [r for r in rows if r["target_linear"] == level and r["arm"] == "fixed_global_histogram"]
        growing = [r for r in rows if r["target_linear"] == level and r["arm"] == "patch_replaces_background"]
        assert len(fixed) == len(growing) == len(manifest["square_patch_sizes"])
        fixed_histograms = {tuple(r["source_histogram_hashes"] + r["reference_histogram_hashes"])
                            for r in fixed}
        assert len(fixed_histograms) == 1
        assert max(r["mean_deltaE76"] for r in fixed) - min(r["mean_deltaE76"] for r in fixed) < 1e-10
        assert len({tuple(r["source_histogram_hashes"] + r["reference_histogram_hashes"])
                    for r in growing}) == len(growing)

    growing_891 = [r for r in rows if r["target_linear"] == 0.891
                   and r["arm"] == "patch_replaces_background"]
    mapped_bins = [r["blue_mapping"]["mapped_reference_bin"] for r in growing_891]
    assert mapped_bins == [224, 227, 230, 235, 241]
    growing_892 = [r for r in rows if r["target_linear"] == 0.892
                   and r["arm"] == "patch_replaces_background"]
    assert {r["blue_mapping"]["mapped_reference_bin"] for r in growing_892} == {242}

    bad = [name for name, expected in manifest["files"].items()
           if not (CAPTURE / name).is_file() or sha(CAPTURE / name) != expected]
    assert not bad, f"Missing or hash-mismatched files: {bad}"
    print(f"RD4 manifest valid: {len(rows)} conditions; fixed histograms invariant; "
          f"area-dependent CDF changes reproduced; {len(manifest['files'])} artifact hashes valid")


if __name__ == "__main__":
    main()
