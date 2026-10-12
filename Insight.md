**最終更新:** 2026-10-12

# Insight Register

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-10 — XPU CPU lanes share oneTBB through Core.Parallel

- **関連:** `Artifact/src/Render/ArtifactRenderQueueService.cppm`、`ArtifactCore/src/Core/Parallel.cppm`、`ArtifactCore/include/Common/ThreadPool.ixx`。
- **確認できた事実:** XPU mixed の CPU frame lanes はソフトウェア renderer を通り、複数の画像処理が `ArtifactCore::Parallel::For` を呼ぶ。同 API は `tbb::parallel_for` に委譲される。既存 `ThreadPool` は `tbb::global_control(max_allowed_parallelism, concurrency_)` を作り、TBB の global control は scheduler 内で同時実行する worker thread 数を制限する。TBB の implicit arena は parallel algorithm を呼ぶ user thread ごとに関連付く ([task arena](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/task_arena/task_arena_cls)、[global control](https://oneapi-spec.uxlfoundation.org/specifications/oneapi/latest/elements/onetbb/source/task_scheduler/scheduling_controls/global_control_cls))。
- **今回の対応:** `Core.Parallel` に共有 TBB arena API を置き、全 `Parallel::For` と XPU CPU frame lanes を同じ arena に通す。XPU は初回 arena 初期化へ process-available thread count を渡し、job host limit を実 concurrency 以下へ上限化する。MSVC module compile で `ThreadPool.ixx` の TBB inline実装を Artifact から使う形が失敗したため撤回し、TBBの実装詳細を既存 `Core.Parallel.cppm` 内へ閉じた。XPU summary と統合テスト source に scheduler と concurrency の確認を加えた。
- **価値／懸念:** XPU CPU frame lanes と内側の TBB kernels が共有 concurrency cap を使う。Taskflow、QThreadPool、GPU submission、writer/encoderとの完全な process-wide budget は未統合。Debug構成でCore/Artifact/test targetがビルド成功し、RTX 4070 Ti Vulkan + CPUのmixed-text、2D/3D particle、10-frame all-adapters/backpressureテストがそれぞれ3回連続成功。共有TBB arena/concurrencyのdiagnosticsも確認済み。複数物理GPU/iGPUでの受入は未確認。
- **次に確認:** ArtifactCoreとArtifactの対象ターゲットをビルドし、XPU mixed runtimeでCPU frame lanesとnested `Parallel::For` が同一arenaに参加すること、summaryの実 concurrency、TBB worker数とprocess CPU utilizationを検証する。

### 2026-10-10 — XPU D3D12 requires matching Agility runtime exports

- **関連:** root `CMakeLists.txt` Windows SDK `try_compile` paths、`Artifact/src/Render/DiligentDeviceManager.cppm`、`Artifact/tests/RenderQueueLayerImageIntegrationTest.cpp`。
- **確認できた事実:** CMake `D3D12_SUPPORTED` と `VULKAN_SUPPORTED` は両方TRUE。ArtifactはAgility SDK 619 のversion/pathを無条件exportしていたが、SDK `D3D12Core.dll` が配置されない構成では `D3D12_ERROR_INVALID_REDIST` (`0x887E0003`) でdevice creationが拒否される。テスト機は Intel Core i7-13700KF と NVIDIA GeForce RTX 4070 Ti の1 GPU構成で、iGPUは列挙されない。
- **今回の対応:** Agility SDK runtimeがCMakeで設定された場合だけversion/pathをexportする。SDKなし構成ではOS D3D12 runtimeへ戻す。新しいDebug treeでRender Queue統合テストを実行し、CPU＋D3D12 primaryの混在10-frame dispatch、D3D12 backend選択、画像内容を確認。自動backend選択のCPU＋Vulkan混在画像テストも成功。
- **価値／懸念:** D3D12 primaryを利用できる構成が復旧した。Agility SDKを設定した構成は未検証。現ホストではiGPUおよび複数物理GPUを使った同時レンダリングを受け入れられない。
- **次に確認:** Agility SDK 619を明示設定したruntimeと、iGPU＋dGPUまたは複数dGPUの実機で、各GPU laneの参加・フレーム数・画像出力を検証する。

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

### 2026-10-11 — Debug executable の PDB 無効化フラグ
- **関連:** `Artifact/CMakeLists.txt` の Debug executable linker option。
- **確認できた事実:** `/PDB:NONE` は PDB 出力名 `NONE` と解釈され、build tree に約6 GBのファイルを作り、再リンクで `LNK1201` になった。MSVC の `/DEBUG:NONE` が PDB を生成しない指定であることを公式 linker docs で確認し、置換した。
- **仮説／未検証:** Visual Studio 2026 と全ての supported Debug configurations での再リンク確認は未実施。今回の `ArtifactUiTest` link は通過した。
- **価値／懸念:** Debug executable link が意図どおり PDB を省略し、`NONE` という巨大な出力ファイルを作らない。
- **次に確認:** clean Debug build 後に PDB が作られず、link 成功することを確認する。

### 2026-10-11 — Cloner GPU draw parity は通常 Viewer の raster source で成立

- **関連:** `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`tools/capture_ui_test.py`、`docs/planned/MILESTONE_CLONER_UNIFICATION_2026-08-23.md`。
- **確認できた事実:** Composition Viewer の raster fixture は、GPU有効時に3×3で9個のplacement-backed sprite drawを記録し、CPU fallback 0を報告した。D3D12上のCPU/GPU renderer readbackは `ja` / `en` / `zh` / `zh-TW` 全ロケールで完全一致し、各946×1085画像に39200個のfixture色画素があった。キャプチャスクリプトは `--cloner-backend cpu|gpu` と診断route assertionを提供する。
- **追加確認 (2026-10-11):** GPU適格streamは `drawWithGpuEligibleClonerGridIndexed` 内で候補ごとの `QMatrix4x4` を生成し、Composition View callback側の `gpuGridPlacementMatchesCpuInstance` が同じdescriptorから期待行列を再構成して全16要素を比較する。加えてsubmitter失敗用fallback行列も各clone packetに保存する。これは挙動の事実であり、比較を消す場合はcomposition fieldによる行列／weight変更とsubmit失敗時のCPU fallbackを保つ別の証明経路が必要。
- **追加確認 (2026-10-11):** `capture_ui_test.py` は `--cloner-rotation-step` を受けても fixture がその値を適用するのはRadialだけだった。誤った Linear + rotation-step 組合せが成功したように見えないよう、fixture起動前に Radial 以外の指定を拒否する検証を追加した。実行時のCPU fallback確認にはまだ非対応fixture条件の入力経路が必要。
- **追加確認 (2026-10-11):** Linear `opacityDecay=0.2` はGPU適格性から除外される既存fixture条件だったため、capture helperに `--cloner-expect-cpu-fallback` を追加して実行確認した。GPU準備済みだがplacement未処理・GPU draw 0件となり、`ja` / `en` / `zh` / `zh-TW` のCPU/GPU renderer readbackは同一frame 2で全画素一致、weight traceも一致した。arena overflow・GPU resource failureは別条件で未検証。
- **追加確認 (2026-10-11):** Radial `opacityDecay=1.0` を既存exeで試すと、CPU weight trace `1,1,0,0,0` に対してGPU stream traceが `1,1,0,-1,-2` となり、GPU 3 draw＋CPU fallback 2件へ分岐した。`drawWithGpuEligibleClonerGridIndexed` の共通candidate callbackでweightを `std::clamp(...,0,1)` し、CPU `appendCloneInstance` と意味論をそろえた。対象 `.ixx` module objectのコンパイルは通過したが、既存exeは修正前のためruntime再確認は未実施。
- **仮説／未検証:** GPU placementを読むdrawでもcloneごとのCPU順序走査とfallback用CPU transform生成を残すため、CPU準備時間が支配的な大規模Gridでは速度向上が限定的な可能性がある。GPU runtime性能、Vulkan runtime parity、容量上限／resource failure時の実機fallbackは未検証。
- **価値／懸念:** 描画段階は実画像を使ったCPU parityまで到達した。透明合成順を保持しながら複数cloneを1 drawへまとめられるかは、順序・blend semanticsを測定してから別段階で判断する。
- **次に確認:** GPU timeとCPU準備時間を分離計測し、固定容量上限近傍とGPU resource failureのfallback挙動を確認してからGPU-3の対象拡張を選ぶ。

### 2026-10-11 — Linear Cloner の平行移動配置をGPU経路へ拡張

- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** Linear generatorはclone index 0から `offsetXYZ * (index + 1)` で配置する。GPU placement recordに同じ順序のtranslationを生成し、HLSL vertex shaderがslotを読み取る経路をGrid/Matrixと共用した。対応対象は単一generator、transform stackなし、回転なし、不透明度減衰なし、Random legacy mode以外。D3D12上のCPU/GPU出力は4ロケールで全画素一致し、Linear fixture色17,396画素とGPU draw 4件、fallback 0件を確認した。Gridも同じ再ビルド後、4ロケール全画素一致・fixture色39,200画素・GPU draw 9件を再確認した。
- **仮説／未検証:** singleton drawを保つため透明合成順とCPU fallbackを維持できるが、大きいcountでCPU準備時間が支配的かは未測定。回転／減衰／Random modeのruntime fallback、容量超過・resource failure fallback、Vulkan runtime parityは未確認。
- **価値／懸念:** LinearとGrid/Matrixが同じGPU output arena、record ABI、sprite vertex pathを利用する段階になった。rows/depthのゼロ組合せをLinear modeとしてHLSLへ伝えるため、そのsentinel契約を別distribution追加時に曖昧にしないこと。
- **次に確認:** rows/depth sentinelを明示的なdistribution enumへ置き換える設計を検討し、Linearの不対応設定とarena容量超過をruntime fixtureでCPU parity確認してからRadialを追加する。

### 2026-10-11 — Radial Cloner GPU placement は回転とローカル矩形原点を保持
- **関連:** `Artifact/src/Layer/ArtifactCloneLayer.cppm` のRadial評価、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx` のgenerator列挙、`Artifact/include/Render/ArtifactIRenderer.ixx` のplacement record、`Artifact/src/Render/ShaderManager.cppm` のCloner sprite vertex shader。
- **確認できた事実:** `layerGenerators()` component Radialの配置は原点周りに半径分移動し、cloneを `angle` 度回転する。GPU placement recordの予約領域をrotation degreesに使い、HLSL sprite pathへlocal rect originを回転順序に含めた。旧式Cloner内部のradialCount/radius/anglesが正規generator descriptorへ反映されていなかったため、descriptor変換時にcanonical内部設定をコピーした。80×48矩形・4 clone fixtureでD3D12の `ja` / `en` / `zh` / `zh-TW` 全てCPU/GPU完全一致（946×1085、fixture色16,599画素）、GPU draw 5件、fallback 0件。GridとLinearもjaで完全一致。旧式ClonerGenerator effectのbounds-center契約は同経路とは別である。
- **価値／懸念:** RadialをCPU fallbackからGPUへ段階的に広げ、設定値もdescriptorへ正しく渡るようになった。Radial非ゼロrotationStepもD3D12 parityを確認済み。opacityDecay／modifier、Vulkan runtime、性能は未検証。
- **次に確認:** opacity decay／modifier等はそれぞれCPU parity fixtureを追加してからGPU対象を選び、Vulkan runtime parityも確認する。

### 2026-10-11 — Grid/Matrix Cloner 減衰weightをclone indexと共有
- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Cloner UI fixture。
- **確認できた事実:** 旧Cloner Gridはflattened index `z*(rows*columns)+y*columns+x` を `1-opacityDecay*index` に適用していたが、canonical component generatorはweightを常に1としていた。Grid/Matrix descriptor CPU instance生成とGPU stream callbackに同じ0..1 clamp式を加え、GPU照合側でもweight式を確認するようにした。D3D12 Grid decay=0.2は `en` / `ja` / `zh` / `zh-TW` 全てCPU/GPU readback全画素一致（946×1085、GPU 9 draw／fallback 0）。trace列は `1,1,0.8,0.6,0.4,0,0,0,0`。enのdecay=0比で26,293画素が変化した。
- **確認できた事実:** Matrix専用descriptor fixtureはdecay=0.2で `ja` / `en` / `zh` / `zh-TW` 全て全画素一致、GPU 9 draw／fallback 0。D3D12とVulkanの両方でMatrix CPU/GPU readbackがframe 2、全画素一致した。さらにVulkan Grid decay=0.2、Radial rotationStep=15／decay=0.2も同じ4ロケールでframe 2のCPU/GPU readbackが全画素一致し、GridはGPU 9 draw、Radialは5 draw、fallback 0。Matrix weight traceは `1,1,0.8,0.6,0.4,0,0,0,0`。decay=0と比べCPU/GPU双方で26,293画素変化し、decay=0同士も全画素一致した。decay=1の英語fixtureはweight `1,1,0,0,0,0,0,0,0` でCPU/GPU全画素一致した。
- **仮説／未検証:** arena容量近傍、性能は未検証。
- **価値／懸念:** legacy Grid weight意味論を正規generator CPU/GPU描画へそろえ、opacity shader修正を使ってGridも段階拡張した。
- **次に確認:** 出力arena容量近傍と性能を確認する。

### 2026-10-11 — Cloner GPU arenaの上限はbase slot込み
- **関連:** `Artifact/src/Render/ArtifactIRenderer.cppm` (`dispatchClonerGridPlacement`)、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`（GPU placement observer／sprite fallback）。
- **確認できた事実:** arenaは65,536 recordsで、各dispatchは候補数+1（base slot）を予約する。batch cursorが0の場合に受け入れ可能な候補数は最大65,535。残容量不足はparameter mapping／compute dispatch前にfalseで返り、range未取得時は通常のCPU行列sprite drawへ進む。cursorはsubmitter batch送信後に0へ戻る。
- **仮説／未検証:** overflow時のCPU fallbackはコード経路上boundedだが、65,536候補を実際に超えるfixtureでfallback count・画像 parity・フレーム時間を確認していない。
- **価値／懸念:** 65,536を「clone数上限」と誤読せず、base recordとbatch内の複数dispatchが共有するcursorを含めて上限を説明できる。大量clone fallbackはCPU transform loopとsprite recordingが続くため、GPU上限拒否だけでは入力 workload を安価にできない。
- **次に確認:** 安全なoverflow fixtureで1 batch超過時のCPU fallbackと画像を検証し、同一clone数でCPU準備時間／GPU時間を計測する。

### 2026-10-11 — transformed-sprite vertex shader がCPU opacityを落としていた
- **関連:** `Artifact/src/Render/ShaderManager.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`、Cloner UI fixture。
- **確認できた事実:** fixture CPU/GPU両経路はclone weight `1.000, 1.000, 0.800, 0.600, 0.400` を作りdraw callbackまで渡していた。通常 transformed-sprite VSは入力頂点カラーattributeを宣言せず、出力を常に白・alpha=1に固定していた。GPU Cloner VSはOpacity cbufferを使うため減衰し、CPU標準sprite経路だけが減衰を失っていた。Artifact側の標準VSを頂点カラー入力へ対応させ、入力layoutへcolor attributeを追加し、Cloner専用PSOは旧2 attribute layoutを明示した。変更後、D3D12 radial `rotationStep=15` でdecay=0.2と0の両CPU/GPU readbackが全画素一致し、decay画像間では11,609画素が変化。GPUは各5 draw／fallback 0。
- **仮説／未検証:** SpriteTransformを使う他の描画操作もopacityが正常化するはずだが、専用UI / matte / Puppet deformationの全変種は未検証。
- **価値／懸念:** GPU Cloner固有のopacity差ではなく、CPU fallback共有vertex shaderの頂点color欠落を修正した。Diligent backend-neutral shader/PSO境界内で、ArtifactCore child repoは変更していない。
- **次に確認:** transformed-spriteの通常opacity、mask、textured triangleで回帰を確認し、Radial decayのlocale / Vulkan runtime coverageを広げる。

### 2026-10-11 — GPU Radial rotationStep parity
- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、`tools/capture_ui_test.py`。
- **確認できた事実:** CPU Radialの配置角は `startAngle + angleStep * index`、描画回転は `placementAngle + rotationStep * index`。GPU placement computeへstepを追加した後、GPU向けCPU stream生成側が回転増分を変換行列に加えていなかったため、matrix照合がclone 1–3をCPU fallbackへ送っていた。GPU stream変換もCPU Component生成と一致させると、D3D12 fixtureでRadial `rotationStep=15` が5 GPU draw／0 fallbackとなり、CPU/GPU readbackが946×1085の全画素で一致した。Grid（9 draw）、Linear（4 draw）、Radial step=0（5 draw）も同buildで全画素一致。Radial step=15、Grid、Linearは `ja` / `en` / `zh` / `zh-TW` すべてで比較した。初回の15度stepキャプチャはframe 1の読み戻しで黒だったが、frame 2で再取得したCPU/GPU画像にはクローンが描画され完全一致した。
- **価値／懸念:** radial offset angle と sprite rotation を別々に運び、GPU配列生成とCPU照合を同じ意味論にそろえた。parameter blockは固定長64 bytesに拡張し、readbackなしのdispatchを保つ。Vulkan runtime、非常に大きいstep値の数値境界、性能は未検証。
- **次に確認:** Vulkan backendでもRadial step parityを確認し、opacity decayやmodifierは個別のCPU/GPU parity fixtureを作ってから段階的に追加する。

### 2026-10-11 — TrackerManager の project load 置換と Controller binding

- **関連:** `Artifact/src/Project/ArtifactProjectImporter.cppm`（TrackerManager::fromJson / clearTrackers）、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（controller session binding）。
- **確認できた事実:** project importer は非同期ロード時にバックグラウンド worker で実行され、従来はその中から `TrackerManager::fromJson()` によりグローバル Manager を置換していた。トラッカー配列を `ArtifactProjectImporterResult` に保持させ、ProjectManager の同期成功後／非同期 UI-thread commit 時に Manager を復元するよう変更した。さらにフレーム収集開始前に元 tracker を job ごとの `MotionTracker` copy へ複製し、フレーム収集と solve は copy 上で進める。成功時、同じ ID／同じ登録 pointer が残る場合だけ結果を元へ反映し、失敗・キャンセル・切替時は copy を破棄する。
- **仮説／未検証:** TrackerManager の置換をまたぐ解析では古い job の結果を破棄する設計だが、実機で project load と解析完了が競合した際の UI・undo・進捗表示は未確認。MotionTracker copy の frame buffer と解析結果の独立性も runtime 未検証。
- **価値／懸念:** importer worker とグローバル TrackerManager の同時更新を避け、Manager 置換後も solve worker が削除済み tracker pointer を使わない。キャンセル時に既存結果・設定を解析準備のクリア処理で失わない。1解析jobにつき開始時に tracker 状態を複製するコールドパス確保が発生する。
- **次に確認:** 許可後に非同期 project load と長時間解析を重ね、旧 job 結果が破棄され、新プロジェクト session／UI が維持されることを確認する。

### 2026-10-11 — Tracker source/session の project 保存境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（source selection）、`Artifact/src/Project/ArtifactProject.cppm`（extension data）、`Artifact/src/Project/ArtifactProjectExporter.cppm`／`ArtifactProjectImporter.cppm`（保存復元）。
- **確認できた事実:** Tracker ドックは追跡元と結果適用先を分離し、composition default source と tracker session 別 source override を project `extension_data` に保存する。session manifest は composition ID で索引し、active tracker ID と tracker ごとの編集 geometry も保存する。解析開始時には入力元を固定する。Controller の Composition 切替／破棄では Manager session を削除せず detach し、再選択時に bind する。
- **確認できた事実:** session create/delete 用の Undo command を追加し、tracker JSON、同じ tracker ID、geometry と source override を保持して復元する。既存 `UndoManagerChangedEvent` 経由で active manifest と controller binding を再同期する。
- **確認できた事実:** `LayerChangedEvent::Removed` は layer undo command の redo でも発行される。source ID をその場で消すと、layer undo で同じ ID の layer を戻しても source 設定が復元されず Undo/Redo の意味を損なうため、イベント時の即時 cleanup は採用しない。source 解決は現在の Composition 内で layer ID の存在を検証し、stale ID は使用せず選択 layer へ fallback する。
- **仮説／未検証:** source metadata の整理は `RemoveLayerCommand` のスナップショットと同じ undoable operation に統合する必要がある。保存後の Manager session 復元、複数 Composition 間での session ID 分離、Undo/Redo の実機動作も未検証。旧データの Manager 内 orphan tracker は安全に所有 Composition を推定できない。
- **価値／懸念:** Core／サブモジュールを変更せず既存 project extension lifecycle を使って複数 session の UI 選択と geometry/source の保存を実現できる。manager session の寿命と project serialization の接続が期待どおりかは受入れ確認が必要。
- **次に確認:** tracker source metadata の remove/restore を `RemoveLayerCommand` の Undo/Redo にどう含めるか設計し、許可後に保存／再読込、session 切替、layer Undo/Redo、tracker create/delete Undo/Redo、複数 Composition 切替を実行確認する。

### 2026-10-11 — Tracker の作業形状とセッション設定の保存境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（TrackerBoxState 初期化とキャプチャ）、`Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm`（viewport geometry）、`ArtifactCore/src/Tracking/MotionTracker.cppm`（settings / track regions）。
- **確認できた事実:** Feature/Search の幅と高さは `LayeredConfigStore` の `Viewport/TrackPoint/*` キーから読み書きされ、作業環境単位の設定になっている。Tracker の Core session には Point の位置・検索領域、Planar の track points / region と解析結果が保存される。ビューポート上の `TrackerBoxState.innerCenter` 自体はその Core session JSON に含まれず、各解析開始時に Gizmo state から Core へ点／領域を作っている。
- **仮説／未検証:** プロジェクトを切り替えた後も workspace の特徴領域サイズは共有される一方、中心座標や編集中の形状は session と一緒に復元されないため、保存済み結果と画面上の編集形状が異なる可能性がある。実際の再読込後レイアウトは未検証。
- **価値／懸念:** session chooser や再開 UI を足す前に、解析済みトラックと編集可能な source geometry のどちらを project/session が所有するか決める必要がある。
- **次に確認:** source layer／composition 単位の geometry 保存、workspace default との優先順位、プロジェクト再読込時の復元先を設計し、複数 session と source 切替で検証する。

### 2026-10-11 — 実写トラスの面追跡は背景除外と幾何誤差の確認が必要

- **関連:** `ArtifactCore/src/Tracking/MotionTracker.cppm:1002`、`ArtifactCore/include/Tracking/MotionTracker.ixx:151`、`tests/ArtifactCore/MotionTrackerVideoInputTest.cpp`。
- **確認できた事実:** planar solveのECC／特徴点検出マスクは矩形ROI全体を有効にする。公開入力は`addTrackRegion(QRectF)`であり、任意の背景除外マスクを渡すAPIは見つからない。CC0橋映像の選択領域にはトラスの穴越しの川と奥側の梁が入る。正解座標のある急角度の独立テストでは、24フレームの貼り付け四隅の最大誤差2px以下という検査を通過した（2026-10-11）。実写の信頼度0.5以上という検査だけでは幾何精度の証明にならない。
- **仮説／未検証:** 実写終盤のズレに、別奥行きの背景や梁の相対運動と、投影ROIを軸平行bounding boxへ戻す処理が寄与している可能性がある。急角度自体の一般的な失敗や、背景除外での改善は未証明。
- **価値／懸念:** 相関の信頼度が高くても、貼り付け面以外の画素に追従すれば合成の位置がずれる。背景除外マスクの保持と実写の独立した特徴点注釈が精度検証に必要。
- **次に確認:** ArtifactCore変更の明示許可後、参照フレームの除外マスクをホモグラフィで伝搬する最小設計と、実写終盤の注釈点に対する再投影誤差を検証する。

### 2026-10-11 — ArtifactHashMap iteratorの衝突chain走査疑い

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** iteratorは現在Nodeとbucket位置を保持する。`operator++()`はbucket位置を先に1つ進め、現在nodeの`next`を参照していない。hash衝突した複数entryが同一bucketに入る構造は`emplaceImpl()`で確認した。また`emplaceImpl()`は既存keyを見つけたときmapped valueを代入する一方、既存の`TryEmplaceDuplicateKeepsOriginalMappedValue`は元値維持を契約している。
- **仮説／未検証:** 同一bucketの連鎖要素をrange iterationで飛ばし、重複tryEmplaceで値を置換する可能性がある。runtime testはAGENTS.mdの実行制約により未実施。
- **価値／懸念:** range traversalでmap entryを取りこぼしたり、insert-only APIが既存valueを書き換えたりすると、反復ベースのシリアライズ／集計や呼び出し側の状態前提が壊れる恐れがある。
- **次に確認:** `ArtifactHashMapContractTest.IterationVisitsEveryEntryInCollisionChain` とmodel testを実行し、再現するならiterator increment／duplicate emplacementの契約を修正・検証する。

### 2026-10-11 — SmallVector debug snapshotのsamples型不一致の疑い

- **関連:** `ArtifactCore/include/Container/SmallVector.ixx`、`ArtifactCore/include/Container/ContainerDebug.ixx`、`ArtifactCore/include/Container/NamedVector.ixx`。
- **確認できた事実:** `SmallVector::debugSnapshot()` は `debugSample(4)` の戻り値 `NamedVector<ContainerElementSample>` を、`ContainerDebugSnapshot::samples` の `std::vector<ContainerElementSample>` aggregate fieldへ直接渡している。`NamedVector` には `toStdVector()` はあるが、確認した宣言範囲では暗黙の `std::vector` 変換演算子は見つからない。
- **仮説／未検証:** debugSnapshot／watch系のtemplate memberをinstantiateすると型変換エラーになる可能性がある。ビルドは禁止中のためコンパイラでは未確認。
- **価値／懸念:** 独立suiteが当該moduleのtemplate実体化で阻害される可能性があり、debug snapshot APIの利用可能性にも影響する。
- **次に確認:** ArtifactCore変更が許可された作業で、`samples.toStdVector()` 相当の明示変換が正しい境界か確認し、module compileとdebugSnapshot利用テストを実行する。

### 2026-10-11 — CppLinq lazy views borrow the input range

- **関連:** `ArtifactCore/include/Utils/CppLinq.ixx`、`tests/ArtifactCore/CppLinqContractTest.cpp`。
- **確認できた事実:** `Where` と `Select` は `const R&` を受け取り、入力rangeへ `views::filter`／`views::transform` をpipeしたviewを返す。既存suiteは元rangeの変更反映と遅延評価を検証している。
- **仮説／未検証:** 一時rangeを渡すと関数内では名前付きconst lvalueとしてview化されるため、呼び出し式の終わりでrangeが破棄された後のview走査がdanglingになる可能性が高い。現在の呼び出し元が一時rangeを使うかは未確認。
- **価値／懸念:** borrowed view契約を明示するか、所有viewと借用viewを分ければ、一時range由来の寿命事故を防ぎやすい。
- **次に確認:** 本番呼び出し箇所を検索し、一時rangeを渡す箇所の有無と許容するownership semanticsを確認してからAPI設計と寿命テストを決める。

### 2026-10-11 — Crash report parser の認識条件が header substring のみ

- **関連:** `ArtifactCore/include/Diagnostics/CoreCrashReportParser.ixx`、`tests/ArtifactCore/CrashReportParserStandaloneTest.cpp`。
- **確認できた事実:** `parseCrashReport()` は `=== Artifact Crash Report ===` が入力内のどこかにあれば `parsed=true` を返し、`parseCrashReportFile()` はそのフラグだけで成功扱いにする。例外情報やstack sectionがないheader単独の入力も成功する。
- **仮説／未検証:** report producer が出力する正式形式で必須sectionが定義されている場合、途中で切れた／壊れたreportを成功として空に近いsnapshotへ変換する可能性がある。headerのみを有効とする仕様かは未検証。
- **価値／懸念:** 最低限の構造検証を行えば、壊れたcrash reportと正常な最小reportを区別しやすい。
- **次に確認:** crash report producerの出力形式と既存保存データを照合し、成功判定に必要なsectionを決めたうえで、header-only／欠落sectionの契約テストを確定する。

### 2026-10-11 — NoiseGenerator legacy seed state is process-global

- **関連:** `ArtifactCore/src/Math/NoiseGenerator.cppm`、`ArtifactCore/include/Math/NoiseGenerator.ixx`、`tests/ArtifactCore/NoiseGeneratorStandaloneTest.cpp`。
- **確認できた事実:** Legacy `NoiseGenerator::setSeed()` rewrites shared static permutation array `p` and `isInitialized`; `perlin()` may initialize this state lazily. `NoiseField` instead stores its permutation table per instance and is documented as concurrently evaluable.
- **仮説／未検証:** Concurrent legacy noise evaluation and reseeding may race. Actual callers and whether they reseed during rendering are not yet checked.
- **価値／懸念:** Per-instance `NoiseField` can isolate deterministic procedural fields from process-wide seed changes.
- **次に確認:** Search production call sites for concurrent `perlin()` evaluation and runtime `setSeed()` use; if needed, add an explicit concurrency contract and migrate only approved call paths.

### 2026-10-11 — LayerBoundsCalculator2D の退化矩形 hit test 境界

- **関連:** `ArtifactCore/src/Geometry/LayerBoundsCalculator.cppm`、`tests/ArtifactCore/LayerBoundsCalculatorStandaloneTest.cpp`。
- **確認できた事実:** `contains()` は四辺の外積符号を判定し、全外積が0の場合も内包として返す。幅・高さがともに0の矩形では、任意の問い合わせ点への各外積が0になる。
- **仮説／未検証:** 退化矩形が hit-test 対象になると、本来の一点以外も含む判定になり得る。UI側が0サイズ矩形を問い合わせる頻度と期待仕様は未検証。
- **価値／懸念:** 0サイズまたは極小レイヤーの選択領域が広がる可能性がある。
- **次に確認:** zero-width／zero-height／zero-area各ケースの境界意味論を決め、必要なら `contains()` の退化形状処理と独立テストを追加する。

### 2026-10-11 — FormantExtractorは末尾の不完全解析windowを出力しない

- **関連:** `ArtifactCore/src/Audio/FormantExtractor.cppm`、`tests/ArtifactCore/FormantExtractorStandaloneTest.cpp`。
- **確認できた事実:** `analyzeTrack()`は`framesPerAnalysis <= totalFrames - offset`の間だけイベントを生成するため、最後にwindow長未満のsamplesが残る場合は解析しない。
- **仮説／未検証:** frame-rateと音声長が割り切れない素材では最後の短区間がmouth-shapeイベントに反映されない可能性がある。切り捨てが意図された仕様かは未検証。
- **価値／懸念:** 音声末尾の短い発音・閉口イベントがtrack出力から欠落する可能性がある。
- **次に確認:** tail paddingまたは短windowの分析を仕様化し、末尾にイベントを生成するかどうかの期待値を決める。

### 2026-10-11 — AudioVolumeのdeserializeが入力を読まない

- **関連:** `ArtifactCore/include/Audio/AudioVolume.ixx`、`ArtifactCore/src/Audio/AudioVolume.cppm`、`tests/ArtifactCore/AudioVolumeStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`はvolume/mutedをJSON文字列へ出力する一方、`deserialize(const UniString&)`の実装は引数を無視し、既定volume 1.0を返す。
- **仮説／未検証:** 保存したvolume値を読み戻す経路ではレベル設定が失われる可能性がある。利用箇所は未確認。
- **価値／懸念:** API名から期待される保存復元が実現していない。
- **次に確認:** 呼び出し元とデータ互換性を調査し、serialize/deserializeのJSON形式と不正入力処理を定義した後、独立roundtripケースを追加する。

### 2026-10-11 — AudioDecibelsのdeserializeが入力を読まない

- **関連:** `ArtifactCore/include/Audio/AudioDecibels.ixx`、`ArtifactCore/src/Audio/AudioDecibels.cppm`、`tests/ArtifactCore/AudioDecibelsStandaloneTest.cpp`。
- **確認できた事実:** `serialize()`は`{"dB":...}`形式のJSON文字列を作る一方、`deserialize(const UniString&)`の実装は引数を無視して0 dBを返す。
- **仮説／未検証:** 保存した非ゼロ値を読み戻す経路で設定が失われる可能性がある。呼び出し元の利用状況は未検証。
- **価値／懸念:** `AudioDecibels` の永続化／複製でレベル値を復元できない。
- **次に確認:** 利用箇所と既存データ互換性を確認し、修正許可範囲でserialize/deserialize roundtripおよび不正JSON入力の契約を独立suiteへ加える。

### 2026-10-11 — AudioWaveformのbinningが端数frameを捨てる可能性

- **関連:** `ArtifactCore/src/Audio/AudioWaveform.cppm`、`ArtifactCore/include/Audio/AudioWaveform.ixx`、`tests/ArtifactCore/AudioWaveformStandaloneTest.cpp`。
- **確認できた事実:** bin幅は整数除算`frames / resolution`で決まり、全binを同じstep幅で処理する。
- **仮説／未検証:** `frames`が`resolution`で割り切れない場合、末尾frameがどのbinにも入らない可能性がある。現suiteは割り切れる寸法を使っており、この境界を実行確認していない。
- **価値／懸念:** preview waveformの末尾にあるtransientが欠落する可能性がある。
- **次に確認:** 端数のあるframe数と最終sampleだけに信号を置く独立caseで、全frameがbinningされるか確認する。

### 2026-10-11 — AudioSpectrumのDFT binはcosine投影の絶対値合計

- **関連:** `ArtifactCore/src/Audio/AudioSpectrum.cppm`、`tests/ArtifactCore/AudioSpectrumStandaloneTest.cpp`。
- **確認できた事実:** `computeFFT()`は各binで`abs(input[i] * cos(angle))`を加算しており、複素DFTのreal/imaginary成分やmagnitude normは計算しない。
- **仮説／未検証:** spectrum binsは正弦波の周波数振幅を一般的なスペクトラム解析の期待どおりには表さない可能性がある。どの表示側がこの結果を利用するか、また現状が意図的な簡易指標かは未検証。
- **価値／懸念:** このAPIをスペクトラム表示や周波数比較へ使う場合、bin値から正しい周波数強度を判断できない可能性がある。
- **次に確認:** 利用側の正規化・表示契約を調査し、DC・単一正弦波・Nyquist近傍の期待スペクトルを定めてから独立信号ケースを追加する。

### 2026-10-11 — AudioBassTrebleの係数atomicが未使用

- **関連:** `ArtifactCore/include/Audio/AudioBassTreble.ixx`、`ArtifactCore/src/Audio/AudioBassTreble.cppm`、`tests/ArtifactCore/AudioBassTrebleStandaloneTest.cpp`。
- **確認できた事実:** `bassCoeff_`と`trebleCoeff_`はメンバー初期化とconstructorで1.0をstoreされるが、`process()`は読み出さず、都度dB値からgainを計算する。
- **仮説／未検証:** atomic係数は旧実装の残存物、または将来の制御用である可能性がある。現時点の処理結果には使われない。
- **価値／懸念:** setter更新とaudio processing間のthread-safe受け渡しを目的にした設計なら、その同期契約が実際には機能していない可能性がある。
- **次に確認:** setter/processの並行利用契約を確認し、不要な残存メンバーか、atomic値を使うべき仕様かを判断する。

### 2026-10-11 — AudioChorusの一部公開設定がprocessへ未接続

- **関連:** `ArtifactCore/include/Audio/AudioChorus.ixx`、`ArtifactCore/src/Audio/AudioChorus.cppm`、`tests/ArtifactCore/AudioChorusStandaloneTest.cpp`。
- **確認できた事実:** `process()`は`rate_`、`depth_`、`baseDelayMs_`を使うが、`feedback_`と`mode_`を参照しない。出力は固定係数でdry 0.7／delayed 0.3を混合する。
- **仮説／未検証:** Feedback parameterやFlanger modeを変更しても音声出力へ作用しない可能性が高い。現状が仮実装か意図的仕様かは未検証。
- **価値／懸念:** UIから調整できる公開parameterが期待どおりの音響変化を生まない可能性がある。
- **次に確認:** Chorus／Flangerの仕様を定め、feedback loopとmode別delay/mixの期待応答を決めてからsignal-level独立ケースを設計する。

### 2026-10-11 — AudioReverbのsize parameterが処理に未接続

- **関連:** `ArtifactCore/include/Audio/AudioReverb.ixx`、`ArtifactCore/src/Audio/AudioReverb.cppm`、`tests/ArtifactCore/AudioReverbStandaloneTest.cpp`。
- **確認できた事実:** `size_`はsetter・parameter metadata・JSON保存復元に使われるが、`process()`は参照しない。また`allpassBuffer_`は確保されるが処理で使われない。
- **仮説／未検証:** sizeを変更しても残響の時間・密度は変わらない。これらが暫定実装か意図的な簡略化かは未検証。
- **価値／懸念:** parameter UIで調整できても、sizeが音響結果へ反映されない可能性がある。
- **次に確認:** DSP仕様を確定し、sizeが残響長・遅延・密度のどれを制御するか決めたうえで、異なるsize設定間の出力差を期待値として追加する。

### 2026-10-11 — AudioStereoMixer center／delay settersがprocessに未接続

- **関連:** `ArtifactCore/include/Audio/AudioStereoMixer.ixx`、`ArtifactCore/src/Audio/AudioStereoMixer.cppm`、`tests/ArtifactCore/AudioStereoMixerStandaloneTest.cpp`。
- **確認できた事実:** `setCenter`、`setLeftDelay`、`setRightDelay`はprivate stateを更新するが、`process()`は`leftRight_`だけを参照して左右gainを適用する。
- **仮説／未検証:** 現行APIの利用者はcenter extractionや左右delayが機能すると期待し得るが、実処理へ反映されない可能性が高い。各項目が将来用placeholderかは未検証。
- **価値／懸念:** UIやautomationに設定導線がある場合、値を変えても音声結果が変化しない。
- **次に確認:** setterの利用箇所とAudioStereoMixerの正式仕様を確認し、実装範囲が許可された際にそれぞれのDSP応答を決めて独立期待値を追加する。

### 2026-10-11 — AudioHighLowPassのhigh-pass recurrenceがDCを残す可能性

- **関連:** `ArtifactCore/src/Audio/AudioHighLowPass.cppm`、`ArtifactCore/include/Audio/AudioHighLowPass.ixx`、`tests/ArtifactCore/AudioHighLowPassStandaloneTest.cpp`。
- **確認できた事実:** high-pass pathは`hpInput = input - hpState`、次に`hpState = hpCoef * hpState + hpInput`を計算して`hpInput`を出力する。low-passも有効な場合、後段で`data[i]`をlow-pass stateへ再代入する。
- **仮説／未検証:** high-pass recurrenceは一定DC入力に対して0へ収束しない可能性が高く、両filter有効時はhigh-pass成分がlow-pass出力に置換される。追加DC rejection caseは意図した期待を表現するが、実行していない。
- **価値／懸念:** UI上high-passとlow-passの双方を設定できる場合、high-pass単独やcascade filterとしての期待と異なる応答になる可能性がある。
- **次に確認:** DSP仕様（one-pole cutoff、係数、cascade順）を決め、impulse／step responseと両filter同時有効時の期待値を定義してから修正範囲と回帰確認を行う。

### 2026-10-11 — AudioDelayが書き込み位置より先のbuffer indexを読む

- **関連:** `ArtifactCore/src/Audio/AudioDelay.cppm`、`ArtifactCore/include/Audio/AudioDelay.ixx`、`tests/ArtifactCore/AudioDelayStandaloneTest.cpp`。
- **確認できた事実:** 各sampleでread indexを`writePos + delaySamples`として計算し、同じ`writePos`へ現在sampleを保存する。delay bufferは初期状態で48000要素のゼロ値を持つ。
- **仮説／未検証:** 通常の遅延範囲では過去に書いたsampleではなく未書込の先行領域を読み続け、Impulseの遅延音が返らない可能性が高い。新しい1-frame impulse regression caseがこの期待を確認するが、ビルド・実行はしていない。
- **価値／懸念:** delay timeとwet mixを設定してもechoが出ず、wet signalが無音になる可能性がある。
- **次に確認:** 許可された作業範囲で独立suiteを実行し、SPSC状態・block境界を含む読出し／書込み位置を正す修正後に、単一blockおよび複数blockのimpulse応答を検証する。

### 2026-10-11 — AudioCompressor parameter metadataとsetter範囲が異なる

- **関連:** `ArtifactCore/include/Audio/AudioCompressor.ixx`、`ArtifactCore/src/Audio/AudioCompressor.cppm`、`tests/ArtifactCore/AudioCompressorStandaloneTest.cpp`。
- **確認できた事実:** `getParameters()`のthresholdは-60〜0 dB、attackは0.1〜100 ms、releaseは1〜1000 ms。一方setterとprocess内clampはthreshold -120〜20 dB、attack 0.1〜1000 ms、release 1〜5000 msを許容する。
- **仮説／未検証:** UIやautomationがmetadata範囲を制約として使用する場合、直接setter／JSON経由で設定できる値域と表示上の値域が異なる可能性がある。
- **価値／懸念:** serialized settingの表示／再編集時に値がmetadata外となり、UIでの再現性が不明瞭になる。
- **次に確認:** `EffectParameter`利用側とcompressor UIのclamp処理を調べ、各境界の正規契約を決めてから値域を横断する独立ケースを追加する。

### 2026-10-11 — AudioTone parameter metadataとsetter／process範囲が異なる

- **関連:** `ArtifactCore/include/Audio/AudioTone.ixx`、`ArtifactCore/src/Audio/AudioTone.cppm`、`tests/ArtifactCore/AudioToneStandaloneTest.cpp`。
- **確認できた事実:** `getParameters()`はfrequencyを20–20000 Hz、amplitudeを0–1として公開する。一方、直接setterはfinite値ならこの範囲外も保持し、`process()`ではfrequencyを0–96000 Hzにclampし、amplitudeはclampしない。
- **仮説／未検証:** parameter metadataをUIやautomationが編集制約として使用し、直接setterやJSONが別経路から範囲外値を渡す場合、表示値とprocess実効値が異なる可能性がある。
- **価値／懸念:** parameter range、serialization、DSP clampの契約が未統一だと、再現性・automation表示で混乱を生む。
- **次に確認:** `EffectParameter`の全利用箇所と既存tone UIのclamp責務を調べ、どの境界が正式な制約か決めてから範囲テストを加える。

### 2026-10-11 — AudioPannerの複数modeが同一gainを返す

- **関連:** `ArtifactCore/include/Audio/AudioPanner.ixx`、`ArtifactCore/src/Audio/AudioPanner.cppm`、`tests/ArtifactCore/AudioPannerStandaloneTest.cpp`。
- **確認できた事実:** `calculateGain()`は`StereoBalance`以外の`EqualPower`、`VBAP`、`Ambisonics`、`Binaural`で、azimuth/elevationに関係なく`{1.0f, 1.0f}`を返す。定電力ゲインの独立static関数は別に存在する。
- **仮説／未検証:** 複数modeは未実装または暫定placeholderである可能性がある。呼び出し側がmode名どおりの立体音響結果を期待するか、modeがUI設定だけで未使用かは未検証。
- **価値／懸念:** mode選択が処理結果へ反映されない場合、設定上の期待と実際のpanが一致しない。
- **次に確認:** 利用箇所とmodeごとのspeaker/HRTF/encoder設計を調べ、各modeの契約と独立期待値を定めてからgain動作テストを追加する。

### 2026-10-11 — AudioRingBufferの公開12chレイアウトと保存上限に差

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/include/Audio/AudioRingBuffer.ixx`、`ArtifactCore/src/Audio/AudioRingBuffer.cppm`、`tests/ArtifactCore/AudioRingBufferStandaloneTest.cpp`。
- **確認できた事実:** `AudioChannelLayout::Surround714`は12chと定義されている一方、`AudioRingBuffer::Impl`は`kMaxChannels = 10`で、`write()`は10超の入力チャンネルを拒否する。
- **仮説／未検証:** RingBufferが12ch layoutを扱う必要があるなら、enum上の対応と実装の最大チャンネル数が一致していない可能性がある。全consumerが12chを使わない構成かは未検証。
- **価値／懸念:** 7.1.4 audioをこのring bufferへ流すとwrite失敗となり、上流が失敗を適切に扱わなければ音声欠落につながる可能性がある。
- **次に確認:** 利用箇所から実際の最大channel layoutとwrite失敗時の扱いを追い、仕様を確定してから最大ch境界のstandalone契約を追加する。

### 2026-10-11 — AudioBufferQueueのチャンネル長チェックがframeCountと整合しない

- **関連:** `ArtifactCore/include/Audio/AudioBufferQueue.ixx`、`ArtifactCore/include/Audio/AudioSegment.ixx`、`tests/ArtifactCore/AudioBufferQueueStandaloneTest.cpp`。
- **確認できた事実:** `AudioSegment::frameCount()`は全チャンネルの最小長を返す。`AudioBufferQueue::push()`はその値より短いチャンネルを探すが、同じチャンネル群から計算した最小長より短いものは存在しないため、この長さ検証条件は成立しない。
- **仮説／未検証:** channelごとにサンプル数が異なる不正segmentもqueueへ入り得る。意図した契約が可変長チャンネルを許さない場合は、不揃いデータを拒否すべき。
- **価値／懸念:** consumerがフレーム数と各channelの有効サンプル数を異なる前提で処理すると、欠落サンプルの無音化や範囲外アクセスにつながる可能性がある。
- **次に確認:** AudioBufferQueue利用側のchannel length前提を追跡し、許可された範囲で均一長要件またはframeCount基準のvalidationを定義して不揃いchannelの契約テストを追加する。

### 2026-10-11 — ArtifactOptional swap writes across inactive union member

- **関連:** `ArtifactCore/src/Core/ArtifactOptional.cppm`、`tests/ArtifactCore/ArtifactOptionalContractTest.cpp`。
- **確認できた事実:** owning `ArtifactOptional<T>::swap` のpresent／empty分岐で、empty側の `contained_` にplacement constructionではなくmove assignmentを行ってから `hasValue_` をtrueにする。
- **仮説／未検証:** 非trivialな `T` ではinactive union memberのobject lifetime開始条件を満たさない可能性がある。新しいmove-only payloadケースは通常の値状態を検査するが、lifetime違反を直接証明するものではない。
- **価値／懸念:** ASan等で検出可能な未定義動作の疑いがあり、`unique_ptr` や他の非trivial payloadに対するswap安全性へ影響する可能性がある。
- **次に確認:** 子repo編集の指示範囲を確認したうえで、標準union lifetime規則を照合し、適切なsanitizer付きstandalone実行またはplacement constructionへの修正後検証を行う。

### 2026-10-11 — Audio ring-buffer capacity unit is implicit

- **関連:** `ArtifactCore/include/Core/Defaults.ixx`、`tests/ArtifactCore/DefaultsContractTest.cpp`。
- **確認できた事実:** `kAudioRingBufferCapacity` は `kDefaultSampleRate * 8` で定義され、コメントは8秒と説明している。チャンネル数は別定数 `kDefaultAudioChannels` で2となっている。
- **仮説／未検証:** 容量がsample frames単位かinterleaved scalar samples単位かは、この定数とコメントだけでは確定できない。チャンネル数を掛けるべきかの断定は未検証。
- **価値／懸念:** 単位を明示しないまま利用側がchannel countを乗除すると、実際の保持時間が意図とずれる可能性がある。
- **次に確認:** `kAudioRingBufferCapacity` の全利用箇所とbuffer indexing conventionを追い、unitと保持時間を定義してから追加契約を設ける。

### 2026-10-11 — Tag StringLike setter implementation is empty

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const S&)` のテンプレート定義は空で、非テンプレート `setName(const UniString&)` だけが名前を更新する。既存契約テストには `std::string` と `QString` を渡して更新を期待する2ケースがある。
- **仮説／未検証:** 空のテンプレート定義ではその2ケースの期待値を満たさない可能性が高い。C++20 module の実際の呼出し解決／ビルドは未確認。
- **価値／懸念:** Tag用standalone suiteの基本API契約に不整合がある可能性があり、ケース追加だけでは回帰品質を高められない。
- **次に確認:** 子repo編集の指示範囲を確認したうえで、許可された作業時にStringLike overloadの意図と実際のsuite結果を確認する。

### 2026-10-11 — RationalTime LCM can overflow before fallback

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** 異なるscaleの加算・減算は`lcm(s1, s2)`を呼び、`lcm`は`abs(a) / gcd(a,b) * abs(b)`をoverflow検査なしで計算する。その後にあるfallbackはLCM計算の後でしか到達しない。
- **仮説／未検証:** scaleのLCMがint64範囲を超える場合は既存の`fromSeconds` fallbackでwrapせず結果化するべきと考え、ほぼint64最大の互いに異なるscaleを使う加算・減算の独立caseを追加した。未実行。
- **価値／懸念:** 適切な値のRationalTime同士でも、異なる大きなtimebaseを混ぜた際に未定義signed overflowや誤結果を検出する。
- **次に確認:** LCMをchecked multiplyで構築し、表現不可時にfallbackへ進むよう修正した後、standalone suiteを実行して確認する。

### 2026-10-11 — HashValue unknown algorithm falls through to MD5

- **関連:** `ArtifactCore/src/Utils/HashValue.cppm`、`tests/ArtifactCore/HashValueContractTest.cpp`。
- **確認できた事実:** Qt algorithm変換は`Unknown`を含むdefaultでMD5を返し、`fromData`はそのdigest bytesを`Algorithm::Unknown`として保持する。結果の`isValid()`はfalseだが、raw bytesとhex digestは存在する。
- **仮説／未検証:** 未知のアルゴリズムをMD5へ代替すると呼び出し側が誤ったdigestを参照し得るため、factoryは空のinvalid値を返すべきと考え、独立caseを追加した。テスト未実行。
- **価値／懸念:** `Unknown`を指定した際のalgorithm downgradeを可視化し、誤ったハッシュ比較を防ぐ。
- **次に確認:** `fromData`と`fromFile`双方で未知enum値をdigest計算前に拒否する契約に統一する。

### 2026-10-11 — artifactAbs preserves negative floating zero

- **関連:** `ArtifactCore/include/Core/ArtifactMath.ixx`、`tests/ArtifactCore/ArtifactMathContractTest.cpp`。
- **確認できた事実:** 浮動小数点にも使える`artifactAbs`は`value < T{}`のときだけ符号反転するため、`-0.0 < 0.0`がfalseとなり符号bitが残る。
- **仮説／未検証:** absolute valueはnegative zeroをpositive zeroへ正規化する期待が妥当と考え、`std::signbit`で検査する独立caseを追加した。テスト未実行。
- **価値／懸念:** zeroの比較では見えない符号差を、後続の符号依存演算やserializationへ持ち越さないための検出。
- **次に確認:** floating overloadを`std::abs`等へ分けるか、negative zero保持を意図した仕様とするか決める。

### 2026-10-11 — RawImage accepts unsupported pixel type as valid

- **関連:** `ArtifactCore/src/Image/RawImage.cppm`、`ArtifactCore/include/Image/RawImage.ixx`、`tests/ArtifactCore/RawImageContractTest.cpp`。
- **確認できた事実:** `getPixelTypeSizeInBytes()`は既知scalar typeのみbyte幅を返し、未知typeでは0を返す。一方`isValid()`はshapeの正値、data非empty、pixelType非emptyだけを確認し、byte幅とdata長の整合を見ない。
- **仮説／未検証:** 有効なraw imageは既知pixel typeを持ち、width × height × channels × byte widthのpayloadを最低限含むべきと考え、未知type拒否と不足buffer拒否の独立caseを追加した。実装はArtifactCore子repoのため変更していない。テスト未実行。
- **価値／懸念:** 下流処理でbyte幅0や途中で切れたpixel bufferを有効画像として扱う危険を検出する。
- **次に確認:** oversized bufferの許容、strideや平面別layoutの有無、サイズ計算overflowを確認して必要byte数の厳密な契約を定める。

### 2026-10-11 — Id parser can ignore suffix after embedded NUL

- **関連:** `ArtifactCore/src/Utils/Id.cppm`、`tests/ArtifactCore/IdContractTest.cpp`。
- **確認できた事実:** QStringからIdを構築するとUTF-8化後の`QByteArray::constData()`をBoost UUID parserへ渡す。NUL以降のbyteはC文字列として見えない。
- **仮説／未検証:** UUID入力全体を検証し、末尾の埋め込みNULとsuffixを含む入力は拒否すべきと考え、valid UUID + NUL + suffixのtestを追加した。テスト未実行。
- **価値／懸念:** 見かけ上不正な入力が有効UUID prefixへ切り詰められるのを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** parserへ明示byte長を渡すか、Boost parser後に消費長と元QString長が一致することを検証し、NUL境界を独立suiteで確認する。

### 2026-10-11 — Result can be successful without a value

- **関連:** `ArtifactCore/include/Utils/Result.ixx`、`tests/ArtifactCore/ResultContractTest.cpp`。
- **確認できた事実:** `Result<T>(Status)`はstatusをそのまま保持し、`Result<T>::success()`はvalue optionalでなくstatusのsuccess flagを返す。そのため`Result<int>(Status::ok())`は`success()==true`かつ`hasValue()==false`になる。
- **仮説／未検証:** valueを持たないResult<T>はfailure状態として正規化すべきと考え、食い違いを防ぐcaseを追加した。テスト未実行。
- **価値／懸念:** callerが成功を確認後にvalueを使う通常の契約で、未定義の空optional参照へ進む可能性を検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** `Status::ok()`からの変換方針を定め、Result<T>の成功状態とvalue presenceを常に整合させる。

### 2026-10-11 — ArtifactHashMap tryEmplace eagerly constructs and replaces duplicate value

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** variadic `tryEmplace(key, args...)`は`emplaceImpl(key, V(args...))`を呼ぶため、重複確認より前にmapped値を構築する。共通`emplaceImpl`は重複時に既存mapped値へ転送代入している。
- **仮説／未検証:** try-emplace系操作では重複時にmapped値を構築せず、既存値も保持するべきと考え、構築回数と値を検査するcaseを追加した。テスト未実行。
- **価値／懸念:** 重複keyに対する不要な副作用と、既存値の意図しない上書きを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** `tryEmplace`と`emplace`の重複時契約を分け、構築遅延と値保持を実装後に独立suiteで検証する。

### 2026-10-11 — ArtifactArray appendAll self-alias can fail to terminate

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `appendAll(const Array& other)`は`other`のrange-for中に`append()`する。`other`が*thisと同じobjectの場合、反復中にsizeが増え続け、end iteratorも動くため処理が終了しない。
- **仮説／未検証:** self-appendは元の要素列を一度だけ複製する契約が使いやすいと考えるが、ハングを起こすテストは追加していない。
- **価値／懸念:** 明示的なaliasingで無限に配列を伸ばし続ける危険がある。ArtifactCore子repoの実装は変更していない。
- **次に確認:** alias-safeなappendAll実装方針を決め、固定された元sizeでのself-appendを独立suiteで検証する。

### 2026-10-11 — Buffer afterLast drops input byte when delimiter is absent

- **関連:** `ArtifactCore/include/Utils/Buffer.ixx`、`tests/ArtifactCore/BufferContractTest.cpp`。
- **確認できた事実:** `splitLast()`はdelimiter不在時に`{empty, value}`を返すが、`afterLast()`は右partがemptyかどうかだけを調べ、非emptyなら先頭byteを常に除去する。
- **仮説／未検証:** delimiter不在時のafter-last helperは入力全体を保つべきと考え、主要aliasを同じcaseで検査するテストを追加した。テスト未実行。
- **価値／懸念:** separatorなしの文字列から誤って先頭byteが失われる境界を検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** delimiterが見つかったかを明示するAPIに分けるか、split結果の区別を使ってbefore/after helperの不在契約を揃える。

### 2026-10-11 — ArtifactRegex invalid recompile retains capture count

- **関連:** `ArtifactCore/include/Core/ArtifactRegex.ixx`、`tests/ArtifactCore/ArtifactRegexContractTest.cpp`。
- **確認できた事実:** `compile()`は開始時にprogramを消去するが、`groupCount_`は正常compile完了時にしか更新しない。そのためcapture付きregexの後にcompile失敗すると、invalid objectから以前のcapture数が返る。
- **仮説／未検証:** invalid状態のcapture数は0へ戻すべきと考え、2 captureから構文エラーへの再compileを検査するcaseを追加した。テスト未実行。
- **価値／懸念:** stale parser metadataを検出する。ArtifactCore子repoの実装は変更していない。
- **次に確認:** invalid patternおよびemitter失敗後に、公開metadataが常に初期状態へ戻る契約を確認する。

### 2026-10-11 — AspectRatio decimal approximation skips positive minimum

- **関連:** `ArtifactCore/src/Core/AspectRatio.cppm`、`tests/ArtifactCore/AspectRatioContractTest.cpp`。
- **確認できた事実:** decimal近似ループは分母を1〜100で探索する一方、`round(ratio * denom) <= 0`の候補をすべて飛ばす。`0.004`では最大分母でも丸めた分子が0となり、初期値の`1:1`が残る。
- **仮説／未検証:** 正の入力は許容分母内で最も近い正の分数へ近似するのが望ましいと考え、`0.004`に対する`1:100`を契約テストに追加した。テスト未実行。
- **価値／懸念:** 1/200より小さい正のratioが1:1へ化ける境界を見つける。ArtifactCore子repoの実装は変更していない。
- **次に確認:** decimal parserの近似仕様を決め、極小正数と最大許容分母周辺を独立suiteで検証する。

### 2026-10-11 — Audio profiler reset leaves last callback frame metadata stale

- **関連:** `ArtifactCore/include/Utils/PerformanceProfiler.ixx`、`tests/ArtifactCore/PerformanceProfilerContractTest.cpp`。
- **確認できた事実:** `AudioEngineProfiler::resetStats()`はcallback/fill counters、sums、maxima、underflow count、buffer levelを0にするが、`lastCallbackFrames_`と`lastRequestedFrames_`をリセットしない。
- **仮説／未検証:** reset後snapshotはframe metadataも初期値0へ戻すと考え、caseを追加した。実行していない。
- **価値／懸念:** reset後のsnapshotが古いcallback状態を示し、集計値と矛盾する境界を検出する。
- **次に確認:** reset APIの期待契約を確認し、必要なら2つのatomicをreset処理へ含めて独立suiteで検証する。

### 2026-10-11 — ArtifactRandom real range width can overflow

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** `between(double min, double max)`は`max - min`を先に計算するため、両端が有限でも`lowest()`から`max()`までの幅は正のinfinityになる。
- **仮説／未検証:** 有限な順序付き端点からは有限で半開区間内の値を返すべきと考え、全域端点のcaseを追加した。テスト未実行。
- **価値／懸念:** 広いrangeでinfinity / NaNが返る数値境界を検出する。ArtifactCore子repoの実装修正は行っていない。
- **次に確認:** overflowしない補間式または生成器を選び、端点近傍・最大範囲の分布を独立suiteで確認する。

### 2026-10-11 — Safe relative path predicate accepts embedded NUL

- **関連:** `ArtifactCore/include/Utils/Text/Path.ixx`、`tests/ArtifactCore/PathContractTest.cpp`。
- **確認できた事実:** `normalizePathSeparators()`はembedded NULを拒否するが、`isSafeRelativePath()`は空・absolute・`..` segmentだけを確認し、`"safe\\0name"`のようなNUL含有文字列をtrueにする。
- **仮説／未検証:** `isSafeRelativePath()`の「safe」判定はNUL含有入力も拒否すべきと考え、単独caseを追加した。テスト未実行。
- **価値／懸念:** C系APIへ渡した場合に文字列が途中で切れる可能性のあるpathを安全と誤判定する境界を検出する。ArtifactCore child repoの修正は行っていない。
- **次に確認:** path predicate群でNULの方針を統一し、修正後に独立suiteで確認する。

### 2026-10-11 — NameGenerator reinterprets placeholder text inside base names

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `makeCandidateZero()`はテンプレートの`(\\name)`をbase文字列で置換した後、完成途中の文字列全体から最初の`***`を検索する。baseに`***`が含まれると、番号スロットより先にbase内の文字が置換される。
- **仮説／未検証:** baseは任意の利用者名として不透明に保持されるべきと考え、`take***`を含む入力のcaseを追加した。実行していない。
- **価値／懸念:** パターン構文と値の文字列が衝突し、生成名が破損する境界を検出する。ArtifactCore側の修正はサブモジュール編集禁止のため行っていない。
- **次に確認:** テンプレートを値挿入前に分解する方法でslot位置を保持し、独立suiteで実行確認する。

### 2026-10-11 — Numeric min/max NaN behavior depends on argument order

- **関連:** `ArtifactCore/include/Utils/Numeric.ixx`、`tests/ArtifactCore/NumericContractTest.cpp`。
- **確認できた事実:** `min_same` / `max_same` はcommon typeへ変換後、`std::min` / `std::max` に委譲する。NaNとの `<` 比較がfalseとなるため、第1引数がNaNならNaNを返し、第2引数がNaNなら有限値を返す。
- **仮説／未検証:** 既存の標準アルゴリズム委譲を契約として記録するcaseを追加した。テストは未実行。
- **価値／懸念:** NaNの順序依存性を可視化し、将来の置換・最適化で無意識に意味が変わるのを検知できる。呼び出し側がNaN伝播を期待する場合は別途明示処理が必要。
- **次に確認:** 数値ユーティリティの公開契約としてNaN伝播を保証すべきか、利用側の要件を確認する。

### 2026-10-11 — NameGenerator padding is capped by local buffer

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `formatPaddedNumber()`は32 byteのstack bufferへ`snprintf`し、幅32以上では終端nullを含む固定領域によりrequested widthを満たせない。
- **仮説／未検証:** `zeroPad`は最小桁数として機能すべきとし、width 40の回帰caseを追加。実行していない。
- **価値／懸念:** 大きいpadding設定で生成名が切れる/幅不足となる問題を検出する。
- **次に確認:** `zeroPad`の上限契約を決め、可変長ではなくbounded安全な生成形式を選んでstandalone testを実行する。

### 2026-10-11 — ArtifactFunctionRef accepts a null function pointer as valid

- **関連:** `ArtifactCore/include/Core/ArtifactFunctionRef.ixx`、`tests/ArtifactCore/ArtifactFunctionRefContractTest.cpp`。
- **確認できた事実:** function pointer overloadは入力pointerのnull検査なしでstorageに保存し、常にinvoke thunkを設定するため`isValid()`がtrueとなる。
- **仮説／未検証:** null function pointerはdefault/nullptr wrapper同様invalidであるべきと考え、trait/runtime caseを追加した。未実行。
- **価値／懸念:** wrapperがnull targetをcallableと誤認してinvoke時に落ちる境界を検出する。
- **次に確認:** constructorでnull関数ポインタを空状態として扱い、standalone caseを実行確認する。

### 2026-10-11 — UniformScale implicitly copies owning Impl pointer

- **関連:** `ArtifactCore/include/Core/UniformScale.ixx`、`ArtifactCore/src/Core/UniformScale.cppm`、`tests/ArtifactCore/UniformScaleContractTest.cpp`。
- **確認できた事実:** `UniformScale`はowning raw `Impl*`とuser-declared destructorを持つ一方、copy/move operationsを宣言していない。C++の暗黙copyはポインタ値を複製する。
- **仮説／未検証:** 公開型は少なくともshallow copy不可であるべきとしてtrait assertionをruntime testにした。現実装ではcopy traitがtrueとなり失敗見込み。未実行。
- **価値／懸念:** 二重deleteを招くcopy pathの回帰を検出する。実装修正はArtifactCore child repo変更にあたるため、今回は行わない。
- **次に確認:** copy/move禁止または安全な所有権移譲を明示し、standalone suiteでtrait契約を実行する。

### 2026-10-11 — RationalTime rounding bias can overflow independently

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `rescaledTo()`が積のoverflowを回避した後、正負の両経路で`denominator / 2`をnumeratorへ足す。`INT64_MAX / 2`相当の端数roundingでbias加算自体が符号付き範囲を超える。
- **仮説／未検証:** 結果はhalf-away-from-zeroの正負対称値になるべきとし、新しい端値caseを追加した。実行していない。
- **価値／懸念:** 積overflow fallbackだけでは保護されないsigned UBを独立に検出する。
- **次に確認:** 端値を安全に丸める除算・剰余ベース実装を行い、両符号の結果をstandalone suiteで検証する。

### 2026-10-11 — RationalTime rescaling negates INT64_MIN

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `rescaledTo()`の負数丸め式は`-numerator`を評価し、`numerator == INT64_MIN`では符号付き範囲を超える。
- **仮説／未検証:** `INT64_MIN`を同scaleまたはscale 2から1へ変換する数学的結果を保つべきとしてcaseを追加した。未実行。
- **価値／懸念:** 丸めbias overflowとは別に、最小値だけで起こるnegation UBを検出する。
- **次に確認:** magnitudeベースの符号付き丸め処理を用意し、INT64_MINの恒等・縮小変換を実行検証する。

### 2026-10-11 — FileTypeDetector may accept invalid UTF-8 bytes as text

- **関連:** `ArtifactCore/src/File/FileTypeDetector.cppm`、`tests/ArtifactCore/FileTypeDetectorContractTest.cpp`。
- **確認できた事実:** magic判定のtext fallbackはheader bytesのcontrol rangeだけを検査し、UTF-8 continuationやscalar validityを確認している処理が見当たらない。既存suiteにはinvalid lead/continuationがBinaryという期待がある。
- **仮説／未検証:** invalid leadとcontinuation以外のoverlong, surrogate, out-of-range, truncated sequenceもBinaryになるべきと考え、table caseを追加した。コード読解上は現実装のtext fallbackで通る可能性があるが、テストは未実行。
- **価値／懸念:** UTF-8妥当性の穴をより完全に検出する。既存期待と実装が不一致の可能性があり、ArtifactCore child repoには変更を加えていない。
- **次に確認:** standalone suiteで上記4 encoding classを実行し、必要ならUTF-8 scalar validatorを実装・別依頼で扱う。

### 2026-10-11 — AssetMetaFile setters preserve extension JSON

- **関連:** `ArtifactCore/src/Asset/AssetMetaFile.cppm`、`tests/ArtifactCore/AssetMetaFileContractTest.cpp`。
- **確認できた事実:** `setUuid()` / `setType()` は既存`asset` objectを取得して対象keyだけを更新し、`data_`内の他のroot fieldも残す。standalone caseはsave/load後の未知field保持を検査する。
- **仮説／未検証:** 将来追加されたmetadata fieldを古いreaderが更新しても壊さず保存できることが互換性上望ましい。caseは追加したが未実行。
- **価値／懸念:** forward-compatible metadata round tripを回帰契約化する。未知fieldを意図して削除するAPIが将来必要な場合は明示操作との区別が要る。
- **次に確認:** `AssetMetaFileStandalone`でroot/asset extension fieldのsave/load往復を実行する。

### 2026-10-11 — SteadyClock tick subtraction can overflow

- **関連:** `ArtifactCore/include/Core/ArtifactChrono.ixx`、`tests/ArtifactCore/ArtifactChronoContractTest.cpp`。
- **確認できた事実:** `SteadyClock::between()`は両OS経路でtick差を符号付き`int64_t`上で減算する。POSIXは`to > from`判定後だが、Windowsは差分を計算してから符号を検査する。全域の正差と逆順差の絶対値は`int64_t`で表せない。
- **仮説／未検証:** 正方向の最大Duration飽和と逆方向の0 clampが期待動作と考え、extreme tick差の両向きcaseを追加した。未実行。
- **価値／懸念:** 長時間や不正tick入力での符号付き整数overflowを検出する。Windows分岐では逆順入力も減算自体がoverflowする。
- **次に確認:** 安全な差分計算（符号付き境界判定）を実装し、WindowsとPOSIX双方で境界を実行検証する。

### 2026-10-11 — FrameRate storage scale can overflow on finite input

- **関連:** `ArtifactCore/include/Frame/FrameRate.ixx`、`tests/ArtifactCore/FrameRateContractTest.cpp`。
- **確認できた事実:** `storageScaleForFps()`はNaN/Inf/nonpositive値をfallbackへ送るが、有限正値は上限確認なしで`std::llround`後`int64_t`へ変換する。
- **仮説／未検証:** `double`の極大有限値は表現可能範囲を大幅に超えるため、整数上限への飽和が安全な契約と考え、新しい回帰caseを追加した。未実行。
- **価値／懸念:** legacy frame scale変換で範囲外の浮動小数点整数変換を検出できる。期待値は上限飽和のAPI契約を明示するため、実装修正が必要になる見込み。
- **次に確認:** `fps > int64_t max`判定を`llround`より先に行う実装へ直し、独立FrameRate suiteで極大値と境界近傍を実行確認する。

# Insight Register

### 2026-10-11 — FastSettingsStore failed reload clears existing cache

- **関連:** `ArtifactCore/src/Core/FastSettingsStore.cppm`、`tests/ArtifactCore/FastSettingsStoreContractTest.cpp`。
- **確認できた事実:** `loadFromDiskUnlocked()`はCBORの読み込み・型検査より前に`cache_`をclearし、invalid payloadではfalseを返す。`reload(true)`はこの関数を直接呼ぶ。
- **仮説／未検証:** reload失敗時は最後に読み込み済みの値を保持し、エラー復旧可能にする方が望ましい。新しいcaseはその期待だが、実行していない。
- **価値／懸念:** ディスク破損や途中書き込み後のreloadでメモリ上の有効設定が消える可能性を検出する。ArtifactCore child repoは変更していない。
- **次に確認:** invalid CBOR・I/O失敗時のreload保証を定義し、必要ならtemporary cacheへのparse後にcommitする方式へ直して独立suiteで確認する。

### 2026-10-11 — FastSettingsStore sync failure retains retry state

- **関連:** `ArtifactCore/src/Core/FastSettingsStore.cppm`、`tests/ArtifactCore/FastSettingsStoreContractTest.cpp`。
- **確認できた事実:** `syncUnlocked()`は未open、親directory作成、`QSaveFile::open/write/commit`の失敗時にfalseを返し、成功時だけdirty/pendingをclearする。
- **仮説／未検証:** 各I/O失敗経路でdirty値とpending countが保たれ、後のretryが可能なことを回帰契約にした。新caseはtargetを既存directoryにして保存open失敗を狙うが、実行していない。
- **価値／懸念:** 保存エラー後に変更が誤ってclean扱いされる回帰を検知する。ディレクトリをファイルとして開けないOS挙動に依存する。
- **次に確認:** 実行環境で`QSaveFile`のopen failureとして安定するかを確認し、必要なら明示的に書込不能な親パスを使う。

### 2026-10-11 — RationalTime rescaledTo accepts negative target scales

- **関連:** `ArtifactCore/src/Time/RationalTime.cppm`、`tests/ArtifactCore/RationalTimeContractTest.cpp`。
- **確認できた事実:** `toFrameCount()`はfpsが0以下なら0を返す一方、`rescaledTo()`はtarget scaleが0の場合だけを無効扱いし、負値では負の分子として再スケールする。RationalTimeのconstructorは非正scaleを1に正規化する。
- **仮説／未検証:** `rescaledTo()`のtarget scaleも正値に限り、負値は0または安全な既定値を返すべきと考える。追加caseは0を期待するが実行していない。
- **価値／懸念:** 不正なtarget scaleで時間方向が反転する結果を防ぐ契約候補。ArtifactCore child repoは変更していない。
- **次に確認:** 呼び出し元で負target scaleが意図的に利用されていないことと、無効入力の期待値を確認してから実装修正する。

### 2026-10-11 — ArtifactHashMap moved-from destruction may dereference null buckets

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** move constructorとmove assignmentは移動元の`buckets_`を`nullptr`にし、`bucketCount_`を初期容量に戻す。destructorは常に`clear()`を呼び、`clear()`は`bucketCount_`回`buckets_[i]`を参照する。
- **仮説／未検証:** 別mapへのmove後に移動元が破棄されるとnull pointer dereferenceになる可能性が高い。実行していない。
- **価値／懸念:** move APIの基本的な破棄安全性に関わる。再現テストはstandalone suite全体をクラッシュさせる可能性があるため今回追加しなかった。ArtifactCore child repoは変更していない。
- **次に確認:** move後のsourceへ空bucketを再確保する等、valid destructible invariantを実装者と確認し、安全に失敗を報告できるテスト形態で検証する。

### 2026-10-11 — ShapeVertex mirrorHandles length mode is indistinguishable

- **関連:** `ArtifactCore/include/Core/ShapePoint.ixx`、`tests/ArtifactCore/ShapeVertexContractTest.cpp`。
- **確認できた事実:** `mirrorHandles(bool keepLength)`はtrue/false両分岐で`outHandle_`を`-inHandle_`に置き換えており、既存outgoing handleの長さを読み取らない。リポジトリ内の他の呼び出し・説明は検索で見つからなかった。
- **仮説／未検証:** `keepLength=false`は反対側ハンドルの既存長を維持しながらincoming方向と反対向きにそろえる意図かもしれない。新しい契約caseはこの解釈を期待するが、実装者の意図を示す仕様は見つけていないためテストは未実行。
- **価値／懸念:** UIから選べる長さモードが無効になっている可能性を切り分ける。ArtifactCore child repoは今回変更していない。
- **次に確認:** 呼び出し元または設計仕様で`keepLength`の意味を確定し、期待値を承認してから実装修正と独立suite実行を行う。

### 2026-10-11 — ArtifactTimer can expire before it starts for negative timeouts

- **関連:** `ArtifactCore/include/Core/ArtifactTimer.ixx`、`tests/ArtifactCore/ArtifactTimerContractTest.cpp`。
- **確認できた事実:** `hasExpired(timeoutMs)` はtimer validityを確認せず、`timer_.elapsed() >= timeoutMs` を返す。Qtの無効な`QElapsedTimer`はelapsedが負値となり、既存テストは未startの0 timeoutを未expired、開始済みの負timeoutをexpiredとしている。
- **仮説:** 未start timerではtimeout値にかかわらず未expiredを返すのがAPIとして一貫する。新しい負timeout未startケースは現実装でexpiredになる見込みだが、実行していない。
- **価値／懸念:** start前に待機完了と誤判定する呼び出しを検出できる。実装修正はArtifactCore child repoにあるため今回は行わない。
- **次に確認:** 意図する無効timerの契約を確認し、必要なら`isValid()` guardを加えて独立suiteを実行する。

### 2026-10-11 — FileTypeDetector treats malformed UTF-8 headers as text

- **関連:** `ArtifactCore/src/File/FileTypeDetector.cppm`、`tests/ArtifactCore/FileTypeDetectorContractTest.cpp`。
- **確認できた事実:** The text fallback rejects ASCII control bytes other than tab/LF/CR, but accepts every byte >= 32 without validating UTF-8. A regression case now expects malformed UTF-8 lead/continuation sequences to classify as Binary; it is not executed.
- **仮説:** Since valid UTF-8 Japanese text is already explicitly supported by a test, malformed multibyte sequences should not be classified as text. Current fallback will classify both new fixtures as Text.
- **価値／懸念:** This distinguishes valid Unicode text from arbitrary high-bit binary data. The implementation is in the ArtifactCore child repository, which is not modified here.
- **次に確認:** Confirm the intended accepted encodings (UTF-8 only or UTF-16/legacy as well), implement matching validation in the child repository if authorized, then run the standalone target.

### 2026-10-11 — AssetMetaFile tag removal does not normalize input

- **関連:** `ArtifactCore/src/Asset/AssetMetaFile.cppm`、`tests/ArtifactCore/AssetMetaFileContractTest.cpp`。
- **確認できた事実:** `addTag()` stores `tag.trimmed()`, while `removeTag()` compares each stored tag directly to the untrimmed argument. Added a standalone contract case that adds and removes the same whitespace-padded tag; based on the implementation it is expected to fail, but it was not run.
- **仮説:** Callers that pass the same user-entered tag string to add and remove can leave a tag behind because only the insertion path normalizes whitespace.
- **価値／懸念:** This test catches asymmetric tag matching without editing the child repository. If the intended contract is exact matching for removal, document that explicitly instead of normalizing.
- **次に確認:** Confirm tag matching policy, then fix/remove the regression case with the user-authorized ArtifactCore change and execute the standalone target when allowed.

### 2026-10-11 — ArtifactArray squeeze currently cannot reduce capacity

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `squeeze()` calls `reserve(size_)`, while `reserve(n)` immediately returns whenever `n <= capacity_`. Thus a non-empty array with spare capacity retains its old allocation. Added a standalone contract case expecting capacity to shrink to size while preserving elements; it was not executed.
- **仮説:** The API name and QVector-like capacity interface imply `squeeze()` should release excess storage; current implementation does not.
- **価値／懸念:** This regression case detects an ineffective public capacity operation. Fixing it requires editing the ArtifactCore child repository and remains outside this test-only scope.
- **次に確認:** With child-repository change authorization, implement allocate/move/release or equivalent safe shrink logic, then run the standalone suite and non-trivial lifetime case.

### 2026-10-11 — ArtifactRandom inclusive maximum overflows its upper bound

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** integer `between(min, max)` computes `max + 1` in signed `int` before passing it to Qt's bounded generator. Existing boundary tests stop at `INT_MAX - 1`.
- **仮説:** `between(INT_MAX, INT_MAX)` overflows before generation and cannot reliably satisfy the documented-by-tests inclusive range behavior. A singleton-at-maximum regression case was added; it was not executed.
- **価値／懸念:** Highest-end integer boundary is now represented in the independent suite, but the case may fail or invoke undefined behavior until the child implementation handles that endpoint.
- **次に確認:** ArtifactCore-side remediation should generate over a wider unsigned interval or special-case the upper endpoint; then execute this standalone target after explicit authorization.

### 2026-10-11 — ArtifactVariant empty-state destruction dispatch

- **関連:** `ArtifactCore/include/Core/ArtifactVariant.ixx`、`tests/ArtifactCore/ArtifactVariantContractTest.cpp`。
- **確認できた事実:** default construction sets `typeIndex_` to zero and exposes `isSet()==false`, but `destroy()` unconditionally dispatches `vtableForCurrent()` using the first alternative before resetting the index. Destructor and assignment call `destroy()` even for that unset state.
- **仮説:** an unset variant may invoke the first alternative's destructor on storage where no object lifetime began; behavior is especially hazardous when the first alternative is non-trivial. Added tests exercise default/copy/move unset state and assignment clearing an active alternative. They are unexecuted; sanitizers/runtime validation is still needed.
- **価値／懸念:** Empty-state lifecycle coverage can expose undefined destruction behavior hidden by the current first-alternative-is-int fixture. Fixing this requires editing the ArtifactCore child repository, which is outside the authorized scope.
- **次に確認:** With explicit permission for child-repository changes and test execution, guard `destroy()` on `typeIndex_ != 0`, then run the standalone suite under an available sanitizer.

### 2026-10-11 — Tag StringLike setter has an empty implementation

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const S&)` の `StringLike` template body は空である。実装側にある非-template setterは `const UniString&` overloadのみ。`StringLike` は `QString` と `std::string` を含む型を受け入れる。
- **仮説:** `QString` / `std::string` を直接渡す通常の呼び出しは空のtemplate overloadを選び、名前を変更しない可能性が高い。追加した2つのstandalone contract caseはこのAPI意図を固定し、実行時に失敗する見込みだが未実行。
- **価値／懸念:** generic setterを使う呼び出しが黙って無視される不具合を検出できる。修正が必要なら子repository編集禁止ルールに従い、ArtifactCore側の実装修正は別途明示依頼を待つ。
- **次に確認:** `Tag` のStringLike setterの意図と所有repoでの修正可否を確認し、許可後にこのstandalone suiteを実行する。

### 2026-10-11 — Id contract test references undeclared isValid

- **関連:** `ArtifactCore/include/Utils/Id.ixx`、`tests/ArtifactCore/IdContractTest.cpp`。
- **確認できた事実:** 現在の `Id` module interfaceには`isValid()`宣言がなく、テストの`ValidUuidStringRoundTripsInCanonicalForm`は`value.isValid()`を呼び出している。`Id.cppm`にもその定義は見当たらない。
- **価値／懸念:** 現行ソースのままでは独立suiteのコンパイルを妨げる可能性が高い。ビルド／テストは指示により実施していないため、実コンパイル診断は未確認。
- **次に確認:** `isValid()`を追加する設計意図があるか確認し、なければテストを`!isNil()`等の実在する契約に修正する。変更後にユーザー許可を得て独立suiteを実行する。

### 2026-10-11 — Tag StringLike setter template is empty

- **関連:** `ArtifactCore/include/Utils/Tag.ixx`、`ArtifactCore/src/Utils/Tag.cppm`、`tests/ArtifactCore/TagContractTest.cpp`。
- **確認できた事実:** `Tag::setName(const UniString&)` は `impl_->name` を更新するが、`StringLike` を受けるheader template overloadの本体は空。
- **価値／懸念:** `setName(QString/std::string/const char*)` など、`StringLike` conceptを満たす型で呼び出しても名前は変わらない。今回の独立suiteは動作する`UniString` overloadだけを検査し、この未実装挙動は通過テストとして隠さない。
- **次に確認:** template setterで`toQString`して`UniString` overloadへ委譲するか、template APIを削除するか判断した後、各対応文字列型のsetNameを回帰テストする。

### 2026-10-11 — ArtifactDict set calls absent ArtifactHashMap insert

- **関連:** `ArtifactCore/include/Core/ArtifactDict.ixx`、`ArtifactCore/src/Core/ArtifactHashMap.cppm`。
- **確認できた事実:** `ArtifactDict::set` は`map_.insert(key, value)`を呼ぶが、基底`ArtifactHashMap`が公開する挿入APIは`tryEmplace` / `emplace`で、`insert`は定義されていない。
- **価値／懸念:** `ArtifactDict::set`をインスタンス化するコードはコンパイルできない。既存の`ArtifactDict`テストはこの経路を使っており、直接module構成で検証する独立suiteもまだない。
- **次に確認:** ArtifactDictがinsert-or-assignかinsert-onlyか意味を決め、ArtifactHashMapの既存APIへ接続してから独立standaloneでset/get/removeを検証する。

### 2026-10-11 — Callback documentation references missing ifValid API

- **関連:** `ArtifactCore/include/Core/ArtifactCallback.ixx`、`tests/ArtifactCore/ArtifactCallbackContractTest.cpp`。
- **確認できた事実:** Callbackのclass commentは`isValid()`確認または`ifValid()`利用を案内するが、公開メソッドに`ifValid`はなく、`isValid` / `operator bool` / `invoke` / `clear` / `reset`だけがある。
- **価値／懸念:** 利用者が文書の例外安全な呼び出しAPIを探しても利用できず、empty時に直接`invoke`するとempty `std::function`の呼び出しになる。
- **次に確認:** null-safe invocation APIを追加するか、commentを実在APIに合わせる判断後、empty経路の仕様をテストする。

### 2026-10-11 — ArtifactFunction unsafe empty invocation and heap move

- **関連:** `ArtifactCore/include/Core/ArtifactFunction.ixx`、`tests/ArtifactCore/ArtifactFunctionContractTest.cpp`。
- **確認できた事実:** `operator()` は`invoker_`未設定時のguardなしに関数pointerを呼ぶ。heap callableのmoveでは`onHeap_`を先にtrueにした状態で、未初期化の移動先storageを`ptr()`経由で参照する。またmove後のsource callableはdestroyerを呼ばずmetadataだけnull化される。
- **価値／懸念:** 空function invocationは無効な関数pointer呼び出しとなる。heap callable moveは無効なstorage参照となり得て、SBO/heap双方のmoveはmoved-from callableのdestructorを欠落させる。
- **次に確認:** 空invokeを明示的な契約（assert/exception/no-op）にし、heap移動時のpointer transferとsource destructionを正しく実装してから、heap callable move・move assignment・destructor balance・empty invocationのテストを追加する。

### 2026-10-11 — ArtifactSpan end on null empty range

- **関連:** `ArtifactCore/include/Core/ArtifactSpan.ixx`、`tests/ArtifactCore/ArtifactSpanContractTest.cpp`。
- **確認できた事実:** default `ArtifactSpan` は`data_ == nullptr`、`size_ == 0`。`end()` は一律 `data_ + size_` を返す。
- **価値／懸念:** C++のポインタ加算対象となる配列がないnull pointerに対する`+ 0`は、標準上安全な配列内／one-past演算として保証されない。空spanをrange-forなどで走査する経路の扱いを確認したい。
- **次に確認:** empty span用にnullをそのまま返す分岐を設けるか、null pointer rangeをiteratorとして使わない契約にするか決め、default/empty spanのbegin/endとrange iterationを回帰テストする。

### 2026-10-11 — Position2D methods on moved-from objects

- **関連:** `ArtifactCore/include/Core/Position2D.ixx`、`ArtifactCore/src/Core/Position2D.cppm`、`tests/ArtifactCore/Position2DContractTest.cpp`。
- **確認できた事実:** move constructor/assignmentは`impl_`の所有権を移してsourceを`nullptr`にする。`Set` / `Get` はnull確認せず`impl_`を参照する一方、destructorはnull-safeな`delete`となる。
- **価値／懸念:** moved-from objectは破棄可能だが、再設定や読み出しはクラッシュする。通常のmoved-from型に期待される再代入・再利用可能性は満たさない。
- **次に確認:** moved-from状態を空として扱うか、有効なゼロ値へ再初期化するかを決め、move後のGet/Setと再代入の契約を追加する。

### 2026-10-11 — ArtifactPtr WeakPtr copy assignment ownership count

- **関連:** `ArtifactCore/include/Core/ArtifactPtr.ixx`、`tests/ArtifactCore/ArtifactPtrContractTest.cpp`。
- **確認できた事実:** `WeakPtr` はcopy constructorでweak countを増やすが、copy assignmentを定義していない。コンパイラ生成assignmentはcontrol-block pointerをmemberwise copyするだけなので、既存weak ownerへの代入でcountを適切に保持・解放しない。
- **価値／懸念:** 一時`WeakPtr`からの代入では一時objectのdestructorがcountを減らし、代入先が解放済みcontrol blockを参照する可能性がある。テストは未定義寿命状態を踏まないよう代入を使用しない。
- **次に確認:** copy/move assignmentの所有権契約を定め、weak countを保持する実装と同一control blockへのself-assignmentを回帰テストする。

### 2026-10-11 — ArtifactPtr UniquePtr reset with its current pointer

- **関連:** `ArtifactCore/include/Core/ArtifactPtr.ixx`、`tests/ArtifactCore/ArtifactPtrContractTest.cpp`。
- **確認できた事実:** `UniquePtr::reset(raw)` は先に `ptr_ = raw` を設定した後、元の `old` を削除する。そのため現在所有中の同じポインタを渡すと、所有値を解放した後にdangling pointerを保持する。
- **価値／懸念:** 同一アドレスへのresetはuse-after-free/double-deleteにつながり得る。
- **次に確認:** self-resetをno-opとする契約を定めて実装を修正し、同一pointerを渡した後の値アクセスと破棄回数を回帰テストする。

### 2026-10-11 — ArtifactThread condition notification synchronization

- **関連:** `ArtifactCore/include/Core/ArtifactThread.ixx`、`tests/ArtifactCore/ArtifactThreadContractTest.cpp`。
- **確認できた事実:** `Cond::wait(Lock&)` は渡された`Lock`のmutexを解放せず、別の`cv_lock_`でcondition variableを待つ。一方、`wakeOne/wakeAll`は`notified_`をmutex lockなしで書き換える。
- **価値／懸念:** wait前後のpredicate保護が呼び出し側mutexと同期せず、通知のlost wakeや`notified_`のdata raceを起こし得る。
- **次に確認:** `Lock`から待機中にmutexを解放／再取得できる設計にし、predicateを同一mutex下で確認するAPI契約を確定してrace・lost-wake回帰ケースを加える。

### 2026-10-11 — ArtifactOptional empty/present swap and empty initializer list

- **関連:** `ArtifactCore/src/Core/ArtifactOptional.cppm`、`tests/ArtifactCore/ArtifactOptionalContractTest.cpp`。
- **確認できた事実:** `ArtifactOptional<T>::swap` の片側だけpresentの分岐はempty側のunion memberへplacement constructionせずmove assignmentを行う。initializer-list constructorは空listでも`hasValue_`をtrueにし、contained `T`を構築しない。
- **価値／懸念:** empty/present swap後のdestructorは未構築`T`を破棄し、empty initializer-list optionalも同じ不正状態となる。
- **次に確認:** 片側presence swapはplacement move constructionを行う状態遷移に修正し、empty initializer-listはemptyとする契約を定めてそれぞれの回帰テストを追加する。

### 2026-10-11 — ArtifactMath signed absolute minimum overflow

- **関連:** `ArtifactCore/include/Core/ArtifactMath.ixx`、`tests/ArtifactCore/ArtifactMathContractTest.cpp`。
- **確認できた事実:** `artifactAbs` は負値を unary `-value` で正値化する。符号付き整数の最小値は対応する正値を同じ型で表せず、評価は符号付きoverflowになる。
- **価値／懸念:** `artifactAbs(INT_MIN)`等は安全な絶対値を返さない。
- **次に確認:** APIの戻り型を広げるか飽和させるか仕様を定め、整数最小値の回帰ケースを決める。

### 2026-10-11 — ArtifactRandom integer upper-bound overflow

- **関連:** `ArtifactCore/include/Core/ArtifactRandom.ixx`、`tests/ArtifactCore/ArtifactRandomContractTest.cpp`。
- **確認できた事実:** 整数版`between(min, max)`はinclusive化のため`max + 1`を`int`で計算してQt APIへ渡す。
- **価値／懸念:** `max == INT_MAX`の場合に符号付き整数overflowとなり、定義済み動作ではない。
- **次に確認:** 上限inclusive要件を保てる範囲幅APIまたは64-bit中間値による実装へ変更し、`INT_MAX`近辺を含む境界回帰ケースを追加する。

### 2026-10-11 — ArtifactVariant destroys storage while empty

- **関連:** `ArtifactCore/include/Core/ArtifactVariant.ixx`、`tests/ArtifactCore/ArtifactVariantContractTest.cpp`。
- **確認できた事実:** `destroy()` は `typeIndex_ == 0` の空状態でも index 0 の vtable を取得して destroyer を呼ぶ。index 0 は第1 alternativeであり、既定構築 variant の storageにはその型のオブジェクトが構築されていない。
- **価値／懸念:** 第1 alternativeが非自明デストラクタを持つ場合、空variantの破棄や値を持つvariantから空variantへの代入で未構築オブジェクトに対して破棄処理を行う。
- **次に確認:** `destroy()` は `isSet()` の時だけ破棄関数を呼ぶようにし、空状態の生成・破棄と値から空へのcopy assignmentを回帰テストする。

### 2026-10-11 — ArtifactAlgorithms erase API does not match ArtifactArray

- **関連:** `ArtifactCore/include/Core/ArtifactAlgorithms.ixx`、`ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactAlgorithmsContractTest.cpp`。
- **確認できた事実:** `artifactRemoveIf` は削除時に`container.eraseAt(index)`を呼ぶ。一方、`ArtifactArray`が提供する削除APIは`removeAt(index)`で、`eraseAt`は定義されていない。
- **価値／懸念:** `artifactRemoveIf(Array<T>&, ...)` は要求メンバーが一致せず直接利用できない。suiteではAPIどおり`eraseAt`を持つ小fixtureを介してアルゴリズム単体を検査する。
- **次に確認:** `artifactRemoveIf` を`removeAt`コンテナへ一般化するか、`ArtifactArray`へaliasを設けるか決めて、実際のArray統合テストを追加する。

### 2026-10-11 — ArtifactHashMap collision iteration and moved-from lifetime

- **関連:** `ArtifactCore/src/Core/ArtifactHashMap.cppm`、`tests/ArtifactCore/ArtifactHashMapContractTest.cpp`。
- **確認できた事実:** iteratorの`operator++()`はbucket indexを進め、現在Nodeの`next`を辿らないため、同一bucket内の複数Nodeをiterationで全件訪問しない。move constructor/assignmentは移動元のbucket pointerをnullにするが、destructorの`clear()`はbucket配列をdereferenceする。また、`const_iterator`はmutable `iterator` のaliasでconst mapの`find()`もmutable referenceを返す。`tryEmplace`重複時は既存mapped valueへ新valueを代入してfalseを返す。
- **価値／懸念:** collision iterationの取りこぼし、move元destruction時のnull dereference、const map経由の変更、try-emplace名称から期待される既存値非変更との不一致がある。
- **次に確認:** iteratorがchainとbucket遷移の両方を扱うよう修正し、const iteratorを読み取り専用にし、move元を空の有効状態にする。重複時のtryEmplaceを非変更契約にするか決める。回帰テストは修正後にcollision iteration、const性、重複値保持、move元スコープ終了を含める。

### 2026-10-11 — ArtifactArray squeeze currently cannot reduce capacity

- **関連:** `ArtifactCore/include/Core/ArtifactArray.ixx`、`tests/ArtifactCore/ArtifactArrayContractTest.cpp`。
- **確認できた事実:** `squeeze()` は `size_ < capacity_` の場合に `reserve(size_)` を呼ぶが、`reserve()` は要求容量が現在容量以下なら即returnする。従ってstorage容量は減らない。
- **価値／懸念:** 名前と一般的なcontainer契約から期待される容量縮小が起きず、過剰確保メモリを保持する。
- **次に確認:** 容量を縮める再配置処理が必要か判断し、必要なら要素move/destructionとallocation failure時の保証を定めた後でcapacity assertionを追加する。

### 2026-10-11 — AudioEngineProfiler reset retains last frame counts

- **関連:** `ArtifactCore/include/Utils/PerformanceProfiler.ixx`、`tests/ArtifactCore/PerformanceProfilerContractTest.cpp`。
- **確認できた事実:** `resetStats()` はcallback/fill counters、durations、underflow、buffer levelを0にするが、`lastCallbackFrames_` と `lastRequestedFrames_` はresetしない。
- **価値／懸念:** reset後snapshotが過去のframe数と新しい累積統計を混在させる。これが意図的なlast-known metadataか、reset漏れかは未検証。
- **次に確認:** reset後もlast-known値を保持する契約か決め、仕様がzero resetなら両atomicもresetして期待値を追加する。

### 2026-10-11 — PatternNameGenerator without a number slot can loop forever

- **関連:** `ArtifactCore/src/Utils/NameGenerator.cppm`、`tests/ArtifactCore/NameGeneratorContractTest.cpp`。
- **確認できた事実:** `Generate` はcandidateが `usedNames` に存在する限り番号を増やして再試行するが、`makeCandidateZero` はpatternに `***` がなければ候補文字列を変化させない。同じ固定文字列が使用済みになった後の再生成は終了しない。
- **価値／懸念:** 固定patternやname slotだけのpatternを複数回使うと呼び出しが停止する可能性がある。再試行上限なしのループであり、通常のテストではプロセスをハングさせうる。
- **次に確認:** number slotがないpatternの重複方針（エラー、同名返却、pattern妥当性検証）を決め、boundedな失敗契約を実装した後に回帰テストを追加する。

### 2026-10-11 — MultipleTag moved-from state is not reusable

- **関連:** `ArtifactCore/src/Utils/MulitpleTag.cppm`、`ArtifactCore/include/Utils/MultipleTag.ixx`。
- **確認できた事実:** move constructor と move assignment は移動元の `impl_` を `nullptr` にする。destructor は許容するが、`count()`、`clear()` などの通常メソッドは `impl_` を直接参照する。
- **価値／懸念:** 移動元を破棄以外に使うと null dereference になる。クラスの moved-from 契約が未定義であり、利用側が再利用可能と期待する場合は問題になる。
- **次に確認:** moved-from 状態を空の有効状態へ戻すか、型の契約として明示するかを決めてから、move 後再利用テストを追加する。

### 2026-10-11 — Buffer view lifetime and missing-delimiter edge

- **関連:** `ArtifactCore/include/Utils/Buffer.ixx`、`tests/ArtifactCore/BufferContractTest.cpp`。
- **確認できた事実:** `toBufferView(const QString&)` はローカル `QByteArray` のdata pointerを `BufferView` として返すため、戻り値はローカル所有物の破棄後を参照する。`afterLast` は区切りがないと `splitLast` が `{empty, value}` を返し、非empty側の先頭1byteを除いたviewを返す。
- **価値／懸念:** QString変換のviewはuse-after-lifetimeとなる。delimiter未検出時の `afterLast` は命名上期待される意味とずれる可能性がある（期待仕様は未確認）。独立suiteではこれらの不安定な期待値を固定せず、テスト対象から外した。
- **次に確認:** Buffer view APIで所有権が期待されるか、または呼び出し側所有のUTF-8作業領域を受け取る形にするかを確認する。`afterLast`／`takeAfter`／`suffixAfter` の delimiter未検出仕様を定めて境界テストを追加する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

### 2026-10-10 — 2D/3D layer transform composition order

- **関連:** `ArtifactCore/src/Transform/TransformHelper.cppm`、`tests/ArtifactCore/CompositionTransformTimeContractTest.cpp`。
- **確認できた事実:** `combineLayerTransform2D(local, parent)` は `local * parent` を返す一方、3D 版は `parent * local` を返す。2D 実装コメントは親変換を子空間で適用すると記述し、3D 実装は `QMatrix4x4` の標準的な親×子順を使っている。
- **価値／懸念:** 2D と 3D の規約差が意図的か、ネストしたレイヤーの描画・座標変換で整合しているかは未検証。単体テストでは各 API の現在の契約を固定できるが、親子レイヤーの実描画結果までは保証しない。
- **次に確認:** 親子レイヤーを使う描画・bounds・入力座標の各経路で変換積の順序を追い、同じ階層構成の 2D/3D テストを追加すべきか判断する。

### 2026-10-12 — GPU Cloner fallback行列へstack scaleを反映

- **関連:** `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`、Cloner GPU placement。
- **確認できた事実:** GPU eligible用streaming列挙はCPU clone transformを生成し、GPU matrix照合失敗時のsprite fallbackとcomposition field中心計算に同じ行列を使う。Plain modifierのXY scaleがplacement dispatchへ渡る一方、このstream行列には反映されず、generator transformがidentityの場合はmodifier適用前にcloneを除外していた。transform stackもshear-freeならrotation＋diagonal XY scaleとして既存placement recordへ集約できる。
- **変更:** CPU参照行列へstackとmodifier translation／Z rotation／XY scaleを適用してからidentity判定するようにした。stack XY scaleはshearを生まない順序に限定してplacement recordへ合成する。
- **価値／懸念:** GPU fallback時の表示・field queryでmodifier scaleが欠落する経路を塞ぎ、identity配置でもmodifierが変形するcloneを保持する。10,000件のshear-free affine合成を数値比較したが、Viewer runtimeの画素一致は未確認。
- **次に確認:** GPU dispatch失敗fixtureでtranslation／rotation／scale stackとPlain chainのCPU fallback画像をGPU placement結果と比較する。

### 2026-10-12 — Composition collision source情報を1評価内で再利用

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm::evaluateLayerCollisionPairs()`。
- **確認できた事実:** 各material targetのsource走査で、同じcollision sourceについてenabled/shape/offset/restitution property、global transform、world collision boundsを再取得・再計算していた。Outlineもsource-target pairごとに生成していた。
- **変更:** Composition-owned `ArtifactArray<CollisionLayerScratch>`へsource/target判定、collision properties、global transform、world bounds、target inverseを一度評価して保持。collider生成をsource outer / target innerへ変更し、各targetへ入るsource順は旧処理と同じに保つ。polygon outlineは最初の有効targetでlazy生成し、そのsourceの他targetで再利用する。頂点のtarget-local変換はtargetごとに残す。
- **価値／懸念:** 重複property accessor・bounds transform・inverse transformとmulti-target時のoutline生成を削減する。scratch要素はLayer SharedPtrを保持するため、従来通りremove/remove-all時の全scratch clearを維持した。source-target順の物理結果とruntime効果は未確認。
- **次に確認:** 1 source/複数target、複数source/複数target、同一source-target除外、inactive/非invertible target、empty polygon outline fallbackを比較し、transform評価回数・outline生成回数・solver collider順が一致することを確認する。

## 2026-10-13 — RAM replay の GPU residency と cache budget

- 確認済み事実: PrimitiveRenderer2D の video upload は owner と寸法ごとの単一 texture を上書きする。既存 sprite/mask texture LRU は 50 entry / 合計512 MiB。controller の RAM replay は CPU cache があってもこの upload を使っていた。
- 関連: Artifact/src/Render/PrimitiveRenderer2D.cppm、Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm。
- 今回の範囲: cold RAM publication 後の32 owner warmupと、内容一致のみを返す resident lookup。cache eviction 時は既存経路。総 budget は増やさない。
- 未検証の仮説: 大きい preview と90 frame以上のループは budget不足により residency改善が限定的になる。GPU-only capture と memory policy は別途寿命・編集無効化を確認する必要がある。
- 次に確認: 実際の resident hit / upload 回避、例外、同条件の late/drop、RGBA parity。短いloopの成功を長尺生成の成功と扱わない。
