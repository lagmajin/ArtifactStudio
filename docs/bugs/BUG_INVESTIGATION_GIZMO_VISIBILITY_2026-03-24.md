# 不具合調査記録: Composition Editor でギズモが正常に表示されない

## 対象

`ArtifactCompositionEditor` / `Composition View` 上の transform gizmo 表示不良を扱う。

このメモでは以下を同じ問題群として扱う。

- 選択してもギズモが出ない
- 一部ハンドルだけ出ない
- レイヤー本体とギズモ位置がずれる
- frame / visibility / active state によってギズモが不安定に消える

---

## 既存の進行中ドキュメントとの関係

### 1. `docs/bugs/BUG_INVESTIGATION_COMPOSITION_VIEW_DILIGENT.md`

既存の Composition View 調査メモでは、`CompositionRenderController` と `ArtifactPreviewCompositionPipeline` の描画経路が二重化していることが強い構造問題として挙がっている。

この二重化はギズモにも当てはまる。

- controller 側は `TransformGizmo` を持ち、`gizmo_->draw(renderer)` を呼ぶ
- preview pipeline 側も独自に bounding box / anchor / handle を描く
- どちらを正規経路にするかが固定されていない

そのため、ギズモ表示不良は単独の描画バグというより、`Composition View` 系の未統一設計の一部として扱うのが妥当。

### 2. `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_2026-03-21.md`

このマイルストーンの `Definition Of Done` には以下が含まれている。

- selection と hit test が editor 内で破綻しない
- image / solid / video / text の基本表示が editor 上で確認できる

ギズモは直接書かれていないが、`Composition Visibility` と `Viewport Behavior` を安定させる上で、選択結果の視覚化として実質必須。

### 3. `Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md`

ここでは `playhead`、`selection box`、`guide`、`transform gizmo`、`anchor`、`bounds` を renderer API 上で共通化する方針が明記されている。

現状はその途中段階で、gizmo 表示は controller / preview pipeline / renderer API の責務がまだ揃っていない。

---

## 現象

現状コードから想定される症状は以下。

- レイヤー選択時にギズモが出たり出なかったりする
- selected layer は取れているのに、ギズモ描画だけ抜ける
- gizmo 本体は出るが、ハンドルや anchor がレイヤー位置と一致しない
- preview pipeline 側の簡易 gizmo と controller 側の `TransformGizmo` が一致しない
- local bounds が不正なレイヤーで gizmo が丸ごとスキップされる

---

## 関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/include/Widgets/Render/TransformGizmo.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

---

## コード上で確認できる現状

### 1. controller 側で `TransformGizmo` を直接描画している

`ArtifactCompositionRenderController.cppm` では overlay フェーズで:

- `selectedLayerId_` から selected layer を取る
- `selectedLayer->isVisible()` と `selectedLayer->isActiveAt(currentFrame)` を満たす場合だけ
- `gizmo_->setLayer(selectedLayer)`
- `gizmo_->draw(renderer_.get())`

を実行している。

つまり、選択中でも以下のどれかでギズモは消える。

- selected layer が `nullptr`
- selected layer が invisible
- 現在 frame で inactive
- renderer が未初期化

### 2. preview pipeline 側にも別実装の gizmo 描画がある

`ArtifactPreviewCompositionPipeline.cppm` では selected layer に対して:

- `localBounds()` を読む
- `global.map(...)` で 4 辺と anchor を計算する
- `drawThickLineLocal()` と `drawSolidRect()` で簡易 gizmo を描く

こちらは `TransformGizmo` を使っていない。

したがって、controller 側だけ直しても preview pipeline 経由では直らない、または逆が起きる。

### 3. `TransformGizmo::draw()` は `localBounds().isNull()` だけで早期 return する

`TransformGizmo.cppm` では:

- `QRectF localRect = layer_->localBounds();`
- `if (localRect.isNull()) return;`

になっている。

`isNull()` は width か height が 0 のときだけで、負値や極端値は別扱いになる。逆に `PreviewPipeline` 側は `!isValid() || width <= 0 || height <= 0` で弾いている。

ここで「どの bounds を invalid と見なすか」が一致していない。

### 4. gizmo 描画と hit test が `localBounds + globalTransform` 前提に強く依存している

`TransformGizmo` は:

- 描画で `localBounds()` を取得
- `GIZMO_OFFSET` を加える
- `getGlobalTransform()` で四隅・辺中央・回転ハンドル・anchor を world/canvas 空間に変換
- hit test は `canvasToViewport` / `viewportToCanvas` と逆変換で処理

このため、`localBounds()` と `getGlobalTransform()` のどちらかがレイヤー実体とずれると、表示と操作の両方が壊れる。

### 5. controller 側のレイヤー描画経路と gizmo の基準が完全には一致していない

Composition View のレイヤー本体は `drawLayerForCompositionView(...)` で個別型ごとに direct draw される一方、gizmo 側は `ArtifactAbstractLayer` の `localBounds()` と `getGlobalTransform()` だけを見ている。

そのため:

- レイヤー本体の見た目
- `localBounds()`
- `transformedBoundingBox()`
- gizmo の world 変換

の 4 つが一致していないレイヤー型では、ギズモだけずれる可能性がある。

---

## 強い原因候補

### 原因候補 1: gizmo 描画経路が二重化しており、修正点が分散している

最有力。

`Composition View` 系の既存調査と同じで、controller 側と preview pipeline 側で gizmo が二重実装されている。

これにより:

- 一方だけ bounds 判定が変わる
- 一方だけ zoom / viewport API の使い方が変わる
- 一方だけ selection / active frame 条件が変わる

といった差分が入りやすい。

### 原因候補 2: `localBounds()` の契約がレイヤー型ごとに揃っていない

`TransformGizmo` も `PreviewPipeline` も `localBounds()` 依存だが、Composition View 本体描画は layer type ごとの実装に分かれている。

特に:

- text
- video
- transformed image
- source size 未確定の layer

では、描画結果と `localBounds()` の一致が崩れるとギズモ位置が破綻する。

### 原因候補 3: active state 条件が強すぎて「選択中なのに見えない」状態が起きる

controller 側は:

- `selectedLayer->isVisible()`
- `selectedLayer->isActiveAt(currentFrame)`

を両方要求している。

これは「現在フレームで表示されないレイヤーには gizmo を出さない」という設計だが、編集用途では:

- レイヤーを選択した瞬間に gizmo を見たい
- in/out の外でも位置や範囲を見たい

という期待と衝突する可能性がある。

仕様なのか不具合なのかが未整理。

### 原因候補 4: invalid bounds 判定が preview pipeline と `TransformGizmo` で食い違っている

`PreviewPipeline`:

- `!isValid() || width <= 0 || height <= 0`

`TransformGizmo`:

- `isNull()` のみ

この差で:

- 一方では描く
- 一方では skip する
- 極端な矩形をそのままハンドル化する

が起こり得る。

### 原因候補 5: zoom / viewport 変換の責務が renderer API 上でまだ安定していない

`TransformGizmo::draw()` は canvas 空間へ直接描いているように見える一方、hit test は viewport 変換を明示的に使う。

`ArtifactIRenderer` の transform API は整理途中であり、既存マイルストーンでも no-op や責務の未固定が言及されている。

この段階では:

- draw 系 API が canvas 前提なのか
- local / viewport を内部でどこまで吸収するのか

が混ざると、見た目だけずれるバグが起きやすい。

---

## 仕様と不具合の境界で未整理な点

以下は実装バグの可能性もあるが、まず仕様確認が必要。

- inactive frame のレイヤーに gizmo を出すべきか
- invisible layer でも selected なら gizmo を出すべきか
- anchor は local origin 基準でよいか
- mask overlay と transform gizmo の優先表示順をどうするか

ここが曖昧なままだと、修正しても別ケースで再度「表示されない」が発生する。

---

## 確認手順

1. `CompositionRenderController` 側で `selectedLayerId_`, `selectedLayer->isVisible()`, `isActiveAt(currentFrame)` をログに出す
2. `TransformGizmo::draw()` で `localBounds()` と `getGlobalTransform()` の値を出す
3. `PreviewPipeline` と controller の両方で同じ layer に対する bounds 判定結果を比較する
4. レイヤー本体の描画矩形と gizmo 四隅の world 座標が一致しているかを見る
5. text / image / video / solid の各 layer type で差が出るかを確認する

---

## 修正方針の優先順位

### 1. gizmo 正規経路を 1 本に寄せる

最優先。

- `TransformGizmo` を唯一の gizmo 実装にする
- または preview pipeline 側の簡易 gizmo を唯一にする

のどちらかに寄せるべき。

現状の構造なら、操作系も持っている `TransformGizmo` を正規経路にした方が自然。

### 2. bounds 契約を統一する

- `localBounds()` が「gizmo / hit test / direct draw の共通基準」であることを明文化
- invalid 判定を `isValid && width > 0 && height > 0` に統一

### 3. active / visible 条件を編集仕様として整理する

- 再生時表示と編集時表示を分ける
- 少なくとも selected layer の gizmo 表示条件を明文化する

### 4. レイヤー本体描画と gizmo の基準を近づける

- `drawLayerForCompositionView(...)` の world rect
- `localBounds() + getGlobalTransform()`

が一致するかを layer type ごとに詰める。

---

## 現時点の仮説まとめ

ギズモが正常に表示されない主因は、単独の描画ミスというより:

- `Composition View` の描画経路が二重化している
- gizmo が `localBounds()` 契約に強く依存している
- レイヤー本体描画と gizmo 描画の基準がまだ完全一致していない

の 3 点の組み合わせである可能性が高い。

そのため、場当たり的にハンドルの座標だけ直しても再発しやすい。まず `gizmo の正規描画経路` と `bounds 契約` を固定する必要がある。

---

## 次アクション

- `docs/bugs/BUG_INVESTIGATION_COMPOSITION_VIEW_DILIGENT.md` から gizmo 関連の派生バグとしてリンクする
- `TransformGizmo::draw()` に最小ログを追加して実測する
- preview pipeline 側の gizmo 描画を一時的に無効化し、controller 側だけで再現を観察する
- layer type ごとの `localBounds()` と実描画結果のずれを記録する
