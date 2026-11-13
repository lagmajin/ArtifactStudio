#!/usr/bin/env python3
"""Evaluate a projected-box coverage approximation for hard-edged froxels."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import fog_local_froxel_hard_boundary_oracle_rd as oracle
import fog_local_froxel_hard_boundary_rd as hard
import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-box-coverage-rd-2026-11-13")
SCENARIOS = ("centered", "x_shift_half", "depth_shift_half")
CONFIGS = ((2, 32), (4, 32), (8, 32), (4, 64))
LIGHT_COUNT = 3
ORACLE_STEPS = 256


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def projected_box_bounds(box):
    aspect = proxy.WIDTH / proxy.HEIGHT
    tan_half = math.tan(math.radians(proxy.scene.FOV_DEGREES) * 0.5)
    points = []
    for x in (box["minimum"][0], box["maximum"][0]):
        for y in (box["minimum"][1], box["maximum"][1]):
            for z in (box["minimum"][2], box["maximum"][2]):
                view_z = z - proxy.CAMERA[2]
                if view_z <= proxy.NEAR_VIEW_Z:
                    continue
                ndc_x = x / view_z / (aspect * tan_half)
                ndc_y = (y - proxy.CAMERA[1]) / view_z / tan_half
                pixel_x = (ndc_x + 1.0) * proxy.WIDTH * 0.5 - 0.5
                pixel_y = (1.0 - ndc_y) * proxy.HEIGHT * 0.5 - 0.5
                points.append((pixel_x, pixel_y))
    if not points:
        return None
    return (min(p[0] for p in points), min(p[1] for p in points),
            max(p[0] for p in points), max(p[1] for p in points))


def overlap_fraction(a0, a1, b0, b1):
    width = max(0.0, min(a1, b1) - max(a0, b0))
    return width / max(a1 - a0, 1e-12)


def build_coverage_grid(tile, slices, box):
    grid_width = math.ceil(proxy.WIDTH / tile)
    grid_height = math.ceil(proxy.HEIGHT / tile)
    edges = proxy.depth_edges(slices, "log")
    bounds = projected_box_bounds(box)
    values = []
    coverage_sum = 0.0
    nonzero_cells = 0
    for z in range(slices):
        z0, z1 = edges[z], edges[z + 1]
        center_z = math.sqrt(z0 * z1)
        depth_coverage = overlap_fraction(z0, z1,
                                          box["minimum"][2] - proxy.CAMERA[2],
                                          box["maximum"][2] - proxy.CAMERA[2])
        for y in range(grid_height):
            py = min(proxy.HEIGHT - 1, int((y + 0.5) * tile))
            cell_y0 = y * tile - 0.5
            cell_y1 = min(proxy.HEIGHT, (y + 1) * tile) - 0.5
            for x in range(grid_width):
                px = min(proxy.WIDTH - 1, int((x + 0.5) * tile))
                cell_x0 = x * tile - 0.5
                cell_x1 = min(proxy.WIDTH, (x + 1) * tile) - 0.5
                if bounds is None:
                    xy_coverage = 0.0
                else:
                    cx = overlap_fraction(cell_x0, cell_x1, bounds[0], bounds[2])
                    cy = overlap_fraction(cell_y0, cell_y1, bounds[1], bounds[3])
                    xy_coverage = cx * cy
                coverage = xy_coverage * depth_coverage
                coverage_sum += coverage
                if coverage > 0.0:
                    nonzero_cells += 1
                ray = proxy.scene.ray_for_pixel(px, py)
                position = proxy.add(proxy.CAMERA, proxy.mul(ray, center_z / ray[2]))
                light = proxy.light_source_at(position, LIGHT_COUNT)
                values.append((hard.DENSITY * coverage, light))
    coverage_stats = {
        "nonzero_coverage_cell_count": nonzero_cells,
        "sum_of_cell_coverage_fractions": coverage_sum,
    }
    return (grid_width, grid_height, edges, values), coverage_stats


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
        print(f"Oracle {scenario}...", flush=True)
        full_reference = [oracle.exact_interval_integral(rays[i], distances[i], sources[i],
                                                         proxy.CAMERA, box, ORACLE_STEPS)
                          for i in range(len(rays))]
        fog_reference = [oracle.exact_interval_integral(rays[i], distances[i], (0.0, 0.0, 0.0),
                                                        proxy.CAMERA, box, ORACLE_STEPS)
                         for i in range(len(rays))]
        rows = {}
        for tile, slices in CONFIGS:
            grid, coverage_stats = build_coverage_grid(tile, slices, box)
            name = f"tile{tile}_z{slices}"
            print(f"  {scenario}: {name}", flush=True)
            full = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid, "log")
                    for i in range(len(rays))]
            fog = [proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid, "log")
                   for i in range(len(rays))]
            cells = grid[0] * grid[1] * slices
            rows[name] = {
                "tile_width_pixels": tile,
                "depth_slices": slices,
                "froxel_cell_count": cells,
                "light_evaluations": cells * LIGHT_COUNT,
                "estimated_bytes_R16F_density_RGBA16F_source": cells * 10,
                "coverage_accounting": coverage_stats,
                "full_image_metrics_vs_interval_oracle": proxy.image_metrics(full_reference, full),
                "fog_only_metrics_vs_interval_oracle": proxy.image_metrics(fog_reference, fog),
            }
        results[scenario] = rows

    manifest = {
        "schema": "fog-local-froxel-box-coverage-rd-v1",
        "method": {
            "description": "Project all 8 AABB corners to screen, compute screen-rectangle overlap fraction per XY cell, multiply by exact view-Z interval overlap fraction, and inject constant density times coverage.",
            "point_light_source": "evaluated at froxel center only",
            "scattering_integration": "existing trilinear froxel integration",
        },
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "boxes": {name: hard.BOX_SCENARIOS[name] for name in SCENARIOS},
            "density_inside": hard.DENSITY,
            "light_count": LIGHT_COUNT,
            "configs": [list(config) for config in CONFIGS],
            "interval_oracle_quadrature_steps": ORACLE_STEPS,
        },
        "results": results,
        "limitations": [
            "CPU proxy only; no GPU runtime or performance measurement.",
            "Projected AABB is approximated by an axis-aligned screen rectangle and may overestimate the true projected footprint.",
            "The product of XY and depth fractions assumes separable occupancy within a froxel and does not model partial correlation or exact ray-box shape at each pixel.",
            "Lighting is sampled at the froxel center; only density coverage is approximated.",
            "No shadow filtering, temporal reprojection, or backend parity.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
        "oracle_script_sha256": sha256(Path(oracle.__file__)),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    for scenario, configs in results.items():
        print(scenario)
        for name, row in configs.items():
            print(" ", name,
                  "full", row["full_image_metrics_vs_interval_oracle"]["relative_rgb_l2"],
                  "fog", row["fog_only_metrics_vs_interval_oracle"]["relative_rgb_l2"])


if __name__ == "__main__":
    main()
