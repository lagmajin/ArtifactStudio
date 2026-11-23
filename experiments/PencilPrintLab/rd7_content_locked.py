"""RD7: content-locked dither comparison (v5) vs screen-locked v4.

Separate offline run. On the synthetic sequence the translation per frame is
known, so the content-locked variant supplies that offset explicitly. This is
a controlled study of lock behavior, not a motion-estimation implementation.

Records descriptor delta, absolute probes, opaque parity, deep temporal
metrics (including integer-aligned MAE) and per-frame runtime/memory. Blind
evidence is regenerated. Gates stay unset; no pass/fail or quality ranking.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import cv2
import numpy as np

import generate_fixtures as gf
from pencil_reference_v4 import apply_candidate_v4
from pencil_reference_v5 import apply_candidate_v5
from rd0_baseline import contact_sheet, diff_metrics, sha256
from rd2_descriptor_safe import PERF_FIXTURES, SEED_BASE, descriptor_delta, probe
from rd6_temporal_deep import deep_temporal

VARIANTS = {
    "screen_multi4": {"line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
                      "texture": "hatch", "strength": 1.0, "dither_lock": "screen"},
    "content_multi4": {"line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
                       "texture": "hatch", "strength": 1.0, "dither_lock": "content"},
    "global_multi4": {"line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
                      "texture": "hatch", "strength": 1.0, "dither_lock": "global", "dither_offset": [0.0, 0.0]},
}
BLIND_SEED = gf.SEED + 6500


def apply_v4(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    return apply_candidate_v4(rgba, spec, seed, descriptor)


def apply_v5(rgba: np.ndarray, spec: dict, seed: int, descriptor: str = "straight") -> np.ndarray:
    return apply_candidate_v5(rgba, spec, seed, descriptor)


def run(output_rd0: Path, output_rd7: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd7.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd7 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd7 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd7", "stage": "content_locked_dither",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "models": {
            "v4": "screen-locked Bayer (RD6 baseline)",
            "v5": "content-locked Bayer with known per-frame translation offset",
        },
        "variants": VARIANTS,
        "alpha_fixtures": {}, "opaque_parity": {}, "temporal": {}, "performance": {},
        "definitions": {
            "content_lock": "threshold sampled at (x - 0.5*k, y) for frame k; controlled fixture-only study",
            "not_a_motion_estimator": "the offset is supplied from the known fixture translation",
        },
    }
    outputs = {}
    for key in ("alpha_boundary_small", "alpha_boundary_large"):
        entry = fm["fixtures"][key]
        straight = np.load(output_rd0 / "fixtures" / entry["npy"])
        premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
        metrics["alpha_fixtures"][key] = {}
        for model, apply in (("v4", apply_v4), ("v5", apply_v5)):
            metrics["alpha_fixtures"][key][model] = {}
            for name, spec in VARIANTS.items():
                seed = SEED_BASE + list(VARIANTS).index(name)
                s = apply(straight, spec, seed, descriptor="straight")
                p = apply(premul, spec, seed, descriptor="premultiplied")
                stem = f"{key}_{model}_{name}"
                np.save(cand_dir / (stem + ".npy"), s.astype(np.float32))
                outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}
                metrics["alpha_fixtures"][key][model][name] = {
                    "descriptor_delta": descriptor_delta(s, p),
                    "absolute_probe": probe(s),
                    "diff_from_input": diff_metrics(s, straight, {"description": "straight input (no-op baseline)"}),
                }

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" in entry:
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        metrics["opaque_parity"][key] = {}
        for name, spec in VARIANTS.items():
            seed = SEED_BASE + list(VARIANTS).index(name)
            a = apply_v4(rgba, spec, seed, descriptor="straight")
            b = apply_v5(rgba, spec, seed, descriptor="straight")
            metrics["opaque_parity"][key][name] = {
                "v4_v5_max_rgb_delta": float(np.max(np.abs(a[..., :3] - b[..., :3])))
            }

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for name, spec in VARIANTS.items():
        seed = SEED_BASE + list(VARIANTS).index(name)
        out_frames = []
        for k, frame in enumerate(frames):
            frame_spec = dict(spec)
            if spec.get("dither_lock") == "content":
                frame_spec["dither_offset"] = [0.5 * k, 0.0]
            out_frames.append(apply_candidate_v5(frame, frame_spec, seed, descriptor="straight"))
        metrics["temporal"][name] = deep_temporal(out_frames, seq["rois"])
        stem = f"sequence_small_{name}_frame03"
        np.save(cand_dir / (stem + ".npy"), out_frames[3].astype(np.float32))
        outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}

    for key in PERF_FIXTURES:
        rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"][key]["npy"])
        metrics["performance"][key] = {}
        for name, spec in VARIANTS.items():
            seed = SEED_BASE + list(VARIANTS).index(name)
            for model, apply in (("v4", apply_v4), ("v5", apply_v5)):
                apply(rgba, spec, seed, descriptor="straight")
                import time
                import tracemalloc
                tracemalloc.start()
                t0 = time.perf_counter()
                apply(rgba, spec, seed, descriptor="straight")
                elapsed = time.perf_counter() - t0
                _, peak = tracemalloc.get_traced_memory()
                tracemalloc.stop()
                metrics["performance"][key].setdefault(name, {})[model] = {
                    "seconds": float(elapsed), "tracemalloc_peak_bytes": int(peak),
                }

    lines = [
        "# RD7 content-locked dither tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Alpha fixtures", "",
        "| fixture | variant | model | descriptor delta | visible max | near max | near >1 | nonfinite |",
        "|---|---|---|---:|---:|---:|---:|---:|",
    ]
    for key, models in metrics["alpha_fixtures"].items():
        for name in VARIANTS:
            for model in ("v4", "v5"):
                c = models[model][name]
                p = c["absolute_probe"]
                lines.append(
                    f"| {key} | {name} | {model} | {c['descriptor_delta']:.3g} | {p['visible_max']:.4g} | "
                    f"{p['near_transparent_max']:.4g} | {p['near_transparent_channels_gt_1']} | {p['nonfinite_count']} |"
                )
    lines.extend(["", "## Opaque v4/v5 max RGB delta", "", "| fixture | variant | max delta |", "|---|---|---:|"])
    for key, variants in metrics["opaque_parity"].items():
        for name, v in variants.items():
            lines.append(f"| {key} | {name} | {v['v4_v5_max_rgb_delta']:.4g} |")
    lines.extend(["", "## Temporal integer-aligned MAE per frame", "", "| variant | aligned MAE steps |", "|---|---|"])
    for name, d in metrics["temporal"].items():
        lines.append(f"| {name} | " + ",".join(f"{a['aligned_mae']:.6f}" for a in d["integer_aligned"]) + " |")
    lines.extend(["", "## Performance", "", "| fixture | variant | model | seconds | tracemalloc peak |", "|---|---|---|---:|---:|"])
    for key, variants in metrics["performance"].items():
        for name, models in variants.items():
            for model, perf in models.items():
                lines.append(f"| {key} | {name} | {model} | {perf['seconds']:.4f} | {perf['tracemalloc_peak_bytes']} |")
    table = review_dir / "RD7_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    records = []
    for name in VARIANTS:
        for k in range(len(frames)):
            records.append({"variant": name, "frame": k})
    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"T{i + 1:02d}"
        rec = records[int(idx)]
        blind_key[bid] = rec
        stem = f"sequence_small_{rec['variant']}_frame{rec['frame']:02d}"
        path = cand_dir / (stem + ".png")
        if not path.exists():
            frame_spec = dict(VARIANTS[rec["variant"]])
            if frame_spec.get("dither_lock") == "content":
                frame_spec["dither_offset"] = [0.5 * rec["frame"], 0.0]
            out = apply_candidate_v5(frames[rec["frame"]], frame_spec,
                                     SEED_BASE + list(VARIANTS).index(rec["variant"]), descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd7_temporal_blind_sheet.png"
    contact_sheet(items, 4, 256, 176).save(sheet)
    key_path = output_rd7 / "temporal_blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "TEMPORAL_BLIND_FORM.md"
    form.write_text(
        "# RD7 content-lock temporal blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge flicker/shimmer, not the 0.5px/frame motion.\n\n"
        "| blind ID | observed motion | suspected flicker | reviewer | date |\n"
        "|---|---|---|---|---|\n" + "".join(f"| {b} | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd7 / "metrics_rd7.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd7", "stage": "content_locked_dither", "date": "2026-11-17",
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
                       "motion estimation for real footage", "product effect / UI / undo / save-reload",
                       "preview and final render paths", "CPU/GPU parity, D3D12/Vulkan",
                       "production performance at 1080p / 4K"],
    }
    manifest_path = output_rd7 / "manifest_rd7.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd7")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD7 content-locked dither complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
