"""Record descriptive local-detail response for the existing RD9 portrait mirrors."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
PREP = ROOT / "captures" / "rd9_real_skin_white_scene"
SCORED = ROOT / "captures" / "rd9_real_skin_white_scored"
OUT = ROOT / "captures" / "rd30_portrait_skin_detail"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rgb(path: Path) -> np.ndarray:
    if path.suffix == ".npy":
        return np.load(path, allow_pickle=False).astype(np.float64)
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def decode_srgb(values: np.ndarray) -> np.ndarray:
    values = np.clip(values, 0.0, 1.0)
    return np.where(values <= 0.04045, values / 12.92,
                    ((values + 0.055) / 1.055) ** 2.4)


def region_metrics(rgb: np.ndarray, mask: np.ndarray) -> dict:
    linear = decode_srgb(rgb)
    luminance = linear @ np.array([0.2126, 0.7152, 0.0722])
    gy, gx = np.gradient(luminance)
    gradient = np.hypot(gx, gy)

    padded = np.pad(luminance, 1, mode="reflect")
    sum_values = np.zeros_like(luminance)
    sum_squares = np.zeros_like(luminance)
    for dy in range(3):
        for dx in range(3):
            window = padded[dy:dy + luminance.shape[0],
                            dx:dx + luminance.shape[1]]
            sum_values += window
            sum_squares += window * window
    local_rms = np.sqrt(np.maximum(0.0, sum_squares / 9.0 -
                                   (sum_values / 9.0) ** 2))

    # Count local contrast only when every sample in its 3x3 window is inside
    # the ROI; outside pixels cannot dilute boundary neighborhoods.
    interior = np.zeros_like(mask)
    interior[1:-1, 1:-1] = mask[1:-1, 1:-1]
    for dy in range(3):
        for dx in range(3):
            interior[1:-1, 1:-1] &= mask[dy:dy + luminance.shape[0] - 2,
                                           dx:dx + luminance.shape[1] - 2]

    grad_values = gradient[mask]
    contrast_values = local_rms[interior]
    measured = rgb[mask]
    return {
        "pixels": int(mask.sum()),
        "local_rms_centers": int(interior.sum()),
        "gradient_p50": float(np.percentile(grad_values, 50)),
        "gradient_p90": float(np.percentile(grad_values, 90)),
        "gradient_p95": float(np.percentile(grad_values, 95)),
        "local_rms_contrast_p50": float(np.percentile(contrast_values, 50)),
        "local_rms_contrast_p90": float(np.percentile(contrast_values, 90)),
        "local_rms_contrast_p95": float(np.percentile(contrast_values, 95)),
        "below_0_channel_fraction": float(np.mean(measured < 0.0)),
        "above_1_channel_fraction": float(np.mean(measured > 1.0)),
        "nonfinite_channel_count": int(measured.size -
                                       np.count_nonzero(np.isfinite(measured))),
    }


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    source_path = PREP / "source.png"
    source = read_rgb(source_path)
    regions = {"full_frame": np.ones(source.shape[:2], dtype=bool)}
    mask_records = {}
    for region, filename in (
        ("face_skin_candidate", "roi_face_skin_reviewed.png"),
        ("arm_skin_candidate", "roi_arm_skin_reviewed.png"),
    ):
        path = PREP / filename
        mask = np.asarray(Image.open(path).convert("L")) > 0
        regions[region] = mask
        mask_records[region] = {"file": filename, "sha256": sha(path),
                                "pixels": int(mask.sum())}

    conditions = [("unchanged_source", 0.0, source_path)]
    for method in ("Reinhard", "MeanStddev", "Histogram"):
        for strength in (0.25, 0.5, 1.0):
            conditions.append((method, strength,
                               SCORED / f"{method}_i{strength:.2f}.npy"))

    source_stats = {name: region_metrics(source, mask)
                    for name, mask in regions.items()}
    rows = []
    for method, strength, path in conditions:
        rgb = read_rgb(path)
        if rgb.shape != source.shape:
            raise RuntimeError(f"Unexpected image dimensions: {path}: {rgb.shape}")
        for region, mask in regions.items():
            current = region_metrics(rgb, mask)
            row = {"method": method, "strength": strength, "region": region,
                   "input_file": path.name, "input_sha256": sha(path), **current}
            for key in ("gradient_p50", "gradient_p90", "gradient_p95",
                        "local_rms_contrast_p50", "local_rms_contrast_p90",
                        "local_rms_contrast_p95"):
                baseline = source_stats[region][key]
                row[f"{key}_ratio_to_source"] = (
                    current[key] / baseline if baseline else None)
            rows.append(row)

    csv_path = OUT / "detail_metrics.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    mirror_script = ROOT / "run_rd1_source_mirror.py"
    score_script = ROOT / "run_rd9_real_skin_white_scene.py"
    manifest = {
        "rd": "rd30-portrait-skin-region-detail-response",
        "status": "exploratory retrospective equation-mirror diagnostic; no acceptance gate",
        "date": "2026-11-17",
        "evaluation_plan": "EVALUATION_PLAN.md#rd30-cc0-portrait-skin-region-detail-response-predeclared-2026-11-17",
        "source": {"file": "source.png", "sha256": sha(source_path),
                   "size": [source.shape[1], source.shape[0]],
                   "provenance": "Wikimedia CC0 1.0; see RD9 prep manifest",
                   "assumption": "encoded sRGB; no ICC profile"},
        "reference_excluded": True,
        "regions": mask_records,
        "methods": ["unchanged_source", "Reinhard", "MeanStddev", "Histogram"],
        "strengths": [0.25, 0.5, 1.0],
        "metrics": {
            "luminance": "linear sRGB Rec.709",
            "gradient": "centered one-pixel finite differences, magnitude percentiles",
            "local_contrast": "3x3 box population standard deviation; center retained only if full window inside mask",
            "clipping": "out-of-range channel fraction before display clipping",
        },
        "script_hashes": {mirror_script.name: sha(mirror_script),
                          score_script.name: sha(score_script),
                          Path(__file__).name: sha(Path(__file__))},
        "limitations": [
            "one portrait; no paired photographic reference",
            "hand-reviewed masks not independently annotated or accepted as ground truth",
            "equation mirrors, not current compiled Core outputs",
            "tone changes confound luminance-gradient response with texture visibility",
            "no acceptance thresholds, ranking, or product-quality decision",
        ],
        "row_count": len(rows),
        "csv_sha256": sha(csv_path),
    }
    (OUT / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"capture": str(OUT), "row_count": len(rows),
                      "csv_sha256": manifest["csv_sha256"],
                      "manifest_sha256": sha(OUT / "manifest.json")}, indent=2))


if __name__ == "__main__":
    main()
