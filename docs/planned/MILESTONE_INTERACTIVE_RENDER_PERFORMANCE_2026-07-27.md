# Milestone: Interactive Render Performance

**ステータス:** Not Started

**作成日:** 2026-07-27
**対象:** Composition Viewer、`ArtifactIRenderer`、effect pipeline、preview pacing
**主目的:** 編集結果の即時性を保ったまま、GPU/CPU 同期、再生成、再転送、全画面再合成を段階的に減らす

**GPU-driven進捗:** 2026-07-27 に `RP-8` のsubmission foundationとしてClone batch、Instanced Mesh、Particleへcapability-gated indirect drawを導入。続いてInstanced MeshとAdditive ParticleへGPU visibility culling、compaction、GPU-side args count生成を追加。Cloneの変形矩形packetはGPU batch / indirectまでで、GPU cullingは残作業。

## 1. Goal

- 平面 1 枚のような軽い composition で、再生とプロパティ操作の frame pacing を安定させる。
- 重い effect / video / 多レイヤーでも、同期ストールの原因を自動ログから特定できるようにする。
- GPU で生成した中間結果を可能な限り GPU 上に維持し、`GPU -> CPU -> GPU` 往復を通常 preview 経路から外す。
- 変更された layer と、その変更に依存する下流だけを再評価・再合成する。
- CPU preparation と GPU execution を限定的に重ねつつ、編集操作の見かけの遅延を増やさない。
- backend fork の変更を前提にせず、まず Artifact アプリ側の境界と運用を最適化する。

## 2. Current Baseline

2026-07-27 時点のコード監査で、以下は既に存在する。

- 通常 frame ごとの `flushAndWait()` は Composition Viewer の本流では無効化されている。
- `ArtifactIRenderer` には staging texture / fence を再利用する async readback ring がある。
- preview render slot は 2 slot 構成で、slot acquisition / hazard の統計がある。
- `RenderKeyState`、`RenderDamageTracker`、layer surface cache、GPU texture cache がある。
- immediate submitter は PSO、SRB、vertex / constant buffer の再利用と一部 batching を行う。
- frame phase、GPU timing、present timing の計測基盤がある。
- pointwise effect の fusion は一部始まっている。

一方、次が主要な未完了点である。

- 多数の effect が `dispatch -> staging copy -> Flush -> WaitForIdle -> Map -> CPU image` を行い、その後 GPU へ再 upload する。
- damage / dirty 情報はあるが、完成済み中間 texture を使った真の partial recompose まで一貫していない。
- full target clear と全 layer composite が残り、overlay や単一 property 変更でも仕事量が大きい。
- swapchain buffer 数は固定的で、VSync / frame latency / present policy を実測比較する運用がない。
- sync readback、fallback、resource rebuild が「なぜ発生したか」を後から AI が追える永続ログに十分残していない。

## 3. Performance Contract

最適化は平均 FPS だけでなく、編集の応答性と正しさを守る。

### Interactive mode

- property drag / gizmo drag の preview は commit を待たず、最新値を coalesce して反映する。
- 入力から最初の見た目更新までを優先し、frame queue を深くしない。
- 古い未表示 frame は破棄できるが、最新の編集結果は破棄しない。
- preview 用の品質低下を使う場合も、操作終了後に full-quality frame へ収束する。

### Playback mode

- timeline clock を authoritative とする。
- decode / render が遅れた場合は、待ち続けて全体速度を落とすのではなく、表示可能な最新 frame へ追いつく。
- last-good-frame を短時間維持し、未 decode frame を理由に不用意な黒画面を出さない。
- drop / repeat / decode late / render late を別イベントとして記録する。

### Offline render mode

- frame drop を禁止し、各 frame の完成を待つ。
- readback は許可するが、preview と同じ同期 policy を共有しない。
- preview 最適化による近似や stale result を最終出力へ持ち込まない。

## 4. Success Metrics

初回実装前に同じ test composition と操作手順で baseline を保存する。

### Required counters

- CPU frame time: median / p95 / p99
- GPU frame time: median / p95 / p99
- present time と present block time
- missed frame budget 数
- synchronous wait count / total wait ms / max wait ms
- readback count / bytes / total ms / reason
- upload count / bytes / total ms / reason
- PSO / SRB / buffer create count
- layer surface cache hit / miss / eviction
- dirty layer count / recomposited layer count / full recompose count
- draw / dispatch count と pipeline state change count
- preview slot wait / collision / dropped stale result count
- property input から visible frame までの latency

### Initial acceptance targets

計測環境差を避けるため、絶対 FPS だけでなく baseline 比を使用する。

- 平面 1 枚、effect なし:
  - synchronous readback: 0 / frame
  - `WaitForIdle`: 0 / frame
  - property drag visible latency p95: 50 ms 以下
  - frame time p95: composition frame budget の 1.25 倍以内
- 静止 20 layer:
  - overlay-only 更新時の layer surface regeneration: 0
  - unchanged layer cache hit: 95% 以上
- GPU effect chain:
  - chain 中間の CPU readback / re-upload: 0
  - chain 前後の explicit boundary readback のみ許可
- 再生:
  - 1 秒以上の連続 backlog を作らない
  - drop / repeat がログ上で decode late と render late に分類される

数値は Phase 0 の baseline 後に GPU 別プロファイルへ更新する。

## 5. Work Packages

## RP-0: Measurement and Automatic Evidence Log

最適化前に、CPU fallback と同期経路を自動で証拠化する。

### Scope

- readback / upload / wait / fallback / resource recreation に reason code を付与する。
- frame 単位の summary と、閾値超過 event を JSON Lines 形式でファイルへ保存する。
- ログには composition ID、frame、layer / effect 種別、backend、解像度、byte 数、duration、call-site category を含める。
- 同一原因の警告は rate limit し、集計値は失わない。
- session 終了時または一定件数ごとに summary を書く。
- AI が後から解析できる固定 schema と保存場所を定義する。

### Required reason examples

- `PreviewRequired`
- `OfflineExportRequired`
- `CpuOnlyEffectFallback`
- `GpuEffectReadback`
- `AdjustmentLayerBackground`
- `AsyncCachePopulate`
- `DeviceRecovery`
- `SwapchainRecreate`
- `ExplicitUserCapture`

### Exit criteria

- 重い frame を選ぶと、どの wait / readback / fallback が frame budget を消費したかファイルだけで追跡できる。
- CPU fallback に入った effect / layer を自動警告できる。
- disabled 時の hot-path overhead が計測誤差内である。

## RP-1: Sync Boundary Cleanup

`flushAndWait()` を一律削除せず、同期が必要な境界と不要な境界を分離する。

### Scope

- per-frame `WaitForIdle` / blocking Map の call sites を分類する。
- preview の通常描画、device recovery、swapchain recreate、offline export を別 policy にする。
- async readback ring の staging texture / fence を共通再利用する。
- sync API が必要な箇所は reason code を必須にする。
- present 前後の暗黙待機を計測し、CPU-side wait と GPU queue wait を区別する。

### Exit criteria

- 通常 preview の軽量 composition で `WaitForIdle` が発生しない。
- device teardown / recovery の安全な待機は維持される。
- sync API の新規利用をログまたは機械チェックで検出できる。

## RP-2: GPU-Resident Effect and Compositing Chain

最大の効果が見込める段階。GPU effect の中間結果を CPU image に戻さない。

### Scope

- effect API を GPU texture input / output の chain として扱える境界へ寄せる。
- ping-pong texture または render-graph-managed intermediate を再利用する。
- pointwise effect fusion を、互換性のある連続 effect に限定して拡張する。
- adjustment layer の背景取得を offscreen texture 参照へ変更し、全画面 readback を外す。
- CPU-only effect は明示 fallback node とし、前後の転送をログへ残す。
- color format / color space / alpha contract を chain 全体で維持する。

### First candidates

1. Color correction / exposure / tint 系 pointwise effect
2. Blur / glow 系の既存 compute effect
3. Adjustment layer background
4. GPU 対応を宣言しているが CPU image を経由する effect

### Exit criteria

- 対象 effect chain 内の `GPU -> CPU -> GPU` 往復が 0。
- CPU / GPU parity と alpha / color space の回帰がない。
- resource lifetime は frame slot と fence completion に従い、使用中 texture を再利用しない。

## RP-3: Persistent Layer Results and Partial Recompose

dirty layer だけを再生成し、変更位置より下流の合成だけを更新する。

### Scope

- dirty を `Content`、`Effect`、`Mask`、`Transform`、`Opacity/Blend`、`OverlayOnly` に分離する。
- layer-local rendered texture を frame を跨いで保持する。
- composition prefix / checkpoint texture を安全な境界で保持する。
- layer order、matte、adjustment layer、non-local effect を dependency barrier として扱う。
- overlay-only 更新では composition texture を再生成しない。
- damage rect は最初から完全対応を要求せず、まず dirty layer / downstream pass の削減を成立させる。

### Recompose rule

- content 変更: 対象 layer surface と、その layer 以降を再合成。
- transform / opacity 変更: layer content surface は再利用し、その layer 以降を再合成。
- overlay 変更: final composition texture を再利用し、overlay pass のみ描画。
- layer order / matte / adjustment 変更: dependency barrier から下流を再合成。
- temporal / full-frame effect: conservative に full dependency range を dirty とする。

### Exit criteria

- 静止 layer の surface は property 変更対象でない限り再生成されない。
- overlay-only 更新で layer pass が走らない。
- cache invalidation の過不足を debug validation mode で full render と比較できる。

## RP-4: Command and Resource Reuse

既存の再利用を計測可能にし、残る毎 frame 生成を除去する。

### Scope

- PSO / SRB / shader / sampler / vertex / constant buffer の create count を記録する。
- pipeline key と texture binding key を安定化する。
- dynamic / ring buffer を用い、毎 frame の小 buffer allocation を減らす。
- frame-local transient texture pool を導入または既存 pool へ統合する。
- device generation 変更時だけ resource set を再構築する。

### Exit criteria

- steady-state preview 中の PSO / shader creation は 0 / frame。
- steady-state preview 中の buffer / texture allocation が予算内かつ bounded。
- device loss / backend 切替後に stale resource を参照しない。

## RP-5: Layer Batching and State Reduction

correctness barrier を守れる単純な連続 layer だけを batch 化する。

### Eligible batch

- 同一 pipeline / blend contract
- mask / matte / adjustment / feedback dependency なし
- batch 対応 texture binding strategy で表現可能
- layer order を保てる

### Scope

- solid / simple sprite / normal blend の連続区間から開始する。
- batch break reason を diagnostics に残す。
- texture atlas は必須とせず、descriptor / array / instance data の実装コストを比較する。

### Exit criteria

- eligible scene で draw / state change count が baseline より減る。
- blend order、premultiplied alpha、clipping の差異がない。
- 複雑な layer を無理に batch せず既存 path へ安全に戻れる。

## RP-6: Frame Overlap Without Editing Lag

CPU preparation、GPU execution、readback wait を 1～2 frame 分だけ重ねる。

### Scope

- 既存 2-slot preview model と async readback ring を基礎にする。
- immutable frame snapshot と generation ID で stale completion を拒否する。
- interactive mode は latest-wins、offline mode は ordered-complete とする。
- immediate context を複数 thread から同時操作しない。
- queue depth と input-to-visible latency を常時計測する。

### Exit criteria

- frame N の GPU 実行中に N+1 の CPU preparation が進む。
- property / gizmo 操作で古い frame が最新編集結果を上書きしない。
- interactive queue depth は原則 1、最大 2 に制限される。
- frame overlap 有効時に visible latency p95 が悪化しない。

## RP-7: Present Pacing

VSync、swapchain buffer 数、最大 frame latency を推測ではなく実測で決める。

### Experiment matrix

- VSync on / off
- buffer count 2 / 3
- interactive / playback / idle
- windowed / maximized
- representative 60 Hz / 120 Hz 以上の display
- GPU-bound / CPU-bound / decode-bound composition

### Scope

- present interval、present duration、missed VBlank、queue depth を記録する。
- interactive mode と playback mode で policy を切り替える必要性を評価する。
- idle 時の無駄な present を抑える。
- tearing 許可は明示設定と backend capability の両方を要求する。

### Exit criteria

- 採用 policy と GPU / display 条件ごとの結果が文書化される。
- frame pacing の p95 / p99 が baseline より改善する。
- CPU 使用率だけを下げて入力遅延を増やす設定は採用しない。

## RP-8: GPU-Driven Submission

CPU が draw item を 1 件ずつ判定・送信する経路を、大量 item に限って GPU culling、compaction、indirect submission へ移行する。

この段階は平面 1 枚の frame pacing 修正ではなく、clone、particle、3D mesh、大量の同種 layer で CPU submission が bottleneck になった場合の拡張である。詳細な draw item contract と MDI 導入手順は `docs/planned/MILESTONE_GPU_DRIVEN_MDI_RENDER_2026-04-02.md` を正本とする。

### Entry gate

- `RP-0` の計測で CPU draw preparation / submission が frame budget の主要因になっている。
- `RP-2`～`RP-4` により、同期往復と毎 frame resource creation が先に除去されている。
- 対象 workload に、同一 mesh / pipeline / material contract の大量 item がある。
- backend capability と indirect args の alignment / lifetime contract が確認されている。

### Scope

- CPU / GPU 共通の immutable draw item contract。
- GPU-visible bounds、material / pipeline key、instance range。
- compute による visibility culling と visible item compaction。
- indirect args buffer の GPU-side generation。
- 同一 pipeline group の indirect draw / dispatch。
- generation ID、overflow counter、fallback reason の diagnostics。
- CPU submission path との frame 単位切替と結果比較。

### First candidates

1. Clone / instanced mesh
2. Particle / repeated primitive
3. 同一 mesh・material の 3D object group
4. 十分な item 数を持つ単純 sprite group

通常の少数 2D layer は、indirect args 生成と barrier の固定費が利益を上回る可能性があるため初期対象にしない。

### Safety rules

- GPU-generated item count と args buffer capacity に上限を持たせる。
- overflow 時は frame を壊さず CPU path または bounded fallback へ戻す。
- matte、adjustment、透明 blend order、feedback dependency を越えて並べ替えない。
- GPU culling の判定差を CPU reference と比較できる validation mode を持つ。
- indirect args buffer は UAV write から indirect read への resource state transition を明示する。
- device recovery、resize、frame slot 再利用時に未完了 args buffer を上書きしない。

### Exit criteria

- 対象 workload で CPU draw preparation / submission time が baseline より減る。
- draw call / API submission count が減り、GPU frame timeを悪化させない。
- CPU reference と visibility、draw order、instance count が一致する。
- unsupported backend、少数 item、overflow、validation failure では既存 path へ安全に戻る。
- 平面 1 枚などの小規模 scene では GPU-driven path を起動せず、固定費を加えない。

## 6. Delivery Order

1. `RP-0 Measurement and Automatic Evidence Log`
2. `RP-1 Sync Boundary Cleanup`
3. `RP-2 GPU-Resident Effect and Compositing Chain`
4. `RP-3 Persistent Layer Results and Partial Recompose`
5. `RP-4 Command and Resource Reuse`
6. `RP-7 Present Pacing`
7. `RP-5 Layer Batching and State Reduction`
8. `RP-6 Frame Overlap Without Editing Lag`
9. `RP-8 GPU-Driven Submission`

`RP-5`、`RP-6`、`RP-8` は、同期往復と不要な再合成を除去した後に行う。先に並列化や indirect submission を入れると bottleneck を隠し、編集 latency、barrier、resource hazard を増やす可能性がある。

## 7. Phase Gates and Rollback

各 work package は feature flag または既存 path への fallback を持ち、次の gate を満たすまで既定有効にしない。

- visual parity
- deterministic invalidation
- device recovery
- resize / swapchain recreate
- project switch / composition switch
- interactive property drag
- playback drop / repeat policy
- offline render correctness
- memory budget / resource lifetime

次の場合は、その work package を既定無効へ戻す。

- stale frame が最新の編集結果を上書きする。
- VRAM 使用量が bounded でない。
- full render と partial render の pixel parity が許容差を超える。
- device recovery 後に resource hazard が残る。
- average FPS は上がっても input-to-visible latency p95 が悪化する。

## 8. Test Scenes

再現性のある独立した test asset / composition を用意する。

1. 平面 1 枚、effect なし
2. 平面 20 枚、overlay 操作のみ
3. 静止画像 20 枚、transform drag
4. pointwise effect 5 段
5. blur / glow を含む spatial effect chain
6. adjustment layer 1 枚
7. H.264 video 1 枚、decode-bound 条件
8. PNG / JPEG image sequence
9. matte / non-Normal blend / mask 混在
10. 4K composition、RAM Preview と offline export

各 scene で baseline JSONL、最適化後 JSONL、画面差分、GPU validation 結果を保存する。

## 9. Boundaries

### In scope

- Artifact アプリ側 renderer boundary
- effect intermediate texture lifecycle
- layer / composition cache invalidation
- preview scheduling and pacing
- automatic performance evidence log

### Out of scope for the first pass

- `libs/DiligentEngine` fork の変更
- renderer backend 全面書き換え
- 全 scene / 全 layer の GPU-driven 化
- 全 effect の一括 GPU 化
- offline export の frame drop
- 見た目の一致を犠牲にした常時低品質 preview

Diligent fork 変更は、Artifact 側で call site、resource lifetime、wait reason を整理した後も backend 内部の待機が支配的であると GPU capture から確認された場合に、別 proposal として扱う。

## 10. Related Documents

- `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`
- `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`
- `Artifact/docs/HYPOTHESIS_RENDER_PASS_OPTIMIZATION_2026-03-28.md`
- `Artifact/docs/MILESTONE_DIAGNOSTIC_PHASE_2_2026-04-27.md`
- `Artifact/docs/planned/MILESTONE_MULTI_FRAME_PREVIEW_RENDERING_2026-06-29.md`
- `docs/planned/MILESTONE_GPU_DRIVEN_MDI_RENDER_2026-04-02.md`
- `docs/analysis/RENDER_PERF_HOTPATH_INVESTIGATION_2026-07-08.md`
- `docs/analysis/COMPOSITION_EFFECT_FORMAT_PATH_MEMO_2026-07-13.md`
- `docs/analysis/EFFECT_MAP_2026-07-16.md`

## 11. First Implementation Cut

最初の実装単位は `RP-0` と `RP-1` の一部に限定する。

- performance event schema と JSONL sink
- sync readback / async readback / upload / wait の reason code
- CPU fallback の rate-limited warning
- frame summary への readback / wait / upload 集計
- 平面 1 枚と adjustment layer の baseline 採取

この段階では描画結果を変更しない。証拠ログが安定した後、最初の GPU-resident 対象を pointwise effect chain または adjustment layer background から選ぶ。
