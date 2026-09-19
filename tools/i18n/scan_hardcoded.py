"""Scan source files for hardcoded Japanese string literals that are NOT inside
a translation call (menuText/tt/tr). Reports per-file counts."""
import re
import sys
from pathlib import Path

ROOT = Path("Artifact/src")
JP = re.compile(r"[\u3040-\u309f\u30a0-\u30ff\u4e00-\u9fff]")
STR = re.compile(r'"([^"\n]*)"')
TR_CALL = re.compile(r"(?:menuText|tt|QObject::tr|\.tr)\s*\(")


def line_is_code(line: str) -> bool:
    s = line.strip()
    return bool(s) and not s.startswith("//") and not s.startswith("*") and not s.startswith("/*")


def protected_spans(content: str):
    spans = []
    for m in TR_CALL.finditer(content):
        depth = 0
        i = m.end() - 1
        first_comma = -1
        closer = -1
        while i < len(content):
            ch = content[i]
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    closer = i
                    break
            elif ch == "," and depth == 1 and first_comma < 0:
                first_comma = i
            i += 1
        if closer >= 0 and first_comma >= 0:
            spans.append((first_comma + 1, closer))
    return spans


total = 0
per_file = {}
for path in ROOT.rglob("*"):
    if path.suffix not in (".cpp", ".cppm", ".ixx", ".h", ".hpp"):
        continue
    try:
        content = path.read_text(encoding="utf-8")
    except Exception:
        continue
    spans = protected_spans(content)
    count = 0
    for line in content.split("\n"):
        if not line_is_code(line):
            continue
        for m in STR.finditer(line):
            if JP.search(m.group(1)):
                # find absolute position
                abs_pos = content.find(m.group(0), 0)
                # approximate: use line start offset
                count += 1
    # The above double counts; do a proper scan instead
    count = 0
    for m in STR.finditer(content):
        if not JP.search(m.group(1)):
            continue
        pos = m.start()
        # skip if inside a comment line
        line_start = content.rfind("\n", 0, pos) + 1
        line = content[line_start:content.find("\n", pos)]
        if not line_is_code(line):
            continue
        if any(s <= pos < e for s, e in spans):
            continue
        count += 1
    if count:
        per_file[str(path).replace("\\", "/")] = count
        total += count

for f, c in sorted(per_file.items(), key=lambda kv: -kv[1]):
    print(f"{c:5d}  {f}")
print(f"\nTOTAL unprotected Japanese strings: {total}")
