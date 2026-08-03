# Maya-style Flexible Grid System 設計

**日付**: 2026-07-31
**現状**: `ArtifactCore/include/Grid/ArtifactGridSystem.ixx` に基本的な `GridSystem` + `GridSettings` が実装済み
**目標**: 単一グリッドを超えて、複数グリッド・複数タイプ・領域別・レイヤー別・ズーム連動を可能にする

---

## 1. 現状の GridSystem の評価

### 1.1 既にあるもの
| 機能 | 状態 |
|------|------|
| major/minor グリッド線の生成 | [x] |
| 単位系（px, cm, inch, pt, mm）+ DPI連動 | [x] |
| 原点軸（X=0, Y=0）の強調線 | [x] |
| 色/線種/太さの設定 | [x] |
| ズーム・パン連動の表示範囲計算 | [x] |
| スナップ計算（`snapToGrid`） | [x] |
| キャッシュ（変更時のみ再計算） | [x] |

### 1.2 ないもの
| 機能 | 優先度 |
|------|--------|
| **複数グリッドの同時表示** | 高 |
| **ズームに応じた動的ステップ（Maya式）** | 高 |
| **グリッドタイプの拡張（極座標、アイソメトリック）** | 中 |
| **領域別グリッド設定** | 中 |
| **グリッドフェード（3D空間）** | 低 |
| **グリッド平面の3D回転** | 低 |
| **グリッド数値ラベルの描画** | 高 |
| **グリッドの視覚的縮退（遠くで薄くなる）** | 中 |

---

## 2. アーキテクチャ全体像

```
GridManager (統合管理)
├── GridLayer[0]: "Composition Grid"   ← コンポジション全体
│   └── GridDescriptor → GridSystem → GridLines
├── GridLayer[1]: "3D Ground Grid"    ← 3Dビューポートの地面
│   └── GridDescriptor → GridSystem → GridLines
├── GridLayer[2]: "Layer Local Grid"  ← 選択レイヤーのローカルグリッド
│   └── GridDescriptor → GridSystem → GridLines
└── GridLayer[N]: ...
```

各 GridLayer は独立した `GridDescriptor` ＋ `GridSystem` のペア。

---

## 3. コア型定義

### 3.1 GridType（グリッドの種類）

```cpp
export enum class GridType : uint8_t {
    Rectangular,    // 標準のXYグリッド（既存）
    Polar,          // 極座標グリッド（同心円 + 放射線）
    Isometric,      // アイソメトリックグリッド（30°-150°）
    Radial,         // 放射状グリッド（中心からの放射線のみ）
    Perspective,    // 遠近グリッド（消失点に向かう線）
    Custom          // カスタム（コールバックで線を生成）
};
```

### 3.2 GridPlane（グリッド平面）

3D 空間でのグリッド配置を定義。

```cpp
export enum class GridPlane : uint8_t {
    XY,  // 正面（2Dコンポジション面）
    XZ,  // 地面（3Dビューポートの地面）
    YZ,  // 側面
    View // カメラに正対する平面（常にビューと平行）
};

export struct GridPlaneTransform {
    GridPlane plane = GridPlane::XY;
    QVector3D origin = {0, 0, 0};     // 3D空間での原点
    QVector3D xAxis  = {1, 0, 0};
    QVector3D yAxis  = {0, 1, 0};
    // 2Dコンポジションでは origin = (0,0), 軸は標準のまま
};
```

### 3.3 GridRegion（領域別グリッド）

```cpp
export struct GridRegion {
    QString name;
    QRectF canvasRect;                 // キャンバス座標での矩形範囲
    std::optional<GridSettings> overrideSettings; // nullopt = 親グリッドの設定を継承
    bool enabled = true;
};
```

### 3.4 GridAutoStep（動的ステップ計算）

```cpp
export struct GridAutoStepConfig {
    bool enabled = false;              // 動的ステップを有効にするか
    float targetViewportInterval = 100.0f; // ビューポート上の目標間隔（px）
    int minSubdivisions = 1;
    int maxSubdivisions = 10;
    bool useNiceNumbers = true;        // Maya式 1-2-5 系列

    // 視覚的縮退
    bool useViewFade = false;          // ズームアウト時にグリッドを段階的に簡略化
    struct FadeLevel {
        float zoomMax;                 // このズーム値以下でこのレベル
        int majorStepMultiplier;       // メジャーステップの倍率（1, 2, 5, 10...）
        float alpha;                   // 不透明度
    };
    std::vector<FadeLevel> fadeLevels;
};
```

### 3.5 GridDescriptor（1つのグリッド定義）

```cpp
export struct GridDescriptor {
    QString name;                       // 識別名（"Composition", "3D Ground" など）
    GridType type = GridType::Rectangular;
    GridPlaneTransform plane;           // 3D平面
    GridSettings baseSettings;          // 基本設定
    GridAutoStepConfig autoStep;        // 動的ステップ設定
    std::vector<GridRegion> regions;    // 領域別オーバーライド（空 = 全体）

    // 表示
    bool visible = true;
    int zOrder = 0;                     // 描画順（小さいほど奥）
    float globalAlpha = 1.0f;

    // スナップ
    bool snapEnabled = true;            // このグリッドをスナップ対象にするか
    float snapPriority = 1.0f;          // 複数グリッド時の優先度（大きいほど優先）

    // ラベル
    bool showLabels = false;
    float labelFontSize = 9.0f;         // VPピクセル単位

    // カスタムグリッド用
    std::function<std::vector<GridLine>(const GridDescriptor&,
                                         const GridViewTransform&)> customGenerator;
};
```

### 3.6 GridLine（1本のグリッド線）

```cpp
export struct GridLine {
    Detail::float2 start;          // キャンバス座標（std::optional で始点/終点が null も可）
    Detail::float2 end;
    FloatColor color;
    float thickness;
    GridLineStyle style;
    std::optional<QString> label;  // ラベル付きの場合
    bool isMajor = true;

    // 3D用（将来）
    float depth = 0.0f;            // 3D空間でのカメラからの距離（フェード計算用）
};
```

### 3.7 GridLayer（1つのグリッドインスタンス）

`GridDescriptor` + `GridSystem` + キャッシュのラッパー。

```cpp
export class GridLayer {
public:
    explicit GridLayer(const GridDescriptor& desc);

    void setDescriptor(const GridDescriptor& desc);
    const GridDescriptor& descriptor() const;

    /// 現在のビュー状態からグリッド線を生成
    /// 動的ステップが有効な場合、zoom に応じて baseSettings の majorInterval を自動調整
    std::vector<GridLine> computeLines(const GridViewTransform& view,
                                       float zoom) const;

    /// スナップ計算
    float snap(float canvasPos, bool isVertical) const;

    /// 領域がこのレイヤーの範囲内か
    bool containsPoint(const QPointF& canvasPos) const;

private:
    GridDescriptor desc_;
    mutable GridSystem system_;
    mutable float lastAutoStepInterval_ = -1.0f; // 動的ステップ再計算用

    void applyAutoStep(float zoom) const;
};
```

### 3.8 GridManager（統合管理）

```cpp
export class GridManager {
public:
    /// GridLayer の追加/削除
    int addLayer(const GridDescriptor& desc);
    void removeLayer(int id);
    void setLayerDescriptor(int id, const GridDescriptor& desc);
    const GridDescriptor* layerDescriptor(int id) const;

    /// 全グリッドの線を生成（zOrder でソート）
    std::vector<GridLine> computeAllLines(const GridViewTransform& view,
                                           float zoom) const;

    /// 全グリッドに対してスナップ（最も近いグリッド線にスナップ）
    float snapAll(float canvasPos, bool isVertical) const;

    /// 特定のグリッドレイヤーを表示/非表示
    void setVisible(int id, bool visible);
    bool isVisible(int id) const;

    /// 表示レイヤー数
    int visibleLayerCount() const;

private:
    struct Entry {
        int id;
        GridLayer layer;
        bool visible = true;
    };
    std::vector<Entry> layers_;
    int nextId_ = 0;
};
```

---

## 4. グリッドタイプ別の線生成ロジック

### 4.1 Rectangular（既存、拡張）

既存の `GridSystem::computeVisibleLines()` をベースに、`GridLine` 形式で出力するよう拡張。

```
入力: GridDescriptor, GridViewTransform, zoom
処理:
  1. autoStep が有効 → zoom に応じて majorInterval を動的変更
  2. regions がある → 領域ごとに異なる設定で線を生成
  3. GridSystem::computeVisibleLines() を呼び出し
  4. GridLine に変換（色/太さ/スタイルを付与）
出力: vector<GridLine>
```

### 4.2 Polar（極座標グリッド）

```
入力: origin (= canvasPos), maxRadius, angularStep, radialStep
処理:
  for r in [0 .. maxRadius] step radialStep:
      → 円を生成 (circle to polyline)
      → majorRadius の倍数なら isMajor = true
  for angle in [0 .. 360] step angularStep:
      → 原点から maxRadius までの放射線
      → 0°, 90°, 180°, 270° は isMajor = true
出力: vector<GridLine>
```

### 4.3 Isometric（アイソメトリック）

```
入力: origin, tileSize, viewRect
処理:
  3方向の線を生成:
  - 方向1: (1, 0.577)   // 30°
  - 方向2: (-1, 0.577)  // 150°
  - 方向3: (0, 1.155)   // 90°（垂直）
  各方向について tileSize 間隔で平行線を生成
出力: vector<GridLine>
```

### 4.4 Perspective（遠近グリッド）

```
入力: vanishingPoint, horizonY, lineCount
処理:
  horizon からの水平線を等間隔で生成（遠近法で間隔が徐々に狭まる）
  消失点に向かう放射線を angularStep で生成
出力: vector<GridLine>
```

---

## 5. 動的ステップ（Auto Step）

### 5.1 アルゴリズム

```cpp
void GridLayer::applyAutoStep(float zoom) const {
    if (!desc_.autoStep.enabled) return;

    // targetViewportInterval: VP上で「見やすい」グリッド線の間隔（例: 100px）
    // → キャンバス単位に変換
    float rawInterval = desc_.autoStep.targetViewportInterval / zoom;

    // Maya式 1-2-5 系列できりのいい値にスナップ
    float niceInterval = snapToNiceValue(rawInterval);

    if (std::abs(niceInterval - lastAutoStepInterval_) < 0.01f) return;

    lastAutoStepInterval_ = niceInterval;
    auto& settings = const_cast<GridSettings&>(system_.settings());
    settings.majorInterval = niceInterval;

    // subdivisions: targetViewportInterval を超えない範囲で分割
    if (desc_.autoStep.useNiceNumbers) {
        float subPx = (niceInterval / settings.subdivisions) * zoom;
        if (subPx < desc_.autoStep.targetViewportInterval * 0.2f) {
            settings.subdivisions = std::max(1, settings.subdivisions / 2);
        } else if (subPx > desc_.autoStep.targetViewportInterval * 0.8f) {
            settings.subdivisions = std::min(desc_.autoStep.maxSubdivisions,
                                              settings.subdivisions * 2);
        }
    }

    system_.setSettings(settings);
}
```

### 5.2 視覚的縮退（Fade Levels）

```cpp
// 例: 遠くでグリッドを簡略化
fadeLevels: [
    { zoomMax: 0.1f,  majorStepMultiplier: 10, alpha: 0.2f },  // 超ズームアウト
    { zoomMax: 0.25f, majorStepMultiplier: 5,  alpha: 0.5f },  // ズームアウト
    { zoomMax: 0.5f,  majorStepMultiplier: 2,  alpha: 0.8f },  // ややズームアウト
    { zoomMax: INF,    majorStepMultiplier: 1,  alpha: 1.0f },  // 通常
]
```

---

## 6. 描画統合

### 6.1 CompositionRenderController への統合

```cpp
// Impl に追加
class Impl {
    // ...
    GridManager gridManager_;
    bool showMayaGrid_ = true;

    /// デフォルトグリッドレイヤーの初期化（初回のみ）
    void ensureDefaultGridLayers() {
        if (gridManager_.layerCount() > 0) return;

        // 1. コンポジショングリッド（2D）
        GridDescriptor compGrid;
        compGrid.name = "Composition";
        compGrid.type = GridType::Rectangular;
        compGrid.baseSettings.majorInterval = 100.0f;
        compGrid.baseSettings.subdivisions = 4;
        compGrid.autoStep.enabled = true;
        compGrid.autoStep.targetViewportInterval = 100.0f;
        compGrid.showLabels = true;
        compGrid.zOrder = 0;
        gridManager_.addLayer(compGrid);

        // 2. 3Dグラウンドグリッド（3Dビューポート時のみ表示）
        GridDescriptor groundGrid;
        groundGrid.name = "3D Ground";
        groundGrid.type = GridType::Rectangular;
        groundGrid.plane.plane = GridPlane::XZ;
        groundGrid.baseSettings.majorInterval = 100.0f;
        groundGrid.baseSettings.subdivisions = 10;
        groundGrid.baseSettings.majorColor = {0.3f, 0.3f, 0.3f, 0.6f};
        groundGrid.autoStep.enabled = true;
        groundGrid.autoStep.targetViewportInterval = 80.0f;
        groundGrid.autoStep.useViewFade = true;
        groundGrid.zOrder = 1;
        groundGrid.visible = false; // 3Dモード時のみ表示
        gridManager_.addLayer(groundGrid);
    }
};
```

### 6.2 描画呼び出し

```cpp
void Impl::drawViewportCanvasOverlay(float cw, float ch) {
    // ... existing grid drawing ...

    // Maya-style flexible grid
    if (showMayaGrid_) {
        ensureDefaultGridLayers();

        const float zoom = std::max(renderer_->getZoom(), 0.001f);
        float panX = 0, panY = 0;
        renderer_->getPan(panX, panY);

        GridViewTransform view;
        view.zoom = zoom;
        view.pan = {panX, panY};
        view.visibleCanvasRect = QRectF(-panX / zoom, -panY / zoom,
                                         hostWidth_ / zoom, hostHeight_ / zoom);
        view.canvasToViewport = /* 適切な変換行列 */;

        auto allLines = gridManager_.computeAllLines(view, zoom);

        for (const auto& line : allLines) {
            switch (line.style) {
            case GridLineStyle::Solid:
                renderer_->drawSolidLine(line.start, line.end, line.color, line.thickness);
                break;
            case GridLineStyle::Dash:
                renderer_->drawDashedLineLocal(line.start, line.end, line.thickness, 8, 4, line.color);
                break;
            case GridLineStyle::Dot:
                renderer_->drawDottedLineLocal(line.start, line.end, line.thickness, 2, 6, line.color);
                break;
            // ...
            }
            if (line.label) {
                // 線の中間点にラベルを描画
                Detail::float2 mid = {(line.start.x + line.end.x)/2,
                                       (line.start.y + line.end.y)/2};
                renderer_->drawText(QRectF(mid.x - 50, mid.y - 10, 100, 20),
                                    *line.label, labelFont, line.color, ...);
            }
        }
    }
}
```

---

## 7. ファイル構成

```
ArtifactCore/include/Grid/
├── ArtifactGridSystem.ixx        ← 既存（拡張）
├── ArtifactGridTypes.ixx          ← 新規: GridType, GridPlane, GridLine, GridDescriptor, etc.
└── ArtifactGridManager.ixx        ← 新規: GridLayer, GridManager

ArtifactCore/src/Grid/
├── ArtifactGridSystem.cppm        ← 既存（拡張）
├── ArtifactGridPolar.cppm         ← 新規: Polarグリッド線生成
├── ArtifactGridIsometric.cppm     ← 新規: Isometricグリッド線生成
└── ArtifactGridPerspective.cppm   ← 新規: Perspectiveグリッド線生成
```

既存の `GridSystem` の変更は**最小限**（動的ステップ対応のための setter 追加のみ）。

---

## 8. スナップ連携

```cpp
// handleMouseMove 内
float snapAllGrids(float canvasPos, bool isVertical) const {
    if (!gridManager_ || gridManager_->visibleLayerCount() == 0)
        return canvasPos;

    return gridManager_->snapAll(canvasPos, isVertical);
}
```

---

## 9. 使用シナリオ

| シナリオ | 設定 |
|----------|------|
| 通常の2Dコンポジション作業 | Rectangular, px, autoStep有効, subdivisions=4 |
| 3Dオブジェクト配置 | Rectangular XZ平面 + autoStep + viewFade |
| UIデザイン（px単位） | Rectangular, px, majorInterval=8/16, autoStep無効 |
| 印刷デザイン（mm単位） | Rectangular, mm, majorInterval=10, subdivisions=10 |
| ゲーム用タイルマップ | Isometric, tileSize=32px |
| 放射状デザイン | Polar, 中心指定, angularStep=15° |
| レイヤー個別グリッド | 選択レイヤーに overlay grid layer 追加 |
