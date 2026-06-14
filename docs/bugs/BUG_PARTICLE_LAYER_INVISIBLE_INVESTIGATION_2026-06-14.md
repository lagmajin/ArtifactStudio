# Particle Layer Invisible Investigation Report

**Date:** 2026-06-14  
**Issue:** Particle layers not visible in composition editor

## Architecture Overview

### Two ParticleSystem Implementations

There are **two separate ParticleSystem implementations** in the codebase:

| Implementation | Location | Usage |
|---------------|----------|-------|
| `ArtifactCore::ParticleSystem` | `ArtifactCore/src/Particle/ParticleSystem.cppm` | Used by core simulation |
| `Artifact::ParticleSystem` | `Artifact/src/Generator/ArtifactParticleGenerator.cppm` | Qt-integrated version used by `ArtifactParticleLayer` |

The `ArtifactParticleLayer` (in `Artifact/src/Layer/ArtifactParticleLayer.cppm`) uses the **Qt-integrated `Artifact::ParticleSystem`** which has:
- `goToFrame(int64_t frame, double fps)` - deterministic simulation to target frame
- `captureRenderData()` - captures particles for rendering
- `emitterCount()` - returns number of emitters

---

## Code Flow Analysis

### 1. Frame Update → goToFrame

**Location:** `ArtifactParticleLayer::draw()` lines 210-217

```
draw() calls goToFrame(frameNumber, fps)
  └─> impl_->particleSystem->goToFrame(std::max(int64_t{1}, frameNumber), fps)
```

**Location:** `Artifact::ParticleSystem::goToFrame()` lines 796-815

```cpp
void ParticleSystem::goToFrame(int64_t frame, double fps)
{
    reset();  // <-- CRITICAL: Clears ALL particles
    if (frame < 0 || fps <= 0.0) return;
    
    const double targetTime = static_cast<double>(frame) / fps;  // Frame 1 at 30fps = 33ms
    const float stepSize = 1.0f / 120.0f;  // 8.33ms steps
    
    float currentTime = 0.0f;
    while (currentTime < targetTime) {
        float dt = std::min(stepSize, static_cast<float>(targetTime - currentTime));
        for (auto& emitter : emitters_) {
            emitter->update(dt);
        }
        currentTime += dt;
    }
}
```

### 2. Particle Emission Logic

**Location:** `ParticleEmitter::simulateStep()` lines 602-626

```cpp
case EmissionMode::Continuous: {
    emitAccumulator_ += deltaTime * params_.rate;
    int toEmit = static_cast<int>(emitAccumulator_);
    if (toEmit > 0) {
        emitParticles(toEmit);
        emitAccumulator_ -= toEmit;
    }
    break;
}
```

**Problem:** With `rate = 50` particles/sec, `deltaTime = 8.33ms`:
- Per step emission: `8.33ms × 50/sec = 0.417 particles per step`
- Over 4 steps (to reach 33ms): `~1.67 particles total`
- Only 1 particle emitted (integer truncation), may not meet threshold

### 3. GPU Rendering Path

**Location:** `ArtifactParticleLayer::draw()` lines 221-231

```cpp
if (rendererReady) {
    const auto sourceData = impl_->particleSystem->captureRenderData();
    qInfo() << "[ParticleLayer] GPU path: particleCount=" << sourceData.particles.size();
    if (!sourceData.particles.empty()) {
        renderer->drawParticles(renderData);
    } else {
        qWarning() << "[ParticleLayer] GPU path: NO PARTICLES - emitter may not generate";
    }
    return;
}
```

**Location:** `ArtifactIRenderer::drawParticles()` lines 752-785

```cpp
auto* pRTV = primitiveRenderer_.currentRTV();  // Gets m_overrideRTV or swapchain RTV
if (!pRTV) {
    qWarning() << "[ParticleRenderer] No active RTV — skipping particle draw";
    return;
}
// ... sets cmdBuf_.targetRTV = pRTV and appends ParticlePkt
```

### 4. Composition View Render Flow

**Location:** `ArtifactCompositionRenderController.cppm` lines 7604-7640

```cpp
renderer_->setOverrideRTV(layerRTV);  // Sets m_overrideRTV
if (layer->isAdjustmentLayer()) {
    renderer_->drawSprite(...);
} else {
    renderer_->clear();
}
drawLayerForCompositionView(layer.get(), ...);  // Calls layer->draw(renderer)
renderer_->flush();  // Submits cmdBuf_ to GPU
renderer_->setOverrideRTV(nullptr);  // Clears m_overrideRTV
renderer_->unbindColorTargetsForCompute();
// ... blend uses layerSRV from m_layerRT
```

---

## Identified Issues

### Issue 1: Particle Emission Threshold Too High

**Symptom:** No particles generated on frame 1
**Root Cause:** `goToFrame(1, fps)` simulates only 33ms (at 30fps)
**Mechanism:**
- `emitAccumulator` starts at 0
- Each step adds ~0.4 particles
- Integer truncation (`int toEmit = static_cast<int>(emitAccumulator_)`) means only 0 or 1 particle emitted
- May result in 1-2 particles total for frame 1, which might be killed immediately

**Evidence in code:** Line 229 shows warning: `[ParticleLayer] GPU path: NO PARTICLES - emitter may not generate`

### Issue 2: RTV Context Mismatch

**Symptom:** Particles drawn to wrong target or not drawn at all
**Root Cause:** `drawParticles()` gets RTV from `primitiveRenderer_.currentRTV()` but this may not match the layer RTV context

**Mechanism:**
- `setOverrideRTV(layerRTV)` sets `m_overrideRTV` on PrimitiveRenderer2D
- `drawParticles()` uses `primitiveRenderer_.currentRTV()` 
- This correctly returns `m_overrideRTV` when set (line 203 in PrimitiveRenderer2D.cppm)
- However, the blend pipeline reads from `m_layerRT` which is cleared BEFORE the layer draw

### Issue 3: Blend Pipeline Uses layerRTV, Not Swapchain

When GPU pipeline path is active:
1. Background drawn to `layerRTV` (m_layerRT) - OK
2. Layer cleared to `layerRTV` - OK  
3. **Particle layer draw**: `drawParticles()` sets `cmdBuf_.targetRTV = pRTV` where `pRTV = primitiveRenderer_.currentRTV()`
4. `flush()` submits draws to `layerRTV` - OK
5. Blend reads from `layerSRV` (m_layerRT) - Should work

**Potential Problem:** If `m_overrideRTV` is set to `layerRTV` but `cmdBuf_.targetRTV` still points to swapchain RTV (race condition), particles render to wrong target.

---

## Debug Logging Added

The following debug logging was added to `ArtifactParticleLayer::draw()` (lines 194-268):

```cpp
qInfo() << "[ParticleLayer] draw() frame=" << frameNumber
        << "rendererInitialized=" << rendererReady
        << "emitters=" << emitterCount;

// After goToFrame:
qInfo() << "[ParticleLayer] GPU path: particleCount=" << sourceData.particles.size();

// If no particles:
qWarning() << "[ParticleLayer] GPU path: NO PARTICLES - emitter may not generate";

// Fallback path:
qInfo() << "[ParticleLayer] Fallback path: cachedFrame=" << impl_->cachedFrameNumber
        << "currentFrame=" << frameNumber
        << "cachedNull=" << impl_->cachedFrame.isNull();
```

Additional logging exists in:
- `ArtifactIRenderer::drawParticles()` - checks for RTV availability
- `DiligentImmediateSubmitter::submitParticles()` - logs skip reasons for particle draw

---

## Recommended Fixes

### Fix 1: Ensure Minimum Particles on Frame 1

Change `goToFrame` to ensure particles are generated even for short simulations:

```cpp
// In ArtifactParticleLayer::draw(), ensure at least 1 second of simulation
impl_->particleSystem->goToFrame(std::max(int64_t{30}, frameNumber), fps);
// OR pre-warm before frame 1
if (frameNumber <= 1) impl_->particleSystem->preWarm(0.5f);  // Warm 0.5 seconds
```

### Fix 2: Use Layer RTV Directly in drawParticles

Instead of relying on `primitiveRenderer_.currentRTV()`:

```cpp
// Add overload: drawParticles(renderData, explicitRTV)
void ArtifactIRenderer::drawParticles(const ArtifactCore::ParticleRenderData& data, ITextureView* explicitRTV)
{
    // ... same logic but use explicitRTV instead of currentRTV()
}
```

### Fix 3: Force Particle Emission on First Frame

In `ParticleEmitter::simulateStep()`, ensure minimum emission:

```cpp
case EmissionMode::Continuous: {
    emitAccumulator_ += deltaTime * params_.rate;
    int toEmit = static_cast<int>(emitAccumulator_);
    // Emit at least 1 particle on first update if rate > 0
    if (toEmit >= 0 && particles_.empty() && params_.rate > 0) {
        toEmit = std::max(1, toEmit);
    }
    if (toEmit > 0) {
        emitParticles(toEmit);
        emitAccumulator_ -= toEmit;
    }
    break;
}
```

---

## Files Involved

| File | Lines | Purpose |
|------|-------|---------|
| `Artifact/src/Layer/ArtifactParticleLayer.cppm` | 194-268 | Layer draw implementation |
| `Artifact/src/Generator/ArtifactParticleGenerator.cppm` | 602-688 | ParticleSystem goToFrame/simulateStep |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 688-786 | drawParticles implementation |
| `Artifact/src/Render/DiligentImmediateSubmitter.cppm` | 677-716 | submitParticles with logging |
| `Artifact/src/Render/PrimitiveRenderer2D.cppm` | 202-208 | currentRTV() logic |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 7577-7640 | Layer render loop with RTV handling |

---

## Test Steps

1. Build application and run with debug console
2. Create new composition with particle layer
3. Add particle layer to timeline at frame 1
4. Observe debug output:
   - `[ParticleLayer] draw() frame=1 ...`
   - `[ParticleLayer] GPU path: particleCount=X` (should be > 0)
   - If `X=0`, check emission rate and simulation time
5. Check for `[ParticleRenderer] No active RTV` warning