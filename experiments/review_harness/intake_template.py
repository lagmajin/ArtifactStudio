"""Intake a filled review template into out/findings and re-verify.

Reads a JSON template from out/templates/<lab>/<rd>/, validates it against the
same contract as validate_findings.py (required fields, valid verdicts, blind
IDs present in that run's key), then copies the validated submission into
out/findings/<lab>_<rd>.json. Finally runs post_findings_verify.py --strict.

An empty template (blank reviewer/date or no findings) is rejected. Existing
submissions are never overwritten unless --force is given. The tool does not
enter thresholds or make quality decisions.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

from validate_findings import validate_finding

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
OUT = HARNESS / "out"
PY = sys.executable
VERDICTS = {"accept", "reject", "undecided"}


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def blank_fields(data: dict) -> list[str]:
    blanks = []
    for field in ("lab", "rd", "reviewer", "date"):
        if not isinstance(data.get(field), str) or not data[field].strip():
            blanks.append(field)
    findings = data.get("findings")
    if not isinstance(findings, list) or not findings:
        blanks.append("findings (empty)")
    return blanks


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template", required=True, help="filled template JSON path")
    parser.add_argument("--findings-dir", default="experiments/review_harness/out/findings")
    parser.add_argument("--report", default="experiments/review_harness/out/intake_report.json")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--skip-verify", action="store_true")
    args = parser.parse_args()

    def resolve(p: str) -> Path:
        return ROOT / p if not Path(p).is_absolute() else Path(p)

    template = resolve(args.template)
    if not template.exists():
        raise SystemExit(f"template not found: {template}")
    data = load(template)

    blanks = blank_fields(data)
    if blanks:
        raise SystemExit(f"rejected: template is not filled: {', '.join(blanks)}")

    # Strip template-only metadata before treating the file as a submission.
    submission = {k: v for k, v in data.items() if k != "_template"}
    for finding in submission.get("findings", []):
        if not isinstance(finding, dict) or finding.get("verdict") not in VERDICTS:
            raise SystemExit(f"rejected: invalid finding verdict")
        if not isinstance(finding.get("blind_id"), str) or not finding["blind_id"]:
            raise SystemExit("rejected: missing blind_id")

    validation = load(OUT / "validation.json")
    check = validate_finding(submission, validation)
    if not check["ok"]:
        raise SystemExit("rejected: " + "; ".join(check["errors"]))

    fd = resolve(args.findings_dir)
    fd.mkdir(parents=True, exist_ok=True)
    dest = fd / f"{submission['lab']}_{submission['rd']}.json"
    if dest.exists() and not args.force:
        raise SystemExit(f"submission already exists (use --force): {dest}")
    dest.write_text(json.dumps(submission, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    verify = None
    if not args.skip_verify:
        proc = subprocess.run(
            [PY, str(HARNESS / "post_findings_verify.py"), "--strict"],
            cwd=ROOT, capture_output=True, text=True,
        )
        verify = {
            "ok": proc.returncode == 0,
            "stdout": proc.stdout.strip(),
            "stderr": proc.stderr.strip(),
        }
        print(proc.stdout.strip())

    report = {
        "template": str(template.relative_to(ROOT)).replace("\\", "/"),
        "submission": str(dest.relative_to(ROOT)).replace("\\", "/"),
        "validation": check,
        "post_findings_verify": verify,
        "note": "No threshold was entered and no quality decision was made by this tool.",
    }
    rp = resolve(args.report)
    rp.parent.mkdir(parents=True, exist_ok=True)
    rp.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"intake OK: {dest.name} (verify {'OK' if verify and verify['ok'] else 'skipped/failed'}) -> {rp}")
    if verify and not verify["ok"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
