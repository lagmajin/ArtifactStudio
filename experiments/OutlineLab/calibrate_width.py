#!/usr/bin/env python3
"""Calibrate requested width against measured width, then check the model.

`soft_line` quantizes `width_px` to a core radius and may add a fringe, so the
measured 50% width is not the requested width. This script derives the
dilation the reference model itself applies, then fits the remaining offset as
a linear function of that dilation:

    core radius      = ceil(width_px / 2)
    fringe radius    = core radius + max(1, ceil(core radius * softness))
    effective_radius = core radius                 if softness < 0.5
                     = fringe radius               if softness >= 0.5
    measured_width_50 ~= slope * effective_radius + intercept

`slope` and `intercept` are fitted per fixture and softness class from the
sweep table. The fit is then evaluated on widths and softness values that were
not in the sweep (hold-out), and residuals are reported for both sets.

This is an empirical calibration of a reference model. It is not a product
measurement and its coefficients must not be reused for another rasterizer.

Run after generate_fixtures.py and sweep_line_width.py.
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

# Widths and softness values that are not in the sweep table. Used to check the
# model instead of re-scoring the same points it was fitted from.
HOLDOUT = (
    (8.0, 0.0),
    (1.5, 0.4),
    (5.0, 0.9),
    (0.75, 0.6),
)


def load_rows(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        raise SystemExit(f"sweep table not found: {path}; run sweep_line_width.py first")
    data = json.loads(path.read_text(encoding="utf-8"))
    rows = data.get("rows") or []
    if not rows:
        raise SystemExit(f"sweep table has no rows: {path}")
    return rows


def softness_class(softness: float) -> str:
    return "hard" if float(softness) < 0.5 else "soft"


def fit_group(rows: list[dict[str, Any]]) -> dict[str, float]:
    """Least-squares fit of measured width against effective radius."""
    x = np.array([float(r["effective_radius"]) for r in rows], dtype=np.float64)
    y = np.array([float(r["width_50_p50"]) for r in rows], dtype=np.float64)
    if np.allclose(x, x[0]):
        return {"slope": float("nan"), "intercept": float(np.mean(y))}
    slope, intercept = np.polyfit(x, y, 1)
    return {"slope": float(slope), "intercept": float(intercept)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sweep",
        default=str(ROOT / "captures" / "width-sweep" / "width-sweep.json"),
    )
    parser.add_argument("--output", default=str(ROOT / "captures" / "width-sweep"))
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)
    rows = [r for r in load_rows(Path(args.sweep)) if r.get("width_50_p50") is not None]

    grouped: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for row in rows:
        grouped.setdefault((str(row["fixture"]), softness_class(row["softness"])), []).append(row)
    models = {key: fit_group(group) for key, group in grouped.items()}

    index = sweep.load_index()
    holdout_rows: list[dict[str, Any]] = []
    for name, alpha_mode in sweep.CASES:
        item = sweep.find_fixture(index, name, alpha_mode)
        inputs = sweep.load_inputs(item) + sweep.truth_for(item)
        for width, softness in HOLDOUT:
            holdout_rows.append(sweep.measure_case(item, width, softness, inputs))

    fitted = _score(rows, models)
    held = _score(holdout_rows, models)
    residuals = np.array([r["residual_px"] for r in fitted], dtype=np.float64)
    holdout_residuals = np.array([r["residual_px"] for r in held], dtype=np.float64)

    achievable = sorted({round(float(r["width_50_p50"]), 3) for r in rows})

    report: dict[str, Any] = {
        "lab": "OutlineLab",
        "created": datetime.now(timezone.utc).isoformat(),
        "stage": "width-calibration",
        "sweep": str(Path(args.sweep)),
        "model": {
            "formula": "width_50 ~= slope * effective_radius + intercept",
            "effective_radius": "core radius when softness < 0.5, otherwise core + fringe radius",
            "core_radius": "ceil(width_px / 2)",
            "fringe_radius": "core + max(1, ceil(core * softness))",
            "fit": "least squares per fixture and softness class (hard: <0.5, soft: >=0.5)",
        },
        "fits": {
            f"{fixture}|{cls}": {
                **model,
                "rows": len(grouped[(fixture, cls)]),
            }
            for (fixture, cls), model in sorted(models.items())
        },
        "fitted": {"rows": fitted, "residual": _stats(residuals)},
        "holdout": {"rows": held, "residual": _stats(holdout_residuals)},
        "achievable_width_50_px": achievable,
        "notes": [
            "Coefficients describe this reference model and these synthetic fixtures only.",
            "A positive residual means the measured line is wider than predicted.",
            "Widths are quantized by ceil(width/2); many requested values map to one measured width.",
            "Residuals are diagnostics, not a product acceptance criterion.",
        ],
    }

    report_path = out_dir / "width-calibration.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# Width calibration",
        "",
        "`width_50 ~= slope * effective_radius + intercept`",
        "",
        "## Fitted coefficients",
        "",
        "| fixture | class | slope | intercept | rows |",
        "|---|---|---|---|---|",
    ]
    for key, fit in report["fits"].items():
        fixture, cls = key.split("|")
        lines.append(
            f"| {fixture} | {cls} | {fit['slope']:.3f} | {fit['intercept']:.3f} | {fit['rows']} |"
        )
    for title, block in (("Sweep rows", report["fitted"]), ("Hold-out rows", report["holdout"])):
        lines += [
            "",
            f"## {title}",
            "",
            "| fixture | requested | softness | effective r | measured | predicted | residual |",
            "|---|---|---|---|---|---|---|",
        ]
        for row in block["rows"]:
            lines.append(
                "| {fixture} | {req:g} | {soft:g} | {er:g} | {m:.2f} | {p:.2f} | {r:+.2f} |".format(
                    fixture=row["fixture"],
                    req=row["requested_width_px"],
                    soft=row["softness"],
                    er=row["effective_radius"],
                    m=row["measured_width_50"],
                    p=row["predicted_width_50"],
                    r=row["residual_px"],
                )
            )
        res = block["residual"]
        lines += [
            "",
            f"residual: max |e| = {res['max_abs']:.2f} px, mean = {res['mean']:+.2f} px, "
            f"p90 |e| = {res['p90_abs']:.2f} px (n = {res['count']})",
        ]
    lines += [
        "",
        "## Achievable measured widths",
        "",
        ", ".join(f"{value:g}" for value in achievable),
    ]
    table_path = out_dir / "width-calibration.md"
    table_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"wrote {report_path}")
    print(f"wrote {table_path}")
    print("\n".join(lines))
    return 0


def _score(
    rows: list[dict[str, Any]], models: dict[tuple[str, str], dict[str, float]]
) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    for row in rows:
        key = (str(row["fixture"]), softness_class(row["softness"]))
        model = models.get(key)
        measured = row.get("width_50_p50")
        if model is None or measured is None:
            continue
        predicted = model["slope"] * float(row["effective_radius"]) + model["intercept"]
        out.append({
            "fixture": row["fixture"],
            "requested_width_px": row["requested_width_px"],
            "softness": row["softness"],
            "core_radius": row["core_radius"],
            "effective_radius": row["effective_radius"],
            "measured_width_50": float(measured),
            "predicted_width_50": float(predicted),
            "residual_px": float(measured) - float(predicted),
        })
    return out


def _stats(values: np.ndarray) -> dict[str, float]:
    if values.size == 0:
        return {"count": 0, "max_abs": 0.0, "mean": 0.0, "p90_abs": 0.0}
    return {
        "count": int(values.size),
        "max_abs": float(np.max(np.abs(values))),
        "mean": float(np.mean(values)),
        "p90_abs": float(np.percentile(np.abs(values), 90.0)),
    }


if __name__ == "__main__":
    raise SystemExit(main())
