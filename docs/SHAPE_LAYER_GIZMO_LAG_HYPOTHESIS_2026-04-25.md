# ShapeLayer Gizmo移動ラグ仮説

Date: 2026-04-25

## 現象

ShapeLayer を Gizmo で移動させると非常に遅く感じる。ImageLayer など他のレイヤタイプと比較した場合に問題量大。

---

## Gizmoドラッグからレンダリングへの流れ

```
TransformGizmo::handleMousePress()
  ├─ dragStartBoundingBox_ = layer_->transformedBoundingBox()   ★ O(n) 各レイヤ
  ├─ dragStartLocalBounds_ = layer_->localBounds()                ★ O(path構築)
  └─ snap lines cache: 全レイヤの transformedBoundingBox() 迭代  ★ O(n²)規模

TransformGizmo::handleMouseMove()
  └─ layer_->setDirty(LayerDirtyFlag::Transform)
  └─ globalEventBus().publish<LayerChangedEvent>()

ArtifactCompositionRenderController::handleMouseMove()
  └─ gizmoDragRenderTimer_.elapsed() >= kGizmoDragRenderIntervalMs (14ms)
       └─ renderOneFrame()  ★ フルレンダリング（~70fps相当）

ArtifactCompositionRenderController::renderOneFrameImpl()
  └─ 全レイヤを描画パスに通す
```

---

## ShapeLayer固有のボトルネック

### 1. localBounds() が常に QPainterPath を構築する

`ArtifactShapeLayer.cppm:528-532`:

```cpp
const QPainterPath path = buildShapePath(impl_->shapeType_, impl_->width_, impl_->height_,
                                        impl_->cornerRadius_, impl_->starPoints_,
                                        impl_->starInnerRadius_, impl_->polygonSides_)
                            .toPainterPath();
bounds = path.boundingRect();
```

Drag開始時に `transformedBoundingBox()` → `localBounds()` が2回走り、加えて全レイヤ分も走る。

### 2. transformedBoundingBox() にキャッシュなし

`ArtifactAbstractLayer.cppm:943-952`:

```cpp
QRectF ArtifactAbstractLayer::transformedBoundingBox() const {
  const QRectF localRect = localBounds();  // ★ 毎回O(path)再計算
  ...
}
```

毎フレームのtransform更新のたびにパス再構築が起きる。

### 3. ShapeLayer に高速パスがない

`ArtifactCompositionRenderController.cppm:1187-1191` で ShapeLayer は `layer->draw(renderer)` へfallthroughし、ImageLayerにあるようなフレームバッファ直接アクセス経路が通らない。

### 4. gizmoDragRenderTimer_ が14ms间隔でフルレンダリング

ImageLayer は `layerUsesSurfaceUploadForCompositionView()` によりバッファ-upload 経路で描画コスト是可绎されるが、ShapeLayer はこの路径をを持たない。

---

## 仮説まとめ

| # | 仮説 | 確信度 |
|---|------|--------|
| A | ShapeLayer::localBounds() がパス構築を毎日持ち、transform変更のたびに再計算される | 高 |
| B | Gizmoドラッグ中、14msごとにフルレンダリングが走り、ShapeLayerはバッファupload経路を持たないためコストが大きい | 高 |
| C | ドラッグ開始時に全レイヤの transformedBoundingBox() を迭代する光がシリアス | 中 |
| D | transformedBoundingBox() の結果がキャッシュされておらず、毎フレームO(n) | 高 |

---

## 次の確認アクション

1. `ShapeLayer::localBounds()` にキャッシュを導入した場合の效果測定
2. `layerUsesSurfaceUploadForCompositionView()` に ShapeLayer を追加可能か
3. gizmoドラッグ中のレンダリング间隔更长にできるか（33msなど）
4. renderOneFrameImpl() で selected layer transform only の場合に轻いパスを通す
