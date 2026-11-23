"""RD2: descriptor-consistency fix comparison (v1 vs v2).

Applies both the RD1 model (unpremultiply first) and the RD2 model
(premultiplied-space processing) to straight and premultiplied versions of the
alpha fixture. Records the recovered-straight delta for each combination.

No pass/fail judgment: gate thresholds are still unset. Offline only.
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
    save_output,
    sha256,
)
from rd1_candidates import ALL_CANDIDATES, CANDIDATE_SEED_BASE
from watercolor_reference import apply_candidate as apply_v1
from watercolor_reference_v2 import apply_candidate_v2


def recovered_straight_delta(straight_out: np.ndarray, premul_out: np.ndarray) -> float:
    a = straight_out[..., 3:4]
    valid = a[..., 0] > 0.01
    recovered = premul_out[..., :3] / np.maximum(a, 1e-6)
    return float(np.max(np.abs(recovered - straight_out[..., :3])[valid]))


def run(output_rd0: Path, output_rd2: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd2.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd2 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd2 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "WatercolorLab",
        "rd": "rd2",
        "stage": "descriptor_fix_comparison",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "models": {
            "v1": "unpremultiply -> process straight RGB -> re-premultiply (RD1)",
            "v2": "process premultiplied RGB + alpha -> single final unpremultiply (RD2)",
        },
        "candidates": ALL_CANDIDATES,
        "comparisons": {},
        "opaque_parity": {},
        "definitions": {
            "recovered_straight_max_delta": "max |straight_out.RGB - premul_out.RGB / premul_out.alpha| over alpha > 0.01",
            "reference": "same fixture, same candidate spec, same seed, only descriptor differs",
        },
    }
    outputs = {}

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" not in entry:
            continue
        straight = np.load(output_rd0 / "fixtures" / entry["npy"])
        premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])

        for model_name, apply in (("v1", apply_v1), ("v2", apply_candidate_v2)):
            metrics["comparisons"][f"{key}_{model_name}"] = {}
            for cand_name, spec in ALL_CANDIDATES.items():
                seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
                s_out = apply(straight, spec, seed, descriptor="straight")
                p_out = apply(premul, spec, seed, descriptor="premultiplied")
                delta = recovered_straight_delta(s_out, p_out)
                stem = f"{key}_{model_name}_{cand_name}"
                outputs[stem] = save_output(cand_dir / stem, s_out)
                metrics["comparisons"][f"{key}_{model_name}"][cand_name] = {
                    "recovered_straight_max_delta": delta,
                    "straight_out_distribution": distribution(s_out),
                    "diff_from_input": diff_metrics(s_out, straight, {"description": "straight input (no-op baseline)"}),
                    "alpha_contract": alpha_contract_metrics(s_out, p_out),
                }

    # Opaque fixtures: v1 and v2 should agree because alpha == 1 makes
    # premultiplication an identity. This is a look-parity sanity check.
    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" in entry:
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        metrics["opaque_parity"][key] = {}
        for cand_name, spec in ALL_CANDIDATES.items():
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
            out_v1 = apply_v1(rgba, spec, seed, descriptor="straight")
            out_v2 = apply_candidate_v2(rgba, spec, seed, descriptor="straight")
            delta = float(np.max(np.abs(out_v1[..., :3] - out_v2[..., :3])))
            metrics["opaque_parity"][key][cand_name] = {
                "v1_v2_max_rgb_delta": delta,
                "note": "descriptive parity probe; no gate set",
            }
            stem = f"{key}_opaque_v2_{cand_name}"
            outputs[stem] = save_output(cand_dir / stem, out_v2)

    # Compact comparison table suitable for a reviewer to read before fixing a tolerance.
    table_lines = [
        "# RD2 descriptor comparison (blank conclusion)",
        "",
        "Same fixture, candidate and seed; only the input alpha descriptor differs.",
        "Values are descriptive. No gate is set, so no pass/fail is recorded here.",
        "",
        "| fixture | candidate | v1 max delta | v2 max delta |",
        "|---|---|---:|---:|",
    ]
    for key in fm["fixtures"]:
        if key == "sequence_small" or "premultiplied" not in fm["fixtures"][key]:
            continue
        for cand in ALL_CANDIDATES:
            v1 = metrics["comparisons"][f"{key}_v1"][cand]["recovered_straight_max_delta"]
            v2 = metrics["comparisons"][f"{key}_v2"][cand]["recovered_straight_max_delta"]
            table_lines.append(f"| {key} | {cand} | {v1:.3g} | {v2:.3g} |")
    table_lines.extend([
        "",
        "## Opaque v1/v2 look parity (alpha == 1)",
        "",
        "| fixture | candidate | v1/v2 max RGB delta |",
        "|---|---|---:|",
    ])
    for key, cands in metrics["opaque_parity"].items():
        for cand, value in cands.items():
            table_lines.append(f"| {key} | {cand} | {value['v1_v2_max_rgb_delta']:.3g} |")
    table_path = review_dir / "DESCRIPTOR_COMPARISON.md"
    table_path.write_text("\n".join(table_lines) + "\n", encoding="utf-8")

    metrics_path = output_rd2 / "metrics_rd2.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd2",
        "stage": "descriptor_fix_comparison",
        "date": "2026-11-17",
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
        "candidate_seed_base": CANDIDATE_SEED_BASE,
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "comparison_table": {"file": f"review/{table_path.name}", "sha256": sha256(table_path.read_bytes())},
        },
        "gates": "unset; no judgment or ranking performed",
        "unverified": [
            "human visual review and quality gate",
            "premultiated-space look parity against v1 on opaque plates",
            "product effect / UI / undo / save-reload",
            "preview and final render paths",
            "CPU/GPU parity, D3D12/Vulkan",
        ],
    }
    manifest_path = output_rd2 / "manifest_rd2.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd2")
    args = parser.parse_args()
    rd0, rd2 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    manifest = run(rd0, rd2)
    print(f"RD2 descriptor comparison complete: {len(manifest['outputs'])} outputs -> {rd2}")


if __name__ == "__main__":
    main()
