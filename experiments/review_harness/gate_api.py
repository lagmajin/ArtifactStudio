"""Machine-consumable gate-status API (JSON) for all R&D runs.

Exposes a stable, versioned JSON document that other tools can consume:
  * schema_version, generated_at, policy (gates are never set by tools)
  * run list with evidence state, gate-form entry state, findings state
  * summary totals

This is the JSON counterpart of gate_dashboard.html. It reads review_summary.json
produced by aggregate_review.py and re-checks validation.json freshness. No
pass/fail decision, threshold, or ranking is produced.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_VERSION = 1


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validation", default="experiments/review_harness/out/validation.json")
    parser.add_argument("--summary", default="experiments/review_harness/out/review_summary.json")
    parser.add_argument("--out", default="experiments/review_harness/out/gate_status.json")
    args = parser.parse_args()

    def resolve(p: str) -> Path:
        return ROOT / p if not Path(p).is_absolute() else Path(p)

    validation = json.loads(resolve(args.validation).read_text(encoding="utf-8"))
    summary = json.loads(resolve(args.summary).read_text(encoding="utf-8"))

    runs = []
    for key in sorted(summary.get("per_run", {})):
        r = summary["per_run"][key]
        runs.append({
            "id": key,
            "lab": r.get("lab"),
            "rd": r.get("rd"),
            "evidence": {
                "ok": r.get("evidence_ok", False),
                "checked_items": r.get("evidence_checked_items", 0),
            },
            "gate_form": r.get("gate_form", {}),
            "findings": {
                "files": r.get("findings_files", 0),
                "valid_files": r.get("valid_findings_files", 0),
                "invalid_files": r.get("invalid_findings_files", 0),
                "gates_fixed_reported": r.get("gates_fixed_reported", False),
                "findings_state": r.get("findings_state", "none"),
                "gate_claim_mismatch": r.get("gate_claim_mismatch", False),
                "advisory_only": r.get("advisory_only", False),
                "verdicts": r.get("verdicts", {}),
                "defect_notes": r.get("defect_notes", 0),
            },
        })

    api = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "policy": {
            "gates_set_by_tools": False,
            "quality_decision_by_tools": False,
            "note": "Thresholds must be entered by a separate reviewer in GATE_FORM.md before scoring.",
        },
        "sources": {
            "validation": {
                "path": args.validation.replace("\\", "/"),
                "sha256": sha(resolve(args.validation)),
            },
            "review_summary": {
                "path": args.summary.replace("\\", "/"),
                "sha256": sha(resolve(args.summary)),
            },
        },
        "summary": {
            "runs": summary.get("runs", 0),
            "evidence_ok_runs": summary.get("evidence_ok_runs", 0),
            "gate_forms_present": summary.get("gate_forms_present", 0),
            "thresholds_entered_runs": summary.get("thresholds_entered_runs", 0),
            "runs_with_valid_findings": summary.get("runs_with_valid_findings", 0),
            "verdict_totals": summary.get("verdict_totals", {}),
            "findings_state_counts": summary.get("findings_state_counts", {}),
            "gate_claim_mismatch_runs": summary.get("gate_claim_mismatch_runs", 0),
        },
        "validation_fresh": {
            "ok_runs": validation.get("ok_runs", 0),
            "failed_runs": validation.get("failed_runs", 0),
        },
        "runs": runs,
    }

    out = resolve(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(api, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"gate API: schema {SCHEMA_VERSION}, {len(runs)} runs -> {out}")
    print(f"  thresholds entered: {api['summary']['thresholds_entered_runs']}, "
          f"evidence ok: {api['summary']['evidence_ok_runs']}")


if __name__ == "__main__":
    main()
