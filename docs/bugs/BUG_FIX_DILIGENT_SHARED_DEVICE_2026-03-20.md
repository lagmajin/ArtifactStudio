# 修正履歴とデバイス初期化設計: Diligent (Vulkan/D3D12) 共有化

## 背景

`Diligent/Vulkan` ではグローバル関数ポインタ使用の制約により、
同一プロセスで複数の `IRenderDevice` / `IDeviceContext` を同時に生成できない。

Composition Viewer と Layer View が別々に Diligent device を生成しており、
後から初期化したビューが失敗する状態になっていた。

---

## 症状

- Composition Viewer が描画されない、または初期化失敗で毎フレーム `skip`。
- `Diligent Engine: ERROR: We use global pointers to Vulkan functions...` が発生。

---

## 根本原因

- 各ビュー（Composition Viewer / Layer View / 3D window）が
  独立に `CreateDeviceAndContexts*` を実行していた。
- Vulkan の制約により、2 回目の device 作成が失敗。
- 失敗後の render loop が継続し、swapchain null などの二次ログが連鎖。

---

## 修正の履歴（要点）

### 1) 失敗時のガード追加

- 初期化失敗時に render loop を開始しないように修正。
- Composition Viewer 側は `renderer->isInitialized()` を確認して早期 return。

対象:
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

### 2) Vulkan 多重 device 生成の回避

- プロセス内で Diligent device/context を 1 つに集約する設計へ移行。
- ビューは共有 device/context を取得し、各自 swapchain を作成する。

対象:
- `Artifact/include/Render/DiligentDeviceManager.ixx`
- `Artifact/src/Render/DiligentDeviceManager.cppm`

### 3) 直接初期化経路の削除

- Layer View が独自に `CreateDeviceAndContexts*` を叩く経路を削除。
- 既存の `ArtifactIRenderer -> DiligentDeviceManager` 経路に統一。

対象:
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

### 4) 3D Model Viewer / RenderWindow の共有化

- `ArtifactDiligentEngineRenderWindow` も共有 device/context を使用。
- 共有参照カウントの解放を destructor で明示。

対象:
- `Artifact/include/Widgets/Render/ArtifactDiligentEngineRenderWindow.ixx`
- `Artifact/src/Widgets/Render/ArtifactDiligentEngineRenderWindow.cpp`

---

## 現在のデバイス初期化の仕組み

### 1) 共有デバイス管理

`DiligentDeviceManager` 内でプロセス共有の `IRenderDevice` / `IDeviceContext` を保持。
`acquireSharedRenderDeviceForCurrentBackend()` で参照カウントを増やして取得し、
`releaseSharedRenderDevice()` で最後の利用者が解放する。

バックエンド選択:
- `ARTIFACT_RENDER_BACKEND=vulkan` の場合は Vulkan を先に試す。
  失敗時は D3D12 にフォールバック。
- `ARTIFACT_RENDER_BACKEND=d3d12` の場合は D3D12 固定。
- `auto` は D3D12 -> Vulkan の順に試す。

実装:
- `Artifact/src/Render/DiligentDeviceManager.cppm`

### 2) ビューの初期化

- Composition Viewer:
  - `ArtifactIRenderer::initialize()` -> `DiligentDeviceManager::initialize()`
  - device/context は共有取得。
  - swapchain は view 毎に作成。

- Layer View:
  - 直接 Diligent 初期化は撤去。
  - `ArtifactIRenderer::initialize()` 経由で共有 device を利用。

- 3D Model Viewer:
  - `ArtifactDiligentEngineRenderWindow::initialize()` で共有 device を取得。
  - swapchain は window 単位。

### 3) Headless / RenderQueue

`initializeHeadless()` は現状、共有ではなく個別 device を生成する。
Vulkan 指定時は D3D12 へフォールバックするため破綻は回避できるが、
将来的に Vulkan headless を同時利用するなら共有化が必要。

対象:
- `Artifact/src/Render/DiligentDeviceManager.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

---

## 補足

- Vulkan の 1 device 制約に合わせるため、UI は共有 device 前提に統一した。
- この構成で `Composition Viewer` と `Layer View` の共存が成立する。

---

## 未解決 / 将来課題

- RenderQueue の GPU headless が Vulkan で並行する場合、共有化が必要。
- Vulkan の device を完全に共有するには、render queue 側の設計整理が必要。

