# Property Keyframe Unification

## Goal

キーフレームの正を `ArtifactCore::AbstractProperty` に一本化する。

対象:

- Inspector の ◆ ボタン
- 前後キーフレーム移動
- 将来の Timeline keyframe lane
- 将来の Graph Editor
- Copy/Paste / 補間 / easing / time remap 連携

## Current State

現在は少なくとも 3 系統ある。

1. `AnimatableTransform3D`
   - transform 専用
   - gizmo / layer transform 更新で使われている
2. `AbstractProperty`
   - Core 側の汎用キーフレーム API
   - `addKeyFrame/removeKeyFrame/interpolateValue` を持つ
3. `ArtifactPropertyWidget`
   - UI ローカルの `keyFrameStore()`
   - Inspector の ◆ と前後移動がここを見ていた

この構成では、UI で打ったキーと再生系の実データが分離しやすい。

## Direction

最終方針:

- キーフレームの保存先は `AbstractProperty`
- UI は `AbstractProperty` を読むだけ・書くだけ
- レイヤー/エフェクト/マスク/タイムリマップは property path で同じ形に揃える
- `AnimatableTransform3D` は移行期間の bridge として扱う

## Migration Plan

### Phase 1

ベースレイヤー property を persistent にする。

- `ArtifactAbstractLayer::getLayerPropertyGroups()` が毎回新しい `AbstractProperty` を作るのをやめる
- `layer.opacity`
- `transform.position.x/y`
- `transform.scale.x/y`
- `transform.rotation`
- `transform.anchor.x/y`
- `time.inPoint/outPoint/startTime`

を layer 内の property cache から返す

### Phase 2

Inspector の keyframe UI を `AbstractProperty` 直結にする。

- `ArtifactPropertyWidget` の `keyFrameStore()` を廃止
- ◆ ボタンは `property->addKeyFrame/removeKeyFrame`
- 前後移動は `property->getKeyFrames()`
- 行の値表示は `property->interpolateValue(now)`

### Phase 3

transform 再生を property ベースへ寄せる。

選択肢:

1. `AnimatableTransform3D` を内部実装として残し、property change と相互同期する
2. transform も property key を正にして、`getLocalTransform()` が property evaluation を使う

当面は 1 が安全。

### Phase 4

Timeline / Graph Editor を property path ベースに統一する。

- `layer.opacity`
- `transform.position.x`
- `effect.glow.gain`
- `text.fillColor`

のような path で lane を張る

## Risks

- `getLayerPropertyGroups()` が derived layer 側でまだ都度生成を続けると、一部 property は persistent にならない
- transform は現在 `AnimatableTransform3D` が実描画に使われているため、`AbstractProperty` だけ切り替えても再生は揃わない
- `RationalTime -> 24fps rescale` の既存挙動があり、transform key の時間系は慎重に扱う必要がある

## Implemented Today

### 1. Base layer property cache

`ArtifactAbstractLayer` の base property は layer 内 cache から返すように変更開始。

効果:

- Inspector refresh のたびに別 `AbstractProperty` へ差し替わらない
- `AbstractProperty` に載せたキーフレームがセッション中に保持される土台になる

### 2. PropertyWidget keyframe source switch

`ArtifactPropertyWidget` の ◆ ボタンと前後移動は、UI ローカル store ではなく `AbstractProperty` 自身を使う形へ変更開始。

効果:

- Inspector のキーフレーム操作が Core API を通る
- 今後 Timeline/Graph Editor から同じ property を見られる前提ができる

### 3. UI ローカル keyframe store の撤去

`ArtifactPropertyWidget` 内に残っていた `keyFrameStore()` 系のローカル保持は削除した。

効果:

- Inspector が別ストアを持たなくなる
- `AbstractProperty` と UI の二重管理が 1 段減る

### 4. Minimal runtime bridge

`ArtifactAbstractLayer` の `getLocalTransform()` / `getLocalTransform4x4()` / `opacity()` は、
該当 property にキーフレームがある場合だけ `AbstractProperty::interpolateValue()` を参照するようにした。

意図:

- gizmo や既存 transform 更新経路を壊さない
- キーフレーム付き property だけは render 側でも `AbstractProperty` を正として読めるようにする

制約:

- `AnimatableTransform3D` との完全統合ではない
- property にキーフレームが無い通常値は、引き続き layer 本体の state を正として扱う
- derived layer / effect property まではまだ同じ橋を入れていない

### 5. Derived layer migration foothold

`ArtifactAbstractLayer::persistentLayerProperty(...)` を追加し、
derived layer でも同じ property cache を使って persistent property を返せるようにした。

現時点で適用済み:

- `ArtifactSolidImageLayer`
- `ArtifactTextLayer`
- `ArtifactVideoLayer`
- `ArtifactCameraLayer`
- `ArtifactLightLayer`
- `ArtifactCompositionLayer`
- `ArtifactImageLayer`
- `ArtifactSolid2DLayer`
- `ArtifactParticleLayer`

これで少なくとも上記 layer の Inspector property は rebuild ごとに別 object へ差し替わらない。

## Next Step

次にやるべきこと:

1. `ArtifactPropertyWidget` 内のローカル keyframe store の完全削除
2. `Video` / `Audio` / `Camera` / `Light` など残りの derived layer property も persistent 化
3. transform 系 property と `AnimatableTransform3D` の同期点を 1 箇所に集約
4. `J/K` を property path ベースの共通ナビゲーションへ接続
5. Timeline keyframe lane が直接 `AbstractProperty` を読むようにする
