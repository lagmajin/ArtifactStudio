# ROI (Region of Interest) 実装仕様書

**作成日:** 2026-03-28  
**ステータス:** 実装仕様確定  
**関連コンポーネント:** RenderPipeline, EffectPipeline, CompositionRenderer, ArtifactIRenderer

---

## 1. 座標系

### 基本座標系

**ROI は `QRectF` または独自矩形**

```cpp
// 基本は QRectF を使用
using RenderROI = QRectF;

// または独自構造体
struct RenderROI {
    float x, y, width, height;
};
```

---

### 座標系の変換

```
基本は composition pixel coordinates
    ↓
renderer に渡す直前に
    ↓
viewport / scissor 座標へ変換
```

**フロー:**

```cpp
// 1. Composition 座標系で計算
RenderROI compROI = calculateCompositionROI(layer);

// 2. Viewport 座標系に変換
RenderROI viewportROI = compToViewport(compROI, renderer);

// 3. Scissor 座標系に変換
RenderROI scissorROI = viewportToScissor(viewportROI, renderer);
```

---

### 座標系の変換実装

```cpp
class RenderCoordinateConverter {
public:
    // Composition → Viewport
    static QRectF compToViewport(const QRectF& compRect,
                                 ArtifactIRenderer* renderer)
    {
        float zoom = renderer->getZoom();
        QPointF pan = renderer->getPan();
        
        return QRectF(
            compRect.x() * zoom + pan.x(),
            compRect.y() * zoom + pan.y(),
            compRect.width() * zoom,
            compRect.height() * zoom
        );
    }
    
    // Viewport → Scissor (ピクセル座標)
    static QRectF viewportToScissor(const QRectF& viewportRect,
                                    ArtifactIRenderer* renderer)
    {
        QSize viewportSize = renderer->getViewportSize();
        
        return QRectF(
            viewportRect.x(),
            viewportSize.height() - viewportRect.y() - viewportRect.height(), // Y 反転
            viewportRect.width(),
            viewportRect.height()
        );
    }
};
```

---

## 2. モード

### RenderMode 列挙型

```cpp
enum class RenderMode {
    Editor,   // エディタ表示（リアルタイム重視）
    Preview,  // プレビュー表示（バランス）
    Final     // 最終レンダリング（品質重視）
};
```

---

### モード別設定

```cpp
struct RenderModeSettings {
    // ROI 計算
    float roiExpansionFactor;  // ROI 拡張係数
    
    // 解像度
    float resolutionScale;     // 解像度スケール
    
    // 最適化
    bool useScissorTest;       // Scissor テスト使用
    bool useROICache;          // ROI キャッシュ使用
    bool skipEmptyROI;         // 空 ROI スキップ
    
    // 品質
    int sampleCount;           // サンプル数
    bool enableEffects;        // エフェクト有効
};

// デフォルト設定
RenderModeSettings getModeSettings(RenderMode mode)
{
    switch (mode) {
        case RenderMode::Editor:
            return {
                .roiExpansionFactor = 0.0f,      // 拡張なし
                .resolutionScale = 0.5f,         // 1/2 解像度
                .useScissorTest = true,
                .useROICache = true,
                .skipEmptyROI = true,
                .sampleCount = 1,
                .enableEffects = false           // エフェクト無効
            };
            
        case RenderMode::Preview:
            return {
                .roiExpansionFactor = 1.0f,      // 1px 拡張
                .resolutionScale = 1.0f,         // フル解像度
                .useScissorTest = true,
                .useROICache = true,
                .skipEmptyROI = true,
                .sampleCount = 2,
                .enableEffects = true            // エフェクト有効
            };
            
        case RenderMode::Final:
            return {
                .roiExpansionFactor = 2.0f,      // 2px 拡張
                .resolutionScale = 1.0f,         // フル解像度
                .useScissorTest = false,         // 全画面レンダリング
                .useROICache = false,            // キャッシュなし
                .skipEmptyROI = false,           // 全て処理
                .sampleCount = 4,
                .enableEffects = true            // エフェクト有効
            };
    }
}
```

---

### モードの使用例

```cpp
void renderLayer(const ArtifactAbstractLayerPtr& layer,
                 RenderMode mode,
                 ArtifactIRenderer* renderer)
{
    auto settings = getModeSettings(mode);
    
    // ROI 計算
    RenderROI roi = calculateROI(layer, settings.roiExpansionFactor);
    
    // 空 ROI チェック
    if (settings.skipEmptyROI && roi.isEmpty()) {
        return; // スキップ
    }
    
    // Scissor テスト設定
    if (settings.useScissorTest) {
        setupScissor(roi, renderer);
    }
    
    // レンダリング
    layer->draw(renderer);
}
```

---

## 3. ROI 構造体

### 基本構造

```cpp
struct RenderROI {
    QRectF rect;  // 矩形（composition 座標系）
    
    // 空チェック
    bool isEmpty() const {
        return rect.isEmpty() || 
               rect.width() <= 0.0 || 
               rect.height() <= 0.0;
    }
    
    // 面積
    float area() const {
        return rect.width() * rect.height();
    }
    
    // 拡張
    RenderROI expanded(float pixels) const {
        return RenderROI {
            rect.adjusted(-pixels, -pixels, pixels, pixels)
        };
    }
    
    // 交差
    RenderROI intersected(const RenderROI& other) const {
        return RenderROI {
            rect.intersected(other.rect)
        };
    }
    
    // 結合
    RenderROI united(const RenderROI& other) const {
        return RenderROI {
            rect.united(other.rect)
        };
    }
    
    // スケーリング
    RenderROI scaled(float factor) const {
        float newW = rect.width() * factor;
        float newH = rect.height() * factor;
        float newX = rect.x() + (rect.width() - newW) / 2.0f;
        float newY = rect.y() + (rect.height() - newH) / 2.0f;
        
        return RenderROI {
            QRectF(newX, newY, newW, newH)
        };
    }
};
```

---

### 使用例

```cpp
// ROI 作成
RenderROI roi { QRectF(0, 0, 1920, 1080) };

// 空チェック
if (roi.isEmpty()) {
    return; // スキップ
}

// 拡張（Blur 用）
RenderROI expandedROI = roi.expanded(10.0f);

// 交差（ビューポート制限）
RenderROI clippedROI = expandedROI.intersected(viewportROI);

// スケーリング（プレビュー用）
RenderROI previewROI = clippedROI.scaled(0.5f);
```

---

## 4. 各レイヤーが持つべきもの

### 必須メソッド

```cpp
class ArtifactAbstractLayer {
public:
    // 1. スクリーンバウンズ（composition 座標系）
    virtual QRectF screenBounds() const = 0;
    
    // 2. 指定フレームでアクティブか
    virtual bool isActiveAt(int64_t frame) const = 0;
    
    // 3. 実効不透明度（アニメーション込み）
    virtual float effectiveOpacity() const = 0;
    
    // 4. 現在のコンテキストで表示可能か
    virtual bool visibleInContext() const = 0;
};
```

---

### デフォルト実装

```cpp
// screenBounds() - デフォルト実装
QRectF ArtifactAbstractLayer::screenBounds() const
{
    // ローカルバウンズをグローバル変換
    QRectF local = localBounds();
    QTransform global = getGlobalTransform();
    return global.mapRect(local);
}

// isActiveAt() - デフォルト実装
bool ArtifactAbstractLayer::isActiveAt(int64_t frame) const
{
    return isVisible() && 
           frame >= inPoint().framePosition() && 
           frame < outPoint().framePosition();
}

// effectiveOpacity() - デフォルト実装
float ArtifactAbstractLayer::effectiveOpacity() const
{
    // アニメーション付き不透明度
    const auto time = RationalTime(currentFrame(), 30);
    const auto& prop = getProperty(QStringLiteral("layer.opacity"));
    
    if (prop.isAnimatable() && !prop.getKeyFrames().isEmpty()) {
        QVariant value = prop.interpolateValue(time);
        return value.toFloat();
    }
    
    return opacity();
}

// visibleInContext() - デフォルト実装
bool ArtifactAbstractLayer::visibleInContext() const
{
    // 基本の可視性
    if (!isVisible()) return false;
    
    // 不透明度チェック
    if (effectiveOpacity() <= 0.0f) return false;
    
    // Solo チェック
    if (auto comp = composition().lock()) {
        bool hasSolo = false;
        for (const auto& layer : comp->allLayer()) {
            if (layer->isSolo()) {
                hasSolo = true;
                break;
            }
        }
        
        if (hasSolo && !isSolo()) {
            return false;
        }
    }
    
    return true;
}
```

---

### ROI 計算での使用

```cpp
RenderROI calculateLayerROI(const ArtifactAbstractLayerPtr& layer,
                            int64_t currentFrame)
{
    // 1. アクティブチェック
    if (!layer->isActiveAt(currentFrame)) {
        return RenderROI { QRectF() }; // 空 ROI
    }
    
    // 2. 可視性チェック
    if (!layer->visibleInContext()) {
        return RenderROI { QRectF() }; // 空 ROI
    }
    
    // 3. 不透明度チェック
    if (layer->effectiveOpacity() <= 0.0f) {
        return RenderROI { QRectF() }; // 空 ROI
    }
    
    // 4. スクリーンバウンズを取得
    QRectF bounds = layer->screenBounds();
    
    // 5. ROI 作成
    return RenderROI { bounds };
}
```

---

## 5. 各エフェクトが持つべきもの

### 必須メソッド（アプローチ 1: 簡易版）

```cpp
class ArtifactAbstractEffect {
public:
    // 出力 ROI から必要な入力 ROI を計算
    // または
    // 入力 ROI を拡張して出力 ROI を計算
    
    // アプローチ 1: ExpandOutputROI（推奨・簡易）
    virtual RenderROI expandOutputROI(const RenderROI& inputROI) const = 0;
};
```

---

### 実装例（アプローチ 1）

```cpp
// Blur エフェクト
class BlurEffect : public ArtifactAbstractEffect {
public:
    RenderROI expandOutputROI(const RenderROI& inputROI) const override
    {
        // Blur 半径分拡張
        return inputROI.expanded(blurRadius_ * 2.0f);
    }
    
private:
    float blurRadius_ = 10.0f;
};

// Glow エフェクト
class GlowEffect : public ArtifactAbstractEffect {
public:
    RenderROI expandOutputROI(const RenderROI& inputROI) const override
    {
        // Glow 半径分拡張
        return inputROI.expanded(glowRadius_ * 2.0f);
    }
    
private:
    float glowRadius_ = 20.0f;
};

// Drop Shadow エフェクト
class DropShadowEffect : public ArtifactAbstractEffect {
public:
    RenderROI expandOutputROI(const RenderROI& inputROI) const override
    {
        // シャドウブラー + オフセット分拡張
        float expand = blurRadius_ + 
                      std::max(std::abs(offsetX_), std::abs(offsetY_));
        return inputROI.expanded(expand);
    }
    
private:
    float blurRadius_ = 10.0f;
    float offsetX_ = 5.0f;
    float offsetY_ = 5.0f;
};
```

---

### 使用例

```cpp
RenderROI calculateEffectROI(const ArtifactAbstractLayerPtr& layer,
                             const RenderROI& baseROI)
{
    RenderROI roi = baseROI;
    
    // 各エフェクトの ROI 拡張を適用
    for (const auto& effect : layer->getEffects()) {
        roi = effect->expandOutputROI(roi);
    }
    
    return roi;
}
```

---

### アプローチ 2: 双方向（上級者向け）

```cpp
class ArtifactAbstractEffect {
public:
    // 出力→入力（逆方向）
    virtual RenderROI computeRequiredInputROI(const RenderROI& outputROI) const = 0;
    
    // 入力→出力（順方向）
    virtual RenderROI expandOutputROI(const RenderROI& inputROI) const = 0;
};
```

**実装例:**

```cpp
class BlurEffect : public ArtifactAbstractEffect {
public:
    // 出力 ROI から必要な入力 ROI を計算
    RenderROI computeRequiredInputROI(const RenderROI& outputROI) const override
    {
        // 出力と同じで OK（Blur は入力全体を参照）
        return outputROI;
    }
    
    // 入力 ROI から出力 ROI を計算
    RenderROI expandOutputROI(const RenderROI& inputROI) const override
    {
        return inputROI.expanded(blurRadius_ * 2.0f);
    }
};
```

---

## 統合実装例

### ROI パイプライン

```cpp
class RenderPipeline {
public:
    void renderLayer(const ArtifactAbstractLayerPtr& layer,
                     RenderMode mode,
                     ArtifactIRenderer* renderer)
    {
        auto settings = getModeSettings(mode);
        int64_t currentFrame = renderer->currentFrame();
        
        // 1. 基本 ROI 計算
        RenderROI roi = calculateLayerROI(layer, currentFrame);
        
        // 2. 空 ROI チェック
        if (settings.skipEmptyROI && roi.isEmpty()) {
            return; // 完全スキップ
        }
        
        // 3. エフェクトによる ROI 拡張
        roi = calculateEffectROI(layer, roi);
        
        // 4. ビューポートとの交差
        RenderROI viewportROI = getViewportROI(renderer);
        roi = roi.intersected(viewportROI);
        
        // 5. 再チェック
        if (settings.skipEmptyROI && roi.isEmpty()) {
            return; // 完全スキップ
        }
        
        // 6. 解像度スケーリング
        if (settings.resolutionScale != 1.0f) {
            roi = roi.scaled(settings.resolutionScale);
        }
        
        // 7. Scissor テスト設定
        if (settings.useScissorTest) {
            setupScissor(roi, renderer);
        }
        
        // 8. レンダリング
        layer->draw(renderer);
    }
};
```

---

## チェックリスト

### レイヤー実装

- [ ] `screenBounds()` 実装
- [ ] `isActiveAt(frame)` 実装
- [ ] `effectiveOpacity()` 実装
- [ ] `visibleInContext()` 実装

### エフェクト実装

- [ ] `expandOutputROI(inputROI)` 実装
- [ ] 適切な拡張量の計算
- [ ] 空 ROI の処理

### パイプライン統合

- [ ] ROI 計算フロー実装
- [ ] 空 ROI スキップ実装
- [ ] Scissor テスト連携
- [ ] 座標系変換実装

---

## 関連ドキュメント

- `docs/technical/ROI_SPECIFICATION_2026-03-28.md` - ROI 仕様書
- `docs/technical/LOD_SYSTEM_PHASE1_IMPLEMENTATION_2026-03-28.md` - LOD システム

---

**文書終了**
