"""Audit manual document update dates under docs/."""

from __future__ import annotations

import argparse
import re
import sys
from datetime import date
from pathlib import Path


UPDATED_RE = re.compile(r"^\*\*最終更新:\*\*\s+(\d{4}-\d{2}-\d{2})\s*$", re.MULTILINE)
GENERATED_RE = re.compile(r"生成日時|generated", re.IGNORECASE)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="docs directory")
    parser.add_argument("--file", dest="files", action="append", type=Path,
                        help="audit only this document; repeatable")
    parser.add_argument("--as-of", type=date.fromisoformat, default=date.today())
    parser.add_argument("--max-age-days", type=int, default=None)
    parser.add_argument("--verbose", action="store_true", help="list every finding")
    args = parser.parse_args()

    missing: list[Path] = []
    invalid: list[tuple[Path, str]] = []
    stale: list[tuple[Path, int]] = []

    paths = args.files or sorted(args.root.rglob("*.md"))
    paths = [path for path in paths if path.exists() and path.is_file()]
    for path in paths:
        text = path.read_text(encoding="utf-8")
        if GENERATED_RE.search(text[:1000]) and "最終更新" not in text[:1000]:
            continue
        match = UPDATED_RE.search(text[:2000])
        if not match:
            missing.append(path)
            continue
        try:
            updated = date.fromisoformat(match.group(1))
        except ValueError as exc:
            invalid.append((path, str(exc)))
            continue
        age = (args.as_of - updated).days
        if age < 0:
            invalid.append((path, "date is in the future"))
        elif args.max_age_days is not None and age > args.max_age_days:
            stale.append((path, age))

    print(f"Documents: {len(paths)}")
    print(f"Missing update dates: {len(missing)}")
    print(f"Invalid dates: {len(invalid)}")
    if args.max_age_days is not None:
        print(f"Stale documents (>{args.max_age_days} days): {len(stale)}")
    if args.verbose:
        for path in missing:
            print(f"  missing: {path}")
        for path, reason in invalid:
            print(f"  invalid: {path} ({reason})")
        for path, age in stale:
            print(f"  stale: {path} ({age} days)")

    return 1 if missing or invalid or stale else 0


if __name__ == "__main__":
    sys.exit(main())
