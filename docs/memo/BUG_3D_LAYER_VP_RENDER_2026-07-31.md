# 3DレイヤーがVPに正常に描画されない不具合 調査メモ

**日付**: 2026-07-31
**状態**: 調査中（原因未確定）

## 描画パイプライン

### メインパス（GPU）

1. `drawGpuLayerToIntermediate()` (ArtifactCompositionRenderController L8909)
2. → `drawLayerForCompositionView()` (L6223)  
3. → L6294-6327: `solid3DCardColor()` → ArtifactSolid2DLayer/ArtifactSolidImageLayer のみ対象 → Artifact3DLayer は**通過**
4. → L6329-6405: `directShape3DCard()` → ArtifactShapeLayer のみ対象 → **通過**
5. → L6537-6565: `layer->is3D()` → **true** → `set3DCameraMatrices()` → `layer->draw(renderer)`
6. → `Artifact3DLayer::draw()` (Artifact3DModelLayer.cppm L701)

### CPUパス（RenderQueueService / 旧パス）

- `ArtifactCompositionViewDrawing.cppm` の `export void drawLayerForCompositionView()` (L1147)
- → `layer->draw(renderer)` を直接呼ぶ（L1874）
- **カメラ行列パラメータを受け取らない**！→ meshViewMatrix_/meshProjMatrix_ が identity のまま

## 疑わしい箇所（優先度順）

### 1. `localBounds()` が無効 (L1168-1175 / L6276-6290)

- `sourceSize()` がゼロ時に `QRectF()` を返す → `isValid()` = false でスキップ
- meshロード前に sourceSize が初期化されてないケース
- ファイル: Artifact3DModelLayer.cppm L822-834

### 2. `meshLoaded_` が false (Artifact3DModelLayer.cppm L736)

- モデルファイル読み込み失敗
- Geometry 変更イベントで false のまま
- 変数: `Impl::meshLoaded_` (L98), デフォルト false

### 3. カメラマトリクス設定の整合性

- `set3DCameraMatrices()` で meshViewMatrix_ / meshProjMatrix_ を設定
- `drawMesh()` 内で `renderer->setViewMatrix(meshViewMatrix_)` (L768)
- PrimitiveRenderer3D::setCameraMatrices() と別経路の setViewMatrix/setProjectionMatrix が競合する可能性
- ファイル: ArtifactIRenderer.cppm L1002-1005, L768-769

### 4. CPUパス（CompositionViewDrawing）のカメラ未設定

- `ArtifactCompositionViewDrawing::drawLayerForCompositionView()` は cameraView/cameraProj パラメータなし
- mesh 描画時に meshViewMatrix_ = identity のまま描画 → 表示外に飛ぶ

## デバッグ方法

`ARTIFACT_DISABLE_3D_RENDER_TRACE` 環境変数を**未設定**にして起動すると、以下がログ出力される:

```
[Artifact3DLayer][RenderTrace] frame=... outcome=<結果> ...
[ArtifactIRenderer][MeshTrace] key="..." outcome=<結果> ...
```

outcome の種類:
- `skip:no-renderer` / `skip:not-visible` / `skip:mesh-not-loaded` / `skip:no-position-data`
- `mesh-submitted` / `wireframe-submitted`
- `skip:no-mesh-renderer` / `skip:no-render-data`

## キーファイル

| ファイル | 役割 |
|----------|------|
| Artifact3DModelLayer.cppm L701-815 | 3Dレイヤー draw() |
| Artifact3DModelLayer.cppm L822-834 | localBounds() |
| Artifact3DModelLayer.cppm L672-681 | updateSourceSizeFromMesh() |
| ArtifactCompositionRenderController.cppm L6223-6590 | drawLayerForCompositionView (GPU) |
| ArtifactCompositionViewDrawing.cppm L1147-1875 | drawLayerForCompositionView (CPU/旧) |
| ArtifactIRenderer.cppm L617-799 | drawMesh() 実装 |
| ArtifactIRenderer.cppm L1002-1026 | set3DCameraMatrices/reset |
