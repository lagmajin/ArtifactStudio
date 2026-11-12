#!/usr/bin/env python3
"""Post-hoc common-support summary for the AABB X-extent temporal sweep."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
import math
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "box_extent_temporal_sweep.csv"
RAW = ROOT / "box_extent_common_cohort.csv"
MANIFEST = ROOT / "box_extent_common_cohort_manifest.json"
SCALES = (0.75, 1.0, 1.25)


def aggregate(rows):
    if not rows:
        return {"pair_count": 0}
    values = sorted(float(row["temporal_residual_max_channel"]) for row in rows)
    squared = sum(float(row["temporal_residual_l2_squared"]) for row in rows)
    return {
        "pair_count": len(values),
        "temporal_residual_rmse_rgb": math.sqrt(squared / (3 * len(values))),
        "mean_temporal_residual_max_channel": sum(values) / len(values),
        "p95_temporal_residual_max_channel": values[math.ceil(0.95 * len(values)) - 1],
        "max_temporal_residual_max_channel": values[-1],
    }


def main():
    with INPUT.open(newline="", encoding="utf-8") as stream:
        source = list(csv.DictReader(stream))
    scales_by_key = {scale: set() for scale in SCALES}
    lookup = {}
    for row in source:
        scale = float(row["x_half_extent_scale"])
        method = row["method"]
        key = (int(row["pair_index"]), ast.literal_eval(row["world_point"]))
        lookup[(scale, method, key)] = row
        if method == "exact_interval_profile_128":
            scales_by_key[scale].add(key)
    common = set.intersection(*(scales_by_key[scale] for scale in SCALES))
    methods = sorted({row["method"] for row in source})
    common_rows = [row for row in source
                   if float(row["x_half_extent_scale"]) in SCALES
                   and (int(row["pair_index"]), ast.literal_eval(row["world_point"])) in common]
    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(common_rows[0]))
        writer.writeheader()
        writer.writerows(common_rows)

    summary = {}
    for scale in SCALES:
        summary[f"{scale:.2f}"] = {}
        for method in methods:
            selected = [row for row in common_rows
                        if float(row["x_half_extent_scale"]) == scale and row["method"] == method]
            summary[f"{scale:.2f}"][method] = {
                "all": aggregate(selected),
                "by_region": {region: aggregate([row for row in selected
                                                 if row["source_region"] == region])
                              for region in ("silhouette_edge", "shadow_interior")},
            }

    baseline_rows = [row for row in source
                     if float(row["x_half_extent_scale"]) == 1.0
                     and row["method"] == "center_plus_2pct_width_exact"]
    worst = max(baseline_rows, key=lambda row: float(row["temporal_residual_max_channel"]))
    worst_key = (int(worst["pair_index"]), ast.literal_eval(worst["world_point"]))
    worst_presence = {}
    for scale in SCALES:
        for method in methods:
            row = lookup.get((scale, method, worst_key))
            worst_presence[f"{scale:.2f}/{method}"] = None if row is None else {
                "temporal_residual_max_channel": float(row["temporal_residual_max_channel"]),
                "previous_interval": ast.literal_eval(row["previous_interval"]),
                "current_interval": ast.literal_eval(row["current_interval"]),
                "previous_exit_face": row["previous_exit_face"],
                "current_exit_face": row["current_exit_face"],
            }

    manifest = {
        "schema": "fog-shadow-box-x-extent-common-cohort-rd-v1",
        "purpose": "Post-hoc compare X-extent scenarios on the identical worldpoint/frame-pair intersection.",
        "input_manifest_sha256": hashlib.sha256((ROOT / "box_extent_temporal_manifest.json").read_bytes()).hexdigest(),
        "input_csv_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "scales": SCALES,
        "scenario_pair_counts": {f"{scale:.2f}": len(scales_by_key[scale]) for scale in SCALES},
        "common_worldpoint_frame_pair_count": len(common),
        "common_support_definition": "Intersection of (pair_index, world_point) keys with exact_interval_profile_128 rows in all three X-extent scenarios.",
        "summary": summary,
        "baseline_center_plus_max_outlier": {
            "pair_index": worst_key[0], "world_point": worst_key[1],
            "source_pixel": ast.literal_eval(worst["source_pixel"]),
            "region": worst["source_region"],
            "baseline_temporal_residual_max_channel": float(worst["temporal_residual_max_channel"]),
            "presence_by_scale_and_method": worst_presence,
        },
        "raw_result_count": len(common_rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "This is a post-hoc selection-bias audit; the common support excludes points not intersected by every box size.",
            "It does not replace scenario-specific statistics, which include each scenario's own valid point set.",
            "The center-plus maximum outlier is absent from the 0.75x scenario because its ray misses the narrower AABB; this is not a zero-error observation.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("common point-pairs", len(common), "rows", len(common_rows))
    for scale in SCALES:
        key = f"{scale:.2f}"
        print(key, summary[key]["center_plus_2pct_width_exact"]["all"])
    print("baseline worst", manifest["baseline_center_plus_max_outlier"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
