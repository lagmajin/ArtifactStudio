# Phase 4 実行メモ: Thread Trace / Lock Trace

> 2026-04-21 作成

## 目的

thread id 付きの trace と lock 取得・解放の記録を追加する。

これで deadlock / 停滞 / 待ち合わせの偏りを追いやすくする。

## 重点対象

- `ArtifactCore/include/Diagnostics/*`
- `ArtifactCore/include/Utils/PerformanceProfiler.ixx`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ArtifactDebugConsoleWidget.cppm`

## やること

- thread id を trace event に含める
- lock acquire / release を記録する
- debug build か設定で有効化できるようにする

## 完了条件

- thread trace が読める
- lock の流れが追える
- deadlock 調査に使える

