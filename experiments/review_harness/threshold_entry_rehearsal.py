"""Rehearsal with thresholds ACTUALLY entered, on a sandbox copy.

The real GATE_FORM.md files stay untouched. This script copies the harness and
one run into an isolated sandbox tree, fills every tolerance cell in the copied
GATE_FORM.md, adds sample findings with gates_fixed=true, and runs the real
pipeline from the sandbox root. It verifies the genuinely scoreable transition:

  findings_state == "scored"
  thresholds_entered_runs > 0
  gate_claim_mismatch == False
  the run appears in the API consumer's scoreable_runs

Then it restores/keeps the sandbox as evidence; the real repository gates stay
unset.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
PY = sys.executable
LAB = "PencilPrintLab"
RD = "rd5"
BID = "C01"

STEPS = [
    ("validate_runs.py", []),
    ("manage_gate_forms.py", []),
    ("validate_findings.py", ["--findings", "experiments/review_harness/out/findings/*.json"]),
    ("aggregate_review.py", []),
    ("gate_api.py", []),
    ("example_api_consumer.py", []),
]


def build_sandbox(sandbox: Path) -> Path:
    if sandbox.exists():
        shutil.rmtree(sandbox)
    harness = sandbox / "experiments" / "review_harness"
    harness.mkdir(parents=True)
    for src in list(HARNESS.glob("*.py")) + list(HARNESS.glob("*.md")):
        shutil.copy2(src, harness / src.name)
    src_run = ROOT / "experiments" / LAB / "captures" / RD
    dst_run = sandbox / "experiments" / LAB / "captures" / RD
    shutil.copytree(src_run, dst_run)
    (harness / "out" / "findings").mkdir(parents=True, exist_ok=True)
    return dst_run


def fill_gate_form(run_dir: Path) -> dict:
    form = run_dir / "review" / "GATE_FORM.md"
    lines = form.read_text(encoding="utf-8").splitlines()
    out = []
    filled = 0
    for line in lines:
        if line.startswith("| 5."):
            cells = line.split("|")
            if len(cells) >= 6:
                cells[4] = " 0.01 "
                line = "|".join(cells)
                filled += 1
        out.append(line)
    form.write_text("\n".join(out) + "\n", encoding="utf-8")
    return {"file": str(form.relative_to(ROOT.parent)).replace("\\", "/"), "cells_filled": filled}


def write_findings(sandbox: Path) -> None:
    data = {
        "lab": LAB, "rd": RD, "reviewer": "threshold-rehearsal",
        "date": "2026-11-18", "gates_fixed": True,
        "findings": [{"blind_id": BID, "verdict": "accept",
                      "note": "sandbox rehearsal", "defect": None}],
    }
    p = sandbox / "experiments" / "review_harness" / "out" / "findings" / "sample.json"
    p.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sandbox", default="experiments/review_harness/out/sandbox")
    args = parser.parse_args()
    sandbox = ROOT / args.sandbox if not Path(args.sandbox).is_absolute() else Path(args.sandbox)
    run_dir = build_sandbox(sandbox)
    filled = fill_gate_form(run_dir)
    write_findings(sandbox)

    out = {}
    for script, extra in STEPS:
        cmd = [PY, str(sandbox / "experiments" / "review_harness" / script), *extra]
        proc = subprocess.run(cmd, cwd=sandbox, capture_output=True, text=True)
        out[script] = {"ok": proc.returncode == 0, "stdout": proc.stdout.strip()}
        print(f"[{'OK' if proc.returncode == 0 else 'FAIL'}] {script}: "
              f"{proc.stdout.strip().splitlines()[0] if proc.stdout.strip() else proc.stderr.strip()[:120]}")
        if proc.returncode != 0:
            raise SystemExit(f"{script} failed")

    o = sandbox / "experiments" / "review_harness" / "out"
    summary = json.loads((o / "review_summary.json").read_text(encoding="utf-8"))
    api = json.loads((o / "gate_status.json").read_text(encoding="utf-8"))
    consumer = json.loads((o / "api_consumer_report.json").read_text(encoding="utf-8"))
    key = f"{LAB}/{RD}"
    run = summary["per_run"][key]

    checks = [
        ("tolerance_cells_filled", filled["cells_filled"] == 7, filled),
        ("thresholds_entered_positive", summary["thresholds_entered_runs"] > 0,
         summary["thresholds_entered_runs"]),
        ("findings_state_scored", run["findings_state"] == "scored", run["findings_state"]),
        ("no_gate_claim_mismatch", not run["gate_claim_mismatch"] and
         not next(r["findings"]["gate_claim_mismatch"] for r in api["runs"] if r["id"] == key), None),
        ("consumer_scoreable", key in consumer["scoreable_runs"], consumer["scoreable_runs"]),
    ]
    passed = all(ok for _, ok, _ in checks)
    report = {
        "sandbox": str(sandbox.relative_to(ROOT)).replace("\\", "/"),
        "real_gate_forms_modified": False,
        "filled": filled,
        "checks": [{"name": n, "ok": ok, "evidence": ev} for n, ok, ev in checks],
        "passed": passed,
        "note": "Sandbox-only rehearsal. Real repository GATE_FORM thresholds remain unset.",
    }
    real_out = HARNESS / "out" / "threshold_entry.json"
    real_out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for name, ok, _ in checks:
        print(f"[{'OK' if ok else 'FAIL'}] {name}")
    print(f"threshold-entry rehearsal {'OK' if passed else 'FAILED'} -> {real_out}")
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
