# Composition Editor Mask / Roto Editing Milestone (2026-03-28)

`Composition Editor` で layer mask / roto を直接編集できるようにするためのマイルストーン。
既存の `LayerMask / MaskPath` 実体と、`ArtifactCompositionRenderController` の mask edit mode を前提に、
「入口」「編集」「同期」「復帰」を一つの作業単位としてまとめる。

## Goal

- composition editor 上で mask path を作成・選択・移動・調整できるようにする
- `RotoMaskEditor` を mask editing のコアとして扱う
- gizmo / selection / playhead と衝突しない入力導線を作る
- mask 編集結果が layer render / inspector / undo に自然に反映されるようにする

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`
- `ArtifactCore/src/UI/RotoMaskEditor.cppm`
- `ArtifactCore/include/UI/RotoMaskEditor.ixx`
- `Artifact/src/Mask`
- `ArtifactCore/src/Mask`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

## Non-Goals

- 完全なペイントツール化
- 3D レイヤーへの直接適用
- マスクベースのノード合成システム新設
- `LayerMask` / `MaskPath` の再設計

## Milestones

### M-CE-MASK-1 Entry Bridge And Mode Routing

- `Mask` tool を composition editor の主要入力モードとして固定する
- toolbar / pie menu / shortcut の入口を統一する
- `EditMode::Mask` の状態遷移を整理する
- selection / gizmo / pan / playhead と mask mode の優先順位を決める

### M-CE-MASK-2 Path Creation And Vertex Editing

- 新規 mask path を viewport 上で作れるようにする
- vertex 追加 / 移動 / 削除の入力を揃える
- path close / open / feather / expansion / invert を編集できるようにする
- hovered / dragged vertex の可視化を安定させる

### M-CE-MASK-2B Bezier Handle Editing

- mask path の各 vertex に対して in / out tangent を編集できるようにする
- anchor point と bezier handle を分離して表示・ドラッグできるようにする
- rotopath / maskpath の保存・復元で tangent 情報を落とさない
- ベジェ曲線の編集結果が rasterize / preview / render queue に一致するようにする

### M-CE-MASK-3 Undo / Redo And Selection Sync

- mask 編集を undoable command として扱う
- current layer / selected layer / active composition の状態を同期する
- layer selection の変更時に mask edit state を安全に解除する
- 他の編集操作と同時に壊れない event ordering を固定する

### M-CE-MASK-4 Inspector And Diagnostics

- mask count / path count / enabled state を inspector に出す
- edit mode の状態を debug log で追えるようにする
- 失敗時の UI feedback を用意する
- layer view / composition editor から見た mask の入口を docs で固定する

## Recommended Order

1. `M-CE-MASK-1 Entry Bridge And Mode Routing`
2. `M-CE-MASK-2 Path Creation And Vertex Editing`
3. `M-CE-MASK-2B Bezier Handle Editing`
4. `M-CE-MASK-3 Undo / Redo And Selection Sync`
5. `M-CE-MASK-4 Inspector And Diagnostics`

## Current Status

- `ArtifactCompositionRenderController` には mask editing state と vertex dragging の入口がすでにある
- `Composition Editor` 側には `Mask` tool 入口が既に入り始めている
- ただし、入力の優先順位と undo / inspector 連携はまだ作業途中
- そのため、今は「入口はあるが編集体験の統一は未完了」という段階

## Validation Checklist

- Mask tool を選んだとき、gizmo と入力競合しにくい
- 新規 mask path を作成できる
- 既存 path の vertex を移動できる
- mask 編集の結果が layer の render に反映される
- undo / redo で mask 編集が戻せる
- inspector で mask 状態を確認できる
