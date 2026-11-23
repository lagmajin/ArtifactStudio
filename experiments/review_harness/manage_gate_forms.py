"""Unified gate-form inventory and generator for all R&D runs.

Finds GATE_FORM.md files under experiments/*/captures/rd*/review/, reports which
runs lack one, and can generate a blank form for a given run using the shared
schema. Existing forms are never overwritten.

The form schema is the six mandatory evaluation items plus effect-specific
items. Thresholds are intentionally blank: a separate reviewer must fill them
after viewing the baseline and before candidate comparison.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LABS = ("WatercolorLab", "PencilPrintLab", "MaterialSeparationLab", "SeamlessTextureLab")
ITEMS = (
    "5.1 効果の正しさ",
    "5.2 画質と細部",
    "5.3 色と alpha",
    "5.4 時間安定性",
    "5.5 経路と解像度",
    "5.6 性能",
    "5.7 効果固有",
)


def form_text(rd: str, lab: str) -> str:
    return (
        f"# {rd} gate form (thresholds intentionally blank)\n\n"
        f"Lab: {lab}. A reviewer independent of the implementer fills tolerances "
        "after viewing the baseline, before candidate comparison. Unfilled items remain unevaluated.\n\n"
        "| item | region | evidence | tolerance (BLANK) | judge | fix date | rationale |\n"
        "|---|---|---|---|---|---|---|\n"
        + "".join(f"| {item} | | | | | | |\n" for item in ITEMS)
    )


def inventory() -> list[dict]:
    rows = []
    for lab in LABS:
        cap = ROOT / "experiments" / lab / "captures"
        if not cap.exists():
            continue
        for run in sorted(p for p in cap.glob("rd*") if p.is_dir()):
            form = run / "review" / "GATE_FORM.md"
            rows.append({
                "lab": lab,
                "rd": run.name,
                "path": str(form.relative_to(ROOT)).replace("\\", "/"),
                "exists": form.exists(),
            })
    return rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="experiments/review_harness/out/gate_inventory.json")
    parser.add_argument("--fill-missing", action="store_true",
                        help="generate blank forms for runs that lack one (never overwrites)")
    args = parser.parse_args()

    rows = inventory()
    created = []
    if args.fill_missing:
        for row in rows:
            if row["exists"]:
                continue
            path = ROOT / row["path"]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(form_text(row["rd"], row["lab"]), encoding="utf-8")
            row["exists"] = True
            row["created"] = True
            created.append(row["path"])

    summary = {
        "runs": len(rows),
        "with_form": sum(1 for r in rows if r["exists"]),
        "missing_before": sum(1 for r in rows if not r.get("created", False) and not r["exists"]),
        "created": created,
        "schema_items": list(ITEMS),
        "runs_inventory": rows,
    }
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"gate forms: {summary['with_form']}/{summary['runs']} runs, created {len(created)} -> {out}")


if __name__ == "__main__":
    main()
