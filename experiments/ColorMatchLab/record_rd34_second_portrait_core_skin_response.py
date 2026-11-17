"""Describe saved Core color/lightness response on RD23's shadowed portrait."""
from __future__ import annotations

import csv
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd23_shadowed_portrait"
CORE_OUT = CAPTURE / "core_outputs"
MANIFEST_PATH = CAPTURE / "manifest.json"
OUT = ROOT / "captures" / "rd34_second_portrait_core_skin_response"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import rgb_to_lab  # noqa: E402


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def pct(values: np.ndarray, qs: tuple[int, ...] = (50, 90, 95)) -> dict:
    return {f"p{q}": float(np.percentile(values, q)) for q in qs}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    core_png_hashes = manifest["core_png_outputs"]
    source_path = CORE_OUT / "Reinhard_0.0.png"
    if sha(source_path) != core_png_hashes[source_path.name]:
        raise RuntimeError("RD23 zero-strength baseline differs from capture manifest")
    source = load_rgb(source_path)
    source_lab = rgb_to_lab(source)
    source_chroma = np.hypot(source_lab[..., 1], source_lab[..., 2])

    roi = manifest["roi"]["face_skin_candidate"]
    mask_path = CAPTURE / roi["mask"]
    if sha(mask_path) != roi["sha256"]:
        raise RuntimeError("RD23 face mask differs from capture manifest")
    mask = np.asarray(Image.open(mask_path).convert("L")) > 0
    if tuple(source.shape[:2][::-1]) != tuple(manifest["source"]["dimensions"]):
        raise RuntimeError("RD23 baseline dimensions differ from capture manifest")
    if int(mask.sum()) != int(roi["pixels"]):
        raise RuntimeError("RD23 face mask pixel count differs from capture manifest")

    conditions = [("unchanged_core_source", 0.0, source_path)]
    for method in ("Reinhard", "MeanStddev", "Histogram"):
        for strength in (0.5, 1.0):
            path = CORE_OUT / f"{method}_{strength:.1f}.png"
            if sha(path) != core_png_hashes[path.name]:
                raise RuntimeError(f"RD23 output differs from capture manifest: {path}")
            conditions.append((method, strength, path))

    rows = []
    for method, strength, path in conditions:
        rgb = load_rgb(path)
        lab = rgb_to_lab(rgb)
        delta = lab - source_lab
        chroma = np.hypot(lab[..., 1], lab[..., 2])
        delta_chroma = chroma - source_chroma
        source_hue = np.arctan2(source_lab[..., 2], source_lab[..., 1])
        output_hue = np.arctan2(lab[..., 2], lab[..., 1])
        delta_hue = np.abs(np.arctan2(np.sin(output_hue-source_hue),
                                      np.cos(output_hue-source_hue))) * 180.0 / np.pi
        hue_mask = mask & (source_chroma >= 5.0) & (chroma >= 5.0)
        endpoint = np.any((rgb == 0.0) | (rgb == 1.0), axis=2)
        l_delta = delta[..., 0][mask]
        a_delta = delta[..., 1][mask]
        b_delta = delta[..., 2][mask]
        rows.append({
            "method": method, "strength": strength, "input_file": path.name,
            "input_sha256": sha(path), "pixels": int(mask.sum()),
            "deltaE76_to_source": pct(np.linalg.norm(delta[mask], axis=1)),
            "delta_a_star_signed": pct(a_delta),
            "delta_b_star_signed": pct(b_delta),
            "delta_Cstar_signed": pct(delta_chroma[mask]),
            "absolute_hue_delta_deg": pct(delta_hue[hue_mask]),
            "hue_eligible_pixels_Cstar_ge_5_both": int(hue_mask.sum()),
            "signed_delta_Lstar": pct(l_delta),
            "absolute_delta_Lstar": pct(np.abs(l_delta)),
            "output_Lstar": pct(lab[..., 0][mask], (5, 50, 95)),
            "output_endpoint_pixel_fraction": float(endpoint[mask].mean()),
        })

    csv_path = OUT / "skin_response.csv"
    fields = list(rows[0])
    dict_fields = ("deltaE76_to_source", "delta_a_star_signed", "delta_b_star_signed",
                   "delta_Cstar_signed", "absolute_hue_delta_deg", "signed_delta_Lstar",
                   "absolute_delta_Lstar", "output_Lstar")
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({**row, **{key: json.dumps(row[key], separators=(",", ":"))
                                       for key in dict_fields}})

    manifest_out = {
        "rd": "rd34-second-portrait-actual-core-face-response",
        "status": "retrospective descriptive actual-Core preview analysis; no acceptance gate",
        "date": "2026-11-17",
        "evaluation_plan": "EVALUATION_PLAN.md#rd34-second-portrait-actual-core-face-colorlightness-response-predeclared-2026-11-17",
        "upstream": {"rd23_manifest_sha256": sha(MANIFEST_PATH),
                     "test": manifest["core_test"]["name"],
                     "test_source_sha256": manifest["core_test"]["test_source_sha256"],
                     "asset_sha256": manifest["source"]["sha256"]},
        "source": {"file": source_path.name, "sha256": sha(source_path),
                   "dimensions": list(manifest["source"]["dimensions"]),
                   "role": "Core-saved unchanged numeric baseline, not desired skin target"},
        "region": {"name": "face_skin_candidate", "file": mask_path.name,
                   "sha256": sha(mask_path), "pixels": int(mask.sum()),
                   "review_status": "single-review candidate, not independent annotation"},
        "conditions": ["unchanged_core_source"] + ["Reinhard", "MeanStddev", "Histogram"],
        "strengths": [0.5, 1.0],
        "metrics": {"space": "pinned RD9 rgb_to_lab D65/2 degree, decoded 8-bit encoded-sRGB PNG",
                    "hue_eligibility": "source and output C*ab >= 5; numerical stability only",
                    "endpoint": "any encoded RGB channel exactly 0 or 255; not proof of clipping",
                    "synthetic_reference": "excluded"},
        "analysis_script_sha256": sha(Path(__file__)),
        "lab_conversion_script_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
        "outputs": {row["input_file"]: row["input_sha256"] for row in rows},
        "limitations": [
            "preview outputs already viewed; retrospective, not blind review",
            "single second portrait; its face mask is source-reviewed only",
            "8-bit Core PNG previews, not retained float output",
            "constructed cast rather than paired WB capture or measured skin color",
            "no measured neutral-white region, demographic generalization, or acceptance gate",
        ],
        "row_count": len(rows), "csv_sha256": sha(csv_path),
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest_out, indent=2) + "\n",
                                         encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "rows": len(rows),
                      "csv_sha256": manifest_out["csv_sha256"],
                      "manifest_sha256": sha(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
