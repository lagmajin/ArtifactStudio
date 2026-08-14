# Multithreading Opportunity Audit

**最終更新:** 2026-08-14

**監査範囲:** `ArtifactCore`、`Artifact` のC++20 modules、Qt UI境界、CPU画像処理、GPUレンダリング、I/O、RAM Preview、NUMA適用候補

**監査方法:** 静的検索と関連関数の読み取り。ビルド、CMake生成、テスト、実機ベンチマークは未実施。

## 結論

最も安全で効果を確認しやすい順序は、次のとおりである。

1. 既存の並列処理が二重並列化や細かすぎる粒度になっていないかを監査する
2. `MotionTracker` の独立サンプル処理を並列化候補として検証する
3. RAM PreviewのCPU preparation、GPU submit、readbackを分離する
4. 大容量のmatte、mask、image effectをタイル単位で処理する
5. 同期I/O、プロキシ生成、Asset Browser待機を既存の非同期経路へ統合する
6. NUMAは大容量ワークバッファとデュアルソケット実機で効果を確認した後に適用する

最終合成、Qt UI、QObject、GPU immediate contextの同時実行は、現状では並列化対象にしない。

## 既存インフラ

## 監査後に実装・push済みの範囲

監査後、以下は既存の `ArtifactCore::Parallel` と明示的な状態同期を再利用して実装した。いずれもビルド、テスト、ベンチマークは未実施であり、速度向上は未計測である。

- `MotionTracker::solveCameraPoseStream()` の順序保持付きサンプル並列化
- `VolumePostProcess`、software blend、CPU matte、HDR monitor の独立 row／pixel 処理
- `RendererQueueManager` と `RenderFarmMaster` の model／callback／job state／remote state の thread boundary と race 修正
- `MediaReader` の worker lifecycle（start／pause／stop／queue 読み取り）の既存 `QMutex` 同期化
- `ImageExporter` の RGBA float 変換、multi-channel synthetic channel、interleaved buffer 構築
- RenderScheduler の未接続 strategy は、所有権契約がないため並列化せず安全な逐次 fallback を維持

これらは「実装済み」であって「性能検証済み」ではない。結果一致、キャンセル、破棄、worker 数、メモリ帯域を個別に検証する必要がある。

### CPU並列

- `ArtifactCore::Parallel::For` は `ArtifactCore/src/Core/Parallel.cppm` でTBB `parallel_for`へ接続されている
- `Artifact`／`ArtifactCore` 内で `Parallel::For` の利用箇所が多数存在する
- 色処理、OpenCV、エフェクト、mask、volume、particle、画像変換などは既に部分的に並列化されている
- 新しいCPUデータ並列処理は、QtConcurrentより先にこの抽象化を検討する

### 非同期処理

- `ArtifactCore/src/Thread/ThreadHelper.cppm` に共有 `QThreadPool` がある
- 共有プールは最大4スレッドに制限されている
- Asset import、画像・音声サムネイル、プロジェクト処理、readbackなどにQtConcurrent／watcher経路が存在する
- 既存のキャンセル、generation、owner-thread publishを再利用する

### GPU

- rendererは `flush`、`present`、readback、GPU resource ownershipを持つ
- 非同期readbackはGPU submitとfence wait／pixel conversionを分離する既存経路がある
- Diligent／DX12のimmediate contextは、コード上で安全な複数スレッドsubmit契約が確認できるまで直列資源として扱う

## 優先候補

| Priority | Location | Work unit | Classification | Evidence | Main hazard | Smallest safe next step |
|---|---|---|---|---|---|---|
| P0 | `ArtifactCore/src/Tracking/MotionTracker.cppm:811` | `solveCameraPose(samples[index])` | Conditional / Parallel-safe candidate | サンプルごとにsolveし、結果を順序付きvectorへ格納している | progress、cancel、結果公開、solver内部の共有状態 | solverのconst性と内部static／OpenCV共有状態を確認し、index順のlocal result配列を作る |
| P0 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm:384-434` | frame preparationとGPU描画 | Conditional / Async-safe | render loopは専用threadだが、`renderMutex_`内で`renderOneFrame()`を完了まで実行 | renderer、composition、共有frame stateの所有権 | CPU preparationだけをsnapshot入力に限定し、GPU submitは現行threadへ残す |
| P0 | RAM Preview | frame preparation、GPU submit、readback | Conditional | frame generation、pending queue、async readback、stale-result判定が存在 | frame-local target、共有framePosition、cache publish、GPU context | 2-slot bounded pipelineの状態機械を実装前に定義する |
| P1 | CPU matte／mask | tile rowsまたは独立surface | Conditional / Parallel-safe candidate | 大容量ピクセルループとCPU matte evaluatorが存在 | halo、blend順、共有scratch、QImage境界 | `ImageF32x4_RGBA`のtile独立性を確認し、単一tileの結果一致を証明する |
| P1 | `Artifact/src/Effect/*` | effectのpixel／tile loop | Already parallel / Audit needed | 多数の`Parallel::For`が存在 | nested TBB、メモリ帯域飽和、effect間の順序依存 | per-effect timingとactive worker数を計測し、未並列箇所だけ抽出する |
| P1 | Asset Browser | thumbnail／audio preview | Already async / no new candidate confirmed | 通常の画像thumbnailと音声waveformは`QFutureWatcher`＋`QtConcurrent::run`で非同期化済み。残る`waitForFinished()`は`Impl`破棄時のwatcher cleanup | 破棄中のworker lifetime、QImage/QPixmap境界、generation、cache invalidation | 新規非同期化は行わず、破棄時のcancel／cleanupとgeneration破棄の回帰確認に限定する |
| P1 | Project／Asset import | copy、probe、metadata | Async-safe | async import経路が既に存在する | import順序、登録owner、失敗報告 | 同期呼び出し元を列挙し、既存async APIとの結果整合を比較する |
| P1 | Proxy／AutoSave | encode、serialize、file write | Async-safe | 重い同期処理が計画文書で確認されている | 編集中snapshot、atomic save、process concurrency | immutable project snapshotを先に定義する |
| P2 | `ArtifactRenderScheduler` | frame tasks | Conditional | `FrameParallel`、`TileParallel`、`LayerParallel` enumとqueue契約が存在 | 実装と契約の乖離、GPU共有、progress signal | scheduler実装の実行経路を読み、宣言だけの戦略を区別する |
| P2 | NUMA work buffer | tile／scratch／large image buffer | Conditional | NUMA対応マイルストーンを追加済み | remote memory、Windows processor group、fallback | topology／allocatorを先に追加し、dual-socket実機でlocal／remoteを測定する |
| P3 | OCIO LUT／CPU color transform | LUT voxel／pixel | GPU-suitable candidate | LUT生成とpixel transformに大きなループがある | GPU/CPU色一致、転送、初回生成コスト | 既存GPU LUT経路とCPU出力の比較測定を行う |
| P3 | multi-channel overlay | channel composition | GPU-suitable candidate | channelごとのreadbackとCPU合成が存在 | readback増加、表示latency、QImage境界 | GPU上での合成結果をreadback 1回にできるか確認する |

## 既存並列処理の監査対象

次の領域は既に`ArtifactCore::Parallel::For`を使っているため、単純な追加並列化ではなく、効率と安全性の監査対象とする。

- `Artifact/src/Effect/ArtifactMotionBlur.cppm`
- `Artifact/src/Effect/ArtifactTransition.cppm`
- `Artifact/src/Effect/ArtifactFilmEffects.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `ArtifactCore/src/ImageProcessing/*`
- `ArtifactCore/src/ImageProcessing/OpenCV/*`
- `ArtifactCore/src/Mask/RotoMask.cppm`
- `ArtifactCore/src/Simulation/PyroSimulation.cppm`
- `ArtifactCore/src/Render/VolumeModifier.cppm`
- `ArtifactCore/src/Video/Stabilizer.cppm`
- `ArtifactCore/src/Color/AutoColorMatch.cppm`
- `ArtifactCore/src/Geometry/Procedural3DGenerators.cppm`

各候補で確認すべき事項は、入力のread-only性、出力partition、row／tile境界、乱数seed、OpenCV内部並列化との二重実行、呼び出し元が既にTBB worker上かどうかである。

## 並列化しない領域

- 最終alpha合成、mask、matte、temporal effectの順序依存部分
- `ArtifactCompositionRenderController`の`renderInProgress_`で保護される再入禁止経路
- `flush()`、`present()`、immediate contextへのcommand submission
- Qt Widget、QObject、model、既存signal／slotのowner-thread処理
- 共有`framePosition`の変更・復元を前提とする処理
- 小規模タスクの大量分割
- ReactiveEvents frozen area
- 子リポジトリの変更を伴う作業（個別に明示依頼が必要）

## NUMA方針

NUMAは全コードに広げず、次の順で扱う。

1. 単一NUMA環境を単一nodeとして表現する
2. topology照会とpreferred-node allocationをOS非依存APIの裏に隠す
3. 大容量の画像tile、CPU合成buffer、render scratchだけに適用する
4. thread affinityはopt-inにする
5. dual-socket実機でlocal／remote memory、throughput、latencyを比較する

P-core／E-core差はNUMAとは別問題なので、Alder Lake以降の単一NUMA環境では既定のschedulerへ任せる。

## 実装前に必要な計測

- 処理ごとのwall time、CPU time、worker数、queue wait
- TBB／共有QThreadPoolのactive thread数
- frame preparation、GPU submit、flush、present、readback fence wait
- pixel bufferのサイズ、copy回数、CPU↔GPU転送量
- cache hit／miss、stale result破棄数、キャンセルから完了までの時間
- dual-socket時のNUMA local／remote memory比率
- single-thread、既存並列、候補並列の同一入力比較

## 必須の検証ケース

### 正しさ

- single-thread結果と並列結果の一致
- frame／layer／tile順序の一致
- mask、matte、blend、temporal effectの一致
- 乱数・seed・浮動小数点許容差の確認

### ライフタイムとキャンセル

- widget破棄中のworker完了
- project／composition切替中のstale result
- cancel直後の結果公開
- renderer／GPU resource破棄とreadback完了の競合

### UIとGPU

- UI操作中の応答性
- Qt objectをworkerから直接触っていないこと
- GPU submitとreadback fenceの所有thread整合
- `flush`／`present`の順序維持

### NUMA

- single NUMA
- dual socket
- NUMA allocation失敗時の標準allocator fallback
- affinity無効時と有効時の比較

## 実装順序

1. 計測用の既存Profiler／診断へthread、queue、render phase情報を追加する
2. `MotionTracker`の独立サンプル処理を小さく検証する
3. Asset Browser／import／proxyの同期待機を非同期契約へ整理する
4. RAM PreviewをCPU preparationとGPU submitに分離する
5. CPU matte／image effectのtile境界を整理する
6. RenderSchedulerの宣言済み戦略と実装状態を一致させる
7. NUMA topology／allocatorを追加し、大容量bufferへ限定適用する
8. 実機測定で効果が確認できた箇所だけ有効化する

## 未実施事項

- ビルド、CMake生成、テスト
- ベンチマーク
- dual-socket実機でのNUMA測定
- 各候補のruntime結果比較

本監査は、未実装候補について実装を自動的に許可するものではない。実装済み範囲も含め、各フェーズは対象範囲、所有権、キャンセル、検証条件を確定してから個別に検証する。

## RenderSchedulerの現状制約

`Artifact/src/Render/ArtifactRenderScheduler.cppm` の `Impl` には `taskExecutor_` があるが、公開ヘッダに設定APIはなく、現行コードの検索範囲では実際のcomposition／frame rendererを接続する経路を確認できない。
したがって、現在の `FrameParallel` は「登録されたRenderTaskを並列に実行できる」ことを意味するだけで、フレームレンダリングが自動的に並列化されることを意味しない。

Tile／Layer strategyを実装するには、次の契約を先に定義する必要がある。

- taskが所有するframe／tile／layer snapshot
- renderer／GPU contextの所有thread
- output surfaceの書き込み範囲とmerge順序
- cancel、failure、stale resultの公開規則
- task executorを注入する既存サービスまたは新規公開APIの責務

この契約なしにexecutorやglobal callbackを追加しない。
