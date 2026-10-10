**最終更新:** 2026-10-10

# Insight Register

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-10 — XPU CPU lanes share oneTBB through Core.Parallel

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/Core/Parallel.cppm`、`ArtifactCore/include/Common/ThreadPool.ixx`。
- **確認できた事実:** XPU mixed の CPU frame lanes はソフトウェア renderer を通り、複数の画像処理が `ArtifactCore::Parallel::For` を呼ぶ。同 API は `tbb::parallel_for` に委譲される。既存 `ThreadPool` は `tbb::global_control(max_allowed_parallelism, concurrency_)` を作り、TBB の global control は scheduler 内で同時実行する worker thread 数を制限する。TBB の implicit arena は parallel algorithm を呼ぶ user thread ごとに関連付く ([task arena](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/task_arena/task_arena_cls)、[global control](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/scheduling_controls/global_control_cls))。
- **今回の対応:** `Core.Parallel` に共有 TBB arena API を置き、全 `Parallel::For` と XPU CPU frame lanes を同じ arena に通す。XPU は初回 arena 初期化へ process-available thread count を渡し、job host limit を実 concurrency 以下へ上限化する。MSVC module compile で `ThreadPool.ixx` の TBB inline実装を Artifact から使う形が失敗したため撤回し、TBBの実装詳細を既存 `Core.Parallel.cppm` 内へ閉じた。XPU summary と統合テスト source に scheduler と concurrency の確認を加えた。
- **価値／懸念:** XPU CPU frame lanes と内側の TBB kernels が共有 concurrency cap を使う。Taskflow、QThreadPool、GPU submission、writer/encoderとの完全な process-wide budget は未統合。Debug構成でCore/Artifact/test targetがビルド成功し、RTX 4070 Ti Vulkan + CPUのmixed-text、2D/3D particle、10-frame all-adapters/backpressureテストがそれぞれ3回連続成功。共有TBB arena/concurrencyのdiagnosticsも確認済み。複数物理GPU/iGPUでの受入は未確認。
- **次に確認:** ArtifactCoreとArtifactの対象ターゲットをビルドし、XPU mixed runtimeでCPU frame lanesとnested `Parallel::For` が同一arenaに参加すること、summaryの実 concurrency、TBB worker数とprocess CPU utilizationを検証する。

### 2026-10-10 — D3D12 build support does not prove an enumerable worker

- **関連:** root `CMakeLists.txt` Windows SDK `try_compile` paths、`Artifact/src/Render/DiligentDeviceManager.cppm`、`Artifact/tests/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** CMake `D3D12_SUPPORTED` と `VULKAN_SUPPORTED` は両方TRUEになり、test targetはDebugでビルドできる。Windows DXGIでRTX 4070 Tiを列挙し、D3D12CreateDeviceをfeature level 11_0〜12_2で直接呼ぶと成功する。ArtifactのD3D12 primary起動は `No suitable hardware adapter found` でVulkan fallbackとなる。XPUのVulkan primary planは同じ物理adapterをD3D12経由で重複dispatchしないため、Vulkan＋D3D12ケースはGoogleTest skip。物理GPUは1基。
- **今回の対応:** SDK Include/Lib環境を `try_compile` に渡し、D3D12 adapter列挙とdevice選択に先立って `LoadD3D12()` を呼ぶ。
- **価値／懸念:** OSのD3D12機能は利用可能だが、Artifact/Diligentのprimary backend経路が使えていない。build対応や直接API probeは実worker参加の代わりにならない。
- **次に確認:** Diligent factoryの `EnumerateAdapters()` と `CreateDeviceAndContextsD3D12()` の間で候補が失われる箇所を特定し、ArtifactのD3D12 primaryで同じRTXがactiveになることを確認する。その後、別GPU/iGPU hostで実機検証する。

### 2026-10-10 — Particle XPU parity uses different rasterizers

- **関連:** `Artifact/src/Generator/ArtifactParticleGenerator.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`、`tests/Artifact/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** software particle rendering paints a QPainter radial-gradient circle sprite. GPU particle rendering draws an instanced quad and computes a soft circular alpha edge with a shader `smoothstep`; the rasterizers differ.
- **今回の対応:** isolated XPU particle Render Queue test verifies deterministic CPU reference output, XPU frame outputs, plan diagnostics, and both 2D/3D layer setup. The GPU-only production particle test separately verifies visible pixels. CPU/GPU lane participation is asserted in the separate multi-frame text case.
- **価値／懸念:** exact CPU/GPU particle parity remains unverified; the XPU case records mismatch count/bounds without guessing a tolerance. Both XPU particle and GPU-only visibility cases passed three consecutive runs.
- **次に確認:** capture and compare XPU particle pixels across different GPU vendors/backends; do not call parity complete from manager output alone.

### 2026-10-10 — XPU pull dispatch adapts frame share without static weights

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU `nextFrameCounter` worker loop。
- **確認できた事実:** mixed/multi-GPU lanes receive one distinct startup frame, then claim the next frame only after `renderOneFrame` completes. There is no fixed per-device frame quota after seeding.
- **今回の対応:** documented the completion-driven pull behavior next to the shared counter and in P4. Per-device timing weights remain diagnostic rather than being fed back into assignment.
- **価値／懸念:** fast lanes naturally claim more remaining frames, so a stale startup weight is unnecessary and could bias work after scene/device conditions change. This does not prove there is no starvation under long-tail frames or output-buffer backpressure.
- **次に確認:** use runtime bench frame counts and lane render durations on dissimilar CPU/iGPU/dGPU hardware. Add a weighted policy only if the pull scheduler shows a repeatable imbalance that lowers throughput.

### 2026-10-10 — Render Queue の HW encoder slot は現行の単一job実行では飽和しない

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`。
- **確認できた事実:** `ArtifactRenderQueueService::startAllJobs()` は `isRendering_` で同一serviceの二重開始を拒否し、単一 `workerThread_` 内で `jobOrder` を順番に処理する。各動画jobのencoder backendはそのjob処理中に作成される。
- **今回の対応:** `ARTIFACT_XPU_MAX_HW_ENCODERS` の共有slotをNative HW / ffmpeg HW pipe / Vulkan pipeへ適用し、session lifetime中の上限をコード化した。
- **価値／懸念:** 将来job並列化または独立encoder利用が入った際のプロセス共通上限を先に確立できる。ただし現行Render Queue単体では複数jobが同時にencoderを開かないため、設定値の差によるthroughput/backoffのruntime効果はまだない。今回の変更だけでXPU並列性能が改善したとは扱わない。
- **次に確認:** encoder backendを呼び出す他の同一process経路とservice構築可視性を確認し、競合可能なconsumerがなければ、job並列化導入時に同時encoder上限の受入試験を追加する。

### 2026-10-10 — XPU consumer worker と RenderQueue writer 上限の統合

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/IO/Image/AsyncImageWriterManager.cppm`。
- **確認できた事実:** XPU混在時はCPU/GPU frame worker上限とは別に画像書き込みfutureが生成されていた。`AsyncImageWriterManager` は既定hardware_concurrencyサイズのthread poolを作り、enqueue件数を呼び出し側へ通知・制限するAPIがない。
- **今回の対応:** RenderQueue側のasync writer futureを1〜2件に固定し、XPU mixed jobではその同時writer数をhardware thread数から予約してframe lane上限を決める。6 logical threads未満ではasync writerを止めて同期書き込みへ戻す。job summaryへ共有枠を出し、mixed画像統合テストに枠の包含関係を追加。manager指定時はbounded future経路へfallbackし、理由を警告する。ArtifactCoreは変更していない。
- **価値／懸念:** mixed job内のCPU render laneとsequence writer threadが別々に上限を消費しない。これはjob-localな保守的予算であり、UI・TBB・他poolを含むprocess-wide schedulerではない。CPU budgetの効果、writer失敗時の挙動は実行未確認。
- **次に確認:** ユーザー許可後に混在レンダー＋async sequenceの低論理CPU数／通常CPU数条件で、summaryのthread枠、キャンセル、書込失敗伝播、長時間メモリ上限を確認し、render/consumer総CPU予算をbenchで確定する。

### 2026-10-10 — XPU mixed video encoder もframe laneのhost budgetを使う

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`、`RenderQueue/XpuEncoderThreads`。
- **確認できた事実:** native FFmpeg settingsは`threadCount=0`のままencoderへ渡しており、external FFmpeg pipeにも`-threads`指定がなかった。video consumerはframe workerと並行して動く。
- **今回の対応:** mixed動画ではencoder thread数の既定を1にし、正数指定もusable host thread数へclampする。service側は同数をframe laneの予算から控除し、nativeおよびpipe backend双方へthread countを渡す。
- **価値／懸念:** frame renderingとencodeが同じCPU資源を超過予約しにくくなる。混在jobのthread枠であり、アプリ全体のpoolを横断してはいない。FFmpegが実際にそのthread上限を守ること、画質・出力と速度への影響は未検証。
- **次に確認:** software/hardware native backendとexternal pipeで、混在動画のsummary予約数・encoder起動ログ・実プロセスthread数を確認し、thread数別benchを行う。

### 2026-10-10 — XPU mixed eligibility must include the selected render backend

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`xpuMixedEligibleForJob`。
- **確認できた事実:** CPU backendを選んだjobでは`cpu-render-backend-selected`をmixed unavailable reasonとして記録していた一方、eligible predicate自体にはGPU backend条件がなく、CPU/GPU lane計画と予算予約へ進む余地があった。
- **今回の対応:** `useGpuBackend`をeligibility条件に加え、実際にGPU rendererが使われるjobだけをXPU mixedとして扱う。Render Queue画像統合テストにCPU backend baselineのunavailable reason、未起動mixed lane、未予約budgetを検査するassertionを追加した。
- **価値／懸念:** unavailable reasonとplan/CPU予算の分類が一致する。統合テストは未実行で、CPU backendを明示した mixed 要求のruntime summaryは未確認。
- **次に確認:** GPU backend有効／CPU backend明示の両ケースで、active mixed plan、frame lane数、writer/encoder予約が期待通り切り替わることを確認する。

### 2026-10-10 — XPU thread budget should use process-available CPUs

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`。
- **確認できた事実:** XPU lane/encoder budgetは`std::thread::hardware_concurrency()`でmachine-wide logical thread数を参照していた。Qtの`QThread::idealThreadCount()`は、OSが対応する場合にprocessが利用可能なlogical processor数を返し、affinityやCPU hotplugで変化し得る。
- **今回の対応:** serviceとencoderの予算計算を`QThread::idealThreadCount()`へ統一し、processに適用されたCPU制限を反映する。
- **価値／懸念:** affinity/container/job objectなどの制約が公開される環境で、実際に使えないCPU数を予算へ数えない。OSごとの検出範囲とjob開始後の変化はruntime未確認。
- **次に確認:** affinityを制限した環境でsummaryのhardwareThreadsが利用可能processor数に一致すること、同一jobのencoder/service両方が同じ数を使うことを確認する。

### 2026-10-10 — 既存Schedulerはdevice-affineなXPU frame laneを表現しない

- **関連:** `Artifact/src/Render/ArtifactRenderScheduler.cppm`、`ArtifactCore/include/Render/MFR/MFRDispatcher.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **確認できた事実:** `ArtifactRenderScheduler` は優先度・重複排除付きの汎用RenderTaskとTaskSystemを持ち、device/renderer affinityをタスクに渡すAPIが見当たらない。`MFRDispatcher` のFrameTaskもframe番号だけを受け取り、実行worker identityをcallbackへ返さない。現在のGPU laneは各immediate context・cache・composition snapshotを単独所有する。
- **設計仮説（未検証）:** 汎用Schedulerへ現状のframe callbackをそのまま移すと、どのGPU laneが実行中かを外部状態で割り当てる必要が生じ、context所有権やin-flight上限が壊れる可能性がある。XPU dispatcher APIはnode/worker identity付きのclaim/completeを提供する必要がある。
- **価値／懸念:** 既存Schedulerの名目上の再利用でGPU資源所有を弱めるリスクを避けられる。一方、現行のローカルatomic採番とraw thread管理はRenderQueueに残る。
- **次に確認:** P2 runtime受入後、device-affine worker descriptorを持つAPIが現在の枠内で追加できるか、シグナル／スロットを増やさず段階的に統合できるかを設計レビューする。

### 2026-10-10 — XPU assist と consumer pipeline は実際の重複実行が必要

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU P3/P4。
- **確認できた事実:** `igpu=assist` のpreview縮小と動画 `pipeline=on` は、CPU処理を `std::async` で開始して同じフレーム内ですぐ `get()` しており、iGPU処理でもencodeとの重複でもなかった。GPU assist plan node は現時点で `maxInFlight=0`。
- **今回の対応:** 誤解を招く即時待機のasync処理を撤去し、動画pipeline要求は非対応警告を出して同期経路を使うようにした。マイルストーン文書も未実装に訂正した。さらに、専用D3D12 workerに偏っていたGPU frame数の観測へprimary rendererのadapter IDとframe数を加えた。
- **価値／懸念:** 「async」指定だけでXPU利用を誤認せず、hot pathで余計なfuture/threadを生成しない。iGPU assistそのものと動画のbounded lookahead pipelineは未実装。
- **次に確認:** GPU資源で実行可能な既存convert/scale/encode経路とrenderer/device ownershipを特定し、assistの一作業を実GPUへ割り当てる。動画pipelineはbounded lookaheadで次フレーム変換と現フレームencodeを実際に重ねる。primaryとdedicated GPU laneを同時に使うfallback caseのbench counter整合は未検証。

### 2026-10-10 — XPU 開発スイッチを ArtifactStartup.json から解決する

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`、`docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`。
- **確認できた事実:** Startup Flags 契約は XPU 系環境変数を `RenderQueue/Xpu*` へ段階移行する計画だが、Render Queue と encoder は環境変数だけを直接読んでいた。また GUI 起動では `ArtifactAppSettings::instance()` が `QApplication` と Startup JSON 読込より先に呼ばれていた。
- **今回の対応:** XPU spec、worker上限、iGPU参加、preview、sequence、parity、pipeline、bench、clone診断、encoder thread/preset/HW上限の値を `LayeredConfigStore` 起点へ移した。JSON があれば対応 env より優先し、各 setting accessor は解決値を起動後初回にキャッシュする。`AppMain` の GUI 起動順を変更し、Startup JSON を最初の `ArtifactAppSettings` 取得前に読み込む。テンプレートに任意keyの一覧を記載。
- **価値／懸念:** IDEやExplorer起動でもXPUを設定でき、解決層の読み込みをrender frame hot pathへ持ち込まない。StartupFlags競合時のログと全keyの型・優先順位はruntime未検証。
- **次に確認:** XPU各keyについて環境変数のみ／JSONのみ／JSONとenv競合／未設定の4条件で解決値を確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — XPU GPU worker の初期化失敗を adapter 単位で隔離

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`GpuFinalWorker` 初期化。
- **確認できた事実:** worker 構築ループは composition snapshot または adapter renderer の初期化に失敗したとき、既に作成済みの全 GPU worker も `workersReady` gate で無効化していた。
- **今回の対応:** 失敗した adapter の worker だけを skip し、他の GPU worker は保持する。CPU fallback snapshot が利用できなくても GPU renderer 自体は登録する。混在時の最低1 worker条件と legacy multi-GPU時の最低2 worker条件は維持する。
- **価値／懸念:** 1台の不調 adapter が他の正常な GPU lane を無効にしない。fallback snapshot がない GPU lane は GPU render 失敗時にCPUへ切り替えられない。
- **次に確認:** adapter 初期化失敗・snapshot clone失敗を個別に注入し、残存 worker のフレーム出力、fallback、summary計数を確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — XPU CPU lane の snapshot clone 部分失敗を縮退扱いにする

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、XPU mixed CPU worker 初期化。
- **確認できた事実:** CPU composition snapshot の作成ループは一つでも clone に失敗すると `mixedCpuReady=false` にし、すでに正常作成した snapshot がある場合も mixed dispatch を有効にしなかった。
- **今回の対応:** 失敗を警告して clone ループを止め、作成済み snapshot が一つ以上あればその数だけ CPU lane を登録する。CPU lane が0ならGPU側の既存dispatchへ縮退する。
- **価値／懸念:** 一時的なclone失敗で既に確保したCPU資源を全て失わず、実際に初期化できたlane数を超えて起動しない。
- **次に確認:** clone failure を注入し、要求／実稼働 lane 数、順序復元、フレーム成功数が縮退後の構成と一致することをruntimeで確認する。ビルド・テストはユーザー明示指示待ち。

### 2026-10-10 — Kuwahara の近傍統計を一走査で集計する

- **関連:** `Artifact/src/Effects/Kuwahara/KuwaharaEffect.cppm`。
- **確認できた事実:** CPU と HLSL の両方で、各 quadrant の平均用と分散用に同じ近傍画素を別々に走査・読込していた。GPU shader は1画素につき4 quadrantを処理する。
- **今回の対応:** RGBの二乗和も平均と同時に集計し、`E[x²] - E[x]²` から分散を得る形へ変更。CPU と HLSL を揃えた。負の丸め誤差は0へクランプする。CPU 側のquadrant loopは0／負のoffsetで終端へ進まず走査が継続する向きだったため、GPU と同じ画素範囲を進むよう修正。
- **価値／懸念:** 近傍テクスチャ読込・CPU画素読込を理論上およそ半減できる。二乗和方式は従来の差分二乗方式と浮動小数点丸めが異なり、HDR値やほぼ一様な領域では分散選択に差が出る可能性がある。速度と画像差は未検証。
- **次に確認:** ビルド後、CPU/GPU pixel parity と半径別の実行時間を比較する。ビルド・テストは明示指示待ち。

## 現在の優先検証

### 2026-10-10 — ArtifactHashMap::operator[] が既存値を黙って消していた

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`ArtifactCore/src/Composition/CompositionRegistry.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `operator[]` が `tryEmplace(key)`（空パックの可変長 overload）に転送され、キー存在時に `curr->data.second = V()` でデフォルト上書きして返していた。`NameMap::operator[]` 経由の読み取りは全て破壊読みだった。実害：`CompositionRegistry::findComposition()` は `contains()` が真でも `entries[key]` で nullptr に潰して返していた（テストで再現・確定）。script host 用の名前解決が全滅していたことになる。
- **今回の対応:** 存在確認→既存参照返却、不存在時のみ `tryEmplace(key, V())` で挿入に修正。単引数 `tryEmplace` の他利用者はなし。回帰用に NameMap 直接試験を残した。
- **価値／懸念:** 全 NameMap/HashMap 利用者の読み取りが正しくなる。逆に今まで「読むと消える」挙動に依存していたコードがあれば顕在化する。既存ビルド済み 14 件は全パス。
- **次に確認:** フルスイート（ArtifactApp 含む）のビルド・実行は未実施。`ArtifactProjectRoundTripTest`（tests/Artifact、ArtifactAppRuntime 要）は未ビルド。

### 2026-10-10 — RenderGraph に executionLevels と executeParallel を追加した

- **関連:** `ArtifactCore/include/Graphics/RenderGraph.ixx`、`tests/ArtifactCore/RenderGraphTest.cpp`。
- **確認できた事実:** `compile()` の Kahn 順序はレベル非減少なので、伝播した level 配列でグルーピングすると並列実行可能なバッチになる。既存 `execute()` の逐次語義は不変。`allocationSlot` / `lifetimes` は算出のみで backend の消費者がいない（未検証ではなく grep で確認済み）。
- **今回の対応:** `CompiledRenderGraph::executionLevels` を追加し、ダックタイピングの launcher（`async(F)->future<bool>`、`Core.TaskSystem` が適合、RenderGraph 側に新規依存なし）でレベル同期実行する `executeParallel` を実装。単一パスレベルはインライン実行で launcher に触らない。単体テスト 15 件（順序・cycle 到達不能ではなく除外・alias 共有/分離・失敗伝播・診断）を追加。cycle は宣言順 append の API では到達不能なためテスト対象外とした。
- **価値／懸念:** 現行グラフは全て線形チェーンのため、フレームパスへの配線は効果ゼロ＋リスクのみ。配線は分岐グラフが出てから。真の並列レンダリングにはパケット構築自体のスレッド分割が別途必要。
- **次に確認:** ユーザーの明示指示後、テストのビルド・実行で検証する。ビルド・テストは明示指示待ち。

### 2026-10-10 — 診断グラフへ層 raster/mask/blend チェーンを載せた

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（`frameDebugSnapshot` の diagnosticGraph）。
- **確認できた事実:** 診断スナップショットのグラフは層ごとの raster が単一 `layerResource` への書き込み羅列＋単一 blend で、マスク段が存在しなかった。実行は変えず記述だけ変える余地がある（診断専用のため）。
- **今回の対応:** 層ごとに raster→mask→blend のチェーンを登録。raster は共有 scratch への書き込み（WAW 辺で直列化され実実行と一致）、mask は enabled matte 参照がある層のみ、`blend` は層ごとに accum 共有トークンへ。`FunctionalRenderPass` 実体は作らず記述子のみで、per-frame の `std::function` 確保は増やさない。空 comp 時は accum 書込維持の Empty blend を残し、下流 effect パスの read-before-write 失敗を避けた。
- **価値／懸念:** FramePipelineViewWidget で層・マスクの有無が可視化され、カリング漏れが目視できる。実行順序・性能は不変。
- **次に確認:** ユーザーの明示指示後、診断スナップショットの表示確認とテストのビルド・実行。ビルド・テストは明示指示待ち。

### 2026-10-10 — Layer2D は未定義メソッドのためテストから除外した

- **関連:** `ArtifactCore/include/Transform/StaticTransform2D.ixx`、`ArtifactCore/src/Layer/Layer2D.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `StaticTransform2D::toQTransform()` と `setTransform2D()` は宣言のみで定義が codebase に存在しない（未検証ではなく grep で確認済み）。`Layer2D::transformedLayer()` は `toQTransform()` を呼ぶ。function-level linking（`/Gy`）は未設定のため、Layer2D の any symbol 参照は static lib の object 全体を引き込み LNK2019 になる。既存テストも Layer2D を参照していない。
- **今回の対応:** 新規 Composition/Transform/Time テストでは Layer2D 本体を外し、StaticTransform2D の定義済みメソッド・Registry・RationalTime・TimeRemap・MatteMode 契約のみにした。除外理由はテスト内コメントに記載。
- **価値／懸念:** Layer2D 所有権テストは定義追加後にしか書けない。本番側も Layer2D.cppm の symbol を参照した瞬間に同じリンク破損が出る。
- **次に確認:** ユーザー承認後に `toQTransform()`（回転・拡縮・アンカー・平行移動の合成順序）と `setTransform2D()`（リセット語義）の定義を追加し、Layer2D 所有権テストを復活させる。ビルド・テストは明示指示待ち。

### 2026-10-10 — Render Manager の visual fixture と queue restore 契約

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm::fromJson`、`Artifact/src/AppMain.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** `fromJson()` は保存済み queue の `Completed` / `Failed` 状態を読み込んだ後、復元した全 job を `Pending`・progress 0 に戻す。失敗の `errorMessage` は保持される。これは実行中状態を安全に復旧しないための既存動作である。
- **今回の対応:** Render Manager の visual fixture は実プロジェクトと composition を作り、待機ジョブ2件と attention 表示用 error 付きジョブ1件を使用する。`fromJson()` の復元安全性を変えずに選択・詳細・status 表示を撮影できる。
- **価値／懸念:** 画像比較が実際の composition/preflight 表示を通り、AppData queue の残存値にも依存しない。Completed / active progress の行表示はこのfixtureでカバーしない。
- **次に確認:** interaction testで完了・進行中の表示状態が必要になった時、永続化の復旧契約を弱めず、テスト専用の状態注入境界で表現できるか検討する。

### 2026-10-10 — Render Manager UI のロケール別翻訳カバレッジ

- **関連:** `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cppm`、`Artifact/src/Widgets/Render/ArtifactRenderQueuePresentation.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** `ArtifactUiTest --lang ja/en/zh/zh-TW` で撮影した同一 fixture の PNG は4言語とも同一 SHA-256 だった。現時点では Render Manager の表示がロケール変更を反映していない。
- **価値／懸念:** 言語別 UI テストを入れることで、ロケール起動の回帰と翻訳の未接続を区別して把握できる。モックは英語のみのため、他言語は英語モックとの差分で評価できない。
- **次に確認:** ユーザーが翻訳対応を依頼した段階で、Render Manager の静的ラベル・状態・ボタン文字列の翻訳経路を調査し、翻訳済み UI に同一言語の基準画像を用意する。

### 2026-10-10 — 3D粒子contractをoffline-render optionから単独登録する

- **関連:** `tests/Artifact/CMakeLists.txt`、`ARTIFACT_ENABLE_OFFLINE_RENDER_TEST`、`ArtifactOfflineRendererContractTest`。
- **確認できた事実:** Artifactのoffline renderer contract targetは `ARTIFACT_BUILD_TESTS` または `ARTIFACT_ENABLE_OFFLINE_RENDER_TEST` で登録される。初期の3D粒子contract登録は `ARTIFACT_BUILD_TESTS` だけに結び付いていた。
- **今回の対応:** 3D particle contractとproduction-layer captureを、既存offline-render optionからも有効にし、READMEに必要なGTest/GPU条件を記載する。
- **価値／懸念:** 全unit suiteを有効化せずに3D headless rendering gateを選択できる。CMake再構成は禁止中のため、登録条件はソース確認のみ。
- **次に確認:** ユーザーの明示指示後、offline-render option単独のconfigureと対象CTest登録を確認する。

### 2026-10-10 — headless particle drawはPSOを同期準備する

- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm::drawParticles`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm::ensureGraphicsPipeline/prepare`、`tests/Artifact/Particle3DRenderContractTest.cpp`。
- **確認できた事実:** offline rendererは `m_offlineWidth > 0` の間、particle draw時に `ensureGraphicsPipeline(data.options)` を呼ぶ。未キャッシュPSOはこのheadless経路で同期作成される一方、interactive経路は非同期準備を利用できる。
- **価値／懸念:** 独立headless testで最初のparticle frameが非同期PSO compile raceに負ける懸念を抑えられる。この前提はoffline draw分岐に依存する。
- **次に確認:** offline rendererの初期化・draw経路を変更した場合、同期PSO準備が保たれるか再確認し、外れるならtest fixtureで明示prewarmまたは完了待ちを行う。

### 2026-10-10 — 共有 renderer の粒子テストではカメラ状態を明示的に戻す

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ArtifactIRenderer::reset3DCameraMatrices()`。
- **確認できた事実:** 粒子 contract tests は同じ headless renderer を共有する。3D camera/model test と depth test は `set3DCameraMatrices()` を呼ぶ一方、後続の2D粒子ケースはカメラを戻しておらず、3D状態を残したまま描画する可能性があった。
- **今回の対応:** 2Dケースの描画前に3D camera matricesをresetし、描画後に `cameraMode=2d` と `depthTest=0` / `depthWrite=0` をassertし、3D depth casesでは送信完了後に期待するdepth flagsをassertする。3D camera/model移動、透視footprint、VelocityAligned、depth-order各caseでも `cameraMode=3d` を明示assertする。depthTest/depthWrite診断はpacket送信完了後に読む。readback画像の寸法が96×96でない場合はpixel走査へ進まず失敗させる。
- **価値／懸念:** 2D粒子の no-depth regression check が、テスト実行順やrenderer状態の偶然に依存しにくくなる。今回のAGENTS.md制約により、この差分のビルド・実行は未確認。
- **次に確認:** ユーザーがビルド・テスト実行を許可した後、3D contract suiteを複数回実行し、全ケースが実行され2D診断が成立することを確認する。

### 2026-10-10 — 3D粒子のheadless runnerはQt platformもoffscreenに固定する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`tools/Particle3DPlayground/main.cpp`。
- **確認できた事実:** 3D contract testとproduction-layer captureはいずれも `QGuiApplication` を生成する。既存のheadless renderer contract testでは同じGPU作業前に `offscreen` を設定している。
- **今回の対応:** GTest global environmentとcapture executableの両方で、`QGuiApplication` 構築前にQt offscreen platformを指定する。
- **価値／懸念:** GPU backendが利用できてもdisplay serverがない環境でQt初期化に失敗する可能性を下げる。Qt offscreen pluginがないホストでの実行は未確認。
- **次に確認:** Linux/Vulkanのdisplayなし環境とWindows/Vulkanでcapture executableを実行し、Qt初期化とDiligent device初期化の失敗を区別して確認する。

### 2026-10-10 — image-only particle runnerはQGuiApplicationに留める

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`tools/Particle3DPlayground/main.cpp`。
- **確認できた事実:** 3D contractとcapture runnerは画像・GPU描画のみを使い、QWidget APIを参照しない。既存の `ArtifactOfflineRendererContractTest` はheadless readbackに `QGuiApplication` を使っている。
- **今回の対応:** 3D test environmentとcapture mainを `QGuiApplication` に変更し、Widgets applicationの初期化を持ち込まない。
- **価値／懸念:** GUI-less GPU render testの初期化責務が実利用APIと揃う。Qt offscreen pluginがない環境については未確認。
- **次に確認:** headless test executableをdisplay serverなしで起動し、Qt GUI platformとDiligent device初期化の両方を確認する。

### 2026-10-10 — 3D particle seek回帰にもフレーム再訪を含める

- **関連:** `tools/Particle3DPlayground/main.cpp`、2D particle firework capture。
- **確認できた事実:** 2D firework captureは連続再生、直接seek、60→10→45再訪の画像を比較している。3D production captureは連続再生と直接seekだけを比較し、同一layerの非単調なframe移動は未検査だった。`ArtifactParticleLayer::draw()` は `simulationReusable` を保持し、`ParticleSystem::goToFrame()` は同一fps・未来フレーム・120Hz完全step境界でのみsimulationを再利用する。60→10はresetし、その後10→45は条件が合えば再利用経路を通る。
- **今回の対応:** production `ArtifactParticle3DLayer` の別instanceで60→10→45を描画し、frame45 PNGとpixel/channel差をレポート・許容値判定へ加える。
- **価値／懸念:** 履歴依存の状態更新が非単調seek後に再現性を壊す回帰を検出できる。GPU runtimeでの再現性は未確認。
- **次に確認:** VulkanとD3D12で再訪画像が連続再生frame45と決定論的許容差内に収まるか実行する。
- **追補:** 再訪のframe 60と10それぞれで、生存粒子数・`state=queued`・`cameraMode=3d`も確認するようにし、途中のGPU draw不成立を識別する。

### 2026-10-10 — Particle depthWrite=falseを後続geometryで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticlePkt` 深度attachment、3D card PSO。
- **確認できた事実:** 既存の前後関係テストは粒子のdepthTestを確認し、production captureはdepthWrite=falseの設定で手前カードによる遮蔽を見る。ただし、粒子がdepth bufferへ書き込まず、後続のgeometryが粒子より奥でも描画できることは独立に確認していなかった。
- **今回の対応:** `depthTest=true, depthWrite=false` の前景particleを描画・readbackした後、depthWrite=trueの後景cardを描画する。最初の画像で粒子色、最終画像でcard色を別々にassertする。
- **価値／懸念:** 粒子PSOのDepthWriteEnable契約と後続3D geometryとの相互作用を画像で検査する。実GPUでのpixel結果は未確認。
- **次に確認:** Vulkan/D3D12で前景particleが先に出て、後景cardが最終pixelを覆うことを実行確認する。
- **追補:** 深度helperは各readback画像の寸法を確認してから中心pixelを読む。GPU初期化後のreadback失敗はzero pixelのassertion failureとして扱い、invalid imageへ直接アクセスしない。

### 2026-10-10 — VelocityAlignedの速度方向を画像bboxで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleRenderer` vertex shader、3D particle milestone P4-4。
- **確認できた事実:** shaderは速度をview spaceへ変換し、VelocityAligned policyのとき画面上速度方向から回転角を加える。円形particleでは回転差が見えないため、stretch付き粒子を使えば姿勢を画像bboxで区別できる。
- **今回の対応:** 正面カメラでX速度/Y速度を、傾斜カメラでワールドX速度をそれぞれGPU readbackし、cameraMode=3dを診断確認したうえで、正面Xは縦長・正面Yは横長・傾斜Xは投影された画面速度に沿う横長bboxとなることをcontract化する。深度test/writeを明示的に無効化し、全caseのGPU position/velocity xyzも明示初期化する。
- **価値／懸念:** マイルストーンに残るVelocityAlignedのruntime gateを自動画像テストへ取り込む。Vulkan/D3D12上の実bboxは未確認。
- **次に確認:** 正面と傾斜カメラの全3画像で粒子pixelが存在し、縦横bboxが期待どおりになることを各backendで実行確認する。

### 2026-10-10 — 3D粒子のモデルZ移動は透視投影サイズで検査する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleRenderer.cppm` のvertex shader。
- **確認できた事実:** Particle vertex shaderはrow-majorのModelRowを使って粒子位置をmodel→viewへ変換し、screen-aligned billboard offsetをview-spaceへ加えてからprojectionする。テストのmodelMatrix[3]/[11]はX/Zの行translationに対応する。従って、同じ小粒子をカメラ方向へZ移動すると画面上のfootprintが増えることを、GPU readbackで契約化できる。
- **今回の対応:** 独立3D contractに、model Z=0とZ=1の画素数比較を追加。既存のX移動テストを保ち、Z移動と粒径をhelper引数で指定できるようにする。
- **価値／懸念:** 3Dテストが画面上の平行移動だけでなく奥行きと透視スケールも検査する。実GPU上での差分安定性は未確認。
- **次に確認:** VulkanとD3D12でnear/far pixel countの両方が非ゼロで、nearがfarを上回ることを実行確認する。

### 2026-10-10 — 3D particle contractもGPU位置xyzを明示初期化する

- **関連:** `tests/Artifact/Particle3DRenderContractTest.cpp`、`ParticleVertex::px/py/pz`。
- **確認できた事実:** `ParticleVertex` のposition成分はdefault member initializerを持たない。2D particle GPU画像テストの調査で、未設定の `pz` がGPU射影後の描画欠落を起こした実例がある。
- **今回の対応:** 3D移動helperとdepth-order helperで `px/py/pz` をそれぞれゼロに明示設定し、Z変化はmodel matrixだけから与える。
- **価値／懸念:** vector element constructionの値初期化規則や将来の生成方法変更に依存せず、テストの意図する座標が固定される。
- **次に確認:** GPU実行時に3D移動・透視footprint・深度ケースが引き続き粒子を描画することを確認する。

### 2026-10-09 — ParticleLayerのGPU画像出力とflipbook経路

- **関連:** `tools/Particle2DPlayground/main.cpp`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`ArtifactCore/src/Graphics/ParticleRenderer.cppm`。
- **確認できた事実:** NVIDIA RTX 4070 Ti / Vulkan の headless readbackで矩形8836画素、明示初期化済みの粒子44324画素を確認した。先行テストの粒子は `ParticleVertex` の `pz` を設定しておらず、射影後Zが不定だった。テスト側で値初期化し `pz=0` にするとproduction shaderのまま表示されたため、ParticleRendererの射影修正は不要だった。Emitter `texturePath` は連番ディレクトリとsprite sheetの両方を既存software rendererが読めるが、通常GPU shaderには画像SRVがない。
- **今回の対応:** ArtifactParticleLayerは画像ソース付き2D emitterを既存のcached-image経路へ送る。通常ビューはQImage spriteをGPU rendererへ渡し、GPU offscreen surfaceは既存呼出側のsoftware fallbackへ戻す。連番と4×4 sheetをGPU render target経由で自動キャプチャし、各5フレーム、双方の動き、および画素差8以下／channel差2以下を検査。確認結果は5時刻中4時刻が完全一致、残りは1画素・最大channel差1。
- **価値／懸念:** 画像粒子は通常Viewportでも表示可能になった。texture sampling自体はGPU ParticleRendererに追加していないため、画像付き大量粒子ではsoftware rasterと動的sprite uploadのコストがかかる。GPU native texture samplingへの置換は未検証。
- **次に確認:** 実際のViewport上で連番／sheetの見え方と多数粒子時のframe costを確認し、必要なら既存texture cacheを使うGPU sprite pathの設計を比較する。

### 2026-10-09 — 風の動きは葉プリセットとEmitter物理値で独立確認できる

- **関連:** `tools/Particle2DPlayground/main.cpp`、`ParticlePresets::leaves()`、`ArtifactParticleLayer::setLayerPropertyValue()`。
- **確認できた事実:** 葉プリセットは連続発生・回転をすでに持ち、公開 property path から wind direction／strength、turbulence amplitude／frequency／evolution、drag を設定できる。Playground の自動キャプチャで30〜150フレームの5枚を保存し、先頭と末尾で7386画素が異なることを確認した。
- **価値／懸念:** 花びらのスプライトアニメとは異なる、Emitter物理による軌道変化を小さなオフライン例で確認できる。PNGは現状、葉テクスチャではなく既存の円形particle描画である。GPU経路での同じ動きとスプライト表示は未検証。
- **次に確認:** GPU粒子画素の欠落原因を特定した後、連番画像を載せた葉のWind/Turbulence表示をGPU／software両経路で比較する。

### 2026-10-09 — ACEScc zero encode/decode の逆変換不一致

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToACEScc` / `acesccToLinear`、`tests/ArtifactCore/ColorBridgeTest.cpp`。
- **確認できた事実:** `encode(0, ACEScc)` は定数 `-0.3584474886f` を返すが、その値を `decode` へ渡すと約 `131072` になる。非ゼロの密なscene/HDR格子ではこの zero sentinel を除外すると往復を検査できる。ArtifactCore は子リポジトリなので本作業では実装を変更していない。
- **価値／懸念:** ACEScc の黒コードが公開decode式と逆変換になっておらず、黒を含む encode→decode 処理は大きな正値を生成する可能性がある。定数の丸め誤差が指数で増幅されている可能性はあるが、原因と規格上の期待値は未検証。
- **次に確認:** ACES ST 2065-4 の負値／black code 規約と定数精度を一次資料で照合し、ユーザーが ArtifactCore 修正を依頼した場合に専用の黒境界回帰テストと最小修正を行う。

### 2026-10-09 — Rec.709 OETF/EOTF 折れ点の非整合

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToRec709` / `rec709ToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 独立した倍精度参照と全1,024個の正規化コード値を照合すると曲線本体は約4e-7以内だった。一方、OETF の線形折れ点 `0.018` を高側式へ入れた値は約 `0.08124793` だが EOTF は `0.081` を境界としており、境界ちょうどで `0.017945` 付近へ戻る。両端の折れ点が一致していない。Rec.2020 の同じ全コード比較は約4e-7以内で通る。
- **価値／懸念:** Rec.709 のブレークポイント近傍に明確な段差・往復誤差がある。原因は実装にあるしきい値 `0.081` と OETF の係数から生じる `0.0812479...` の差と見られるが、採用する規格定数と許容誤差は未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** 参照する BT.709 版と規格上の breakpoint 定義を一次資料で確認し、ユーザーが ArtifactCore 修正を明示した場合に OETF/EOTF 境界の最小修正と回帰テストを行う。

### 2026-10-09 — sRGB OETF breakpoint直後の微小な下向き段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToSRGB`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `0.0031308f` とその次の表現可能floatを別々の枝へ通すと、OETF出力が約 `0.040449936` から `0.040449910` へ約 `2.7e-8` 下がる。倍精度の区分式参照との比較でもこの値を再現し、隣接float専用テストに現在の段差をcharacterizationした。
- **価値／懸念:** 数値としてはごく小さいが、OETFの厳密な単調性を破る。丸められたbreakpointと係数による既知の標準近似として許容するか、分岐点／係数を調整すべきかは未検証。ArtifactCore は子リポジトリなので本作業では実装を変更していない。
- **次に確認:** IEC 61966-2-1 の採用定数と出力精度要求を一次資料で確認し、ユーザーがArtifactCore修正を明示した場合に最小の境界修正と回帰テストを行う。

### 2026-10-09 — PQ float 実装の全10bitコード精度

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`ColorTransferFunction::linearToPQ` / `pqToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** SMPTE ST 2084 の独立倍精度式に対し、正規化された全1,024コード値の OETF/EOTF を比較した。現在の float 実装は最大絶対差が OETF 約 `9.94e-6`（code 274）、EOTF 約 `3.85e-5`（code 1022）。テストではこの観測差を少し上回る `1.1e-5` / `4.0e-5` を回帰上限としている。
- **価値／懸念:** PQ EOTF の高コード側で線形値の差が OETF より増幅される。これが許容可能な float 実装誤差か、要求精度に対して不足かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** 用途別のPQ精度要求と参照規格の試験点を確認し、ユーザーが ArtifactCore 修正を明示した場合に係数・演算精度の最小変更を検討する。

### 2026-10-09 — Canon Log 3 low toe 境界の段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`linearToCanonLog3` / `canonLog3ToLinear`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** low encoded threshold `0.04076162` から計算した線形 low toe 境界の直下では OETF が約 `0.04076`、境界ちょうどでは middle affine branch に入り約 `0.04700` を返す。EOTF も encoded threshold の直下で約 `-0.0113`、threshold ちょうどで middle branch の約 `-0.0140` に切り替わる。独立テストに全10bitコードの照合と、この low toe の枝切替を固定する characterization を追加した。
- **価値／懸念:** Canon Log 3 low toe の encode/decode に目立つ不連続がある。式の係数・境界の取り違えか、負値域の設計仕様かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** Canon Log 3 の公開仕様にある low-end toe と負値範囲を一次資料で照合し、ユーザーが ArtifactCore 修正を明示した場合に最小修正と回帰テストを行う。

### 2026-10-09 — Canon Log 2 負側 toe 境界の OETF 段差

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`linearToCanonLog2`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** 負側 toe 境界 `-(10^(toe/slope)-1)/scale` の直下は負値用式で 0 付近を返し、境界ちょうどは通常式 `slope*log10(linear*scale+1)+toe` に切り替わって約 `-0.0146` になる。回帰テストでは両枝の値と段差を characterise した。Cineon の black code は式上 `95/1023` と確認した。
- **価値／懸念:** Canon Log 2 の負値 toe に不連続がある。正値域の通常コードでは目立たない一方、負値・scene-linear入力を通すと境界に段差を生む。負値処理が規格要件かは未検証。ArtifactCore は子リポジトリのため実装変更はしていない。
- **次に確認:** Canon Log 2 の公開仕様で負の scene-linear 値と toe 境界の規約を確認し、ユーザーが ArtifactCore 修正を明示した場合に最小修正する。

### 2026-10-09 — ACESlog enum の汎用変換 dispatch 未接続

- **関連:** `ArtifactCore/include/Color/ColorTransferFunction.ixx`、`TransferFunction::ACESlog`、`ColorTransferFunction::encode/decode`、`tests/ArtifactCore/ColorTransferFunctionStandaloneTest.cpp`。
- **確認できた事実:** `TransferFunction` enum に `ACESlog` があるが、汎用 encode/decode の switch に専用 case はなく default の linear identity を返す。独立テストに現在の identity fallback を固定した。ACESlog の専用 OETF/EOTF を提供する実装はこのインターフェースで確認できなかった。
- **価値／懸念:** UI や呼び出し側が enum を選んでも ACESlog 変換は行われず、入力値がそのまま返る。これは未実装か意図的 fallback か未検証であり、テストは現状の dispatch を characterization している。
- **次に確認:** ACESlog の対象規格・必要な用途と UI 露出有無を確認し、ユーザーが ArtifactCore 機能追加を明示した場合に専用変換と独立参照テストを追加する。

### 2026-10-09 — XYZ_D60 gamut enum が Bradford dispatch に未接続

- **関連:** `ArtifactCore/include/Color/ColorGamutConversion.ixx`、`Gamut::XYZ_D60` / `getConversionMatrix()`、`tests/ArtifactCore/ColorGamutConversionTest.cpp`。
- **確認できた事実:** `XYZ_D60` と `XYZ_D65` の両方向変換は `getConversionMatrix()` で identity となる。Bradford 判定が `ACES_AP0` / `ACES_AP1` のみを D60 source/target として扱い、`XYZ_D60` は含めていない。D60白 `(.9526461, 1, 1.0088252)` は D65へ変換しても不変で、D65白から逆方向も変化しない。独立テストに現行 fallback を characterization として追加した。
- **価値／懸念:** 色域 enum の名前と実際の white-point routing が一致せず、D60↔D65 XYZ 変換ではBradford adaptationが省略される。ArtifactCore は子リポジトリのためこの作業では実装を変更していない。
- **次に確認:** `XYZ_D60` を単なるXYZ座標系タグとして扱う設計か、D60 white-point adaptation 対象として扱うべきかを仕様確認し、ユーザーが ArtifactCore 修正を明示した場合に最小 dispatch 修正を行う。

### 2026-10-09 — ArtifactUiTest の QSettings 分離境界

- **関連:** `Artifact/src/AppMain.cppm`、Artifact 内の `QSettings(organization, application)` 呼び出し。
- **確認済み:** Qt の公式 API 仕様では `setDefaultFormat()` は既定コンストラクタにのみ効き、組織名・アプリ名を明示したコンストラクタは `NativeFormat` を使う。Windows では `NativeFormat` の `setPath()` も効かない。UI Test 起動の `QStandardPaths::setTestModeEnabled(true)` は AppData／AppConfig 等の標準書込先を test 領域へ変えるが、明示的な QSettings コンストラクタの Windows Registry 保存先は分離しない。
- **価値／懸念:** 現状の UI Test exe は実画面を独立起動できる一方、これらの明示設定を読む／書く画面は通常版と同じ Registry 設定を共有する。完全な設定分離を行うなら、QSettings 呼出側の共通 factory 化などが必要で、起動処理だけの変更では実現できない。
- **次に確認:** UI テストで通常版の既存設定を読む・変更することが許容されるかを決め、完全分離が要件なら明示コンストラクタ呼出箇所を一括で扱う小さな設定アダプタの設計を検討する。

### 2026-10-09 — Filmstrip 未取得フレーム生成の実行経路

- **関連:** `Artifact/src/Widgets/ArtifactFilmstripWidget.cppm`、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`、`Artifact/src/Service/ArtifactPlaybackService.cppm`。
- **確認済み:** Filmstrip は RAM キャッシュの縮小表示とパネル内の onion skin／比較を提供する。既存の offscreen renderer の `renderFrame(position, composition)` は position をレイヤーの active 判定に使うが、各レイヤーを指定フレームへ評価していない。共有レイヤーを描画するため、そのままワーカーへ渡す根拠もない。Playback Service の build queue は単一で、Filmstrip が置き換えると他のプレビュー要求に干渉する。
- **未検証の案:** 未取得分の独立生成には、既存 GPU submission の所有経路で固定上限の要求を処理し、composition revision と表示世代で古い結果を破棄する仕組みを検討する。
- **次の確認:** 共有レイヤー評価状態と immediate context の所有者を確認し、表示中のフレームを変更せずに生成できる経路を選ぶ。

### 粒子 — GPU第一経路化・非同期PSO・ディスクPSOキャッシュの実機確認

- **関連:** `ArtifactCore/src/Graphics/ParticleRenderer.cppm`（非同期ワーカー・PSOキャッシュ・ディスクregistry）、`Artifact/src/Layer/ArtifactParticleLayer.cppm`（trail送出・GPU面）、`Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **確認すること:** 層追加の固まり解消、初回CPU→GPU切替の継ぎ目、trail／stretch／エフェクト付き表示、2回目起動での待機診断消失とキャッシュファイル生成、Vulkanでの96バイト stride。

### 2026-10-09 — 粒子キャプチャはレイヤーのフレーム同期経路を通す

- **関連:** `tools/Particle2DPlayground/main.cpp`、`Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Generator/ArtifactParticleGenerator.cppm`。
- **確認済み:** `renderFrame()` に大きな時間差を直接渡しただけでは burst が出ず、空PNGになった。`ArtifactParticleLayer::draw()` は `goToFrame()` で決定論的な粒子状態を同期してからソフト描画へ進む。Playground の自動キャプチャもこの既存経路を通すと粒子があるPNGを出力できた。
- **価値／懸念:** 自動画像テストが空画像を成功扱いしないよう、粒子数・保存成否を検証する必要がある。現在の `--capture-firework` はソフト描画経路のみで、GPU readback と連続再生／seek間の画素一致は未検証。
- **次に確認:** 同じ burst を GPU と software で保存し、連続再生・直接seek・巻き戻し後の同一フレーム画像を比較する。

### 静止画・連番画像 — GPU cache と実素材の再生／出力確認

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`、`Artifact/src/Render/GPUTextureCacheManager.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** 4K連番、欠番、Time Remap、再リンク、Preview／Render Queueでフレーム・GPUメモリ・出力が一致すること。

### Shape — Path keyframe／Merge Paths／SVG出力の実機確認

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **状態:** GPU／互換フォールバック／boundsの同期は実装済み。SVGのグラデーション、stroke taper／alignは未対応。
- **確認すること:** 複数subpathと各Merge Paths mode、Path keyframe、GPU／出力SVGの一致。

### Shape — 複数コンテンツ／GPUベクター描画の実機確認（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`ShapeContent`、`paintGpuPaintItems`、`resolveContentVisPaths`、`renderContentsToImage`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証（ビルドは指示待ちのため未実行）。
- **内容:** 1レイヤー複数パス（形状＋塗り＋線＋表示＋結合モード、空＝従来動作）。グラデーションは三角形重心サンプリング、Inside／Outsideはハーフオフセット＋中央線、テーパー／勾配線はセグメント分割でGPU描画し、viewportのQImageスプライト分岐を撤去。結合はCPU側QPainterPath真偽値演算で解決し、スタイルは保持。物理グリッド・3Dカード高速パス・オペレータキー評価は従来のまま。
- **確認すること:** 既存単一シェイプの見た目不変（solid高速パス・operator分岐は温存）、グラデーション／align／taperのGPU描画、Subtract／Intersect／Differenceの穴・境界線、連番・サムネイル・SVG出力、Repeater大量複製時の負荷。
- **既知の近似（未検証の仮説ではない仕様）:** テーパー線の結合部は Butt 重ね、Roundキャップは矩形延長近似、dash＋taper併用はdash優先、コンテンツのパス頂点アニメは未対応（静的）。

### Shape — 沿路グラデーション線／ダッシュオフセット（2026-09-03）

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`（`PolylineStyle`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawStyledPolyline`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `PolylineStyle`に`gradientEnabled/gradientStart/gradientEnd/dashOffset`を追加。`drawStyledPolyline`は累積長パラメータでセグメント・ダッシュ・結合・キャップを沿路補間色で描画し、dash位相は`dashOffset`の剰余で解決。レイヤー側は`shape.dashOffset`（アニメ可）＋コンテンツ別`dashOffset`、勾配のみの線は taper 分割器ではなく`drawStyledPolyline`経由に変更（結合・キャップ・dashと合成可）。QImage互換・SVG出力（`dashOffset`のみ）・保存も配線。
- **確認すること:** 既存実線の見た目不変（新フィールド既定で旧経路と同一）、勾配＋dash＋round結合の合成、負offset・巨大offset、マーチングアンツのキーフレーム補間。
- **既知の近似:** taper＋dash併用はdash優先でtaper無効、勾配サンプリングは線形補間。

### Shape — SVG相互運用（取込・書出）（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`SvgImport`、`shapeContentsToSvg`、`parseShapeContentsFromSvg`、`addShapeContentsFromSvg`、`importSvgFileContents`）。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** 書出は結合解決済みパス＋塗り／線／dash／fill-ruleを`<path>`＋`linear/radialGradient` defsで出力（taper線→通常線、conical→単色、勾配線→中間色に縮退）。取込は`path(d全命令・Aはベジェ化)`・rect（角丸可）・circle・ellipse・polygon・polyline・line＋線形／円形グラデーション（前方参照可）＋transform bake＋継承スタイルを編集可能コンテンツ化（座標はbounds正規化、256件cap、64MB cap）。`ClipboardManager`は未変更で、受渡し自体は素のSVGテキストを呼出側に委譲。
- **確認すること:** Illustrator／Figma出力SVGの往復、userSpace勾配・奇数dash・相対命令・指数表記の数値、空・不正SVG（0件／-1）、既存JSON互換。
- **既知の近似:** 複数subpathは1要素に統合（線描画で連結線が出る）、3 stops以上は両端のみ、gradientTransform・非等方scale下の線幅・group fill-opacity継承は近似、strokeのurl()は勾配線として解決（fillのみ前方参照対応だった点をstore側で統一）。

### Shape — コンテンツ編集サポート（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`activeContentIndex_`、`ShapeContentProxy`、`duplicateShapeContent`、`moveShapeContent`、`insertShapeContent`、`swapShapeContents`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `activeContentIndex_`（-1 = レガシーモード）と`ShapeContentProxy`（`ArtifactShapeLayer*` + index）を導入。Proxyは`name`/`visible`/`opacity`/`merge`/`fill`/`stroke`/`geometry`/`duplicate`を`setShapeContentAt`経由で直接編集し、PropertyEditorは`shape.activeContentIndex`で操作対象を切り替える。複製（挿入位置にコピー）、挿入、`move`、`swap` APIを追加。`shape.content.<i>.type/width/height/cornerRadius/starPoints/starInnerRadius/polygonSides/fillRule` を `setLayerPropertyValue` で直接編集可能に拡張。JSONシリアライズに`activeContentIndex`を含む。
- **確認すること:** Proxyのスワイプ（他のインデックス参照）、move/swap後のbounds・visPaths再構築、JSON往復、PropertyEditorでのアクティブコンテンツ切り替え時の描画反映、contentジオメトリ編集時の再構築・保存。

### 2.5D — 局所DOF／motion blurの品質と負荷

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/Artifact{Image,Shape,Text}Layer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** Image／Shape／Textで深度・focus・shutterを変え、alpha順、極端なblur、再生時のGPU負荷、書き出し結果を確認する。

### Gobo runtime texture — 将来接続の安定ID境界

- **関連:** `ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`。
- **状態:** ファイルGoboを置換できるruntime SRV入力は実装済み。Image Layerとの接続・UI・保存は未実装。
- **判断待ち:** 接続を始める時点で、packed scene-light slotではなく安定したLight IDとresource revisionを対応付ける。

## 現在の設計判断

### Semantic Debugger — 外部 `ArtifactDebugger.exe` を正規UI境界とする

- **関連:** `docs/planned/MILESTONE_EXTERNAL_SEMANTIC_DEBUGGER_2026-09-02.md`、既存のMCP／Trace／Shared Memory IPC診断基盤。
- **状態:** 未着手。設計判断をマイルストーン化。
- **判断:** ArtifactStudio本体には低コストの `ArtifactDebugRuntime`（semantic identity、mutation provenance、frame snapshot、safe-point制御）だけを置き、意味表示・原因解析・timeline・semantic breakpointのUIは外部プロセスへ分離する。既存MCPはAI専用に作り直さず、Debuggerとheadless harnessが共有するread／control protocolの基盤として再利用する。
- **価値／懸念:** 本体のQt／レンダリング状態をデバッガUIから隔離し、VS native debuggerとの併用、実行中Attach、Debugger単独更新を可能にする。一方、protocol version、履歴欠落の「未観測」表示、frame boundaryでのpause、snapshot復元とdeterministic replayの境界が必要。
- **次に確認すること:** Phase 0で既存MCP TCP／QLocalSocket／Named Pipeの接続候補、共有されるsemantic schema、diagnostic buildと通常buildのruntime有効化方針を確定する。

### Layer modulation は opacity以外へ拡張しない

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Audio/Modulation/Router.ixx`。
- **状態:** opacityの評価、保存、Undo基盤は実装済み。Inspector導線とruntime確認は未完。
- **判断:** Transformはvariant／physics／layoutとの評価順を定義するまで追加しない。

### 物理 — rigid joint／polygon colliderの動作確認を先行する

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **状態:** jointとpolygon colliderの導線は実装済み、runtime未検証。
- **確認すること:** 重力の画面座標符号、scrub復元の制約、凹形状の凸近似、SoftBody／MPMとの接触。

### 3D rendering — AOVと実GPU契約は別スライスに保つ

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`。
- **状態:** World Position AOVのtarget基盤はあるが、書き込みPSO／runtime検証は未実装。
- **判断:** 2D／2.5Dの完成度を優先し、AOV、GPU skinning、ray tracingは個別の受入条件を定義してから進める。

### プロキシ生成 — Out-of-Process 専用ワーカー (ArtifactProxyWorker) 方式の採用

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`、`Artifact/include/Proxy/ProxyService.ixx`。
- **状態:** `ArtifactProxyWorker.exe` の native / Media Foundation / ffmpeg 経路と JSON Lines 通知を実装。native は実エンコーダー名と検出候補も返す。host は jobId/outputPath/outputBytes を照合し、Project View から queue キャンセルも可能。Eighth、音声再エンコード、hardware encoder、auto fallback、staged package の worker 実機確認済み。Media Foundation は H.264 入力で成功するが、ProRes 入力は非互換で、極小出力は 64px/axis にクランプする。成功・失敗・キャンセル時の partial cleanup と、検証ツールの worker timeout 回収も確認済み。host UI 統合の実機確認は未完。
- **判断:** 本体プロセスの安定性（クラッシュ・OOM 巻き添え防止）と FFmpeg C API 直接利用（進捗通知・GPU HW エンコード）を両立するため、専用の子プロセスワーカー（`ArtifactProxyWorker`）を設けて非同期 IPC で連携する構成を正規方針とする。

### Proxy worker — 成果物の原子性と timeout 回収を同じ受入条件にする

- **関連:** `Artifact/src/Worker/ArtifactProxyWorker.cpp`、`tools/proxy_worker_smoke_test.py`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **状態:** 実装済み、host UI実機確認待ち。
- **判断:** worker の exit code だけでは成功とみなさず、completed message、final output、partial cleanup、timeout／強制終了後の状態を一組で検証する。これにより、ハングや中断を有効な proxy と誤認する経路を受入段階で検出できる。

## 保留中の設計判断

### シェイプレイヤー VP 操作の増強範囲（2026-09-02 ユーザー質問）

- **質問:** 「シェイプレイヤーのVP操作機能増強いけそうか」
- **解釈:** `VP = メインコンポジション Viewport`（`taste.md` の communication-integrity 規範に従い grep で `Viewport`/`TextViewport` へ展開、コード上に独立した `VisualProgramming` 系は無いため）。
- **現状（コード読みで確認済み、未検証含む）:**
  - `Artifact/src/Tool/ArtifactToolManager.cppm:44-46` に `ToolType::Shape / Rectangle / Ellipse` が定義され、`ToolType::Shape` は `ArtifactToolService` で `Shape modeling` 入口と紐付け済み（`docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:258`）。
  - `Artifact/src/Layer/ArtifactShapeLayer.cppm` は `ShapeType`（Rect/Square/Ellipse/Star/Polygon/Triangle/Line）+ `customPolygonPoints_` + `CustomPathVertex { pos, inTangent, outTangent, smooth }` + `customPathClosed_` を保持し、`evaluatePathAt(frame)` でパス頂点キーフレーム評価を実装。
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のシェイプ系 VP ハンドラ:
    - `mousePress` L22853-22923 で Rectangle/Ellipse/Shape ツールのドラッグ → 矩形/楕円/スター/ポリゴン/三角のシェイプレイヤー新規作成（選択レイヤー有りは mask 経路、無しなら `RectangleToolMode::Shape/EllipseShape/StarShape/PolygonShape/TriangleShape` で `ArtifactShapeLayer` を生成、L27803-27873）。
    - `mousePress` L22940-22991 で Pen ツールが Shape レイヤー選択時のみマスクではなくカスタムパスを `pendingShapePathVertices_` へ追加（開始点クリック or Enter で確定、Backspace で取消、Escape 取消）。
    - `mousePress` L23527 `beginShapePathVertexDrag`、`updateShapePathVertexDrag` L25095、`endShapePathVertexDrag` L27345 経由でカスタムパスの頂点/タンジェントドラッグ編集。`ShapePathVertexEditCommand` で Undo。
    - `mousePress` L23506-23527 で Line シェイプの端点ドラッグ (`isDraggingLineEndpoint_` / `draggingLineLayer_`) を実装。
    - `isDraggingShapePathVertex_` / `shapePathEditPending_` / `shapePathEditDirty_` / `hoveredShapePathVertex_` / `hoveredShapePathTangent_` / `draggingShapePathTangent_` (0=vertex / 1=in / 2=out) を state に保持 (L12488-12505)。
  - `ArtifactCompositionRenderOverlay.cppm` のシェイプレイヤー描画: L1028-1105 でカスタム Polygon の頂点ストローク描画、Line の 2 端点描画、customPathVertices のベジェ描画を実装。ただし **頂点ハンドル/タンジェントハンドル/選択ハイライト/セグメント挿入マーカーのオーバーレイ描画パスは未確認**（grep 上このファイルには vertex overlay / hit-area / ハンドルサイズ定数が Shape 用に出てこない）。
  - `ArtifactRenderLayerWidgetv2`（LayerEditorPanel 内、`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:255-279`）に vertex / segment / tangent のコンテキストメニュー・Ctrl-click 選択追加・Shift toggle・numbering・hover/select 表示・path vertex duplication・polygon vertex duplication・segment insert 経路がある。`MILESTONE_LAYER_EDIT_2026-04-25.md:209-219` で `customPolygonPoints` を `CustomPathVertex` へ拡張済み。
- **増強候補（メイン VP 視点で未着手／不足）:**
  - (1) シェイプ専用 vertex/tangent/segment overlay 描画の RenderOverlay 統合（`LayerEditorPanel` の機能をメイン VP へ移植した残骸: `INSIGHT_ARCHIVE_2026-09-01.md:4549-4551` の懸念「ビルド未実施。タンジェント smooth 反射の長さ保存比、パスキーフレームの UI は次段階」）。
  - (2) Rect の `cornerRadius` ハンドル（`hitTestCornerRadiusHandle()` は `ArtifactRenderLayerWidgetv2` にあり、メイン VP 側は `setSize()` を width/height ドラッグで更新する経路のみ、`MILESTONE_LAYER_EDIT_2026-04-25.md:60` に「ShapeEditCommand 同様」とあるが RenderController 側 grep で未確認）。
  - (3) Star の `starInnerRadius_` ハンドル、Polygon の頂点ドラッグ挿入。
  - (4) `ToolType::Shape` のプリセット図形選択 UI（Rect/Ellipse/Star/Polygon/Line/Triangle のアクティブ切替。現状シェイプ作成は `Shape` 単独か `Rectangle/Ellipse` ツールの `rectangleToolMode_` 切替のみで、Panel 上にプリセット導線なし）。
  - (5) シェイプ operator stack（TrimPaths/Merge Paths/Offset/Pucker/Rounded/Wiggle/ZigZag/Twist/HandDrawnWobble/Stroke taper、9種実装済）のシェイプ VP 上インスペクタ／数値ハンドルドラッグ編集。
  - (6) パスの open/closed トグル、smooth toggle、corner ↔ bezier 切替を VP ハンドルで（`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:243-245` の selection grammar 整備と並ぶ）。
  - (7) シェイプレイヤー選択時の頂点/セグメント/タンジェントの選択 grammar をメイン VP 上で完成させ、`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:11-12,29` で計画中の `Artifact.Widgets.LayerEditor.ShapeOverlay` / `ShapeEditSession` / `ShapeHoverController` へ接続する。
- **価値／懸念:** AE 互換のシェイプ編集（特に頂点ドラッグ・tangent smooth・polygon segment insert・operator ハンドル）は既存の描画・データ層を破壊せずに機能を乗せられる層が既に厚く、メイン VP 側の実装ギャップはおおむね UI と routing 追加で済む。一方で (1) RenderOverlay への新規シェイプ専用 HUD 描画と `(7) ShapeEditSession` 抽出は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` と相互作用し、`ArtifactCompositionRenderController.cppm` 28214 行・`ArtifactShapeLayer.cppm` 3380 行・`ArtifactCompositionRenderOverlay.cppm` 1827 行という巨大ファイル状態では変更影響範囲の見積もりが難しい。`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md` の "incremental / stable" 方針に従い、まず最小スライスで 1 機能ずつ上げるのが安全。
- **次に必要なユーザー判断:**
  1. スコープ: 既存の `MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` Phase D（キャンバス頂点編集）と Phase E（複数シェイプ）をそれぞれ独立に進めるか、または一括で (1)〜(7) をフェーズ計画に起こすか。
  2. 編集ホスト: メイン VP で直接編集（既存 `ArtifactCompositionRenderController` を拡張）か、`ArtifactRenderLayerWidgetv2`（LayerEditorPanel）側に集約して「ソロビュー」相当の編集ペインにするか。
  3. データモデル: 現状の単一 primitive を維持して `ShapeType` をツールプリセットにマップするか、コア `ShapeGroup` ベース（Phase E）へ移行してから VP 操作を実装するか。

## 検証運用

- **2026-09-04 — AI セグメンテーションの Core 契約:** `ArtifactCore/include/AI/ImageSegmenter.ixx` と `ArtifactCore/include/Image/DepthMap.ixx`。**事実:** 既存の `applySegmentationMask()` は空実装で、推論結果を書き込む `DepthMap` API がなかった。**対応:** 推論を `IImageSegmenter`（正規化された1ch前景マスク出力）へ限定し、Core 側で bilinear resample、閾値・softness・反転、alpha乗算／置換を適用する共有契約を実装した。`refineSegmentationMask()`で閾値／softness、foreground expand／contract、featherの共通後処理を追加し、`segmentBatch()`で複数の静止画／フレームから非破壊マスクを一括生成できるようにした。`analyzeSegmentationMask()`は foreground coverage／平均信頼度／bounds を返し、空マスクや過大マスクを App 側で警告できる。連番では `stabilizeSegmentationMask()` が前フレームマスクを控えめに混ぜ、推論のちらつきを抑える（動き追従は行わない）。モデル未配置時は、非AI・低品質であることを明示した `LuminanceImageSegmenter` を高コントラスト素材用のフォールバックとして追加した。**価値／懸念:** ONNX／DirectML、CPU fallback、将来のGPU推論はいずれも同一結果型に接続できるが、実モデル・モデル資産契約・GPU経路／実機品質は未検証。**次に確認:** 人物セグメンテーションモデルを1つ選定し、静止画の alpha 結果と既存 GPU mask 合成の preview／export parity を確認する。

- **2026-09-04 — ONNX/DirectML セグメンテーションアダプタ:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**事実:** 既存のONNX DirectML実装はテキスト生成専用で、画像モデルの入力・出力を `IImageSegmenter` に正規化する実装がなかった。またONNX compile definition/link は `ArtifactCoreAI` ではなく親 target にのみ付与されていた。**対応:** NCHW float 入力、最終2次元をマスクとするfloat出力のONNXモデルを、DirectML優先で読み込み、`DepthMap`へ戻すアダプタを追加。入力RGBのscale／mean／stddevと出力のNone／Sigmoid／Softmax、複数出力モデルの `outputIndex` を設定可能にし、出力マスクは bilinear で元解像度へ戻してsoft matteの連続値を保つ。AI target 自身へONNX link/defineを付与した。**価値／懸念:** 背景除去モデルをCoreだけで動かせるが、複数入力・動的shapeなどは設定契約を拡張してから対応する。**次に確認:** 実モデル（例: U²-Net系）を配置し、人物／髪のマット品質、DirectML利用、失敗時メッセージを実機確認する。

- **2026-09-04 — ONNX image module の明示BMI参照:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`。**事実:** `ArtifactCore` は実装 `.cppm` の primary interface attachment を自動dependency scanへ任せず、同ファイルで明示的なBMI参照を管理する。**対応:** `OnnxImageSegmenter.cppm` に primary interface と `Core.AI.ImageSegmenter` の参照を追加した。**価値／懸念:** Ninja/MSVCのdyndep不安定化を避けられるが、今後の新規 `import` 追加時にも同ファイルを同期する必要がある。**次に確認:** ユーザー許可後のCMake生成／ビルドで、OnnxImageSegmenterのIFC参照とONNXヘッダ解決を確認する。

- **2026-09-04 — ONNX image model diagnostics:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `modelInfo()` に ready、DirectML有効状態、入力サイズ・チャンネル、入力／出力テンソル名をまとめた read-only snapshot を追加。**価値:** App/UIを変更せずに、モデル契約と実行バックエンドの診断を接続できる。**次に確認:** 実モデル読み込み時にsnapshotとONNX Runtimeのsession情報が一致すること。

- **2026-09-04 — ONNX segmentation JSON configuration:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `loadOptionsFromJson()` を追加し、入力サイズ、前処理、letterbox、出力選択／activation、DirectML優先度を外部JSONから読み込む。既存sessionは設定変更時にresetする。**価値:** モデル資産を後で導入する際、コード変更なしにモデル固有契約を再現できる。**次に確認:** 実モデルの配布設定JSONを1つ作成し、モデル入力仕様と照合する。

- **2026-09-04 — ONNX segmentation letterbox pre-process:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `preserveAspectRatio` と padding value を追加し、固定サイズモデルへletterboxで渡し、出力マスクを同じ座標変換で元解像度へ戻す処理を追加。既定はstretchで後方互換を維持。**価値:** 縦長素材や正方形モデルで人物形状を歪めずに推論できる。**次に確認:** 16:9／9:16／1:1の実モデル結果でpadding境界とmask座標を確認する。

- **2026-09-04 — セグメンテーション失敗診断の統一:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `IImageSegmenter::lastError()` を共通契約へ加え、`segmentBatch()` が未ready・各item失敗・不正itemの最後の理由を返すようにした。**価値:** App側の一括処理UIが推論失敗を空マスクと誤認せず、ユーザーへ具体的に表示できる。**次に確認:** 実ONNXモデル不在・不正モデル・正常モデルでエラーが期待どおり更新されること。

- **2026-09-04 — 複数セグメンテーションマスクのCore合成:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `combineSegmentationMasks()` に Replace／Union／Intersect／Subtract を追加。入力解像度が異なっても `DepthMap` のbilinear samplingで target座標へ合わせる。**価値:** 人物＋髪、AIマスク＋手動補正、複数推論モデルの結果をQImage経由なしに共通マットへ統合できる。**次に確認:** 異解像度マスクでの境界品質、連続アルファのSubtract意味論、GPU cutoutとのpixel parity。

- **2026-09-04 — セグメンテーション自動適用の受入れガード:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `acceptsSegmentationMask()` と coverage／平均信頼度のしきい値設定を追加。**価値:** 空、または誤って画面全体を前景と判定したマスクを、Appが非破壊プレビューのまま停止・確認できる。**次に確認:** 実モデル別に人物／物体／背景なし素材の適正な閾値を決める。

- **2026-09-04 — OpenCV RotoBrush の IImageSegmenter adapter:** `ArtifactCore/include/AI/RotoBrushImageSegmenter.ixx`、`ArtifactCore/src/AI/RotoBrushImageSegmenter.cppm`。**事実:** 既存 `OpenCVRotoBrushEngine` はGrabCut初期マスク、前景／背景ストローク、Optical Flow伝播を実装済みだが、画像AIの共通契約へ未接続だった。**対応:** canonical BGRA float bufferを既存engineへ明示的に渡し、出力 `CV_8UC1` を `DepthMap` へ変換するadapterを追加。`propagateToNextFrame()` で、初期マスクを作成後の既存 Farneback flow 伝播をCore APIとして公開した。**価値:** モデルが無い環境でも、手動補正付きマットをONNX経路と同じbatch／refine／apply経路に渡せ、連番ではRotoBrushの追従を利用できる。**次に確認:** 現行engineのストローク座標・GrabCutマット・OpenCV例外時・大きなオクルージョンでの実機結果を確認する。

- **2026-09-04 — 軽量タスク facade の配置:** `ArtifactCore/include/Thread/LightweightTask.ixx` に、共有 `QThreadPool` を使う `executeLightweightTask` / `dispatchLightweightTasks` と、完了・キャンセル・失敗状態だけを持つ `LightweightTaskContext` を追加した。**事実:** 既存の `ThreadPool`、`Parallel`、`BackgroundTaskWorkerPool` は粒度や責務が異なる。**価値／懸念:** 短い非同期処理の入口を統一できる一方、context はタスク完了前に破棄できず、タスク内から `wait()` するとデッドロックする。**次に確認:** 実利用箇所を1つ選び、キャンセル・例外・pool飽和時の挙動をビルド／runtimeで検証する。

- **2026-09-04 — 2D Transform Gizmo の視覚ノイズ削減（Scale 中央 Y+ 軸線・Rotate 楕円重ね・軸 sweep 縮小）:** `Artifact/src/Widgets/Render/TransformGizmo.cppm` の `drawScaleCenterHandle` から Y+ 軸線 + tip ハンドルを撤去し、Rotate 描画ブロックから `drawEllipse` 2 本（X 軸赤 / Y 軸緑）と 68° sweep の X/Y 色分け弧 4 本のうち範囲を 36° に縮小。**事実:** 旧 `GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の「Scale の中心→四隅 X 線」記述は既に解消済みで、現コードの X 線正体は中央ハンドルの Y+ 軸線だった。Aspect Lock は `isCornerScaleHandle()` 側にあり、Center ハンドルの Y 軸線とは無関係。Rotate リングは `hitThickness = ringThickness * rotateRingHitBoost` で既に hit area と visual thickness が分離済み。**価値／懸念:** X 線ノイズ・4 軸 rainbow 効果・楕円重ねがそれぞれ薄れ、平面/画像レイヤーの Scale と Rotate 操作の視認性が上がるはず。`drawEllipse` ローカル関数（816 行）は未使用になるが残置、hit test・Undo・ショートカットには触れていない。**次に確認:** ユーザー許可後に `Artifact` のモジュールビルドを実行し、`ArtifactTransformGizmo` の IFC が正常に再生成され、Scale 4 隅ハンドル・Center ハンドル・Rotate リング・Leader・Drag arc の描画が既存と一致することを確認。

- **2026-09-04 — M-VP-9 Navigation Contract 現状マップの固定:** `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md` を新規作成し、既存実装の静的マップ（Alt+LMB orbit / MMB pan / Wheel zoom / `PreviewOrbitSnapshot` による orientation・pan・zoom 保存復元 / Frame Selected・All・View Undo・Redo の QAction + QShortcut 経路 / `activeViewport()` 系）と未着手項目（navigation cross 表示 / active viewport 細い枠 / preview-only と camera layer の厳密分離 / pivot・orbit source selector / surface snap）を表形式で明文化した。**事実:** `ArtifactCompositionEditor.cppm:9220-9272` の `setPreviewOrbitMode` は camera state のみを snapshot 化し、navigation session フラグ（`isAltOrbiting_` / `isPanning_` / `isAltZooming_`）は含まれない。`maskNavigationLocked` 経路は ON 時の抑制のみ。Work Cursor は配置・中央化・消去・overlay 表示まで既存、Pivot source 切替と surface snap は未着手。**価値／懸念:** AGENTS.md の「RenderScheduler / DX12 パスはシビア扱い」「既存挙動を不用意に変えない」「新規 signal/slot 接続禁止」「QPainter / QImage / QtCSS 禁止」に従うと、navigation cross 追加は Editor → Overlay への状態渡し経路が必要で pane manager (M-VP-2) 移行と密結合のため、Phase 3 では**コード改変ではなく状態マップの固定**で止めた。**次に確認:** ユーザー許可後に (1) preview-orbit snapshot に `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを含めた場合の復元整合、(2) navigation cross を `previewOrbitMode_` ON 時のみ theme token のみで描画する場合の最小実装可否、(3) active viewport 細い枠を pane manager 移行なしで 1 段重ね描画できるかどうか、を順に判断する。

- **2026-09-04 — FFmpeg C API のモジュール境界:** `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm` では、vcpkg の FFmpeg ヘッダが C リンケージを自動付与しない構成だったため、`extern "C"` でグローバルモジュールフラグメント内のヘッダ群を包む必要がある。**事実:** 未解決シンボルが `?av...` と C++ 修飾されていたが、修正後は通常リンクまで進み、`/WHOLEARCHIVE` は複数定義を起こした。**価値／懸念:** C++20 module の import／リンク問題に見えても、まず ABI のリンケージ名を確認する。**次に確認:** FFmpeg を参照する他の module 実装でも同じヘッダ配置を維持する。

- **2026-09-02 — C++ module split target の IFC 参照は分岐順に注意:** `ArtifactCore/CMakeLists.txt` では `src/Mask/` の包括分岐が個別の `RotoMask.cppm` 分岐より先に評価されるため、後置した個別参照設定だけでは実際のコンパイルコマンドに反映されない。**関連:** `ArtifactCore/CMakeLists.txt`, `RotoMask.cppm`, `ConfigSchema.cppm`, `Artifact/CMakeLists.txt`。**価値／懸念:** split target 化では「設定が存在する」だけでなく、最終的な source property の適用順と生成コマンドへの反映を確認する必要がある。**次に確認:** ユーザー許可後の再生成・ビルドで、対象コマンドに `/reference` が現れ、C1199 が解消することを確認する。

- ビルド・テスト・CMakeはユーザーの明示許可後に実行する。
- runtime検証済みになった項目は、このファイルからアーカイブへ移す。
- 実装済みの細かな履歴や重複した検証候補は、新規Insightとして追加せずアーカイブを更新する。

## Weekly insight files

### 2026

- [ISO Week 41 (2026-10-05 – 2026-10-11)](insights/2026/ISO-W41.md) — 129 entries
- [ISO Week 40 (2026-09-28 – 2026-10-04)](insights/2026/ISO-W40.md) — 94 entries
- [ISO Week 39 (2026-09-21 – 2026-09-27)](insights/2026/ISO-W39.md) — 164 entries
- [ISO Week 38 (2026-09-14 – 2026-09-20)](insights/2026/ISO-W38.md) — 31 entries
- [ISO Week 37 (2026-09-07 – 2026-09-13)](insights/2026/ISO-W37.md) — 98 entries
- [ISO Week 36 (2026-08-31 – 2026-09-06)](insights/2026/ISO-W36.md) — 79 entries
