# Animation Dynamics Core Milestone

> 2026-03-28 作成

## 目的

`Physics2D` とは別に、アニメーション向けの軽量なダイナミクス層を `ArtifactCore` に用意する。

この milestone は AI 補助実装と相性がよい前提で切る。理由は、対象が「決定論的な更新則」「小さい状態量」「明確な入出力」を持つためで、汎用物理のような複雑な接触解決を含まないからである。

ここでいうダイナミクスは剛体シミュレーションではなく、次のような編集向け挙動を指す。

- follow through
- lag / delay
- spring / damping
- overshoot / settle
- secondary motion
- per-channel easing / inertia

狙いは、`AnimatableValue` と `AnimatableTransform2D / 3D` の上に載せて、レイヤーやパラメータの「動き方」を制御しやすくすること。

---

## 背景

現状の `ArtifactCore` には以下がある。

- `Animation.Value`
- `Animation.Transform2D`
- `Animation.Transform3D`
- `Physics2D`
- `ParticleSystem`
- `SimulationSettings`

ただし、それぞれの責務は分かれている。

- `Physics2D` は剛体・ジョイント・衝突
- `ParticleSystem` は群れや力場
- `Animation` は keyframe interpolation

アニメーション制作で欲しいのは、その中間にある「編集用の動き」であり、剛体物理ほど重くなく、単純な補間よりも表現力がある層。

---

## 方針

### 原則

1. `Physics2D` を置き換えない
2. 衝突判定や rigid body は持たない
3. stateful でも deterministic に保つ
4. `dt` ベースで更新できるようにする
5. property / layer 側に後から載せやすい形にする

### AI 実装条件

1. 1ファイル 1責務 を基本にする
2. 更新関数は `input -> state -> output` を明示する
3. 暗黙のグローバル状態を持たない
4. ユニットテストで振る舞いを固定できること
5. 既存の `Animation` / `Transform` と型名を混ぜない

### 想定用途

- 文字やロゴの follow through
- 位置・回転・スケールの遅れ追従
- camera / mask / helper layer の微妙な慣性
- clone や stagger motion
- overshoot のある settle 動作

---

## 既存資産

- `ArtifactCore/include/Animation/AnimatableValue.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform2D.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- `ArtifactCore/include/Geometry/Interpolate.ixx`
- `ArtifactCore/include/Physics/2D/Physics2D.ixx`
- `ArtifactCore/include/Scene/SimulationSettings.ixx`
- `docs/planned/IMPLEMENTATION_ANIMATION_PHYSICS_2026-03-22.md`
- `docs/planned/MILESTONE_LAYER_COMPONENTS_PHYSICS_BEHAVIOR_2026-03-28.md`

---

## Phase 1: Core Solver Primitives

### 目的

アニメーション向けの最小ダイナミクス solver を `ArtifactCore` に置く。

### 対象

- 1D / 2D / 3D の spring-damper
- lag / low-pass follower
- overshoot clamp
- velocity accumulator
- parameter validation

### 作業項目

- `Animation.Dynamics` もしくは同等の Core module を新設する
- scalar / vector の状態構造体を定義する
- underdamped / critically damped の両方を扱えるようにする
- `dt` と `target` を受けて次状態を返す API を固定する

### 完了条件

- 1D の spring-damper が単体で動く
- 2D / 3D でも同じ更新規約で扱える
- 再生速度やフレームレート差で挙動が破綻しない
- 数値的に安定し、同一入力で同一結果を返す

### 検証観点

- `dt` が小さくても大きくても破綻しない
- target を急変させても発散しない
- 連続更新と seek 後再開で状態が説明可能である

---

## Phase 0: Spec Freeze And Test Harness

### 目的

AI に実装させる前に、仕様の曖昧さを潰して、最低限の自動検証を置く。

### 作業項目

- scalar / vector / transform の対象型を固定する
- solver の更新式を「入力・状態・出力」に分解する
- 代表的な preset を 3 つ程度に絞る
- 期待値ベースの unit test を先に置く

### 完了条件

- 実装前にテストケースが書ける
- 仕様変更点が doc と test で追跡できる
- AI 生成コードの差分を小さく保てる

---

## Phase 2: Keyframe Bridge

### 目的

既存の keyframe interpolation とダイナミクスをつなぐ。

### 作業項目

- `AnimatableValueT` の target path をダイナミクス入力にできるようにする
- `AnimatableTransform2D / 3D` に channel 単位の adapter を用意する
- seek / scrub 時の state reset を定義する
- manual keyframe と dynamic follow の優先順位を決める

### 完了条件

- keyframe は target、ダイナミクスは追従として扱える
- スクラブ後に残留 state が暴れない
- channel 単位で有効化 / 無効化できる

---

## Phase 3: Presets and Serialization

### 目的

編集で使いやすい preset と保存形式を先に固める。

### 作業項目

- `Smooth`
- `Bouncy`
- `Jelly`
- `Heavy`
- `Floaty`
- `Rigid`

### 完了条件

- preset を property として保存できる
- 既存 layer との互換を壊さない
- inspector から見て意味が読める

---

## Phase 4: Layer Bridge

### 目的

Core の dynamics を layer / property に載せる。

### 作業項目

- layer-local な `physics.*` / `behavior.*` の受け皿を作る
- `followThroughGain` のような親子連動を定義する
- `Text` / `Image` / `Video` / `Camera` に適用しやすくする

### 完了条件

- layer 単位で動きを調整できる
- `Physics2D` を触らずに制作向け motion が作れる

---

## Phase 5: Workflow Integration

### 目的

UI と制作導線に載せる。

### 作業項目

- Animation menu に preset entry を追加する
- Property Editor に summary を出す
- Timeline / Composition Editor から参照できるようにする

### 完了条件

- どの layer にも同じ UI で触れる
- 設定探しが長くならない

---

## Non-Goals

- rigid body / collision / constraint solver
- 物理エンジンの置き換え
- GPU compute 化の先行
- scene 全体の流体 / 布 / 粒子の統合

---

## Risks

| Risk | Mitigation |
|---|---|
| timeline scrub で state が残る | seek 時に reset を必須化する |
| channel ごとに挙動が割れて管理不能になる | preset と schema を先に固定する |
| `Physics2D` と責務が混ざる | module 名と property 名を分ける |
| 遅延や overshoot が強すぎて使いづらい | clamp / damping / default preset を用意する |

---

## Recommended Order

1. Phase 0
2. Phase 1
3. Phase 2
4. Phase 3
5. Phase 4
6. Phase 5

---

## Current Status

2026-03-28 時点では、アニメーション向けの物理は `Physics2D` と別に明示した core milestone としてはまだ未整備。
この文書で、`Animation` の上に載る軽量ダイナミクス層の切り出し方と、AI で実装しやすい最小単位を固定する。
