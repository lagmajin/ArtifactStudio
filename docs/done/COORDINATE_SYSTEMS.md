# 座標系仕様書 (Coordinate Systems Specification)

## 概要

本アプリケーションは 4 つの座標系を持ち、描画パイプラインを通じて変換される。

---

## 4 つの座標系

### 1. Screen Space（スクリーンスペース）

- **定義**: 実際のウィンドウ座標（ピクセル単位）
- **範囲**: `(0, 0)` to `(windowWidth, windowHeight)`
- **用途**: 
  - DirectX 12 スワップチェーンのバックバッファ
  - ウィンドウ上の物理ピクセル位置
- **例**: `1920x1080` ウィンドウの場合、`(0,0)` が左上、`(1920,1080)` が右下

---

### 2. View Space（ビュー空間）

- **定義**: コンポジション表示領域（ズーム・パン適用後）
- **範囲**: ズーム・パンに依存
- **用途**:
  - ユーザーが表示している領域
  - ビューポート変換の中間座標系
- **変換**:
  ```
  View = Composition × Zoom + Pan
  ```
- **例**: 
  - ズーム 2.0 倍、パン (100, 50) の場合
  - Composition (0,0) → View (100, 50)
  - Composition (100,100) → View (300, 250)

---

### 3. Composition Space（コンポジションスペース）

- **定義**: コンポジションの論理サイズ
- **範囲**: `(0, 0)` to `(compWidth, compHeight)`
- **用途**:
  - レイヤー配置の基準座標系
  - タイムライン上のすべてのレイヤーはこの座標で管理される
- **デフォルト**: `1920 x 1080`（フル HD）
- **例**:
  - レイヤー位置 `(100, 50)` = コンポジション左上から右に 100px、下に 50px

---

### 4. Local Space（ローカルスペース）

- **定義**: 各レイヤーのローカル座標（親基準）
- **範囲**: `(0, 0)` to `(layerWidth, layerHeight)`
- **用途**:
  - レイヤー固有の座標系
  - アンカーポイント、回転、スケールの基準
- **変換**:
  ```
  Composition = Local × Transform2D/3D
  ```
- **例**:
  - 1920x1080 のソリッドレイヤー
  - ローカル `(0,0)` = レイヤー左上
  - ローカル `(960,540)` = レイヤー中心

---

## 描画パイプライン

### 変換フロー

```
Local Space
    ↓ [Transform: 位置/回転/スケール]
Composition Space
    ↓ [Viewport: ズーム/パン]
View Space
    ↓ [Projection: NDC 変換]
Screen Space
```

---

### 変換数式

#### Local → Composition

```cpp
// 2D Transform
compPos = localPos × layerScale × layerRotation + layerPosition
```

#### Composition → View

```cpp
// Zoom & Pan
viewPos = compPos × zoom + panOffset
```

#### View → Screen

```cpp
// NDC (Normalized Device Coordinates) 変換
ndcPos = (viewPos / screenSize) × 2.0 - 1.0
ndcPos.y = -ndcPos.y  // Y 反転（D3D12）

// Screen Space
screenPos = ndcPos × (windowWidth, windowHeight) / 2.0 + (windowWidth, windowHeight) / 2.0
```

---

## 実装詳細

### 頂点シェーダーでの実装

```hlsl
// 定数バッファ
cbuffer TransformCB : register(b0)
{
    float2 offset;      // パンオフセット
    float2 scale;       // ズームスケール
    float2 screenSize;  // ウィンドウサイズ
};

// 頂点入力（Local Space）
struct VSInput
{
    float2 position : ATTRIB0;  // ローカル座標
    float4 color    : ATTRIB1;
};

// 頂点出力
struct VSOutput
{
    float4 position : SV_POSITION;  // Screen Space (NDC)
    float4 color    : COLOR0;
};

// 頂点シェーダー
VSOutput main(VSInput input)
{
    VSOutput output;
    
    // Local → Composition → View
    float2 viewPos = input.position * scale + offset;
    
    // View → NDC
    float2 ndc = (viewPos / screenSize) * 2.0 - float2(1.0, 1.0);
    ndc.y = -ndc.y;  // Y 反転
    
    output.position = float4(ndc, 0.0, 1.0);
    output.color = input.color;
    
    return output;
}
```

---

### 定数バッファの設定

```cpp
// PrimitiveRenderer2D.cppm

// 1. ViewportTransformer から値を取得
auto viewportCB = impl_->viewport_.GetViewportCB();
// viewportCB.scale  = ズーム倍率
// viewportCB.offset = パンオフセット
// viewportCB.screenSize = ウィンドウサイズ

// 2. レイヤーのローカル座標を設定
RectVertex vertices[4] = {
    {{0.0f, 0.0f}, ...},       // 左上（Local Space）
    {{1.0f, 0.0f}, ...},       // 右上
    {{0.0f, 1.0f}, ...},       // 左下
    {{1.0f, 1.0f}, ...},       // 右下
};

// 3. 定数バッファに設定
CBSolidTransform2D cbTransform;
cbTransform.offset     = { x * viewportCB.scale.x + viewportCB.offset.x, 
                           y * viewportCB.scale.y + viewportCB.offset.y };
cbTransform.scale      = { w * viewportCB.scale.x, h * viewportCB.scale.y };
cbTransform.screenSize = viewportCB.screenSize;
```

---

## 描画順序

### 正しい描画フロー

```cpp
// 1. ウィンドウクリア（Screen Space）
pContext->ClearRenderTarget(pRTV, clearColor, ...);

// 2. チェッカーボード描画（Composition Space）
//    コンポジション全体をカバー
drawCheckerboard(0, 0, compWidth, compHeight, ...);

// 3. レイヤー描画（Local → Composition → View → Screen）
for (auto& layer : layers) {
    layer->draw(renderer);
    // レイヤーは Local Space で定義
    // シェーダーで自動変換：Local → Composition → View → Screen
}
```

---

## 共通の落とし穴

### 問題 1: 座標系の混同

❌ **悪い例**:
```cpp
// Screen Space の値を Composition Space で使っている
drawRect(0, 0, windowWidth, windowHeight);  // ウィンドウサイズで描画
```

⭕ **良い例**:
```cpp
// Composition Space の値を使う
drawRect(0, 0, compWidth, compHeight);  // コンポジションサイズで描画
```

---

### 問題 2: スケールの誤用

❌ **悪い例**:
```cpp
// ズームスケールをピクセル変換に使っている
cbTransform.scale = { w * viewportCB.scale.x, h * viewportCB.scale.y };
// これだと scale が「ピクセル数」になってしまう
```

⭕ **良い例**:
```cpp
// ズームスケールは変換係数として使用
cbTransform.scale = { viewportCB.scale.x, viewportCB.scale.y };
// または
cbTransform.scale = { 1.0f, 1.0f };  // 既に頂点座標が Composition Space の場合
```

---

### 問題 3: 未初期化の変数

❌ **悪い例**:
```cpp
class Impl {
    Size_2D sourceSize_;  // 未初期化
};
```

⭕ **良い例**:
```cpp
class Impl {
    Size_2D sourceSize_{1920, 1080};  // デフォルト値で初期化
};
```

---

## デバッグガイド

### ログ出力項目

```cpp
qDebug() << "[VIEWPORT]"
         << "scale=" << viewportCB.scale      // ズーム倍率
         << "offset=" << viewportCB.offset    // パンオフセット
         << "screenSize=" << viewportCB.screenSize;  // ウィンドウサイズ

qDebug() << "[LAYER]"
         << "localPos=" << layerPosition      // Local Space
         << "compSize=" << compSize           // Composition Space
         << "sourceSize=" << sourceSize;      // レイヤーサイズ
```

### 検証チェックリスト

- [ ] `sourceSize()` が初期化されているか？
- [ ] チェッカーボードは Composition Space で描画されているか？
- [ ] レイヤー座標は Local Space で定義されているか？
- [ ] ビューポート変換は正しく適用されているか？
- [ ] NDC 変換で Y 反転しているか？

---

## 用語集

| 用語 | 定義 |
|------|------|
| **NDC** | Normalized Device Coordinates. -1〜1 の範囲 |
| **Viewport** | 表示領域の変換パラメータ（ズーム・パン） |
| **Composition** | コンポジション。レイヤーのコンテナ |
| **Layer** | レイヤー。描画要素 |
| **Transform** | 変換行列（位置・回転・スケール） |
| **Anchor Point** | レイヤーの基準点 |

---

## 改版履歴

| 版本 | 日付 | 変更内容 |
|------|------|----------|
| 1.0 | 2026-03-18 | 初版作成 |
