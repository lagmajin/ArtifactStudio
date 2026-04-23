# M-DIAG-5 Phase 1 Execution

## Ticket 1: Trace Domain Tagging

### Target

- `ArtifactCore/include/Diagnostics/Trace.ixx`

### Planned Change

- `TraceEventRecord` に `domain` / `phase` / `origin` を持たせる
- startup / init / lazy-load を区別できる enum か文字列を固定する

## Ticket 2: Worker Lifecycle Hook

### Target

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- `Artifact/src/Render/ArtifactRenderScheduler.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

### Planned Change

- worker start / finish の trace record を入れる
- domain ごとに `Render`, `Decode`, `Asset`, `Playback` へ振る

## Ticket 3: Trace Timeline Startup Lane

### Target

- `Artifact/src/Widgets/Diagnostics/TraceTimelineWidget.cppm`

### Planned Change

- startup domain を lane か色で区別する
- short-lived thread を見つけやすい強調を入れる
