"""Finish ColorSwatchDialog: convert the concatenated accessible-name suffix
into a single %1 key."""

import json
from pathlib import Path

SRC = Path("Artifact/src/Widgets/Dialog/ColorSwatchDialog.cppm")
EN = Path("Artifact/translations/en.json")
JA = Path("Artifact/translations/ja.json")

c = SRC.read_text(encoding="utf-8")
old = 'addBtn->setAccessibleName(cat.name + QStringLiteral(" にカラーを追加"));'
new = (
    'addBtn->setAccessibleName(\n'
    '        TranslationManager::instance()\n'
    '            .tr(QStringLiteral("dialog.color_swatch.add_color_to"),\n'
    '                QStringLiteral("%1 にカラーを追加"))\n'
    '            .arg(cat.name));'
)
if old in c:
    c = c.replace(old, new, 1)
    SRC.write_text(c, encoding="utf-8")
    print("source: replaced")
else:
    print("source: NOT FOUND")


def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


for locale, path in (("en", EN), ("ja", JA)):
    data = json.loads(path.read_text(encoding="utf-8"))
    existing = data.get("dialog", {}).get("color_swatch", {}).get("add_color_to")
    if existing is None:
        set_nested(data, "dialog.color_swatch.add_color_to".split("."),
                   "Add a color to %1" if locale == "en" else "%1 にカラーを追加")
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"{locale}: key added")
    else:
        print(f"{locale}: key already present ({existing!r})")
