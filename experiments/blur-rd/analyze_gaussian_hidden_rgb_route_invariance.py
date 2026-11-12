"""Check hidden-RGB invariance on a synthetic alpha edge for Gaussian routes.

This is an offline source-equation comparison. It does not run Artifact or a GPU.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = ROOT / "experiments/blur-rd/fixtures/gaussian-r1"
SOURCE = ROOT / "Artifact/src/Effects/Blur/GauusianBlur.cpp"
ANALYZER_PATH = ROOT / "experiments/blur-rd/analyze_gaussian_fixture_bundle.py"
OUTPUT = ROOT / "experiments/blur-rd/results/gaussian_hidden_rgb_route_invariance_2026-11-12.json"
SPEC = importlib.util.spec_from_file_location("gaussian_fixture_analyzer", ANALYZER_PATH)
assert SPEC and SPEC.loader
ANALYZER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ANALYZER)
SIGMAS = (0.5, 1.0, 3.0, 16.0, 64.0)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stats(values: np.ndarray) -> dict[str, float | int]:
    a = np.abs(values.astype(np.float64)).reshape(-1)
    return {
        "components": int(a.size),
        "mae": float(a.mean()) if a.size else 0.0,
        "rmse": float(np.sqrt(np.mean(a * a))) if a.size else 0.0,
        "p95": float(np.quantile(a, 0.95)) if a.size else 0.0,
        "max_abs": float(a.max()) if a.size else 0.0,
        "nonzero_count": int(np.count_nonzero(a)),
    }


def with_hidden_rgb(straight: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    changed = straight.copy()
    alpha_zero = changed[..., 3] == 0.0
    yy, xx = np.indices(alpha_zero.shape)
    checker = ((xx // 8 + yy // 8) % 2) == 0
    # BGRA physical channels: opposing HDR/negative hidden colors, only at A=0.
    changed[..., 0][alpha_zero] = np.where(checker[alpha_zero], 8.0, -3.0)
    changed[..., 1][alpha_zero] = np.where(checker[alpha_zero], -2.0, 5.0)
    changed[..., 2][alpha_zero] = np.where(checker[alpha_zero], 1.5, -7.0)
    return changed, alpha_zero


def cpu_visible_premultiplied(straight: np.ndarray, sigma: float) -> np.ndarray:
    out = ANALYZER.blur_cpu_wrapper(straight, sigma)
    return out[..., :3] * out[..., 3:4]


def background_composite(pm: np.ndarray, alpha: np.ndarray, background: np.ndarray) -> np.ndarray:
    return pm + background * (1.0 - alpha[..., None])


def main() -> None:
    fixture_path = BUNDLE / "premultiplied_alpha_edge_hdr.npy"
    authored_premultiplied = np.load(fixture_path)
    straight_sanitized = ANALYZER.straight_from_premultiplied(authored_premultiplied)
    straight_hidden, alpha_zero = with_hidden_rgb(straight_sanitized)
    hidden_premultiplied = authored_premultiplied.copy()
    hidden_premultiplied[..., :3][alpha_zero] = straight_hidden[..., :3][alpha_zero]
    backgrounds = {
        "black": np.zeros(3, dtype=np.float32),
        "gray_18_percent": np.full(3, 0.18, dtype=np.float32),
        "white": np.ones(3, dtype=np.float32),
    }
    measurements = []
    for sigma in SIGMAS:
        cpu_sanitized = ANALYZER.blur_cpu_wrapper(straight_sanitized, sigma)
        cpu_hidden = ANALYZER.blur_cpu_wrapper(straight_hidden, sigma)
        cpu_sanitized_pm = cpu_sanitized[..., :3] * cpu_sanitized[..., 3:4]
        cpu_hidden_pm = cpu_hidden[..., :3] * cpu_hidden[..., 3:4]
        # Mirror the current HLSL two-pass per-sample RGB*alpha accumulation,
        # weight normalization, and per-pass unassociation.
        gpu_sanitized = ANALYZER.blur_standalone_gpu_wrapper(straight_sanitized, sigma)
        gpu_hidden = ANALYZER.blur_standalone_gpu_wrapper(straight_hidden, sigma)
        gpu_sanitized_pm = gpu_sanitized[..., :3] * gpu_sanitized[..., 3:4]
        gpu_hidden_pm = gpu_hidden[..., :3] * gpu_hidden[..., 3:4]

        route = {
            "cpu_wrapper_hidden_vs_sanitized": {
                "premultiplied_rgb_full": stats(cpu_hidden_pm - cpu_sanitized_pm),
                "alpha_full": stats(cpu_hidden[..., 3] - cpu_sanitized[..., 3]),
            },
            "resident_equation_hidden_vs_sanitized": {
                "premultiplied_rgb_full": stats(gpu_hidden_pm - gpu_sanitized_pm),
                "alpha_full": stats(gpu_hidden[..., 3] - gpu_sanitized[..., 3]),
            },
            "cpu_wrapper_vs_resident_for_sanitized": {
                "premultiplied_rgb_full": stats(cpu_sanitized_pm - gpu_sanitized_pm),
                "alpha_full": stats(cpu_sanitized[..., 3] - gpu_sanitized[..., 3]),
            },
            "cpu_wrapper_vs_resident_for_hidden": {
                "premultiplied_rgb_full": stats(cpu_hidden_pm - gpu_hidden_pm),
                "alpha_full": stats(cpu_hidden[..., 3] - gpu_hidden[..., 3]),
            },
            "background_composite_hidden_vs_sanitized": {},
        }
        for bg_name, bg in backgrounds.items():
            cpu_a = background_composite(cpu_sanitized_pm, cpu_sanitized[..., 3], bg)
            cpu_b = background_composite(cpu_hidden_pm, cpu_hidden[..., 3], bg)
            gpu_a = background_composite(gpu_sanitized_pm, gpu_sanitized[..., 3], bg)
            gpu_b = background_composite(gpu_hidden_pm, gpu_hidden[..., 3], bg)
            route["background_composite_hidden_vs_sanitized"][bg_name] = {
                "cpu_wrapper": stats(cpu_b - cpu_a),
                "resident_equation": stats(gpu_b - gpu_a),
            }
        measurements.append({"sigma_px": sigma, "metrics": route})

    report = {
        "study": "Gaussian hidden-RGB invariance across alpha edge routes",
        "run_date": "2026-11-12",
        "evidence_class": "offline source-equation model only; not Artifact output or GPU dispatch",
        "source": "Artifact/src/Effects/Blur/GauusianBlur.cpp",
        "source_sha256": sha256(SOURCE),
        "runner": "experiments/blur-rd/analyze_gaussian_hidden_rgb_route_invariance.py",
        "runner_sha256": sha256(Path(__file__)),
        "analyzer": "experiments/blur-rd/analyze_gaussian_fixture_bundle.py",
        "analyzer_sha256": sha256(ANALYZER_PATH),
        "fixture": {
            "base": "experiments/blur-rd/fixtures/gaussian-r1/premultiplied_alpha_edge_hdr.npy",
            "base_sha256": sha256(fixture_path),
            "derived_hidden_rgb_sha256_float32": hashlib.sha256(hidden_premultiplied.tobytes()).hexdigest(),
            "shape_yx_channels": list(authored_premultiplied.shape),
            "dtype": str(authored_premultiplied.dtype),
            "storage": "BGRA physical channel order",
            "transfer": "synthetic linear numeric values, unclamped",
            "alpha_association": "base authored premultiplied; derived input modifies RGB only where alpha==0",
            "alpha_zero_pixel_count": int(alpha_zero.sum()),
            "hidden_rgb_values_by_physical_channel": [[-3.0, 8.0], [-2.0, 5.0], [-7.0, 1.5]],
            "visible_and_alpha_unchanged": bool(np.array_equal(hidden_premultiplied[..., 3], authored_premultiplied[..., 3]) and np.array_equal(hidden_premultiplied[~alpha_zero, :3], authored_premultiplied[~alpha_zero, :3])),
        },
        "conditions": {
            "sigma_px": list(SIGMAS),
            "cpu": "premultiply numeric RGB by alpha, OpenCV separable Gaussian with REFLECT_101, unpremultiply only where output alpha>1e-5, otherwise zero RGB",
            "resident_equation": "HLSL two-pass clamp sampling, accumulating RGB*alpha*weight and alpha*weight, normalizing by weight sum and unassociating RGB when alpha>1e-5 on each pass; input/output numeric RGB modeled as straight",
            "comparison": "CPU and GPU-equation straight outputs are reassociated before comparing visible premultiplied RGB; alpha is measured separately",
            "backgrounds": {name: value.tolist() for name, value in backgrounds.items()},
            "display_transform": "none; composite values remain linear numeric and unclipped",
        },
        "measurements": measurements,
        "analysis_discrepancy": "The former analyze_gaussian_fixture_bundle.py blur_resident() helper performed direct RGBA convolution and did not mirror current HLSL RGB*alpha accumulation/unassociation. It is now named blur_direct_rgba_control(). This runner uses blur_standalone_gpu_wrapper() for the current shader equation. Historical results using the former helper are not current HLSL alpha-contract evidence.",
        "interpretation": "Hidden RGB at alpha zero should not affect the premultiplied visible result. This verifies equation-level invariance on one synthetic alpha-edge family. It does not establish the descriptor contract reaching either route, a live runtime result, visual acceptance, or performance.",
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(OUTPUT)
    for item in measurements:
        sigma = item["sigma_px"]
        cpu = item["metrics"]["cpu_wrapper_hidden_vs_sanitized"]["premultiplied_rgb_full"]["max_abs"]
        gpu = item["metrics"]["resident_equation_hidden_vs_sanitized"]["premultiplied_rgb_full"]["max_abs"]
        print(f"sigma={sigma:g}: hidden RGB max abs CPU/GPU equation={cpu:.9g}/{gpu:.9g}")


if __name__ == "__main__":
    main()
