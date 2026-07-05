# M-LC-3 Live Field Authoring UX

**ステータス:** In Progress

作成日: 2026-07-04  
対象: `Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`, `Artifact/src/Layer/ArtifactAbstractLayer.cppm`, `Artifact/src/Composition/ArtifactAbstractComposition.cppm`, `Artifact/include/Composition/ArtifactAbstractComposition.ixx`  
位置づけ: `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md` の field authoring を、既存 layer transform に対する lightweight な live field として先行実装する。  
参照:
- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md`
- `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`
- `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`
- `docs/COMPOSITION_EDITOR_CONTRACT.md`

---

## 1. 目的

Cinema 4D / Unreal Motion Design 的な field 制作体験を、そのまま巨大な generator system にせず、まずは既存 layer 群へ非破壊で作用する live field として育てる。

今回の先行実装で、次までは到達済み:

- 選択 layer から live radial field を作成
- composition 保存
- enabled / disabled / edit / remove
- non-destructive transform evaluation
- viewport overlay 表示
- one-shot radial transform

一方で、日常的に使うにはまだ次が不足している:

- viewport 上で field center / radius を直接ドラッグできない
- field の選択状態や hover が弱い
- strength / blend / invert / reorder など stack 的な操作がない
- radial 以外の field shape がない
- generator / modifier 側の field mask 契約へまだ接続していない

この milestone は、その未完了分を次回すぐ再開できる形に固定する。

---

## 2. 現在地

### 2.1 実装済み

- `CompositionTransformField` を composition 直下に保持
- `transformFields` の JSON serialize / deserialize
- layer local transform に対する field evaluation
- add / update / remove の undo
- menu からの作成、編集、有効/無効、削除
- 選択対象に関係する field の viewport overlay

### 2.2 未完了

| 軸 | 状況 | 影響 |
|---|---|---|
| Viewport drag | 未実装 | center / radius の編集が毎回ダイアログ寄り |
| Hover / active state | 弱い | どの field を触るか迷いやすい |
| Field stack controls | 未実装 | 複数 field を重ねた制作に移れない |
| Blend / weight / invert | 未実装 | C4D 的な influence 設計に広がらない |
| Shape variety | radial のみ | box / linear / noise 系に発展できない |
| Modifier integration | 未実装 | generator / modifier / dynamics 共通 contract へ繋がっていない |

---

## 3. スコープ

### In Scope

- live transform field の viewport 直接編集
- field の active / hover / hit-test
- radial field の parameter surface 改善
- field list の最小 stack 操作
- 今後の `generator / modifier / field` 共有契約へ繋がるデータ拡張

### Out of Scope

- 完全な C4D clone/effector system
- 3D field
- 新しい global signal 配線
- field ごとの専用 dock 大量追加

---

## 4. 次の実装候補

- viewport direct manipulation の drag handle を小さく追加する
- field list で active / hover / reorder を読めるようにする
- `strength / blend / invert` の最小パラメータを先に通す
- radial 以外の shape は 1 種だけでも追加して契約を広げる

