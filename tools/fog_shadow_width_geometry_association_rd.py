#!/usr/bin/env python3
"""Post-hoc rank association of ray interval motion and width-factor residuals."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "quadrature_width_geometry_sensitivity.csv"
OUTPUT = ROOT / "quadrature_width_geometry_association.csv"
MANIFEST = ROOT / "quadrature_width_geometry_association_manifest.json"
FEATURES = (
    "absolute_normalized_entry_delta",
    "absolute_normalized_exit_delta",
    "absolute_normalized_width_delta",
    "signed_normalized_entry_delta",
    "signed_normalized_exit_delta",
    "signed_normalized_width_delta",
)
SIGNS = ("minus_2pct", "plus_2pct")
AMPLITUDES = (0.5, 1.0, 2.0)


def average_ranks(values):
    order = sorted(range(len(values)), key=lambda index: (values[index], index))
    ranks = [0.0] * len(values)
    start = 0
    while start < len(order):
        end = start + 1
        while end < len(order) and values[order[end]] == values[order[start]]:
            end += 1
        rank = ((start + 1) + end) / 2.0
        for position in range(start, end):
            ranks[order[position]] = rank
        start = end
    return ranks


def pearson(x, y):
    n = len(x)
    if n < 2:
        return None
    mean_x = sum(x) / n
    mean_y = sum(y) / n
    dx = [value - mean_x for value in x]
    dy = [value - mean_y for value in y]
    denom = math.sqrt(sum(value * value for value in dx) * sum(value * value for value in dy))
    if denom == 0:
        return None
    return sum(a * b for a, b in zip(dx, dy)) / denom


def spearman(x, y):
    return pearson(average_ranks(x), average_ranks(y))


def quantile(values, q):
    ordered = sorted(values)
    if not ordered:
        return None
    position = (len(ordered) - 1) * q
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1 - fraction) + ordered[upper] * fraction


def summarize(values):
    return {
        "count": len(values),
        "p50": quantile(values, 0.50),
        "p95": quantile(values, 0.95),
        "p99": quantile(values, 0.99),
        "max": max(values) if values else None,
        "mean": sum(values) / len(values) if values else None,
    }


def top_enrichment(rows, feature, residual_fraction):
    n = len(rows)
    residual_cut = quantile([row["residual"] for row in rows], 1 - residual_fraction)
    feature_cut = quantile([abs(row[feature]) for row in rows], 0.95)
    residual_top = [row for row in rows if row["residual"] >= residual_cut]
    selected = [row for row in rows if abs(row[feature]) >= feature_cut]
    selected_keys = {(row["pair_index"], row["source_point_index"]) for row in selected}
    overlap = sum((row["pair_index"], row["source_point_index"]) in selected_keys
                  for row in residual_top)
    baseline_rate = sum(row["residual"] >= residual_cut for row in rows) / n if n else 0
    selected_rate = overlap / len(residual_top) if residual_top else None
    return {
        "residual_top_fraction": residual_fraction,
        "residual_top_rows": len(residual_top),
        "feature_top_5pct_cutoff_absolute": feature_cut,
        "feature_top_rows": len(selected_keys),
        "overlap_residual_top_rows": overlap,
        "residual_top_capture_rate": selected_rate,
        "baseline_capture_rate": baseline_rate,
        "enrichment_over_baseline": selected_rate / baseline_rate if baseline_rate else None,
    }


def main():
    rows = []
    with INPUT.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["width_sign"] not in SIGNS:
                continue
            item = {
                "motion_amplitude": float(row["motion_amplitude"]),
                "width_sign": row["width_sign"],
                "pair_index": int(row["pair_index"]),
                "source_point_index": int(row["source_point_index"]),
                "source_region": row["source_region"],
                "any_face_changed": row["any_face_changed"].lower() == "true",
                "residual": float(row["combined_candidate_shift_max_channel"]),
            }
            for name in FEATURES:
                value_column = name
                if name.startswith("absolute_"):
                    value_column = "signed_" + name.removeprefix("absolute_")
                value = float(row[value_column])
                item[name] = abs(value) if name.startswith("absolute_") else value
            rows.append(item)

    outputs = []
    summaries = {}
    for amplitude in AMPLITUDES:
        summaries[f"{amplitude:.2f}"] = {}
        for sign in SIGNS:
            cohort = [row for row in rows if row["motion_amplitude"] == amplitude
                      and row["width_sign"] == sign]
            point_rows = {}
            for row in cohort:
                key = row["source_point_index"]
                if key not in point_rows or row["residual"] > point_rows[key]["residual"]:
                    point_rows[key] = {**row, "aggregation": "unique_point_max"}
            pair_rows = [{**row, "aggregation": "pair"} for row in cohort]
            groups = {"pair": pair_rows, "unique_point_max": list(point_rows.values())}
            result = {"pair_rows": len(pair_rows), "unique_points": len(point_rows),
                      "face_switch_pair_rows": sum(row["any_face_changed"] for row in cohort),
                      "features": {}}
            for feature in FEATURES:
                result["features"][feature] = {}
                for group_name, group_rows in groups.items():
                    x = [row[feature] for row in group_rows]
                    y = [row["residual"] for row in group_rows]
                    abs_x = [abs(value) for value in x]
                    item = {
                        "spearman": spearman(x, y),
                        "spearman_absolute_feature": spearman(abs_x, y),
                        "feature_distribution": summarize(abs_x),
                        "residual_distribution": summarize(y),
                        "tail_enrichment_top_1pct": top_enrichment(group_rows, feature, 0.01),
                        "tail_enrichment_top_5pct": top_enrichment(group_rows, feature, 0.05),
                    }
                    result["features"][feature][group_name] = item
                    for row in group_rows:
                        outputs.append({
                            "motion_amplitude": amplitude,
                            "width_sign": sign,
                            "aggregation": group_name,
                            "pair_index": row["pair_index"],
                            "source_point_index": row["source_point_index"],
                            "source_region": row["source_region"],
                            "any_face_changed": row["any_face_changed"],
                            "feature": feature,
                            "feature_value": row[feature],
                            "residual": row["residual"],
                        })
            summaries[f"{amplitude:.2f}"][sign] = result

    with OUTPUT.open("w", newline="", encoding="utf-8") as stream:
        fields = list(outputs[0])
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(outputs)
    manifest = {
        "schema": "fog-shadow-width-geometry-association-rd-v1",
        "purpose": "Descriptive rank association and upper-tail overlap between continuous AABB interval motion features and existing width-factor temporal residuals.",
        "input_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "amplitudes": AMPLITUDES,
        "width_signs": SIGNS,
        "input_pair_rows_per_amplitude_sign": 2508,
        "candidate_steps": 128,
        "reference_steps": 4096,
        "validation_steps": 2048,
        "correlation": "Spearman rho using average ranks for ties; exact floating-point ties; no p-values or causal interpretation.",
        "point_aggregation": "For each source point, retain the pair row with the largest residual within amplitude/sign; its associated geometry is retained.",
        "top_enrichment": "Top residual fraction defined by empirical linearly interpolated quantile; feature tail is absolute feature >= empirical p95; ties can increase selected fraction.",
        "summary": summaries,
        "output_csv": {"path": OUTPUT.as_posix(),
                       "rows": len(outputs),
                       "sha256": hashlib.sha256(OUTPUT.read_bytes()).hexdigest()},
        "limitations": [
            "Same synthetic AABB camera sequence and source-frame selected points; no independent scene replication.",
            "Pair rows repeat world points and adjacent temporal observations; unique-point max is a sensitivity aggregation, not a sampling confidence interval.",
            "Associations are descriptive and do not establish geometric causation, perceptual flicker, product quality, GPU behavior, or runtime.",
            "Alpha/background, display transform, image-space visual review, and product host path remain unvalidated.",
        ],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("input rows", len(rows), "output rows", len(outputs))
    for amplitude in AMPLITUDES:
        sign_result = summaries[f"{amplitude:.2f}"]["plus_2pct"]
        width = sign_result["features"]["absolute_normalized_width_delta"]
        exit_delta = sign_result["features"]["absolute_normalized_exit_delta"]
        print("amplitude", amplitude,
              "width rho pair/point", width["pair"]["spearman_absolute_feature"],
              width["unique_point_max"]["spearman_absolute_feature"],
              "exit rho pair/point", exit_delta["pair"]["spearman_absolute_feature"],
              exit_delta["unique_point_max"]["spearman_absolute_feature"],
              "width top5 capture", width["pair"]["tail_enrichment_top_5pct"]["residual_top_capture_rate"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
