# Form Grid Particle Layer Milestone

Trapcode Form のような「格子・面・立体点群を粒子として見せる」表現を、
ArtifactStudio では既存 `ParticleSystem` や `ClonerGenerator` に寄せすぎず、
独立した generator layer として実装する。

この文書では、既存資産との統合を最小化し、renderer の billboard / particle draw
入口だけを共有する設計を固定する。

## Goal

- Grid / Plane / Box / Layer Sample から安定した点群を生成する
- 各点を particle billboard として描画できるようにする
- time / noise / field によって点群を変形し、Form らしい動きを作る
- 既存 `ArtifactParticleLayer` の emitter / lifetime simulation とは別物として扱う
- 既存 `ClonerGenerator` の clone / effector model とは別物として扱う
- renderer は既存 particle draw path を再利用し、低レベル Diligent 実装を増やさない

## Non-Goals

- 初期段階で fluid / rigid body / collision を持つ particle simulator にすること
- `ArtifactParticleLayer` の simulation model を置き換えること
- `ClonerGenerator` / `CloneData` / clone effectors と深く統合すること
- 初手から GPU compute generation にすること
- Diligent / DX12 backend を広く改修すること
- QtCSS / `QColorDialog` / 新規 signal-slot 経路を追加すること

## Product Shape

ユーザー向け名称は `Form Particle Layer` とする。

作成導線:

- `Layer > New > Form Particle Layer`
- 追加後は通常の layer として timeline / inspector に出る
- 初期 preset は `Dot Grid`

最初に成立させる見た目:

- composition 中心に 2D grid dots
- particle size / opacity / color を inspector で変更できる
- time に応じた noise displacement
- additive / alpha blend の切り替え

## Architecture

`Form Particle Layer` は「永続点群 generator」であり、emitter ではない。
粒子は毎秒生まれて死ぬのではなく、index を持つ lattice point として存在する。

```text
ArtifactFormParticleLayer
  FormParticleSettings
  FormPointCloudCache
  FormPointCloudGenerator
  FormFieldEvaluator
      |
      v
  FormRenderPacket
      |
      v
ArtifactCore::ParticleRenderData
      |
      v
ArtifactIRenderer::drawParticles()
```

再利用するもの:

- `ArtifactIRenderer::drawParticles()`
- `ArtifactCore::ParticleRenderData`
- 既存 particle billboard renderer
- 既存 frame diagnostics vocabulary where useful

再利用しないもの:

- `ArtifactParticleLayer` の emitter / lifetime simulation
- `Artifact::ParticleSystem`
- `ArtifactCore::ParticleSystem`
- `ClonerGenerator`
- `CloneData`
- clone effector stack

## Data Model

### FormParticleSettings

Layer が保存する authoring state。

```text
FormParticleSettings
- generatorMode: Grid2D / Grid3D / LayerMap
- columns
- rows
- depth
- spacingX
- spacingY
- spacingZ
- originMode: Center / TopLeft / LayerBounds
- seed
- particleSize
- particleOpacity
- colorMode: Solid / AxisGradient / SourceColor
- solidColor
- blendMode: Alpha / Additive / ScreenLike
- billboardMode: ScreenAligned / ViewPlane
- noiseAmount
- noiseScale
- noiseSpeed
- noisePhase
- twistAmount
- sphericalFalloff
- maxParticles
```

### FormPoint

Internal generated point. This should stay independent from renderer data.

```text
FormPoint
- basePosition: float3
- position: float3
- color: float4
- size: float
- opacity: float
- index: uint32
- row: uint32
- column: uint32
- depth: uint32
- uv: float2
- random: float
```

### FormRenderPacket

Layer-local bridge object used before conversion to renderer data.

```text
FormRenderPacket
- points: vector<FormPoint>
- bounds
- frame
- timeSeconds
- settingsHash
- sourceLayerRevision
```

The final conversion step maps `FormPoint` to `ArtifactCore::ParticleRenderData`.
This keeps Form authoring independent while allowing renderer reuse.

## Responsibilities

### ArtifactFormParticleLayer

- owns `FormParticleSettings`
- owns cache lifetime
- evaluates current frame time
- asks generator / field evaluator for points
- converts points to `ParticleRenderData`
- submits draw through `ArtifactIRenderer::drawParticles()`
- serializes settings

### FormPointCloudGenerator

- creates base lattice points
- applies origin / spacing / bounds logic
- enforces `maxParticles`
- does not know about renderer APIs
- does not know about UI widgets

### FormFieldEvaluator

- applies deterministic deformation
- evaluates noise / twist / falloff against frame time
- keeps output stable for the same settings + frame
- does not mutate authoring settings

### FormPointCloudCache

- caches base point cloud separately from animated point cloud
- invalidates base cache when generator dimensions/source change
- invalidates animated cache when time or field settings change
- exposes simple cache summary for diagnostics

### Renderer

- remains a draw backend
- receives only `ParticleRenderData`
- does not learn about Form settings
- does not evaluate Form fields

## Initial Inspector Groups

Use normal property rows. Do not add QtCSS. Do not add `QColorDialog`.

- `Form`
  - Mode
  - Columns
  - Rows
  - Depth
  - Spacing X/Y/Z
  - Max Particles
- `Particle`
  - Size
  - Opacity
  - Color Mode
  - Solid Color
  - Blend Mode
- `Displace`
  - Noise Amount
  - Noise Scale
  - Noise Speed
  - Twist
  - Falloff
- `Render`
  - Billboard Mode
  - Depth Test
  - Sort Mode

## Serialization

Layer JSON should be explicit and backward tolerant.

```text
type: "formParticle"
isFormParticleLayer: true
formParticle:
  version: 1
  generatorMode
  dimensions
  spacing
  render
  field
```

Do not piggyback on `particleLayer` JSON. This avoids accidental coupling with
emitter simulation settings.

## Phases

### Phase 1: Independent Grid Layer Skeleton

- add `ArtifactFormParticleLayer`
- add settings struct and JSON save/load
- add layer factory/menu entry
- generate stable Grid2D points
- convert points to `ParticleRenderData`
- draw through existing renderer entry

Done:

- a new Form Particle Layer can be created
- it displays a static dot grid
- no `ParticleSystem` / `ClonerGenerator` dependency is introduced

### Phase 2: Inspector Controls

- expose `Form`, `Particle`, `Render` groups
- support size, opacity, color, columns, rows, spacing
- keep color selection on approved picker path
- avoid new global signal-slot wiring

Done:

- common look controls can be edited from Inspector
- project reload preserves settings

### Phase 3: Deterministic Fields

- add noise displacement
- add twist deformation
- add spherical falloff
- make animation depend on frame/time deterministically
- cache base grid separately from animated result

Done:

- scrubbing gives stable repeatable motion
- changing field settings invalidates only the needed cache

### Phase 4: Grid3D and Camera-Aware Draw

- add depth dimension
- map Grid3D points into particle render data
- respect billboard mode / depth test / sort mode
- keep renderer contract unchanged

Done:

- 3D point volumes are visible through the existing particle renderer
- no Diligent low-level expansion is needed

### Phase 5: LayerMap Sampling

- sample a source layer or source image into a point cloud
- use alpha/luma threshold to enable/disable points
- optionally inherit source color
- keep source sampling behind an explicit function boundary
- do not introduce implicit `QImage` conversion in hot path

Done:

- text/image-like point clouds can be formed
- source sampling is explicit and auditable

### Phase 6: Diagnostics and Presets

- add concise diagnostics for point count / cache state / renderer skipped reason
- add presets:
  - Dot Grid
  - Star Volume
  - Digital Sand
  - Wave Matrix
  - Source Pixels

Done:

- failed draw can be diagnosed as generation/cache/renderer
- users can start from useful looks

## Implementation Guardrails

- Prefer `.cppm` implementation changes over `.ixx` surface changes where possible
- Add new module files only with explicit CMake registration
- Keep `#include` in global module fragments only
- Do not add includes after `module X;`
- Do not self-import a module
- Keep Qt includes local and explicit
- Do not add `QImage` to the Form hot path
- Do not add `QColorDialog`
- Do not add QtCSS
- Do not add new global signal-slot routes
- Do not edit submodules for this feature
- Do not run build / test / CMake unless explicitly requested

## Open Questions

- Should `Form Particle Layer` live under `Artifact/src/Layer` or a new generator-layer subfolder?
- Should the first implementation use only screen-aligned billboards, or expose view-plane mode immediately?
- Should source layer sampling be limited to still snapshots in Phase 5, with animated source sampling later?
- Should presets be plain JSON snippets or C++ defaults first?

## Recommended First Slice

Build only Phase 1 + a minimal part of Phase 2:

1. create `ArtifactFormParticleLayer`
2. create `FormParticleSettings`
3. generate Grid2D points
4. convert to `ParticleRenderData`
5. add menu/factory creation
6. expose only Columns / Rows / Spacing / Size / Color

This produces visible value without coupling the new system to existing particle
or cloner internals.
