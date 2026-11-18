"""Clean-room end-to-end acceptance runner for the review harness.

R&D captures are large (~7.9 GB), so they are read in place. The harness OUTPUT
directory is backed up to a temporary location, removed, and regenerated from
scratch by the real tools. Then:

  * every phase of REVIEW_RUNBOOK.md is verified
  * the integrated rehearsal and all three focused rehearsals are re-run
  * templates are regenerated
  * real GATE_FORM.md thresholds and real findings are asserted untouched

On failure the original output directory is restored. On success the freshly
generated outputs are kept as the canonical harness state.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
OUT = HARNESS / "out"
PY = sys.executable

PIPELINE = [
    "validate_runs.py",
    "manage_gate_forms.py",
    "build_review_page.py",
    "validate_findings.py",
    "aggregate_review.py",
    "gate_api.py",
    "example_api_consumer.py",
    "downstream_gate_report.py",
    "generate_review_templates.py",
    "post_findings_verify.py",
    "rehearse.py",
    "state_transition_rehearsal.py",
    "threshold_entry_rehearsal.py",
    "completed_phases_rehearsal.py",
    "intake_cleanroom_rehearsal.py",
    "verify_runbook_phases.py",
]


def invoke(script: str, extra: list[str] | None = None) -> dict:
    cmd = [PY, str(HARNESS / script), *(extra or [])]
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    return {
        "step": script,
        "ok": proc.returncode == 0,
        "returncode": proc.returncode,
        "stdout": proc.stdout.strip(),
        "stderr": proc.stderr.strip(),
    }


def read(name: str) -> dict | None:
    p = OUT / name
    return json.loads(p.read_text(encoding="utf-8")) if p.exists() else None


def real_gate_state() -> dict:
    forms = []
    for lab in ("WatercolorLab", "PencilPrintLab", "MaterialSeparationLab", "SeamlessTextureLab"):
        for form in sorted((ROOT / "experiments" / lab / "captures").glob("rd*/review/GATE_FORM.md")):
            filled = total = 0
            for line in form.read_text(encoding="utf-8").splitlines():
                if not line.startswith("| 5."):
                    continue
                cells = [c.strip() for c in line.split("|")]
                if len(cells) < 6:
                    continue
                total += 1
                if cells[4]:
                    filled += 1
            forms.append({"file": str(form.relative_to(ROOT)).replace("\\", "/"),
                          "filled": filled, "total": total})
    findings = sorted(p.name for p in (OUT / "findings").glob("*.json")) if (OUT / "findings").exists() else []
    return {"forms": len(forms), "filled_cells": sum(f["filled"] for f in forms),
            "findings_files": findings,
            "all_forms": forms}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--keep-backup-on-success", action="store_true")
    args = parser.parse_args()

    backup_root = Path(tempfile.mkdtemp(prefix="artifact_review_backup_"))
    backup = backup_root / "out"
    shutil.copytree(OUT, backup)

    logs = []
    failed = False
    report = {"passed": False}
    try:
        shutil.rmtree(OUT)
        OUT.mkdir(parents=True)
        (OUT / "findings").mkdir()
        (OUT / "findings" / "README.md").write_text(
            "# Review findings submissions\n\nVerified submissions only. Templates live in ../templates/.\n",
            encoding="utf-8",
        )

        for script in PIPELINE:
            extra = None
            if script == "validate_findings.py":
                extra = ["--findings", "experiments/review_harness/out/findings/*.json"]
            log = invoke(script, extra)
            logs.append(log)
            print(f"[{'OK' if log['ok'] else 'FAIL'}] {script}")
            if not log["ok"]:
                print(log["stderr"][:400] or log["stdout"][-400:])
                failed = True
                break

        if not failed:
            validation = read("validation.json") or {}
            rehearsal = read("rehearsal.json") or {}
            state = read("state_transition.json") or {}
            threshold = read("threshold_entry.json") or {}
            completed = read("completed_phases_rehearsal.json") or {}
            intake_clean = read("intake_cleanroom_rehearsal.json") or {}
            phases = read("runbook_verification.json") or {}
            templates = read("templates/index.json") or {}

            unique_runs = {(r.get("lab"), r.get("rd")) for r in validation.get("results", [])
                            if r.get("lab") and r.get("rd")}
            expected_runs = len(unique_runs)
            checks = [
                ("validation_all_ok", validation.get("runs", 0) > 0
                 and validation.get("runs") == validation.get("ok_runs"),
                 {"runs": validation.get("runs"), "ok": validation.get("ok_runs")}),
                ("integrated_rehearsal", bool(rehearsal.get("ok")), None),
                ("state_transition", bool(state.get("passed")), None),
                ("sandbox_threshold_entry", bool(threshold.get("passed")), None),
                ("sandbox_completed_phases", bool(completed.get("passed")), None),
                ("intake_cleanroom_chain", bool(intake_clean.get("passed")), None),
                ("runbook_phases_ok_with_pending", phases.get("overall") == "ok_with_pending",
                 phases.get("overall")),
                ("templates_match_runs", expected_runs > 0
                 and templates.get("runs") == expected_runs
                 and len(templates.get("generated", [])) == expected_runs * 2,
                 {"expected_runs": expected_runs, "runs": templates.get("runs"),
                  "generated": len(templates.get("generated", []))}),
            ]
            gate = real_gate_state()
            checks.append(("real_gates_unset", gate["filled_cells"] == 0 and gate["findings_files"] == [],
                           {"filled_cells": gate["filled_cells"], "findings": gate["findings_files"]}))
            passed = all(ok for _, ok, _ in checks)
            report = {
                "lab": "review_harness", "stage": "cleanroom_acceptance", "date": "2026-11-17",
                "backup_dir": str(backup_root),
                "regenerated_steps": [x["step"] for x in logs],
                "steps": [{k: v for k, v in x.items() if k not in ("stdout", "stderr")} for x in logs],
                "checks": [{"name": n, "ok": ok, "evidence": ev} for n, ok, ev in checks],
                "real_gate_state": gate,
                "passed": passed and not failed,
                "note": "Real GATE_FORM thresholds were never modified; no quality decision was made.",
            }
            for name, ok, _ in checks:
                print(f"[{'OK' if ok else 'FAIL'}] {name}")
    finally:
        pass

    report_path = OUT / "cleanroom_acceptance.json"
    if failed or not report.get("passed", False):
        shutil.rmtree(OUT, ignore_errors=True)
        shutil.copytree(backup, OUT)
        print("cleanroom FAILED: restored original output directory")
        raise SystemExit(1)

    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if not args.keep_backup_on_success:
        shutil.rmtree(backup_root, ignore_errors=True)
    print(f"cleanroom acceptance OK: {len(logs)} tools regenerated -> {OUT.relative_to(ROOT).as_posix()}")


if __name__ == "__main__":
    main()
