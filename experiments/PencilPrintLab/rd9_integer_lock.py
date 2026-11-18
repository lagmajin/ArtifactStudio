"""RD9: integer-shift temporal reference for pure dither-following measurement.

RD6-RD8 built the reference by bilinear-warping frame 0 by 0.5*k px. That
reference is itself blurred, so a content-locked dither cannot align to it
exactly. RD9 instead creates a controlled sequence by translating frame 0 by
an INTEGER k pixels (no interpolation) and supplies the exact same k as the
dither offset. Any residual in `aligned_mae` then measures dither-following
behavior only, not warp blur or motion estimation.

Runs v6 screen / content-nearest / content-interp on that sequence, records
descriptor delta, absolute probes, per-step and integer-aligned MAE, runtime,
and regenerates blind evidence. Gates stay unset; no quality ranking.
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
from rd2_descriptor_safe import probe, descriptor_delta

VARIANTS = {
    "screen": {"dither_lock": "screen", "phase_mode": "interp"},
    "content_nearest": {"dither_lock": "content", "phase_mode": "nearest"},
    "content_interp": {"dither_lock": "content", "phase_mode": "interp"},
}
BASE_SPEC = {
    "line": "sobel", "line_strength": 0.7, "tone": "bayer_multi", "dither_levels": 4,
    "texture": "hatch", "strength": 1.0,
}
SHIFT_PER_FRAME = 1
BLIND_SEED = gf.SEED + 8500
SEED_BASE = gf.SEED + 9000


def integer_shift(image: np.ndarray, dx: int) -> np.ndarray:
    """Exact integer translation with replicate fill (no interpolation)."""
    h, w = image.shape[:2]
    m = np.array([[1.0, 0.0, float(dx)], [0.0, 1.0, 0.0]], dtype=np.float32)
    return cv2.warpAffine(image, m, (w, h), flags=cv2.INTER_NEAREST, borderMode=cv2.BORDER_REPLICATE)


def metrics_sequence(out_frames: list[np.ndarray], source_frames: list[np.ndarray], rois: dict) -> dict:
    step = []
    for k in range(1, len(out_frames)):
        a = out_frames[k][..., :3].astype(np.float64)
        b = out_frames[k - 1][..., :3].astype(np.float64)
        step.append({"step": k, "global_mae": float(np.abs(a - b).mean())})
    aligned = []
    for k in range(1, len(out_frames)):
        reference = apply_free(source_frames[0], k)
        mae = float(np.abs(reference[..., :3].astype(np.float64)
                           - out_frames[k][..., :3].astype(np.float64)).mean())
        aligned.append({"frame": k, "aligned_mae": mae})
    return {
        "per_step": step,
        "integer_aligned": aligned,
        "flicker_proxy_step_std": float(np.std([s["global_mae"] for s in step])),
        "note": "reference = source frame 0 translated by exact integer k px; no bilinear warp blur",
    }


def apply_free(frame: np.ndarray, k: int) -> np.ndarray:
    """Integer-shift reference with no effect applied (used only as alignment target)."""
    return integer_shift(frame, SHIFT_PER_FRAME * k)


def spec_for(name: str, k: int) -> dict:
    spec = {**BASE_SPEC, **VARIANTS[name]}
    if spec["dither_lock"] == "content":
        spec["dither_offset"] = [float(SHIFT_PER_FRAME * k), 0.0]
    return spec


def run(output_rd0: Path, output_rd9: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd9.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd9 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd9 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    seq = fm["fixtures"]["sequence_small"]
    source = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    base = source[0]
    frames = [integer_shift(base, SHIFT_PER_FRAME * k) for k in range(len(source))]

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd9", "stage": "integer_shift_reference",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "definitions": {
            "reference": "source frame 0 translated by exact integer k px (INTER_NEAREST, replicate fill)",
            "dither_offset": "exact integer k px for content-locked variants",
            "purpose": "isolate dither-following residual from bilinear warp blur",
        },
        "shift_per_frame_px": SHIFT_PER_FRAME,
        "variants": VARIANTS,
        "temporal": {}, "performance": {}, "absolute_probe": {}, "descriptor_check": {},
    }
    outputs = {}

    for name in VARIANTS:
        seed = SEED_BASE + list(VARIANTS).index(name)
        out_frames = [apply_candidate_v6(frames[k], spec_for(name, k), seed, descriptor="straight")
                      for k in range(len(frames))]
        metrics["temporal"][name] = metrics_sequence(out_frames, frames, seq["rois"])
        stem = f"sequence_int_{name}_frame03"
        np.save(cand_dir / (stem + ".npy"), out_frames[3].astype(np.float32))
        outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}

        straight = np.load(output_rd0 / "fixtures" / fm["fixtures"]["alpha_boundary_small"]["npy"])
        premul = np.load(output_rd0 / "fixtures" / fm["fixtures"]["alpha_boundary_small"]["premultiplied"]["npy"])
        spec0 = spec_for(name, 0)
        s = apply_candidate_v6(straight, spec0, seed, descriptor="straight")
        p = apply_candidate_v6(premul, spec0, seed, descriptor="premultiplied")
        metrics["absolute_probe"][name] = probe(s)
        metrics["descriptor_check"][name] = descriptor_delta(s, p)

        perf_seed = seed
        apply_candidate_v6(frames[3], spec_for(name, 3), perf_seed, descriptor="straight")
        times = []
        for _ in range(3):
            tracemalloc.start()
            t0 = time.perf_counter()
            apply_candidate_v6(frames[3], spec_for(name, 3), perf_seed, descriptor="straight")
            times.append(time.perf_counter() - t0)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
        metrics["performance"][name] = {
            "seconds_median": float(np.median(times)), "tracemalloc_peak_bytes": int(peak),
        }

    lines = [
        "# RD9 integer-shift reference tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Integer-aligned MAE per frame (pure dither-following residual)", "",
        "| variant | aligned MAE steps |", "|---|---|",
    ]
    for name, d in metrics["temporal"].items():
        lines.append(f"| {name} | " + ",".join(f"{a['aligned_mae']:.6f}" for a in d["integer_aligned"]) + " |")
    lines.extend(["", "## Per-step global MAE and flicker proxy", "", "| variant | step MAE | step std |", "|---|---|---:|"])
    for name, d in metrics["temporal"].items():
        lines.append(f"| {name} | " + ",".join(f"{s['global_mae']:.6f}" for s in d["per_step"])
                     + f" | {d['flicker_proxy_step_std']:.6f} |")
    lines.extend(["", "## Descriptor / absolute probe", "", "| variant | descriptor delta | near >1 | nonfinite |", "|---|---:|---:|---:|"])
    for name in VARIANTS:
        p = metrics["absolute_probe"][name]
        lines.append(f"| {name} | {metrics['descriptor_check'][name]:.3g} | "
                     f"{p['near_transparent_channels_gt_1']} | {p['nonfinite_count']} |")
    lines.extend(["", "## Performance", "", "| variant | median s | tracemalloc peak |", "|---|---:|---:|"])
    for name, p in metrics["performance"].items():
        lines.append(f"| {name} | {p['seconds_median']:.4f} | {p['tracemalloc_peak_bytes']} |")
    table = review_dir / "RD9_TABLES.md"
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
        stem = f"sequence_int_{rec['variant']}_frame{rec['frame']:02d}"
        path = cand_dir / (stem + ".png")
        if not path.exists():
            out = apply_candidate_v6(frames[rec["frame"]], spec_for(rec["variant"], rec["frame"]),
                                     SEED_BASE + list(VARIANTS).index(rec["variant"]), descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd9_temporal_blind_sheet.png"
    contact_sheet(items, 4, 256, 176).save(sheet)
    key_path = output_rd9 / "temporal_blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "TEMPORAL_BLIND_FORM.md"
    form.write_text(
        "# RD9 integer-shift temporal blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge flicker/shimmer, not the 1px/frame motion.\n\n"
        "| blind ID | observed motion | suspected flicker | reviewer | date |\n"
        "|---|---|---|---|---|\n" + "".join(f"| {b} | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd9 / "metrics_rd9.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd9", "stage": "integer_shift_reference", "date": "2026-11-17",
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
    manifest_path = output_rd9 / "manifest_rd9.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd9")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD9 integer-shift reference complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
