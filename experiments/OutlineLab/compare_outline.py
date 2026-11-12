#!/usr/bin/env python3
"""Compare outline reference outputs.

Each fixture is summarized twice: `primary` uses the configured
`alpha_detail_weight`, and `detail_variant` forces it to 1.0 so the silhouette
and alpha-detail channels can be compared from one report. It intentionally does
not invent quality thresholds and does not compare against a product capture.

Run after generate_fixtures.py and run_outline_experiment.py. This script does
not execute the experiment by itself.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _row(name: str, item: dict[str, Any], variant: str) -> dict[str, Any]:
    data = item.get(variant) or {}
    position = data.get("position_error") or {}
    return {
        "name": name,
        "variant": variant,
        "alpha_mode": item.get("alpha_mode"),
        "depth_applied": item.get("depth_applied"),
        "truth": item.get("truth"),
        "line_coverage": data.get("line_coverage"),
        "line_mean": data.get("line_mean"),
        "width_50_p50": (data.get("width_50") or {}).get("p50"),
        "width_50_p90": (data.get("width_50") or {}).get("p90"),
        "width_90_p50": (data.get("width_90") or {}).get("p50"),
        "line_to_truth_p50": (position.get("line_to_truth") or {}).get("p50"),
        "line_to_truth_p90": (position.get("line_to_truth") or {}).get("p90"),
        "truth_to_line_p50": (position.get("truth_to_line") or {}).get("p50"),
        "truth_to_line_p90": (position.get("truth_to_line") or {}).get("p90"),
        "unwanted_line_rate": position.get("unwanted_line_rate"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", default=str(Path(__file__).resolve().parent / "captures" / "outline-rd1" / "outline-report.json"))
    parser.add_argument("--output", default=None)
    args = parser.parse_args()

    report_path = Path(args.report)
    if not report_path.exists():
        raise SystemExit(f"report not found: {report_path}")
    report = load_json(report_path)

    summary: dict[str, Any] = {
        "report": str(report_path),
        "fixture_count": len(report.get("fixtures", [])),
        "requested_width_px": (report.get("settings") or {}).get("width_px"),
        "settings": report.get("settings"),
        "fixtures": [],
        "notes": [
            "This summary is descriptive only.",
            "No product parity, visual acceptance, or quality ranking is claimed.",
            "Thresholds are intentionally absent until a baseline is captured.",
            "primary uses the configured alpha_detail_weight; detail_variant forces it to 1.0.",
        ],
    }
    for item in report.get("fixtures", []):
        summary["fixtures"].append(_row(item.get("name"), item, "primary"))
        summary["fixtures"].append(_row(item.get("name"), item, "detail_variant"))

    out_path = Path(args.output) if args.output else report_path.with_name("outline-comparison.json")
    out_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())