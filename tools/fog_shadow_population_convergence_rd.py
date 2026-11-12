#!/usr/bin/env python3
"""Sweep interval-estimation errors over every visible shadow-volume ray."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_shadow_temporal_outlier_rd as outlier
import fog_sun_transmittance_rd as scattering


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
RAW_CSV = OUTPUT_DIR / "population_convergence_sweep.csv"
MANIFEST = OUTPUT_DIR / "population_convergence_manifest.json"
CENTER_OFFSETS = (-0.02, -0.005, 0.0, 0.005, 0.02)
WIDTH_ERRORS = (-0.02, -0.005, 0.0, 0.005, 0.02)
STEP_COUNTS = (32, 128, 512, 2048)
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
GATE = soft.GATE


def rel_l2(candidate, reference):
    squared = sum((candidate[c] - reference[c]) ** 2 for c in range(3))
    norm = sum(value * value for value in reference)
    return math.sqrt(squared / max(norm, 1e-30))


def quantile(values, q):
    return sorted(values)[max(0, math.ceil(q * len(values)) - 1)]


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    sun = outlier.sun_direction()
    population = []

    for frame_index, camera_x in enumerate(sequence.CAMERA_X):
        origin, right, up, forward = sequence.camera_frame(camera_x)
        intervals_by_pixel = []
        frame_rays = []
        for y in range(sequence.HEIGHT):
            for x in range(sequence.WIDTH):
                ray = scene.ray_for_pixel(x, y, right, up, forward)
                _color, distance, surface = scene.scene_sample(origin, ray)
                truth = sequence.ray_box_interval(origin, ray, distance) or ()
                intervals_by_pixel.append(truth)
                if not truth:
                    continue
                case = {"camera_y": origin[1], "view_direction": ray,
                        "distance": distance, "sun_direction": sun}
                frame_rays.append({"frame": frame_index, "camera_x": camera_x,
                                   "x": x, "y": y, "surface": surface,
                                   "region": "silhouette_edge", "case": case,
                                   "true_intervals": truth})
        _inside, edge_indices = sequence.boundary_mask(intervals_by_pixel)
        edge_set = set(edge_indices)
        for item in frame_rays:
            item["region"] = "silhouette_edge" if item["y"] * sequence.WIDTH + item["x"] in edge_set else "shadow_interior"
        population.extend(frame_rays)

    # Deterministic stratification: retain up to three spatially distributed rays
    # from each frame × region × nearest-surface stratum.
    strata = {}
    for ray_index, item in enumerate(population):
        strata.setdefault((item["frame"], item["region"], item["surface"]), []).append(ray_index)
    subset = set()
    for indices in strata.values():
        indices.sort(key=lambda i: (population[i]["y"], population[i]["x"]))
        for position in sorted(set((0, len(indices) // 2, len(indices) - 1))):
            subset.add(indices[position])

    rows = []
    ray_inventory = []
    validation_max = 0.0
    for ray_index, item in enumerate(population):
        case, truth = item["case"], item["true_intervals"]
        reference = soft.integrate_cells(
            case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH, REFERENCE_STEPS),
            truth, sequence.PENUMBRA_WIDTH)
        validation = soft.integrate_cells(
            case, soft.reference_cells(case, truth, sequence.PENUMBRA_WIDTH, VALIDATION_STEPS),
            truth, sequence.PENUMBRA_WIDTH)
        validation_error = rel_l2(validation, reference)
        validation_max = max(validation_max, validation_error)
        ray_id = f"f{item['frame']:02d}_x{item['x']:02d}_y{item['y']:02d}"
        ray_inventory.append({"ray_id": ray_id, "frame": item["frame"],
                              "camera_x": item["camera_x"], "pixel": [item["x"], item["y"]],
                              "surface": item["surface"], "region": item["region"],
                              "true_intervals": truth,
                              "reference_validation_relative_rgb_l2": validation_error})
        for center in CENTER_OFFSETS:
            for width in WIDTH_ERRORS:
                estimated = combined.estimate_intervals(truth, center, width)
                error_steps = STEP_COUNTS if ray_index in subset else (128,)
                for steps in error_steps:
                    cells = soft.weighted_cells(case["camera_y"], case["view_direction"][1],
                                                case["distance"], steps, estimated,
                                                sequence.PENUMBRA_WIDTH)
                    value = soft.integrate_cells(case, cells, truth, sequence.PENUMBRA_WIDTH)
                    rows.append({"ray_id": ray_id, "frame": item["frame"],
                                 "camera_x": item["camera_x"], "pixel_x": item["x"],
                                 "pixel_y": item["y"], "surface": item["surface"],
                                 "region": item["region"], "steps": steps,
                                 "center_offset_fraction_of_ray": center,
                                 "core_relative_width_error": width,
                                 "true_intervals": truth, "estimated_intervals": estimated,
                                 "reference_rgb": reference, "candidate_rgb": value,
                                 "relative_rgb_l2_error": rel_l2(value, reference),
                                 "reference_validation_relative_rgb_l2": validation_error,
                                 "exact_interval_baseline": False,
                                 "interaction_subset": ray_index in subset})
        for steps in STEP_COUNTS:
            cells = soft.weighted_cells(case["camera_y"], case["view_direction"][1],
                                        case["distance"], steps, truth,
                                        sequence.PENUMBRA_WIDTH)
            value = soft.integrate_cells(case, cells, truth, sequence.PENUMBRA_WIDTH)
            rows.append({"ray_id": ray_id, "frame": item["frame"],
                         "camera_x": item["camera_x"], "pixel_x": item["x"],
                         "pixel_y": item["y"], "surface": item["surface"],
                         "region": item["region"], "steps": steps,
                         "center_offset_fraction_of_ray": 0.0,
                         "core_relative_width_error": 0.0,
                         "true_intervals": truth, "estimated_intervals": truth,
                         "reference_rgb": reference, "candidate_rgb": value,
                         "relative_rgb_l2_error": rel_l2(value, reference),
                         "reference_validation_relative_rgb_l2": validation_error,
                         "exact_interval_baseline": True})

    with RAW_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    summary = {}
    for region in ("all_visible_shadow_rays", "silhouette_edge", "shadow_interior",
                   "interaction_subset"):
        summary[region] = {}
        for center in CENTER_OFFSETS:
            for width in WIDTH_ERRORS:
                for steps in STEP_COUNTS:
                    selected = [r["relative_rgb_l2_error"] for r in rows
                                if r["steps"] == steps
                                and r.get("exact_interval_baseline") is not True
                                and math.isclose(r["center_offset_fraction_of_ray"], center)
                                and math.isclose(r["core_relative_width_error"], width)
                                and (region == "all_visible_shadow_rays"
                                     or region == "interaction_subset" and r.get("interaction_subset") is True
                                     or r["region"] == region)]
                    if not selected:
                        continue
                    summary[region][f"center={center:+.3%},width={width:+.3%},steps={steps}"] = {
                        "ray_count": len(selected), "p95_relative_rgb_l2_error": quantile(selected, 0.95),
                        "max_relative_rgb_l2_error": max(selected),
                        "gate_exceedance_count": sum(value > GATE for value in selected),
                        "passes_max_gate": max(selected) <= GATE,
                    }
        summary[region]["exact_interval_baseline"] = {}
        for steps in STEP_COUNTS:
            selected = [r["relative_rgb_l2_error"] for r in rows
                        if r["steps"] == steps and r.get("exact_interval_baseline") is True
                        and (region == "all_visible_shadow_rays"
                             or region == "interaction_subset" and r.get("interaction_subset") is True
                             or r["region"] == region)]
            if not selected:
                continue
            summary[region]["exact_interval_baseline"][str(steps)] = {
                "ray_count": len(selected), "p95_relative_rgb_l2_error": quantile(selected, 0.95),
                "max_relative_rgb_l2_error": max(selected),
                "gate_exceedance_count": sum(value > GATE for value in selected),
                "passes_max_gate": max(selected) <= GATE,
            }

    frame_counts = {str(frame): sum(ray["frame"] == frame for ray in population)
                    for frame in range(len(sequence.CAMERA_X))}
    per_frame = {}
    for frame in range(len(sequence.CAMERA_X)):
        frame_rows = [r for r in rows if r["frame"] == frame]
        per_frame[str(frame)] = {"camera_x": sequence.CAMERA_X[frame],
                                 "shadow_ray_count": frame_counts[str(frame)],
                                 "exact_interval_baseline": {},
                                 "all_population_128_center_width_grid": {}}
        for steps in STEP_COUNTS:
            values = [r["relative_rgb_l2_error"] for r in frame_rows
                      if r["steps"] == steps and r.get("exact_interval_baseline") is True]
            per_frame[str(frame)]["exact_interval_baseline"][str(steps)] = {
                "p95_relative_rgb_l2_error": quantile(values, 0.95),
                "max_relative_rgb_l2_error": max(values),
                "gate_exceedance_count": sum(value > GATE for value in values)}
        for center in CENTER_OFFSETS:
            for width in WIDTH_ERRORS:
                values = [r["relative_rgb_l2_error"] for r in frame_rows
                          if r["steps"] == 128 and r.get("exact_interval_baseline") is False
                          and r.get("interaction_subset") is False
                          and math.isclose(r["center_offset_fraction_of_ray"], center)
                          and math.isclose(r["core_relative_width_error"], width)]
                per_frame[str(frame)]["all_population_128_center_width_grid"][
                    f"center={center:+.3%},width={width:+.3%}"] = {
                        "p95_relative_rgb_l2_error": quantile(values, 0.95),
                        "max_relative_rgb_l2_error": max(values),
                        "gate_exceedance_count": sum(value > GATE for value in values)}

    manifest = {
        "schema": "fog-shadow-visible-population-convergence-rd-v1",
        "purpose": "Measure quadrature and interval-estimation errors over all pixel-center rays intersecting the visible synthetic shadow volume.",
        "execution_scope": "Direct continuous-ray CPU integral for every visible AABB-shadowed ray in the deterministic 7-frame 64x36 sequence; no image resampling.",
        "sequence_script_sha256": hashlib.sha256(Path("tools/fog_shadow_camera_sequence_rd.py").read_bytes()).hexdigest(),
        "population_ray_count": len(population), "frame_ray_counts": frame_counts,
        "interaction_subset_ray_count": len(subset),
        "interaction_subset_rule": "Within each frame × silhouette-edge/interior × nearest-surface stratum, choose first, middle, and last rays after sorting by pixel y then x.",
        "region_ray_counts": {name: sum(ray["region"] == name for ray in population)
                               for name in ("silhouette_edge", "shadow_interior")},
        "center_offsets_fraction_of_ray": CENTER_OFFSETS, "core_relative_width_errors": WIDTH_ERRORS,
        "candidate_step_counts": STEP_COUNTS, "reference_steps": REFERENCE_STEPS,
        "validation_steps": VALIDATION_STEPS, "reference_validation_max_relative_rgb_l2": validation_max,
        "pre_registered_gate_max_relative_rgb_l2": GATE,
        "raw_result_count": len(rows), "raw_csv": {"path": RAW_CSV.as_posix(),
            "sha256": hashlib.sha256(RAW_CSV.read_bytes()).hexdigest()},
        "source_script": {"path": Path(__file__).as_posix(),
                          "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "ray_inventory": ray_inventory, "summary": summary, "per_frame_summary": per_frame,
        "limitations": [
            "The synthetic AABB and cubic 1% ray-length penumbra do not represent geometric shadow maps or production filtering.",
            "The inherited 1e-3 max relative RGB L2 gate is a numerical stress-study gate, not a perceptual or temporal acceptance threshold.",
            "This pixel-center population does not include subpixel rays, motion-vector reprojection, GPU/runtime behavior, alpha, or performance.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("rays", len(population), "frames", frame_counts, "rows", len(rows))
    print("region rays", manifest["region_ray_counts"], "validation max", validation_max)
    for name, data in summary.items():
        print(name, "exact baseline", data["exact_interval_baseline"])
        for key in ("center=+0.000%,width=+0.000%,steps=32",
                    "center=+0.000%,width=+0.000%,steps=128",
                    "center=+0.000%,width=+0.000%,steps=512",
                    "center=+0.000%,width=+0.000%,steps=2048",
                    "center=+2.000%,width=+0.000%,steps=128",
                    "center=+0.000%,width=+2.000%,steps=128"):
            if key in data:
                print(" ", key, data[key])
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
