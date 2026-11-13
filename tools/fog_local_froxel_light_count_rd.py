#!/usr/bin/env python3
"""Sweep point-light counts for the local-fog CPU froxel proxy."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-light-count-rd-2026-11-13")
LIGHT_COUNTS = (1, 2, 3)
REFERENCE_STEPS = 512
VALIDATION_STEPS = 256
DIRECT_STEPS = 32
FROXEL_CONFIGS = ((4, 32), (4, 64), (8, 64))
SHADOW_DENSITY_THRESHOLDS = (0.003, 0.0001, 0.00001)


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
            source_rgb, distance = proxy.scene.scene_sample(ray)
            rays.append(ray)
            distances.append(distance)
            sources.append(source_rgb)

    count_results = {}
    for light_count in LIGHT_COUNTS:
        print(f"Evaluating {light_count} light(s)...", flush=True)
        reference = [proxy.integrate_direct(rays[i], distances[i], sources[i], REFERENCE_STEPS,
                                            light_count)
                     for i in range(len(rays))]
        validation = [proxy.integrate_direct(rays[i], distances[i], sources[i], VALIDATION_STEPS,
                                             light_count)
                      for i in range(len(rays))]
        direct = [proxy.integrate_direct(rays[i], distances[i], sources[i], DIRECT_STEPS,
                                         light_count)
                  for i in range(len(rays))]
        captures = {
            "validation_direct_256": validation,
            "direct_32": direct,
        }
        config_rows = []
        for tile, slices in FROXEL_CONFIGS:
            key = f"tile{tile}_z{slices}"
            grid = proxy.build_froxel_grid(tile, slices, light_count)
            captures[f"froxel_{key}"] = [
                proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                       i // proxy.WIDTH, tile, slices, grid)
                for i in range(len(rays))
            ]
            cells = grid[0] * grid[1] * slices
            config_rows.append({
                "name": key,
                "tile_size": tile,
                "depth_slices": slices,
                "froxel_cell_count": cells,
                "light_evaluations": cells * light_count,
                "estimated_bytes_R16F_density_RGBA16F_source": cells * 10,
            })

        shadow_masks = {}
        for threshold in SHADOW_DENSITY_THRESHOLDS:
            mask = []
            for ray, distance in zip(rays, distances):
                affected = False
                if ray[2] > 1e-8 and distance > 0.0:
                    ds = distance / 64
                    for probe in range(64):
                        position = proxy.add(proxy.CAMERA, proxy.mul(ray, (probe + 0.5) * ds))
                        if proxy.density_at(position) < threshold:
                            continue
                        if any(proxy.segment_hits_box(position, light[0], box)
                               for light in proxy.LIGHTS[:light_count]
                               for box in proxy.OCCLUDERS):
                            affected = True
                            break
                mask.append(affected)
            shadow_masks[f"density_ge_{threshold:g}"] = [i for i, flag in enumerate(mask) if flag]
        count_results[str(light_count)] = {
            "reference_validation_direct_256_vs_512": proxy.image_metrics(reference, validation),
            "shadow_masks": {
                name: {
                    "pixel_count": len(indices),
                    "metrics_vs_direct_512": {
                        capture_name: proxy.image_metrics([reference[i] for i in indices],
                                                          [pixels[i] for i in indices])
                        for capture_name, pixels in captures.items()
                    },
                }
                for name, indices in shadow_masks.items()
            },
            "configurations": config_rows,
            "metrics_vs_direct_512": {
                name: proxy.image_metrics(reference, pixels)
                for name, pixels in captures.items()
            },
            "light_evaluation_arithmetic": {
                "direct_32": len(rays) * DIRECT_STEPS * light_count,
                "direct_256_validation": len(rays) * VALIDATION_STEPS * light_count,
                "direct_512_reference": len(rays) * REFERENCE_STEPS * light_count,
            },
        }

    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "schema": "fog-local-froxel-light-count-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "light_counts": list(LIGHT_COUNTS),
            "light_prefix_order": [list(light[0]) for light in proxy.LIGHTS],
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "direct_candidate_steps": DIRECT_STEPS,
            "froxel_configurations": [list(config) for config in FROXEL_CONFIGS],
        },
        "results": count_results,
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Light counts use the first N fixed lights; this does not represent arbitrary light selection.",
            "Density threshold affects shadow-mask pixel counts and prevents direct comparison of mask-region metrics across thresholds.",
            "Shadow mask is an analytic AABB probe, not shadow-map filtering or volumetric shadowing.",
            "Memory estimates are arithmetic scenarios, not a production resource layout.",
            "No temporal reprojection, anisotropic scattering, or backend parity.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
    }
    manifest_path = OUTPUT / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {manifest_path}")
    for light_count, result in count_results.items():
        print("lights", light_count,
              "validation", result["reference_validation_direct_256_vs_512"],
              "shadow masks", {name: data["pixel_count"]
                               for name, data in result["shadow_masks"].items()})
        for name, metrics in result["metrics_vs_direct_512"].items():
            print(" ", name, metrics["relative_rgb_l2"])


if __name__ == "__main__":
    main()
