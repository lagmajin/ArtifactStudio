#!/usr/bin/env python3
"""Post-hoc interval and profile-knot analysis for box aspect ratio sweep."""

from __future__ import annotations

import csv
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
import fog_shadow_box_aspect_width_rd as experiment
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft


ROOT = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
INPUT = ROOT / "box_aspect_width_temporal.csv"
OUTPUT = ROOT / "box_aspect_geometry_analysis.csv"
MANIFEST = ROOT / "box_aspect_geometry_analysis_manifest.json"
GROUPS = ("Q1", "Q2", "Q3", "Q4")


def quantile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        return None
    return ordered[min(len(ordered) - 1, max(0, math.ceil(fraction * len(ordered)) - 1))]


def stats(values):
    return {"count": len(values), "p50": quantile(values, 0.50),
            "p95": quantile(values, 0.95), "max": max(values) if values else None}


def assign_quartiles(rows, feature, output_name):
    ordered = sorted(rows, key=lambda row: (row[feature], row["pair_index"],
                                             row["source_point_index"]))
    n = len(ordered)
    for rank, row in enumerate(ordered):
        row[output_name] = GROUPS[min(3, rank * 4 // n)]
    return {
        group: {
            "min": min(row[feature] for row in ordered if row[output_name] == group),
            "max": max(row[feature] for row in ordered if row[output_name] == group),
            "pair_rows": sum(row[output_name] == group for row in ordered),
        }
        for group in GROUPS
    }


def summarize(rows):
    residual = [row["residual"] for row in rows]
    by_point = defaultdict(list)
    for row in rows:
        by_point[row["source_point_index"]].append(row)
    point_max = [max(group, key=lambda row: row["residual"]) for group in by_point.values()]
    return {
        "pair_rows": len(rows),
        "unique_points": len(by_point),
        "pair_residual": stats(residual),
        "unique_point_max_residual": stats([row["residual"] for row in point_max]),
        "pair_abs_exit_delta_rho": experiment.correlation(
            [row["abs_exit_delta_over_penumbra"] for row in rows], residual),
        "pair_abs_width_delta_rho": experiment.correlation(
            [row["abs_width_delta_over_penumbra"] for row in rows], residual),
        "pair_mean_width_rho": experiment.correlation(
            [row["mean_width_over_penumbra"] for row in rows], residual),
        "pair_knot_shift_rho": experiment.correlation(
            [row["max_knot_shift_over_penumbra"] for row in rows], residual),
        "point_max_abs_exit_delta_rho": experiment.correlation(
            [row["abs_exit_delta_over_penumbra"] for row in point_max],
            [row["residual"] for row in point_max]),
        "point_max_abs_width_delta_rho": experiment.correlation(
            [row["abs_width_delta_over_penumbra"] for row in point_max],
            [row["residual"] for row in point_max]),
    }


def main():
    by_group = defaultdict(list)
    with INPUT.open(newline="", encoding="utf-8") as stream:
        for raw in csv.DictReader(stream):
            entry_prev, entry_curr = float(raw["previous_entry"]), float(raw["current_entry"])
            exit_prev, exit_curr = float(raw["previous_exit"]), float(raw["current_exit"])
            width_prev, width_curr = float(raw["previous_width"]), float(raw["current_width"])
            true_prev = ((entry_prev, exit_prev),)
            true_curr = ((entry_curr, exit_curr),)
            estimated_prev = combined.estimate_intervals(true_prev, 0.0, experiment.WIDTH_ERROR)
            estimated_curr = combined.estimate_intervals(true_curr, 0.0, experiment.WIDTH_ERROR)

            def knot_motion(true_intervals, estimated_intervals):
                true_knots = soft.profile_knots(true_intervals, experiment.PENUMBRA)
                estimated_knots = soft.profile_knots(estimated_intervals, experiment.PENUMBRA)
                if len(true_knots) != len(estimated_knots):
                    raise ValueError("True and estimated knot topology differs")
                matched = [abs(a - b) for a, b in zip(true_knots, estimated_knots)]
                return max(matched, default=0.0), len(true_knots)

            prev_shift, prev_knot_count = knot_motion(true_prev, estimated_prev)
            curr_shift, curr_knot_count = knot_motion(true_curr, estimated_curr)
            max_knot_shift = max(prev_shift, curr_shift)
            mean_width = 0.5 * (width_prev + width_curr)
            key = (raw["shape"], float(raw["motion_amplitude"]))
            row = {
                "shape": raw["shape"], "motion_amplitude": float(raw["motion_amplitude"]),
                "pair_index": int(raw["pair_index"]),
                "source_point_index": int(raw["source_point_index"]),
                "previous_entry_face": raw["previous_entry_face"],
                "current_entry_face": raw["current_entry_face"],
                "previous_exit_face": raw["previous_exit_face"],
                "current_exit_face": raw["current_exit_face"],
                "entry_face_changed": raw["entry_face_changed"].lower() == "true",
                "exit_face_changed": raw["exit_face_changed"].lower() == "true",
                "previous_width_over_penumbra": width_prev / experiment.PENUMBRA,
                "current_width_over_penumbra": width_curr / experiment.PENUMBRA,
                "mean_width_over_penumbra": mean_width / experiment.PENUMBRA,
                "abs_entry_delta_over_penumbra": abs(entry_curr - entry_prev) / experiment.PENUMBRA,
                "abs_exit_delta_over_penumbra": abs(exit_curr - exit_prev) / experiment.PENUMBRA,
                "abs_width_delta_over_penumbra": abs(width_curr - width_prev) / experiment.PENUMBRA,
                "previous_max_knot_shift_over_penumbra": prev_shift / experiment.PENUMBRA,
                "current_max_knot_shift_over_penumbra": curr_shift / experiment.PENUMBRA,
                "max_knot_shift_over_penumbra": max_knot_shift / experiment.PENUMBRA,
                "true_profile_knot_count": prev_knot_count,
                "estimated_profile_knot_count": prev_knot_count,
                "residual": float(raw["temporal_residual_max_channel"]),
                "common_mean_width_quartile": "",
                "common_width_delta_quartile": "",
            }
            by_group[key].append(row)

    summary = {}
    detailed_rows = []
    for (shape, amplitude), rows in by_group.items():
        amp_key = f"{amplitude:.2f}"
        summary.setdefault(shape, {})[amp_key] = {"all_support": summarize(rows)}
        cutpoints = {
            "mean_width_over_penumbra": assign_quartiles(
                rows, "mean_width_over_penumbra", "mean_width_quartile"),
            "abs_width_delta_over_penumbra": assign_quartiles(
                rows, "abs_width_delta_over_penumbra", "width_delta_quartile"),
        }
        for feature_name, field in (("mean_width", "mean_width_quartile"),
                                    ("width_delta", "width_delta_quartile")):
            summary[shape][amp_key][feature_name + "_quartiles"] = {
                label: summarize([row for row in rows if row[field] == label])
                for label in GROUPS
            }
        summary[shape][amp_key]["quartile_cutpoints"] = cutpoints
        summary[shape][amp_key]["face_switch_pair_rows"] = sum(
            row["entry_face_changed"] or row["exit_face_changed"] for row in rows)
        detailed_rows.extend(rows)

    common_rows = []
    for amplitude in experiment.AMPLITUDES:
        key = f"{amplitude:.2f}"
        supports = {
            shape: {(row["pair_index"], row["source_point_index"])
                    for row in by_group[(shape, amplitude)]}
            for shape in experiment.SHAPES
        }
        common = set.intersection(*(supports[shape] for shape in experiment.SHAPES))
        common_summary = {"pair_point_rows": len(common),
                          "unique_points": len({point for _pair, point in common})}
        for shape in experiment.SHAPES:
            rows = [row for row in by_group[(shape, amplitude)]
                    if (row["pair_index"], row["source_point_index"]) in common]
            summary[shape][key]["common_support"] = summarize(rows)
            common_cutpoints = {
                "mean_width_over_penumbra": assign_quartiles(
                    rows, "mean_width_over_penumbra", "common_mean_width_quartile"),
                "abs_width_delta_over_penumbra": assign_quartiles(
                    rows, "abs_width_delta_over_penumbra", "common_width_delta_quartile"),
            }
            summary[shape][key]["common_support_quartiles"] = {
                "cutpoints": common_cutpoints,
                "mean_width": {
                    label: summarize([row for row in rows
                                     if row["common_mean_width_quartile"] == label])
                    for label in GROUPS
                },
                "width_delta": {
                    label: summarize([row for row in rows
                                     if row["common_width_delta_quartile"] == label])
                    for label in GROUPS
                },
            }
            common_rows.extend(rows)
        common_summary["shape_metrics"] = {
            shape: summary[shape][key]["common_support"] for shape in experiment.SHAPES
        }
        summary.setdefault("common_support", {})[key] = common_summary

    with OUTPUT.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(detailed_rows[0]))
        writer.writeheader()
        writer.writerows(detailed_rows)
    input_scripts = [Path(__file__), Path(experiment.__file__),
                     Path(experiment.combined.__file__), Path(experiment.soft.__file__)]
    manifest = {
        "schema": "fog-shadow-box-aspect-geometry-analysis-rd-v1",
        "purpose": "Relate within-shape true interval evolution and +2% width-estimate profile-knot motion to temporal residuals.",
        "input_csv": {"path": INPUT.as_posix(), "rows": sum(map(len, by_group.values())),
                      "sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest()},
        "shape_names": list(experiment.SHAPES),
        "camera_amplitudes": experiment.AMPLITUDES,
        "penumbra_ray_length_fraction": experiment.PENUMBRA,
        "candidate_width_error": experiment.WIDTH_ERROR,
        "metric_definitions": {
            "width_over_penumbra": "Normalized ray interval width divided by normalized penumbra width.",
            "abs_deltas_over_penumbra": "Absolute frame-to-frame normalized interval delta divided by normalized penumbra width.",
            "knot_shift_over_penumbra": "Maximum absolute position shift between sorted true/estimated profile_knots for one frame interval, divided by normalized penumbra width; pair value is max over previous/current frames.",
            "correlation": "Spearman rho with average ranks for ties; descriptive, no p-values.",
            "quartiles": "Deterministic rank quartiles within each shape/amplitude, computed from the named geometry feature only.",
        },
        "summary": summary,
        "output_csv": {"path": OUTPUT.as_posix(), "rows": len(detailed_rows),
                       "sha256": hashlib.sha256(OUTPUT.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "input_scripts": [{"path": path.as_posix(),
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in input_scripts],
        "limitations": [
            "All outcomes are post-hoc analyses of one deterministic synthetic AABB sequence.",
            "Point-pair rows repeat source world points; unique-point max is a sensitivity aggregation, not an independent sample CI.",
            "Profile-knot motion is algebraically derived from the same estimated interval endpoints and penumbra setting; it is not an independent causal variable.",
            "No visual/perceptual judgment, real shadow filtering, alpha contract, GPU/backend parity, or performance result is implied.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    for shape in experiment.SHAPES:
        for amplitude in experiment.AMPLITUDES:
            values = summary[shape][f"{amplitude:.2f}"]["all_support"]
            print(shape, amplitude,
                  "width/penumbra p50/p95", values["pair_rows"],
                  "knot rho", values["pair_knot_shift_rho"],
                  "exit rho", values["pair_abs_exit_delta_rho"],
                  "width rho", values["pair_abs_width_delta_rho"])
    print("raw rows", len(detailed_rows), "common support", summary["common_support"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
