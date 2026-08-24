# Particle Layer Status and Open Issues (2026-03-27)

## Current State

`ArtifactParticleLayer` is now routed like this:

```text
ArtifactParticleLayer::draw(renderer)
  -> goToFrame(frameNumber, fps)
  -> if renderer is initialized:
       captureRenderData()
       renderer->drawParticles(renderData)
     else:
       renderToImage() fallback via QPainter
```

So the live composition / preview path is currently GPU billboard first, and the software image path is only a fallback when the renderer is not initialized.

Relevant files:

- [`Artifact/src/Layer/ArtifactParticleLayer.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Layer/ArtifactParticleLayer.cppm)
- [`Artifact/src/Render/ArtifactIRenderer.cppm`](x:/Dev/ArtifactStudio/Artifact/src/Render/ArtifactIRenderer.cppm)
- [`ArtifactCore/src/Graphics/ParticleRenderer.cppm`](x:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/ParticleRenderer.cppm)

## What Has Already Been Addressed

- The particle layer was switched away from always using the QPainter fallback when a renderer exists.
- The renderer side now receives particle render data through `ArtifactIRenderer::drawParticles()`.
- The renderer camera matrices are forwarded through `ArtifactIRenderer::setViewMatrix()` / `setProjectionMatrix()` to both the 2D and 3D primitive paths.
- The previous "particle layer does not show at all" symptom was narrowed down to matrix forwarding / PSO / SRB / buffer initialization instead of the layer abstraction itself.

## Open Issues

### 1. Render settings are serialized and editable, but not fully consumed by the GPU path — ✅ 部分解消 (2026-08-21)

The particle layer exposes and saves:

- `blendMode`
- `billboardMode`
- `sortMode`
- `depthTest`
- `depthWrite`

Those values are visible in the property UI and are written to JSON, but `ArtifactCore::ParticleRenderer` still hardcodes the billboard draw path and additive blending in its PSO setup.

**2026-08-21 修正**: ブレンドモード / billboard モード / depthTest / depthWrite は `coreRenderOptionsFromSettings()`(`ArtifactParticleLayer.cppm`)経由で `ParticleRenderData.options` に転写され、GPU パスでも反映されるようになった。残る未反映項目は `sortMode`(GPU パスはソート非対応)と `softParticles`。

### 1b. VS StructuredBuffer ストライド不一致 — ✅ 修正済み (2026-08-21)

頂点シェーダー内の `ParticleData` 構造体に sprite 系 3 フィールドが欠落しており(60 バイト)、C++ の `ParticleVertex`(72 バイト)およびカリング CS のレイアウトと不一致だった。2 個目以降の粒子データが崩壊する/描画が拒否される原因。VS 側にフィールドを追加してストライドを一致させた(`ParticleRenderer.cppm`)。

### 1c. 冒頭フレームの preWarm ポップ — ✅ 修正済み (2026-08-21)

`ParticleSystem::goToFrame()` が frame<=1 で常に 0.5 秒分のプリウォームを実行し、frame 1→2 で粒子数が急減していた。プリウォーム時間を目標フレーム時刻以下に制限し、通常シミュレーションと連続になるよう修正(`ArtifactParticleGenerator.cppm`)。

### 1d. 最小サイズ 4.0 クランプ — ✅ 修正済み (2026-08-21)

寿命末にサイズ 0 で消える想定のプリセット(sparks/fire 等)が GPU パスで最低 4 単位(quad 幅 40px 相当)より小さくなれず、加算合成時に白飛びして残っていた。クランプを撤去(`ArtifactParticleLayer.cppm` の `transformParticleRenderData`)。

### 1e. drag の数値不安定性 — ✅ 修正済み (2026-08-21)

線形減衰 `v *= (1 - drag*dt)` は drag > 1/dt(=120)で係数が負になり速度が反転・発散していた。指数減衰 `v *= (1 - min(drag,1))^dt` に変更し、任意の drag 値で安定(`ArtifactParticleGenerator.cppm`)。

### 1f. directionSpread の 180° クランプ — ✅ 修正済み (2026-08-21)

UI/プリセット(dust/explosion/pollen)は 360° を使用するが内部で 180° にクランプされ、全方向放出が半円になっていた。360° まで許容するよう修正(`ArtifactParticleGenerator.cppm` の `getEmissionDirection`)。

### 1g. FormParticleLayer がレイヤー opacity を無視 — ✅ 修正済み (2026-08-21)

`buildRenderData()` に `layerOpacity` 引数を追加し、`draw()` から `opacity()` を渡すよう修正。キャッシュシグネチャにも opacity を混入させ、opacity 変更が即座に反映される(`ArtifactFormParticleLayer.cppm`)。

### 1h. ソフト描画と GPU 描画のサイズ 2 倍差 — ✅ 修正済み (2026-08-21)

ソフト経路は半径 `scale*10`(直径 20*scale)、GPU 経路は quad 幅 `10*size`(半幅 size*5)で、同一設定でもソフトが GPU の約 2 倍大きかった。GPU の halfWidth を `size*2.5`(直径 = ソフトと同一)に変更(`ParticleRenderer.cppm`)。

### 2. `drawParticles()` still depends on the renderer camera state being valid

`ArtifactIRenderer::drawParticles()` forwards `primitiveRenderer_.viewMatrix()` and `primitiveRenderer_.projectionMatrix()` to `ParticleRenderer`.

That means particle visibility still depends on:

- the composition renderer having initialized its camera matrices
- the current viewport path having pushed the correct view/projection matrices before drawing

If the matrices are identity or stale, particles can disappear or appear in the wrong place.

### 3. `goToFrame()` is called on every draw

`ArtifactParticleLayer::draw()` advances the particle system to the current frame before every render.

This keeps the result deterministic, but it also means particle rendering cost is tied to frame sampling and simulation work. For complex emitters this can become expensive, especially if the layer is evaluated often during preview.

### 4. Software fallback still exists

When the renderer is not initialized, the layer still falls back to `renderToImage()` and `QPainter`.

That fallback is useful for compatibility, but it also means there are still two rendering paths to keep in sync:

- GPU billboard path
- software image path

Any mismatch between them can show up as different color, alpha, or sizing behavior.

## Investigation Notes

If particles still look wrong, the next checks are:

1. Is `ArtifactIRenderer::setViewMatrix()` / `setProjectionMatrix()` being called before `drawParticles()`?
2. Does `ParticleRenderer::initialize()` succeed and create PSO / SRB / buffers?
3. Is the particle layer actually emitting particles at the current frame?
4. Does the current composition use the GPU preview path or the software fallback?

## Reference

The earlier focused investigation is here:

- [`docs/bugs/PARTICLE_BILLBOARD_NOT_RENDERING_2026-03-26.md`](x:/Dev/ArtifactStudio/docs/bugs/PARTICLE_BILLBOARD_NOT_RENDERING_2026-03-26.md)

