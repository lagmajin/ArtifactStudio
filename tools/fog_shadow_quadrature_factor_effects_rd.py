#!/usr/bin/env python3
"""Post-hoc vector-space contrasts for the registered quadrature factorial sweep."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
import math
from pathlib import Path


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "quadrature_factor_temporal.csv"
RAW = ROOT / "quadrature_factor_effects.csv"
MANIFEST = ROOT / "quadrature_factor_effects_manifest.json"
SIGNS = ("minus_2pct", "plus_2pct")
EFFECTS = ("segmentation_main_effect", "importance_weight_main_effect",
           "segmentation_weight_interaction", "combined_candidate_shift")


def vec_sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def vec_add(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def summarize(rows):
    if not rows:
        return {"pair_count": 0}
    magnitudes = sorted(max(abs(v) for v in row["rgb_vector"]) for row in rows)
    squared = sum(sum(value * value for value in row["rgb_vector"]) for row in rows)
    count = len(rows)
    return {
        "pair_count": count,
        "rgb_vector_rmse": math.sqrt(squared / (3 * count)),
        "mean_max_channel_absolute": sum(magnitudes) / count,
        "p95_max_channel_absolute": magnitudes[math.ceil(0.95 * count) - 1],
        "max_channel_absolute": magnitudes[-1],
    }


def main():
    with INPUT.open(newline="", encoding="utf-8") as stream:
        source = list(csv.DictReader(stream))
    keyed = {}
    for row in source:
        key = (float(row["motion_amplitude"]), int(row["pair_index"]),
               int(row["source_point_index"]))
        item = keyed.setdefault(key, {"source_pixel": ast.literal_eval(row["source_pixel"]),
                                      "world_point": ast.literal_eval(row["world_point"]),
                                      "source_region": row["source_region"], "vectors": {}})
        item["vectors"][row["method"]] = ast.literal_eval(row["temporal_residual_delta_rgb"])

    rows = []
    for (amplitude, pair_index, point_index), item in keyed.items():
        r00 = item["vectors"]["true_segmentation_true_weights"]
        for sign in SIGNS:
            r01 = item["vectors"][f"true_segmentation_estimated_weights_center_{sign}"]
            r10 = item["vectors"][f"estimated_segmentation_true_weights_center_{sign}"]
            r11 = item["vectors"][f"estimated_segmentation_estimated_weights_center_{sign}"]
            effects = {
                "segmentation_main_effect": vec_sub(r10, r00),
                "importance_weight_main_effect": vec_sub(r01, r00),
                "segmentation_weight_interaction": tuple(
                    r11[i] - r10[i] - r01[i] + r00[i] for i in range(3)),
                "combined_candidate_shift": vec_sub(r11, r00),
            }
            for name, vector in effects.items():
                rows.append({
                    "motion_amplitude": amplitude, "pair_index": pair_index,
                    "source_point_index": point_index, "source_pixel": item["source_pixel"],
                    "world_point": item["world_point"], "source_region": item["source_region"],
                    "center_sign": sign, "effect": name, "rgb_vector": vector,
                    "max_channel_absolute": max(abs(value) for value in vector),
                })

    with RAW.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {}
    for amplitude in (0.5, 1.0, 2.0):
        summary[f"{amplitude:.2f}"] = {}
        for sign in SIGNS:
            summary[f"{amplitude:.2f}"][sign] = {}
            for effect in EFFECTS:
                selected = [row for row in rows if row["motion_amplitude"] == amplitude
                            and row["center_sign"] == sign and row["effect"] == effect]
                summary[f"{amplitude:.2f}"][sign][effect] = {
                    "all": summarize(selected),
                    "by_region": {region: summarize([row for row in selected
                                                     if row["source_region"] == region])
                                  for region in ("silhouette_edge", "shadow_interior")},
                }

    manifest = {
        "schema": "fog-shadow-quadrature-factor-effects-rd-v1",
        "purpose": "Algebraically decompose temporal RGB residual vectors from the registered 2x2 profile-segmentation/weight-allocation factorial.",
        "input_manifest_sha256": hashlib.sha256((ROOT / "quadrature_factor_manifest.json").read_bytes()).hexdigest(),
        "input_csv_sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest(),
        "effect_definitions": {
            "segmentation_main_effect": "r10 - r00",
            "importance_weight_main_effect": "r01 - r00",
            "segmentation_weight_interaction": "r11 - r10 - r01 + r00",
            "combined_candidate_shift": "r11 - r00",
            "corner_codes": {"r00": "true segmentation, true weights",
                             "r01": "true segmentation, estimated weights",
                             "r10": "estimated segmentation, true weights",
                             "r11": "estimated segmentation, estimated weights"},
        },
        "summary": summary, "raw_result_count": len(rows),
        "raw_csv": {"path": RAW.as_posix(),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "limitations": [
            "This is an algebraic contrast of numerical residual vectors; component magnitudes are not independent causal contributions because the integrator is nonlinear.",
            "The interaction term captures non-additivity across segmentation and weight-allocation corners.",
            "Values are linear RGB temporal residual radiance in the synthetic AABB CPU fixture, with no perceptual acceptance threshold.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("factor effect rows", len(rows))
    for amplitude, signs in summary.items():
        print("amplitude", amplitude)
        for sign in SIGNS:
            print(sign, {effect: values["all"] for effect, values in signs[sign].items()})
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
