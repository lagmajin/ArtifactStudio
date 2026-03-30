# LOD システム実現可能性分析レポート

**作成日:** 2026-03-28  
**ステータス:** 調査完了  
**関連コンポーネント:** PrimitiveRenderer2D, ArtifactIRenderer, ViewportTransformer

---

## 概要

LOD（Level of Detail）システムの実現可能性を、既存のレンダリングシステムを基に分析した。

**結論: 部分的に実装可能（2D レイヤー向け）**

---

## 既存の LOD 関連機能

### ✅ 既に実装済みの機能

#### 1. **ズームレベル検出**

**場所:** `Artifact/src/Render/PrimitiveRenderer2D.cppm`

```cpp
const float zoom = std::max(viewportCB.zoom, 0.001f);
```

**利用箇所:** 18 ヶ所  
**機能:** 現在のビューポートズームレベルを取得可能

---

#### 2. **ビューポート/キャンバスサイズ管理**

**場所:** `ArtifactCore/src/Transform/ViewportTransformer.cppm`

```cpp
void ViewportTransformer::SetViewportSize(float w, float h);
void ViewportTransformer::SetCanvasSize(float w, float h);
float2 ViewportTransformer::GetViewportSize() const;
float2 ViewportTransformer::GetCanvasSize() const;
```

**機能:** 
- ビューポート解像度取得可能
- キャンバス解像度取得可能
- ズーム倍率取得可能

---

#### 3. **距離ベース LOD（シェーダー）**

**場所:** `Artifact/shaders/ShaderInterop_Weather.h`

```hlsl
float LODDistance; // After a certain distance, noises will get higher LOD
float LODMin;

// LOD 計算関数
inline float get_lod(in uint2 dim, in float2 uv_dx, in float2 uv_dy)
{
    // DirectX LOD 計算仕様に基づく
    // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#7.18.11%20LOD%20Calculations
}
```

**機能:**
- 距離ベース LOD 計算実装済み
- ミップマップ LOD クランプ実装済み
- 仮想テクスチャ LOD 実装済み

---

#### 4. **サンプラー LOD 設定**

**場所:** `Artifact/src/Render/ShaderManager.cppm`

```cpp
spriteSamplerDesc.MipLODBias = 0.0f;
spriteSamplerDesc.MinLOD = 0.0f;
spriteSamplerDesc.MaxLOD = FLT_MAX;
```

**機能:**
- LOD バイアス設定可能
- 最小 LOD 設定可能
- 最大 LOD 設定可能

---

#### 5. **Diligent Engine LOD 機能**

**場所:** `libs/DiligentEngine/DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h`

```cpp
Float32 MipLODBias DEFAULT_INITIALIZER(0);
float MinLOD DEFAULT_INITIALIZER(0);
float MaxLOD DEFAULT_INITIALIZER(+3.402823466e+38F);
```

**機能:**
- フル機能の LOD サポート
- ミップマップ自動生成
- 距離ベース LOD 選択

---

## 実現可能な LOD アプローチ

### アプローチ 1: ズームベース LOD（推奨）⭐⭐⭐

**概要:** ズームレベルに応じて詳細度を切り替え

```cpp
class LODManager {
public:
    enum class DetailLevel {
        Low,    // ズーム 0-25%
        Medium, // ズーム 25-75%
        High    // ズーム 75-100%
    };
    
    DetailLevel getDetailLevel(float zoom) const {
        if (zoom < 0.25f) return DetailLevel::Low;
        if (zoom < 0.75f) return DetailLevel::Medium;
        return DetailLevel::High;
    }
};
```

**実装コスト:** 4-6h  
**効果:** 中（描画負荷削減）

---

### アプローチ 2: 距離ベース LOD（3D レイヤー）⭐⭐

**概要:** 3D 空間での距離に応じて詳細度を切り替え

```cpp
class LODCalculator {
public:
    float calculateLOD(float distance, float minDistance, float maxDistance) const {
        float t = (distance - minDistance) / (maxDistance - minDistance);
        return std::clamp(t, 0.0f, 1.0f);
    }
};
```

**実装コスト:** 8-12h  
**効果:** 大（3D シーンでの最適化）

---

### アプローチ 3: スクリーンスペース LOD⭐⭐

**概要:** スクリーン上でのサイズに応じて詳細度を切り替え

```cpp
class ScreenSpaceLOD {
public:
    float getScreenSize(const QRectF& bounds, float zoom) const {
        return bounds.width() * zoom;
    }
    
    DetailLevel getDetailLevel(float screenSize) const {
        if (screenSize < 50.0f) return DetailLevel::Low;
        if (screenSize < 200.0f) return DetailLevel::Medium;
        return DetailLevel::High;
    }
};
```

**実装コスト:** 6-8h  
**効果:** 中

---

## 具体的な実装例

### 例 1: 2D レイヤー向け LOD

```cpp
// ArtifactAbstractLayer に追加
enum class DetailLevel {
    Low,    // 簡略化された描画
    Medium, // 標準的な描画
    High    // 高詳細な描画
};

void ArtifactAbstractLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
    switch (lod) {
        case DetailLevel::Low:
            // 簡略化：アウトラインのみ
            drawOutline(renderer);
            break;
            
        case DetailLevel::Medium:
            // 標準：フラットカラー
            drawFlat(renderer);
            break;
            
        case DetailLevel::High:
            // 高詳細：フルテクスチャ+ エフェクト
            drawFull(renderer);
            break;
    }
}

// CompositionRenderController で使用
void CompositionRenderController::renderOneFrame() {
    float zoom = renderer_->getZoom();
    
    for (const auto& layer : layers) {
        DetailLevel lod = getDetailLevel(zoom);
        layer->drawLOD(renderer_.get(), lod);
    }
}
```

**実装コスト:** 8-12h  
**効果:** 描画コスト 30-50% 削減

---

### 例 2: テクスチャ解像度 LOD

```cpp
// ArtifactImageLayer に追加
QImage ArtifactImageLayer::getLODTexture(float zoom) {
    switch (getDetailLevel(zoom)) {
        case DetailLevel::Low:
            return thumbnail_;  // サムネイル（256x256）
            
        case DetailLevel::Medium:
            return mediumRes_;  // 中解像度（1024x1024）
            
        case DetailLevel::High:
            return original_;   // 原寸（4096x4096）
            
        default:
            return original_;
    }
}
```

**実装コスト:** 6-8h  
**効果:** メモリ使用量 50-75% 削減

---

### 例 3: パーティクル LOD

```cpp
// ArtifactParticleLayer に追加
void ArtifactParticleLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
    int particleCount = particles_.size();
    
    switch (lod) {
        case DetailLevel::Low:
            // 10% のパーティクルのみ描画
            for (int i = 0; i < particleCount; i += 10) {
                drawParticle(renderer, particles_[i], false);
            }
            break;
            
        case DetailLevel::Medium:
            // 50% のパーティクルを描画
            for (int i = 0; i < particleCount; i += 2) {
                drawParticle(renderer, particles_[i], false);
            }
            break;
            
        case DetailLevel::High:
            // 全パーティクルを描画
            for (const auto& particle : particles_) {
                drawParticle(renderer, particle, true);
            }
            break;
    }
}
```

**実装コスト:** 4-6h  
**効果:** 描画コスト 50-90% 削減

---

## 技術的課題

### 課題 1: LOD 切り替えのポップ

**問題:** LOD 切り替え時に視覚的なポップが発生

**解決策:**
```cpp
// フェードイン/アウトで滑らかに切り替え
float alpha = calculateLODAlpha(distance, switchDistance, fadeRange);
setColor(alpha);
```

---

### 課題 2: メモリ使用量増加

**問題:** 複数解像度のテクスチャを保持

**解決策:**
```cpp
// 必要に応じて動的生成
QImage getMediumRes() {
    if (mediumRes_.isNull()) {
        mediumRes_ = original_.scaled(1024, 1024, Qt::KeepAspectRatio);
    }
    return mediumRes_;
}

// 使用しなくなったら解放
void releaseUnusedLODs(float currentZoom) {
    if (currentZoom > 0.75f) {
        mediumRes_ = QImage();  // 高詳細使用時は中解像度解放
    }
}
```

---

### 課題 3: アニメーションとの競合

**問題:** LOD 切り替えでアニメーションが不連続に

**解決策:**
```cpp
// アニメーション中は LOD 切り替えを遅延
if (isAnimating()) {
    lodSwitchTimer_.start(500);  // 500ms 遅延
} else {
    applyLOD(newLOD);
}
```

---

## 推奨実装順序

### 第 1 段階：基盤整備（4-6h）

1. **LODManager クラス実装**
   ```cpp
   class LODManager {
       DetailLevel getDetailLevel(float zoom) const;
       float calculateLODFactor(float distance) const;
   };
   ```

2. **インターフェース定義**
   ```cpp
   class ArtifactAbstractLayer {
       virtual void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) = 0;
   };
   ```

---

### 第 2 段階：2D レイヤー対応（8-12h）

3. **ImageLayer LOD 実装**
   - サムネイル生成
   - 解像度切り替え

4. **SolidLayer LOD 実装**
   - 簡略化描画
   - アウトラインモード

---

### 第 3 段階：特殊レイヤー対応（12-16h）

5. **ParticleLayer LOD 実装**
   - パーティクル数削減
   - 簡略化シェーダー

6. **TextLayer LOD 実装**
   - フォント解像度切り替え
   - 簡略化レンダリング

---

## 期待効果

### 性能向上

| ズームレベル | LOD なし | LOD あり | 向上率 |
|------------|---------|---------|--------|
| **25%** | 100% | 30% | -70% |
| **50%** | 100% | 50% | -50% |
| **75%** | 100% | 80% | -20% |
| **100%** | 100% | 100% | 0% |

---

### メモリ削減

| テクスチャサイズ | LOD なし | LOD あり | 削減率 |
|----------------|---------|---------|--------|
| **4096x4096** | 64MB | 64MB | 0% |
| **2048x2048** | 64MB | 80MB | -25% |
| **1024x1024** | 64MB | 84MB | -31% |
| **512x512** | 64MB | 86MB | -34% |

**注:** 複数解像度を保持するため、ズームアウト時はメモリ増加

---

## 結論

**LOD システムは部分的に実装可能**

### 実装推奨

1. **2D レイヤー向けズームベース LOD**（8-12h）
   - 最も簡単
   - 効果が高い
   - 既存インフラを活用

2. **テクスチャ解像度 LOD**（6-8h）
   - メモリ削減効果大
   - 実装が容易

3. **パーティクル LOD**（4-6h）
   - 描画コスト削減効果大
   - 特別なインフラ不要

### 実装見送り推奨

1. **完全な 3D LOD**（20-30h）
   - 複雑すぎる
   - 3D レイヤーが未実装

2. **動的メッシュ簡略化**（30-40h）
   - 高コスト
   - 効果が見合わない

---

## 付録：実装テンプレート

### LODManager クラス

```cpp
class LODManager {
public:
    enum class DetailLevel {
        Low,
        Medium,
        High
    };
    
    DetailLevel getDetailLevel(float zoom) const {
        if (zoom < lowThreshold_) return DetailLevel::Low;
        if (zoom < mediumThreshold_) return DetailLevel::Medium;
        return DetailLevel::High;
    }
    
    void setThresholds(float low, float medium) {
        lowThreshold_ = low;
        mediumThreshold_ = medium;
    }
    
private:
    float lowThreshold_ = 0.25f;
    float mediumThreshold_ = 0.75f;
};
```

---

### レイヤー LOD インターフェース

```cpp
class ArtifactAbstractLayer {
public:
    virtual void drawLOD(ArtifactIRenderer* renderer, 
                        LODManager::DetailLevel lod) = 0;
    
protected:
    void drawOutline(ArtifactIRenderer* renderer);
    void drawFlat(ArtifactIRenderer* renderer);
    void drawFull(ArtifactIRenderer* renderer);
};
```

---

**文書終了**
