#!/usr/bin/env python3
"""Separate froxel grid representation error from ray integration sampling error."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_hard_boundary_rd as hard
import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-integration-convergence-rd-2026-11-13")
SCENARIOS = ("centered", "x_shift_half", "depth_shift_half")
GRID_CONFIGS = ((4, 32), (8, 32), (4, 64))
INTEGRATION_SUBSTEPS = (1, 2, 4, 8)
LIGHT_COUNT = 3
REFERENCE_STEPS = 512


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


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
        density_fn = lambda position, selected=box: hard.box_density(position, selected)
        print(f"Reference for {scenario}...", flush=True)
        reference = [proxy.integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS,
                                            LIGHT_COUNT, proxy.CAMERA, (), density_fn)
                     for i in range(len(rays))]
        fog_reference = [proxy.integrate_direct(rays[i], distances[i], (0.0, 0.0, 0.0),
                                                REFERENCE_STEPS, LIGHT_COUNT, proxy.CAMERA, (), density_fn)
                         for i in range(len(rays))]
        config_results = {}
        for tile, slices in GRID_CONFIGS:
            grid = proxy.build_froxel_grid(tile, slices, LIGHT_COUNT, "log", proxy.CAMERA,
                                           (), density_fn)
            for substeps in INTEGRATION_SUBSTEPS:
                name = f"tile{tile}_z{slices}_substeps{substeps}"
                print(f"  {scenario}: {name}", flush=True)
                pixels = [
                    proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid, "log", substeps)
                    for i in range(len(rays))
                ]
                fog_pixels = [
                    proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid, "log", substeps)
                    for i in range(len(rays))
                ]
                config_results[name] = {
                    "tile_width_pixels": tile,
                    "depth_slices": slices,
                    "integration_substeps_per_slice": substeps,
                    "pixel_slice_field_sample_count": proxy.WIDTH * proxy.HEIGHT * slices * substeps,
                    "full_image_metrics_vs_direct_512": proxy.image_metrics(reference, pixels),
                    "fog_only_metrics_vs_direct_512": proxy.image_metrics(fog_reference, fog_pixels),
                }
        results[scenario] = config_results

    manifest = {
        "schema": "fog-local-froxel-integration-convergence-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "hard_box_scenarios": {name: hard.BOX_SCENARIOS[name] for name in SCENARIOS},
            "density_inside": hard.DENSITY,
            "light_count": LIGHT_COUNT,
            "grid_configurations": [list(config) for config in GRID_CONFIGS],
            "integration_substeps_per_slice": list(INTEGRATION_SUBSTEPS),
            "reference_steps": REFERENCE_STEPS,
        },
        "results": results,
        "interpretation_note": "Substeps refine ray integration of an already sampled/interpolated froxel grid; they do not add injection cells or recover density missed during grid construction.",
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Direct reference uses 512 uniform ray steps while froxel integration subdivides log-depth slices.",
            "Hard box density and AABB visibility are analytic proxies, not production textures or shadow maps.",
            "No temporal reprojection, jitter, filtering beyond trilinear grid lookup, or backend parity.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
        "hard_boundary_script_sha256": sha256(Path(hard.__file__)),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    for scenario, configs in results.items():
        print(scenario)
        for name, row in configs.items():
            print(" ", name,
                  "full", row["full_image_metrics_vs_direct_512"]["relative_rgb_l2"],
                  "fog", row["fog_only_metrics_vs_direct_512"]["relative_rgb_l2"])


if __name__ == "__main__":
    main()
