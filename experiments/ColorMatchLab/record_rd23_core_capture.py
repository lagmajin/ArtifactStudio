"""Record reproducible metadata and a fixed-view sheet for the RD23 Core run."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
CAPTURE = ROOT / "captures" / "rd23_shadowed_portrait"
OUTPUTS = CAPTURE / "core_outputs"
TEST_SOURCE = REPO / "tests" / "ArtifactCore" / "AutoColorMatcherContractTest.cpp"
FULL_SUITE_LOG = REPO / "temp" / "rd28_core_full.log"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    csv_path = OUTPUTS / "face_metrics.csv"
    with csv_path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    expected_methods = {"Reinhard", "MeanStddev", "Histogram"}
    expected_intensities = {"0", "0.5", "1"}
    if len(rows) != 9:
        raise RuntimeError(f"Expected 9 method/intensity rows, got {len(rows)}")
    if {row["method"] for row in rows} != expected_methods:
        raise RuntimeError("Incomplete RD23 method coverage")
    if {row["intensity"] for row in rows} != expected_intensities:
        raise RuntimeError("Incomplete RD23 intensity coverage")
    if {int(row["roi_pixels"]) for row in rows} != {25913}:
        raise RuntimeError("Unexpected RD23 ROI pixel count")
    numeric_fields = list(rows[0].keys())[1:]
    for row in rows:
        for field in numeric_fields:
            value = float(row[field])
            if not math.isfinite(value):
                raise RuntimeError(f"Non-finite {field}: {row}")

    performance_path = OUTPUTS / "performance.csv"
    with performance_path.open(newline="", encoding="utf-8") as stream:
        performance_rows = list(csv.DictReader(stream))
    expected_performance = {(name, method) for name in
                            ("portrait_1280x851", "portrait_1536x1024")
                            for method in expected_methods}
    if len(performance_rows) != 6 or {
            (row["image"], row["method"]) for row in performance_rows
    } != expected_performance:
        raise RuntimeError("Incomplete RD23 Core performance matrix")
    for row in performance_rows:
        for field in ("width", "height", "intensity", "samples",
                      "hardware_threads", "min_ms", "median_ms", "max_ms"):
            if not math.isfinite(float(row[field])):
                raise RuntimeError(f"Non-finite RD23 performance value {field}")

    columns = (
        ("Source", CAPTURE / "source.png"),
        ("Synthetic reference", CAPTURE / "synthetic_cast_reference.png"),
        ("Reinhard 1.0", OUTPUTS / "Reinhard_1.0.png"),
        ("MeanStddev 1.0", OUTPUTS / "MeanStddev_1.0.png"),
        ("Histogram 1.0", OUTPUTS / "Histogram_1.0.png"),
    )
    images = [(label, Image.open(path).convert("RGB")) for label, path in columns]
    tile_w, header_h, full_h, face_h = 320, 32, 213, 300
    sheet = Image.new("RGB", (tile_w * len(images), header_h + full_h + header_h + face_h),
                      (32, 33, 36))
    draw = ImageDraw.Draw(sheet)
    for col, (label, image) in enumerate(images):
        x = col * tile_w
        draw.text((x + 8, 8), label, fill=(245, 245, 245))
        full = image.copy()
        full.thumbnail((tile_w - 12, full_h - 8), Image.Resampling.LANCZOS)
        sheet.paste(full, (x + (tile_w - full.width) // 2,
                           header_h + (full_h - full.height) // 2))
        face_y = header_h + full_h
        if col == 0:
            draw.text((8, face_y + 8), "Fixed face crop", fill=(200, 210, 220))
        crop = image.crop((625, 205, 855, 455))
        crop.thumbnail((tile_w - 12, face_h - 8), Image.Resampling.LANCZOS)
        sheet.paste(crop, (x + (tile_w - crop.width) // 2,
                           face_y + header_h + (face_h - crop.height) // 2))
    review_path = CAPTURE / "core_visual_review.png"
    sheet.save(review_path, format="PNG", optimize=False)

    manifest_path = CAPTURE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["status"] = "exploratory Core synthetic-cast stress measurement completed; not acceptance"
    manifest["core_test"] = {
        "name": "AutoColorMatcherContractTest.CoreImageOverloadMeasuresShadowedPortraitSyntheticCast",
        "test_source_sha256": sha(TEST_SOURCE),
        "roi_pixels": 25913,
        "metrics_csv": "core_outputs/face_metrics.csv",
        "run_log": str(FULL_SUITE_LOG.relative_to(REPO)),
        "methods": sorted(expected_methods),
        "intensities": [0.0, 0.5, 1.0],
        "metrics": "D65/2-degree Lab mean and nearest-rank p95/max source-relative DeltaE76, descriptive fraction above DeltaE76 2, and synthetic-reference mean DeltaE76",
        "role": "actual Core ImageF32x4_RGBA overload output against the source and constructed synthetic cast; not captured WB ground truth",
        "full_suite_run_log": str(FULL_SUITE_LOG.relative_to(REPO)),
        "full_suite_run_log_sha256": sha(FULL_SUITE_LOG),
    }
    manifest["metrics"] = {"rows": rows, "sha256": sha(csv_path)}
    manifest["performance"] = {
        "csv": "core_outputs/performance.csv",
        "sha256": sha(performance_path),
        "rows": performance_rows,
        "measurement": "raw-buffer Core match() call only; one warm-up and seven timed samples per method/resolution; source restore, IO and reporting excluded; median/min/max in milliseconds",
        "scope": "single Visual Studio Debug run on two fixed portrait dimensions; descriptive CPU Core timing only, no app budget or acceptance threshold",
    }
    manifest["core_png_outputs"] = {
        path.name: sha(path) for path in sorted(OUTPUTS.glob("*.png"))
    }
    manifest["core_visual_review"] = {
        "file": review_path.name,
        "sha256": sha(review_path),
        "view": "source, synthetic reference, and three actual Core strength-1 outputs; full frame and fixed face crop",
        "review_status": "single unblinded R&D inspection; no independent acceptance review",
        "acceptance": False,
    }
    manifest["limits"] = sorted(set(manifest["limits"] + [
        "single-review hand-drawn face ROI candidate",
        "synthetic cast rather than paired white-balance capture",
        "no neutral-white ROI",
        "no independent or blind visual acceptance",
        "DeltaE76 greater than 2 is descriptive, not a quality threshold",
    ]))
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n",
                             encoding="utf-8", newline="\n")
    print(json.dumps({"rows": len(rows), "roi_pixels": 25913,
                      "visual_review": str(review_path),
                      "visual_review_sha256": sha(review_path),
                      "metrics_sha256": sha(csv_path)}, indent=2))


if __name__ == "__main__":
    main()
