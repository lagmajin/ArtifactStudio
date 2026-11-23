#!/usr/bin/env python3
"""Exploratory active-box-face analysis for direct-ray temporal residuals."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
import math
from pathlib import Path

import fog_shadow_camera_sequence_rd as sequence


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "population_temporal_direct_ray.csv"
RAW = ROOT / "population_boundary_face_audit.csv"
MANIFEST = ROOT / "population_boundary_face_manifest.json"
BOX_MIN = sequence.SHADOW_VOLUME_MIN
BOX_MAX = sequence.SHADOW_VOLUME_MAX


def subtract(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def norm(v):
    return math.sqrt(dot(v, v))


def slab_faces(world_point, camera_x):
    origin, _right, _up, _forward = sequence.camera_frame(camera_x)
    delta = subtract(world_point, origin)
    distance = norm(delta)
    direction = tuple(value / distance for value in delta)
    entries, exits = [], []
    for axis, name in enumerate("xyz"):
        low_t = (BOX_MIN[axis] - origin[axis]) / direction[axis]
        high_t = (BOX_MAX[axis] - origin[axis]) / direction[axis]
        if low_t <= high_t:
            entries.append((low_t, f"{name}:min"))
            exits.append((high_t, f"{name}:max"))
        else:
            entries.append((high_t, f"{name}:max"))
            exits.append((low_t, f"{name}:min"))
    entry_t, entry_face = max(entries)
    exit_t, exit_face = min(exits)
    return {"entry_face": entry_face, "exit_face": exit_face,
            "entry_t": entry_t, "exit_t": exit_t,
            "distance": distance,
            "normalized_entry": entry_t / distance,
            "normalized_exit": exit_t / distance,
            "normalized_width": (exit_t - entry_t) / distance}


def aggregate(rows, method):
    values = sorted(float(row[method]) for row in rows)
    if not values:
        return {"pair_count": 0}
    return {"pair_count": len(values),
            "mean_temporal_residual_max_channel": sum(values) / len(values),
            "p50_temporal_residual_max_channel": values[len(values) // 2],
            "p95_temporal_residual_max_channel": values[math.ceil(0.95 * len(values)) - 1],
            "max_temporal_residual_max_channel": values[-1]}


def main():
    with INPUT.open(newline="", encoding="utf-8") as stream:
        source_rows = list(csv.DictReader(stream))
    methods = sorted({row["method"] for row in source_rows})
    grouped = {}
    for row in source_rows:
        key = (int(row["pair_index"]), ast.literal_eval(row["world_point"]))
        item = grouped.setdefault(key, {"pair_index": int(row["pair_index"]),
                                        "source_pixel": ast.literal_eval(row["source_pixel"]),
                                        "world_point": ast.literal_eval(row["world_point"]),
                                        "region": row["source_region"], "surface": row["surface"],
                                        "previous_camera_x": float(row["previous_camera_x"]),
                                        "current_camera_x": float(row["current_camera_x"]),
                                        "previous_true_intervals": ast.literal_eval(row["previous_true_intervals"]),
                                        "current_true_intervals": ast.literal_eval(row["current_true_intervals"]),
                                        "residuals": {}})
        item["residuals"][row["method"]] = float(row["temporal_residual_max_channel"])

    rows = []
    for item in grouped.values():
        previous = slab_faces(item["world_point"], item["previous_camera_x"])
        current = slab_faces(item["world_point"], item["current_camera_x"])
        row = {
            "pair_index": item["pair_index"], "source_pixel": item["source_pixel"],
            "world_point": item["world_point"], "region": item["region"],
            "surface": item["surface"], "previous_camera_x": item["previous_camera_x"],
            "current_camera_x": item["current_camera_x"],
            "previous_entry_face": previous["entry_face"], "current_entry_face": current["entry_face"],
            "previous_exit_face": previous["exit_face"], "current_exit_face": current["exit_face"],
            "entry_face_changed": previous["entry_face"] != current["entry_face"],
            "exit_face_changed": previous["exit_face"] != current["exit_face"],
            "previous_normalized_interval": (previous["normalized_entry"], previous["normalized_exit"]),
            "current_normalized_interval": (current["normalized_entry"], current["normalized_exit"]),
            "previous_normalized_width": previous["normalized_width"],
            "current_normalized_width": current["normalized_width"],
            "normalized_width_delta": current["normalized_width"] - previous["normalized_width"],
            "normalized_entry_delta": current["normalized_entry"] - previous["normalized_entry"],
            "normalized_exit_delta": current["normalized_exit"] - previous["normalized_exit"],
        }
        row.update({f"residual_{key}": value for key, value in item["residuals"].items()})
        rows.append(row)

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {}
    for method in methods:
        column = f"residual_{method}"
        groups = {
            "all": rows,
            "exit_face_unchanged": [row for row in rows if not row["exit_face_changed"]],
            "exit_face_changed": [row for row in rows if row["exit_face_changed"]],
            "entry_face_unchanged": [row for row in rows if not row["entry_face_changed"]],
            "entry_face_changed": [row for row in rows if row["entry_face_changed"]],
            "silhouette_edge": [row for row in rows if row["region"] == "silhouette_edge"],
            "shadow_interior": [row for row in rows if row["region"] == "shadow_interior"],
        }
        summary[method] = {name: aggregate(group, column) for name, group in groups.items()}

    center_cases = ["center_minus_2pct_width_exact", "center_plus_2pct_width_exact"]
    top = sorted(rows, key=lambda row: max(float(row[f"residual_{name}"]) for name in center_cases),
                 reverse=True)[:20]
    top_outliers = [{
        "pair_index": row["pair_index"], "source_pixel": row["source_pixel"],
        "world_point": row["world_point"], "region": row["region"], "surface": row["surface"],
        "entry_faces": [row["previous_entry_face"], row["current_entry_face"]],
        "exit_faces": [row["previous_exit_face"], row["current_exit_face"]],
        "intervals": [row["previous_normalized_interval"], row["current_normalized_interval"]],
        "normalized_width_delta": row["normalized_width_delta"],
        "center_minus_residual": row["residual_center_minus_2pct_width_exact"],
        "center_plus_residual": row["residual_center_plus_2pct_width_exact"],
    } for row in top]

    manifest = {
        "schema": "fog-shadow-population-boundary-face-audit-rd-v1",
        "purpose": "Post-hoc descriptive audit of active AABB faces associated with direct-ray temporal residual outliers.",
        "input_temporal_manifest_sha256": hashlib.sha256((ROOT / "population_temporal_manifest.json").read_bytes()).hexdigest(),
        "input_csv_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "pair_count": len(rows), "methods": methods,
        "box_min": BOX_MIN, "box_max": BOX_MAX,
        "active_face_rule": "For each world point/camera ray, max slab near-t determines entry face and min slab far-t determines exit face; source and current camera are evaluated independently.",
        "summary": summary, "top_center_offset_outliers": top_outliers,
        "raw_result_count": len(rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "This is post-hoc exploratory analysis after inspecting the maximum temporal outlier; it is not an independent hypothesis test.",
            "The AABB is a synthetic shadow-volume proxy and is not a filtered geometric shadow map.",
            "The audit describes association between active faces and numerical residuals; it establishes neither causation in a production renderer nor perceptual acceptability.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("pairs", len(rows))
    for method in methods:
        values = summary[method]
        print(method, "all", values["all"],
              "exit face changed", values["exit_face_changed"],
              "unchanged", values["exit_face_unchanged"])
    print("top outlier", top_outliers[0])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
