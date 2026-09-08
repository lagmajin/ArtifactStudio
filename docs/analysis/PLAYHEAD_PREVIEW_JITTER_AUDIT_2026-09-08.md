# プレビュー再生時プレイヘッドのガタつき調査

**最終更新:** 2026-09-08

## 1. 調査対象・スナップショット

| 項目 | 内容 | 根拠 | 状態 |
|---|---|---|---|
| Project | ArtifactStudio（親）／Artifact（対象 child） | `J:/dev/ArtifactStudio` | 確認済み |
| 対象 | プレビュー再生中のタイムライン・プレイヘッド更新 | ユーザー報告 | 確認済み |
| Branch / commit | parent `main` / `6fabd47e`; Artifact `main` / `c3608cec` | 2026-09-08 の Git 読み取り | 確認済み |
| Dirty state | 親・Artifact・他 child に多数の既存未コミット変更。本文書以外は調査で変更しない。 | `git status --short` | 確認済み |
| 制約 | ビルド・CMake・テストは未実行。Qt CSS・新規 signal/slot は不可。 | `AGENTS.md` | 確認済み |

## 2. 情報の信頼度モデル

本書では、採用済み要求・仕様、実装観測事実、検証済み事実、設計案、仮説、未確認、廃止済み・旧情報を区別する。コードの静的読解は実装観測事実であり、再生中の視覚的なガタつきの再現・改善は未確認として扱う。

## 3. 正本・決定根拠

| 優先度 | Source | 範囲 | 根拠・注意 |
|---|---|---|---|
| 1 | ユーザー報告 | プレビュー時の体感不良 | ガタつきの再現条件・頻度は未取得 |
| 2 | `AGENTS.md` | 実装・検証制約 | GPU経路優先、ビルド／テストは明示指示まで禁止 |
| 3 | 現行実装 | 時刻・UI更新経路 | `ArtifactPlaybackEngine`、`ArtifactPlaybackService`、`ArtifactTimelineWidget` |
| 4 | Hot-Path milestone | 既知の性能仮説と計画 | 静的監査であり runtime 実証ではない |

## 4. 不変条件・設計制約・保護機構

| ID | 不変条件・制約 | 根拠 / 状態 |
|---|---|---|
| INV-001 | 再生中の論理フレームはエンジンが一方向に進め、通常速度で重複 emit しない。 | `ArtifactPlaybackEngine::Impl::runPlaybackLoop()` の `lastEmittedFrame_` 比較。静的確認済み。 |
| INV-002 | UIの補間表示は実フレーム通知ごとに再始動せず、実値との差が1.5 frameを超えたときだけ再アンカーする。 | `ArtifactTimelineWidget` の `FrameChangedEvent` 購読。静的確認済み。 |
| INV-003 | 同じ意味のフレーム状態の更新で、GUIスレッドへ全フレーム走査を重複投入しない。 | `cacheVisualRefreshPending_` による single-shot 集約を実装。静的確認済み、runtime未確認。 |
| INV-004 | 再生時のフレーム時刻と表示用プレイヘッドの更新は、重いキャッシュ・ツールチップ生成から分離する。 | `FrameChangedEvent` からの直接走査を削除。cacheイベント経由で最大約15Hzに集約。静的確認済み、runtime未確認。 |

## 5. 依存関係・影響範囲

```text
PlaybackEngine worker
  -> BlockingQueued frameChanged (通常再生)
  -> PlaybackService::syncCurrentCompositionFrame
  -> EventBus FrameChangedEvent
  -> Timeline smooth-playhead timer (16ms, GUI thread)
  -> Timeline cache visual refresh / LayerPanel repaint / Control update
```

- `ArtifactPlaybackEngine::Impl::updateFrame()` は通常再生で `BlockingQueuedConnection` を使い、GUI側の composition 同期を待つ。したがってGUI負荷は再生workerの進行にも直接跳ね返る。
- `ArtifactTimelineWidget` は `QTimer(16ms, PreciseTimer)` で fractional frame を描く。ただし timer はGUIスレッド上なので、長いUI処理中はtickできない。
- `FrameChangedEvent` の購読で `updateCacheVisuals()` が毎フレーム呼ばれる。同関数は frame bitmapを複製し、全frameについて状態を問い合わせ、長いtool tip文字列も生成する。
- `PlaybackRamPreviewStateChangedEvent` と `PlaybackRamPreviewStatsChangedEvent` からも `updateCacheVisuals()` を queued で呼ぶため、frame通知と別系統の更新が競合する。

## 6. 既知の不具合・再発防止知識

| ID | 観測された失敗 / リスク | 根拠 | 原因の確度 | 再発防止 |
|---|---|---|---|---|
| REG-001 | GUIが詰まると補間playheadが16msごとに更新されず、段階的に見える。 | GUIスレッドの `QTimer` と毎frameのUI更新経路 | 高確度の設計上の帰結。runtime未確認。 | 再生時のUI処理量をframe時間から切り離し、cache表示をcoalesceする。 |
| REG-002 | `updateCacheVisuals()` が frame通知ごとに O(duration) 状態走査・bitmap比較・tooltip整形を行う。 | `ArtifactTimelineWidget::updateCacheVisuals()` | 実装観測事実。 | cache generation/versionで更新を間引き、内容変更時のみ表示を更新する。 |
| REG-003 | cache状態／統計通知が別queueで同じUI更新を積む。 | `PlaybackRamPreview*ChangedEvent` 購読 | 実装観測事実。 | 1つのsingle-shot coalescerに統合し、pending中は追加予約しない。 |
| REG-004 | GPU readback、texture cache miss、renderOneFrame多重実行がGUIフレーム予算を圧迫し得る。 | `MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md` | 設計監査上の高確度仮説。runtime未確認。 | 同milestone Phase 1〜3を別途検証して実装する。 |

## 7. 受入基準・検証マトリクス

| ID | 基準 | 検証方法 | 期待結果 | 現在の証拠 | 状態 |
|---|---|---|---|---|---|
| ACC-001 | 30fps/60fps再生でplayheadが単調前進し、見た目の停止がない。 | runtime動画または画面録画で16ms間隔を確認 | 500ms超の視覚的停滞なし | 未実行 | Pending |
| ACC-002 | cache stateが変わらない再生中、cache表示の再計算はframeごとに走らない。 | profiler/log counter | 再計算回数はcache version変化時のみ | `FrameChangedEvent` の直接呼出を除去し、pending中は追加予約しない。 | Static pass / runtime pending |
| ACC-003 | 実フレームと補間値の差が1.5 frame以内では補間クロックを再始動しない。 | unitまたはruntime log | saw-toothの再アンカーなし | 現行コードに条件あり | Static pass / runtime pending |
| ACC-004 | GUIが重い時も、workerからの通常再生frameは古いqueued tickを蓄積しない。 | playback session logのsync/coalesced計数 | backlogの単調増加なし | BlockingQueuedの設計あり | Runtime pending |
| ACC-005 | 停止・seek・loop境界では表示が正しいframeに一度だけ再アンカーする。 | 手動確認 | backward jitterなし | 未実行 | Pending |

## 8. 古い情報・矛盾・暫定対策

- `MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md` はGPU readback、texture cache、render多重実行を原因候補として記録する。一方、現行Timelineにはsmooth-playhead timerと1.5 frame drift guardが存在する。前者は全体性能、後者は視覚補間の対策であり矛盾しない。
- 「補間timerがあるためガタつきは解消済み」とは断定できない。timer自身がGUIスレッドに依存し、cache表示更新が同じスレッドで毎frame走るためである。
- 再生セッションログは実装されているが、実機ログの取得・解析は未実施である。

## 9. 変更候補

1. `updateCacheVisuals()` をcache-state通知のsingle-shotでcoalesceし、frameChangedから直接の全frame走査を外す。`ArtifactTimelineWidget.cppm` に実装済み。低〜中リスク、runtime確認待ち。
2. 再生中のtooltip全再生成を停止し、停止時またはcache状態変更時に限定する。低リスク。
3. playhead用の軽量snapshot（frame, elapsed, range）とcache状態表示を別更新帯域へ分離する。中リスク。既存EventBusの責務整理が必要。
4. Hot-Path milestoneのGPU readback排除、texture key安定化、render入口統合を実測付きで進める。高影響・広範囲。

## 10. 証拠索引

- 制約: `AGENTS.md`, `docs/DOC_LIFECYCLE.md`
- 再生producer: `Artifact/src/Playback/ArtifactPlaybackEngine.cppm` — `runPlaybackLoop`, `updateFrame`
- GUI境界: `Artifact/src/Service/ArtifactPlaybackService.cppm` — `syncCurrentCompositionFrame`, `frameChanged` connection
- playhead consumer: `Artifact/src/Widgets/ArtifactTimelineWidget.cppm` — `updateSmoothPlaybackPlayhead`, `FrameChangedEvent` subscriber, `setCurrentFrameForAll`, `updateCacheVisuals`
- cache display: `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm` — `setFrameStateBitmaps`
- known-risk plan: `docs/planned/MILESTONE_VP_TIMELINE_HOTPATH_STABILITY_2026-08-30.md`
- diagnostic availability: `docs/planned/MILESTONE_PLAYBACK_DIAGNOSTIC_LOGGING_2026-08-08.md`
- focused playback/playhead runtime test: リポジトリ検索では確認できず。`ArtifactPlaybackControlTestWidget` は存在するが自動テストである根拠は未確認。
