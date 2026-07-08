# Timeline Right Pane Keyframe Edit Refinement

**Date**: 2026-05-23
**Status**: Completed
**Window**: Next 1-2 weeks
**Parent**: `M-TL-5 Timeline Keyframe Editing`

このマイルストーンは、タイムライン右下の編集面である `TimelineTrackView` を、実際の制作で迷わず使える keyframe edit surface に寄せるための follow-up です。

`docs/WIDGET_MAP.md` の責務に従い、対象は右パネルの keyframe 編集体験に限定します。左ペインの行操作や Inspector の summary 責務までは飲み込まず、`ArtifactTimelineWidget` の orchestration と `ArtifactTimelineTrackPainterView` の right-side editing surface を磨くことに集中します。

---

## Goal

- 右パネルで「どの keyframe が今の操作対象か」をすぐ読める
- 選択、範囲選択、ドラッグ移動、前後ジャンプの意図がぶれない
- `ArtifactPropertyWidget` / `PropertyEditor` と timeline が同じ keyframe source を見る
- 既にある add / remove / copy / paste / interpolation 導線を、迷わず使える形に揃える
- motion path / current frame / search / filter と timeline keyframe state の食い違いを減らす

---

## Why Now

- `MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` で visibility の土台はできてきた
- `ArtifactTimelineTrackPainterView` には marker selection、rectangle selection、batch move の足場が既にある
- `ArtifactTimelineWidget` には add / remove / copy / paste / jump 系の action が生えている
- それでも実務上は「見えるが、まだ迷う」状態が残りやすい

今のボトルネックは機能不足より、右パネル上の state readability と input consistency にある。

Progress note:

- keyframe selection summary now calls out nearest/current proximity more explicitly
- hovered and selected state are still surfaced through the existing summary path
- current/nearest/hovered marker emphasis is already separated in the timeline painter

---

## In Scope

- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/include/Widgets/ArtifactTimelineWidget.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
- `Artifact/include/Widgets/Timeline/ArtifactTimelineKeyframeModel.ixx`
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
- `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 必要に応じて `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

## Out Of Scope

- Graph Editor の全面導入
- 左ペインの大規模再設計
- `DiligentEngine` / DX12 backend の低レベル変更
- 新しいグローバル signal / slot 網の追加
- `AbstractProperty` 以外を source of truth にする別系統モデルの増設

---

## Current Ground Truth

- 右パネルの正規 editing surface は `ArtifactTimelineTrackPainterView`
- orchestration は `ArtifactTimelineWidget`
- keyframe source は `ArtifactTimelineKeyframeModel` と property 側 API
- menu / shortcut 入口は `ArtifactAnimationMenu`
- property row 側の keyframe button は `ArtifactPropertyEditor`

このマイルストーンでは、新しい中央配線を増やさず、既存の action / model / property path を揃える。

---

## Problem Statement

現状の右パネルには次のズレが残りやすい。

- marker は見えていても、selected / nearest / current-frame-hit の意味差が弱い
- keyframe selection と clip / layer selection の責務境界が一瞬で読めない
- range selection、drag move、seek の入力意図が衝突しやすい
- add / remove / copy / paste / jump はあるが、右パネル中心の編集フローとしてつながり切っていない
- property row、timeline marker、motion path keyframe が別々に見えやすい

---

## Phases

### Phase 1: Right Pane State Readability

目的:
右パネルだけ見て、keyframe の状態差が分かるようにする。

作業項目:

- selected / hovered / nearest / current-frame-hit の marker visual を分離する
- selected layer emphasis と selected keyframe emphasis を混ぜない
- marker 密度が高い行でも current frame 周辺を読みやすくする
- empty lane / hidden-by-filter / keyframed-but-collapsed の見え方を整理する

Done when:

- 右パネル上で「何が選ばれていて、何が近く、何が今フレームにあるか」を説明できる
- 状態違いが色だけに依存しない

### Phase 2: Selection And Intent Discipline

目的:
click / drag / marquee / seek の意図を固定する。

作業項目:

- keyframe selection と clip selection の入力優先順位を明文化する
- plain click / Shift / Ctrl の追加・解除・置換ルールを揃える
- marquee selection の対象と除外対象を固定する
- drag 開始閾値と seek-only click の境界を調整する

Done when:

- 誤って seek したり move したりする頻度が下がる
- 複数 keyframe 編集で selection の挙動を予測できる

### Phase 3: Move Safety And Snap Polish

目的:
keyframe 移動を怖くない操作にする。

作業項目:

- single / multi-selection move の preview と commit の挙動を揃える
- current frame、近傍 keyframe、grid への snap を整理する
- drag 中の時間差表示と相対移動量表示を読みやすくする
- collision、same-frame merge、locked state の扱いを決める

Done when:

- batch move でも結果を予測しやすい
- move requested と property 更新結果がズレない

### Phase 4: Edit Flow Convergence

目的:
右パネル上の編集操作を断片化させない。

作業項目:

- add / remove / copy / paste / interpolation を timeline action と同じ語彙で揃える
- `ArtifactAnimationMenu`、shortcut、context menu の対象判定を一致させる
- playhead 基準 paste と selection 基準 paste のルールを固定する
- jump-to-next / previous / first / last の対象集合を明文化する

Done when:

- menu でも shortcut でも context menu でも結果が一致する
- 「どの keyframe 集合に対する操作か」が曖昧にならない

### Phase 5: Property / Viewport Round-Trip

目的:
property row、timeline、viewport overlay の往復コストを下げる。

作業項目:

- property row の keyframe button 状態と timeline marker state を揃える
- timeline で選んだ keyframe が property 側でも追えるようにする
- transform / motion path keyframe の current frame 文脈を合わせる
- keyframe selection を変えた後の header / status summary を安定化する

Done when:

- property と timeline を往復しても編集対象を見失わない
- viewport の motion path 操作と timeline の keyframe state が矛盾しない

### Phase 6: Regression Gate

目的:
右パネル改良後の退行を追いやすくする。

作業項目:

- transform、opacity、effect parameter の代表ケースを手動確認項目にする
- search / keyframes-only / layer filter 下での keyframe visibility を確認する
- audio / video / text / shape の混在時に marker と clip が読めるか確認する
- multi-select move、copy / paste、jump navigation の回帰観点を固定する

Done when:

- 改良ごとに最低限の regression checklist を回せる
- 右パネルの見え方と入力が別々に壊れても検出しやすい

---

## Implementation Notes

### Guardrails

- 新しい公開 signal / slot は増やさず、既存 action と model API を優先する
- `ArtifactLayerPanelWidget` は左ペイン責務に留め、右パネルの state source にしすぎない
- property path 表示名は transform 系 milestone と矛盾させない
- `Keyframes Only`、search、filter は右パネルの visibility rule として扱い、別の selection source にしない

### First Files

1. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
2. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
3. `Artifact/src/Widgets/Timeline/ArtifactTimelineKeyframeModel.cppm`
4. `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`
5. `Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm`

### Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5
6. Phase 6

---

## Done Criteria

- 右パネル中心で keyframe の選択、移動、追加、削除、ジャンプが迷わず使える
- property row と timeline marker が同じ state を示す
- search / filter / keyframes-only を有効にしても編集対象を見失いにくい
- transform 系とそれ以外の property が同じ editing grammar で扱える

---

## Related Docs

- [MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md)
- [MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md](/x:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_TIMELINE_TRANSFORM_KEYFRAME_EDITING_2026-04-12.md)
- [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md)
- [MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md](/x:/Dev/ArtifactStudio/docs/done/MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md)
