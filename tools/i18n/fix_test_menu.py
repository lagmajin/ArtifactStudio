"""Replace the two multi-line adjacent-literal strings in ArtifactTestMenu.cppm
with single TranslationManager::instance().tr(key, fallback) calls, and register
the two combined keys."""

import json
from pathlib import Path

SRC = Path("Artifact/src/Widgets/Menu/ArtifactTestMenu.cppm")
EN = Path("Artifact/translations/en.json")
JA = Path("Artifact/translations/ja.json")

content = SRC.read_text(encoding="utf-8")

case1_old = (
    'QStringLiteral("Debug blend test layers を追加しました。\\n\\n"\n'
    '                     "- Debug Base Plate\\n"\n'
    '                     "- Debug Multiply Plate\\n"\n'
    '                     "- Debug Screen Plate\\n\\n"\n'
    '                     "タイムライン上で並び替えたり、不透明度を変えて合成検証できます。")'
)
case1_new = (
    'TranslationManager::instance().tr(QStringLiteral("menu.test.debug_blend_added"),\n'
    '          QStringLiteral("Debug blend test layers を追加しました。\\n\\n- Debug Base Plate\\n- Debug Multiply Plate\\n- Debug Screen Plate\\n\\nタイムライン上で並び替えたり、不透明度を変えて合成検証できます。"))'
)

case2_old = (
    'QStringLiteral(\n'
    '          "Software Test Pipeline を初期化しました。\\n\\n"\n'
    '          "1) コンポジション作成\\n"\n'
    '          "2) 平面レイヤー追加\\n"\n'
    '          "3) Software Composition Test 起動\\n\\n"\n'
    '          "このメニューからいつでも再起動できます。")'
)
case2_new = (
    'TranslationManager::instance().tr(QStringLiteral("menu.test.pipeline_initialized"),\n'
    '          QStringLiteral("Software Test Pipeline を初期化しました。\\n\\n1) コンポジション作成\\n2) 平面レイヤー追加\\n3) Software Composition Test 起動\\n\\nこのメニューからいつでも再起動できます。"))'
)

for label, old, new in (("case1", case1_old, case1_new), ("case2", case2_old, case2_new)):
    if old in content:
        content = content.replace(old, new, 1)
        print(f"{label}: replaced")
    else:
        print(f"{label}: NOT FOUND")

SRC.write_text(content, encoding="utf-8")

combined = [
    ("menu.test.debug_blend_added",
     r"Added debug blend test layers:\n\n- Debug Base Plate\n- Debug Multiply Plate\n- Debug Screen Plate\n\nYou can reorder them on the timeline and verify compositing by adjusting opacity.",
     r"Debug blend test layers を追加しました。\n\n- Debug Base Plate\n- Debug Multiply Plate\n- Debug Screen Plate\n\nタイムライン上で並び替えたり、不透明度を変えて合成検証できます。"),
    ("menu.test.pipeline_initialized",
     r"Software Test Pipeline initialized.\n\n1) Create composition\n2) Add plane layer\n3) Launch Software Composition Test\n\nYou can restart it anytime from this menu.",
     r"Software Test Pipeline を初期化しました。\n\n1) コンポジション作成\n2) 平面レイヤー追加\n3) Software Composition Test 起動\n\nこのメニューからいつでも再起動できます。"),
]


def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


for locale, idx in (("en", 1), ("ja", 2)):
    path = EN if locale == "en" else JA
    data = json.loads(path.read_text(encoding="utf-8"))
    for entry in combined:
        set_nested(data, entry[0].split("."), entry[idx])
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{locale}: +{len(combined)} keys")
