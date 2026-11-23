"""Measure descriptive Lab a*/b*/C*ab shifts in RD24 chart ROIs."""
from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd24_colorchecker_core"
METRICS = CAPTURE / "core_patch_metrics.csv"
SOURCE_IMAGE = CAPTURE / "source_wb1.png"
REFERENCE_IMAGE = CAPTURE / "reference_wb2.png"
OUTPUT = CAPTURE / "chroma_summary.json"
EXPECTED_PIXELS = {"skin_tone_proxy": 7200, "white_patch": 3600, "neutral_gray": 18000}


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    with METRICS.open(newline="", encoding="utf-8") as stream:
        core_rows = list(csv.DictReader(stream))
    if len(core_rows) != 27:
        raise RuntimeError(f"Expected 27 RD24 rows, got {len(core_rows)}")
    baselines = {}
    for region, expected_count in EXPECTED_PIXELS.items():
        row = next(item for item in core_rows if item["region"] == region)
        count = int(row["pixels"])
        if count != expected_count:
            raise RuntimeError(f"Unexpected {region} ROI size: {count}")
        baselines[region] = {
            "source": {"pixels": count, "mean_L": float(row["source_mean_L"]),
                       "mean_a": float(row["source_mean_a"]),
                       "mean_b": float(row["source_mean_b"]),
                       "mean_Cstar": float(row["source_mean_Cstar"])},
            "camera_auto_wb": {"pixels": count, "mean_L": float(row["wb2_mean_L"]),
                                "mean_a": float(row["wb2_mean_a"]),
                                "mean_b": float(row["wb2_mean_b"]),
                                "mean_Cstar": float(row["wb2_mean_Cstar"])},
        }
    output_rows = []
    for row in core_rows:
        region = row["region"]
        source_lab = baselines[region]["source"]
        wb2_lab = baselines[region]["camera_auto_wb"]
        output_a = float(row["output_mean_a"])
        output_b = float(row["output_mean_b"])
        output_c = math.hypot(output_a, output_b)
        output_rows.append({
            "method": row["method"],
            "intensity": float(row["intensity"]),
            "region": region,
            "pixels": int(row["pixels"]),
            "source_mean_a": source_lab["mean_a"],
            "output_mean_a": output_a,
            "wb2_mean_a": wb2_lab["mean_a"],
            "delta_a_from_source": output_a - source_lab["mean_a"],
            "source_mean_b": source_lab["mean_b"],
            "output_mean_b": output_b,
            "wb2_mean_b": wb2_lab["mean_b"],
            "delta_b_from_source": output_b - source_lab["mean_b"],
            "source_mean_Cstar": source_lab["mean_Cstar"],
            "output_mean_Cstar": output_c,
            "wb2_mean_Cstar": wb2_lab["mean_Cstar"],
            "delta_Cstar_from_source": output_c - source_lab["mean_Cstar"],
        })
    summary = {
        "rd": "rd24-chart-roi-chroma-and-neutrality-descriptive-analysis",
        "status": "exploratory; not natural-skin/white preservation or acceptance",
        "metrics_sha256": sha(METRICS),
        "source_png_sha256": sha(SOURCE_IMAGE),
        "reference_png_sha256": sha(REFERENCE_IMAGE),
        "color_space": "Lab D65/2-degree derived from Core float input/output values interpreted as encoded sRGB; source/reference PNGs have no embedded ICC and loaded Core descriptors are unknown",
        "statistics": "mean per-pixel Lab L*/a*/b* from actual Core float inputs/outputs; C*ab is computed from each ROI mean a*/b*. Source, WB2, and output means all come from the same float-valued Core measurement loop; saved PNGs are provenance/visualization only",
        "interpretation": "positive/negative delta_Cstar_from_source describes chroma increase/decrease relative to WB1; for the white/neutral chart patches C*ab is a chroma proxy, not material neutrality ground truth",
        "limits": [
            "WB2 is camera auto-WB, not calibrated/colorimetric truth",
            "skin-tone patch is not human skin; white and gray patches are not natural objects",
            "input transfer/primaries are unknown, so sRGB interpretation is assumed",
            "mean C* does not describe local hue spread or texture/detail",
            "no threshold, preference, or acceptance gate is defined",
        ],
        "roi_baselines": baselines,
        "rows": output_rows,
    }
    OUTPUT.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8", newline="\n")
    for region in EXPECTED_PIXELS:
        base = baselines[region]
        print(f"{region}: source C*={base['source']['mean_Cstar']:.2f}, WB2 C*={base['camera_auto_wb']['mean_Cstar']:.2f}")
        for row in output_rows:
            if row["region"] == region and row["intensity"] == 1.0:
                print(f"  {row['method']}: output C*={row['output_mean_Cstar']:.2f}, ΔC*source={row['delta_Cstar_from_source']:+.2f}")
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
