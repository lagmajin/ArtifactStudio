# LOD システム第 1 段階 実装レポート

**作成日:** 2026-03-28  
**ステータス:** 実装完了  
**工数:** 2 時間  
**関連コンポーネント:** LODManager, ArtifactAbstractLayer, CompositionRenderController

---

## 概要

LOD（Level of Detail）システムの第 1 段階（基盤整備）を実装した。

---

## 実装内容

### 1. DetailLevel 列挙型

**ファイル:** `Artifact/include/Layer/ArtifactAbstractLayer.ixx`

```cpp
enum class DetailLevel {
  Low,    // 簡略化された描画（ズーム 0-25%）
  Medium, // 標準的な描画（ズーム 25-75%）
  High    // 高詳細な描画（ズーム 75-100%）
};
```

---

### 2. LODManager クラス

**ファイル:** 
- `Artifact/include/LOD/ArtifactLODManager.ixx`（新規作成）
- `Artifact/src/LOD/ArtifactLODManager.cppm`（新規作成）

#### ヘッダー

```cpp
class LODManager : public QObject {
    Q_OBJECT

public:
    enum class DetailLevel {
        Low,    // ズーム 0-25%
        Medium, // ズーム 25-75%
        High    // ズーム 75-100%
    };
    Q_ENUM(DetailLevel)

    // ズームレベルから詳細度を取得
    Q_INVOKABLE DetailLevel getDetailLevel(float zoom) const;
    
    // 閾値設定
    Q_INVOKABLE void setThresholds(float low, float medium);
    
    // LOD ファクター計算（0.0-1.0）
    Q_INVOKABLE float calculateLODFactor(float zoom) const;

signals:
    void detailLevelChanged(DetailLevel old, DetailLevel new);
    void thresholdsChanged();
};
```

#### 実装

```cpp
LODManager::DetailLevel LODManager::getDetailLevel(float zoom) const
{
    if (zoom < lowThreshold_) {
        return DetailLevel::Low;
    } else if (zoom < mediumThreshold_) {
        return DetailLevel::Medium;
    } else {
        return DetailLevel::High;
    }
}

float LODManager::calculateLODFactor(float zoom) const
{
    if (zoom < lowThreshold_) {
        // Low ゾーン：0.0-0.5
        return std::clamp(zoom / lowThreshold_ * 0.5f, 0.0f, 0.5f);
    } else if (zoom < mediumThreshold_) {
        // Medium ゾーン：0.5-0.75
        float t = (zoom - lowThreshold_) / (mediumThreshold_ - lowThreshold_);
        return 0.5f + t * 0.25f;
    } else {
        // High ゾーン：0.75-1.0
        return std::clamp(0.75f + (zoom - mediumThreshold_) * 1.0f, 0.75f, 1.0f);
    }
}
```

---

### 3. ArtifactAbstractLayer に LOD インターフェース

**ファイル:** 
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx`
- `Artifact/src/Layer/ArtifactAbstractLayer.cppm`

#### インターフェース追加

```cpp
class ArtifactAbstractLayer {
    // ...
    
    // LOD (Level of Detail) rendering
    virtual void drawLOD(ArtifactIRenderer* renderer, DetailLevel lod);
};
```

#### デフォルト実装

```cpp
void ArtifactAbstractLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod)
{
    Q_UNUSED(lod);
    // デフォルトでは通常描画と同じ
    // サブクラスで LOD 固有の描画を実装
    draw(renderer);
}
```

---

### 4. CompositionRenderController に LOD 統合

**ファイル:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

#### Impl メンバー追加

```cpp
class CompositionRenderController::Impl {
    // ...
    
    // LOD (Level of Detail)
    std::unique_ptr<LODManager> lodManager_;
    bool lodEnabled_ = true;
};
```

#### API 追加

```cpp
// ヘッダーに追加
LODManager* lodManager() const;
void setLODEnabled(bool enabled);
bool isLODEnabled() const;
```

---

## 変更ファイル一覧

| ファイル | 追加行数 | 内容 |
|---------|---------|------|
| `Artifact/include/Layer/ArtifactAbstractLayer.ixx` | +7 | DetailLevel 列挙型・drawLOD 宣言 |
| `Artifact/src/Layer/ArtifactAbstractLayer.cppm` | +10 | drawLOD 実装 |
| `Artifact/include/LOD/ArtifactLODManager.ixx` | +70 | LODManager 宣言（新規） |
| `Artifact/src/LOD/ArtifactLODManager.cppm` | +70 | LODManager 実装（新規） |
| `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` | +8 | LOD API 宣言 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | +5 | LOD メンバー追加 |

**合計:** 170 行（新規 140 行）

---

## 使用例

### 基本的な使い方

```cpp
// CompositionRenderController から LODManager にアクセス
auto* lodManager = controller->lodManager();

// ズームレベルから詳細度を取得
float zoom = renderer->getZoom();
DetailLevel lod = lodManager->getDetailLevel(zoom);

// レイヤーを LOD で描画
for (const auto& layer : layers) {
    layer->drawLOD(renderer.get(), lod);
}
```

### LOD ファクターの取得

```cpp
// LOD ファクター（0.0-1.0）を取得
float factor = lodManager->calculateLODFactor(zoom);

// パーティクル数などに使用
int particleCount = baseCount * factor;
```

### 閾値のカスタマイズ

```cpp
// 閾値を変更（デフォルト：0.25, 0.75）
lodManager->setThresholds(0.3f, 0.8f);
```

---

## 次のステップ（第 2 段階）

### 実装予定

1. **ImageLayer LOD 実装**（4-6h）
   ```cpp
   void ArtifactImageLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
       switch (lod) {
           case DetailLevel::Low:
               // サムネイル描画
               drawThumbnail(renderer);
               break;
           case DetailLevel::Medium:
               // 中解像度描画
               drawMediumRes(renderer);
               break;
           case DetailLevel::High:
               // 高詳細描画
               drawFull(renderer);
               break;
       }
   }
   ```

2. **SolidLayer LOD 実装**（2-3h）
   ```cpp
   void ArtifactSolid2DLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
       if (lod == DetailLevel::Low) {
           // アウトラインのみ
           drawOutline(renderer);
       } else {
           // 通常描画
           draw(renderer);
       }
   }
   ```

3. **ParticleLayer LOD 実装**（4-6h）
   ```cpp
   void ArtifactParticleLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod) {
       int skip = 1;
       if (lod == DetailLevel::Low) skip = 10;      // 10%
       else if (lod == DetailLevel::Medium) skip = 2; // 50%
       
       for (int i = 0; i < particles_.size(); i += skip) {
           drawParticle(renderer, particles_[i]);
       }
   }
   ```

---

## 期待効果

### 性能向上（第 2 段階実装後）

| ズームレベル | LOD なし | LOD あり | 向上率 |
|------------|---------|---------|--------|
| **25%** | 100% | 30% | -70% |
| **50%** | 100% | 50% | -50% |
| **75%** | 100% | 80% | -20% |
| **100%** | 100% | 100% | 0% |

---

## 技術的詳細

### ズーム閾値のデフォルト

```cpp
lowThreshold_ = 0.25f;    // 25% ズームで Low→Medium
mediumThreshold_ = 0.75f; // 75% ズームで Medium→High
```

### LOD ファクター計算

```
ズーム 0.0-0.25: LOD ファクター 0.0-0.5 (Low)
ズーム 0.25-0.75: LOD ファクター 0.5-0.75 (Medium)
ズーム 0.75-1.0: LOD ファクター 0.75-1.0 (High)
```

### シグナル発火

```cpp
// 状態変化を検出してシグナル発火
if (newLevel != currentLevel_) {
    currentLevel_ = newLevel;
    emit detailLevelChanged(oldLevel, newLevel);
}
```

---

## テスト項目

### 単体テスト

- [ ] `getDetailLevel(0.1f)` が `DetailLevel::Low` を返す
- [ ] `getDetailLevel(0.5f)` が `DetailLevel::Medium` を返す
- [ ] `getDetailLevel(0.9f)` が `DetailLevel::High` を返す
- [ ] `calculateLODFactor(0.0f)` が `0.0` を返す
- [ ] `calculateLODFactor(1.0f)` が `1.0` を返す
- [ ] 閾値変更後に `thresholdsChanged` シグナルが発火する

### 統合テスト

- [ ] ズームイン/アウトで LOD が自動切り替え
- [ ] LOD 切り替え時に `detailLevelChanged` シグナルが発火
- [ ] サブクラスが `drawLOD` をオーバーライド可能

---

## 関連ドキュメント

- `docs/technical/LOD_SYSTEM_FEASIBILITY_ANALYSIS_2026-03-28.md` - 実現可能性分析
- `docs/technical/MILESTONE_EXPANSION_ANALYSIS_2026-03-28.md` - マイルストーン拡充分析

---

## 結論

**LOD システムの第 1 段階（基盤整備）が完了。**

- ✅ DetailLevel 列挙型定義
- ✅ LODManager クラス実装
- ✅ ArtifactAbstractLayer に LOD インターフェース追加
- ✅ CompositionRenderController に LOD 統合

**次のステップ:** 第 2 段階（個別レイヤーの LOD 実装）

---

**文書終了**
