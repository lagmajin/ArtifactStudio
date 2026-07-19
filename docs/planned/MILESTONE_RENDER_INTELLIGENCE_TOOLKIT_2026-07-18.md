# Render Intelligence Toolkit

**ステータス:** In Progress

## 目的

ArtifactStudio の Render Graph / Temporal History 基盤を実描画へ接続し、制作機能と診断機能を同じ GPU 契約上で段階導入する。

対象は次の 5 項目とする。

1. Render Graph Inspector / GPU resource visualization
2. Jump Flood Algorithm (JFA) による 2D signed distance field と高度な mask primitive
3. Optical Flow の共通時間解析サービス化
4. Temporal History を使う preview-only denoiser
5. GPU Breadcrumbs / device-lost diagnostics

## 基本方針

- 先行する `ArtifactCore/docs/MILESTONE_RENDER_FOUNDATION_TRIAD_2026-07-18.md` を土台にし、別の Render Graph を新設しない。
- Composition / SceneNode と Diligent backend を直接再結合しない。
- Render Pass、resource、temporal history、diagnostic event は安定 ID で関連付ける。
- `QImage` / `QPainter` を本流の描画・合成・転送に導入しない。
- D3D12 / Vulkan / Diligent の低レベル変更は各 phase で最小化し、設計レビュー後に着手する。
- preview 品質改善と final render の意味論を分離する。Temporal Denoiser は既定で preview-only とする。
- Optical Flow、JFA、Denoiser は個別 widget に埋め込まず、Core の再利用可能な処理契約として公開する。

## 依存関係

```text
Render Graph Foundation
  +-- Diagnostics schema --------+--> Render Graph Inspector
  |                              +--> GPU Breadcrumbs
  +-- GPU resource execution ----+--> JFA SDF
  +-- Temporal History ----------+--> Optical Flow cache
                                 +--> Temporal Preview Denoiser

Optical Flow ------------------------> Temporal Preview Denoiser
```

## Phase 1: Render Graph Diagnostics Contract

### 進捗 (2026-07-18)

- [x] `Graphics.RenderGraph` に backend 非依存の diagnostic snapshot を追加
- [x] pass ID / resource ID / execution order / lifetime / estimated bytes を取得
- [x] scheduled / disabled / blocked の pass 状態を区別
- [x] `Frame.Debug` に JSON 変換を追加（64-bit ID / byte size は文字列で保持）
- [ ] Render Graph 実行結果の CPU / GPU timing と cache / invalidation reason を接続
- [x] 既存 Frame Pipeline View に snapshot 表示を接続
- [x] Composition の確定済み Frame Pass Plan から観測専用 capture producer を接続
- [ ] bounded capture ring を接続

### 目的

実 GPU 接続より先に、Render Graph が何を実行し、なぜ再実行・無効化したかを機械可読にする。

### Core 契約

- `RenderPassId` / `RenderResourceId` / `RenderExecutionId` の安定 ID
- pass type: graphics / compute / copy / present
- resource descriptor: kind / format / extent / usage / estimated bytes
- read / write edge と compiled execution order
- lifetime first-use / last-use
- culled / cached / executed / failed 状態
- temporal invalidation reason
- CPU setup time / GPU timestamp（取得可能な backend のみ）
- barrier / queue / fence の要約。ネイティブ handle は public Core API に露出しない

### 最初の成果物

1. 1 frame 分の immutable diagnostic snapshot
2. JSON dump
3. pass order / lifetime / invalidation reason のテキスト表示
4. snapshot サイズ上限とリング保持数

### Inspector UI

- pass list と依存 graph
- resource lifetime timeline
- pass 選択時の input / output / cache / timing 詳細
- resource name / pass name / type による filter
- capture は明示操作または開発設定時のみ。通常再生時は無効

### 完了条件

- CPU-only graph fixture を snapshot 化できる
- read-before-write / cycle / missing producer を表示できる
- 1 frame capture が通常描画の所有権を変更しない
- diagnostics 無効時に snapshot 用 allocation を継続しない

## Diligent Sample Adoption Workstream

### DSA-1 GPU Query Ring / Pass Timing

進捗 (2026-07-18):

- [x] frame duration query を 2-slot から 3-slot へ拡張
- [x] submitted slot を追跡し、未完了queryの再利用を防止
- [x] `GetData()` 未完了時は待機せず、計測を安全にスキップ
- [x] timing availability / sample execution ID をFrame Debugへ公開
- [ ] scheduled pass単位のduration query pool

参照: `DiligentSamples/Tutorials/Tutorial18_Queries`

現状、`ArtifactIRenderer` には frame duration query の 2-slot ring と
`lastFrameGpuTimeMs()` が存在する。これを置換せず次の順で拡張する。

1. query capability を確認し、非対応時は CPU timing のみ維持
2. frame query ring を in-flight frame 数に耐える深さへ拡張
3. Render Graph の scheduled pass に duration query slot を割り当てる
4. `GetData()` が未完了なら待たず、前回値と age を保持
5. GPU duration / availability / sample frame を diagnostic snapshot へ格納
6. Pipeline View に CPU / GPU timing を別列で表示

Query結果取得のために `Flush()` / fence wait を追加しない。timestamp query 自体が
queue overlapへ影響する可能性があるため、Pass単位計測は明示capture中だけ有効にする。

### DSA-2 Render State Cache

参照: `DiligentSamples/Tutorials/Tutorial26_StateCache`

1. device type / adapter / driver / build / shader revision を含む cache key
2. cache file のatomic保存と破損時fallback
3. JFA / Temporal Denoiser の新規PSOだけで限定試行
4. cache hit / miss / rejected reason をDiagnosticsへ公開
5. 開発buildのみhot reloadを有効化
6. 安定後に既存 `ShaderManager` のPSOを段階移行

Tutorial25のoffline State Packagerは本workstreamの対象外とし、runtime cacheの安定後に
別途判断する。

### DSA-3 Command Queues

参照: `DiligentSamples/Tutorials/Tutorial23_CommandQueues`

1. adapter queue capability とqueue数を診断snapshotへ公開
2. graphics / compute / transfer contextを capability adapter で保持
3. Render Graph compilerにqueue assignment / cross-queue dependencyを追加
4. general fence valueをexecution単位で管理
5. JFAを最初のasync compute候補とする
6. upload / readbackをtransfer queue候補として計測

複数queueでは自動resource transitionへ依存せず、resourceの
`ImmediateContextMask`、owner queue、release/acquire stateを明示する。単一queue fallbackを
常に維持し、Queriesで実測上の改善がないdeviceではasync経路を無効化する。

### 導入順

1. DSA-1 GPU Query Ring / Pass Timing
2. DSA-2 Render State Cache（新規PSO限定）
3. DSA-3 Command Queues（JFA限定）

## Phase 2: JFA Mask Distance Field

### 目的

2D alpha mask から GPU 上で signed distance field を生成し、複数の制作機能で共有する。

### Pass 構成

1. seed initialization
2. jump passes (`2^n ... 1`)
3. signed distance resolve
4. optional normalization / crop

各 pass は Render Graph の compute pass として登録し、中間 texture は transient resource とする。

### 公開 primitive

- expand / contract
- constant-width feather
- inner / outer stroke
- inner / outer glow 用 distance input
- edge-distance map output

### 品質と fallback

- alpha threshold と inside / outside の定義を明示する
- ROI に safety margin を加え、広い feather で境界が切れないようにする
- CPU reference は正しさ確認用とし、hot path にしない
- 非対応 backend では既存 mask path を維持し、暗黙の GPU/CPU 往復を行わない

### 完了条件

- 非正方形、透明画像、全面不透明、細線、ROI端で結果が定義される
- 同じ SDF から expand / feather / stroke を生成できる
- Inspector から jump pass と transient lifetime を確認できる

## Phase 3: Shared Optical Flow Service

### 目的

用途ごとに Optical Flow を重複計算せず、frame pair に対する flow / confidence / scene-cut を共通資産として提供する。

### 入力キー

- source / composition stable ID
- previous / current time
- input resolution / ROI
- quality tier
- color transfer / luminance interpretation
- algorithm revision

### 出力

- forward flow
- backward flow（要求時）
- confidence / consistency
- occlusion or invalid region
- scene-cut flag

### 利用先

- motion tracking
- pixel-motion frame blending
- temporal denoiser
- motion blur
- stabilization
- mask / roto propagation
- vector-flow effects

### 実装順

1. backend-neutral request / result / cache key
2. 既存 OpenCV tracking backend adapter
3. GPU backend capability と dispatch adapter
4. forward-backward consistency / scene-cut
5. consumer migration。個別機能の旧経路は一度に撤去しない

### 完了条件

- 同じ frame pair / quality / ROI の要求がキャッシュ共有される
- seek、camera cut、source change で history が無効化される
- consumer が backend 固有画像型を参照しない
- confidence が低い領域を consumer 側で判別できる

## Phase 4: Temporal Preview Denoiser

### 目的

低品質 preview のちらつきとノイズを、時間履歴と分散推定で軽減する。final render の既定結果は変更しない。

### Pass 構成

1. history reprojection
2. disocclusion / confidence rejection
3. temporal accumulation
4. luminance moments / variance update
5. edge-aware spatial filter
6. history store

### 適用候補

- pyro / volume preview
- stochastic particle preview
- low-sample blur / depth-of-field preview
- temporal AI analysis preview

### 安全条件

- seek、time discontinuity、scene cut、resolution、ROI、quality、effect parameter change で明示無効化
- flow がない consumer は motion-vectorなしの保守的 accumulation を選択可能
- ghosting debug view と history rejection view を Inspector に提供
- export / final render への適用は別途明示設定と設計レビューを必要とする

### 完了条件

- 静止領域でノイズが低下する
- scene cut 後に旧履歴を混ぜない
- 動領域の rejection 状態を可視化できる
- denoiser 無効時に既存 preview の画素結果を変更しない

## Phase 5: GPU Breadcrumbs / Device-Lost Diagnostics

### 目的

GPU hang / device lost 発生時に、最後に開始・完了した Render Graph pass と関連資産を復元可能な形で記録する。

### 記録内容

- execution / frame / composition ID
- pass ID / pass name / queue
- begin / end marker
- shader / PSO の安定識別子と revision
- resource ID の要約。画像内容やユーザーデータは保存しない
- backend / adapter / driver / feature capability
- 直前の temporal invalidation reason

### 方針

- Render Graph pass boundary から自動挿入し、各 effect が独自 marker を実装しない
- 固定長リングと事前確保領域を使い、障害時の追加 allocation に依存しない
- D3D12 DRED、Vulkan device fault 等は capability adapter として隔離する
- 通常ログと crash artifact は同じ ID schema を使う
- release build では名前・保持量・有効化条件を制限可能にする

### 完了条件

- synthetic failure で最後の incomplete pass を識別できる
- backend extension 非対応でも共通 breadcrumb は残る
- crash report が resource 所有権を延長しない
- Inspector snapshot と crash artifact の pass ID が一致する

## 実装順とゲート

| 順序 | Work Package | 着手ゲート |
|---:|---|---|
| 1 | Diagnostics snapshot / JSON | Render Foundation Triad の ID / pass descriptor を再利用できること |
| 2 | Render Graph の Diligent 接続 | resource ownership / barrier 方針の設計レビュー |
| 3 | Inspector 最小 UI | snapshot が backend 非依存で安定していること |
| 4 | JFA seed / jump / resolve | compute pass と transient texture が実行可能であること |
| 5 | Optical Flow request / cache | Temporal History key と source identity が確定していること |
| 6 | Temporal Preview Denoiser | flow confidence / invalidation が利用可能であること |
| 7 | GPU Breadcrumbs backend adapters | pass ID と marker lifetime の設計レビュー |

## 非対象

- FSR Frame Generation swapchain の導入
- final render への自動 temporal denoise
- Render Graphとは別系統の effect graph / node editor
- DiligentEngine fork の変更
- Qt composition fallback の拡張
- AIによる flow / mask 生成の追加

## 検証計画

ビルド・テスト・CMake実行はユーザー承認後に行う。承認時は phase ごとに以下を分離して確認する。

1. Core contract / graph fixture
2. Render Graph compilation / validation
3. D3D12 GPU execution
4. Vulkan parity
5. visual reference image comparison
6. seek / cut / resize / device-lost fault injection

## 参照

- `ArtifactCore/docs/MILESTONE_RENDER_FOUNDATION_TRIAD_2026-07-18.md`
- `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` (`C-RND-2`)
- `docs/planned/DILIGENT_RENDER_EXTENSION_REPORT_2026-06-17.md`
- `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`
- `docs/planned/MILESTONE_APP_INTERNAL_DEBUGGER_2026-04-17.md`
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_PHYSICAL_MOTION_BLUR_2026-06-07.md`
- `docs/planned/MILESTONE_TEMPORAL_EFFECT_HOST_FOR_TIME_DISPLACEMENT_2026-07-01.md`
