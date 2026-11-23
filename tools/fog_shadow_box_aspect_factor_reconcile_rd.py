#!/usr/bin/env python3
"""Reconcile baseline true/true aspect-factor rows with existing width raw."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
NEW = ROOT / "box_aspect_factor_temporal.csv"
OLD = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/quadrature_width_factor_temporal.csv")
OUTPUT = ROOT / "box_aspect_factor_baseline_reconciliation.json"


def main():
    new_rows = {}
    with NEW.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["shape"] != "baseline":
                continue
            key = (float(row["motion_amplitude"]), int(row["pair_index"]),
                   int(row["source_point_index"]))
            vector = ast.literal_eval(row["true_true"])
            new_rows[key] = {
                "vector": vector,
                "reference_validation_max": float(row["reference_validation_max"]),
            }

    old_rows = {}
    with OLD.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["method"] != "true_segmentation_true_weights":
                continue
            key = (float(row["motion_amplitude"]), int(row["pair_index"]),
                   int(row["source_point_index"]))
            old_rows[key] = {
                "vector": ast.literal_eval(row["temporal_residual_delta_rgb"]),
                "reference_validation_max": max(
                    float(row["previous_reference_validation_relative_rgb_l2"]),
                    float(row["current_reference_validation_relative_rgb_l2"])),
            }

    shared = set(new_rows) & set(old_rows)
    differences = [abs(new_rows[key]["vector"][channel] - old_rows[key]["vector"][channel])
                   for key in shared for channel in range(3)]
    validation_differences = [abs(new_rows[key]["reference_validation_max"]
                                  - old_rows[key]["reference_validation_max"])
                              for key in shared]
    manifest = {
        "schema": "fog-shadow-box-aspect-factor-baseline-reconciliation-v1",
        "new_input": {"path": NEW.as_posix(), "sha256": hashlib.sha256(NEW.read_bytes()).hexdigest()},
        "prior_input": {"path": OLD.as_posix(), "sha256": hashlib.sha256(OLD.read_bytes()).hexdigest()},
        "new_baseline_true_true_rows": len(new_rows),
        "prior_true_true_rows": len(old_rows),
        "shared_keys": len(shared),
        "new_only_keys": len(set(new_rows) - set(old_rows)),
        "prior_only_keys": len(set(old_rows) - set(new_rows)),
        "max_temporal_rgb_component_absolute_difference": max(differences, default=0.0),
        "exact_temporal_vector_matches": sum(
            new_rows[key]["vector"] == old_rows[key]["vector"] for key in shared),
        "max_reference_validation_summary_difference": max(validation_differences, default=0.0),
        "conclusion": "Numerical parity check only; it validates baseline fixture reproduction, not factor validity outside the shared fixture.",
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    OUTPUT.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
