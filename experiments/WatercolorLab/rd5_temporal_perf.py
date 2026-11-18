"""RD5: temporal evaluation and joint-bilateral runtime/memory probe for v3.

Records, as a separate run:
  * v3 temporal metrics on the 8-frame 0.5px/frame sequence (all candidates),
    plus v1/v2/v3 comparison for the three full candidates;
  * wall time and tracemalloc peak for the bilateral/quantize paths on existing
    fixtures (v2 vs v3, small and large);
  * a blind temporal contact sheet with its own key and blank review form.

Gate thresholds remain unset; no pass/fail or ranking is produced. Offline only.
"""

from __future__ import annotations

import argparse
import json
import platform
import time
import tracemalloc
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from rd0_baseline import contact_sheet, save_output, sha256, temporal_metrics
from rd1_candidates import ABLATION_CANDIDATES, ALL_CANDIDATES, CANDIDATE_SEED_BASE, FULL_CANDIDATES
from watercolor_reference import apply_candidate as apply_v1
from watercolor_reference_v2 import apply_candidate_v2
from watercolor_reference_v3 import apply_candidate_v3

MODELS = {"v1": apply_v1, "v2": apply_candidate_v2, "v3": apply_candidate_v3}
TEMPORAL_BLIND_SEED = gf.SEED + 3500
PERF_FIXTURES = ("color_ramp_small", "color_ramp_large", "alpha_boundary_small")
PERF_CANDIDATES = ("only_bleed", "wc_b")


def measure(apply, rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> dict:
    # Warm-up keeps JIT/allocator first-touch effects out of the reported run.
    apply(rgba, spec, seed, descriptor=descriptor)
    tracemalloc.start()
    t0 = time.perf_counter()
    out = apply(rgba, spec, seed, descriptor=descriptor)
    elapsed = time.perf_counter() - t0
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return {
        "seconds": float(elapsed),
        "tracemalloc_peak_bytes": int(peak),
        "pixels": int(rgba.shape[0] * rgba.shape[1]),
        "output_finite": bool(np.all(np.isfinite(out[..., :3]))),
    }


def run(output_rd0: Path, output_rd5: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd5.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd5 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd5 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "WatercolorLab",
        "rd": "rd5",
        "stage": "temporal_and_perf",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "models": {
            "v1": "unpremultiply -> process straight RGB -> re-premultiply",
            "v2": "premultiplied space, v1 bleed kernels",
            "v3": "premultiplied space, alpha-consistent bleed kernels",
        },
        "temporal": {},
        "temporal_model_comparison": {},
        "performance": {},
        "definitions": {
            "sequence": "8 frames, 0.5px/frame translation, 512x320",
            "performance_scope": "offline Python reference model only; not product performance",
            "tracemalloc": "Python heap peak only; excludes native OpenCV allocations",
        },
    }
    outputs = {}

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for cand_name, spec in ALL_CANDIDATES.items():
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
        out_frames = [apply_candidate_v3(f, spec, seed, descriptor="straight") for f in frames]
        metrics["temporal"][cand_name] = temporal_metrics(out_frames, seq["rois"])
        stem = f"sequence_small_v3_{cand_name}_frame03"
        outputs[stem] = save_output(cand_dir / stem, out_frames[3])

    for cand_name in FULL_CANDIDATES:
        spec = ALL_CANDIDATES[cand_name]
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
        metrics["temporal_model_comparison"][cand_name] = {}
        for model_name, apply in MODELS.items():
            out_frames = [apply(f, spec, seed, descriptor="straight") for f in frames]
            tm = temporal_metrics(out_frames, seq["rois"])
            metrics["temporal_model_comparison"][cand_name][model_name] = {
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
            metrics["performance"][fixture_key][cand_name] = {}
            for model_name, apply in MODELS.items():
                if model_name == "v1":
                    continue  # v1 has no joint-bilateral path; v2/v3 comparison is the point
                metrics["performance"][fixture_key][cand_name][model_name] = measure(apply, rgba, spec, seed)

    # Blind temporal contact sheet: 8 frames x 3 full candidates = 24 items.
    blind_records = []
    for cand_name in FULL_CANDIDATES:
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
        for k in range(len(frames)):
            blind_records.append({"candidate": cand_name, "frame": k, "model": "v3", "seed": seed})
    rng = np.random.default_rng(TEMPORAL_BLIND_SEED)
    order = rng.permutation(len(blind_records))
    blind_key = {}
    sheet_items = []
    for i, idx in enumerate(order):
        bid = f"T{i + 1:02d}"
        rec = blind_records[int(idx)]
        blind_key[bid] = rec
        stem = f"sequence_small_v3_{rec['candidate']}_frame{rec['frame']:02d}"
        if stem not in outputs:
            frame = apply_candidate_v3(frames[rec["frame"]], ALL_CANDIDATES[rec["candidate"]], rec["seed"], descriptor="straight")
            outputs[stem] = save_output(cand_dir / stem, frame)
        sheet_items.append((bid, cand_dir / (stem + ".png")))
    sheet_path = review_dir / "rd5_v3_temporal_blind_sheet.png"
    contact_sheet(sheet_items, cols=4, cell_w=256, cell_h=176).save(sheet_path)
    key_path = output_rd5 / "temporal_blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form_path = review_dir / "TEMPORAL_BLIND_FORM.md"
    form_path.write_text(
        "# RD5 temporal blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge flicker/shimmer,\n"
        "not the intentional 0.5px/frame motion. Blind IDs hide candidate and frame index.\n\n"
        "| blind ID | observed motion | suspected flicker | reviewer | date |\n"
        "|---|---|---|---|---|\n"
        + "".join(f"| {bid} | | | | |\n" for bid in blind_key),
        encoding="utf-8",
    )

    table_lines = [
        "# RD5 temporal and performance tables (blank conclusion)",
        "",
        "No gate is set; no pass/fail or ranking is recorded.",
        "",
        "## v3 temporal (per-step global MAE, static-flat MAE, aligned MAE)",
        "",
        "| candidate | global MAE steps | static-flat MAE steps | aligned MAE |",
        "|---|---|---|---|",
    ]
    for cand, tm in metrics["temporal"].items():
        g = ",".join(f"{x:.6f}" for x in [s["global_mae"] for s in tm["per_step"]])
        s = ",".join(f"{x:.6f}" for x in [st["static_flat_mae"] for st in tm["per_step"]])
        a = ",".join(f"{x['aligned_mae']:.6f}" for x in tm["integer_alignment_probe"])
        table_lines.append(f"| {cand} | {g} | {s} | {a} |")
    table_lines.extend([
        "",
        "## Performance (offline reference model; tracemalloc = Python heap only)",
        "",
        "| fixture | candidate | model | seconds | tracemalloc peak bytes | finite output |",
        "|---|---|---|---:|---:|---|",
    ])
    for fixture, cands in metrics["performance"].items():
        for cand, models in cands.items():
            for model, perf in models.items():
                table_lines.append(
                    f"| {fixture} | {cand} | {model} | {perf['seconds']:.4f} | "
                    f"{perf['tracemalloc_peak_bytes']} | {perf['output_finite']} |"
                )
    table_path = review_dir / "RD5_TABLES.md"
    table_path.write_text("\n".join(table_lines) + "\n", encoding="utf-8")

    metrics_path = output_rd5 / "metrics_rd5.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd5",
        "stage": "temporal_and_perf",
        "date": "2026-11-17",
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
        "temporal_blind_seed": TEMPORAL_BLIND_SEED,
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "tables": {"file": f"review/{table_path.name}", "sha256": sha256(table_path.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet_path.name}", "sha256": sha256(sheet_path.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "blind_form": {"file": f"review/{form_path.name}", "sha256": sha256(form_path.read_bytes())},
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
    manifest_path = output_rd5 / "manifest_rd5.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd5")
    args = parser.parse_args()
    rd0, rd5 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    manifest = run(rd0, rd5)
    print(f"RD5 temporal/perf complete: {len(manifest['outputs'])} outputs -> {rd5}")


if __name__ == "__main__":
    main()
