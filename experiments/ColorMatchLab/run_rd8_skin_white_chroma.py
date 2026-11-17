"""Decompose skin-proxy hue/chroma and white neutrality for RD6 outputs."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd6_colorchecker_pair"
OUT = ROOT / "captures" / "rd8_skin_white_chroma"
RD6_MANIFEST = CAPTURE / "manifest.json"
RUNNER = ROOT / "run_rd1_source_mirror.py"
CORE = ROOT.parent.parent / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
EXPECTED_CORE_SHA256 = "62b4354eff5de8462b007632a801fe7bb930220b338e2ea2eb7533b82e60a4ab"

from run_rd1_source_mirror import rgb_to_lab, sha  # noqa: E402

CONDITIONS = [("unchanged_source", 0.0)] + [
    (f"{method}_i{intensity:.2f}", intensity)
    for method in ("Reinhard", "MeanStddev", "Histogram")
    for intensity in (0.25, 0.5, 1.0)
]


def circular_mean_deg(values: np.ndarray) -> float:
    radians = np.deg2rad(values)
    angle = np.rad2deg(np.arctan2(np.sin(radians).mean(), np.cos(radians).mean()))
    return float((angle + 360.0) % 360.0)


def summarize(rgb: np.ndarray, mask: np.ndarray) -> dict:
    lab = rgb_to_lab(np.clip(rgb[mask], 0.0, 1.0))
    L, a, b = lab.T
    chroma = np.hypot(a, b)
    hue = np.rad2deg(np.arctan2(b, a)) % 360.0
    mean_a, mean_b = float(a.mean()), float(b.mean())
    return {
        "pixels": int(mask.sum()),
        "Lab_mean": [float(L.mean()), mean_a, mean_b],
        "Cstar_from_mean_ab": float(np.hypot(mean_a, mean_b)),
        "Cstar_pixel_mean": float(chroma.mean()),
        "Cstar_pixel_p95": float(np.percentile(chroma, 95)),
        "hue_circular_mean_deg": circular_mean_deg(hue),
        "hue_circular_resultant_length": float(np.hypot(np.sin(np.deg2rad(hue)).mean(),
                                                          np.cos(np.deg2rad(hue)).mean())),
        "Lab_L_p05_p50_p95": [float(v) for v in np.percentile(L, [5, 50, 95])],
        "a_p05_p50_p95": [float(v) for v in np.percentile(a, [5, 50, 95])],
        "b_p05_p50_p95": [float(v) for v in np.percentile(b, [5, 50, 95])],
    }


def delta_e(lab_a: list[float], lab_b: list[float]) -> float:
    return float(np.linalg.norm(np.asarray(lab_a) - np.asarray(lab_b)))


def main() -> None:
    if sha(CORE) != EXPECTED_CORE_SHA256:
        raise RuntimeError("AutoColorMatch.cpp changed; re-audit the equation mirrors")
    rd6 = json.loads(RD6_MANIFEST.read_text(encoding="utf-8"))
    source = np.load(CAPTURE / "source_wb1_srgb.npy", allow_pickle=False)
    reference = np.load(CAPTURE / "reference_wb2_srgb.npy", allow_pickle=False)
    # Patch masks are reconstructed from the fixed RD6 grid definition.
    skin_masks = []
    for col in (0, 1):
        mask = np.zeros(source.shape[:2], dtype=bool)
        mask[20:80, col * 101 + 20:col * 101 + 80] = True
        skin_masks.append(mask)
    white_mask = np.zeros(source.shape[:2], dtype=bool)
    white_mask[3 * 101 + 20:3 * 101 + 80, 20:80] = True
    regions = {
        "skin_dark_proxy_patch_r0_c0": skin_masks[0],
        "skin_light_proxy_patch_r0_c1": skin_masks[1],
        "skin_proxy_combined": skin_masks[0] | skin_masks[1],
        "white_patch_r3_c0": white_mask,
    }
    reference_stats = {name: summarize(reference, mask) for name, mask in regions.items()}
    source_stats = {name: summarize(source, mask) for name, mask in regions.items()}
    records = []
    for name, intensity in CONDITIONS:
        image = source if name == "unchanged_source" else np.load(CAPTURE / f"{name}.npy", allow_pickle=False)
        row = {"condition": name, "intensity": intensity, "regions": {}}
        for region, mask in regions.items():
            stats = summarize(image, mask)
            stats["deltaE76_of_mean_Lab_to_source"] = delta_e(stats["Lab_mean"], source_stats[region]["Lab_mean"])
            stats["deltaE76_of_mean_Lab_to_reference"] = delta_e(stats["Lab_mean"], reference_stats[region]["Lab_mean"])
            stats["hue_shift_deg_from_source"] = float((stats["hue_circular_mean_deg"] - source_stats[region]["hue_circular_mean_deg"] + 180) % 360 - 180)
            stats["hue_delta_deg_to_reference"] = float(abs((stats["hue_circular_mean_deg"] - reference_stats[region]["hue_circular_mean_deg"] + 180) % 360 - 180))
            stats["Cstar_delta_to_reference"] = stats["Cstar_from_mean_ab"] - reference_stats[region]["Cstar_from_mean_ab"]
            row["regions"][region] = stats
        records.append(row)
    OUT.mkdir(parents=True, exist_ok=True)
    result = {
        "rd": "rd8-colorchecker-skin-white-chroma-decomposition",
        "status": "offline metric decomposition of RD6 source-mirror outputs; exploratory, no acceptance gate",
        "measurement": "Lab D65/2-degree from clipped sRGB decode; Cstar=sqrt(mean(a)^2+mean(b)^2), per-pixel chroma mean/p95; hue is circular mean of atan2(b,a); ΔE76 between mean Lab vectors",
        "interpretation_limit": "ColorChecker skin-tone patches are proxies, not human skin; WB2 is camera auto-WB, not calibrated ground truth. Metrics do not establish perceptual preservation or image quality.",
        "inputs": {"rd6_manifest_sha256": sha(RD6_MANIFEST),
                   "rd6_source_array_sha256": sha(CAPTURE / "source_wb1_srgb.npy"),
                   "rd6_reference_array_sha256": sha(CAPTURE / "reference_wb2_srgb.npy"),
                   "core_source_sha256": sha(CORE), "mirror_runner_sha256": sha(RUNNER)},
        "regions": {name: {"pixels": int(mask.sum()), "source": source_stats[name], "reference": reference_stats[name]}
                    for name, mask in regions.items()},
        "conditions": records,
    }
    output = OUT / "metrics.json"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    manifest = {"script": Path(__file__).name, "script_sha256": sha(Path(__file__)),
                "metrics_sha256": sha(output), "rd6_manifest_sha256": sha(RD6_MANIFEST),
                "conditions": len(records), "regions": list(regions)}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "manifest": manifest,
                      "source_vs_reference": {name: {
                          "source_Lab": source_stats[name]["Lab_mean"],
                          "reference_Lab": reference_stats[name]["Lab_mean"],
                          "source_Cstar": source_stats[name]["Cstar_from_mean_ab"],
                          "reference_Cstar": reference_stats[name]["Cstar_from_mean_ab"],
                          "source_hue": source_stats[name]["hue_circular_mean_deg"],
                          "reference_hue": reference_stats[name]["hue_circular_mean_deg"],
                      } for name in regions}}, indent=2))


if __name__ == "__main__":
    main()
