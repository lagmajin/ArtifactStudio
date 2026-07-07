# Debug Render Harness

**Date**: 2026-04-30
**Status**: Completed
**Parent**: `DebugRenderHarnessWidget` / `AppDebuggerWidget`

Debug Render Harness が既に regression surface として機能しているため、この milestone を完了扱いにする。  
particle-only / video-only / blend-only / overlay-only / mixed-media の最小再現 preset があり、App Debugger と同じ frame snapshot vocabulary を参照できる。

## Evidence

- `docs/planned/MILESTONES_BACKLOG.md`
- `Artifact/src/Widgets/Diagnostics/DebugRenderHarnessWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`

## Result

- 最小再現 preset が揃っている
- AppMain から独立 dock として開ける
- AppDebuggerWidget と報告語彙を共有できる
