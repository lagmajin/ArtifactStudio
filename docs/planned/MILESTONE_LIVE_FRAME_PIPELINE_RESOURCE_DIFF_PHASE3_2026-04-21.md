> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md](MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md)

# Phase 3: State Diff Tracker

> 2026-04-21 作成

## 目的

[`docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md) の Phase 3 を、壊れ始めた瞬間を追う差分トラッカーとして切り出す。

---

## 方針

1. 前フレームとの差分を自動で取る
2. PSO / CB / SRV / UAV の変更履歴を持つ
3. 壊れ始めたフレームを判定する
4. `renderScheduled_` 系の再描画不整合を追いやすくする

---

## 実装タスク

### 1. Diff model を定義する

追加候補:

- `FrameStateDiff`
- `FrameStateDiffChange`
- `FrameStateDiffSeverity`

やること:

- before / after を持つ
- change kind を分類する
- warning / error 相当の判定を付ける

### 2. Diff の材料を集める

候補ファイル:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- `ArtifactCore/include/Diagnostics/Trace.ixx`

やること:

- 前フレームとの比較材料を取る
- backend / queue / selection / pass state の差分を記録する
- lock / scope / crash の変化を関連付ける

### 3. 壊れ始め判定を作る

やること:

- failed frame を見つける
- 直前の正常フレームとの差を取る
- diff summary を text で出す

---

## File Tickets

### P3-T1 Diff Model

対象:

- `ArtifactCore/include/Render/FrameStateDiff.ixx`
- `ArtifactCore/include/Render/FrameStateDiffChange.ixx`

完了条件:

- before / after / severity を持つ diff が読める

### P3-T2 Diff Bridge

対象:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

完了条件:

- 前フレームとの差分を 1 画面から読める

---

## Static audit follow-up (2026-07-25)

`FrameStateDiffWidget`、FrameDebug snapshot、pipeline/resource diff の現行実装を確認した。ビルド・実行時の自動検出は未確認。

| Ticket | 現状 | 判定 |
|---|---|---|
| P3-T1 Diff Model | before/current snapshot、change kind、compare state、density/resource/pass 差分のモデル・表示がある。PSO/CB/SRV/UAV 専用の完全履歴は未確認。 | 部分実装 |
| P3-T2 Diff Bridge | Frame Debug/State Diff に previous/current と text diff summary がある。queue/playback/backend/selection の全差分接続は未確認。 | 部分実装 |
| 壊れ始め判定 | failed pass/resource warning と前回との差分は表示できる。直前の正常 frame の自動確定と renderScheduled 系の自動原因特定は未確認。 | 未完了 |

### Phase 3 判定

snapshot 差分の read-only first cut は実装済みだが、GPU state 履歴と壊れ始め frame の自動判定が残る。Phase 3 は「部分実装／統合確認待ち」とする。
