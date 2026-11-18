"""Aggregate validated review findings into per-run and cross-run summaries,
plus a gate-entry status dashboard.

Inputs:
  * validation.json       (from validate_runs.py)
  * gate_inventory.json   (from manage_gate_forms.py)
  * findings JSON files   (matching a glob; each file is one reviewer's submission)

Outputs (default experiments/review_harness/out/):
  * review_summary.json   per-run + cross-run summaries
  * gate_dashboard.html   gate-entry status for every run

The tool does NOT decide pass/fail, set thresholds, or rank candidates. It
reports data integrity and the state of reviewer-entered information only.
"""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path

from validate_findings import validate_finding

ROOT = Path(__file__).resolve().parents[2]
VERDICTS = ("accept", "reject", "undecided")


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def gate_form_status(run: dict) -> dict:
    """Count non-empty tolerance cells in the GATE_FORM.md table, if present."""
    manifest = ROOT / run["manifest"]
    form = manifest.parent / "review" / "GATE_FORM.md"
    if not form.exists():
        return {"form_exists": False, "tolerance_filled": 0, "tolerance_total": 0, "status": "no_form"}
    total = filled = 0
    for line in form.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|") or "item" in line or "---" in line:
            continue
        cells = [c.strip() for c in line.split("|")]
        if len(cells) < 6:
            continue
        total += 1
        if cells[4]:  # tolerance column
            filled += 1
    if filled == 0:
        status = "thresholds_unset"
    elif filled < total:
        status = "thresholds_partial"
    else:
        status = "thresholds_entered"
    return {"form_exists": True, "tolerance_filled": filled, "tolerance_total": total, "status": status}


def run_key(lab: str, rd: str) -> str:
    return f"{lab}/{rd}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validation", default="experiments/review_harness/out/validation.json")
    parser.add_argument("--gate-inventory", default="experiments/review_harness/out/gate_inventory.json")
    parser.add_argument("--findings", default="experiments/review_harness/out/findings/*.json")
    parser.add_argument("--out", default="experiments/review_harness/out")
    args = parser.parse_args()

    def resolve(p: str) -> Path:
        return ROOT / p if not Path(p).is_absolute() else Path(p)

    validation = load(resolve(args.validation))
    inventory = load(resolve(args.gate_inventory))
    out_dir = resolve(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    paths = sorted(ROOT.glob(args.findings)) if any(c in args.findings for c in "*?") else [resolve(args.findings)]
    raw_findings = []
    for p in paths:
        if not p.exists():
            continue
        data = load(p)
        check = validate_finding(data, validation)
        raw_findings.append({"file": str(p.relative_to(ROOT)).replace("\\", "/"), "data": data, "check": check})

    gate_by_key = {
        run_key(r["lab"], r["rd"]): r for r in inventory.get("runs_inventory", [])
    }
    val_by_key = {run_key(r["lab"], r["rd"]): r for r in validation.get("results", [])}

    per_run = {}
    for key, val in val_by_key.items():
        findings = [f for f in raw_findings
                    if run_key(f["data"].get("lab", ""), f["data"].get("rd", "")) == key]
        valid_findings = [f for f in findings if f["check"]["ok"]]
        verdicts = {v: 0 for v in VERDICTS}
        defects = 0
        reviewers = set()
        gates_fixed = False
        for f in valid_findings:
            reviewers.add(f["data"].get("reviewer", ""))
            gates_fixed = gates_fixed or bool(f["data"].get("gates_fixed"))
            for finding in f["data"].get("findings", []):
                if finding.get("verdict") in verdicts:
                    verdicts[finding["verdict"]] += 1
                if finding.get("defect"):
                    defects += 1
        form = gate_form_status(val)
        if not valid_findings:
            findings_state = "none"
        elif gates_fixed:
            findings_state = "scored"
        else:
            findings_state = "advisory"
        # Honesty check: a reviewer may report gates_fixed=true while the
        # GATE_FORM.md tolerance cells are still empty. Do not treat that as
        # thresholds actually being entered.
        gate_claim_mismatch = gates_fixed and form["status"] != "thresholds_entered"
        per_run[key] = {
            "lab": val.get("lab"),
            "rd": val.get("rd"),
            "evidence_ok": bool(val.get("ok")),
            "evidence_checked_items": val.get("checked_items", 0),
            "gate_form": form,
            "findings_files": len(findings),
            "valid_findings_files": len(valid_findings),
            "invalid_findings_files": len(findings) - len(valid_findings),
            "reviewers": sorted(r for r in reviewers if r),
            "gates_fixed_reported": gates_fixed,
            "findings_state": findings_state,
            "gate_claim_mismatch": gate_claim_mismatch,
            "verdicts": verdicts,
            "defect_notes": defects,
            "advisory_only": findings_state == "advisory",
        }

    totals = {v: sum(r["verdicts"][v] for r in per_run.values()) for v in VERDICTS}
    summary = {
        "runs": len(per_run),
        "evidence_ok_runs": sum(1 for r in per_run.values() if r["evidence_ok"]),
        "gate_forms_present": sum(1 for r in per_run.values() if r["gate_form"]["form_exists"]),
        "thresholds_entered_runs": sum(1 for r in per_run.values()
                                       if r["gate_form"]["status"] == "thresholds_entered"),
        "runs_with_valid_findings": sum(1 for r in per_run.values() if r["valid_findings_files"] > 0),
        "findings_state_counts": {
            state: sum(1 for r in per_run.values() if r["findings_state"] == state)
            for state in ("none", "advisory", "scored")
        },
        "gate_claim_mismatch_runs": sum(1 for r in per_run.values() if r["gate_claim_mismatch"]),
        "findings_files_total": len(raw_findings),
        "findings_files_valid": sum(1 for f in raw_findings if f["check"]["ok"]),
        "verdict_totals": totals,
        "defect_notes_total": sum(r["defect_notes"] for r in per_run.values()),
        "by_lab": {},
        "per_run": per_run,
        "note": "No pass/fail decision or ranking is produced by this aggregation.",
    }
    for lab in sorted({r["lab"] for r in per_run.values()}):
        keys = [k for k, r in per_run.items() if r["lab"] == lab]
        summary["by_lab"][lab] = {
            "runs": len(keys),
            "evidence_ok": sum(1 for k in keys if per_run[k]["evidence_ok"]),
            "thresholds_entered": sum(1 for k in keys
                                      if per_run[k]["gate_form"]["status"] == "thresholds_entered"),
            "verdicts": {v: sum(per_run[k]["verdicts"][v] for k in keys) for v in VERDICTS},
        }

    summary_path = out_dir / "review_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    rows = []
    for key in sorted(per_run):
        r = per_run[key]
        g = r["gate_form"]
        badge = {
            "no_form": "no form",
            "thresholds_unset": "unset",
            "thresholds_partial": f"partial {g['tolerance_filled']}/{g['tolerance_total']}",
            "thresholds_entered": "entered",
        }[g["status"]]
        rows.append(
            f"<tr><td>{html.escape(key)}</td>"
            f"<td>{'ok' if r['evidence_ok'] else 'FAIL'}</td>"
            f"<td>{html.escape(badge)}</td>"
            f"<td>{r['valid_findings_files']}/{r['findings_files']}</td>"
            f"<td>{html.escape(r['findings_state'])}</td>"
            f"<td>{'MISMATCH' if r['gate_claim_mismatch'] else '-'}</td>"
            f"<td>{r['verdicts']['accept']}/{r['verdicts']['reject']}/{r['verdicts']['undecided']}</td>"
            f"<td>{r['defect_notes']}</td></tr>"
        )
    dashboard = f"""<!doctype html>
<html lang="ja">
<head><meta charset="utf-8"><title>ArtifactStudio gate dashboard</title>
<style>
body {{ font-family: sans-serif; background:#1e1e1e; color:#ddd; margin:1.5rem; }}
table {{ border-collapse:collapse; width:100%; }}
th,td {{ border:1px solid #555; padding:.35rem; text-align:left; font-size:.9rem; }}
h1 {{ font-size:1.3rem; }}
.note {{ color:#aaa; }}
</style></head>
<body>
<h1>ArtifactStudio R&amp;D gate-entry dashboard</h1>
<p class="note">Runs: {summary['runs']} | evidence ok: {summary['evidence_ok_runs']} |
gate forms: {summary['gate_forms_present']} | thresholds entered: {summary['thresholds_entered_runs']} |
valid findings files: {summary['findings_files_valid']}/{summary['findings_files_total']}</p>
<p class="note">Verdict totals: accept {totals['accept']}, reject {totals['reject']},
undecided {totals['undecided']}. Findings state: none {summary['findings_state_counts']['none']},
advisory {summary['findings_state_counts']['advisory']}, scored {summary['findings_state_counts']['scored']}.
Gate-claim mismatches: {summary['gate_claim_mismatch_runs']} (reviewer reported gates fixed while
GATE_FORM thresholds remain empty). This dashboard makes no quality decision.</p>
<table>
<tr><th>run</th><th>evidence</th><th>gate form</th><th>findings valid/total</th>
<th>findings state</th><th>gate claim</th><th>A/R/U</th><th>defect notes</th></tr>
{''.join(rows)}
</table>
</body></html>
"""
    dashboard_path = out_dir / "gate_dashboard.html"
    dashboard_path.write_text(dashboard, encoding="utf-8")

    print(f"aggregate: {summary['runs']} runs, {summary['findings_files_valid']}/{summary['findings_files_total']} "
          f"findings files valid, thresholds entered {summary['thresholds_entered_runs']}")
    print(f"  verdicts: {totals}")
    print(f"  -> {summary_path}")
    print(f"  -> {dashboard_path}")


if __name__ == "__main__":
    main()
