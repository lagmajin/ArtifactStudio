**最終更新:** 2026-09-02
**ステータス:** Not Started

# Behavior Particle Layer Milestone (2026-09-02)

## 目的

目標・近傍・障害物を読み、群れ・追跡・回避といった局所的な判断で動く粒子を、既存の演出用
`ArtifactParticleLayer` から分離して提供する。ユーザー向け名称は **Behavior Particle Layer** とする。

通常 Particle は大量・軽量な emitter / lifetime / effector の演出を担当し続ける。Behavior Particle は
少数から中規模の agent を対象に、明示的な状態、行動の重み、近傍検索、目標参照を持つ。必要なら
通常 Particle を agent の軌跡、尾、煙、発光の見た目として追従生成するが、両者のシミュレーションを
同一のデータモデルにはしない。

## 現状と根拠

- `ArtifactParticleLayer` は2Dの emitter / lifetime simulation を持つ通常 Particle であり、
  `ParticleSystem::goToFrame()` と `captureRenderData()` により CPU simulation の結果を既存GPU drawへ渡す。
- `ArtifactParticle3DLayer` は通常 Particle の simulation / renderer を共有しつつ、保存形式と3D camera/depth
  のlayer identityを分けている。2D/3Dの分離は既に正規方針である。
- `ArtifactCore::ParticleRenderData` は renderer 用 snapshot であり、位置・速度・色・size を渡せる。
  行動設定やagent状態を renderer に持ち込まず、Behavior Layer側で変換する境界にできる。
- `Graphics.ParticleCompute` には compute shader の更新基盤があるが、通常 Particle の既定 simulation loop には
  接続されていない。このマイルストーンの初期段階では、GPU simulation化を前提にしない。

## 非目標

- 既存 `ArtifactParticleLayer`、`ParticleSystem`、既存プリセットの置換または意味変更
- 数十万agent向けの全粒子GPU AI、NavMesh、global pathfinding、機械学習推論
- Fluid / Collision component / ReactiveEvents との統合
- Diligent / DX12 backend、既存 ParticleRenderer の広域改修
- 通常Property Widgetへコンポーネント由来グループを露出すること

## 正規アーキテクチャ

```text
BehaviorParticleSettings + BehaviorTargetSet
                 |
                 v
BehaviorParticleSimulation
  AgentState + spatial-hash near-neighbor query + state machine
                 |
                 v
BehaviorParticleRenderSnapshot
                 |
                 v
ArtifactCore::ParticleRenderData -> ArtifactIRenderer::drawParticles()
                 |
                 +-> optional VisualFollowerEmitter -> ArtifactParticleLayer
```

| 領域 | 責務 |
|---|---|
| `BehaviorParticleLayer` | authoring state、保存、frame evaluation、target解決、render snapshot変換 |
| `BehaviorParticleSimulation` | agent状態、固定step、行動合成、近傍検索、決定論的更新 |
| `BehaviorParticleBehavior` | Seek / Arrive / Separation / Alignment / Cohesion / Avoid の純粋な加速度計算 |
| `ArtifactCore::ParticleRenderData` | 描画用の位置・速度・色・sizeだけを受け取る既存境界 |
| `ArtifactParticleLayer` | agentに追従する視覚表現のみ。agent stateの所有者にならない |

初期実装は `ArtifactBehaviorParticleLayer` と専用の simulation data を新設する。公開型・保存識別子も
`LayerType::BehaviorParticle` として独立させ、`LayerType::Particle` / `Particle3D` へのmode toggleは作らない。
新規モジュールが必要になった場合はCMakeの明示登録とmodule hygiene確認を同じ変更単位で行う。

## データ契約

### Agent runtime state

- stable `agentId`、固定seed、position、velocity、acceleration、age、lifetime
- `BehaviorState`: `Seek` / `Arrive` / `Flock` / `Avoid` / `Idle`（初期セット）
- state elapsed time、最大速度、最大操舵力、半径、render scale
- 設定由来のtarget index。生ポインタやrenderer resourceを保持しない。

### Authoring state

- agent count、seed、fixed step、max substeps、world/local space
- target layer / explicit point、arrival radius、seek / arrive weight
- neighbor radius、separation / alignment / cohesion weight
- obstacle set、avoid radius / look-ahead / weight
- speed / steering / boundary policy、color / size / billboard render設定

同じauthoring state、固定seed、composition frameからは同じ結果を再現する。seekやtimeline scrubは
任意時刻のmutable stateを使い回さず、初期状態から固定stepで再評価するか、明示的なcheckpoint cacheを
使用する。cacheが未確定の段階では正しさを優先する。

## Phase 0: 境界と受入れ基盤

- `LayerType::BehaviorParticle`、factory、menu、JSON識別子、timeline/icon識別を追加する設計を確定する。
- 既存 `Particle` / `Particle3D` / `FormParticle` と作成名・保存名・描画責務が衝突しないことを確認する。
- target参照は安定Layer IDを保存し、欠落targetは `Idle` と診断しクラッシュや暗黙の別target選択をしない。
- 既存のイベント／command／Undo経路を調査して再利用し、新しいsignal-slot接続を追加しない。

Done:

- 新しいレイヤーの保存・作成・target欠落時の契約が文書化される。
- 通常 Particle への互換性影響がないことを静的に確認する。

## Phase 1: 最小agent simulation

- dedicated agent array と fixed-step integrator を実装する。
- explicit point とtarget layer transformへの **Seek**、減速する **Arrive**、速度・操舵力上限を実装する。
- `ArtifactCore::ParticleRenderData`へsnapshot変換し、既存 `drawParticles()` で表示する。
- 専用Property surfaceは Quick Setup / Target / Motion / Appearance / Simulation のみとする。
- 初期preset: `Target Seek`、`Soft Arrival`。

Done:

- pointまたはlayer targetを移動するとagentが追従する。
- fixed seed、保存／再読込、frame seekで結果が再現する。
- targetを削除してもlayer全体は表示不能にならず、明示的にIdleとなる。

## Phase 2: flocking と空間分割

- Uniform grid / spatial hashで近傍候補を取得し、全agent総当たりを避ける。
- Separation、Alignment、Cohesionを独立weightで合成する。
- neighbor半径、最大neighbor数、agent上限をauthoring設定として制限する。
- debug overlay / diagnosticsにagent数、neighbor query数、substep数、target解決状態を出す。

Done:

- 512〜2,048 agentで群れが分離・整列・結合する。
- 同一設定で再生とseekが視覚的にも数値的にも安定する。
- debug表示は通常のtimeline左ペインに常時バッジを増やさない。

## Phase 3: 障害物回避と状態遷移

- 初期obstacleは明示point / circle / layer boundsのみとし、NavMeshやmesh衝突へ拡張しない。
- 進行方向のlook-aheadによるAvoid steeringを実装し、Seek / Arrive / Avoidを優先度またはweightで合成する。
- state machineは `Seek -> Arrive -> Idle` と `* -> Avoid -> previous` の小さい遷移セットに限定する。
- state切替条件、target欠落、agent上限をdiagnosticsで識別可能にする。

Done:

- agentが目的地へ向かい、単純障害物を迂回し、到達後に安定する。
- 回避不能な状況でもNaN、無限速度、フレームごとの状態振動を起こさない。

## Phase 4: 通常Particleによる視覚フォロワー

- Behavior agentをsimulationの親、通常Particleを見た目の子として一方向に接続する。
- trail / smoke / sparkはagentのposition・velocity・stateを読むだけで、通常Particleからagentへ力や状態を返さない。
- agentの寿命、emission数、visual followerの最大数を独立に制限する。

Done:

- agent群の挙動を変えずに通常Particleのtrailを付け外しできる。
- followerの有無でagent simulation結果が変化しない。

## Phase 5: GPU simulationの適格性評価（任意・後続）

Phase 1〜4のCPU側の意味・再現性・受入シナリオが確立した後にのみ検討する。
GPU化の対象は、固定サイズagent state、spatial hash、neighbor gather、steering integrationであり、
target解決・保存・Undo・UI責務はApp側に残す。`Graphics.ParticleCompute` の既存APIをそのまま
agent computeへ流用できるとは仮定しない。CPU/GPU結果差、readbackなしのdiagnostics、容量制限を
個別に設計し、別実装スライスとして承認する。

## 品質ゲート

### Functional

- 通常 Particle / Particle3D / FormParticle の既存JSONを読み込んでも意味が変わらない。
- Behavior Particle はtarget追従、群れ、回避を個別にon/offできる。
- save / reload / duplicate / undo / redo後にtarget参照とseedが維持される。

### Determinism

- 同じseed、composition frame、settingsで同じagent snapshotになる。
- pause / resume / seekの順序により結果が変わらない。
- fixed-stepのsubstep上限超過は明示的にdiagnosticsへ記録し、時刻を暗黙に飛ばさない。

### Performance

- Phase 1の初期受入: 256 agent、60fpsのinteractive previewを目標とする。
- Phase 2の目標: 1,024 agent、近傍検索がO(n²)に戻らないことをprofileで確認する。
- GPU移行は上記が満たせない場合の前提ではなく、CPU実装の意味が確定してから判断する。

### Safety

- 新規QtCSS、`QColorDialog`、`QImage` hot path、新規signal-slot、ReactiveEventsへの変更を行わない。
- Diligent / D3D12 backendは最小の既存draw入口再利用に留める。
- ビルド、CMake、runtime testは明示許可後にのみ実行する。

## 実装順序

1. Phase 0のlayer identity、保存、target参照、UI責務を確定する。
2. Phase 1のSeek / Arrive + deterministic snapshotを実装し、最小受入を行う。
3. Phase 2でspatial hashとBoidsを加える。
4. Phase 3で単純障害物と限定状態機械を追加する。
5. Phase 4で通常Particleの視覚フォロワーを追加する。
6. GPU化はPhase 5として別途判断する。

## 関連文書

- [Particle 2D Particular-style Workflow](MILESTONE_PARTICLE_2D_PARTICULAR_WORKFLOW_2026-08-30.md)
- [GPU Particle System](MILESTONE_GPU_PARTICLE_SYSTEM_2026-04-19.md)
- [VFX Particle & Fluid](MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md)
- [3D Particle completion milestone](../../Artifact/docs/MILESTONE_3D_PARTICLE_2026-08-29.md)
- [Form Grid Particle Layer](MILESTONE_FORM_GRID_PARTICLE_LAYER_2026-06-26.md)
