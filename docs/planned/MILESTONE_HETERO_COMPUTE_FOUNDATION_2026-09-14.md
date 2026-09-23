# XPU ヘテロコンピューティング基盤（CPU複数＋dGPU複数＋iGPU）詳細計画 — 2026-09-14

**最終更新:** 2026-09-23
**ステータス:** Partial — D3D12 final render の iGPU フレーム worker 参加は実装済み。XPU 正式名称・`XpuNodeDesc` 統一 plan・budget 判定・起動ログ（P1残分）を実装中。CPU/GPU 統合 scheduler、adapter 別 in-flight 適用、iGPU assist 本格化、実機 parity／性能検証は未完了。
**正式名称:** XPU（旧称 hetero。コード上の正規型は `XpuNodeDesc`、`HeteroNodeDesc` は別名として残す。正規環境変数は `ARTIFACT_XPU`、`ARTIFACT_HETERO` と `ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED` は後方互換の別名）
**作成日:** 2026-09-14
**対象:** ファイル書き出し（レンダーキュー）経路の CPU＋GPU 併用と、将来の複雑なヘテロ実行の土台
**範囲:** `Artifact/src/Render/` 中心。`ArtifactCore` 変更は提案のみ（子リポ編集は承認必須）
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
  QString id;                  // "cpu:0", "gpu:1", "igpu:assist"
  int maxInFlight = 1;
  size_t memoryBudgetBytes = 0;
  GpuDeviceDesc gpu;           // GPU 系のみ有効
};
```

- 列挙は `DiligentDeviceManager::availableAdapters()` 単一入口。
- フィルタは環境変数拡張で表現し、コード分岐を増やさない:
  - `ARTIFACT_GPU_POLICY`: 既存 `auto/high-performance/power-saving/specific`
- `ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED`: `0` / `false` / `off` / `no` で iGPU final worker を除外。未設定時は D3D12 Integrated を含める。
- 将来の `ARTIFACT_HETERO`（例: `cpu=auto,gpu=all,igpu=assist`）は、CPU/GPU 統合 scheduler を導入する P2 まで追加しない。
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

- 作業:
  - `availableAdapters()` をヘテロ登録の正規入口に固定。
  - `HeteroNodeDesc` 構築＋`ARTIFACT_HETERO` パース（既定は現行動作と同一）。
  - `Software` 除外、iGPU=assist 既定、dGPU=full。
  - 起動ログに選択 node 一覧（adapter/score/budget）を出す。診断ログの遅延評価を守る。
- 対象: `DiligentDeviceManager.cppm`、`ArtifactRenderQueueService.cppm`（plan 構築部）、`ArtifactIRenderer.cppm`（adapter 初期化は現状維持）。
- DoD: 単GPU／複数dGPU／iGPU混在の各環境で plan のみ正しく列挙される（render 挙動不変）。
- 非DoD: 実分配の変更なし。
- 2026-09-21 実装済み: `ArtifactRenderQueueService` の既存 final worker 分配で、D3D12 Integrated adapter を Discrete adapter と同じ独立 worker として候補化した。`ARTIFACT_MULTI_GPU_INCLUDE_INTEGRATED=0` で除外できる。`HeteroNodeDesc`、統一 plan、budget 判定、実機検証は未実装。
- 2026-09-23 実装済み（P1残分）: 正規型 `XpuNodeDesc`（`HeteroNodeDesc` は別名）を `ArtifactRenderQueueService` に追加。`buildXpuPlan()` が CPU group＋非 Software adapter 列挙（Discrete→`GpuFull`、Integrated→`igpu=` 指定、既定 `full`）＋adapter 別 memory budget（local→unified フォールバック）を構築し、multi-GPU active 時と single-GPU fallback 時の両方で `xpuPlanDebugState()` を起動ログに出す。実機での plan 列挙確認は未実施。
- 2026-09-23 実装中（P2 最小作動・opt-in）: `ARTIFACT_XPU=mixed=on` でのみ CPU worker＋GPU worker 混在。条件は multi-GPU 成立＋simulation なし＋FullFrame＋非HTML＋非マルチチャンネル/deep＋残フレーム・残 in-flight あり。CPU worker は MFR と同じ契約（isolated snapshot＋software path、`renderSingleFrame` の mutex を取らない）で `renderOneFrame(forceCpuPath)` に接続し、frame ログは `xpu-cpu` 識別。合計 thread 数は `maxInFlightFrames_` 以内（GPU 優先、CPU は残枠）。clone 失敗・条件不成立は既存 multi-GPU 動作へ fallback。`compositionFrameStateMutex_` は維持（MFR Phase 0 未完のため撤去しない）。`useMfr`／`useMultiGpu` の既定判定は不変。実機の混在受入（全 frame 成功＋順序復元＋逐次一致＋cancel/failure/ledger）は未実施。

### P2: Hetero dispatcher（CPU+GPU 混在の最小作動）

- 作業:
  - raw thread 分配を `ArtifactRenderScheduler` または Farm へ寄せ、CPU worker＋GPU worker を同一 `renderOneFrame` に接続。
  - device 別 in-flight（CPU=`maxInFlightFrames_` 可変化、GPU=adapter 数＋VRAM 予算）。
  - `useMfr`／`useMultiGpu` の二重判定を `HeteroPlan` 単一判定へ集約（振る舞いは P1 と同一から開始し、混在は flag で opt-in）。
  - `cloneCompositionSnapshot` pool 化（確保回数削減）。
- 対象: `ArtifactRenderQueueService.cppm:6280-6504` 付近、`ArtifactRenderScheduler.cppm`、`MFRDispatcher`（concurrency 数理の参照）。
- DoD: 混在 opt-in 時に全 frame 成功＋順序復元＋逐次一致。cancel/failure/ledger が従来通り。
- 注意: `compositionFrameStateMutex_` は P0 未了なら残す。外すのは P0 受入後。

### P3: Consumer pipeline＋encode 効率（利用率の主戦場）— 2026-09-23 部分実装（DoD 未達）

- 済（出力不変・既定不変）: (1) `FFmpegEncoderSettings.threadCount` を追加し `codecCtx_->thread_count` へ接続。未設定時は encoder default（=現行動作）。`ARTIFACT_XPU_ENCODER_THREADS` で上書き可（0〜64、0 は default）、`buildNativeVideoSettings`/`buildGpuVideoSettings` 共通。preset の機械的変更なし。(2) 進捗 preview の時間間引き（既定 250ms、最終フレームは常時 publish）。`ARTIFACT_XPU_PREVIEW_MIN_INTERVAL_MS` で上書き可（0 で従来どおり、出力に影響しない）。(3a) 連番書込の bounded async（`ARTIFACT_XPU_ASYNC_SEQUENCE=on` または `ARTIFACT_XPU=asyncseq=on` でのみ有効、既定 off）。単チャンネル image sequence のみを `std::async` で並列書込し、pending 上限は `2*maxInFlightFrames_`、最終 drain で失敗を ledger へ反映。video/HTML/SVG/multi-channel/deep は同期のまま。(3b) 動画向け 3-stage pipeline の最小形（`ARTIFACT_XPU_PIPELINE=on` または `ARTIFACT_XPU=pipeline=on` でのみ有効、既定 off）。取得 serial→変換 parallel（RGBA8888 への detach を `std::async` で並列）→encode serial（`addFrame` は順序必須）の構造を consumer thread 内で実現。出力不変。(5) `maxInFlightFrames_` を `ARTIFACT_XPU_MAX_IN_FLIGHT` で可変化（既定 4=現行動作）。
- 残（P3 DoD 向け）: sws/YUV 変換の本格並列化（`thread_count` 有効範囲の bench と `AsyncImageWriterManager` への置換検討）＋ 4K wall-time 受入。preset 露出設計は P3 で機械的 `slow→medium` を行わない方針を維持。
- 対象: `ArtifactCore/src/Image/FFmpegEncoder.cppm`、`ArtifactCore/include/Video/FFMpegEncoder.ixx`、`Artifact/src/Render/ArtifactRenderQueueEncoder.cppm`（(1)）、`Artifact/src/Render/ArtifactRenderQueueService.cppm`（(2)(3a)(3b)(5)）。
- DoD: 同一出力で wall-time 短縮、画質・仕様不変、4K 時の変換律速が緩和（未受入・実機 bench 要）。

### P4: iGPU assist 本格化＋適応調整 — 2026-09-23 部分実装（DoD 未達）

- 済（出力不変・opt-in/plan 予約）: `buildXpuPlan` で `igpu=assist` 時に `GpuAssist` ノードを列挙し、`xpuPlanDebugState` と job summary に出す。preview 縮小のみ `igpu=assist` 時に async（将来の iGPU offload 雛形）で実行し、full/off では同期のまま。device 生成は従来どおり直列・専有を維持。
- 残: convert/scale/scope/proxy/encode 補助への本格接続、device 別 throughput 計測→重み autotune（起動時固定＋job 内 slow-start）、NVENC/QSV 同時実行上限・PCIe 競合の backoff（失敗時は CPU/GPU-full へ fallback）。
- 対象: `Artifact/src/Render/ArtifactRenderQueueService.cppm`（`xpuIgpuRole`/`buildXpuPlan`/`shouldIncludeIntegratedGpuWorkers`、preview assist 分岐）。
- DoD: iGPU on/off で破綻なし。律速が render→consumer へ移動しない（未受入・実機 bench 要）。

### P5: 観測・受入・文書化 — 2026-09-23 部分実装（DoD 未達）

- 済（cold path のみ、出力不変）: frame 別 `renderBackend` を `gpu-multi`/`xpu-cpu`/`gpu`/`cpu` に拡張（P2）、XPU plan を起動ログと job summary の両方に出す（P1/P5）、job 終了時に wallMs＋gpuFrames/xpuCpuFrames＋mixed/asyncSeq＋parity＋pipeline を `XPU job summary` として出す。parity は `ARTIFACT_XPU_PARITY_HASH=on` または `ARTIFACT_XPU=parity=on` でのみ FNV-sample hash を frame 毎に累積し、逐次 vs ヘテロの同一性検証に使える（既定 off）。bench JSON は `ARTIFACT_XPU_BENCH=on` または `ARTIFACT_XPU=bench=on` でのみ `temp/ArtifactStudio/xpu-bench/xpu_job*.json` に plan/wallMs/内訳/parity を残す（既定 off、出力不変）。`RenderPerformanceMonitor`/`FrameCache` の既存集計は温存。
- 再現手順（固定条件・受入用）: 4K comp（3840x2160/30fps/100frames/png sequence または h264 mp4）で `ARTIFACT_XPU` 未設定（逐次）vs `ARTIFACT_XPU=mixed=on`/`asyncseq=on`/`pipeline=on`/`parity=on`/`bench=on` の組合せを同一入力で実行し、`XPU job summary` の wallMs と bench JSON の hash 一致で parity と consumer 律速の内訳を比較する。`threads` は `ARTIFACT_XPU_ENCODER_THREADS`、`maxInFlight` は `ARTIFACT_XPU_MAX_IN_FLIGHT` で固定し、preset は変えない。
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
2. iGPU の既定役割（提案: `assist`）。`full` 参加を既定 on にするか。
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
