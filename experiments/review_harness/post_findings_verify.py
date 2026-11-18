"""Post-findings re-verification automation.

Run this AFTER a reviewer submits findings JSON (and optionally enters gate
thresholds). It snapshots the current harness state, re-runs the full offline
pipeline, then compares before/after and checks integrity.

It never sets thresholds and never makes a quality decision. It reports:
  * which state fields changed (evidence / findings_state / thresholds / verdicts)
  * whether findings_state transitions are consistent with gates_fixed
  * whether GATE_FORM tolerance cells actually match the reported status
  * whether the API consumer's scoreable set matches the computed eligibility

Exit code is non-zero only with --strict and detected integrity errors.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
OUT = HARNESS / "out"
PY = sys.executable

STEPS = [
    ("validate_runs.py", []),
    ("manage_gate_forms.py", []),
    ("validate_findings.py", ["--findings", "experiments/review_harness/out/findings/*.json"]),
    ("aggregate_review.py", []),
    ("gate_api.py", []),
    ("example_api_consumer.py", []),
    ("downstream_gate_report.py", []),
]


def read_json(path: Path) -> dict | None:
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def snapshot() -> dict:
    summary = read_json(OUT / "review_summary.json")
    api = read_json(OUT / "gate_status.json")
    consumer = read_json(OUT / "api_consumer_report.json")
    validation = read_json(OUT / "validation.json")
    if not summary or not api:
        return {"available": False}
    return {
        "available": True,
        "evidence_ok_runs": summary.get("evidence_ok_runs", 0),
        "runs": summary.get("runs", 0),
        "thresholds_entered_runs": summary.get("thresholds_entered_runs", 0),
        "findings_state_counts": summary.get("findings_state_counts", {}),
        "gate_claim_mismatch_runs": summary.get("gate_claim_mismatch_runs", 0),
        "verdict_totals": summary.get("verdict_totals", {}),
        "scoreable_runs": consumer.get("scoreable_runs", []) if consumer else [],
        "advisory_runs": consumer.get("advisory_runs", []) if consumer else [],
        "validation_ok_runs": validation.get("ok_runs", 0) if validation else None,
    }


def actual_gate_cells(run_key: str) -> tuple[int, int] | None:
    lab, rd = run_key.split("/", 1)
    form = ROOT / "experiments" / lab / "captures" / rd / "review" / "GATE_FORM.md"
    if not form.exists():
        return None
    total = filled = 0
    for line in form.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|") or "item" in line or "---" in line:
            continue
        cells = [c.strip() for c in line.split("|")]
        if len(cells) < 6:
            continue
        total += 1
        if cells[4]:
            filled += 1
    return filled, total


def integrity_checks(after: dict) -> list[dict]:
    summary = read_json(OUT / "review_summary.json") or {}
    checks = []

    # Reported thresholds must match actual GATE_FORM.md cells.
    mismatched = []
    for key, run in summary.get("per_run", {}).items():
        cells = actual_gate_cells(key)
        if not cells:
            continue
        filled, total = cells
        reported = run["gate_form"]
        if reported.get("tolerance_filled") != filled or reported.get("tolerance_total") != total:
            mismatched.append({"run": key, "reported": reported, "actual": [filled, total]})
    checks.append({
        "name": "gate_form_cells_match",
        "ok": not mismatched,
        "evidence": mismatched,
    })

    # findings_state scored requires gates_fixed_reported.
    bad_state = []
    for key, run in summary.get("per_run", {}).items():
        if run["findings_state"] == "scored" and not run["gates_fixed_reported"]:
            bad_state.append(key)
    checks.append({"name": "scored_requires_gates_fixed", "ok": not bad_state, "evidence": bad_state})

    # Consumer scoreable set must match independently computed eligibility.
    api = read_json(OUT / "gate_status.json") or {}
    consumer = read_json(OUT / "api_consumer_report.json") or {}
    expected = set()
    for run in api.get("runs", []):
        f = run["findings"]
        g = run["gate_form"]
        if (run["evidence"]["ok"] and g.get("status") == "thresholds_entered"
                and f.get("findings_state") == "scored" and not f.get("gate_claim_mismatch")):
            expected.add(run["id"])
    actual = set(consumer.get("scoreable_runs", []))
    checks.append({
        "name": "consumer_scoreable_matches",
        "ok": expected == actual,
        "evidence": {"expected": sorted(expected), "actual": sorted(actual)},
    })
    return checks


def changed_fields(before: dict, after: dict) -> dict:
    fields = [
        "evidence_ok_runs", "runs", "thresholds_entered_runs", "findings_state_counts",
        "gate_claim_mismatch_runs", "verdict_totals", "scoreable_runs", "advisory_runs",
    ]
    return {f: {"before": before.get(f), "after": after.get(f)}
            for f in fields if before.get(f) != after.get(f)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strict", action="store_true", help="exit non-zero on integrity errors")
    parser.add_argument("--out", default="experiments/review_harness/out/post_findings_verify.json")
    args = parser.parse_args()

    before = snapshot()
    logs = []
    for script, extra in STEPS:
        proc = subprocess.run([PY, str(HARNESS / script), *extra], cwd=ROOT,
                              capture_output=True, text=True)
        logs.append({"step": script, "ok": proc.returncode == 0, "output": proc.stdout.strip()})
        print(f"[{'OK' if proc.returncode == 0 else 'FAIL'}] {script}")
        if proc.returncode != 0:
            print(proc.stderr.strip())
    steps_ok = all(x["ok"] for x in logs)
    after = snapshot()
    checks = integrity_checks(after)
    report = {
        "before": before,
        "after": after,
        "changed": changed_fields(before, after) if before.get("available") else {},
        "steps": logs,
        "steps_ok": steps_ok,
        "integrity_checks": checks,
        "integrity_ok": all(c["ok"] for c in checks),
        "note": "Re-verification only. No threshold was entered and no quality decision was made.",
    }
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"post-findings verify: steps {'OK' if steps_ok else 'FAILED'}, "
          f"integrity {'OK' if report['integrity_ok'] else 'FAILED'}, "
          f"changed fields: {len(report['changed'])}")
    print(f"-> {out}")
    if not steps_ok or (args.strict and not report["integrity_ok"]):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
