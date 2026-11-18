"""Clean-room sandbox rehearsal of the exact chain:

    intake_template.py -> post_findings_verify.py --strict -> verify_runbook_phases.py

A fresh sandbox copy of the harness and one run is built from the real repo.
The baseline pipeline is regenerated there, a template is filled (phase 2/3
simulation), and the three tools are run in order. The script asserts that the
REAL repository GATE_FORM thresholds remain 0 and real findings remain empty.
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


def run(sandbox: Path, script: str, extra: list[str] | None = None) -> dict:
    proc = subprocess.run(
        [PY, str(sandbox / "experiments" / "review_harness" / script), *(extra or [])],
        cwd=sandbox, capture_output=True, text=True,
    )
    return {
        "step": script, "ok": proc.returncode == 0, "returncode": proc.returncode,
        "stdout": proc.stdout.strip(), "stderr": proc.stderr.strip(),
    }


def filled_template(sandbox: Path) -> Path:
    d = sandbox / "experiments" / "review_harness" / "out" / "templates" / LAB / RD
    d.mkdir(parents=True, exist_ok=True)
    path = d / "submission.json"
    data = {
        "lab": LAB, "rd": RD, "reviewer": "intake-cleanroom",
        "date": "2026-11-18", "gates_fixed": True,
        "findings": [{"blind_id": BID, "verdict": "accept", "note": "cleanroom chain", "defect": None}],
    }
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return path


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
    findings = sorted(p.name for p in (HARNESS / "out" / "findings").glob("*.json"))
    return {"forms": len(forms), "filled_cells": sum(f["filled"] for f in forms),
            "findings_files": findings}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sandbox", default="experiments/review_harness/out/sandbox_intake_cleanroom")
    args = parser.parse_args()
    sandbox = ROOT / args.sandbox if not Path(args.sandbox).is_absolute() else Path(args.sandbox)

    run_dir = build_sandbox(sandbox)  # fresh copy, no prior harness outputs
    filled = fill_gate_form(run_dir)  # phase 2 simulation (sandbox only)
    template = filled_template(sandbox)

    logs = []
    baseline = run(sandbox, "rehearse.py")
    logs.append(baseline)
    print(f"[{'OK' if baseline['ok'] else 'FAIL'}] rehearse.py (clean baseline)")
    if not baseline["ok"]:
        print(baseline["stderr"][:400])
        raise SystemExit(1)

    # The exact requested chain, with the verification step explicit.
    intake = run(sandbox, "intake_template.py", [
        "--template", str(template.relative_to(sandbox)), "--skip-verify",
    ])
    logs.append(intake)
    print(f"[{'OK' if intake['ok'] else 'FAIL'}] intake_template.py: "
          f"{intake['stdout'].splitlines()[-1] if intake['stdout'] else intake['stderr'][:240]}")
    if not intake["ok"]:
        raise SystemExit(1)

    post = run(sandbox, "post_findings_verify.py", ["--strict"])
    logs.append(post)
    print(f"[{'OK' if post['ok'] else 'FAIL'}] post_findings_verify.py --strict: "
          f"{post['stdout'].splitlines()[-2] if len(post['stdout'].splitlines()) > 1 else post['stderr'][:240]}")
    if not post["ok"]:
        raise SystemExit(1)

    phases = run(sandbox, "verify_runbook_phases.py")
    logs.append(phases)
    print(phases["stdout"] or phases["stderr"][:240])
    if not phases["ok"]:
        raise SystemExit(1)

    o = sandbox / "experiments" / "review_harness" / "out"
    intake_report = json.loads((o / "intake_report.json").read_text(encoding="utf-8"))
    post_report = json.loads((o / "post_findings_verify.json").read_text(encoding="utf-8"))
    phase_report = json.loads((o / "runbook_verification.json").read_text(encoding="utf-8"))
    summary = json.loads((o / "review_summary.json").read_text(encoding="utf-8"))
    key = f"{LAB}/{RD}"
    state = summary["per_run"][key]

    checks = [
        ("template_gate_form_filled", filled["cells_filled"] == 7, filled),
        ("intake_validated", intake_report["validation"]["ok"], intake_report["validation"]["errors"]),
        ("post_verify_strict_ok", post_report["steps_ok"] and post_report["integrity_ok"], None),
        ("phase_2_and_3_ok", all(
            p["status"] == "ok" for p in phase_report["phases"] if p["id"] in (2, 3)), None),
        ("all_phases_ok", phase_report["overall"] == "ok", phase_report["overall"]),
        ("sandbox_state_scored", state["findings_state"] == "scored", state["findings_state"]),
        ("sandbox_thresholds_entered", summary["thresholds_entered_runs"] == 1, None),
    ]
    gate = real_gate_state()
    checks.append(("real_gates_untouched", gate["filled_cells"] == 0 and gate["findings_files"] == [], gate))
    passed = all(ok for _, ok, _ in checks)

    report = {
        "chain": ["intake_template.py", "post_findings_verify.py --strict", "verify_runbook_phases.py"],
        "sandbox": str(sandbox.relative_to(ROOT)).replace("\\", "/"),
        "logs": [{k: v for k, v in x.items() if k not in ("stdout", "stderr")} for x in logs],
        "checks": [{"name": n, "ok": ok, "evidence": ev} for n, ok, ev in checks],
        "real_repository": gate,
        "passed": passed,
        "note": "Sandbox-only chain rehearsal; real thresholds were never entered.",
    }
    out = HARNESS / "out" / "intake_cleanroom_rehearsal.json"
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for name, ok, _ in checks:
        print(f"[{'OK' if ok else 'FAIL'}] {name}")
    print(f"intake cleanroom chain {'OK' if passed else 'FAILED'} -> {out}")
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
