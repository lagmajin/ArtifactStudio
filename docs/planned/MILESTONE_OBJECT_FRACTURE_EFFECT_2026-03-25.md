# マイルストーン: Object Fracture / Shatter Effect

**最終更新:** 2026-08-31

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

---

## Static audit follow-up (2026-07-25)

現行ソースでは `Geometry.Fracture` / `Physics.Fracture` に `FractureEffect`、source bounds / source mesh、impact、radial / grid / Voronoi / hybrid shard 生成、debris 粒子、lifetime、material 系 settings、shatter state / impulse motion、Glass / Concrete / Stone / Metal / Wood / Dust preset が存在する。Composition 側にも fracture property、physics event からの impact 適用、状態保存・復元、image layer の fracture overlay が確認できる。

したがって文書の「Core 側基盤追加済み」は現状と整合する。一方、実際の Layer / Effect メニューからの制作導線、shard geometry の最終描画、collision response の実運用、material 継承の出力、HLSL/GPU path、source 差し替え UI、runtime での破砕品質は未確認である。

### Audit status

- Phase 1: 実装済み相当 — fracture specification / settings / serialization 基盤を確認
- Phase 2: 部分実装 — shard / debris の生成・motion はあるが、独立 effect の全 render 接続未確認
- Phase 3: 未完了 — collision / material / source replacement workflow は未確認
- Phase 4: 未完了 — GPU/HLSL 化とアプリ UI 導線は未確認
- Definition of Done: Core reference は進展済み、制作向け end-to-end は未完了

## Update 2026-08-15

- `ArtifactAbstractLayer` の現行実装で fracture component の設定、trigger frame、impact適用、shard motion（速度・回転・重力・damping・lifetime・opacity）、floor／composition bounds の簡易衝突、debris 粒子生成、shard polygon／debris の `ArtifactIRenderer` 描画が確認できる。
- fracture state と shard motion は composition／layer の snapshot・復元・JSON経路へ含まれている。Physics LOD の Reduced／Minimal 設定から shard／debris 数、破砕計算、サブステップを制御する接続もある。
- したがって「Core基盤のみでアプリ層未接続」という初期説明は古く、最小限の Layer runtime／preview 描画まで実装済みとする。ただしこれは独立した `CreativeEffect` stack の完全なeffect登録を意味しない。
- 現行コード検索では、通常の Effect menu／専用破砕UIからの作成導線、mesh material／texture UV 継承、実物理collision backend、HLSL／GPU破砕、source差し替え・再リンク、Undo専用コマンド、外部品質受入れは確認できない。
- よって現状は `Phase 1〜3 and CPU layer preview: partial-to-implemented / CreativeEffect integration, material/source workflow, GPU path and runtime validation pending` と判定する。Core referenceのみ未完了とする旧判定は更新する。

## Update 2026-08-31: Layer-owned Fragment Object Pipeline

### 目的

破砕した破片を新しいレイヤーへ分割せず、元レイヤーの `Fracture Component` が所有する
一時的な `Fragment Object` として描画・シミュレーションする。レイヤー一覧、親子関係、
Undo、保存単位を破片数に比例して増やさないことを優先する。

### オブジェクト境界

`Fragment Object` はレイヤーではなく、次の情報を持つ runtime object とする。

- owner layer ID と deterministic fragment ID
- 切り出し済みの shard geometry、または source geometry への参照
- source material / color / texture UV の継承情報
- layer-local transform、velocity、angular velocity
- age、lifetime、opacity、active 状態
- 必要な場合だけ接続する rigid-body handle

元レイヤーは source と component state の owner として残し、Fragment Object の生成・更新・
破棄を管理する。Fragment Object を Timeline の通常行、Layer Panel の通常ノード、通常の
選択対象として公開しない。

### 正規ライフサイクル

```text
source layer
  -> deterministic shard generation
  -> fragment object spawn
  -> simulation / collision / lifetime
  -> fragment render batch extraction
  -> reset / scrub restore / release
```

破片の保存単位は、通常は component settings、seed、trigger、shard specification とする。
実行中の個々の object は保存データの主単位にしない。スクラブ復帰では deterministic
generation と fragment motion snapshot を組み合わせ、同じ入力から同じ object 集合を再構成する。

### 実装フェーズ

#### Phase F1: Fragment Object Contract

- `FragmentObject` と `FragmentObjectSet` の Core データ契約を定義
- source layer owner、fragment ID、geometry/material/transform/motion/lifetime を分離
- layer split、QObject、UI tree 依存を持たせない

完了条件: 破片をレイヤー以外のオブジェクト集合として一意に表現できる。

#### Phase F2: Layer Component Ownership

- `Fracture Component` が object set の生成、再生成、破棄を管理
- trigger、preGenerate、preset、shardCount、seed の変更で object set を無効化
- source bounds / mask / mesh の変更時に古い object を残さない

完了条件: 元レイヤーを分割せず、破砕状態だけを再構築できる。

#### Phase F3: Motion / Physics Bridge

- fragment object の linear / angular motion を既存の fracture motion と接続
- 必要な shard だけ Box2D body を作成し、body owner を fragment ID で追跡
- gravity、drag、restitution、floor / collider を object 単位で適用
- debris particle と mesh shard の状態を明確に分離

完了条件: 破片が独立して移動・回転・衝突し、寿命終了時に解放される。

#### Phase F4: Render Batch Extraction

- object set から shard geometry と per-object transform を render batch へ抽出
- source material / texture UV を維持し、破片ごとの draw call 増加を抑制
- GPU/Diligent 経路を優先し、CPU は互換 preview / reference に限定
- 既存の layer blend / mask / opacity 境界を壊さない

完了条件: 通常の composition render と preview の両方で破片が見える。

#### Phase F5: Timeline / Snapshot / Reset

- trigger 前、破砕直後、寿命終了後の状態を定義
- frame scrub、逆再生、loop で object set が重複生成されないようにする
- snapshot は fragment ID と motion state の対応を検証して復元

完了条件: 同じ frame へ移動したとき、同じ seed と設定から同じ破片配置を得られる。

#### Phase F6: Optional Bake to Layers

- 明示的な `Bake Fragments to Layers` 操作だけで layer 化する
- bake は現在の object set を新しい編集可能レイヤーへ複製する別ワークフローとする
- 通常の破砕再生では layer tree、selection、undo stack を増やさない

完了条件: 必要な場合だけ編集可能なレイヤー群へ変換でき、通常経路と混ざらない。

### 非目標

- 破片ごとの通常レイヤー自動生成
- 破片を独立した composition layer として常時公開すること
- 破砕 object のための新しい Qt signal / slot 中央配線
- 既存の `ReactiveEvents` を使った新規イベント経路
- 破砕を単なる particle preset へ置き換えること

### 受入条件

- 100 個以上の破片を layer tree のノード増加なしに表示できる
- source layer の差し替え、property reset、trigger 往復で古い破片が残らない
- 同じ seed / frame / settings から同じ Fragment Object 集合を再生成できる
- mesh shard と debris particle を別々に enable / lifetime / budget 制御できる
- layerization は明示的な Bake 操作に限定される
- GPU 経路、CPU reference、snapshot 復元、runtime 性能の状態を個別に検証できる
