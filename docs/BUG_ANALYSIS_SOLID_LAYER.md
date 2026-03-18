# 平面レイヤー描画バグ分析

## 問題の概要

タブ切り替え後に平面レイヤー（Solid Layer）が正しく描画されず、小さい点しか表示されない。

---

## 発覚までの経緯

1. **初期状態**: 不透明度 100% で正常描画
2. **不透明度変更**: 50% に変更 → 正常に半透明になる
3. **タブ切り替え**: 非表示 → 再表示
4. **再表示後**: 小さい点しか見えない
5. **不透明度戻す**: 100% に戻しても小さい点のまま

---

## 収集したログ

### 正常時（初期状態）
```
[VIEWPORT] scale=(1,1) offset=(50,154.719) screenSize=(549,562)
[SOLIDRECT] x=0 y=0 w=1920 h=1080 color.rgba=(1,1,1,1) opacity=1 alpha=1
[SOLIDRECT] Vertex[0] position=(0,0) color.a=1
[SOLIDRECT] TransformCB: offset=(50,154.719) scale=(1920,1080) screenSize=(549,562)
```

### 異常時（タブ戻し後）
```
[VIEWPORT] scale=(1,1) offset=(50,154.719) screenSize=(549,562)
[SOLIDRECT] x=0 y=0 w=96774 h=1080 color.rgba=(1,1,1,1) opacity=1 alpha=1
[SOLIDRECT] Vertex[0] position=(0,0) color.a=1
[SOLIDRECT] TransformCB: offset=(50,154.719) scale=(96774,1080) screenSize=(549,562)
```

### 追加の異常描画
```
[SOLIDRECT] x=-8192 y=-8192 w=16384 h=16384 color.rgba=(0.1,0.1,0.1,1) opacity=1 alpha=1
[SOLIDRECT] TransformCB: offset=(-8142,-8037.28) scale=(16384,16384) screenSize=(549,562)
```

---

## 問題の分析

### 問題 1: `sourceSize()` の異常値

**期待値**: `1920 x 1080`  
**実際**: `96774 x 1080`

```cpp
// ArtifactAbstractLayer.cppm:530
Size_2D ArtifactAbstractLayer::sourceSize() const
{
    return impl_->sourceSize_;  // ← 96774 が返っている
}
```

**原因**: `impl_->sourceSize_` が初期化されていない、または破損している。

---

### 問題 2: 座標系の混同

**2 種類の描画が混在**:

1. **コンポジション背景？**
   ```
   x=-8192, y=-8192, w=16384, h=16384
   ```
   
2. **平面レイヤー**
   ```
   x=0, y=0, w=96774, h=1080  ← sourceSize が壊れてる
   ```

**設計上の問題**:
- コンポジション背景描画とレイヤー描画の責務が不明確
- 座標系（Composition vs Local）の区別が失われている

---

### 問題 3: スケールの暴走

**定数バッファの設定**:
```cpp
// PrimitiveRenderer2D.cppm:345-350
cbTransform.offset = { x * viewportCB.scale.x + viewportCB.offset.x, ... };
cbTransform.scale  = { w * viewportCB.scale.x, h * viewportCB.scale.y };
```

**問題**:
- `viewportCB.scale = (1, 1)`（ズーム倍率）
- `w = 96774`（壊れた sourceSize）
- 結果：`cbTransform.scale = (96774, 1080)`

**頂点シェーダーで**:
```hlsl
float2 pos = input.position * scale + offset;
// input.position = (0,0) to (1,1)
// scale = (96774, 1080)
// pos = (0,0) to (96774,1080)  ← 座標が暴走
```

---

## 根本原因

### 原因 1: `Impl::sourceSize_` の初期化漏れ

```cpp
// ArtifactAbstractLayer.cppm:122-124
ArtifactAbstractLayer::Impl::Impl()
{
    // ← sourceSize_ の初期化がない！
}
```

**結果**: 未初期化のゴミ値（96774）が返る。

---

### 原因 2: 設計の経緯による責務の混同

**過去の改修で**:
1. コンポジション背景（単色）を描画
2. 平面レイヤー（単色）を描画
3. **この 2 つが混同された**

**現在**:
- コンポジション背景 = 平面レイヤー と勘違い
- 座標系がごっちゃ（Composition vs Local）
- スケールが暴走

---

### 原因 3: タブ切り替え時のライフサイクル

**タブ切り替え時**:
1. ウィジェットが非表示
2. 一部のリソースが解放される
3. 再表示時に再初期化
4. **`sourceSize_` が再設定されない**

---

## 必要な修正

### 修正 1: `Impl::sourceSize_` の初期化

```cpp
// 案 A: メンバー初期化
class Impl {
    Size_2D sourceSize_{1920, 1080};  // デフォルト値
};

// 案 B: コンストラクタで初期化
ArtifactAbstractLayer::Impl::Impl()
  : sourceSize_(1920, 1080)
{
}
```

---

### 修正 2: 責務の分離

**コンポジション背景描画**:
```cpp
void CompositionRenderer::drawBackground()
{
    // Composition Space: (0,0) to (compWidth, compHeight)
    drawSolidRect(0, 0, compWidth_, compHeight_, backgroundColor_);
}
```

**レイヤー描画**:
```cpp
void CompositionRenderer::drawLayers()
{
    for (auto& layer : layers_) {
        layer->draw(renderer_);
        // レイヤーは Local Space で定義
    }
}
```

---

### 修正 3: 平面レイヤーの座標修正

```cpp
// ArtifactSolid2DLayer.cppm
void ArtifactSolid2DLayer::draw(ArtifactIRenderer* renderer)
{
    auto size = sourceSize();  // 1920x1080（正常値）
    
    // Local Space: (0,0) to (width, height)
    renderer->drawSolidRect(0.0f, 0.0f, 
                           static_cast<float>(size.width),
                           static_cast<float>(size.height),
                           color_,
                           opacity());
}
```

---

### 修正 4: 異常描画の特定

```
x=-8192, y=-8192, w=16384, h=16384
```

この描画を特定して修正：

```bash
grep -r "\-8192\|16384" --include="*.cppm" --include="*.ixx"
```

---

## 検証チェックリスト

- [ ] `sourceSize()` が `1920x1080` を返す
- [ ] タブ切り替え後も値が維持される
- [ ] コンポジション背景とレイヤー描画が分離されている
- [ ] 座標系が正しく使い分けられている
- [ ] スケールが暴走しない

---

## 参考ドキュメント

- [座標系仕様書](../docs/COORDINATE_SYSTEMS.md)
- [High Quality Glow Design](../ArtifactCore/docs/HIGH_QUALITY_GLOW_DESIGN.md)

---

## 改版履歴

| 版本 | 日付 | 変更内容 |
|------|------|----------|
| 1.0 | 2026-03-18 | 初版作成 |
