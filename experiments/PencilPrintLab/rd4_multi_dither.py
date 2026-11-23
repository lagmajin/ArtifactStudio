"""RD4: multi-level ordered dithering comparison (v3 binary vs v4 multi).

Separate offline run on existing fixtures. Records descriptor delta, absolute
recovered-RGB probes, opaque parity, temporal metrics and runtime/memory.
v4 blind evidence is regenerated. Gates stay unset; no pass/fail or ranking.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import cv2
import numpy as np

import generate_fixtures as gf
from pencil_reference_v3 import apply_candidate_v3
from pencil_reference_v4 import apply_candidate_v4
from rd0_baseline import contact_sheet, diff_metrics, sha256, temporal
from rd2_descriptor_safe import PERF_FIXTURES, SEED_BASE, descriptor_delta, measure, probe

CANDIDATES = {
    "pp_b_binary": {"line": "sobel", "line_strength": 0.7, "tone": "bayer", "texture": "hatch", "strength": 1.0},
    "pp_b_multi4": {"line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4, "texture": "hatch", "strength": 1.0},
    "pp_b_multi8": {"line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 8, "texture": "hatch", "strength": 1.0},
    "pp_a": {"line": "sobel", "line_strength": 0.8, "tone": "posterize", "tone_levels": 6, "texture": "grain", "strength": 1.0},
    "pp_c": {"line": "alpha_edge", "line_strength": 0.8, "tone": "posterize", "tone_levels": 6, "texture": "hatch", "strength": 1.0},
}
BLIND_SEED = gf.SEED + 3500


def legacy_spec(spec: dict) -> dict:
    """v3 has no bayer_multi, so its baseline uses binary bayer."""
    out = dict(spec)
    if out.get("tone") == "bayer_multi":
        out["tone"] = "bayer"
    return out


def apply_v3(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    return apply_candidate_v3(rgba, legacy_spec(spec), seed, descriptor)


def run(output_rd0: Path, output_rd4: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd4.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd4 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd4 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd4", "stage": "multi_dither_comparison",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "models": {
            "v3": "binary Bayer baseline (bayer_multi mapped to binary bayer)",
            "v4": "multi-level ordered dither with RD3 per-pixel cap",
        },
        "alpha_fixtures": {}, "opaque_parity": {}, "temporal": {}, "performance": {},
    }
    outputs = {}
    for key in ("alpha_boundary_small", "alpha_boundary_large"):
        entry = fm["fixtures"][key]
        straight = np.load(output_rd0 / "fixtures" / entry["npy"])
        premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
        metrics["alpha_fixtures"][key] = {}
        for model, apply in (("v3", apply_v3), ("v4", apply_candidate_v4)):
            metrics["alpha_fixtures"][key][model] = {}
            for cand, spec in CANDIDATES.items():
                seed = SEED_BASE + list(CANDIDATES).index(cand)
                s = apply(straight, spec, seed, descriptor="straight")
                p = apply(premul, spec, seed, descriptor="premultiplied")
                stem = f"{key}_{model}_{cand}"
                np.save(cand_dir / (stem + ".npy"), s.astype(np.float32))
                outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}
                metrics["alpha_fixtures"][key][model][cand] = {
                    "descriptor_delta": descriptor_delta(s, p),
                    "absolute_probe": probe(s),
                    "diff_from_input": diff_metrics(s, straight, {"description": "straight input (no-op baseline)"}),
                }

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" in entry:
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        metrics["opaque_parity"][key] = {}
        for cand, spec in CANDIDATES.items():
            seed = SEED_BASE + list(CANDIDATES).index(cand)
            a = apply_v3(rgba, spec, seed, descriptor="straight")
            b = apply_candidate_v4(rgba, spec, seed, descriptor="straight")
            metrics["opaque_parity"][key][cand] = {
                "v3_v4_max_rgb_delta": float(np.max(np.abs(a[..., :3] - b[..., :3])))
            }

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for model, apply in (("v3", apply_v3), ("v4", apply_candidate_v4)):
        metrics["temporal"][model] = {}
        for cand, spec in CANDIDATES.items():
            seed = SEED_BASE + list(CANDIDATES).index(cand)
            out_frames = [apply(f, spec, seed, descriptor="straight") for f in frames]
            tm = temporal(out_frames, seq["rois"])
            metrics["temporal"][model][cand] = {
                "global_mae_per_step": [s["global_mae"] for s in tm["per_step"]],
                "static_flat_mae_per_step": [s["static_flat_mae"] for s in tm["per_step"]],
            }

    for key in PERF_FIXTURES:
        rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][key]["npy"])
        metrics["performance"][key] = {}
        for cand, spec in CANDIDATES.items():
            seed = SEED_BASE + list(CANDIDATES).index(cand)
            metrics["performance"][key][cand] = {
                "v3": measure(apply_v3, rgba, spec, seed),
                "v4": measure(apply_candidate_v4, rgba, spec, seed),
            }

    lines = [
        "# RD4 multi-dither comparison tables (blank conclusion)", "",
        "No gate is set; no pass/fail or ranking is recorded.", "",
        "## Alpha fixtures: descriptor delta and absolute probes", "",
        "| fixture | candidate | model | descriptor delta | visible max | near max | near >1 | near >10 | nonfinite |",
        "|---|---|---|---:|---:|---:|---:|---:|---:|",
    ]
    for key, models in metrics["alpha_fixtures"].items():
        for cand in CANDIDATES:
            for model in ("v3", "v4"):
                c = models[model][cand]
                p = c["absolute_probe"]
                lines.append(
                    f"| {key} | {cand} | {model} | {c['descriptor_delta']:.3g} | {p['visible_max']:.4g} | "
                    f"{p['near_transparent_max']:.4g} | {p['near_transparent_channels_gt_1']} | "
                    f"{p['near_transparent_channels_gt_10']} | {p['nonfinite_count']} |"
                )
    lines.extend(["", "## Opaque v3/v4 max RGB delta", "", "| fixture | candidate | max delta |", "|---|---|---:|"])
    for key, cands in metrics["opaque_parity"].items():
        for cand, v in cands.items():
            lines.append(f"| {key} | {cand} | {v['v3_v4_max_rgb_delta']:.4g} |")
    lines.extend(["", "## Temporal global MAE per step (v3 / v4)", "", "| candidate | v3 | v4 |", "|---|---|---|"])
    for cand in CANDIDATES:
        a = ",".join(f"{x:.6f}" for x in metrics["temporal"]["v3"][cand]["global_mae_per_step"])
        b = ",".join(f"{x:.6f}" for x in metrics["temporal"]["v4"][cand]["global_mae_per_step"])
        lines.append(f"| {cand} | {a} | {b} |")
    lines.extend(["", "## Performance", "", "| fixture | candidate | model | seconds | tracemalloc peak bytes |", "|---|---|---|---:|---:|"])
    for key, cands in metrics["performance"].items():
        for cand, models in cands.items():
            for model, perf in models.items():
                lines.append(f"| {key} | {cand} | {model} | {perf['seconds']:.4f} | {perf['tracemalloc_peak_bytes']} |")
    table = review_dir / "RD4_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    records = []
    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or not key.endswith("_small"):
            continue
        for cand in CANDIDATES:
            records.append({"fixture": key, "candidate": cand, "model": "v4"})
    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"C{i + 1:02d}"
        rec = records[int(idx)]
        blind_key[bid] = rec
        stem = f"{rec['fixture']}_v4_{rec['candidate']}"
        path = cand_dir / (stem + ".png")
        if not path.exists():
            rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][rec["fixture"]]["npy"])
            out = apply_candidate_v4(rgba, CANDIDATES[rec["candidate"]],
                                     SEED_BASE + list(CANDIDATES).index(rec["candidate"]), descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd4_v4_blind_sheet.png"
    contact_sheet(items, 4, 288, 200).save(sheet)
    key_path = output_rd4 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "CANDIDATE_BLIND_FORM.md"
    form.write_text(
        "# RD4 (v4) candidate blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed.\n\n"
        "| blind ID | full image | ROI | defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n" + "".join(f"| {b} | | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd4 / "metrics_rd4.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd4", "stage": "multi_dither_comparison", "date": "2026-11-17",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": cv2.__version__, "platform": platform.platform()},
        "seed_base": SEED_BASE, "blind_seed": BLIND_SEED, "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "tables": {"file": f"review/{table.name}", "sha256": sha256(table.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
        },
        "gates": "unset; no judgment or ranking performed",
        "predecessor": "RD1-RD3 blind sets are retained as history; do not use them as v4 evidence",
        "unverified": ["human visual review and quality gate", "reference artwork images",
                       "product effect / UI / undo / save-reload", "preview and final render paths",
                       "CPU/GPU parity, D3D12/Vulkan", "production performance at 1080p / 4K"],
    }
    manifest_path = output_rd4 / "manifest_rd4.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd4")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD4 multi-dither comparison complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
