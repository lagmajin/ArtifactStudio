#!/usr/bin/env python3
"""Validate the hard-box fog reference using exact ray/box intervals."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import fog_local_froxel_hard_boundary_rd as hard
import fog_local_froxel_proxy_rd as proxy


OUTPUT = Path("docs/worklog/artifacts/fog-local-froxel-hard-boundary-oracle-rd-2026-11-13")
SCENARIOS = ("centered", "x_shift_half", "depth_shift_half")
ORACLE_STEPS = (8, 16, 32, 64, 128, 256)
FROXEL_CONFIGS = ((2, 32), (4, 32), (8, 32), (4, 64))
LIGHT_COUNT = 3


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def ray_box_interval(origin, direction, box, max_distance):
    near, far = 0.0, max_distance
    for axis in range(3):
        lo, hi = box["minimum"][axis], box["maximum"][axis]
        if abs(direction[axis]) < 1e-12:
            if origin[axis] < lo or origin[axis] > hi:
                return None
            continue
        t0 = (lo - origin[axis]) / direction[axis]
        t1 = (hi - origin[axis]) / direction[axis]
        if t0 > t1:
            t0, t1 = t1, t0
        near = max(near, t0)
        far = min(far, t1)
        if near >= far:
            return None
    return (near, far) if far > near else None


def exact_interval_integral(ray, distance, source_rgb, camera, box, steps):
    interval = ray_box_interval(camera, ray, box, distance)
    if interval is None:
        return source_rgb
    t0, t1 = interval
    density = hard.DENSITY
    ds = (t1 - t0) / steps
    transmittance = 1.0
    scattered = [0.0, 0.0, 0.0]
    for index in range(steps):
        position = proxy.add(camera, proxy.mul(ray, t0 + (index + 0.5) * ds))
        light = proxy.light_source_at(position, LIGHT_COUNT)
        segment_t = __import__("math").exp(-proxy.EXTINCTION * density * ds)
        weight = transmittance * (1.0 - segment_t) * proxy.SCATTERING_ALBEDO
        for channel in range(3):
            scattered[channel] += weight * light[channel]
        transmittance *= segment_t
    return tuple(source_rgb[channel] * transmittance + scattered[channel]
                 for channel in range(3))


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
        camera = proxy.CAMERA
        intervals = [ray_box_interval(camera, rays[i], box, distances[i]) for i in range(len(rays))]
        intersecting = [i for i, interval in enumerate(intervals) if interval is not None]
        print(f"Oracle {scenario}: {len(intersecting)} intersecting rays", flush=True)
        references = {}
        for steps in ORACLE_STEPS:
            references[steps] = [exact_interval_integral(rays[i], distances[i], sources[i],
                                                         camera, box, steps)
                                 for i in range(len(rays))]
        fog_references = {}
        for steps in ORACLE_STEPS:
            fog_references[steps] = [exact_interval_integral(rays[i], distances[i],
                                                             (0.0, 0.0, 0.0), camera, box, steps)
                                     for i in range(len(rays))]
        full_uniform_512 = [proxy.integrate_direct(rays[i], distances[i], sources[i], 512,
                                                   LIGHT_COUNT, camera, (),
                                                   lambda p, selected=box: hard.box_density(p, selected))
                            for i in range(len(rays))]
        rows = {
            "ray_box_intersection_pixel_count": len(intersecting),
            "maximum_ray_interval_length": max((interval[1] - interval[0]
                                                 for interval in intervals if interval), default=0.0),
            "uniform_raymarch_512_vs_exact_interval_256": proxy.image_metrics(references[256],
                                                                              full_uniform_512),
            "oracle_convergence": {},
            "froxel_metrics_vs_exact_interval_256": {},
        }
        for steps in ORACLE_STEPS[:-1]:
            rows["oracle_convergence"][str(steps)] = {
                "full_image": proxy.image_metrics(references[256], references[steps]),
                "fog_only": proxy.image_metrics(fog_references[256], fog_references[steps]),
            }
        for tile, slices in FROXEL_CONFIGS:
            name = f"tile{tile}_z{slices}"
            density_fn = lambda p, selected=box: hard.box_density(p, selected)
            grid = proxy.build_froxel_grid(tile, slices, LIGHT_COUNT, "log", camera, (), density_fn)
            full = [proxy.integrate_froxel(rays[i], distances[i], sources[i], i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid)
                    for i in range(len(rays))]
            fog = [proxy.integrate_froxel(rays[i], distances[i], (0.0, 0.0, 0.0), i % proxy.WIDTH,
                                           i // proxy.WIDTH, tile, slices, grid)
                   for i in range(len(rays))]
            rows["froxel_metrics_vs_exact_interval_256"][name] = {
                "full_image": proxy.image_metrics(references[256], full),
                "fog_only": proxy.image_metrics(fog_references[256], fog),
            }
        results[scenario] = rows

    manifest = {
        "schema": "fog-local-froxel-hard-boundary-oracle-rd-v1",
        "scene": {
            "size": [proxy.WIDTH, proxy.HEIGHT],
            "boxes": {name: hard.BOX_SCENARIOS[name] for name in SCENARIOS},
            "density_inside": hard.DENSITY,
            "light_count": LIGHT_COUNT,
            "exact_ray_box_interval": "slab intersection in ray-distance parameter, clipped to scene surface distance",
            "scattering_quadrature_steps_inside_interval": list(ORACLE_STEPS),
            "froxel_configurations": [list(config) for config in FROXEL_CONFIGS],
        },
        "results": results,
        "oracle_definition": "Density/transmittance interval is exact for a binary box. Point-light scattering integral inside that interval uses midpoint quadrature; 256-step result is compared against 128-step convergence in manifest.",
        "limitations": [
            "CPU synthetic proxy; no GPU shader, runtime, or wall-clock performance measurement.",
            "Exact interval handles the box medium only; point-light source and analytic AABB visibility remain numerically sampled.",
            "The 256-step interval reference is a convergence target, not symbolic closed-form single scattering.",
            "Box uses one hard-edged volume and synthetic scene surfaces.",
        ],
        "source_script_sha256": sha256(Path(__file__)),
        "proxy_script_sha256": sha256(Path(proxy.__file__)),
        "hard_boundary_script_sha256": sha256(Path(hard.__file__)),
    }
    OUTPUT.mkdir(parents=True, exist_ok=True)
    path = OUTPUT / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"Wrote {path}")
    for scenario, row in results.items():
        print(scenario, "uniform512 vs interval256", row["uniform_raymarch_512_vs_exact_interval_256"])
        print(" convergence 128 full", row["oracle_convergence"]["128"]["full_image"],
              "fog", row["oracle_convergence"]["128"]["fog_only"])
        for name, metrics in row["froxel_metrics_vs_exact_interval_256"].items():
            print(" ", name, "fog relative", metrics["fog_only"]["relative_rgb_l2"])


if __name__ == "__main__":
    main()
