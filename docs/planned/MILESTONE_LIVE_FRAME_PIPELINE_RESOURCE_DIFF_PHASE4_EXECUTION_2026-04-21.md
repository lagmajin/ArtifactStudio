# Phase 4 実行メモ: Diagnostics Integration - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE4_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: UI summary hooks

対象:
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`

やること:
- pipeline / resource / diff の summary を text で見せる
- 既存 surface から読めるようにする

完了条件:
- diagnostics surface に summary が出る

## チケット 2: app entry points

対象:
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

やること:
- App Debugger から辿れる導線を作る
- 既存 dock から開けるようにする

完了条件:
- 既存 surface から開ける

