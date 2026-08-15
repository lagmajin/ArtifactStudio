# Cross-Industry Inspection Tools - Phase 1 Execution

**Date**: 2026-06-02

**最終更新:** 2026-08-15

**Source**: [`MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md`](./MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md)

## 現行コード監査 (2026-08-15)

`FrameDebugViewWidget`、`FrameStateDiffWidget`、`AppDebuggerWidget` の接続、および `ArtifactContentsViewer` の比較表示（wipe / split / difference）は実装済みです。一方、Effect Hitbox の専用データ契約・選択レイヤー overlay、alpha-only / thresholded leakage の専用 compare mode、現在フレームと前フレームを結ぶ本マイルストーン固有の導線は確認できませんでした。

したがって Phase 1 は **比較・診断の基盤のみ部分実装**で、Hitbox View と Leakage View の Done Criteria は未達です。bounds の種類別取得、overlay への描画接続、差分モードの切り替え、実フレームでの警告表示、実行時のパフォーマンスは未検証です。

---

## Phase 1 Goal

`Effect Hitbox View` と `Difference / Leakage View` を、既存の `Overlay.Composition` と `Frame Debug` に無理なく差し込める形へ整理する。

この段階では新しい巨大 surface を増やさず、まずは **見える化** と **比較導線** を先に作る。

---

## Scope

### In

- effect hitbox overlay
- alpha / bounds / diff の可視化
- frame debug から読める compare surface
- overlay / debug で同じ語彙を使うこと

### Out

- full version history browser
- full pixel history slicer
- new global event bus
- heavy render backend rewrite

---

## Current Boundary Note

- `Overlay.Composition` は viewport annotation と guide の責務を守る
- `FrameDebugViewWidget` は原因追跡用の compare / inspect 面として使う
- `ArtifactContentsViewer` は閲覧 / compare の窓口として使えるが、composition 編集責務は持たせない
- permanent overlay で不具合を隠さず、理由説明は `Frame Debug` へ落とす

---

## Phase 1 Targets

### 1. Effect Hitbox View

最初に見る対象:

- layer visible bounds
- alpha bounds
- mask bounds
- matte source bounds
- blur / glow の expanded bounds

表示ルール:

- visible: cyan
- mask: green
- matte: amber
- blur / glow reach: magenta
- alpha leak warning: red

最小導線:

- toggle on/off
- selected layer only
- current frame only

### 2. Difference / Leakage View

最初に見る対象:

- current vs previous frame absolute diff
- alpha-only diff
- thresholded diff
- false-color diff heatmap

最小導線:

- compare mode switch
- current / previous frame A/B
- alpha leak emphasize

---

## First Files

1. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
2. `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
3. `Artifact/src/Widgets/Diagnostics/FrameStateDiffWidget.cppm`
4. `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
5. `Artifact/include/Render/ArtifactRenderROI.ixx`

---

## Recommended Order

1. layer / effect bounds の read path を確認する
2. `Overlay.Composition` に selected-layer hitbox を出す
3. `FrameStateDiffWidget` に alpha-only diff を追加する
4. `FrameDebugViewWidget` から diff mode を切り替えられるようにする

---

## Hitbox Data Contract

```text
EffectHitboxRecord
├─ layerId
├─ category (visible/mask/matte/blur/glow/alpha)
├─ rect
├─ color
├─ enabled
└─ note
```

Phase 1 では矩形ベースでよい。polygon や per-pixel mask は後回し。

---

## Difference Data Contract

```text
FrameDifferenceRecord
├─ mode (absDiff/alphaOnly/threshold/heatmap)
├─ sourceA
├─ sourceB
├─ changedPixelRatio
├─ maxDelta
└─ warning
```

Phase 1 では current frame と previous frame のみを前提にする。

---

## Done Criteria

- selected layer の hitbox を overlay で読める
- alpha leak と diff を frame debug で切り替えられる
- overlay と frame debug の語彙がぶれない
- 「どこまで効いているか」「どこが変わったか」が短時間で読める

---

## Next Step

Phase 2 では `Motion Ghost / Version Ghost` を追加し、timeline 側の比較へ広げる。
