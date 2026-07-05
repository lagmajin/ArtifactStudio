#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = [ROOT / "Artifact", ROOT / "ArtifactCore", ROOT / "ArtifactWidgets", ROOT / "ArtifactRenderer"]


SELF_IMPORT_RE = re.compile(r"(?m)^\s*(?:export\s+)?import\s+([A-Za-z0-9_.]+)\s*;\s*$")
MODULE_RE = re.compile(r"(?m)^\s*(?:export\s+)?module\s+([A-Za-z0-9_.]+)\s*;\s*$")
EXPORTED_MODULE_RE = re.compile(r"(?m)^\s*export\s+module\s+([A-Za-z0-9_.]+)\s*;\s*$")
INCLUDE_RE = re.compile(r"^\s*#\s*include\b")


def iter_sources():
    for base in SOURCE_ROOTS:
        if not base.exists():
            continue
        yield from base.rglob("*.ixx")
        yield from base.rglob("*.cppm")


def check_self_imports():
    problems = []
    for path in iter_sources():
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        module_match = MODULE_RE.search(text)
        if not module_match:
            continue
        module_name = module_match.group(1)
        if re.search(rf"(?m)^\s*(?:export\s+)?import\s+{re.escape(module_name)}\s*;\s*$", text):
            problems.append(f"{path}: self import of module '{module_name}'")
    return problems


def check_forward_decls():
    problems = []
    banned = {
        ROOT / "Artifact" / "include" / "Layer" / "ArtifactAbstractLayer.ixx": [
            r"class\s+ArtifactAbstractComposition\s*;",
            r"using\s+ArtifactCompositionPtr\s*=",
            r"using\s+ArtifactCompositionWeakPtr\s*=",
        ],
    }
    for path, patterns in banned.items():
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8")
        for pattern in patterns:
            if re.search(pattern, text):
                problems.append(f"{path}: banned pattern matched /{pattern}/")
    return problems


def check_purview_includes():
    problems = []
    for path in iter_sources():
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError:
            continue

        module_declaration_line = None
        for line_number, line in enumerate(lines, start=1):
            if MODULE_RE.fullmatch(line):
                module_declaration_line = line_number
                continue
            if module_declaration_line is not None and INCLUDE_RE.match(line):
                problems.append(
                    f"{path}:{line_number}: #include after module declaration "
                    f"(module purview starts at line {module_declaration_line})"
                )
    return problems


def check_duplicate_exported_modules():
    problems = []
    owners = {}
    for path in iter_sources():
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        match = EXPORTED_MODULE_RE.search(text)
        if not match:
            continue
        module_name = match.group(1)
        owners.setdefault(module_name, []).append(path)

    for module_name, paths in sorted(owners.items()):
        if len(paths) < 2:
            continue
        joined_paths = ", ".join(str(path) for path in sorted(paths))
        problems.append(
            f"duplicate exported module '{module_name}' declared by: {joined_paths}"
        )
    return problems


def main() -> int:
    problems = []
    problems.extend(check_self_imports())
    problems.extend(check_forward_decls())
    problems.extend(check_purview_includes())
    problems.extend(check_duplicate_exported_modules())
    if problems:
        print("Module hygiene check failed:")
        for problem in problems:
            print(f" - {problem}")
        return 1
    print("Module hygiene check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
