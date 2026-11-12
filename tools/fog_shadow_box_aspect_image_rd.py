#!/usr/bin/env python3
"""Render volume-preserving AABB shadow shapes into a deterministic fog image."""

from __future__ import annotations

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


WIDTH = 96
HEIGHT = 54
OUTPUT_DIR = Path("docs/worklog/artifacts/fog-shadow-box-aspect-image-rd-2026-11-12")
REFERENCE_STEPS = 4096
VALIDATION_STEPS = 2048
CANDIDATE_STEPS = 128
CROP_BOX = (8, 6, 80, 42)
SHAPES = geometry.SHAPES


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def region_metrics(candidate, reference, indices):
    c = [candidate[i] for i in indices]
    r = [reference[i] for i in indices]
    if not indices:
        return {"pixel_count": 0, "relative_rgb_l2": None, "comparison": None}
    compare = image.scene.compare_pixels(r, c)
    deltas = sorted(max(abs(c[i][channel] - r[i][channel]) for channel in range(3))
                    for i in range(len(indices)))
    compare["p95_absolute_rgb"] = deltas[math.ceil(0.95 * len(deltas)) - 1]
    return {"pixel_count": len(indices),
            "relative_rgb_l2": image.image_relative_l2(c, r),
            "comparison": compare}


def edge_mask(mask: list[bool]) -> set[int]:
    result = set()
    for index, value in enumerate(mask):
        x, y = index % WIDTH, index // WIDTH
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < WIDTH and 0 <= ny < HEIGHT:
                if mask[ny * WIDTH + nx] != value:
                    result.add(index)
                    result.add(ny * WIDTH + nx)
    return result


def pixel_bounds(indices: list[int]):
    if not indices:
        return None
    xs = [i % WIDTH for i in indices]
    ys = [i // WIDTH for i in indices]
    return [min(xs), min(ys), max(xs), max(ys)]


def main() -> None:
    world.WIDTH, world.HEIGHT = WIDTH, HEIGHT
    image.scene.WIDTH, image.scene.HEIGHT = WIDTH, HEIGHT
    image.WIDTH, image.HEIGHT = WIDTH, HEIGHT
    image.OUTPUT_DIR = OUTPUT_DIR
    origin, right, up, forward = sequence.camera_frame(0.0)
    sun_el = math.radians(45.0)
    sun_az = math.radians(scattering.SUN_AZIMUTH_DEGREES)
    sun = scattering.normalize((math.cos(sun_el) * math.sin(sun_az), math.sin(sun_el),
                                math.cos(sun_el) * math.cos(sun_az)))
    center = (0.0, 3.5, 8.25)

    rays, sources, distances = [], [], []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = world.ray_for_pixel(x, y, right, up, forward)
            color, distance, _surface = world.scene_sample(origin, ray)
            rays.append(ray)
            sources.append(color)
            distances.append(distance)

    references, validations, candidates = {}, {}, {}
    masks, geometry_summary = {}, {}
    validation_by_shape = {}
    for shape_name, half_extents in SHAPES.items():
        shape_mask, intervals = [], []
        ref_pixels, val_pixels, candidate_pixels = [], [], []
        for ray, source, distance in zip(rays, sources, distances):
            hit = geometry.box_interval(origin, ray, distance, center, half_extents)
            truth = hit["intervals"] if hit is not None else ()
            shape_mask.append(bool(truth))
            intervals.append(truth)
            case = {"camera_y": origin[1], "view_direction": ray,
                    "distance": distance, "sun_direction": sun}
            ref_cells = soft.reference_cells(case, truth, image.PENUMBRA_WIDTH,
                                             REFERENCE_STEPS)
            val_cells = soft.reference_cells(case, truth, image.PENUMBRA_WIDTH,
                                             VALIDATION_STEPS)
            cells = soft.weighted_cells(origin[1], ray[1], distance, CANDIDATE_STEPS,
                                        truth, image.PENUMBRA_WIDTH)
            # Compose with the same source and exact exponential view transmittance.
            ref_pixels.append(image.compose(case, source, ref_cells, truth))
            val_pixels.append(image.compose(case, source, val_cells, truth))
            candidate_pixels.append(image.compose(case, source, cells, truth))

        shape_edge = edge_mask(shape_mask)
        inside = [i for i, value in enumerate(shape_mask) if value]
        masks[shape_name] = {"inside": set(inside), "edge": shape_edge}
        references[shape_name] = ref_pixels
        validations[shape_name] = val_pixels
        candidates[shape_name] = candidate_pixels
        validation_by_shape[shape_name] = image.image_relative_l2(val_pixels, ref_pixels)
        geometry_summary[shape_name] = {
            "half_extents": half_extents,
            "volume": 8.0 * math.prod(half_extents),
            "projected_shadow_pixels": len(inside),
            "projected_shadow_bounds_xy_inclusive": pixel_bounds(inside),
            "edge_band_pixels": len(shape_edge),
        }
        print("finished", shape_name, "shadow pixels", len(inside),
              "validation", validation_by_shape[shape_name])

    baseline_mask = [i in masks["baseline"]["inside"] for i in range(WIDTH * HEIGHT)]
    regions_by_shape = {}
    for shape_name in SHAPES:
        current_mask = [i in masks[shape_name]["inside"] for i in range(WIDTH * HEIGHT)]
        xor = {i for i, (a, b) in enumerate(zip(baseline_mask, current_mask)) if a != b}
        edges = masks["baseline"]["edge"] | masks[shape_name]["edge"]
        regions_by_shape[shape_name] = {
            "all": list(range(WIDTH * HEIGHT)),
            "shape_shadow_interior": sorted(masks[shape_name]["inside"]),
            "baseline_shape_silhouette_xor": sorted(xor),
            "baseline_shape_edge_union": sorted(edges),
        }

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    captures = {"source": sources}
    for shape_name in SHAPES:
        captures[f"{shape_name}_reference_4096"] = references[shape_name]
        captures[f"{shape_name}_candidate_128"] = candidates[shape_name]
    capture_files = {name: image.save_capture(name, pixels)
                     for name, pixels in captures.items()}

    metrics = {"candidate_quadrature_error": {}, "shape_geometry_difference_vs_baseline": {}}
    diffs = {"candidate_minus_shape_reference": {},
             "shape_reference_minus_baseline_reference": {}}
    crops = {}
    for shape_name in SHAPES:
        regions = regions_by_shape[shape_name]
        metrics["candidate_quadrature_error"][shape_name] = {
            region: region_metrics(candidates[shape_name], references[shape_name], indices)
            for region, indices in regions.items()
        }
        diffs["candidate_minus_shape_reference"][shape_name] = image.write_difference(
            f"{shape_name}_candidate_minus_reference", candidates[shape_name],
            references[shape_name],
        )
        crops[f"{shape_name}_reference"] = image.save_crop(
            f"{shape_name}_reference_center_crop", references[shape_name], CROP_BOX)
        if shape_name != "baseline":
            metrics["shape_geometry_difference_vs_baseline"][shape_name] = {
                region: region_metrics(references[shape_name], references["baseline"], indices)
                for region, indices in regions.items()
            }
            diffs["shape_reference_minus_baseline_reference"][shape_name] = image.write_difference(
                f"{shape_name}_reference_minus_baseline_reference",
                references[shape_name], references["baseline"],
            )

    input_digest = hashlib.sha256(json.dumps({
        "size": [WIDTH, HEIGHT], "camera_origin": origin, "camera_basis": [right, up, forward],
        "fov_degrees": world.FOV_DEGREES, "source": sources, "distance": distances,
        "rays": rays, "box_center": center, "shapes": SHAPES,
    }, separators=(",", ":")).encode("utf-8")).hexdigest()
    source_paths = [Path(__file__), Path(world.__file__), Path(geometry.__file__),
                    Path(sequence.__file__), Path(image.__file__), Path(soft.__file__),
                    Path(scattering.__file__)]
    manifest = {
        "schema": "fog-shadow-box-aspect-image-rd-v1",
        "execution_scope": "CPU single-scattering scene proxy with true AABB ray intersections; no product renderer or GPU runtime.",
        "pre_registered_evaluation_plan": {
            "scene": "fog_height_camera_scene_rd.py sampled at 96x54 with unchanged colors, ray distances and scene parameters.",
            "camera_x": 0.0,
            "camera_origin": origin,
            "target": sequence.TARGET,
            "sun_elevation_degrees": 45.0,
            "box_center": center,
            "shapes": SHAPES,
            "volume_each": 33.0,
            "penumbra_ray_length_fraction": image.PENUMBRA_WIDTH,
            "reference_steps": REFERENCE_STEPS,
            "validation_steps": VALIDATION_STEPS,
            "candidate_steps": CANDIDATE_STEPS,
            "candidate": "Exact true AABB interval for both profile knots/weights and visibility; no center or width estimation error.",
            "regions": "all image pixels, shape shadow interior, baseline/shape silhouette XOR and edge-band union.",
            "statistics": "Scene-linear relative RGB L2, RMSE, mean absolute RGB, p95/max max-channel delta, and count above 1e-3.",
            "crop_xywh": list(CROP_BOX),
            "gate": "No shape quality threshold; reference-validation is a numerical integration validity check only.",
        },
        "size": [WIDTH, HEIGHT],
        "alpha": "Opaque scene fixture; alpha is not represented.",
        "color": "Scene-linear float32 RGB; shared Reinhard plus gamma 1/2.2 PNG previews.",
        "input_fixture_sha256": input_digest,
        "geometry_by_shape": geometry_summary,
        "maximum_reference_validation_relative_rgb_l2_by_shape": validation_by_shape,
        "maximum_reference_validation_relative_rgb_l2": max(validation_by_shape.values()),
        "region_pixel_counts": {shape: {region: len(indices)
                                         for region, indices in regions.items()}
                                for shape, regions in regions_by_shape.items()},
        "source_captures": capture_files,
        "metrics": metrics,
        "difference_captures": diffs,
        "center_crops": crops,
        "source_scripts": [{"path": path.as_posix(), "sha256": file_hash(path)}
                           for path in source_paths],
        "unverified": ["human production-quality approval", "alpha edges", "temporal stability",
                       "real shadow map filtering", "GPU/backend parity", "preview/final parity",
                       "product host integration", "processing time and memory"],
        "limitations": [
            "The AABB intersection is geometrically ray-cast, but the 96x54 surface chart and CPU single-scattering path remain synthetic proxies.",
            "Only one camera pose, one sun elevation, one medium, and three fixed shapes are compared.",
            "Shape-to-baseline image difference measures changed projected occlusion and scattering, not a perceptual quality ranking.",
        ],
    }
    manifest_path = OUTPUT_DIR / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("maximum reference validation", manifest["maximum_reference_validation_relative_rgb_l2"])
    print(manifest_path.as_posix())


if __name__ == "__main__":
    main()
