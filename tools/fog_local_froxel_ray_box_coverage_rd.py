#!/usr/bin/env python3
"""Estimate per-froxel box occupancy from stratified rays and exact intervals."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_local_froxel_hard_boundary_oracle_rd as oracle
import fog_local_froxel_hard_boundary_rd as hard
import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-ray-box-coverage-rd-2026-11-13")
SCENARIOS = ("centered", "x_shift_half", "depth_shift_half")
TILE = 4
SLICE_COUNTS = (32, 64)
XY_RAYS_PER_AXIS = (1, 2, 4, 8)
LIGHT_COUNT = 3
ORACLE_STEPS = 256


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def build_ray_coverage_grid(box, slices, rays_per_axis):
    grid_width = math.ceil(proxy.WIDTH / TILE)
    grid_height = math.ceil(proxy.HEIGHT / TILE)
    edges = proxy.depth_edges(slices, "log")
    values = []
    interval_count = 0
    total_coverage = 0.0
    max_coverage = 0.0
    for z in range(slices):
        z0, z1 = edges[z], edges[z + 1]
        bin_width = z1 - z0
        center_z = math.sqrt(z0 * z1)
        for y in range(grid_height):
            for x in range(grid_width):
                coverage_sum = 0.0
                light_weighted_sum = [0.0, 0.0, 0.0]
                sample_count = rays_per_axis * rays_per_axis
                for sy in range(rays_per_axis):
                    py = y * TILE + ((sy + 0.5) / rays_per_axis) * TILE - 0.5
                    for sx in range(rays_per_axis):
                        px = x * TILE + ((sx + 0.5) / rays_per_axis) * TILE - 0.5
                        ray = proxy.scene.ray_for_pixel(px, py)
                        interval = oracle.ray_box_interval(proxy.CAMERA, ray, box,
                                                           proxy.FAR_VIEW_Z / ray[2])
                        if interval is None:
                            continue
                        entry_z = max(z0, interval[0] * ray[2])
                        exit_z = min(z1, interval[1] * ray[2])
                        if exit_z <= entry_z:
                            continue
                        fraction = (exit_z - entry_z) / bin_width
                        coverage_sum += fraction
                        interval_count += 1
                        midpoint = 0.5 * (entry_z + exit_z) / ray[2]
                        position = proxy.add(proxy.CAMERA, proxy.mul(ray, midpoint))
                        light = proxy.light_source_at(position, LIGHT_COUNT)
                        for channel in range(3):
                            light_weighted_sum[channel] += light[channel] * fraction
                coverage = coverage_sum / sample_count
                total_coverage += coverage
                max_coverage = max(max_coverage, coverage)
                center_x = min(proxy.WIDTH - 1, int((x + 0.5) * TILE))
                center_y = min(proxy.HEIGHT - 1, int((y + 0.5) * TILE))
                center_ray = proxy.scene.ray_for_pixel(center_x, center_y)
                center_position = proxy.add(proxy.CAMERA, proxy.mul(center_ray, center_z / center_ray[2]))
                fallback_light = proxy.light_source_at(center_position, LIGHT_COUNT)
                if coverage > 0.0:
                    light = tuple(value / coverage_sum for value in light_weighted_sum)
                else:
                    light = fallback_light
                values.append((hard.DENSITY * coverage, light))
    return (grid_width, grid_height, edges, values), {
        "ray_box_interval_evaluations": grid_width * grid_height * slices * rays_per_axis * rays_per_axis,
        "nonempty_subray_intervals": interval_count,
        "sum_of_froxel_coverage_fractions": total_coverage,
        "max_froxel_coverage_fraction": max_coverage,
    }


def main() -> None:
    proxy.scene.WIDTH, proxy.scene.HEIGHT = proxy.WIDTH, proxy.HEIGHT
    rays, distances, sources = [], [], []
    for y in range(proxy.HEIGHT):
        for x in range(proxy.WIDTH):
            ray = proxy.scene.ray_for_pixel(x, y)
            source, distance = proxy.scene.scene_sample(ray)
            rays.append(ray)
            distances.append(distance)
            sources.append(source)

    results = {}
    for scenario in SCENARIOS:
        box = hard.BOX_SCENARIOS[scenario]
        print(f"Building oracle for {scenario}...", flush=True)
        full_reference = [oracle.exact_interval_integral(rays[i], distances[i], sources[i],
                                                         proxy.CAMERA, box, ORACLE_STEPS)
                          for i in range(len(rays))]
        fog_reference = [oracle.exact_interval_integral(rays[i], distances[i], (0.0, 0.0, 0.0),
                                                        proxy.CAMERA, box, ORACLE_STEPS)
                         for i in range(len(rays))]
        configs = {}
        for slices in SLICE_COUNTS:
            for ray_count in XY_RAYS_PER_AXIS:
                name = f"z{slices}_xy{ray_count}x{ray_count}"
                print(f"  {scenario}: {name}", flush=True)
                grid, coverage_stats = build_ray_coverage_grid(box, slices, ray_count)
                full = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                               i // proxy.WIDTH, TILE, slices, grid, "log")
                        for i in range(len(rays))]
                fog = [proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                               i // proxy.WIDTH, TILE, slices, grid, "log")
                       for i in range(len(rays))]
                cells = grid[0] * grid[1] * slices
                configs[name] = {
                    "tile_width_pixels": TILE,
                    "depth_slices": slices,
                    "xy_rays_per_axis": ray_count,
                    "froxel_cell_count": cells,
                    "coverage_ray_box_tests": coverage_stats["ray_box_interval_evaluations"],
                    "coverage_light_evaluations_upper_bound": coverage_stats["nonempty_subray_intervals"] * LIGHT_COUNT,
                    "coverage_accounting": coverage_stats,
                    "estimated_bytes_R16F_density_RGBA16F_source": cells * 10,
                    "full_image_metrics_vs_interval_oracle": proxy.image_metrics(full_reference, full),
                    "fog_only_metrics_vs_interval_oracle": proxy.image_metrics(fog_reference, fog),
                }
        results[scenario] = configs

    manifest = {
        "schema": "fog-local-froxel-ray-box-coverage-rd-v1",
        "method": {
            "description": "For every froxel XY cell and log-depth slice, trace stratified subpixel rays, compute exact ray/AABB interval, clip its view-Z interval to the slice, and average occupied fraction. Lighting is averaged over occupied ray intervals.",
            "compared_to": "center-sample froxel grid and ray-box interval oracle",
        },
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "tile_width": TILE,
            "boxes": {name: hard.BOX_SCENARIOS[name] for name in SCENARIOS},
            "density_inside": hard.DENSITY,
            "light_count": LIGHT_COUNT,
            "depth_slices": list(SLICE_COUNTS),
            "xy_rays_per_axis": list(XY_RAYS_PER_AXIS),
            "oracle_quadrature_steps": ORACLE_STEPS,
        },
        "results": results,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock measurement.",
            "XY occupancy is estimated from finite stratified rays; convergence in XY is not guaranteed for subpixel volumes.",
            "Binary box density is exact only for this analytic AABB; arbitrary density fields need other integration methods.",
            "Point-light visibility remains analytic hard-AABB shadowing.",
            "Grid values store averaged density and occupancy-conditioned source lighting; nonlinear scattering/transmittance remains approximated after injection.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
        "oracle_script_sha256": sha256(Path(oracle.__file__)),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {path}")
    for scenario, configs in results.items():
        print(scenario)
        for name, row in configs.items():
            print(" ", name,
                  "full", row["full_image_metrics_vs_interval_oracle"]["relative_rgb_l2"],
                  "fog", row["fog_only_metrics_vs_interval_oracle"]["relative_rgb_l2"],
                  "coverage tests", row["coverage_ray_box_tests"])


if __name__ == "__main__":
    main()
