# Group Layer Render Design

## Goal
Group layers should composite their child layers into a single renderable unit so group-level blend modes, opacity, masks, and effects can be applied atomically. The composition must preserve transforms, masks, and avoid alpha edge-bleed (reuse the same accum strategy where appropriate).

## Requirements
- Preserve children's global transforms (including parent-child transforms).
- Support group-level opacity and blend mode as a single composite layer.
- Support masks and clipping attached to the group.
- Avoid 1–2px bilinear edge bleeding at composition boundaries.
- Reasonable performance; introduce caching where helpful.

## Options

### Option A — Simple per-frame temporary RT (Recommended to start)
1. At group draw time, query composition-sized target (use renderer->layerRenderTargetView() or underlying texture desc).
2. Create an ephemeral texture (RTV + SRV) with same size/format via renderer->device() or TextureManager.
3. Set renderer->setOverrideRTV(tempRTV) and clear to transparent.
4. Draw all child layers into tempRTV (children already respect global transforms if parent ID is set).
5. Restore renderer->setOverrideRTV(originalLayerRTV) and blit tempSRV to original layer target (drawSpriteTransformed with identity matrix) so the controller's blend pipeline consumes a single-layer image.
6. If group has a mask, apply it using renderer->drawMaskedTextureLocal or by drawing the mask into a stencil/alpha and sampling it when blitting.
7. Free or cache the temp texture. For perf, cache textures keyed by (groupID,width,height,format) and reuse across frames until invalidated (children changed, size changed, or property changed).

Pros: Minimal central refactor, clear semantics. Cons: texture allocation cost if not cached.

### Option B — Reuse RenderPipeline / central pool (Longer-term)
- Extend RenderPipeline (or TextureManager) to provide per-group temporary bundles, reuse accum/temp textures and allow nested composition without per-group allocations.
- More efficient but requires refactoring render-pipeline ownership and lifetime.

## Edge cases
- Nested groups: when a group contains groups, render recursion works naturally using the same approach; pay attention to caching and resource lifetime.
- 3D layers or special render paths: fall back to existing behavior (draw children directly) if offscreen path is unsupported.
- Masks with GPU-only representations: ensure mask SRV is available to the blit shader or use drawMaskedTextureLocal.

## Tests
- Nested transforms with rotation/scale/translate; verify group-level transform correctness.
- Group opacity and blend mode tests vs. per-child blending.
- Masked group tests.
- Edge-bleed tests (zoomed-out composition boundaries).

## Implementation steps
1. Implement Option A minimal flow in Artifact/src/Layer/ArtifactGroupLayer.cppm::draw().
2. Add caching mechanism (TextureManager based) for temp RTs.
3. Add unit/visual tests in tests/ArtifactTestLayerGroup and a runtime QA scenario.
4. Measure and, if necessary, refactor to use RenderPipeline pool.

## Example sketch (pseudocode)

```cpp
// inside ArtifactGroupLayer::draw(renderer)
if (!needsOffscreenComposite()) {
  // current behavior: draw children directly
  for (auto &child : children) child->draw(renderer);
  return;
}

// 1. obtain size/format from renderer or composition
auto* origLayerRTV = renderer->layerRenderTargetView();
auto* device = renderer->device();
auto texDesc = createTexDescFrom(origLayerRTV);
auto tempTex = device->CreateTexture(texDesc);
auto tempRTV = tempTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
auto tempSRV = tempTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

// 2. draw children into temp
renderer->setOverrideRTV(tempRTV);
renderer->clear();
for (auto &child : children) child->draw(renderer);
renderer->setOverrideRTV(nullptr);

// 3. draw temp into orig layer target
renderer->setOverrideRTV(origLayerRTV);
QMatrix4x4 identity; identity.setToIdentity();
renderer->drawSpriteTransformed(0,0,canvasW,canvasH, identity, tempSRV, 1.0f);
renderer->setOverrideRTV(nullptr);
```

---

This document proposes starting with Option A. After approval I'll implement the code sketch, add caching, and create visual tests. If approved, a PR will be prepared with unit tests and docs.

## Implementation status

- Prototype implemented in Artifact/src/Layer/ArtifactGroupLayer.cppm using Option A (offscreen RT -> blit). A per-group temporary RT is cached and reused when size/format match; cache is invalidated on child/property changes.
- Next work: integrate TextureManager/RenderPipeline pooling for cross-group reuse, add mask support for group blits, and add visual/unit tests under tests/.
- Note: changes exist in the working tree; run a full build and tests after committing to verify behavior and performance.

