"""Findings-driven state-transition rehearsal (advisory -> scored).

Places temporary sample findings in out/findings, runs the real pipeline, and
checks that the reported state changes honestly:

  1. baseline: no findings -> findings_state "none" everywhere
  2. advisory: findings with gates_fixed=false -> "advisory"
  3. scored claim: findings with gates_fixed=true -> "scored" reported,
     BUT GATE_FORM.md thresholds remain empty, so
       * thresholds_entered_runs stays 0
       * gate_claim_mismatch is True for that run
       * the API consumer does NOT list it as scoreable
  4. cleanup: remove samples, restore baseline

No real GATE_FORM.md is modified. No product build/runtime is performed.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
FINDINGS = HARNESS / "out" / "findings"
PY = sys.executable

RUN_LAB = "PencilPrintLab"
RUN_RD = "rd5"
BID = "C01"  # exists in rd5 blind key


def write_sample(gates_fixed: bool) -> Path:
    data = {
        "lab": RUN_LAB,
        "rd": RUN_RD,
        "reviewer": "rehearsal-sample",
        "date": "2026-11-18",
        "gates_fixed": gates_fixed,
        "findings": [{"blind_id": BID, "verdict": "undecided", "note": "state-transition sample", "defect": None}],
    }
    path = FINDINGS / "_transition_sample.json"
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return path


def run_step(script: str, extra: list[str] | None = None) -> None:
    cmd = [PY, str(HARNESS / script), *(extra or [])]
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"{script} failed: {proc.stderr.strip() or proc.stdout.strip()}")


def snapshot() -> dict:
    summary = json.loads((HARNESS / "out" / "review_summary.json").read_text(encoding="utf-8"))
    api = json.loads((HARNESS / "out" / "gate_status.json").read_text(encoding="utf-8"))
    key = f"{RUN_LAB}/{RUN_RD}"
    run = summary["per_run"][key]
    return {
        "findings_state": run["findings_state"],
        "thresholds_entered_runs": summary["thresholds_entered_runs"],
        "gate_claim_mismatch": run["gate_claim_mismatch"],
        "gate_form_status": run["gate_form"]["status"],
        "api_gate_claim_mismatch": next(r["findings"]["gate_claim_mismatch"] for r in api["runs"] if r["id"] == key),
    }


def refresh() -> None:
    run_step("validate_findings.py", ["--findings", "experiments/review_harness/out/findings/*.json"])
    run_step("aggregate_review.py")
    run_step("gate_api.py")
    run_step("example_api_consumer.py")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    args = parser.parse_args()

    FINDINGS.mkdir(parents=True, exist_ok=True)
    sample = FINDINGS / "_transition_sample.json"
    checks = []
    try:
        # 1. baseline
        if sample.exists():
            sample.unlink()
        refresh()
        base = snapshot()
        checks.append(("baseline_none", base["findings_state"] == "none", base))

        # 2. advisory
        write_sample(gates_fixed=False)
        refresh()
        advisory = snapshot()
        checks.append(("advisory_state", advisory["findings_state"] == "advisory", advisory))
        checks.append(("advisory_thresholds_still_zero", advisory["thresholds_entered_runs"] == 0, advisory))

        # 3. scored claim (thresholds still unset in GATE_FORM.md)
        write_sample(gates_fixed=True)
        refresh()
        scored = snapshot()
        checks.append(("scored_state_reported", scored["findings_state"] == "scored", scored))
        checks.append(("scored_thresholds_still_zero", scored["thresholds_entered_runs"] == 0, scored))
        checks.append(("gate_form_still_unset", scored["gate_form_status"] == "thresholds_unset", scored))
        checks.append(("mismatch_detected", scored["gate_claim_mismatch"] and scored["api_gate_claim_mismatch"], scored))
        consumer = json.loads((HARNESS / "out" / "api_consumer_report.json").read_text(encoding="utf-8"))
        key = f"{RUN_LAB}/{RUN_RD}"
        checks.append(("consumer_not_scoreable", key not in consumer["scoreable_runs"], consumer["scoreable_runs"]))
    finally:
        if sample.exists():
            sample.unlink()
        refresh()

    restored = snapshot()
    checks.append(("restored_none", restored["findings_state"] == "none", restored))

    passed = all(ok for _, ok, _ in checks)
    report = {
        "checks": [{"name": n, "ok": ok, "evidence": ev} for n, ok, ev in checks],
        "passed": passed,
        "note": "Sample findings only; real GATE_FORM.md thresholds were never modified.",
    }
    out = HARNESS / "out" / "state_transition.json"
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for name, ok, _ in checks:
        print(f"[{'OK' if ok else 'FAIL'}] {name}")
    print(f"state transition {'OK' if passed else 'FAILED'} -> {out}")
    if not passed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
