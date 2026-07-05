# M-PAINT-1 Paint Layer / Raster Editing Foundation Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Layer/ArtifactImageLayer.cppm`,
      `Artifact/src/Layer/ArtifactNullLayer.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Tool/ArtifactToolManager.cppm`,
      `Artifact/src/Service/ArtifactLayerService.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `Artifact/src/Undo/*`,
      `ArtifactCore/src/Image/ImageF32x4_RGBA.ixx`,
      `ArtifactCore/src/Image/ImageF32x4RGBAWithCache.ixx`,
      `ArtifactCore/src/Image/ImageInterface.ixx`
位置づけ: `MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` を実装フェーズに進める foundation。新規 `ArtifactPaintLayer` + BrushTool / EraserTool で Photoshop 風の直接描画を導入。
参照:
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.1
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P2)
- `docs/planned/MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` (parent)
- `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` (linear canonical)

---

## 1. 目的

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.1:

> - Paint layer (animated brush): 0 hit
> - Paint effects (Smear, Clone Stroke 等): 0 hit

`MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` は **設計段階** で止まっている。本 milestone はそれを **実装 phase** に進める:

- 新規 `ArtifactPaintLayer` を layer type として追加
- `BrushTool` / `EraserTool` を別 ownership で分離
- stroke preview は `Overlay.Composition` に閉じる
- software / Diligent の両方で破綻しない構造

> 重要: `QImage` を **新規 hot path に入れない**。`ImageF32x4RGBAWithCache` / `ImageF32x4_RGBA` 経由で描画。`ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` — 設計
- `MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md` — toolbar。`brush / eraser / clone stamp` 候補
- `Artifact/src/Layer/ArtifactImageLayer.cppm` — image layer
- `Artifact/src/Layer/ArtifactNullLayer.cppm` — null layer
- `Artifact/src/Tool/ArtifactToolManager.cppm` — tool manager
- `ArtifactCore/src/Image/ImageF32x4_RGBA.ixx` — image 型
- `ArtifactCore/src/Image/ImageF32x4RGBAWithCache.ixx` — cache 付き image
- `ArtifactCore/src/Image/ImageInterface.ixx` — interface

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| PaintLayer type | 0 hit | 新規 layer type 不在 |
| Brush stroke データ | 0 hit | stroke 履歴が無い |
| Brush / Eraser tool | toolbar 候補のみ | 実装なし |
| Overlay preview | 0 hit | brush cursor などの一時描画 |
| Undo | 0 hit | stroke 単位の undo 無し |
| 永続化 | 0 hit | paint content 保存なし |
| PSD 互換 | 0 hit | PSD pixel layer 読込 |
| Diagnostics | 0 hit | paint integrity チェック無し |

### 2.3 既存 milestone との関係

- `MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` — 親。本 milestone は実装 phase
- `MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md` — toolbar。`brush / eraser` を `ArtifactPaintLayer` と接続
- `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` — `B` (brush) / `E` (eraser) ショートカット登録先

---

## 3. 設計の柱

### 3.1 ArtifactPaintLayer

`Artifact/include/Layer/ArtifactPaintLayer.ixx` を新規追加:

```cpp
namespace Artifact {

class ArtifactPaintLayer : public ArtifactAbstractLayer {
public:
    explicit ArtifactPaintLayer(QObject* parent = nullptr);

    // 描画サイズ
    QSize canvasSize() const;
    void setCanvasSize(const QSize& size);

    // ピクセル内容 (linear premultiplied)
    std::shared_ptr<ArtifactCore::ImageF32x4RGBAWithCache> canvas() const;
    void setCanvas(std::shared_ptr<ArtifactCore::ImageF32x4RGBAWithCache> canvas);

    // 操作
    void applyStroke(const PaintStroke& stroke);

    // 永続化
    QJsonObject toJson() const;
    static ArtifactPaintLayer* fromJson(const QJsonObject& obj, QObject* parent = nullptr);
};

} // namespace Artifact
```

- `ArtifactAbstractLayer` 派生
- `canvas_` は `shared_ptr<ImageF32x4RGBAWithCache>` で linear premultiplied canonical
- `QImage` を使わず、`ImageF32x4RGBAWithCache::pixelData(x, y)` 経由で編集

### 3.2 PaintStroke データモデル

`ArtifactCore/include/Paint/PaintStroke.ixx`:

```cpp
namespace ArtifactCore {

enum class PaintTool {
    Brush,
    Eraser,
    Smear,         // 将来
    CloneStamp,    // 将来
    SoftBrush,     // 将来
    HardBrush,     // 将来
};

struct BrushSettings {
    PaintTool tool = PaintTool::Brush;
    float radius = 10.0f;            // px
    float hardness = 0.8f;           // 0..1
    float opacity = 1.0f;            // 0..1
    float flow = 1.0f;               // 0..1
    ArtifactCore::FloatRGBA color;   // linear premultiplied
    bool eraseMode = false;          // eraser 時は true
};

struct PaintPoint {
    QPointF position;        // layer 座標
    float pressure = 1.0f;    // 筆圧 (将来: tablet 入力)
    float timestamp = 0.0f;   // ms
};

struct PaintStroke {
    QString id;
    BrushSettings settings;
    QList<PaintPoint> points;
    int64_t frame = 0;        // 適用 frame

    QJsonObject toJson() const;
    static PaintStroke fromJson(const QJsonObject& obj);
};

} // namespace ArtifactCore
```

- 1 stroke = 1 つの連続した drag
- 点列 (QList<PaintPoint>) で stroke 形状を保持
- pressure / timestamp は将来 tablet 対応

### 3.3 BrushTool / EraserTool

`Artifact/src/Tool/Paint/ArtifactBrushTool.cppm` を新規追加:

```cpp
class ArtifactBrushTool : public QObject {
public:
    explicit ArtifactBrushTool(QWidget* parent = nullptr);

    void setBrushSettings(const BrushSettings& settings);
    BrushSettings brushSettings() const;

    // stroke 入力
    void beginStroke(const QPointF& pos, int64_t frame);
    void addPoint(const QPointF& pos, float pressure = 1.0f);
    void endStroke();

    // 直前 stroke
    PaintStroke lastStroke() const;

signals:
    void strokeApplied(const PaintStroke& stroke);
};
```

- `EraserTool` は `ArtifactBrushTool` 派生 (`eraseMode = true` 固定)
- 既存 `ArtifactToolManager` に登録
- stroke preview は **Overlay.Composition** 側

### 3.4 PaintEngine

`ArtifactCore/src/Paint/PaintEngine.cppm` を新規追加:

```cpp
class PaintEngine {
public:
    // stroke を canvas に適用
    static void applyStroke(
        std::shared_ptr<ImageF32x4RGBAWithCache> canvas,
        const PaintStroke& stroke);

    // 1 点を canvas に適用
    static void applyDab(
        std::shared_ptr<ImageF32x4RGBAWithCache> canvas,
        const PaintPoint& point,
        const BrushSettings& settings,
        const QList<QPointF>& lastStrokePoints);  // smear 用履歴
};
```

- 1 dab = 円形 brush の stamp
- hardness で edge falloff
- opacity / flow で alphablend
- eraser は `dst *= (1 - alpha)`
- `QImage` を使わず `ImageF32x4RGBAWithCache::pixelData(x, y)` 経由で編集

### 3.4 Future Brush Extensions

この milestone の first pass では扱わないが、paint / brush core が安定したあとに検討する拡張。

- `NanoPixel`
  - 超高解像度ブラシ描画の将来案。
  - 画素単位の stamp だけに頼らず、サブピクセル精度、複数サンプルの畳み込み、dab 拡散の物理モデルを使って、見た目の精細さを画素数以上に引き上げる方向で検討する。
  - まずは stroke core が安定してから、解像度依存しない brush engine の拡張として扱う。
- `Pigment Color Mixing`
  - RGB 合成ではなく、顔料ベースの混色を扱う将来案。
  - cyan / magenta / yellow / white / black などの顔料チャネルを持つ brush preset や stroke preset を候補にする。
  - まずは表示用 RGB とは別に編集用の物理混色モデルとして定義し、レンダリング側で RGB へ落とす。

### 3.5 Overlay.Composition 統合

既存 `Overlay.Composition` の `paintEvent` に **brush cursor** を追加:

- brush radius を円で描画
- 押下中は stroke preview を描画
- 色は `theme token` 経由

### 3.6 Undo

`Artifact/Undo/PaintStrokeCommand.cppm`:

```cpp
class PaintStrokeCommand : public QUndoCommand {
public:
    PaintStrokeCommand(ArtifactPaintLayer* layer,
                       const PaintStroke& stroke,
                       std::shared_ptr<ImageF32x4RGBAWithCache> before);

    void undo() override;   // canvas 復元
    void redo() override;
};
```

- 1 stroke = 1 undo
- stroke 適用前の canvas snapshot を保持
- `QUndoStack::beginMacro` で複数 stroke を束ねる

### 3.7 Tool 統合

`Artifact/src/Tool/ArtifactToolManager.cppm` に新規 tool を登録:

- `ToolType::PaintBrush`
- `ToolType::PaintEraser`
- ショートカット:
  - `B` (Brush)
  - `E` (Eraser)
  - `[` (radius -)
  - `]` (radius +)
  - `Shift+[` (hardness -)
  - `Shift+]` (hardness +)

### 3.8 Project 保存

- `ArtifactProjectManager` の project JSON に `layer.type = "Paint"` 追加
- `layer.paint.canvas` を base64 で保存 (PNG / TIFF 経由ではなく **生 float**)
- 旧プロジェクトは paint 欠落を許容

### 3.9 不変条件 (Guardrails)

- `QImage` を **新規 hot path に入れない**。`ImageF32x4RGBAWithCache` 経由
- 既存 `ArtifactImageLayer` を **変更しない**。新 layer type として並走
- `RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear premultiplied canonical に従う
- 既存 `ArtifactToolManager` の API は温存。新規 tool を追加するだけ
- 既存 `setStyleSheet` / 新規 `QImage` 流入禁止
- 新規 signal-slot 接続は `strokeApplied / paintLayerChanged` 2 個に限定
- brush stroke は worker thread ではなく main thread 描画 (UI レスポンス重視)

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `paint.canvas.missing` (severity=error, canvas が未設定)
- `paint.canvas.size-mismatch` (severity=warning, canvas size と layer size 不一致)
- `paint.stroke.invalid` (severity=info, stroke が空 / 不正)
- `paint.uncompressed-large` (severity=info, canvas が 100 MB 超)

---

## 4. フェーズ計画

### Phase 1: Core data + PaintStroke (P0, 1 セッション)

- `ArtifactCore/include/Paint/PaintStroke.ixx` 新規
- `ArtifactCore/src/Paint/PaintStroke.cppm` 実装
- 永続化

**Done criteria:**
- 1 stroke を追加 / 削除 / 永続化
- `points[]` を保存
- round-trip 一致

### Phase 2: PaintEngine + dab (P0, 1〜2 セッション)

- `ArtifactCore/src/Paint/PaintEngine.cppm` 実装
- `applyDab` で円形 brush stamp
- hardness / opacity / flow 反映
- eraser 対応

**Done criteria:**
- 1 dab が canvas に正しく描画
- hardness 0.0 で edge falloff
- eraser で alpha 減衰
- `QImage` を経由しない

### Phase 3: ArtifactPaintLayer (P0, 1 セッション)

- `Artifact/include/Layer/ArtifactPaintLayer.ixx` 新規
- `Artifact/src/Layer/ArtifactPaintLayer.cppm` 実装
- `applyStroke(stroke)` 実装
- layer factory 登録

**Done criteria:**
- PaintLayer を composition に追加可能
- `applyStroke` で canvas 更新
- 既存 layer と同じ保存 / 復元

### Phase 4: BrushTool / EraserTool (P0, 1〜2 セッション)

- `Artifact/src/Tool/Paint/ArtifactBrushTool.cppm` 実装
- mouseDown / mouseMove / mouseUp で stroke 構築
- 既存 `ArtifactToolManager` 登録
- ショートカット登録

**Done criteria:**
- mouse drag で stroke 構築
- 既存 tool 切替と干渉しない
- ショートカット `B` / `E` で切替

### Phase 5: Overlay + UI (P0, 1〜2 セッション)

- brush cursor を Overlay に描画
- stroke preview
- Inspector に brush settings panel

**Done criteria:**
- brush radius が円で見える
- stroke preview が描画される
- Inspector で size / hardness / opacity / color 変更

### Phase 6: Undo + Project 保存 (P0, 1 セッション)

- `PaintStrokeCommand` 追加
- project JSON に paint layer 追加
- 旧プロジェクトの default 補完

**Done criteria:**
- 1 stroke が 1 undo で復元
- project 保存 → 再読込で paint 復元
- 旧プロジェクトが開ける

### Phase 7: Diagnostics (P1, 1 セッション)

- Problem View への `paint.*` 健全性 contribution

**Done criteria:**
- `paint.canvas.missing` 等が表示
- `paint.uncompressed-large` で size 警告

### Phase 8: Smear / Clone Stamp / Healer (P2, 別 milestone 推奨)

- Smear brush (履歴利用)
- Clone Stamp (source 参照)
- Healer (近傍 patch)
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_PAINT_ADVANCED_2026-XX-XX.md` のエントリポイントを作る

### Phase 9: PSD pixel layer 読込 (P2, 別 milestone 推奨)

- PSD 互換の pixel layer stack
- `MILESTONE_DESIRED_IMPORT_FORMATS_2026-04-19.md` の PSD 言及
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_PSD_PIXEL_LAYER_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` | 親。本 milestone は実装 phase。 |
| `MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md` | toolbar。本 milestone は新規 tool。 |
| `MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md` | shortcut 登録先。 |
| `MILESTONE_RENDER_FORMAT_CONTRACT_2026-05-16.md` | linear canonical。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Dab 計算の数値安定性**。alpha blend の edge で over / under 防止
2. **Eraser の alpha 計算**。premultiplied での eraser 演算
3. **Smear**。履歴 reference。Phase 8 で対応
4. **Tablet 入力**。pressure / tilt。Phase 1 は固定 1.0
5. **Stroke の undo granularity**。stroke 単位 vs dab 単位。本 milestone は stroke 単位
6. **PSD 互換**。`ImageF32x4RGBAWithCache` と PSD の uint8 RGBA との変換

### 6.2 契約上の未解決

- **既存 image layer への統合**。`ArtifactImageLayer` に paint 機能を追加するか別 layer type か。本 milestone は **別 layer type**。統合は Phase 9 以降
- **Render path への影響**。PaintLayer は render path で `ImageF32x4RGBAWithCache` を直接 sample
- **GPU upload**。PaintLayer の canvas を GPU texture に upload する経路。`GPUTextureCacheManager` 経由
- **3D / VFX 用途**。PaintLayer は 2D のみ。3D は別 milestone

### 6.3 サブモジュール境界

- `ArtifactCore/include/Paint/PaintStroke.ixx` を新規追加
- `ArtifactCore/src/Paint/PaintEngine.cppm` を新規追加
- `Artifact/include/Layer/ArtifactPaintLayer.ixx` を新規追加
- `Artifact/src/Layer/ArtifactPaintLayer.cppm` を新規追加
- `Artifact/src/Tool/Paint/ArtifactBrushTool.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- PaintLayer を composition に追加できる
- Brush / Eraser tool で stroke 描画
- stroke が canvas に正しく反映
- 1 stroke = 1 undo で復元
- brush cursor が Overlay に表示
- stroke preview が描画される
- Inspector で brush 設定変更
- project 保存 → 再読込で paint 復元
- 旧プロジェクトは paint 欠落を許容
- Problem View に `paint.*` 健全性表示
- `QImage` を **新規 hot path に入れない**
- 既存 `ArtifactImageLayer` API が温存
- 既存 `ArtifactToolManager` API が温存
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.1 / §4 を正式 milestone に起こした。Paint Layer / Raster Editing foundation。`MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md` の実装 phase。
