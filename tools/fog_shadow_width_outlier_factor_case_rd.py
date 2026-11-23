#!/usr/bin/env python3
"""Factor width-driven segmentation and allocation for one selected outlier pair."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_shadow_outlier_convergence_rd as outlier
import fog_shadow_quadrature_width_factor_rd as factor
import fog_shadow_combined_error_rd as combined
import fog_shadow_temporal_outlier_rd as temporal
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_penumbra_rd as soft


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "temporal_outlier_manifest.json"
RAW = ROOT / "width_outlier_factor_case.csv"
MANIFEST = ROOT / "width_outlier_factor_case_manifest.json"
OUTLIER_INDEX = 1
STEP_COUNTS = (128, 512, 2048, 4096)
WIDTH_CASES = (("minus_2pct", -0.02), ("plus_2pct", 0.02))
FACTORS = (("true_segmentation_true_weights", "true", "true"),
           ("true_segmentation_estimated_weights", "true", "estimated"),
           ("estimated_segmentation_true_weights", "estimated", "true"),
           ("estimated_segmentation_estimated_weights", "estimated", "estimated"))
REFERENCE_STEPS = 16384


def relative_l2(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)) /
                     max(sum(value * value for value in b), 1e-30))


def main():
    selected_manifest = json.loads(INPUT.read_text(encoding="utf-8"))
    selected = selected_manifest["selected_outliers"][OUTLIER_INDEX]
    world_point = tuple(selected["world_point"])
    pair_index = int(selected["pair_index"])
    sun = temporal.sun_direction()
    rows = []
    per_frame = {}
    max_ref_validation = 0.0

    for frame_side, camera_x in (("previous", sequence.CAMERA_X[pair_index]),
                                 ("current", sequence.CAMERA_X[pair_index + 1])):
        case, truth = outlier.direct_case(world_point, camera_x, sun)
        reference = soft.integrate_cells(
            case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH,
                                       REFERENCE_STEPS), truth, sequence.PENUMBRA_WIDTH)
        validation = soft.integrate_cells(
            case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH,
                                       REFERENCE_STEPS // 2), truth, sequence.PENUMBRA_WIDTH)
        ref_validation = relative_l2(validation, reference)
        max_ref_validation = max(max_ref_validation, ref_validation)
        per_frame[frame_side] = {}

        for steps in STEP_COUNTS:
            exact_cells = factor.weighted_cells_split(
                case, truth, truth, truth, sequence.PENUMBRA_WIDTH, steps)
            exact_value = soft.integrate_cells(
                case, exact_cells, truth, sequence.PENUMBRA_WIDTH)
            per_frame[frame_side][(steps, "exact_true_true")] = exact_value
            rows.append({
                "frame_side": frame_side, "camera_x": camera_x,
                "steps": steps, "width_error": 0.0,
                "method": "exact_true_true", "true_intervals": truth,
                "estimated_intervals": truth, "candidate_rgb": exact_value,
                "reference_rgb": reference,
                "relative_rgb_l2_error": relative_l2(exact_value, reference),
                "reference_validation_relative_rgb_l2": ref_validation,
            })

            for width_name, width_error in WIDTH_CASES:
                estimated = combined.estimate_intervals(truth, 0.0, width_error)
                for method, segmentation_factor, weight_factor in FACTORS:
                    segmentation_intervals = truth if segmentation_factor == "true" else estimated
                    weight_intervals = truth if weight_factor == "true" else estimated
                    cells = factor.weighted_cells_split(
                        case, truth, segmentation_intervals, weight_intervals,
                        sequence.PENUMBRA_WIDTH, steps)
                    value = soft.integrate_cells(
                        case, cells, truth, sequence.PENUMBRA_WIDTH)
                    key = (steps, method, width_name)
                    per_frame[frame_side][key] = value
                    rows.append({
                        "frame_side": frame_side, "camera_x": camera_x,
                        "steps": steps, "width_error": width_error,
                        "method": f"{method}_{width_name}",
                        "true_intervals": truth, "estimated_intervals": estimated,
                        "candidate_rgb": value, "reference_rgb": reference,
                        "relative_rgb_l2_error": relative_l2(value, reference),
                        "reference_validation_relative_rgb_l2": ref_validation,
                    })

    temporal_rows = []
    for steps in STEP_COUNTS:
        exact_pair = tuple(per_frame["current"][(steps, "exact_true_true")][c]
                           - per_frame["previous"][(steps, "exact_true_true")][c]
                           for c in range(3))
        reference_pair = []
        for frame_side in ("previous", "current"):
            case, truth = outlier.direct_case(
                world_point,
                sequence.CAMERA_X[pair_index] if frame_side == "previous"
                else sequence.CAMERA_X[pair_index + 1], sun)
            reference_pair.append(soft.integrate_cells(
                case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH,
                                           REFERENCE_STEPS), truth,
                sequence.PENUMBRA_WIDTH))
        reference_delta = tuple(reference_pair[1][c] - reference_pair[0][c]
                                for c in range(3))
        for width_name, width_error in WIDTH_CASES:
            for method, _seg, _weight in FACTORS:
                prev = per_frame["previous"][(steps, method, width_name)]
                curr = per_frame["current"][(steps, method, width_name)]
                residual_delta = tuple((curr[c] - reference_pair[1][c])
                                       - (prev[c] - reference_pair[0][c])
                                       for c in range(3))
                shift_from_exact = tuple(residual_delta[c] -
                                         (exact_pair[c] - reference_delta[c])
                                         for c in range(3))
                temporal_rows.append({
                    "pair_index": pair_index, "steps": steps,
                    "width_sign": width_name, "method": method,
                    "temporal_residual_delta_rgb": residual_delta,
                    "temporal_residual_max_channel": max(abs(v) for v in residual_delta),
                    "factor_shift_from_exact_true_true_rgb": shift_from_exact,
                    "factor_shift_max_channel": max(abs(v) for v in shift_from_exact),
                })

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    temporal_raw = ROOT / "width_outlier_factor_case_temporal.csv"
    with temporal_raw.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(temporal_rows[0]))
        writer.writeheader()
        writer.writerows(temporal_rows)

    face_audit = ROOT / "population_boundary_face_audit.csv"
    manifest = {
        "schema": "fog-shadow-width-outlier-factor-case-rd-v1",
        "purpose": "Post-hoc 2x2 width factor and step convergence case study for the largest +2% paired contrast outlier.",
        "input_outlier_manifest_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "face_audit_sha256": hashlib.sha256(face_audit.read_bytes()).hexdigest(),
        "selected_outlier_index": OUTLIER_INDEX,
        "pair_index": pair_index, "world_point": world_point,
        "width_errors": [error for _, error in WIDTH_CASES],
        "candidate_steps": STEP_COUNTS, "reference_steps": REFERENCE_STEPS,
        "validation_steps": REFERENCE_STEPS // 2,
        "maximum_reference_validation_relative_rgb_l2": max_ref_validation,
        "factor_definitions": {
            "segmentation": "estimated interval selects profile knots and fully blocked-piece inclusion",
            "importance_weights": "estimated interval selects rho-times-visibility sample allocation",
            "integrand": "true interval in every candidate",
        },
        "raw_result_count": len(rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "temporal_result_count": len(temporal_rows),
        "temporal_raw_csv": {"path": temporal_raw.as_posix(),
                              "sha256": hashlib.sha256(temporal_raw.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "This is one selected outlier pair chosen after observing the prior sweep; it is descriptive, not an independent cohort.",
            "CPU synthetic AABB single-scattering only; no image, GPU, runtime, or perceptual evaluation.",
            "The paired factor shift is finite-step and does not prove an infinite-step bias limit.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("pair", pair_index, "world point", world_point,
          "rows", len(rows), "temporal rows", len(temporal_rows),
          "reference validation max", max_ref_validation)
    for row in temporal_rows:
        if row["width_sign"] == "plus_2pct":
            print(row["steps"], row["method"],
                  "temporal max", row["temporal_residual_max_channel"],
                  "shift vs exact", row["factor_shift_max_channel"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
