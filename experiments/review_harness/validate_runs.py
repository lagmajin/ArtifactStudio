"""Validate evidence hashes and blind-ID consistency across all R&D runs.

Reads manifests under experiments/WatercolorLab/captures/rd* and
experiments/PencilPrintLab/captures/rd*. Does not build or launch anything.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LABS = ("WatercolorLab", "PencilPrintLab", "MaterialSeparationLab", "SeamlessTextureLab")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    base = path.parent
    errors = []
    checked = 0

    evidence = manifest.get("evidence", {})
    items = evidence.items() if isinstance(evidence, dict) else enumerate(evidence)
    for kind, ev in items:
        if not isinstance(ev, dict):
            continue
        rel = ev.get("file")
        if not rel:
            errors.append(f"evidence {kind}: no file")
            continue
        f = base / rel
        if not f.exists():
            errors.append(f"evidence {kind}: missing {rel}")
            continue
        checked += 1
        if sha(f.read_bytes()) != ev.get("sha256"):
            errors.append(f"evidence {kind}: hash mismatch {rel}")

    for stem, out in manifest.get("outputs", {}).items():
        for key in ("npy", "png"):
            name = out.get(key)
            if not name:
                continue
            candidates = [base / "candidates" / name, base / "baselines" / name, base / "fixtures" / name]
            f = next((c for c in candidates if c.exists()), None)
            if f is None:
                errors.append(f"output {stem}.{key}: missing {name}")
                continue
            checked += 1
            if sha(f.read_bytes()) != out.get(f"{key}_sha256"):
                errors.append(f"output {stem}.{key}: hash mismatch {name}")

    for name in ("blind_key.json", "temporal_blind_key.json"):
        key_path = base / name
        if not key_path.exists():
            continue
        key = json.loads(key_path.read_text(encoding="utf-8"))
        forms = list(base.glob("review/*BLIND_FORM.md")) + list(base.glob("review/*BLIND_FORM*.md"))
        if not forms:
            continue
        ids = set()
        for form in set(forms):
            for line in form.read_text(encoding="utf-8").splitlines():
                if line.startswith("| C") or line.startswith("| T"):
                    parts = [p.strip() for p in line.split("|")]
                    if len(parts) > 1:
                        ids.add(parts[1])
        if not ids:
            continue
        checked += 1
        if set(key) != ids:
            errors.append(f"{name}: key/form ID mismatch ({len(set(key) ^ ids)} ids)")

    return {
        "manifest": str(path.relative_to(ROOT)).replace("\\", "/"),
        "lab": manifest.get("lab"),
        "rd": manifest.get("rd"),
        "stage": manifest.get("stage"),
        "gates": manifest.get("gates", "unknown"),
        "checked_items": checked,
        "errors": errors,
        "ok": not errors,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="experiments/review_harness/out")
    args = parser.parse_args()
    out = ROOT / args.out if not Path(args.out).is_absolute() else Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    results = []
    for lab in LABS:
        cap = ROOT / "experiments" / lab / "captures"
        if not cap.exists():
            continue
        for manifest in sorted(cap.glob("rd*/manifest*.json")):
            results.append(validate_manifest(manifest))

    results.sort(key=lambda r: (r["lab"] or "", r["rd"] or "", r["manifest"]))
    summary = {
        "runs": len(results),
        "ok_runs": sum(1 for r in results if r["ok"]),
        "failed_runs": sum(1 for r in results if not r["ok"]),
        "results": results,
    }
    path = out / "validation.json"
    path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"validated {summary['runs']} runs: {summary['ok_runs']} OK, {summary['failed_runs']} failed")
    for r in results:
        if not r["ok"]:
            print(f"  FAILED {r['manifest']}: {'; '.join(r['errors'][:3])}")
    print(f"-> {path}")


if __name__ == "__main__":
    main()
