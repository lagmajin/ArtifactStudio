"""Record descriptive per-pixel color response for RD9 portrait mask candidates."""
from __future__ import annotations

import csv
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
SCORED = ROOT / "captures" / "rd9_real_skin_white_scored"
OUT = ROOT / "captures" / "rd31_portrait_skin_color_response"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import rgb_to_lab  # noqa: E402


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rgb(path: Path) -> np.ndarray:
    if path.suffix == ".npy":
        return np.load(path, allow_pickle=False).astype(np.float64)
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def summarize(values: np.ndarray) -> dict:
    return {f"p{int(p)}": float(np.percentile(values, p)) for p in (50, 90, 95)}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    source_path = PREP / "source.png"
    source = read_rgb(source_path)
    regions = {}
    for name, filename in (("face_skin_candidate", "roi_face_skin_reviewed.png"),
                           ("arm_skin_candidate", "roi_arm_skin_reviewed.png")):
        mask_path = PREP / filename
        regions[name] = (np.asarray(Image.open(mask_path).convert("L")) > 0, mask_path)

    conditions = [("unchanged_source", 0.0, source_path)]
    for method in ("Reinhard", "MeanStddev", "Histogram"):
        for strength in (0.25, 0.5, 1.0):
            conditions.append((method, strength,
                               SCORED / f"{method}_i{strength:.2f}.npy"))

    source_lab = rgb_to_lab(np.clip(source, 0.0, 1.0))
    rows = []
    for method, strength, path in conditions:
        rgb = read_rgb(path)
        if rgb.shape != source.shape:
            raise RuntimeError(f"Unexpected image dimensions: {path}: {rgb.shape}")
        lab = rgb_to_lab(np.clip(rgb, 0.0, 1.0))
        delta = lab - source_lab
        source_chroma = np.hypot(source_lab[..., 1], source_lab[..., 2])
        output_chroma = np.hypot(lab[..., 1], lab[..., 2])
        chroma_delta = output_chroma - source_chroma
        source_hue = np.arctan2(source_lab[..., 2], source_lab[..., 1])
        output_hue = np.arctan2(lab[..., 2], lab[..., 1])
        hue_delta = np.abs(np.arctan2(np.sin(output_hue-source_hue),
                                     np.cos(output_hue-source_hue))) * 180.0 / np.pi

        for region, (mask, _) in regions.items():
            lab_pixels = delta[mask]
            raw_pixels = rgb[mask]
            eligible = mask & (source_chroma >= 5.0) & (output_chroma >= 5.0)
            hue_values = hue_delta[eligible]
            rows.append({
                "method": method, "strength": strength, "region": region,
                "input_file": path.name, "input_sha256": sha(path),
                "pixels": int(mask.sum()),
                "deltaE76_to_source": summarize(np.linalg.norm(lab_pixels, axis=1)),
                "delta_a_star_signed": summarize(lab_pixels[:, 1]),
                "delta_b_star_signed": summarize(lab_pixels[:, 2]),
                "delta_Cstar_signed": summarize(chroma_delta[mask]),
                "absolute_hue_delta_deg": summarize(hue_values) if hue_values.size else None,
                "hue_eligible_pixels_Cstar_ge_5_both": int(eligible.sum()),
                "below_0_channel_fraction": float(np.mean(raw_pixels < 0.0)),
                "above_1_channel_fraction": float(np.mean(raw_pixels > 1.0)),
                "nonfinite_channel_count": int(raw_pixels.size-
                                                np.count_nonzero(np.isfinite(raw_pixels))),
            })

    output_csv = OUT / "color_response.csv"
    fields = ["method", "strength", "region", "input_file", "input_sha256", "pixels",
              "deltaE76_to_source", "delta_a_star_signed", "delta_b_star_signed",
              "delta_Cstar_signed", "absolute_hue_delta_deg",
              "hue_eligible_pixels_Cstar_ge_5_both", "below_0_channel_fraction",
              "above_1_channel_fraction", "nonfinite_channel_count"]
    with output_csv.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({**row,
                **{key: json.dumps(row[key], separators=(",", ":"))
                   for key in ("deltaE76_to_source", "delta_a_star_signed",
                               "delta_b_star_signed", "delta_Cstar_signed",
                               "absolute_hue_delta_deg")}})

    masks = {name: {"file": path.name, "sha256": sha(path),
                    "pixels": int(mask.sum())}
             for name, (mask, path) in regions.items()}
    mirror = ROOT / "run_rd1_source_mirror.py"
    manifest = {
        "rd": "rd31-portrait-skin-candidate-color-response",
        "status": "exploratory descriptive equation-mirror response; no acceptance gate",
        "date": "2026-11-17",
        "evaluation_plan": "EVALUATION_PLAN.md#rd31-cc0-portrait-candidate-skin-color-response-distribution-predeclared-2026-11-17",
        "input": {"source_file": source_path.name, "source_sha256": sha(source_path),
                  "dimensions": [source.shape[1], source.shape[0]],
                  "assumption": "encoded sRGB; no ICC profile in retrieved source"},
        "regions": masks,
        "conditions": {"methods": ["unchanged_source", "Reinhard", "MeanStddev", "Histogram"],
                       "strengths": [0.25, 0.5, 1.0]},
        "metrics": {"lab": "pinned RD9 rgb_to_lab mirror; D65/2 degree; clipped encoded sRGB inputs",
                    "hue_eligibility": "C*ab >= 5 in both source and output; numerical stability convention only",
                    "source_role": "numeric baseline only, not a desired skin appearance",
                    "reference": "excluded"},
        "script_sha256": sha(Path(__file__)),
        "mirror_script_sha256": sha(mirror),
        "limitations": ["single portrait and hand-reviewed, not independently annotated mask candidates",
                        "equation mirrors, not current compiled Core output",
                        "no measured colorimetry or paired photographic reference",
                        "not perceptual skin-tone validation; no method ranking or acceptance gate"],
        "row_count": len(rows),
        "csv_sha256": sha(output_csv),
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "rows": len(rows),
                      "csv_sha256": manifest["csv_sha256"],
                      "manifest_sha256": sha(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
