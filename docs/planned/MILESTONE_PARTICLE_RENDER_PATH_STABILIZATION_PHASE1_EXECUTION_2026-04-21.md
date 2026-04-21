# Particle Render Path Stabilization Phase 1

## Draw Entry Audit

### Ticket 1: `drawParticles()` Preconditions

#### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`

#### Planned Change

- pending submit flush
- active RTV check
- viewport validity check
- matrix upload
- PSO / SRB readiness

を helper 単位に切り出す

### Ticket 2: Layer-side Emit / Draw Contract

#### Target

- `Artifact/src/Layer/ArtifactParticleLayer.cppm`

#### Planned Change

- layer 側は
  - alive particle count
  - billboard mode
  - render mode
  - source transform
  の構築に集中させる

### Ticket 3: Failure Summary

#### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/include/Diagnostics/Trace.ixx`

#### Planned Change

- `no RTV`
- `empty particle buffer`
- `PSO null`
- `SRB null`
- `invalid matrix`

を summary / trace に出す
