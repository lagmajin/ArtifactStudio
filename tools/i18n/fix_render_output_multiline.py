"""Fix the remaining two multi-line concatenations (exact indentation)."""
from pathlib import Path

SRC = Path("Artifact/src/Widgets/Dialog/ArtifactRenderOutputSettingDialog.cppm")
content = SRC.read_text(encoding="utf-8")

old1 = (
    '   auto* detailText = new QLabel(QStringLiteral(\n'
    '       "各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\\n"\n'
    '       "解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\\n\\n"\n'
    '       "透過素材は ProRes 4444 または WebM/VP9 を選択してください。\\n"\n'
    '       "一般配布用は MP4/H.264 が基本です。"), detailGroup);'
)
new1 = (
    '   auto* detailText = new QLabel(TranslationManager::instance().tr(QStringLiteral("dialog.render_output.detail_text"),\n'
    '       QStringLiteral("各行は既存の出力プリセットに対応します。ここでは出力の意図を選び、\\n解像度・フレーム範囲・保存先の最終調整は Render Queue で行います。\\n\\n透過素材は ProRes 4444 または WebM/VP9 を選択してください。\\n一般配布用は MP4/H.264 が基本です。")),\n'
    '       detailGroup);'
)

old2 = (
    '     outputPackageLabel->setText(QStringLiteral(\n'
    '         "出力パッケージ: 画像連番のみ。連番には音声を格納できません。"\n'
    '         "音声が必要な場合はレンダーキューで音声形式（WAV PCM）を別ジョブとして追加してください。"));'
)
new2 = (
    '     outputPackageLabel->setText(TranslationManager::instance().tr(QStringLiteral("dialog.render_output.package_image_sequence_full"),\n'
    '         QStringLiteral("出力パッケージ: 画像連番のみ。連番には音声を格納できません。音声が必要な場合はレンダーキューで音声形式（WAV PCM）を別ジョブとして追加してください。")));'
)

for old, new, label in ((old1, new1, "detailText"), (old2, new2, "packageLabel")):
    if old in content:
        content = content.replace(old, new, 1)
        print("fixed:", label)
    else:
        print("NOT FOUND:", label)

SRC.write_text(content, encoding="utf-8")
