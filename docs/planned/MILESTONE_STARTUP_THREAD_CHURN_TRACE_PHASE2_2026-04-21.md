# M-DIAG-5 Phase 2

## Burst Correlation

- startup burst を `Trace Timeline` / `ProfilerPanelWidget` / `AppDebuggerWidget` に反映する
- `short-lived worker`, `pool burst`, `decode burst`, `scheduler burst` を分類表示する

## Target

- `Artifact/src/Widgets/Diagnostics/TraceTimelineWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

## Done When

- 「どの subsystem が何本 thread を立てて落としているか」が UI から読める
