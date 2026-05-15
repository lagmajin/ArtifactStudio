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

## 関連ファイル

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Dock/DockStyleManager.cppm`
- `ArtifactWidgets/src/Dock/Pane.cpp`

