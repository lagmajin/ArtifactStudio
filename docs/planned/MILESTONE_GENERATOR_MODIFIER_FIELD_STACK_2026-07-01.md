# Generator / Modifier / Field Stack Migration (2026-07-01)

`component.cloner.*` を単発機能のまま肥大化させず、
複数 generator、複数 modifier、複数 field が矛盾なく共存できる構造へ移行するための計画。

この文書は、次の 5 段階を実装順として固定する。

1. まず `single cloner properties` を維持
2. 裏で `cloner descriptors[]` へ寄せる
3. evaluator 側を `for generator in generators` に変える
4. field は別スタックにする
5. 各 generator / modifier が field mask / blend / strength / remap を持つ

## Goal

- Cloner を 1 個前提にせず、複数 generator を並列追加できる
- Time Offset / Sequence / Random / Delay を cloner 特化設定ではなく modifier 的に育てられる
- Field を cloner 専用でなく、crowd / fracture / emit / future dynamics にも横断適用できる
- 既存 `component.cloner.*` / `component.cloner.transforms.*` を即破壊しない

## Why This Split

今の `component.cloner.*` は、次の責務が 1 箇所に混在し始めている。

- clone を生成する
- clone を配置する
- clone の時間差を作る
- clone の見え方を sequence 的に制御する
- 将来 field を当てたい

このまま拡張すると、

- C4D の Cloner / Effector / Field 的な分離ができない
- Blender 的な generator -> modifier -> field の順序も曖昧になる
- crowd / emit / fracture へ modulation を共通化しにくい

したがって、今後の正規構造は次の 3 層で扱う。

## Canonical Layers

### 1. Generator

instance 集合そのものを生む。

例:

- Linear Cloner
- Grid Cloner
- Radial Cloner
- Echo Cloner
- Scatter Generator
- Text To Instances

### 2. Modifier

既存 instance 集合に対して transform / weight / time / color などを変更する。

例:

- Time Offset
- Sequence / Stagger
- Random
- Delay
- Step
- Color / Opacity
- Transform Offset

### 3. Field

generator / modifier / dynamics / emit に共通の influence source。

例:

- Solid
- Sphere
- Box
- Linear
- Radial
- Noise
- Age
- Time Window
- Velocity

## Existing In-Repo Seeds

すでに再利用できる土台がある。

- `Artifact/include/Components/FieldComponent.ixx`
  - `AbstractFieldComponent`
  - `SphericalFieldComponent`
  - `BoxFieldComponent`
  - `LinearFieldComponent`
  - `RadialFieldComponent`
  - `NoiseFieldComponent`
  - `SolidFieldComponent`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
  - `component.cloner.transforms.*`
  - 単発 cloner property 群
- `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
  - `cloneRenderInstances()`
  - `CloneRenderInstance`
  - `timeOffset` / `weight` の伝搬

## Phase 1: Keep Single Cloner Properties

### Rule

既存 `component.cloner.*` は維持する。

例:

- `component.cloner.mode`
- `component.cloner.cloneCount`
- `component.cloner.offsetX`
- `component.cloner.timeOffsetStep`
- `component.cloner.sequenceEnabled`

### Reason

- 既存 project 互換を守る
- UI を一気に壊さない
- まず runtime の内部表現だけを進化させる

### Contract

single cloner property 群は、内部では「generator[0] の互換 alias」と見なす。

## Phase 2: Move Internally To `descriptors[]`

### New Internal Model

`component.cloner.*` を内部で次のような配列 descriptor へ正規化する。

```text
generators[0]
generators[1]
generators[2]
```

最小 descriptor 例:

```text
id
type
enabled
order
settings
```

cloner 系の初期 type 候補:

- `artifact.generator.cloner.linear`
- `artifact.generator.cloner.grid`
- `artifact.generator.cloner.radial`

### Compatibility Mapping

既存 single property は、読み込み時に `generators[0]` へ変換する。

例:

- `component.cloner.mode` -> `component.generators.0.type/settings`
- `component.cloner.cloneCount` -> `component.generators.0.settings.count`
- `component.cloner.transforms.*` -> `component.generators.0.modifiers.transformStack.*`

### Guardrail

旧 path は当面 read/write 両対応にする。
保存時に完全移行するか、互換保存するかは後段で決める。

## Phase 3: Evaluator Becomes `for generator in generators`

### Current Problem

いまの `cloneRenderInstances()` は、暗黙に「cloner は 1 個だけ」という前提で分岐している。

### Target

評価は次の 3 段に分ける。

1. `for generator in generators`
   - base instance set を生成
2. generator outputs を merge
3. modifier 群を順に適用

### Merge Rules

generator merge は最低限、次を持つ。

- `append`
- `replace`
- `union by source`

初期は `append` だけでよい。

### Result

これで

- Cloner A
- Cloner B
- Echo Generator

を同一 layer で並列追加できる。

## Phase 4: Fields Become A Separate Stack

### Rule

field は generator / modifier から分離した独立 stack にする。

```text
fields[0]
fields[1]
fields[2]
```

### Why Separate

field を generator の内側に閉じ込めると、

- crowd へ同じ falloff を使えない
- fracture / emit の trigger に使い回せない
- field authoring が component ごとに重複する

### Initial Field Descriptor

最小限:

```text
id
type
enabled
blendMode
strength
invert
settings
```

type 候補は、既存 `FieldComponent.ixx` に合わせる。

- `solid`
- `sphere`
- `box`
- `linear`
- `radial`
- `noise`

### Evaluation Contract

field は world position だけでなく、将来的に次も読めるようにする。

- instance index
- source layer id
- age
- time
- velocity
- user tags

## Phase 5: Generator / Modifier Own Field Mask And Remap

### Rule

各 generator / modifier は、0 個以上の field binding を持てる。

最小 binding:

```text
fieldId
strength
blendMode
remapMin
remapMax
invert
targetChannels
```

### Target Channels

初期候補:

- `weight`
- `timeOffset`
- `position`
- `rotation`
- `scale`
- `opacity`
- `spawnRate`

### Example

`Time Offset Modifier`

- base `timeOffsetStep = 0.12s`
- field = sphere
- target = `timeOffset`

なら、sphere 内だけ強く time offset する。

`Sequence Modifier`

- base `sequenceRate = 12`
- field = linear
- target = `weight`

なら、線形 field の中だけ順次出現する。

## Migration Example

### Current

```text
component.cloner.mode = radial
component.cloner.radialCount = 24
component.cloner.timeOffsetStep = 0.08
component.cloner.sequenceEnabled = true
component.cloner.sequenceRate = 10
```

### Internal Target

```text
generators[0]:
  type = artifact.generator.cloner.radial
  settings.count = 24

modifiers[0]:
  type = artifact.modifier.time-offset
  settings.step = 0.08

modifiers[1]:
  type = artifact.modifier.sequence
  settings.enabled = true
  settings.rate = 10
```

これで将来、同じ layer に

- `generators[1] = grid cloner`
- `modifiers[2] = random`
- `fields[0] = sphere`

を素直に足せる。

## Proposed Descriptor Families

### Generator Types

- `artifact.generator.cloner.linear`
- `artifact.generator.cloner.grid`
- `artifact.generator.cloner.radial`
- `artifact.generator.echo`
- `artifact.generator.scatter`

### Modifier Types

- `artifact.modifier.time-offset`
- `artifact.modifier.sequence`
- `artifact.modifier.random`
- `artifact.modifier.delay`
- `artifact.modifier.transform-offset`

### Field Types

- `artifact.field.solid`
- `artifact.field.sphere`
- `artifact.field.box`
- `artifact.field.linear`
- `artifact.field.radial`
- `artifact.field.noise`

## Recommended Execution Order

1. internal generator descriptor を追加
2. single cloner -> generator[0] adapter を作る
3. multiple generator evaluator を append merge だけで入れる
4. field descriptor と `FieldComponent.ixx` bridge を作る
5. `time-offset` と `sequence` を cloner property から modifier descriptor へ昇格

## Guardrails

- 旧 `component.cloner.*` を即削除しない
- field を cloner 専用設定に埋め込まない
- generator が dynamics 責務を持たない
- modifier が authoritative collision を解決しない
- phase 順序は既存 `LayerComponentPhase` を壊さず、`Generate -> Arrange/Modify -> Intent -> Dynamics` を守る

## Related Files

- `Artifact/include/Components/FieldComponent.ixx`
- `Artifact/include/Layer/ArtifactCloneEffectSupport.ixx`
- `Artifact/include/Layer/ArtifactLayerComponentSystem.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `docs/planned/MILESTONE_LAYER_COMPONENT_PIPELINE_2026-07-01.md`

## 2026-07-25 実装監査

- 既存 `component.cloner.*` 互換 property、`layerCloneModifiers()` の複数 modifier 管理、time-offset／sequence 系の compatibility modifier、weight／scale／time offset の伝播はコード上で確認できる。
- `FieldComponent.ixx` の Solid／Sphere／Box／Linear／Radial／Noise field 基盤と、field influence の consumer 接続も存在する。
- 一方、single cloner を正式な `generators[0]` descriptor へ正規化する永続的内部契約、複数 generator の append evaluator、独立 `fields[]` stack、generator／modifier ごとの field binding／remap の完全実装は確認できない。
- 旧 `component.cloner.*` 互換を維持したまま段階移行する設計方針は妥当だが、5段階全体は未完了と判定する。runtime の複数 generator／modifier／field 評価も未検証である。
