"""Record Lab L* response for RD32's existing Core and mirror candidates."""
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
MIRRORS = CAPTURES / "rd9_real_skin_white_scored"
MIRROR_MANIFEST = MIRRORS / "manifest.json"
MASKS = CAPTURES / "rd9_real_skin_white_scene"
PREP_MANIFEST = MASKS / "manifest.json"
RD32_MANIFEST = CAPTURES / "rd32_core_portrait_skin_color_response" / "manifest.json"
OUT = CAPTURES / "rd33_portrait_skin_lightness_response"
sys.path.insert(0, str(ROOT))
from run_rd1_source_mirror import rgb_to_lab  # noqa: E402


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rgb(path: Path) -> np.ndarray:
    if path.suffix == ".npy":
        return np.load(path, allow_pickle=False).astype(np.float64)
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def pct(values: np.ndarray, quantiles: tuple[int, ...]) -> dict:
    return {f"p{q}": float(np.percentile(values, q)) for q in quantiles}


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    core_manifest = json.loads(CORE_MANIFEST.read_text(encoding="utf-8"))
    mirror_manifest = json.loads(MIRROR_MANIFEST.read_text(encoding="utf-8"))
    prep_manifest = json.loads(PREP_MANIFEST.read_text(encoding="utf-8"))
    rd32_manifest = json.loads(RD32_MANIFEST.read_text(encoding="utf-8"))
    source_path = CORE_PNG / "source.png"
    if sha(source_path) != core_manifest["core_png_outputs"][source_path.name]:
        raise RuntimeError("RD22 source PNG hash differs from its capture manifest")
    source_lab = rgb_to_lab(load_rgb(source_path))
    regions = {}
    for name, filename in (("face_skin_candidate", "roi_face_skin_reviewed.png"),
                           ("arm_skin_candidate", "roi_arm_skin_reviewed.png")):
        path = MASKS / filename
        key = "face_skin_reviewed" if name == "face_skin_candidate" else "arm_skin_reviewed"
        if sha(path) != prep_manifest["roi"][key]["sha256"]:
            raise RuntimeError(f"RD9 mask hash differs from prep manifest: {path}")
        regions[name] = (np.asarray(Image.open(path).convert("L")) > 0, path)

    rows = []
    outputs = {}
    for method in ("Reinhard", "MeanStddev", "Histogram"):
        for strength in (0.5, 1.0):
            for route, directory, ext, filename in (
                ("actual_core_preview", CORE_PNG, "png", f"{method}_{strength:.1f}.png"),
                ("equation_mirror", MIRRORS, "npy", f"{method}_i{strength:.2f}.npy"),
            ):
                path = directory / filename
                expected_hash = (core_manifest["core_png_outputs"][path.name]
                                 if route == "actual_core_preview"
                                 else mirror_manifest["files"][path.name])
                if sha(path) != expected_hash:
                    raise RuntimeError(f"Capture output hash mismatch: {path}")
                rgb = np.clip(load_rgb(path), 0.0, 1.0)
                lab = rgb_to_lab(rgb)
                signed_delta = lab[..., 0] - source_lab[..., 0]
                abs_delta = np.abs(signed_delta)
                outputs[f"{route}/{path.name}"] = sha(path)
                for region, (mask, _) in regions.items():
                    rows.append({
                        "route": route, "method": method, "strength": strength,
                        "region": region, "input_file": path.name,
                        "input_sha256": sha(path), "pixels": int(mask.sum()),
                        "signed_delta_Lstar": pct(signed_delta[mask], (50, 90, 95)),
                        "absolute_delta_Lstar": pct(abs_delta[mask], (50, 90, 95)),
                        "output_Lstar": pct(lab[..., 0][mask], (5, 50, 95)),
                    })

    csv_path = OUT / "lightness_response.csv"
    fields = ["route", "method", "strength", "region", "input_file", "input_sha256",
              "pixels", "signed_delta_Lstar", "absolute_delta_Lstar", "output_Lstar"]
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({**row, **{key: json.dumps(row[key], separators=(",", ":"))
                                       for key in fields[-3:]}})

    masks = {name: {"file": path.name, "sha256": sha(path),
                    "pixels": int(mask.sum())}
             for name, (mask, path) in regions.items()}
    manifest = {
        "rd": "rd33-portrait-skin-candidate-lightness-response",
        "status": "retrospective descriptive response; no acceptance gate",
        "date": "2026-11-17",
        "evaluation_plan": "EVALUATION_PLAN.md#rd33-actual-core-portrait-candidate-skin-lightness-response-predeclared-2026-11-17",
        "source": {"file": source_path.name, "sha256": sha(source_path),
                   "dimensions": [1280, 851], "role": "numeric baseline, not target skin brightness"},
        "upstream_manifests": {"rd20_core_manifest_sha256": sha(CORE_MANIFEST),
                               "rd9_prep_manifest_sha256": sha(PREP_MANIFEST),
                               "rd9_scored_mirror_manifest_sha256": sha(MIRROR_MANIFEST),
                               "rd32_color_response_manifest_sha256": sha(RD32_MANIFEST)},
        "core_capture": {"test": core_manifest["test"],
                         "core_source_sha256": core_manifest["inputs"]["core_source_sha256"],
                         "test_source_sha256": core_manifest["inputs"]["test_source_sha256"],
                         "asset_sha256": core_manifest["source"]["sha256"]},
        "regions": masks, "outputs": outputs,
        "routes": ["actual_core_preview", "equation_mirror"],
        "metrics": {"space": "pinned RD9 rgb_to_lab D65/2 degree from clipped encoded-sRGB values",
                    "signed_delta": "per-pixel output L* minus common source L*",
                    "absolute_delta": "absolute per-pixel L* change",
                    "output_lightness": "per-pixel output L* distribution"},
        "analysis_script_sha256": sha(Path(__file__)),
        "lab_conversion_script_sha256": sha(ROOT / "run_rd1_source_mirror.py"),
        "limitations": [
            "retrospective; output previews had already been viewed",
            "one unpaired portrait and non-independent candidate masks",
            "Core output is 8-bit preview; mirrors come from separately PIL-prepared input",
            "not exposure preference, skin acceptance, measured colorimetry, white preservation, or float parity",
        ],
        "row_count": len(rows), "csv_sha256": sha(csv_path),
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                       encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "rows": len(rows),
                      "csv_sha256": manifest["csv_sha256"],
                      "manifest_sha256": sha(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
