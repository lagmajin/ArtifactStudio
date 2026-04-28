# ShapeLayer Gizmo Drag 追加調査（修正案2）

Date: 2026-04-25

## 现状分析

### localBounds キャッシュ導入後の狀況

| 原因 | 対応状況 |
|------|----------|
| localBounds() のパス構築再計算 | ✅ 対応済み |
| gizmoDragRenderTimer 14ms→33ms | ✅ 対応済み |
| useCachePipeline() が-simple shapesでfalse | ❌ 未対応 |

### 残りのボトルネック

`useCachePipeline()` が simple shapes (Rect/Ellipse/Triangle/Star/Polygon) で `false` を返すため、`draw()` が毎フレーム `buildRenderablePoints()` + `drawSolidPolygonLocal/drawThickLineLocal` を呼ぶ。

```
ShapeLayer::draw() [simple shape時]
  ├─ buildRenderablePoints()  ★ 毎フレーム走る
  ├─ for each point: mapPoint(transform, p)  ★ transform行列適用
  ├─ renderer->drawSolidPolygonLocal()  ★  fill描画（矩形=1回）
  └─ renderer->drawThickLineLocal()  ★  stroke描画（矩形=4辺）
```

ImageLayer など他のレイヤ 타입は `hasCurrentFrameBuffer()` で GPU バッファ持有を確認し、`drawSpriteTransformed()` で高速描画できるが、ShapeLayer にはこの路径がない。

### simple shape でも QImage キャッシュを強制する案

`useCachePipeline()` を拡張し、「描绘內容が変化していない場合は常に QImage キャッシュを使う」方式にする。描绘內容＝シェイプタイプ・サイズ・パラメータ。transform は含まない。

```cpp
// ArtifactShapeLayer.cppm - Impl

mutable QImage cachedImage_;
bool cacheDirty_ = true;
// 新規: simple shape でも内容をキャッシュするフラグ
bool forceCachePipeline_ = false;
```

```cpp
bool useCachePipeline() const {
  return forceCachePipeline_ ||  // 新規: ドラッグ中等に強制有効化
         customPathVertices_.size() >= 3 ||
         strokeCap_ != StrokeCap::Flat ||
         strokeJoin_ != StrokeJoin::Miter ||
         strokeAlign_ != StrokeAlign::Center ||
         !dashPattern_.empty();
}
```

问题是transformが变化すると描いた內容も变化するので、缓存が陈腐になる。単純な解法：

**方案A**: 描绘內容キャッシュ + transformはdrawSpriteTransformedにのみ依赖（描绘缓存이미지には	transform 不要）

`drawSpriteTransformed(transform, cachedImage, ...)` にすれば、transformは GPU 側で適用される。

**方案B**: `buildRenderablePoints()` 結果を캐싱する（transformには无关系）

描绘內容である points 配列だけを先に計算して缓存、以後 transform 適用は各ポイント마다 `mapPoint()` 就行。

### 推奨案: 方案A に近い简单な改进

`useCachePipeline()` が `true` を返す狀態では `rebuildCache()` が走る。simple shape でも `useCachePipeline()=true` にした場合、`cachedImage_` に描绘结果是キャッシュされ、`drawSpriteTransformed()` で transform 込みで GPU に描画できる。

ドラッグ中に限らず、常時 simple shape てもキャッシュを有効化すれば品率は上がるが、シェイプ內容変更のたびにキャッシュ再構築が必要。

**IMPLEMENT:**

1. `Impl` に `bool shapeContentCacheValid_ = false` を追加
2. `localBounds()` 陪同決定的に `buildRenderablePoints()` 結果を `cachedShapePoints_` に缓存する（描绘內容だけなので transform 无关系）
3. `draw()` では `buildRenderablePoints()` ではなく `cachedShapePoints_` を使う
4. シェイプ內容变化時（setter呼叫時）に `shapeContentCacheValid_ = false` + `localBoundsCacheDirty_ = true`

これで `buildRenderablePoints()` の Call 回数が劇的に減る。
