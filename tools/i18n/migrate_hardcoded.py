"""Migrate hardcoded Japanese strings to translation keys.

Usage:
  python tools/i18n/migrate_hardcoded.py <source_file> --mapping <mapping.json>
  python tools/i18n/migrate_hardcoded.py <source_file> --mapping <mapping.json> --dry-run

The mapping file is a JSON array of objects:
  [{"jp": "はい", "key": "dialog.button.yes", "en": "Yes"}, ...]

The script:
  1. Adds new keys to en.json + ja.json (under the appropriate nested path)
  2. Replaces hardcoded strings in the source file with menuText()/tt() calls
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def load_keys(path: Path) -> set[str]:
    """Flatten a locale JSON to a set of dotted keys."""
    data = json.loads(path.read_text(encoding="utf-8"))
    def flatten(d, prefix=""):
        for k, v in d.items():
            fk = f"{prefix}.{k}" if prefix else k
            if isinstance(v, dict):
                yield from flatten(v, fk)
            else:
                yield fk
    return set(flatten(data))


def set_nested(data: dict, parts: list[str], value: str):
    """Set a value at a nested dotted key path, creating intermediate dicts."""
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--mapping", type=Path, required=True,
                        help="JSON file mapping jp strings to keys")
    parser.add_argument("--en-json", type=Path, default=Path("Artifact/translations/en.json"))
    parser.add_argument("--ja-json", type=Path, default=Path("Artifact/translations/ja.json"))
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    mapping = json.loads(args.mapping.read_text(encoding="utf-8"))
    # mapping is list of {"jp": str, "key": str, "en": str}

    source = args.source
    content = source.read_text(encoding="utf-8")

    existing_en = load_keys(args.en_json)
    existing_ja = load_keys(args.ja_json)
    all_existing = existing_en | existing_ja

    en_data = json.loads(args.en_json.read_text(encoding="utf-8"))
    ja_data = json.loads(args.ja_json.read_text(encoding="utf-8"))

    uses_menu_text = "menuText" in content
    uses_tt = bool(re.search(r"QString\s+tt\s*\(", content))

    # Determine the right call: prefer menuText() if file uses it, else tt(), else tr()
    def build_call(key: str, jp_text: str) -> str:
        if uses_menu_text:
            return f'menuText(QStringLiteral("{key}"), QStringLiteral("{jp_text}"))'
        elif uses_tt:
            return f'tt("{key}", QStringLiteral("{jp_text}"))'
        else:
            return f'TranslationManager::instance().tr(QStringLiteral("{key}"), QStringLiteral("{jp_text}"))'

    new_keys = []

    for entry in mapping:
        jp_text = entry["jp"]
        key = entry["key"]
        en_text = entry.get("en", jp_text)

        if key not in all_existing:
            set_nested(en_data, key.split("."), en_text)
            set_nested(ja_data, key.split("."), jp_text)
            new_keys.append(key)
            all_existing.add(key)
        else:
            # Key already exists — update the value if the mapping provides a
            # different one (the mapping is the curated source of truth).
            existing_en_data = en_data
            existing_ja_data = ja_data
            for part in key.split(".")[:-1]:
                if isinstance(existing_en_data, dict):
                    existing_en_data = existing_en_data.get(part, {})
                if isinstance(existing_ja_data, dict):
                    existing_ja_data = existing_ja_data.get(part, {})
            leaf = key.split(".")[-1]
            if isinstance(existing_en_data, dict) and existing_en_data.get(leaf) != en_text:
                set_nested(en_data, key.split("."), en_text)
            if isinstance(existing_ja_data, dict) and existing_ja_data.get(leaf) != jp_text:
                set_nested(ja_data, key.split("."), jp_text)

    if args.dry_run:
        print(f"Would add {len(new_keys)} new keys to JSON")
        for k in new_keys:
            # Find the corresponding mapping entry
            for entry in mapping:
                if entry["key"] == k:
                    print(f"  {k}: en={entry.get('en','')} ja={entry['jp'][:40]}")
                    break
        # Show what would be replaced in source
        replacements = 0
        for entry in mapping:
            jp_text = entry["jp"]
            key = entry["key"]
            # Count occurrences in source
            count = content.count(f'"{jp_text}"')
            if count:
                replacements += count
                call = build_call(key, jp_text)
                print(f"  Source: replace \"{jp_text}\" ({count}x) -> {call}")
        print(f"\nTotal source replacements: {replacements}")
        return 0

    # Write JSON files
    args.en_json.write_text(json.dumps(en_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    args.ja_json.write_text(json.dumps(ja_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Updated JSON files. Added {len(new_keys)} new keys.")

    # Replace in source — only match Japanese strings that are NOT already the
    # fallback argument of an existing translation call.
    #
    # Strategy: find the spans of all translation calls (menuText / tt / .tr /
    # QObject::tr), then protect only the string literals that appear AFTER the
    # first top-level comma inside such a call (i.e. the fallback parameters).
    # Everything else — including QMessageBox titles and messages — is fair game.
    translated_call_re = re.compile(
        r'(?:'
        r'menuText\s*\('
        r'|tt\s*\('
        r'|TranslationManager::instance\(\)\.tr\s*\('
        r'|QObject::tr\s*\('
        r')'
    )

    # For each call site, find its balanced-paren extent, then protect only the
    # region after the first top-level comma (the fallback args).
    protected: list[tuple[int, int]] = []
    for m in translated_call_re.finditer(content):
        depth = 0
        i = m.end() - 1  # positioned on the opening '('
        first_comma = -1
        closer = -1
        while i < len(content):
            ch = content[i]
            if ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                if depth == 0:
                    closer = i
                    break
            elif ch == ',' and depth == 1 and first_comma < 0:
                first_comma = i
            i += 1
        if closer < 0:
            continue
        if first_comma >= 0:
            # Protect the fallback arguments (from the comma to the closing paren)
            protected.append((first_comma + 1, closer))
        # If there is no comma, the whole call is a single key — nothing to protect.

    def is_protected(pos: int) -> bool:
        for s, e in protected:
            if s <= pos < e:
                return True
        return False

    # Now replace Japanese string literals only outside protected spans.
    # Sort by length descending so longer strings match first.
    sorted_map = sorted(mapping, key=lambda e: len(e["jp"]), reverse=True)
    alternation = "|".join(re.escape(e["jp"]) for e in sorted_map)
    pattern = re.compile(
        r'QStringLiteral\s*\(\s*"(' + alternation + r')"\s*\)'
        r'|u8"(' + alternation + r')"'
        r'|"(' + alternation + r')"'
    )

    lookup = {}
    for entry in sorted_map:
        lookup[entry["jp"]] = (entry["key"], build_call(entry["key"], entry["jp"]))

    def is_concatenated(start: int, end: int) -> bool:
        """True if this string literal is part of an adjacent-literal
        concatenation (e.g. "a" "b" spanning lines). Wrapping such a fragment
        in a function call would break the literal concatenation."""
        before = content[:start].rstrip()
        after = content[end:].lstrip()
        return before.endswith('"') or after.startswith('"')

    def replacer(m):
        text = m.group(1) or m.group(2) or m.group(3)
        if is_protected(m.start()):
            return m.group(0)
        if is_concatenated(m.start(), m.end()):
            return m.group(0)
        _, call = lookup[text]
        return call

    new_content = pattern.sub(replacer, content)

    if new_content != content:
        source.write_text(new_content, encoding="utf-8")
        print(f"Rewrote {source}")
    else:
        print(f"No changes needed in {source}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
