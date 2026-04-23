# M-DIAG-5 Startup Thread Churn / Worker Burst Trace

コンポジション初期化やアプリ起動直後に、大量の worker thread が短時間で生成・終了する現象を可視化し、不要な burst を減らすためのマイルストーン。

## Goal

- startup / first composition open / first preview の thread churn を見える化する
- `code 0` 終了の大量発生を「正常終了」で片付けず、発生源を特定する
- `Trace Timeline` / `ProfilerPanelWidget` で startup burst を追えるようにする

## Why

- 観測上、コンポ初期化程度で大量の thread が生成・終了している
- crash ではないが、初回体感・CPU burst・scheduler contention・debuggability に影響する
- `ImmediateContext` 境界整理や preview/render path 整理と並行して、worker churn を抑えたい

## Primary Suspects

- `ArtifactCore/src/Thread/ThreadHelper.cppm`
  - `sharedBackgroundThreadPool()`
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
  - `openFuture_`, `decodeFuture_`, `publishVideoLayerModifiedAsync`
- `Artifact/src/Layer/ArtifactImageLayer.cppm`
  - prefetch future
- `Artifact/src/Layer/ArtifactSvgLayer.cppm`
  - prefetch future
- `Artifact/src/Render/ArtifactRenderScheduler.cppm`
  - dedicated `QThreadPool`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
  - playback worker `QThread`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - lazy pipeline init / background jobs

## Phases

### Phase 1: Trace Tagging

- startup worker を domain ごとに trace できるようにする
- `Render / Decode / Asset / Playback / Project / AI` の lane に分ける
- 実行メモ: `docs/planned/MILESTONE_STARTUP_THREAD_CHURN_TRACE_PHASE1_EXECUTION_2026-04-21.md`

### Phase 2: Burst Correlation

- `Trace TimelineWidget` で startup burst を表示
- `ProfilerPanelWidget` に startup hotspots を追加

### Phase 3: Pool Consolidation

- 単発 `QtConcurrent::run(...)` と専用 thread / pool の乱立を整理
- shared pool へ寄せるべきものと、専用 worker で持つべきものを分ける

### Phase 4: Initialization Deferral

- 初回表示に不要な prefetch / decode / lazy init を遅延
- first paint / first interaction を優先する

## Completion Criteria

- startup 時の thread burst が trace 上で読める
- 発生源ごとの thread churn 件数を比較できる
- 不要な単発 worker の起動が減る
- 初回コンポ表示時の体感 burst が軽くなる

## Related

- `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`
- `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`
