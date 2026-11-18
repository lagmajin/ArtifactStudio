"""RD1 candidate acquisition for pencil/print-style R&D.

Generates offline reference-model outputs and descriptive evidence. No
pass/fail judgment or ranking: gate thresholds must be fixed by a separate
reviewer before final comparison. Offline only.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from pencil_reference import apply_candidate
from rd0_baseline import (
    alpha_contract,
    contact_sheet,
    diff_metrics,
    distribution,
    patch_colors,
    roi_metrics,
    sha256,
    temporal,
)

FULL_CANDIDATES = {
    "pp_a": {"line": "sobel", "line_strength": 0.8, "tone": "posterize", "tone_levels": 6, "texture": "grain", "strength": 1.0},
    "pp_b": {"line": "sobel", "line_strength": 0.7, "tone": "bayer", "texture": "hatch", "strength": 1.0},
    "pp_c": {"line": "alpha_edge", "line_strength": 0.8, "tone": "posterize", "tone_levels": 6, "texture": "hatch", "strength": 1.0},
}
ABLATION_CANDIDATES = {
    "only_line": {"line": "sobel", "line_strength": 0.8, "tone": None, "texture": None, "strength": 1.0},
    "only_tone": {"line": None, "tone": "posterize", "tone_levels": 6, "texture": None, "strength": 1.0},
    "only_texture": {"line": None, "tone": None, "texture": "grain", "strength": 1.0},
}
ALL_CANDIDATES = {**FULL_CANDIDATES, **ABLATION_CANDIDATES}
CANDIDATE_SEED_BASE = gf.SEED + 1000
BLIND_SEED = gf.SEED + 1500


def run(output_rd0: Path, output_rd1: Path) -> dict:
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd1.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd1 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd1 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "PencilPrintLab", "rd": "rd1", "stage": "candidate_outputs",
        "gates": "unset; no pass/fail judgment or ranking performed",
        "candidates": ALL_CANDIDATES, "fixtures": {}, "sequence_small": {}, "descriptor_check": {},
        "definitions": {
            "descriptor_policy": "premultiplied pointwise modulation; alpha unchanged or quantized on the same grid",
        },
    }
    outputs = {}
    blind_records = []

    for key, entry in fm["fixtures"].items():
        if key == "sequence_small":
            continue
        is_small = key.endswith("_small")
        rgba = np.load(output_rd0 / "fixtures" / entry["npy"])
        rois = entry["rois"]
        metrics["fixtures"][key] = {}
        for cand_name, spec in ALL_CANDIDATES.items():
            if cand_name in ABLATION_CANDIDATES and not is_small:
                continue
            seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
            result = apply_candidate(rgba, spec, seed, descriptor="straight")
            stem = f"{key}_{cand_name}"
            np.save(cand_dir / (stem + ".npy"), result.astype(np.float32))
            import cv2
            cv2.imwrite(str(cand_dir / (stem + ".png")),
                        cv2.cvtColor(gf.linear_to_srgb_u8(result[..., :3]), cv2.COLOR_RGB2BGR))
            npy, png = cand_dir / (stem + ".npy"), cand_dir / (stem + ".png")
            outputs[stem] = {"npy": npy.name, "npy_sha256": sha256(npy.read_bytes()),
                             "png": png.name, "png_sha256": sha256(png.read_bytes())}
            per = {
                "distribution": distribution(result),
                "rois": {label: roi_metrics(result, rect) for label, rect in rois.items()},
                "diff_from_input": diff_metrics(result, rgba, {"description": "input image (no-op baseline)"}),
                "patch_colors": patch_colors(result, rois),
            }
            if "premultiplied" in entry:
                premul = np.load(output_rd0 / "fixtures" / entry["premultiplied"]["npy"])
                premul_result = apply_candidate(premul, spec, seed, descriptor="premultiplied")
                per["alpha_contract"] = alpha_contract(result, premul_result)
                metrics["descriptor_check"][stem] = {
                    "recovered_straight_rgb_max_delta": per["alpha_contract"]["recovered_straight_rgb_max_delta"],
                    "note": "descriptive; tolerance fixed by reviewer",
                }
                visible = result[..., 3] > 0.5
                near = (result[..., 3] > 0.001) & (result[..., 3] <= 0.5)
                per["absolute_probe"] = {
                    "visible_max": float(result[..., :3][visible].max()) if visible.any() else None,
                    "near_transparent_max": float(result[..., :3][near].max()) if near.any() else None,
                    "near_transparent_channels_gt_10": int(np.sum(result[..., :3][near] > 10.0)) if near.any() else 0,
                    "nonfinite_count": int(np.sum(~np.isfinite(result[..., :3]))),
                }
            metrics["fixtures"][key][cand_name] = per
            if is_small:
                blind_records.append({"fixture": key, "candidate": cand_name})

    seq = fm["fixtures"]["sequence_small"]
    frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for cand_name, spec in FULL_CANDIDATES.items():
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
        out_frames = [apply_candidate(f, spec, seed, descriptor="straight") for f in frames]
        metrics["sequence_small"][cand_name] = temporal(out_frames, seq["rois"])

    rng = np.random.default_rng(BLIND_SEED)
    order = rng.permutation(len(blind_records))
    blind_key = {}
    items = []
    for i, idx in enumerate(order):
        bid = f"C{i + 1:02d}"
        rec = blind_records[int(idx)]
        blind_key[bid] = rec
        items.append((bid, cand_dir / (rec["fixture"] + "_" + rec["candidate"] + ".png")))
    sheet = review_dir / "rd1_blind_sheet.png"
    contact_sheet(items, 4, 288, 200).save(sheet)
    key_path = output_rd1 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form = review_dir / "CANDIDATE_BLIND_FORM.md"
    form.write_text(
        "# RD1 candidate blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed.\n\n"
        "| blind ID | full image | ROI | defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n"
        + "".join(f"| {bid} | | | | | |\n" for bid in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd1 / "metrics_rd1.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest = {
        "lab": "PencilPrintLab", "rd": "rd1", "stage": "candidate_outputs", "date": "2026-11-17",
        "environment": {"python": platform.python_version(), "numpy": np.__version__,
                        "opencv": __import__("cv2").__version__, "platform": platform.platform()},
        "candidate_seed_base": CANDIDATE_SEED_BASE, "blind_seed": BLIND_SEED,
        "candidates": ALL_CANDIDATES, "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet.name}", "sha256": sha256(sheet.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "form": {"file": f"review/{form.name}", "sha256": sha256(form.read_bytes())},
        },
        "gates": "unset; no judgment or ranking performed",
        "unverified": ["human visual review and quality gate", "reference artwork images",
                       "product effect / UI / undo / save-reload", "preview and final render paths",
                       "CPU/GPU parity, D3D12/Vulkan", "production performance at 1080p / 4K"],
    }
    manifest_path = output_rd1 / "manifest_rd1.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/PencilPrintLab/captures/rd0")
    parser.add_argument("--output", default="experiments/PencilPrintLab/captures/rd1")
    args = parser.parse_args()
    rd0, rd1 = Path(args.rd0), Path(args.output)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures: {rd0}")
    manifest = run(rd0, rd1)
    print(f"RD1 candidate outputs complete: {len(manifest['outputs'])} outputs -> {rd1}")


if __name__ == "__main__":
    main()
