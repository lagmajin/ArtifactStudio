import re
import json
from pathlib import Path

mapping = json.loads(Path("tools/i18n/mappings/file_menu.json").read_text(encoding="utf-8"))

# Find the mapping entry for "名前をつけて保存"
target = None
for e in mapping:
    if "名前を" in e["jp"]:
        target = e
        break

if target:
    print(f"Mapping jp: {target['jp']!r}")
    print(f"Mapping jp bytes: {target['jp'].encode('utf-8')!r}")

# Find the actual string in the source file
content = Path("Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm").read_text(encoding="utf-8")
lines = content.split("\n")

# Check line 467
line = lines[466]  # 0-indexed
print(f"\nLine 467: {line!r}")

# Find the string "名前をつけて保存" in the line
idx = line.find("名前をつけて保存")
if idx >= 0:
    extracted = line[idx:idx+len("名前をつけて保存")]
    print(f"Extracted from source: {extracted!r}")
    print(f"Extracted bytes: {extracted.encode('utf-8')!r}")
    
    if target:
        print(f"Match: {extracted == target['jp']}")
        # Check char by char
        for i, (a, b) in enumerate(zip(extracted, target["jp"])):
            if a != b:
                print(f"  Diff at char {i}: source={a!r} (U+{ord(a):04X}), mapping={b!r} (U+{ord(b):04X})")
