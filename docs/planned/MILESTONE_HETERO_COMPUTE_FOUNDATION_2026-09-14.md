# XPU ヘテロコンピューティング基盤（CPU複数＋dGPU複数＋iGPU）詳細計画 — 2026-09-14

**最終更新:** 2026-10-10
**ステータス:** Partial — Render QueueのCPU＋GPU mixed frame dispatch、D3D12 adapter worker、backend別plan／diagnostics、shared TBB arenaは実装済み。Windows SDK探索を修正しD3D12/Vulkan双方のビルドを有効化。OSのDXGI列挙と直接D3D12CreateDeviceはRTX 4070 Tiで成功する一方、Artifact経由のD3D12 primary起動は「No suitable hardware adapter found」で失敗し、Vulkanへfallbackする。Vulkan primary＋D3D12 workerのテストは同一物理GPUをbackend間で重複dispatchしないためskip。複数物理GPU/iGPU、cross-backend parity／性能受入は未完了。
**正式名称:** XPU（旧称 hetero。コード上の正規型は `XpuNodeDesc`、`HeteroNodeDesc` は別名として残す。正規環境変数は `ARTIFACT_XPU`、`ARTIFACT_HETERO` と `ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED` は後方互換の別名）
**作成日:** 2026-09-14
**対象:** ファイル書き出し（レンダーキュー）経路の CPU＋GPU 併用と、将来の複雑なヘテロ実行の土台
**範囲:** `Artifact/src/Render/` と `ArtifactCore` の共有CPU並列実行契約。子リポ変更はユーザーの明示要求がある範囲に限る。
**関連:** `docs/planned/PROPOSAL_RENDER_EXPORT_EFFICIENCY_2026-07-28.md`、`docs/planned/MILESTONE_MFR_MULTI_FRAME_RENDER_2026-08-01.md`、`docs/analysis/PERFORMANCE_ASYNC_GPU_OPTIMIZATION_2026-08-06.md`、`docs/analysis/REPORT_TBB_WORK_STEALING_CANDIDATES_2026-07-28.md`、`Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`（M-IR-6）

---

## 1. 目的 / 非目的

### 目的

- ファイル書き出し時に CPU（複数コア／複数グループ）、dGPU（複数枚）、iGPU を同時に使い、ハード利用率を上げる。
- 場当たりの分岐増殖ではなく、デバイス登録→能力評価→分配→順序復元→書出という単一のヘテロ実行基盤に寄せる。
- 既存資産（MFR dispatcher、Farm、Scheduler、非同期 readback、`AsyncImageWriterManager`、`Parallel::For`）の再利用を最優先し、新規機構を最小化する。

### 非目的

- タイル単位のフレーム内 CPU+GPU 分割の本格導入（本計画では設計予約に留める。§7）。
- プレビュー描画パスの作り替え、タイムライン UI の移行。
- 新規シグナル／スロット、新規 QImage 経路、新規 QtCSS の導入（AGENTS 禁止事項のため採用しない）。
- ビルド・ベンチの実行（ユーザー指示待ち）。

---

## 2. 現状（調査済み・根拠付き）

| 領域 | 現状 | 根拠 |
|---|---|---|
| dGPU+iGPU final | D3D12 の Discrete / Integrated adapter を frame-parallel final worker 候補にする。各 worker が `ArtifactIRenderer`＋immediate context＋cache set＋composition snapshot を専有し、生成は mutex 直列。iGPU は既定で含め、環境変数で除外できる | `Artifact/src/Render/ArtifactRenderQueueService.cppm:3231-3289`、`6389-6428`、`Artifact/src/Render/ArtifactIRenderer.cppm:1852` |
| アダプタ列挙 | backend/type/VRAM/RT/スコアリング＋ポリシー選択あり。final worker は D3D12 の Discrete と、`ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED` が無効でない Integrated を採用する | `Artifact/src/Render/DiligentDeviceManager.cppm:381-516`（score/policy）、`518-`（candidates）、`Artifact/src/Render/ArtifactRenderQueueService.cppm:3231-3265` |
| CPU 並列 | MFR dispatcher 基盤あり。実ループは GPU 時 1 worker 化（`useMfr=!useGpuBackend`）、`maxInFlightFrames_=4` 固定、outputBuffer 有界（件数＋メモリ） | `ArtifactRenderQueueService.cppm:2694`、`6280-6282`、`6368-6376`、`MILESTONE_MFR_MULTI_FRAME_RENDER_2026-08-01.md` |
| 直列化残 | `renderSingleFrame` 全体覆いの mutex、共有 mutable キャッシュ（S1–S15整理済み）が MFR 前提条件 | `ArtifactRenderQueueService.cppm:5813-5832`、MFR 文書 §Phase 0（S1–S15） |
| consumer | 完全シリアル（RGBA変換→縮小→encode/書込）。非同期書込・非同期 readback は実装済み未接続 | PROPOSAL §現状、`ArtifactIRenderer.cppm:3893`（async）、`ArtifactCore/src/IO/Image/AsyncImageWriterManager.cppm` |
| encode | `thread_count=1` 固定残存、preset `slow` ハードコード問題、sws 単スレッド疑い | `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm:146,228`、`ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm:175`、PROPOSAL §1/§4 |
| 計算カーネル | 空間系は `Parallel::For` 行並列が普及。一部（粒子・流体・Mpm P2G 等）は未並列または順序契約あり | TBB 候補文書 §1–§2 |
| プール二重化 | 共有バックグラウンド QThreadPool は 4 固定、TBB は HW 並列。用途分離は成立、集中制御なし | `ArtifactCore/src/Thread/ThreadHelper.cppm:26-36`、TBB 文書 §3 |
| 未使用 dispatcher | `RenderFarmMaster`（capabilities/pool/checkpoint）、`ArtifactRenderScheduler`、`RendererQueueManager`、`MFRDispatcher` が分散・一部未接続 | `ArtifactCore/src/Render/RenderFarmMaster.cppm`、`Artifact/src/Render/ArtifactRenderScheduler.cppm`、`ArtifactCore/src/Render/RendererQueueManager.cppm` |

結論: デバイス列挙・worker 分離・順序復元 buffer・非同期 readback・能力付き farm という断片は揃っている。欠けているのは「それらを束ねる単一のヘテロ dispatcher と能力コストモデル」である。

---

## 3. 要件

### 機能要件

- FR-1: CPU worker 群＋GPU worker 群（dGPU×N＋iGPU×0/1）を同一ジョブで混在実行できる。
- FR-2: フレーム粒度で work-stealing し、遅いデバイスに引っ張られない（順序復元は outputBuffer 側で担保）。
- FR-3: デバイス別 in-flight 上限（件数＋メモリ）で VRAM/RAM を食い潰さない。
- FR-4: 失敗時は当該フレームのみ再試行／代替デバイスへフォールバックし、ジョブ全体を止めない（設定依存）。
- FR-5: preview 縮小・encode・連番書込の consumer を pipeline 化し、render と overlap する。
- FR-6: iGPU の役割を選択できる（`off`／`assist`（copy/encode/軽量compute のみ）／`full`）。

### 非機能要件

- NFR-1: preview/export parity を壊さない。逐次結果とピクセル一致（許容差は reduce 系のみ定義）。
- NFR-2: 決定論を壊さない（粒子 spawn 順、P2G 等の加算順、bake 順序）。
- NFR-3: ホットパスで重い確保をしない（AGENTS 割当規則）。作業領域・pool 再利用。
- NFR-4: QImage 新規採用なし。交換形式は `ImageF32x4_RGBA` 中心＋明示変換。
- NFR-5: D3D12/Diligent 低レベルは最小接触。device 生成直列・context 専有を維持。
- NFR-6: 既存の公開挙動（出力仕様・画質 preset）を機械的に変えない。preset 露出は別設計。

### 全体受入（抜粋）

- 逐次 vs ヘテロで全フレーム一致（hash 比較）。
- 2 dGPU 時に単一 dGPU より wall-time 短縮（倍率は環境依存のため「短縮」で受入、数値目標は bench で確定）。
- CPU のみ時も従来より低下なし。iGPU `assist` で consumer 律速が緩和される。
- 長時間ジョブで VRAM/RAM 上限超過・ deadlock なし。cancel 即時反映。

---

## 4. 設計原則（AGENTS 準拠）

- `.ixx` は最小 import・前方宣言優先。実装依存は `.cppm` 側に閉じる。自己 import・`export import` 乱用なし。
- PImpl は `Impl*` 明示所有、`delete` は `.cppm` 側。`shared_ptr/unique_ptr<Impl>` 新規なし。
- Qt 型は使用側で直接 include。`module X;` 以降に `#include` を置かない。
- 新規 signal/slot なし。進捗は既存 `QueuedConnection`・watcher・callback  poll に寄せる。
- `Parallel::For`／TBB arena は単一箇所で制御（各所で arena 乱立なし、`global_control` は初期化一点）。
- ビルド・ベンチは指示待ち。Core 変更は承認待ち。

---

## 5. アーキテクチャ

### 5.1 全体像

```text
RenderQueue job
  → HeteroPlan (device set + weights + limits)
  → FrameScheduler (work-stealing, per-device in-flight)
  → Executors
  │    CpuExecutor ×N (MFR worker + snapshot clone pool)
  │    GpuExecutor dGPU/iGPU×N (dedicated IRenderer + cache set)
  │    GpuAssist iGPU×0/1 (将来の copy/encode/light compute)
  → OrderedJoin (existing outputBuffer extension)
  → ConsumerPipeline (convert → preview → encode/write)
  → Ledger/Progress/Diagnostics
```

新規クラスを増やさず、既存のどれに寄せるかを固定する:

| 役割 | 既存の寄せ先 | 備考 |
|---|---|---|
| 能力・pool・checkpoint | `RenderFarmMaster` | `requiredCapabilities{pool,gpu,vram}` を流用、新規 farm は作らない |
| 優先度・cancel・重複排除 | `ArtifactRenderScheduler` | raw `std::thread` 群の置換先候補 |
| frame 並列数・memory・retry | `MFRDispatcher` | CPU 側の数理（memory 連動 concurrency）を流用 |
| device 列挙・score | `DiligentDeviceManager` | `availableAdapters/autoScore/policy` を正とする |
| 書込並列 | `AsyncImageWriterManager` | 連番側の接続先 |
| 計算並列 | `Parallel::For`／TBB | カーネル側は現状維持＋不足分のみ追加 |

### 5.2 DeviceRegistry（登録のみ・判断は Scheduler）

```cpp
struct GpuDeviceDesc {
  int adapterId = -1;          // Diligent adapter index
  QString backend;             // d3d12 / vulkan ...
  QString type;                // Discrete / Integrated / Software
  quint64 localBytes = 0;
  quint64 unifiedBytes = 0;
  bool rayTracing = false;
  int autoScore = 0;
};

struct HeteroNodeDesc {
  enum class Kind { CpuGroup, GpuFull, GpuAssist } kind;
  QString id;                  // "cpu:0", "gpu:d3d12:1", "igpu:d3d12:0"
  int maxInFlight = 1;
  size_t memoryBudgetBytes = 0;
  GpuDeviceDesc gpu;           // GPU 系のみ有効
};
```

- 列挙は `DiligentDeviceManager::availableAdapters()` 単一入口。
- フィルタは環境変数拡張で表現し、コード分岐を増やさない:
  - `ARTIFACT_GPU_POLICY`: 既存 `auto/high-performance/power-saving/specific`
- `ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED`: `0` / `false` / `off` / `no` で iGPU final worker を除外。未設定時は D3D12 Integrated を含める。
- 2026-10-10 実装済み: `ARTIFACT_XPU`（legacy `ARTIFACT_HETERO`）の `mixed=on` が CPU/GPU lane を同一 job-local atomic frame-pull dispatch で実行する。現在の scheduler は lane ごとの device ownership を維持するため、汎用 `ArtifactRenderScheduler` / Farm には載せ替えていない。CPU render worker と sequence writer、既存poolを合算するプロセス全体の CPU 予算制御も未実装。
- 2026-09-23: 正式名称を XPU に変更。正規 spec 変数は `ARTIFACT_XPU`（例: `igpu=assist` で将来の assist 予約、`igpu=off` で除外、`igpu=full` で明示参加）。`ARTIFACT_HETERO` は後方互換の別名として読み替える。iGPU 包含の真偽判定は `ARTIFACT_XPU_INCLUDE_INTEGRATED` を正規とし、`ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED` は別名として残す。既定動作は現行実装（D3D12 Integrated を独立 full worker として含める）と同一に保つ。`assist` は P4 まで実ディスパッチに接続しない予約値。
- `Software` adapter は既定除外。

### 5.3 Scheduler（frame 粒度・順序不問・join で復元）

- `nextFrameCounter`（atomic）＋ device 別 in-flight セマフォ。
- 取得順: 空きのある node が `fetch_add` で次 frame を奪う。遅延 device の取残しは他 node が自然に吸収。
- 出力は `(frame, output)` を bounded buffer へ。順序制約は consumer のみ:
  - 動画 encode: `serial_in_order` で `addFrame`。
  - 連番: 書込自体も parallel 可（順序不問）。
- CPU/GPU の成果物は同一 `FrameRenderOutput` 型に正規化し、後段を分岐させない。

### 5.4 Executor 構成

- `CpuExecutor`: worker ごとに `cloneCompositionSnapshot` を pool 化（JSON clone の再利用）。`compositionFrameStateMutex_` 撤去は MFR Phase 0（S8/S10 保護＋複製一致テスト）完了が条件。拙速に外さない。
- `GpuFull (dGPU/iGPU)`: 現 `GpuFinalWorker` を踏襲（renderer＋textureCache＋surfaceCache＋mattePool を worker 専有）。D3D12 iGPU も独立した frame-parallel final worker として参加できる。device 生成直列維持、device 境界を越える resource 共有なし。
- `GpuAssist (iGPU)`: 将来は readback 後の convert/scale、scope 集計、proxy/encode 補助、軽量 compute の offload 先にもできる。full worker の実機 throughput と共有メモリ圧迫は未検証のため、scheduler の重み付けや assist への自動降格はまだ行わない。

### 5.5 ConsumerPipeline（PROPOSAL §3 の具体化）

```text
serial_in_order: buffer から次 frame 取得
parallel: RGBA変換・preview縮小・EXR圧縮・YUV前処理
serial_in_order: encoder.addFrame / 進捗 / ledger
parallel(連番のみ): ファイル書込（AsyncImageWriterManager）
```

- 既存 worker/buffer と二重化しない。置換か「変換のみ先行 task_group」かは P2 で確定。
- preview 縮小は時間間引き（PROPOSAL §2）。出力に影響させない。

### 5.6 メモリ・転送モデル

- 上限は二重管理: 件数（in-flight）＋バイト（outputBufferMemory 拡張）。
- GPU 側 staging/readback は既存 ring＋async を正とし、同期 readback を consumer から排除（AO 対応）。
- CPU→GPU upload と GPU→CPU readback は明示関数のみ。暗黙変換・QImage 経由の往復を増やさない。
- AOV/deep は初期は対象外。AD（8回 readback）は別途 1-fence 集約として扱い、本基盤の汎用 path と混ぜない。

---

## 6. フェーズ計画

### P0: 前提分離（MFR Phase 0 の完遂・再発明なし）— 2026-09-23 静的確認（DoD 未達）

- 作業: MFR 文書 S8（AssetManager 保護確認）、S10（Registry mutex）、S9（queue 容量）、S15（atomic）、複製一致テスト。
- 静的確認（ビルド・実機なし）: S8 `ArtifactCore/src/Asset/AssetManager.cppm` は `QMutex`＋`QMutexLocker` で全公開操作を保護済み。S10 `Artifact/include/Render/ArtifactRenderContext.ixx:472` は `mutable std::mutex mutex_` で `registerSnapshot`/`contains`/`snapshot`/`clear` を保護済み（従来の無保護記述は現行コードでは解消）。S9 `Artifact/src/IO/AsyncAssetReadScheduler.cppm:838` は `static AsyncAssetReadScheduler(3)` で同時実行 3、キュー容量は `setMaxQueuedJobs`/`setQueuedByteBudget` で可変だが MFR concurrency と合算した飽和レビューは未実施。S15 `Artifact/src/Layer/ArtifactSolidImageLayer.cppm:653` は `static int drawLogSamples` のまま非 atomic（S15 低リスク、ログ欠落のみ）。`compositionFrameStateMutex_` は XPU P2 でも温存し、S1–S7 の複製分離が前提のため撤去しない。
- DoD: `clone→renderSingleFrame` が逐次と一致。全レイヤー種別。CompositionContext/3D カメラの toJson 欠落があれば先に `COMPOSITION_API_HARDENING` P1（未受入・実機テスト要）。
- 対象: MFR 文書の Phase 0 に準拠。新規設計なし。

### P1: DeviceRegistry＋ポリシー（Partial）

- 2026-10-10 実装: Startup Flags phase 4 の XPU key 群を `RenderQueue/Xpu*` へ接続。Render Queue と encoder は JSON → 対応env → 既定値で値を一度解決し、以降 accessor の cached value を使用する。`AppMain` のGUI起動順を調整し、`QApplication` 後・最初の `ArtifactAppSettings` 取得前にJSONを読み込む。`Artifact/ArtifactStartup.template.json` は任意key一覧のみを記載して既定動作を維持。競合警告・JSON優先順位・設定型のruntime確認は未実施。

- 作業:
  - `availableAdapters()` をヘテロ登録の正規入口に固定。
  - `HeteroNodeDesc` 構築＋`ARTIFACT_HETERO` パース（既定は現行動作と同一）。
  - `Software` 除外、iGPU/dGPU とも full-frame worker を既定とする。`assist` は consumer offload 実装後に opt-in で有効化する。
  - 起動ログに選択 node 一覧（adapter/score/budget）を出す。診断ログの遅延評価を守る。
- 対象: `DiligentDeviceManager.cppm`、`ArtifactRenderQueueService.cppm`（plan 構築部）、`ArtifactIRenderer.cppm`（adapter 初期化は現状維持）。
- DoD: 単GPU／複数dGPU／iGPU混在の各環境で plan のみ正しく列挙される（render 挙動不変）。
- 非DoD: 実分配の変更なし。
- 2026-09-21 実装済み: `ArtifactRenderQueueService` の既存 final worker 分配で、D3D12 Integrated adapter を Discrete adapter と同じ独立 worker として候補化した。`ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED=0` で除外できる。`HeteroNodeDesc`、統一 plan、budget 判定、実機検証は未実装。
- 2026-09-23 実装済み（P1残分）: 正規型 `XpuNodeDesc`（`HeteroNodeDesc` は別名）を `ArtifactRenderQueueService` に追加。`buildXpuPlan()` が CPU group＋非 Software adapter 列挙（Discrete→`GpuFull`、Integrated→`igpu=` 指定、既定 `full`）＋adapter 別 memory budget（local→unified フォールバック）を構築する。実機での plan 列挙確認は未実施。
- 2026-10-10 実装: `ARTIFACT_XPU=mixed=on` のCPU/GPU混在 dispatchを、複数D3D12 GPUが成立した場合だけでなく、単一の有効GPU（Diligent Vulkanを含む）でも利用可能にした。GPU workerを優先して確保し、残りのworker枠をCPU software workerへ割り当てる。`ARTIFACT_XPU_MAX_IN_FLIGHT`未指定時はCPU論理数−2をCPU/GPU共通のworker上限とし、GPU制御スレッドを加算してCPU枠を超過しない。さらに16 bytes/pixelの作業領域見積りと512 MiBのin-flight予算で最大64へ制限する。明示値は上限として尊重する。CPU workerは独立Composition snapshotを使用し、既存のbounded output buffer・frame番号復元を維持する。simulation／Tiled／HTML／multi-channel／deepは引き続き対象外。XPU planはprimary rendererを含む候補を保持する。Vulkan primary時はVulkan factoryを再列挙せず、D3D12候補を追加してVulkan primary lane＋独立D3D12 GPU lanes＋CPU lanesを混在 dispatchする。Vulkan physical deviceの複数同時使用はDiligent forkがglobal関数ポインタを共有し複数instance/logical deviceを禁止するため未対応。job開始時に一度作るplanをD3D12 full-worker選定・ログ・summaryで共用し、選択中adapterがXPUのfull nodeに含まれない場合はplan上のGPU adapter workerへ切り替え、適格なGPU nodeがなければmixed modeではCPU workerへfallbackする。`igpu=assist`をfull-frame workerへ誤投入しない。CPU/GPUの両workerから完了frame数を記録し、GPU workerごとのadapter ID・処理frame数をjob summaryとbench JSONに出力する。これを検査するRender Queue画像統合テストを追加したが未実行。
- 2026-10-10 実装: XPU連番出力のwriter future上限をレンダーworker数に比例させず1〜2件に固定し、pending上限・backpressure・最終drainを同じ値で運用する。`AsyncImageWriterManager` は hardware_concurrency 数のthread poolと上限なしenqueueを持ち、ArtifactCoreを変更せずRenderQueueから投入数を制限できないため、`asyncseq=manager` 指定時はbounded future経路へfallbackし理由を警告する。ビルド・テスト未実施。

### P2: Hetero dispatcher（CPU+GPU 混在の最小作動）

- 作業:
  - job-scoped `XpuPlan` と共通atomic frame claimを、全CPU/GPU lane選定・bounded outputBuffer・順序復元に一貫して使う。
  - device別 in-flight（CPUは共通host worker budgetの残枠、GPUはadapterごと専有worker＋VRAM budget）を適用する。
  - `useMfr`／`useMultiGpu` の既定動作は維持しつつ、XPU mixed opt-in時のdispatch判断と実行node記録をplanへ集約する。
  - `cloneCompositionSnapshot` pool 化（確保回数削減）。
- 対象: `Artifact/src/Render/ArtifactRenderQueueService.cppm` と、将来のaffinity-aware dispatcher API。
- Scheduler調査: 現行 `ArtifactRenderScheduler` は汎用RenderTask/TaskSystemでdevice affinityを保持せず、`MFRDispatcher::FrameTask` もworker identityをcallbackへ渡さない。GPU immediate context・cache・snapshot所有を守るため、この2 APIへraw frame callbackを無理に載せ替えない。現行のjob-local atomic採番はlane間work-stealingとして維持し、affinity-aware dispatcherへの置換は別の設計項目として扱う。
- DoD: 混在 opt-in 時に全 frame 成功＋順序復元＋逐次一致。cancel/failure/ledger が従来通り。
- 注意: `compositionFrameStateMutex_` は P0 未了なら残す。外すのは P0 受入後。
- dispatchの共通atomic frame claimは現在のGPU専有lane affinityを保つためローカルexecutorとして残す。汎用Scheduler/Farmへはworker identityとdevice ownershipを表せるAPIなしに移行しない。
- 2026-10-10 実装: XPU mixed対象ジョブではGPU lane失敗時にlane専用isolated Composition snapshotでCPU software pathへ、CPU lane失敗時はworker snapshotを共有primary GPU pathへ再投入する（primary adapterがplan上のGpuFull nodeの場合のみ）。GPU→CPU fallback用snapshotはD3D12 GPU workerごと、およびprimary GPU lane用にjob開始時に用意し、通常フレームに追加threadやfallback用allocationを発生させない。CPU→GPU fallbackは既存`compositionFrameStateMutex_`でshared device/contextを直列化する。cancel要求中はfallbackしない。fallback成功数をCPU/GPU frame数へ反映し、`xpuCpuFallbackFrames`/`xpuGpuFallbackFrames`をsummary/benchへ記録。両laneが失敗した場合は両方の理由をconsumerへ渡す。出力parity・失敗注入を含むruntime検証は未実施。
- 2026-10-10 補完: dedicated GPU workerの描画とそのCPU fallbackの両方が失敗した場合、plan上で異なるbackendまたはadapter IDを持つselected primary GPUがあれば、同じframeを共有primary rendererへ一度fallbackする。成功時は専用workerではなくprimary adapterのframe/time countersへ計上し、全fallback失敗時はworker・CPU・primary GPUの失敗理由を保つ。same-adapter再試行は別device扱いしない。故障注入runtime確認は未実施。
- 2026-10-10 実装: jobのGPU adapter初期化後にplanの各GPU node `maxInFlight`を実際のworker/primary adapter参加状態へ更新し、CPU nodeはclone生成完了後の実CPU worker数に更新する。初期化失敗adapterをCPU枠の算定から除外し、assist nodeはGPU offloadが未実装のため`maxInFlight=0`とする。`XPU active plan`ログとjob summaryは候補数ではなく実際のlane上限を表す。runtime検証未実施。
- 2026-10-10 補完: 以前のadapter別frame数は専用D3D12 workerだけを記録し、single-GPU/Vulkanで使用するprimary rendererの実績が欠けていた。job summaryとbench JSONに`primaryGpuAdapterId`/`primaryGpuFrames`を追加し、CPU laneからprimary GPUへのfallbackもこの数へ含める。単体・実機での計数整合は未検証。
- 2026-10-10 補完: D3D12 GPU worker の composition snapshot 作成または renderer 初期化に失敗した場合、その adapter だけを skip し、準備できた他 adapter worker を保持する。CPU fallback snapshot が作れない場合も GPU lane は維持する。利用可能 worker 数が最低条件を満たさないときは従来どおり専用 worker 群を破棄して primary GPU 経路へ戻す。adapter 故障注入による runtime 検証は未実施。
- 2026-10-10 補完: XPU mixed の CPU composition snapshot 作成が部分的に失敗しても、作成済み CPU lane を破棄せず、その lane 数へ縮退して混在 dispatch を続ける。全 CPU snapshot が作成できない場合は既存 GPU 経路を維持する。clone failure 注入時の runtime 検証は未実施。
- 2026-10-10 補完: mixed mode で追加 D3D12 GPU worker を作る際、memory budget が既知で16 bytes/pixelの1フレーム作業見積りを下回るadapterはworker候補から除外する。primary renderer の既存laneと、capacityが不明なadapterは維持する。低VRAM device でのruntime受入は未実施。
- 2026-10-10 補完: shared worker 上限によりGPU候補の一部しかworker化できない場合、autoScore順の先頭ではなくplan上でactive rendererとしてselectedになったadapterを先に選ぶ。設定で選んだprimary GPUがworker枠不足で外れるケースを防ぐ。複数adapter環境のruntime確認は未実施。
- 2026-10-10 補完: worker が記録した失敗理由を consumer の失敗フレーム検出時に job の失敗理由へ引き継ぐ。GPU/CPU fallback がともに失敗した場合も generic な null-frame message で上書きしない。job ledger/UI での表示は runtime 未確認。
- 2026-10-10 実装: Vulkan primary rendererを維持したまま、DiligentのVulkan global instance制約に触れないようVulkan adapter再列挙を避け、D3D12候補を独立GPU workerとして追加。XPU混在時はVulkan primary＋D3D12 worker群＋CPU worker群を同じframe budget内で dispatchし、worker index、primary GPU fallback、plan参加数の計上を分離した。Vulkan+D3D12同時レンダーの実機受入は未実施。
- 2026-10-10 補完: `ARTIFACT_XPU=mixed=on` 要求時に、CPU backend選択、GPU-only reveal、component simulation、Tiled、HTML、multi-channel、deep exportのどの条件でmixed dispatchが無効になったかjob開始ログへ列挙する。出力形式・従来fallbackは変更しない。実際の全制約組合せは未検証。
- 2026-10-10 補完: mixed jobではlogical thread数から予約writer slot（6以上なら1、12以上なら2）と既存の2 thread余白を差し引き、その範囲内でCPU/GPU frame laneを制限する。6 logical threads未満ではasync sequence writerを止め同期書き込みへ戻す。job summaryへhardware thread数、host lane上限、writer予約数、pending上限を出力し、XPU mixed画像統合テストでworker枠とwriter枠の包含関係を検査する。UI/TBB/他poolを含むprocess-wide予算ではなく、runtime受入未実施。
- 2026-10-10 修正: `xpuMixedEligibleForJob` に実際の `useGpuBackend` を含め、CPU backendを選択したjobをmixed active planやmixed予算予約へ誤分類しない。CPU backend選択理由は既存のunavailable diagnosticsに残し、Render Queue画像統合テストでCPU baselineにmixed lane／budgetが起動しないことを検査する。テスト未実行。
- 2026-10-10 補完: XPU mixedのhost CPU数はmachine-wideの`std::thread::hardware_concurrency()`ではなく`QThread::idealThreadCount()`で取得する。OSが公開するprocess-available logical CPU countに合わせ、process affinityなどを超えるworker/encoder予約を抑える。CPU制限環境での検証未実施。
- 2026-10-10 補完: mixed video jobはnative/software/hardware FFmpeg encoderとexternal FFmpeg pipe encoderをmixed CPU budget内に制限する。encoder thread数の未設定または0は1、正数はhardware thread余白内へclampし、その数をframe lane budgetから控除する。low-coreでは2 threadのservice reserveを縮小して最低1 frame laneを確保する。runtime検証未実施。
- 異なるbackendで同じvendor/device IDの物理GPUを二重計上しない。複数の同一型GPUのcross-API identityは現在のDiligent `GraphicsAdapterInfo` がLUIDを公開しないため完全には識別できず、その組合せのruntime受入は未実施。

### P3: Consumer pipeline＋encode 効率（利用率の主戦場）— 2026-09-23 部分実装（DoD 未達）

- 2026-10-10 訂正: 以前「動画向け3-stage pipeline」と記載した `std::async` 変換は、同一フレームの直後に `get()` しており encode と重ならないため、実際の並列 pipeline ではなかった。該当処理を撤去し、`pipeline=on` は未対応警告を出して同期経路を使う。bounded lookahead と encode との重複実行を実装・検証するまで bench 手順から除外する。

- 済（出力不変・既定不変）: (1) `FFmpegEncoderSettings.threadCount` を追加し `codecCtx_->thread_count` へ接続。未設定時は encoder default（=現行動作）。`ARTIFACT_XPU_ENCODER_THREADS` で上書き可（0〜64、0 は default）、`buildNativeVideoSettings`/`buildGpuVideoSettings` 共通。XPU mixed動画ではCPU予算を共有するため、未設定／0を1 thread、正数指定を利用可能host thread数以下へclampし、frame worker上限からencoder分を差し引く。pipe FFmpegにも同じthread上限を渡す。(2) 進捗 preview の時間間引き（既定 250ms、最終フレームは常時 publish）。`ARTIFACT_XPU_PREVIEW_MIN_INTERVAL_MS` で上書き可（0 で従来どおり、出力に影響しない）。(3a) 連番書込のbounded async（既定on、`ARTIFACT_XPU_ASYNC_SEQUENCE=off` または `ARTIFACT_XPU=asyncseq=off` で停止）。単チャンネル image sequence のみを並列書込し、pending上限は1〜2件、最終drainで失敗をledgerへ反映する。`asyncseq=manager` 指定時も、上限なしenqueueを避けるためbounded future経路を使用する。video/HTML/SVG/multi-channel/deepは同期のまま。(3b) 動画向けoverlapped conversion/encode pipelineは未実装（後段の訂正参照）。(5) `maxInFlightFrames_` を `ARTIFACT_XPU_MAX_IN_FLIGHT` で可変化（非XPUは既定4=現行動作、XPU mixedは未指定時にhardware/memoryから算出）。preset 露出は bench 用に `ARTIFACT_XPU_PRESET` / `ARTIFACT_XPU=preset=<value>` で opt-in 上書き可（未設定は `slow`/`p4` のまま、出力が変わるため既定では使わない）。
- 残（P3 DoD 向け）: sws/YUV 変換の本格並列化（`thread_count` と `preset` の有効範囲 bench、sws 並列の要否判定）＋ 4K wall-time 受入。preset の既定変更は行わず、bench 時のみ opt-in で比較する方針を維持。
- 対象: `ArtifactCore/src/Image/FFmpegEncoder.cppm`、`ArtifactCore/include/Video/FFMpegEncoder.ixx`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（(1)(5)＋preset override）、`Artifact/src/Render/ArtifactRenderQueueService.cppm`（(2)(3a)(3b)(5)、bounded sequence writer futures）。
- Bench 手順（P3 有効範囲）: 4K png sequence（`asyncseq=on` vs off）と 4K h264 mp4（`threads=0/4/8/16` × `preset=slow/medium/fast`/`p4/p7`、pipeline on/off）で同一入力の wallMs と出力同一（hash）を比較。sws 律速は `threads` と `pipeline` の組合せで分離し、consumer 律速は `asyncseq` で分離する。
- DoD: 同一出力で wall-time 短縮、画質・仕様不変、4K 時の変換律速が緩和（未受入・実機 bench 要）。

### P4: iGPU assist 本格化＋適応調整 — 2026-09-23 部分実装（DoD 未達）

- 2026-10-10 訂正: 以前の `igpu=assist` preview 縮小はCPUで `std::async` 起動後すぐ待機する処理で、iGPU offloadではなかったため撤去した。`GpuAssist` はplan表示のみ（実行上限0）を維持し、assist要求時はjobごとに非対応警告を出す。iGPUの実処理は現状 `full` worker lane のみ。

- 済（出力不変・opt-in/plan 予約）: `buildXpuPlan` で `igpu=assist` 時に `GpuAssist` ノードを列挙し、`xpuPlanDebugState` と job summary に出す。GPU assistへのoffload処理は未接続（後段の訂正参照）。device 生成は従来どおり直列・専有を維持。throughput は `xpuCpuTimeNs`/`xpuGpuTimeNs` をframeごとにnanosecond計測し、job summary/benchに累積ms・平均ms・`weightGpu`/`weightCpu`（`gpuAvg/cpuAvg`）を出す（診断用）。dispatchはcompletion-driven pullのため、各laneがframe完了後に次をclaimし、実行中のthroughputに応じて速いlaneが自然に多く処理する。固定weightは適用しない。
- 2026-10-10 実装: `ARTIFACT_XPU_MAX_HW_ENCODERS`（既定2、1〜16）を共有atomic slot上限として適用。Native FFmpeg hardware、ffmpeg.exe hardware pipe、Vulkan pipeを同一上限で管理し、slotはencoder open/probe前に獲得、close/finalize後に解放する。上限到達時は新規hardware openを即時拒否し、既存backend選択のsoftware fallbackへ進む。failure/close/destructorのlease releaseは静的確認のみ。`ArtifactRenderQueueService::startAllJobs()` はjobを逐次処理するため通常の単一queueでは上限競合が起きず、実際のbackoff効果は独立backend利用または将来のjob並列化時に限られる。
- 2026-10-10 計測補完: CPU/GPU aggregate と各 GPU lane の経過時間をnanosecondで累積し、summary/benchで小数msへ変換する。GPU失敗後にCPU fallbackが成功したframeは、失敗したGPU試行時間をCPU throughputへ混ぜず、CPU fallback区間だけを加算する。runtimeでの計測値照合は未実施。
- 残: convert/scale/scope/proxy/encode 補助への本格接続、completion-driven pullが異種adapter間の速度差を十分吸収するか実測benchで確認（starvation/偏りが見つかった場合だけstatic weightを検討）、encoder上限の同時job runtime受入、PCIe 競合の backoff（失敗時は CPU/GPU-full へ fallback）。
- 対象: `Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuIgpuRole`/`buildXpuPlan`/`shouldIncludeIntegratedGpuWorkers`、preview assist 分岐、`xpuFrameTimer`/`xpuCpuMs`/`xpuGpuMs`）、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（`ARTIFACT_XPU_MAX_HW_ENCODERS` ログ）。
- DoD: iGPU on/off で破綻なし。律速が render→consumer へ移動しない（未受入・実機 bench 要）。

### P5: 観測・受入・文書化 — 2026-09-23 部分実装（DoD 未達）

- 済（cold path のみ、出力不変）: frame 別 `renderBackend` を `gpu-multi`/`xpu-cpu`/`gpu`/`cpu` に拡張（P2）、XPU plan を起動ログと job summary の両方に出す（P1/P5）、job 終了時に wallMs＋gpuFrames/xpuCpuFrames＋mixed/asyncSeq＋parity＋pipeline を `XPU job summary` として出す。parity は `ARTIFACT_XPU_PARITY_HASH=on` または `ARTIFACT_XPU=parity=on` でのみ FNV-sample hash を frame 毎に累積し、逐次 vs ヘテロの同一性検証に使える（既定 off）。bench JSON は `ARTIFACT_XPU_BENCH=on` または `ARTIFACT_XPU=bench=on` でのみ `temp/ArtifactStudio/xpu-bench/xpu_job*.json` に plan/wallMs/内訳/parity を残す（既定 off、出力不変）。`RenderPerformanceMonitor`/`FrameCache` の既存集計は温存。
- 2026-10-10 補完: XPUを使わないCPU専用の逐次／MFR renderも成功したCPU frame数とelapsed timeに計上する。これによりCPU baselineとXPU mixedのjob summaryが同じ定義で比較できる。bench runtime値での確認は未実施。
- 再現手順（固定条件・受入用）: 4K comp（3840x2160/30fps/100frames/png sequence または h264 mp4）で `ARTIFACT_XPU` 未設定（逐次）vs `ARTIFACT_XPU=mixed=on`/`asyncseq=on`/`parity=on`/`bench=on` の組合せを同一入力で実行し、`XPU job summary` の wallMs と bench JSON の hash 一致で parity と consumer 律速の内訳を比較する。`threads` は `ARTIFACT_XPU_ENCODER_THREADS`、`maxInFlight` は `ARTIFACT_XPU_MAX_IN_FLIGHT` で固定し、preset は変えない。動画pipelineはbounded lookahead実装後に追加する。
- 残: 上記手順での実機 bench と数値目標の確定（M-IR-6 受入。harness 自体は上記 JSON で最小は揃う）。
- 対象: `Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuWallTimer`/`xpuCpuFrames`/`xpuGpuFrames`/`xpuParityCombined`/`xpuBenchEnabled`/`writeXpuBenchRecord`/`xpuPipelineEnabled`、`XPU job summary`）。
- DoD: M-IR-6 の「再現可能な bench」未達を本基盤で解消。数値目標はここで確定（未受入・実機 bench 要）。

### §7 予約: タイル内分散（本計画では実装しない）— 2026-09-23 再確認

- 条件: P0–P5 受入後、かつ halo・決定論・parity の設計レビュー通過後のみ。本計画の P1–P5 が DoD 未達のため着手しない。
- 候補: 大フレームの効果適用タイル分割、CPU タイル＋GPU タイルの合成。`ComputeMode::AUTO` の device 拡張として扱い、dispatcher とは層を分ける。
- 留意: タイル分割は halo（エフェクトの周辺参照）、決定論（加算順・spawn 順）、parity（CPU/GPU 画素差）、メモリ budget、QImage 経路禁止の各制約を同時に満たす必要があり、frame 粒度のヘテロ基盤とは独立した設計レビューが必須。2026-09-23 時点では着手せず、XPU の実機 bench（P5）で frame 粒度の効果を確定してから再検討する。

---

## 7. スレッド・メモリの固定事項

- QThreadPool（I/O・単発）と TBB（計算カーネル）の住み分け維持。`global_control/task_arena` は初期化一点のみ。
- `Parallel::For` の grain 16 固定は行処理用。少数重反復が出たら overload 検討（現時点では追加しない）。
- 共有 pool 上限（現 4）はヘテロ in-flight と合算で飽和させない。P2 で上限設計を一本化する。
- QWidget/QPixmap/signal 発火を worker から行わない。QImage は detach 済み前提で触る（新規採用なし）。

---

## 8. フォールバック／決定論／parity

- 優先順位: `GpuFull → Cpu → GpuAssist` ではなく、frame 毎の空き取得＋失敗時代替実行。
- 失敗分類: device init 失敗→当該 node 除外、frame render 失敗→retry（上限）→他 node で再実行→最終失敗は ledger＋held。
- 決定論: spawn/kill 遅延反映の index 順ソート、reduce 系は許容差付き比較、それ以外は完全一致。
- preview/export parity: `SequenceTimelineRenderer` と preview の同一式を維持。遷移・opacity 等の差分は parity 項目として受入表へ。

---

## 9. リスク

| リスク | 影響 | 緩和 |
|---|---|---|
| JSON clone が worker 増で律速 | 起動・初動遅延 | pool 化＋再利用、P2 で計測 |
| D3D12 context 多重化の不安定化 | クラッシュ・artifact | 生成直列・専有維持、P2 は opt-in、切替時 artifact 回避を P5 で受入 |
| VRAM/帯域競合（特に iGPU） | 逆に遅化 | device 別 budget＋backoff、assist 限定開始 |
| encode 並列の順序・画質変化 | 仕様逸脱 | serial_in_order 維持、preset 機械変更なし |
| Asset/Registry 競合 | 破損・順序不定 | P0 で保護、mutex 範囲最小化 |
| oversubscription | CPU 飢餓 | 上限一本化、arena 乱立禁止 |

---

## 10. 検証計画（実行は指示待ち）

- Parity: 全レイヤー種別×CPU/GPU/混在で hash 一致。AOV/deep は別表。
- Bench: 4K 固定 comp、同一 range、thread/preset 条件明示。`speedupVsSequential` と consumer 律速分離のため render/convert/encode の内訳計測。
- Stress: 上限超過・cancel・失敗注入・ checkpoint 復旧（farm 経路）。
- 受入証跡: ログ＋ledger＋bench 記録を残し、M-IR-6 の runtime 受入に接続。

---

## 11. ユーザー決定事項（承認待ち）

1. `ArtifactCore` 変更（FFmpeg thread/sws、`AsyncImageWriterManager` 接続、Registry 保護）を本計画内で行ってよいか。
2. 解決済み（2026-10-10）: iGPU は `full` frame worker を既定とし、`igpu=off` で除外できる。`igpu=assist` は consumer offload が未実装のため明示要求時も非参加で、P4 完了まで未対応警告を出す。
3. `ARTIFACT_HETERO` の導入可否と既定値（提案: 未設定＝現行動作）。
4. タイル内分散を将来 scope に残すか、完全に除外するか。
5. bench 条件（4K/30fps 目標の固定 comp・range・codec）の確定。

---

## 12. 関連文書

- `docs/planned/PROPOSAL_RENDER_EXPORT_EFFICIENCY_2026-07-28.md`
- `docs/planned/MILESTONE_MFR_MULTI_FRAME_RENDER_2026-08-01.md`
- `docs/analysis/PERFORMANCE_ASYNC_GPU_OPTIMIZATION_2026-08-06.md`（AO/AP/AQ/AC/AM/AN）
- `docs/analysis/REPORT_TBB_WORK_STEALING_CANDIDATES_2026-07-28.md`
- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`（M-IR-6）
- `docs/planned/MILESTONE_NUMA_AWARE_RUNTIME_2026-08-14.md`（CPU 配置と合流要）

---

## 更新履歴

- 2026-09-14: 初版。CPU複数＋dGPU複数＋iGPU のヘテロ基盤として、現状根拠・DeviceRegistry・dispatcher・consumer pipeline・P0–P5 を定義。タイル内分散は予約に留める。
- 2026-10-10: `ARTIFACT_XPU=mixed=on` を単一GPU＋CPUでも利用可能にし、単一planに実行選択とdiagnosticsを統一。未指定worker上限をCPU論理数・GPU node数・画像作業領域予算から自動算出。選択中adapterとfull nodeの不一致時のworker選択、GPU nodeがない場合のCPU fallback、`igpu=assist`のfull worker誤分類を修正。CPU/GPUとGPU adapter別workerの参加数をsummary/benchに記録し、出力画像とworker参加を検査するRender Queue統合テストを追加。実機受入は未実施。
- 2026-10-10: XPU mixed integration test を追加し、静止テキスト compositionの複数フレーム出力、解像度、CPU/GPU lane参加を検査する。環境ごとの60秒制限に対して30個の逐次CPU jobは過大だったため、3フレームの混在 jobへ縮小した。
- 2026-10-10: XPU CPU baseline の単一frame rangeは `[start,end)` なので、endをframe番号と同値にしていたテストを `frame + 1` に修正。初回実行で「Render frame range is empty」を検出した。CPU基準フレームとXPU出力の両方で half-open frame range を確認し、混在テストで実行済み。
- 2026-10-10: 静止テキストcompositionをCPU逐次jobと3フレームXPU mixed jobでレンダーする。CPU/GPUラスタライザ間のRGBA完全一致は実機で成立しなかったため、テストは各出力の可読性・解像度・可視テキストとCPU/GPU両lane参加を検査し、ピクセル同一性は受入条件にしない。
- 2026-10-10: XPU parity integration test の `ARTIFACT_XPU_MAX_IN_FLIGHT` を2に固定し、host core countに依存せずGPU lane 1＋CPU lane 1を対象にする。複数GPU候補がある場合は選択中primary adapterを worker枠へ優先する経路も通す。adapter数別のruntime確認は未実施。
- 2026-10-10: XPU統合テストに2D/3D particle compositionのRender Queue出力ケースを追加。粒子描画の可視性とcross-rasterizer parityは既存のGPU粒子テスト・専用比較ケースで別途扱う。混在dispatch自体の両lane参加は静止テキストの複数フレームケースで検査する。
- 2026-10-10: worker上限を16にした独立XPU統合ケースで8 frameのoutput-buffer上限も超え、順序復元のcapacity backpressureを含めてCPU laneと列挙された対応済みfull dGPU adapterのactive state、各active GPU laneのframe参加を検査する。10フレームで8-frame output-buffer上限を超えて順序復元backpressureを検証。iGPUはこのテストで強制せず、GPU topology別の実機実行は未確認。
- 2026-10-10: Vulkan primaryで起動するXPU統合シナリオをCTest登録。D3D12/VulkanのCMake検出は双方有効。現在のRTX 4070 Ti hostでは同一PCI adapterが両backendに現れるため、重複dispatch防止でD3D12 laneをplanから外し、混在ケースはskipする。OS DXGI経由のRTX adapter列挙とD3D12CreateDeviceは成功したが、Artifactの自動D3D12 primary経路は「No suitable hardware adapter found」でVulkanへfallbackする。Diligent factoryの列挙／初期化経路は未解決。
- 2026-10-10: XPU job summary と bench JSON の GPU worker frame数に backend を追加し、Vulkan primary と D3D12 workerの adapter ID が重複しても識別可能にした。Qt debug出力の空白・引用符を許容するよう統合テストのログ解析も合わせた。
- 2026-10-10: XPU plan node ID を `gpu:<backend>:<adapterId>` / `igpu:<backend>:<adapterId>` 形式にし、異なる backend でadapter番号が重複しても一意になるようにした。all-adapters統合テストでnode IDの重複を検出する。Debugビルド済みで10-frameケースを3回連続成功。
- 2026-10-10: Vulkan primary CTest で plan の Vulkan node だけでなく job summary の `primaryGpuBackend` も `vulkan` として記録されることを検査する。
- 2026-10-10: all-adapters統合テストでactive plan nodeをbackend+adapter単位でworker summaryへ照合し、lane数だけが一致する誤検出を防ぐ。各active adapterにframe実績があり、worker実績がactive planに存在することを個別検査する。
- 2026-10-10: XPU mixed／multi-GPU workerは最初のframeをlaneごとに重複なく予約し、残りを共通atomic counterから取得する。thread起動順の偏りでactive GPU laneが0 frameになるのを防ぐ。
- 2026-10-10: XPU bench JSONにCPU/GPU別の累積render時間、平均frame時間、dispatch weightを保存し、summaryログだけにあったscheduler観測値を再現可能なbench記録へ含める。
- 2026-10-10: `Core.Parallel` に一つの共有 TBB arena を設け、`Parallel::For` と XPU mixed CPU frame lanes の両方を同じ arena concurrency で実行する。RenderQueue は初回 arena 初期化に `QThread::idealThreadCount()` を渡し、既存 arena が先に初期化済みなら実 concurrency を使って job host limit を上限化する。統合テストは scheduler 名と予算包含を確認するよう更新。初回ビルドではMSVCが直接importした `ThreadPool.ixx` 内のoneTBB inline実装で失敗したため、その経路を撤去し `Core.Parallel` 実装境界へ集約。代替変更はDebugビルド済み。混在テキストと全アダプターケースで実runtime確認済み。
- 2026-10-10: 専用GPU workerごとの成功render累積時間と平均frame時間をsummary/bench JSONへ記録し、iGPU/dGPU間の実測throughput差を個別比較できるようにする。
- 2026-10-10: shared primary GPU lane の累積時間と平均時間も専用workerとは分けてsummary/bench JSONへ保存し、Vulkan primary＋D3D12 workers構成の全GPUレーンを比較可能にする。
- 2026-10-10: mixed integration testで `primaryGpuMs` summary field の存在も検査する。
- 2026-10-10: GPU worker／primary laneの専用時間はnanosecondで累積し、bench出力時に小数msへ変換する。1ms未満のframeを0msとして扱いthroughput比較を潰さない。
- 2026-10-10: CPU/GPU aggregate時間もnanosecond累積へ統一し、summary/benchの小数msを維持する。GPU失敗後のCPU fallback frameはCPU fallback区間だけをCPU throughputへ加算し、失敗したGPU試行時間を混在させない。計測精度・分類のruntime照合は未実施。
- 2026-10-10: job summaryへCPU/GPU累積render msを追加し、mixed統合テストで両欄の数値出力を確認する。測定値の実機解釈・性能比較は未実施。
- 2026-10-10: 順序復元用output bufferが後続frameで満杯でも、consumerが次に待つframeだけは容量待ちを通過できるようにする。boundedな通常上限を保ちながら、最古frameが後続frameに塞がれて全体停止するのを防ぐ。

- 2026-10-10: Debug構成でXPU対象を再ビルド。RTX 4070 Ti Vulkan + CPU hostで混在テキスト、独立した2D/3D particle composition、10-frame all-adapters/backpressureの各Render Queue統合ケースが3回連続成功。テキストのCPU/GPU cross-backend exact RGBA一致は不成立のため合否条件にせず、parityは未完了。HarfBuzzのAVは、thread-local FreeType libraryに対してface cacheがprocess-global共有だった不整合を修正し、CPU worker同時実行時のクラッシュを解消。Vulkan＋D3D12ケースはテスト上skip（このhostに対応topologyなし）。対応済みGPUが一枚のhostでの確認であり、複数物理GPU/iGPU並列の実機受入は未完了。
- 2026-10-10: D3D12 SDK try_compileへ実SDK Include/Libパスを設定し、D3D12_SUPPORTED/VULKAN_SUPPORTEDをTRUEにした。Debug test targetはビルド成功。XPU mixed-text、particle、all-adaptersはCTest成功。Vulkan+D3D12 CTestは同一GPUのbackend重複排除によりGoogleTest skip。OSのDXGI/D3D12直接probeはRTX adapterとfeature level 11_0〜12_2で成功する一方、Artifact経由のD3D12 primary作成は失敗しVulkan fallbackとなる。複数物理GPU/iGPU並列は未検証。
