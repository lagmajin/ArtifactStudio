import json
from pathlib import Path

mapping_path = Path("tools/i18n/mappings/view_menu.json")
mapping = json.loads(mapping_path.read_text(encoding="utf-8"))

# Fix entries whose jp text does not match the actual source strings.
fixes = {
    "menu.view.load_bookmark_failed": {
        "jp": "ブックマーク「%1」を読み込めませんでした。",
        "en": "Failed to load bookmark '%1'.",
    },
    "menu.view.restore_bookmark_failed": {
        "jp": "ブックマーク「%1」の復元に失敗しました。",
        "en": "Failed to restore bookmark '%1'.",
    },
    "menu.view.load_view_settings_failed": {
        "jp": "ビュー設定「%1」を読み込めませんでした。",
        "en": "Failed to load view settings '%1'.",
    },
    "menu.view.restore_view_settings_failed": {
        "jp": "ビュー設定「%1」の復元に失敗しました。",
        "en": "Failed to restore view settings '%1'.",
    },
}

for entry in mapping:
    fix = fixes.get(entry["key"])
    if fix:
        entry["jp"] = fix["jp"]
        entry["en"] = fix["en"]

mapping_path.write_text(json.dumps(mapping, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Mapping updated: {len(mapping)} entries")
