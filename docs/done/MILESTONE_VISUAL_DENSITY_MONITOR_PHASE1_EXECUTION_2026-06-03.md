# Visual Density Monitor - Phase 1 Execution

**Date**: 2026-06-03

**Source**: [`MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md`](./MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md)

**Status**: 完了

---

## Phase 1 Goal

`Visual Density Monitor` を、既存の `Overlay.Composition` と `Frame Debug` に載せられる最小診断として定義する。

この段階では、画面全体を最適化するのではなく、まず「詰まりの見える化」を固定する。

---

## Scope

### In

- visual density score
- information density score
- luminance density score
- motion density score
- density heatmap
- warning / next action summary

### Out

- automatic layout rewriting
- full image segmentation
- expensive global pixel scanning
- new global event bus

---

## Priority Order

### 1. Visual Density

最初に出す指標。

理由:

- レイヤー数や bounds の重なりは既存データから取りやすい
- overlay にそのまま載せやすい
- 画面の「詰まり」を人間が直感的に理解しやすい

### 2. Information Density

次に出す指標。

理由:

- warning / label / annotation の重なりは app debugger と相性がよい
- diagnostics cohesion にもつなげやすい

### 3. Motion Density

三番目に出す指標。

理由:

- timeline / animation をまたぐので設計は少し重い
- ただし AE らしさへの効きは大きい

### 4. Luminance Density

四番目に出す指標。

理由:

- 解析の見た目は強いが、測り方に幅がある
- color management の前提が固まるほど精度が上がる

---

## First Surfaces

### Overlay.Composition

- selected region の density heatmap
- crowding warning
- space candidate hint

### FrameDebugViewWidget

- density summary
- warning reason
- next action

### AppDebuggerWidget

- density score の全体要約
- frame debug への導線
- compare state との兼ね合い

---

## Suggested First Files

1. `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
2. `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
3. `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
4. `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
5. `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`

---

## Implementation Notes

- Phase 1 は grid-based sampling で十分
- selected layer 周辺だけを先に見ると負荷が軽い
- score は絶対値よりも `Low / Medium / High` を優先する
- warning は 1 行に絞る

---

## Done Criteria

- density score が 1 画面で読める
- warning が `詰まりすぎ` という主観を補助できる
- overlay と frame debug の言葉が一致する
- 何を空けるべきかの候補が返る
