"""Migrate the remaining bare Japanese literals in LayerMenu and RenderMenu to
TranslationManager::instance().tr(key, fallback), and register the matching keys
in Artifact/translations/{en,ja}.json without reformatting the files.

Safety: every replacement asserts the needle occurs exactly once. Nothing is
written unless all asserts pass, so a mismatch can never corrupt the JSON.
"""
import json
from pathlib import Path

ROOT = Path("Artifact")
EN = ROOT / "translations" / "en.json"
JA = ROOT / "translations" / "ja.json"
LAYER = ROOT / "src/Widgets/Menu/ArtifactLayerMenu.cppm"
RENDER = ROOT / "src/Widgets/Menu/ArtifactRenderMenu.cppm"

# Single source of truth. Real newlines; the C++ backslash-n form is derived.
ENTRIES = {
    "menu.render.add_to_queue":
        ("Add to Render Queue (&A)", "現在のコンポジションをレンダーキューに追加(&A)"),
    "menu.render.show_queue":
        ("Show Render Queue (&Q)...", "レンダーキューを表示(&Q)..."),
    "menu.render.show_manager":
        ("Show Render Manager (&M)...", "レンダーマネージャーを表示(&M)..."),
    "menu.render.output_settings":
        ("Render Output Settings (&S)...", "レンダー出力設定(&S)..."),
    "menu.render.start":
        ("Start / Resume Rendering (&S)", "レンダリングを開始／再開(&S)"),
    "menu.render.pause":
        ("Pause Rendering", "レンダリングを一時停止"),
    "menu.render.clear_all":
        ("Clear All Jobs (&C)", "すべてのジョブをクリア(&C)"),
    "menu.render.cancel_all":
        ("Cancel All Jobs", "全ジョブをキャンセル"),
    "menu.render.add_all_compositions":
        ("Add All Compositions to Queue (&A)", "全コンポジションをキューに追加(&A)"),
    "dialog.render.no_active_composition":
        ("No active composition.", "アクティブなコンポジションがありません。"),
    "dialog.render.added_to_queue":
        ("%1 composition(s) added to the render queue.", "%1 個のコンポジションをキューに追加しました。"),
    "dialog.render.no_composition_to_add":
        ("There are no compositions that can be added.", "追加できるコンポジションがありません。"),
    "menu.layer.debug_blend_added":
        ("Added debug blend test layers:\n\n- Debug Base Plate\n- Debug Multiply Plate\n- Debug Screen Plate\n\nReorder on the timeline and verify compositing by adjusting opacity.",
         "Debug blend test layers を追加しました。\n\n- Debug Base Plate\n- Debug Multiply Plate\n- Debug Screen Plate\n\nタイムライン上で並び替えたり、不透明度を変えて合成検証できます。"),
    "menu.layer.debug_bindless_added":
        ("Added debug bindless sprite planes.\n\nComposed of gradient solid planes only, so the bindless batch of the SpriteXform packet can be verified in composition rendering.",
         "Debug Bindless Sprite Planes を追加しました。\n\ngradient solid planes だけで構成されるため、composition 描画では SpriteXform packet の bindless batch を検証できます。"),
    "menu.layer.debug_billboard_added":
        ("Added debug billboard particle layer.\n\nUses the sparkles preset, so the billboard rendering should be easy to verify.",
         "Debug Billboard Particle を追加しました。\n\nsparkles プリセットを使うので、ビルボード描画の見え方を確認しやすいはずです。"),
    "menu.layer.debug_particle_added":
        ("Added debug particle layer.\n\nA debug layer independent of the normal ParticleLayer.",
         "Debug Particle Layer を追加しました。\n\n通常の ParticleLayer とは独立したデバッグ用レイヤーです。"),
    "dialog.layer.outer_offset":
        ("Outer layer offset (%)\nPositive values move outward; negative values move toward center.",
         "外側レイヤーの移動量 (%)\n正数で外側へ、負数で中心へ移動します。"),
    "dialog.layer.outer_scale":
        ("Outer perimeter scale (%)\nThe center retains its original scale.",
         "最外周のスケール (%)\n中心は元のスケールを維持します。"),
}


def to_src_inner(ja):
    # C++ source represents a newline as backslash-n. Real chars -> C++ escapes.
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


def tr_call(key, ja):
    inner = to_src_inner(ja)
    return ('TranslationManager::instance().tr('
            'QStringLiteral("%s"), QStringLiteral("%s"))' % (key, inner))


def needle(key, form):
    inner = to_src_inner(ENTRIES[key][1])
    if form == "ql":
        return 'QStringLiteral("%s")' % inner
    if form == "qstr":
        return 'QString("%s")' % inner
    return '"%s"' % inner  # bare


layer_needles = {
    "menu.layer.debug_blend_added": "ql",
    "menu.layer.debug_bindless_added": "ql",
    "menu.layer.debug_billboard_added": "ql",
    "menu.layer.debug_particle_added": "ql",
    "dialog.layer.outer_offset": "ql",
    "dialog.layer.outer_scale": "ql",
}
render_needles = {
    "menu.render.add_to_queue": "bare",
    "menu.render.show_queue": "bare",
    "menu.render.show_manager": "bare",
    "menu.render.output_settings": "bare",
    "menu.render.start": "bare",
    "menu.render.pause": "ql",
    "menu.render.cancel_all": "ql",
    "menu.render.clear_all": "bare",
    "menu.render.add_all_compositions": "bare",
    "dialog.render.no_active_composition": "bare",
    "dialog.render.added_to_queue": "qstr",
    "dialog.render.no_composition_to_add": "bare",
}

layer_src = LAYER.read_text(encoding="utf-8")
render_src = RENDER.read_text(encoding="utf-8")

for key, form in layer_needles.items():
    ndl = needle(key, form)
    n = layer_src.count(ndl)
    assert n == 1, "LayerMenu %s found %d x" % (key, n)
    layer_src = layer_src.replace(ndl, tr_call(key, ENTRIES[key][1]))

for key, form in render_needles.items():
    ndl = needle(key, form)
    n = render_src.count(ndl)
    assert n == 1, "RenderMenu %s found %d x" % (key, n)
    render_src = render_src.replace(ndl, tr_call(key, ENTRIES[key][1]))

# Verify the keys are now referenced in source.
for key in list(layer_needles) + list(render_needles):
    assert key in layer_src or key in render_src, key

LAYER.write_text(layer_src, encoding="utf-8")
RENDER.write_text(render_src, encoding="utf-8")
print("source updated OK: LayerMenu(%d), RenderMenu(%d)" % (len(layer_needles), len(render_needles)))


def set_nested(d, keys, value):
    cur = d
    for k in keys[:-1]:
        cur = cur.setdefault(k, {})
        if not isinstance(cur, dict):
            raise RuntimeError("conflict at " + k)
    cur[keys[-1]] = value


def dump_json(path, d):
    txt = json.dumps(d, ensure_ascii=False, indent=2)
    if not txt.endswith("\n"):
        txt += "\n"
    path.write_text(txt, encoding="utf-8")


for path, idx in ((EN, 0), (JA, 1)):
    d = json.load(open(path, encoding="utf-8"))
    for key, (en, ja) in ENTRIES.items():
        set_nested(d, key.split("."), ja if idx else en)
    dump_json(path, d)
    print("json updated:", path.name)
