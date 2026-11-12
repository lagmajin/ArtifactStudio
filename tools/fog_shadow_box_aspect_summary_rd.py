#!/usr/bin/env python3
"""Rebuild box aspect ratio summaries from the durable temporal raw CSV."""

from __future__ import annotations

import csv
import hashlib
import json
from collections import defaultdict
from pathlib import Path

import fog_shadow_box_aspect_width_rd as experiment


ROOT = Path("docs/worklog/artifacts/fog-shadow-box-aspect-rd-2026-11-12")
RAW = ROOT / "box_aspect_width_temporal.csv"
MANIFEST = ROOT / "box_aspect_width_manifest.json"


def main():
    rows = []
    support = defaultdict(lambda: defaultdict(set))
    by_group = defaultdict(list)
    validation_max = 0.0
    with RAW.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            item = {
                "shape": row["shape"],
                "motion_amplitude": float(row["motion_amplitude"]),
                "pair_index": int(row["pair_index"]),
                "source_point_index": int(row["source_point_index"]),
                "entry_face_changed": row["entry_face_changed"].lower() == "true",
                "exit_face_changed": row["exit_face_changed"].lower() == "true",
                "signed_exit_delta": float(row["signed_exit_delta"]),
                "signed_width_delta": float(row["signed_width_delta"]),
                "temporal_residual_max_channel": float(row["temporal_residual_max_channel"]),
                "previous_reference_validation_relative_rgb_l2": float(
                    row["previous_reference_validation_relative_rgb_l2"]),
                "current_reference_validation_relative_rgb_l2": float(
                    row["current_reference_validation_relative_rgb_l2"]),
            }
            rows.append(item)
            key = (item["shape"], f"{item['motion_amplitude']:.2f}")
            by_group[key].append(item)
            support[item["shape"]][f"{item['motion_amplitude']:.2f}"].add(
                (item["pair_index"], item["source_point_index"]))
            validation_max = max(validation_max,
                                 item["previous_reference_validation_relative_rgb_l2"],
                                 item["current_reference_validation_relative_rgb_l2"])

    scenario_summaries = {shape: {} for shape in experiment.SHAPES}
    for shape in experiment.SHAPES:
        for amplitude in experiment.AMPLITUDES:
            key = f"{amplitude:.2f}"
            cohort = by_group[(shape, key)]
            scenario_summaries[shape][key] = {
                "valid_pair_rows": len(cohort),
                "unique_points": len({row["source_point_index"] for row in cohort}),
                "face_switch_pair_rows": sum(row["entry_face_changed"] or row["exit_face_changed"]
                                              for row in cohort),
                "metrics": experiment.summarize(cohort),
            }

    common_support = {}
    for amplitude in experiment.AMPLITUDES:
        key = f"{amplitude:.2f}"
        common = set.intersection(*(support[shape][key] for shape in experiment.SHAPES))
        common_support[key] = {
            "pair_point_rows": len(common),
            "unique_points": len({point for _pair, point in common}),
            "fraction_of_baseline_pairs": len(common) / max(len(support["baseline"][key]), 1),
        }
        for shape in experiment.SHAPES:
            cohort = [row for row in by_group[(shape, key)]
                      if (row["pair_index"], row["source_point_index"]) in common]
            scenario_summaries[shape][key]["common_support_pair_rows"] = len(cohort)
            scenario_summaries[shape][key]["common_support_metrics"] = experiment.summarize(cohort)

    input_paths = [Path(experiment.__file__), Path(experiment.scene.__file__),
                   Path(experiment.sequence.__file__), Path(experiment.combined.__file__),
                   Path(experiment.soft.__file__), Path(experiment.scattering.__file__)]
    manifest = {
        "schema": "fog-shadow-volume-preserving-box-aspect-rd-v1",
        "purpose": "Compare temporal sensitivity to a +2% estimated core width over volume-preserving AABB aspect-ratio scenarios.",
        "source_point_count": 440,
        "source_selection": "Baseline-shape center-frame pixel-center rays intersecting the original AABB, world points held fixed across shape scenarios.",
        "box_center": (0.0, 3.5, 8.25),
        "shapes_half_extents": experiment.SHAPES,
        "box_volume_each": {name: 8.0 * __import__("math").prod(extents)
                            for name, extents in experiment.SHAPES.items()},
        "camera_amplitudes": experiment.AMPLITUDES,
        "camera_x_world_units": {f"{amp:.2f}": [x * amp for x in experiment.sequence.CAMERA_X]
                                 for amp in experiment.AMPLITUDES},
        "candidate": {"center_offset": 0.0, "core_width_error": experiment.WIDTH_ERROR,
                      "steps": experiment.CANDIDATE_STEPS},
        "reference_steps": experiment.REFERENCE_STEPS,
        "validation_steps": experiment.VALIDATION_STEPS,
        "maximum_reference_validation_relative_rgb_l2": validation_max,
        "penumbra_ray_length_fraction": experiment.PENUMBRA,
        "common_support": common_support,
        "scenario_summaries": scenario_summaries,
        "raw_csv": {"path": RAW.as_posix(), "rows": len(rows),
                    "sha256": hashlib.sha256(RAW.read_bytes()).hexdigest()},
        "input_scripts": [{"path": path.as_posix(),
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                          for path in input_paths],
        "limitations": [
            "All shapes share a source-point cohort selected using the baseline AABB; changed silhouettes and lost/gained support affect inclusion.",
            "The common support is an intersection of visible source point/pair keys and may exclude shape-specific difficult regions.",
            "This is a single synthetic scene, single sun configuration, direct-ray visibility AABB, CPU single-scattering fixture.",
            "No visual/perceptual judgment, real shadow filtering, alpha/color integration, GPU/backend parity, or runtime result is implied.",
        ],
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("rows", len(rows), "validation max", validation_max)
    print("common support", common_support)
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
