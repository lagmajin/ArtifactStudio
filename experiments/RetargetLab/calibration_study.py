"""Calibration study (rd3): where does detail loss become breakage?

Adds control lanes at several removal fractions on the same fixtures:
  none      - 0% removal, sanity check
  decimate  - content-blind even column removal (selectivity reference, ~1.0 by construction)
  seam_*    - the same number of columns removed by seam carving

For synthetic fixtures the known-structure gates from rd0 are measured at each removal
fraction, so the Q4 block metrics can be bracketed against measured breakage instead of
guessed thresholds.

Procedure is pre-registered in EVALUATION_PLAN.md (rd3). This script only measures; the
derived thresholds are recorded in the plan and milestone doc afterwards.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

import localization_study as ls
import photo_study as ps
import retarget_study as rs

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "captures" / "rd3"
FRACTIONS = [0.0, 0.10, 0.20, 0.30, 1.0]  # 1.0 = maximum removable (W - height) / W
FAMILIES = ["decimate", "seam_backward", "seam_forward"]
SYNTHETIC = {
    "wires": "wires",
    "facade": "facade",
    "portrait": "portrait",
}


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def remove_decimate(work: np.ndarray, k: int):
    rows, width = work.shape[:2]
    if k <= 0:
        return work.copy(), np.broadcast_to(np.arange(width), (rows, width)).copy()
    indices = np.unique(np.floor(np.arange(k) * width / k).astype(int))[:k]
    keep = np.ones(width, bool)
    keep[indices] = False
    source_x = np.broadcast_to(np.nonzero(keep)[0], (rows, width - keep.size + keep.sum())).copy()
    return work[:, keep], source_x


def seam_lane(work: np.ndarray, k: int, mode: str):
    width = work.shape[1]
    if k <= 0:
        rows = work.shape[0]
        return work.copy(), np.broadcast_to(np.arange(width), (rows, width)).copy()
    return ps.carve(work, width - k, mode)[:2]


def known_structure(name: str, out: np.ndarray, alive: np.ndarray, fixture: dict) -> dict:
    """rd0's ground-truth gates, re-measured at this removal fraction."""
    result: dict[str, object] = {}
    if name == "wires":
        groups, _ = rs.column_structures(out, 0.45, 0.10)
        mask = fixture["mask"]
        result["wires_surviving"] = len(groups)
        result["wire_pixel_retention"] = round(float(alive[mask].mean()), 4)
        result["structure_pass"] = len(groups) >= 5
    elif name == "facade":
        groups, score = rs.column_structures(out, 0.45, 0.10)
        result["vertical_lines"] = len(groups)
        result["vertical_line_width_mean"] = round(float(np.mean([len(g) for g in groups])), 3) if groups else 0.0
        result["structure_pass"] = len(groups) >= 11
    elif name == "portrait":
        mask = fixture["mask"]
        retention = float(alive[mask].mean())
        bbox = rs.subject_bbox(out, fixture["subject_color"], 0.15)
        if bbox is None:
            aspect_error = None
        else:
            x0, y0, x1, y1 = bbox
            aspect = (x1 - x0 + 1) / float(y1 - y0 + 1)
            gt = (2 * 34.0) / (2 * 58.0)
            aspect_error = round(abs(aspect - gt) / gt, 4)
        result["subject_retention"] = round(retention, 4)
        result["subject_aspect_error"] = aspect_error
        result["structure_pass"] = retention >= 0.97 and (aspect_error is not None and aspect_error <= 0.05)
    return result


def evaluate_fixture(name: str, work: np.ndarray, kind: str, fixture: dict | None) -> dict:
    rows, width = work.shape[:2]
    full_k = width - rows
    entry: dict[str, object] = {
        "kind": kind,
        "source_size": [int(width), int(rows)],
        "full_removal_fraction": round(full_k / width, 4),
        "conditions": {},
    }
    for fraction in FRACTIONS:
        k = full_k if fraction >= 1.0 else int(round(fraction * width))
        families = ["none"] if k == 0 else FAMILIES
        for family in families:
            if family == "none":
                out, source_x = work.copy(), np.broadcast_to(np.arange(width), (rows, width)).copy()
            elif family == "decimate":
                out, source_x = remove_decimate(work, k)
            else:
                out, source_x = seam_lane(work, k, family.split("_", 1)[1])
            alive = ps.alive_map(source_x, width)
            block = ls.block_metrics(work, alive)
            record: dict[str, object] = {
                "requested_fraction": fraction if k != full_k else "full",
                "removed_columns": int(k),
                "actual_fraction": round(k / width, 4),
                "output_size": [int(out.shape[1]), int(out.shape[0])],
                "q4_compression_mean": block["q4_compression_mean"],
                "q4_compression_p95": block["q4_compression_p95"],
                "q1_compression_mean": block["q1_compression_mean"],
                "selectivity": block["selectivity"],
                "q4_energy_retention": block["q4_energy_retention"],
            }
            if fixture is not None:
                record.update(known_structure(name, out, alive, fixture))
            entry["conditions"][f"{family}@{k}/{width}"] = record
    return entry


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {
        "rd": "rd3",
        "study": "threshold calibration for detail-block removal",
        "fractions": FRACTIONS,
        "families": FAMILIES,
        "generator_sha256": sha256_file(Path(__file__)),
        "fixtures": {},
        "derivation_inputs": {
            "known_structure_gates": {
                "wires": "wires_surviving >= 5 of 6",
                "facade": "vertical_lines >= 11 of 12",
                "portrait": "subject_retention >= 0.97 and aspect_error <= 0.05",
            },
            "note": "Thresholds are derived from these measurements per the rule in EVALUATION_PLAN.md (rd3 B).",
        },
        "notes": [
            "decimate removes evenly spaced columns with no content awareness; its selectivity is ~1 by construction.",
            "The none lane must show zero compression and 1.0 retention; a deviation would indicate a bookkeeping bug.",
            "No visual review was performed.",
        ],
    }

    synthetic = rs.build_fixtures()
    for label, key in SYNTHETIC.items():
        fixture = synthetic[key]
        report["fixtures"][f"synthetic_{label}"] = evaluate_fixture(
            key, fixture["image"], "synthetic", fixture
        )
        report["fixtures"][f"synthetic_{label}"]["fixture_key"] = key

    for plate in ps.PLATES:
        work, info = ps.load_plate(plate)
        entry = evaluate_fixture(plate["name"], work, "photo", None)
        entry.update(
            {
                "source_path": plate["path"],
                "source_sha256": plate["sha256"],
                "license": plate["license"],
                "provenance": plate["provenance"],
            }
        )
        report["fixtures"][plate["name"]] = entry

    (OUT / "metrics.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")

    for name, entry in report["fixtures"].items():
        print(f"== {name} ({entry['kind']}, full={entry['full_removal_fraction']})")
        for condition, record in entry["conditions"].items():
            extra = ""
            if "structure_pass" in record:
                extra = f" pass={record['structure_pass']}"
                for key in ("wires_surviving", "vertical_lines", "subject_retention", "subject_aspect_error"):
                    if key in record:
                        extra += f" {key}={record[key]}"
            print("  %-24s q4_mean=%.3f q4_p95=%.3f sel=%s q4_ret=%s%s"
                  % (condition, record["q4_compression_mean"], record["q4_compression_p95"],
                     record["selectivity"], record["q4_energy_retention"], extra))


if __name__ == "__main__":
    main()
