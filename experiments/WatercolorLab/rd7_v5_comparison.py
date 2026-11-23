"""RD7: tuned fast shared-weight model (v5) vs v3 and v4.

Separate offline run comparing quality, temporal behavior and runtime/memory
on existing fixtures. Gates stay unset; no pass/fail or ranking is produced.
The v5 blind set is regenerated because the model changed.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from rd0_baseline import diff_metrics, save_output, sha256, temporal_metrics
from rd1_candidates import ALL_CANDIDATES, CANDIDATE_SEED_BASE, FULL_CANDIDATES
from rd3_v2_blind import run as build_blind_set
from rd4_v3_comparison import absolute_probe, recovered_straight_delta
from rd5_temporal_perf import measure
from watercolor_reference_v3 import apply_candidate_v3
from watercolor_reference_v4 import apply_candidate_v4
from watercolor_reference_v5 import DEFAULT_PARAMS, apply_candidate_v5

MODELS = {"v3": apply_candidate_v3, "v4": apply_candidate_v4, "v5": apply_candidate_v5}
BLIND_SEED = gf.SEED + 5500
PERF_FIXTURES = ("color_ramp_small", "color_ramp_large", "alpha_boundary_small")
PERF_CANDIDATES = ("only_bleed", "wc_b")


def run(output_rd0: Path, output_rd7: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd7.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd7 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd7 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "WatercolorLab",
        "rd": "rd7",
        "stage": "tuned_fast_model_comparison",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "v5_params": DEFAULT_PARAMS,
        "models": {
            "v3": "joint 4-channel bilateral with shared weights (RD4)",
            "v4": "fast shared-weight approximation, untuned defaults (RD6)",
            "v5": "fast shared-weight approximation, RD7-tuned parameters",
        },
        "alpha_fixtures": {},
        "opaque_parity": {},
        "temporal": {},
        "performance": {},
        "definitions": {
            "descriptor_delta": "max |straight_out.RGB - premul_out.RGB / premul_out.alpha| over alpha > 0.01",
            "visible": "alpha > 0.5",
            "near_transparent": "0.001 < alpha <= 0.5",
            "performance_scope": "offline Python reference model; tracemalloc is Python heap only",
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
            metrics["opaque_parity"][key][cand_name] = {
                "v4_v5_max_rgb_delta": float(np.max(np.abs(outs["v4"][..., :3] - outs["v5"][..., :3]))),
                "v3_v5_max_rgb_delta": float(np.max(np.abs(outs["v3"][..., :3] - outs["v5"][..., :3]))),
            }
            outputs[f"{key}_opaque_v5_{cand_name}"] = save_output(cand_dir / f"{key}_opaque_v5_{cand_name}", outs["v5"])

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for model_name, apply in MODELS.items():
        metrics["temporal"][model_name] = {}
        for cand_name in FULL_CANDIDATES:
            spec = ALL_CANDIDATES[cand_name]
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
            out_frames = [apply(f, spec, seed, descriptor="straight") for f in frames]
            tm = temporal_metrics(out_frames, seq["rois"])
            metrics["temporal"][model_name][cand_name] = {
                "global_mae_per_step": [s["global_mae"] for s in tm["per_step"]],
                "static_flat_mae_per_step": [s["static_flat_mae"] for s in tm["per_step"]],
                "aligned_mae": [a["aligned_mae"] for a in tm["integer_alignment_probe"]],
            }

    for fixture_key in PERF_FIXTURES:
        rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][fixture_key]["npy"])
        metrics["performance"][fixture_key] = {}
        for cand_name in PERF_CANDIDATES:
            spec = ALL_CANDIDATES[cand_name]
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
            metrics["performance"][fixture_key][cand_name] = {
                name: measure(apply, rgba, spec, seed) for name, apply in MODELS.items()
            }

    lines = [
        "# RD7 v5 comparison tables (blank conclusion)",
        "",
        f"v5 params: {DEFAULT_PARAMS}. No gate is set; no pass/fail or ranking is recorded.",
        "",
        "## Alpha fixtures: descriptor delta and absolute probes",
        "",
        "| fixture | candidate | model | descriptor delta | visible max | near max | near >10 channels |",
        "|---|---|---|---:|---:|---:|---:|",
    ]
    for key, models in metrics["alpha_fixtures"].items():
        for cand in ALL_CANDIDATES:
            for model in MODELS:
                cell = models[model][cand]
                p = cell["absolute_probe"]
                lines.append(
                    f"| {key} | {cand} | {model} | {cell['descriptor_delta']:.3g} | "
                    f"{p['visible_max']:.4g} | {p['near_transparent_max']:.4g} | "
                    f"{p['near_transparent_channels_gt_10']} |"
                )
    lines.extend([
        "",
        "## Opaque max RGB delta",
        "",
        "| fixture | candidate | v4-v5 | v3-v5 |",
        "|---|---|---:|---:|",
    ])
    for key, cands in metrics["opaque_parity"].items():
        for cand, v in cands.items():
            lines.append(f"| {key} | {cand} | {v['v4_v5_max_rgb_delta']:.4g} | {v['v3_v5_max_rgb_delta']:.4g} |")
    lines.extend([
        "",
        "## Temporal: global MAE per step (v3 / v4 / v5)",
        "",
        "| candidate | v3 | v4 | v5 |",
        "|---|---|---|---|",
    ])
    for cand in FULL_CANDIDATES:
        cols = []
        for model in MODELS:
            cols.append(",".join(f"{x:.6f}" for x in metrics["temporal"][model][cand]["global_mae_per_step"]))
        lines.append(f"| {cand} | " + " | ".join(cols) + " |")
    lines.extend([
        "",
        "## Performance (offline reference model; tracemalloc = Python heap only)",
        "",
        "| fixture | candidate | model | seconds | tracemalloc peak bytes | finite output |",
        "|---|---|---|---:|---:|---|",
    ])
    for fixture, cands in metrics["performance"].items():
        for cand, models in cands.items():
            for model, perf in models.items():
                lines.append(
                    f"| {fixture} | {cand} | {model} | {perf['seconds']:.4f} | "
                    f"{perf['tracemalloc_peak_bytes']} | {perf['output_finite']} |"
                )
    table_path = review_dir / "RD7_TABLES.md"
    table_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    metrics_path = output_rd7 / "metrics_rd7.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    blind_manifest = build_blind_set(
        output_rd0, output_rd7 / "blind", model_name="v5", apply=apply_candidate_v5,
        blind_seed=BLIND_SEED, manifest_name="manifest_rd7_blind.json", sheet_name="rd7_v5_blind_sheet.png",
    )

    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd7",
        "stage": "tuned_fast_model_comparison",
        "date": "2026-11-17",
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
        "v5_params": DEFAULT_PARAMS,
        "blind_seed": BLIND_SEED,
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "tables": {"file": f"review/{table_path.name}", "sha256": sha256(table_path.read_bytes())},
        },
        "blind_run": {
            "manifest": "blind/manifest_rd7_blind.json",
            "blind_sheet": "blind/review/rd7_v5_blind_sheet.png",
            "blind_key": "blind/blind_key.json",
            "blind_ids": len(json.loads((output_rd7 / "blind" / "blind_key.json").read_text(encoding="utf-8"))),
        },
        "gates": "unset; no judgment or ranking performed",
        "unverified": [
            "human visual review and quality gate",
            "product performance at 1080p / 4K",
            "native OpenCV allocation footprint (tracemalloc is Python-heap only)",
            "GPU/CPU parity, D3D12/Vulkan",
            "product effect / UI / undo / save-reload",
        ],
    }
    manifest_path = output_rd7 / "manifest_rd7.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd7")
    args = parser.parse_args()
    rd0, rd7 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    manifest = run(rd0, rd7)
    print(f"RD7 v5 comparison complete: {len(manifest['outputs'])} outputs -> {rd7}")


if __name__ == "__main__":
    main()
