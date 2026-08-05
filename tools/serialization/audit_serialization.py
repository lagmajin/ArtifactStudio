"""List statically registered serialization types without invoking a build."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


DIRECT_PATTERN = re.compile(r"registerSerializableType\s*<\s*([A-Za-z_][A-Za-z0-9_:]*)\s*>\s*\(")
ADAPTER_PATTERN = re.compile(
    r"registerJson(?:Array)?SerializableType\s*<\s*([A-Za-z_][A-Za-z0-9_:]*)\s*>\s*\("
)
MIGRATION_PATTERN = re.compile(
    r"registerMigration\s*\(\s*QStringLiteral\(\s*\"([^\"]+)\"\s*\)\s*,\s*(\d+)\s*,\s*(\d+)"
)


def scan(root: Path) -> tuple[set[str], set[str], set[tuple[str, int, int]]]:
    direct: set[str] = set()
    adapters: set[str] = set()
    migrations: set[tuple[str, int, int]] = set()
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in {".cpp", ".cppm", ".ixx"}:
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        direct.update(DIRECT_PATTERN.findall(content))
        adapters.update(ADAPTER_PATTERN.findall(content))
        migrations.update(
            (name, int(source), int(target))
            for name, source, target in MIGRATION_PATTERN.findall(content)
        )
    return direct, adapters, migrations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="+", type=Path)
    args = parser.parse_args()

    direct: set[str] = set()
    adapters: set[str] = set()
    migrations: set[tuple[str, int, int]] = set()
    for root in args.root:
        found_direct, found_adapters, found_migrations = scan(root)
        direct |= found_direct
        adapters |= found_adapters
        migrations |= found_migrations

    print(f"Direct registrations: {len(direct)}")
    for name in sorted(direct):
        print(f"  direct: {name}")
    print(f"JSON adapter registrations: {len(adapters)}")
    for name in sorted(adapters):
        print(f"  adapter: {name}")
    print(f"Schema migrations: {len(migrations)}")
    for name, source, target in sorted(migrations):
        print(f"  migration: {name} v{source}->v{target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
