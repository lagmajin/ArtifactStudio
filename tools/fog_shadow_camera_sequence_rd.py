#!/usr/bin/env python3
"""Measure temporal quadrature error over a short deterministic camera move."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_shadow_combined_error_rd as combined
import fog_shadow_penumbra_rd as soft
import fog_shadow_sampling_rd as shadow
import fog_sun_transmittance_rd as scattering


WIDTH = 64
HEIGHT = 36
CAMERA_X = (-0.30, -0.20, -0.10, 0.0, 0.10, 0.20, 0.30)
CAMERA_HEIGHT = 4.0
TARGET = (0.0, 4.0, 18.0)
SHADOW_VOLUME_MIN = (-2.2, 1.0, 7.5)
SHADOW_VOLUME_MAX = (2.2, 6.0, 9.0)
PENUMBRA_WIDTH = 0.01
CENTER_ERROR = 0.005
WIDTH_ERROR = 0.005
REFERENCE_STEPS = 2048
VALIDATION_STEPS = 1024
CANDIDATE_STEPS = 128
OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
DIFF_GAIN = 4096.0


def add(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def mul(a, value):
    return tuple(component * value for component in a)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalize(v):
    return mul(v, 1.0 / math.sqrt(max(sum(component * component for component in v), 1e-20)))


def camera_frame(x: float):
    origin = (x, CAMERA_HEIGHT, 0.0)
    forward = normalize(sub(TARGET, origin))
    right = normalize(cross((0.0, 1.0, 0.0), forward))
    up = normalize(cross(forward, right))
    return origin, right, up, forward


def ray_box_interval(origin, direction, max_distance: float):
    near, far = 0.0, max_distance
    for axis in range(3):
        if abs(direction[axis]) < 1e-10:
            if origin[axis] < SHADOW_VOLUME_MIN[axis] or origin[axis] > SHADOW_VOLUME_MAX[axis]:
                return None
            continue
        a = (SHADOW_VOLUME_MIN[axis] - origin[axis]) / direction[axis]
        b = (SHADOW_VOLUME_MAX[axis] - origin[axis]) / direction[axis]
        near = max(near, min(a, b))
        far = min(far, max(a, b))
        if far <= near:
            return None
    if far <= 0.0 or near >= max_distance:
        return None
    start = max(0.0, near) / max_distance
    end = min(max_distance, far) / max_distance
    return ((start, end),) if end > start else None


def relative_l2(candidate, reference):
    err = sum((candidate[i][c] - reference[i][c]) ** 2
              for i in range(len(reference)) for c in range(3))
    norm = sum(value * value for pixel in reference for value in pixel)
    return math.sqrt(err / max(norm, 1e-30))


def rgb_error_summary(candidate, reference, indices):
    if not indices:
        return {"pixel_count": 0, "rgb_rmse": 0.0, "mean_absolute_rgb": 0.0,
                "max_absolute_rgb": 0.0, "relative_rgb_l2": 0.0}
    deltas = [max(abs(candidate[i][c] - reference[i][c]) for c in range(3)) for i in indices]
    sum_sq = sum((candidate[i][c] - reference[i][c]) ** 2 for i in indices for c in range(3))
    ref_norm = sum(reference[i][c] ** 2 for i in indices for c in range(3))
    return {
        "pixel_count": len(indices),
        "rgb_rmse": math.sqrt(sum_sq / (3 * len(indices))),
        "mean_absolute_rgb": sum(sum(abs(candidate[i][c] - reference[i][c])
                                      for c in range(3)) for i in indices) / (3 * len(indices)),
        "p95_max_channel_delta": sorted(deltas)[math.ceil(0.95 * len(deltas)) - 1],
        "max_absolute_rgb": max(deltas),
        "relative_rgb_l2": math.sqrt(sum_sq / max(ref_norm, 1e-30)),
    }


def temporal_delta_summary(deltas, indices):
    if not indices:
        return {"pixel_count": 0, "rgb_rmse": 0.0, "mean_absolute_rgb": 0.0,
                "p95_max_channel_delta": 0.0, "max_absolute_rgb": 0.0}
    peak_by_pixel = [max(abs(deltas[i][c]) for c in range(3)) for i in indices]
    squared = sum(deltas[i][c] * deltas[i][c] for i in indices for c in range(3))
    mean_abs = sum(abs(deltas[i][c]) for i in indices for c in range(3)) / (3 * len(indices))
    return {
        "pixel_count": len(indices),
        "rgb_rmse": math.sqrt(squared / (3 * len(indices))),
        "mean_absolute_rgb": mean_abs,
        "p95_max_channel_delta": sorted(peak_by_pixel)[math.ceil(0.95 * len(peak_by_pixel)) - 1],
        "max_absolute_rgb": max(peak_by_pixel),
    }


def save_frame(name: str, pixels):
    pfm = OUTPUT_DIR / f"{name}.pfm"
    png = OUTPUT_DIR / f"{name}_preview.png"
    scene.write_pfm(pfm, pixels)
    scene.write_preview(png, pixels)
    return {"pfm": pfm.name, "pfm_sha256": hashlib.sha256(pfm.read_bytes()).hexdigest(),
            "preview_png": png.name, "preview_sha256": hashlib.sha256(png.read_bytes()).hexdigest()}


def save_difference(name: str, candidate, reference):
    raw = [tuple(abs(candidate[i][c] - reference[i][c]) for c in range(3))
           for i in range(WIDTH * HEIGHT)]
    display = [tuple(value * DIFF_GAIN for value in pixel) for pixel in raw]
    return {"raw_absolute_difference": save_frame(f"{name}_diff_raw", raw),
            "gain_preview": save_frame(f"{name}_diff_gain{int(DIFF_GAIN)}", display),
            "display_gain": DIFF_GAIN}


def boundary_mask(intervals_by_pixel):
    inside = [bool(value) for value in intervals_by_pixel]
    edge = set()
    for index, current in enumerate(inside):
        x, y = index % WIDTH, index // WIDTH
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < WIDTH and 0 <= ny < HEIGHT:
                other = inside[ny * WIDTH + nx]
                if current != other:
                    edge.add(index)
                    edge.add(ny * WIDTH + nx)
    return [i for i, value in enumerate(inside) if value], sorted(edge)


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    scene.WIDTH, scene.HEIGHT = WIDTH, HEIGHT
    sun_el = math.radians(45.0)
    sun_az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_el) * math.sin(sun_az), math.sin(sun_el),
                                math.cos(sun_el) * math.cos(sun_az)))
    frames = []
    frame_files = {}
    frame_metrics = {}
    frame_edges = []
    validation_max = 0.0

    for frame_index, camera_x in enumerate(CAMERA_X):
        origin, right, up, forward = camera_frame(camera_x)
        cases, sources, intervals_by_pixel, references, validation = [], [], [], [], []
        exact_candidate, estimated_candidate = [], []
        for y in range(HEIGHT):
            for x in range(WIDTH):
                ray = scene.ray_for_pixel(x, y, right, up, forward)
                source_color, distance, _surface = scene.scene_sample(origin, ray)
                truth = ray_box_interval(origin, ray, distance) or ()
                case = {"camera_y": origin[1], "view_direction": ray,
                        "distance": distance, "sun_direction": sun}
                ref = soft.integrate_cells(case,
                    soft.reference_cells(case, truth, PENUMBRA_WIDTH, REFERENCE_STEPS),
                    truth, PENUMBRA_WIDTH)
                val = soft.integrate_cells(case,
                    soft.reference_cells(case, truth, PENUMBRA_WIDTH, VALIDATION_STEPS),
                    truth, PENUMBRA_WIDTH)
                view_depth = scattering.exact_density_integral(
                    origin[1], ray[1], distance, scattering.HEIGHT_FALLOFF, scattering.GROUND,
                )
                view_t = math.exp(-scattering.ABSORPTION * scattering.DENSITY_AT_GROUND * view_depth)
                references.append(tuple(source_color[c] * view_t + ref[c] for c in range(3)))
                validation.append(tuple(source_color[c] * view_t + val[c] for c in range(3)))

                exact_cells = soft.weighted_cells(origin[1], ray[1], distance,
                                                   CANDIDATE_STEPS, truth, PENUMBRA_WIDTH)
                estimated = combined.estimate_intervals(truth, CENTER_ERROR, WIDTH_ERROR)
                estimated_cells = soft.weighted_cells(origin[1], ray[1], distance,
                                                       CANDIDATE_STEPS, estimated, PENUMBRA_WIDTH)
                exact_rad = soft.integrate_cells(case, exact_cells, truth, PENUMBRA_WIDTH)
                estimated_rad = soft.integrate_cells(case, estimated_cells, truth, PENUMBRA_WIDTH)
                exact_candidate.append(tuple(source_color[c] * view_t + exact_rad[c] for c in range(3)))
                estimated_candidate.append(tuple(source_color[c] * view_t + estimated_rad[c] for c in range(3)))
                cases.append(case)
                sources.append(source_color)
                intervals_by_pixel.append(truth)
                validation_max = max(validation_max,
                    math.sqrt(sum((validation[-1][c] - references[-1][c]) ** 2 for c in range(3))
                              / max(sum(references[-1][c] ** 2 for c in range(3)), 1e-30)))

        interior, edge = boundary_mask(intervals_by_pixel)
        frame_edges.append(edge)
        frame_name = f"frame_{frame_index:02d}_camera_x_{camera_x:+.2f}"
        refs = save_frame(f"{frame_name}_reference", references)
        exact_file = save_frame(f"{frame_name}_exact_profile_128", exact_candidate)
        estimated_file = save_frame(f"{frame_name}_estimated_profile_128", estimated_candidate)
        frame_files[frame_name] = {"reference": refs, "exact_profile_128": exact_file,
                                   "estimated_profile_128": estimated_file,
                                   "exact_difference": save_difference(
                                       f"{frame_name}_exact_profile_128", exact_candidate, references),
                                   "estimated_difference": save_difference(
                                       f"{frame_name}_estimated_profile_128", estimated_candidate, references)}
        frame_metrics[frame_name] = {
            "camera_origin": list(origin),
            "reference_validation_relative_rgb_l2": relative_l2(validation, references),
            "exact_profile_128": {
                "whole_image_relative_rgb_l2": relative_l2(exact_candidate, references),
                "shadow_interior": rgb_error_summary(exact_candidate, references, interior),
                "shadow_edge_band": rgb_error_summary(exact_candidate, references, edge),
            },
            "estimated_profile_128": {
                "whole_image_relative_rgb_l2": relative_l2(estimated_candidate, references),
                "shadow_interior": rgb_error_summary(estimated_candidate, references, interior),
                "shadow_edge_band": rgb_error_summary(estimated_candidate, references, edge),
            },
        }
        frames.append({"reference": references, "exact": exact_candidate,
                       "estimated": estimated_candidate})

    temporal = {"exact_profile_128": {}, "estimated_profile_128": {}}
    for key in temporal:
        pair_all, pair_edge = [], []
        for i in range(1, len(frames)):
            current = frames[i]["exact" if key == "exact_profile_128" else "estimated"]
            previous = frames[i - 1]["exact" if key == "exact_profile_128" else "estimated"]
            current_ref, previous_ref = frames[i]["reference"], frames[i - 1]["reference"]
            current_error = [tuple(current[j][c] - current_ref[j][c] for c in range(3))
                             for j in range(WIDTH * HEIGHT)]
            previous_error = [tuple(previous[j][c] - previous_ref[j][c] for c in range(3))
                              for j in range(WIDTH * HEIGHT)]
            residual_delta = [tuple(current_error[j][c] - previous_error[j][c] for c in range(3))
                              for j in range(WIDTH * HEIGHT)]
            indices = sorted(set(frame_edges[i]) | set(frame_edges[i - 1]))
            pair_all.append(temporal_delta_summary(residual_delta, list(range(WIDTH * HEIGHT))))
            pair_edge.append(temporal_delta_summary(residual_delta, indices))
        temporal[key] = {
            "consecutive_frame_pairs": len(pair_all),
            "mean_all_pixel_temporal_residual_rmse": sum(x["rgb_rmse"] for x in pair_all) / len(pair_all),
            "max_all_pixel_temporal_residual_rmse": max(x["rgb_rmse"] for x in pair_all),
            "mean_union_edge_band_temporal_residual_rmse": sum(x["rgb_rmse"] for x in pair_edge) / len(pair_edge),
            "max_union_edge_band_temporal_residual_rmse": max(x["rgb_rmse"] for x in pair_edge),
            "per_pair_all_pixel": pair_all,
            "per_pair_union_edge_band": pair_edge,
        }

    manifest = {
        "schema": "fog-shadow-camera-sequence-rd-v1",
        "purpose": "Measure quadrature residual stability while a camera translates relative to a fixed world-space shadow volume.",
        "execution_scope": "Deterministic CPU synthetic sequence; not application playback, real shadow-map behavior, or GPU timing.",
        "pre_registered_evaluation_plan": {
            "sequence": f"{len(CAMERA_X)} frames, horizontal camera translation x={CAMERA_X[0]} to {CAMERA_X[-1]} world units; camera aims at fixed target.",
            "image": f"{WIDTH}x{HEIGHT} opaque scene-linear world-space chart; every frame recomputes rays, nearest scene intersection and shadow-volume interval.",
            "shadow": f"World-fixed AABB {SHADOW_VOLUME_MIN} to {SHADOW_VOLUME_MAX}; {PENUMBRA_WIDTH:.1%} ray-length cubic smoothstep penumbra.",
            "candidate_error": {"center_offset_fraction_of_ray": CENTER_ERROR,
                                "core_relative_width_error": WIDTH_ERROR},
            "reference": f"{REFERENCE_STEPS}-step profile/ground aligned midpoint; {VALIDATION_STEPS}-step validation.",
            "metrics": "Per-frame relative RGB L2 against reference; per-frame interior/edge p95 and max; consecutive-frame difference of candidate-minus-reference residuals for all pixels and union of projected shadow-edge masks.",
            "gate": f"Per-frame relative RGB L2 <= {soft.GATE}; temporal residual has no pass threshold because this synthetic sequence has no production temporal tolerance.",
        },
        "camera_x_world_units": CAMERA_X,
        "candidate_steps": CANDIDATE_STEPS,
        "reference_validation_max_per_ray_relative_rgb_l2": validation_max,
        "frames": frame_metrics,
        "temporal_residual_metrics": temporal,
        "frame_captures": frame_files,
        "source_scripts": {
            name: {"path": f"tools/{name}.py", "sha256": hashlib.sha256((Path("tools") / f"{name}.py").read_bytes()).hexdigest()}
            for name in ("fog_height_camera_scene_rd", "fog_shadow_penumbra_rd",
                         "fog_shadow_combined_error_rd", "fog_shadow_sampling_rd",
                         "fog_sun_transmittance_rd", "fog_shadow_camera_sequence_rd")
        },
        "limitations": [
            "The fixed AABB represents a synthetic shadowed fog volume, not directional-light shadowing from a geometric occluder.",
            "Camera motion is horizontal translation with fixed target; no rotation, zoom, acceleration, shutter integration, or motion blur.",
            "Temporal residual compares same screen coordinates and does not use motion-vector reprojection; its frame-difference statistic includes the intended movement of the projected silhouette.",
            "Opaque CPU fixture only; no alpha, product renderer, actual shadow filtering, GPU backend, or performance measurement.",
        ],
    }
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(manifest_path.as_posix())
    print("reference validation max per-ray relative RGB L2", validation_max)
    for key, metrics in temporal.items():
        print(key, "mean temporal residual RMSE", metrics["mean_all_pixel_temporal_residual_rmse"],
              "edge", metrics["mean_union_edge_band_temporal_residual_rmse"])
    print("estimated per-frame image errors", [v["estimated_profile_128"]["whole_image_relative_rgb_l2"]
                                               for v in frame_metrics.values()])


if __name__ == "__main__":
    main()
