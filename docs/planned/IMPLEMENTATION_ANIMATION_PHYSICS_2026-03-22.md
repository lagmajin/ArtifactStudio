# Animation-Style Physics for Layer Transforms — Implementation Proposal

**Date:** 2026-03-22
**Status:** Proposal
**Author:** AI-assisted analysis

## Goal

Transform ArtifactStudio's layer transform system from direct keyframe interpolation to **animation-style physics** — enabling Follow Through, Overlapping Action, Secondary Motion, and cartoon-style exaggeration on layer transforms. Think DUIK/Newton/AE expressions, not rigid body simulation.

## Motivation

アニメーションソフトウェアとして、リアルな物理シミュレーションよりも「アニメっぽい物理」が重要:
- キャラクターが止まった後、髪や服がオーバーシュートしてから収束
- 親オブジェクトの動きが子に遅れて伝播（Follow Through）
- 異なるチャンネル（位置/回転/スケール）が異なるタイミングで動く（Overlapping Action）
- 弾みのある反発（Bounce）、ゴムのように伸び縮み（Elastic）
- MoGraphクローンの波打ち運動

## Current State Analysis

### What exists
- `AnimatableValueT<float>::at()` — keyframe evaluation, but **only linear interpolation** regardless of `InterpolationType`
- `AnimatableTransform3D` — 9 channels (x, y, z, rotation, scaleX, scaleY, anchorX, anchorY, anchorZ) with `initial + offset` composition
- `InterpolationType` enum — 30+ types including `Spring`, `SmoothDamp`, `Elastic*`, `Bounce*`, `Back*` — **all unimplemented** (enum only, no evaluation logic)
- `ArtifactAbstractLayer::getGlobalTransform()` — parent-child hierarchy via `parentLayerId_`
- `goToFrame(int64_t)` — per-frame update, no delta-time, no velocity tracking
- `Physics2D` (Box2D) — distance joints with damping/stiffness exist but are separate rigid body system
- `ParticleSystem` — force fields (gravity, wind, vortex, attractor, drag, turbulence) but no transform-level spring

### What's missing (the gaps)
1. Interpolation types are enumerated but not evaluated
2. No velocity/spring state on transform channels
3. No delta-time in update loop
4. No follow-through / overlapping motion mechanism
5. No staggered motion for clones
6. No squash & stretch on layer transforms
7. No smear frame support

### Key Code Locations
| Component | File | Line |
|-----------|------|------|
| `InterpolationType` enum | `ArtifactCore/include/Geometry/Interpolate.ixx` | 46-101 |
| `AnimatableValueT::at()` | `ArtifactCore/include/Animation/AnimatableValue.ixx` | 97-136 |
| `AnimatableTransform3D::Impl` | `ArtifactCore/src/Animation/AnimatableTransform3D.cppm` | 49-79 |
| `AnimatableTransform3D::getAllMatrixAt()` | `ArtifactCore/src/Animation/AnimatableTransform3D.cppm` | 374-414 |
| Layer parent-child | `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | 307-404 |
| Composition update loop | `Artifact/src/Composition/ArtifactAbstractComposition.cppm` | goToFrame |
| CloneData / Effectors | `Artifact/include/Effets/Clone/CloneCore.ixx` | - |

---

## Implementation Phases

### Phase 1: Interpolation Function Library
**Files:** `ArtifactCore/include/Geometry/Interpolate.ixx`

Add concrete implementations for the physics-based interpolation types that are already enumerated:

```cpp
// Standard easing functions (t in [0,1] -> [0,1])
float easeInOut(float t);          // smooth acceleration/deceleration
float easeInQuad(float t);         // t^2
float easeOutQuad(float t);        // 1-(1-t)^2
float easeInCubic(float t);        // t^3
float easeOutCubic(float t);       // 1-(1-t)^3
float easeInQuint(float t);        // t^5
float easeOutQuint(float t);       // 1-(1-t)^5

// Bounce (classic bouncing ball)
float bounceOut(float t);
float bounceIn(float t);
float bounceInOut(float t);

// Elastic (rubber band snap)
float elasticOut(float t, float amplitude = 1.0f, float period = 0.3f);
float elasticIn(float t, float amplitude = 1.0f, float period = 0.3f);
float elasticInOut(float t, float amplitude = 1.0f, float period = 0.3f);

// Back (overshoot + settle)
float backOut(float t, float overshoot = 1.70158f);
float backIn(float t, float overshoot = 1.70158f);
float backInOut(float t, float overshoot = 1.70158f);

// Sigmoid
float sigmoid(float t);            // S-curve

// Dispatcher
float evaluateEasing(InterpolationType type, float t, float param = 0.0f);
```

**Acceptance criteria:** `evaluateEasing(InterpolationType::BounceOut, 0.8f)` returns a bouncing value. All enum types have a working implementation.

---

### Phase 2: Stateful Spring-Damper on AnimatableValueT
**Files:** `ArtifactCore/include/Animation/AnimatableValue.ixx`

Current `at()` does: `mix(prev, next, t)` — pure interpolation, no state.

Add a **spring evaluation mode** where the value follows a spring-damper toward the keyframe target:

```cpp
// New struct for spring state (not persisted, runtime only)
struct SpringState {
    float velocity = 0.0f;
    float stiffness = 120.0f;   // k: spring constant (higher = snappier)
    float damping = 12.0f;      // c: damping coefficient (higher = less oscillation)
    float mass = 1.0f;          // m: inertia
    float currentValue = 0.0f;  // current spring position
};

// New method on AnimatableValueT
T atSpring(const FramePosition& frame, float dt, SpringState& state) const;
```

Integration (semi-implicit Euler):
```
target = mix(prev, next, t)           // what keyframes say
force = -stiffness * (current - target) - damping * velocity
velocity += (force / mass) * dt
current += velocity * dt
```

The keyframe defines the "target path", the spring defines how the value chases it.
- High stiffness + low damping = tight snap with oscillation (Elastic feel)
- Low stiffness + low damping = floaty overshoot (Bouncy feel)
- High stiffness + high damping = smooth no-overshoot (Smooth feel)

**Acceptance criteria:** A channel with spring state follows keyframes with visible overshoot/oscillation when underdamped.

---

### Phase 3: Spring Physics on AnimatableTransform3D
**Files:**
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `ArtifactCore/src/Animation/AnimatableTransform3D.cppm`

Add per-channel spring configuration and runtime state:

```cpp
// New enum
enum class TransformChannel { PositionX, PositionY, Rotation, ScaleX, ScaleY };

// In Impl:
struct ChannelSpringConfig {
    bool enabled = false;
    float stiffness = 120.0f;
    float damping = 12.0f;
    float mass = 1.0f;
};

ChannelSpringConfig springX_, springY_, springRot_, springScaleX_, springScaleY_;
float springVelX_ = 0, springVelY_ = 0, springVelRot_ = 0;
float springVelScaleX_ = 0, springVelScaleY_ = 0;
float springCurrentX_, springCurrentY_, springCurrentRot_;
float springCurrentScaleX_, springCurrentScaleY_;

// New public API:
void setSpringEnabled(bool enabled);
void setSpringConfig(float stiffness, float damping, float mass);
void setSpringConfigPerChannel(TransformChannel ch, float stiffness, float damping, float mass);
void updateSpring(float dt, int64_t currentFrame);
void resetSpringState();
float2 getSpringVelocity() const;  // for follow-through cascade
```

`updateSpring(dt, frame)` flow:
1. Read keyframe target: `targetX = x_.at(frame) + initialX_`
2. Per channel: spring-damper integration
3. Write to `currentX_` etc. (which `getMatrix()` already reads)

**Existing rendering code works unchanged** — only the `currentX_` values become spring-smoothed.

**Acceptance criteria:** Setting spring config on a layer, then playing the timeline, shows overshoot and settling on position/rotation.

---

### Phase 4: Update Loop Integration
**Files:**
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`

Current flow:
```
goToFrame(frame) -> sets currentFrame_ -> getMatrixAt(time) reads keyframes directly
```

New flow:
```
goToFrame(frame) -> sets currentFrame_
updatePhysics(dt) -> for each layer with spring enabled, layer->updateSpring(dt)
render -> getMatrix() reads spring-smoothed currentX_ etc.
```

Add to `ArtifactAbstractLayer`:
```cpp
void updatePhysics(float dt) {
    impl_->transform3D_.updateSpring(dt, impl_->currentFrame_);
}
```

Add to composition update loop:
```cpp
void update(float dt) {
    for (auto& layer : layers) {
        layer->updatePhysics(dt);
    }
}
```

dt = `1.0 / frameRate` from the existing playback controller tick.

**Acceptance criteria:** During playback, spring-enabled layers show smooth follow-through motion.

---

### Phase 5: Follow-Through / Overlapping Action (Cascade)
**Files:**
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

Follow-through = parent stops, child keeps moving then settles.
Overlapping Action = different channels have different spring parameters.

Implementation:
- **Per-channel independent springs** (Phase 3 handles this)
  - Position: stiff (fast settle)
  - Rotation: loose (exaggerated overshoot)
  - Scale: medium
- **Parent velocity injection for children:**
  ```cpp
  // In ArtifactAbstractLayer::updatePhysics():
  if (hasParent()) {
      auto parent = parentLayer();
      if (parent) {
          auto parentVel = parent->getSpringVelocity();
          transform3D_.injectVelocity(parentVel.x * followThroughGain_,
                                      parentVel.y * followThroughGain_);
      }
  }
  ```
- New layer property `followThroughGain_` (0.0 = none, 1.0 = full inheritance)

**Acceptance criteria:** A Null Layer parent that moves and stops causes child layers to overshoot and settle.

---

### Phase 6: Clone Stagger (MoGraph Integration)
**Files:**
- `Artifact/include/Effets/Clone/BasicEffectors.ixx`
- New: `Artifact/include/Effets/Clone/SpringEffector.ixx`

New effector that applies staggered spring delay per clone:

```cpp
class SpringStaggerEffector : public AbstractCloneEffector {
    float baseDelay = 0.04f;    // seconds per clone index
    float stiffness = 100.0f;
    float damping = 10.0f;

    void apply(std::vector<CloneData>& clones, float globalTime) override {
        for (auto& clone : clones) {
            float localTime = globalTime - clone.index * baseDelay;
            // Evaluate original transform at localTime
            // Apply spring-damper toward that target
            // Write back to clone.transform
        }
    }
};
```

Creates the classic "wave" / "chain" animation effect.

---

### Phase 7: Presets & UI
**Files:**
- New: `ArtifactCore/include/Physics/AnimationPhysicsPresets.ixx`
- `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx`

Preset configurations:

| Preset | Stiffness | Damping | Mass | Feel |
|--------|-----------|---------|------|------|
| `Rigid` | 999 | 100 | 1 | No spring (current behavior) |
| `Smooth` | 80 | 15 | 1 | Gentle follow |
| `Bouncy` | 60 | 5 | 1 | Exaggerated overshoot |
| `Jelly` | 40 | 3 | 0.5 | Squishy, slow settle |
| `Elastic` | 200 | 8 | 1 | Tight snap with oscillation |
| `Heavy` | 100 | 20 | 3 | Slow, weighted motion |
| `Floaty` | 30 | 8 | 0.5 | Light, airy drift |

UI additions:
- "Apply Spring Physics" -> opens spring config dialog
- Preset selector dropdown in animation menu
- Per-channel stiffness/damping sliders in property panel

---

## File Change Summary

| File | Change | Phase |
|------|--------|-------|
| `ArtifactCore/include/Geometry/Interpolate.ixx` | Add `evaluateEasing()` for all InterpolationType values | 1 |
| `ArtifactCore/include/Animation/AnimatableValue.ixx` | Add `SpringState`, `atSpring()`, velocity tracking | 2 |
| `ArtifactCore/include/Animation/AnimatableTransform3D.ixx` | Add spring API: setSpringEnabled, updateSpring, setSpringConfig | 3 |
| `ArtifactCore/src/Animation/AnimatableTransform3D.cppm` | Implement spring integration per channel | 3 |
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | Add updatePhysics(dt), parent velocity injection | 4, 5 |
| `Artifact/src/Composition/ArtifactAbstractComposition.cppm` | Add physics update pass before render | 4 |
| `Artifact/include/Effets/Clone/SpringEffector.ixx` (new) | Staggered spring effector for clones | 6 |
| `ArtifactCore/include/Physics/AnimationPhysicsPresets.ixx` (new) | Preset configurations | 7 |
| `Artifact/include/Widgets/Menu/ArtifactAnimationMenu.ixx` | Spring physics menu items | 7 |

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Spring state breaks timeline scrubbing | `resetSpringState()` on seek; spring only active during playback |
| Performance with many spring layers | Per-channel spring is O(1), negligible cost for 100+ layers |
| Box2D interaction conflict | Spring transforms are separate from Physics2D; different abstraction levels |
| Parent-child cycle spring explosion | Semi-implicit Euler + velocity clamping; existing cycle detection prevents hierarchy loops |
| Numerical instability at high stiffness | Semi-implicit Euler is unconditionally stable for damping-dominant case; clamp velocity as safety net |

## Non-Goals (for now)

- 3D rigid body spring physics (this is 2D layer animation)
- GPU spring compute (CPU per-channel is fast enough for typical layer counts)
- Replacing Box2D (Box2D stays for rigid body use cases, spring is for keyframe animation enhancement)
- Smear frame rendering (requires post-process shader work, separate milestone)
