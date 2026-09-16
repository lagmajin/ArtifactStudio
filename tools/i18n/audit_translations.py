"""Audit translation keys used by Artifact sources against locale JSON files."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable


SOURCE_SUFFIXES = {".cpp", ".cppm", ".ixx", ".h", ".hpp"}
KEY_PATTERNS = (
    re.compile(r"\btt\s*\(\s*[\"']([^\"']+)[\"']"),
    re.compile(r"\b(?:tr|AT_TR)\s*\(\s*[\"']([^\"']+)[\"']"),
    re.compile(r"TranslationManager::instance\(\)\.tr\s*\(\s*[\"']([^\"']+)[\"']"),
)

# Dotted snake_case namespaces, e.g. layer_panel.menu_delete, timeline.kind_video.
# Full English sentences must never be used as keys (use tt("ns.key", "English")).
KEY_NAME_RE = re.compile(r"^[a-z][a-z0-9_]*(\.[a-z0-9_]+)+$")


def iter_sources(source_dirs: Iterable[Path]) -> Iterable[Path]:
    for source_dir in source_dirs:
        if source_dir.is_file() and source_dir.suffix in SOURCE_SUFFIXES:
            yield source_dir
        elif source_dir.is_dir():
            yield from (
                path for path in source_dir.rglob("*")
                if path.is_file() and path.suffix in SOURCE_SUFFIXES
            )


def extract_keys(source_dirs: Iterable[Path]) -> set[str]:
    keys: set[str] = set()
    for path in iter_sources(source_dirs):
        try:
            content = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        for pattern in KEY_PATTERNS:
            keys.update(
                key for match in pattern.finditer(content)
                if "." in (key := match.group(1))
            )
    return keys


def flatten_strings(value: object, prefix: str = "") -> dict[str, str]:
    if not isinstance(value, dict):
        return {}
    result: dict[str, str] = {}
    for key, child in value.items():
        full_key = f"{prefix}.{key}" if prefix else key
        if isinstance(child, dict):
            result.update(flatten_strings(child, full_key))
        elif isinstance(child, (str, int, float, bool)):
            result[full_key] = str(child)
    return result


def load_locale(path: Path) -> dict[str, str]:
    with path.open(encoding="utf-8") as handle:
        return flatten_strings(json.load(handle))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="+", type=Path)
    parser.add_argument("--locale", required=True, type=Path)
    parser.add_argument("--baseline", type=Path)
    parser.add_argument("--min-coverage", type=float, default=0.0)
    parser.add_argument("--max-untranslated", type=int, default=None)
    parser.add_argument("--max-invalid-keys", type=int, default=0)
    parser.add_argument("--allow-same", nargs="*", default=[],
                        help="keys intentionally identical to the baseline (e.g. system tokens)")
    args = parser.parse_args()

    source_keys = extract_keys(args.source)
    invalid_keys = sorted(key for key in source_keys if not KEY_NAME_RE.match(key))
    locale_values = load_locale(args.locale)
    translated = set(locale_values)
    baseline_values = load_locale(args.baseline) if args.baseline else {}
    baseline = set(baseline_values)
    expected = source_keys | baseline
    covered = expected & translated
    missing = sorted(expected - translated)
    unused = sorted(translated - expected) if expected else []
    coverage = 100.0 if not expected else len(covered) / len(expected) * 100.0

    print(f"Locale: {args.locale}")
    print(f"Keys used: {len(source_keys)}")
    print(f"Expected: {len(expected)}")
    print(f"Translated: {len(covered)}")
    print(f"Coverage: {coverage:.1f}%")
    untranslated = sorted(
        key for key in expected
        if key in locale_values and key in baseline_values
        and locale_values[key] == baseline_values[key]
        and key not in set(args.allow_same or [])
    )
    print(f"Untranslated (same as baseline): {len(untranslated)}")
    if invalid_keys:
        print(f"Invalid key names: {len(invalid_keys)}")
        print("\n".join(f"  - {key}" for key in invalid_keys))
    if missing:
        print("Missing keys:")
        print("\n".join(f"  - {key}" for key in missing))
    if unused:
        print(f"Unused locale keys: {len(unused)}")

    if args.max_untranslated is not None and len(untranslated) > args.max_untranslated:
        return 1
    if len(invalid_keys) > args.max_invalid_keys:
        return 1
    return 1 if coverage < args.min_coverage else 0


if __name__ == "__main__":
    sys.exit(main())
