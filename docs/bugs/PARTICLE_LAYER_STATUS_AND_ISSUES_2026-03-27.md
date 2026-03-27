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

### 1. Render settings are serialized and editable, but not fully consumed by the GPU path

The particle layer exposes and saves:

- `blendMode`
- `billboardMode`
- `sortMode`
- `depthTest`
- `depthWrite`

Those values are visible in the property UI and are written to JSON, but `ArtifactCore::ParticleRenderer` still hardcodes the billboard draw path and additive blending in its PSO setup.

In other words:

- the data model is richer than the GPU renderer
- the GPU renderer still behaves like a simplified billboard path
- UI changes may not yet produce the expected render-style variation

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

