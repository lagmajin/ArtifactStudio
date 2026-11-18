"""RD3: blind candidate sheets from the v2 (premultiplied-space) model.

RD1 used the v1 model (unpremultiply first). RD2 showed v1 changes straight
color at alpha boundaries while v2 does not. The v1 blind key/sheet must not
be reused as v2 evidence, so this run regenerates the candidate outputs, blind
sheet, blind key and blank review form from v2 as a separate run.

No pass/fail judgment, no ranking, no gate thresholds: the separate reviewer
fixes those in the RD0 GATE_FORM before candidate scoring. Offline only.
"""

from __future__ import annotations

import argparse
import json
import platform
from pathlib import Path

import numpy as np

import generate_fixtures as gf
from rd0_baseline import (
    contact_sheet,
    diff_metrics,
    distribution,
    patch_color_metrics,
    roi_metrics,
    save_output,
    sha256,
    temporal_metrics,
)
from rd1_candidates import ABLATION_CANDIDATES, ALL_CANDIDATES, CANDIDATE_SEED_BASE, FULL_CANDIDATES
from watercolor_reference_v2 import apply_candidate_v2

BLIND_SEED = gf.SEED + 2900
MODEL_LABELS = {
    "v2": "v2 (premultiplied-space processing)",
    "v3": "v3 (premultiplied-space processing with alpha-consistent bleed kernels)",
    "v4": "v4 (fast shared-weight bilateral approximation, alpha-consistent)",
    "v5": "v5 (tuned fast shared-weight bilateral approximation, alpha-consistent)",
}


def run(output_rd0: Path, output_rd3: Path, model_name: str = "v2", apply=apply_candidate_v2,
        blind_seed: int = BLIND_SEED, manifest_name: str = "manifest_rd3.json",
        sheet_name: str = "rd3_v2_blind_sheet.png") -> dict:
    if model_name not in MODEL_LABELS:
        raise ValueError(f"unknown model: {model_name}")
    fm = json.loads((output_rd0 / "fixtures_manifest.json").read_text(encoding="utf-8"))
    output_rd3.mkdir(parents=True, exist_ok=True)
    cand_dir = output_rd3 / "candidates"
    cand_dir.mkdir(parents=True, exist_ok=True)
    review_dir = output_rd3 / "review"
    review_dir.mkdir(parents=True, exist_ok=True)

    metrics = {
        "lab": "WatercolorLab",
        "rd": "rd3",
        "stage": f"{model_name}_blind_candidates",
        "model": MODEL_LABELS[model_name],
        "gates": "unset; no pass/fail judgment or ranking performed",
        "candidates": ALL_CANDIDATES,
        "fixtures": {},
        "sequence_small": {},
        "descriptive_findings": {},
        "definitions": {
            "descriptor_policy": "process premultiplied RGB and alpha with the same spatial transform, unpremultiply once at the end",
            "paper_noise": "screen-locked deterministic noise with per-candidate seed",
            "visible_pixel": "alpha > 0.5; transparent pixels' RGB must not be judged from PNG previews",
        },
    }
    outputs = {}
    blind_records: list[dict] = []

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
            result = apply(rgba, spec, seed, descriptor="straight")
            stem = f"{key}_{cand_name}"
            outputs[stem] = save_output(cand_dir / stem, result)
            metrics["fixtures"][key][cand_name] = {
                "distribution": distribution(result),
                "rois": {label: roi_metrics(result, rect) for label, rect in rois.items()},
                "diff_from_input": diff_metrics(result, rgba, {"description": "input image (no-op baseline)"}),
                "patch_colors": patch_color_metrics(result, rois),
            }
            if key == "alpha_boundary_small":
                visible = result[..., 3] > 0.5
                rgb = result[..., :3]
                metrics["descriptive_findings"][cand_name] = {
                    "max_rgb_all_pixels": float(rgb.max()),
                    "max_rgb_visible_pixels": float(rgb[visible].max()) if visible.any() else None,
                    "extreme_rgb_over_10_all_pixels": int(np.sum(rgb > 10.0)),
                    "note": "PNG previews show raw RGB even at alpha=0; judge only alpha>0.5 regions",
                }
            if is_small:
                blind_records.append({"fixture": key, "candidate": cand_name})

    seq = fm["fixtures"]["sequence_small"]
    source_frames = [np.load(output_rd0 / "fixtures" / f["npy"]) for f in seq["frames"]]
    for cand_name, spec in FULL_CANDIDATES.items():
        seed = CANDIDATE_SEED_BASE + list(ALL_CANDIDATES).index(cand_name)
        frames = [apply(f, spec, seed, descriptor="straight") for f in source_frames]
        metrics["sequence_small"][cand_name] = temporal_metrics(frames, seq["rois"])
        stem = f"sequence_small_{cand_name}"
        outputs[stem] = save_output(cand_dir / stem, frames[3])

    rng = np.random.default_rng(blind_seed)
    order = rng.permutation(len(blind_records))
    blind_key = {}
    sheet_items = []
    for i, idx in enumerate(order):
        blind_id = f"C{i + 1:02d}"
        rec = blind_records[int(idx)]
        blind_key[blind_id] = rec
        sheet_items.append((blind_id, cand_dir / (rec["fixture"] + "_" + rec["candidate"] + ".png")))

    sheet_path = review_dir / sheet_name
    contact_sheet(sheet_items, cols=4, cell_w=288, cell_h=200).save(sheet_path)

    key_path = output_rd3 / "blind_key.json"
    key_path.write_text(json.dumps(blind_key, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    form_path = review_dir / "CANDIDATE_BLIND_FORM.md"
    form_path.write_text(
        "# RD3 (v2 model) candidate blind review form (blank)\n\n"
        "Do not score until GATE_FORM.md thresholds are fixed by a separate reviewer.\n"
        f"This sheet uses the {model_name} model, not the RD1 v1 sheet.\n"
        "PNG previews display raw RGB even where alpha is 0; judge only visible "
        "(alpha > 0.5) regions. Extreme values near transparent boundaries are "
        "recorded descriptively in metrics_rd3.json and are not scored here.\n\n"
        "| blind ID | full image notes | ROI notes | suspected defect | reviewer | date |\n"
        "|---|---|---|---|---|---|\n"
        + "".join(f"| {bid} | | | | | |\n" for bid in blind_key),
        encoding="utf-8",
    )

    metrics_path = output_rd3 / "metrics_rd3.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    manifest = {
        "lab": "WatercolorLab",
        "rd": "rd3",
        "stage": f"{model_name}_blind_candidates",
        "date": "2026-11-17",
        "model": MODEL_LABELS[model_name],
        "environment": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "platform": platform.platform(),
        },
        "candidate_seed_base": CANDIDATE_SEED_BASE,
        "blind_seed": blind_seed,
        "candidates": ALL_CANDIDATES,
        "outputs": outputs,
        "evidence": {
            "metrics": {"file": metrics_path.name, "sha256": sha256(metrics_path.read_bytes())},
            "blind_sheet": {"file": f"review/{sheet_path.name}", "sha256": sha256(sheet_path.read_bytes())},
            "blind_key": {"file": key_path.name, "sha256": sha256(key_path.read_bytes())},
            "review_form": {"file": f"review/{form_path.name}", "sha256": sha256(form_path.read_bytes())},
        },
        "gates": "unset; no judgment or ranking performed",
        "predecessor": (
            "RD1 blind set uses v1 and is retained as history; do not use it as v2/v3 evidence"
            if model_name == "v2"
            else "RD3 blind set uses v2 and is retained as history; do not use it as v3 evidence"
        ),
        "unverified": [
            "human visual review and quality gate",
            "reference watercolor images (license not confirmed)",
            "product effect / UI / undo / save-reload",
            "preview and final render paths",
            "CPU/GPU parity, D3D12/Vulkan",
            "production performance at 1080p / 4K",
        ],
    }
    manifest_path = output_rd3 / manifest_name
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def check(output_rd3: Path, manifest_name: str = "manifest_rd3.json") -> None:
    manifest = json.loads((output_rd3 / manifest_name).read_text(encoding="utf-8"))
    bad = []
    for kind, ev in manifest["evidence"].items():
        f = output_rd3 / ev["file"]
        if not f.exists() or sha256(f.read_bytes()) != ev["sha256"]:
            bad.append(f"evidence {kind}: {ev['file']}")
    for stem, ev in manifest["outputs"].items():
        f = output_rd3 / "candidates" / ev["npy"]
        if not f.exists() or sha256(f.read_bytes()) != ev["npy_sha256"]:
            bad.append(f"output {stem}.npy")
        f = output_rd3 / "candidates" / ev["png"]
        if not f.exists() or sha256(f.read_bytes()) != ev["png_sha256"]:
            bad.append(f"output {stem}.png")
    key = json.loads((output_rd3 / "blind_key.json").read_text(encoding="utf-8"))
    form_ids = {
        line.split("|")[1].strip()
        for line in (output_rd3 / "review" / "CANDIDATE_BLIND_FORM.md").read_text(encoding="utf-8").splitlines()
        if line.startswith("| C")
    }
    if set(key) != form_ids:
        bad.append(f"blind key/form ID mismatch: {sorted(set(key) ^ form_ids)}")
    for bid, rec in key.items():
        stem = f"{rec['fixture']}_{rec['candidate']}"
        if stem not in manifest["outputs"]:
            bad.append(f"blind key {bid} points to missing output {stem}")
    if bad:
        raise SystemExit("RD3 consistency check FAILED:\n  " + "\n  ".join(bad))
    print(f"RD3 consistency check OK: {len(manifest['evidence'])} evidence, "
          f"{len(manifest['outputs'])} outputs, {len(key)} blind IDs")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rd0", default="experiments/WatercolorLab/captures/rd0")
    parser.add_argument("--output", default="experiments/WatercolorLab/captures/rd3")
    parser.add_argument("--model", choices=sorted(MODEL_LABELS), default="v2")
    parser.add_argument("--check", action="store_true", help="verify hashes and blind key/sheet/form consistency")
    parser.add_argument("--manifest-name", default="manifest_rd3.json")
    args = parser.parse_args()
    output = Path(args.output)
    if args.check:
        check(output, args.manifest_name)
        return
    rd0 = Path(args.rd0)
    if not (rd0 / "fixtures_manifest.json").exists():
        raise SystemExit(f"missing RD0 fixtures manifest: {rd0}")
    from watercolor_reference_v3 import apply_candidate_v3
    from watercolor_reference_v4 import apply_candidate_v4
    from watercolor_reference_v5 import apply_candidate_v5

    apply = {"v2": apply_candidate_v2, "v3": apply_candidate_v3,
             "v4": apply_candidate_v4, "v5": apply_candidate_v5}[args.model]
    blind_seed = BLIND_SEED + {"v2": 0, "v3": 300, "v4": 400, "v5": 500}[args.model]
    manifest = run(rd0, output, model_name=args.model, apply=apply, blind_seed=blind_seed)
    print(f"{args.model} blind set complete: {len(manifest['outputs'])} outputs -> {output}")
    check(output)


if __name__ == "__main__":
    main()
