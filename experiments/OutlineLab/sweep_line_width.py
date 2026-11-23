#!/usr/bin/env python3
"""Sweep requested line width and measure the resulting line width.

The reference model quantizes `width_px` to an integer core radius and then
expands a fringe, so the measured width is not the requested width. This script
records the mapping from requested to measured width for the default
silhouette-only configuration so later work can calibrate instead of assuming.

It is a measurement table, not a quality ranking. Values are specific to the
synthetic fixtures and to this reference model.

Run after generate_fixtures.py.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np

import outline_metrics as metrics
import outline_reference as ref


ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets" / "generated"

WIDTHS = (0.5, 1.0, 2.0, 3.0, 4.0, 6.0)
SOFTNESS = (0.0, 0.75)
CASES = (
    ("alpha-shapes", "straight"),
    ("depth-planes", "straight"),
)


def load_index() -> dict[str, Any]:
    path = ASSETS / "index.json"
    if not path.exists():
        raise SystemExit(
            f"fixtures not found: {path}; run generate_fixtures.py first"
        )
    return json.loads(path.read_text(encoding="utf-8"))


def find_fixture(index: dict[str, Any], name: str, alpha_mode: str) -> dict[str, Any]:
    for item in index.get("fixtures", []):
        if item.get("name") == name and item.get("alpha_mode") == alpha_mode:
            return item
    raise SystemExit(f"fixture not found in index: {name}/{alpha_mode}")


def truth_for(item: dict[str, Any]) -> tuple[np.ndarray, str]:
    if item.get("truth_depth"):
        return np.load(ROOT / item["truth_depth"]), "depth"
    return np.load(ROOT / item["truth_alpha"]), "alpha"


def load_inputs(item: dict[str, Any]) -> tuple[np.ndarray, np.ndarray | None]:
    rgba_path = ROOT / item["preview"].replace(".preview.png", ".npy")
    rgba = np.load(rgba_path).astype(np.float32)
    depth = None
    if item.get("has_depth"):
        depth = np.load(rgba_path.with_name(f"{item['name']}.depth.npy")).astype(np.float32)
    return rgba, depth


def _p(value: Any) -> float | None:
    if not isinstance(value, dict):
        return None
    raw = value.get("p50")
    return None if raw is None else float(raw)


def effective_radius(width_px: float, softness: float) -> tuple[int, float]:
    """Return (core radius, radius that survives a 50% threshold).

    `soft_line` dilates with the core radius and then adds a fringe at
    `softness` opacity. Only a fringe at or above 0.5 can stay inside the 50%
    band, so this mirrors the reference model instead of fitting a curve.
    """
    core = max(0, int(np.ceil(max(float(width_px), 0.0) / 2.0)))
    s = float(np.clip(softness, 0.0, 1.0))
    if s < 0.5:
        return core, float(core)
    return core, float(core + max(1, int(np.ceil(core * s))))


def measure_case(
    item: dict[str, Any],
    width: float,
    softness: float,
    inputs: tuple[np.ndarray, np.ndarray | None, np.ndarray, str] | None = None,
) -> dict[str, Any]:
    """Measure one (fixture, width, softness) combination."""
    if inputs is None:
        inputs = load_inputs(item) + truth_for(item)
    rgba, depth, truth, truth_kind = inputs
    settings = ref.OutlineSettings(
        width_px=width,
        softness=softness,
        alpha_detail_weight=0.0,
        include_depth=depth is not None,
    )
    line = ref.extract(rgba, depth, settings)["line"]
    widths = metrics.line_widths(line, truth)
    position = metrics.position_error(line, truth, width_px=width)
    core, effective = effective_radius(width, softness)
    return {
        "fixture": item.get("name"),
        "alpha_mode": item.get("alpha_mode"),
        "truth": truth_kind,
        "requested_width_px": float(width),
        "softness": float(softness),
        "core_radius": core,
        "effective_radius": effective,
        "width_50_p50": _p(widths["width_50"]),
        "width_90_p50": _p(widths["width_90"]),
        "line_coverage": float(np.mean(line >= 0.5)),
        "line_to_truth_p90": _p((position.get("line_to_truth") or {})),
        "unwanted_line_rate": position.get("unwanted_line_rate"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=str(ROOT / "captures" / "width-sweep"))
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)
    index = load_index()

    report: dict[str, Any] = {
        "lab": "OutlineLab",
        "created": datetime.now(timezone.utc).isoformat(),
        "stage": "width-sweep",
        "alpha_detail_weight": 0.0,
        "widths_px": list(WIDTHS),
        "softness": list(SOFTNESS),
        "rows": [],
        "notes": [
            "Measured width is not the requested width; do not substitute one for the other.",
            "Values are specific to the synthetic fixtures and this reference model.",
            "width_50 / width_90 are sampled along truth-boundary normals at 0.25px steps.",
        ],
    }

    for name, alpha_mode in CASES:
        item = find_fixture(index, name, alpha_mode)
        inputs = load_inputs(item) + truth_for(item)
        for width in WIDTHS:
            for softness in SOFTNESS:
                report["rows"].append(measure_case(item, width, softness, inputs))

    report_path = out_dir / "width-sweep.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    lines = [
        "| fixture | requested | softness | width_50 | width_90 | coverage | line_to_truth p90 | unwanted |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for row in report["rows"]:
        lines.append(
            "| {fixture} | {requested_width_px:g} | {softness:g} | {w50:.2f} | {w90:.2f} | "
            "{cov:.4f} | {l2t:.2f} | {un:.3f} |".format(
                fixture=row["fixture"],
                requested_width_px=row["requested_width_px"],
                softness=row["softness"],
                w50=row["width_50_p50"] or 0.0,
                w90=row["width_90_p50"] or 0.0,
                cov=row["line_coverage"],
                l2t=row["line_to_truth_p90"] or 0.0,
                un=row["unwanted_line_rate"] or 0.0,
            )
        )
    table_path = out_dir / "width-sweep.md"
    table_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"wrote {report_path}")
    print(f"wrote {table_path}")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
