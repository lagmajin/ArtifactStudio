# パーティクルビルボード非表示: 根本原因と修正 (2026-03-27)

## 症状

パーティクルレイヤーを追加してもコンポジションビューに何も描画されない。
ソフトウェアフォールバック（`renderer->isInitialized()` が false のとき）は描画される。

---

## 関連する既存調査

| ドキュメント | 内容 |
|---|---|
| `PARTICLE_BILLBOARD_NOT_RENDERING_2026-03-26.md` | Identity マトリクスが渡されている問題を特定、対策案を記述 |

既存ドキュメントは問題を正確に指摘していたが修正が未着手だった。
本ドキュメントで全根本原因を確定し、修正を記録する。

---

## 調査: 描画フロー追跡

```
ArtifactCompositionRenderController::renderOneFrameImpl()
  → renderer_->setOverrideRTV(layerRTV)          // offscreen layer RT を設定
  → renderer_->clear()                            // PrimitiveRenderer2D が layerRTV を RTV にセット
  → drawLayerForCompositionView(layer, renderer)
      ※ ParticleLayer は専用ハンドラなし → fallback: layer->draw(renderer)
  → ArtifactParticleLayer::draw(renderer)
      → renderer->drawParticles(renderData)
  → renderer_->setOverrideRTV(nullptr)            // RTV 解除
  → blendPipeline_->blend(...)                    // layerRTV を accum へ合成
```

**レンダーターゲット:** `layerRTV` は `RenderPipeline::Layer` = `TEX_FORMAT_RGBA8_UNORM_SRGB`  
**パーティクル PSO:** `DefaultParticleRTVFormat` = `TEX_FORMAT_RGBA8_UNORM_SRGB`  
→ フォーマットは一致 ✓  
→ `clear()` が layerRTV を RTV にバインド済み → RTV バインドも問題なし ✓

---

## 根本原因 1 (致命的): マトリクスが常に Identity

### 問題

`ArtifactIRenderer::Impl::drawParticles()` は以下のようにマトリクスを取得していた:

```cpp
// 修正前 ArtifactIRenderer.cppm
particleRenderer_->setViewMatrix(primitiveRenderer_.viewMatrix().constData());
particleRenderer_->setProjectionMatrix(primitiveRenderer_.projectionMatrix().constData());
```

`primitiveRenderer_.viewMatrix()` は `externalViewMatrix_` を返す。  
コンポジションエディタは pan/zoom を **`ViewportTransformer`**（`setPan`/`setZoom`）で管理し、
`setViewMatrix` / `setProjectionMatrix` は一切呼ばない。  
→ `externalViewMatrix_` = `externalProjMatrix_` = **常に Identity**

### シェーダーへの影響

パーティクル VS (ParticleRenderer.cppm):

```hlsl
float4 viewPos = mul(float4(p.position, 1.0), ViewMatrix);  // Identity → viewPos = position
viewPos.xy += rotatedOffset;
Out.Pos = mul(viewPos, ProjMatrix);                          // Identity → clip = position
```

Identity マトリクス → `Out.Pos.xy = particle.position.xy`  

NDC の有効範囲は `[-1, 1]`。パーティクルがコンポジション座標（例: 960, 540 px）に存在する場合、
クリップ空間では `(960, 540)` → **完全に画面外** → 何も描画されない。

---

## 根本原因 2 (致命的): マトリクス転置の欠如

### 問題

HLSL の `mul(vector, matrix)` は **行ベクトル × 行列** の乗算:

```hlsl
result[j] = Σ_i ( v[i] * M[i][j] )   // row-vector convention
```

Diligent のデフォルトは `column_major` cbuffer。  
QMatrix4x4 も列優先（column-major）でデータを格納し、`constData()` は列優先の 16 float を返す。  
つまり HLSL の `M` は QMatrix4x4 の `M_qt` と**同じメモリ配置**になる。

しかし乗算の意味は異なる:

| 表現 | 意味 |
|---|---|
| `mul(v, M_qt)` | `v × M_qt` (行ベクトル × 行列) |
| QMatrix4x4 の意図 | `M_qt × v` (列ベクトル変換) |

`v × M_qt ≠ M_qt × v` → **転置が必要**:

```
mul(v, transpose(M_qt))  ≡  M_qt × v   ✓
```

この問題は、仮に根本原因 1 が修正されて正しいマトリクスを渡せたとしても、
**変換結果がすべて誤ったまま**になることを意味する。

---

## 根本原因 3 (なし): レンダーターゲット

- フォーマット: `DefaultParticleRTVFormat` = `MainRTVFormat` = `TEX_FORMAT_RGBA8_UNORM_SRGB` ✓
- バインド: `clear()` が layerRTV を RTV にセット、`prepare()` は RTV を変更しない ✓
- 深度バッファ: 未使用（PSO に DSVFormat 指定なし）、layerRTV に DSV なし → 問題なし ✓

---

## 修正内容

**修正ファイル:** `Artifact/src/Render/ArtifactIRenderer.cppm`

### 変更 1: ビューポートサイズの追跡

正射影行列の構築にビューポートのサイズが必要なため、`Impl` に追跡フィールドを追加:

```cpp
// Impl クラスのメンバー変数に追加
float m_viewportWidth  = 0.0f;
float m_viewportHeight = 0.0f;
```

`setViewportSize` ラッパーを更新して同期:

```cpp
void setViewportSize(float w, float h) {
    primitiveRenderer_.setViewportSize(w, h);
    m_viewportWidth  = w;
    m_viewportHeight = h;
}
```

`initializeHeadless()` は `primitiveRenderer_` を直接呼ぶため、そこでも明示的に設定:

```cpp
primitiveRenderer_.setViewportSize(float(width), float(height));
m_viewportWidth  = float(width);
m_viewportHeight = float(height);
```

### 変更 2: drawParticles に正しい View/Proj を構築

`drawParticles()` 内で PrimitiveRenderer2D の現在の pan/zoom/viewport から
適切なマトリクスを構築し、**転置してから**パーティクルレンダラーに渡す:

```cpp
if (m_viewportWidth <= 0.0f || m_viewportHeight <= 0.0f) return;

float panX = 0.0f, panY = 0.0f;
primitiveRenderer_.getPan(panX, panY);
const float zoom = primitiveRenderer_.getZoom();

// View: canvas space → viewport pixel space
QMatrix4x4 view;
view.translate(panX, panY, 0.0f);
view.scale(zoom, zoom, 1.0f);

// Proj: ortho, viewport pixels → NDC (Y-flip for screen Y-down convention)
QMatrix4x4 proj;
proj.ortho(0.0f, m_viewportWidth, m_viewportHeight, 0.0f, -1.0f, 1.0f);

// Transpose for HLSL mul(vector, matrix) row-vector convention
particleRenderer_->setViewMatrix(view.transposed().constData());
particleRenderer_->setProjectionMatrix(proj.transposed().constData());
```

---

## マトリクス変換の検証

コンポジション座標 `(cx, cy)` → NDC の計算:

**View 適用後 (viewport pixel space):**
```
vx = cx * zoom + panX
vy = cy * zoom + panY
```

**Proj 適用後 (ortho, left=0, right=vw, bottom=vh, top=0):**
```
NDC_x = (2 * vx / vw) - 1   = (2*(cx*zoom + panX) / vw) - 1
NDC_y = -(2 * vy / vh) + 1  = 1 - (2*(cy*zoom + panY) / vh)   ← Y flip
```

これは PrimitiveRenderer2D の `ViewportTransformer` が行う変換と等価。
→ パーティクルと 2D スプライト・シェイプが同一座標系に揃う ✓

---

## オフスクリーンパスとの整合性

コンポジションレンダーループはレイヤー描画前に以下を設定する:

```cpp
// ArtifactCompositionRenderController.cppm:1965-1967
renderer_->setViewportSize(rcw, rch);               // ← m_viewportWidth/Height も更新される
renderer_->setZoom(origZoom * offscreenScale);
renderer_->setPan(origPanX * offscreenScale, origPanY * offscreenScale);
```

`drawParticles()` はこれらの値を読み取るため、オフスクリーン RT のスケールに自動的に追従する。

---

## 3D ギズモ修正との比較

| 項目 | 3D ギズモ | パーティクル |
|---|---|---|
| 根本原因 1 | `swapChain_` 未渡し → `hasRenderTarget()` = false | Identity マトリクス |
| 根本原因 2 | `viewMatrix_ * projMatrix_` の順序が逆 | マトリクス転置なし |
| マトリクス修正方法 | `(projMatrix_ * viewMatrix_).transposed()` | 個別に `view.transposed()` / `proj.transposed()` |
| RT フォーマット | 問題なし | 問題なし |

どちらの修正も `mul(v, M)` 行ベクトル × `column_major` cbuffer の変換規約に起因している。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Render/ArtifactIRenderer.cppm` | (drawParticles) | 修正済み: 正射影 View/Proj + 転置 |
| `ArtifactCore/src/Graphics/ParticleRenderer.cppm` | 51-75 | VS: `mul(pos, ViewMatrix)` / `mul(viewPos, ProjMatrix)` |
| `ArtifactCore/include/Graphics/ParticleData.ixx` | 13 | `DefaultParticleRTVFormat = TEX_FORMAT_RGBA8_UNORM_SRGB` |
| `Artifact/src/Layer/ArtifactParticleLayer.cppm` | 110-132 | GPU パス: `renderer->drawParticles()` を呼ぶ |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 2039-2047 | オフスクリーン RT 設定とレイヤー描画ループ |
