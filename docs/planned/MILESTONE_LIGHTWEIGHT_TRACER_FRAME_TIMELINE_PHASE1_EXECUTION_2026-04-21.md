# Phase 1 実行メモ: Crash Stack Capture - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE1_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: `Trace.Crash`

対象候補:
- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `ArtifactCore/include/Diagnostics/TraceCrash.ixx`
- `ArtifactCore/include/Diagnostics/SessionLedger.ixx`

やること:
- crash stack / crash report のデータ構造を定義する
- frame / thread / last scope を持てるようにする
- minidump ではなくまず軽量 report を作る

完了条件:
- crash report 型が読める
- crash stack を保存できる

## チケット 2: `AppMain` 連携

対象:
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

やること:
- crash report を debug console から読めるようにする
- 既存の diagnostics surface に text で出す

完了条件:
- crash report が UI から参照できる

