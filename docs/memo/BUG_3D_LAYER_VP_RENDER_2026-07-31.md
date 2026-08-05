# 3DレイヤーがVPに正常に描画されない不具合 調査メモ

**日付**: 2026-07-31
**状態**: 調査中 — 2026-08-04 追記で「フレームギズモは出る / メッシュ本体が不可視」に絞り込み。原因は MeshRenderer（三角形パイプライン）側、とくにバックフェイスカリングの巻き順不一致を最有力候補とする。

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

## 2026-08-04 追記: 症状の絞り込み（フレームギズモは出るがメッシュ本体が見えない）

### 観測事実
- コンポジション上で **フレームギズモ（3D フレーム / カメラフラスタム）は正しく描画される**。
- しかし **3D モデル本体（メッシュ）がまったく表示されない**。

### この事実が意味するもの
フレームギズモは `ArtifactCompositionRenderController.cppm:5779` の
`renderer->set3DCameraMatrices(visual.viewMatrix, visual.projectionMatrix)`
と同じ view/proj 行列を使い、`PrimitiveRenderer3D` 経由で**同じ 3D レンダーターゲット**に線（line）として描かれる。
→ したがって「カメラ行列の未設定」「レンダーターゲットの未バインド」「GPU パス自体の不在」は**除外**できる。

### 修正された疑わしさ（優先度順）

1. **メッシュ描画はギズモとは別のレンダラー（MeshRenderer）を使う**（最重要）
   - ギズモ = `PrimitiveRenderer3D`（line）。
   - メッシュ = `ArtifactCore::MeshRenderer`（`ArtifactIRenderer::drawMesh` → `meshRenderer_->draw`）。
   - 両者は独立したパイプライン。ギズモが出るからといって MeshRenderer の三角形パイプラインが正常とは限らない。

2. **バックフェイスカリングによる全面カリング（最有力）**
   - `createCubeMesh()` 等の面の巻き順（winding）と、MeshRenderer 側のカリングモード（CCW/CW）が食い違うと、全三角形がカリングされて「線は見えるが面が見えない」になる。
   - ギズモは line なのでカリングの影響を受けず、この症状と完全一致する。
   - 確認箇所: `Artifact3DModelLayer.cppm` の `create*Mesh()` 群（L397-735）の頂点順序、`MeshRenderer` の `SetCullMode` / ラスタライザ設定。

3. **インスタンス（モデル）行列の転置ミスマッチ**
   - `ArtifactIRenderer.cppm:790-795` で `instance.transform[row*4+col] = modelData[col*4+row]` と転置して渡している。
   - MeshRenderer の頂点シェーダが想定するレイアウトと合わないと、キューブが縮退（plane/line 化）または視錐台外へ。
   - ただし「完全に見えない」だけでなく「変な形で見える」はずなので、カリング説より優先度は下。

4. **geometry が生成されていない / 空データ**
   - `ArtifactIRenderer.cppm:670-674` で `mesh.generateRenderData()` が空なら `skip:no-render-data`。
   - また `mesh.revision()` が更新されずキャッシュヒットして古い（空）geometry のまま、の可能性。

5. **法線ゼロによる真っ黒（低優先）**
   - `useTextureInSolid_=false`（shading=8）で baseColor 白・ライト有なら通常表示されるはず。法線欠損で黒になるだけで「消える」わけではない。

### 次の確認ステップ
- `ARTIFACT_DISABLE_3D_RENDER_TRACE` を未設定で起動し、以下を確認:
  - `[Artifact3DLayer][RenderTrace] outcome=` が `mesh-submitted` になっているか（mesh の draw まで到達しているか）。
  - `[ArtifactIRenderer][MeshTrace] outcome=` が `gpu-draw-issued` になっているか（GPU にドローが出ているか）。
- `gpu-draw-issued` なら原因は MeshRenderer 側（カリング/シェーダ/頂点レイアウト）。`skip:*` ならその手前の生成/キャッシュ。
- MeshRenderer のカリングモードと `createCubeMesh()` の巻き順を突き合わせる。

### MeshRenderer PSO 調査結果（2026-08-04 深掘り）

`ArtifactCore/src/Graphics/MeshRenderer.cppm` を確認:

1. **PSO に `RasterizerState.CullMode` が一切設定されていない（最重要）**
   - `createPSO()` (L870-1009) で CullMode / RasterizerState / FrontCounterClockwise を一度も設定せず。
   - Diligent のデフォルトは `CULL_MODE_BACK`（裏面カリング ON）＋ `FrontCounterClockwise=false`（CW が表）。
   - ギズモは `PrimitiveRenderer3D` の **line** 描画でカリングの影響を受けないが、メッシュは **triangle list**（`PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`, L880）で裏面カリングされる。
   - → `createCubeMesh()` 等の面の巻き順とこのデフォルト表裏規定が食い違うと、全三角形がカリングされて「線は見える／面が見えない」になる。症状と一致。

2. **ピクセルシェーダ mode 8（デフォルトソリッド）は可視色を返す（除外）**
   - `MeshPSSource` (L370): `solidBase = In.Color`（白）で、`SceneLightingMeta.x==0` なら studio rig (L454-) で可視なグレー/白を出力。
   - よってシェーディング・黒出力による不可視は除外できる。

3. **GPU 可視カリングは n=1 で無効（除外）**
   - `prepare()` (L1101-1106): `uploadedInstanceCount_ >= 64` が必須。単一 3D レイヤーは 1 インスタンスなので `gpuCullActive_=false`。通常の `DrawIndexed` 経路（L1243-1249）。

4. **（二次）法線変換の転置不整合**
   - VSMain (L249-251): `(float3x3)inst.transform` で回転を取り出しているが、CPU 側で transform を転置してアップロードしているため、ここで得られるのは回転の逆転置。法線が誤変換される。
   - ただし mode 8 studio は N が多少ずれても可視範囲内の出力になるため「不可視」の主因ではない。

5. **補足: インスタンス行列は view/proj と同じ転置規約でアップロードされている**（ArtifactIRenderer.cppm L790-795 の transpose と、MeshRenderer 側 `transpose4x4` で整合）。位置・スケールはおそらく正しい。

### 推奨する確定手順
- `createPSO()` に `PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;`（および `FrontCounterClockwise` の明示）を追加し、かつ `createCubeMesh()` の巻き順が outward-CCW と合致するか確認。
- 診断として一時的に `CULL_MODE_NONE` にしてメッシュが出現するか確認すれば、裏面カリングが原因か即座に判別できる。
- ※ `MeshRenderer` は Diligent 低レベル実装にあたるため、修正は確認後に実施。

### キーファイル

| ファイル | 役割 |
|----------|------|
| Artifact3DModelLayer.cppm L701-815 | 3Dレイヤー draw() |
| Artifact3DModelLayer.cppm L397-735 | create*Mesh() 巻き順（カリング影響） |
| Artifact3DModelLayer.cppm L822-834 | localBounds() |
| Artifact3DModelLayer.cppm L672-681 | updateSourceSizeFromMesh() |
| ArtifactCompositionRenderController.cppm L5777-5817 | フレームギズモ描画（set3DCameraMatrices 使用） |
| ArtifactCompositionRenderController.cppm L6223-6590 | drawLayerForCompositionView (GPU) |
| ArtifactCompositionRenderController.cppm L7162-7191 | 3D レイヤー mesh 描画ブロック |
| ArtifactCompositionViewDrawing.cppm L1147-1875 | drawLayerForCompositionView (CPU/旧) |
| ArtifactIRenderer.cppm L617-857 | drawMesh() 実装（MeshRenderer 呼出し） |
| ArtifactIRenderer.cppm L790-795 | インスタンス行列転置 |
| ArtifactIRenderer.cppm L1002-1026 | set3DCameraMatrices/reset |
| ArtifactCore MeshRenderer (libs/MeshRenderer 等) | 三角形パイプライン / カリング / シェーダ（要別途調査） |
