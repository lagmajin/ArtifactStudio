from pathlib import Path
import re

c = Path("Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm").read_text(encoding="utf-8")
for i, line in enumerate(c.split("\n"), 1):
    if "QFileDialog" in line and "getSaveFileName" in line:
        print(f"{i}: {line.strip()[:200]}")
    if "名前を" in line:
        print(f"{i}: {line.strip()[:200]}")
