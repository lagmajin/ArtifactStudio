# Milestone: QADS Floating Surface Stabilization

**Date:** 2026-05-16  
**Status:** Planned

## Goal

QADS floating mode で surface が

1. 白画面になる
2. 初期化されない
3. resize / activation 後にだけ復帰する

といった不安定さを減らし、dock / floating で同じ readiness contract で開けるようにする。

## Primary Repro

- `Composition Editor` を floating dock で開く
- 白いまま何も表示されないことがある
- 一度 dock すると復帰することがある

代表調査:

- `docs/bugs/BUG_QADS_FLOATING_COMPOSITION_EDITOR_SHOWEVENT_2026-05-15.md`

## Scope

- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- 必要なら vendored QADS floating container path

## Non-Goal

- QADS 全体の完全再設計
- 全 dock widget の個別 workaround 積み増し
- permanent debug overlay の追加

## Core Reading

この問題は次の 2 層を分けて扱う。

### Layer A: QADS Floating Lifecycle

- floating container は別 top-level window
- dock 時と `show/activate/resize` の順序が異なる
- repaint/refresh hook はあっても child renderer readiness を保証しない

### Layer B: Surface Readiness Contract

- `CompositionViewport::showEvent()` 依存が強い
- `isVisible()` / minimized guard によって初期化が先送りされる
- retry の再契機が弱いと白画面のまま残る

## Milestones

### M-QADS-1 Readiness Vocabulary

まず surface の状態を明確化する。

最低限:

1. host visible
2. host native handle ready
3. renderer initialized
4. swapchain ready
5. preferred composition synced

`showEvent happened` だけでは readiness を表現しない。

### M-QADS-2 Explicit Ensure Path

`CompositionViewport` 側に、`showEvent` 以外からも呼べる ready 化入口を作る。

例:

1. `ensureViewportReady()`
2. `ensureRendererInitialized()`
3. `ensureSwapChainReady()`
4. `ensurePreferredCompositionSynced()`

これにより floating container 側は `showEvent` 再発火に依存せず、ready 化を再依頼できる。

### M-QADS-3 Floating Lifecycle Hooks

`ArtifactMainWindow` / floating container から、少なくとも次のタイミングで ready 再評価をかける。

1. floating widget created
2. dock visibility changed
3. first activation
4. first stable resize after floating

ただし repaint hook と renderer initialization hook は責務を分ける。

### M-QADS-4 Cross-Surface Generalization

Composition Editor だけでなく、将来の render-heavy surface でも同じ契約を使えるようにする。

候補:

1. composition surface
2. layer view
3. debugger frame surfaces

## Recommended Order

1. `M-QADS-1 Readiness Vocabulary`
2. `M-QADS-2 Explicit Ensure Path`
3. `M-QADS-3 Floating Lifecycle Hooks`
4. `M-QADS-4 Cross-Surface Generalization`

## Guardrails

1. repaint workaround だけで白画面を説明し切らない
2. `showEvent` を唯一の renderer 初期化契機にしない
3. QADS floating polish と render readiness を同じ helper に押し込まない
4. debug overlay を増やして見えない問題を隠さない

## Done Criteria

- floating で Composition Editor を開いても白画面のまま止まりにくい
- dock / floating のどちらでも renderer readiness の説明が同じ
- retry が `showEvent` 任せではなく、明示的 ensure path で再評価できる
- log 上で `どこまで準備できて止まったか` が読める
