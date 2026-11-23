#!/usr/bin/env python3
"""Measure silhouette motion and quadrature residuals for equal-volume AABBs."""

from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

import fog_height_camera_scene_rd as world
import fog_shadow_box_aspect_width_rd as geometry
import fog_shadow_camera_sequence_rd as sequence
import fog_shadow_image_rd as image
import fog_shadow_penumbra_rd as soft
import fog_sun_transmittance_rd as scattering


WIDTH = sequence.WIDTH
HEIGHT = sequence.HEIGHT
CAMERA_X = sequence.CAMERA_X
REFERENCE_STEPS = sequence.REFERENCE_STEPS
VALIDATION_STEPS = sequence.VALIDATION_STEPS
CANDIDATE_STEPS = sequence.CANDIDATE_STEPS
PENUMBRA = sequence.PENUMBRA_WIDTH
OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-box-aspect-camera-sequence-rd-2026-11-12")
RAW_FRAME_CSV = OUTPUT_DIR / "shape_frame_metrics.csv"
RAW_PAIR_CSV = OUTPUT_DIR / "shape_temporal_pair_metrics.csv"
SHAPES = geometry.SHAPES


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pixel_relative_l2_max(candidate, reference) -> float:
    maximum = 0.0
    for c, r in zip(candidate, reference):
        norm = sum(value * value for value in r)
        error = math.sqrt(sum((c[channel] - r[channel]) ** 2 for channel in range(3))
                          / max(norm, 1.0e-30))
        maximum = max(maximum, error)
    return maximum


def bool_edge_mask(mask: list[bool]) -> list[int]:
    edge = set()
    for index, value in enumerate(mask):
        x, y = index % WIDTH, index // WIDTH
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < WIDTH and 0 <= ny < HEIGHT:
                if mask[ny * WIDTH + nx] != value:
                    edge.add(index)
                    edge.add(ny * WIDTH + nx)
    return sorted(edge)


def mask_comparison(first: list[bool], second: list[bool]) -> dict[str, float | int]:
    intersection = sum(a and b for a, b in zip(first, second))
    union = sum(a or b for a, b in zip(first, second))
    xor = sum(a != b for a, b in zip(first, second))
    return {"intersection_pixels": intersection, "union_pixels": union,
            "xor_pixels": xor, "iou": intersection / union if union else 1.0}


def rgb_temporal_summary(current, previous, indices):
    delta = [tuple(current[i][c] - previous[i][c] for c in range(3))
             for i in range(WIDTH * HEIGHT)]
    return sequence.temporal_delta_summary(delta, indices)


def error_temporal_summary(current, current_reference, previous, previous_reference, indices):
    delta = [tuple((current[i][c] - current_reference[i][c])
                   - (previous[i][c] - previous_reference[i][c])
                   for c in range(3)) for i in range(WIDTH * HEIGHT)]
    return sequence.temporal_delta_summary(delta, indices)


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    world.WIDTH, world.HEIGHT = WIDTH, HEIGHT
    image.scene.WIDTH, image.scene.HEIGHT = WIDTH, HEIGHT
    image.WIDTH, image.HEIGHT = WIDTH, HEIGHT
    image.OUTPUT_DIR = OUTPUT_DIR

    sun_el = math.radians(45.0)
    sun_az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_el) * math.sin(sun_az), math.sin(sun_el),
                                math.cos(sun_el) * math.cos(sun_az)))
    center = (0.0, 3.5, 8.25)
    frames_by_shape = {}
    frame_files = {}
    frame_rows = []
    max_validation_per_pixel = 0.0
    max_validation_per_image = 0.0
    input_records = []

    for shape_name, half_extents in SHAPES.items():
        shape_frames = []
        for frame_index, camera_x in enumerate(CAMERA_X):
            origin, right, up, forward = sequence.camera_frame(camera_x)
            references, validations, candidates, masks, sources = [], [], [], [], []
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    ray = world.ray_for_pixel(x, y, right, up, forward)
                    source, distance, surface = world.scene_sample(origin, ray)
                    hit = geometry.box_interval(origin, ray, distance, center, half_extents)
                    truth = hit["intervals"] if hit else ()
                    case = {"camera_y": origin[1], "view_direction": ray,
                            "distance": distance, "sun_direction": sun}
                    ref_cells = soft.reference_cells(case, truth, PENUMBRA, REFERENCE_STEPS)
                    val_cells = soft.reference_cells(case, truth, PENUMBRA, VALIDATION_STEPS)
                    candidate_cells = soft.weighted_cells(
                        origin[1], ray[1], distance, CANDIDATE_STEPS, truth, PENUMBRA,
                    )
                    references.append(image.compose(case, source, ref_cells, truth))
                    validations.append(image.compose(case, source, val_cells, truth))
                    candidates.append(image.compose(case, source, candidate_cells, truth))
                    masks.append(bool(truth))
                    sources.append(source)
                    input_records.append((shape_name, frame_index, x, y, ray, distance,
                                          source, truth, surface))

            edge = bool_edge_mask(masks)
            inside = [i for i, value in enumerate(masks) if value]
            frame_name = f"{shape_name}_frame_{frame_index:02d}_camera_x_{camera_x:+.2f}"
            source_file = image.save_capture(f"{frame_name}_source", sources)
            reference_file = image.save_capture(f"{frame_name}_reference", references)
            candidate_file = image.save_capture(f"{frame_name}_candidate_128", candidates)
            difference_file = image.write_difference(
                f"{frame_name}_candidate_128", candidates, references)
            frame_files[frame_name] = {"source": source_file, "reference": reference_file,
                                       "candidate_128": candidate_file,
                                       "candidate_difference": difference_file}

            validation_image_l2 = image.image_relative_l2(validations, references)
            max_validation_per_image = max(max_validation_per_image, validation_image_l2)
            max_validation_per_pixel = max(
                max_validation_per_pixel,
                pixel_relative_l2_max(validations, references),
            )
            frame_metric = {
                "frame_index": frame_index,
                "camera_x": camera_x,
                "camera_origin": origin,
                "shadow_pixels": len(inside),
                "edge_band_pixels": len(edge),
                "reference_validation_image_relative_rgb_l2": validation_image_l2,
                "reference_validation_max_pixel_relative_rgb_l2": pixel_relative_l2_max(
                    validations, references),
                "candidate_reference_image_relative_rgb_l2": image.image_relative_l2(
                    candidates, references),
                "candidate_reference_all_pixels": sequence.rgb_error_summary(
                    candidates, references, list(range(WIDTH * HEIGHT))),
                "candidate_reference_edge_band": sequence.rgb_error_summary(
                    candidates, references, edge),
                "reference_capture": reference_file,
                "candidate_capture": candidate_file,
            }
            shape_frames.append({"reference": references, "validation": validations,
                                 "candidate": candidates, "mask": masks, "edge": edge,
                                 "metric": frame_metric})
            frame_rows.append({
                "shape": shape_name, "frame_index": frame_index, "camera_x": camera_x,
                "shadow_pixels": len(inside), "edge_band_pixels": len(edge),
                "reference_validation_image_relative_rgb_l2": validation_image_l2,
                "reference_validation_max_pixel_relative_rgb_l2": frame_metric[
                    "reference_validation_max_pixel_relative_rgb_l2"],
                "candidate_reference_image_relative_rgb_l2": frame_metric[
                    "candidate_reference_image_relative_rgb_l2"],
                "candidate_reference_edge_rmse": frame_metric[
                    "candidate_reference_edge_band"]["rgb_rmse"],
                "candidate_reference_edge_p95": frame_metric[
                    "candidate_reference_edge_band"]["p95_max_channel_delta"],
                "candidate_reference_edge_max": frame_metric[
                    "candidate_reference_edge_band"]["max_absolute_rgb"],
            })
            print("finished", shape_name, "frame", frame_index,
                  "shadow pixels", len(inside), "validation", validation_image_l2)
        frames_by_shape[shape_name] = shape_frames

    temporal_by_shape = {}
    pair_rows = []
    for shape_name, shape_frames in frames_by_shape.items():
        pair_metrics = []
        for frame_index in range(1, len(shape_frames)):
            previous, current = shape_frames[frame_index - 1], shape_frames[frame_index]
            edge_union = sorted(set(previous["edge"]) | set(current["edge"]))
            silhouette = mask_comparison(previous["mask"], current["mask"])
            reference_motion_all = rgb_temporal_summary(
                current["reference"], previous["reference"], list(range(WIDTH * HEIGHT)))
            reference_motion_edge = rgb_temporal_summary(
                current["reference"], previous["reference"], edge_union)
            residual_all = error_temporal_summary(
                current["candidate"], current["reference"],
                previous["candidate"], previous["reference"], list(range(WIDTH * HEIGHT)))
            residual_edge = error_temporal_summary(
                current["candidate"], current["reference"],
                previous["candidate"], previous["reference"], edge_union)
            pair = {
                "previous_frame": frame_index - 1,
                "current_frame": frame_index,
                "previous_camera_x": CAMERA_X[frame_index - 1],
                "current_camera_x": CAMERA_X[frame_index],
                "silhouette_motion": silhouette,
                "union_edge_band_pixels": len(edge_union),
                "reference_same_screen_motion_all": reference_motion_all,
                "reference_same_screen_motion_edge_union": reference_motion_edge,
                "quadrature_residual_delta_all": residual_all,
                "quadrature_residual_delta_edge_union": residual_edge,
            }
            pair_metrics.append(pair)
            pair_rows.append({
                "shape": shape_name,
                "previous_frame": frame_index - 1,
                "current_frame": frame_index,
                "previous_camera_x": CAMERA_X[frame_index - 1],
                "current_camera_x": CAMERA_X[frame_index],
                "previous_shadow_pixels": sum(previous["mask"]),
                "current_shadow_pixels": sum(current["mask"]),
                "silhouette_intersection_pixels": silhouette["intersection_pixels"],
                "silhouette_union_pixels": silhouette["union_pixels"],
                "silhouette_xor_pixels": silhouette["xor_pixels"],
                "silhouette_iou": silhouette["iou"],
                "reference_motion_all_rmse": reference_motion_all["rgb_rmse"],
                "reference_motion_edge_rmse": reference_motion_edge["rgb_rmse"],
                "quadrature_residual_delta_all_rmse": residual_all["rgb_rmse"],
                "quadrature_residual_delta_all_p95": residual_all["p95_max_channel_delta"],
                "quadrature_residual_delta_all_max": residual_all["max_absolute_rgb"],
                "quadrature_residual_delta_edge_rmse": residual_edge["rgb_rmse"],
                "quadrature_residual_delta_edge_p95": residual_edge[
                    "p95_max_channel_delta"],
                "quadrature_residual_delta_edge_max": residual_edge["max_absolute_rgb"],
                "union_edge_band_pixels": len(edge_union),
            })

        ref_rms = [pair["reference_same_screen_motion_all"]["rgb_rmse"]
                   for pair in pair_metrics]
        ref_edge_rms = [pair["reference_same_screen_motion_edge_union"]["rgb_rmse"]
                        for pair in pair_metrics]
        residual_rms = [pair["quadrature_residual_delta_all"]["rgb_rmse"]
                        for pair in pair_metrics]
        residual_edge_rms = [pair["quadrature_residual_delta_edge_union"]["rgb_rmse"]
                             for pair in pair_metrics]
        temporal_by_shape[shape_name] = {
            "per_frame": [frame["metric"] for frame in shape_frames],
            "per_pair": pair_metrics,
            "mean_reference_same_screen_motion_rmse": sum(ref_rms) / len(ref_rms),
            "mean_reference_same_screen_edge_motion_rmse": sum(ref_edge_rms) / len(ref_edge_rms),
            "mean_quadrature_residual_delta_rmse": sum(residual_rms) / len(residual_rms),
            "max_quadrature_residual_delta_rmse": max(residual_rms),
            "mean_quadrature_residual_delta_edge_rmse": sum(residual_edge_rms) / len(residual_edge_rms),
            "max_quadrature_residual_delta_edge_rmse": max(residual_edge_rms),
        }

    baseline_frames = frames_by_shape["baseline"]
    shape_vs_baseline_by_frame = {}
    for shape_name, shape_frames in frames_by_shape.items():
        shape_vs_baseline_by_frame[shape_name] = []
        for frame_index, frame in enumerate(shape_frames):
            baseline = baseline_frames[frame_index]
            mask_delta = mask_comparison(baseline["mask"], frame["mask"])
            shape_vs_baseline_by_frame[shape_name].append({
                "frame_index": frame_index,
                "camera_x": CAMERA_X[frame_index],
                "silhouette_vs_baseline": mask_delta,
                "reference_image_relative_rgb_l2_vs_baseline": image.image_relative_l2(
                    frame["reference"], baseline["reference"]),
            })

    with RAW_FRAME_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(frame_rows[0]))
        writer.writeheader()
        writer.writerows(frame_rows)
    with RAW_PAIR_CSV.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(pair_rows[0]))
        writer.writeheader()
        writer.writerows(pair_rows)

    input_digest = hashlib.sha256(json.dumps({
        "size": [WIDTH, HEIGHT], "camera_x": CAMERA_X, "target": sequence.TARGET,
        "camera_height": sequence.CAMERA_HEIGHT, "fov_degrees": world.FOV_DEGREES,
        "scene_rays_source_distance_intervals": input_records,
        "box_center": center, "shapes": SHAPES,
    }, separators=(",", ":")).encode("utf-8")).hexdigest()
    source_paths = [Path(__file__), Path(world.__file__), Path(geometry.__file__),
                    Path(sequence.__file__), Path(image.__file__), Path(soft.__file__),
                    Path(scattering.__file__)]
    manifest = {
        "schema": "fog-shadow-box-aspect-camera-sequence-rd-v1",
        "execution_scope": "CPU synthetic 7-frame sequence; direct AABB ray intersection; no product renderer/GPU/runtime timing.",
        "pre_registered_evaluation_plan": {
            "image": f"{WIDTH}x{HEIGHT} opaque scene-linear world-space chart; same camera poses as fog_shadow_camera_sequence_rd.py.",
            "camera_x_world_units": CAMERA_X,
            "fixed_target": sequence.TARGET,
            "box_center": center,
            "shapes": SHAPES,
            "volume_each": 33.0,
            "sun_elevation_degrees": 45.0,
            "penumbra_ray_length_fraction": PENUMBRA,
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "candidate_steps": CANDIDATE_STEPS,
            "candidate": "True AABB interval for visibility, profile knots and weights; no center or width error.",
            "primary_separation": "Record reference same-screen image motion and silhouette-mask motion separately from candidate-reference quadrature-residual motion.",
            "temporal_measurement": "Same-screen-pixel candidate-reference residual delta; report all pixels and consecutive projected-edge union, without motion-vector reprojection.",
            "metrics": "Per-frame shadow coverage, reference/candidate error, silhouette pair IoU/XOR, reference motion RMSE and quadrature residual-delta RMSE/p95/max.",
            "gate": "No production temporal or image-quality pass threshold; reference-validation is the numerical integration check.",
        },
        "size": [WIDTH, HEIGHT],
        "alpha": "Opaque fixture only; alpha is not represented.",
        "color": "Scene-linear float32 RGB; PNG previews use shared Reinhard plus gamma 1/2.2.",
        "input_fixture_sha256": input_digest,
        "geometry_by_shape": {name: {"half_extents": extents,
                                      "volume": 8.0 * math.prod(extents)}
                              for name, extents in SHAPES.items()},
        "reference_validation_max_image_relative_rgb_l2": max_validation_per_image,
        "reference_validation_max_pixel_relative_rgb_l2": max_validation_per_pixel,
        "temporal_metrics_by_shape": temporal_by_shape,
        "shape_reference_vs_baseline_by_frame": shape_vs_baseline_by_frame,
        "frame_captures": frame_files,
        "raw_frame_metrics_csv": {"path": RAW_FRAME_CSV.as_posix(), "rows": len(frame_rows),
                                  "sha256": sha256(RAW_FRAME_CSV)},
        "raw_temporal_pair_metrics_csv": {"path": RAW_PAIR_CSV.as_posix(),
                                          "rows": len(pair_rows),
                                          "sha256": sha256(RAW_PAIR_CSV)},
        "source_scripts": [{"path": path.as_posix(), "sha256": sha256(path)}
                           for path in source_paths],
        "unverified": ["human visual production approval", "alpha-bearing material",
                       "motion-vector reprojection", "production flicker threshold",
                       "real shadow map filtering", "GPU/backend parity", "preview/final parity",
                       "product host integration", "processing time and memory"],
        "limitations": [
            "AABB ray visibility is geometric in this CPU model, but camera scene and single-scattering renderer are synthetic proxies.",
            "Same-screen residual differences include no motion compensation; they are distinct from reference image motion but are not perceptual flicker measurements.",
            "One camera path, sun elevation, medium, and scene only; shape ranking is not generalized.",
        ],
    }
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("max reference-validation image L2", max_validation_per_image)
    print("max reference-validation pixel L2", max_validation_per_pixel)
    for shape_name, metrics in temporal_by_shape.items():
        print(shape_name, "shadow coverage by frame",
              [frame["shadow_pixels"] for frame in metrics["per_frame"]],
              "mean reference motion", metrics["mean_reference_same_screen_motion_rmse"],
              "mean residual delta", metrics["mean_quadrature_residual_delta_rmse"],
              "edge residual delta", metrics["mean_quadrature_residual_delta_edge_rmse"])
    print(manifest_path.as_posix())


if __name__ == "__main__":
    main()
