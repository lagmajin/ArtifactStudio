# マイルストーン: Object Fracture / Shatter Effect

> 2026-03-25 作成

## 背景

CG ツールでよくある「オブジェクト破砕」「shatter」「destruct」系のエフェクトを、将来的にレイヤー/エフェクトの一種として扱えるようにしたい。

現状の `ArtifactCore` には以下の基盤がある。

- `ParticleSystem` / `ParticleRenderer`
- `ArtifactParticleGenerator`
- `CreativeEffect` 系の土台
- `Mesh` / `Geometry` / `GPU` 周辺の描画基盤

ただし、**破砕専用の shard 生成、破砕境界、衝突反応、マテリアル継承、破片寿命制御** は未整備で、粒子システムだけでは表現が足りない。

---

## 目的

1. 破砕エフェクトを Core 側で定義する
2. 粒子ベースの debris と、メッシュベースの shard を分けて扱う
3. CPU reference を残しつつ、将来的に GPU / HLSL 化できる形にする
4. アプリ層は当面触らず、Core 側の effect / generator 基盤を先に固める

---

## 進捗

- Core 側の fracture 基盤クラスを追加済み
- source bounds / source mesh / impact / shard / debris のデータ経路を実装済み
- shard 生成は radial / grid / voronoi-like / hybrid の最小版まで実装済み
- `Geometry.Fracture` モジュールを追加済み
- mesh face topology から shard を起こす経路を追加済み
- Glass / Concrete / Stone / Metal / Wood / Dust の preset を追加済み
- アプリ層の layer / UI は未変更

---

## 方針

### 原則

1. 破砕は「単なる particle preset」にはしない
2. `debris` は particle、`shard` は geometry を持つ別概念として分ける
3. 破片の生成規則を effect spec として Core に置く
4. render / simulation / metadata を分離する
5. UI 実装は後段でよい

---

## 想定する Core 構成

- `FractureEffect`
  - 破砕シミュレーションの入口
  - source geometry / mask / impact 情報を受ける
- `FracturePattern`
  - Voronoi / radial / grid / noise ベースの shard 生成
- `FractureShard`
  - shard geometry
  - transform
  - lifetime
  - velocity / angular velocity
- `FractureSolver`
  - 初期 break
  - impulse propagation
  - debris spawn
  - optional collision response
- `FractureRenderData`
  - shard mesh
  - particle debris
  - material / color / texture sampling

---

## Phase 1: Core Fracture Specification

### 目的

破砕効果の共通データ構造を定義する。

### 機能

- impact point / impact normal
- break radius / break force
- shard count / density
- debris spawn ratio
- shard lifetime / fade
- material inheritance
- fracture mask / protected region

### 完了条件

- 破砕条件を Core で表現できる
- メッシュ系 / 粒子系に分岐できる
- effect parameter として保存可能

---

## Phase 2: Shard / Debris Generation

### 目的

破片生成ロジックを実装する。

### 機能

- Voronoi shatter
- radial break
- edge biased break
- protected zone
- small debris spawn
- shard seed / deterministic generation

### 完了条件

- 同じ入力から同じ shard 分割を再現できる
- 破片のサイズ分布を制御できる
- debris と shard を分けて出せる

---

## Phase 3: Simulation / Motion Integration

### 目的

破片の動きと寿命を扱う。

### 機能

- initial impulse
- gravity / drag / damping
- spin / angular velocity
- collision response
- fade out / dissolve
- particle system との合流

### 完了条件

- 破片が時間で移動・回転・消滅する
- particle debris との組み合わせができる

---

## Phase 4: Rendering Path

### 目的

破砕見た目をレンダリングできるようにする。

### 機能

- CPU preview
- shard mesh draw
- material/color inheritance
- texture UV preserve / regenerate
- optional shadow / outline

### 完了条件

- preview と render 出力で破砕が見える
- 既存コンポジションに載せられる

---

## Phase 5: Creative Effect Integration

### 目的

破砕を effect stack へ入れる。

### 機能

- `CreativeEffect` 化
- parameter panel
- preset
- CPU reference / GPU backend split
- diff test

### 完了条件

- 破砕が effect として追加できる
- preset ベースで再利用できる

---

## Phase 6: Asset / Source / Import Integration

### 目的

`ISource` や asset 系と結合する。

### 機能

- source geometry 参照
- relink
- missing source 表示
- import from mesh / image / mask
- persistence

### 完了条件

- 破砕対象を source として差し替えられる
- 保存/復元で設定が落ちない

---

## 既存基盤との関係

- `ParticleSystem`
  - debris の土台
- `ArtifactParticleGenerator`
  - debris emission / force の参考
- `CreativeEffect`
  - effect 化の土台
- `Mesh`
  - shard geometry の土台
- `ISource`
  - 対象オブジェクト参照や素材再リンクの土台

---

## 完了条件

- 破砕が particle preset ではなく独立した effect として表現できる
- shard と debris を分離できる
- CPU reference がある
- 将来の HLSL 化に耐える
