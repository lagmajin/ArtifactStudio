"""Audit RD6 ColorChecker pair provenance, ROI coverage, and output hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets" / "MiddleburyColorChecker24" / "CanonEOS20D"
CAPTURE = ROOT / "captures" / "rd6_colorchecker_pair"
MANIFEST = CAPTURE / "manifest.json"
SCRIPT = ROOT / "run_rd6_colorchecker_pair.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUNNER = ROOT / "run_rd1_source_mirror.py"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
EXPECTED_ROI_PIXELS = 60 * 60


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["rd"] == "rd6-middlebury-colorchecker-wb-pair"
    assert manifest["pair"]["dimensions"] == [606, 404]
    assert manifest["pair"]["source_sha256"] == sha(ASSETS / manifest["pair"]["source"])
    assert manifest["pair"]["reference_sha256"] == sha(ASSETS / manifest["pair"]["reference"])
    assert manifest["pair"]["target_role"] == "camera auto-WB rendition; not colorimetric ground truth"
    assert manifest["method_source"]["sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert manifest["method_source"]["runner_sha256"] == sha(RUNNER)
    assert manifest["method_source"]["script_sha256"] == sha(SCRIPT)

    source = np.load(CAPTURE / "source_wb1_srgb.npy", allow_pickle=False)
    reference = np.load(CAPTURE / "reference_wb2_srgb.npy", allow_pickle=False)
    assert source.shape == reference.shape == (404, 606, 3)
    assert np.isfinite(source).all() and np.isfinite(reference).all()
    assert source.min() >= 0 and source.max() <= 1
    assert reference.min() >= 0 and reference.max() <= 1
    for name in ("skin_tone_proxy", "white_patch", "neutral_gray_patches", "all_chart_patches"):
        roi = np.asarray(Image.open(CAPTURE / f"roi_{name}.png").convert("L")) > 0
        assert roi.shape == (404, 606)
        if name != "all_chart_patches":
            assert int(roi.sum()) == EXPECTED_ROI_PIXELS * {
                "skin_tone_proxy": 2, "white_patch": 1, "neutral_gray_patches": 5,
            }[name]
        else:
            assert int(roi.sum()) == EXPECTED_ROI_PIXELS * 24

    results = manifest["results"]
    assert len(results) == 10
    assert {(row["method"], row["intensity"]) for row in results} == {
        ("unchanged_source", 0.0),
        *((method, intensity) for method in ("Reinhard", "MeanStddev", "Histogram")
          for intensity in (0.25, 0.5, 1.0)),
    }
    for row in results:
        assert {"skin_tone_proxy", "white_patch", "neutral_gray_patches",
                "all_chart_patches"}.issubset(row["regions"])
        for stats in row["regions"].values():
            assert stats["non_finite_values"] == 0

    bad = [name for name, expected in manifest["files"].items()
           if not (CAPTURE / name).is_file() or sha(CAPTURE / name) != expected]
    assert not bad, f"Missing or hash-mismatched artifacts: {bad}"
    print(f"RD6 manifest valid: 10 conditions, 24 patch ROIs plus grouped masks, "
          f"{len(manifest['files'])} artifact hashes valid")


if __name__ == "__main__":
    main()
