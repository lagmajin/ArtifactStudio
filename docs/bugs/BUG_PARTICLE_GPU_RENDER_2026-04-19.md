# BUG: ArtifactParticleLayer GPU Particle Rendering Failure

**Date:** 2026-04-19  
**Status:** Fixed  
**Severity:** High — particles completely invisible in GPU path  
**Files changed:**
- `Artifact/include/Render/PrimitiveRenderer2D.ixx`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`
- `Artifact/src/Layer/ArtifactParticleLayer.cppm`

---

## Symptom

`ArtifactParticleLayer` takes the GPU path (`renderer->isInitialized() == true`) and calls
`renderer->drawParticles(renderData)`, but no particles are ever visible in the
Composition Editor or any Solo view.

---

## Root Cause Analysis

### Bug 1 — Render target unbound before particle draw (PRIMARY)

**Location:** `ArtifactIRenderer::Impl::drawParticles()`

`ArtifactCompositionRenderController` drives the per-layer render loop as:

```
renderer->setOverrideRTV(layerRTV)   // (1) store override RTV
renderer->clear()                    // (2) bind + clear layerRTV
layer->draw(renderer)                // (3) user draw code here
renderer->setOverrideRTV(nullptr)    // (4) restore default
```

Inside `clear()` the flow is:

```cpp
// primitiveRenderer_.clear() →
ctx->SetRenderTargets(1, &layerRTV, nullptr, TRANSITION);  // ← bind & clear color
ctx->ClearRenderTarget(layerRTV, ...);

// Then, if m_layerDepthTex exists (always true for the interactive renderer):
ctx->SetRenderTargets(0, nullptr, dsv, TRANSITION);        // ← clears the color RT binding!
ctx->ClearDepthStencil(dsv, ...);
```

After `clear()` returns, the Diligent device context has **no color render target bound**
(only a depth target). Ordinary `submitter_.submit()` calls re-bind the RTV per packet,
so `drawSolidRect`, `drawText`, etc. work correctly. But `drawParticles` bypasses the
command buffer and calls Diligent's device context directly — it never re-bound the
color RT, so the GPU silently discarded all rasterised fragments.

**Fix:** In `drawParticles`:
1. Flush the pending command buffer first (`submitter_.submit(cmdBuf_, ctx)`) to preserve
   ordering with preceding primitive draw calls.
2. Retrieve the current RTV from `primitiveRenderer_.currentRTV()` and call
   `ctx->SetRenderTargets(1, &pRTV, nullptr, TRANSITION)` before `particleRenderer_->prepare()`.

`currentRTV()` was added as a new public method on `PrimitiveRenderer2D` that forwards to
the existing private `Impl::getCurrentRTV()` helper (returns the override RTV or the
swapchain back-buffer).

### Bug 2 — Null-pointer crash in PSO creation (SECONDARY)

**Location:** `ArtifactCore::ParticleRenderer::createPSO()`

```cpp
pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pImpl_->pPSO_);

// CRASH if PSO creation failed (e.g. shader compile error):
pImpl_->pPSO_->GetStaticVariableByName(...)->Set(...);
```

If either shader failed to compile or PSO creation was rejected by the D3D12 runtime,
`pImpl_->pPSO_` was left null and the immediate dereference caused an access violation.
Similarly, `GetStaticVariableByName` can return null if the variable name doesn't match.

**Fix:** Added null-checks after `CreateGraphicsPipelineState` and after
`GetStaticVariableByName`. On failure, a `qWarning()` is emitted and the function
returns early — leaving `pPSO_` / `pSRB_` null. `prepare()` now guards on null
`pPSO_`/`pSRB_` and skips the draw with another warning.

### Bug 3 — No particle count guard in `drawParticles` (MINOR)

**Location:** `ArtifactIRenderer::Impl::drawParticles()`

Previously `drawParticles` would proceed through the full lazy-init path even when
`data.particles.empty()` — wasting a `submitter_.submit()` call and a `SetRenderTargets`
call for nothing.

**Fix:** Early return when `data.particles.empty()`.

---

## Debug Logging Added

To aid future diagnosis, the following `qDebug()`/`qWarning()` messages were added:

| Location | Message |
|---|---|
| `ArtifactParticleLayer::draw()` | Frame number, alive particle count, renderer state |
| `ArtifactIRenderer::drawParticles()` | Particle count, zoom, pan, viewport size; warning on no RTV |
| `ParticleRenderer::initialize()` | Logs first-time init |
| `ParticleRenderer::createPSO()` | Logs PSO success; warns on null PSO or missing cbuffer variable |
| `ParticleRenderer::prepare()` | Warns if PSO/SRB null |

---

## Secondary Risks / Outstanding Items

| Issue | Status |
|---|---|
| Particle billboard size — default `scale=1` gives a 5 px half-extent which may be too small at 100% zoom | To verify at runtime with debug output |
| Particle coordinate space — `captureRenderData()` emits positions in canvas pixels; view matrix applies pan+zoom; looks correct but needs visual confirmation | To verify |
| Additive blend mode selected in PSO — particles over a dark background will be bright, over a light background nearly invisible | Expected behaviour; user can change emitter colour |
| `GpuContext` wraps the same `IRenderDevice` / `IDeviceContext` as `deviceManager_` — no double-device issue | Confirmed |

---

## How to Verify the Fix

1. Create a new Composition.
2. Add a Particle Layer.
3. Press Play or scrub to frame > 0.
4. Open Qt's debug output (Output panel / stderr) and confirm:
   - `[ParticleLayer] <name> frame=N alive particles=M …` with M > 0
   - `[ParticleRenderer] Initialized (max 100k particles)`
   - `[ParticleRenderer] PSO created successfully`
   - `[ParticleRenderer] Drawing M particles zoom=… pan=…`
5. Particles should be visible as bright additive billboards over the canvas.
