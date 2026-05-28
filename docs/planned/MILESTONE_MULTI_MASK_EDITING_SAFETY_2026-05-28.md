# Multi Mask Editing Safety

`maskCount() > 1` の状態を、作れるだけでなく安全に編集できるようにするためのマイルストーン。

この milestone は、マルチマスクを新機能として大きく広げるものではない。
むしろ、すでにある複数 mask の保持・合成・編集経路を前提にして、
「削除」「選択」「順序」「property path」の食い違いを減らすことを目的とする。

## Goal

- 複数 mask を持つ layer で、どの mask を触っているかが分かる
- mask の削除後も、選択と property 文脈が壊れにくい
- mask の合成順が意味を持つことを UI と文書で明示する
- 途中の mask を消しても、残りの mask がなるべく予測可能に扱える
- 2 個以上の mask を持つ layer でも、編集が怖くない

## Scope

- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
- `Artifact/src/Mask/LayerMask.cppm`
- `Artifact/include/Mask/LayerMask.ixx`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

## Non-Goals

- mask rasterization algorithm の再設計
- pen / roto の深い path authoring 全体の再構築
- GPU mask pipeline の全面変更
- layer selection architecture の再設計
- 新しい global signal/slot architecture の追加

## Background

現状の実装は、複数 mask を layer に持てる。
`LayerMask` は複数 `MaskPath` を合成でき、`ArtifactAbstractLayer` は複数 mask を保持できる。

一方で、編集 UI はまだ単一 mask の感覚が強い。
mask の選択は index 1 本で、property path も index ベースなので、
途中の mask を削除したときに後ろの index が詰まり、文脈がずれやすい。

この milestone は、その弱点を先に塞ぐ。

## Current Observed Boundary

重要な前提:

1. `LayerMask` は 1 つの mask の中で複数 `MaskPath` を合成する
2. `ArtifactAbstractLayer` は 1 layer に複数 `LayerMask` を持つ
3. `ArtifactCompositionRenderController` は `maskCount()` に応じて複数 mask を描画・適用する
4. `ArtifactLayerPanelWidget` は mask の選択・削除・property 編集の入口を持つ

つまり、土台はあるが、編集安全性はまだ完成していない。

## Proposed Model

- `MaskSlot`
  - layer 内の 1 つの mask エントリ
- `MaskSelectionAnchor`
  - 現在選択している mask の index / layer
- `MaskOrder`
  - 合成順と表示順の一致を表す
- `MaskAddress`
  - `layerId + maskIndex + pathIndex` の編集文脈

## Work Packages

### 1. Selection Stability

対象:

- `ArtifactLayerPanelWidget`
- `ArtifactAbstractLayer`

内容:

- mask 選択の文脈が index ずれで壊れないようにする
- 削除後に `selectedMaskIndex` が無効なら安全にクリアする
- `currentPropertyPath` と mask 選択の対応を崩しにくくする

完了条件:

- 途中の mask を消しても、選択状態が変な index を指さない
- 画面上の選択と内部選択が食い違いにくい

### 2. Order Visibility

対象:

- `ArtifactLayerPanelWidget`
- `ArtifactCompositionRenderController`

内容:

- 複数 mask がある layer で、順序が意味を持つことを示す
- `Mask 1`, `Mask 2` のような index を、単なる番号ではなく順序として見せる
- 必要なら mask count badge や簡単な順序ラベルを追加する

完了条件:

- どの mask が先に合成されるかが分かる
- 「順番を変えると見た目が変わる」ことが UI から読み取れる

### 3. Path Editing Safety

対象:

- `ArtifactAbstractLayer`
- `ArtifactCompositionRenderController`

内容:

- `maskIndex` / `pathIndex` を使う経路で、対象が無効なときの扱いを揃える
- mask 削除後に残った path 編集が別 mask に飛ばないようにする
- property path の parse / apply の境界を明示する

完了条件:

- 削除後も、別 mask の path を誤って触りにくい
- maskAddress の失敗が静かに別対象へ化けない

### 4. Delete / Reindex Policy

対象:

- `ArtifactLayerPanelWidget`
- `ArtifactAbstractLayer`
- `ArtifactCompositionRenderController`

内容:

- mask 削除時に後続 index を詰めるか、その影響をどう扱うかを明文化する
- 選択中の mask を削除したら、次にどの mask をフォーカスするか決める
- undo / redo で index と選択が矛盾しないようにする

完了条件:

- mask を消しても、次に触る対象が予測できる
- undo/redo で選択が壊れにくい

### 5. Minimal UI Affordance

対象:

- `ArtifactLayerPanelWidget`
- `ArtifactCompositionRenderOverlay`

内容:

- 複数 mask を持つ layer で、今どの mask を見ているかを軽く表示する
- 過剰な UI 追加は避けつつ、選択中の mask とその他を見分けやすくする

完了条件:

- 2 個以上の mask を持つ layer で迷いにくい

## Recommended Order

1. Selection Stability
2. Path Editing Safety
3. Delete / Reindex Policy
4. Order Visibility
5. Minimal UI Affordance

## First Files

1. `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`
2. `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
3. `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
4. `Artifact/src/Mask/LayerMask.cppm`
5. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
6. `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

## Notes

この milestone は、`MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_PHASE1_EXECUTION_2026-05-12.md`
の延長線にあるが、主題は shape / pen の描画ではなく、複数 mask の管理安全性にある。

また、`docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md`
のような合成契約とも相性が良い。

マルチマスクの本質的な難しさは、作ることよりも「消した後に壊れないこと」なので、
この milestone はそこを最優先に置く。
