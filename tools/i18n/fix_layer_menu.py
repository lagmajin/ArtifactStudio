"""Fix the 6 multi-line concatenated strings that the migrator could not handle,
and register combined translation keys for them."""
import json
from pathlib import Path

SRC = Path("Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm")
EN = Path("Artifact/translations/en.json")
JA = Path("Artifact/translations/ja.json")

content = SRC.read_text(encoding="utf-8")

# (old_fragment, new_fragment) — exact text replacements
fixes = [
    (
        'QStringLiteral("Debug blend test layers を追加しました。\\n\\n"\n'
        '                               "- Debug Base Plate\\n"\n'
        '                               "- Debug Multiply Plate\\n"\n'
        '                               "- Debug Screen Plate\\n\\n"\n'
        '                               TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_composite_note"), QStringLiteral("タイムライン上で並び替えたり、不透明度を変えて合成検証できます。"))));',
        'TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_blend_message"),\n'
        '                    QStringLiteral("Debug blend test layers を追加しました。\\n\\n- Debug Base Plate\\n- Debug Multiply Plate\\n- Debug Screen Plate\\n\\nタイムライン上で並び替えたり、不透明度を変えて合成検証できます。")));',
    ),
    (
        'QStringLiteral("Debug Bindless Sprite Planes を追加しました。\\n\\n"\n'
        '                               TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_gradient_note"), QStringLiteral("gradient solid planes だけで構成されるため、"))\n'
        '                               TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_composition_note"), QStringLiteral("composition 描画では SpriteXform packet の bindless batch を検証できます。"))));',
        'TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_bindless_message"),\n'
        '                    QStringLiteral("Debug Bindless Sprite Planes を追加しました。\\n\\ngradient solid planes だけで構成されるため、composition 描画では SpriteXform packet の bindless batch を検証できます。")));',
    ),
    (
        'QStringLiteral("Debug Billboard Particle を追加しました。\\n\\n"\n'
        '                TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_sparkles_note"), QStringLiteral("sparkles プリセットを使うので、ビルボード描画の見え方を"))\n'
        '                TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_visibility_note"), QStringLiteral("確認しやすいはずです。"))));',
        'TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_billboard_message"),\n'
        '                    QStringLiteral("Debug Billboard Particle を追加しました。\\n\\nsparkles プリセットを使うので、ビルボード描画の見え方を確認しやすいはずです。")));',
    ),
    (
        'QStringLiteral("Debug Particle Layer を追加しました。\\n\\n"\n'
        '                               TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_particle_note"), QStringLiteral("通常の ParticleLayer とは独立したデバッグ用レイヤーです。"))));',
        'TranslationManager::instance().tr(QStringLiteral("dialog.layer.debug_particle_message"),\n'
        '                    QStringLiteral("Debug Particle Layer を追加しました。\\n\\n通常の ParticleLayer とは独立したデバッグ用レイヤーです。")));',
    ),
    (
        'QStringLiteral("外側レイヤーの移動量 (%)\\n"\n'
        '                       TranslationManager::instance().tr(QStringLiteral("dialog.layer.direction_note"), QStringLiteral("正数で外側へ、負数で中心へ移動します。"))),',
        'TranslationManager::instance().tr(QStringLiteral("dialog.layer.outer_layer_amount_label"),\n'
        '            QStringLiteral("外側レイヤーの移動量 (%)\\n正数で外側へ、負数で中心へ移動します。")),',
    ),
    (
        'QStringLiteral("最外周のスケール (%)\\n"\n'
        '                       TranslationManager::instance().tr(QStringLiteral("dialog.layer.center_scale_note"), QStringLiteral("中心は元のスケールを維持します。"))),',
        'TranslationManager::instance().tr(QStringLiteral("dialog.layer.outer_scale_label"),\n'
        '            QStringLiteral("最外周のスケール (%)\\n中心は元のスケールを維持します。")),',
    ),
]

for old, new in fixes:
    if old not in content:
        print("NOT FOUND:", old[:70].replace("\n", "\\n"))
    else:
        content = content.replace(old, new, 1)
        print("fixed:", new[:70].replace("\n", "\\n"))

SRC.write_text(content, encoding="utf-8")

combined = [
    ("dialog.layer.debug_blend_message",
     "Added Debug blend test layers.\\n\\n- Debug Base Plate\\n- Debug Multiply Plate\\n- Debug Screen Plate\\n\\nYou can reorder them on the timeline and change opacity to test compositing.",
     "Debug blend test layers を追加しました。\\n\\n- Debug Base Plate\\n- Debug Multiply Plate\\n- Debug Screen Plate\\n\\nタイムライン上で並び替えたり、不透明度を変えて合成検証できます。"),
    ("dialog.layer.debug_bindless_message",
     "Added Debug Bindless Sprite Planes.\\n\\nBecause it consists only of gradient solid planes, composition rendering verifies SpriteXform packet bindless batches.",
     "Debug Bindless Sprite Planes を追加しました。\\n\\ngradient solid planes だけで構成されるため、composition 描画では SpriteXform packet の bindless batch を検証できます。"),
    ("dialog.layer.debug_billboard_message",
     "Added Debug Billboard Particle.\\n\\nIt uses the sparkles preset, so it should be easier to check how billboard rendering looks.",
     "Debug Billboard Particle を追加しました。\\n\\nsparkles プリセットを使うので、ビルボード描画の見え方を確認しやすいはずです。"),
    ("dialog.layer.debug_particle_message",
     "Added Debug Particle Layer.\\n\\nThis debug layer is independent of the normal ParticleLayer.",
     "Debug Particle Layer を追加しました。\\n\\n通常の ParticleLayer とは独立したデバッグ用レイヤーです。"),
    ("dialog.layer.outer_layer_amount_label",
     "Outer layer amount (%)\\nPositive values move outward, negative values move toward the center.",
     "外側レイヤーの移動量 (%)\\n正数で外側へ、負数で中心へ移動します。"),
    ("dialog.layer.outer_scale_label",
     "Outermost scale (%)\\nThe center keeps its original scale.",
     "最外周のスケール (%)\\n中心は元のスケールを維持します。"),
]


def set_nested(data, parts, value):
    d = data
    for part in parts[:-1]:
        if part not in d or not isinstance(d.get(part), dict):
            d[part] = {}
        d = d[part]
    d[parts[-1]] = value


en = json.loads(EN.read_text(encoding="utf-8"))
ja = json.loads(JA.read_text(encoding="utf-8"))
for key, en_text, ja_text in combined:
    set_nested(en, key.split("."), en_text)
    set_nested(ja, key.split("."), ja_text)

EN.write_text(json.dumps(en, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
JA.write_text(json.dumps(ja, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Registered {len(combined)} combined keys")
