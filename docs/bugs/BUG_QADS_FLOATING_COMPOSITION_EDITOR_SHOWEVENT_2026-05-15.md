# QADS floating mode で Composition Editor が白画面になる調査

## 現象

- `Composition Editor` を floating dock で開くと、白いまま何も表示されないことがある。
- 同じウィジェットを一度 dock すると表示が復帰する。
- リサイズ不具合は別途解消済みだが、lazy 初期化の floating だけで再現しやすい。

## 調査対象の経路

### 1. AppMain の dock 生成

`Artifact/src/AppMain.cppm` では `CompositionEditor` を同期生成し、`mw->show()` の時点で native HWND を持たせる意図がある。

```cpp
// Create the composition editor synchronously so its native HWND exists when
// mw->show() fires. Deferring this inside singleShot(0) caused the widget to
// miss its showEvent and never initialize the Diligent renderer.
```

関連箇所:
- `Artifact/src/AppMain.cppm`
- `ArtifactMainWindow::addDockedWidgetFloating(...)`

### 2. QADS floating container の補正

`ArtifactMainWindow.cppm` では floating container に対して見た目補正と再描画フックを入れている。

主な関数:
- `refreshFloatingWidgetTree(QWidget *)`
- `findFloatingDockContainer(QWidget *)`
- `refreshDockWidgetSurface(ads::CDockWidget *)`
- `scheduleFloatingRefresh(ads::CFloatingDockContainer *)`
- `prepareFloatingDockContainer(ads::CFloatingDockContainer *, QObject *)`
- `wireDockWidgetSignals(ads::CDockWidget *, QObject *)`

観測した挙動:
- `topLevelChanged`
- `visibilityChanged`
- `Show / Hide / Resize / ActivationChange / WindowStateChange`

ただしこれらは主に floating container の repaint / style 再適用であり、`CompositionViewport` の初期化を直接保証しない。

### 3. Composition Editor 側の初期化

`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の `CompositionViewport::showEvent()` は、遅延初期化を行う。

現在の流れ:

1. `showEvent()` 受信
2. 16ms 後に `controller_->initialize(this)` を試行
3. `isVisible()` や `window()->isMinimized()` が false なら再試行へ
4. `controller_->recreateSwapChain(this)`
5. `controller_->setViewportSize(...)`
6. さらに 250ms 後に `syncPreferredComposition()` を試行

関連箇所:
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `CompositionViewport::scheduleViewportInitializationRetry()`
- `CompositionViewport::syncPreferredComposition()`

## 重点観測

- `showEvent()` は来ていても、`isVisible()` 判定で初期化が先送りされる可能性がある。
- floating dock 側の refresh は `showEvent()` の再発火を保証しない。
- dock に戻すと表示が復帰するのは、そこで再度 visibility / activation 系イベントが入り、初期化条件が満たされるためと考えられる。

## 主要仮説

### 仮説 1: floating ライフサイクル中に `CompositionViewport` が初期化条件を満たさない

- QADS の floating container は別 top-level window として扱われる。
- そのため `showEvent` / `visibilityChanged` / `ActivationChange` の順序が dock 時と異なる。
- 初期化が retry に回ったあと、次の再試行契機が来ないと白画面のまま止まる。

### 仮説 2: `ArtifactMainWindow` の floating refresh は見た目補正に留まり、renderer 初期化を再トリガーしていない

- `refreshFloatingWidgetTree(...)` と `scheduleFloatingRefresh(...)` は表示更新には効く。
- ただし `controller_->initialize(this)` を直接呼ぶ経路ではない。
- そのため、QADS 側で widget は visible でも renderer が未初期化のまま残る可能性がある。

### 仮説 3: `CompositionEditor` の lazy 初期化と floating 生成タイミングの競合

- lazy dock の作成は `ArtifactMainWindow::createLazyDockWidgetNow(...)` で行われる。
- `widget->show()` と `dock->toggleView(true)` の順序、および QADS の container 生成タイミング次第で、`CompositionViewport` の初回 `showEvent` が弱くなる可能性がある。

## 補助観測

- `DockStyleManager.cppm` でも floating dock container は `CFloatingDockContainer` として別 top-level window 扱い。
- `ArtifactWidgets/src/Dock/Pane.cpp` は素の `CDockWidget` ラッパーで、直接の原因候補は薄い。
- `ArtifactCompositionRenderController::initialize(QWidget *)` は host widget 前提で renderer を立ち上げるため、host 側の visible/handle 条件が重要。

## 現時点の判断

- QADS を「壊した」より、QADS の floating ライフサイクルが既存の遅延初期化と噛み合っていない可能性が高い。
- dock すると直るのは、再度 show/activation 系イベントが入り、renderer 初期化の条件が整うからと考えられる。

## 次の確認候補

1. `CompositionViewport::showEvent()` の直前で `isVisible()` / `window()->isMinimized()` / `winId()` をログ出力する。
2. `prepareFloatingDockContainer(...)` の直後に、対象 dock 内の `CompositionEditor` が本当に `showEvent()` を受けているか確認する。
3. floating 時だけ `syncPreferredComposition()` を明示的に再実行し、初期化抜けかどうかを切り分ける。

## Long-Term Reading

この不具合は「QADS の repaint バグ」だけで閉じるより、次の 2 層に分けて扱う方がよい。

1. `CFloatingDockContainer` の floating lifecycle
2. `CompositionViewport` の lazy initialization / retry contract

特に `dock すると直る` という性質は、pure repaint failure よりも
`renderer 初期化契機が floating で弱い`
ことを示唆している。

## Recommended Fix Order

### 1. showEvent 依存を弱める

`CompositionViewport::showEvent()` を唯一の起点にしない。

少なくとも次のいずれかで再評価できるようにする。

1. floating container activation
2. dock visibility change
3. first non-zero resize after floating
4. explicit `ensureViewportReady()` call from shell/main-window side

### 2. renderer 初期化と composition sync を分ける

現状は:

1. initialize
2. recreateSwapChain
3. setViewportSize
4. delayed `syncPreferredComposition()`

が `showEvent` 起点でゆるくつながっている。

長期的には:

1. `ensureRendererInitialized(host)`
2. `ensureSwapChainReady(host, size)`
3. `ensurePreferredCompositionSynced()`

の 3 段階へ分離し、floating 側から必要段だけ再実行できる形が望ましい。

### 3. QADS floating refresh に「見た目補正」と「viewport readiness」を混ぜない

`prepareFloatingDockContainer(...)` と `scheduleFloatingRefresh(...)` は、
見た目補正・再描画補助としては有効でも、renderer 初期化保証の責務までは持っていない。

今後も次を混ぜない方がよい。

1. floating window polish
2. layout / repaint refresh
3. Composition viewport readiness

### 4. white screen を専用診断語彙で観測する

`FrameDebugSnapshot` とは別に、少なくともログ上で次を追えるようにしたい。

1. viewport host visible
2. viewport winId acquired
3. controller initialized
4. swapchain created
5. preferred composition synced

これで `白画面` が

- no show event
- visible guard retry loop
- swapchain missing
- composition sync missing

のどれかを早く切り分けられる。

## Practical Direction

短期 workaround を積み増すより、`CompositionViewport` 側に
`floating-safe readiness contract`
を導入するのが本筋。

つまり、

1. `showEvent` が来たら試す
2. しかし `showEvent` が弱くても、`visibility/resize/activation` で再度 ready 化できる
3. `MainWindow` 側は floating container を見つけたら、その配下の composition surface に対して readiness 再評価を依頼できる

という構造に寄せる。

## 2026-05-16 Fix Applied

`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` の
`CompositionViewport::ensureViewportReady()` を、単なる
`renderer->hasSwapChain()` 判定ではなく、次の host surface fingerprint を
追跡する形へ更新した。

1. `winId()`
2. physical size
3. device pixel ratio

これにより、QADS floating 化で top-level native surface が差し替わった場合も、
既存 swapchain が残っているだけで正常扱いせず、明示的に
`CompositionRenderController::recreateSwapChain(this)` を再実行する。

また `CompositionViewport::event()` で `QEvent::WinIdChange` と
`QEvent::PlatformSurface` を readiness 再評価のトリガーに追加した。
`Show` / `ActivationChange` / `WindowStateChange` だけに依存しないため、
floating container の生成順序が変わっても復帰契機を失いにくい。

残る確認観点:

1. floating dock を別モニタへ移動した時の DPR 変更
2. floating -> dock -> floating の連続切り替え
3. minimized 状態から復帰した直後の first frame

## Related Planning

- [MILESTONE_APP_SURFACE_COHESION_2026-05-13.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_APP_SURFACE_COHESION_2026-05-13.md)
- [MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_QADS_FLOATING_SURFACE_STABILIZATION_2026-05-16.md)

## 関連ファイル

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Dock/DockStyleManager.cppm`
- `ArtifactWidgets/src/Dock/Pane.cpp`
