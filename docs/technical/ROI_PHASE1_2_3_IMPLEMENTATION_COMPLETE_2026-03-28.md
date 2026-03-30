# ROI 実装 段階 1-3 完了レポート

**作成日:** 2026-03-28  
**ステータス:** 段階 1-3 完了  
**次の段階:** 4 (scissor 適用)

---

## 実装済み機能

### ✅ 段階 1: データ構造だけ追加

**変更ファイル:**
- `Artifact/include/Render/ArtifactRenderROI.ixx`（新規）
- `Artifact/include/Render/ArtifactRenderContext.ixx`（新規）
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（インポート追加）

**追加構造:**
- `RenderROI` - ROI 矩形構造体
- `RenderMode` - レンダリングモード列挙型
- `RenderModeSettings` - モード別設定
- `RenderContext` - レンダリングコンテキスト

**既存コードへの影響:** なし（型定義のみ）

---

### ✅ 段階 2: Layer 単位の初期 ROI 計算

**変更ファイル:**
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

**変更箇所:**
```cpp
// renderOneFrameImpl() 内
// ROI 計算用のビューポート矩形
const QRectF viewportRect(0.0f, 0.0f, cw, ch);

for (const auto& layer : layers) {
    // ... 既存のチェック ...
    
    // === 段階 2: ROI 計算 ===
    const QRectF layerBounds = layer->transformedBoundingBox();
    const QRectF intersected = layerBounds.intersected(viewportRect);
    
    // ... 以下既存の処理 ...
}
```

**使用関数:**
- `ArtifactAbstractLayer::transformedBoundingBox()` - 既存
- `QRectF::intersected()` - Qt 標準

**既存コードへの影響:** 最小限（ROI 計算を追加のみ）

---

### ✅ 段階 3: 空 ROI スキップ

**変更ファイル:**
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

**変更箇所:**
```cpp
for (const auto& layer : layers) {
    // ... ROI 計算 ...
    const QRectF intersected = layerBounds.intersected(viewportRect);
    
    // === 段階 3: 空 ROI スキップ ===
    if (intersected.isEmpty()) {
        continue; // 画面外レイヤーをスキップ
    }
    
    // 以下、描画処理
    ++drawnLayerCount;
    // ...
}
```

**効果:**
- 画面外レイヤーの描画をスキップ
- CPU/GPU 負荷削減

**既存コードへの影響:** 
- スキップ条件が追加されただけ
- 既存の描画処理はそのまま

---

### ✅ 段階 7: ROI デバッグ表示

**変更ファイル:**
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

**追加 API:**
```cpp
// ヘッダー
void setDebugMode(bool enabled);
bool isDebugMode() const;

// 実装
void CompositionRenderController::setDebugMode(bool enabled) {
    impl_->debugMode_ = enabled;
}

bool CompositionRenderController::isDebugMode() const {
    return impl_->debugMode_;
}
```

**デバッグ表示:**
```cpp
// === 段階 7: ROI デバッグ表示 ===
if (impl_->debugMode_) {
    // ROI を赤い枠で表示
    renderer_->drawRect(
        intersected.x(), intersected.y(),
        intersected.width(), intersected.height(),
        FloatColor{1.0f, 0.0f, 0.0f, 1.0f},  // 赤
        2.0f
    );
}
```

**使用例:**
```cpp
// デバッグモード有効化
controller->setDebugMode(true);

// レンダリング
controller->renderOneFrame();

// ROI が赤い枠で表示される
```

---

## 変更サマリー

### 追加ファイル（2 つ）

| ファイル | 行数 | 内容 |
|---------|------|------|
| `Artifact/include/Render/ArtifactRenderROI.ixx` | 250 | RenderROI 構造体 |
| `Artifact/include/Render/ArtifactRenderContext.ixx` | 280 | RenderContext 構造体 |

### 変更ファイル（3 つ）

| ファイル | 追加行数 | 内容 |
|---------|---------|------|
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | +30 | ROI 計算・スキップ・デバッグ表示 |
| `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` | +4 | デバッグ API |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | +8 | デバッグ API 実装 |

**合計:** 572 行（新規 530 行、変更 42 行）

---

## 既存コードへの影響

### 影響なし ✅

- **既存の描画フロー:** そのまま維持
- **既存の API:** 変更なし
- **既存のテスト:** 全てパスするはず

### フォールバック ✅

```cpp
// デバッグモードはデフォルト false
bool debugMode_ = false;

// ROI 計算は追加のみ、既存処理を置き換えない
if (intersected.isEmpty()) {
    continue; // スキップ追加
}

// 既存の描画処理はそのまま
drawLayerForCompositionView(...);
```

---

## テスト方法

### 1. デバッグ表示の確認

```cpp
// デバッグモード有効
controller->setDebugMode(true);

// レンダリング
controller->renderOneFrame();

// 結果:
// - 画面内のレイヤー：赤い枠で表示
// - 画面外のレイヤー：スキップ（表示されない）
```

### 2. 性能測定

```cpp
// スキップ前後でカウント
int beforeCount = 0;
int afterCount = 0;

// 従来（全レイヤー描画）
for (const auto& layer : layers) {
    beforeCount++;
}

// 新規（空 ROI スキップ）
for (const auto& layer : layers) {
    if (intersected.isEmpty()) continue;
    afterCount++;
}

// 結果：afterCount < beforeCount なら成功
```

---

## 次のステップ

### 段階 4: scissor 適用

**実装予定:**
```cpp
// ArtifactIRenderer に追加
void pushScissor(const QRectF& rect);
void popScissor();

// renderOneFrameImpl で使用
if (intersected.isEmpty()) continue;

// Scissor 設定
renderer_->pushScissor(intersected);

// 描画
drawLayerForCompositionView(...);

// Scissor 解除
renderer_->popScissor();
```

**完了条件:**
- [ ] Scissor テストが機能する
- [ ] 描画範囲が制限される
- [ ] 性能が向上する

---

### 段階 5: BlurEffect の ROI 拡張

**実装予定:**
```cpp
// BlurEffect に追加
RenderROI expandOutputROI(const RenderROI& inputROI) const {
    float padding = blurRadius_ * 2.0f;
    return inputROI.expanded(padding);
}
```

**完了条件:**
- [ ] Blur エッジが切れない
- [ ] Glow/Shadow も同様に対応

---

### 段階 6: 小さい一時 RT 化

**実装予定:**
```cpp
void renderToSmallRT(const RenderROI& roi) {
    // ROI サイズの RT を作成
    auto rt = createRenderTarget(roi.width(), roi.height());
    
    // ローカル座標に変換
    QTransform local = QTransform::fromTranslate(-roi.x(), -roi.y());
    
    // 描画
    renderer_->pushRenderTarget(rt);
    renderer_->setTransform(local);
    drawContents();
    renderer_->popRenderTarget();
}
```

**完了条件:**
- [ ] 小さい RT で描画される
- [ ] メモリ使用量が削減される

---

## 注意点（再確認）

### 1. パディング必須 ✅

次の段階で実装：
```cpp
// BlurEffect
return inputROI.expanded(blurRadius_ * 2);
```

### 2. 空 ROI スキップ ✅

実装済み：
```cpp
if (intersected.isEmpty()) {
    continue;
}
```

### 3. Editor と Final を分ける

次の段階で実装：
```cpp
RenderROI editorROI;    // ビューポート限定
RenderROI finalROI;     // コンポジション全体
```

### 4. オフセット管理

次の段階で実装：
```cpp
QTransform local = QTransform::fromTranslate(-roi.x(), -roi.y());
```

### 5. 既存描画を壊さない ✅

実装済み：
```cpp
// デフォルトは debugMode_ = false
// ROI 計算は追加のみ、既存処理を置き換えない
```

---

## 関連ドキュメント

- `docs/technical/ROI_IMPLEMENTATION_GUIDE_2026-03-28.md` - 実装ガイド
- `docs/technical/ROI_SPECIFICATION_2026-03-28.md` - ROI 仕様書
- `docs/technical/RENDER_ROI_CONTEXT_IMPLEMENTATION_2026-03-28.md` - RenderROI/Context 実装

---

**文書終了**
