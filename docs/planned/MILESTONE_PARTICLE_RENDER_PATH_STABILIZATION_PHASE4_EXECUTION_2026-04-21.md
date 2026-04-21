# Particle Render Path Stabilization Phase 4

## Diagnostics

### Ticket 1: Frame Debug Particle Summary

#### Target

- `ArtifactCore/include/Frame/FrameDebug.ixx`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

#### Planned Change

- particle draw attempt count
- skipped reason
- billboard mode
- particle count

を summary 化する

### Ticket 2: Trace Timeline Correlation

#### Target

- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `Artifact/src/Widgets/Diagnostics/TraceTimelineWidget.cppm`

#### Planned Change

- particle draw prepare / draw / skip を trace domain として見えるようにする

### Ticket 3: Pipeline View Integration

#### Target

- `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`

#### Planned Change

- particle pass がどこで走ったか、どの target を使ったかを見えるようにする
