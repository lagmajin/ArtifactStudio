"""Audit RD11 cast-direction sensitivity conditions and file hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd11_cast_direction_sensitivity"
MANIFEST = OUT / "manifest.json"
SCRIPT = ROOT / "run_rd11_cast_direction_sensitivity.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
EXPECTED_CASTS = {
    "cool": ([0.94, 1.015, 1.075], [0.012, 0.006, -0.008]),
    "warm": ([1.06, 1.0, 0.94], [-0.008, 0.006, 0.012]),
    "green": ([0.98, 1.05, 0.98], [0.006, -0.006, 0.006]),
}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    prep = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    assert data["script_sha256"] == sha(SCRIPT)
    assert data["visual_review"]["generator_sha256"] == sha(ROOT / "make_rd11_visual_sheets.py")
    assert len(data["visual_review"]["sheets"]) == 4
    assert data["input"]["prep_manifest_sha256"] == sha(PREP / "manifest.json")
    assert data["input"]["core_source_sha256"] == EXPECTED_CORE_SHA256 == sha(CORE)
    assert data["condition_count"] == len(data["results"]) == 21
    assert data["regions"] == [*prep["roi"], "full_frame"]
    for cast, (scale, offset) in EXPECTED_CASTS.items():
        rows = [r for r in data["results"] if r["cast"] == cast]
        assert len(rows) == 7
        assert {(r["method"], r["intensity"]) for r in rows} == {
            ("unchanged_source", 0.0),
            *((method, i) for method in ("Reinhard", "MeanStddev", "Histogram") for i in (0.5, 1.0)),
        }
        assert all(r["scale"] == scale and r["offset"] == offset for r in rows)
        assert set(data["constructed_target_metrics"][cast]) == set(data["regions"])
        for target_region in data["constructed_target_metrics"][cast].values():
            assert np.isfinite([*target_region["Lab_mean"], target_region["Cstar_from_mean_ab"],
                                target_region["hue_circular_mean_deg"]]).all()
        target = np.load(OUT / f"{cast}_target.npy", allow_pickle=False)
        assert target.shape == (851, 1280, 3) and np.isfinite(target).all()
        assert (OUT / f"{cast}_target.png").is_file()
    for row in data["results"]:
        assert set(row["regions"]) == set(data["regions"])
        for region in row["regions"].values():
            assert region["non_finite_values"] == 0
            assert np.isfinite([region["deltaE76_mean_to_source"],
                                region["deltaE76_mean_to_constructed_target"],
                                region["Cstar_from_mean_ab"],
                                region["hue_circular_mean_deg"]]).all()
    bad = [name for name, digest in data["files"].items()
           if not (OUT / name).is_file() or sha(OUT / name) != digest]
    assert not bad, f"Artifact hash mismatch: {bad}"
    assert len(data["files"]) == 46
    print("RD11 valid: 3 cast directions × 7 conditions, five regions, 42 artifact hashes")


if __name__ == "__main__":
    main()
