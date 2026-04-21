# Particle Render Path Stabilization Phase 3

## Layer / Renderer Split

### Ticket 1: Render Data Builder

#### Target

- `Artifact/src/Layer/ArtifactParticleLayer.cppm`

#### Planned Change

- render data 構築関数を draw から分離
- simulation / emission と render packet 構築を切り分ける

### Ticket 2: Particle Pass Context

#### Target

- `Artifact/src/Render/ArtifactIRenderer.cppm`

#### Planned Change

- particle 専用の internal context helper を作る
- target bind / matrix / prepare / draw をまとめる

### Ticket 3: Preview / Composition Parity

#### Target

- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`

#### Planned Change

- preview と composition で particle path の前提条件がずれないようにする
