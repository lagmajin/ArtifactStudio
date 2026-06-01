# Bug Report - Composition Editor Zoom / Fill Misposition (2026-05-30)

## Summary

`Composition Editor` で `Fill` または `100%` を選ぶと、コンポジション画面が期待どおりの大きさ・位置にならない。

現時点の調査では、主因は 1 つではなく、少なくとも次の 3 点が重なっている可能性が高い。

1. `Fill` は実装上 `fit` ではなく `cover` 動作で、コンポジション全体を収める保証がない
2. `100%` は physical pixel ベースの viewport / pan 計算を使っており、高 DPI 環境で見た目がずれやすい
3. 初期 `Fill` と resize debounce のタイミングがずれると、古い viewport 情報に対して zoom / pan が決まりうる

## Symptom

- `Fill` を選んでもコンポジションが全面に収まらない
- `100%` を選ぶと、中央位置や表示サイズが期待と違って見える
- ウィンドウサイズ変更直後や dock / QADS 再表示直後に、表示位置の違和感が出やすい

## Reproduction

1. Composition Editor を開く
2. ツールバーの `Fill` を押す
3. 必要に応じて `100%` を押す
4. 縦横比が異なるコンポジションで、表示位置と余白を確認する

## Investigation Notes

### 1. `Fill` の意味が、一般的な「全体を収める」ではない

`ViewportTransformer::FillCanvasToViewport()` は `std::max(zoomW, zoomH)` を使っている。これは viewport を埋めるための cover 計算であり、片方向は必ず切れる。

- [ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm:140)
- [ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm:152)
- [ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm:155)

一方、全体を収める `fit` 相当は `std::min(zoomW, zoomH)` 側。

- [ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm:118)
- [ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm:127)

このため、ユーザーが `Fill` に「全体表示」を期待している場合、見た目が不適切に感じられる。

### 2. `100%` は physical pixel 基準で pan を作っている

`CompositionRenderController::initialize()` と `setViewportSize()` は、host widget の logical size を devicePixelRatio 倍して `hostWidth_ / hostHeight_` に入れている。

- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:2949)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:2970)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:2974)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:3163)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:3180)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:3182)

`zoom100()` は `hostWidth_` と `lastCanvasWidth_` の差で pan を決めるので、高 DPI 環境では「見た目の 100%」とズレやすい。

- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:4053)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:4057)

### 3. 初期 `Fill` は resize / readiness のタイミングに影響される

`CompositionViewport::scheduleInitialFit()` は `resizePending_` が立っている間は待ち、準備完了後に `controller_->zoomFill()` を呼ぶ。

- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:2533)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:2553)

さらに `CompositionViewport::resizeEvent()` は、初期化前後で `setViewportSize()` と `resizeDebounceTimer_` を使い分けている。

- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:1702)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:1709)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:1716)

このため、dock 再構築や初期表示の直後に `Fill` を押すと、古い viewport サイズを元に pan / zoom が決まる可能性がある。

## UI Confusion Factor

ツールバーでは、変数名が `zoomFitAction_` なのに表示ラベルが `Fill` になっており、接続先は `zoomFill()` になっている。

- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:3650)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:3651)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4147)
- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4149)

この命名は、ユーザーにも実装者にも「Fit と Fill の違い」を曖昧にしやすい。

## Most Likely Root Cause

単独原因ではなく、以下の組み合わせが最も疑わしい。

1. `Fill` が cover 動作であること
2. `100%` が physical pixel / DPR 基準であること
3. 初期 fit と resize debounce のタイミング差

## Not Yet Verified

- `100%` の見た目ズレが、全環境で DPR 起因かどうか
- `Fill` の期待値が、本当に「全体表示」なのか「面を埋める」なのか
- dock / QADS の再配置時に viewport サイズが遅れて更新されるかどうか

## Suggested Next Checks

1. `Fill` と `100%` 実行時の `hostWidth_ / hostHeight_ / lastCanvasWidth_ / lastCanvasHeight_ / zoom / pan / DPR` をログで並べる
2. 高 DPI 環境と 1.0 DPR 環境で挙動を比較する
3. `Fill` の UX 定義を「fit」なのか「cover」なのか先に固定する
4. 初期 `zoomFill()` が resize 直後に古い viewport を参照していないか確認する

## Related Files

- [ArtifactCompositionEditor.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [ArtifactCore/ViewportTransformer.cppm](X:/Dev/ArtifactStudio/ArtifactCore/src/Transform/ViewportTransformer.cppm)
