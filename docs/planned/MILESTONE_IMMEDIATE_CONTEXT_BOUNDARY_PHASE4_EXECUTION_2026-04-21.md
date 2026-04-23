# M-IR-8 Phase 4 Execution

renderer boundary を詰めたあと、`Frame Debug` / `Pipeline View` / `Trace` と衝突しない形で diagnostics を維持するための実行メモ。

## Ticket 1: Renderer Summary Surface

### Target

- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/include/Frame/FrameDebug.ixx`

### Planned Summary

- backend kind
- active RTV / DSV summary
- offscreen target stack depth
- last submit packet counts
- viewport / canvas / zoom / pan
- readback availability

### Reason

`Frame Debug View` や `Pipeline View` が low-level context を直接読まなくても、
renderer の状態を追えるようにするため。

## Ticket 2: Frame Debug Compatibility

### Target

- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### Planned Change

- `FrameDebugSnapshot` に renderer summary を追加
- target bind / blend pass / offscreen path の概況を summary として保持

### Done When

- `FrameDebugViewWidget` が direct context なしで renderer 状態を読める

## Ticket 3: Pipeline / Resource View Compatibility

### Target

- `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameResourceInspectorWidget.cppm`

### Planned Change

- pass DAG / resource lifetime / barrier hint は summary ベースで読む
- readback が必要なときだけ renderer helper を通る

### Done When

- inspector 系 widget が `IDeviceContext` を知らないまま成立する

## Ticket 4: Trace / Thread Churn Correlation

### Target

- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `Artifact/src/Widgets/Diagnostics/TraceTimelineWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

### Planned Change

- startup / init の worker churn を lane として読めるようにする
- `sharedBackgroundThreadPool()` / decode futures / render scheduler / playback thread を trace domain で分ける

### Reason

初期化時に大量の `code 0` thread exit が出る現象を、
「正常終了した thread が多い」で終わらせず、どこで burst しているかを見えるようにするため。

## Ticket 5: Documentation / Guardrail Update

### Target

- `docs/WIDGET_MAP.md`
- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`

### Planned Change

- diagnostics 面が `ImmediateContext` を前提にしないことを明記
- renderer summary API を参照境界として追記

## Done Criteria

- diagnostics 面が renderer summary / helper API で成立する
- thread churn の観測が `Trace Timeline` と結びつく
- `ImmediateContext` は backend 内部責務として残しつつ、debug 面からは直接触れない
