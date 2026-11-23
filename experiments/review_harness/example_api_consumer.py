"""Example machine consumer for gate_status.json.

Demonstrates how an external tool can read the gate-status API without
interpreting raw markdown. Produces a concise, deterministic text report and
a JSON subset containing only runs that are actually scoreable.

A run is scoreable only when ALL of the following hold:
  * evidence passed hash validation
  * the GATE_FORM.md has all tolerance cells entered
  * valid findings exist with gates_fixed=true
  * no gate-claim mismatch

This consumer never sets thresholds or makes a quality decision.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--api", default="experiments/review_harness/out/gate_status.json")
    parser.add_argument("--out", default="experiments/review_harness/out/api_consumer_report.json")
    args = parser.parse_args()

    api_path = ROOT / args.api if not Path(args.api).is_absolute() else Path(args.api)
    api = json.loads(api_path.read_text(encoding="utf-8"))

    scoreable = []
    advisory = []
    blocked = []
    for run in api.get("runs", []):
        if not run["evidence"]["ok"]:
            blocked.append((run["id"], "evidence_failed"))
            continue
        form = run["gate_form"]
        findings = run["findings"]
        if findings["gate_claim_mismatch"]:
            blocked.append((run["id"], "gate_claim_mismatch"))
        elif findings["findings_state"] == "scored" and form["status"] == "thresholds_entered":
            scoreable.append(run["id"])
        elif findings["findings_state"] in ("advisory", "scored"):
            advisory.append(run["id"])
        else:
            blocked.append((run["id"], "no_findings"))

    report = {
        "api_schema_version": api.get("schema_version"),
        "api_policy": api.get("policy", {}),
        "summary": api.get("summary", {}),
        "scoreable_runs": scoreable,
        "advisory_runs": advisory,
        "blocked_runs": [{"id": i, "reason": r} for i, r in blocked],
        "note": "Consumer reports eligibility only; it makes no quality decision.",
    }
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print(f"API consumer: schema {api.get('schema_version')}, runs {len(api.get('runs', []))}")
    print(f"  scoreable={len(scoreable)} advisory={len(advisory)} blocked={len(blocked)}")
    print(f"  -> {out}")


if __name__ == "__main__":
    main()
