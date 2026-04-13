# レンダリングバグ調査レポート — 2026-04-11

## 対象バグ

1. コンポジットエディタに表示される **水色の謎の矩形**
2. レイヤーソロビュー（`ArtifactLayerEditorWidgetV2` / DiligentEngine）に **何も表示されない**

---

## 1. 水色の謎の矩形（TransformGizmo の描画領域過剰拡張）

### 現象

- 1920×1080 のソリッドレイヤーを追加するとコンポジション領域の周囲に水色（シアン）の細い矩形枠が現れる。
- 選択していないレイヤーにも常時表示される。

### 根本原因

`Artifact/src/Widgets/Render/TransformGizmo.cppm` line 621 の定数:

```cpp
static constexpr double GIZMO_OFFSET = 15.0;
```

`draw()` (line 640) でこの値を使ってレイヤーのローカル矩形を全方向に拡張している:

```cpp
localRect.adjust(-GIZMO_OFFSET, -GIZMO_OFFSET, GIZMO_OFFSET, GIZMO_OFFSET);
```

結果として、1920×1080 ソリッドレイヤーのギズモ矩形は `(-15, -15) → (1935, 1095)` になり、
コンポジション境界の4辺の外側に水色の線分として視認される。

テーマのアクセントカラーがシアン系（Arctic=`#88C0D0`, Daydream=`#52C0FE`, Maya Like=`#4FA8FF`）
のため、特に目立っていた。

### 修正

`GIZMO_OFFSET` を `0.0` に変更。

```cpp
// before
static constexpr double GIZMO_OFFSET = 15.0;
// after
static constexpr double GIZMO_OFFSET = 0.0;  // handles use offsetPointAwayFromCenter so no border expansion needed
```

ハンドル描画は `offsetPointAwayFromCenter(pos, center, outward)` で中心から outward (≥4.2px) 外側に
押し出されるため、`GIZMO_OFFSET = 0` でも操作ハンドルはレイヤー境界の外側に位置する。
`hitTest()` (line 936) も同定数を使用しているため、描画と操作の一貫性は維持される。

---

## 2. レイヤーソロビューに何も表示されない

### 現象

- `ArtifactLayerEditorWidgetV2` ウィジェットを開いてレイヤーを指定してもキャンバスが真っ黒/空白のまま。
- コンポジットエディタは正常に描画される。

### 根本原因 A — `setViewportSize` が初期化時に呼ばれない

**ファイル**: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

初期化フロー:

```
showEvent()
  └─ initialize(this)          ← renderer_ 作成、シェーダー/PSO 初期化
  └─ initializeSwapChain(this) ← recreateSwapChain のみ (setViewportSize 未呼び出し ← バグ)
  └─ renderer_->fitToViewport()← デフォルト {1920, 1080} を前提に zoom/pan を計算
  └─ startRenderLoop()
```

`ViewportTransformer` のデフォルト値は `viewportSize = {1920, 1080}` であり、
最初の `resizeEvent` が発火するまで実際のウィジェットサイズに更新されない。
これにより `fitToViewport()` が誤ったサイズを基準に zoom/pan を設定し、
描画ジオメトリが画面外にオフセットされる。

### 根本原因 B — D3D12 GPU ビューポートが共有コンテキストで汚染される

**ファイル**: `Artifact/src/Render/PrimitiveRenderer2D.cppm`

`ArtifactIRenderer` / `PrimitiveRenderer2D` は `SharedRenderDeviceState`（静的シングルトン）を通じて
D3D12 デバイスおよび immediateContext を全インスタンスで共有する。

`PrimitiveRenderer2D::clear()` は `SetRenderTargets` を呼ぶが **`SetViewports` を呼ばない**:

```cpp
// before (バグ)
void PrimitiveRenderer2D::clear(const FloatColor& color)
{
    ...
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, ...);
    impl_->pCtx_->ClearRenderTarget(pRTV, clearColor, ...);
}
```

コンポジットエディタ (`ArtifactCompositionRenderController.cppm` line 3237/3355/3433) は
毎フレーム `ctx->SetViewports(...)` を呼び、GPU のビューポートをコンポジション用サイズに設定する。

その後にソロビューのレンダリングが実行されると、GPU ビューポートはコンポジットエディタが最後に設定した
サイズ（例: 1600×900）のままになり、ソロビューの RTV サイズ（例: 400×300）と不一致になる。
NDC 座標変換が正しくても GPU のビューポート変換でジオメトリがオフスクリーンへ変換される。

### 修正

#### Fix A: `initializeSwapChain` に `setViewportSize` を追加

```cpp
void ArtifactLayerEditorWidgetV2::Impl::initializeSwapChain(QWidget* window)
{
    if (!renderer_) return;
    renderer_->recreateSwapChain(window);
    // 実際のウィジェットサイズで ViewportTransformer を初期化
    if (window && window->width() > 0 && window->height() > 0) {
        renderer_->setViewportSize(static_cast<float>(window->width()),
                                   static_cast<float>(window->height()));
    }
}
```

#### Fix B: `PrimitiveRenderer2D::clear()` に `SetViewports` を追加

```cpp
void PrimitiveRenderer2D::clear(const FloatColor& color)
{
    if (!impl_->hasRenderTarget() || !impl_->pCtx_) return;

    // 共有コンテキスト汚染防止: このレンダラーの GPU ビューポートを再設定
    const auto vp = impl_->viewport_.GetViewportCB();
    const float vpW = std::max(vp.screenSize.x, 1.0f);
    const float vpH = std::max(vp.screenSize.y, 1.0f);
    Viewport VP;
    VP.TopLeftX = 0.0f; VP.TopLeftY = 0.0f;
    VP.Width    = vpW;  VP.Height   = vpH;
    VP.MinDepth = 0.0f; VP.MaxDepth = 1.0f;
    impl_->pCtx_->SetViewports(1, &VP, static_cast<Uint32>(vpW), static_cast<Uint32>(vpH));

    float clearColor[] = { color.r(), color.g(), color.b(), color.a() };
    auto* pRTV = impl_->getCurrentRTV();
    impl_->pCtx_->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    impl_->pCtx_->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
```

---

## 修正ファイル一覧

| ファイル | 修正内容 |
|---|---|
| `Artifact/src/Widgets/Render/TransformGizmo.cppm` | `GIZMO_OFFSET` 15.0 → 0.0 |
| `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` | `initializeSwapChain` に `setViewportSize` 追加 |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | `clear()` に `SetViewports` 追加 |

---

## 前セッション修正（ビルド未検証）

| ファイル | 修正内容 |
|---|---|
| `Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx` | `enterEvent(QEnterEvent*)` 宣言追加 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` | `enterEvent` 実装（リサイズ後カーソルがリセットされない問題） |
| `Artifact/src/Render/ShaderManager.cppm` | `createShaders()` を TBB `task_group` で並列化 |
