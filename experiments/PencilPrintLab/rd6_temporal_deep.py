"""RD6: deeper temporal analysis (integer alignment, seek-like jumps) and
per-frame runtime/memory on existing fixtures.

Separate offline run. Uses the v4 reference model with the binary / multi4 /
multi5 tone variants from RD5. Adds:
  * integer-aligned MAE (warp frame 0 by 0.5*k back to frame k),
  * seek-like MAE (frame k vs frame 0 without alignment),
  * per-step variance as a flicker proxy,
  * static-region temporal standard deviation,
  * per-frame runtime and tracemalloc peak.

A temporal blind sheet/key/form is regenerated. Gates remain unset; no
pass/fail or quality ranking is produced.
"""

from __future__ import annotations

import argparse
import json
import platform
import time
import tracemalloc
from pathlib import Path

import cv2
import numpy as np

import generate_fixtures as gf
from pencil_reference_v4 import apply_candidate_v4
from rd0_baseline import contact_sheet, sha256, temporal
from rd5_dither_sweep import CANDIDATES as SWEEP_CANDIDATES, SEED_BASE

MODELS = {"binary": SWEEP_CANDIDATES["binary"], "multi4": SWEEP_CANDIDATES["multi4"], "multi5": SWEEP_CANDIDATES["multi5"]}
BLIND_SEED = gf.SEED + 5500


def deep_temporal(frames: list[np.ndarray], rois: dict) -> dict:
    base = temporal(frames, rois)
    h, w = frames[0].shape[:2]
    aligned = []
    seek = []
    for k in range(1, len(frames)):
        m = np.array([[1.0, 0.0, 0.5 * k], [0.0, 1.0, 0.0]], dtype=np.float32)
        warped = cv2.warpAffine(frames[0], m, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)
        aligned.append({
            "frame": k,
            "aligned_mae": float(np.mean(np.abs(warped[..., :3].astype(np.float64)
                                                - frames[k][..., :3].astype(np.float64)))),
        })
        seek.append({
            "frame": k,
            "seek_mae": float(np.mean(np.abs(frames[0][..., :3].astype(np.float64)
                                            - frames[k][..., :3].astype(np.float64)))),
        })
    per_step = [s["global_mae"] for s in base["per_step"]]
    static = [s["static_flat_mae"] for s in base["per_step"]]
    return {
        "per_step": base["per_step"],
        "integer_aligned": aligned,
        "seek_like": seek,
        "flicker_proxy_step_std": float(np.std(per_step)),
        "static_region_temporal_std": float(np.std(static)),
    }


def frame_perf(spec: dict, seed: int, frames: list[np.ndarray]) -> dict:
    samples = []
    for frame in frames:
        apply_candidate_v4(frame, spec, seed, descriptor="straight")
        tracemalloc.start()
        t0 = time.perf_counter()
        apply_candidate_v4(frame, spec, seed, descriptor="straight")
        elapsed = time.perf_counter() - t0
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        samples.append((elapsed, int(peak)))
    times = [s[0] for s in samples]
    return {
        "per_frame_seconds": [float(t) for t in times],
        "median_seconds": float(np.median(times)),
        "max_seconds": float(max(times)),
        "tracemalloc_peak_bytes_max": max(s[1] for s in samples),
    }


def run(output_rd0: Path, output_rd6: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd6.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd6 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd6 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    metrics = {
        "lab": "PencilPrintLab", "rd": "rd6", "stage": "deep_temporal",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "sequence": "8 frames, 0.5px/frame translation, 512x320",
        "models": {name: "v4 pipeline with " + name + " tone" for name in MODELS},
        "temporal": {}, "frame_performance": {},
        "definitions": {
            "integer_aligned_mae": "frame 0 warped by +0.5*k px compared with frame k",
            "seek_like_mae": "frame k compared with frame 0 without alignment",
            "flicker_proxy": "std of per-step global MAE; not a gate",
        },
    }
    outputs = {}
    for name, spec in MODELS.items():
        seed = SEED_BASE + list(SWEEP_CANDIDATES).index(name)
        out_frames = [apply_candidate_v4(f, spec, seed, descriptor="straight") for f in frames]
        metrics["temporal"][name] = deep_temporal(out_frames, seq["rois"])
        metrics["frame_performance"][name] = frame_perf(spec, seed, frames)
        stem = f"sequence_small_{name}_frame03"
        np.save(cand_dir / (stem + ".npy"), out_frames[3].astype(np.float32))
        outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}

    lines = [
        "# RD6 deep temporal tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Global MAE / aligned MAE / seek MAE per frame", "",
        "| model | frame | step MAE | aligned MAE | seek MAE |",
        "|---|---:|---:|---:|---:|",
    ]
    for name, d in metrics["temporal"].items():
        for i, step in enumerate(d["per_step"]):
            k = step["step"]
            aligned = next(a["aligned_mae"] for a in d["integer_aligned"] if a["frame"] == k)
            seek = next(s["seek_mae"] for s in d["seek_like"] if s["frame"] == k)
            lines.append(f"| {name} | {k} | {step['global_mae']:.6f} | {aligned:.6f} | {seek:.6f} |")
    lines.extend(["", "## Flicker proxy and static-region temporal std", "", "| model | step std | static std |", "|---|---:|---:|"])
    for name, d in metrics["temporal"].items():
        lines.append(f"| {name} | {d['flicker_proxy_step_std']:.6f} | {d['static_region_temporal_std']:.6f} |")
    lines.extend(["", "## Per-frame performance", "", "| model | median s | max s | tracemalloc peak max |", "|---|---:|---:|---:|"])
    for name, d in metrics["frame_performance"].items():
        lines.append(f"| {name} | {d['median_seconds']:.4f} | {d['max_seconds']:.4f} | {d['tracemalloc_peak_bytes_max']} |")
    table = review_dir / "RD6_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    records = []
    for name in MODELS:
        for k in range(len(frames)):
            records.append({"model": name, "frame": k})
    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"T{i + 1:02d}"
        rec = records[int(idx)]
        blind_key[bid] = rec
        stem = f"sequence_small_{rec['model']}_frame{rec['frame']:02d}"
        path = cand_dir / (stem + ".png")
        if not path.exists():
            out = apply_candidate_v4(frames[rec["frame"]], MODELS[rec["model"]],
                                     SEED_BASE + list(SWEEP_CANDIDATES).index(rec["model"]), descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd6_temporal_blind_sheet.png"
    contact_sheet(items, 4, 256, 176).save(sheet)
    key_path = output_rd6 / "temporal_blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "TEMPORAL_BLIND_FORM.md"
    form.write_text(
        "# RD6 temporal blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge flicker/shimmer, not the 0.5px/frame motion.\n\n"
        "| blind ID | observed motion | suspected flicker | reviewer | date |\n"
        "|---|---|---|---|---|\n" + "".join(f"| {b} | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd6 / "metrics_rd6.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd6", "stage": "deep_temporal", "date": "2026-11-17",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": cv2.__version__, "platform": platform.platform()},
        "blind_seed": BLIND_SEED, "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "tables": {"file": f"review/{table.name}", "sha256": sha256(table.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
        },
        "gates": "unset; no judgment or quality ranking performed",
        "unverified": ["human visual review and quality gate", "reference artwork images",
                       "product effect / UI / undo / save-reload", "preview and final render paths",
                       "CPU/GPU parity, D3D12/Vulkan", "production performance at 1080p / 4K"],
    }
    manifest_path = output_rd6 / "manifest_rd6.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd6")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD6 deep temporal complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
