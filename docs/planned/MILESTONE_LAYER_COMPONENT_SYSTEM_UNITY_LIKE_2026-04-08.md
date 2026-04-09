# マイルストーン: レイヤー向け軽量コンポーネントシステム

> 2026-04-08 作成

## 目的

Layer に Unity ライクな軽量コンポーネントシステムを導入し、
「レイヤー本体は薄く保つ」「機能は差し替え可能な単位で足す」「実行順と依存を明示する」ことを両立する。

この設計は、レイヤーに Physics / Behavior / Constraint / Binding / Visual Hint などの振る舞いを足すための共通基盤を作る。
あわせて、質量・密度・慣性・破砕閾値のような「layer にぶら下がるデータ」も同じ枠組みで扱えるようにする。
ただし、ECS の全面導入やゲームエンジン相当の一般化は狙わない。

---

## 背景

現在の layer は、見た目・時系列・選択・表示・一部の挙動が同じモデルに寄りやすい。
その結果、機能追加のたびに layer 本体へ責務が蓄積しやすく、
「レイヤーの種類ごとの個別実装」と「共通の振る舞い」を分けにくい。

このマイルストーンでは、layer を Entity に近い扱いにしつつ、
component を小さく保って既存システムと段階的に接続できる形を採る。

---

## 設計方針

### 1. Layer 本体は識別子とコア状態に集中する

Layer は次の責務に絞る。

- ID / name / visibility / lock / solo / opacity / blend mode などの基礎状態
- parent / child / group との接続
- property / undo / serialization との橋渡し
- component の所有とライフサイクル管理

### 2. 振る舞いは component に閉じ込める

各 component は単一目的にする。

- `Physics`: 追従、慣性、減衰、簡易力学
- `Behavior`: フレーム駆動の条件分岐、トリガー、状態遷移
- `Constraint`: 位置・回転・スケールの制約
- `Binding`: 他レイヤー / property / selection / time と接続する薄い接着層
- `VisualHint`: 色分け、表示補助、UI 上の注釈

### 2.5. データだけの component も許容する

動的な処理を持たない、あるいは処理よりもメタデータ保持が主目的の component も許容する。

- `PhysicsData`: 質量、密度、慣性、重心、摩擦、減衰、ドラッグなどの物理属性
- `FractureData`: 破砕可否、閾値、分割数、破片サイズ、ランダム性、残骸寿命などの破砕属性

これらは `Physics` / `Fracture` 系 component の入力データにもなるし、Inspector や Export でも参照できる。
データ計算は push 型ではなく pull 型を基本にし、必要な時点で依存を解決して値を取りに行く。
これにより、更新順の複雑化や不要な再計算を抑えやすくする。

### 3. 既存の layer / effect / group を壊さない

この設計は layer group 導入や effect stack と競合させない。
group は「整理・階層・可視性」の軸、component は「振る舞い」の軸として分ける。

### 4. Unity 風だが、必要最小限に留める

以下は採用する。

- component の attach / detach
- enable / disable
- lifecycle callbacks
- update order
- type-safe lookup

以下は初期導入では採用しない。

- reflection-heavy な自動 DI
- prefab / scene graph の全面置換
- generic system registry の大規模化
- script runtime 前提の動的スクリプティング依存

---

## 用語

- **Layer**
  編集対象の中心ノード。component の所有者。
- **Component**
  Layer にぶら下がる小さな振る舞い単位。
- **System**
  複数 layer / component を横断して処理する集約ロジック。
- **Behavior**
  1 layer 内で完結する状態変化や簡易ロジック。
- **Binding**
  layer と他の data source を接続する薄い層。
- **PhysicsData**
  layer にぶら下がる物理属性のデータ塊。振る舞いそのものではなく、他 component の入力になる。
- **FractureData**
  layer の破砕や崩壊を制御する authoring data。破片生成や耐久値の元になる。

---

## 推奨アーキテクチャ

### LayerComponentHost

Layer は `LayerComponentHost` を内部に持ち、component の追加・削除・検索を委譲する。

- `addComponent<T>()`
- `removeComponent<T>()`
- `getComponent<T>()`
- `hasComponent<T>()`
- `components()`

### LayerComponent

共通インターフェースは最小限にする。

- `onAttach(Layer&)`
- `onDetach(Layer&)`
- `onEnable()` / `onDisable()`
- `onFrame(frame)` または `update(deltaTime)`
- `serialize()` / `deserialize()`
- `clone()` は必要時のみ

### Component Payload

挙動を持たない component は、純粋なデータ保持として扱ってよい。

- `PhysicsData`
  - `mass`
  - `density`
  - `centerOfMass`
  - `inertia`
  - `friction`
  - `bounciness`
  - `linearDamping`
  - `angularDamping`
- `FractureData`
  - `enabled`
  - `breakThreshold`
  - `damageAccumulation`
  - `shardCount`
  - `shardSizeMin` / `shardSizeMax`
  - `randomSeed`
  - `debrisLifetime`
  - `postBreakImpulse`
  - `propagationRadius`

### Data Evaluation

データ系 component の計算は pull 型を基本にする。

- 値は必要になった時だけ解決する
- 中間結果の push 連鎖を前提にしない
- 参照元が明確なときだけ再計算する
- inspector 表示、export、physics 系 component の参照時に評価する

推奨ルール:

- component 自体は source of truth を持つ
- 派生値は getter もしくは evaluate 関数で取得する
- キャッシュは許容するが、無効化条件を明示する
- push 更新が必要なものは data component ではなく behavior component 側へ寄せる

### Component Context

component が layer 以外へ直接強依存しないように、必要な参照は context 経由で渡す。

- current composition
- selection state
- project / service access
- time / frame context
- property mutation gateway

---

## ライフサイクル

### Attach / Detach

1. Layer に component を追加する
2. component が owner layer を受け取る
3. 初期値を読み込む
4. 必要なら初回同期を行う

detach 時は逆順で後始末する。

### Enable / Disable

component は保持したまま無効化できる。
これは UI 上の mute / solo / temporary bypass と相性が良い。

### Update

update は 2 系統に分ける。

- `frame update`: タイムラインと同期する離散更新
- `runtime update`: 再生中やプレビュー時の連続更新

初期導入では frame update を主に使い、必要な component だけ runtime update を持たせる。

---

## データモデル

### Component Identity

各 component は以下を持つ。

- `componentId`
- `typeId`
- `enabled`
- `order`
- `dirty` フラグ

### Type ID

type は文字列ではなく、安定した識別子で扱う。

- C++ の型情報に依存しすぎない
- serialization で再現できる
- 将来の plugin 追加に耐える

### Order

同一 layer 内で component の実行順を制御する。

- 低い順に前処理
- 中央で主処理
- 高い順に後処理

order は「依存解決の代替」ではなく、軽量な並び替えに留める。

---

## 依存関係の扱い

component 間の依存は強制しすぎない。

- 明示的な dependency 宣言は可
- 自動循環解決はしない
- 解決不能な場合は disabled / warning にする

推奨ルール:

- 1 component は 1 つの責務に限定
- component 同士の直接呼び出しは最小限
- 共通データは layer property または context に寄せる

---

## 既存機能との接続

### Undo / Redo

- component の追加 / 削除 / 有効無効切り替えはコマンド化する
- property 変更と同じ undo 系に乗せる
- component 固有 state は snapshot 化できるようにする

### Serialization

- project save/load で component 構造を失わない
- 未知 component は破棄せず保留できる設計が望ましい
- 将来の version upgrade に備え、typeId と version を持つ

### Inspector

- layer inspector に component list を表示する
- add/remove/enable/disable/reorder を触れるようにする
- details は component 固有 panel に委譲する

### Timeline / Playback

- 再生中に動く component と静的 component を分ける
- keyframe 由来の変化は component へ委譲可能にする
- track 表示に component の存在を出せる余地を残す

---

## 初期対象コンポーネント

### M-LG-2a Physics Component

- follow / spring / damping / offset compensation
- 1 layer 内で完結する簡易追従
- 既存 transform / animation との競合を避ける

### M-LG-2b Behavior Component

- frame 条件による切り替え
- trigger / one-shot / repeat
- selection / visibility / time に反応する薄いロジック

### M-LG-2c Constraint Component

- parent 依存の補正
- scale / rotation / position の制約
- transform 系との橋渡し

### M-LG-2d Binding Component

- property 名と component state の接続
- inspector 変更との同期
- 外部サービスへの薄い adapter

### M-LG-2e Physics Data Component

- layer の物理属性をまとめて保持する
- mass / density / inertia / center of mass を持つ
- 追従や衝突ではなく、他 component や export のための authoring data とする

### M-LG-2f Fracture Data Component

- 破砕可能かどうかを示す
- 破砕閾値や破片数を持つ
- 破片のサイズ分布、残骸寿命、生成ランダム性を保持する
- fracture system や damage 系 component の入力にする

---

## 非目標

- 完全な ECS 化
- すべての layer を component のみで表現すること
- scriptable object 的な汎用シリアライズ基盤の全面刷新
- runtime plugin を前提にした熱い拡張機構
- transform / effect / group のモデルを再定義すること

---

## 実装フェーズ案

### Phase 1: 基本ホスト

- Layer に component host を追加
- attach / detach / lookup の最小 API を実装
- serialization に component 枠を追加

### Phase 2: Inspector 接続

- component list UI を追加
- enable / disable / reorder を操作可能にする
- component state の概要を表示する
- PhysicsData / FractureData の主要項目を Inspector で編集できるようにする

### Phase 3: Physics / Behavior

- follow / spring / damping の実装
- 条件分岐・トリガーの実装
- playback 連携
- 物理属性を参照して動き方を変えられるようにする

### Phase 4: Constraint / Binding

- parent 追従制約
- property binding
- selection / time 連携
- 破砕設定を参照して、後続処理に通知できるようにする
- データ参照は pull 型で解決する

### Phase 5: 安定化

- undo / redo の整備
- version migration
- unknown component の扱い
- 破砕関連データの後方互換を確認する
- pull 型評価のキャッシュ無効化条件を固める

---

## 検証ポイント

- component の追加削除で layer の基本動作が壊れない
- 再読込後も component 構成が復元される
- component の順序変更が予測可能に動く
- group / effect / property と責務が混ざらない
- 追従や減衰のような軽い振る舞いを追加しやすい
- 物理属性と破砕属性を layer ごとに独立して持てる
- 破砕用データが Inspector と serialization で破綻しない
- データ計算が pull 型で、不要な再計算や依存伝播を増やさない

---

## 関連

- `docs/planned/MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md`
- `docs/planned/MILESTONES_BACKLOG.md`
- `docs/WIDGET_MAP.md`
