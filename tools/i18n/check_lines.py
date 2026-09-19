from pathlib import Path
import sys

c = Path("Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm").read_text(encoding="utf-8")
lines = c.split("\n")
for lineno in [908, 965, 467, 1086, 1072, 1076, 1136, 1107]:
    line = lines[lineno - 1]
    print(f"Line {lineno}: {repr(line.strip())}")
