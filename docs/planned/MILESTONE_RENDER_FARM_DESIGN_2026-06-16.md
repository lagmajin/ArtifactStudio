# M-RE-2 Render Farm Foundation Milestone

作成日: 2026-06-16
対象: `ArtifactCore/src/Network/NetworkRPCServer.cppm`,
      `ArtifactCore/src/Render/RendererQueueManager.cppm`,
      `Artifact/src/Widgets/Render/RenderQueueManagerWidget.cppm`,
      `Artifact/src/Service/ArtifactRenderQueueService.cppm`,
      `ArtifactCore/src/Render/ArtifactJobContract*`,
      `docs/planned/MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md`,
      `docs/planned/MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md`,
      `docs/planned/MILESTONE_RENDER_QUEUE_2026-03-22.md`,
      `docs/planned/RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md`
位置づけ: 既存 `EXTERNAL_RENDERER_DESIGN` の **上位レイヤー** として、master / worker の job 契約 / 進捗同期 / checkpoint / retry / 健全性 を 1 つの milestone にまとめる。
参照:
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P3)
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#28 Render Farm, #29 Queue checkpoint)
- `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`
- `docs/planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`

---

## 1. 目的

Render Farm の **foundation** は 2 つの既存設計で薄く定義されている。

- `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` — 親 / 子プロセスの分離、job を snapshot で渡す、データ境界の純データ化
- `MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md` — サムネ / waveform / proxy などの雑用を in-process job contract に揃える

しかし、**「複数 worker で frame range を分割して並列にレンダリングし、結果を master が束ねる」** ための contract と運用は未着手。

アーティストの痛点メモ (2026-04-19) では「**Render Farm オーケストレーション（master/slave スケジューラ）が無い**」が P3 として挙げられ、`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` でも #28 / #29 が未実装扱い。

この milestone は「`ArtifactAbstractLayer` / `ArtifactAbstractEffect` の live object を直接 worker へ渡さない」「`ImmediateContext` を wire で共有しない」 という `M-IR-8` / `RENDER_BOUNDARY_SAFETY_GATE` の方針を保ったまま、**in-process multi-worker を 1 段噛ませて farm の挙動を検証できる foundation** を 1 つの表にまとめる。

> 重要: out-of-process worker は **Phase 4 以降** とし、Phase 1〜3 は **in-process worker** のみで成立させる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/src/Network/NetworkRPCServer.cppm` — 低レベル RPC サーバ。実体はソケット 1 本
- `ArtifactCore/src/Render/RendererQueueManager.cppm` — queue manager
- `Artifact/src/Widgets/Render/RenderQueueManagerWidget.cppm` — queue UI
- `Artifact/src/Service/ArtifactRenderQueueService.cppm` — queue service
- `MILESTONE_RENDER_QUEUE_2026-03-22.md` — 既存の queue milestone
- `MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md` — エンコード
- `MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md` — GPU backend
- `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` — 親 / 子プロセス分離
- `MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md` — 雑用 job
- `RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` — 16 件の gap（画像シーケンス出力、ETA、checkpoint/retry 等）

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Job contract | `ArtifactJobContract*` の下位型はあるが、frame range を分割する protocol がない | worker 間分配不可 |
| Worker 発見 | 固定 host 1 台前提。discovery なし | 複数 worker 登録不可 |
| Job 分配 | FIFO のみ。frame range 分割 / 優先度 / affinity なし | スケールしない |
| Checkpoint | なし。途中失敗で全 frame やり直し | 長時間 job が再実行できない |
| Retry | なし。失敗時の backoff / 再試行戦略がない | 1 frame 失敗で queue が止まる |
| 健全性 | worker の heartbeat / dead detection なし | 落ちた worker の job が永遠に queued |
| 進捗同期 | 単一 queue 内の progress のみ。master 集約なし | UI に farm 全体像が出ない |
| セキュリティ | RPC 認証なし、payload 暗号化なし | farm が LAN 前提で安全 |
| ログ集約 | 各 worker 個別ログのみ。master への streaming なし | 失敗解析が困難 |
| Diagnostics | 健全性が `M-CE-CRIT-1` に乗りにくい | regression gate に組み込めない |

### 2.3 既存 milestone との関係

- `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` — 「親 / 子のプロセス分離」の **アーキテクチャ**。本 milestone はそれの **上位レイヤー** で、master / worker の job contract を担う
- `MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md` — 雑用 job contract。本 milestone はレンダリング job 側で、共通 runtime を共有する想定
- `MILESTONE_RENDER_QUEUE_2026-03-22.md` — 単一 host の queue。本 milestone はそれを farm に拡張する
- `RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` — 16 件の gap。**そのうち checkpoint / retry / auto-restart を本 milestone がカバー**。画像シーケンス出力や ETA は別 milestone
- `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` — `ImmediateContext` を wire で共有しない方針。本 milestone もこれに従う
- `MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md` — 診断文法を共通化。本 milestone の健全性 contribution は同文法で揃える

---

## 3. 設計の柱

### 3.1 全体構成

```
[UI / Editor]
  └─ RenderQueueService
       └─ RenderFarmMaster
            ├─ WorkerRegistry   (host 探索 / heartbeat / dead detect)
            ├─ JobScheduler     (frame range 分割 / 優先度)
            ├─ CheckpointStore  (job 単位の進行 snapshot)
            ├─ RetryPolicy      (backoff / max attempts / 失敗時挙動)
            ├─ ProgressAggregator
            └─ LogCollector
                 ↓
            [Transport]
                 ├─ in-process (Phase 1〜3)
                 └─ out-of-process via NetworkRPCServer (Phase 4)
                 ↓
            [Worker]
                 ├─ RenderFarmWorker
                 ├─ FrameRangeRunner
                 └─ SnapshotHydrator
```

- **Master** は in-process に常駐
- **Worker** は in-process で 1 プロセス内に複数起動できる（Phase 1〜3）
- **Out-of-process** は Phase 4 で `NetworkRPCServer` ベースに乗せる

### 3.2 Job Contract

`MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md` の `UtilityJobRequest / Result / Progress` を **共通基底** として、レンダリング job 専用の派生を作る。

```cpp
struct RenderJobRequest : UtilityJobRequest {
    RenderSnapshot snapshot;            // pure data, live object 不可
    FrameRange requestedRange;          // 0..N
    RenderSettings settings;            // size / fps / format / codec
    AffinityHints affinity;             // GPU / CPU / disk
    CheckpointPolicy checkpointPolicy;  // every N frames
};

struct RenderJobProgress : UtilityJobProgress {
    FrameRange completedRange;          // 完了 frame
    FrameRange failedRange;             // 失敗 frame
    CheckpointRef lastCheckpoint;
    std::optional<JobHealthSnapshot> health;
};

struct RenderJobResult : UtilityJobResult {
    QVector<RenderedFrame> frames;      // 0..N
    FailureManifest failure;            // 失敗 frame の list
    WorkerDiagnostics diagnostics;
};
```

`RenderSnapshot` は `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` の `RenderSnapshot` をそのまま流用。

### 3.3 Worker 発見

- **Phase 1〜3**: 固定 worker pool。`ArtifactProjectManager` の `farm.workers` 設定で `count` を指定
- **Phase 4**: `WorkerRegistry` が `mDNS` または固定 IP list を `ArtifactCore/src/Network/NetworkRPCServer.cppm` の上に載せる
- **Heartbeat**: 5 秒間隔。最後の heartbeat から 30 秒無応答で dead とみなす

### 3.4 Job 分配

- **Default 戦略**: `GreedyFrameSplit`。frame range を worker 数で等分
- **優先度**: `High / Normal / Low` の 3 段。`High` は worker 空くまで即時割当
- **Affinity**: `AffinityHints` で `GPU=N` / `CPU=N` を指定。worker の capability と一致しない job は pending
- **再分配**: worker dead 時に `JobScheduler` が pending job を再分配

### 3.5 Checkpoint

- **保存先**: `<project_root>/.artifact/farm/checkpoints/<job_id>.json`
- **頻度**: `CheckpointPolicy` による。`every N frames` または `every M seconds`
- **内容**: `lastCheckpoint.frame` + `RenderSnapshot` の hash（実体は再構築可能）
- **復元**: 新しい job が `requestId` ベースで checkpoint を引き継げる

### 3.6 Retry Policy

- **Default**: `RetryPolicy { maxAttempts: 3, backoff: Exponential(initial=2s, factor=2, max=60s) }`
- **失敗時**: `frame` 単位で再試行し、N 回失敗で `failure.manifest` に積む
- **Dead letter**: `failure.manifest` に入った frame は `Hold` 状態にして手動再試行または skip を選択

### 3.7 Transport (in-process)

- `QMetaObject::invokeMethod` ベースで master → worker / worker → master の RPC を再現
- payload は **必ず QVariant / QJsonDocument** に閉じ、`QObject*` や生ポインタを跨がない
- `ArtifactCore/src/Network/NetworkRPCServer.cppm` の既存 transport を wrap する `InProcessTransport` を `ArtifactCore/src/Network/InProcessTransport.cppm` に追加

### 3.8 Transport (out-of-process, Phase 4)

- 既存 `NetworkRPCServer` を transport として再利用
- payload は JSON
- 認証: 共有 secret ベース（環境変数 `ARTIFACT_FARM_TOKEN`）。Phase 4 で TLS 対応は別 milestone
- **dead 扱い**: master ↔ worker の接続が切れたら即 dead。worker 側で再起動可能

### 3.9 Progress Aggregator

- master が worker からの `RenderJobProgress` を受信するたびに `ProgressAggregator` を更新
- `RenderQueueManagerWidget` は master 側の `ProgressAggregator` だけを見る
- 1 job 内の `completedRange / failedRange` を job 単位で集約

### 3.10 Log Collector

- 各 worker の stdout / stderr を master へ stream
- `<project_root>/.artifact/farm/logs/<worker_id>/<job_id>.log` に保存
- UI 側は worker 単位 / job 単位の 2 段フィルタでログを絞り込み

### 3.11 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 文法に:

- `farm.worker.dead` (severity=error)
- `farm.checkpoint.stale` (severity=warning, hash 不一致)
- `farm.frame.failed` (severity=info, `failure.manifest` 内 frame 数)
- `farm.queue.starvation` (severity=warning, queue > N 件で worker 待ち)

### 3.12 不変条件 (Guardrails)

- `ArtifactAbstractLayer / ArtifactAbstractEffect` の **生ポインタは絶対に wire を跨がない**。常に `RenderSnapshot` 経由
- `ImmediateContext` / `IDeviceContext` / `GpuContext` を wire で共有しない
- `QImage` の **hot path 流入禁止**。download は export 時に限定
- 既存の `W_OBJECT` シグナル / `QUndoCommand` の整合を保つ。新規 `W_OBJECT` 派生は追加しない
- farm worker の **GPU メモリ使用量** は worker 単位で上限を設ける（既定 4 GB）。超えたら job を別 worker に退避
- master 側の状態は **project JSON に保存** しない。チェックポイントのみ

---

## 4. フェーズ計画

### Phase 1: Job Contract + In-process worker pool (P0, 2〜3 セッション)

- `RenderJobRequest / Progress / Result` 追加
- `RenderFarmMaster` クラス追加（in-process 固定 pool のみ）
- `RenderFarmWorker` クラス追加
- `InProcessTransport` 追加

**Done criteria:**
- 1 つの job を 2 worker で並列にレンダリングできる
- frame range が等分される
- 結果フレームが master に集約される

### Phase 2: Checkpoint + Retry (P0, 2 セッション)

- `CheckpointStore` 追加
- `RetryPolicy` 追加
- 失敗 frame を `failure.manifest` に積み、`Hold` 状態で停止

**Done criteria:**
- 100 frame job を 50 frame 完了で stop → 復元で 51 frame から再開できる
- 1 frame を 3 回失敗させると `Hold` 状態になり、`failure.manifest` に出る

### Phase 3: Progress / Log / Diagnostics (P0, 1〜2 セッション)

- `ProgressAggregator` 追加
- `LogCollector` 追加
- Problem View への contribution

**Done criteria:**
- `RenderQueueManagerWidget` に farm 全体の進捗と ETA が出る
- 失敗 frame を Problem View で確認できる
- log が `<project_root>/.artifact/farm/logs/` に書かれる

### Phase 4: Out-of-process transport (P1, 2〜3 セッション)

- `NetworkRPCServer` を transport として再利用
- worker discovery と heartbeat
- dead detection と再分配

**Done criteria:**
- 別プロセスで起動した worker が master に登録される
- master ↔ worker の接続が切れたら 30 秒以内に dead 検出
- dead worker の job が他 worker へ再分配される

### Phase 5: Affinity / 優先度 / セキュリティ (P2, 別 milestone 推奨)

- `AffinityHints` の本格対応
- 認証 / TLS / アクセス制御
- これは **P2** のため、Phase 5 は本 milestone のスコープ外。将来 milestone のエントリだけ作る

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_FARM_AFFINITY_SECURITY_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md` | 親 / 子分離。本 milestone はその上の master / worker contract。 |
| `MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md` | 雑用 job contract。本 milestone のレンダリング job 派生が共通基底を共有。 |
| `MILESTONE_RENDER_QUEUE_2026-03-22.md` | 単一 queue。本 milestone が farm に拡張。 |
| `RENDER_QUEUE_MANAGER_GAP_ANALYSIS_2026-04-13.md` | 16 gap のうち checkpoint / retry / auto-restart を本 milestone が吸収。 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | `ImmediateContext` を wire で共有しない方針。 |
| `MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md` | 診断文法を共通化。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | Problem View。本 milestone Phase 3 が contribution。 |
| `MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md` | `M-IR-8` を侵さない。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Snapshot hash 衝突**。`RenderSnapshot` の hash 不一致時に復元できない。hash 衝突は実質無視できるが、JSON 容量の注意
2. **GPU メモリ退避**。worker 単位の上限を超えても、`D3D12/Vulkan` の context 切替には数百 ms かかる。`AffinityHints` で早期に routing する方が現実的
3. **In-process transport の QMetaObject 制約**。`Q_DECLARE_METATYPE` の登録漏れは致命的。`ArtifactCore` の Job Contract に必ず `W_REGISTER_ARGTYPE` を入れる
4. **Checkpoint サイズ**。`<project_root>/.artifact/farm/checkpoints/` の容量。`CheckpointPolicy` で `every N frames` にして肥大化を防ぐ
5. **Heartbeat 失敗と dead 検出の境界**。ネットワーク瞬断で誤検出しないよう、3 回連続失敗で dead とする

### 6.2 契約上の未解決

- **Out-of-process 認証**。`ARTIFACT_FARM_TOKEN` のスコープ。Phase 4 で決定
- **Affinity 戦略**。`GPU=N` の N が worker 側の GPU 数を超える場合の挙動。Phase 1 では worker 1 個 = GPU 1 個 前提
- **Worker capability**。`Capability::gpus / gpusMemory / diskFree` の自動検出。Phase 4 で OS 依存 path を追加
- **複数 project の farm 共有**。複数 project を 1 farm で並走させるか、project ごとに独立させるか。Phase 1 では project ごと独立
- **Retry 粒度**。frame 単位 retry が基本だが、frame 内の effect 単位 retry まで細分化するかが論点。本 milestone では frame 単位まで
- **Checkpoint 互換**。project スキーマ変更時に旧 checkpoint が読めない可能性。Phase 2 で `schemaVersion` を持つ

### 6.3 サブモジュール境界

- `ArtifactCore/src/Network/NetworkRPCServer.cppm` は **既存 RPC を再利用**。破壊変更はしない
- `ArtifactCore/src/Render/ArtifactJobContract*` を **拡張** する破壊変更は禁止。新規ファイル追加で対応
- `ArtifactWidgets` は触らない
- `ArtifactCore/CMakeLists.txt` に新ファイルを登録するが、既存 target を壊さない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- 1 つの job を 2 worker で並列にレンダリングできる
- 失敗 frame を retry して `failure.manifest` に積める
- 100 frame job を途中で stop → 復元で続きからレンダリングできる
- farm 全体の進捗と ETA が `RenderQueueManagerWidget` に出る
- log が `<project_root>/.artifact/farm/logs/` に書かれる
- Problem View に farm 健全性が表示される
- out-of-process worker が master に登録され、dead 検出で再分配される
- `ArtifactAbstractLayer / ArtifactAbstractEffect` の生ポインタが wire を跨がない
- `ImmediateContext / IDeviceContext` を wire で共有していない
- 新規 `QImage` / `setStyleSheet` / 新規 global signal が増えていない
- `ArtifactWidgets` を触っていない
- `ArtifactCore` への bump 手順が `.github/GIT_WORKFLOW_PARENT_CHILD.md` に整合

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`EXTERNAL_RENDERER_DESIGN` / `BACKGROUND_UTILITY_WORKER_PROCESS` を上位で束ねる farm foundation として再整理。
