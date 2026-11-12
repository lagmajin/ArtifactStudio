"""Run provenance-checked chromatic-alpha diagnostics on an isolated CC0 hair sprite."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from retro_lab import natural_alpha_sample_policy_detail_study, natural_alpha_sample_policy_study  # noqa: E402


def main() -> dict[str, object]:
    corpus = ROOT / "eval-inputs" / "cc0-hair-cell-study"
    output_dir = ROOT / "captures" / "hair-frequency"
    output_dir.mkdir(parents=True, exist_ok=True)
    asset = "summer_hair_cell_a.png"
    provenance = json.loads((corpus / "provenance.json").read_text(encoding="utf-8"))
    entry = provenance[asset]
    digest = hashlib.sha256((corpus / asset).read_bytes()).hexdigest()
    if digest != entry["source_sha256"]:
        raise ValueError(f"source crop hash mismatch: expected {entry['source_sha256']}, got {digest}")

    policy = natural_alpha_sample_policy_study(corpus, output_dir / "hair-cell-alpha-policy.png")
    detail = natural_alpha_sample_policy_detail_study(
        corpus,
        asset,
        (0, 0, 64, 64),
        output_dir / "hair-cell-alpha-detail.png",
    )
    report = {
        "asset": asset,
        "source sha256": digest,
        "source provenance": entry,
        "sampler study": "Existing RetroImageLab radial RGB chromatic sampler with current, alpha-zero fallback, partial-alpha fallback, and coverage-blend policies at R/B offsets 2, 4, and 8 pixels. Whole edge and partial-edge regions; normalized sRGB code-value alpha composite over black, mid-gray, and white (not linear-light).",
        "detail crop": [0, 0, 64, 64],
        "sampler metrics": policy,
        "detail composite metrics": detail,
        "limitations": [
            "One 64x64 pixel-art hair sprite isolated from a larger atlas; 24 partial-alpha pixels only.",
            "The 64x64 crop is the whole sprite; no individually labeled strand centerlines.",
            "Metric values compare candidate policies to current output and do not identify a preferred look.",
            "CPU Python reference study only; no Artifact runtime, GPU, or performance validation.",
        ],
    }
    report_path = output_dir / "hair-cell-alpha-metrics.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
    return report


if __name__ == "__main__":
    result = main()
    print(f"Saved hair-cell-alpha-metrics.json for {result['asset']}")
