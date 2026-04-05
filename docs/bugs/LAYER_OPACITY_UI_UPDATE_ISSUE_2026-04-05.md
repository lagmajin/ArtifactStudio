# Opacity Property UI Update Issue

**Issue ID**: `LAYER_OPACITY_UI_UPDATE_20260405`  
**Severity**: Medium  
**Status**: Investigating  
**Date**: 2026-04-05  
**Component**: Property Widget, Composition Rendering, Layer System

---

## Problem Statement

プロパティウィジェット（Inspector）からレイヤーの Opacity 値を変更しても、コンポジションエディタの表示に反映されない。値はプロパティパネル上で更新されるが、実際の描画結果が変化しない。

---

## Symptoms

- プロパティパネルの Opacity スライダー/数値入力は即時反映される（UI 上では値が変わる）
- コンポジションエディタのレイヤー表示は不透明度の変化に追随しない
- 他のレイヤープロパティ（位置、スケール、表示/非表示）は正常に反映される
- タイムライン上でのキーフレーム設定は可能だが、再生時の Opacity 変化が画面上に見られない

---

## Investigation Findings

### 1. Data Flow Overview

```
ArtifactPropertyWidget
  └─ setLayerPropertyValue("layer.opacity", value)
       └─ ArtifactAbstractLayer::setOpacity(value)
            └─ notifyLayerMutation(this, LayerDirtyFlag::Transform, LayerDirtyReason::PropertyChanged)
                 └─ Q_EMIT changed()
                      └─ CompositionRenderController (connected via layerChangedConnections_)
                           └─ invalidateLayerSurfaceCache(layer)
                                └─ invalidateBaseComposite()
                                     └─ renderOneFrame()
```

### 2. Signal Emission Confirmed

- `ArtifactAbstractLayer::setOpacity()` 内部で `notifyLayerMutation()` を呼び出し、この関数内で `Q_EMIT changed()` を発行（`ArtifactAbstractLayer.cppm:62`）
- よって、Opacity 変更時には `changed` シグナルが確実に発行される

### 3. CompositionRenderController Connection

- `setupLayerConnectionsForComposition()` 内で各レイヤーに対して `layer->changed` シグナルを購読（1306-1321行, 1637-1644行）
- コールバックで `invalidateLayerSurfaceCache()` + `invalidateBaseComposite()` + `renderOneFrame()` を呼び出している

### 4. Rendering Pipeline Uses Opacity

- GPU パス: `blendPipeline_->blend(ctx, layerSRV, accumSRV, tempUAV, blendMode, opacity)` に opacity を渡している（2977行）
- Fallback パス: `drawLayerForCompositionView()` を通じて `finalOpacity = baseOpacity * instanceWeight` が `renderer->drawSpriteTransformed(..., finalOpacity)` に渡される（697, 722-726行）
- つまり描画コード自体は opacity 値を正しく使用している

### 5. Key Observation

Opacity 変更によって `changed` シグナルは発行され、`renderOneFrame()` が呼ばれるべきであるが、実際の描画では古い opacity 値が使われている兆候がある。

---

## Hypotheses

### H1: Render Pass Skipped (最有力)
`renderOneFrame()` 呼び出し後、何らかの条件により実際の描画処理（レイヤーループ）がスキップされている。
- `hostWidget_` が非表示
- `stopped_` フラグが立っている
- `frameOutOfRange` が true になっている
- 別の invalidaton によって throttling されている

### H2: Cache Signature Does Not Include Opacity
`buildLayerSurfaceCacheKey()` が opacity を含んでいないため、`surfaceCache_` でキャッシュヒットし、古いサーフェス画像が使い回される。ただし `drawLayerForCompositionView` 内では `layer->opacity()` を毎フレーム読むので、opacity 自体が反映されるはず。ただし、キャッシュ済みサーフェスに既に opacity がベイクされているわけではないので、この仮説は部分的。

### H3: Layer Instance Mismatch
- プロパティパネルが参照しているレイヤーと、`CompositionRenderController` が描画しているレイヤーが異なる ID/インスタンス
- `setComposition()` が呼ばれた後、別途レイヤーが置き換わった場合、`layerChangedConnections_` が再接続されていない可能性

### H4: Opacity Value Not Actually Updated
`setLayerPropertyValue()` 内の `if (propertyPath == QStringLiteral("layer.opacity"))` が文字列比较で一致しないケース（例: `.opacity` と `.opacity ` など、trimming の問題）。ただし文字列リテラルは一致しているので可能性は低い。

### H5: Draw Call Bypass for GPU Path
GPU パス（CS ブレンド）では `layer->draw()` 自体は呼ばれるが、その内部で `layer->opacity()` を参照している。しかし `blendPipeline_->blend()` に渡される opacity が `layer->opacity()` の最新値を反映していない。
実際のコードでは `const float opacity = layer->opacity() * ...` とその場で取得しているので、これはない。

### H6: Overdraw / Composition Order Inversion
以前発見したように、`allLayer()` が返す順序が戻り順（後ろから）であるのに対し、描画ループがそのまま前列挙している場合、レイヤー順序が逆転。opacity 0.5 のレイヤーが別のレイヤーにすべて隠れて見えない、といった可能性はある。

---

## Evidence Required

1. **Log at `setLayerPropertyValue` entry for opacity**
   ```cpp
   // ArtifactAbstractLayer.cppm
   if (propertyPath == QStringLiteral("layer.opacity")) {
       qDebug() << "[Layer]" << id() << "setOpacity" << value;
       ...
   }
   ```

2. **Log at `renderOneFrame` entry**
   ```cpp
   void CompositionRenderController::renderOneFrame() {
       qDebug() << "[RenderController] renderOneFrame"
                << "visible=" << (hostWidget_ && hostWidget_->isVisible())
                << "stopped=" << stopped_
                << "layersCount=" << layers.size();
   }
   ```

3. **Log before layer draw**
   ```cpp
   // In drawLayerForCompositionView, right after computing finalOpacity
   qDebug() << "[Draw] layer" << layer->id()
            << "layerOpacity=" << layer->opacity()
            << "opacityOverride=" << opacityOverride
            << "finalOpacity=" << finalOpacity;
   ```

4. **Check cache key composition**
   - `buildLayerSurfaceCacheKey()` に opacity が含まれているか確認
   - 含まれていない場合、opacity 変更でもキャッシュミスが起きず再利用される

---

## Proposed Fixes (Based on Hypotheses)

### If H1 (Render skipped):
- `renderOneFrame()` のスキップ条件を見直す
- `hostWidget_->isVisible()` ではなく、`renderOneFrame()`  unconditionally にする
- または、`changed` シグナル受信時に強制的に `hostWidget_->update()` を呼ぶ

### If H2 (Cache key missing):
- `buildLayerSurfaceCacheKey()` に opacity を加算する
  ```cpp
  签名 = ... + QString("_opacity=%1").arg(layer->opacity());
  ```

### If H3 (Connection lost):
- `setComposition()` 時に全レイヤーへのシグナル接続を確実に張り直す
- レイヤー追加/削除時に `layerChangedConnections_` を更新するリスナーを追加する

### If H4 (String mismatch):
- `setLayerPropertyValue()` の文字列比較を堅牢に（trim する、ケースを無視するなど）

### If H5 (Opacity not applied):
- `drawLayerForCompositionView()` 内の `layer->draw(renderer)` 呼び出し直前に、renderer に opacity を設定する state を追加
- または、`layer->draw()` 自体が `this->opacity()` を参照しているので、その値が確実に最新か確認

### If H6 (Order inversion):
- 描画ループで `allLayer()` の結果を `std::reverse` などで逆順にする（以前提案のlayer レンダリング順序逆転問題に対応）

---

## Related Code Paths

| File | Lines | Purpose |
|------|-------|---------|
| `ArtifactAbstractLayer.cppm` | 1544-1551 | `setOpacity()` implementation |
| `ArtifactAbstractLayer.cppm` | 55-63 | `notifyLayerMutation()` helper |
| `ArtifactCompositionRenderController.cppm` | 1306-1321, 1637-1644 | Layer `changed` signal connections |
| `ArtifactCompositionRenderController.cppm` | 1217-1226 | `invalidateLayerSurfaceCache()` |
| `ArtifactCompositionRenderController.cppm` | 2925-2985 | GPU path layer loop with opacity |
| `ArtifactCompositionRenderController.cppm` | 3166-3223 | Fallback path layer loop with opacity |
| `ArtifactCompositionRenderController.cppm` | 537-729 | `drawLayerForCompositionView()` |
| `ArtifactPropertyWidget.cppm` | 1343-1346, 916-922, 995-999 | `setLayerPropertyValue` calls |

---

## Debug Checklist

- [ ] Verify `setOpacity()` actually called with new value from property widget
- [ ] Confirm `changed` signal reaches `CompositionRenderController` lambda
- [ ] Ensure `renderOneFrame()` is invoked and not skipped
- [ ] Log actual `layer->opacity()` inside render loop
- [ ] Check if `surfaceCache_` returns stale surface despite opacity change
- [ ] Confirm composition is current (same instance) when changing property

---

## Status

 awaiting logs to pinpoint exact failure point.

---

## Next Steps

1. Add debug logging to key locations (listed above)
2. Reproduce: change Opacity in property panel, observe console output
3. Verify signal propagation end-to-end
4. Apply minimal fix once root cause identified

---

**Last Updated**: 2026-04-05  
**Owner**: Kilo (AI)
