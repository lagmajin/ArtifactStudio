"""Audit RD1 crop-sheet provenance and hashes."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd1_visual_review"
RD1 = ROOT / "captures" / "rd1_source_mirror"
GENERATOR = ROOT / "make_rd1_visual_review.py"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    manifest = json.loads((CAPTURE / "manifest.json").read_text(encoding="utf-8"))
    rd1 = json.loads((RD1 / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["rd"] == "rd1-visual-review-preparation"
    assert manifest["source_portrait_sha256"] == rd1["source"]["sha256"]
    assert manifest["core_source_sha256"] == rd1["implementation_boundary"]["source_sha256"]
    assert manifest["runner_sha256"] == rd1["implementation_boundary"]["mirror_script_sha256"]
    assert manifest["generator_sha256"] == sha(GENERATOR)
    assert set(manifest["artifacts"]) == {"face_candidate_roi.png", "inserted_neutral_card.png"}
    for filename, item in manifest["artifacts"].items():
        assert sha(CAPTURE / filename) == item["sha256"]
        for source in item["source_files"]:
            assert source in rd1["files"] and sha(RD1 / source) == rd1["files"][source]
    print("RD1 visual sheets valid: 2 crops, 8 source previews each")


if __name__ == "__main__":
    main()
