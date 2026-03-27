# 3D ギズモが表示されない原因 (2026-03-26)

## 症状

平面レイヤー選択時に 3D ギズモ（Move/Rotate/Scale 軸）が表示されない。
正射影マトリクスをセットしても表示されない。

---

## 再調査結果: 2D レンダラーと 3D レンダラーのマトリクスが別

`ArtifactIRenderer` には **2つの独立したレンダラー** がある:

| レンダラー | マトリクス | 用途 |
|-----------|----------|------|
| `primitiveRenderer_` (2D) | `externalViewMatrix_` / `externalProjMatrix_` | 2D 描画 |
| `primitiveRenderer3D_` (3D) | `viewMatrix_` / `projMatrix_` | 3D 描画 |

### 問題のコードフロー

```cpp
// CompositionRenderController.cppm:2154-2161
QMatrix4x4 view, proj;
view.translate(panX, panY, 0); view.scale(zoom, zoom, 1);
proj.ortho(0, viewportW, viewportH, 0, -1000, 1000);
gizmo3D_->draw(renderer_.get(), view, proj);
```

```cpp
// Artifact3DGizmo.cppm:323-325
renderer->setViewMatrix(view);        // ← ArtifactIRenderer::setViewMatrix
renderer->setProjectionMatrix(proj);  // ← ArtifactIRenderer::setProjectionMatrix
```

```cpp
// ArtifactIRenderer.cppm:596
void ArtifactIRenderer::setViewMatrix(const QMatrix4x4& m) {
    impl_->primitiveRenderer_.setViewMatrix(m);  // ← 2D renderer にセット
    // 3D renderer にはセットしない！
}
```

```cpp
// ArtifactIRenderer.cppm:752-755
void ArtifactIRenderer::draw3DArrow(...) {
    impl_->primitiveRenderer3D_.draw3DArrow(...);  // ← 3D renderer で描画
}
```

**結果:** 3D レンダラーの `viewMatrix_` / `projMatrix_` は Identity のまま。
ギズモの座標が NDC [-1,1] 外 → 表示されない。

### 証拠: PrimitiveRenderer3D が独自マトリクスを持つ

**場所:** `PrimitiveRenderer3D.cppm:508-528`

```cpp
void PrimitiveRenderer3D::setViewMatrix(const QMatrix4x4& view) {
    impl_->viewMatrix_ = view;  // 独自の viewMatrix_
}
void PrimitiveRenderer3D::setProjectionMatrix(const QMatrix4x4& proj) {
    impl_->projMatrix_ = proj;  // 独自の projMatrix_
}
void PrimitiveRenderer3D::resetMatrices() {
    // Identity にリセット
}
```

### 3D レンダラーがマトリクスを使用する箇所

**場所:** `PrimitiveRenderer3D.cppm` の Impl 内

`drawBillboard` や `draw3DLine` で `impl_->viewMatrix_` と `impl_->projMatrix_` を使用して MVP を構築。
Identity のままなので座標が NDC 外。

---

## 対策案

### 対策1: ArtifactIRenderer の setViewMatrix/setProjectionMatrix で両レンダラーにセット (推奨)

```cpp
void ArtifactIRenderer::setViewMatrix(const QMatrix4x4& m) {
    impl_->primitiveRenderer_.setViewMatrix(m);
    impl_->primitiveRenderer3D_.setViewMatrix(m);  // ← 追加
}
void ArtifactIRenderer::setProjectionMatrix(const QMatrix4x4& m) {
    impl_->primitiveRenderer_.setProjectionMatrix(m);
    impl_->primitiveRenderer3D_.setProjectionMatrix(m);  // ← 追加
}
```

### 対策2: ArtifactIRenderer に set3DCameraMatrices を呼ぶ

既存の `set3DCameraMatrices(view, proj)` メソッド (`ArtifactIRenderer.cppm:126`) を使用:
```cpp
renderer_->set3DCameraMatrices(view, proj);
```

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 596-597 | setViewMatrix — 2D renderer にのみセット |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 752-755 | draw3DArrow — 3D renderer を使用 |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 126 | set3DCameraMatrices — 既存 API |
| `Artifact/src/Render/PrimitiveRenderer3D.cppm` | 508-528 | 独自の viewMatrix_/projMatrix_ |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 2143-2164 | gizmo3D_->draw 呼び出し |
| `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` | 323-325 | setViewMatrix/setProjectionMatrix を呼ぶが 2D にしかセットされない |

## 原因: View/Projection マトリクスが Identity のまま

**場所:** `CompositionRenderController.cppm:2145`

```cpp
gizmo3D_->draw(renderer_.get(), renderer_->getViewMatrix(), renderer_->getProjectionMatrix());
```

`renderer_->getViewMatrix()` と `renderer_->getProjectionMatrix()` が
**Identity マトリクスを返す**。

### 根本原因

1. `PrimitiveRenderer2D` の `externalViewMatrix_` / `externalProjMatrix_` はデフォルト Identity (`PrimitiveRenderer2D.cppm:94-95`)
2. **どこからも `setViewMatrix()` / `setProjectionMatrix()` が呼ばれていない**
   - `PrimitiveRenderer2D.cppm:325-326` の setter は定義されているが呼び出し元がない
   - コンポジションエディタは 2D ビューポートなので 3D マトリクスをセットアップするコードがない
3. 3D ギズモ自身の `draw()` 内で `renderer->setViewMatrix(view)` / `renderer->setProjectionMatrix(proj)` を呼んでいるが、渡される `view` / `proj` 引数自体が Identity

### 結果: 座標が画面外

Identity マトリクス = ワールド座標がそのまま NDC になる:

```
MVP = Identity × Identity × model = model
```

ギズモ位置 = レイヤーの canvas 座標（例: 960, 540）
→ NDC では (960, 540, 0)
→ NDC 有効範囲 = [-1, 1]
→ **完全に画面外** → 表示されない

### draw3DLineLocal のマトリクス使用

**場所:** `PrimitiveRenderer2D.cppm:1417-1418`

```cpp
QMatrix4x4 mvp = impl_->externalProjMatrix_ * impl_->externalViewMatrix_;
float mvpData[16];
// ... MVP を定数バッファに書き込み
```

`externalProjMatrix_` × `externalViewMatrix_` = Identity × Identity = Identity
→ 頂点座標がそのまま NDC として使用される。

### draw3DArrow も同様

**場所:** `ArtifactIRenderer.cppm` 経由で `draw3DLineLocal` を使用。
矢印の軸（例: X軸 = `center` から `{center.x + s, center.y, center.z}`）
→ NDC (960+24, 540, 0) = 画面外。

---

## 対策案

### 対策1: レンダーループで正射影マトリクスをセット（推奨）

コンポジションエディタのビューポートから正射影マトリクスを構築し、
3D ギズモ描画直前にセット:

```cpp
// CompositionRenderController の renderOneFrameImpl() 内、gizmo3D_->draw の前:
if (gizmo3D_) {
    // 正射影マトリクスを構築
    QMatrix4x4 orthoProj;
    const float cw = static_cast<float>(comp->width());
    const float ch = static_cast<float>(comp->height());
    const float zoom = renderer_->getZoom();
    const auto [panX, panY] = /* renderer の pan 値 */;
    orthoProj.ortho(0, cw, ch, 0, -1000, 1000); // Y 反転（Qt 座標系）

    // ビューマトリクス（pan + zoom）
    QMatrix4x4 viewMatrix;
    viewMatrix.translate(panX, panY, 0);
    viewMatrix.scale(zoom, zoom, 1);

    syncGizmo3DFromLayer(selectedLayer);
    gizmo3D_->draw(renderer_.get(), viewMatrix, orthoProj);
}
```

### 対策2: syncGizmo3DFromLayer でワールド座標を NDC に変換

`syncGizmo3DFromLayer` でレイヤーのワールド座標をビューポートの NDC に変換してからセット。

### 対策3: ArtifactIRenderer に getOrthoMatrix() を追加

`ViewportTransformer` が既に pan/zoom を管理しているので、そこから正射影マトリクスを取得する API を追加。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 2143-2145 | gizmo3D_->draw 呼び出し（マトリクス未セット） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 637-668 | syncGizmo3DFromLayer |
| `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` | 313-355 | draw() — setViewMatrix/setProjectionMatrix を呼ぶが引数が Identity |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 94-95 | externalViewMatrix_ / externalProjMatrix_ デフォルト Identity |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 325-326 | setViewMatrix / setProjectionMatrix（未使用） |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 1417-1418 | draw3DLineLocal の MVP 計算 |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | 127-257 | マウスイベント（ビューポート操作、pan/zoom 管理） |
