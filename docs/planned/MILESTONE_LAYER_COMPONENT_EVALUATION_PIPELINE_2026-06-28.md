# Layer Component Evaluation Pipeline

> 2026-06-28

## Goal

Cloner、Layout、Crowd、Animation Dynamics、Collision、Fracture、
Particle Emitter を、互いを直接呼び合う機能群ではなく、
固定された評価フェーズと型付き中間データで統合する。

## Evaluation order

1. `Source`
2. `Drive` — animation spring / follow-through for the source layer
3. `Generate` — Cloner
4. `Arrange` — Layout
5. `Intent` — Crowd
6. `Dynamics` — Gravity / per-instance collision
7. `Topology` — Fracture
8. `Emit` — Particle Emitter
9. `RenderExtraction`

コンポーネントの `order` は同一フェーズ内だけで有効とする。
フェーズを跨いだ自由な並べ替えは許可しない。

## Ownership

- Layer
  - component configuration
  - property / undo / serialization bridge
  - stable component identity
- Composition simulation world
  - cross-layer collision state
  - fixed timestep
  - contact and trigger events
- Core solver
  - physics / crowd / fracture / particle calculations
- Renderer
  - immutable instance / fragment / particle snapshots only

## Stable identity

仮想インスタンスは実レイヤーへ展開せず、次の組で識別する。

```text
ownerLayerId + componentId + localId + generation
```

Cloner、Crowd agent、破砕片、Particle はこの identity model を共有する。
編集が必要な場合だけ明示的に Layer または keyframe へ bake する。

## Implemented foundation

- `Artifact.Layer.Component.System`
  - stable component descriptor
  - phase / scope / order
  - dependency validation
  - unknown component preserving JSON records
  - instance / intent / contact / fracture / particle event contracts
- `ArtifactAbstractLayer`
  - built-in component descriptors
  - component graph serialization
  - phase-filtered component query
- Runtime bridge
  - Cloner generates instances
  - Layout arranges generated instances
  - Crowd adds deterministic motion intent to instances
  - source Physics applies spring / gravity drive
  - instance Collision resolves the composition floor after Crowd
  - collision impact drives Fracture
  - impact can emit GPU particle snapshots

## Constraints

- Collision floor resolution is the first deterministic collision slice.
  Inter-layer broadphase and contact manifolds remain composition-world work.
- Crowd currently operates on generated instances. It does not materialize
  hundreds of Artifact layers.
- Fracture topology changes and particle spawns occur after Dynamics.
- No component introduces a signal/slot connection. Mutations continue through
  the existing layer property and event paths.

## Next hardening gates

- composition-world AABB broadphase
- fixed-step checkpoint cache for random timeline seeks
- bake-to-keyframes and make-instances-editable commands
- Inspector validation messages for missing or incompatible components
- create/save/load/runtime smoke verification after build permission
