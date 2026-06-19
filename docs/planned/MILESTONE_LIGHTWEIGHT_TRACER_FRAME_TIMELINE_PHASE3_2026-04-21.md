# Phase 3 実行メモ: Frame Timeline Visualization

> 2026-04-21 作成

## 目的

フレームごとの scope を timeline 上に可視化する。

イメージは「超軽量 Tracy」。

## 重点対象

- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`
- `Artifact/src/AppMain.cppm`

## やること

- `Render / Decode / UI / Event` をレーン表示する
- frame ごとに帯として見せる
- 既存 profiler panel から開けるようにする

## 完了条件

- 1 frame の時間配分が読める
- レーン単位で重さが見える

## File Tickets

- 親文書へ統合済み
- `ProfilerPanelWidget`
- `Frame Timeline View`
