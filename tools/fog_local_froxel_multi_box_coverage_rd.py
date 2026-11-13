#!/usr/bin/env python3
"""Test ray-box slice occupancy with overlapping local fog volumes."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_local_froxel_hard_boundary_oracle_rd as oracle
import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-multi-box-coverage-rd-2026-11-13")
SCENARIOS = {
    "overlap": (
        {"minimum": (-2.5, 0.0, 15.0), "maximum": (1.5, 8.0, 19.0)},
        {"minimum": (-0.5, 1.0, 17.0), "maximum": (3.5, 7.0, 21.0)},
    ),
    "disjoint": (
        {"minimum": (-5.0, 0.0, 15.0), "maximum": (-1.0, 8.0, 19.0)},
        {"minimum": (2.0, 0.0, 19.0), "maximum": (6.0, 8.0, 23.0)},
    ),
    "staggered_overlap": (
        {"minimum": (-3.0, 0.0, 14.0), "maximum": (1.0, 8.0, 20.0)},
        {"minimum": (-1.0, 2.0, 17.0), "maximum": (3.0, 7.0, 24.0)},
    ),
}
DENSITY_PER_BOX = 0.025
TILE = 4
SLICES = 32
XY_RAYS_PER_AXIS = (1, 2, 4)
LIGHT_COUNT = 3
ORACLE_STEPS_PER_INTERVAL = 128


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def intervals_for_ray(ray, distance, boxes):
    far = min(distance, proxy.FAR_VIEW_Z / ray[2]) if ray[2] > 1e-8 else 0.0
    return [interval for box in boxes
            if (interval := oracle.ray_box_interval(proxy.CAMERA, ray, box, far)) is not None]


def composite_exact(ray, distance, source, boxes, steps_per_interval):
    intervals = intervals_for_ray(ray, distance, boxes)
    if not intervals:
        return source
    endpoints = sorted({point for interval in intervals for point in interval})
    transmittance = 1.0
    scattered = [0.0, 0.0, 0.0]
    for start, end in zip(endpoints, endpoints[1:]):
        midpoint = 0.5 * (start + end)
        active = sum(1 for a, b in intervals if a <= midpoint <= b)
        if active == 0:
            continue
        density = active * DENSITY_PER_BOX
        ds = (end - start) / steps_per_interval
        for step in range(steps_per_interval):
            t = start + (step + 0.5) * ds
            position = proxy.add(proxy.CAMERA, proxy.mul(ray, t))
            light = proxy.light_source_at(position, LIGHT_COUNT)
            segment_t = math.exp(-proxy.EXTINCTION * density * ds)
            weight = transmittance * (1.0 - segment_t) * proxy.SCATTERING_ALBEDO
            for channel in range(3):
                scattered[channel] += weight * light[channel]
            transmittance *= segment_t
    return tuple(source[channel] * transmittance + scattered[channel] for channel in range(3))


def build_multi_box_grid(boxes, rays_per_axis):
    grid_width = math.ceil(proxy.WIDTH / TILE)
    grid_height = math.ceil(proxy.HEIGHT / TILE)
    edges = proxy.depth_edges(SLICES, "log")
    values = []
    nonempty_intervals = 0
    for z in range(SLICES):
        z0, z1 = edges[z], edges[z + 1]
        bin_width = z1 - z0
        center_z = math.sqrt(z0 * z1)
        for y in range(grid_height):
            for x in range(grid_width):
                density_sum = 0.0
                weighted_light = [0.0, 0.0, 0.0]
                for sy in range(rays_per_axis):
                    py = y * TILE + ((sy + 0.5) / rays_per_axis) * TILE - 0.5
                    for sx in range(rays_per_axis):
                        px = x * TILE + ((sx + 0.5) / rays_per_axis) * TILE - 0.5
                        ray = proxy.scene.ray_for_pixel(px, py)
                        for box in boxes:
                            interval = oracle.ray_box_interval(proxy.CAMERA, ray, box,
                                                               proxy.FAR_VIEW_Z / ray[2])
                            if interval is None:
                                continue
                            entry_z = max(z0, interval[0] * ray[2])
                            exit_z = min(z1, interval[1] * ray[2])
                            if exit_z <= entry_z:
                                continue
                            nonempty_intervals += 1
                            fraction = (exit_z - entry_z) / bin_width
                            rho_fraction = DENSITY_PER_BOX * fraction
                            density_sum += rho_fraction
                            t = 0.5 * (entry_z + exit_z) / ray[2]
                            position = proxy.add(proxy.CAMERA, proxy.mul(ray, t))
                            light = proxy.light_source_at(position, LIGHT_COUNT)
                            for channel in range(3):
                                weighted_light[channel] += rho_fraction * light[channel]
                sample_count = rays_per_axis * rays_per_axis
                density = density_sum / sample_count
                if density_sum > 0.0:
                    light = tuple(value / density_sum for value in weighted_light)
                else:
                    px = min(proxy.WIDTH - 1, int((x + 0.5) * TILE))
                    py = min(proxy.HEIGHT - 1, int((y + 0.5) * TILE))
                    ray = proxy.scene.ray_for_pixel(px, py)
                    position = proxy.add(proxy.CAMERA, proxy.mul(ray, center_z / ray[2]))
                    light = proxy.light_source_at(position, LIGHT_COUNT)
                values.append((density, light))
    return (grid_width, grid_height, edges, values), nonempty_intervals


def exact_optical_depth(ray, distance, boxes):
    intervals = intervals_for_ray(ray, distance, boxes)
    endpoints = sorted({point for interval in intervals for point in interval})
    tau = 0.0
    for start, end in zip(endpoints, endpoints[1:]):
        midpoint = 0.5 * (start + end)
        active = sum(1 for a, b in intervals if a <= midpoint <= b)
        tau += proxy.EXTINCTION * active * DENSITY_PER_BOX * (end - start)
    return tau


def froxel_optical_depth(ray, distance, x, y, grid):
    grid_width, grid_height, edges, _ = grid
    maximum_view_z = min(proxy.FAR_VIEW_Z, distance * ray[2])
    tau = 0.0
    for index in range(len(edges) - 1):
        z0 = edges[index]
        z1 = min(edges[index + 1], maximum_view_z)
        if z1 <= z0:
            continue
        sample_z = math.sqrt(z0 * z1)
        density, _ = proxy.sample_froxel(grid, grid_width, grid_height, len(edges) - 1,
                                         TILE, x, y, sample_z, "log")
        tau += proxy.EXTINCTION * density * (z1 - z0) / ray[2]
        if z1 >= maximum_view_z:
            break
    return tau


def transmission_metrics(reference_taus, candidate_taus, indices):
    tau_errors = [candidate_taus[i] - reference_taus[i] for i in indices]
    exact_t = [math.exp(-reference_taus[i]) for i in indices]
    approx_t = [math.exp(-candidate_taus[i]) for i in indices]
    t_errors = [approx_t[i] - exact_t[i] for i in range(len(indices))]
    return {
        "pixel_count": len(indices),
        "optical_depth_rmse": math.sqrt(sum(e * e for e in tau_errors) / max(1, len(tau_errors))),
        "max_absolute_optical_depth_error": max((abs(e) for e in tau_errors), default=0.0),
        "transmittance_rmse": math.sqrt(sum(e * e for e in t_errors) / max(1, len(t_errors))),
        "max_absolute_transmittance_error": max((abs(e) for e in t_errors), default=0.0),
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
    for scenario, boxes in SCENARIOS.items():
        print(f"Oracle {scenario}...", flush=True)
        reference = [composite_exact(rays[i], distances[i], sources[i], boxes,
                                     ORACLE_STEPS_PER_INTERVAL)
                     for i in range(len(rays))]
        fog_reference = [composite_exact(rays[i], distances[i], (0.0, 0.0, 0.0), boxes,
                                         ORACLE_STEPS_PER_INTERVAL)
                         for i in range(len(rays))]
        validation = [composite_exact(rays[i], distances[i], sources[i], boxes,
                                     ORACLE_STEPS_PER_INTERVAL * 2)
                      for i in range(len(rays))]
        exact_taus = [exact_optical_depth(rays[i], distances[i], boxes)
                      for i in range(len(rays))]
        fog_indices = [i for i, tau in enumerate(exact_taus) if tau > 0.0]
        configs = {}
        density_fn = lambda position, selected=boxes: DENSITY_PER_BOX * sum(
            1 for box in selected if all(box["minimum"][axis] <= position[axis] <= box["maximum"][axis]
                                         for axis in range(3)))
        center_grid = proxy.build_froxel_grid(TILE, SLICES, LIGHT_COUNT, "log",
                                              proxy.CAMERA, (), density_fn)
        center_full = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                              i // proxy.WIDTH, TILE, SLICES, center_grid, "log")
                       for i in range(len(rays))]
        center_fog = [proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                             i // proxy.WIDTH, TILE, SLICES, center_grid, "log")
                      for i in range(len(rays))]
        configs["center_sample_baseline"] = {
            "froxel_cell_count": center_grid[0] * center_grid[1] * SLICES,
            "full_image_metrics_vs_oracle": proxy.image_metrics(reference, center_full),
            "fog_only_metrics_vs_oracle": proxy.image_metrics(fog_reference, center_fog),
            "transmittance_metrics": {
                "all_pixels": transmission_metrics(
                    exact_taus,
                    [froxel_optical_depth(rays[i], distances[i], i % proxy.WIDTH,
                                          i // proxy.WIDTH, center_grid)
                     for i in range(len(rays))],
                    list(range(len(rays))),
                ),
                "volume_intersecting_pixels": transmission_metrics(
                    exact_taus,
                    [froxel_optical_depth(rays[i], distances[i], i % proxy.WIDTH,
                                          i // proxy.WIDTH, center_grid)
                     for i in range(len(rays))],
                    fog_indices,
                ),
            },
        }
        for ray_count in XY_RAYS_PER_AXIS:
            print(f"  {scenario}: xy{ray_count}x{ray_count}", flush=True)
            grid, interval_tests = build_multi_box_grid(boxes, ray_count)
            full = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                           i // proxy.WIDTH, TILE, SLICES, grid, "log")
                    for i in range(len(rays))]
            fog = [proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                           i // proxy.WIDTH, TILE, SLICES, grid, "log")
                   for i in range(len(rays))]
            configs[f"xy{ray_count}x{ray_count}"] = {
                "xy_rays_per_axis": ray_count,
                "froxel_cell_count": grid[0] * grid[1] * SLICES,
                "ray_box_interval_tests": grid[0] * grid[1] * SLICES * ray_count * ray_count * len(boxes),
                "nonempty_box_slice_intersections": interval_tests,
                "validation_oracle_128_vs_256": proxy.image_metrics(reference, validation),
                "full_image_metrics_vs_oracle": proxy.image_metrics(reference, full),
                "fog_only_metrics_vs_oracle": proxy.image_metrics(fog_reference, fog),
                "transmittance_metrics": {
                    "all_pixels": transmission_metrics(
                        exact_taus,
                        [froxel_optical_depth(rays[i], distances[i], i % proxy.WIDTH,
                                              i // proxy.WIDTH, grid)
                         for i in range(len(rays))],
                        list(range(len(rays))),
                    ),
                    "volume_intersecting_pixels": transmission_metrics(
                        exact_taus,
                        [froxel_optical_depth(rays[i], distances[i], i % proxy.WIDTH,
                                              i // proxy.WIDTH, grid)
                         for i in range(len(rays))],
                        fog_indices,
                    ),
                },
            }
        results[scenario] = configs

    manifest = {
        "schema": "fog-local-froxel-multi-box-coverage-rd-v1",
        "method": {
            "density_composition": "additive extinction density; overlapping boxes contribute independently and sum",
            "source_composition": "scattering source weighted by each box's density times slice occupancy",
            "oracle": "exact ray/AABB intervals partitioned at every box entry/exit; constant active density integrated with midpoint source quadrature",
        },
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "boxes_by_scenario": SCENARIOS,
            "density_per_box": DENSITY_PER_BOX,
            "tile_width": TILE,
            "depth_slices": SLICES,
            "light_count": LIGHT_COUNT,
            "xy_rays_per_axis": list(XY_RAYS_PER_AXIS),
            "oracle_steps_per_interval": ORACLE_STEPS_PER_INTERVAL,
        },
        "results": results,
        "limitations": [
            "CPU synthetic AABB proxy; no GPU runtime/performance measurement.",
            "Only additive homogeneous box media are covered; overlapping heterogeneous phase functions or anisotropy are not represented.",
            "Each froxel stores average density and density-weighted source separately; nonlinear multi-medium scattering remains approximated after injection.",
            "Lighting uses analytic hard-AABB visibility, not filtered shadow maps.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {path}")
    for scenario, configs in results.items():
        print(scenario)
        for name, row in configs.items():
            print(" ", name,
                  "validation", row.get("validation_oracle_128_vs_256", {}).get("relative_rgb_l2"),
                  "full", row["full_image_metrics_vs_oracle"]["relative_rgb_l2"],
                  "fog", row["fog_only_metrics_vs_oracle"]["relative_rgb_l2"])


if __name__ == "__main__":
    main()
