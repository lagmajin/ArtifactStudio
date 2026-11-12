#!/usr/bin/env python3
"""Reaggregate selected width-factor outliers by occurrence, pair, and point."""

from __future__ import annotations

import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT_MANIFEST = ROOT / "temporal_outlier_manifest.json"
INPUT_CSV = ROOT / "width_outlier_factor_cohort_temporal.csv"
RAW = ROOT / "width_outlier_dependence.csv"
MANIFEST = ROOT / "width_outlier_dependence_manifest.json"
METHODS = ("true_segmentation_true_weights",
           "true_segmentation_estimated_weights",
           "estimated_segmentation_true_weights",
           "estimated_segmentation_estimated_weights")
WIDTH_SIGNS = ("minus_2pct", "plus_2pct")
STEP_COUNTS = (128, 512, 2048, 4096)


def main():
    selected = json.loads(INPUT_MANIFEST.read_text(encoding="utf-8"))["selected_outliers"]
    identity = {str(index): {
        "pair_index": int(item["pair_index"]),
        "center_pixel_index": item["center_pixel_index"],
        "world_point": tuple(item["world_point"]),
    } for index, item in enumerate(selected)}
    observations = defaultdict(list)
    with INPUT_CSV.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            index = row["outlier_index"]
            item = identity[index]
            key = (row["width_sign"], int(row["steps"]), row["method"])
            value = float(row["temporal_residual_max_channel"])
            observations[("selected_pair", index, key)].append((value, item))
            observations[("unique_camera_pair", str(item["pair_index"]), key)].append((value, item))
            point_key = json.dumps([float(axis) for axis in item["world_point"]],
                                   separators=(",", ":"))
            observations[("unique_world_point", point_key, key)].append((value, item))

    grouped = {}
    for (level, group_key, (width_sign, steps, method)), members in observations.items():
        representative = max(members, key=lambda pair: pair[0])[1]
        grouped[(level, group_key, width_sign, steps, method)] = {
            "group_count": len(members),
            "max_channel_absolute": max(value for value, _ in members),
            "pair_index": representative["pair_index"] if level != "unique_camera_pair" else int(group_key),
            "world_point": representative["world_point"],
            "center_pixel_index": representative["center_pixel_index"],
        }

    rows = []
    summary = {}
    for width_sign in WIDTH_SIGNS:
        summary[width_sign] = {}
        for steps in STEP_COUNTS:
            summary[width_sign][str(steps)] = {}
            for method in METHODS:
                summary[width_sign][str(steps)][method] = {}
                for level in ("selected_pair", "unique_camera_pair", "unique_world_point"):
                    selected_groups = [
                        (group_key, value) for (group_level, group_key, sign, count, candidate), value
                        in grouped.items()
                        if group_level == level and sign == width_sign
                        and count == steps and candidate == method
                    ]
                    selected_groups.sort(key=lambda entry: entry[1]["max_channel_absolute"], reverse=True)
                    values = [value["max_channel_absolute"] for _, value in selected_groups]
                    summary[width_sign][str(steps)][method][level] = {
                        "group_count": len(values),
                        "maximum": max(values) if values else None,
                        "top_groups": [{"group_key": group_key,
                                        "max_channel_absolute": value["max_channel_absolute"],
                                        "pair_index": value["pair_index"],
                                        "world_point": value["world_point"],
                                        "selected_occurrence_count": value["group_count"]}
                                       for group_key, value in selected_groups[:5]],
                    }

    for (level, group_key, width_sign, steps, method), value in sorted(grouped.items()):
        rows.append({
            "aggregation_level": level, "group_key": group_key,
            "width_sign": width_sign, "steps": steps, "method": method,
            "selected_occurrence_count": value["group_count"],
            "pair_index": value["pair_index"],
            "center_pixel_index": value["center_pixel_index"],
            "world_point": value["world_point"],
            "max_channel_absolute": value["max_channel_absolute"],
        })
    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    manifest = {
        "schema": "fog-shadow-width-outlier-dependence-rd-v1",
        "purpose": "Aggregation sensitivity of selected temporal outlier width-factor results to repeated pair and world-point identities.",
        "input_manifest_sha256": hashlib.sha256(INPUT_MANIFEST.read_bytes()).hexdigest(),
        "input_factor_csv_sha256": hashlib.sha256(INPUT_CSV.read_bytes()).hexdigest(),
        "selected_occurrence_count": len(identity),
        "unique_camera_pair_count": len({item["pair_index"] for item in identity.values()}),
        "unique_world_point_count": len({item["world_point"] for item in identity.values()}),
        "raw_result_count": len(rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "summary": summary,
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "All selected occurrences were chosen by a prior screen-space temporal residual ranking.",
            "Repeated points and pairs are dependent; this is descriptive aggregation, not inferential statistics.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("occurrences", len(identity), "unique pairs", manifest["unique_camera_pair_count"],
          "unique points", manifest["unique_world_point_count"],
          "rows", len(rows))
    for steps in STEP_COUNTS:
        point_summary = summary["plus_2pct"][str(steps)]["estimated_segmentation_estimated_weights"]["unique_world_point"]
        pair_summary = summary["plus_2pct"][str(steps)]["estimated_segmentation_estimated_weights"]["unique_camera_pair"]
        occurrence_summary = summary["plus_2pct"][str(steps)]["estimated_segmentation_estimated_weights"]["selected_pair"]
        print(steps, "selected max", occurrence_summary["maximum"],
              "unique-point max", point_summary["maximum"],
              "unique-pair max", pair_summary["maximum"],
              "top point", point_summary["top_groups"][0])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
