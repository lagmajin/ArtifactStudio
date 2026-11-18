"""End-to-end offline rehearsal of the review harness on existing R&D data.

Runs the full pipeline in-process:
  validate runs -> inventory gate forms -> validate findings -> aggregate -> gate API

No gate thresholds are entered, no findings are fabricated, and no product
build or runtime operation is performed. The purpose is to verify that all
tools work together and that the reported state is honest (gates unset).
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
PY = sys.executable

STEPS = [
    ("validate_runs.py", []),
    ("manage_gate_forms.py", []),
    ("build_review_page.py", []),
    ("validate_findings.py", ["--findings", "experiments/review_harness/out/findings/*.json"]),
    ("aggregate_review.py", []),
    ("gate_api.py", []),
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--keep-going", action="store_true")
    args = parser.parse_args()

    results = []
    failed = False
    for name, extra in STEPS:
        cmd = [PY, str(HARNESS / name), *extra]
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        ok = proc.returncode == 0
        results.append({"step": name, "ok": ok, "stdout": proc.stdout.strip(), "stderr": proc.stderr.strip()})
        print(f"[{'OK' if ok else 'FAIL'}] {name}: {proc.stdout.strip().splitlines()[0] if proc.stdout.strip() else ''}")
        if not ok and not args.keep_going:
            failed = True
            break

    out = HARNESS / "out" / "rehearsal.json"
    report = {
        "steps": results,
        "ok": not failed and all(r["ok"] for r in results),
        "gates_unset": True,
        "note": "Rehearsal only. No thresholds entered, no findings fabricated, no product build/runtime.",
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"rehearsal {'OK' if report['ok'] else 'FAILED'} -> {out}")

    if not report["ok"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
