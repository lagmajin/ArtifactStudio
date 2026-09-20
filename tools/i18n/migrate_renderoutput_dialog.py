import json
from pathlib import Path

ROOT = Path("Artifact")
EN = ROOT / "translations" / "en.json"
JA = ROOT / "translations" / "ja.json"
P = ROOT / "src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm"

ENTRIES = {
    "dialog.render_output.preset_intent_detail": (
        "Each row corresponds to an existing output preset. Here, choose the intent of the output; "
        "final adjustments to resolution, frame range, and destination are made in the Render Queue.\n\n"
        "For transparent assets, choose ProRes 4444 or WebM/VP9. MP4/H.264 is standard for general distribution.",
        "各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\n"
        "解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\n\n"
        "透過素材は ProRes 4444 または WebM/VP9 を選択してください。\n"
        "一般配布用は MP4/H.264 が基本です。"),
    "dialog.render_output.image_seq_no_audio": (
        "Output package: image sequence only. Image sequences cannot contain audio. "
        "If you need audio, add an audio format (WAV PCM) as a separate job in the Render Queue.",
        "出力パッケージ: 画像連番のみ。連番には音声を格納できません。音声が必要な場合はレンダーキューで音声形式（WAV PCM）を別ジョブとして追加してください。"),
    "dialog.render_output.deep_exr_single_sample": (
        "Writes a Deep EXR with 1 sample per pixel from Beauty and Depth AOV. "
        "This is not native multi-sample output.",
        "Beauty と Depth AOV から1ピクセルあたり1サンプルの Deep EXR を書き出します。ネイティブな複数可視サンプル出力ではありません。"),
}


def inner(ja):
    out = []
    for ch in ja:
        if ch == "\\":
            out.append("\\\\")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\"":
            out.append("\\\"")
        else:
            out.append(ch)
    return "".join(out)


src = P.read_text(encoding="utf-8")
for key, (en, ja) in ENTRIES.items():
    needle = 'QStringLiteral("%s")' % inner(ja)
    n = src.count(needle)
    assert n == 1, "%s found %d x" % (key, n)
    src = src.replace(needle, 'TranslationManager::instance().tr(QStringLiteral("%s"), QStringLiteral("%s"))' % (key, inner(ja)))
    for path in (EN, JA):
        d = json.load(open(path, encoding="utf-8"))
        keys = key.split(".")
        # ensure key is new (no clobber)
        cur = d
        exist = True
        for k in keys[:-1]:
            cur = cur.get(k) if isinstance(cur, dict) else None
            if cur is None:
                exist = False; break
        if exist and isinstance(cur, dict) and keys[-1] in cur:
            raise SystemExit("collision risk: %s already exists in %s" % (key, path.name))
        cur = d
        for k in keys[:-1]:
            cur = cur.setdefault(k, {})
            assert isinstance(cur, dict)
        cur[keys[-1]] = ja if path == JA else en
        txt = json.dumps(d, ensure_ascii=False, indent=2)
        if not txt.endswith("\n"):
            txt += "\n"
        path.write_text(txt, encoding="utf-8")
    print("migrated", key)

assert "TranslationManager::instance().tr" in src
P.write_text(src, encoding="utf-8")
print("source updated")
