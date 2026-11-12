#!/usr/bin/env python3
"""Conditional Add Noise alpha association/compositing equation study."""
from __future__ import annotations

import hashlib
import json
import platform
from pathlib import Path

import cv2
import numpy as np

from add_noise_route_equation_audit import evaluate

ROOT = Path(__file__).resolve().parents[2]
PLAN = ROOT / "experiments/FilmEffectsLab/plans/add-noise-alpha-compositing-r1.json"
ROUTE_SCRIPT = ROOT / "experiments/FilmEffectsLab/add_noise_route_equation_audit.py"
SOURCE = ROOT / "Artifact/src/Effects/AddNoise/AddNoiseEffect.cpp"
OUT = ROOT / "experiments/FilmEffectsLab/captures/add-noise-alpha-compositing-r1"
W, H = 256, 128
REGIONS = {
    "transparent": lambda a: a == 0,
    "low_alpha": lambda a: (a > 0) & (a <= .1),
    "edge": lambda a: (a > .1) & (a < .9),
    "near_opaque": lambda a: (a >= .9) & (a < 1),
    "opaque": lambda a: a == 1,
    "whole": lambda a: np.ones(a.shape, dtype=bool),
}
BGS = {"black": np.zeros(3, np.float32), "gray": np.full(3, .18, np.float32),
       "white": np.ones(3, np.float32)}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture() -> tuple[np.ndarray, np.ndarray]:
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    radius = np.sqrt(((xx - 128) / 78) ** 2 + ((yy - 64) / 42) ** 2)
    alpha = np.clip((1.08 - radius) * 8, 0, 1).astype(np.float32)
    u, v = xx / (W - 1), yy / (H - 1)
    rgb = np.stack((.12 + .42 * u, .18 + .34 * v, .68 - .26 * u), axis=-1)
    checker = ((xx.astype(np.int32) // 4 + yy.astype(np.int32) // 4) % 2) == 0
    rgb[checker & (u > .72)] = (.12, .76, .91)
    for x in range(8, W, 17):
        rgb[:, max(0, x - 1):x + 1] = .96
    straight = np.dstack((rgb[..., ::-1], alpha)).astype(np.float32)
    premul = straight.copy()
    premul[..., :3] *= alpha[..., None]
    return straight, premul


def composite(stored_bgra: np.ndarray, bg_rgb: np.ndarray, association: str) -> np.ndarray:
    rgb = stored_bgra[..., :3][..., ::-1]
    a = stored_bgra[..., 3:4]
    if association == "straight":
        return rgb * a + bg_rgb * (1 - a)
    return rgb + bg_rgb * (1 - a)


def stats(delta: np.ndarray) -> dict:
    x = np.abs(delta.astype(np.float64)).reshape(-1)
    return {"mae": float(x.mean()), "rmse": float(np.sqrt(np.mean(x * x))),
            "p99_abs": float(np.quantile(x, .99)), "max_abs": float(x.max()) if x.size else 0.0}


def save_preview(path: Path, rgb: np.ndarray) -> None:
    u8 = np.rint(np.clip(rgb[..., ::-1], 0, 1) * 255).astype(np.uint8)
    cv2.imwrite(str(path), u8)


def main() -> dict:
    OUT.mkdir(parents=True, exist_ok=True)
    straight, premul = fixture()
    h, w = straight.shape[:2]
    alpha = straight[..., 3]
    source = SOURCE.read_text(encoding="utf-8")
    markers = {"numeric_seed": "seed = static_cast<uint32_t>(seed_)" in source,
               "floor_block_coordinates": "std::floor(static_cast<float>(x) / size_)" in source,
               "rgb_clamp": "std::clamp(p[0] + noiseAt" in source and "std::clamp(p[2] + noiseAt" in source,
               "alpha_passthrough_by_rgb_only_writes": "dst = src;" in source and "p[2] = std::clamp" in source}
    if not all(markers.values()):
        raise RuntimeError(f"Source markers changed: {markers}")

    results = {}
    for association, image in (("straight", straight), ("premultiplied", premul)):
        baseline = evaluate(image, 0, 1.5, 73, "color")
        results[association] = {"region_pixel_counts": {}, "amounts": {}}
        for name, mask_fn in REGIONS.items():
            results[association]["region_pixel_counts"][name] = int(mask_fn(alpha).sum())
        base_composites = {bg: composite(baseline, color, "straight" if association == "straight" else "premultiplied")
                           for bg, color in BGS.items()}
        for amount in (.2, .4):
            candidate = evaluate(image, amount, 1.5, 73, "color")
            record = {"amount": amount, "alpha_max_abs_delta": float(np.max(np.abs(candidate[..., 3] - alpha))),
                      "premultiplied_rgb_gt_alpha_fraction": (float(np.mean(candidate[..., :3] > candidate[..., 3:4] + 1e-6)) if association == "premultiplied" else None),
                      "stored_rgb_delta": {}, "composite_delta": {}}
            for name, mask_fn in REGIONS.items():
                mask = mask_fn(alpha)
                record["stored_rgb_delta"][name] = stats(candidate[mask, :3] - baseline[mask, :3])
                record["composite_delta"][name] = {}
                for bg, color in BGS.items():
                    out_comp = composite(candidate, color, "straight" if association == "straight" else "premultiplied")
                    record["composite_delta"][name][bg] = stats(out_comp[mask] - base_composites[bg][mask])
            trans = alpha == 0
            record["transparent_rgb_delta_mean_abs"] = float(np.mean(np.abs(candidate[trans, :3] - baseline[trans, :3]))) if trans.any() else 0.0
            results[association]["amounts"][str(amount)] = record
            for bg, color in BGS.items():
                save_preview(OUT / f"{association}-amount{amount}-{bg}.png", composite(candidate, color, "straight" if association == "straight" else "premultiplied"))
        save_preview(OUT / f"{association}-input-over-gray.png", base_composites["gray"])

    # Predicted route pair: CPU rasterizer receives premultiplied storage;
    # resident Composition GPU receives straight storage after layer conversion.
    # These remain independent equation mirrors, not product captures.
    paired = {"effect_off_baseline": {}, "amounts": {}}
    for bg, color in BGS.items():
        cpu_base = composite(premul, color, "premultiplied")
        gpu_base = composite(straight, color, "straight")
        paired["effect_off_baseline"][bg] = stats(cpu_base - gpu_base)
    for amount in (.2, .4):
        cpu_candidate = evaluate(premul, amount, 1.5, 73, "color")
        gpu_candidate = evaluate(straight, amount, 1.5, 73, "color")
        amount_record = {"amount": amount, "regions": {}}
        for region, mask_fn in REGIONS.items():
            mask = mask_fn(alpha)
            amount_record["regions"][region] = {}
            for bg, color in BGS.items():
                cpu_output = composite(cpu_candidate, color, "premultiplied")
                gpu_output = composite(gpu_candidate, color, "straight")
                amount_record["regions"][region][bg] = stats(cpu_output[mask] - gpu_output[mask])
                # High-gain display-only difference image; metrics use raw float output.
                if region == "whole":
                    save_preview(OUT / f"cpu-premul-minus-gpu-straight-amount{amount}-{bg}-x4.png",
                                 np.clip(np.abs(cpu_output - gpu_output) * 4.0, 0.0, 1.0))
        paired["amounts"][str(amount)] = amount_record

    report = {
        "plan_id": "add-noise-alpha-compositing-r1", "plan_sha256": sha(PLAN),
        "script_sha256": sha(Path(__file__)), "route_equation_runner_sha256": sha(ROUTE_SCRIPT),
        "source_sha256": sha(SOURCE), "python": platform.python_version(),
        "numpy": np.__version__, "opencv": cv2.__version__, "size": [w, h],
        "parameters": {"seed": 73, "size": 1.5, "amounts": [0, .2, .4], "mode": "color"},
        "regions": {k: v for k, v in results["straight"]["region_pixel_counts"].items()},
        "source_markers_verified": markers,
        "limits": "Synthetic NumPy equation mirror only; alpha association and runtime compositor are assumptions in each branch, not observed product facts.",
        "results": results, "source_path_predicted_route_pair": paired,
    }
    report_path = OUT / "metrics.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    (OUT / "metrics.sha256").write_text(f"{sha(report_path)}  metrics.json\n", encoding="ascii", newline="\n")
    print(json.dumps({"metrics": str(report_path), "plan_sha256": report["plan_sha256"],
                      "script_sha256": report["script_sha256"], "markers": markers,
                      "png_count": len(list(OUT.glob("*.png"))),
                      "route_pair_amounts": list(paired["amounts"])}, indent=2))
    return report


if __name__ == "__main__":
    main()
