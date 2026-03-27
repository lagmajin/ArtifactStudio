# 3D ギズモ非表示: 根本原因と修正 (2026-03-27)

## 症状

コンポジションエディタでレイヤーを選択しても 3D ギズモ (Move/Rotate/Scale 軸) が描画されない。
2D `TransformGizmo`（バウンディングボックス、ハンドル）は正常に表示される。

---

## 関連する既存調査

| ドキュメント | 内容 |
|---|---|
| `GIZMO3D_NOT_VISIBLE_2026-03-26.md` | 行列が Identity のまま 3D renderer に届かない問題を指摘 |
| `GIZMO3D_COUNTERMEASURES_2026-03-26.md` | `setViewMatrix`/`setProjectionMatrix` の 3D 転送、`setGizmoCameraMatrices` 追加等の対策記録 |
| `BUG_INVESTIGATION_GIZMO_VISIBILITY_2026-03-24.md` | gizmo 描画経路の二重化・localBounds 契約等の構造的問題 |

既存対策で行列転送は修正されたが、**ギズモはまだ表示されない**。
本ドキュメントでは残存する 2 つの根本原因を特定し、修正を記録する。

---

## 根本原因 1 (致命的): PrimitiveRenderer3D にレンダーターゲットがない

### 問題

`PrimitiveRenderer3D::drawGizmoLineGeometry()` は冒頭で以下のガード条件を持つ:

```cpp
// PrimitiveRenderer3D.cppm:472-477
if (!hasRenderTarget() || !ctx_ || !gizmo3DPsoAndSrb_.pPSO || !gizmo3DPsoAndSrb_.pSRB ||
    !gizmoLineVertexBuffer_ || !gizmoLineConstantBuffer_ || vertexCount < 2) {
    return;  // ← ここで常に return していた
}
```

`hasRenderTarget()` は以下で定義される:

```cpp
// PrimitiveRenderer3D.cppm:199-205
ITextureView* currentRTV() const {
    if (overrideRTV_) return overrideRTV_;
    return swapChain_ ? swapChain_->GetCurrentBackBufferRTV() : nullptr;
}
bool hasRenderTarget() const { return currentRTV() != nullptr; }
```

ギズモ描画タイミングでは:
- `overrideRTV_` = `nullptr` (オフスクリーンパス終了後に `setOverrideRTV(nullptr)` 済み)
- `swapChain_` = `nullptr` (**初期化時に渡されていない**)

→ `hasRenderTarget()` は常に `false` → **全 3D ギズモ描画が無条件スキップ**

### 原因箇所

`ArtifactIRenderer::Impl::initialize()` での初期化:

```cpp
// ArtifactIRenderer.cppm:260-267
primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                              deviceManager_.swapChain());  // ← 2D は swapChain あり
// ...
primitiveRenderer3D_.setContext(deviceManager_.immediateContext());
                             // ↑ swapChain が渡されていない！
```

同様のパターンが全 `setContext` 呼び出し箇所（`ensureInitialized`、`recreateSwapChain`）に存在していた。

### 修正内容

`primitiveRenderer3D_.setContext()` の全箇所に `deviceManager_.swapChain()` を追加:

```cpp
primitiveRenderer3D_.setContext(deviceManager_.immediateContext(),
                                deviceManager_.swapChain());
```

**修正対象ファイル:** `Artifact/src/Render/ArtifactIRenderer.cppm`
**修正箇所:** 行 267 (initialize)、行 464 (ensureInitialized)、行 478 (recreateSwapChain)

---

## 根本原因 2: WVP 行列の乗算順序が不正

### 問題

`PrimitiveRenderer3D::Impl::updateGizmoLineConstants()` で WVP 行列を合成する際:

```cpp
// PrimitiveRenderer3D.cppm:449 (修正前)
const QMatrix4x4 worldViewProj = viewMatrix_ * projMatrix_;
```

HLSL シェーダは:

```hlsl
// ShaderManager.cppm:246-247
PSIn.Pos = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
```

#### 行列規約の不整合

| 要素 | 規約 |
|------|------|
| `QMatrix4x4::constData()` | 列優先 (column-major) 格納 |
| HLSL cbuffer (デフォルト) | `column_major` |
| `mul(vec, mat)` | 行ベクトル × 行列 |

`mul(pos, M)` が正しいクリップ空間座標を返すには:

```
M = transpose(Proj × View)
```

しかし実際に渡されるのは `View × Proj`（QMatrix4x4 の列ベクトル規約での合成）。

#### 検証: ビルボードシェーダとの比較

同じ `PrimitiveRenderer3D` のビルボードシェーダ (`kBillboardVSSource`) は行列を**分離して**受け取り、**2 段**で変換する:

```hlsl
float4 viewPos = mul(float4(g_CenterAndRoll.xyz, 1.0), g_View);  // 行 82
Out.Pos = mul(viewPos, g_Proj);                                    // 行 85
```

これは `mul(pos, View)` → `mul(result, Proj)` = `pos × View × Proj` (行ベクトル規約) であり正しい。

しかし、ギズモ用 WVP を**事前合成**する場合、QMatrix4x4 の `operator*` は列ベクトル規約の乗算を行うため:

```
QMatrix4x4: view * proj = T(pan) × S(zoom) × Ortho  (列ベクトル規約)
```

行ベクトル規約での WVP とは**一致しない** (転置関係にある)。

#### 具体例: 平行移動行列での検証

QMatrix4x4 の平行移動行列 T(tx, ty, 0) を `constData()` → HLSL `column_major` cbuffer に入れて `mul(pos, T)` を計算:

```
result.x = x × 1 + y × 0 + z × 0 + w × 0 = x  (移動なし!)
result.w = x × tx + y × ty + z × 0 + w × 1     (移動が w に入る)
```

→ 平行移動が xy に反映されず w に漏れるため、**座標変換が完全に破綻**する。

### 修正内容

```cpp
// PrimitiveRenderer3D.cppm:447-451 (修正後)
void updateGizmoLineConstants()
{
    // HLSL uses mul(pos, M) with default column_major cbuffer layout.
    // For correct clip-space: M = transpose(Proj * View).
    const QMatrix4x4 worldViewProj = (projMatrix_ * viewMatrix_).transposed();
    std::memcpy(gizmoLineConstants_.worldViewProj, worldViewProj.constData(), sizeof(float) * 16);
}
```

**修正対象ファイル:** `Artifact/src/Render/PrimitiveRenderer3D.cppm`

---

## 修正後の描画フロー

```
CompositionRenderController::renderOneFrameImpl()
  ├─ レイヤー描画 (layerRTV → compute blend → accumRT → swap chain blit)
  ├─ setOverrideRTV(nullptr)          ← オフスクリーン終了
  │
  ├─ gizmo_->draw(renderer_)         ← 2D ギズモ (swap chain 直接描画) ← 既に動作
  │
  └─ gizmo3D_->draw(renderer_, view, proj)
       ├─ renderer->setViewMatrix(view)           → primitiveRenderer3D_.viewMatrix_ = view
       ├─ renderer->setProjectionMatrix(proj)     → primitiveRenderer3D_.projMatrix_ = proj
       ├─ renderer->setGizmoCameraMatrices(view, proj)
       ├─ renderer->drawGizmoArrow(...)
       │    └─ primitiveRenderer3D_.drawGizmoLineGeometry()
       │         ├─ hasRenderTarget()              → swapChain_->GetCurrentBackBufferRTV() ← 修正で有効に
       │         ├─ updateGizmoLineConstants()     → WVP = transpose(proj * view) ← 修正で正しい順序に
       │         └─ ctx_->Draw(...)                ← ギズモが描画される
       ├─ renderer->setUseExternalMatrices(false)
       └─ renderer->resetGizmoCameraMatrices()
```

---

## 影響範囲

| 修正 | 影響 |
|------|------|
| swapChain 追加 | `primitiveRenderer3D_` 経由の全描画 (3D ギズモ、3D ライン等) がスワップチェーンに描画可能になる。既存の `overrideRTV` 使用パスは `overrideRTV` 優先のため影響なし |
| WVP 行列順序 | ギズモ専用の `drawGizmoLineGeometry()` → `drawGizmoArrow()` / `drawGizmoRing()` にのみ影響。ビルボード描画は別 CB (`BillboardCB`) で個別に view/proj を渡すため影響なし |

---

## 残存する注意点

### 1. `setUseExternalMatrices` が 3D renderer に転送されない

```cpp
// ArtifactIRenderer.cppm:125
void setUseExternalMatrices(bool use) { primitiveRenderer_.setUseExternalMatrices(use); }
                                     // ↑ 2D のみ。3D にはフラグなし
```

`PrimitiveRenderer3D` は現状 `setViewMatrix` / `setProjectionMatrix` の値を直接使うため実害はないが、
将来 external matrix フラグが 3D 側にも必要になった場合はここを追加する必要がある。

### 2. gizmo 描画経路の二重化 (既存課題)

`TransformGizmo` (2D) と `Artifact3DGizmo` (3D) の二重描画は未解決。
詳細は `BUG_INVESTIGATION_GIZMO_VISIBILITY_2026-03-24.md` を参照。
