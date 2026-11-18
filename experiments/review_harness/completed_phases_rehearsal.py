"""Rehearse completed phases 2 and 3 on a sandbox copy.

Uses the same sandbox approach as threshold_entry_rehearsal.py:
  * copy harness + one run into out/sandbox_completed/
  * fill that copy's GATE_FORM tolerance cells (phase 2, simulated reviewer)
  * fill that copy's submission template (phase 3, simulated reviewer)
  * intake the template into the sandbox findings via intake_template.py
  * verify all six runbook phases with verify_runbook_phases.py

The real repository's GATE_FORM thresholds and findings stay untouched.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from threshold_entry_rehearsal import build_sandbox, fill_gate_form

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
PY = sys.executable
LAB, RD, BID = "PencilPrintLab", "rd5", "C01"


def run(sandbox: Path, script: str, extra: list[str] | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        [PY, str(sandbox / "experiments" / "review_harness" / script), *(extra or [])],
        cwd=sandbox, capture_output=True, text=True,
    )


def fill_template(sandbox: Path) -> Path:
    d = sandbox / "experiments" / "review_harness" / "out" / "templates" / LAB / RD
    d.mkdir(parents=True, exist_ok=True)
    template = d / "submission.json"
    data = {
        "lab": LAB, "rd": RD, "reviewer": "completed-phase-rehearsal",
        "date": "2026-11-18", "gates_fixed": True,
        "findings": [{"blind_id": BID, "verdict": "accept", "note": "sandbox", "defect": None}],
    }
    template.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return template


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sandbox", default="experiments/review_harness/out/sandbox_completed")
    args = parser.parse_args()
    sandbox = ROOT / args.sandbox if not Path(args.sandbox).is_absolute() else Path(args.sandbox)
    run_dir = build_sandbox(sandbox)
    filled = fill_gate_form(run_dir)
    template = fill_template(sandbox)

    logs = []
    baseline = run(sandbox, "rehearse.py")
    logs.append({"step": "rehearse.py", "ok": baseline.returncode == 0,
                 "output": baseline.stdout.strip()})
    print(f"[{'OK' if baseline.returncode == 0 else 'FAIL'}] rehearse.py (baseline)")
    if baseline.returncode != 0:
        print(baseline.stderr.strip())
        raise SystemExit(1)

    intake = run(sandbox, "intake_template.py", [
        "--template", str(template.relative_to(sandbox)),
    ])
    logs.append({"step": "intake_template.py", "ok": intake.returncode == 0,
                 "output": intake.stdout.strip()})
    print(f"[{'OK' if intake.returncode == 0 else 'FAIL'}] intake_template.py: "
          f"{intake.stdout.strip().splitlines()[-1] if intake.stdout.strip() else intake.stderr.strip()[:240]}")
    if intake.returncode != 0:
        raise SystemExit(1)

    phases = run(sandbox, "verify_runbook_phases.py")
    logs.append({"step": "verify_runbook_phases.py", "ok": phases.returncode == 0,
                 "output": phases.stdout.strip()})
    print(phases.stdout.strip())

    o = sandbox / "experiments" / "review_harness" / "out"
    verification = json.loads((o / "runbook_verification.json").read_text(encoding="utf-8"))
    summary = json.loads((o / "review_summary.json").read_text(encoding="utf-8"))
    key = f"{LAB}/{RD}"
    run_state = summary["per_run"][key]

    checks = [
        ("gate_form_filled", filled["cells_filled"] == 7, filled),
        ("phase2_ok", next(p for p in verification["phases"] if p["id"] == 2)["status"] == "ok", None),
        ("phase3_ok", next(p for p in verification["phases"] if p["id"] == 3)["status"] == "ok", None),
        ("phases_overall_ok", verification["overall"] == "ok", verification["overall"]),
        ("thresholds_entered", summary["thresholds_entered_runs"] == 1, summary["thresholds_entered_runs"]),
        ("findings_scored", run_state["findings_state"] == "scored", run_state["findings_state"]),
        ("no_mismatch", not run_state["gate_claim_mismatch"], None),
    ]
    passed = all(ok for _, ok, _ in checks)

    real_summary = json.loads((HARNESS / "out" / "review_summary.json").read_text(encoding="utf-8"))
    real_findings = sorted(p.name for p in (HARNESS / "out" / "findings").glob("*.json"))
    real_check = {
        "thresholds_entered_runs": real_summary["thresholds_entered_runs"],
        "findings_files": real_findings,
    }
    real_ok = real_check["thresholds_entered_runs"] == 0 and real_check["findings_files"] == []
    checks.append(("real_gates_untouched", real_ok, real_check))
    passed = all(ok for _, ok, _ in checks)

    report = {
        "sandbox": str(sandbox.relative_to(ROOT)).replace("\\", "/"),
        "steps": logs,
        "checks": [{"name": n, "ok": ok, "evidence": ev} for n, ok, ev in checks],
        "real_repository": real_check,
        "passed": passed,
        "note": "Sandbox-only rehearsal. Real GATE_FORM thresholds remain unset.",
    }
    out = HARNESS / "out" / "completed_phases_rehearsal.json"
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for name, ok, _ in checks:
        print(f"[{'OK' if ok else 'FAIL'}] {name}")
    print(f"completed phases rehearsal {'OK' if passed else 'FAILED'} -> {out}")
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
