# Maya-style Viewport Scale / Ruler Overlay 設計

**日付**: 2026-07-31
**目標**: Maya のような「VP端に目盛り（ruler/scale）とグリッド情報を表示する汎用的な仕組み」を実装する。

---

## 1. コンセプト

### Maya のビューポート目盛りの特徴
- ビューポートの**左下隅**に、現在のズームに応じたスケールバー（ruler）を表示
- 「1 unit = 100px」「1 unit = 10px」のように、現在のズームでの単位長を可視化
- 動的にズームに応じて目盛りの単位と間隔が変わる（10m → 1m → 10cm → 1cm ...）
- 単なる固定グリッドではなく、**ズーム連動**の動的スケール表示

### 汎用性の要件
- 単一の「スケールバー」だけでなく、**任意の位置・方向・スタイル**で配置可能
- **複数のオーバーレイを同時表示**可能（例: 左上にmm単位、右下にpx単位）
- レンダラーに依存しない純粋なデータ生成 + 描画分離
- 将来的な機能拡張（角度ルーラー、距離計測、3D空間のグリッドなど）にも対応

---

## 2. アーキテクチャ

### 2.1 レイヤー構造

```
ViewportScaleOverlay (管理クラス)
├── ViewportRuler (水平/垂直ルーラー)
├── ViewportScaleBar (スケールバー / 距離表示)
├── ViewportGridLabel (グリッド間隔ラベル)
└── ViewportCompassWidget (3D空間の方向指示)
```

### 2.2 データ駆動設計

描画データは「ゼロから生成」し、レンダラーに依存しない中間表現に変換してから描画する。これにより、複数のレンダリングバックエンド（Diligent / 2D Primitive / etc）に対応できる。

```
[Input Params] → [Tick Calculator] → [Tick List] → [Renderer Adapter] → [draw calls]
```

---

## 3. コアクラス設計

### 3.1 ViewportTickCalculator

ズーム値から「どの単位で」「どの間隔で」目盛りを打つかを計算する。

```cpp
struct ViewportTickStep {
    float value;         // 実際のワールド単位（例: 100.0f px）
    float interval;      // 目盛り間隔（キャンバス座標、例: 100.0f）
    float subInterval;   // サブ目盛り間隔（0 = なし）
    QString label;       // 表示ラベル（例: "100 px", "1 cm"）
    int subdivisionsPerMajor; // メジャー間のサブ目盛り数
};

class ViewportTickCalculator {
public:
    /// ズーム値とターゲット間隔(px)から最適な目盛りステップを計算
    /// @param zoom 現在のズーム値（1.0 = 100%）
    /// @param targetPixels 目標の目盛り間隔（ビューポートピクセル）
    /// @param unitName 単位名（"px", "mm", "cm", "m" など）
    static ViewportTickStep compute(float zoom, float targetPixels,
                                     const QString& unitName = "px");

    /// Maya式: 1-2-5 系列で「きりのいい数字」を選択
    static float snapToNiceValue(float rawValue);
};
```

**計算ロジック（Maya式 1-2-5 ステップ）**:
```
rawValue = targetPixels / zoom  // ビューポート上の目標ピクセル間隔をキャンバス単位に変換

if   rawValue < 0.1  → step = 0.1, 0.2, 0.5
elif rawValue < 1.0  → step = 1, 2, 5
elif rawValue < 10   → step = 10, 20, 50
elif rawValue < 100  → step = 100, 200, 500
...
```

**サブ目盛りの間隔**:
- メジャー目盛り間隔の 1/5 または 1/10
- ビューポート上で 4px 未満になる場合はサブ目盛りを省略

### 3.2 ViewportRulerTick（目盛り1つのデータ）

```cpp
struct ViewportRulerTick {
    enum class Level { Major, Minor, SubMinor };
    Level level;
    float canvasPos;       // キャンバス座標（水平ルーラーならX、垂直ならY）
    float viewportPos;     // ビューポート座標（描画位置の計算用）
    QString label;         // Major の場合のみラベル
};
```

### 3.3 ViewportRuler（ルーラー本体）

```cpp
enum class RulerOrientation { Horizontal, Vertical };
enum class RulerAnchor { Start, Center, End };

struct ViewportRulerConfig {
    RulerOrientation orientation = RulerOrientation::Horizontal;
    RulerAnchor anchor = RulerAnchor::Start;       // キャンバス原点 or 中心 or 右端
    float margin = 8.0f;                            // VP端からのマージン（px）
    float tickLengthMajor = 12.0f;
    float tickLengthMinor = 6.0f;
    float tickLengthSub = 3.0f;
    FloatColor tickColor = {0.5f, 0.5f, 0.5f, 0.6f};
    FloatColor labelColor = {0.8f, 0.8f, 0.8f, 0.9f};
    float labelFontSize = 10.0f;
    bool showLabels = true;
    bool showTicks = true;
};

class ViewportRuler {
public:
    void configure(const ViewportRulerConfig& cfg);
    
    /// ズームとビューポートサイズから目盛りリストを生成
    /// @param zoom 現在のズーム
    /// @param viewportOrigin ビューポート左上のキャンバス座標
    /// @param viewportSize ビューポートサイズ（物理ピクセル）
    /// @param canvasSize キャンバスサイズ
    std::vector<ViewportRulerTick> generateTicks(
        float zoom, const QPointF& viewportOrigin,
        const QSizeF& viewportSize, const QSizeF& canvasSize) const;
    
    /// 生成された目盛りリストを描画
    void draw(ArtifactIRenderer* renderer,
              const std::vector<ViewportRulerTick>& ticks) const;
    
private:
    ViewportRulerConfig config_;
};
```

### 3.4 ViewportScaleBar（スケールバー）

Maya の左下隅にあるような「物理的距離を示すバー」。

```cpp
struct ViewportScaleBarConfig {
    QPointF position = {16.0f, -16.0f}; // VP左下からの相対位置（px）
    RelativeTo relativeTo = RelativeTo::BottomLeft;
    float barHeight = 4.0f;
    float barWidth = 100.0f;            // 目標バー幅（px）、実際のキャンバス距離は zoom 依存
    FloatColor barColor = {0.8f, 0.8f, 0.8f, 0.8f};
    FloatColor labelColor = {0.9f, 0.9f, 0.9f, 0.9f};
    float labelFontSize = 10.0f;
    QString unitName = "px";
};

class ViewportScaleBar {
public:
    void configure(const ViewportScaleBarConfig& cfg);
    
    /// ズームからバーデータを生成
    struct ScaleBarData {
        float barCanvasWidth;      // バーのキャンバス幅
        float barViewportX, barViewportY; // 描画位置（VP座標）
        float barHeightPx;
        QString label;             // "100 px" など
    };
    ScaleBarData generate(float zoom, const QSizeF& viewportSize) const;
    
    void draw(ArtifactIRenderer* renderer, const ScaleBarData& data) const;
};
```

### 3.5 ViewportOverlayManager（統合管理クラス）

```cpp
class ViewportOverlayManager {
public:
    /// ルーラーを追加（複数追加可能）
    int addRuler(const ViewportRulerConfig& cfg);
    void removeRuler(int id);
    void configureRuler(int id, const ViewportRulerConfig& cfg);
    
    /// スケールバーを追加
    int addScaleBar(const ViewportScaleBarConfig& cfg);
    void removeScaleBar(int id);
    
    /// グリッドラベルを追加
    int addGridLabel(const ViewportGridLabelConfig& cfg);
    
    /// 全オーバーレイを再計算して描画
    /// @param zoom, viewportOrigin, viewportSize, canvasSize: 現在のVP状態
    /// @param オプションで filter を指定して特定の overlay だけ描画
    void drawAll(ArtifactIRenderer* renderer,
                 float zoom, const QPointF& viewportOrigin,
                 const QSizeF& viewportSize, const QSizeF& canvasSize);
    
    /// キャッシュ無効化（zoom/pan変更時など）
    void invalidateCache();
    
    /// 個別に表示/非表示
    void setVisible(int id, bool visible);

private:
    struct RulerEntry { int id; ViewportRuler ruler; bool visible; };
    struct ScaleBarEntry { int id; ViewportScaleBar bar; bool visible; };
    std::vector<RulerEntry> rulers_;
    std::vector<ScaleBarEntry> scaleBars_;
    
    // キャッシュ（zoomが変わらない限り再計算不要）
    struct TickCache {
        float cachedZoom = -1.0f;
        std::vector<ViewportRulerTick> ticks;
    };
    std::unordered_map<int, TickCache> rulerCache_;
};
```

---

## 4. 使用例

### 4.1 基本的な水平ルーラー（コンポジション下部）

```cpp
ViewportRulerConfig rulerCfg;
rulerCfg.orientation = RulerOrientation::Horizontal;
rulerCfg.anchor = RulerAnchor::Start; // キャンバス原点基準
rulerCfg.margin = 4.0f;
rulerCfg.showLabels = true;

int rulerId = overlayMgr_->addRuler(rulerCfg);
```

### 4.2 中心基準ルーラー

```cpp
ViewportRulerConfig centerRuler;
centerRuler.orientation = RulerOrientation::Horizontal;
centerRuler.anchor = RulerAnchor::Center; // コンポジション中心基準
centerRuler.margin = 4.0f;
centerRuler.tickColor = {1.0f, 0.3f, 0.3f, 0.6f}; // 中心は赤系

int centerRulerId = overlayMgr_->addRuler(centerRuler);
```

### 4.3 垂直ルーラー

```cpp
ViewportRulerConfig vRuler;
vRuler.orientation = RulerOrientation::Vertical;
vRuler.anchor = RulerAnchor::Start;
vRuler.margin = 4.0f;
overlayMgr_->addRuler(vRuler);
```

### 4.4 スケールバー

```cpp
ViewportScaleBarConfig barCfg;
barCfg.relativeTo = ViewportScaleBarConfig::RelativeTo::BottomLeft;
barCfg.position = {16.0f, -16.0f};
barCfg.unitName = "px";
overlayMgr_->addScaleBar(barCfg);
```

### 4.5 VP再描画時の呼び出し（CompositionRenderController 内）

```cpp
void CompositionRenderController::Impl::drawViewportCanvasOverlay(
    float cw, float ch) {
    // ... existing grid drawing ...
    
    // Maya-style overlays
    if (showViewportRuler_) {
        const float zoom = renderer_->getZoom();
        float panX = 0, panY = 0;
        renderer_->getPan(panX, panY);
        
        const QPointF viewportOrigin(-panX / zoom, -panY / zoom);
        const QSizeF viewportSize(hostWidth_, hostHeight_);
        const QSizeF canvasSize(cw, ch);
        
        overlayManager_->drawAll(renderer_, zoom, viewportOrigin,
                                  viewportSize, canvasSize);
    }
}
```

---

## 5. 将来拡張

### 5.1 角度ルーラー（Rotation Ruler）
- 回転ツール使用時に、現在の角度を円弧状ルーラーで表示
- 0°/90°/180°/270° にメジャーティック

### 5.2 距離計測（Measure Tool）
- 2点間の距離を計測するツール
- スケールバーの延長として距離ラベルを表示

### 5.3 3D 空間グリッド
- 3D ビューポートでの地面グリッド（XZ平面）
- 遠近感を考慮したフェードアウト

### 5.4 スナップガイドのティック表示
- スナップ位置に小さな三角形マーク

---

## 6. フォルダ構成

```
Artifact/
├── include/
│   └── Widgets/Render/
│       ├── ViewportOverlayManager.ixx        ← 新規
│       ├── ViewportRuler.ixx                  ← 新規
│       ├── ViewportScaleBar.ixx               ← 新規
│       └── ViewportTickCalculator.ixx         ← 新規
└── src/
    └── Widgets/Render/
        ├── ViewportOverlayManager.cppm        ← 新規
        ├── ViewportRuler.cppm                 ← 新規
        ├── ViewportScaleBar.cppm              ← 新規
        └── ViewportTickCalculator.cppm        ← 新規
```

既存の `GridRenderer` は単純な矩形グリッド専用なので、この新しい階層とは別に維持する。`ViewportOverlayManager` が将来的に `GridRenderer` を内包することも可能。

---

## 7. 描画パイプライン統合

### 7.1 呼び出しポイント
`CompositionRenderController::Impl::drawViewportCanvasOverlay()` の末尾に追加。

### 7.2 キャッシュ戦略
- 目盛り計算は zoom, viewportOrigin, canvasSize に依存 → 変更がなければスキップ
- 目盛りデータ（`vector<ViewportRulerTick>`）を各 ruler 毎にキャッシュ
- `invalidateCache()` で全キャッシュを破棄

### 7.3 パフォーマンス
- 目盛り数は最大でも viewportSize / minTickSpacing ≒ 数百個程度
- 各目盛りの描画は `drawSolidLine` 1本 + `drawText` 1回（Majorのみ）
- 毎フレームのオーバーヘッドは最小限（キャッシュヒット時はゼロ）
