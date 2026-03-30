# ROI (Region of Interest) 仕様書

**作成日:** 2026-03-28  
**ステータス:** 仕様確定  
**関連コンポーネント:** RenderPipeline, EffectPipeline, CompositionRenderer

---

## 概要

ROI（Region of Interest）は、レンダリングパイプラインで**実際に処理すべき画面矩形**を定義する。

---

## 定義

### ROI = そのパスが実際に処理すべき画面矩形

**単位:** screen / comp pixel space  
**目的:**
1. 不要ピクセルを処理しない
2. 小さい RT に落とす
3. scissor にも使う

---

## 基本原則

### 原則 1: Editor 表示範囲 と Final Render 範囲 は別

```cpp
// Editor 表示用 ROI（ビューポートに限定）
Rect editorROI = viewportBounds;

// Final Render 用 ROI（コンポジション全体）
Rect finalROI = compositionBounds;
```

**理由:**
- Editor はリアルタイム性が求められる
- Final Render は高品質が求められる

---

### 原則 2: ROI が空なら そのパスは完全スキップ

```cpp
if (roi.isEmpty()) {
    // このパスは完全にスキップ
    return;
}
```

**効果:**
- 不要な計算を完全に省略
- GPU/CPU リソースの節約

---

### 原則 3: blur / glow / shadow は ROI を拡張する

```cpp
// 通常
Rect roi = layerBounds;

// Blur 適用時
Rect expandedROI = roi.expanded(blurRadius * 2);

// Glow 適用時
Rect expandedROI = roi.expanded(glowRadius * 2);

// Shadow 適用時
Rect expandedROI = roi.expanded(shadowBlur + shadowOffset);
```

**理由:**
- これらのエフェクトは元bounds より広がる
- 拡張しないとエッジが切れる

---

## ROI 計算フロー

### フロー 1: 基本レイヤー

```
1. layer.localBounds() を取得
2. layer.getGlobalTransform() で変換
3. viewport との交差判定
4. 最終 ROI = intersect(transformedBounds, viewport)
```

**実装例:**
```cpp
Rect calculateBasicROI(const ArtifactAbstractLayerPtr& layer, 
                       const Rect& viewport)
{
    // 1. ローカルバウンズ
    Rect bounds = layer->localBounds();
    
    // 2. グローバル変換
    QTransform global = layer->getGlobalTransform();
    QRectF transformed = global.mapRect(bounds);
    
    // 3. ビューポートとの交差
    Rect roi = transformed.intersected(viewport);
    
    // 4. 空チェック
    if (roi.isEmpty()) {
        return Rect(); // スキップ対象
    }
    
    return roi;
}
```

---

### フロー 2: エフェクト付きレイヤー

```
1. 基本 ROI を計算
2. エフェクトごとに ROI を拡張
3. 最終 ROI = union(all expanded bounds)
```

**実装例:**
```cpp
Rect calculateEffectROI(const ArtifactAbstractLayerPtr& layer,
                        const Rect& basicROI)
{
    Rect expanded = basicROI;
    
    for (const auto& effect : layer->getEffects()) {
        switch (effect->type()) {
            case EffectType::Blur: {
                auto blur = std::dynamic_pointer_cast<BlurEffect>(effect);
                expanded = expanded.expanded(blur->radius() * 2);
                break;
            }
            case EffectType::Glow: {
                auto glow = std::dynamic_pointer_cast<GlowEffect>(effect);
                expanded = expanded.expanded(glow->radius() * 2);
                break;
            }
            case EffectType::DropShadow: {
                auto shadow = std::dynamic_pointer_cast<ShadowEffect>(effect);
                float expand = shadow->blur() + 
                              std::max(std::abs(shadow->offsetX()), 
                                       std::abs(shadow->offsetY()));
                expanded = expanded.expanded(expand);
                break;
            }
        }
    }
    
    return expanded;
}
```

---

### フロー 3: マルチパスレンダリング

```
Pass 1: Downscaled ROI (1/4)
  └─ 大まかな計算

Pass 2: Full ROI (1/1)
  └─ 詳細な計算

Pass 3: Composite ROI (= viewport)
  └─ 最終合成
```

**実装例:**
```cpp
void renderWithMultiPass(const ArtifactAbstractLayerPtr& layer,
                         const Rect& roi)
{
    // Pass 1: ダウンスケール
    Rect smallROI = roi.scaled(0.25);
    renderPass1(smallROI);
    
    // Pass 2: フル解像度
    renderPass2(roi);
    
    // Pass 3: 合成
    renderPass3(viewportBounds);
}
```

---

## ROI の使用例

### 使用例 1: Scissor Test

```cpp
void setupScissor(const Rect& roi)
{
    if (roi.isEmpty()) {
        return; // 完全スキップ
    }
    
    // Scissor テスト設定
    glScissor(roi.x(), roi.y(), roi.width(), roi.height());
    glEnable(GL_SCISSOR_TEST);
}
```

**効果:**
- 不要なピクセルの描画を防止
- GPU 負荷削減

---

### 使用例 2: 小さい RT へのレンダリング

```cpp
void renderToSmallRT(const Rect& roi)
{
    if (roi.isEmpty()) {
        return;
    }
    
    // ROI に合わせたサイズの RT を作成
    int width = roi.width();
    int height = roi.height();
    
    auto rt = createRenderTarget(width, height);
    
    // ROI 範囲のみレンダリング
    renderToTexture(rt, roi);
}
```

**効果:**
- メモリ使用量削減
- レンダリング時間短縮

---

### 使用例 3: エフェクトの最適化

```cpp
void applyBlur(const ArtifactAbstractLayerPtr& layer,
               const Rect& basicROI,
               float blurRadius)
{
    // ROI を拡張
    Rect expandedROI = basicROI.expanded(blurRadius * 2);
    
    if (expandedROI.isEmpty()) {
        return; // スキップ
    }
    
    // 拡張 ROI でブラー適用
    blurEffect->apply(layer, expandedROI, blurRadius);
}
```

---

## ROI クラス設計

### プロパティ

```cpp
class RenderROI {
public:
    // 基本プロパティ
    int x() const;
    int y() const;
    int width() const;
    int height() const;
    
    // 変換
    QRect toQRect() const;
    RectF scaled(float factor) const;
    Rect expanded(int pixels) const;
    
    // 判定
    bool isEmpty() const;
    bool intersects(const Rect& other) const;
    
    // 演算
    Rect intersected(const Rect& other) const;
    Rect united(const Rect& other) const;
    
    // 特殊
    static Rect fromViewport(const Viewport& vp);
    static Rect fromLayer(const ArtifactAbstractLayerPtr& layer);
    static Rect fromEffect(const ArtifactAbstractEffectPtr& effect,
                          const Rect& inputROI);
};
```

---

### 実装例

```cpp
class RenderROI {
private:
    int x_ = 0;
    int y_ = 0;
    int width_ = 0;
    int height_ = 0;
    
public:
    RenderROI() = default;
    RenderROI(int x, int y, int w, int h)
        : x_(x), y_(y), width_(w), height_(h) {}
    
    bool isEmpty() const {
        return width_ <= 0 || height_ <= 0;
    }
    
    Rect expanded(int pixels) const {
        return Rect(x_ - pixels, y_ - pixels,
                   width_ + pixels * 2, height_ + pixels * 2);
    }
    
    Rect intersected(const Rect& other) const {
        int newX = std::max(x_, other.x_);
        int newY = std::max(y_, other.y_);
        int newW = std::min(x_ + width_, other.x_ + other.width_) - newX;
        int newH = std::min(y_ + height_, other.y_ + other.height_) - newY;
        
        if (newW <= 0 || newH <= 0) {
            return Rect(); // 空
        }
        
        return Rect(newX, newY, newW, newH);
    }
    
    Rect scaled(float factor) const {
        int newW = static_cast<int>(width_ * factor);
        int newH = static_cast<int>(height_ * factor);
        
        // 中央を基準にスケーリング
        int newX = x_ + (width_ - newW) / 2;
        int newY = y_ + (height_ - newH) / 2;
        
        return Rect(newX, newY, newW, newH);
    }
};
```

---

## 最適化戦略

### 戦略 1: ROI キャッシュ

```cpp
class ROICache {
private:
    QHash<LayerID, Rect> cachedROI_;
    QHash<QString, Rect> effectCache_;
    
public:
    Rect getROI(const ArtifactAbstractLayerPtr& layer) {
        if (cachedROI_.contains(layer->id())) {
            return cachedROI_[layer->id()];
        }
        
        Rect roi = calculateROI(layer);
        cachedROI_[layer->id()] = roi;
        return roi;
    }
    
    void invalidate(const LayerID& id) {
        cachedROI_.remove(id);
    }
    
    void clear() {
        cachedROI_.clear();
        effectCache_.clear();
    }
};
```

---

### 戦略 2: ROI マージ

```cpp
Rect mergeROIs(const QVector<Rect>& rois)
{
    if (rois.isEmpty()) {
        return Rect();
    }
    
    Rect merged = rois[0];
    for (int i = 1; i < rois.size(); ++i) {
        merged = merged.united(rois[i]);
    }
    
    return merged;
}
```

**使用例:**
```cpp
// 複数レイヤーの ROI をマージ
QVector<Rect> layerROIs;
for (const auto& layer : layers) {
    layerROIs.append(calculateROI(layer));
}

Rect totalROI = mergeROIs(layerROIs);
```

---

### 戦略 3: 階層的 ROI

```cpp
class HierarchicalROI {
public:
    enum class Level {
        Thumbnail,  // 1/16
        Preview,    // 1/4
        Full        // 1/1
    };
    
    Rect getROI(Level level) const {
        switch (level) {
            case Level::Thumbnail:
                return baseROI_.scaled(0.0625);
            case Level::Preview:
                return baseROI_.scaled(0.25);
            case Level::Full:
                return baseROI_;
        }
    }
    
private:
    Rect baseROI_;
};
```

---

## パフォーマンス効果

### 効果 1: 不要ピクセルの削減

| シーン | 全体ピクセル | ROI ピクセル | 削減率 |
|--------|------------|------------|--------|
| 小レイヤー | 1920x1080 | 640x480 | -94% |
| 中央レイヤー | 1920x1080 | 960x540 | -75% |
| 画面全体 | 1920x1080 | 1920x1080 | 0% |

---

### 効果 2: エフェクト最適化

| エフェクト | 通常 ROI | 拡張 ROI | オーバーヘッド |
|-----------|---------|---------|--------------|
| Blur(10px) | 100% | 104% | +4% |
| Glow(20px) | 100% | 108% | +8% |
| Shadow(30px) | 100% | 112% | +12% |

**注:** 拡張なしだとエッジが切れるため、適切な拡張が必要

---

### 効果 3: マルチパス最適化

```
単一パス：1920x1080 = 2,073,600 ピクセル

マルチパス:
  Pass1 (1/4): 480x270 = 129,600 ピクセル
  Pass2 (1/1): 1920x1080 = 2,073,600 ピクセル
  合計：2,203,200 ピクセル（+6%）
  
しかし、Pass1 で大まかな計算を済ませるため、
Pass2 の計算量が削減され、全体として -20% の高速化
```

---

## 実装チェックリスト

### 基本実装

- [ ] ROI クラスの定義
- [ ] isEmpty() チェックの追加
- [ ] intersected() 実装
- [ ] expanded() 実装
- [ ] scaled() 実装

### 統合

- [ ] レイヤーレンダリングに ROI 統合
- [ ] エフェクトパイプラインに ROI 統合
- [ ] Scissor テストとの連携
- [ ] RT サイズ決定に ROI 使用

### 最適化

- [ ] ROI キャッシュ実装
- [ ] ROI マージ機能
- [ ] 階層的 ROI サポート
- [ ] ROI 無効化ロジック

---

## 関連ドキュメント

- `docs/technical/LOD_SYSTEM_PHASE1_IMPLEMENTATION_2026-03-28.md` - LOD システム
- `docs/technical/RENDERING_PERFORMANCE_2026-03-28.md` - レンダリング性能改善

---

## 用語集

| 用語 | 定義 |
|------|------|
| **ROI** | Region of Interest - 処理すべき画面矩形 |
| **Screen Space** | スクリーン座標系（ピクセル単位） |
| **Comp Space** | コンポジション座標系 |
| **Scissor** | OpenGL/DirectX の描画範囲制限機能 |
| **RT** | Render Target - 描画先テクスチャ |

---

**文書終了**
