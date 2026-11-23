#!/usr/bin/env python3
"""Compare multisample XY injection for hard-edged local fog volumes."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_hard_boundary_rd as hard
import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-injection-sampling-rd-2026-11-13")
TILES = (4, 8)
SLICES = 32
SPATIAL_SAMPLE_COUNTS = (1, 2, 4)
LIGHT_COUNT = 3
REFERENCE_STEPS = 512
BOX_SCENARIOS = ("centered", "x_shift_half", "depth_shift_half")


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
    for scenario in BOX_SCENARIOS:
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
        for tile in TILES:
            for sample_count in SPATIAL_SAMPLE_COUNTS:
                name = f"tile{tile}_z{SLICES}_samples{sample_count}x{sample_count}"
                print(f"  {scenario}: {name}", flush=True)
                grid = proxy.build_froxel_grid(tile, SLICES, LIGHT_COUNT, "log", proxy.CAMERA,
                                               (), density_fn, sample_count)
                pixels = [
                    proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, SLICES, grid, "log")
                    for i in range(len(rays))
                ]
                fog_pixels = [
                    proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, SLICES, grid, "log")
                    for i in range(len(rays))
                ]
                cells = grid[0] * grid[1] * SLICES
                config_results[name] = {
                    "tile_width_pixels": tile,
                    "depth_slices": SLICES,
                    "spatial_samples_per_axis": sample_count,
                    "injection_field_evaluations_per_cell": sample_count * sample_count,
                    "froxel_cell_count": cells,
                    "total_light_evaluations": cells * sample_count * sample_count * LIGHT_COUNT,
                    "estimated_bytes_R16F_density_RGBA16F_source": cells * 10,
                    "full_image_metrics_vs_direct_512": proxy.image_metrics(reference, pixels),
                    "fog_only_metrics_vs_direct_512": proxy.image_metrics(fog_reference, fog_pixels),
                }
        results[scenario] = config_results

    manifest = {
        "schema": "fog-local-froxel-injection-sampling-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "hard_box_scenarios": {name: hard.BOX_SCENARIOS[name] for name in BOX_SCENARIOS},
            "density_inside": hard.DENSITY,
            "tiles": list(TILES),
            "depth_slices": SLICES,
            "light_count": LIGHT_COUNT,
            "spatial_samples_per_axis": list(SPATIAL_SAMPLE_COUNTS),
            "reference_steps": REFERENCE_STEPS,
        },
        "results": results,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Sub-sample averaging reduces injection aliasing but does not increase froxel cell count or add spatial reconstruction support.",
            "Box density is analytic binary membership, not a production volume injection or filtered density texture.",
            "The evaluation count is arithmetic; GPU cache behavior, SIMD occupancy, and divergence are not measured.",
            "No temporal reprojection, jitter, or backend parity.",
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
                  "fog", row["fog_only_metrics_vs_direct_512"]["relative_rgb_l2"],
                  "evals", row["total_light_evaluations"])


if __name__ == "__main__":
    main()
