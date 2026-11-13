#!/usr/bin/env python3
"""Stress hard-edged local fog boxes against froxel XY/depth sampling."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-hard-boundary-rd-2026-11-13")
TILE_WIDTHS = (1, 2, 4, 8)
SLICE_COUNTS = (16, 32, 64)
LIGHT_COUNT = 3
REFERENCE_STEPS = 512
BOX_SCENARIOS = {
    "centered": {"minimum": (-2.0, 0.0, 15.0), "maximum": (2.0, 8.0, 19.0)},
    "x_shift_half": {"minimum": (-1.5, 0.0, 15.0), "maximum": (2.5, 8.0, 19.0)},
    "depth_shift_half": {"minimum": (-2.0, 0.0, 15.5), "maximum": (2.0, 8.0, 19.5)},
}
DENSITY = 0.035


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def box_density(position, box):
    return DENSITY if all(box["minimum"][axis] <= position[axis] <= box["maximum"][axis]
                          for axis in range(3)) else 0.0


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
    for scenario, box in BOX_SCENARIOS.items():
        density_fn = lambda position, selected=box: box_density(position, selected)
        print(f"Reference for {scenario}...", flush=True)
        reference = [proxy.integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS,
                                            LIGHT_COUNT, proxy.CAMERA, (), density_fn)
                     for i in range(len(rays))]
        fog_reference = [proxy.integrate_direct(rays[i], distances[i], (0.0, 0.0, 0.0),
                                                REFERENCE_STEPS, LIGHT_COUNT, proxy.CAMERA, (), density_fn)
                         for i in range(len(rays))]
        scenario_results = {}
        for slices in SLICE_COUNTS:
            for tile in TILE_WIDTHS:
                name = f"tile{tile}_z{slices}"
                print(f"  {scenario}: {name}", flush=True)
                grid = proxy.build_froxel_grid(tile, slices, LIGHT_COUNT, "log", proxy.CAMERA,
                                               (), density_fn)
                pixels = [
                    proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid, "log")
                    for i in range(len(rays))
                ]
                fog_pixels = [
                    proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid, "log")
                    for i in range(len(rays))
                ]
                cells = grid[0] * grid[1] * slices
                scenario_results[name] = {
                    "tile_width_pixels": tile,
                    "depth_slices": slices,
                    "froxel_cell_count": cells,
                    "light_evaluations": cells * LIGHT_COUNT,
                    "estimated_bytes_R16F_density_RGBA16F_source": cells * 10,
                    "full_image_metrics_vs_direct_512": proxy.image_metrics(reference, pixels),
                    "fog_only_metrics_vs_direct_512": proxy.image_metrics(fog_reference, fog_pixels),
                }
        results[scenario] = scenario_results

    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": "fog-local-froxel-hard-boundary-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "boxes": BOX_SCENARIOS,
            "density_inside": DENSITY,
            "density_outside": 0.0,
            "light_count": LIGHT_COUNT,
            "tile_widths_pixels": list(TILE_WIDTHS),
            "depth_slices": list(SLICE_COUNTS),
            "depth_distribution": "logarithmic view-Z",
            "reference_steps": REFERENCE_STEPS,
        },
        "results": results,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Boxes use a hard binary density and analytic scene-space test, not production volume injection or rasterized depth.",
            "Point-light visibility remains the existing analytic AABB shadow proxy.",
            "Memory estimates are arithmetic scenarios, not production resource layouts.",
            "No temporal reprojection, jitter, filtering, or backend parity.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    for scenario, rows in results.items():
        print(scenario)
        for name, row in rows.items():
            print(" ", name,
                  "full", row["full_image_metrics_vs_direct_512"]["relative_rgb_l2"],
                  "fog", row["fog_only_metrics_vs_direct_512"]["relative_rgb_l2"])


if __name__ == "__main__":
    main()
