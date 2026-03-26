# Composition Editor Rubber Band Multi-Selection Milestone

`Composition Editor` 上での rubber band 選択を使って、複数レイヤーを一括選択できるようにするためのマイルストーン。

## Goal

- viewport 上で矩形ドラッグして複数レイヤーを選択できるようにする
- current composition の selection と `layerSelectionManager` を同期する
- Shift / Ctrl を含む複数選択の操作感を既存の timeline / project selection と揃える
- selection box の見た目と hit test を editor 内で破綻させない

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Layer/ArtifactLayerSelectionManager.cppm`
- `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`

## Non-Goals

- timeline 左ペインの drag-selection を全面再設計すること
- project view の rubber band selection をここで統一すること
- selection dependency graph や grouping UI を完成させること

## Background

Composition Editor 側には `selection` と `hit test` の前提がすでにあり、layer selection manager も存在する。
ただし、矩形ドラッグで複数レイヤーをまとめて選ぶ導線はまだ明示的に整理されていない。

この milestone は、`click` による単体選択の延長として rubber band を追加し、editor 内の複数選択を自然にすることを目的とする。

## Phases

### Phase 1: Rubber Band Gesture

- viewport 上で drag start / drag move / drag release を区別する
- selection box の矩形を viewport 座標で描画する
- gizmo / pan / playhead 操作と競合しない優先順位を決める

### Phase 2: Multi Hit Test

- 現在の composition の全 layer に対して hit test を行う
- `localBounds()` と `transformedBoundingBox()` を基準に矩形交差を判定する
- visible / hidden / locked の扱いを決める

### Phase 3: Selection Sync

- 既存の `layerSelectionManager` に複数選択を流し込む
- Shift で追加、Ctrl でトグル、素のドラッグで置換を整理する
- timeline / inspector / property panel と current selection を揃える

### Phase 4: UX Feedback

- 選択中レイヤーのハイライトを composition editor に反映する
- selection box の色、透明度、キャンセル操作を整理する
- 選択数や現在の mode を debug 表示できるようにする

## Recommended Order

1. `Phase 1`
2. `Phase 2`
3. `Phase 3`
4. `Phase 4`

## Current Status

- 2026-03-26 時点で、composition editor は単体 selection と hit test の基盤を持つ
- ただし rubber band による複数選択は専用の導線としてまだ明示されていない
- そのため、まず gesture と selection sync の責務を分けるのが安全

