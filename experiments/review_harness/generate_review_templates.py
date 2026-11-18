"""Generate review-submission templates for every validated R&D run.

Templates are written to out/templates/, NOT to out/findings/, so they are
never mistaken for actual submissions. Each template carries the run's real
blind IDs and a blank reviewer/verdict structure. Existing templates are kept
unless --force is given.

No gate is set and no quality decision is made.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "experiments" / "review_harness"
OUT = HARNESS / "out"
TEMPLATES = OUT / "templates"
ITEMS = (
    "5.1 効果の正しさ", "5.2 画質と細部", "5.3 色と alpha", "5.4 時間安定性",
    "5.5 経路と解像度", "5.6 性能", "5.7 効果固有",
)


def blind_ids(run: dict) -> list[str]:
    manifest = ROOT / run["manifest"]
    ids = []
    for name in ("blind_key.json", "temporal_blind_key.json"):
        p = manifest.parent / name
        if p.exists():
            ids.extend(json.loads(p.read_text(encoding="utf-8")).keys())
    return sorted(set(ids))


def json_template(run: dict, ids: list[str]) -> str:
    data = {
        "lab": run.get("lab"),
        "rd": run.get("rd"),
        "reviewer": "",
        "date": "",
        "gates_fixed": False,
        "findings": [],
        "_template": {
            "purpose": "Copy this file to out/findings/ after filling reviewer/date/findings.",
            "blind_ids_available": ids,
            "verdict_values": ["accept", "reject", "undecided"],
            "example_finding": {"blind_id": ids[0] if ids else "C01", "verdict": "undecided",
                                "note": "", "defect": None},
            "warning": "Set gates_fixed=true only after entering thresholds in GATE_FORM.md.",
        },
    }
    return json.dumps(data, indent=2, ensure_ascii=False) + "\n"


def md_template(run: dict, ids: list[str]) -> str:
    rows = "\n".join(f"| {bid} | | | | |" for bid in ids)
    gates = "\n".join(f"| {item} | | | | | | |" for item in ITEMS)
    return (
        f"# Review submission template: {run.get('lab')}/{run.get('rd')}\n\n"
        "**This is a template, not a submission.** Fill it and copy the JSON form to `out/findings/`.\n\n"
        "## Findings\n\n"
        "| blind ID | verdict | note | defect | reviewer |\n"
        "|---|---|---|---|---|\n" + rows + "\n\n"
        "## Gate thresholds (mirror of GATE_FORM.md)\n\n"
        "| item | region | evidence | tolerance | judge | fix date | rationale |\n"
        "|---|---|---|---|---|---|---|\n" + gates + "\n\n"
        "Set thresholds after viewing the baseline. `gates_fixed=true` only when all thresholds are entered.\n"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validation", default="experiments/review_harness/out/validation.json")
    parser.add_argument("--out", default="experiments/review_harness/out/templates")
    parser.add_argument("--force", action="store_true", help="overwrite existing templates")
    args = parser.parse_args()

    vp = ROOT / args.validation if not Path(args.validation).is_absolute() else Path(args.validation)
    validation = json.loads(vp.read_text(encoding="utf-8"))
    out_dir = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    generated, skipped = [], []
    unique_runs = {}
    for run in validation.get("results", []):
        lab, rd = run.get("lab"), run.get("rd")
        if not lab or not rd:
            continue
        unique_runs.setdefault((lab, rd), run)  # nested blind manifests share the parent run
    for (lab, rd), run in unique_runs.items():
        d = out_dir / lab / rd
        d.mkdir(parents=True, exist_ok=True)
        ids = blind_ids(run)
        for suffix, content in (("submission.json", json_template(run, ids)),
                                ("submission.md", md_template(run, ids))):
            path = d / suffix
            if path.exists() and not args.force:
                skipped.append(str(path.relative_to(ROOT)).replace("\\", "/"))
                continue
            path.write_text(content, encoding="utf-8")
            generated.append(str(path.relative_to(ROOT)).replace("\\", "/"))

    index = {
        "generated": generated,
        "skipped": skipped,
        "runs": len(unique_runs),
        "note": "Templates are in out/templates/, never in out/findings/.",
    }
    (out_dir / "index.json").write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"review templates: generated {len(generated)}, skipped {len(skipped)}, "
          f"runs {index['runs']} -> {out_dir.relative_to(ROOT).as_posix()}")
    if skipped:
        print(f"  skipped existing (use --force to overwrite): {len(skipped)}")


if __name__ == "__main__":
    main()
