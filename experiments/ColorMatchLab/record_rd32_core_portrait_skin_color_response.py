"""Describe color response in already saved RD22 actual-Core PNG previews."""
from __future__ import annotations

import csv
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
CAPTURES = ROOT / "captures"
CORE_PNG = CAPTURES / "rd22_core_outputs"
CORE_MANIFEST = CAPTURES / "rd20_core_skin_white" / "manifest.json"
MASKS = CAPTURES / "rd9_real_skin_white_scene"
PREP_MANIFEST = MASKS / "manifest.json"
MIRRORS = CAPTURES / "rd9_real_skin_white_scored"
MIRROR_MANIFEST = MIRRORS / "manifest.json"
OUT = CAPTURES / "rd32_core_portrait_skin_color_response"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import rgb_to_lab  # noqa: E402


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rgb(path: Path) -> np.ndarray:
    if path.suffix == ".npy":
        return np.load(path, allow_pickle=False).astype(np.float64)
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def percentiles(values: np.ndarray) -> dict:
    return {f"p{int(q)}": float(np.percentile(values, q))
            for q in (50, 90, 95)}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    core_manifest = json.loads(CORE_MANIFEST.read_text(encoding="utf-8"))
    prep_manifest = json.loads(PREP_MANIFEST.read_text(encoding="utf-8"))
    mirror_manifest = json.loads(MIRROR_MANIFEST.read_text(encoding="utf-8"))
    source_path = CORE_PNG / "source.png"
    if sha(source_path) != core_manifest["core_png_outputs"][source_path.name]:
        raise RuntimeError("RD22 source PNG hash differs from its capture manifest")
    source = load_rgb(source_path)
    source_lab = rgb_to_lab(source)
    source_chroma = np.hypot(source_lab[..., 1], source_lab[..., 2])
    regions = {}
    for region, filename in (("face_skin_candidate", "roi_face_skin_reviewed.png"),
                             ("arm_skin_candidate", "roi_arm_skin_reviewed.png")):
        mask_path = MASKS / filename
        expected_mask_hash = prep_manifest["roi"][region.removesuffix("_candidate") + "_reviewed"]["sha256"]
        if sha(mask_path) != expected_mask_hash:
            raise RuntimeError(f"RD9 mask hash differs from prep manifest: {mask_path}")
        mask = np.asarray(Image.open(mask_path).convert("L")) > 0
        if mask.shape != source.shape[:2]:
            raise RuntimeError(f"Mask dimensions differ from Core source: {mask_path}")
        regions[region] = (mask, mask_path)

    rows = []
    output_hashes = {}
    for method in ("Reinhard", "MeanStddev", "Histogram"):
        for strength in (0.5, 1.0):
            for route, directory, suffix in (("actual_core_preview", CORE_PNG, "png"),
                                             ("equation_mirror", MIRRORS, "npy")):
                path = directory / f"{method}_i{strength:.2f}.{suffix}" if suffix == "npy" else directory / f"{method}_{strength:.1f}.png"
                expected_hash = (core_manifest["core_png_outputs"][path.name]
                                 if route == "actual_core_preview"
                                 else mirror_manifest["files"][path.name])
                if sha(path) != expected_hash:
                    raise RuntimeError(f"Capture output hash mismatch: {path}")
                rgb = load_rgb(path)
                if rgb.shape != source.shape:
                    raise RuntimeError(f"Output dimensions differ from source: {path}")
                lab = rgb_to_lab(np.clip(rgb, 0.0, 1.0))
                delta_lab = lab - source_lab
                source_c = source_chroma
                output_c = np.hypot(lab[..., 1], lab[..., 2])
                delta_c = output_c - source_c
                source_hue = np.arctan2(source_lab[..., 2], source_lab[..., 1])
                output_hue = np.arctan2(lab[..., 2], lab[..., 1])
                delta_hue = np.abs(np.arctan2(np.sin(output_hue-source_hue),
                                              np.cos(output_hue-source_hue))) * 180 / np.pi
                endpoint_pixel = np.any((rgb == 0.0) | (rgb == 1.0), axis=2)
                output_hashes[f"{route}/{path.name}"] = sha(path)
                for name, (mask, _) in regions.items():
                    eligible = mask & (source_c >= 5.0) & (output_c >= 5.0)
                    rows.append({
                        "route": route, "method": method, "strength": strength, "region": name,
                        "input_file": path.name, "input_sha256": sha(path),
                        "pixels": int(mask.sum()),
                        "deltaE76_to_saved_core_source": percentiles(
                            np.linalg.norm(delta_lab[mask], axis=1)),
                        "delta_a_star_signed": percentiles(delta_lab[..., 1][mask]),
                        "delta_b_star_signed": percentiles(delta_lab[..., 2][mask]),
                        "delta_Cstar_signed": percentiles(delta_c[mask]),
                        "absolute_hue_delta_deg": percentiles(delta_hue[eligible]),
                        "hue_eligible_pixels_Cstar_ge_5_both": int(eligible.sum()),
                        "output_endpoint_pixel_fraction": (
                            float(endpoint_pixel[mask].mean()) if route == "actual_core_preview" else None),
                    })

    fields = ["route", "method", "strength", "region", "input_file", "input_sha256", "pixels",
              "deltaE76_to_saved_core_source", "delta_a_star_signed",
              "delta_b_star_signed", "delta_Cstar_signed", "absolute_hue_delta_deg",
              "hue_eligible_pixels_Cstar_ge_5_both", "output_endpoint_pixel_fraction"]
    csv_path = OUT / "color_response.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({**row, **{key: json.dumps(row[key], separators=(",", ":"))
                for key in ("deltaE76_to_saved_core_source", "delta_a_star_signed",
                            "delta_b_star_signed", "delta_Cstar_signed",
                            "absolute_hue_delta_deg")}})

    mask_records = {name: {"file": path.name, "sha256": sha(path),
                           "pixels": int(mask.sum())}
                    for name, (mask, path) in regions.items()}
    manifest = {
        "rd": "rd32-saved-core-portrait-skin-color-response",
        "status": "retrospective actual-Core saved-preview and equation-mirror response diagnostic; no acceptance gate",
        "date": "2026-11-17",
        "evaluation_plan": "EVALUATION_PLAN.md#rd32-actual-core-saved-preview-portrait-skin-color-response-predeclared-2026-11-17",
        "core_capture": {"rd20_manifest_sha256": sha(CORE_MANIFEST),
                         "test": core_manifest["test"],
                         "core_source_sha256": core_manifest["inputs"]["core_source_sha256"],
                         "test_source_sha256": core_manifest["inputs"]["test_source_sha256"],
                         "asset_sha256": core_manifest["source"]["sha256"]},
        "upstream_manifests": {"rd9_prep_manifest_sha256": sha(PREP_MANIFEST),
                               "rd9_scored_mirror_manifest_sha256": sha(MIRROR_MANIFEST)},
        "source": {"file": source_path.name, "sha256": sha(source_path),
                   "dimensions": [source.shape[1], source.shape[0]],
                   "role": "same Core loader/save path baseline; not desired skin appearance"},
        "regions": mask_records,
        "outputs": output_hashes,
        "comparison": "Both routes use the same Core-saved source baseline. Core outputs are 8-bit PNG previews; mirror outputs are float32 equation results generated from the RD9 PIL-prepared source. This is descriptive only, not float parity.",
        "metrics": {"lab": "pinned RD9 rgb_to_lab mirror; D65/2 degree; encoded-sRGB inputs (8-bit Core PNG or float32 mirror)",
                    "hue_eligibility": "C*ab >= 5 in source and output; numeric stability rule only",
                    "endpoint": "fraction of mask pixels with any encoded channel equal to 0 or 255; not proof of clipping",
                    "synthetic_reference": "excluded"},
        "limitations": [
            "candidate previews had been viewed before this retrospective analysis",
            "Core route is 8-bit PNG previews, not retained Core float output",
            "mirror route was generated from RD9's separately PIL-prepared source; differences mix source preparation, float/PNG quantization, and Core/mirror implementation",
            "one unpaired CC0 portrait and masks not independently annotated",
            "Lab conversion assumes encoded sRGB; no measured colorimetry",
            "no measured neutral/white ROI, blind review, ranking, or acceptance gate",
        ],
        "analysis_script_sha256": sha(Path(__file__)),
        "lab_conversion_script_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
        "row_count": len(rows),
        "csv_sha256": sha(csv_path),
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "rows": len(rows),
                      "csv_sha256": manifest["csv_sha256"],
                      "manifest_sha256": sha(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
