> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md](MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md)

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

- 親文書へ統合済み
- `Trace.Crash`
- `AppMain` 連携

---

## Static audit follow-up (2026-07-25)

`CrashHandler`、`AppMain` の callback／pending report ingestion、`ArtifactDebugConsoleWidget`／`TraceTimelineWidget` の表示経路を現行ソースで照合した。ビルド・実機クラッシュ検証は未実施。

| 完了条件 | 現状 | 判定 |
|---|---|---|
| crash stack が取れる | `CrashHandler::install()` と crash report の stack trace 出力、`AppMain` の `traceCrashFromReportPath()` が存在する。 | 実装済み／実機確認待ち |
| report が Debug Console から読める | `ingestPendingReports()`、`TraceRecorder::recordCrash()`、Debug Console／Trace Timeline の crash summary・stack 表示を確認した。 | 実装済み／実機確認待ち |
| frame / thread / last scope を report に含める | timestamp・thread 情報と stack の取り込みは確認できるが、crash report 自体への frame／last scope の完全な関連付けは未確認。 | 部分実装 |

### 現在の判定

Crash report の生成・取り込み・診断 UI 表示はコード上実装済み。frame／last scope の完全な同梱と実機クラッシュ経路は未検証のため、Phase 1 は「部分実装／実行確認待ち」とする。
