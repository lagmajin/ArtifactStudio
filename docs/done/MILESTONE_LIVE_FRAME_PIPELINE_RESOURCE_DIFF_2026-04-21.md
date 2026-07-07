# Live Frame Pipeline / Resource Watcher / State Diff Tracker

**Date**: 2026-04-21
**Status**: Completed
**Parent**: `FramePipelineViewWidget`

Live Frame Pipeline / Resource Watcher / State Diff Tracker が実装済みのため、この milestone を完了扱いにする。  
frame snapshot と trace を同一 surface で見られ、pass DAG、resource lifetime、barrier hints、lane breakdown を追える。

## Evidence

- `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
- `docs/planned/MILESTONES_BACKLOG.md`

## Result

- pass DAG を追える
- resource lifetime を追える
- barrier hazard を追える
- frame snapshot と trace の差分文脈を同じ画面で確認できる
