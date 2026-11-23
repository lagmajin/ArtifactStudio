"""Run source-mirror color transfer on a real portrait with fixed synthetic cast."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
ASSET = ROOT / "assets" / "WikimediaCC0" / "Pexels-photo-707448-1280.jpg"
OUT = ROOT / "captures" / "rd9_real_skin_white_scored"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
METHODS = ("Reinhard", "MeanStddev", "Histogram")
INTENSITIES = (0.25, 0.5, 1.0)

from run_rd1_source_mirror import (  # noqa: E402
    apply_histogram, apply_mean_std, apply_reinhard, rgb_to_lab, sha,
)


def gamma_to_linear(c: np.ndarray) -> np.ndarray:
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def stats(image: np.ndarray, target: np.ndarray, mask: np.ndarray) -> dict:
    raw = image[mask]
    clipped = np.clip(raw, 0.0, 1.0)
    target_pixels = target[mask]
    lab, lab_target = rgb_to_lab(clipped), rgb_to_lab(target_pixels)
    delta_e = np.linalg.norm(lab - lab_target, axis=1)
    linear = gamma_to_linear(clipped)
    L, a, b = lab.T
    chroma = np.hypot(a, b)
    hues = np.arctan2(b, a)
    mean_a, mean_b = float(a.mean()), float(b.mean())
    luma = linear @ np.array([0.2126, 0.7152, 0.0722])
    h, w = mask.shape
    image_linear = gamma_to_linear(np.clip(image, 0.0, 1.0))
    image_luma = image_linear @ np.array([0.2126, 0.7152, 0.0722])
    gy, gx = np.gradient(image_luma)
    gradient = np.hypot(gx, gy)[mask]
    return {
        "pixels": int(mask.sum()),
        "deltaE76_mean_to_synthetic_reference": float(delta_e.mean()),
        "deltaE76_p95_to_synthetic_reference": float(np.percentile(delta_e, 95)),
        "deltaE76_max_to_synthetic_reference": float(delta_e.max()),
        "encoded_sRGB_MAE_to_synthetic_reference": float(np.abs(raw - target_pixels).mean()),
        "encoded_sRGB_p95_to_synthetic_reference": float(np.percentile(np.abs(raw-target_pixels), 95)),
        "Lab_mean": [float(L.mean()), mean_a, mean_b],
        "Cstar_from_mean_ab": float(np.hypot(mean_a, mean_b)),
        "Cstar_pixel_mean": float(chroma.mean()),
        "Cstar_pixel_p95": float(np.percentile(chroma, 95)),
        "hue_circular_mean_deg": float((np.rad2deg(np.arctan2(np.sin(hues).mean(), np.cos(hues).mean())) + 360) % 360),
        "hue_resultant_length": float(np.hypot(np.sin(hues).mean(), np.cos(hues).mean())),
        "Lab_L_p05_p50_p95": [float(v) for v in np.percentile(L, [5, 50, 95])],
        "linear_Rec709_luminance_mean": float(luma.mean()),
        "linear_Rec709_luminance_gradient_mean": float(gradient.mean()),
        "linear_Rec709_luminance_gradient_p95": float(np.percentile(gradient, 95)),
        "clip_low_ratio": float(np.mean(raw < 0.0)),
        "clip_high_ratio": float(np.mean(raw > 1.0)),
        "clip_low_pixel_ratio": float(np.mean(np.any(raw.reshape(-1, 3) < 0.0, axis=1))),
        "clip_high_pixel_ratio": float(np.mean(np.any(raw.reshape(-1, 3) > 1.0, axis=1))),
        "non_finite_values": int(raw.size - np.count_nonzero(np.isfinite(raw))),
        "output_rgb_min_max": [float(raw.min()), float(raw.max())],
    }


def save_preview(path: Path, image: np.ndarray) -> None:
    encoded = np.uint8(np.clip(np.rint(np.clip(image, 0.0, 1.0) * 255.0), 0, 255))
    Image.fromarray(encoded, "RGB").save(path, format="PNG", optimize=False)


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit the equation mirrors")
    prep_manifest_path = PREP / "manifest.json"
    prep = json.loads(prep_manifest_path.read_text(encoding="utf-8"))
    if prep["manual_review"]["reviewed_before_candidate_scoring"] is not True:
        raise RuntimeError("The ROI review must precede candidate scoring")
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float64) / 255.0
    reference = np.asarray(Image.open(PREP / "synthetic_cast_reference.png").convert("RGB"), dtype=np.float64) / 255.0
    masks = {name: np.asarray(Image.open(PREP / item["mask"]).convert("L")) > 0
             for name, item in prep["roi"].items()}
    masks["full_frame"] = np.ones(source.shape[:2], dtype=bool)
    OUT.mkdir(parents=True, exist_ok=True)
    conditions = [{"method": "unchanged_source", "intensity": 0.0, "image": source}]
    for name, function in (("Reinhard", apply_reinhard), ("MeanStddev", apply_mean_std),
                           ("Histogram", apply_histogram)):
        for intensity in INTENSITIES:
            conditions.append({"method": name, "intensity": intensity,
                               "image": function(source, reference, intensity)})
    records = []
    for condition in conditions:
        method, intensity = condition["method"], condition["intensity"]
        image = condition["image"]
        key = "unchanged_source" if method == "unchanged_source" else f"{method}_i{intensity:.2f}"
        if method != "unchanged_source":
            np.save(OUT / f"{key}.npy", image.astype(np.float32), allow_pickle=False)
            save_preview(OUT / f"{key}.png", image)
        records.append({"method": method, "intensity": intensity,
                        "regions": {name: stats(image, reference, mask) for name, mask in masks.items()}})
    manifest = {
        "rd": "rd9-real-portrait-synthetic-cast-source-mirror",
        "status": "real portrait content with constructed cast; offline equation mirrors only; exploratory no acceptance gate",
        "measurement": "clipped sRGB decode to Lab D65/2-degree ΔE76; encoded RGB errors on unclipped mirror output; C*ab and circular hue on clipped display-sRGB; luminance gradient from decoded linear sRGB",
        "input": {"asset_sha256": sha(ASSET), "prep_manifest_sha256": sha(prep_manifest_path),
                  "source_png_sha256": sha(PREP / "source.png"), "reference_png_sha256": sha(PREP / "synthetic_cast_reference.png"),
                  "core_source_sha256": sha(CORE), "mirror_runner_sha256": sha(ROOT / "run_rd1_source_mirror.py")},
        "scoring_script": Path(__file__).name,
        "scoring_script_sha256": sha(Path(__file__)),
        "regions": {name: {"pixels": int(mask.sum()), "mask_sha256": None if name == "full_frame" else sha(PREP / prep["roi"][name]["mask"])}
                    for name, mask in masks.items()},
        "methods": list(METHODS), "intensities": list(INTENSITIES),
        "limitations": ["synthetic cast is not a paired real WB capture", "single portrait and one patterned shaded blouse",
                        "manual ROI selection was not independently reviewed", "no C++ differential or compiled Core output",
                        "no blinded human quality assessment, HDR/ICC/alpha test, or performance measurement"],
        "results": records,
    }
    files = {p.name: sha(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name != "manifest.json"}
    manifest["files"] = files
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    chosen = [{"method": r["method"], "intensity": r["intensity"],
               "skin_dE": r["regions"]["skin_combined_reviewed"]["deltaE76_mean_to_synthetic_reference"],
               "blouse_dE": r["regions"]["white_blouse_light_fabric_candidate"]["deltaE76_mean_to_synthetic_reference"],
               "full_dE": r["regions"]["full_frame"]["deltaE76_mean_to_synthetic_reference"]}
              for r in records]
    print(json.dumps({"capture": str(OUT), "conditions": chosen, "artifacts": len(files)}, indent=2))


if __name__ == "__main__":
    main()
