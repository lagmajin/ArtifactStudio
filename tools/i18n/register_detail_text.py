import json
from pathlib import Path


def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


key = "dialog.render_output.detail_text"
en = (
    r"Each row corresponds to an existing output preset. Here you choose the output intent, and\n"
    r"final adjustments to resolution, frame range, and destination are made in the Render Queue.\n\n"
    r"For transparent material, select ProRes 4444 or WebM/VP9.\n"
    r"MP4/H.264 is the standard for general distribution."
)
ja = (
    r"各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\n"
    r"解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\n\n"
    r"透過素材は ProRes 4444 または WebM/VP9 を選択してください。\n"
    r"一般配布用は MP4/H.264 が基本です。"
)

for locale, text in (("en", en), ("ja", ja)):
    p = Path(f"Artifact/translations/{locale}.json")
    d = json.loads(p.read_text(encoding="utf-8"))
    set_nested(d, key.split("."), text)
    p.write_text(json.dumps(d, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{locale}: registered {key}")
