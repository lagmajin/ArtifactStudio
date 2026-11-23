"""RD5: dither-level sweep and deeper temporal/runtime comparison.

Separate offline run. Sweeps ordered-dither levels 3/4/5/6/8 plus the binary
baseline, recording descriptor delta, absolute recovered-RGB probes, opaque
difference from the binary baseline, per-step temporal metrics and runtime/
memory. Blind evidence is regenerated for the sweep. Gates stay unset; no
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
from rd0_baseline import contact_sheet, diff_metrics, sha256, temporal
from rd2_descriptor_safe import PERF_FIXTURES, SEED_BASE, descriptor_delta, probe

BLIND_SEED = gf.SEED + 4500
SWEEP_LEVELS = (3, 4, 5, 6, 8)


def spec_for(level: int | None) -> dict:
    tone = "bayer" if level is None else "bayer_multi"
    spec = {
        "line": "sobel", "line_strength": 0.7, "tone": tone,
        "texture": "hatch", "strength": 1.0,
    }
    if level is not None:
        spec["dither_levels"] = level
    return spec


CANDIDATES = {"binary": spec_for(None)}
for _level in SWEEP_LEVELS:
    CANDIDATES[f"multi{_level}"] = spec_for(_level)


def measure(spec: dict, seed: int, rgba: np.ndarray) -> dict:
    apply_candidate_v4(rgba, spec, seed, descriptor="straight")
    samples = []
    for _ in range(3):
        tracemalloc.start()
        t0 = time.perf_counter()
        apply_candidate_v4(rgba, spec, seed, descriptor="straight")
        elapsed = time.perf_counter() - t0
        _, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        samples.append((elapsed, int(peak)))
    times = [s[0] for s in samples]
    return {
        "seconds_median": float(np.median(times)),
        "seconds_min": float(min(times)),
        "seconds_max": float(max(times)),
        "tracemalloc_peak_bytes_max": max(s[1] for s in samples),
        "pixels": int(rgba.shape[0] * rgba.shape[1]),
    }


def run(output_rd0: Path, output_rd5: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd5.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd5 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd5 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd5", "stage": "dither_sweep",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "candidates": CANDIDATES,
        "alpha_fixtures": {}, "opaque_vs_binary": {}, "temporal": {}, "performance": {},
        "definitions": {
            "opaque_vs_binary_max": "max RGB delta from the binary-baseline output, same fixture/seed",
            "objective": "descriptive only; closest-to-binary is not a quality winner",
        },
    }
    outputs = {}

    for key in ("alpha_boundary_small", "alpha_boundary_large"):
        entry = fm["fixtures"][key]
        straight = np.load(output_rd0 / "fixtures" / entry["npy"])
        premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
        metrics["alpha_fixtures"][key] = {}
        for cand, spec in CANDIDATES.items():
            seed = SEED_BASE + list(CANDIDATES).index(cand)
            s = apply_candidate_v4(straight, spec, seed, descriptor="straight")
            p = apply_candidate_v4(premul, spec, seed, descriptor="premultiplied")
            stem = f"{key}_{cand}"
            np.save(cand_dir / (stem + ".npy"), s.astype(np.float32))
            outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}
            metrics["alpha_fixtures"][key][cand] = {
                "descriptor_delta": descriptor_delta(s, p),
                "absolute_probe": probe(s),
            }

    binary_by_fixture = {}
    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" in entry:
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        binary_by_fixture[key] = apply_candidate_v4(rgba, CANDIDATES["binary"], SEED_BASE, descriptor="straight")
        metrics["opaque_vs_binary"][key] = {}
        for cand, spec in CANDIDATES.items():
            seed = SEED_BASE + list(CANDIDATES).index(cand)
            out = apply_candidate_v4(rgba, spec, seed, descriptor="straight")
            metrics["opaque_vs_binary"][key][cand] = {
                "max_rgb_delta": float(np.max(np.abs(out[..., :3] - binary_by_fixture[key][..., :3]))),
                "mae_rgb": float(np.mean(np.abs(out[..., :3] - binary_by_fixture[key][..., :3]))),
            }

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for cand, spec in CANDIDATES.items():
        seed = SEED_BASE + list(CANDIDATES).index(cand)
        out_frames = [apply_candidate_v4(f, spec, seed, descriptor="straight") for f in frames]
        tm = temporal(out_frames, seq["rois"])
        metrics["temporal"][cand] = {
            "global_mae_per_step": [s["global_mae"] for s in tm["per_step"]],
            "static_flat_mae_per_step": [s["static_flat_mae"] for s in tm["per_step"]],
        }

    for key in PERF_FIXTURES:
        rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][key]["npy"])
        metrics["performance"][key] = {}
        for cand, spec in CANDIDATES.items():
            metrics["performance"][key][cand] = measure(spec, SEED_BASE + list(CANDIDATES).index(cand), rgba)

    lines = [
        "# RD5 dither sweep tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Alpha fixtures", "",
        "| fixture | candidate | descriptor delta | visible max | near max | near >1 | near >10 | nonfinite |",
        "|---|---|---:|---:|---:|---:|---:|---:|",
    ]
    for key, cands in metrics["alpha_fixtures"].items():
        for cand, c in cands.items():
            p = c["absolute_probe"]
            lines.append(
                f"| {key} | {cand} | {c['descriptor_delta']:.3g} | {p['visible_max']:.4g} | "
                f"{p['near_transparent_max']:.4g} | {p['near_transparent_channels_gt_1']} | "
                f"{p['near_transparent_channels_gt_10']} | {p['nonfinite_count']} |"
            )
    lines.extend(["", "## Opaque difference from binary baseline", "", "| fixture | candidate | max delta | MAE |", "|---|---|---:|---:|"])
    for key, cands in metrics["opaque_vs_binary"].items():
        for cand, v in cands.items():
            lines.append(f"| {key} | {cand} | {v['max_rgb_delta']:.4g} | {v['mae_rgb']:.6f} |")
    lines.extend(["", "## Temporal global MAE per step", "", "| candidate | steps |", "|---|---|"])
    for cand, v in metrics["temporal"].items():
        lines.append(f"| {cand} | " + ",".join(f"{x:.6f}" for x in v["global_mae_per_step"]) + " |")
    lines.extend(["", "## Performance (3 runs)", "", "| fixture | candidate | median s | min s | max s | tracemalloc peak max |", "|---|---|---:|---:|---:|---:|"])
    for key, cands in metrics["performance"].items():
        for cand, v in cands.items():
            lines.append(
                f"| {key} | {cand} | {v['seconds_median']:.4f} | {v['seconds_min']:.4f} | "
                f"{v['seconds_max']:.4f} | {v['tracemalloc_peak_bytes_max']} |"
            )
    table = review_dir / "RD5_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    records = []
    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or not key.endswith("_small"):
            continue
        for cand in CANDIDATES:
            records.append({"fixture": key, "candidate": cand})
    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"C{i + 1:02d}"
        rec = records[int(idx)]
        blind_key[bid] = rec
        stem = f"{rec['fixture']}_{rec['candidate']}"
        path = cand_dir / (stem + ".png")
        if not path.exists():
            rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][rec["fixture"]]["npy"])
            out = apply_candidate_v4(rgba, CANDIDATES[rec["candidate"]],
                                     SEED_BASE + list(CANDIDATES).index(rec["candidate"]), descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd5_sweep_blind_sheet.png"
    contact_sheet(items, 4, 288, 200).save(sheet)
    key_path = output_rd5 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "CANDIDATE_BLIND_FORM.md"
    form.write_text(
        "# RD5 sweep blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed.\n\n"
        "| blind ID | full image | ROI | defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n" + "".join(f"| {b} | | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd5 / "metrics_rd5.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd5", "stage": "dither_sweep", "date": "2026-11-17",
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
        "gates": "unset; no judgment or quality ranking performed",
        "unverified": ["human visual review and quality gate", "reference artwork images",
                       "product effect / UI / undo / save-reload", "preview and final render paths",
                       "CPU/GPU parity, D3D12/Vulkan", "production performance at 1080p / 4K"],
    }
    manifest_path = output_rd5 / "manifest_rd5.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd5")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD5 dither sweep complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
