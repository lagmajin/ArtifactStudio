import re
import json
from pathlib import Path

mapping = json.loads(Path("tools/i18n/mappings/file_menu.json").read_text(encoding="utf-8"))

# Sort by length descending
sorted_map = sorted(mapping, key=lambda e: len(e["jp"]), reverse=True)
alternation = "|".join(re.escape(e["jp"]) for e in sorted_map)

pattern = re.compile(
    r'QStringLiteral\s*\(\s*"(' + alternation + r')"\s*\)'
    r'|"(' + alternation + r')"'
)

content = Path("Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm").read_text(encoding="utf-8")

# Check if "名前をつけて保存" matches
test_str = '"名前をつけて保存"'
m = pattern.search(test_str)
if m:
    print(f"Match in test: {m.group(0)!r}")
else:
    print("No match in test!")
    # Try individual
    for e in sorted_map:
        if "名前" in e["jp"]:
            print(f"  Entry: {e['jp']!r} (len={len(e['jp'])})")
            escaped = re.escape(e["jp"])
            p2 = re.compile(r'"' + escaped + r'"')
            m2 = p2.search(test_str)
            print(f"  Individual match: {m2}")

# Search in content
for i, line in enumerate(content.split("\n"), 1):
    if "名前を" in line:
        m = pattern.search(line)
        if m:
            print(f"Line {i}: MATCH -> {m.group(0)!r}")
        else:
            print(f"Line {i}: NO MATCH -> {line.strip()!r}")
