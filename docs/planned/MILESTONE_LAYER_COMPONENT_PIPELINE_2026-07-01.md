# Layer Component Pipeline / Simulation Contract (2026-07-01)

**ステータス:** In Progress

レイヤーコンポーネントの `cloner / layout / crowd / physics / fracture / emit / simulation` を、
その場しのぎで足すのではなく、将来の本格シミュレーションまで見据えて矛盾なく連携させるための実装指針。

## Goal

- クローン生成、配置、群集、物理、破砕、放出が同じ順序規約で評価される
- `見た目を即座に返す軽量プレビュー経路` と `状態を持つシミュレーション経路` を分離する
- 既存の `Artifact.Layer.Component.System` と `Artifact.Layer.CloneEffectSupport` を出発点にする
- 将来 `Pyro / soft-body / rigid / crowd / layout solver / particle emission` を追加しても、責務の衝突を起こしにくくする

## Current Snapshot

2026-07-01 時点で、すでに以下の土台がある。

- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`
  - `LayerComponentPhase`
  - `LayerComponentScope`
  - `LayerEvaluationState`
  - `LayerEvaluationContext`
  - builtin descriptor (`cloner / layout / crowd / motion-dynamics / collision / fracture / particle-emitter`)
- `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
  - `cloneRenderInstances()`
  - `applyLayoutComponent()`
  - `applyCrowdComponent()`
  - `applyInstanceCollisionComponent()`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
  - builtin component descriptor 同期
  - layer property 側とのブリッジ

ただし現状は、`cloneRenderInstances()` の中で
`generate -> arrange -> crowd-ish motion -> collision correction`
を即時に順適用している簡易モデルであり、以下が未分離。

- 一時的な見た目補正
- フレームをまたぐ持続状態
- composition 全体で共有される衝突世界
- fracture / emit の event 消費先
- deterministic cache / rollback / bake の前提

## Core Rule

今後は「全部が transform を直接上書きする」方式を避け、以下の 3 層を明確に分ける。

1. `Authoring Layer`
   - ユーザーが編集する設定値
   - component descriptor と settings
   - キーフレームや preset の入力面

2. `Evaluation Layer`
   - 現フレームで評価された instance 群、intent 群、contact/event 群
   - `LayerEvaluationState` が受け皿
   - ここでは `何をしたいか` と `何が起きたか` を集約する

3. `Simulation Layer`
   - fixed timestep で進む持続状態
   - crowd / rigid / soft-body / pyro / particle の各 solver
   - snapshot / reset / deterministic seed / bake 対応の主体

## Canonical Phase Order

`LayerComponentPhase` は今の並びを基本維持し、意味を次のように固定する。

1. `Source`
   - 元レイヤー、元ジオメトリ、元サンプル、source bounds を確定する

2. `Drive`
   - layer 単位の follow-through / lag / spring など、クローン生成前の駆動を入れる
   - AE ライクな animation physics はまずここ

3. `Generate`
   - cloner、scatter、array、text-to-instance など、instance 集合を作る

4. `Arrange`
   - grid / radial / stack / surface-fit / path-fit など、静的配置を決める
   - layout は solver ではなく「初期整列責務」を優先する

5. `Intent`
   - crowd / steering / goal / avoidance desire を生成する
   - ここではまだ transform を確定させず、`LayerMotionIntent` を主に出す

6. `Dynamics`
   - collision / rigid / soft-body / cloth / particle / pyro など、状態を持つ solver が intent を消費して結果を返す
   - transform の最終更新はここが正規

7. `Topology`
   - fracture / split / spawn-child-instance / topology mutation
   - Dynamics の結果として起きた damage や contact を消費する

8. `Emit`
   - particle / decal / secondary pyro / debris を放出する
   - Topology や contact に依存してよい

9. `RenderExtraction`
   - renderer が使う instance data / debug data / overlay data を確定する

## Non-Contradiction Rules

### 1. Layout は Dynamics の代わりをしない

- `layout` は「配置の初期条件」を決める
- 押し戻し、衝突、床判定、速度積分は `dynamics` 側
- `layout` が毎フレーム collision 解決まで担うと、本格 physics 導入時に責務が重複する

### 2. Crowd は transform を直書きしない方向へ寄せる

- 軽量 preview では仮に transform 補正してもよい
- 正規経路では `LayerMotionIntent` を生成する責務へ寄せる
- `crowd = desire`、`physics = resolve` を固定する

### 3. Collision は composition scope を守る

- `collision` は instance 単体ではなく composition/world の責務
- 他レイヤーや他 emitter と接続される将来を考え、`LayerComponentScope::Composition` を維持する
- layer ローカルの押し戻しは preview fallback に限定する

### 4. Fracture と Emit は event 消費型にする

- fracture は毎回の transform パスで直接 shard を増やさない
- `LayerFractureEvent` を受けて topology mutation する
- particle / debris / smoke は `LayerParticleSpawnEvent` と将来の pyro event へ寄せる

### 5. Preview fallback と authoritative sim を分ける

- editor 停止中の軽量プレビューは `ArtifactCloneEffectSupport.ixx` の簡易経路を維持してよい
- ただし再生中、bake 中、cache 検証中は authoritative simulation snapshot を優先する
- 同じ component setting でも、preview fallback は近似、simulation は正規値、という二層を許容する

## Recommended Architecture

### A. `LayerComponentHost` は registry のまま保つ

`LayerComponentHost` 自体には solver 実装を持たせず、

- descriptor 保持
- phase/scope/dependency validation
- json serialize

までに留める。

ここに runtime state を混ぜると、undo/redo と playback state が絡みやすい。

### B. evaluator を 2 系統持つ

1. `ImmediatePreviewEvaluator`
   - 現在の `cloneRenderInstances()` を発展させた軽量経路
   - 停止中 UI、scrub、中間編集向け

2. `SimulationBackedEvaluator`
   - fixed timestep の solver state を読む正規経路
   - playback、RAM preview、render queue、bake 向け

両者は descriptor と phase order を共有し、内部 state の有無だけを分ける。

### C. state の所有者を composition 側へ寄せる

将来の crowd / collision / pyro は複数 layer をまたぐため、
state owner は layer 単体ではなく composition/session 側に寄せる。

候補:

- `CompositionSimulationContext`
- `LayerSimulationWorld`
- `ComponentEvaluationSession`

最低限ここが持つべきもの:

- frame / time / fixed dt
- deterministic seed
- per-layer instance map
- composition-wide collision world
- solver snapshot cache
- event queues (`contact / fracture / spawn / pyro`)

### D. entity identity を永続寄りに扱う

`SimulationEntityId` は今後かなり重要になる。

- `ownerLayerId`
- `componentId`
- `localId`
- `generation`

を使って、

- clone index 変動
- fracture による split
- emitter による spawn/despawn
- cache restore

に耐えられる identity へ寄せる。

特に `localId = visible order` 依存は避ける。

## Implementation Phases

### Phase 1: Contract stabilization

- `LayerComponentPhase` の意味をこの文書どおり固定
- builtin component の dependency を明示
- validation を強化

優先 dependency 例:

- `layout` requires `cloner`
- `crowd` requires `cloner`
- `collision` requires `cloner`
- `fracture` requires `collision` or future dynamics result producer
- `particle-emitter` may require `cloner` or `fracture`

### Phase 2: intent/result separation

- `applyCrowdComponent()` を最終的に `intent` 生成寄りへ移す
- `applyInstanceCollisionComponent()` は preview-only correction と明示する
- `LayerEvaluationState` を `instances / intents / contacts / events` の正規バスとして使い始める

### Phase 3: composition simulation session

- layer 単体でなく composition 単位の simulation session を導入
- playback 時は fixed timestep update をここで進める
- crowd / rigid / soft-body / particle / pyro の土台をここへ集約

2026-07-14 static implementation:

- `ArtifactAbstractComposition` がframe単位のcomponent simulation sessionを所有
- crowdは`LayerMotionIntent`と継続velocityからauthoritative transformを生成
- clone collisionは同一layer内だけでなくcomposition内の対象instance間で解決
- layer-localの即時crowd/collisionは、composition sessionがない単独previewだけのfallbackへ縮小
- seek時はsessionを再初期化し、連続frameだけfixed-step状態を継承
- build / runtime verificationは未実施

### Phase 4: event-driven topology / emit

- fracture を contact / impulse / damage 由来 event に寄せる
- emit を contact / fracture / density / trigger event 起点に統一
- secondary systems を「前段の結果を読む consumer」として追加可能にする

2026-07-14 static implementation:

- composition session が新規 contact を前フレームとの差分で抽出
- `fracture.enabled` は impulse / sensitivity / threshold から `LayerFractureEvent` を生成
- `component.particleEmitter.enabled` は contact 起点の `LayerParticleSpawnEvent` を生成
- spawn seed は layer / frame / local entity id から決定論的に生成
- `setAuthoritativeComponentEvaluationState()` がfracture queueを既存topology mutationへ接続
- particle spawn queueはevent位置・速度・seedを使って既存component particle bufferへ接続
- 旧composition collision passからの直接fracture発火を撤去し、二重発火を防止
- 非連続seekではshard / component particle stateをresetし、旧frameのtopologyを重ねない

### Phase 5: bake / cache / deterministic replay

- component stack 全体の snapshot 化
- render queue と editor preview の再現性を合わせる
- RAM preview / offline render / future network render で同じ simulation result を共有できるようにする

2026-07-14 static implementation:

- composition sessionに最大120 frame / 64 MiBのbounded snapshot cacheを追加
- 次frame評価開始時に、描画で進んだshard motion / component particle ageを含む直前frameを確定保存
- exact-frame seekはinstance/intents/contactsとlayer runtimeを同時復元し、event queueは再消費しない
- same-frame authoring再評価ではcacheを破棄し、古いdescriptor/property stateの再利用を防止
- Render QueueのCPU/GPU両経路を同じcomposition evaluatorへ接続
- component simulation使用jobはqueue clone内でcomposition開始frameからwarm-upし、frame順を維持してsub-rangeもfull-rangeと同じ履歴を再生
- stateful jobだけMFRを抑止し、stateless jobの並列レンダーは維持
- layer runtime snapshotにversioned JSON codecを追加し、fracture shard / component particle / last-frame値を欠落なく往復可能にした
- malformed bakeによる過大確保を避けるためshard / particle countに読込上限を設定
- composition単位のversioned frame bundleにinstance / intent / contact / runtimeを保存
- composition ID、frame rate、layer authoring hashを検証し、古いauthoring stateのbakeを拒否
- composition JSONにもframe rateを保存し、queue cloneでfixed-delta契約を維持
- editor sessionのbounded cacheをqueue cloneへ直接移送し、start直前frameがあればwarm-upを省略
- external renderer manifestにも同じbake bundleを格納
- project保存と同時に`<project>.component-bake.json`へatomic sidecar保存し、同期／非同期loadで検証後に復元
- bakeが空になった場合は古いsidecarを削除し、stale cacheの誤復元を防止
- external renderer受信側でversion / composition / fps / hash / frame payloadを検証し、summaryとrenderStarted eventへ受理状態を記録
- external rendererは現状Artifact本体をlinkしないdiagnostic rendererのため、実描画へのstate適用は将来のrenderer統合時に接続
- Composition Settingsに`Bake Simulation Cache` / `Clear Bake`を追加
- 新しいsignal/slot接続は増やさず、専用buttonのdialog result経路から既存composition/project serviceを呼び出す
- Bakeはcomposition全範囲をfixed-step事前評価し、最大119 checkpoint + current frameを範囲全体へ均等配置
- modal progressをpolling更新し、Cancel時は開始前のsession / layer runtime / frame位置を復元
- Render Queueは開始直前の完全一致がなくても、直前checkpointから不足frameだけ決定論的に再生
- sidecar詳細管理UIは未実装

## Immediate Next Steps

次に実装するなら、順番は以下が安全。

1. `ArtifactLayerComponentSystem.ixx`
   - builtin dependency を追加
   - validation message を少し厚くする

2. `ArtifactCloneEffectSupport.ixx`
   - crowd/collision の役割を `preview fallback` とコメント/関数名で明確化
   - `intent` 相当の中間表現を入れられる余地を作る

3. composition 側
   - playback 中に使う simulation session の最小 owner を追加
   - まず crowd + collision だけ authoritative path へ移す

## Guardrails

- `Drive` と `Dynamics` を混ぜない
- `Arrange` で衝突解決しない
- `Intent` で最終 transform を確定しない
- `Topology` で renderer 直結の描画ロジックを増やさない
- preview fallback を authoritative solver の代替にしない
- component 追加時は `phase / scope / dependency / state owner / deterministic behavior` を必ずセットで決める

## Related Files

- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`
- `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `ArtifactCore/include/Core/SimulationSystem.ixx`
- `docs/planned/IMPLEMENTATION_ANIMATION_PHYSICS_2026-03-22.md`
- `docs/planned/EDITOR_RENDER_RULES.md`
