# パーティクルビルボード描画が機能しない原因 (2026-03-26)

## 症状

パーティクルレイヤーのビルボード描画が全く表示されない。

## レンダリングフロー

```
ArtifactParticleLayer::draw(renderer)
  → renderer->drawParticles(renderData)
    → ArtifactIRenderer::Impl::drawParticles(data)
      → particleRenderer_->setViewMatrix(primitiveRenderer_.viewMatrix().constData())  // ← Identity!
      → particleRenderer_->setProjectionMatrix(primitiveRenderer_.projectionMatrix().constData()) // ← Identity!
      → particleRenderer_->updateBuffer(data)
      → particleRenderer_->prepare(ctx)
      → particleRenderer_->draw(ctx, count)
```

## 根本原因: マトリクスが Identity のまま

`drawParticles()` (ArtifactIRenderer.cppm:186-207) がマトリクスを取得する:

```cpp
particleRenderer_->setViewMatrix(primitiveRenderer_.viewMatrix().constData());
particleRenderer_->setProjectionMatrix(primitiveRenderer_.projectionMatrix().constData());
```

`primitiveRenderer_.viewMatrix()` = `externalViewMatrix_` = **Identity**
`primitiveRenderer_.projectionMatrix()` = `externalProjMatrix_` = **Identity**

コンポジションエディタはビューポートの pan/zoom を `ViewportTransformer` (setPan/setZoom) で管理し、
**`setViewMatrix` / `setProjectionMatrix` を呼ばない**。

## シェーダーでの影響

**ParticleRenderer VS** (ParticleRenderer.cppm:51-75):

```hlsl
float4 viewPos = mul(float4(p.position, 1.0), ViewMatrix);  // Identity → viewPos = position
viewPos.xy += rotatedOffset;                                  // billboard offset
Out.Pos = mul(viewPos, ProjMatrix);                           // Identity → clip = position
```

Identity マトリクス → `Out.Pos = float4(position.x, position.y, position.z, 1.0)`

NDC 範囲 = [-1, 1]。パーティクルの座標がコンポジション座標系（例: 960, 540）の場合、
NDC では (960, 540, 0) → **完全に画面外** → 何も描画されない。

## 2D レンダラーとの違い

2D レンダラー (`PrimitiveRenderer2D`) は pan/zoom を**定数バッファ** (`ViewportTransformer`) で管理:
```hlsl
cbuffer CBSolidTransform2D {
    float2 offset;    // pan
    float2 scale;     // zoom
    float2 screenSize;
};
finalPos.xy = vertex.xy * zoom + pan;
```

`setViewMatrix` / `setProjectionMatrix` は使われない（`externalViewMatrix_` は3D描画用）。

パーティクルレンダラーは `ViewMatrix` / `ProjMatrix` を**使用する**設計。
2D レンダラーの定数バッファ方式と互換がない。

## 証拠

- `ArtifactIRenderer.cppm:201-202` — Identity マトリクスが渡される
- `ArtifactIRenderer.cppm:313,320` — `setPan`/`setZoom` は `ViewportTransformer` を使用
- `PrimitiveRenderer2D.cppm:325-326` — `setViewMatrix` は `externalViewMatrix_` にセットするだけ
- コンポジションエディタは `setPan`/`setZoom` のみ使用、`setViewMatrix` を呼ばない

## 対策案

### 対策1: drawParticles の前にビューマトリクスを構築してセット

コンポジションエディタのレンダーループで:
```cpp
// pan/zoom から ViewMatrix / ProjMatrix を構築
float panX, panY, zoom;
renderer_->getPan(panX, panY);
zoom = renderer_->getZoom();

QMatrix4x4 view;
view.translate(panX, panY, 0.0f);
view.scale(zoom, zoom, 1.0f);

QMatrix4x4 proj;
float cw = comp->width();
float ch = comp->height();
proj.ortho(0.0f, cw, ch, 0.0f, -1000.0f, 1000.0f);

renderer_->setViewMatrix(view);
renderer_->setProjectionMatrix(proj);
```

### 対策2: drawParticles 内で pan/zoom からマトリクスを自動構築

`ArtifactIRenderer::Impl::drawParticles()` 内で `ViewportTransformer` の値から
ViewMatrix / ProjMatrix を構築してからセットする。

### 対策3: パーティクルシェーダーを 2D 方式に変更

`ViewportTransformer` の定数バッファを使用するように VS を書き換え:
```hlsl
cbuffer Constants {
    float2 pan;
    float2 zoom;
    float2 screenSize;
};
float4 worldPos = float4(p.position, 1.0);
worldPos.xy = worldPos.xy * zoom + pan;
Out.Pos = worldPos / float4(screenSize * 0.5, 1.0, 1.0) - 1.0;
```

---

## 追記: 現在の実装状態

このメモの後で、`ArtifactParticleLayer::draw()` は **renderer が初期化済みなら Diligent billboard 描画を優先し、ソフト描画へは落とさない** 形に寄せた。

- `renderer->drawParticles(renderData)` を呼ぶ
- `renderer` が使える場合は、その後の `QPainter` fallback をスキップする
- fallback は `renderer` 未初期化時だけ残る

つまり、今「描画されない」なら、原因はほぼ次のどちらかに絞れる。

1. `ArtifactIRenderer::drawParticles()` に渡している view/projection がまだ Identity のまま
2. `ParticleRenderer` 側の billboard PSO / SRB / buffer 生成が失敗している

関連ファイル:

- [`Artifact/src/Layer/ArtifactParticleLayer.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactParticleLayer.cppm)
- [`Artifact/src/Render/ArtifactIRenderer.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)
- [`ArtifactCore/src/Graphics/ParticleRenderer.cppm`](x:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/ParticleRenderer.cppm)

現在の状態整理はこちら:

- [`docs/bugs/PARTICLE_LAYER_STATUS_AND_ISSUES_2026-03-27.md`](x:/Dev/ArtifactStudio/docs/bugs/PARTICLE_LAYER_STATUS_AND_ISSUES_2026-03-27.md)

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 186-207 | drawParticles — Identity マトリクスを渡す |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 201-202 | setViewMatrix/setProjectionMatrix 呼び出し |
| `ArtifactCore/src/Graphics/ParticleRenderer.cppm` | 51-75 | VS — ViewMatrix / ProjMatrix を使用 |
| `ArtifactCore/src/Graphics/ParticleRenderer.cppm` | 206-212 | setViewMatrix/setProjectionMatrix |
| `Artifact/src/Layer/ArtifactParticleLayer.cppm` | 110-133 | GPU パス — drawParticles を呼び出し |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 313-320 | setPan/setZoom — ViewportTransformer |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 325-326 | setViewMatrix — externalViewMatrix_ にセット |
