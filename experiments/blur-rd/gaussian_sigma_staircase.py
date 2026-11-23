"""Map Gaussian CPU/GPU support staircases over the exposed UI sigma range.

This is a float32 source-equation analysis, not a live Artifact route capture.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np

from gaussian_boundary_energy_model import ROOT, SOURCE, cpu_kernel_size, shader_kernel

OUTPUT = ROOT / "experiments/blur-rd/results/gaussian_sigma_staircase_2026-11-12.json"
SIGMAS = tuple(float(np.float32(i / 10.0)) for i in range(1, 641))


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cpu_radius(sigma: float) -> int:
    return (cpu_kernel_size(sigma) - 1) // 2


def gpu_radius(sigma: float) -> int:
    applied = max(np.float32(0.1), np.float32(sigma))
    return min(64, int(math.ceil(float(np.float32(applied * np.float32(3.0))))))


def summarize_runs(values: list[tuple[float, int, int]]) -> list[dict[str, object]]:
    runs: list[dict[str, object]] = []
    for sigma, cr, gr in values:
        matched = cr == gr
        if runs and runs[-1]["cpu_radius_px"] == cr and runs[-1]["gpu_radius_px"] == gr:
            runs[-1]["sigma_max"] = sigma
            runs[-1]["sample_count"] += 1
        else:
            runs.append({
                "sigma_min": sigma,
                "sigma_max": sigma,
                "cpu_radius_px": cr,
                "gpu_radius_px": gr,
                "radius_match": matched,
                "sample_count": 1,
            })
    return runs


def main() -> None:
    values = [(sigma, cpu_radius(sigma), gpu_radius(sigma)) for sigma in SIGMAS]
    matches = [v for v in values if v[1] == v[2]]
    mismatches = [v for v in values if v[1] != v[2]]
    # Compare the actual normalized float32 1D kernels for the same nominal sigma,
    # before any 2D image, alpha, border, or output-descriptor behavior is involved.
    kernel_rmse: list[tuple[float, float]] = []
    for sigma, cr, gr in values:
        if cr != gr:
            continue
        cpu = np.asarray(__import__("cv2").getGaussianKernel(2 * cr + 1, float(np.float32(sigma)), __import__("cv2").CV_32F)).reshape(-1)
        gpu = shader_kernel(sigma)
        kernel_rmse.append((sigma, float(np.sqrt(np.mean((cpu.astype(np.float64) - gpu.astype(np.float64)) ** 2)))))
    report = {
        "study": "Gaussian CPU/GPU finite support staircase over UI sigma steps",
        "run_date": "2026-11-12",
        "evidence_class": "float32 source-equation model only; not Artifact output or GPU dispatch",
        "source": "Artifact/src/Effects/Blur/GauusianBlur.cpp",
        "source_sha256": sha256(SOURCE),
        "runner": "experiments/blur-rd/gaussian_sigma_staircase.py",
        "runner_sha256": sha256(Path(__file__)),
        "base_model": "experiments/blur-rd/gaussian_boundary_energy_model.py",
        "base_model_sha256": sha256(Path(__file__).with_name("gaussian_boundary_energy_model.py")),
        "conditions": {
            "sigma_ui_min": 0.1,
            "sigma_ui_max": 64.0,
            "sigma_ui_step": 0.1,
            "sample_count": len(values),
            "cpu_support": "odd kernel size max(1,int(6*float32(sigma)+1)); radius=(size-1)/2",
            "gpu_support": "min(64,ceil(float32(float32(sigma)*3)))",
            "gpu_radius_cap_first_ui_sigma": next((float(f"{i / 10:.1f}") for i in range(1, 641) if gpu_radius(float(np.float32(i / 10.0))) == 64), None),
        },
        "summary": {
            "support_match_samples": len(matches),
            "support_match_fraction": len(matches) / len(values),
            "support_mismatch_samples": len(mismatches),
            "cpu_radius_max": max(v[1] for v in values),
            "gpu_radius_max": max(v[2] for v in values),
            "radius_runs": summarize_runs(values),
            "match_sigma_ranges": [[run["sigma_min"], run["sigma_max"]] for run in summarize_runs(matches)],
            "mismatch_sigma_ranges": [[run["sigma_min"], run["sigma_max"]] for run in summarize_runs(mismatches)],
            "matched_kernel_rmse_max": max((v for _, v in kernel_rmse), default=0.0),
            "matched_kernel_rmse_p95": float(np.percentile([v for _, v in kernel_rmse], 95)) if kernel_rmse else None,
            "matched_kernel_rmse_worst_sigma": max(kernel_rmse, key=lambda item: item[1])[0] if kernel_rmse else None,
        },
        "interpretation": "Support matches mean equal finite radius only. They do not establish full CPU/GPU parity. The 0.1 UI sampling is discrete and is not a proof over all representable float values.",
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(OUTPUT)
    print(f"support radius matches={len(matches)}/{len(values)} ({len(matches) / len(values):.2%})")
    print(f"matched 1D kernel RMSE p95/max={report['summary']['matched_kernel_rmse_p95']:.9g}/{report['summary']['matched_kernel_rmse_max']:.9g}")


if __name__ == "__main__":
    main()
