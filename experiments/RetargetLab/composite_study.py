"""Composite retarget study (rd4): split the reduction between scale, crop and seams.

Pure seam carving showed a heavy local tail (Q4 p95 0.40-0.62) and strong material
dependence. This study allocates the same total reduction across three operators and
checks whether the tail and the material spread shrink.

Procedure is pre-registered in EVALUATION_PLAN.md (rd4):
  - grid: scale factor s in {1.00, 0.90, 0.85} x crop share in {0.0, 0.5}
  - crop window = highest-energy contiguous window of the required width
  - selection objective: structure_correlation, subject to anisotropy <= 1.18
  - local tail and known-structure counts are reported as independent evidence
"""

from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import numpy as np

import localization_study as ls
import photo_study as ps
import retarget_study as rs

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd4"
SCALES = [1.00, 0.90, 0.85]
CROP_SHARES = [0.0, 0.5]
ANISOTROPY_LIMIT = 1.18
SYNTHETIC = {"wires": "wires", "facade": "facade", "portrait": "portrait"}


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def gradient_correlation(a: np.ndarray, b: np.ndarray) -> float:
    ga = ps.structure_masks(a)[0].reshape(-1).astype(np.float64)
    gb = ps.structure_masks(b)[0].reshape(-1).astype(np.float64)
    ga -= ga.mean()
    gb -= gb.mean()
    denom = float(np.sqrt((ga * ga).sum() * (gb * gb).sum()))
    if denom <= 1e-12:
        return float("nan")
    return float((ga * gb).sum() / denom)


def best_crop_window(work: np.ndarray, keep: int) -> int:
    """Offset of the contiguous keep-wide window with the most gradient energy."""
    width = work.shape[1]
    if keep >= width:
        return 0
    columns = ps.structure_masks(work)[0].sum(axis=0).astype(np.float64)
    prefix = np.concatenate(([0.0], np.cumsum(columns)))
    window = prefix[keep:] - prefix[:-keep]
    return int(np.argmax(window))


def scale_mask(mask: np.ndarray, width: int) -> np.ndarray:
    rows, source_width = mask.shape
    if width == source_width:
        return mask.copy()
    source_x = np.clip((np.arange(width) * source_width / width).astype(int), 0, source_width - 1)
    return mask[:, source_x]


def run_config(work: np.ndarray, mask: np.ndarray | None, scale: float, crop_share: float):
    rows, width = work.shape[:2]
    target = rows
    if scale < 1.0:
        intermediate = rs.resize_stretch(work, max(target, int(round(width * scale))), rows)
        inter_mask = None if mask is None else scale_mask(mask, intermediate.shape[1])
    else:
        intermediate = work.copy()
        inter_mask = None if mask is None else mask.copy()

    surplus = intermediate.shape[1] - target
    crop_columns = int(round(crop_share * surplus))
    keep = intermediate.shape[1] - crop_columns
    offset = best_crop_window(intermediate, keep)
    cropped = intermediate[:, offset:offset + keep]
    cropped_mask = None if inter_mask is None else inter_mask[:, offset:offset + keep]

    seam_columns = max(0, keep - target)
    if seam_columns > 0:
        out, source_x, _ = ps.carve(cropped, target, "forward")
    else:
        out = cropped
        source_x = np.broadcast_to(np.arange(keep), (rows, keep)).copy()

    alive_crop_seam = ps.alive_map(source_x, keep)
    # Map back onto the pre-crop intermediate grid so block metrics share the same frame.
    alive_intermediate = np.zeros((rows, intermediate.shape[1]), bool)
    alive_intermediate[:, offset:offset + keep] = alive_crop_seam
    if cropped_mask is None:
        gt = None
    else:
        gt = {
            "mask": cropped_mask,
            "alive": alive_crop_seam,
            "retention": float(alive_crop_seam[cropped_mask].mean()) if cropped_mask.any() else None,
        }
    return {
        "output": out,
        "intermediate": intermediate,
        "alive": alive_intermediate,
        "crop_columns": crop_columns,
        "crop_offset": offset,
        "seam_columns": seam_columns,
        "gt": gt,
    }


def known_structure(name: str, out: np.ndarray, result: dict, fixture: dict) -> dict:
    info: dict[str, object] = {}
    if name == "wires":
        groups, _ = rs.column_structures(out, 0.45, 0.10)
        info["wires_surviving"] = len(groups)
        info["wire_pixel_retention"] = round(float(result["gt"]["retention"]), 4)
    elif name == "facade":
        groups, _ = rs.column_structures(out, 0.45, 0.10)
        info["vertical_lines"] = len(groups)
        info["vertical_line_width_mean"] = (
            round(float(np.mean([len(g) for g in groups])), 3) if groups else 0.0
        )
    elif name == "portrait":
        info["subject_retention"] = round(float(result["gt"]["retention"]), 4)
        bbox = rs.subject_bbox(out, fixture["subject_color"], 0.15)
        if bbox is None:
            info["subject_aspect_error"] = None
        else:
            x0, y0, x1, y1 = bbox
            aspect = (x1 - x0 + 1) / float(y1 - y0 + 1)
            gt_aspect = (2 * 34.0) / (2 * 58.0)
            info["subject_aspect_error"] = round(abs(aspect - gt_aspect) / gt_aspect, 4)
    return info


def evaluate(name: str, work: np.ndarray, mask: np.ndarray | None, kind: str, fixture: dict | None) -> dict:
    rows, width = work.shape[:2]
    reference = rs.resize_stretch(work, rows, rows)
    entry: dict[str, object] = {
        "kind": kind,
        "source_size": [int(width), int(rows)],
        "configs": {},
    }

    configs = [("stretch_only", rows / width, None)]
    for scale in SCALES:
        for share in CROP_SHARES:
            configs.append((f"s{scale:.2f}_c{share:.1f}", scale, share))

    for label, scale, share in configs:
        t0 = time.perf_counter()
        if share is None:  # pure non-uniform stretch, no seam/crop: correlation anchor
            out = reference
            entry_config: dict[str, object] = {
                "anisotropy": round(1.0 / scale, 4),
                "crop_columns": 0,
                "seam_columns": 0,
                "structure_correlation": 1.0,
                "q4_compression_mean": None,
                "q4_compression_p95": None,
                "runtime_ms": round((time.perf_counter() - t0) * 1000.0, 1),
            }
        else:
            result = run_config(work, mask, scale, share)
            out = result["output"]
            block = ls.block_metrics(result["intermediate"], result["alive"])
            entry_config = {
                "anisotropy": round(1.0 / scale, 4),
                "crop_columns": result["crop_columns"],
                "crop_offset": result["crop_offset"],
                "seam_columns": result["seam_columns"],
                "structure_correlation": round(gradient_correlation(out, reference), 4),
                "q4_compression_mean": block["q4_compression_mean"],
                "q4_compression_p95": block["q4_compression_p95"],
                "q4_energy_retention": block["q4_energy_retention"],
                "runtime_ms": round((time.perf_counter() - t0) * 1000.0, 1),
            }
            if fixture is not None and result["gt"] is not None:
                entry_config.update(known_structure(name, out, result, fixture))
        entry["configs"][label] = entry_config

    # Pre-registered selection: best correlation under the anisotropy constraint.
    feasible = [
        (label, c) for label, c in entry["configs"].items()
        if c["anisotropy"] <= ANISOTROPY_LIMIT and c.get("structure_correlation") is not None
        and label != "stretch_only"
    ]
    if feasible:
        chosen = max(feasible, key=lambda item: item[1]["structure_correlation"])
        entry["selected"] = chosen[0]
        entry["selection_rule"] = "max structure_correlation subject to anisotropy <= 1.18"
    return entry


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "rd": "rd4",
        "study": "composite scale + crop + seam allocation",
        "grid": {"scales": SCALES, "crop_shares": CROP_SHARES},
        "anisotropy_limit": ANISOTROPY_LIMIT,
        "generator_sha256": sha256_file(Path(__file__)),
        "fixtures": {},
        "notes": [
            "Crop window is the highest-energy contiguous window, so crop is content aware.",
            "Local tail metrics are computed on the post-scale intermediate image.",
            "structure_correlation compares gradient magnitude against a LANCZOS resize of the source to the output width.",
            "No pass/fail gate: rd3 retired absolute block thresholds. This study compares configurations.",
            "No visual review was performed.",
        ],
    }

    synthetic = rs.build_fixtures()
    for label, key in SYNTHETIC.items():
        fixture = synthetic[key]
        entry = evaluate(key, fixture["image"], fixture["mask"], "synthetic", fixture)
        entry["fixture_key"] = key
        report["fixtures"][f"synthetic_{label}"] = entry

    for plate in ps.PLATES:
        work, _ = ps.load_plate(plate)
        entry = evaluate(plate["name"], work, None, "photo", None)
        entry.update(
            {
                "source_path": plate["path"],
                "source_sha256": plate["sha256"],
                "license": plate["license"],
            }
        )
        report["fixtures"][plate["name"]] = entry

    (OUT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")

    for name, entry in report["fixtures"].items():
        print(f"== {name} selected={entry.get('selected')}")
        for label, c in entry["configs"].items():
            extra = ""
            for key in ("wires_surviving", "vertical_lines", "subject_retention", "subject_aspect_error"):
                if key in c:
                    extra += f" {key}={c[key]}"
            print("  %-16s aniso=%.2f crop=%3d seam=%3d corr=%s q4p95=%s q4mean=%s%s"
                  % (label, c["anisotropy"], c["crop_columns"], c["seam_columns"],
                     c["structure_correlation"], c["q4_compression_p95"],
                     c["q4_compression_mean"], extra))


if __name__ == "__main__":
    main()
