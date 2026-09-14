# ヘテロコンピューティング基盤（CPU複数＋dGPU複数＋iGPU）詳細計画 — 2026-09-14

**最終更新:** 2026-09-14
**ステータス:** Not Started
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
| dGPU 複数 | Discrete D3D12 のみ、2枚以上・final限定の雛形あり。各 worker が `ArtifactIRenderer`＋immediate context を専有、生成は mutex 直列 | `Artifact/src/Render/ArtifactRenderQueueService.cppm:3229-3277`、`6318-6366`、`Artifact/src/Render/ArtifactIRenderer.cppm:1852` |
| アダプタ列挙 | backend/type/VRAM/RT/スコアリング＋ポリシー選択あり。iGPU も列挙できるが書出し worker には未使用 | `Artifact/src/Render/DiligentDeviceManager.cppm:381-516`（score/policy）、`518-`（candidates）、`412-428`（`ARTIFACT_GPU_POLICY`） |
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
  │    GpuExecutor dGPU×N (dedicated IRenderer + cache set)
  │    GpuAssist iGPU×0/1 (copy/encode/light compute)
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
  - 新規 `ARTIFACT_HETERO`（例: `cpu=auto,gpu=all,igpu=assist`）。未設定時は現行動作（Discrete 複数のみ）にフォールバック。
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
- `GpuFull (dGPU)`: 現 `GpuFinalWorker` を踏襲（renderer＋textureCache＋surfaceCache＋mattePool を worker 専有）。device 生成直列維持。
- `GpuAssist (iGPU)`: 初期は render 主担当にしない。readback 後の convert/scale、scope 集計、proxy/encode 補助、軽量 compute の offload 先。VRAM 小・帯域共有のため full 参加は opt-in。

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

### P0: 前提分離（MFR Phase 0 の完遂・再発明なし）

- 作業: MFR 文書 S8（AssetManager 保護確認）、S10（Registry mutex）、S9（queue 容量）、S15（atomic）、複製一致テスト。
- DoD: `clone→renderSingleFrame` が逐次と一致。全レイヤー種別。CompositionContext/3D カメラの toJson 欠落があれば先に `COMPOSITION_API_HARDENING` P1。
- 対象: MFR 文書の Phase 0 に準拠。新規設計なし。

### P1: DeviceRegistry＋ポリシー（I/O なし・判断なし）

- 作業:
  - `availableAdapters()` をヘテロ登録の正規入口に固定。
  - `HeteroNodeDesc` 構築＋`ARTIFACT_HETERO` パース（既定は現行動作と同一）。
  - `Software` 除外、iGPU=assist 既定、dGPU=full。
  - 起動ログに選択 node 一覧（adapter/score/budget）を出す。診断ログの遅延評価を守る。
- 対象: `DiligentDeviceManager.cppm`、`ArtifactRenderQueueService.cppm`（plan 構築部）、`ArtifactIRenderer.cppm`（adapter 初期化は現状維持）。
- DoD: 単GPU／複数dGPU／iGPU混在の各環境で plan のみ正しく列挙される（render 挙動不変）。
- 非DoD: 実分配の変更なし。

### P2: Hetero dispatcher（CPU+GPU 混在の最小作動）

- 作業:
  - raw thread 分配を `ArtifactRenderScheduler` または Farm へ寄せ、CPU worker＋GPU worker を同一 `renderOneFrame` に接続。
  - device 別 in-flight（CPU=`maxInFlightFrames_` 可変化、GPU=adapter 数＋VRAM 予算）。
  - `useMfr`／`useMultiGpu` の二重判定を `HeteroPlan` 単一判定へ集約（振る舞いは P1 と同一から開始し、混在は flag で opt-in）。
  - `cloneCompositionSnapshot` pool 化（確保回数削減）。
- 対象: `ArtifactRenderQueueService.cppm:6280-6504` 付近、`ArtifactRenderScheduler.cppm`、`MFRDispatcher`（concurrency 数理の参照）。
- DoD: 混在 opt-in 時に全 frame 成功＋順序復元＋逐次一致。cancel/failure/ledger が従来通り。
- 注意: `compositionFrameStateMutex_` は P0 未了なら残す。外すのは P0 受入後。

### P3: Consumer pipeline＋encode 効率（利用率の主戦場）

- 作業（PROPOSAL 順）:
  1. FFmpeg `thread_count/thread_type`（Core・承認要）＋ preset 露出設計（機械的 `slow→medium` 置換はしない）。
  2. preview 縮小間引き。
  3. consumer 3段 pipeline（取得 serial／変換 parallel／encode serial）。連番書込は parallel 段へ。
  4. sws 変換の並列化（threads option or `Parallel::For` 前処理）。
  5. `maxInFlightFrames_` 可変化（HW＋memory 連動）。
- 対象: `ArtifactCore/src/Image/FFmpegEncoder.cppm`、`ArtifactRenderQueueService.cppm`（consumer）、`AsyncImageWriterManager` 接続。
- DoD: 同一出力で wall-time 短縮、画質・仕様不変、4K 時の変換律速が緩和。

### P4: iGPU assist 本格化＋適応調整

- 作業:
  - iGPU を convert/scale/scope/proxy/encode 補助に正式接続。
  - device 別 throughput 計測→重み autotune（起動時固定＋job 内 slow-start）。
  - NVENC/QSV 同時実行上限・PCIe 競合の backoff（失敗時は CPU/GPU-full へ fallback）。
- DoD: iGPU on/off で破綻なし。律速が render→consumer へ移動しない。

### P5: 観測・受入・文書化

- 作業:
  - frame 別 `renderBackend` ログ（既存 `renderBackend=gpu-multi/gpu/cpu` 拡張で `cpu/gpu:N/igpu` 識別）。
  - `RenderPerformanceMonitor`/`FrameCache` 集計＋bench harness（4K 固定条件・再現手順）。
  - parity hash の job 終了時記録。
- DoD: M-IR-6 の「再現可能な bench」未達を本基盤で解消。数値目標はここで確定。

### §7 予約: タイル内分散（本計画では実装しない）

- 条件: P0–P5 受入後、かつ halo・決定論・parity の設計レビュー通過後のみ。
- 候補: 大フレームの効果適用タイル分割、CPU タイル＋GPU タイルの合成。`ComputeMode::AUTO` の device 拡張として扱い、dispatcher とは層を分ける。

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
