"""Summarize RD24's source-change versus camera-render-distance tradeoff."""
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CAPTURE = ROOT / "captures" / "rd24_colorchecker_core"
SOURCE = CAPTURE / "core_patch_metrics.csv"
OUTPUT = CAPTURE / "tradeoff_summary.json"
REGIONS = ("skin_tone_proxy", "white_patch", "neutral_gray")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    with SOURCE.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if len(rows) != 27:
        raise RuntimeError(f"Expected 27 RD24 rows, got {len(rows)}")

    groups: dict[str, list[dict]] = {region: [] for region in REGIONS}
    for row in rows:
        groups[row["region"]].append({
            "method": row["method"],
            "intensity": float(row["intensity"]),
            "pixels": int(row["pixels"]),
            "source_change_mean_de76": float(row["source_mean_delta_e76"]),
            "camera_auto_wb_distance_mean_de76": float(row["wb2_mean_delta_e76"]),
            "source_change_p95_de76": float(row["source_p95_delta_e76"]),
            "camera_auto_wb_distance_p95_de76": float(row["wb2_p95_delta_e76"]),
        })

    result = {}
    for region, candidates in groups.items():
        frontier = []
        for candidate in candidates:
            dominated = any(
                other["source_change_mean_de76"] <= candidate["source_change_mean_de76"]
                and other["camera_auto_wb_distance_mean_de76"] <= candidate["camera_auto_wb_distance_mean_de76"]
                and (other["source_change_mean_de76"] < candidate["source_change_mean_de76"]
                     or other["camera_auto_wb_distance_mean_de76"] < candidate["camera_auto_wb_distance_mean_de76"])
                for other in candidates if other is not candidate
            )
            if not dominated:
                frontier.append(candidate)
        result[region] = {
            "candidates": sorted(candidates, key=lambda item: (item["source_change_mean_de76"], item["camera_auto_wb_distance_mean_de76"])),
            "pareto_frontier": sorted(frontier, key=lambda item: (item["source_change_mean_de76"], item["camera_auto_wb_distance_mean_de76"])),
        }

    manifest_path = CAPTURE / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    summary = {
        "rd": "rd24-source-change-camera-render-distance-tradeoff",
        "status": "descriptive secondary analysis; no method winner or quality gate",
        "input_metrics_sha256": sha(SOURCE),
        "input_manifest_sha256": sha(manifest_path),
        "metrics": "mean and p95 per-pixel Lab D65/2-degree DeltaE76; source_change is distance from WB1 source, camera_auto_wb_distance is distance from WB2 camera JPEG",
        "color_interpretation": manifest["measurement"]["interpretation_limit"],
        "reference_limit": "WB2 is an in-camera auto-WB rendition, not calibrated truth; chart skin/white/gray patches are proxies/material samples, not people or natural objects",
        "frontier_rule": "nondominated on the two mean distances only; descriptive geometry, not preference or acceptance",
        "regions": result,
    }
    OUTPUT.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8", newline="\n")
    for region, record in result.items():
        points = record["pareto_frontier"]
        print(region + ": " + "; ".join(
            f"{p['method']}@{p['intensity']:g} sourceΔE={p['source_change_mean_de76']:.2f}, WB2ΔE={p['camera_auto_wb_distance_mean_de76']:.2f}"
            for p in points
        ))
    print(f"Wrote {OUTPUT}")


if __name__ == "__main__":
    main()
