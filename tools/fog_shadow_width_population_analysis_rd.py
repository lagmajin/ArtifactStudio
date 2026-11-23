#!/usr/bin/env python3
"""Summarize width-factor temporal effects across the complete 440-point fixture."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
EFFECTS_CSV = ROOT / "quadrature_width_factor_effects.csv"
EFFECTS_MANIFEST = ROOT / "quadrature_width_factor_effects_manifest.json"
OUTLIER_MANIFEST = ROOT / "temporal_outlier_manifest.json"
RAW = ROOT / "quadrature_width_population_analysis.csv"
MANIFEST = ROOT / "quadrature_width_population_analysis_manifest.json"
AMPLITUDES = (0.5, 1.0, 2.0)
SIGNS = ("minus_2pct", "plus_2pct")
EFFECTS = ("segmentation_main_effect", "importance_weight_main_effect",
           "segmentation_weight_interaction", "combined_candidate_shift")
REGIONS = ("all", "silhouette_edge", "shadow_interior")


def percentile(values, p):
    ordered = sorted(values)
    return ordered[math.ceil(p * len(ordered)) - 1]


def main():
    effect_manifest = json.loads(EFFECTS_MANIFEST.read_text(encoding="utf-8"))
    outlier_manifest = json.loads(OUTLIER_MANIFEST.read_text(encoding="utf-8"))
    point_info = {}
    by_point = {}
    pair_row_count = defaultdict(int)
    with EFFECTS_CSV.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            amp = float(row["motion_amplitude"])
            sign = row["width_sign"]
            effect = row["effect"]
            point_index = int(row["source_point_index"])
            region = row["source_region"]
            point = tuple(float(v) for v in ast.literal_eval(row["world_point"]))
            point_info[point_index] = {"world_point": point, "source_region": region}
            key = (amp, sign, effect, point_index)
            value = float(row["max_channel_absolute"])
            by_point[key] = max(value, by_point.get(key, 0.0))
            pair_row_count[(amp, sign, effect, point_index)] += 1

    records = []
    summaries = {}
    for amp in AMPLITUDES:
        summaries[f"{amp:.2f}"] = {}
        for sign in SIGNS:
            summaries[f"{amp:.2f}"][sign] = {}
            for effect in EFFECTS:
                summaries[f"{amp:.2f}"][sign][effect] = {}
                for region in REGIONS:
                    selected_points = [
                        (index, info, value) for (candidate_amp, candidate_sign,
                                                  candidate_effect, index), value in by_point.items()
                        if candidate_amp == amp and candidate_sign == sign
                        and candidate_effect == effect
                        and (region == "all" or point_info[index]["source_region"] == region)
                        for info in (point_info[index],)
                    ]
                    values = [item[2] for item in selected_points]
                    summary = {
                        "unique_point_count": len(values),
                        "pair_rows_total": sum(pair_row_count[(amp, sign, effect, index)]
                                                for index, _, _ in selected_points),
                        "point_max_p50": percentile(values, 0.50) if values else None,
                        "point_max_p95": percentile(values, 0.95) if values else None,
                        "point_max_max": max(values) if values else None,
                    }
                    summaries[f"{amp:.2f}"][sign][effect][region] = summary
                    records.append({
                        "record_type": "population_point_distribution",
                        "motion_amplitude": amp, "width_sign": sign,
                        "effect": effect, "region": region,
                        **summary, "source_point_index": "", "world_point": "",
                        "selected_outlier_indices": "", "population_rank": "",
                    })

    selected_points = []
    for outlier_index, item in enumerate(outlier_manifest["selected_outliers"]):
        target = tuple(float(v) for v in item["world_point"])
        match_index = min(point_info, key=lambda index: math.sqrt(sum(
            (point_info[index]["world_point"][i] - target[i]) ** 2 for i in range(3))))
        distance = math.sqrt(sum((point_info[match_index]["world_point"][i] - target[i]) ** 2
                                 for i in range(3)))
        if distance > 1.0e-4:
            continue
        selected_points.append((outlier_index, match_index, target, int(item["pair_index"])))
    unique_selected = {}
    for outlier_index, point_index, point, pair_index in selected_points:
        unique_selected.setdefault(point_index, {"world_point": point, "outlier_indices": [],
                                                "pair_indices": set()})
        unique_selected[point_index]["outlier_indices"].append(outlier_index)
        unique_selected[point_index]["pair_indices"].add(pair_index)

    selected_summary = []
    for amp in AMPLITUDES:
        for sign in SIGNS:
            for point_index, info in unique_selected.items():
                population_values = sorted(
                    ((value, index) for (candidate_amp, candidate_sign, effect, index), value
                     in by_point.items()
                     if candidate_amp == amp and candidate_sign == sign
                     and effect == "combined_candidate_shift"), reverse=True)
                lookup = {index: value for value, index in population_values}
                if point_index not in lookup:
                    continue
                value = lookup[point_index]
                rank = 1 + sum(other_value > value for other_value, _ in population_values)
                selected_summary.append({
                    "record_type": "selected_point_population_rank",
                    "motion_amplitude": amp, "width_sign": sign,
                    "effect": "combined_candidate_shift", "region": point_info[point_index]["source_region"],
                    "unique_point_count": len(population_values), "pair_rows_total": "",
                    "point_max_p50": "", "point_max_p95": "", "point_max_max": "",
                    "source_point_index": point_index, "world_point": info["world_point"],
                    "selected_outlier_indices": info["outlier_indices"],
                    "population_rank": rank, "point_max_absolute": value,
                    "selected_pair_indices": sorted(info["pair_indices"]),
                })
    records.extend(selected_summary)

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        fieldnames = list(dict.fromkeys(key for record in records for key in record))
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(records)

    manifest = {
        "schema": "fog-shadow-width-population-analysis-rd-v1",
        "purpose": "Post-hoc complete-fixture point-level distribution and selected-outlier rank analysis for the width factorial.",
        "population_source_points": len(point_info),
        "motion_amplitudes": AMPLITUDES,
        "width_signs": SIGNS,
        "candidate_steps": 128,
        "reference_steps": 4096,
        "validation_steps": 2048,
        "effect_input_sha256": hashlib.sha256(EFFECTS_CSV.read_bytes()).hexdigest(),
        "effect_manifest_sha256": hashlib.sha256(EFFECTS_MANIFEST.read_bytes()).hexdigest(),
        "selected_outlier_manifest_sha256": hashlib.sha256(OUTLIER_MANIFEST.read_bytes()).hexdigest(),
        "selected_occurrences_matched": len(selected_points),
        "selected_unique_points_matched": len(unique_selected),
        "record_count": len(records),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "summary": summaries,
        "limitations": [
            "This reuses a fixed synthetic fixture cohort of 440 center-frame shadowed points; it does not represent arbitrary scenes.",
            "All results are post-hoc CPU temporal numeric diagnostics; no visual/perceptual, GPU, alpha, or runtime acceptance is implied.",
            "Selected outlier points are ranked within the same source population that produced the width factorial, not an independent validation set.",
        ],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("population points", len(point_info), "selected matched", len(selected_points),
          "unique selected", len(unique_selected), "records", len(records))
    for amp in AMPLITUDES:
        x = summaries[f"{amp:.2f}"]["plus_2pct"]["combined_candidate_shift"]["all"]
        print("amp", amp, "combined +2 point-max p50/p95/max",
              x["point_max_p50"], x["point_max_p95"], x["point_max_max"])
    for record in selected_summary:
        if record["width_sign"] == "plus_2pct" and record["motion_amplitude"] == 1.0:
            print("selected rank", record["source_point_index"], record["population_rank"],
                  "/", record["unique_point_count"], "effect", record["point_max_absolute"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
