# コンポジットエディタ プロパティ編集ラグ仮説

Date: 2026-04-25

## 現象

プロパティウィジェットで値を編集すると反映はされるが、引っかかり感のある動き不快である。

---

## データフロー概略

```
ArtifactPropertyEditorRowWidget (エディタ編集)
  └─ commitValue(value)
       ├─ layer->setLayerPropertyValue(name, value)
       └─ notifyLayerPropertyAnimationChanged(layer)
            ├─ layer->changed()
            └─ LayerChangedEvent を globalEventBus で publish

ArtifactCompositionRenderController (イベント受信側)
  └─ LayerChangedEvent 受信
       ├─ レイヤー表面キャッシュ無効化
       ├─ ベースコンポジット無効化
       └─ renderOneFrame() を即時呼び出し  ★ ここでフルGPUパイプラインが通る
```

---

## 推定原因（優先度高順）

### 1. LayerChangedEvent 受領時に throttling がない

`CompositionRenderController` は `LayerChangedEvent` を受けるたびに即座に `renderOneFrame()` を呼ぶ。プロパティスライダーdrag中など、短時間に連続して送られたイベントは全て独立したレンダリングをトリガーする。

- 現在の debounce は UI 再構築（80ms）と値更新（16ms）のみに適用
- イベント伝播 → レンダリング には debounce がない

### 2. プレビュー中に renderOneFrame() が走っている

`ArtifactFloatPropertyEditor` では：

- `sliderMoved()` / `previewValue()` → 即座に `commitValue(preview)`
- その後 `editingFinished()` でもう一度走る

ドラッグ中に何度も `LayerChangedEvent` が飛び、そのたびにフルパイプラインが描画される。

### 3. full repaint のコストが過大

プロパティ編集内容（金利・不透明度・色など）に関わらず、`BasePass → LayerPass → GizmoMask → Overlay → Present` の全パスが毎回走る。ビューポート外のエディットでも同じ。

### 4. rebuildUI のwidget破棄コスト

`ArtifactPropertyWidget::rebuildUI()` は `clearLayoutRecursive()` で全widget階層を破棄後、再構築する。80ms debounce ではあるが、ラグの体感に寄与している可能性がある。

---

## 仮説まとめ

| # | 仮説 | 確信度 |
|---|------|--------|
| A | ドラッグ中最に renderOneFrame() が過剰に呼ばれ、GPU処理が詰まる | 高 |
| B | LayerChangedEvent にレンダリングdebounceがなく、フレームごとに独立描画が走る | 高 |
| C | rebuildUI のwidget破棄再構築が debounce 込みでもUXに悪影響 | 中 |
| D | 編集対象レイヤーがビューポート外でもレンダリングが走る | 中 |

---

## 確認すべきポイント

1. `CompositionRenderController` での `LayerChangedEvent` 受領部に timer を入れずに直接 `renderOneFrame()` している箇所の特定
2. `previewValue()` 中に `LayerChangedEvent` を発行しているか、あるいは `editingFinished` まで遅延しているか
3. `renderOneFrame()` の各 pass のコスト内訳（プロファイルデータ）
4. ドラッグ中に Qt  thérapeutictimer が発生しているか

---

## 次のアクション（本案）

A, B の検証：首先 `CompositionRenderController` にレンダリング用 debounce timer（例: 33ms single-shot）を導入し、LayerChangedEvent ごとに即時レンダリングする現行構造を変える。

C, D は A/B 対応後に悪影響が残っていれば追加調査。
