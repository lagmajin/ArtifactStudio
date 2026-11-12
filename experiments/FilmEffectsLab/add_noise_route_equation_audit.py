#!/usr/bin/env python3
"""Equation-only comparison of Add Noise CPU and two HLSL paths."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/add-noise-route-equation-audit-r1.json"
CPP = ROOT / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
IX = ROOT / "Artifact/include/Effects/AddNoise/AddNoiseEffect.ixx"
CONTROLLER = ROOT / "Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/add-noise-route-equation-audit-r1"
F32 = np.float32
U32 = np.uint32


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_image(width: int, height: int) -> np.ndarray:
    yy, xx = np.mgrid[0:height, 0:width].astype(F32)
    u = xx / F32(max(1, width - 1))
    v = yy / F32(max(1, height - 1))
    # Canonical CPU order is B,G,R,A. Ramps include low, middle, highlight,
    # and HDR values; alpha is straight with a soft elliptical boundary.
    b = F32(-0.05) + F32(0.65) * u + F32(0.25) * v
    g = F32(0.04) + F32(0.78) * u
    r = F32(0.02) + F32(1.45) * v
    image = np.stack((b, g, r), axis=-1).astype(F32)
    image[:, width // 12:width // 5] = (F32(0.05), F32(0.08), F32(0.1))
    image[:, width // 5:width * 2 // 5] = (F32(0.28), F32(0.4), F32(0.52))
    image[:, width * 2 // 5:width * 3 // 5] = (F32(0.88), F32(0.92), F32(0.98))
    image[:, width * 3 // 5:width * 4 // 5] = (F32(1.2), F32(1.35), F32(1.5))
    checker = ((xx.astype(np.int32) // max(2, width // 128) +
                yy.astype(np.int32) // max(2, height // 64)) % 2) == 0
    image[checker & (u > 0.80) & (u < 0.96)] = (F32(0.12), F32(0.72), F32(0.94))
    for col in range(width // 7, width, max(8, width // 32)):
        image[:, max(0, col - 1):col + 1] = (F32(0.96), F32(0.96), F32(0.96))
    for row in range(height // 8, height, max(8, height // 16)):
        image[max(0, row - 1):row + 1, :] = (F32(0.02), F32(0.02), F32(0.02))
    radius = np.sqrt(((xx - F32(width * .5)) / F32(width * .40)) ** 2 +
                     ((yy - F32(height * .5)) / F32(height * .42)) ** 2)
    alpha = np.clip((F32(1.08) - radius) * F32(10.0), F32(0.0), F32(1.0))
    return np.dstack((image, alpha)).astype(F32)


def hash_values(x: np.ndarray, y: np.ndarray, channel: int, seed: int) -> np.ndarray:
    with np.errstate(over="ignore"):
        value = (x.astype(U32) * U32(73856093) ^
                 y.astype(U32) * U32(19349663) ^
                 U32(channel) * U32(83492791) ^ U32(seed))
        value ^= value >> U32(16)
        value *= U32(0x7FEB352D)
        value ^= value >> U32(15)
        value *= U32(0x846CA68B)
        value ^= value >> U32(16)
    unit = (value & U32(0x00FFFFFF)).astype(F32) / F32(16777215.0)
    return unit * F32(2.0) - F32(1.0)


def uint_seed(seed: int) -> int:
    return int(U32(seed))


def float_bit_seed(seed: int) -> int:
    return int(np.asarray(F32(seed)).view(U32))


def evaluate(source: np.ndarray, amount: float, size: float, seed_value: int,
             mode: str) -> np.ndarray:
    """Mirror the common source expression with an explicit uint32 key."""
    height, width = source.shape[:2]
    size32 = max(F32(size), F32(0.1))
    x = np.floor(np.arange(width, dtype=F32) / size32).astype(U32)[None, :]
    y = np.floor(np.arange(height, dtype=F32) / size32).astype(U32)[:, None]
    amount32 = F32(amount)
    out = source.copy()
    p = source[..., :3]
    noise1 = hash_values(x, y, 1, seed_value) * amount32
    if mode == "mono":
        rgb_logical = p[..., ::-1].copy()
        rgb_logical = np.clip(rgb_logical + noise1[..., None], F32(0.0), F32(1.0))
        out[..., :3] = rgb_logical[..., ::-1]
    elif mode == "color":
        noise_r = hash_values(x, y, 1, seed_value) * amount32
        noise_g = hash_values(x, y, 2, seed_value) * amount32
        noise_b = hash_values(x, y, 3, seed_value) * amount32
        out[..., 0] = np.clip(p[..., 0] + noise_b, F32(0.0), F32(1.0))
        out[..., 1] = np.clip(p[..., 1] + noise_g, F32(0.0), F32(1.0))
        out[..., 2] = np.clip(p[..., 2] + noise_r, F32(0.0), F32(1.0))
    elif mode == "luma":
        # CPU source arithmetic in BGRA order; HLSL dot uses logical RGB.
        luma = p[..., 0] * F32(.114) + p[..., 1] * F32(.587) + p[..., 2] * F32(.299)
        new_luma = np.clip(luma + noise1, F32(0.0), F32(1.0))
        diff = new_luma - luma
        out[..., 0] = np.clip(p[..., 0] + diff, F32(0.0), F32(1.0))
        out[..., 1] = np.clip(p[..., 1] + diff, F32(0.0), F32(1.0))
        out[..., 2] = np.clip(p[..., 2] + diff, F32(0.0), F32(1.0))
    else:
        raise ValueError(mode)
    out[..., 3] = source[..., 3]
    return out


def route_models(source: np.ndarray, amount: float, size: float,
                 seed: int, mode: str) -> dict[str, np.ndarray]:
    return {
        "cpu": evaluate(source, amount, size, uint_seed(seed), mode),
        "resident": evaluate(source, amount, size, uint_seed(seed), mode),
        "dedicated_gpu_wrapper": evaluate(source, amount, size, float_bit_seed(seed), mode),
    }


def metric(a: np.ndarray, b: np.ndarray, mask: np.ndarray) -> dict[str, float | int]:
    av = a[mask, :3].astype(np.float64)
    bv = b[mask, :3].astype(np.float64)
    delta = av - bv
    return {
        "pixel_count": int(mask.sum()),
        "rgb_rmse": float(np.sqrt(np.mean(delta * delta))) if delta.size else 0.0,
        "rgb_mae": float(np.mean(np.abs(delta))) if delta.size else 0.0,
        "rgb_p99_abs_difference": float(np.quantile(np.abs(delta), .99)) if delta.size else 0.0,
        "rgb_max_abs_difference": float(np.max(np.abs(delta))) if delta.size else 0.0,
        "exact_channel_fraction": float(np.mean(delta == 0.0)) if delta.size else 1.0,
        "alpha_max_abs_difference": float(np.max(np.abs(a[mask, 3] - b[mask, 3]))) if mask.any() else 0.0,
    }


def output_stats(image: np.ndarray, source: np.ndarray, mask: np.ndarray) -> dict[str, float]:
    rgb = image[mask, :3]
    src = source[mask, :3]
    d = rgb.astype(np.float64) - src.astype(np.float64)
    return {
        "rgb_min": float(np.min(rgb)), "rgb_max": float(np.max(rgb)),
        "rgb_out_of_0_1_fraction": float(np.mean((rgb < 0) | (rgb > 1))),
        "rgb_at_0_or_1_fraction": float(np.mean((rgb == 0) | (rgb == 1))),
        "rgb_delta_rmse": float(np.sqrt(np.mean(d * d))),
        "alpha_max_abs_delta": float(np.max(np.abs(image[mask, 3] - source[mask, 3]))) if mask.any() else 0.0,
    }


def write16(path: Path, bgra: np.ndarray) -> None:
    encoded = np.rint(np.clip(bgra, 0, 1) * 65535).astype(np.uint16)
    cv2.imwrite(str(path), encoded)


def preview8(image: np.ndarray, background: float = .18) -> np.ndarray:
    rgb = image[..., :3] * image[..., 3:4] + F32(background) * (F32(1.0) - image[..., 3:4])
    return np.rint(np.clip(rgb[..., ::-1], 0, 1) * 255).astype(np.uint8)


def contact_sheet(path: Path, rows: list[tuple[str, np.ndarray]]) -> None:
    cells = []
    for label, image in rows:
        cell = cv2.resize(preview8(image), (384, 192), interpolation=cv2.INTER_AREA)
        cv2.putText(cell, label, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, .55,
                    (255, 255, 255), 1, cv2.LINE_AA)
        cells.append(cell)
    cv2.imwrite(str(path), np.vstack(cells))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    plan = json.loads(PLAN.read_text(encoding="utf-8"))
    cpp_text = CPP.read_text(encoding="utf-8")
    ix_text = IX.read_text(encoding="utf-8")
    controller_text = CONTROLLER.read_text(encoding="utf-8")
    markers = {
        "cpu_numeric_uint_seed": "static_cast<uint32_t>(seed_)" in cpp_text,
        "dedicated_hlsl_float_bitcast_seed": "asuint(g_Seed)" in cpp_text,
        "resident_hlsl_numeric_seed": "(uint)max(g_P4, 0.0)" in cpp_text,
        "resident_size_scaled_mask": "resolutionScaledParameterMask = (1u << 1)" in ix_text,
        "lod_scales_masked_parameters": "node.parameters[parameterIndex] *= effectResolutionScale" in controller_text,
        "dedicated_gpu_registered": "kAddNoiseResidentHlsl" in cpp_text and "registerGpuGenericShader" in cpp_text,
    }
    if not all(markers.values()):
        raise SystemExit(f"Source marker verification failed: {markers}")

    reports: dict[str, object] = {}
    preview_hash_targets: list[Path] = []
    for width, height in ((512, 256), (1024, 512)):
        source = source_image(width, height)
        yy, xx = np.mgrid[0:height, 0:width]
        regions = {
            "whole": np.ones((height, width), bool),
            "transparent": source[..., 3] < .01,
            "alpha_edge": (source[..., 3] >= .01) & (source[..., 3] < .99),
            "opaque": source[..., 3] >= .99,
            "hdr_input": np.any(source[..., :3] > 1.0, axis=2),
            "shadow_patch": (xx > width*.08) & (xx < width*.18) & (yy > height*.08) & (yy < height*.92),
            "highlight_patch": (xx > width*.61) & (xx < width*.78) & (yy > height*.08) & (yy < height*.92),
            "fine_detail": (xx > width*.80) & (xx < width*.96) & (yy > height*.08) & (yy < height*.92),
        }
        size_rows: dict[str, object] = {}
        mode_rows: dict[str, object] = {}
        amount_rows: dict[str, object] = {}
        preview_rows: list[tuple[str, np.ndarray]] = [("input", source)]

        # Full route comparison matrix on the high-contrast alpha/HDR plate.
        for seed in (0, 73, 1977):
            for mode in ("color", "mono", "luma"):
                for size in (.5, 1.0, 1.5, 3.0):
                    routes = route_models(source, .4, size, seed, mode)
                    key = f"seed_{seed}_{mode}_size_{size:g}"
                    size_rows[key] = {
                        "seed": seed, "mode": mode, "amount": .4, "size": size,
                        "resident_vs_cpu": {name: metric(routes["resident"], routes["cpu"], mask)
                                             for name, mask in regions.items()},
                        "dedicated_vs_cpu": {name: metric(routes["dedicated_gpu_wrapper"], routes["cpu"], mask)
                                              for name, mask in regions.items()},
                        "cpu_output": {name: output_stats(routes["cpu"], source, mask)
                                       for name, mask in regions.items()},
                        "resident_output": {name: output_stats(routes["resident"], source, mask)
                                            for name, mask in regions.items()},
                        "dedicated_output": {name: output_stats(routes["dedicated_gpu_wrapper"], source, mask)
                                             for name, mask in regions.items()},
                    }
                    if seed == 73 and mode == "color" and size == 1.5:
                        for route, image in routes.items():
                            name = f"{route}-seed73-color-size1.5-{width}x{height}.png"
                            path = OUT / name
                            write16(path, image)
                            preview_hash_targets.append(path)
                            preview_rows.append((route, image))
                        dedicated_delta = np.abs(routes["dedicated_gpu_wrapper"][..., :3] - routes["cpu"][..., :3]) * F32(8.0)
                        amplified = source.copy()
                        amplified[..., :3] = np.clip(dedicated_delta, 0, 1)
                        diff_path = OUT / f"dedicated-minus-cpu-amplified-{width}x{height}.png"
                        write16(diff_path, amplified)
                        preview_hash_targets.append(diff_path)
                        preview_rows.append(("dedicated-cpu-diff-x8", amplified))
                    if seed == 0 and mode == "color" and size == 1.5:
                        for route, image in routes.items():
                            path = OUT / f"{route}-seed0-color-size1.5-{width}x{height}.png"
                            write16(path, image)
                            preview_hash_targets.append(path)
                            preview_rows.append((f"{route}-seed0", image))

        # Zero amount is not assumed to be identity: all paths saturate RGB.
        for mode in ("color", "mono", "luma"):
            routes = route_models(source, 0.0, 1.5, 73, mode)
            amount_rows[mode] = {
                "amount": 0.0, "size": 1.5, "seed": 73,
                "cpu": {name: output_stats(routes["cpu"], source, mask) for name, mask in regions.items()},
                "resident": {name: output_stats(routes["resident"], source, mask) for name, mask in regions.items()},
                "dedicated_gpu_wrapper": {name: output_stats(routes["dedicated_gpu_wrapper"], source, mask) for name, mask in regions.items()},
            }
        for amount in (.1, .4):
            routes = route_models(source, amount, 1.5, 73, "color")
            amount_rows[f"amount_{amount:g}"] = {
                "amount": amount,
                "cpu": {name: output_stats(routes["cpu"], source, mask) for name, mask in regions.items()},
                "resident": {name: output_stats(routes["resident"], source, mask) for name, mask in regions.items()},
                "dedicated_gpu_wrapper": {name: output_stats(routes["dedicated_gpu_wrapper"], source, mask) for name, mask in regions.items()},
            }
        for mode in ("color", "mono", "luma"):
            routes = route_models(source, .4, 1.5, 73, mode)
            mode_rows[mode] = {
                "cpu_vs_resident_whole": metric(routes["cpu"], routes["resident"], regions["whole"]),
                "cpu_vs_dedicated_whole": metric(routes["cpu"], routes["dedicated_gpu_wrapper"], regions["whole"]),
            }

        size_mapping = []
        for requested_size in (.1, .5, 1.0, 1.5, 3.0):
            for lod_scale in (.25, .5, 1.0):
                scaled = F32(requested_size) * F32(lod_scale)
                size_mapping.append({
                    "requested_size": requested_size,
                    "effect_resolution_scale": lod_scale,
                    "parameter_after_mask": float(scaled),
                    "shader_denominator_after_max_point_1": float(max(scaled, F32(.1))),
                })

        source_path = OUT / f"input-{width}x{height}.png"
        write16(source_path, source)
        preview_hash_targets.append(source_path)
        contact_path = OUT / f"contact-sheet-{width}x{height}.png"
        contact_sheet(contact_path, preview_rows)
        preview_hash_targets.append(contact_path)
        reports[f"{width}x{height}"] = {
            "region_pixel_counts": {name: int(mask.sum()) for name, mask in regions.items()},
            "route_sweep": size_rows,
            "mode_summary": mode_rows,
            "amount_and_neutral_response": amount_rows,
            "resident_size_lod_parameter_mapping": size_mapping,
        }

    metrics = {
        "plan_id": plan["plan_id"],
        "plan_sha256": sha(PLAN),
        "script_sha256": sha(Path(__file__)),
        "source_sha256": {"implementation": sha(CPP), "interface": sha(IX), "lod_scaling": sha(CONTROLLER)},
        "python": platform.python_version(),
        "numpy": np.__version__,
        "opencv": cv2.__version__,
        "source_markers_verified": markers,
        "route_seed_conversion": {
            str(seed): {"cpu_uint32": uint_seed(seed), "resident_numeric_uint": uint_seed(seed),
                        "dedicated_float_asuint": float_bit_seed(seed)} for seed in (0, 73, 1977)
        },
        "measurements": reports,
        "preview_sha256": {p.name: sha(p) for p in sorted(set(preview_hash_targets))},
        "limits": "Equation mirrors do not exercise C++/HLSL compilers, GPU precision, runtime path selection, color/alpha ingress, or user-visible output.",
    }
    metrics_path = OUT / "metrics.json"
    metrics_path.write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (OUT / "metrics.sha256").write_text(f"{sha(metrics_path)}  metrics.json\n", encoding="utf-8")
    print(json.dumps({"metrics": str(metrics_path), "plan_sha256": metrics["plan_sha256"],
                      "script_sha256": metrics["script_sha256"], "source_markers_verified": markers,
                      "png_count": len(list(OUT.glob("*.png")))}, indent=2))


if __name__ == "__main__":
    main()
