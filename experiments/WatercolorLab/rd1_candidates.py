"""RD1 candidate acquisition for the watercolor-style R&D.

Generates offline reference-model outputs and descriptive evidence for the
bleed / paper / edge candidates. It deliberately performs NO pass/fail
judgment and NO ranking: gate thresholds must be fixed by a separate reviewer
before final comparison (EVALUATION_PLAN.md section 5.0).

Offline only: no build, no Artifact launch, no product code changes.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from rd0_baseline import (
    alpha_contract_metrics,
    contact_sheet,
    diff_metrics,
    distribution,
    patch_color_metrics,
    roi_metrics,
    save_output,
    sha256,
    temporal_metrics,
)
from watercolor_reference import apply_candidate

FULL_CANDIDATES = {
    "wc_a": {"bleed": "bilateral", "paper": "value_noise", "edge": "softstep", "strength": 1.0},
    "wc_b": {"bleed": "quantize_gauss", "paper": "value_noise", "edge": "wobble", "strength": 1.0},
    "wc_c": {"bleed": "edge_weighted", "paper": "fibers", "edge": "softstep", "strength": 1.0},
}
ABLATION_CANDIDATES = {
    "only_bleed": {"bleed": "bilateral", "paper": None, "edge": None, "strength": 1.0},
    "only_paper": {"bleed": None, "paper": "value_noise", "edge": None, "strength": 1.0},
    "only_edge": {"bleed": None, "paper": None, "edge": "softstep", "strength": 1.0},
}
ALL_CANDIDATES = {**FULL_CANDIDATES, **ABLATION_CANDIDATES}
CANDIDATE_SEED_BASE = gf.SEED + 1000


def recovered_straight_delta(straight_out: np.ndarray, premul_out: np.ndarray) -> float:
    a = straight_out[..., 3:4]
    valid = a[..., 0] > 0.01
    recovered = premul_out[..., :3] / np.maximum(a, 1e-6)
    return float(np.max(np.abs(recovered - straight_out[..., :3])[valid]))


def run(output_rd0: Path, output_rd1: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd1.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd1 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd1 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "WatercolorLab",
        "rd": "rd1",
        "stage": "candidate_outputs",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "candidates": ALL_CANDIDATES,
        "fixtures": {},
        "sequence_small": {},
        "descriptor_check": {},
        "definitions": {
            "descriptor_policy": "unpremultiply -> process straight RGB -> re-premultiply",
            "paper_noise": "screen-locked deterministic noise with per-candidate seed",
            "luma_linear": "Rec.709 weights on linear RGB",
        },
    }
    outputs = {}
    blind_records: list[dict] = []

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small":
            continue
        is_small = key.endswith("_small")
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        rois = entry["rois"]
        metrics["fixtures"][key] = {}

        for cand_name, spec in ALL_CANDIDATES.items():
            if cand_name in ABLATION_CANDIDATES and not is_small:
                continue
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
            result = apply_candidate(rgba, spec, seed)
            stem = f"{key}_{cand_name}"
            outputs[stem] = save_output(cand_dir / stem, result)

            per_candidate = {
                "distribution": distribution(result),
                "rois": {label: roi_metrics(result, rect) for label, rect in rois.items()},
                "diff_from_input": diff_metrics(result, rgba, {"description": "input image (no-op baseline)"}),
                "patch_colors": patch_color_metrics(result, rois),
            }
            if "premultiplied" in entry:
                premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
                premul_result = apply_candidate(premul, spec, seed, descriptor="premultiplied")
                per_candidate["alpha_contract"] = alpha_contract_metrics(result, premul_result)
                per_candidate["descriptor_recovered_straight_max_delta"] = recovered_straight_delta(result, premul_result)
                metrics["descriptor_check"][stem] = {
                    "recovered_straight_rgb_max_delta": per_candidate["descriptor_recovered_straight_max_delta"],
                    "note": "descriptive; tolerance fixed by reviewer",
                }
            metrics["fixtures"][key][cand_name] = per_candidate

            if is_small:
                blind_records.append({"fixture": key, "candidate": cand_name})

    seq = fm["fixtures"]["sequence_small"]
    source_frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for cand_name, spec in FULL_CANDIDATES.items():
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
        frames = [apply_candidate(f, spec, seed) for f in source_frames]
        metrics["sequence_small"][cand_name] = temporal_metrics(frames, seq["rois"])
        stem = f"sequence_small_{cand_name}"
        outputs[stem] = save_output(cand_dir / stem, frames[3])

    # Blind IDs: candidate and fixture names are hidden on the sheet; key is separate.
    rng = np.random.default_rng(gf.SEED + 1900)
    order = rng.permutation(len(blind_records))
    blind_key = {}
    for i, idx in enumerate(order):
        blind_id = f"C{i + 1:02d}"
        rec = blind_records[int(idx)]
        blind_key[blind_id] = rec
    sheet_items = [(f"C{i + 1:02d}", cand_dir / (blind_records[int(idx)]["fixture"] + "_"
                                                + blind_records[int(idx)]["candidate"] + ".png"))
                   for i, idx in enumerate(order)]
    sheet_path = review_dir / "rd1_blind_sheet.png"
    contact_sheet(sheet_items, cols=4, cell_w=288, cell_h=200).save(sheet_path)

    key_path = output_rd1 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (review_dir / "CANDIDATE_BLIND_FORM.md").write_text(
        "# RD1 candidate blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed by a separate reviewer.\n\n"
        "| blind ID | full image notes | ROI notes | suspected defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n"
        + "".join(f"| {bid} | | | | | |\n" for bid in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd1 / "metrics_rd1.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd1",
        "stage": "candidate_outputs",
        "date": "2026-11-17",
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
        "candidate_seed_base": CANDIDATE_SEED_BASE,
        "candidates": ALL_CANDIDATES,
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet_path.name}", "sha256": sha256(sheet_path.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
        },
        "gates": "unset; no judgment or ranking performed",
        "unverified": [
            "human visual review and quality gate",
            "reference watercolor images (license not confirmed)",
            "product effect / UI / undo / save-reload",
            "preview and final render paths",
            "CPU/GPU parity, D3D12/Vulkan",
            "production performance at 1080p / 4K",
        ],
    }
    manifest_path = output_rd1 / "manifest_rd1.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd1")
    args = parser.parse_args()
    rd0, rd1 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    manifest = run(rd0, rd1)
    print(f"RD1 candidate outputs complete: {len(manifest['outputs'])} outputs -> {rd1}")


if __name__ == "__main__":
    main()
