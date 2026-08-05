> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md](MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md)

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

---

## Static audit follow-up (2026-07-25)

`Diagnostics.Trace`、`TraceTimelineWidget`、`ProfilerPanelWidget` の現行ソースを照合した。ビルド・実機計測は未実施。

| 完了条件 | 現状 | 判定 |
|---|---|---|
| thread trace が読める | thread ID／name、scope・lock・crash件数、lock depth／last mutex を snapshot と UI で表示する。 | 実装済み／実機確認待ち |
| lock の流れが追える | acquire／release を `TraceLockRecord` と event に記録し、mutex chain、thread focus、lock focus を表示する。 | 実装済み／実機確認待ち |
| deadlock 調査に使える | 未解放 depth、mutex balance、最後の acquire／release は表示されるが、cycle／wait graph による deadlock 自動判定や starvation 判定は未確認。 | 部分実装 |
| debug build／設定で有効化できる | bounded な上限値はあるが、TraceRecorder 専用の明示的 enable／disable 設定は確認できない。 | 未完了 |

### 現在の判定

thread／lock の記録と診断表示はコード上実装済み。ただし自動 deadlock 判定と有効化設定が不足しているため、Phase 4 は「部分実装／拡張・実行確認待ち」とする。
