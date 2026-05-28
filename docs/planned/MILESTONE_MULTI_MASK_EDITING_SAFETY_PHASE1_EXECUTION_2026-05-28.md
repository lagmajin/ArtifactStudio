# Multi Mask Editing Safety - Phase 1 Execution

**Date**: 2026-05-28

**Source**: [`MILESTONE_MULTI_MASK_EDITING_SAFETY_2026-05-28.md`](./MILESTONE_MULTI_MASK_EDITING_SAFETY_2026-05-28.md)

## Phase 1 Goal

複数 mask を持つ layer で、選択と削除の安全性を先に固める。

この段階では、順序変更や見た目の拡張よりも、
`selectedMaskIndex` や `property path` の文脈が壊れないことを優先する。

## Scope

### In

- mask selection stability
- delete / reindex policy
- property path safety
- minimal selection feedback

### Out

- mask order reordering UI
- full multi-select mask editing
- rasterization algorithm changes
- GPU mask pipeline changes

## Current Boundary Note

- この Phase 1 は「消しても壊れない」ことに絞る
- `maskCount() > 1` の存在を前提にしつつ、編集文脈の安定化だけを扱う
- 順序変更は次段に回す
- `MaskPath` の描画やラスタライズは触らない
- 既存の `LayerMask` / `ArtifactAbstractLayer` の保持構造は維持する

## First Files

1. `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
2. `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
3. `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
4. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
5. `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

## First Move

1. `ArtifactLayerPanelWidget.cpp` で mask 選択と削除の入口を確認する
2. `ArtifactAbstractLayer.cppm` で `maskIndex` / `pathIndex` の安全性を確認する
3. `ArtifactCompositionRenderController.cppm` で編集反映と undo/redo の境界を確認する
4. `ArtifactRenderLayerWidgetv2.cppm` は mask の可視編集経路の確認に使う

## Tasks

### 1. Selection Stability

- `selectedMaskIndex` が無効になったときの扱いを決める
- 選択中の mask を削除した場合のフォールバックを固定する
- `currentPropertyPath` と選択状態の不整合を減らす

### 2. Path Editing Safety

- `maskIndex` / `pathIndex` の参照が無効になったときに安全に失敗する
- mask 削除後に別 mask へ誤って飛ばないようにする
- property path の parse/apply 境界を明確にする

### 3. Delete / Reindex Policy

- mask 削除時に後続 index を詰める前提を明文化する
- 削除後に次のフォーカス先をどう決めるかを固定する
- undo / redo で index 文脈が壊れないようにする

## Recommended Order

1. selection stability を固める
2. path editing safety を確認する
3. delete / reindex policy を整理する
4. その後に順序表示や UI affordance を次段へ送る

## Done Criteria

- mask を 2 個以上持つ layer で、削除後の選択が壊れない
- `selectedMaskIndex` が不正な値を保持しない
- property path の参照が別 mask に誤適用されにくい
- 次段の順序表示・UI 改善に進める状態になる

## Related Docs

- [`MILESTONE_MULTI_MASK_EDITING_SAFETY_2026-05-28.md`](./MILESTONE_MULTI_MASK_EDITING_SAFETY_2026-05-28.md)
- [`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_PHASE1_EXECUTION_2026-05-12.md`](./MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_PHASE1_EXECUTION_2026-05-12.md)
- [`docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`](/x:/Dev/ArtifactStudio/docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md)
