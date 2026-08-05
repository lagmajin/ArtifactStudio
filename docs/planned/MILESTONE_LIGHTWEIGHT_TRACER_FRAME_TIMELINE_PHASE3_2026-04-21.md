> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md](MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md)

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

---

## Static audit follow-up (2026-07-25)

`TraceTimelineWidget`、`ProfilerPanelWidget`、`ArtifactDebugConsoleWidget` の現行ソースを照合した。ビルド・実機表示確認は未実施。

| 完了条件 | 現状 | 判定 |
|---|---|---|
| 1 frame の時間配分が読める | Timeline canvas が event の start/end から span を算出し、domain 別 duration/count の summary も生成する。 | 部分実装／実機確認待ち |
| レーン単位で重さが見える | Render／Decode／UI／Event の色・名称は存在するが、canvas の主行は thread 単位で、domain lane の独立表示は未確認。 | 部分実装 |
| Render / Decode / UI / Event をレーン表示する | domain は event に保持され、summary に集計される。一方、要求どおりの4レーン固定表示は確認できない。 | 部分実装 |
| 既存 profiler panel から開ける | ProfilerPanel に Trace Timeline の summary／描画コードはある。独立 widget の開閉導線としての接続は未確認。 | 部分実装／統合確認待ち |

### 現在の判定

Trace Timeline の描画・集計面は存在するが、Phase 3 の中心である固定 domain レーン表示と Profiler からの明確な導線は未確定。全体は「部分実装／実行・統合確認待ち」とする。
