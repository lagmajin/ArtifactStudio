"""Measure portrait skin/light-fabric response across constructed cast strengths."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
OUT = ROOT / "captures" / "rd12_cast_strength_sensitivity"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
CASTS = {
    "cool": ([0.94, 1.015, 1.075], [0.012, 0.006, -0.008]),
    "warm": ([1.06, 1.0, 0.94], [-0.008, 0.006, 0.012]),
    "green": ([0.98, 1.05, 0.98], [0.006, -0.006, 0.006]),
}
STRENGTHS = (0.5, 1.0, 1.5)
INTENSITIES = (0.5, 1.0)

from run_rd1_source_mirror import (  # noqa: E402
    apply_histogram, apply_mean_std, apply_reinhard, gamma_to_linear, rgb_to_lab,
)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def lab_summary(lab: np.ndarray) -> dict:
    L, a, b = lab.T
    hue = np.arctan2(b, a)
    return {
        "Lab_mean": [float(L.mean()), float(a.mean()), float(b.mean())],
        "Cstar_from_mean_ab": float(np.hypot(a.mean(), b.mean())),
        "Cstar_pixel_mean": float(np.hypot(a, b).mean()),
        "hue_circular_mean_deg": float((np.rad2deg(np.arctan2(np.sin(hue).mean(), np.cos(hue).mean())) + 360.0) % 360.0),
    }


def measure(image: np.ndarray, source: np.ndarray, target: np.ndarray,
            mask: np.ndarray) -> dict:
    out = image[mask]
    source_pixels, target_pixels = source[mask], target[mask]
    lab = rgb_to_lab(np.clip(out, 0.0, 1.0))
    lab_src = rgb_to_lab(source_pixels)
    lab_tgt = rgb_to_lab(target_pixels)
    de_source = np.linalg.norm(lab - lab_src, axis=1)
    de_target = np.linalg.norm(lab - lab_tgt, axis=1)
    de_rgb_target = np.abs(out - target_pixels)
    linear = gamma_to_linear(np.clip(image, 0.0, 1.0))
    luma = linear @ np.array([0.2126, 0.7152, 0.0722])
    gy, gx = np.gradient(luma)
    gradient = np.hypot(gx, gy)[mask]
    return {
        "pixels": int(mask.sum()),
        "deltaE76_mean_to_source": float(de_source.mean()),
        "deltaE76_p95_to_source": float(np.percentile(de_source, 95)),
        "deltaE76_mean_to_constructed_target": float(de_target.mean()),
        "deltaE76_p95_to_constructed_target": float(np.percentile(de_target, 95)),
        "encoded_sRGB_MAE_to_constructed_target": float(de_rgb_target.mean()),
        "encoded_sRGB_p95_to_constructed_target": float(np.percentile(de_rgb_target, 95)),
        "linear_Rec709_luminance_gradient_mean": float(gradient.mean()),
        "linear_Rec709_luminance_gradient_p95": float(np.percentile(gradient, 95)),
        **lab_summary(lab),
        "channel_clip_low_ratio": float(np.mean(out < 0.0)),
        "channel_clip_high_ratio": float(np.mean(out > 1.0)),
        "pixel_clip_low_ratio": float(np.mean(np.any(out < 0.0, axis=1))),
        "pixel_clip_high_ratio": float(np.mean(np.any(out > 1.0, axis=1))),
        "non_finite_values": int(out.size - np.count_nonzero(np.isfinite(out))),
    }


def save_preview(path: Path, image: np.ndarray) -> None:
    encoded = np.uint8(np.clip(np.rint(np.clip(image, 0.0, 1.0) * 255.0), 0, 255))
    Image.fromarray(encoded, "RGB").save(path, format="PNG", optimize=False)


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit equation mirrors")
    prep_manifest = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float64) / 255.0
    masks = {name: np.asarray(Image.open(PREP / roi["mask"]).convert("L")) > 0
             for name, roi in prep_manifest["roi"].items()}
    masks["full_frame"] = np.ones(source.shape[:2], dtype=bool)
    methods = {"Reinhard": apply_reinhard, "MeanStddev": apply_mean_std,
               "Histogram": apply_histogram}
    OUT.mkdir(parents=True, exist_ok=True)
    records: list[dict] = []
    target_summaries: dict[str, dict] = {}

    for direction, (scale_values, offset_values) in CASTS.items():
        scale = np.asarray(scale_values, dtype=np.float64)
        offset = np.asarray(offset_values, dtype=np.float64)
        for strength in STRENGTHS:
            cast_id = f"{direction}_s{strength:.1f}"
            target = np.clip(source * scale + offset * strength, 0.0, 1.0)
            np.save(OUT / f"{cast_id}_target.npy", target.astype(np.float32), allow_pickle=False)
            save_preview(OUT / f"{cast_id}_target.png", target)
            target_summaries[cast_id] = {
                name: {"pixels": int(mask.sum()), **lab_summary(rgb_to_lab(target[mask]))}
                for name, mask in masks.items()
            }
            conditions = [("unchanged_source", 0.0, source)]
            for method, function in methods.items():
                for intensity in INTENSITIES:
                    conditions.append((method, intensity, function(source, target, intensity)))
            for method, intensity, image in conditions:
                if method != "unchanged_source":
                    key = f"{cast_id}_{method}_i{intensity:.2f}"
                    np.save(OUT / f"{key}.npy", image.astype(np.float32), allow_pickle=False)
                    save_preview(OUT / f"{key}.png", image)
                records.append({
                    "direction": direction, "strength": strength,
                    "scale": scale_values, "offset": offset_values,
                    "offset_multiplier": strength,
                    "method": method, "intensity": intensity,
                    "regions": {name: measure(image, source, target, mask)
                                for name, mask in masks.items()},
                })

    files = {path.name: sha(path) for path in sorted(OUT.iterdir())
             if path.is_file() and path.name != "manifest.json"}
    manifest = {
        "rd": "rd12-portrait-constructed-cast-strength-sensitivity",
        "status": "predeclared descriptive probe; nine constructed targets; source equation mirrors only; no quality gate",
        "measurement": "CIE Lab D65/2-degree; per-pixel DeltaE76 mean/p95 to source and target; encoded-sRGB MAE/p95; Lab Cstar/hue; linear Rec.709 luminance gradient mean/p95; raw mirror clipping and non-finite values",
        "input": {
            "prep_manifest_sha256": sha(PREP / "manifest.json"),
            "source_sha256": sha(PREP / "source.png"),
            "core_source_sha256": sha(CORE),
            "mirror_runner_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
        },
        "cast_model": "clip(source_sRGB * direction_scale + direction_offset * strength, 0, 1)",
        "directions": {name: {"scale": scale, "offset": offset}
                       for name, (scale, offset) in CASTS.items()},
        "strengths": list(STRENGTHS),
        "intensities": list(INTENSITIES),
        "regions": list(masks),
        "condition_count": len(records),
        "constructed_target_metrics": target_summaries,
        "limitations": [
            "one CC0 portrait and analyst-reviewed masks; blouse is patterned and not a calibrated neutral",
            "cast targets are mathematical encoded-sRGB transforms, not photographed WB references or physical illuminants",
            "equations are offline source mirrors, not compiled Core output; no independent or blinded visual review",
            "no alpha/hidden-RGB, ICC/HDR, backend, product-path or performance evidence",
        ],
        "script": Path(__file__).name,
        "script_sha256": sha(Path(__file__)),
        "results": records,
        "files": files,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "conditions": len(records),
                      "targets": len(target_summaries), "artifacts": len(files)}, indent=2))


if __name__ == "__main__":
    main()
