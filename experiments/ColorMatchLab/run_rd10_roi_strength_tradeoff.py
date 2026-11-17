"""Probe manual-ROI protection as a post-match blend tradeoff."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
RD9 = ROOT / "captures" / "rd9_real_skin_white_scored"
OUT = ROOT / "captures" / "rd10_roi_strength_tradeoff"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"
FACTORS = (0.0, 0.5, 1.0)

from run_rd1_source_mirror import gamma_to_linear, rgb_to_lab, sha  # noqa: E402


def lab_summary(lab: np.ndarray) -> dict:
    L, a, b = lab.T
    mean_a, mean_b = float(a.mean()), float(b.mean())
    hue = np.arctan2(b, a)
    return {"Lab_mean": [float(L.mean()), mean_a, mean_b],
            "Cstar_from_mean_ab": float(np.hypot(mean_a, mean_b)),
            "Cstar_pixel_mean": float(np.hypot(a, b).mean()),
            "hue_circular_mean_deg": float((np.rad2deg(np.arctan2(np.sin(hue).mean(), np.cos(hue).mean())) + 360) % 360)}


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit the equation mirrors")
    prep_manifest = json.loads((PREP / "manifest.json").read_text(encoding="utf-8"))
    rd9_manifest_path = RD9 / "manifest.json"
    rd9_manifest = json.loads(rd9_manifest_path.read_text(encoding="utf-8"))
    source = np.asarray(Image.open(PREP / "source.png").convert("RGB"), dtype=np.float64) / 255.0
    reference = np.asarray(Image.open(PREP / "synthetic_cast_reference.png").convert("RGB"), dtype=np.float64) / 255.0
    masks = {name: np.asarray(Image.open(PREP / item["mask"]).convert("L")) > 0
             for name, item in prep_manifest["roi"].items()}
    masks["full_frame"] = np.ones(source.shape[:2], dtype=bool)
    regions = list(masks)
    source_lab = {name: rgb_to_lab(source[mask]) for name, mask in masks.items()}
    target_lab = {name: rgb_to_lab(reference[mask]) for name, mask in masks.items()}
    base_outputs = {method: np.load(RD9 / f"{method}_i1.00.npy", allow_pickle=False).astype(np.float64)
                    for method in ("Reinhard", "MeanStddev", "Histogram")}

    records = []
    for method, base in base_outputs.items():
        for skin_factor in FACTORS:
            for blouse_factor in FACTORS:
                out = base.copy()
                skin = masks["skin_combined_reviewed"]
                blouse = masks["white_blouse_light_fabric_candidate"]
                out[skin] = source[skin] + (base[skin] - source[skin]) * skin_factor
                out[blouse] = source[blouse] + (base[blouse] - source[blouse]) * blouse_factor
                record = {"method": method, "global_intensity": 1.0,
                          "skin_factor": skin_factor, "blouse_factor": blouse_factor,
                          "regions": {}}
                for name, mask in masks.items():
                    raw = out[mask]
                    lab = rgb_to_lab(np.clip(raw, 0.0, 1.0))
                    de_source = np.linalg.norm(lab - source_lab[name], axis=1)
                    de_target = np.linalg.norm(lab - target_lab[name], axis=1)
                    lab_stats = lab_summary(lab)
                    record["regions"][name] = {
                        "pixels": int(mask.sum()),
                        "deltaE76_mean_to_input": float(de_source.mean()),
                        "deltaE76_mean_to_synthetic_reference": float(de_target.mean()),
                        "encoded_sRGB_MAE_to_input": float(np.abs(raw - source[mask]).mean()),
                        "encoded_sRGB_MAE_to_synthetic_reference": float(np.abs(raw - reference[mask]).mean()),
                        **lab_stats,
                        "clip_low_ratio": float(np.mean(raw < 0.0)),
                        "clip_high_ratio": float(np.mean(raw > 1.0)),
                        "non_finite_values": int(raw.size - np.count_nonzero(np.isfinite(raw))),
                    }
                records.append(record)

    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "rd": "rd10-manual-roi-strength-tradeoff",
        "status": "offline post-match ROI blend probe; not AutoColorMatcher implementation; no acceptance gate",
        "recipe": "For the fixed RD9 intensity-1 global output, apply output=source+factor*(global_output-source) in encoded sRGB independently inside reviewed skin and blouse masks. Outside both masks use the global output.",
        "factor_semantics": {"0.0": "preserve source pixels in that ROI", "0.5": "midpoint blend in encoded sRGB", "1.0": "unmodified global match in that ROI"},
        "input": {"rd9_prep_manifest_sha256": sha(PREP / "manifest.json"),
                  "rd9_scored_manifest_sha256": sha(rd9_manifest_path),
                  "core_source_sha256": sha(CORE), "mirror_runner_sha256": sha(ROOT / "run_rd1_source_mirror.py")},
        "scoring_script": Path(__file__).name,
        "scoring_script_sha256": sha(Path(__file__)),
        "regions": {name: {"pixels": int(mask.sum()), "mask_sha256": None if name == "full_frame" else sha(PREP / prep_manifest["roi"][name]["mask"])}
                    for name, mask in masks.items()},
        "methods": list(base_outputs), "global_intensity": 1.0,
        "factors": list(FACTORS), "combination_count": len(records),
        "limitations": ["single image and constructed cast", "manual fixed masks are oracle regions, not automatic segmentation",
                        "post-match encoded-sRGB blending is not a Core method or measured product path",
                        "the blouse is patterned and shaded, not calibrated neutral white", "no independent blind visual review or performance measurement"],
        "results": records,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "conditions": len(records), "regions": regions,
                      "pins": manifest["input"]}, indent=2))


if __name__ == "__main__":
    main()
