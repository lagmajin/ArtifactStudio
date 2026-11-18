"""Validate and cross-check review-findings JSON against validation.json.

A reviewer writes one JSON file per run, e.g.:

{
  "lab": "PencilPrintLab",
  "rd": "rd9",
  "reviewer": "name",
  "date": "2026-11-18",
  "gates_fixed": true,
  "findings": [
    {"blind_id": "T01", "verdict": "accept", "note": "...", "defect": null}
  ]
}

This tool checks:
  * required top-level fields and types
  * every blind_id exists in that run's blind key
  * no duplicate blind_id
  * verdict is one of accept / reject / undecided
  * referenced run exists in validation.json and passed hash validation
  * gate state is reported honestly (gates_fixed false => findings are advisory)

No pass/fail is decided by this tool. It only checks data integrity.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VERDICTS = {"accept", "reject", "undecided"}


def find_run(validation: dict, lab: str, rd: str) -> dict | None:
    for r in validation.get("results", []):
        if r.get("lab") == lab and r.get("rd") == rd:
            return r
    return None


def blind_ids(run: dict) -> set[str]:
    cap = ROOT / run["manifest"]
    base = cap.parent
    ids = set()
    for name in ("blind_key.json", "temporal_blind_key.json"):
        p = base / name
        if p.exists():
            ids |= set(json.loads(p.read_text(encoding="utf-8")))
    return ids


def validate_finding(data: dict, validation: dict) -> dict:
    errors = []
    warnings = []
    for field in ("lab", "rd", "reviewer", "date"):
        if not isinstance(data.get(field), str) or not data[field]:
            errors.append(f"missing/invalid field: {field}")
    if not isinstance(data.get("findings"), list):
        errors.append("findings must be a list")
        return {"errors": errors, "ok": False}

    run = find_run(validation, data.get("lab", ""), data.get("rd", ""))
    if run is None:
        errors.append("run not found in validation.json")
    elif not run.get("ok"):
        errors.append("referenced run failed hash validation")

    known = blind_ids(run) if run else set()
    seen = set()
    for i, f in enumerate(data["findings"]):
        if not isinstance(f, dict):
            errors.append(f"findings[{i}] must be an object")
            continue
        bid = f.get("blind_id")
        if not isinstance(bid, str) or not bid:
            errors.append(f"findings[{i}] missing blind_id")
            continue
        if bid in seen:
            errors.append(f"duplicate blind_id: {bid}")
        seen.add(bid)
        if known and bid not in known:
            errors.append(f"blind_id not in key: {bid}")
        if f.get("verdict") not in VERDICTS:
            errors.append(f"findings[{i}] invalid verdict: {f.get('verdict')}")
    if not data.get("gates_fixed") and data["findings"]:
        warnings.append("gates_fixed=false: findings are advisory only, not a scored review")

    return {
        "lab": data.get("lab"),
        "rd": data.get("rd"),
        "reviewer": data.get("reviewer"),
        "findings_count": len(data["findings"]),
        "gates_fixed": bool(data.get("gates_fixed")),
        "warnings": warnings,
        "errors": errors,
        "ok": not errors,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validation", default="experiments/review_harness/out/validation.json")
    parser.add_argument("--findings", required=True, help="glob or path to findings JSON")
    parser.add_argument("--out", default="experiments/review_harness/out/findings_check.json")
    args = parser.parse_args()

    vp = ROOT / args.validation if not Path(args.validation).is_absolute() else Path(args.validation)
    validation = json.loads(vp.read_text(encoding="utf-8"))

    paths = sorted(ROOT.glob(args.findings)) if any(c in args.findings for c in "*?") else [Path(args.findings)]
    results = []
    for p in paths:
        fp = p if p.is_absolute() else ROOT / p
        if not fp.exists():
            results.append({"file": str(p), "ok": False, "errors": ["file not found"]})
            continue
        data = json.loads(fp.read_text(encoding="utf-8"))
        result = validate_finding(data, validation)
        result["file"] = str(fp.relative_to(ROOT)).replace("\\", "/")
        results.append(result)

    summary = {
        "files": len(results),
        "ok_files": sum(1 for r in results if r["ok"]),
        "failed_files": sum(1 for r in results if not r["ok"]),
        "results": results,
    }
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"findings check: {summary['ok_files']}/{summary['files']} OK -> {out}")
    for r in results:
        if not r["ok"]:
            print(f"  FAILED {r['file']}: {'; '.join(r['errors'][:3])}")


if __name__ == "__main__":
    main()
