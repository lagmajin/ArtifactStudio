#!/usr/bin/env python3
"""Rebuild signed factor summaries and common support from durable raw rows."""

from __future__ import annotations

import ast
import csv
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path

import fog_shadow_box_aspect_factor_sign_rd as sign_analysis


ROOT = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
RAW = ROOT / "box_aspect_factor_sign_temporal.csv"
MANIFEST = ROOT / "box_aspect_factor_sign_manifest.json"
SIGNS = ("minus_2pct", "plus_2pct")


def read_rows():
    grouped = {}
    with RAW.open(newline="", encoding="utf-8") as stream:
        for raw in csv.DictReader(stream):
            key = (raw["shape"], float(raw["motion_amplitude"]), int(raw["pair_index"]),
                   int(raw["source_point_index"]))
            row = {name: ast.literal_eval(raw[name])
                   for name in (*sign_analysis.FACTOR_NAMES, *sign_analysis.CONTRAST_NAMES)}
            row.update({
                "shape": key[0], "motion_amplitude": key[1], "pair_index": key[2],
                "source_point_index": key[3], "width_sign": raw["width_sign"],
                "width_delta_quartile": raw["width_delta_quartile"],
                "common_width_delta_quartile": raw["common_width_delta_quartile"],
                "any_face_changed": raw["any_face_changed"].lower() == "true",
            })
            grouped.setdefault(key, {})[row["width_sign"]] = row
    return grouped


def sign_terms(rows_by_key, keys):
    result = {}
    terms = (*sign_analysis.FACTOR_NAMES, *sign_analysis.CONTRAST_NAMES)
    for term in terms:
        plus = [rows_by_key[key]["plus_2pct"][term] for key in keys]
        minus = [rows_by_key[key]["minus_2pct"][term] for key in keys]
        result[term] = {
            "plus_minus_delta": sign_analysis.vector_stats(
                [tuple(a[i] - b[i] for i in range(3)) for a, b in zip(plus, minus)]),
            "even_component": sign_analysis.vector_stats(
                [tuple((a[i] + b[i]) * 0.5 for i in range(3)) for a, b in zip(plus, minus)]),
            "odd_component": sign_analysis.vector_stats(
                [tuple((a[i] - b[i]) * 0.5 for i in range(3)) for a, b in zip(plus, minus)]),
        }
    return result


def main():
    grouped = read_rows()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    common_pair_points = {}
    for amplitude in (0.5, 1.0, 2.0):
        supports = []
        for shape in sign_analysis.aspect.SHAPES:
            supports.append({(key[2], key[3]) for key in grouped
                             if key[0] == shape and key[1] == amplitude})
        common_pair_points[amplitude] = set.intersection(*supports)

    scenario_summary = {shape: {} for shape in sign_analysis.aspect.SHAPES}
    symmetry_summary = {shape: {} for shape in sign_analysis.aspect.SHAPES}
    common_summary = {}
    for amplitude in (0.5, 1.0, 2.0):
        amp_key = f"{amplitude:.2f}"
        common = common_pair_points[amplitude]
        common_summary[amp_key] = {
            "pair_point_rows": len(common),
            "unique_points": len({point for _pair, point in common}),
        }
        for shape in sign_analysis.aspect.SHAPES:
            keys = [key for key in grouped if key[0] == shape and key[1] == amplitude]
            scenario_summary[shape][amp_key] = {}
            for sign in SIGNS:
                rows = [grouped[key][sign] for key in keys]
                common_rows = [grouped[key][sign] for key in keys
                               if (key[2], key[3]) in common]
                scenario_summary[shape][amp_key][sign] = {
                    "all_support": sign_analysis.summary(rows),
                    "by_width_delta_quartile": {
                        label: sign_analysis.summary([row for row in rows
                                                      if row["width_delta_quartile"] == label])
                        for label in ("Q1", "Q2", "Q3", "Q4")
                    },
                    "common_support": sign_analysis.summary(common_rows),
                    "common_support_by_width_delta_quartile": {
                        label: sign_analysis.summary([row for row in common_rows
                                                      if row["common_width_delta_quartile"] == label])
                        for label in ("Q1", "Q2", "Q3", "Q4")
                    },
                }
            all_keys = keys
            common_keys = [key for key in keys if (key[2], key[3]) in common]
            symmetry_summary[shape][amp_key] = {
                "all_support": sign_terms(grouped, all_keys),
                "common_support": sign_terms(grouped, common_keys),
                "common_support_by_width_delta_quartile": {
                    label: sign_terms(grouped, [key for key in common_keys
                                                if grouped[key]["plus_2pct"][
                                                    "common_width_delta_quartile"] == label])
                    for label in ("Q1", "Q2", "Q3", "Q4")
                },
            }

    max_true_true_delta = max(
        (abs(group["plus_2pct"]["true_true"][channel]
             - group["minus_2pct"]["true_true"][channel])
         for group in grouped.values() for channel in range(3)), default=0.0)
    identity_error = max(
        (abs(row[term][channel] - row["segmentation_effect"][channel]
             - row["weight_effect"][channel] - row["interaction_effect"][channel])
         for group in grouped.values() for row in group.values()
         for term in ("full_effect",) for channel in range(3)), default=0.0)

    manifest["common_support"] = common_summary
    manifest["scenario_summary_by_sign"] = scenario_summary
    manifest["sign_symmetry_summary"] = symmetry_summary
    manifest["plus_minus_true_true_max_rgb_component_difference"] = max_true_true_delta
    manifest["factor_identity_max_rgb_component_error"] = identity_error
    manifest["raw_csv"] = {"path": RAW.as_posix(), "rows": sum(len(group) for group in grouped.values()),
                           "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()}
    manifest["raw_generation_script"] = manifest.get("analysis_script")
    manifest["analysis_script"] = {
        "path": sign_analysis.Path(sign_analysis.__file__).as_posix(),
        "sha256": hashlib.sha256(sign_analysis.Path(sign_analysis.__file__).read_bytes()).hexdigest(),
    }
    manifest["posthoc_summary_script"] = {
        "path": Path(__file__).as_posix(),
        "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("paired keys", len(grouped), "common support", common_summary,
          "true/true sign delta", max_true_true_delta, "factor identity", identity_error)
    for shape in sign_analysis.aspect.SHAPES:
        for amplitude in (0.5, 1.0, 2.0):
            key = f"{amplitude:.2f}"
            plus = scenario_summary[shape][key]["plus_2pct"]["all_support"]["full_effect"]
            minus = scenario_summary[shape][key]["minus_2pct"]["all_support"]["full_effect"]
            even = symmetry_summary[shape][key]["all_support"]["full_effect"]["even_component"]
            odd = symmetry_summary[shape][key]["all_support"]["full_effect"]["odd_component"]
            print(shape, amplitude,
                  "plus/minus RMSE", plus["rgb_rmse"], minus["rgb_rmse"],
                  "even/odd RMSE", even["rgb_rmse"], odd["rgb_rmse"])


if __name__ == "__main__":
    main()
