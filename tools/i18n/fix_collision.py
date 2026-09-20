import json
from pathlib import Path

SRC = Path("Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm")
MAP = Path("tools/i18n/mappings/view_menu.json")
EN = Path("Artifact/translations/en.json")
JA = Path("Artifact/translations/ja.json")

# 1) Rename the leaf key in the source
content = SRC.read_text(encoding="utf-8")
old = 'tr(QStringLiteral("menu.view.resolution"), QStringLiteral("解像度(&R)"))'
new = 'tr(QStringLiteral("menu.view.resolution_label"), QStringLiteral("解像度(&R)"))'
if old in content:
    content = content.replace(old, new)
    SRC.write_text(content, encoding="utf-8")
    print("source renamed")
else:
    print("source pattern not found")

# 2) Rename in the mapping
mapping = json.loads(MAP.read_text(encoding="utf-8"))
renamed = 0
for e in mapping:
    if e["key"] == "menu.view.resolution":
        e["key"] = "menu.view.resolution_label"
        renamed += 1
MAP.write_text(json.dumps(mapping, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"mapping renamed: {renamed}")


def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


en = json.loads(EN.read_text(encoding="utf-8"))
ja = json.loads(JA.read_text(encoding="utf-8"))
set_nested(en, "menu.view.resolution_label".split("."), "Resolution(&R)")
set_nested(ja, "menu.view.resolution_label".split("."), "解像度(&R)")
EN.write_text(json.dumps(en, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
JA.write_text(json.dumps(ja, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print("JSON keys added")
