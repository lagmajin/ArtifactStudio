"""Downstream consumer of gate_status.json (a second example beyond the JSON consumer).

Reads the machine-consumable gate-status API and emits a human-readable CSV
and Markdown report. It does not set thresholds or make quality decisions.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--api", default="experiments/review_harness/out/gate_status.json")
    parser.add_argument("--out-csv", default="experiments/review_harness/out/gate_status.csv")
    parser.add_argument("--out-md", default="experiments/review_harness/out/gate_status.md")
    args = parser.parse_args()

    api_path = ROOT / args.api if not Path(args.api).is_absolute() else Path(args.api)
    api = json.loads(api_path.read_text(encoding="utf-8"))

    csv_path = ROOT / args.out_csv if not Path(args.out_csv).is_absolute() else Path(args.out_csv)
    md_path = ROOT / args.out_md if not Path(args.out_md).is_absolute() else Path(args.out_md)
    csv_path.parent.mkdir(parents=True, exist_ok=True)

    with csv_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["run", "evidence_ok", "gate_form", "thresholds_filled",
                         "findings_state", "gate_claim_mismatch", "accept", "reject", "undecided"])
        for run in api.get("runs", []):
            form = run["gate_form"]
            writer.writerow([
                run["id"],
                run["evidence"]["ok"],
                form.get("status", ""),
                f"{form.get('tolerance_filled', 0)}/{form.get('tolerance_total', 0)}",
                run["findings"]["findings_state"],
                run["findings"]["gate_claim_mismatch"],
                run["findings"]["verdicts"].get("accept", 0),
                run["findings"]["verdicts"].get("reject", 0),
                run["findings"]["verdicts"].get("undecided", 0),
            ])

    s = api.get("summary", {})
    lines = [
        "# Gate status report (generated from gate_status.json)",
        "",
        f"- schema_version: {api.get('schema_version')}",
        f"- runs: {s.get('runs', 0)}",
        f"- evidence ok: {s.get('evidence_ok_runs', 0)}",
        f"- thresholds entered: {s.get('thresholds_entered_runs', 0)}",
        f"- gate-claim mismatches: {s.get('gate_claim_mismatch_runs', 0)}",
        f"- findings states: {s.get('findings_state_counts', {})}",
        f"- verdict totals: {s.get('verdict_totals', {})}",
        "",
        "This report is generated from machine-readable state. It sets no threshold "
        "and makes no quality decision.",
    ]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"downstream report: {len(api.get('runs', []))} runs -> {csv_path.name}, {md_path.name}")


if __name__ == "__main__":
    main()
