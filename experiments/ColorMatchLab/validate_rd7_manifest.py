"""Audit RD7 natural-scene pair provenance, regions, and output hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets" / "MiddleburyNaturalScenes" / "CanonEOS5D"
CAPTURE = ROOT / "captures" / "rd7_natural_scene_pair"
MANIFEST = CAPTURE / "manifest.json"
SCRIPT = ROOT / "run_rd7_natural_scene_pair.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUNNER = ROOT / "run_rd1_source_mirror.py"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["rd"] == "rd7-middlebury-natural-scene-raw-jpeg-pair"
    pair = manifest["pair"]
    assert pair["source_sha256"] == sha(ASSETS / pair["source"])
    assert pair["reference_sha256"] == sha(ASSETS / pair["reference"])
    assert pair["dimensions"] == [1000, 667]
    assert pair["reference_role"] == "camera rendition; not colorimetric ground truth"
    assert manifest["method_source"]["sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert manifest["method_source"]["runner_sha256"] == sha(RUNNER)
    assert manifest["method_source"]["script_sha256"] == sha(SCRIPT)

    source = np.load(CAPTURE / "source_srgb_from_linear_raw.npy", allow_pickle=False)
    reference = np.load(CAPTURE / "reference_camera_jpeg_srgb.npy", allow_pickle=False)
    assert source.shape == reference.shape == (667, 1000, 3)
    assert np.isfinite(source).all() and np.isfinite(reference).all()
    assert source.min() >= 0 and source.max() <= 1
    assert reference.min() >= 0 and reference.max() <= 1
    assert manifest["roi"]["light_wall_candidate"]["xyxy"] == [20, 108, 155, 164]
    assert manifest["roi"]["light_wall_candidate"]["status"] == "unreviewed candidate region"
    assert manifest["roi"]["refrigerator_detail_candidate"]["status"] == "unreviewed candidate region"
    for name in ("light_wall_candidate", "refrigerator_detail_candidate"):
        mask = np.asarray(Image.open(CAPTURE / f"roi_{name}.png").convert("L")) > 0
        assert int(mask.sum()) == manifest["roi"][name]["pixels"] > 0

    results = manifest["results"]
    assert len(results) == 10
    assert {(r["method"], r["intensity"]) for r in results} == {
        ("unchanged_source", 0.0),
        *((method, intensity) for method in ("Reinhard", "MeanStddev", "Histogram")
          for intensity in (0.25, 0.5, 1.0)),
    }
    for row in results:
        assert set(row["regions"]) == set(manifest["roi"])
        for stats in row["regions"].values():
            assert stats["non_finite_values"] == 0
            assert "output_luminance_gradient" in stats

    bad = [name for name, expected in manifest["files"].items()
           if not (CAPTURE / name).is_file() or sha(CAPTURE / name) != expected]
    assert not bad, f"Missing or hash-mismatched artifacts: {bad}"
    print(f"RD7 manifest valid: 10 conditions, 3 regions, {len(manifest['files'])} artifact hashes")


if __name__ == "__main__":
    main()
