"""Validate and freeze the RD27 actual-Core natural-scene ROI diagnostic."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd27_core_natural_scene"
RD7 = ROOT / "captures" / "rd7_natural_scene_pair"
RD18 = ROOT / "captures" / "rd18_lamp_white_roi"
TEST_SOURCE = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
CORE_SOURCE = REPO / "ArtifactCore" / "src" / "Color" / "AutoColorMatch.cpp"
RUN_LOG = REPO / "temp" / "rd28_core_test.log"
METHODS = ("Reinhard", "MeanStddev", "Histogram")
INTENSITIES = ("0.25", "0.5", "1")
REGION_PIXELS = {"whole_frame": 667000, "left_lampshade": 1222,
                 "right_lampshade": 1141}


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    rows_path = CAPTURE / "core_roi_metrics.csv"
    differential_path = CAPTURE / "mirror_differential.csv"
    with rows_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    with differential_path.open(newline="", encoding="utf-8") as stream:
        differentials = list(csv.DictReader(stream))
    expected_roi = {(method, intensity, region)
                    for method in METHODS for intensity in INTENSITIES
                    for region in REGION_PIXELS}
    if len(rows) != 27 or {
            (row["method"], row["intensity"], row["region"]) for row in rows
    } != expected_roi:
        raise RuntimeError("RD27 Core ROI matrix is incomplete")
    expected_diff = {(method, intensity) for method in METHODS
                     for intensity in INTENSITIES}
    if len(differentials) != 9 or {
            (row["method"], row["intensity"]) for row in differentials
    } != expected_diff:
        raise RuntimeError("RD27 Core/mirror differential matrix is incomplete")

    numeric_fields = tuple(key for key in rows[0] if key not in
                           ("method", "intensity", "region", "pixels"))
    for row in rows:
        if int(row["pixels"]) != REGION_PIXELS[row["region"]]:
            raise RuntimeError(f"RD27 ROI size mismatch: {row}")
        if not all(math.isfinite(float(row[key])) for key in numeric_fields):
            raise RuntimeError(f"Non-finite RD27 metric: {row}")
    for row in differentials:
        if int(row["channels"]) != 667000 * 3:
            raise RuntimeError(f"Unexpected RD27 differential sample count: {row}")
        if not all(math.isfinite(float(row[key]))
                   for key in ("mae", "p95", "max")):
            raise RuntimeError(f"Non-finite RD27 differential: {row}")

    test_log = RUN_LOG.read_text(encoding="utf-8", errors="replace")
    if "100% tests passed, 0 tests failed out of 1" not in test_log:
        raise RuntimeError("RD27 CTest log does not show a passing target")
    source_pair = json.loads((RD7 / "manifest.json").read_text(encoding="utf-8"))
    roi_review = json.loads((RD18 / "roi_review.json").read_text(encoding="utf-8"))
    if roi_review.get("status") != "accepted_for_candidate_scoring":
        raise RuntimeError("RD27 source-only ROI review is not accepted")

    source_path = RD7 / "source_srgb_from_linear_raw.npy"
    reference_path = RD7 / "reference_camera_jpeg_srgb.npy"
    left_mask = RD18 / "left_lampshade_mask.png"
    right_mask = RD18 / "right_lampshade_mask.png"
    for mask_path, region in ((left_mask, "left_lampshade"),
                              (right_mask, "right_lampshade")):
        with Image.open(mask_path) as image:
            if image.size != (1000, 667):
                raise RuntimeError(f"Unexpected mask dimensions: {mask_path}")
            pixels = sum(value > 0 for value in image.convert("L").tobytes())
        if pixels != REGION_PIXELS[region]:
            raise RuntimeError(f"RD27 mask pixel count mismatch: {mask_path}")

    mirror_manifest = json.loads((RD18 / "manifest.json").read_text(encoding="utf-8"))
    comparison = []
    for row in rows:
        if row["intensity"] != "1":
            continue
        mirror_candidate = next(item for item in mirror_manifest["candidates"]
                                if item["method"] == row["method"]
                                and item["intensity"] == 1.0)
        mirror_value = mirror_candidate["regions"][row["region"]]["reference"]["deltaE76_mean"]
        comparison.append({"method": row["method"], "region": row["region"],
                           "core_reference_delta_e76_mean": float(row["reference_delta_e76_mean"]),
                           "rd18_mirror_reference_delta_e76_mean": mirror_value,
                           "absolute_difference": abs(float(row["reference_delta_e76_mean"]) - mirror_value)})

    output_names = ("source.png", "reference.png", "Reinhard_i1.0.png",
                    "MeanStddev_i1.0.png", "Histogram_i1.0.png",
                    "core_comparison_sheet.png")
    outputs = {}
    for name in output_names:
        path = CAPTURE / name
        with Image.open(path) as image:
            expected_size = (1500, 737) if name == "core_comparison_sheet.png" else (1000, 667)
            if image.size != expected_size:
                raise RuntimeError(f"Unexpected RD27 output dimensions: {path}")
        outputs[name] = sha(path)

    manifest = {
        "rd": "rd27-middlebury-natural-scene-actual-core-lamp-rois",
        "status": "actual Core descriptive diagnostic; no acceptance gate",
        "pair": {
            "dataset": "Middlebury registered EOS 5D natural-scene RAW/JPEG",
            "source_page": source_pair["dataset"]["source_page"],
            "permission": source_pair["dataset"]["permission_note"],
            "source": str(source_path.relative_to(ROOT)),
            "source_sha256": sha(source_path),
            "reference": str(reference_path.relative_to(ROOT)),
            "reference_sha256": sha(reference_path),
            "reference_role": "camera rendition, not colorimetric ground truth",
            "dimensions": [1000, 667],
            "transfer_assumption": "source derived from linear RGB using sRGB OETF; reference treated as encoded sRGB",
        },
        "roi": {
            "review": "source-only analyst review; not independent and does not establish neutral material",
            "review_sha256": sha(RD18 / "roi_review.json"),
            "regions": {
                "left_lampshade": {"pixels": 1222, "mask": str(left_mask.relative_to(ROOT)),
                                    "mask_sha256": sha(left_mask)},
                "right_lampshade": {"pixels": 1141, "mask": str(right_mask.relative_to(ROOT)),
                                     "mask_sha256": sha(right_mask)},
                "whole_frame": {"pixels": 667000, "mask": None},
            },
        },
        "conditions": {
            "methods": list(METHODS), "intensities": [0.25, 0.5, 1.0],
            "metrics_csv": rows_path.name, "mirror_differential_csv": differential_path.name,
            "metrics_sha256": sha(rows_path), "mirror_differential_sha256": sha(differential_path),
            "comparison": "Lab D65/2-degree DeltaE76 source-relative/reference-relative mean/p95/max, encoded RGB MAE, mean Lab-vector C*ab; full-frame RGB absolute difference to existing equation-mirror float arrays",
            "mirror_roi_mean_reference_delta_comparison": comparison,
        },
        "implementation": {
            "test": "AutoColorMatcherContractTest.CoreImageOverloadMatchesMiddleburyNaturalSceneRawBuffer",
            "test_source_sha256": sha(TEST_SOURCE),
            "core_source_sha256": sha(CORE_SOURCE),
            "record_script_sha256": sha(Path(__file__).resolve()),
            "visual_sheet_script_sha256": sha(ROOT / "make_rd27_visual_review.py"),
            "test_log": str(RUN_LOG.relative_to(REPO)), "test_log_sha256": sha(RUN_LOG),
            "configuration": "Visual Studio Debug; CTest target only",
        },
        "outputs": outputs,
        "visual_review": {
            "sheet": "core_comparison_sheet.png",
            "sheet_sha256": outputs["core_comparison_sheet.png"],
            "review": "one unblinded R&D inspection of fixed full-frame and source-reviewed ROI crops; no acceptance review",
            "observations": [
                "All three strength-1 Core outputs visibly change the lamp shade appearance from the source toward the camera JPEG in this scene.",
                "Histogram appears closest to the camera JPEG in these two crops; this is a one-scene descriptive observation and not a method acceptance or ranking.",
            ],
        },
        "limits": [
            "one warm-lit kitchen scene and one camera",
            "lamp shades are visually selected but not measured for neutrality",
            "camera JPEG is processed rendition, not a WB/colorimetric target",
            "ROI review is source-only but performed by one analyst",
            "no human skin, user-supplied pair, blind review, product caller, GPU parity, or acceptance threshold",
        ],
        "results": rows,
        "mirror_differentials": differentials,
    }
    path = CAPTURE / "manifest.json"
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"rows": len(rows), "mirror_differentials": len(differentials),
                      "max_roi_mean_delta_from_rd18": max(item["absolute_difference"] for item in comparison),
                      "manifest_sha256": sha(path)}, indent=2))


if __name__ == "__main__":
    main()
