#!/usr/bin/env python3
"""Point-cluster bootstrap for width geometry/residual rank associations."""

from __future__ import annotations

import csv
import hashlib
import json
import math
import random
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "quadrature_width_geometry_sensitivity.csv"
OUTPUT = ROOT / "quadrature_width_geometry_cluster_bootstrap.csv"
MANIFEST = ROOT / "quadrature_width_geometry_cluster_bootstrap_manifest.json"
AMPLITUDES = (0.5, 1.0, 2.0)
SIGNS = ("minus_2pct", "plus_2pct")
FEATURES = ("absolute_exit_delta", "absolute_width_delta")
BOOTSTRAP_REPLICATES = 2000
SEED = 20261112


def average_ranks(values, weights):
    order = sorted(range(len(values)), key=lambda index: (values[index], index))
    ranks = [0.0] * len(values)
    start = 0
    cumulative_weight = 0
    while start < len(order):
        end = start + 1
        while end < len(order) and values[order[end]] == values[order[start]]:
            end += 1
        tie_weight = sum(weights[order[position]] for position in range(start, end))
        rank = cumulative_weight + (tie_weight + 1) / 2.0
        for position in range(start, end):
            ranks[order[position]] = rank
        cumulative_weight += tie_weight
        start = end
    return ranks


def weighted_pearson(x, y, weights):
    total = sum(weights)
    if total < 2:
        return None
    mean_x = sum(value * weight for value, weight in zip(x, weights)) / total
    mean_y = sum(value * weight for value, weight in zip(y, weights)) / total
    numerator = sum(weight * (vx - mean_x) * (vy - mean_y)
                    for vx, vy, weight in zip(x, y, weights))
    sum_x = sum(weight * (vx - mean_x) ** 2 for vx, weight in zip(x, weights))
    sum_y = sum(weight * (vy - mean_y) ** 2 for vy, weight in zip(y, weights))
    denominator = math.sqrt(sum_x * sum_y)
    return numerator / denominator if denominator else None


def weighted_spearman(x, y, weights):
    return weighted_pearson(average_ranks(x, weights), average_ranks(y, weights), weights)


def percentile(sorted_values, fraction):
    if not sorted_values:
        return None
    rank = max(0, min(len(sorted_values) - 1, math.ceil(fraction * len(sorted_values)) - 1))
    return sorted_values[rank]


def stats(values):
    defined = sorted(value for value in values if value is not None)
    return {
        "defined_replicates": len(defined),
        "undefined_replicates": len(values) - len(defined),
        "median": percentile(defined, 0.5),
        "ci95_low": percentile(defined, 0.025),
        "ci95_high": percentile(defined, 0.975),
    }


def main():
    grouped = defaultdict(lambda: defaultdict(list))
    with INPUT.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            key = (float(row["motion_amplitude"]), row["width_sign"])
            point = int(row["source_point_index"])
            item = {
                "point": point,
                "pair": int(row["pair_index"]),
                "residual": float(row["combined_candidate_shift_max_channel"]),
                "absolute_exit_delta": abs(float(row["signed_normalized_exit_delta"])),
                "absolute_width_delta": abs(float(row["signed_normalized_width_delta"])),
            }
            grouped[key][point].append(item)

    rng = random.Random(SEED)
    results = {}
    output_rows = []
    for amplitude in AMPLITUDES:
        results[f"{amplitude:.2f}"] = {}
        for sign in SIGNS:
            point_rows = grouped[(amplitude, sign)]
            points = sorted(point_rows)
            if len(points) != 440:
                raise ValueError(f"Expected 440 source points, got {len(points)} for {amplitude}/{sign}")
            pair_values = {feature: [] for feature in FEATURES}
            point_max_values = {feature: [] for feature in FEATURES}
            for _ in range(BOOTSTRAP_REPLICATES):
                multiplicity = Counter(rng.choices(points, k=len(points)))
                sampled_points = [point for point, count in multiplicity.items() if count]

                # Pair-row estimand: preserve every neighboring-pair observation
                # for each sampled cluster, weighted by its bootstrap multiplicity.
                flat_rows = []
                flat_weights = []
                for point, count in multiplicity.items():
                    for item in point_rows[point]:
                        flat_rows.append(item)
                        flat_weights.append(count)
                residual = [item["residual"] for item in flat_rows]
                for feature in FEATURES:
                    geom = [item[feature] for item in flat_rows]
                    pair_values[feature].append(weighted_spearman(geom, residual, flat_weights))

                # Point-max estimand: within each sampled source point select its
                # largest residual pair; bootstrap multiplicity supplies point weight.
                chosen = []
                chosen_weights = []
                for point in sampled_points:
                    item = max(point_rows[point], key=lambda value: value["residual"])
                    chosen.append(item)
                    chosen_weights.append(multiplicity[point])
                chosen_residual = [item["residual"] for item in chosen]
                for feature in FEATURES:
                    geom = [item[feature] for item in chosen]
                    point_max_values[feature].append(
                        weighted_spearman(geom, chosen_residual, chosen_weights))

            results[f"{amplitude:.2f}"][sign] = {}
            for feature in FEATURES:
                key_name = f"absolute_normalized_{feature.removeprefix('absolute_')}"
                record = {
                    "pair_row_weighted": stats(pair_values[feature]),
                    "unique_point_max": stats(point_max_values[feature]),
                }
                results[f"{amplitude:.2f}"][sign][key_name] = record
                output_rows.append({
                    "motion_amplitude": amplitude,
                    "width_sign": sign,
                    "feature": key_name,
                    "bootstrap_replicates": BOOTSTRAP_REPLICATES,
                    "seed": SEED,
                    "pair_median": record["pair_row_weighted"]["median"],
                    "pair_ci95_low": record["pair_row_weighted"]["ci95_low"],
                    "pair_ci95_high": record["pair_row_weighted"]["ci95_high"],
                    "point_max_median": record["unique_point_max"]["median"],
                    "point_max_ci95_low": record["unique_point_max"]["ci95_low"],
                    "point_max_ci95_high": record["unique_point_max"]["ci95_high"],
                    "undefined_pair_replicates": record["pair_row_weighted"]["undefined_replicates"],
                    "undefined_point_max_replicates": record["unique_point_max"]["undefined_replicates"],
                })

    with OUTPUT.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(output_rows[0]))
        writer.writeheader()
        writer.writerows(output_rows)
    manifest = {
        "schema": "fog-shadow-width-geometry-cluster-bootstrap-rd-v1",
        "input_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "amplitudes": AMPLITUDES,
        "width_signs": SIGNS,
        "source_point_clusters": 440,
        "replicates": BOOTSTRAP_REPLICATES,
        "seed": SEED,
        "interval": "95% percentile interval over cluster bootstrap replicates; not a product acceptance interval.",
        "estimands": {
            "pair_row_weighted": "Resample source point clusters with replacement; retain all pair rows in each sampled cluster, weighted by cluster multiplicity; compute Spearman correlation over expanded weighted observations.",
            "unique_point_max": "Within each source point choose its max-residual pair row, then compute weighted Spearman across resampled source points using bootstrap multiplicity.",
        },
        "summary": results,
        "output_csv": {"path": OUTPUT.as_posix(), "rows": len(output_rows),
                       "sha256": hashlib.sha256(OUTPUT.read_bytes()).hexdigest()},
        "limitations": [
            "Bootstrap resamples points only within one fixed synthetic scene and selected 440-point cohort.",
            "Adjacent pairs within a point are retained as a cluster; spatial dependence between distinct points remains unmodeled.",
            "The percentile intervals quantify point-sampling stability under this fixture, not scene-to-scene or perceptual uncertainty.",
            "No causal inference, visual quality gate, GPU/runtime or host integration result is implied.",
        ],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    for amplitude in AMPLITUDES:
        for sign in SIGNS:
            values = results[f"{amplitude:.2f}"][sign]
            print(amplitude, sign,
                  "exit pair", values["absolute_normalized_exit_delta"]["pair_row_weighted"],
                  "exit point", values["absolute_normalized_exit_delta"]["unique_point_max"],
                  "width pair", values["absolute_normalized_width_delta"]["pair_row_weighted"],
                  "width point", values["absolute_normalized_width_delta"]["unique_point_max"])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
