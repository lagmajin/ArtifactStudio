# マイルストーン: Property / Keyframe 統合 実行計画

> 2026-03-25 作成

## 目的

`ArtifactCore::AbstractProperty` をキーフレームの正として完成させる。

この文書は、既存の統合方針を実装順へ落としたもの。

対象:

- Inspector の ◆ / 前後移動
- Timeline の keyframe lane
- property / transform / effect / mask / camera の統一
- `AnimatableTransform3D` との移行橋
- Copy/Paste / easing / time remap 連携

---

## 現状の前提

既に以下がある。

- `ArtifactCore::AbstractProperty`
- `ArtifactAbstractLayer::persistentLayerProperty(...)`
- `ArtifactPropertyWidget` のローカル keyframe store 撤去
- base layer の一部 persistent property 化
- `getLocalTransform()` / `opacity()` の minimal runtime bridge

ただし、まだ次の差分が残っている。

- derived layer 全体で persistent property が揃っていない
- `AnimatableTransform3D` と `AbstractProperty` の責務が二重に見える
- Timeline 上の keyframe lane が未完成
- property path ベースの navigation が未統一

---

## 実装フェーズ

### Phase 1: Property Source of Truth を完全に固定する

#### 目標

Inspector が参照する property を、毎回作り直す状態から脱却する。

#### 作業

- derived layer の property cache を揃える
- `Video` / `Audio` / `Camera` / `Light` を persistent 化する
- base / derived で `AbstractProperty` を返す契約を統一する

#### 完了条件

- Inspector refresh 後も property object が変わらない
- keyframe が session 中に消えない

---

### Phase 2: Transform bridge の単一路線化

#### 目標

`AnimatableTransform3D` を残しつつ、変換点を 1 箇所に集める。

#### 作業

- property key から transform へ反映する場所を 1 つにする
- `getLocalTransform()` / `getLocalTransform4x4()` の評価順を整理する
- `RationalTime -> 24fps` 変換の責務を明示する

#### 完了条件

- gizmo / drag / playback で transform が二重管理にならない
- property と render の見え方が一致する

---

### Phase 3: Inspector の keyframe 操作を Core 直結にする

#### 目標

◆ / 前後 / 追加 / 削除 を `AbstractProperty` に直接接続する。

#### 作業

- property 行ごとの keyframe 有無を表示
- `addKeyFrame/removeKeyFrame` を UI から直接呼ぶ
- `interpolateValue()` を行表示へ反映する
- keyframe の選択、ジャンプ、コピーの導線を整理する

#### 完了条件

- UI ローカルの keyframe 状態が残らない
- Inspector からの keyframe 操作が全て Core に通る

---

### Phase 4: Timeline keyframe lane を実装する

#### 目標

Timeline 上で property key を可視化・操作できるようにする。

#### 関連

- Timeline の具体的な編集 UI は `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` に分離した
- この文書では property / core 側の source of truth と lane の前提を固定する

#### 作業

- lane の表示単位を property path にする
- keyframe の点描画と選択を入れる
- lane から keyframe の追加・削除・移動を行えるようにする
- current frame に応じた補間値を表示する

#### 完了条件

- `layer.opacity` や `transform.position.x` が lane として見える
- lane から直接 key を打てる

---

### Phase 5: Property path の共通化

#### 目標

`layer.opacity` / `transform.position.x` / `effect.glow.amount` のような property path を共通言語にする。

#### 作業

- property path の命名規則を固定する
- selection / navigation / search を path ベースに寄せる
- effect / mask / text / camera も同じ検索・表示方式にする

#### 完了条件

- どのレイヤーでも同じ手順で keyframe に到達できる
- Graph Editor の下地になる

---

### Phase 6: Copy/Paste / Easing / Time Remap 連携

#### 目標

単純な keyframe 追加だけでなく、編集作業として完結させる。

#### 作業

- keyframe copy / paste
- easing / interpolation mode
- offset / retime
- time remap との整合

#### 完了条件

- keyframe 編集が制作手順として使える
- timeline / property / playback で矛盾しない

---

## 優先順位

### 最優先

1. derived layer の persistent property 化
2. transform bridge の単一路線化
3. Inspector の keyframe 操作直結化

### 高

1. Timeline keyframe lane
2. property path 共通化
3. copy/paste と easing

### 中

1. Graph Editor
2. time remap との本格連携
3. 追跡結果からの keyframe bake

---

## リスク

- `AnimatableTransform3D` と `AbstractProperty` を二重正にすると再び破綻しやすい
- property path の設計が曖昧だと later stage で検索や lane 表示が崩れる
- Timeline lane を先に作りすぎると、Core 側の責務が未確定でやり直しが出る

---

## 関連文書

- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md`
- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_OPERATION_FEEL_REFINEMENT_2026-03-25.md`
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_LONG_RUNNING_FEATURE_WORKSTREAMS_2026-03-25.md`

---

## Next Step

最初にやるべき順番は次の 3 つ。

1. derived layer の persistent property を揃える
2. transform bridge の責務を 1 箇所に集める
3. Inspector から keyframe を Core 直結にする

