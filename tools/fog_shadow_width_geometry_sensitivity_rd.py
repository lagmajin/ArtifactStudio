#!/usr/bin/env python3
"""Relate all-population width-factor sensitivity to ray interval geometry."""

from __future__ import annotations

import ast
import bisect
import csv
import hashlib
import json
import math
import sys
from collections import defaultdict
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parent))
import fog_shadow_population_boundary_face_rd as face_audit


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
FACTOR_CSV = ROOT / "quadrature_width_factor_temporal.csv"
EFFECTS_CSV = ROOT / "quadrature_width_factor_effects.csv"
RAW = ROOT / "quadrature_width_geometry_sensitivity.csv"
MANIFEST = ROOT / "quadrature_width_geometry_sensitivity_manifest.json"
AMPLITUDES = (0.5, 1.0, 2.0)
SIGNS = ("minus_2pct", "plus_2pct")
QUARTILE_LABELS = ("Q1", "Q2", "Q3", "Q4")


def percentile(values, p):
    ordered = sorted(values)
    return ordered[math.ceil(p * len(ordered)) - 1]


def vector_stats(values):
    return {"count": len(values),
            "p50": percentile(values, 0.50) if values else None,
            "p95": percentile(values, 0.95) if values else None,
            "max": max(values) if values else None}


def main():
    exact_rows = {}
    with FACTOR_CSV.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["method"] != "true_segmentation_true_weights":
                continue
            amp = float(row["motion_amplitude"])
            key = (amp, int(row["pair_index"]), int(row["source_point_index"]))
            exact_rows[key] = row

    effect_values = {}
    with EFFECTS_CSV.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["effect"] != "combined_candidate_shift":
                continue
            key = (float(row["motion_amplitude"]), int(row["pair_index"]),
                   int(row["source_point_index"]), row["width_sign"])
            effect_values[key] = float(row["max_channel_absolute"])

    geometry_rows = []
    max_interval_reconciliation_error = 0.0
    geometry_by_amp = defaultdict(list)
    for (amp, pair_index, point_index), row in exact_rows.items():
        point = tuple(float(v) for v in ast.literal_eval(row["world_point"]))
        prev_x, curr_x = float(row["previous_camera_x"]), float(row["current_camera_x"])
        previous = face_audit.slab_faces(point, prev_x)
        current = face_audit.slab_faces(point, curr_x)
        prev_intervals = ast.literal_eval(row["previous_interval"])
        curr_intervals = ast.literal_eval(row["current_interval"])
        prev_width = sum(end - start for start, end in prev_intervals)
        curr_width = sum(end - start for start, end in curr_intervals)
        max_interval_reconciliation_error = max(
            max_interval_reconciliation_error,
            abs(prev_width - previous["normalized_width"]),
            abs(curr_width - current["normalized_width"]),
            abs(prev_intervals[0][0] - previous["normalized_entry"]),
            abs(curr_intervals[0][0] - current["normalized_entry"]),
            abs(prev_intervals[-1][1] - previous["normalized_exit"]),
            abs(curr_intervals[-1][1] - current["normalized_exit"]),
        )
        base = {
            "motion_amplitude": amp, "pair_index": pair_index,
            "source_point_index": point_index, "world_point": point,
            "source_pixel": ast.literal_eval(row["source_pixel"]),
            "source_region": row["source_region"], "surface": row["surface"],
            "previous_camera_x": prev_x, "current_camera_x": curr_x,
            "previous_entry_face": previous["entry_face"],
            "current_entry_face": current["entry_face"],
            "previous_exit_face": previous["exit_face"],
            "current_exit_face": current["exit_face"],
            "entry_face_changed": previous["entry_face"] != current["entry_face"],
            "exit_face_changed": previous["exit_face"] != current["exit_face"],
            "any_face_changed": (previous["entry_face"] != current["entry_face"] or
                                 previous["exit_face"] != current["exit_face"]),
            "previous_normalized_width": prev_width,
            "current_normalized_width": curr_width,
            "signed_normalized_width_delta": curr_width - prev_width,
            "absolute_normalized_width_delta": abs(curr_width - prev_width),
            "previous_normalized_entry": prev_intervals[0][0],
            "current_normalized_entry": curr_intervals[0][0],
            "signed_normalized_entry_delta": curr_intervals[0][0] - prev_intervals[0][0],
            "previous_normalized_exit": prev_intervals[-1][1],
            "current_normalized_exit": curr_intervals[-1][1],
            "signed_normalized_exit_delta": curr_intervals[-1][1] - prev_intervals[-1][1],
        }
        geometry_by_amp[amp].append(base)
        geometry_rows.append(base)

    # Quartile buckets are rank-based, deterministic, and geometry-only.
    cutpoints = {}
    for amp, items in geometry_by_amp.items():
        ordered = sorted(items, key=lambda item: (
            item["absolute_normalized_width_delta"], item["pair_index"],
            item["source_point_index"]))
        n = len(ordered)
        for rank, item in enumerate(ordered):
            item["width_delta_quartile"] = QUARTILE_LABELS[min(3, rank * 4 // n)]
        cutpoints[f"{amp:.2f}"] = {
            label: {"min": min(item["absolute_normalized_width_delta"] for item in ordered
                               if item["width_delta_quartile"] == label),
                    "max": max(item["absolute_normalized_width_delta"] for item in ordered
                               if item["width_delta_quartile"] == label),
                    "pair_rows": sum(item["width_delta_quartile"] == label for item in ordered)}
            for label in QUARTILE_LABELS
        }

    detailed_rows = []
    summary = {}
    for base in geometry_rows:
        amp = base["motion_amplitude"]
        for sign in SIGNS:
            detailed_rows.append({
                **base, "width_sign": sign,
                "combined_candidate_shift_max_channel": effect_values[(
                    amp, base["pair_index"], base["source_point_index"], sign)],
            })

    for amp in AMPLITUDES:
        summary[f"{amp:.2f}"] = {}
        cohort = geometry_by_amp[amp]
        for sign in SIGNS:
            summary[f"{amp:.2f}"][sign] = {}
            sensitivity = {(item["pair_index"], item["source_point_index"]): effect_values[
                (amp, item["pair_index"], item["source_point_index"], sign)] for item in cohort}
            for grouping, group_value in (
                ("face_switch", lambda item: "changed" if item["any_face_changed"] else "unchanged"),
                ("width_delta_quartile", lambda item: item["width_delta_quartile"]),
            ):
                grouped = defaultdict(list)
                for item in cohort:
                    grouped[group_value(item)].append(item)
                summary[f"{amp:.2f}"][sign][grouping] = {}
                for label, group in grouped.items():
                    pair_values = [sensitivity[(item["pair_index"], item["source_point_index"])]
                                   for item in group]
                    point_max = defaultdict(float)
                    for item, value in zip(group, pair_values):
                        point_max[item["source_point_index"]] = max(
                            point_max[item["source_point_index"]], value)
                    unique_point_values = list(point_max.values())
                    summary[f"{amp:.2f}"][sign][grouping][label] = {
                        "pair_level": vector_stats(pair_values),
                        "unique_point_max": vector_stats(unique_point_values),
                        "unique_world_points": len(unique_point_values),
                        "silhouette_edge_pair_rows": sum(item["source_region"] == "silhouette_edge"
                                                          for item in group),
                        "shadow_interior_pair_rows": sum(item["source_region"] == "shadow_interior"
                                                          for item in group),
                    }

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        fieldnames = list(dict.fromkeys(key for row in detailed_rows for key in row))
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(detailed_rows)
    manifest = {
        "schema": "fog-shadow-width-geometry-sensitivity-rd-v1",
        "purpose": "Post-hoc geometry-stratified association between AABB interval evolution and width-factor temporal sensitivity across the complete source-point fixture.",
        "factor_input_sha256": hashlib.sha256(FACTOR_CSV.read_bytes()).hexdigest(),
        "effects_input_sha256": hashlib.sha256(EFFECTS_CSV.read_bytes()).hexdigest(),
        "source_point_count": len({item["source_point_index"] for item in geometry_rows}),
        "pair_geometry_rows": len(geometry_rows),
        "temporal_effect_rows": len(detailed_rows),
        "motion_amplitudes": AMPLITUDES, "width_signs": SIGNS,
        "candidate_steps": 128, "reference_steps": 4096, "validation_steps": 2048,
        "geometry_width_delta_quartile_cutpoints": cutpoints,
        "maximum_interval_endpoint_reconciliation_error": max_interval_reconciliation_error,
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "summary": summary,
        "limitations": [
            "Quartile grouping is a descriptive post-hoc geometry stratification; rows repeat world points across camera pairs.",
            "source_region remains a center-frame mask label and is not a current-frame edge mask.",
            "No causal inference, perceptual gate, GPU/backend result, or runtime estimate is implied.",
        ],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("source points", manifest["source_point_count"],
          "geometry rows", len(geometry_rows), "effect rows", len(detailed_rows),
          "max interval reconciliation", max_interval_reconciliation_error)
    for amp in AMPLITUDES:
        print("amplitude", amp, "face switch", summary[f"{amp:.2f}"]["plus_2pct"]["face_switch"])
        print("amplitude", amp, "width quartiles", summary[f"{amp:.2f}"]["plus_2pct"]["width_delta_quartile"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
