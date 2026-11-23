#!/usr/bin/env python3
"""Render deterministic scene-linear fog comparison fixtures without product code."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import zlib
from pathlib import Path

from fog_height_integral_rd import exact_density_integral, marched_density_integral


WIDTH = 256
HEIGHT = 144
CAMERA = (0.0, 4.0, 0.0)
GROUND = 0.0
RHO0 = 0.008
FALLOFF = 0.08
ABSORPTION = 0.7
SCATTERING = 0.65
FOG_COLOR = (0.46, 0.59, 0.72)
SUN_TINT = (1.0, 0.82, 0.58)
SUN_INTENSITY = 38.0
SUN_ELEVATION_DEGREES = 32.0
RAY_LIMIT = 120.0
ATMOSPHERE_TOP = 120.0
FOV_DEGREES = 58.0
CPU_DEFAULT_DENSITY = 0.1
CPU_DEFAULT_HEIGHT_FALLOFF = 0.5
CPU_DEFAULT_ABSORPTION = 0.1
CPU_DEFAULT_FOG_COLOR = (0.5, 0.6, 0.7)
CPU_DEFAULT_HORIZON_COLOR = (0.67, 0.53, 0.36)
GPU_HEIGHT_CAMERA_SWEEP = (0.0, 4.0, 20.0, 100.0)
GPU_HEIGHT_START_DISTANCE = 0.0


def dot(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def normalize(v: tuple[float, float, float]) -> tuple[float, float, float]:
    inv_len = 1.0 / math.sqrt(max(dot(v, v), 1e-20))
    return (v[0] * inv_len, v[1] * inv_len, v[2] * inv_len)


def ray_for_pixel(x: int, y: int) -> tuple[float, float, float]:
    aspect = WIDTH / HEIGHT
    half_height = math.tan(math.radians(FOV_DEGREES) * 0.5)
    ndc_x = 2.0 * (x + 0.5) / WIDTH - 1.0
    ndc_y = 1.0 - 2.0 * (y + 0.5) / HEIGHT
    return normalize((ndc_x * aspect * half_height, ndc_y * half_height, 1.0))


def panel_color(x: float, y: float, far_panel: bool) -> tuple[float, float, float]:
    palette = (
        (0.9, 0.08, 0.035),
        (0.025, 0.35, 0.95),
        (0.95, 0.52, 0.025),
        (0.08, 0.72, 0.22),
        (0.6, 0.12, 0.8),
        (0.72, 0.76, 0.8),
    )
    cell_x = math.floor((x + (10.0 if far_panel else 4.0)) * (0.72 if far_panel else 1.15))
    cell_y = math.floor(y * 1.1)
    color = palette[(cell_x + 2 * cell_y + (3 if far_panel else 0)) % len(palette)]
    seam = abs(((x + 0.13) * 7.0) % 1.0 - 0.5) < 0.025
    if seam or abs((y * 5.0) % 1.0 - 0.5) < 0.025:
        color = (0.018, 0.022, 0.028)
    if 1.1 < y < 1.42 and -0.55 < x < 0.55:
        color = (5.0, 3.2, 1.4)  # HDR marker for highlight attenuation.
    if y > 4.2 and abs(x) < 0.14:
        color = (2.4, 2.4, 2.4)  # narrow bright line.
    return color


def scene_sample(ray: tuple[float, float, float]) -> tuple[tuple[float, float, float], float]:
    candidates: list[tuple[float, tuple[float, float, float]]] = []
    dx, dy, dz = ray
    ox, oy, oz = CAMERA

    # Near chart: a vertical plane with a slight depth slope.
    denom = dz - 0.12 * dx
    if abs(denom) > 1e-8:
        distance = (12.0 - oz) / denom
        if distance > 0.0:
            px = ox + distance * dx
            py = oy + distance * dy
            pz = oz + distance * dz
            if -5.0 <= px <= 5.0 and 0.0 <= py <= 6.0 and abs(pz - (12.0 + 0.12 * px)) < 1e-4:
                candidates.append((distance, panel_color(px, py, False)))

    # Far chart: opposite slope creates a visible depth discontinuity.
    denom = dz + 0.20 * dx
    if abs(denom) > 1e-8:
        distance = (28.0 - oz) / denom
        if distance > 0.0:
            px = ox + distance * dx
            py = oy + distance * dy
            pz = oz + distance * dz
            if -12.0 <= px <= 12.0 and 0.0 <= py <= 10.0 and abs(pz - (28.0 - 0.20 * px)) < 1e-4:
                candidates.append((distance, panel_color(px, py, True)))

    # Checker ground provides a low-height reference and strong horizon detail.
    if dy < -1e-8:
        distance = (GROUND - oy) / dy
        if 0.0 < distance <= RAY_LIMIT:
            px = ox + distance * dx
            pz = oz + distance * dz
            checker = (math.floor(px * 0.5) + math.floor(pz * 0.5)) & 1
            stripe = abs((pz * 0.16) % 1.0 - 0.5) < 0.018
            if stripe:
                color = (1.8, 1.5, 0.9)
            elif checker:
                color = (0.19, 0.22, 0.25)
            else:
                color = (0.055, 0.075, 0.095)
            candidates.append((distance, color))

    if candidates:
        return min(candidates, key=lambda item: item[0])[1], min(min(candidates, key=lambda item: item[0])[0], RAY_LIMIT)

    # Sky is a finite atmosphere segment, with a gradient source color.
    if dy > 1e-8:
        distance = min((ATMOSPHERE_TOP - oy) / dy, RAY_LIMIT)
    else:
        distance = RAY_LIMIT
    horizon = max(0.0, min(1.0, (ray[1] + 0.18) / 0.72))
    sky = (
        0.16 + 0.25 * horizon,
        0.24 + 0.34 * horizon,
        0.36 + 0.48 * horizon,
    )
    return sky, distance


def rayleigh_phase(cos_theta: float) -> float:
    return 3.0 * (1.0 + cos_theta * cos_theta) / (16.0 * math.pi)


def sun_transmittance(sample_height: float, sun_y: float) -> float:
    if sun_y <= 0.0 or sample_height >= ATMOSPHERE_TOP:
        return 0.0
    distance = (ATMOSPHERE_TOP - sample_height) / sun_y
    integral = exact_density_integral(sample_height, sun_y, distance, FALLOFF, GROUND)
    return math.exp(-RHO0 * ABSORPTION * integral)


def fog_uniform(source: tuple[float, float, float], ray: tuple[float, float, float], distance: float) -> tuple[float, float, float]:
    transmittance = math.exp(-RHO0 * ABSORPTION * distance)
    return tuple(source[c] * transmittance + FOG_COLOR[c] * (1.0 - transmittance) for c in range(3))


def fog_height_analytic(source: tuple[float, float, float], ray: tuple[float, float, float], distance: float) -> tuple[float, float, float]:
    integral = exact_density_integral(CAMERA[1], ray[1], distance, FALLOFF, GROUND)
    transmittance = math.exp(-RHO0 * ABSORPTION * integral)
    return tuple(source[c] * transmittance + FOG_COLOR[c] * (1.0 - transmittance) for c in range(3))


def fog_height_marched(source: tuple[float, float, float], ray: tuple[float, float, float], distance: float, steps: int) -> tuple[float, float, float]:
    integral = marched_density_integral(CAMERA[1], ray[1], distance, steps, FALLOFF, GROUND)
    transmittance = math.exp(-RHO0 * ABSORPTION * integral)
    return tuple(source[c] * transmittance + FOG_COLOR[c] * (1.0 - transmittance) for c in range(3))


def fog_gpu_height_shader_mirror(
    source: tuple[float, float, float], ray: tuple[float, float, float], distance: float,
    camera_height: float, density: float = RHO0, falloff: float = FALLOFF,
    start_distance: float = GPU_HEIGHT_START_DISTANCE,
) -> tuple[float, float, float]:
    """Mirror heightFogCS.hlsl's camera-height approximation, not GPU execution."""
    normalized_depth = distance / RAY_LIMIT
    shader_distance = normalized_depth * RAY_LIMIT  # nearPlane=0, farPlane=RAY_LIMIT
    distance_factor = max(0.0, min(1.0, (shader_distance - start_distance) /
                                    max(RAY_LIMIT - start_distance, 0.001)))
    height_density = density * math.exp(-max(camera_height - GROUND, 0.0) * falloff)
    transmittance = math.exp(-height_density * distance_factor * max(shader_distance, 0.0))
    fog_amount = 1.0 - max(0.0, min(1.0, transmittance))
    return tuple(source[c] * (1.0 - fog_amount) + FOG_COLOR[c] * fog_amount for c in range(3))


def fog_gpu_height_world_reference(
    source: tuple[float, float, float], ray: tuple[float, float, float], distance: float,
    camera_height: float, density: float = RHO0, falloff: float = FALLOFF,
) -> tuple[float, float, float]:
    """Exact exponential-height extinction with the GPU prototype's unit absorption."""
    integral = exact_density_integral(camera_height, ray[1], distance, falloff, GROUND)
    transmittance = math.exp(-density * integral)
    return tuple(source[c] * transmittance + FOG_COLOR[c] * (1.0 - transmittance) for c in range(3))


def fog_gpu_camera_density_linear_path(
    source: tuple[float, float, float], distance: float, camera_height: float,
) -> tuple[float, float, float]:
    """Isolate camera-height-only density while retaining a linear path integral."""
    density = RHO0 * math.exp(-max(camera_height - GROUND, 0.0) * FALLOFF)
    transmittance = math.exp(-density * max(distance, 0.0))
    amount = 1.0 - transmittance
    return tuple(source[c] * transmittance + FOG_COLOR[c] * amount for c in range(3))


def byte_encode(value: float) -> int:
    return int(max(0.0, min(1.0, value)) * 255.0 + 0.5)


def byte_decode(value: int) -> float:
    return value / 255.0


def fog_l8_source_formula_mirror(
    source: tuple[float, float, float], ray: tuple[float, float, float], distance: float,
    height_aware: bool,
) -> tuple[float, float, float]:
    """Mirror source equations and RGB8 IO; this does not call the C++ implementation."""
    color_bytes = [byte_encode(channel) for channel in source]
    depth_byte = byte_encode(distance / RAY_LIMIT)
    quantized_color = [byte_decode(channel) for channel in color_bytes]
    quantized_distance = byte_decode(depth_byte) * RAY_LIMIT
    effective_density = CPU_DEFAULT_DENSITY * CPU_DEFAULT_HEIGHT_FALLOFF
    if height_aware:
        density_integral = exact_density_integral(
            CAMERA[1], ray[1], quantized_distance, CPU_DEFAULT_HEIGHT_FALLOFF, GROUND)
        transmittance = math.exp(-CPU_DEFAULT_DENSITY * CPU_DEFAULT_ABSORPTION * density_integral)
    else:
        # Source: opticalDepth = (density * heightFalloff) * tMax * absorption.
        transmittance = math.exp(-effective_density * CPU_DEFAULT_ABSORPTION * quantized_distance)
    fog_color = tuple((CPU_DEFAULT_FOG_COLOR[c] + CPU_DEFAULT_HORIZON_COLOR[c]) * 0.5 for c in range(3))
    output = [quantized_color[c] * transmittance + fog_color[c] * (1.0 - transmittance) for c in range(3)]
    # Source casts the clamped `value * 255.999f` to uint8 (truncation).
    output_bytes = [int(max(0.0, min(255.0, channel * 255.999))) for channel in output]
    return tuple(byte_decode(channel) for channel in output_bytes)


def fog_single_scattering(source: tuple[float, float, float], ray: tuple[float, float, float], distance: float, steps: int) -> tuple[float, float, float]:
    sun_elevation = math.radians(SUN_ELEVATION_DEGREES)
    sun_dir = (math.cos(sun_elevation), math.sin(sun_elevation), 0.0)
    phase = rayleigh_phase(dot(tuple(-v for v in ray), sun_dir))
    ds = distance / steps
    view_t = 1.0
    scatter = [0.0, 0.0, 0.0]
    for i in range(steps):
        t = (i + 0.5) * ds
        y = CAMERA[1] + ray[1] * t
        rho = RHO0 * math.exp(-FALLOFF * max(y - GROUND, 0.0))
        sun_t = sun_transmittance(y, sun_dir[1])
        scale = view_t * rho * SCATTERING * phase * SUN_INTENSITY * sun_t * ds
        for c in range(3):
            scatter[c] += scale * SUN_TINT[c]
        view_t *= math.exp(-rho * ABSORPTION * ds)
    return tuple(source[c] * view_t + scatter[c] for c in range(3))


def pack_pfm(path: Path, pixels: list[tuple[float, float, float]]) -> None:
    path.write_bytes(
        f"PF\n{WIDTH} {HEIGHT}\n-1.0\n".encode("ascii")
        + b"".join(struct.pack("<3f", *pixels[y * WIDTH + x]) for y in reversed(range(HEIGHT)) for x in range(WIDTH))
    )


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    data = kind + payload
    return struct.pack(">I", len(payload)) + data + struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)


def write_preview_png(path: Path, pixels: list[tuple[float, float, float]]) -> None:
    scanlines = bytearray()
    for y in range(HEIGHT):
        scanlines.append(0)  # PNG filter: None
        for x in range(WIDTH):
            rgb = pixels[y * WIDTH + x]
            for channel in rgb:
                mapped = max(0.0, channel) / (1.0 + max(0.0, channel))
                encoded = max(0.0, min(1.0, mapped ** (1.0 / 2.2)))
                scanlines.append(int(encoded * 255.0 + 0.5))
    header = struct.pack(">2I5B", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header)
    png += png_chunk(b"IDAT", zlib.compress(bytes(scanlines), level=9)) + png_chunk(b"IEND", b"")
    path.write_bytes(png)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compare_pixels(reference: list[tuple[float, float, float]], candidate: list[tuple[float, float, float]]) -> dict[str, object]:
    absolute_sum = [0.0, 0.0, 0.0]
    maximum = [0.0, 0.0, 0.0]
    changed_pixels = 0
    squared_sum = 0.0
    sample_count = len(reference) * 3
    for ref, value in zip(reference, candidate):
        pixel_max = 0.0
        for channel in range(3):
            delta = abs(value[channel] - ref[channel])
            absolute_sum[channel] += delta
            maximum[channel] = max(maximum[channel], delta)
            pixel_max = max(pixel_max, delta)
            squared_sum += delta * delta
        if pixel_max > 1e-3:
            changed_pixels += 1
    return {
        "mean_absolute_rgb": [value / len(reference) for value in absolute_sum],
        "max_absolute_rgb": maximum,
        "rgb_rmse": math.sqrt(squared_sum / sample_count),
        "pixels_max_channel_delta_over_1e-3": changed_pixels,
        "pixel_count": len(reference),
    }


def render_fixture(output_dir: Path) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    source_pixels: list[tuple[float, float, float]] = []
    depth_pixels: list[tuple[float, float, float]] = []
    ray_data: list[tuple[tuple[float, float, float], float]] = []
    for y in range(HEIGHT):
        for x in range(WIDTH):
            ray = ray_for_pixel(x, y)
            source, distance = scene_sample(ray)
            source_pixels.append(source)
            ray_data.append((ray, distance))
            normalized_distance = distance / RAY_LIMIT
            depth_pixels.append((normalized_distance, normalized_distance, normalized_distance))

    candidates: dict[str, list[tuple[float, float, float]]] = {
        "f0_no_fog": list(source_pixels),
        "f1_uniform": [],
        "f2_height_analytic": [],
        "f3_height_midpoint_16": [],
        "f3_height_midpoint_64": [],
        "f4_single_scattering_64": [],
        "gpu_height_camera_approximation_mirror": [],
        "cpu_applytopixel_source_mirror": [],
        "cpu_settings_height_analytic_rgb8": [],
    }
    for source, (ray, distance) in zip(source_pixels, ray_data):
        candidates["f1_uniform"].append(fog_uniform(source, ray, distance))
        candidates["f2_height_analytic"].append(fog_height_analytic(source, ray, distance))
        candidates["f3_height_midpoint_16"].append(fog_height_marched(source, ray, distance, 16))
        candidates["f3_height_midpoint_64"].append(fog_height_marched(source, ray, distance, 64))
        candidates["f4_single_scattering_64"].append(fog_single_scattering(source, ray, distance, 64))
        candidates["gpu_height_camera_approximation_mirror"].append(
            fog_gpu_height_shader_mirror(source, ray, distance, CAMERA[1]))
        candidates["cpu_applytopixel_source_mirror"].append(
            fog_l8_source_formula_mirror(source, ray, distance, height_aware=False))
        candidates["cpu_settings_height_analytic_rgb8"].append(
            fog_l8_source_formula_mirror(source, ray, distance, height_aware=True))

    entries: dict[str, dict[str, str]] = {}
    for name, pixels in candidates.items():
        pfm_path = output_dir / f"{name}.pfm"
        png_path = output_dir / f"{name}_preview.png"
        pack_pfm(pfm_path, pixels)
        write_preview_png(png_path, pixels)
        entries[name] = {"pfm": pfm_path.name, "pfm_sha256": file_sha256(pfm_path),
                         "preview_png": png_path.name, "preview_sha256": file_sha256(png_path)}

    analytic_reference = candidates["f2_height_analytic"]
    deltas_from_analytic = {
        name: compare_pixels(analytic_reference, pixels)
        for name, pixels in candidates.items()
        if name != "f2_height_analytic" and not name.endswith("_8bit_same_io")
        and name != "cpu_applytopixel_source_mirror"
    }

    depth_path = output_dir / "depth_normalized_ray_distance.pfm"
    pack_pfm(depth_path, depth_pixels)
    gpu_camera_height_sweep: dict[str, object] = {}
    for camera_height in GPU_HEIGHT_CAMERA_SWEEP:
        shader_pixels = [fog_gpu_height_shader_mirror(source, ray, distance, camera_height)
                         for source, (ray, distance) in zip(source_pixels, ray_data)]
        camera_density_pixels = [fog_gpu_camera_density_linear_path(source, distance, camera_height)
                                 for source, (ray, distance) in zip(source_pixels, ray_data)]
        world_height_pixels = [fog_gpu_height_world_reference(source, ray, distance, camera_height)
                               for source, (ray, distance) in zip(source_pixels, ray_data)]
        gpu_camera_height_sweep[f"{camera_height:g}"] = {
            "shader_mirror_vs_world_height_analytic": compare_pixels(world_height_pixels, shader_pixels),
            "camera_height_only_linear_path_vs_world_height_analytic": compare_pixels(
                world_height_pixels, camera_density_pixels),
            "shader_mirror_vs_current_height_analytic": compare_pixels(
                candidates["f2_height_analytic"], shader_pixels),
        }
    gpu_density_falloff_sweep: dict[str, object] = {}
    for density in (0.002, 0.008, 0.02):
        for falloff in (0.02, 0.08, 0.2):
            shader_pixels = [fog_gpu_height_shader_mirror(
                source, ray, distance, CAMERA[1], density, falloff)
                for source, (ray, distance) in zip(source_pixels, ray_data)]
            world_height_pixels = [fog_gpu_height_world_reference(
                source, ray, distance, CAMERA[1], density, falloff)
                for source, (ray, distance) in zip(source_pixels, ray_data)]
            key = f"density={density:g},falloff={falloff:g}"
            gpu_density_falloff_sweep[key] = compare_pixels(world_height_pixels, shader_pixels)
    manifest = {
        "schema": "fog-atmosphere-rd-fixture-v1",
        "size": [WIDTH, HEIGHT],
        "projection": {"type": "perspective", "vertical_fov_degrees": FOV_DEGREES,
                       "camera_position": list(CAMERA), "view_direction": [0.0, 0.0, 1.0]},
        "depth": {"encoding": "euclidean_ray_distance / ray_limit", "ray_limit": RAY_LIMIT,
                  "file": depth_path.name, "sha256": file_sha256(depth_path),
                  "note": "Synthetic normalized linear distance; not hardware perspective depth."},
        "color": {"format": "PFM RGB float32 little-endian", "transfer": "linear scene-referred",
                  "range": "unclamped; HDR markers exceed 1", "alpha": "opaque; not represented",
                  "cpu_reference_io": "ArtifactCore ImageBuffer is RGB8; alpha is absent"},
        "scene": {"geometry": "sloped near/far color charts, checker ground, finite sky", "seed": None},
        "medium": {"ground": GROUND, "density_at_ground": RHO0, "height_falloff": FALLOFF,
                   "absorption": ABSORPTION, "scattering": SCATTERING, "fog_color": FOG_COLOR,
                   "sun_tint": SUN_TINT, "sun_intensity": SUN_INTENSITY,
                   "sun_elevation_degrees": SUN_ELEVATION_DEGREES, "atmosphere_top": ATMOSPHERE_TOP},
        "candidates": entries,
        "linear_rgb_comparison_to_f2_height_analytic": deltas_from_analytic,
        "gpu_height_shader_equation_mirror": {
            "source_file": "Artifact/shaders/heightFogCS.hlsl",
            "status": "Equation-level Python mirror only; shader was not compiled or dispatched.",
            "contract": {"depth": "normalized linear ray distance / 120", "near": 0.0,
                         "far": RAY_LIMIT, "start_distance": GPU_HEIGHT_START_DISTANCE,
                         "source_equation_distance_factor": "distance / far when start_distance=0; the shader multiplies this by distance again in opticalDepth",
                         "camera_height": "constant for all pixels", "world_position_reconstruction": False},
            "camera_height_sweep": gpu_camera_height_sweep,
            "density_falloff_sweep_at_camera_height_4": gpu_density_falloff_sweep,
            "interpretation": "Compares the source shader equation (including its distanceFactor * distance term) and an isolated camera-height-only linear path against analytic world-height extinction on the same fixed depth/color fixture; the sweep holds scene depth/color fixed.",
        },
        "cpu_source_formula_mirror": {
            "source_files": ["ArtifactCore/src/Render/AtmosphereFog.cpp::applyToImage",
                             "ArtifactCore/src/Render/AtmosphereFog.cpp::applyToPixel"],
            "status": "Equation-level Python mirror only; C++ implementation was not built or called.",
            "io": {"color": "RGB8", "depth": "RGB8 normalized linear distance", "near": 0.0,
                   "far": RAY_LIMIT, "max_distance": RAY_LIMIT, "alpha": "not represented"},
            "settings": {"density": CPU_DEFAULT_DENSITY,
                         "height_falloff": CPU_DEFAULT_HEIGHT_FALLOFF,
                         "absorption": CPU_DEFAULT_ABSORPTION,
                         "max_distance": RAY_LIMIT,
                         "fog_color": CPU_DEFAULT_FOG_COLOR,
                         "horizon_color": CPU_DEFAULT_HORIZON_COLOR},
            "hdr_source_pixels_clipped_by_rgb8_input": sum(1 for pixel in source_pixels if any(channel > 1.0 for channel in pixel)),
            "source_formula_vs_height_analytic_same_io": compare_pixels(
                candidates["cpu_settings_height_analytic_rgb8"],
                candidates["cpu_applytopixel_source_mirror"]),
            "interpretation": "applyToPixel uses uniform density * heightFalloff, while the paired Beer-Lambert height reference uses density * exp(-heightFalloff * height). Same authoring settings and RGB8 IO; the reference does not reproduce evaluateFog in-scattering. ImageBuffer has no alpha channel.",
        },
        "preview_transform": "Reinhard channel mapping followed by gamma 1/2.2; PFM remains linear unclamped.",
        "limitations": ["Synthetic scene and arbitrary world units.",
                        "F4 is single scattering with exact sun-path extinction in an unoccluded exponential medium.",
                        "F4 omits scene shadowing, multiple scattering, polarization, wavelength sampling, and planetary curvature.",
                        "No product renderer, GPU backend, alpha path, or display color-management path is exercised."],
    }
    manifest_path = output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path("docs/worklog/artifacts/fog-atmosphere-rd-2026-11-12"))
    args = parser.parse_args()
    manifest = render_fixture(args.output_dir)
    print(f"Generated {len(manifest['candidates'])} float captures and previews in {args.output_dir}")
    for name, item in manifest["candidates"].items():
        print(f"{name}: {item['pfm_sha256']}")


if __name__ == "__main__":
    main()
