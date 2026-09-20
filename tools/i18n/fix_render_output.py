"""Fix the RenderOutputSettingDialog mapping (literal backslash-n) and the
multi-line concatenated string that the migrator broke."""
import json
from pathlib import Path

MAP = Path("tools/i18n/mappings/render_output_dialog.json")
SRC = Path("Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm")

# --- 1) Fix mapping values: real newline -> literal backslash + n ---------
mapping = json.loads(MAP.read_text(encoding="utf-8"))
fixed = 0
for e in mapping:
    for field in ("jp", "en"):
        v = e.get(field)
        if isinstance(v, str) and "\n" in v:
            e[field] = v.replace("\n", r"\n")
            fixed += 1
MAP.write_text(json.dumps(mapping, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"mapping values fixed: {fixed}")


def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


# --- 2) Register the combined multi-line key -----------------------------
combined_key = "dialog.render_output.detail_text"
combined_en = (
    r"Each row corresponds to an existing output preset. Here you choose the output intent, and\n"
    r"final adjustments to resolution, frame range, and destination are made in the Render Queue.\n\n"
    r"For transparent material, select ProRes 4444 or WebM/VP9.\n"
    r"MP4/H.264 is the standard for general distribution."
)
combined_ja = (
    r"各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\n"
    r"解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\n\n"
    r"透過素材は ProRes 4444 または WebM/VP9 を選択してください。\n"
    r"一般配布用は MP4/H.264 が基本です。"
)
for locale, text in (("en", combined_en), ("ja", combined_ja)):
    p = Path(f"Artifact/translations/{locale}.json")
    d = json.loads(p.read_text(encoding="utf-8"))
    set_nested(d, combined_key.split("."), text)
    p.write_text(json.dumps(d, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print("combined key registered")

# --- 3) Rebuild the broken multi-line QLabel -----------------------------
content = SRC.read_text(encoding="utf-8")

broken = (
    'auto* detailText = new QLabel(QStringLiteral(\n'
    '        "各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\\n"\n'
    '        "解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\\n\\n"\n'
    '        "透過素材は ProRes 4444 または WebM/VP9 を選択してください。\\n"\n'
    '        TranslationManager::instance().tr(QStringLiteral("dialog.render_output.mp4_basic"), QStringLiteral("一般配布用は MP4/H.264 が基本です。"))), detailGroup);'
)
good = (
    'auto* detailText = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.render_output.detail_text"),\n'
    '        QStringLiteral("各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\\n解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\\n\\n透過素材は ProRes 4444 または WebM/VP9 を選択してください。\\n一般配布用は MP4/H.264 が基本です。")), detailGroup);'
)

if broken in content:
    content = content.replace(broken, good, 1)
    SRC.write_text(content, encoding="utf-8")
    print("multi-line label fixed")
else:
    print("multi-line label NOT FOUND")
