#!/usr/bin/env python3
"""Measure local-fog CPU proxy errors across a camera-translation sequence."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-temporal-rd-2026-11-13")
FRAME_OFFSETS_X = (-0.4, -0.2, 0.0, 0.2, 0.4)
TILE = 4
SLICES = 32
LIGHT_COUNT = 3
REFERENCE_STEPS = 512
VALIDATION_STEPS = 256


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def scene_sample_at(ray, camera):
    candidates = []
    dx, dy, dz = ray
    ox, oy, oz = camera
    denom = dz - 0.12 * dx
    if abs(denom) > 1e-8:
        distance = (12.0 - oz) / denom
        if distance > 0.0:
            px, py, pz = ox + distance * dx, oy + distance * dy, oz + distance * dz
            if -5.0 <= px <= 5.0 and 0.0 <= py <= 6.0 and abs(pz - (12.0 + 0.12 * px)) < 1e-4:
                candidates.append((distance, proxy.scene.panel_color(px, py, False)))
    denom = dz + 0.20 * dx
    if abs(denom) > 1e-8:
        distance = (28.0 - oz) / denom
        if distance > 0.0:
            px, py, pz = ox + distance * dx, oy + distance * dy, oz + distance * dz
            if -12.0 <= px <= 12.0 and 0.0 <= py <= 10.0 and abs(pz - (28.0 - 0.20 * px)) < 1e-4:
                candidates.append((distance, proxy.scene.panel_color(px, py, True)))
    if dy < -1e-8:
        distance = (proxy.scene.GROUND - oy) / dy
        if 0.0 < distance <= proxy.scene.RAY_LIMIT:
            px, pz = ox + distance * dx, oz + distance * dz
            checker = (math.floor(px * 0.5) + math.floor(pz * 0.5)) & 1
            stripe = abs((pz * 0.16) % 1.0 - 0.5) < 0.018
            color = ((1.8, 1.5, 0.9) if stripe else
                     (0.19, 0.22, 0.25) if checker else (0.055, 0.075, 0.095))
            candidates.append((distance, color))
    if candidates:
        distance, color = min(candidates, key=lambda item: item[0])
        return color, min(distance, proxy.scene.RAY_LIMIT)
    distance = min((proxy.scene.ATMOSPHERE_TOP - oy) / dy, proxy.scene.RAY_LIMIT) if dy > 1e-8 else proxy.scene.RAY_LIMIT
    horizon = max(0.0, min(1.0, (ray[1] + 0.18) / 0.72))
    color = (0.16 + 0.25 * horizon, 0.24 + 0.34 * horizon, 0.36 + 0.48 * horizon)
    return color, distance


def frame_inputs(offset_x):
    camera = (proxy.CAMERA[0] + offset_x, proxy.CAMERA[1], proxy.CAMERA[2])
    rays, distances, sources = [], [], []
    for y in range(proxy.HEIGHT):
        for x in range(proxy.WIDTH):
            ray = proxy.scene.ray_for_pixel(x, y)
            source, distance = scene_sample_at(ray, camera)
            rays.append(ray)
            distances.append(distance)
            sources.append(source)
    return camera, rays, distances, sources


def image_delta_metrics(previous, current):
    deltas = [current[i][c] - previous[i][c]
              for i in range(len(previous)) for c in range(3)]
    absolute = sorted(abs(value) for value in deltas)
    return {
        "rgb_temporal_rmse": math.sqrt(sum(value * value for value in deltas) / max(1, len(deltas))),
        "max_absolute_channel_delta": max(absolute, default=0.0),
        "p95_absolute_channel_delta": absolute[min(len(absolute) - 1, int(0.95 * len(absolute)))] if absolute else 0.0,
    }


def main() -> None:
    proxy.scene.WIDTH, proxy.scene.HEIGHT = proxy.WIDTH, proxy.HEIGHT
    frame_rows = []
    previous = {"reference": None, "rebuilt": None, "stale": None}
    previous_errors = {"rebuilt": None, "stale": None}
    stale_grid = None
    for frame_index, offset_x in enumerate(FRAME_OFFSETS_X):
        print(f"Evaluating frame {frame_index + 1}/{len(FRAME_OFFSETS_X)}...", flush=True)
        camera, rays, distances, sources = frame_inputs(offset_x)
        reference = [proxy.integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS,
                                            LIGHT_COUNT, camera)
                     for i in range(len(rays))]
        validation = [proxy.integrate_direct(rays[i], distances[i], sources[i], VALIDATION_STEPS,
                                             LIGHT_COUNT, camera)
                      for i in range(len(rays))]
        grid = proxy.build_froxel_grid(TILE, SLICES, LIGHT_COUNT, "log", camera)
        rebuilt = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                          i // proxy.WIDTH, TILE, SLICES, grid, "log")
                   for i in range(len(rays))]
        if stale_grid is None:
            stale_grid = grid
        stale = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                        i // proxy.WIDTH, TILE, SLICES, stale_grid, "log")
                 for i in range(len(rays))]
        errors = {
            "rebuilt": proxy.image_metrics(reference, rebuilt),
            "stale_grid": proxy.image_metrics(reference, stale),
        }
        residual_images = {
            "rebuilt": [tuple(rebuilt[i][c] - reference[i][c] for c in range(3))
                        for i in range(len(reference))],
            "stale": [tuple(stale[i][c] - reference[i][c] for c in range(3))
                      for i in range(len(reference))],
        }
        temporal = None
        residual_temporal = None
        if previous["reference"] is not None:
            temporal = {
                "reference": image_delta_metrics(previous["reference"], reference),
                "rebuilt": image_delta_metrics(previous["rebuilt"], rebuilt),
                "stale_grid": image_delta_metrics(previous["stale"], stale),
            }
            residual_temporal = {
                mode: image_delta_metrics(previous_errors[mode], residual_images[mode])
                for mode in ("rebuilt", "stale")
            }
        frame_rows.append({
            "frame_index": frame_index,
            "camera_position": list(camera),
            "validation_direct_256_vs_512": proxy.image_metrics(reference, validation),
            "candidate_metrics_vs_direct_512": errors,
            "same_pixel_frame_delta": temporal,
            "candidate_minus_reference_residual_frame_delta": residual_temporal,
        })
        previous = {"reference": reference, "rebuilt": rebuilt, "stale": stale}
        previous_errors = residual_images

    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": "fog-local-froxel-temporal-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "camera_x_offsets": list(FRAME_OFFSETS_X),
            "camera_motion": "world-space horizontal translation; no rotation; fixed scene and lighting",
            "tile_size": TILE,
            "depth_slices": SLICES,
            "light_count": LIGHT_COUNT,
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "candidate_modes": ["rebuild current-frame froxel grid", "reuse first-frame grid without reprojection"],
        },
        "frames": frame_rows,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Stale-grid mode intentionally has no reprojection and is a negative control, not a production temporal filter.",
            "No motion vectors, jitter, temporal accumulation, disocclusion rejection, or history clamping.",
            "The proxy uses analytic Gaussian media and hard AABB visibility, not production depth/shadow inputs.",
            "Same-pixel image deltas include real scene motion; candidate-minus-reference residual deltas isolate approximation drift more closely.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    for row in frame_rows:
        print("frame", row["frame_index"], "rebuilt", row["candidate_metrics_vs_direct_512"]["rebuilt"]["relative_rgb_l2"],
              "stale", row["candidate_metrics_vs_direct_512"]["stale_grid"]["relative_rgb_l2"])


if __name__ == "__main__":
    main()
