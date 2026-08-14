# レイヤーコンポーネント パイプライン整合性検証

**最終更新:** 2026-08-13

レイヤーコンポーネントシステムの「宣言（descriptor）」「登録」「評価実行」「プロパティ露出」の 4 層が実際に一致しているかを、ソースコード（`.ixx` / `.cppm`）を一次情報として検証した結果。

## 検証観点

1. 宣言と登録（descriptor / phase / scope / ID 整合性）
2. 評価実行パイプライン（phase 順の実行、各コンポーネントの接続状態）
3. プロパティ露出と評価の整合性（パス一致、孤児プロパティ、未露出参照）

## 対象ファイル

- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm`
- `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
- `Artifact/src/Generator/` / `Artifact/src/Physics/`
- `ArtifactCore/src/Physics/` / `ArtifactCore/src/Particle/` / `ArtifactCore/src/Crowd/`

---

## 1. 宣言と登録

### フェーズ定義（`ArtifactLayerComponentSystem.ixx:22-32`）

enum の並び順が暗黙のパイプライン順。

| 順序 | enum | 名前文字列 |
|---|---|---|
| 0 | `Source` | `"source"` |
| 1 | `Drive` | `"drive"` |
| 2 | `Generate` | `"generate"` |
| 3 | `Arrange` | `"arrange"` |
| 4 | `Intent` | `"intent"` |
| 5 | `Dynamics` | `"dynamics"` |
| 6 | `Topology` | `"topology"` |
| 7 | `Emit` | `"emit"` |
| 8 | `RenderExtraction` | `"render"` |

注意: `RenderExtraction` だけ文字列が `"render"` で、enum 名と非対称。

### scope（`ArtifactLayerComponentSystem.ixx:34-38`）

| enum | 意味 |
|---|---|
| `Layer` | レイヤー単体に適用 |
| `InstanceSet` | クローン等で生成されたインスタンス集合単位 |
| `Composition` | コンポジション全体（複数レイヤー横断） |

### builtin コンポーネント ID 一覧

全て descriptor factory（`make*ComponentDescriptor`）に定義され、`ArtifactAbstractLayer.cppm:1013-1127` の `syncBuiltinComponentDescriptors()` で登録される。

| componentId | typeId | phase | scope | order | requiredTypeIds |
|---|---|---|---|---|---|
| `builtin.cloner` | `artifact.component.cloner` | Generate | InstanceSet | 100 | — |
| `builtin.layout` | `artifact.component.layout` | Arrange | InstanceSet | 200 | `artifact.component.cloner` |
| `builtin.crowd` | `artifact.component.crowd` | Intent | InstanceSet | 300 | `artifact.component.cloner` |
| `builtin.motion-dynamics` | `artifact.component.motion-dynamics` | Drive | Layer | 400 | — |
| `builtin.sequence-player` | `artifact.component.sequence-player` | Drive | InstanceSet | 350 | — |
| `builtin.collision` | `artifact.component.collision` | Dynamics | Composition | 500 | `artifact.component.cloner` |
| `builtin.fracture` | `artifact.component.fracture` | Topology | InstanceSet | 600 | — |
| `builtin.particle-emitter` | `artifact.component.particle-emitter` | Emit | InstanceSet | 700 | `artifact.component.cloner` |
| `builtin.fluid` | `artifact.component.fluid` | Dynamics | Composition | 750 | — |

### 宣言層の整合性判定: 良好

- componentId / typeId は 9 件とも重複・typo なし。
- `requiredTypeIds` の参照先 `artifact.component.cloner` も正確。
- `validate()` は componentId 重複と依存フェーズ検査（`ArtifactLayerComponentSystem.ixx:497-539`）を持つ。

### 宣言層の不整合・注意点

1. **命名規約の乖離**: typeId は kebab-case（`particle-emitter`, `motion-dynamics`）だが、実プロパティパスは camelCase（`component.particleEmitter.enabled`, `motion.enabled`）。`fracture` は descriptor が `builtin.fracture` なのに実プロパティは `fracture.enabled`（`component.fracture.*` ではない）。
2. **`script` コンポーネントが descriptor 未登録**: プロパティ `component.script.enabled`（`:6741`）とメンバ `scriptComponentEnabled_`（`:827`）は存在するが、`makeScriptComponentDescriptor` 相当の factory が無い。
3. **order と phase の直感不一致**: `motion-dynamics`（Drive）が order=400、`sequence-player`（Drive）が order=350 で、Intent(300) より後、Dynamics(500) より前。同一 phase 内でのみ比較されるため機能バグではないが、グローバル実行順として誤解を招く。

### 登録タイミング

`syncBuiltinComponentDescriptors()` は「コンストラクタ即時登録」と「アクセス/検証/保存/復元時の遅延再同期」の併用。

- コンストラクタ: `ArtifactAbstractLayer.cppm:1004-1009`
- 遅延再同期: `layerComponents()` / `enabledLayerComponents()` / `layerGenerators()` / `validateLayerComponents()` / `toJson` / `fromJson`

注意: `fromJson` は `componentGraph` 読込直後に `syncBuiltinComponentDescriptors()` を呼ぶため、保存されていた builtin descriptor の enabled/order/settings は読み込み直後に現在のプロパティ値で上書きされる。**builtin descriptor の永続化は実質無効**。

---

## 2. 評価実行パイプライン

### 重大な発見: フェーズ機構は実行経路に接続されていない

- `LayerComponentHost::enabledForPhase()`（`ArtifactLayerComponentSystem.ixx:462-479`）と、それを公開する `ArtifactAbstractLayer::enabledLayerComponents()`（`.cppm:5625-5630`）は存在するが、**呼び出し元が 1 件もない**。
- フェーズ昇順に回して実行するループはコードベース上に存在しない。
- 実際の実行は、descriptor/phase を経由せず、**legacy の bool メンバ / プロパティパスを直接参照**している。

### 実際の評価エントリポイント

1. **本番シミュレーション経路**（コンポジション単位、同期 CPU）
   - `ArtifactAbstractComposition::evaluateLayerComponentSimulation(const FramePosition&, bool)`
   - `Artifact/src/Composition/ArtifactAbstractComposition.cppm:2697-2701`（公開）/ `:1521-2002`（実装）
   - 呼び出し元: `ArtifactRenderQueueService.cppm` / `ArtifactCompositionRenderController.cppm` / bake loop

2. **プレビュー/legacy 描画経路**（レイヤー単位、遅延、同期 CPU）
   - `cloneRenderInstancesImpl()` — `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx:792-880`
   - `drawWithClonerEffect()` / `cloneRenderInstances()` 経由

3. **レイヤー局所評価**
   - `getLocalTransform()` / `getLocalTransformAt()` — motion-dynamics / physics
   - `drawFractureOverlay()` — fracture / fluid / particle-emitter

### 実行順序（実態）

宣言の phase 順（Source → Drive → Generate → Arrange → Intent → Dynamics → Topology → Emit → RenderExtraction）に対し、実際はハードコード呼び出し:

1. `layerGenerators()` が cloner generator を合成（Generate）
2. `cloneRenderInstancesImpl()` が固定順で適用: cloner → layout → crowd → physics timing → fields → collision
3. 本番経路が crowd（Intent）と collision（Dynamics）を実行し、contact を生成
4. Topology（fracture）と Emit（particle）は contact イベント経由で間接的に発火

ハードコード順は wired なコンポーネントでは意図した phase 順に「たまたま」揃っているが、`LayerComponentDescriptor.phase`/`order` から導出されていない。

### コンポーネントごとの評価接続状態

| コンポーネント | phase | 接続状態 | 詳細 |
|---|---|---|---|
| cloner | Generate | **接続済み** | `clonerComponentInstances()` / `layerGenerators()` |
| layout | Arrange | **接続済み** | `applyLayoutComponent()` / `parentAutoLayoutOffset()` |
| crowd | Intent | **接続済み（部分）** | 簡易 centroid/steering 近似。専用 `BoidsSwarmSystem` 未使用 |
| motion-dynamics | Drive | **接続済み** | `DynamicsChannel1D::update()`（getLocalTransform 内） |
| sequence-player | Drive | **宣言のみ（未接続）** | `enabled=false` 固定、評価参照ゼロ |
| collision | Dynamics | **接続済み** | 本番 collision loop + プレビュー `applyInstanceCollisionComponent()` |
| fracture | Topology | **接続済み** | `applyFractureImpact()` / `drawFractureOverlay()` / `syncFragmentDataset()` |
| particle-emitter | Emit | **接続済み（部分）** | 手書き `componentParticles_` ループ。専用 `ParticleSystem` 未使用 |
| fluid | Dynamics | **部分接続（描画内のみ）** | `FluidSolver2D` は `drawFractureOverlay()` 内のみ。本番ループ未参加 |

### 疑われる欠落

- **フェーズ順実行ループなし**（`enabledForPhase` が無呼び出し）
- **sequence-player が完全に未接続**
- **fluid が本番シミュレーションループ（`evaluateLayerComponentSimulation`）に入っていない**。`usesLayerComponentSimulation()` は crowd/collision のみでゲートしている（`ArtifactAbstractComposition.cppm:2714-2724`）
- **`builtin.crowd` が専用 `BoidsSwarmSystem` を使っていない**
- **`builtin.particle-emitter` が専用 `ParticleEmitter`/`ParticleSystem` を使っていない**（`ArtifactParticleLayer` は使うが builtin は使わない）
- **`LayerEvaluationContext` が定義のみで未使用**（`ArtifactLayerComponentSystem.ixx:308-314`）

### 評価状態の更新箇所

- 本番状態: `setAuthoritativeComponentEvaluationState()`（`ArtifactAbstractLayer.cppm:5809-5900`）で `authoritativeComponentState_` に保存。
- 局所状態: `componentEvaluationState_` を `syncFragmentDataset()` / `drawFractureOverlay()` / `applyFractureImpact()` が更新。
- 実行は**全て同期 CPU**。GPU/compute や async/thread はコンポーネント評価経路では使われない（データ並列 `Parallel::For` はソルバー内部に限る）。

### 評価層の整合性判定: 部分乖離

phase/order による実行機構が未接続で、実行順序はハードコード。fluid / particle-emitter / crowd が専用エンジン未使用または部分接続、sequence-player が完全未接続。

---

## 3. プロパティ露出と評価の整合性

### 露出されるコンポーネント系グループ

`getLayerPropertyGroups()`（`ArtifactAbstractLayer.cppm:6158-8015`）が生成するグループ:

- `Components`（`component.script/cloner/collision/crowd/particleEmitter/fluid.enabled`）
- `Collision`（shape/width/height/radius/offsetX/offsetY/floorY/compositionBounds）
- `Layout`（enabled/mode/anchorMode/horizontalPin/verticalPin/scaleMode/safeArea*/stackDirection/gap/maxPerRow）
- `Cloner`（mode/cloneCount/timeOffsetStep/sequence*/offset*/jitter*/seed/columns/rows/depth/spacing*/radialCount/radius/startAngle/endAngle/rotationStep/opacityDecay + transforms）
- `Crowd`（cohesion/separation/alignment/maxSpeed/jitter）
- `Particle Emitter`（count/speed/lifetime）
- `Fluid`（gridWidth/gridHeight/viscosity/diffusion/buoyancy/vorticity/solverIterations）
- `Generator N`（動的、`component.generators.N.*`）
- `Field N`（動的、`component.fields.N.*`）
- `Clone Modifier N`（動的、`component.cloneModifiers.N.*` / compat）

### 一致している根拠

- 主要な有効化フラグ（script/cloner/collision/crowd/particleEmitter/fluid）は、露出・setter・評価の 3 点でパス文字列が一致。
- Collision / Crowd / Particle Emitter / Fluid の全パラメータは、露出・setter・評価間で一貫。
- Cloner 主要パラメータは `cloneComponentMode()` または `layerGenerators()` の descriptor settings 経由で評価に到達。
- 動的グループ（generators/fields/cloneModifiers/transforms）は descriptor と同一インデックス・typeId で生成され、setter も同一パスをパース。

### 孤児プロパティ（露出はあるが評価コードで未使用）

1. **Layout の 7 件**（setter と serialize に存在するが評価に到達しない）:
   - `component.layout.anchorMode`
   - `component.layout.horizontalPin`
   - `component.layout.verticalPin`
   - `component.layout.scaleMode`
   - `component.layout.safeAreaEnabled`
   - `component.layout.safeAreaPaddingX`
   - `component.layout.safeAreaPaddingY`
   - 根拠: `applyLayoutComponent()`（`ArtifactCloneEffectSupport.ixx:189-233`）は `enabled`/`gap`/`stackDirection`/`maxPerRow` のみ使用。`syncBuiltinComponentDescriptors()` も layout.mode/gap/maxPerRow のみ descriptor に書き込み。

2. **`artifact.field.solid` の `value`**:
   - 露出されるが、どの評価経路（clone/fragment/text）でも `solid` が処理されない。

### 実質的な値不整合

- **compat cloner generator の settings キー欠落**:
  - `syncBuiltinComponentDescriptors()` は cloner settings に `mode`/`count`/`seed`/`timeOffsetStep`/`sequenceEnabled`/`sequenceRate`/`sequenceSoftness` のみを書く。
  - しかし評価側 `clonerComponentInstances()` は `offsetX`/`offsetY`/`offsetZ`/`rotationStep`/`opacityDecay`/`jitterX/Y/Z`/`seed` を settings から読む。
  - 結果、これらのキーが compat 経路に存在せず、評価側はフォールバック値に依存。露出された `component.cloner.offsetX` 等の実値が compat 経路で反映されない可能性がある。

### 未露出参照（評価が参照するが露出されていない）

- 顕著な不一致は検出されなかった。

### dead code / 軽微な不整合

- `applyClonerComponentTransform()`（`ArtifactCloneEffectSupport.ixx:577`）は定義のみで呼び出しなし。
- `component.generators.{N}.radialCount` の setter に上限クランプなし（露出側は hard range 1..2048、setter は `std::max(1, ...)` のみ）。
- `component.fields.{N}.summary` は読み取り専用（setter no-op）で、これは意図的。

### descriptor とプロパティパス生成の対応

- `ArtifactLayerComponentSystem.ixx` の descriptor はプロパティパス文字列の生成に直接関与しない。
- descriptor の `typeId`（例 `artifact.component.cloner`）とプロパティプレフィックス（`component.cloner.*`）は命名規則で対応しているが、文字列を相互変換するマッピング関数は存在しない。
- つまり両者は同じデータソースを参照するが、文字列リテラルを共有していない。

### プロパティ露出層の整合性判定: 概ね一致（孤児・実質不整合あり）

---

## 総合判定: 部分乖離

**宣言・ID 層は健全、実行層は descriptor をバイパスして legacy プロパティ参照が実体、という二重系統化が根本問題。**

### 良好な点

- componentId / typeId に重複・typo なし。
- `requiredTypeIds` と依存フェーズ検査は正確。
- 主要コンポーネントのプロパティパスは露出・setter・評価間で一致。
- 実行順序は（ハードコードではあるが）wired なコンポーネントで意図した phase 順に揃っている。

### 問題点（優先度順）

1. **フェーズ実行パイプライン未接続** — `enabledForPhase()` が無呼び出し。descriptor の phase/order が実行に使われない。
2. **sequence-player が完全未接続** — `enabled=false` 固定、評価参照ゼロ。
3. **fluid が本番シミュレーションループ未参加** — 描画内のみ。
4. **crowd / particle-emitter が専用エンジン未使用** — `BoidsSwarmSystem` / `ParticleSystem` が宙に浮く。
5. **compat cloner settings キー欠落** — 露出値と評価結果が乖離する恐れ。
6. **孤児プロパティ** — Layout 7 件、solid field。
7. **命名乖離** — `builtin.fracture` vs `fracture.enabled`、kebab-case vs camelCase。
8. **builtin descriptor の永続化無効** — `fromJson` 直後に上書き。
9. **`LayerEvaluationContext` / `applyClonerComponentTransform()` が dead code**。

### 結論

descriptor システムは「型・依存関係・検証・JSON 入出力」のメタデータ層として整備済みだが、実行パイプラインがそれを参照していない。整合性を回復するには、**実行経路を descriptor（`enabledForPhase` + phase/order）駆動に一本化**し、legacy の bool メンバ / プロパティパス参照を置き換える方向が本筋。同時に、sequence-player と fluid の本番接続、crowd / particle-emitter の専用エンジン接続、compat cloner settings のキー補完が必要。
