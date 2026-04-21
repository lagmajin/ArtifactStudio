# Particle Render Path Stabilization Phase 2

## Billboard Contract Freeze

### Ticket 1: Billboard Mode Matrix Contract

#### Target

- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

#### Planned Change

- `ScreenAligned`
- `ViewPlane`

の view/proj 前提を固定する

### Ticket 2: Blend / Depth Meaning Fix

#### Target

- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`
- `Artifact/include/Layer/ArtifactParticleLayer.ixx`

#### Planned Change

- additive / alpha / depth write / depth test の扱いを明文化
- property UI 上の値と renderer 実装の意味を揃える

### Ticket 3: Particle Size Visibility Floor

#### Target

- `ArtifactCore/src/Graphics/ParticleRenderer.cppm`

#### Planned Change

- scale が小さすぎて見えないケースを避ける visibility floor を検討する
