#!/usr/bin/env python3
"""Resolve a target measured line width to a requested width.

The calibration in `calibrate_width.py` maps a requested width to a predicted
measured width. This script inverts that mapping: given a target measured
`width_50`, it searches the requested-width grid for the closest prediction and
then measures the chosen setting to confirm it.

Two properties of the reference model make a naive inverse unsafe:

- the grid is quantized (`width_50` moves in steps of about 1.1px for
  alpha-shapes), so many targets are unreachable;
- `width_50` grows with the effective radius, so a target below the minimum
  measured width cannot be produced at all.

Results are reported with the nearest achievable width and the residual against
the target, so an unreachable target is visible instead of silently clamped.

Run after calibrate_width.py.
"""

from __future__ import annotations

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np

import sweep_line_width as sweep


ROOT = Path(__file__).resolve().parent

TARGETS_PX = (1.0, 2.0, 4.0, 8.0)
SOFTNESS = (0.0, 0.5, 0.75, 0.9)
GRID = tuple(round(0.25 * i, 2) for i in range(1, 49))  # 0.25 .. 12.0
TOLERANCE_PX = 0.5


def load_calibration(
    path: Path,
) -> tuple[dict[str, dict[str, float]], dict[str, tuple[float, float]]]:
    """Return fitted coefficients and the effective-radius range each fit covers."""
    if not path.exists():
        raise SystemExit(f"calibration not found: {path}; run calibrate_width.py first")
    data = json.loads(path.read_text(encoding="utf-8"))
    fits = data.get("fits") or {}
    if not fits:
        raise SystemExit(f"calibration has no fits: {path}")
    ranges: dict[str, list[float]] = {}
    for row in (data.get("fitted") or {}).get("rows") or []:
        cls = "hard" if float(row["softness"]) < 0.5 else "soft"
        key = f"{row['fixture']}|{cls}"
        radius = float(row["effective_radius"])
        if key in ranges:
            ranges[key][0] = min(ranges[key][0], radius)
            ranges[key][1] = max(ranges[key][1], radius)
        else:
            ranges[key] = [radius, radius]
    return fits, {key: (value[0], value[1]) for key, value in ranges.items()}


def predict(fit: dict[str, float], width_px: float, softness: float) -> float:
    _, effective = sweep.effective_radius(width_px, softness)
    return float(fit["slope"]) * effective + float(fit["intercept"])


def choose(
    fit: dict[str, float],
    softness: float,
    target: float,
    radius_range: tuple[float, float] | None = None,
) -> tuple[float, float, float]:
    """Return (requested, core radius, predicted) minimizing |predicted - target|.

    Candidates are limited to the effective-radius range the calibration was
    fitted on, so the result is not extrapolated.
    """
    best: tuple[float, float, float] | None = None
    for width in GRID:
        _, effective = sweep.effective_radius(width, softness)
        if radius_range is not None and not (
            radius_range[0] - 1e-9 <= effective <= radius_range[1] + 1e-9
        ):
            continue
        predicted = predict(fit, width, softness)
        distance = abs(predicted - target)
        if best is None or distance < best[0]:
            best = (distance, width, predicted)
        elif distance == best[0] and width < best[1]:
            best = (distance, width, predicted)
    if best is None:
        raise SystemExit(
            f"no candidate width inside the calibrated radius range: "
            f"softness={softness:g} range={radius_range}"
        )
    _, requested, predicted = best
    core, _ = sweep.effective_radius(requested, softness)
    return requested, float(core), predicted


def achievable(
    fit: dict[str, float],
    softness: float,
    radius_range: tuple[float, float] | None = None,
) -> list[float]:
    values = set()
    for width in GRID:
        _, effective = sweep.effective_radius(width, softness)
        if radius_range is not None and not (
            radius_range[0] - 1e-9 <= effective <= radius_range[1] + 1e-9
        ):
            continue
        values.add(round(predict(fit, width, softness), 2))
    return sorted(values)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--calibration",
        default=str(ROOT / "captures" / "width-sweep" / "width-calibration.json"),
    )
    parser.add_argument("--output", default=str(ROOT / "captures" / "width-sweep"))
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)
    fits, ranges = load_calibration(Path(args.calibration))
    index = sweep.load_index()

    rows: list[dict[str, Any]] = []
    for name, alpha_mode in sweep.CASES:
        item = sweep.find_fixture(index, name, alpha_mode)
        inputs = sweep.load_inputs(item) + sweep.truth_for(item)
        for softness in SOFTNESS:
            cls = "hard" if softness < 0.5 else "soft"
            key = f"{name}|{cls}"
            fit = fits.get(key)
            if fit is None:
                continue
            radius_range = ranges.get(key)
            for target in TARGETS_PX:
                requested, core, predicted = choose(fit, softness, target, radius_range)
                measured = sweep.measure_case(item, requested, softness, inputs)
                actual = measured["width_50_p50"]
                error = None if actual is None else float(actual) - float(target)
                rows.append({
                    "fixture": name,
                    "softness": softness,
                    "class": cls,
                    "target_width_50": target,
                    "requested_width_px": requested,
                    "core_radius": core,
                    "predicted_width_50": predicted,
                    "measured_width_50": actual,
                    "error_px": error,
                    "reachable": bool(error is not None and abs(error) <= TOLERANCE_PX),
                })

    report: dict[str, Any] = {
        "lab": "OutlineLab",
        "created": datetime.now(timezone.utc).isoformat(),
        "stage": "width-resolve",
        "calibration": str(Path(args.calibration)),
        "targets_px": list(TARGETS_PX),
        "softness": list(SOFTNESS),
        "grid_px": {"start": GRID[0], "end": GRID[-1], "step": 0.25},
        "tolerance_px": TOLERANCE_PX,
        "rows": rows,
        "achievable": {
            f"{name}|softness={softness:g}": achievable(
                fits[f"{name}|{'hard' if softness < 0.5 else 'soft'}"],
                softness,
                ranges.get(f"{name}|{'hard' if softness < 0.5 else 'soft'}"),
            )
            for name, _ in sweep.CASES
            for softness in SOFTNESS
            if f"{name}|{'hard' if softness < 0.5 else 'soft'}" in fits
        },
        "notes": [
            "The chosen request minimizes predicted error, then the measured value is verified.",
            "reachable means |measured - target| <= tolerance; unreachable targets report their best effort.",
            "Candidates are limited to the effective-radius range covered by the calibration (no extrapolation).",
            "The requested-width grid and the achievable widths are quantized; do not expect continuous control.",
        ],
    }

    report_path = out_dir / "width-resolve.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Width resolve",
        "",
        f"targets: {', '.join(f'{t:g}' for t in TARGETS_PX)} px; "
        f"tolerance: +/-{TOLERANCE_PX:g} px; grid: {GRID[0]:g}..{GRID[-1]:g} px",
        "",
        "| fixture | softness | target | requested | effective r | predicted | measured | error | reachable |",
        "|---|---|---|---|---|---|---|---|---|",
    ]
    for row in rows:
        lines.append(
            "| {fixture} | {soft:g} | {target:g} | {req:g} | {core:g} | {p:.2f} | {m:.2f} | {e:+.2f} | {ok} |".format(
                fixture=row["fixture"],
                soft=row["softness"],
                target=row["target_width_50"],
                req=row["requested_width_px"],
                core=row["core_radius"],
                p=row["predicted_width_50"],
                m=row["measured_width_50"] or 0.0,
                e=row["error_px"] or 0.0,
                ok="yes" if row["reachable"] else "no",
            )
        )
    lines += ["", "## Achievable measured widths (predicted)"]
    for key, values in report["achievable"].items():
        lines.append(f"- `{key}`: " + ", ".join(f"{v:g}" for v in values))
    table_path = out_dir / "width-resolve.md"
    table_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"wrote {report_path}")
    print(f"wrote {table_path}")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
