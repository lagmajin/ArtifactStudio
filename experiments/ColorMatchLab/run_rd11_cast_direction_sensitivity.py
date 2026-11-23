"""Measure skin/white ROI response for fixed warm/cool/green cast targets."""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd11_cast_direction_sensitivity"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
CASTS = {
    "cool": ([0.94, 1.015, 1.075], [0.012, 0.006, -0.008]),
    "warm": ([1.06, 1.0, 0.94], [-0.008, 0.006, 0.012]),
    "green": ([0.98, 1.05, 0.98], [0.006, -0.006, 0.006]),
}
INTENSITIES = (0.5, 1.0)

from run_rd1_source_mirror import (  # noqa: E402
    apply_histogram, apply_mean_std, apply_reinhard, gamma_to_linear, rgb_to_lab, sha,
)


def lab_summary(lab: np.ndarray) -> dict:
    L, a, b = lab.T
    mean_a, mean_b = float(a.mean()), float(b.mean())
    hue = np.arctan2(b, a)
    return {
        "Lab_mean": [float(L.mean()), mean_a, mean_b],
        "Cstar_from_mean_ab": float(np.hypot(mean_a, mean_b)),
        "Cstar_pixel_mean": float(np.hypot(a, b).mean()),
        "hue_circular_mean_deg": float((np.rad2deg(np.arctan2(np.sin(hue).mean(), np.cos(hue).mean())) + 360) % 360),
    }


def measure(image: np.ndarray, source: np.ndarray, target: np.ndarray, mask: np.ndarray) -> dict:
    out = image[mask]
    source_pixels, target_pixels = source[mask], target[mask]
    lab = rgb_to_lab(np.clip(out, 0.0, 1.0))
    lab_src, lab_tgt = rgb_to_lab(source_pixels), rgb_to_lab(target_pixels)
    de_source = np.linalg.norm(lab - lab_src, axis=1)
    de_target = np.linalg.norm(lab - lab_tgt, axis=1)
    de_channels = np.abs(out - target_pixels)
    rgb_linear = gamma_to_linear(np.clip(image, 0.0, 1.0))
    luma = rgb_linear @ np.array([0.2126, 0.7152, 0.0722])
    gy, gx = np.gradient(luma)
    grad = np.hypot(gx, gy)[mask]
    return {
        "pixels": int(mask.sum()),
        "deltaE76_mean_to_source": float(de_source.mean()),
        "deltaE76_mean_to_constructed_target": float(de_target.mean()),
        "deltaE76_p95_to_constructed_target": float(np.percentile(de_target, 95)),
        "encoded_sRGB_MAE_to_constructed_target": float(de_channels.mean()),
        "linear_Rec709_luminance_gradient_mean": float(grad.mean()),
        "linear_Rec709_luminance_gradient_p95": float(np.percentile(grad, 95)),
        **lab_summary(lab),
        "channel_clip_low_ratio": float(np.mean(out < 0.0)),
        "channel_clip_high_ratio": float(np.mean(out > 1.0)),
        "pixel_clip_low_ratio": float(np.mean(np.any(out.reshape(-1, 3) < 0.0, axis=1))),
        "pixel_clip_high_ratio": float(np.mean(np.any(out.reshape(-1, 3) > 1.0, axis=1))),
        "non_finite_values": int(out.size - np.count_nonzero(np.isfinite(out))),
    }


def save_srgb(path: Path, image: np.ndarray) -> None:
    encoded = np.uint8(np.clip(np.rint(np.clip(image, 0.0, 1.0) * 255.0), 0, 255))
    Image.fromarray(encoded, "RGB").save(path, format="PNG", optimize=False)


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit equation mirrors")
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float64) / 255.0
    masks = {name: np.asarray(Image.open(PREP / item["mask"]).convert("L")) > 0
             for name, item in json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))["roi"].items()}
    masks["full_frame"] = np.ones(source.shape[:2], dtype=bool)
    funcs = {"Reinhard": apply_reinhard, "MeanStddev": apply_mean_std, "Histogram": apply_histogram}
    OUT.mkdir(parents=True, exist_ok=True)
    records = []
    reference_metrics = {}
    for cast_name, (scale, offset) in CASTS.items():
        target = np.clip(source * np.asarray(scale) + np.asarray(offset), 0.0, 1.0)
        reference_metrics[cast_name] = {
            name: {"pixels": int(mask.sum()), **lab_summary(rgb_to_lab(target[mask]))}
            for name, mask in masks.items()
        }
        np.save(OUT / f"{cast_name}_target.npy", target.astype(np.float32), allow_pickle=False)
        save_srgb(OUT / f"{cast_name}_target.png", target)
        conditions = [("unchanged_source", 0.0, source)]
        for method, function in funcs.items():
            for intensity in INTENSITIES:
                conditions.append((method, intensity, function(source, target, intensity)))
        for method, intensity, image in conditions:
            if method == "unchanged_source":
                key = f"{cast_name}_unchanged_source"
            else:
                key = f"{cast_name}_{method}_i{intensity:.2f}"
                np.save(OUT / f"{key}.npy", image.astype(np.float32), allow_pickle=False)
                save_srgb(OUT / f"{key}.png", image)
            records.append({"cast": cast_name, "scale": scale, "offset": offset,
                            "method": method, "intensity": intensity,
                            "regions": {name: measure(image, source, target, mask)
                                        for name, mask in masks.items()}})
    files = {p.name: sha(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name != "manifest.json"}
    manifest = {
        "rd": "rd11-portrait-cast-direction-sensitivity",
        "status": "three constructed encoded-sRGB casts; source equation mirrors only; descriptive no gate",
        "measurement": "CIE Lab D65/2-degree; per-pixel ΔE76; source-relative Lab/Cstar/hue; linear Rec.709 luminance gradient; raw mirror clipping before display clamp",
        "input": {"prep_manifest_sha256": sha(PREP / "manifest.json"),
                  "source_sha256": sha(PREP / "source.png"),
                  "core_source_sha256": sha(CORE),
                  "mirror_runner_sha256": sha(ROOT / "run_rd1_source_mirror.py")},
        "script": Path(__file__).name, "script_sha256": sha(Path(__file__)),
        "intensities": list(INTENSITIES), "regions": list(masks),
        "constructed_target_metrics": reference_metrics,
        "condition_count": len(records),
        "limitations": ["one CC0 image and analyst-reviewed masks", "casts are mathematical sRGB transforms, not real illuminants or captured WB targets",
                        "no C++/Core differential, independent review, product-path, alpha/HDR/ICC, or performance result"],
        "results": records, "files": files,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "conditions": len(records), "artifacts": len(files)}, indent=2))


if __name__ == "__main__":
    main()
