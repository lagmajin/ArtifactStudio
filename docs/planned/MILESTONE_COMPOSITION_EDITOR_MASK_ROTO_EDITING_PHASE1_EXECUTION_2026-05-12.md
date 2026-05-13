# Composition Editor Mask / Roto Editing - Phase 1 Execution

**Date**: 2026-05-12

**Source**: [`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md)

**Status**: Active slice in the May 12 triad
**Order**: 3 of 3

---

## Phase 1 Goal

`Mask` tool の入口と mode routing を先に固定する。

この段階では path editing の完成度よりも、入力優先順位と edit mode の切り替えを安定させる。

---

## Scope

### In

- entry bridge
- mode routing
- selection / gizmo / playhead の優先順位
- current layer と active composition の整合

### Out

- full vertex editing
- bezier handle 完全対応
- inspector の完全整備

---

## Current Boundary Note

- この Phase 1 は `Mask` tool の入口と mode routing に絞る
- path editing の本体は次段で扱う
- `selection / gizmo / playhead` と競合しない入力優先順位を先に決める
- `FrameDebugSnapshot` と report text の観測は編集 UI の外側で維持する

---

## First Files

1. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
2. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
3. `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
4. `ArtifactCore/src/UI/RotoMaskEditor.cppm`
5. `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`

---

## First Move

1. `ArtifactCompositionEditor.cppm` で entry bridge を確認する
2. `ArtifactCompositionRenderController.cppm` で mode routing を整理する
3. `ArtifactRenderLayerWidgetv2.cppm` で selection / gizmo / playhead の優先順位を詰める
4. `RotoMaskEditor.cppm` は state sync の確認に使う

---

## Tasks

### 1. Entry Bridge

- toolbar / shortcut / menu の入口を統一する
- `Mask` tool の起点を1つに寄せる

### 2. Mode Routing

- `EditMode::Mask` の遷移を整理する
- gizmo / pan / playhead と衝突しない優先順位を決める

### 3. State Sync

- current layer / selected layer / composition の状態を壊さない
- mode 切替時に古い editing state を残さない

---

## Recommended Order

1. entry bridge の入口をまとめる
2. `EditMode::Mask` の routing を固める
3. selection / gizmo / playhead の優先順位を確認する
4. state sync の後始末を整える
5. 次の path editing に渡す

---

## Done Criteria

- `Mask` tool の入口が迷わない
- edit mode が安定して切り替わる
- 次の path editing に入る準備ができる
