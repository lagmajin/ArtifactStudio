"""RD8: sub-pixel phase interpolation comparison (v5 nearest vs v6 interp).

Separate offline run on existing fixtures. Uses the known 0.5*k px translation
of the synthetic sequence as a controlled study. Records descriptor delta,
absolute probes, opaque parity, deep temporal metrics (integer-aligned MAE)
and per-frame runtime/memory. Blind evidence is regenerated. Gates stay unset;
no pass/fail or quality ranking is produced.
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
from pencil_reference_v6 import apply_candidate_v6
from rd0_baseline import contact_sheet, sha256
from rd2_descriptor_safe import SEED_BASE, descriptor_delta, probe
from rd6_temporal_deep import deep_temporal

VARIANTS = {
    "nearest": {
        "line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
        "texture": "hatch", "strength": 1.0, "dither_lock": "content", "phase_mode": "nearest",
    },
    "interp": {
        "line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
        "texture": "hatch", "strength": 1.0, "dither_lock": "content", "phase_mode": "interp",
    },
    "screen_interp": {
        "line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
        "texture": "hatch", "strength": 1.0, "dither_lock": "screen", "phase_mode": "interp",
    },
}
FRAME_SPECS = {}
for _name, _spec in VARIANTS.items():
    for _k in range(8):
        s = dict(_spec)
        if _spec.get("dither_lock") == "content":
            s["dither_offset"] = [0.5 * _k, 0.0]
        FRAME_SPECS[(_name, _k)] = s

BLIND_SEED = gf.SEED + 7500


def run(output_rd0: Path, output_rd8: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd8.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd8 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd8 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd8", "stage": "subpixel_phase",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "models": {
            "nearest": "v6 with integer-truncated phase (reproduces RD7 content-lock)",
            "interp": "v6 with bilinear blend of neighboring Bayer phases",
            "screen_interp": "interp with screen-locked threshold (control)",
        },
        "variants": VARIANTS,
        "alpha_fixtures": {}, "opaque_parity": {}, "temporal": {}, "performance": {},
        "definitions": {
            "subpixel_phase": "threshold = bilinear blend of adjacent Bayer phases by fractional offset",
            "controlled_study": "0.5*k px offset supplied from known fixture translation, not estimated",
        },
    }
    outputs = {}
    for key in ("alpha_boundary_small", "alpha_boundary_large"):
        entry = fm["fixtures"][key]
        straight = np.load(output_rd0 / "fixtures" / entry["npy"])
        premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
        metrics["alpha_fixtures"][key] = {}
        for name, spec in VARIANTS.items():
            seed = SEED_BASE + list(VARIANTS).index(name)
            s = apply_candidate_v6(straight, spec, seed, descriptor="straight")
            p = apply_candidate_v6(premul, spec, seed, descriptor="premultiplied")
            stem = f"{key}_{name}"
            np.save(cand_dir / (stem + ".npy"), s.astype(np.float32))
            outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}
            metrics["alpha_fixtures"][key][name] = {
                "descriptor_delta": descriptor_delta(s, p),
                "absolute_probe": probe(s),
            }

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small" or "premultiplied" in entry:
            continue
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        metrics["opaque_parity"][key] = {}
        base = apply_candidate_v6(rgba, VARIANTS["nearest"], SEED_BASE, descriptor="straight")
        for name, spec in VARIANTS.items():
            out = apply_candidate_v6(rgba, spec, SEED_BASE + list(VARIANTS).index(name), descriptor="straight")
            metrics["opaque_parity"][key][name] = {
                "max_rgb_delta_from_nearest": float(np.max(np.abs(out[..., :3] - base[..., :3])))
            }

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for name in VARIANTS:
        out_frames = [apply_candidate_v6(frames[k], FRAME_SPECS[(name, k)],
                                         SEED_BASE + list(VARIANTS).index(name), descriptor="straight")
                      for k in range(len(frames))]
        metrics["temporal"][name] = deep_temporal(out_frames, seq["rois"])
        stem = f"sequence_small_{name}_frame03"
        np.save(cand_dir / (stem + ".npy"), out_frames[3].astype(np.float32))
        outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}

    rgba = np.load(output_rd0 / "fixtures" / fm["fixtures"]["color_ramp_large"]["npy"])
    metrics["performance"] = {}
    for name, spec in VARIANTS.items():
        seed = SEED_BASE + list(VARIANTS).index(name)
        apply_candidate_v6(rgba, spec, seed, descriptor="straight")
        times = []
        for _ in range(3):
            tracemalloc.start()
            t0 = time.perf_counter()
            apply_candidate_v6(rgba, spec, seed, descriptor="straight")
            times.append(time.perf_counter() - t0)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
        metrics["performance"][name] = {
            "seconds_median": float(np.median(times)),
            "tracemalloc_peak_bytes": int(peak),
        }

    lines = [
        "# RD8 sub-pixel phase tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Alpha fixtures", "",
        "| fixture | variant | descriptor delta | visible max | near max | near >1 | nonfinite |",
        "|---|---|---:|---:|---:|---:|---:|",
    ]
    for key, variants in metrics["alpha_fixtures"].items():
        for name, c in variants.items():
            p = c["absolute_probe"]
            lines.append(
                f"| {key} | {name} | {c['descriptor_delta']:.3g} | {p['visible_max']:.4g} | "
                f"{p['near_transparent_max']:.4g} | {p['near_transparent_channels_gt_1']} | {p['nonfinite_count']} |"
            )
    lines.extend(["", "## Opaque max delta from nearest", "", "| fixture | variant | max delta |", "|---|---|---:|"])
    for key, variants in metrics["opaque_parity"].items():
        for name, v in variants.items():
            lines.append(f"| {key} | {name} | {v['max_rgb_delta_from_nearest']:.4g} |")
    lines.extend(["", "## Temporal integer-aligned MAE per frame", "", "| variant | aligned MAE steps |", "|---|---|"])
    for name, d in metrics["temporal"].items():
        lines.append(f"| {name} | " + ",".join(f"{a['aligned_mae']:.6f}" for a in d["integer_aligned"]) + " |")
    lines.extend(["", "## Performance (color_ramp_large)", "", "| variant | median s | tracemalloc peak |", "|---|---:|---:|"])
    for name, p in metrics["performance"].items():
        lines.append(f"| {name} | {p['seconds_median']:.4f} | {p['tracemalloc_peak_bytes']} |")
    table = review_dir / "RD8_TABLES.md"
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
            out = apply_candidate_v6(frames[rec["frame"]], FRAME_SPECS[(rec["variant"], rec["frame"])],
                                     SEED_BASE + list(VARIANTS).index(rec["variant"]), descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd8_temporal_blind_sheet.png"
    contact_sheet(items, 4, 256, 176).save(sheet)
    key_path = output_rd8 / "temporal_blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "TEMPORAL_BLIND_FORM.md"
    form.write_text(
        "# RD8 sub-pixel phase temporal blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge flicker/shimmer, not the 0.5px/frame motion.\n\n"
        "| blind ID | observed motion | suspected flicker | reviewer | date |\n"
        "|---|---|---|---|---|\n" + "".join(f"| {b} | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd8 / "metrics_rd8.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd8", "stage": "subpixel_phase", "date": "2026-11-17",
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
    manifest_path = output_rd8 / "manifest_rd8.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd8")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD8 sub-pixel phase complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
