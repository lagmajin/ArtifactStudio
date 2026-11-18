"""Phase-by-phase verification derived from REVIEW_RUNBOOK.md.

Reads the runbook headings (`## フェーズ N: ...`), maps each phase to offline
artifacts, and reports an honest status. Manual phases (threshold entry and
findings submission) are reported as `awaiting_reviewer`, never as passed and
never as failures. No gate is set and no quality decision is made.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
OUT = HARNESS / "out"
RUNBOOK = HARNESS / "REVIEW_RUNBOOK.md"


def load(name: str) -> dict | None:
    path = OUT / name
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else None


def parse_phases() -> list[dict]:
    text = RUNBOOK.read_text(encoding="utf-8")
    return [
        {"id": int(num), "title": title.strip()}
        for num, title in re.findall(r"^## フェーズ\s+(\d+):\s*(.+)$", text, flags=re.MULTILINE)
    ]


def finding_files() -> list[Path]:
    d = OUT / "findings"
    return sorted(p for p in d.glob("*.json")) if d.exists() else []


def check_phase(phase_id: int) -> tuple[str, dict]:
    validation = load("validation.json")
    inventory = load("gate_inventory.json")
    summary = load("review_summary.json")
    pfv = load("post_findings_verify.json")

    if phase_id == 0:
        if validation is None:
            return "not_run", {"reason": "validation.json missing"}
        ok = validation.get("failed_runs", 0) == 0 and validation.get("ok_runs", 0) == validation.get("runs", 0)
        return ("ok" if ok else "failed"), {
            "runs": validation.get("runs"), "ok_runs": validation.get("ok_runs"),
            "failed_runs": validation.get("failed_runs"),
        }

    if phase_id == 1:
        page = OUT / "review.html"
        ok = page.exists() and page.stat().st_size > 0 and inventory is not None and inventory.get("runs", 0) > 0
        return ("ok" if ok else "failed"), {
            "review_html_bytes": page.stat().st_size if page.exists() else 0,
            "gate_form_runs": inventory.get("runs") if inventory else None,
        }

    if phase_id == 2:
        entered = summary.get("thresholds_entered_runs", 0) if summary else 0
        total = summary.get("runs", 0) if summary else 0
        if entered == 0:
            return "awaiting_reviewer", {"thresholds_entered_runs": 0, "runs": total,
                                         "reason": "manual threshold entry not performed"}
        status = "ok" if entered == total else "awaiting_reviewer"
        return status, {"thresholds_entered_runs": entered, "runs": total}

    if phase_id == 3:
        files = finding_files()
        if not files:
            return "awaiting_reviewer", {"findings_files": 0, "reason": "no reviewer submission"}
        check = load("findings_check.json")
        ok = check is not None and check.get("ok_files") == check.get("files") == len(files)
        return ("ok" if ok else "failed"), {
            "findings_files": len(files),
            "ok_files": check.get("ok_files") if check else None,
        }

    if phase_id == 4:
        if pfv is None:
            return "not_run", {"reason": "post_findings_verify.json missing"}
        ok = bool(pfv.get("steps_ok")) and bool(pfv.get("integrity_ok"))
        return ("ok" if ok else "failed"), {
            "steps_ok": pfv.get("steps_ok"), "integrity_ok": pfv.get("integrity_ok"),
        }

    if phase_id == 5:
        api = load("gate_status.json")
        consumer = load("api_consumer_report.json")
        csv = OUT / "gate_status.csv"
        md = OUT / "gate_status.md"
        ok = (api is not None and api.get("schema_version") == 1 and consumer is not None
              and csv.exists() and md.exists())
        return ("ok" if ok else "failed"), {
            "schema_version": api.get("schema_version") if api else None,
            "consumer_present": consumer is not None,
            "csv_bytes": csv.stat().st_size if csv.exists() else 0,
            "md_bytes": md.stat().st_size if md.exists() else 0,
        }

    return "not_run", {"reason": f"no check defined for phase {phase_id}"}


def check_rehearsals() -> dict:
    expected = {
        "rehearsal.json": "ok",
        "state_transition.json": "passed",
        "threshold_entry.json": "passed",
    }
    rows = []
    for name, key in expected.items():
        data = load(name)
        rows.append({"file": name, "status": "ok" if data and data.get(key) else "not_run"})
    failed = [r for r in rows if r["status"] == "not_run"]
    return {"checks": rows, "status": "ok" if not failed else "incomplete"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="experiments/review_harness/out/runbook_verification.json")
    parser.add_argument("--require-complete", action="store_true",
                        help="exit non-zero if any phase is awaiting_reviewer or not_run")
    args = parser.parse_args()

    phases = []
    for p in parse_phases():
        status, evidence = check_phase(p["id"])
        phases.append({"id": p["id"], "title": p["title"], "status": status, "evidence": evidence})

    statuses = [p["status"] for p in phases]
    if "failed" in statuses:
        overall = "failed"
    elif "not_run" in statuses:
        overall = "incomplete"
    elif "awaiting_reviewer" in statuses:
        overall = "ok_with_pending"
    else:
        overall = "ok"

    report = {
        "runbook": str(RUNBOOK.relative_to(ROOT)).replace("\\", "/"),
        "derived_phases": len(phases),
        "phases": phases,
        "rehearsals": check_rehearsals(),
        "overall": overall,
        "note": "Manual phases are never auto-passed. No threshold set, no quality decision.",
    }
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for p in phases:
        print(f"[{'OK' if p['status'] == 'ok' else p['status'].upper()}] フェーズ {p['id']}: {p['title']}")
    print(f"runbook verification: {overall} ({len(phases)} phases) -> {out}")
    if overall == "failed" or (args.require_complete and overall != "ok"):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
