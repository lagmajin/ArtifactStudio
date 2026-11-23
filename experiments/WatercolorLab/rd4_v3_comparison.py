"""RD4: v3 comparison with absolute recovered-RGB probes and a fresh blind set.

RD3 showed that "descriptor delta 0" in RD2 did not prove absolute soundness:
v2 bleed components left alpha untouched and divided moved premul by ~0 near
transparent pixels. v3 makes both bleed paths transform alpha with the same
kernel. This run records, per model and candidate:

  * descriptor delta (straight input vs premultiplied input)
  * absolute recovered-RGB max in visible pixels (alpha > 0.5)
  * absolute recovered-RGB max in near-transparent pixels (0.001 < alpha <= 0.5)
  * counts of near-transparent channels above 1.0 and above 10.0

It then regenerates blind evidence from v3 as a separate run. Gate thresholds
remain unset; no pass/fail or ranking is produced. Offline only.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from rd0_baseline import diff_metrics, save_output, sha256
from rd1_candidates import ALL_CANDIDATES, CANDIDATE_SEED_BASE
from rd3_v2_blind import BLIND_SEED, run as build_blind_set
from watercolor_reference import apply_candidate as apply_v1
from watercolor_reference_v2 import apply_candidate_v2
from watercolor_reference_v3 import apply_candidate_v3

MODELS = {"v1": apply_v1, "v2": apply_candidate_v2, "v3": apply_candidate_v3}
V3_BLIND_SEED = BLIND_SEED + 300


def recovered_straight_delta(straight_out: np.ndarray, premul_out: np.ndarray) -> float:
    a = straight_out[..., 3:4]
    valid = a[..., 0] > 0.01
    recovered = premul_out[..., :3] / np.maximum(a, 1e-6)
    return float(np.max(np.abs(recovered - straight_out[..., :3])[valid]))


def absolute_probe(out: np.ndarray) -> dict:
    rgb = out[..., :3]
    alpha = out[..., 3]
    visible = alpha > 0.5
    near = (alpha > 0.001) & (alpha <= 0.5)
    return {
        "nonfinite_count": int(np.sum(~np.isfinite(rgb))),
        "visible_max": float(rgb[visible].max()) if visible.any() else None,
        "visible_pixels": int(visible.sum()),
        "near_transparent_max": float(rgb[near].max()) if near.any() else None,
        "near_transparent_pixels": int(near.sum()),
        "near_transparent_channels_gt_1": int(np.sum(rgb[near] > 1.0)) if near.any() else 0,
        "near_transparent_channels_gt_10": int(np.sum(rgb[near] > 10.0)) if near.any() else 0,
    }


def run(output_rd0: Path, output_rd4: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd4.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd4 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd4 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "WatercolorLab",
        "rd": "rd4",
        "stage": "v3_comparison",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "models": {
            "v1": "unpremultiply -> process straight RGB -> re-premultiply",
            "v2": "premultiplied-space processing; bleed kernels do not transform alpha",
            "v3": "premultiplied-space processing; bleed kernels transform alpha with the same kernel",
        },
        "alpha_fixtures": {},
        "opaque_parity": {},
        "definitions": {
            "descriptor_delta": "max |straight_out.RGB - premul_out.RGB / premul_out.alpha| over alpha > 0.01",
            "visible": "alpha > 0.5",
            "near_transparent": "0.001 < alpha <= 0.5",
            "note": "descriptor delta measures input-descriptor consistency only; absolute probes measure soundness",
        },
    }
    outputs = {}

    for key in ("alpha_boundary_small", "alpha_boundary_large"):
        entry = fm["fixtures"][key]
        straight = np.load(output_rd0 / "fixtures" / entry["npy"])
        premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
        metrics["alpha_fixtures"][key] = {}
        for model_name, apply in MODELS.items():
            metrics["alpha_fixtures"][key][model_name] = {}
            for cand_name, spec in ALL_CANDIDATES.items():
                seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
                s_out = apply(straight, spec, seed, descriptor="straight")
                p_out = apply(premul, spec, seed, descriptor="premultiplied")
                stem = f"{key}_{model_name}_{cand_name}"
                outputs[stem] = save_output(cand_dir / stem, s_out)
                metrics["alpha_fixtures"][key][model_name][cand_name] = {
                    "descriptor_delta": recovered_straight_delta(s_out, p_out),
                    "absolute_probe": absolute_probe(s_out),
                    "diff_from_input": diff_metrics(s_out, straight, {"description": "straight input (no-op baseline)"}),
                }

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" in entry:
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        metrics["opaque_parity"][key] = {}
        for cand_name, spec in ALL_CANDIDATES.items():
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
            outs = {name: apply(rgba, spec, seed, descriptor="straight") for name, apply in MODELS.items()}
            deltas = {}
            for i, a_name in enumerate(MODELS):
                for b_name in list(MODELS)[i + 1 :]:
                    deltas[f"{a_name}_vs_{b_name}"] = float(
                        np.max(np.abs(outs[a_name][..., :3] - outs[b_name][..., :3]))
                    )
            metrics["opaque_parity"][key][cand_name] = deltas
            outputs[f"{key}_opaque_v3_{cand_name}"] = save_output(cand_dir / f"{key}_opaque_v3_{cand_name}", outs["v3"])

    lines = [
        "# RD4 comparison table (blank conclusion)",
        "",
        "Descriptor delta checks input-descriptor consistency; absolute probes check soundness.",
        "No gate is set, so no pass/fail or ranking is recorded here.",
        "",
        "| fixture | candidate | model | descriptor delta | visible max | near-transparent max | near >10 channels |",
        "|---|---|---|---:|---:|---:|---:|",
    ]
    for key, models in metrics["alpha_fixtures"].items():
        for cand in ALL_CANDIDATES:
            for model in MODELS:
                cell = models[model][cand]
                probe = cell["absolute_probe"]
                lines.append(
                    f"| {key} | {cand} | {model} | {cell['descriptor_delta']:.3g} | "
                    f"{probe['visible_max']:.4g} | {probe['near_transparent_max']:.4g} | "
                    f"{probe['near_transparent_channels_gt_10']} |"
                )
    lines.extend([
        "",
        "## Opaque look parity (v1 / v2 / v3 max RGB delta)",
        "",
        "| fixture | candidate | v1-v2 | v1-v3 | v2-v3 |",
        "|---|---|---:|---:|---:|",
    ])
    for key, cands in metrics["opaque_parity"].items():
        for cand, deltas in cands.items():
            lines.append(
                f"| {key} | {cand} | {deltas['v1_vs_v2']:.3g} | {deltas['v1_vs_v3']:.3g} | {deltas['v2_vs_v3']:.3g} |"
            )
    table_path = review_dir / "COMPARISON_RD4.md"
    table_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    metrics_path = output_rd4 / "metrics_rd4.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    # Fresh blind evidence from v3 as a separate run.
    blind_manifest = build_blind_set(
        output_rd0, output_rd4 / "blind", model_name="v3",
        apply=apply_candidate_v3, blind_seed=V3_BLIND_SEED,
        manifest_name="manifest_rd4_blind.json", sheet_name="rd4_v3_blind_sheet.png",
    )

    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd4",
        "stage": "v3_comparison",
        "date": "2026-11-17",
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
        "candidate_seed_base": CANDIDATE_SEED_BASE,
        "blind_seed": V3_BLIND_SEED,
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "comparison_table": {"file": f"review/{table_path.name}", "sha256": sha256(table_path.read_bytes())},
        },
        "blind_run": {
            "manifest": "blind/manifest_rd4_blind.json",
            "blind_sheet": "blind/review/rd4_v3_blind_sheet.png",
            "blind_key": "blind/blind_key.json",
            "blind_ids": len(json.loads((output_rd4 / "blind" / "blind_key.json").read_text(encoding="utf-8"))),
        },
        "gates": "unset; no judgment or ranking performed",
        "unverified": [
            "human visual review and quality gate",
            "reference watercolor images (license not confirmed)",
            "product effect / UI / undo / save-reload",
            "preview and final render paths",
            "CPU/GPU parity, D3D12/Vulkan",
        ],
    }
    manifest_path = output_rd4 / "manifest_rd4.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd4")
    args = parser.parse_args()
    rd0, rd4 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    manifest = run(rd0, rd4)
    print(f"RD4 comparison complete: {len(manifest['outputs'])} outputs -> {rd4}")


if __name__ == "__main__":
    main()
