# Fluid Component vs Pyro Domain Split (2026-07-01)

`fluid` と `pyro` を同じ「流体」という言葉でまとめず、
今の `LayerComponentPhase` / generator / field / modifier / simulation contract に沿って
役割を分離するための方針。

## Decision

### 1. `fluid` は layer component に入れる

`artifact.component.fluid` は、レイヤーや instance 群に対して使う
軽量・局所・2D 寄りの流体シミュレーション component とする。

想定責務:

- 2D grid ベースの velocity / density preview
- particle / clone / layer distortion への influence source
- editor 中の軽量 preview fallback
- small-domain smoke / ink / vortex / dissolve の土台

この系統は、既存 `FluidSolver2D` と相性が良い。

### 2. `pyro` は独立 simulation domain にする

`pyro` は `artifact.component.pyro` に即押し込まず、
まず `ArtifactCore` 側の independent volume simulation として扱う。

想定責務:

- voxel-based gas simulation
- density / temperature / fuel / velocity の持続状態
- deterministic fixed timestep
- frame cache / bake / snapshot
- renderer へ immutable volume snapshot を渡す

これは `ArtifactCore/docs/MILESTONE_PYRO_VOLUME_SIMULATION_CORE_2026-06-27.md`
の責務と一致する。

## Why They Must Be Separate

### Fluid component

- 小さく、軽く、interactive でよい
- layer や instance set に近い
- preview fallback と相性が良い
- crowd / particle / clone modifier の influence に再利用しやすい

### Pyro domain

- 重く、状態を持ち、composition/world owner が必要
- renderer / cache / bake と強く結びつく
- particle に埋め込むより volume snapshot の方が自然
- deterministic replay が重要

このため、`fluid = component-local or layer-facing`,
`pyro = composition simulation world-facing` を基本ルールにする。

## Canonical Placement

### `artifact.component.fluid`

- phase: `Dynamics`
- scope: `Composition`
- owner: layer descriptor
- runtime:
  - immediate preview evaluator
  - 将来的には composition simulation session に参加可能

### `pyro`

- phase: layer component ではなく、まず simulation domain
- scope: composition/world
- owner:
  - `PyroSimulation`
  - `PyroDomain`
  - `PyroFrameSnapshot`
- layer 側は将来的に
  - emitter source
  - collider source
  - render consumer
  のどれかとして接続する

## Integration Contract

### Fluid -> layer/component path

`fluid` は次の経路に接続してよい。

- particle emitter の velocity source
- clone modifier / field の influence source
- distortion / advection effect の preview source
- small smoke / ink preview

### Pyro -> simulation path

`pyro` は次の経路に接続する。

- layer / generator / particle を emitter source として受ける
- collider component / soft-body / fracture result を collider source として受ける
- render extraction で volume snapshot を renderer に渡す
- cache / bake / queue render が正規 consumer になる

## Guardrails

- `pyro` を `FluidSolver2D` の単純拡張として扱わない
- `fluid` に fuel / temperature / voxel cache / bake 責務を押し込まない
- `pyro` を clone/layout/crowd と同じ lightweight preview pathに混ぜない
- `fluid` と `pyro` の両方が `Dynamics` に見えても、owner を混同しない

## Near-Term Execution Order

1. `artifact.component.fluid` を layer component として維持
2. `FluidSolver2D` preview bridge を layer-side に追加
3. `fluid` を particle / clone influence sourceへ接続
4. `pyro` は `ArtifactCore` 側の domain / cache / snapshot を先に完成
5. その後、layer は `pyro emitter` / `pyro collider` / `pyro consumer` として接続

## Summary

- `fluid` は component
- `pyro` は domain simulation
- 両者は related だが同一レイヤーに押し込まない
- layer component pipeline と authoritative simulation path の分離を守る

## 2026-07-25 実装監査

- `artifact.component.fluid` と `FluidSolver2D` は layer/component 側に存在し、軽量な 2D density／velocity preview と particle field 経路に分離されている。
- `PyroDomain`／`PyroSimulation` は `ArtifactCore` の独立 simulation module として存在し、density／temperature／fuel／velocity／pressure、fixed timestep、source／collider、checkpoint／snapshot／seek を持つ。
- したがって fluid を pyro の単純拡張にせず、component-local と domain-owned を分ける基本方針はコード構造と一致する。
- 一方、layer の pyro emitter／collider／render consumer 接続、immutable volume snapshot の renderer extraction、cache／bake／queue render の正規統合は未確認である。
- 本文書は方針・配置の監査としては整合しているが、統合実装と runtime parity は未完了とする。
