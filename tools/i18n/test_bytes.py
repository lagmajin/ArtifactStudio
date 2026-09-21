import json
from pathlib import Path

# Load mapping
mapping = json.loads(Path("tools/i18n/mappings/file_menu.json").read_text(encoding="utf-8"))

# Find the save_as entry
for e in mapping:
    if e["key"] == "dialog.save_as.title":
        jp = e["jp"]
        print(f"Mapping jp: {jp!r}")
        print(f"Mapping bytes: {jp.encode('utf-8')!r}")
        # Check in source
        content = Path("Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm").read_text(encoding="utf-8")
        search = f'"{jp}"'
        count = content.count(search)
        print(f"Count in source: {count}")
        # Try to find it manually
        idx = content.find("名前を")
        if idx >= 0:
            snippet = content[idx-5:idx+20]
            print(f"Found at {idx}: {snippet!r}")
            print(f"Snippet bytes: {[b for b in snippet.encode('utf-8')]}")
        break
