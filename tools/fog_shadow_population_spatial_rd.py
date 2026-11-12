#!/usr/bin/env python3
"""Create fixed-scale screen-space maps of population shadow-integration error."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_penumbra_rd as soft


ROOT = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
INPUT = ROOT / "population_convergence_sweep.csv"
MANIFEST = ROOT / "population_spatial_manifest.json"
WIDTH = sequence.WIDTH
HEIGHT = sequence.HEIGHT
STEPS = 128
GATE = soft.GATE
DISPLAY_GAIN = 50.0
CASES = {
    "center_minus_2pct_width_exact": (-0.02, 0.0),
    "center_plus_2pct_width_exact": (0.02, 0.0),
    "center_exact_width_minus_2pct": (0.0, -0.02),
    "center_exact_width_plus_2pct": (0.0, 0.02),
}


def quantile(values, q):
    return sorted(values)[max(0, math.ceil(q * len(values)) - 1)]


def summarize(rows):
    values = [row["error"] for row in rows]
    worst = sorted(rows, key=lambda row: row["error"], reverse=True)[:8]
    return {
        "ray_count": len(values),
        "p50_relative_rgb_l2_error": quantile(values, 0.50),
        "p95_relative_rgb_l2_error": quantile(values, 0.95),
        "max_relative_rgb_l2_error": max(values),
        "gate_exceedance_count": sum(value > GATE for value in values),
        "worst_screen_coordinates": [
            {"frame": row["frame"], "camera_x": row["camera_x"],
             "x": row["x"], "y": row["y"], "region": row["region"],
             "surface": row["surface"], "error": row["error"]}
            for row in worst
        ],
    }


def main():
    scene.WIDTH, scene.HEIGHT = WIDTH, HEIGHT
    with INPUT.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        input_rows = list(reader)

    population = {}
    for row in input_rows:
        if row["exact_interval_baseline"] != "False":
            continue
        if int(row["steps"]) != STEPS:
            continue
        key = (int(row["frame"]), int(row["pixel_x"]), int(row["pixel_y"]))
        population.setdefault(key, {"region": row["region"], "surface": row["surface"],
                                    "camera_x": float(row["camera_x"]), "cases": {}})
        pair = (float(row["center_offset_fraction_of_ray"]),
                float(row["core_relative_width_error"]))
        population[key]["cases"][pair] = float(row["relative_rgb_l2_error"])

    if not population:
        raise RuntimeError("no all-population, non-subset rows found")

    output_files = {}
    summary = {}
    for case_name, pair in CASES.items():
        summary[case_name] = {"all_frames": {}, "per_frame": {}, "per_region": {}}
        rows_by_frame = {frame: [] for frame in range(len(sequence.CAMERA_X))}
        rows_by_region = {"silhouette_edge": [], "shadow_interior": []}
        for (frame, x, y), pixel in population.items():
            error = pixel["cases"].get(pair)
            if error is None:
                continue
            entry = {"frame": frame, "camera_x": pixel["camera_x"], "x": x, "y": y,
                     "region": pixel["region"], "surface": pixel["surface"], "error": error}
            rows_by_frame[frame].append(entry)
            rows_by_region[pixel["region"]].append(entry)
        all_rows = [row for frame_rows in rows_by_frame.values() for row in frame_rows]
        summary[case_name]["all_frames"] = summarize(all_rows)
        for frame, frame_rows in rows_by_frame.items():
            summary[case_name]["per_frame"][str(frame)] = summarize(frame_rows)
        for region, region_rows in rows_by_region.items():
            summary[case_name]["per_region"][region] = summarize(region_rows)

        safe = case_name
        for frame in range(len(sequence.CAMERA_X)):
            pixels = [(0.0, 0.0, 0.0)] * (WIDTH * HEIGHT)
            for row in rows_by_frame[frame]:
                value = min(1.0, row["error"] * DISPLAY_GAIN)
                pixels[row["y"] * WIDTH + row["x"]] = (value, value, value)
            stem = f"population_error_{safe}_frame_{frame:02d}"
            pfm = ROOT / f"{stem}.pfm"
            png = ROOT / f"{stem}_gain{int(DISPLAY_GAIN)}.png"
            scene.write_pfm(pfm, pixels)
            scene.write_preview(png, pixels)
            output_files[f"{safe}/frame_{frame:02d}"] = {
                "pfm": pfm.name, "pfm_sha256": hashlib.sha256(pfm.read_bytes()).hexdigest(),
                "preview_png": png.name,
                "preview_sha256": hashlib.sha256(png.read_bytes()).hexdigest(),
                "camera_x": sequence.CAMERA_X[frame],
                "shadowed_ray_count": len(rows_by_frame[frame]),
            }

    manifest = {
        "schema": "fog-shadow-population-spatial-error-rd-v1",
        "purpose": "Spatialize the full-population relative RGB L2 error for preselected center/width perturbations.",
        "input_csv": {"path": INPUT.as_posix(),
                       "sha256": hashlib.sha256(INPUT.read_bytes()).hexdigest()},
        "population_ray_count": len(population), "resolution": [WIDTH, HEIGHT],
        "candidate_steps": STEPS, "pre_registered_cases": CASES,
        "relative_error_gate": GATE,
        "preview_mapping": {"formula": "min(1, relative_rgb_l2_error * 50)",
                            "black_outside_shadow_population": True,
                            "white_at_or_above_error": 0.02,
                            "pfm": "linear grayscale float32 map of scaled error",
                            "png": "display-only preview generated by fog_height_camera_scene_rd.write_preview"},
        "summary": summary, "captures": output_files,
        "limitations": [
            "These are integration-error maps, not fog radiance or visual-quality images.",
            "Same-screen changes across frames include camera motion and moving projected shadow boundaries; they do not measure flicker.",
            "Synthetic AABB, analytic interval, 1% cubic smoothstep, pixel-center CPU rays only; no GPU, runtime, or geometric shadow filtering.",
        ],
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    for name, cases in summary.items():
        print(name, cases["all_frames"])
        print(" regions", {region: data["max_relative_rgb_l2_error"]
                           for region, data in cases["per_region"].items()})
    print("captured maps", len(output_files), "population", len(population))
    print(MANIFEST.as_posix())


if __name__ == "__main__":
    main()
