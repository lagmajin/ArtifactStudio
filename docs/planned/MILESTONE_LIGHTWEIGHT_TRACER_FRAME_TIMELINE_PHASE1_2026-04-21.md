# Phase 1 実行メモ: Crash Stack Capture

> 2026-04-21 作成

## 目的

クラッシュ時の stack を、アプリ内でまず残せるようにする。

この Phase は minidump まで行かず、軽量な crash report を最初に作る。

## 重点対象

- `ArtifactCore/include/Diagnostics/*`
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

## やること

- crash 時の stack を記録する
- frame / thread / last scope を report に含める
- report を text で出せるようにする

## 完了条件

- crash stack が取れる
- report が `Debug Console` から読める

## File Tickets

- [`docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE1_EXECUTION_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE1_EXECUTION_2026-04-21.md)
- `Trace.Crash`
- `AppMain` 連携
