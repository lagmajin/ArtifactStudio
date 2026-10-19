#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = [ROOT / "Artifact", ROOT / "ArtifactCore", ROOT / "ArtifactWidgets", ROOT / "ArtifactRenderer"]


SELF_IMPORT_RE = re.compile(r"(?m)^\s*(?:export\s+)?import\s+([A-Za-z0-9_.]+)\s*;\s*$")
MODULE_RE = re.compile(r"(?m)^\s*(?:export\s+)?module\s+([A-Za-z0-9_.]+)\s*;\s*$")
EXPORTED_MODULE_RE = re.compile(
    r"(?m)^\s*export\s+module\s+([A-Za-z0-9_.:]+)\s*;\s*$"
)
MODULE_DECLARATION_RE = re.compile(
    r"(?m)^\s*(?:export\s+)?module\s+([A-Za-z0-9_.:]+)\s*;\s*$"
)
INCLUDE_RE = re.compile(r"^\s*#\s*include\b")
CORE_MODULE_REFERENCE_TABLE = (
    ROOT / "ArtifactCore" / "cmake" / "ArtifactCoreModuleReferences.cmake"
)


def normalize_module_name(name: str) -> str:
    # C++ partitions use `:` while CMake's generated module maps and diagnostic
    # tables commonly spell the same provider with `.`.
    return name.replace(":", ".")


def cmake_reference_entries(table_name: str) -> list[tuple[str, ...]]:
    if not CORE_MODULE_REFERENCE_TABLE.is_file():
        return []
    text = CORE_MODULE_REFERENCE_TABLE.read_text(encoding="utf-8-sig")
    match = re.search(
        rf"(?ms)^\s*set\({re.escape(table_name)}\s*(.*?)^\)", text
    )
    if not match:
        return []
    return [
        tuple(field.strip() for field in entry.split("|"))
        for entry in re.findall(r'"([^"\r\n]+)"', match.group(1))
    ]


def declared_module(path: Path) -> str | None:
    if not path.is_file():
        return None
    match = MODULE_DECLARATION_RE.search(path.read_text(encoding="utf-8-sig"))
    return match.group(1) if match else None


def exported_module(path: Path) -> str | None:
    if not path.is_file():
        return None
    match = EXPORTED_MODULE_RE.search(path.read_text(encoding="utf-8-sig"))
    return match.group(1) if match else None


def check_artifactcore_module_reference_tables() -> list[str]:
    """Catch stale or mismatched paths in MSVC/Ninja's explicit IFC maps."""
    problems: list[str] = []
    core_root = ROOT / "ArtifactCore"

    for entry in cmake_reference_entries(
        "ARTIFACTCORE_IMPLEMENTATION_MODULE_REFERENCES"
    ):
        if len(entry) != 3:
            problems.append(
                f"{CORE_MODULE_REFERENCE_TABLE}: malformed implementation reference {entry}"
            )
            continue
        implementation_relative, module_name, interface_relative = entry
        implementation = core_root / implementation_relative
        interface = core_root / interface_relative
        if not implementation.is_file():
            problems.append(f"ArtifactCore IFC map: missing implementation {implementation_relative}")
        if not interface.is_file():
            problems.append(f"ArtifactCore IFC map: missing interface {interface_relative}")
        implementation_module = declared_module(implementation)
        interface_module = exported_module(interface)
        if interface.is_file() and not interface_module:
            problems.append(
                f"ArtifactCore IFC map: {interface_relative} has no exported "
                "module declaration"
            )
        if (
            implementation_module
            and normalize_module_name(implementation_module)
            != normalize_module_name(module_name)
        ):
            problems.append(
                f"ArtifactCore IFC map: {implementation_relative} attaches to "
                f"'{implementation_module}', expected '{module_name}'"
            )
        if (
            interface_module
            and normalize_module_name(interface_module)
            != normalize_module_name(module_name)
        ):
            problems.append(
                f"ArtifactCore IFC map: {interface_relative} declares "
                f"'{interface_module}', expected '{module_name}'"
            )

    imported_provider_by_key: dict[tuple[str, str], str] = {}
    for entry in cmake_reference_entries(
        "ARTIFACTCORE_IMPLEMENTATION_IMPORTED_MODULE_REFERENCES"
    ):
        if len(entry) != 3:
            problems.append(
                f"{CORE_MODULE_REFERENCE_TABLE}: malformed imported reference {entry}"
            )
            continue
        consumer_relative, module_name, provider_relative = entry
        consumer = core_root / consumer_relative
        provider = core_root / provider_relative
        if not consumer.is_file():
            problems.append(f"ArtifactCore IFC map: missing consumer {consumer_relative}")
            continue
        if not provider.is_file():
            problems.append(f"ArtifactCore IFC map: missing provider {provider_relative}")
            continue

        provider_module = declared_module(provider)
        if (
            provider_module
            and normalize_module_name(provider_module)
            != normalize_module_name(module_name)
        ):
            problems.append(
                f"ArtifactCore IFC map: {provider_relative} declares "
                f"'{provider_module}', expected provider '{module_name}'"
            )
        key = (consumer_relative, module_name)
        previous_provider = imported_provider_by_key.setdefault(key, provider_relative)
        if previous_provider != provider_relative:
            problems.append(
                f"ArtifactCore IFC map: {consumer_relative} maps '{module_name}' to both "
                f"{previous_provider} and {provider_relative}"
            )
    return problems


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
    problems.extend(check_artifactcore_module_reference_tables())
    if problems:
        print("Module hygiene check failed:")
        for problem in problems:
            print(f" - {problem}")
        return 1
    print("Module hygiene check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
