"""RD10: analytic continuous dither threshold fields (IGN / R2) vs Bayer.

Two offline sequences are used, because they answer different questions:

  * integer sequence (1 px/frame, exact integer translation): the frame 0
    output can be shifted by the same integer amount without interpolation,
    so `lock_aligned_mae` measures dither-following exactly, without the blur
    that a bilinear warp would introduce. RD9 intended this measurement but
    compared the candidate output against the raw shifted input, so it
    measured the effect magnitude instead of the lock behaviour.
  * half-pixel sequence (0.5 px/frame, the RD0 fixture): the sub-pixel case
    that RD7/RD8 could not resolve. Here the alignment reference has to be
    resampled, which low-passes the dither pattern, so the alignment numbers
    are blur-limited and are reported as descriptive only. The sub-pixel
    quantity that stays meaningful is the threshold contrast, i.e. how much
    dither strength a lock keeps at a fractional offset.

Dither-only variants (no line gate, no texture) isolate the dither; full
variants add the screen-locked hatch texture so the size of that confound is
visible in the same table.

Gates stay unset; no pass/fail judgment and no quality ranking is performed.
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
from pencil_reference_v7 import apply_candidate_v7, threshold_field
from rd0_baseline import contact_sheet, sha256, temporal
from rd2_descriptor_safe import descriptor_delta, probe

BLIND_SEED = gf.SEED + 10500
SEED_BASE = gf.SEED + 10600
INTEGER_SHIFT_PER_FRAME = 1
HALF_SHIFT_PER_FRAME = 0.5

BASE_SPEC = {
    "line": "sobel", "line_strength": 0.7, "tone": "multi", "dither_levels": 4,
    "texture": "hatch", "strength": 1.0, "threshold_source": "bayer",
}
DITHER_ONLY_SPEC = {
    "line": None, "line_strength": 0.0, "tone": "multi", "dither_levels": 4,
    "texture": None, "strength": 1.0, "threshold_source": "bayer",
}

# name -> (pipeline, threshold_source, lock)
VARIANTS = {
    "dither_bayer_screen": ("dither", "bayer", "screen"),
    "dither_bayer_content": ("dither", "bayer", "content"),
    "dither_ign_screen": ("dither", "ign", "screen"),
    "dither_ign_content": ("dither", "ign", "content"),
    "dither_r2_content": ("dither", "r2", "content"),
    "full_bayer_screen": ("full", "bayer", "screen"),
    "full_bayer_content": ("full", "bayer", "content"),
    "full_ign_content": ("full", "ign", "content"),
    "full_r2_content": ("full", "r2", "content"),
}
BLIND_VARIANTS = ("full_bayer_screen", "full_bayer_content", "full_ign_content")


def spec_for(name: str, offset_px: float) -> dict:
    pipeline, source, lock = VARIANTS[name]
    base = DITHER_ONLY_SPEC if pipeline == "dither" else BASE_SPEC
    spec = {**base, "threshold_source": source, "dither_lock": lock}
    if lock == "content":
        spec["dither_offset"] = [float(offset_px), 0.0]
    return spec


def integer_shift(image: np.ndarray, dx: int) -> np.ndarray:
    """Exact integer translation with edge replicate fill (no interpolation).

    cv2.warpAffine with INTER_NEAREST is not an exact integer translation for
    larger offsets (its fixed-point mapping disagrees with a true shift), so
    the reference is built with numpy indexing instead.
    """
    w = image.shape[1]
    idx = np.clip(np.arange(w) - int(dx), 0, w - 1)
    return image[:, idx, ...]


def half_shift(image: np.ndarray, dx: float) -> np.ndarray:
    """Bilinear translation used only to build the RD0-style half-pixel sequence."""
    h, w = image.shape[:2]
    m = np.array([[1.0, 0.0, float(dx)], [0.0, 1.0, 0.0]], dtype=np.float32)
    return cv2.warpAffine(image, m, (w, h), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_REPLICATE)


def lock_aligned_mae(out_frames: list[np.ndarray], sequence: str) -> list[dict]:
    """Compare frame k with frame 0 shifted by the same amount.

    integer: exact integer shift of the frame 0 output, no resampling.
    half:    bilinear shift of the frame 0 output (blur-limited, descriptive).

    The margin excludes the replicate-fill band. The reference is shifted with
    an edge fill while the candidate is computed on an already shifted input,
    so the filled columns disagree by construction. The band is as wide as the
    largest motion in the sequence, therefore the margin covers that width.
    """
    margin = {"integer": INTEGER_SHIFT_PER_FRAME * (len(out_frames) - 1) + 1,
              "half": int(HALF_SHIFT_PER_FRAME * (len(out_frames) - 1)) + 2}[sequence]
    rows = []
    for k in range(1, len(out_frames)):
        if sequence == "integer":
            reference = integer_shift(out_frames[0], INTEGER_SHIFT_PER_FRAME * k)
        else:
            reference = half_shift(out_frames[0], HALF_SHIFT_PER_FRAME * k)
        diff = np.abs(reference[..., :3].astype(np.float64)
                      - out_frames[k][..., :3].astype(np.float64))
        inner = diff[margin:-margin, margin:-margin]
        rows.append({
            "frame": k,
            "aligned_mae": float(diff.mean()),
            "aligned_mae_inner": float(inner.mean()),
        })
    return rows


def threshold_contrast(source: str, offsets: list[float]) -> dict:
    """Ratio of threshold std at a fractional offset to the std at zero offset.

    A ratio near 1 means the lock keeps the full dither contrast; a lower
    ratio means the field is low-passed by the fractional offset (the RD8
    Bayer interpolation case).
    """
    h, w = 320, 512
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    base = threshold_field(xx, yy, source)
    base_std = float(base.std())
    rows = []
    for off in offsets:
        shifted = threshold_field(xx - off, yy, source)
        ratio = float(shifted.std()) / base_std if base_std > 0 else float("nan")
        rows.append({"offset_px": off, "contrast_ratio": ratio})
    return {"source": source, "zero_offset_std": base_std, "ratios": rows}


def run(output_rd0: Path, output_rd10: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd10.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd10 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd10 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    seq = fm["fixtures"]["sequence_small"]
    half_frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    source0 = half_frames[0]
    integer_frames = [integer_shift(source0, INTEGER_SHIFT_PER_FRAME * k) for k in range(len(half_frames))]

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd10", "stage": "analytic_continuous_threshold",
        "gates": "unset; no pass/fail judgment or quality ranking performed",
        "variants": {k: {"pipeline": v[0], "threshold_source": v[1], "lock": v[2]} for k, v in VARIANTS.items()},
        "sequences": {
            "integer": "1 px/frame exact integer translation; frame 0 output shifted by the same integer "
                       "amount without interpolation, so lock_aligned_mae is blur-free",
            "half": "0.5 px/frame translation (RD0 fixture); the alignment reference must be resampled, "
                    "which low-passes the dither, so aligned numbers are blur-limited and descriptive",
        },
        "definitions": {
            "lock_aligned_mae": "mean |frame0_output shifted by the same motion - frame k output| over RGB",
            "threshold_contrast_ratio": "std(threshold at offset) / std(threshold at offset 0)",
            "dither_only": "no line gate and no texture; isolates the dither contribution",
            "full": "line gate + screen-locked hatch texture, i.e. the product-like pipeline",
        },
        "temporal": {}, "threshold_contrast": {}, "absolute_probe": {}, "descriptor_check": {},
        "performance": {},
    }

    outputs = {}
    for sequence, frames, step in (("integer", integer_frames, INTEGER_SHIFT_PER_FRAME),
                                   ("half", half_frames, HALF_SHIFT_PER_FRAME)):
        metrics["temporal"][sequence] = {}
        for name in VARIANTS:
            seed = SEED_BASE + list(VARIANTS).index(name)
            out_frames = [apply_candidate_v7(frames[k], spec_for(name, step * k), seed, descriptor="straight")
                          for k in range(len(frames))]
            base = temporal(out_frames, seq["rois"])
            metrics["temporal"][sequence][name] = {
                "lock_aligned": lock_aligned_mae(out_frames, sequence),
                "per_step": base["per_step"],
                "flicker_proxy_step_std": float(np.std([s["global_mae"] for s in base["per_step"]])),
                "static_region_temporal_std": float(np.std([s["static_flat_mae"] for s in base["per_step"]])),
            }
            stem = f"{sequence}_{name}_frame03"
            np.save(cand_dir / (stem + ".npy"), out_frames[3].astype(np.float32))
            outputs[stem] = {"npy": stem + ".npy", "npy_sha256": sha256((cand_dir / (stem + ".npy")).read_bytes())}

    for source in ("bayer", "ign", "r2"):
        metrics["threshold_contrast"][source] = threshold_contrast(
            source, [HALF_SHIFT_PER_FRAME * k for k in range(1, len(half_frames))])

    alpha_key = "alpha_boundary_small"
    entry = fm["fixtures"][alpha_key]
    straight = np.load(output_rd0 / "fixtures" / entry["npy"])
    premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
    for name in VARIANTS:
        seed = SEED_BASE + list(VARIANTS).index(name)
        s = apply_candidate_v7(straight, spec_for(name, 0.0), seed, descriptor="straight")
        p = apply_candidate_v7(premul, spec_for(name, 0.0), seed, descriptor="premultiplied")
        metrics["absolute_probe"][name] = probe(s)
        metrics["descriptor_check"][name] = descriptor_delta(s, p)

    for name in VARIANTS:
        seed = SEED_BASE + list(VARIANTS).index(name)
        spec = spec_for(name, 3 * HALF_SHIFT_PER_FRAME)
        apply_candidate_v7(half_frames[3], spec, seed, descriptor="straight")
        times = []
        for _ in range(3):
            tracemalloc.start()
            t0 = time.perf_counter()
            apply_candidate_v7(half_frames[3], spec, seed, descriptor="straight")
            times.append(time.perf_counter() - t0)
            _, peak = tracemalloc.get_traced_memory()
            tracemalloc.stop()
        metrics["performance"][name] = {
            "seconds_median": float(np.median(times)), "tracemalloc_peak_bytes": int(peak),
        }

    lines = [
        "# RD10 analytic threshold tables (blank conclusion)", "",
        "No gate is set; no pass/fail or quality ranking is recorded.", "",
        "## Lock-aligned MAE on the integer sequence (1 px/frame, blur-free reference)", "",
        "`inner` excludes the replicate-fill band of the shifted reference (as wide as the largest "
        "motion in the sequence); the filled columns disagree by construction.", "",
        "| variant | aligned MAE steps | mean | inner mean |", "|---|---|---:|---:|",
    ]
    for name in VARIANTS:
        rows = metrics["temporal"]["integer"][name]["lock_aligned"]
        mean = float(np.mean([r["aligned_mae"] for r in rows]))
        inner = float(np.mean([r["aligned_mae_inner"] for r in rows]))
        lines.append(f"| {name} | " + ",".join(f"{r['aligned_mae']:.6f}" for r in rows)
                     + f" | {mean:.6f} | {inner:.8f} |")
    lines.extend(["", "## Lock-aligned MAE on the half-pixel sequence (0.5 px/frame, blur-limited reference)", "",
                  "| variant | aligned MAE steps | mean | inner mean |", "|---|---|---:|---:|"])
    for name in VARIANTS:
        rows = metrics["temporal"]["half"][name]["lock_aligned"]
        mean = float(np.mean([r["aligned_mae"] for r in rows]))
        inner = float(np.mean([r["aligned_mae_inner"] for r in rows]))
        lines.append(f"| {name} | " + ",".join(f"{r['aligned_mae']:.6f}" for r in rows)
                     + f" | {mean:.6f} | {inner:.8f} |")
    lines.extend(["", "## Per-step MAE and flicker proxy (half-pixel sequence)", "",
                  "| variant | step MAE | step std | static std |", "|---|---|---:|---:|"])
    for name in VARIANTS:
        d = metrics["temporal"]["half"][name]
        lines.append(f"| {name} | " + ",".join(f"{s['global_mae']:.6f}" for s in d["per_step"])
                     + f" | {d['flicker_proxy_step_std']:.6f} | {d['static_region_temporal_std']:.6f} |")
    lines.extend(["", "## Threshold contrast ratio at a 0.5 px/frame offset", "",
                  "| source | zero-offset std | contrast ratios (0.5 px steps) |", "|---|---:|---|"])
    for source, d in metrics["threshold_contrast"].items():
        lines.append(f"| {source} | {d['zero_offset_std']:.6f} | "
                     + ",".join(f"{r['contrast_ratio']:.4f}" for r in d["ratios"]) + " |")
    lines.extend(["", "## Descriptor / absolute probe", "",
                  "| variant | descriptor delta | visible max | near max | near >1 | nonfinite |",
                  "|---|---:|---:|---:|---:|---:|"])
    for name in VARIANTS:
        p = metrics["absolute_probe"][name]
        lines.append(f"| {name} | {metrics['descriptor_check'][name]:.3g} | {p['visible_max']:.4g} | "
                     f"{p['near_transparent_max']:.4g} | {p['near_transparent_channels_gt_1']} | "
                     f"{p['nonfinite_count']} |")
    lines.extend(["", "## Performance", "", "| variant | median s | tracemalloc peak |", "|---|---:|---:|"])
    for name, p in metrics["performance"].items():
        lines.append(f"| {name} | {p['seconds_median']:.4f} | {p['tracemalloc_peak_bytes']} |")
    table = review_dir / "RD10_TABLES.md"
    table.write_text("\n".join(lines) + "\n", encoding="utf-8")

    records = []
    for name in BLIND_VARIANTS:
        for k in range(len(half_frames)):
            records.append({"variant": name, "frame": k, "sequence": "half"})
    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"T{i + 1:02d}"
        rec = records[int(idx)]
        blind_key[bid] = rec
        stem = f"half_{rec['variant']}_frame{rec['frame']:02d}"
        path = cand_dir / (stem + ".png")
        if not path.exists():
            seed = SEED_BASE + list(VARIANTS).index(rec["variant"])
            out = apply_candidate_v7(half_frames[rec["frame"]],
                                     spec_for(rec["variant"], HALF_SHIFT_PER_FRAME * rec["frame"]),
                                     seed, descriptor="straight")
            cv2.imwrite(str(path), cv2.cvtColor(gf.linear_to_srgb_u8(out[..., :3]), cv2.COLOR_RGB2BGR))
        items.append((bid, path))
    sheet = review_dir / "rd10_temporal_blind_sheet.png"
    contact_sheet(items, 4, 256, 176).save(sheet)
    key_path = output_rd10 / "temporal_blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "TEMPORAL_BLIND_FORM.md"
    form.write_text(
        "# RD10 analytic-threshold temporal blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed. Judge flicker/shimmer, not the 0.5px/frame motion.\n\n"
        "| blind ID | observed motion | suspected flicker | reviewer | date |\n"
        "|---|---|---|---|---|\n" + "".join(f"| {b} | | | | |\n" for b in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd10 / "metrics_rd10.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd10", "stage": "analytic_continuous_threshold", "date": "2026-11-18",
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
                       "production performance at 1080p / 4K",
                       "IGN/R2 spatial appearance versus ordered Bayer (no gate)"],
    }
    manifest_path = output_rd10 / "manifest_rd10.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd10")
    args = parser.parse_args()
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, Path(args.output))
    print(f"RD10 analytic threshold complete: {len(manifest['outputs'])} outputs -> {args.output}")


if __name__ == "__main__":
    main()
