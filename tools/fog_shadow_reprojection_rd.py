#!/usr/bin/env python3
"""Reproject known world points to measure camera-motion-compensated fog error."""

from __future__ import annotations

import csv
import hashlib
import json
import math
import struct
from pathlib import Path

import fog_height_camera_scene_rd as scene
import fog_shadow_camera_sequence_rd as sequence


OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12")
SEQUENCE_MANIFEST = OUTPUT_DIR / "manifest.json"
REPROJECTION_CSV = OUTPUT_DIR / "world_reprojection_pairs.csv"
REPROJECTION_MANIFEST = OUTPUT_DIR / "world_reprojection_manifest.json"
STABLE_COLOR_NEIGHBOR_MAX_DELTA = 0.35
SURFACE_DISTANCE_TOLERANCE = 0.05


def load_pfm(path: Path, width: int, height: int):
    with path.open("rb") as stream:
        if stream.readline().strip() != b"PF":
            raise ValueError(f"Not RGB PFM: {path}")
        dims = stream.readline().split()
        file_width, file_height = int(dims[0]), int(dims[1])
        scale = float(stream.readline().strip())
        if (file_width, file_height) != (width, height) or scale >= 0.0:
            raise ValueError(f"Unexpected PFM dimensions/endianness: {path}")
        raw = stream.read()
    values = list(struct.iter_unpack("<3f", raw))
    return [tuple(values[(height - 1 - y) * width + x])
            for y in range(height) for x in range(width)]


def dot(a, b):
    return sum(a[i] * b[i] for i in range(3))


def project(point, camera_x: float):
    origin, right, up, forward = sequence.camera_frame(camera_x)
    relative = tuple(point[i] - origin[i] for i in range(3))
    z = dot(relative, forward)
    if z <= 1e-6:
        return None
    aspect = scene.WIDTH / scene.HEIGHT
    tangent = math.tan(math.radians(scene.FOV_DEGREES) * 0.5)
    nx = dot(relative, right) / (z * tangent * aspect)
    ny = dot(relative, up) / (z * tangent)
    px = (nx + 1.0) * scene.WIDTH * 0.5 - 0.5
    py = (1.0 - ny) * scene.HEIGHT * 0.5 - 0.5
    if px < 0.0 or py < 0.0 or px >= scene.WIDTH - 1 or py >= scene.HEIGHT - 1:
        return None
    return px, py, math.sqrt(sum(v * v for v in relative))


def bilinear(image, x: float, y: float):
    x0, y0 = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - x0, y - y0
    indices = (y0 * scene.WIDTH + x0, y0 * scene.WIDTH + x0 + 1,
               (y0 + 1) * scene.WIDTH + x0, (y0 + 1) * scene.WIDTH + x0 + 1)
    weights = ((1.0 - fx) * (1.0 - fy), fx * (1.0 - fy),
               (1.0 - fx) * fy, fx * fy)
    return tuple(sum(image[index][c] * weight for index, weight in zip(indices, weights))
                 for c in range(3))


def stable_center_points():
    center_frame = len(sequence.CAMERA_X) // 2
    origin, right, up, forward = sequence.camera_frame(sequence.CAMERA_X[center_frame])
    center_intervals = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, right, up, forward)
            _source, distance, _surface = scene.scene_sample(origin, ray)
            center_intervals.append(sequence.ray_box_interval(origin, ray, distance) or ())
    interior_indices, edge_indices = sequence.boundary_mask(center_intervals)
    interior_indices = set(interior_indices)
    edge_indices = set(edge_indices)
    points = []
    for y in range(scene.HEIGHT):
        for x in range(scene.WIDTH):
            ray = scene.ray_for_pixel(x, y, right, up, forward)
            source, distance, surface = scene.scene_sample(origin, ray)
            if surface == "sky":
                continue
            index = y * scene.WIDTH + x
            color_stable = True
            label_stable = True
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if not (0 <= nx < scene.WIDTH and 0 <= ny < scene.HEIGHT):
                    continue
                adjacent_ray = scene.ray_for_pixel(nx, ny, right, up, forward)
                adjacent_source, _adjacent_distance, adjacent_surface = scene.scene_sample(origin, adjacent_ray)
                if adjacent_surface != surface:
                    label_stable = False
                if max(abs(source[c] - adjacent_source[c]) for c in range(3)) >= STABLE_COLOR_NEIGHBOR_MAX_DELTA:
                    color_stable = False
            if not label_stable or not color_stable:
                continue
            point = tuple(origin[i] + ray[i] * distance for i in range(3))
            shadow_interval = sequence.ray_box_interval(origin, ray, distance)
            region = "shadow_volume_interior" if index in interior_indices else "clear"
            if index in edge_indices:
                region = "shadow_silhouette_edge_proxy"
            points.append({"center_pixel_index": index, "world_point": point,
                           "surface": surface, "region": region})
    return points


def residual_delta(candidate_now, reference_now, candidate_previous, reference_previous):
    now = tuple(candidate_now[c] - reference_now[c] for c in range(3))
    previous = tuple(candidate_previous[c] - reference_previous[c] for c in range(3))
    return tuple(now[c] - previous[c] for c in range(3))


def summarize(rows):
    if not rows:
        return {"point_count": 0}
    rms = math.sqrt(sum(row["delta_error_l2_squared"] for row in rows) / (3 * len(rows)))
    max_delta = sorted(row["delta_error_max_channel"] for row in rows)
    return {
        "point_count": len(rows),
        "temporal_residual_rmse_rgb": rms,
        "mean_temporal_residual_max_channel": sum(max_delta) / len(max_delta),
        "p95_temporal_residual_max_channel": max_delta[math.ceil(0.95 * len(max_delta)) - 1],
        "max_temporal_residual_max_channel": max(max_delta),
        "mean_reference_motion_delta_rmse_rgb": math.sqrt(
            sum(row["reference_motion_l2_squared"] for row in rows) / (3 * len(rows))),
    }


def main() -> None:
    scene.WIDTH, scene.HEIGHT = sequence.WIDTH, sequence.HEIGHT
    manifest = json.loads(SEQUENCE_MANIFEST.read_text(encoding="utf-8"))
    frame_data = []
    for frame_name, files in manifest["frame_captures"].items():
        frame_data.append({
            "name": frame_name,
            "reference": load_pfm(OUTPUT_DIR / files["reference"]["pfm"], scene.WIDTH, scene.HEIGHT),
            "exact": load_pfm(OUTPUT_DIR / files["exact_profile_128"]["pfm"], scene.WIDTH, scene.HEIGHT),
            "estimated": load_pfm(OUTPUT_DIR / files["estimated_profile_128"]["pfm"], scene.WIDTH, scene.HEIGHT),
        })

    points = stable_center_points()
    pair_rows = []
    visible_counts = []
    for pair_index in range(1, len(frame_data)):
        previous_x = sequence.CAMERA_X[pair_index - 1]
        current_x = sequence.CAMERA_X[pair_index]
        for point in points:
            projections = []
            visible = True
            for camera_x in (previous_x, current_x):
                projected = project(point["world_point"], camera_x)
                if projected is None:
                    visible = False
                    break
                origin, _right, _up, _forward = sequence.camera_frame(camera_x)
                direction = tuple((point["world_point"][i] - origin[i]) / projected[2]
                                  for i in range(3))
                _source, hit_distance, hit_surface = scene.scene_sample(origin, direction)
                if hit_surface != point["surface"] or abs(hit_distance - projected[2]) > SURFACE_DISTANCE_TOLERANCE:
                    visible = False
                    break
                projections.append((projected[0], projected[1]))
            if not visible:
                continue
            (px0, py0), (px1, py1) = projections
            ref0 = bilinear(frame_data[pair_index - 1]["reference"], px0, py0)
            ref1 = bilinear(frame_data[pair_index]["reference"], px1, py1)
            reference_delta = tuple(ref1[c] - ref0[c] for c in range(3))
            for method in ("exact", "estimated"):
                cand0 = bilinear(frame_data[pair_index - 1][method], px0, py0)
                cand1 = bilinear(frame_data[pair_index][method], px1, py1)
                delta = residual_delta(cand1, ref1, cand0, ref0)
                pair_rows.append({
                    "pair_index": pair_index - 1,
                    "previous_frame": frame_data[pair_index - 1]["name"],
                    "current_frame": frame_data[pair_index]["name"],
                    "method": method,
                    "surface": point["surface"],
                    "region": point["region"],
                    "center_pixel_index": point["center_pixel_index"],
                    "world_point": point["world_point"],
                    "previous_projected_xy": (px0, py0),
                    "current_projected_xy": (px1, py1),
                    "temporal_residual_delta_rgb": delta,
                    "delta_error_max_channel": max(abs(value) for value in delta),
                    "delta_error_l2_squared": sum(value * value for value in delta),
                    "reference_motion_delta_rgb": reference_delta,
                    "reference_motion_l2_squared": sum(value * value for value in reference_delta),
                })
        visible_counts.append({"pair_index": pair_index - 1,
                               "visible_world_points": sum(1 for row in pair_rows
                                   if row["pair_index"] == pair_index - 1 and row["method"] == "exact")})

    with REPROJECTION_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(pair_rows[0]))
        writer.writeheader()
        writer.writerows(pair_rows)

    summary = {}
    for method in ("exact", "estimated"):
        summary[method] = {}
        for region in ("all", "shadow_volume_interior", "shadow_silhouette_edge_proxy", "clear"):
            selected = [row for row in pair_rows if row["method"] == method
                        and (region == "all" or row["region"] == region)]
            summary[method][region] = summarize(selected)
        summary[method]["by_surface"] = {
            surface: summarize([row for row in pair_rows if row["method"] == method
                                and row["surface"] == surface])
            for surface in sorted({row["surface"] for row in pair_rows})
        }

    result_manifest = {
        "schema": "fog-shadow-world-reprojection-rd-v1",
        "purpose": "Measure temporal quadrature residual at known static world points after camera-motion reprojection.",
        "execution_scope": "Post-analysis of the deterministic CPU camera sequence; no motion-vector renderer or GPU is involved.",
        "pre_registered_plan": {
            "reference_sequence_manifest_sha256": hashlib.sha256(SEQUENCE_MANIFEST.read_bytes()).hexdigest(),
            "world_points": "Visible non-sky points from the center camera frame, excluding points whose 4-neighbor surface labels differ or whose source RGB jump reaches 0.35; project each known world point into adjacent frames.",
            "visibility_test": f"Require matching nearest surface label and ray distance agreement within {SURFACE_DISTANCE_TOLERANCE} scene units in both frames.",
            "image_sampling": "Bilinear sample of linear RGB PFM at calibrated projected pixel positions.",
            "temporal_metric": "Difference of candidate-minus-reference RGB residual at the same world point between adjacent frames; report RMSE, p95 and maximum channel delta, plus reference appearance-motion delta RMSE.",
            "decision": "No pass threshold; compare exact-profile numerical residual and estimated-boundary residual, grouped by surface and shadow-volume proximity.",
        },
        "surface_distance_tolerance": SURFACE_DISTANCE_TOLERANCE,
        "stable_color_neighbor_threshold": STABLE_COLOR_NEIGHBOR_MAX_DELTA,
        "world_point_count_center_frame": len(points),
        "pair_visible_point_counts": visible_counts,
        "raw_result_count": len(pair_rows),
        "raw_csv": {"path": REPROJECTION_CSV.as_posix(),
                    "sha256": hashlib.sha256(REPROJECTION_CSV.read_bytes()).hexdigest()},
        "analysis_script": {"path": Path(__file__).as_posix(),
                            "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
        "summary": summary,
        "limitations": [
            "Correspondences come from the known synthetic world scene and exact camera calibration; this is not optical-flow performance.",
            "Bilinear image sampling and the 0.05-unit visibility tolerance can reject edges or introduce small resampling error.",
            "Only static opaque scene surfaces and a fixed AABB shadow-volume proxy are used.",
            "This metric isolates numerical candidate-reference residual change at a world point; it does not model perceptual flicker or display thresholds.",
        ],
    }
    REPROJECTION_MANIFEST.write_text(json.dumps(result_manifest, indent=2) + "\n",
                                     encoding="utf-8", newline="\n")
    print(REPROJECTION_MANIFEST.as_posix())
    print("stable center-frame world points", len(points), "raw pair rows", len(pair_rows))
    print("visible points by pair", visible_counts)
    for method in summary:
        print(method, "all", summary[method]["all"], "edge", summary[method]["shadow_silhouette_edge_proxy"])


if __name__ == "__main__":
    main()
