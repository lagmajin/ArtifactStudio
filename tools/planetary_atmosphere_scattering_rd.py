#!/usr/bin/env python3
"""Small CPU single-scattering reference for a spherical planetary atmosphere."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import zlib
from pathlib import Path


WIDTH = 96
HEIGHT = 54
PLANET_RADIUS = 6_360_000.0
ATMOSPHERE_HEIGHT = 100_000.0
CAMERA_ALTITUDE = 2_000.0
RAYLEIGH_SCALE_HEIGHT = 8_000.0
MIE_SCALE_HEIGHT = 1_200.0
BETA_R = (5.8e-6, 13.5e-6, 33.1e-6)  # RGB inverse meters
BETA_M_SCATTER = (2.1e-5, 2.1e-5, 2.1e-5)
BETA_M_EXTINCTION = tuple(value / 0.9 for value in BETA_M_SCATTER)
SUN_RADIANCE = 18.0
SUN_ELEVATION_DEGREES = 18.0
SUN_AZIMUTH_DEGREES = 25.0
MIE_G = 0.76
FOV_DEGREES = 70.0


Vector = tuple[float, float, float]
RGB = tuple[float, float, float]


def add(a: Vector, b: Vector) -> Vector:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def scale(v: Vector, amount: float) -> Vector:
    return v[0] * amount, v[1] * amount, v[2] * amount


def dot(a: Vector, b: Vector) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def length(v: Vector) -> float:
    return math.sqrt(dot(v, v))


def normalize(v: Vector) -> Vector:
    return scale(v, 1.0 / max(length(v), 1e-20))


def sphere_roots(origin: Vector, direction: Vector, radius: float) -> tuple[float, float] | None:
    b = dot(origin, direction)
    c = dot(origin, origin) - radius * radius
    discriminant = b * b - c
    if discriminant < 0.0:
        return None
    root = math.sqrt(discriminant)
    return -b - root, -b + root


def atmosphere_segment(origin: Vector, direction: Vector) -> tuple[float, bool]:
    outer = sphere_roots(origin, direction, PLANET_RADIUS + ATMOSPHERE_HEIGHT)
    if outer is None:
        return 0.0, False
    end = outer[1]
    if end <= 0.0:
        return 0.0, False
    inner = sphere_roots(origin, direction, PLANET_RADIUS)
    if inner is not None and inner[0] > 1e-5 and inner[0] < end:
        return inner[0], True
    return end, False


def density_at(position: Vector) -> tuple[float, float]:
    altitude = max(0.0, length(position) - PLANET_RADIUS)
    return (math.exp(-altitude / RAYLEIGH_SCALE_HEIGHT),
            math.exp(-altitude / MIE_SCALE_HEIGHT))


def rayleigh_phase(cos_theta: float) -> float:
    return 3.0 * (1.0 + cos_theta * cos_theta) / (16.0 * math.pi)


def cornette_shanks_phase(g: float, cos_theta: float) -> float:
    denominator = max(1e-12, 1.0 + g * g - 2.0 * g * cos_theta)
    return (3.0 * (1.0 - g * g) * (1.0 + cos_theta * cos_theta)
            / (8.0 * math.pi * (2.0 + g * g) * denominator ** 1.5))


def sun_transmittance(position: Vector, sun_direction: Vector, steps: int) -> RGB:
    distance, occluded = atmosphere_segment(position, sun_direction)
    if occluded or distance <= 0.0:
        return 0.0, 0.0, 0.0
    ds = distance / steps
    tau_r = 0.0
    tau_m = 0.0
    for i in range(steps):
        sample = add(position, scale(sun_direction, (i + 0.5) * ds))
        density_r, density_m = density_at(sample)
        tau_r += density_r * ds
        tau_m += density_m * ds
    return tuple(math.exp(-BETA_R[c] * tau_r - BETA_M_EXTINCTION[c] * tau_m) for c in range(3))


def view_direction(x: int, y: int) -> Vector:
    aspect = WIDTH / HEIGHT
    half_height = math.tan(math.radians(FOV_DEGREES) * 0.5)
    ndc_x = 2.0 * (x + 0.5) / WIDTH - 1.0
    ndc_y = 1.0 - 2.0 * (y + 0.5) / HEIGHT
    return normalize((ndc_x * aspect * half_height, ndc_y * half_height, 1.0))


def integrate_pixel(direction: Vector, view_steps: int, sun_steps: int,
                    mie_g: float = MIE_G, rayleigh_enabled: bool = True,
                    mie_enabled: bool = True,
                    sun_elevation_degrees: float = SUN_ELEVATION_DEGREES,
                    camera_altitude_m: float = CAMERA_ALTITUDE,
                    view_sampling: str = "uniform") -> RGB:
    origin = (0.0, PLANET_RADIUS + camera_altitude_m, 0.0)
    distance, _hits_ground = atmosphere_segment(origin, direction)
    if distance <= 0.0:
        return 0.0, 0.0, 0.0
    sun_elevation = math.radians(sun_elevation_degrees)
    sun_azimuth = math.radians(SUN_AZIMUTH_DEGREES)
    sun_direction = normalize((math.cos(sun_elevation) * math.sin(sun_azimuth),
                                math.sin(sun_elevation),
                                math.cos(sun_elevation) * math.cos(sun_azimuth)))
    cosine = max(-1.0, min(1.0, dot(direction, sun_direction)))
    phase_r = rayleigh_phase(cosine)
    phase_m = cornette_shanks_phase(mie_g, cosine)
    tau_r = 0.0
    tau_m = 0.0
    radiance = [0.0, 0.0, 0.0]
    for i in range(view_steps):
        u0 = i / view_steps
        u1 = (i + 1) / view_steps
        um = (i + 0.5) / view_steps
        if view_sampling == "altitude_biased":
            tangent_blend = min(1.0, abs(direction[1]) / 0.1)
            tangent_blend = tangent_blend * tangent_blend * (3.0 - 2.0 * tangent_blend)
            if direction[1] >= 0.0:
                warp_u = um * um
            else:
                warp_u = 1.0 - (1.0 - um) * (1.0 - um)
            warp_edge0 = u0 + tangent_blend * ((u0 * u0 if direction[1] >= 0.0 else
                                                 1.0 - (1.0 - u0) * (1.0 - u0)) - u0)
            warp_edge1 = u1 + tangent_blend * ((u1 * u1 if direction[1] >= 0.0 else
                                                 1.0 - (1.0 - u1) * (1.0 - u1)) - u1)
            mapped_u = um + tangent_blend * (warp_u - um)
            t = distance * mapped_u
            ds = distance * (warp_edge1 - warp_edge0)
        else:
            t = distance * um
            ds = distance / view_steps
        position = add(origin, scale(direction, t))
        density_r, density_m = density_at(position)
        sun_t = sun_transmittance(position, sun_direction, sun_steps)
        view_t = tuple(math.exp(-BETA_R[c] * (tau_r + 0.5 * density_r * ds)
                                - BETA_M_EXTINCTION[c] * (tau_m + 0.5 * density_m * ds))
                       for c in range(3))
        for c in range(3):
            source = 0.0
            if rayleigh_enabled:
                source += BETA_R[c] * density_r * phase_r
            if mie_enabled:
                source += BETA_M_SCATTER[c] * density_m * phase_m
            radiance[c] += view_t[c] * sun_t[c] * source * SUN_RADIANCE * ds
        tau_r += density_r * ds
        tau_m += density_m * ds
    return radiance[0], radiance[1], radiance[2]


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    data = kind + payload
    return struct.pack(">I", len(payload)) + data + struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)


def write_pfm(path: Path, pixels: list[RGB]) -> None:
    body = b"".join(struct.pack("<3f", *pixels[y * WIDTH + x])
                    for y in reversed(range(HEIGHT)) for x in range(WIDTH))
    path.write_bytes(f"PF\n{WIDTH} {HEIGHT}\n-1.0\n".encode("ascii") + body)


def write_preview(path: Path, pixels: list[RGB]) -> None:
    raw = bytearray()
    for y in range(HEIGHT):
        raw.append(0)
        for x in range(WIDTH):
            for channel in pixels[y * WIDTH + x]:
                mapped = max(0.0, channel) / (1.0 + max(0.0, channel))
                raw.append(int(max(0.0, min(1.0, mapped ** (1.0 / 2.2))) * 255.0 + 0.5))
    header = struct.pack(">2I5B", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    encoded = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header)
    encoded += png_chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + png_chunk(b"IEND", b"")
    path.write_bytes(encoded)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def compare(reference: list[RGB], candidate: list[RGB]) -> dict[str, object]:
    abs_sum = [0.0, 0.0, 0.0]
    max_delta = [0.0, 0.0, 0.0]
    squared = 0.0
    changed = 0
    for a, b in zip(reference, candidate):
        pixel_delta = 0.0
        for c in range(3):
            delta = abs(a[c] - b[c])
            abs_sum[c] += delta
            max_delta[c] = max(max_delta[c], delta)
            pixel_delta = max(pixel_delta, delta)
            squared += delta * delta
        changed += pixel_delta > 1e-5
    return {"mean_absolute_rgb": [value / len(reference) for value in abs_sum],
            "max_absolute_rgb": max_delta,
            "rgb_rmse": math.sqrt(squared / (len(reference) * 3)),
            "pixels_max_channel_delta_over_1e-5": changed}


def radiance_summary(pixels: list[RGB]) -> dict[str, object]:
    mean_rgb = [sum(pixel[c] for pixel in pixels) / len(pixels) for c in range(3)]
    max_rgb = [max(pixel[c] for pixel in pixels) for c in range(3)]
    mean_luminance = sum(0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2] for p in pixels) / len(pixels)
    return {"mean_rgb": mean_rgb, "max_rgb": max_rgb, "mean_luminance": mean_luminance}


def phase_integral(phase_function, samples: int = 262144) -> float:
    """Numerically integrate an isotropic phase function over the sphere."""
    step = 2.0 / samples
    total = 0.0
    for i in range(samples):
        cosine = -1.0 + (i + 0.5) * step
        total += phase_function(cosine) * step
    return 2.0 * math.pi * total


def ray_convergence_diagnostics() -> dict[str, object]:
    sample_pixels = {
        "center_horizon": (WIDTH // 2, HEIGHT // 2),
        "center_zenith": (WIDTH // 2, 0),
        "center_groundward": (WIDTH // 2, HEIGHT * 2 // 3),
        "left_horizon": (0, HEIGHT // 2),
        "right_horizon": (WIDTH - 1, HEIGHT // 2),
        "sunward_upper": (WIDTH * 3 // 4, HEIGHT // 4),
    }
    levels = ((16, 8), (32, 16), (64, 24), (128, 48), (256, 96), (512, 192))
    results: dict[str, object] = {}
    for name, (x, y) in sample_pixels.items():
        direction = view_direction(x, y)
        values = [integrate_pixel(direction, view_steps, sun_steps) for view_steps, sun_steps in levels]
        reference = values[-1]
        level_metrics = []
        for (view_steps, sun_steps), value in zip(levels, values):
            relative = [abs(value[c] - reference[c]) / max(abs(reference[c]), 1e-5) for c in range(3)]
            level_metrics.append({"view_steps": view_steps, "sun_steps": sun_steps,
                                  "rgb": value, "max_relative_rgb_error_to_512x192": max(relative)})
        results[name] = {"pixel": [x, y], "direction": direction, "levels": level_metrics}
    independent_sweeps: dict[str, object] = {}
    sweep_pixels = {"center_horizon": sample_pixels["center_horizon"],
                    "sunward_upper": sample_pixels["sunward_upper"]}
    for name, (x, y) in sweep_pixels.items():
        direction = view_direction(x, y)
        view_reference = results[name]["levels"][-1]["rgb"]
        view_levels = []
        for view_steps in (32, 64, 128, 256):
            value = integrate_pixel(direction, view_steps, 192)
            relative = [abs(value[c] - view_reference[c]) / max(abs(view_reference[c]), 1e-5)
                        for c in range(3)]
            view_levels.append({"view_steps": view_steps, "sun_steps_fixed": 192,
                                "max_relative_rgb_error_to_512x192": max(relative)})
        sun_reference = integrate_pixel(direction, 128, 256)
        sun_levels = []
        for sun_steps in (8, 16, 32, 64, 128):
            value = integrate_pixel(direction, 128, sun_steps)
            relative = [abs(value[c] - sun_reference[c]) / max(abs(sun_reference[c]), 1e-5)
                        for c in range(3)]
            sun_levels.append({"sun_steps": sun_steps, "view_steps_fixed": 128,
                               "max_relative_rgb_error_to_128x256": max(relative)})
        independent_sweeps[name] = {"pixel": [x, y], "view_step_sweep": view_levels,
                                    "sun_step_sweep": sun_levels}
    results["independent_axis_sweeps"] = independent_sweeps
    sampling_comparison: dict[str, object] = {}
    sampling_pixels = {"center_horizon": sample_pixels["center_horizon"],
                       "center_zenith": sample_pixels["center_zenith"],
                       "center_groundward": sample_pixels["center_groundward"],
                       "sunward_upper": sample_pixels["sunward_upper"]}
    for name, (x, y) in sampling_pixels.items():
        direction = view_direction(x, y)
        reference = results[name]["levels"][-1]["rgb"]
        levels = []
        for steps in (16, 32, 64):
            variants = {}
            for sampling in ("uniform", "altitude_biased"):
                value = integrate_pixel(direction, steps, 192, view_sampling=sampling)
                relative = [abs(value[c] - reference[c]) / max(abs(reference[c]), 1e-5)
                            for c in range(3)]
                variants[sampling] = {"max_relative_rgb_error_to_uniform_512x192": max(relative),
                                      "rgb": value}
            levels.append({"view_steps": steps, "sun_steps_fixed": 192, "variants": variants})
        sampling_comparison[name] = {"pixel": [x, y], "direction": direction, "levels": levels}
    results["view_sampling_comparison"] = sampling_comparison
    return results


def camera_motion_stability_diagnostics() -> dict[str, object]:
    """Measure angular sampling error during a small deterministic camera pitch."""
    frame_offsets_degrees = (-1.0, -0.5, 0.0, 0.5, 1.0)
    sample_pixels = {"center_horizon": (WIDTH // 2, HEIGHT // 2),
                     "sunward_upper": (WIDTH * 3 // 4, HEIGHT // 4)}
    results: dict[str, object] = {}
    for name, (x, y) in sample_pixels.items():
        base = view_direction(x, y)
        frames: dict[str, list[RGB]] = {"reference_uniform_512x192": [],
                                        "uniform_32x16": [], "altitude_biased_32x16": []}
        for offset in frame_offsets_degrees:
            angle = math.radians(offset)
            ca, sa = math.cos(angle), math.sin(angle)
            direction = normalize((base[0], base[1] * ca - base[2] * sa,
                                   base[1] * sa + base[2] * ca))
            frames["reference_uniform_512x192"].append(integrate_pixel(direction, 512, 192))
            frames["uniform_32x16"].append(integrate_pixel(direction, 32, 16))
            frames["altitude_biased_32x16"].append(
                integrate_pixel(direction, 32, 16, view_sampling="altitude_biased"))

        reference = frames["reference_uniform_512x192"]
        candidate_metrics: dict[str, object] = {}
        for candidate_name in ("uniform_32x16", "altitude_biased_32x16"):
            values = frames[candidate_name]
            frame_errors = []
            temporal_error_sq = 0.0
            temporal_sample_count = 0
            temporal_max_rgb = 0.0
            for ref, value in zip(reference, values):
                relative = [abs(value[c] - ref[c]) / max(abs(ref[c]), 1e-5) for c in range(3)]
                frame_errors.append(max(relative))
            for index in range(1, len(values)):
                residual = [((values[index][c] - values[index - 1][c]) -
                             (reference[index][c] - reference[index - 1][c])) for c in range(3)]
                temporal_max_rgb = max(temporal_max_rgb, *(abs(value) for value in residual))
                temporal_error_sq += sum(value * value for value in residual)
                temporal_sample_count += 3
            candidate_metrics[candidate_name] = {
                "max_relative_rgb_error_per_frame_to_reference": frame_errors,
                "max_relative_rgb_error_over_sweep": max(frame_errors),
                "temporal_delta_rgb_rmse_vs_reference": math.sqrt(
                    temporal_error_sq / max(1, temporal_sample_count)),
                "temporal_delta_max_absolute_rgb_error_vs_reference": temporal_max_rgb,
            }
        results[name] = {"base_pixel": [x, y], "pitch_offsets_degrees": frame_offsets_degrees,
                         "reference": "uniform 512x192 at each angular sample",
                         "candidates": candidate_metrics}
    return {"method": "Rotate a fixed pixel ray around camera-right; compare coarse quadrature frame deltas against high-step CPU reference.",
            "note": "Angular ray sweep is a deterministic camera-pitch proxy, not a rendered sequence or temporal-reprojection test.",
            "rays": results}


def render(output_dir: Path) -> dict[str, object]:
    output_dir.mkdir(parents=True, exist_ok=True)
    directions = [view_direction(x, y) for y in range(HEIGHT) for x in range(WIDTH)]
    variants = {
        "rayleigh_32x16": [integrate_pixel(d, 32, 16, mie_enabled=False) for d in directions],
        "rayleigh_mie_g076_32x16": [integrate_pixel(d, 32, 16) for d in directions],
        "rayleigh_mie_g076_altitude_biased_32x16": [
            integrate_pixel(d, 32, 16, view_sampling="altitude_biased") for d in directions],
        "rayleigh_mie_isotropic_32x16": [integrate_pixel(d, 32, 16, mie_g=0.0) for d in directions],
        "mie_g_050_32x16": [integrate_pixel(d, 32, 16, mie_g=0.5) for d in directions],
        "rayleigh_mie_g076_64x24": [integrate_pixel(d, 64, 24) for d in directions],
        "mie_g_090_32x16": [integrate_pixel(d, 32, 16, mie_g=0.9) for d in directions],
        "rayleigh_mie_g076_128x48": [integrate_pixel(d, 128, 48) for d in directions],
        "sun_elevation_0_32x16": [integrate_pixel(d, 32, 16, sun_elevation_degrees=0.0) for d in directions],
        "sun_elevation_5_32x16": [integrate_pixel(d, 32, 16, sun_elevation_degrees=5.0) for d in directions],
        "sun_elevation_45_32x16": [integrate_pixel(d, 32, 16, sun_elevation_degrees=45.0) for d in directions],
        "sun_elevation_75_32x16": [integrate_pixel(d, 32, 16, sun_elevation_degrees=75.0) for d in directions],
        "camera_altitude_0_5km_32x16": [integrate_pixel(d, 32, 16, camera_altitude_m=500.0) for d in directions],
        "camera_altitude_10km_32x16": [integrate_pixel(d, 32, 16, camera_altitude_m=10_000.0) for d in directions],
        "camera_altitude_50km_32x16": [integrate_pixel(d, 32, 16, camera_altitude_m=50_000.0) for d in directions],
    }
    entries: dict[str, dict[str, str]] = {}
    for name, pixels in variants.items():
        pfm = output_dir / f"{name}.pfm"
        preview = output_dir / f"{name}_preview.png"
        write_pfm(pfm, pixels)
        write_preview(preview, pixels)
        entries[name] = {"pfm": pfm.name, "pfm_sha256": sha256(pfm),
                         "preview_png": preview.name, "preview_sha256": sha256(preview)}
    baseline = variants["rayleigh_mie_g076_128x48"]
    metrics = {name: compare(baseline, pixels) for name, pixels in variants.items()
               if name != "rayleigh_mie_g076_128x48"}
    manifest = {
        "schema": "planetary-atmosphere-scattering-rd-v1",
        "image_size": [WIDTH, HEIGHT],
        "camera": {"planet_center": [0.0, 0.0, 0.0], "planet_radius_m": PLANET_RADIUS,
                   "altitude_m": CAMERA_ALTITUDE, "view_direction": [0.0, 0.0, 1.0],
                   "vertical_fov_degrees": FOV_DEGREES},
        "atmosphere": {"top_height_m": ATMOSPHERE_HEIGHT,
                       "rayleigh_scale_height_m": RAYLEIGH_SCALE_HEIGHT,
                       "mie_scale_height_m": MIE_SCALE_HEIGHT,
                       "beta_rayleigh_rgb_per_m": BETA_R,
                       "beta_mie_scattering_rgb_per_m": BETA_M_SCATTER,
                       "beta_mie_extinction_rgb_per_m": BETA_M_EXTINCTION,
                       "mie_g": MIE_G, "sun_elevation_degrees": SUN_ELEVATION_DEGREES,
                       "sun_azimuth_degrees_from_view_forward": SUN_AZIMUTH_DEGREES,
                       "sun_radiance": SUN_RADIANCE},
        "output": {"format": "PFM RGB float32 little-endian", "space": "linear scene-referred radiance",
                   "preview_transform": "Reinhard channel mapping followed by gamma 1/2.2",
                   "mean_luminance_metric": "Rec.709 weighted RGB proxy; not photometrically calibrated"},
        "variants": entries,
        "radiance_summary": {name: radiance_summary(pixels) for name, pixels in variants.items()},
        "sun_elevation_sweep_degrees": {
            "0": "sun_elevation_0_32x16", "5": "sun_elevation_5_32x16",
            "18": "rayleigh_mie_g076_32x16", "45": "sun_elevation_45_32x16",
            "75": "sun_elevation_75_32x16",
        },
        "camera_altitude_sweep_m": {
            "500": "camera_altitude_0_5km_32x16",
            "2000": "rayleigh_mie_g076_32x16",
            "10000": "camera_altitude_10km_32x16",
            "50000": "camera_altitude_50km_32x16",
        },
        "phase_function_sphere_integral_diagnostic": {
            "method": "midpoint integration over cosine in [-1,1], multiplied by 2*pi",
            "samples_over_cosine_interval": 262144,
            "rayleigh": phase_integral(rayleigh_phase),
            "cornette_shanks_mie_g076": phase_integral(lambda cosine: cornette_shanks_phase(MIE_G, cosine)),
            "cornette_shanks_mie_by_g": {
                f"{g:.2f}": phase_integral(lambda cosine, phase_g=g: cornette_shanks_phase(phase_g, cosine))
                for g in (0.0, 0.5, 0.76, 0.9)
            },
        },
        "ray_convergence_diagnostics": ray_convergence_diagnostics(),
        "camera_motion_stability_diagnostics": camera_motion_stability_diagnostics(),
        "linear_rgb_comparison_to_rayleigh_mie_g076_128x48": metrics,
        "phase_function": "Rayleigh plus Cornette-Shanks Mie phase (g parameter)",
        "view_sampling_candidate": {
            "variant": "rayleigh_mie_g076_altitude_biased_32x16",
            "status": "CPU diagnostic only; not a GPU implementation.",
            "mapping": "Blend linear u with outward u^2 or inward 1-(1-u)^2 using smoothstep(abs(radial_direction_y)/0.1); this avoids a discrete tangent-angle branch. Each uniform-u cell uses its mapped world-distance width as the quadrature weight.",
            "comparison": "Same view sample count as uniform; sun integration remains unchanged.",
        },
        "mie_anisotropy_sweep": {"0.0": "rayleigh_mie_isotropic_32x16",
                                 "0.5": "mie_g_050_32x16",
                                 "0.76": "rayleigh_mie_g076_32x16",
                                 "0.9": "mie_g_090_32x16"},
        "mie_anisotropy_comparison_to_g076_32x16": {
            name: compare(variants["rayleigh_mie_g076_32x16"], variants[name])
            for name in ("rayleigh_mie_isotropic_32x16", "mie_g_050_32x16", "mie_g_090_32x16")
        },
        "limitations": ["Single scattering only; no multiple-scattering LUT.",
                        "No ground albedo, clouds, local lights, or solar disk.",
                        "Homogeneous spherical planet and fixed RGB coefficients; arbitrary sun radiance.",
                        "Midpoint integration; CPU reference only, not GPU performance or production color management."],
    }
    (output_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path,
                        default=Path("docs/worklog/artifacts/planetary-atmosphere-rd-2026-11-12"))
    args = parser.parse_args()
    manifest = render(args.output_dir)
    print(f"Generated {len(manifest['variants'])} atmosphere captures in {args.output_dir}")
    for name, result in manifest["linear_rgb_comparison_to_rayleigh_mie_g076_128x48"].items():
        print(f"{name}: RMSE={result['rgb_rmse']:.8g}, meanAbs={result['mean_absolute_rgb']}")


if __name__ == "__main__":
    main()
