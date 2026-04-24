# Timeline Flat Keyframe View Execution Memo

> 2026-04-23 作成

`ArtifactTimelineWidget` の keyframe 表示を、After Effects の `U` キーに近い思想で再編するための実装メモです。

この memo は、`MILESTONE_TIMELINE_FLAT_KEYFRAME_VIEW_2026-04-03.md` の内容を、実際にどのファイルから手を付けるかまで落としたものです。

## 目的

- keyframe が存在する property だけを先に見せる
- group 階層を編集の邪魔にならない形へ吸収する
- `All Properties` / `Keyframes Only` の切り替えを明確にする
- timeline の編集密度を下げて、見通しを上げる

## 先に触るファイル

1. `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
2. `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
3. `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp`
4. `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`
5. `ArtifactCore/src/Property/*`
6. 必要なら `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

## Phase 1: Flat Filter

### 触る場所

- `ArtifactLayerPanelWidget.cpp`
- `ArtifactTimelineWidget.cpp`
- `ArtifactTimelineTrackPainterView.cpp`
- `ArtifactCore/src/Property/*`

### やること

- property group を走査して、animatable かつ keyframe を持つものだけ残す
- lane 表示は `displayLabel` 優先、なければ humanized name
- `Transform` / `Physics` のような group 名は補助表示に回す
- selected layer の keyframe が先に見えるようにする

### 完了条件

- 動いていない property が初期表示で邪魔しない
- keyframe のある項目が見つけやすい

## Phase 2: Toggle Surface

### 触る場所

- `ArtifactTimelineWidget.cpp`
- `ArtifactLayerPanelWidget.cpp`

### やること

- `[All Properties]` / `[Keyframes Only]` の切り替えを UI に置く
- `U` キーで `Keyframes Only` をトグルする
- mode の状態を timeline header に出す
- 表示モードの切り替えを header / toolbar / menu のどこかに集約する

### 完了条件

- 1 操作で表示モードを切り替えられる
- いまどちらの状態か迷わない

## Phase 3: Expand / Collapse Behavior

### 触る場所

- `ArtifactLayerPanelWidget.cpp`
- `ArtifactTimelineWidget.cpp`

### やること

- layer 単位の展開 / 折りたたみを整理する
- selected layer を優先して自動展開する
- keyframe のある property だけを見せる簡易モードを維持する

### 完了条件

- 全表示でも UI が破綻しない
- keyframe only へすぐ戻れる

## Phase 4: Property Coverage

### 触る場所

- `ArtifactCore/src/Property/*`
- `ArtifactPropertyWidget.cppm`
- `ArtifactLayerPanelWidget.cpp`

### やること

- 表示対象を主要 property に広げる
- `transform.position.x`
- `transform.position.y`
- `transform.scale.x`
- `transform.scale.y`
- `transform.rotation`
- `layer.opacity`
- `effect parameter`

### 完了条件

- 表示が group 単位ではなく、実際の編集対象単位に寄る
- 何を動かしているかが読みやすい

## 実装順のおすすめ

1. `ArtifactCore/src/Property/*` の keyframe / animatable 判定を確認する
2. `ArtifactLayerPanelWidget.cpp` で表示対象を絞る
3. `ArtifactTimelineWidget.cpp` で toggle surface を出す
4. `ArtifactTimelineTrackPainterView.cpp` で見え方と選択の整合を詰める
5. `ArtifactPropertyWidget.cppm` で property surface と整える

## 注意点

- `property group` をそのままフラット化しすぎるとラベルが長くなりすぎる
- `All Properties` と `Keyframes Only` の責務を混ぜない
- keyframe なし property を隠しすぎると、初見で何が編集できるか分かりにくくなる
- 新しい signal / slot は増やさない

## ひとことメモ

- この milestone は「表示を減らす」ためのものではなく、「今触るべきものを先に見せる」ためのもの
- Phase 1 と Phase 2 が通るだけでも、タイムラインの読みやすさはかなり変わる
