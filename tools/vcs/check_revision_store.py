#!/usr/bin/env python3
"""Read-only integrity checker for ArtifactStudio's persisted revision stores."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Any


def read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def load_records(store: Path) -> tuple[list[dict[str, Any]], list[str]]:
    errors: list[str] = []
    jsonl = store / "ledger.jsonl"
    legacy = store / "ledger.json"
    records: list[dict[str, Any]] = []
    if jsonl.is_file():
        with jsonl.open("r", encoding="utf-8-sig") as stream:
            for line_number, line in enumerate(stream, 1):
                if not line.strip():
                    continue
                try:
                    value = json.loads(line)
                    if not isinstance(value, dict):
                        raise ValueError("record is not an object")
                    records.append(value)
                except (json.JSONDecodeError, ValueError) as exc:
                    errors.append(f"ledger.jsonl:{line_number}: invalid record ({exc})")
    elif legacy.is_file():
        try:
            root = read_json(legacy)
            values = root.get("revisions", []) if isinstance(root, dict) else []
            if not isinstance(values, list):
                errors.append("ledger.json: revisions is not an array")
            else:
                records.extend(value for value in values if isinstance(value, dict))
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"ledger.json: cannot parse ({exc})")
    else:
        errors.append("missing ledger.jsonl / ledger.json")
    return records, errors


def check_store(store: Path) -> tuple[list[str], list[str], int]:
    errors: list[str] = []
    notes: list[str] = []
    records, ledger_errors = load_records(store)
    errors.extend(ledger_errors)
    ids: dict[str, dict[str, Any]] = {}
    for index, record in enumerate(records, 1):
        revision_id = record.get("id")
        if not isinstance(revision_id, str) or not revision_id.strip():
            errors.append(f"record {index}: missing revision id")
            continue
        if revision_id in ids:
            errors.append(f"duplicate revision id: {revision_id}")
        ids[revision_id] = record

    snapshots = store / "revisions"
    for revision_id, record in ids.items():
        parents = record.get("parentIds")
        if not isinstance(parents, list):
            parents = [record.get("parentId")] if record.get("parentId") else []
        for parent in parents:
            if parent not in ids:
                errors.append(f"{revision_id}: parent revision not in ledger: {parent}")
        snapshot_name = record.get("snapshotFile") or f"{revision_id}.json"
        if not isinstance(snapshot_name, str) or Path(snapshot_name).name != snapshot_name:
            errors.append(f"{revision_id}: unsafe or invalid snapshotFile")
            continue
        snapshot_path = snapshots / snapshot_name
        if not snapshot_path.is_file():
            errors.append(f"{revision_id}: missing snapshot {snapshot_name}")
            continue
        try:
            snapshot = read_json(snapshot_path)
            if not isinstance(snapshot, dict):
                errors.append(f"{revision_id}: snapshot root is not an object")
        except (OSError, json.JSONDecodeError) as exc:
            errors.append(f"{revision_id}: invalid snapshot ({exc})")

    head_path = store / "head.json"
    if head_path.is_file():
        try:
            head = read_json(head_path).get("headRevisionId", "")
            if head and head not in ids:
                errors.append(f"head points to unknown revision: {head}")
        except (OSError, json.JSONDecodeError, AttributeError) as exc:
            errors.append(f"head.json: cannot parse ({exc})")
    else:
        notes.append("head.json is absent")

    branches_path = store / "branches.json"
    if branches_path.is_file():
        try:
            root = read_json(branches_path)
            branches = root.get("branches", [])
            current_id = root.get("currentBranchId", "")
            if not isinstance(branches, list):
                errors.append("branches.json: branches is not an array")
                branches = []
            branch_ids: set[str] = set()
            for branch in branches:
                if not isinstance(branch, dict):
                    errors.append("branches.json: branch entry is not an object")
                    continue
                branch_id = branch.get("id")
                if not isinstance(branch_id, str) or not branch_id:
                    errors.append("branches.json: branch missing id")
                    continue
                if branch_id in branch_ids:
                    errors.append(f"duplicate branch id: {branch_id}")
                branch_ids.add(branch_id)
                branch_head = branch.get("headRevisionId", "")
                base = branch.get("baseRevisionId", "")
                if branch_head and branch_head not in ids:
                    errors.append(f"branch {branch_id}: unknown head revision {branch_head}")
                if base and base not in ids:
                    errors.append(f"branch {branch_id}: unknown base revision {base}")
            if current_id and current_id not in branch_ids:
                errors.append(f"currentBranchId points to unknown branch: {current_id}")
        except (OSError, json.JSONDecodeError, AttributeError) as exc:
            errors.append(f"branches.json: cannot parse ({exc})")
    else:
        notes.append("branches.json is absent")

    # Parent references should form a DAG. Walk from each node with a local
    # visiting set so malformed cycles are reported without recursion limits.
    parents_by_id: dict[str, list[str]] = {}
    for revision_id, record in ids.items():
        parents = record.get("parentIds")
        if not isinstance(parents, list):
            parents = [record.get("parentId")] if record.get("parentId") else []
        parents_by_id[revision_id] = [p for p in parents if isinstance(p, str) and p in ids]
    colors: dict[str, int] = {}  # 0/unseen, 1/on traversal stack, 2/complete
    cycle_reported = False
    for origin in ids:
        if colors.get(origin, 0):
            continue
        traversal: list[tuple[str, bool]] = [(origin, False)]
        while traversal:
            node, exiting = traversal.pop()
            if exiting:
                colors[node] = 2
                continue
            if colors.get(node, 0) == 2:
                continue
            colors[node] = 1
            traversal.append((node, True))
            for parent in parents_by_id.get(node, []):
                color = colors.get(parent, 0)
                if color == 1 and not cycle_reported:
                    errors.append(f"revision parent cycle detected at {parent}")
                    cycle_reported = True
                elif color == 0:
                    traversal.append((parent, False))

    return errors, notes, len(ids)


def discover_stores(root: Path) -> list[Path]:
    if (root / "ledger.jsonl").is_file() or (root / "ledger.json").is_file():
        return [root]
    return sorted({path.parent for name in ("ledger.jsonl", "ledger.json")
                   for path in root.rglob(name)})


def default_root() -> Path:
    if os.name == "nt":
        base = os.environ.get("APPDATA")
        if base:
            candidate = Path(base) / "ArtifactStudio" / "ArtifactVCS"
            if candidate.exists():
                return candidate
    return Path.home() / ".local" / "share" / "ArtifactStudio" / "ArtifactVCS"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", type=Path, default=default_root(),
                        help="ArtifactVCS root or one project store (default: app data location)")
    args = parser.parse_args()
    root = args.root.expanduser()
    if not root.is_dir():
        print(f"ERROR: revision store directory not found: {root}", file=sys.stderr)
        return 2
    stores = discover_stores(root)
    if not stores:
        print(f"No revision stores found under {root}")
        return 1

    total_errors = 0
    for store in stores:
        errors, notes, count = check_store(store)
        print(f"{'FAIL' if errors else 'PASS'} {store} ({count} revisions)")
        for note in notes:
            print(f"  NOTE: {note}")
        for error in errors:
            print(f"  ERROR: {error}")
        total_errors += len(errors)
    print(f"Checked {len(stores)} store(s); {total_errors} error(s).")
    return 1 if total_errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
