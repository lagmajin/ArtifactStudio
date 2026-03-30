# ROI 実装ガイド - 成功率を上げるための完全手順

**作成日:** 2026-03-28  
**ステータス:** 実装手順確定  
**対象:** AI 開発者・実装担当者

---

## ⚠️ 重要：AI への指示事項

**AI は放っておくと抽象論に逃げやすい。**

そのため、以下の**実装順序**と**注意点**を必ず守ること。

---

## 📋 実装順序（7 段階）

### 段階 1: データ構造だけ追加

**目標:** 既存機能に影響せず型だけ定義

**実装:**
- `RenderROI` 構造体の定義
- `RenderMode` 列挙型の定義
- `RenderContext` 構造体の定義

**変更ファイル:**
- `Artifact/include/Render/ArtifactRenderROI.ixx`（新規）
- `Artifact/include/Render/ArtifactRenderContext.ixx`（新規）

**既存コードへの影響:** なし（型定義のみ）

**完了条件:**
- [ ] コンパイルが通る
- [ ] 既存テストが全てパスする

---

### 段階 2: Layer 単位の初期 ROI 計算

**目標:** 各レイヤーの基本バウンズを ROI として取得

**実装:**

```cpp
// ArtifactAbstractLayer に追加
QRectF ArtifactAbstractLayer::screenBounds() const
{
    return getGlobalTransform().mapRect(localBounds());
}

// CompositionRenderController で使用
void CompositionRenderController::renderOneFrame()
{
    for (const auto& layer : comp->allLayer()) {
        // ROI 計算
        QRectF bounds = layer->screenBounds();
        RenderROI layerROI(bounds);
        
        // 空チェック
        if (layerROI.isEmpty()) {
            continue; // スキップ
        }
        
        // 従来通り描画
        layer->draw(renderer_.get());
    }
}
```

**変更箇所:**
- `ArtifactAbstractLayer::screenBounds()` - 新規追加
- `CompositionRenderController::renderOneFrame()` - ROI 計算追加

**既存コードへの影響:** 最小限（既存の draw はそのまま）

**完了条件:**
- [ ] 全レイヤーで ROI が計算される
- [ ] デバッグ表示で ROI を確認できる

---

### 段階 3: 空 ROI スキップ

**目標:** 画面外レイヤーを完全スキップ

**実装:**

```cpp
void CompositionRenderController::renderOneFrame()
{
    QRectF viewportRect(0, 0, canvasWidth_, canvasHeight_);
    
    for (const auto& layer : comp->allLayer()) {
        // 1. 基本 ROI 計算
        QRectF bounds = layer->screenBounds();
        
        // 2. 可視性チェック
        if (!layer->isVisible()) {
            continue;
        }
        
        // 3. アクティブチェック
        if (!layer->isActiveAt(currentFrame)) {
            continue;
        }
        
        // 4. ビューポートとの交差
        QRectF intersected = bounds.intersected(viewportRect);
        
        // 5. 空 ROI スキップ
        if (intersected.isEmpty()) {
            continue; // ← ここが重要
        }
        
        // 6. 描画
        layer->draw(renderer_.get());
    }
}
```

**変更箇所:**
- `CompositionRenderController::renderOneFrame()` - 空 ROI チェック追加

**テスト:**
```cpp
// テストケース
TEST(ROI, SkipEmptyROI) {
    // 画面外レイヤー
    layer->setTransform(QTransform().translate(-10000, -10000));
    
    // スキップされることを確認
    EXPECT_FALSE(layer->isVisibleInViewport(viewport));
}
```

**完了条件:**
- [ ] 画面外レイヤーがスキップされる
- [ ] 描画時間が短縮される

---

### 段階 4: scissor 適用

**目標:** GPU 描画範囲を制限

**実装:**

```cpp
void CompositionRenderController::renderOneFrame()
{
    for (const auto& layer : comp->allLayer()) {
        // ROI 計算
        QRectF intersected = calculateLayerROI(layer);
        
        if (intersected.isEmpty()) {
            continue;
        }
        
        // Scissor 設定 ← 新規追加
        renderer_->pushScissor(intersected);
        
        // 描画
        layer->draw(renderer_.get());
        
        // Scissor 解除
        renderer_->popScissor();
    }
}
```

**変更箇所:**
- `ArtifactIRenderer::pushScissor(const QRectF&)` - 新規追加
- `ArtifactIRenderer::popScissor()` - 新規追加
- `CompositionRenderController::renderOneFrame()` - Scissor 適用

**既存コードへの影響:** 
- `pushScissor()` が未実装でもフォールバック動作

**完了条件:**
- [ ] Scissor テストが機能する
- [ ] 描画範囲が制限される

---

### 段階 5: BlurEffect の ROI 拡張

**目標:** エフェクトがはみ出さないように拡張

**実装:**

```cpp
// BlurEffect に追加
RenderROI BlurEffect::expandOutputROI(const RenderROI& inputROI) const
{
    // パディング必須！← ここが重要
    float padding = blurRadius_ * 2.0f;
    return inputROI.expanded(padding);
}

// EffectPipeline で使用
void EffectPipeline::applyEffects(const RenderROI& baseROI)
{
    RenderROI currentROI = baseROI;
    
    for (const auto& effect : effects_) {
        // ROI を拡張
        currentROI = effect->expandOutputROI(currentROI);
        
        // Scissor 設定
        renderer_->pushScissor(currentROI.rect);
        
        // エフェクト適用
        effect->apply(renderer_.get(), currentROI);
        
        renderer_->popScissor();
    }
}
```

**変更箇所:**
- `ArtifactAbstractEffect::expandOutputROI()` - 純粋仮想関数
- `BlurEffect::expandOutputROI()` - 実装
- `EffectPipeline::applyEffects()` - ROI 拡張

**⚠️ 注意点:**
```cpp
// ❌ ダメ：パディングなし
return inputROI;  // エッジが切れる！

// ⭕️ 良い：パディングあり
return inputROI.expanded(blurRadius_ * 2);
```

**完了条件:**
- [ ] Blur エッジが切れない
- [ ] Glow/Shadow も同様に対応

---

### 段階 6: 小さい一時 RT 化

**目標:** ROI に合わせた小さい RT でレンダリング

**実装:**

```cpp
void EffectPipeline::renderToSmallRT(const RenderROI& roi)
{
    if (roi.isEmpty()) {
        return; // スキップ
    }
    
    // ROI サイズの RT を作成
    int width = static_cast<int>(roi.width());
    int height = static_cast<int>(roi.height());
    
    auto rt = createRenderTarget(width, height);
    
    // ROI 左上を原点としたローカル座標に変換
    QTransform localTransform = QTransform::fromTranslate(-roi.x(), -roi.y());
    
    // RT に描画
    renderer_->pushRenderTarget(rt);
    renderer_->setTransform(localTransform);
    
    // 描画
    drawContents();
    
    // 元に戻す
    renderer_->setTransform(QTransform());
    renderer_->popRenderTarget();
    
    // 画面に合成（roi の位置に）
    renderer_->drawTexture(rt, roi.rect);
}
```

**変更箇所:**
- `EffectPipeline::renderToSmallRT()` - 新規追加
- `ArtifactIRenderer::pushRenderTarget()` - 既存
- `ArtifactIRenderer::setTransform()` - 既存

**⚠️ 注意点:**
```cpp
// 座標変換を明示的に
QTransform localTransform = QTransform::fromTranslate(-roi.x(), -roi.y());

// comp 座標との変換を保持
struct RenderState {
    QRectF roiInCompSpace;      // comp 座標系
    QRectF roiInLocalSpace;     // ローカル座標系
    QTransform compToLocal;     // 変換行列
};
```

**完了条件:**
- [ ] 小さい RT で描画される
- [ ] メモリ使用量が削減される

---

### 段階 7: ROI デバッグ表示

**目標:** ROI を可視化して確認

**実装:**

```cpp
void CompositionRenderController::renderOneFrame()
{
    for (const auto& layer : comp->allLayer()) {
        RenderROI roi = calculateLayerROI(layer);
        
        // 描画
        layer->draw(renderer_.get());
        
        // デバッグ表示 ← 新規追加
        if (debugMode_) {
            // ROI を赤い枠で表示
            renderer_->drawRect(
                roi.rect,
                QColor(255, 0, 0),
                2.0f
            );
            
            // 拡張 ROI を青い枠で表示
            if (auto effect = layer->getEffect<BlurEffect>()) {
                RenderROI expandedROI = effect->expandOutputROI(roi);
                renderer_->drawRect(
                    expandedROI.rect,
                    QColor(0, 0, 255),
                    1.0f
                );
            }
        }
    }
}
```

**変更箇所:**
- `CompositionRenderController::setDebugMode(bool)` - 新規追加
- `CompositionRenderController::renderOneFrame()` - デバッグ表示追加

**完了条件:**
- [ ] ROI が赤い枠で表示される
- [ ] 拡張 ROI が青い枠で表示される

---

## ⚠️ 絶対に守るべき 5 つの注意点

### 1. パディング必須

**Blur/Glow/Shadow は入力 bounds をそのまま使わず、半径やオフセット分の padding を必ず入れること。**

```cpp
// ❌ ダメ：パディングなし
RenderROI BlurEffect::expandOutputROI(const RenderROI& inputROI) const
{
    return inputROI;  // エッジが切れる！
}

// ⭕️ 良い：パディングあり
RenderROI BlurEffect::expandOutputROI(const RenderROI& inputROI) const
{
    float padding = blurRadius_ * 2.0f;  // 半径の 2 倍
    return inputROI.expanded(padding);
}

// ⭕️ 良い：Shadow も同様
RenderROI DropShadowEffect::expandOutputROI(const RenderROI& inputROI) const
{
    float padding = blurRadius_ + std::max(abs(offsetX_), abs(offsetY_));
    return inputROI.expanded(padding);
}
```

---

### 2. 空 ROI スキップ

**ROI が空なら draw / effect pass / RT 確保を全部スキップすること。**

```cpp
// ⭕️ 良い：全部スキップ
void renderLayer(const ArtifactAbstractLayerPtr& layer)
{
    RenderROI roi = calculateROI(layer);
    
    if (roi.isEmpty()) {
        return;  // ← 全部スキップ
    }
    
    // 以下、処理が続く
    layer->draw();
    applyEffects();
    renderToRT();
}

// ❌ ダメ：一部だけスキップ
void renderLayer(const ArtifactAbstractLayerPtr& layer)
{
    RenderROI roi = calculateROI(layer);
    
    // draw はスキップするが...
    if (roi.isEmpty()) {
        return;
    }
    
    // effect pass はスキップしない ← バグ！
    applyEffects();  // 空 ROI で実行される！
}
```

---

### 3. Editor と Final を分ける

**Editor 表示用の見せる範囲と、最終レンダ用の厳密 ROI を混同しないこと。**

```cpp
// ⭕️ 良い：別々に管理
struct RenderContext {
    RenderROI editorROI;    // ビューポートに限定
    RenderROI finalROI;     // コンポジション全体
};

void render()
{
    if (mode == RenderMode::Editor) {
        // Editor モード：ビューポート制限
        setROI(editorROI);
    } else {
        // Final モード：全体レンダリング
        setROI(finalROI);
    }
}

// ❌ ダメ：混同
void render()
{
    // Editor でも Final でも同じ ROI ← バグ！
    setROI(viewportROI);  // Final で範囲不足！
}
```

---

### 4. オフセット管理

**小さい RT に描くときは、ROI 左上を原点としたローカル座標へ変換すること。comp 座標との変換を明示的に持つこと。**

```cpp
// ⭕️ 良い：変換を明示
struct RenderState {
    QRectF roiInCompSpace;      // comp 座標系
    QRectF roiInLocalSpace;     // ローカル座標系（ROI 左上が原点）
    QTransform compToLocal;     // 変換行列
};

void renderToSmallRT(const RenderROI& roi)
{
    // ローカル座標に変換
    QTransform localTransform = QTransform::fromTranslate(-roi.x(), -roi.y());
    
    renderer_->setTransform(localTransform);
    drawContents();  // ローカル座標で描画
    
    // 元に戻す
    renderer_->setTransform(QTransform());
}

// ❌ ダメ：変換しない
void renderToSmallRT(const RenderROI& roi)
{
    // comp 座標のまま描画 ← はみ出る！
    drawContents();  // 座標がおかしい！
}
```

---

### 5. 既存描画を壊さない

**まずは fallback として ROI 無効でも従来通り描けるようにすること。**

```cpp
// ⭕️ 良い：fallback あり
void renderLayer(const ArtifactAbstractLayerPtr& layer)
{
    RenderROI roi = calculateROI(layer);
    
    // ROI が無効でも従来通り描画
    if (!roi.isValid() || !useROI_) {
        layer->draw();  // 従来パス
        return;
    }
    
    // ROI 最適化パス
    renderWithROI(layer, roi);
}

// ❌ ダメ：fallback なし
void renderLayer(const ArtifactAbstractLayerPtr& layer)
{
    RenderROI roi = calculateROI(layer);
    
    // ROI 必須 ← バグ！
    renderWithROI(layer, roi);  // ROI 計算失敗でクラッシュ！
}
```

---

## 📝 実装チェックリスト

### 段階 1: データ構造

- [ ] `RenderROI` 構造体を定義
- [ ] `RenderMode` 列挙型を定義
- [ ] `RenderContext` 構造体を定義
- [ ] コンパイルが通る
- [ ] 既存テストがパスする

---

### 段階 2: 初期 ROI 計算

- [ ] `ArtifactAbstractLayer::screenBounds()` を追加
- [ ] `CompositionRenderController` で ROI 計算
- [ ] デバッグ表示で確認

---

### 段階 3: 空 ROI スキップ

- [ ] 可視性チェックを追加
- [ ] アクティブチェックを追加
- [ ] ビューポートとの交差を計算
- [ ] 空 ROI でスキップ

---

### 段階 4: scissor 適用

- [ ] `ArtifactIRenderer::pushScissor()` を追加
- [ ] `ArtifactIRenderer::popScissor()` を追加
- [ ] 描画前に scissor 設定
- [ ] 描画後に scissor 解除

---

### 段階 5: BlurEffect の ROI 拡張

- [ ] `ArtifactAbstractEffect::expandOutputROI()` を定義
- [ ] `BlurEffect::expandOutputROI()` を実装
- [ ] パディングを追加（半径*2）
- [ ] Glow/Shadow も同様に対応

---

### 段階 6: 小さい一時 RT 化

- [ ] `renderToSmallRT()` を実装
- [ ] ROI サイズの RT を作成
- [ ] ローカル座標に変換
- [ ] 画面に合成

---

### 段階 7: ROI デバッグ表示

- [ ] `setDebugMode()` を追加
- [ ] ROI を赤い枠で表示
- [ ] 拡張 ROI を青い枠で表示

---

## 🎯 成功基準

### 性能向上

| 指標 | 目標 | 測定方法 |
|------|------|---------|
| **描画時間** | -30% | フレームタイム計測 |
| **メモリ使用量** | -25% | RT サイズ計測 |
| **GPU 負荷** | -40% | GPU プロファイラ |

---

### 品質維持

- [ ] 既存の描画が壊れていない
- [ ] エフェクトのエッジが切れていない
- [ ] アニメーションが正常に動作
- [ ] 全テストがパス

---

## 📚 関連ドキュメント

- `docs/technical/ROI_SPECIFICATION_2026-03-28.md` - ROI 仕様書
- `docs/technical/ROI_IMPLEMENTATION_SPECIFICATION_2026-03-28.md` - ROI 実装仕様
- `docs/technical/RENDER_ROI_CONTEXT_IMPLEMENTATION_2026-03-28.md` - RenderROI/Context 実装

---

**文書終了**
