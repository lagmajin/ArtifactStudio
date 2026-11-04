# PlaybackClock 契約テスト静的監査

**最終更新:** 2026-11-03

## 対象

- `tests/ArtifactCore/PlaybackClockContractTest.cpp`（2026-11-03 新規・未コミット）
- 実装: `ArtifactCore/include/Playback/PlaybackClock.ixx` / `ArtifactCore/src/Playback/PlaybackClock.cpp`
- 関連モジュール: `Playback.State`（`ArtifactCore/include/Playback/PlaybackState.ixx`）、`Frame.Position`、`Frame.Rate`、`Event.Bus`

`Playback.Clock` は Core の公開モジュールだが、専用テストが存在しなかった。本次は新規契約テストを追加する作業が進行中のため、既存 14 ケースを実装と 1 項目ずつ突合し、不足カバレッジを補った。ArtifactCore は子リポジトリであり、ユーザー明示指示がないため実装は変更していない。

## 本番利用状況

`PlaybackClock` の本番利用者は現時点で存在しない。`Artifact/src/Composition/ArtifactAbstractComposition.cpp:2312` の `//PlaybackClock playbackClock_;` はコメントアウトされたままで、`Artifact` / `ArtifactCore` 两侧に `PlaybackClock` のインスタンス生成は無い。`ArtifactPlaybackService::setPlaybackClockProvider(const std::function<double()>&)`（`Artifact/include/Service/ArtifactPlaybackService.ixx:209`）は別の注入機構で、このクラスとは無関係である。

つまり今回の指摘は、現時点では到達経路の無い公開 API の境界不備であり、再生 Transport へ接続する阶段的で顕在化する可能性がある。`LIBRARY_DLL_API` 付きで外部からも参照できる型なので、接続前に契約と null ガードを固める価値がある。

## 突合結果

既存 14 ケースはいずれも実装と整合すると判断した。根拠となる実装側の仕組みは次のとおり。

- `setFrame()` は `currentFrame_` と `startFrame_` を同時に書き換える（`PlaybackClock.cpp:399-419`）。このため `stop()`（`:321-334`）と `reset()`（`:653-668`）は直近の `setFrame()` 値へ戻る。
- `publishSnapshotChange()` は `sameSnapshot()` が真なら通知しない（`:191-217`）。無効遷移や同値代入はイベントを発生させない。
- `currentFrame()` は経過時間から再計算するが、停止中は `timeToFrame()` が `startFrame_` に固定される（`:120-123`, `:130-172`）。drop-frame 検出の `-1 → 100 → 200` シーケンスと `droppedFrameCount == 99` はこの挙動に依存する。
- `syncToAudioClock()` は同期無効時に即 return し（`:531`）、観測可能なスナップショットが変化した時だけ通知する。
- `statistics()` の出力文字列（`:670-694`）は期待文字列と一致する。

## 追加したケース

| ケース | 固定する契約 |
|---|---|
| `SetFrameRatePublishesReasonWithoutMovingTheCurrentFrame` | `setFrameRate` の通知理由と、現在フレームを動かさないこと |
| `SetPositionPublishesTheSetFrameReason` | `setPosition` が `setFrame` と同じ通知理由を運ぶこと |
| `PublishedSnapshotMatchesTheObservedSnapshot` | 通知の `current` と観測時の `snapshot()` が全項目で一致すること |
| `ResetWhilePlayingReturnsToTheStartFrame` | 再生中の `reset()` が開始フレームへ戻り、1 件の通知になること |
| `SetFrameWhilePlayingUpdatesTheSnapshotImmediately` | 再生中に `setFrame` した直後のスナップショットが要求フレームを報告すること |
| `SelfAssignmentKeepsTheClockUsable` | 自己代入・自己 move 代入で clock が壊れないこと |
| `CopyingAMovedFromClockYieldsAFreshStoppedClock` | move 済みオブジェクトのコピーが既定状態の clock になること |
| `TimecodeFormatsHoursMinutesAndSeconds` | 1 分 30 秒・1 時間 2 分 5 秒の timecode 整形 |
| `DeltaTimeIsMeasuredFromTheLastUpdate` | `deltaTime()` が最終更新からの経過を返し、負にならないこと |
| `LoopRangeEndpointsAreStoredVerbatim` | 逆順端点がそのまま保存されること |

追加前の未カバレッジは公開メンバ中 `deltaTime()` のみだった。ループの折り返し（`updateCurrentFrame()` の `looping_` 分岐）は実時間に依存するため確定的な検証が困難で、未カバレッジのままとした。

## 実装側の指摘（未修正）

いずれも ArtifactCore 子リポジトリの修正が必要で、ユーザー明示指示がないため未修正。details は `Insight.md` の 2026-11-03 エントリを参照。

1. move 済みの `PlaybackClock` は `impl_` が null になり、全公開メソッドが null 参照になる。コピーと破棄だけが安全。
2. `start()` 前の `deltaTime()` は epoch 起点の巨大な値を返す。
3. 負フレームの `timecode()` は `00:00:-1:00` という不正な形式になる。
4. 再生中の `setPlaybackSpeed()` は速度を二重に適用し、現在フレームJump を起こす。
5. `setLoopRange()` は逆順端点を黙って受理し、`isLooping()` は真のまま折り返し条件（`loopEnd_ > loopStart_`）を満たさない。

## 確認状況

プロジェクトのルールに従い、ビルド・CMake 実行・テスト起動は行っていない。上記の判定はすべてソース読解に基づく静的突合であり、runtime の合否は未確認。`ArtifactCorePlaybackClockContractTest` は `tests/ArtifactCore/CMakeLists.txt` へ `ArtifactCore` 依存で登録済みで、既存テストと同じ `artifact_add_test` 形式（gtest + Qt6::Core/Widgets）を踏襲している。
