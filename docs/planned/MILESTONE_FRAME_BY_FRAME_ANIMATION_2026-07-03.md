# M-FBF-1 Frame-by-Frame Animation Milestone

**作成日:** 2026-07-03
**ステータス:** Draft
**関連:**
- `docs/planned/MILESTONE_PAINT_LAYER_RASTER_EDITING_2026-06-01.md`
- `docs/planned/MILESTONE_PEN_TOUCH_JOYSTICK_2026-06-16.md`

---

## 1. 目的

Moho / Toon Boom 風のフレームバイフレームアニメーション機能を Artifact に追加する。
ペイントレイヤー + フレーム管理 + オニオンスキンで構成する。

---

## 2. 現状

| 要素 | 状態 | 詳細 |
|------|:----:|------|
| ペイントレイヤー設計 | 🟡 設計書あり | `MILESTONE_PAINT_LAYER_RASTER_EDITING` |
| PaintLayer クラス | ❌ 未実装 | — |
| BrushTool / EraserTool | ❌ 未実装 | PenTool のみ存在 |
| オニオンスキン | 🟡 API実装中 | フレームキャプチャ + オーバーレイ描画 |
| フレーム管理 | ❌ 未実装 | フレーム追加/削除/複製/ナビゲーション |
| ImageF32x4_RGBA バッファ | ✅ 利用可能 | ペイント内容の保存に使用可能 |

---

## 3. 設計

### 3.1 PaintLayer

新しいレイヤータイプとして `ArtifactPaintLayer` を追加:

```cpp
class ArtifactPaintLayer : public ArtifactAbstract2DLayer {
    // フレームごとのラスターバッファ
    std::map<FramePosition, ImageF32x4_RGBA> frames_;
    FramePosition currentFrame_;
    int frameRate_ = 24;

    // ブラシストローク適用
    void applyStroke(const BrushStroke& stroke);
    void undoLastStroke();

    // フレーム管理
    void addFrame(const FramePosition& pos);
    void removeFrame(const FramePosition& pos);
    void duplicateFrame(const FramePosition& src, const FramePosition& dst);
    FramePosition nextFrame() const;
    FramePosition prevFrame() const;
};
```

### 3.2 BrushTool

既存の PenTool パターンを流用:

```cpp
class BrushTool : public ArtifactAbstractTool {
    float radius_ = 10.0f;
    float opacity_ = 1.0f;
    FloatColor color_;
    bool eraserMode_ = false;
    // stroke recording for undo
    BrushStroke currentStroke_;
};
```

### 3.3 フレーム管理UI

| UI | 場所 | 動作 |
|----|------|------|
| フレーム追加 | Timeline + ショートカット | 現在位置に空フレーム追加 |
| 前/次フレーム | `PageUp`/`PageDown` | フレーム間移動 |
| Onion Skin | Viewport overlay | 前後Nフレーム半透明表示 |

### 3.4 既存資産の活用

- `ImageF32x4RGBAWithCache` → ペイントバッファの保存形式
- `PenTool` (210行) → ブラシの軌跡取得
- `CompositionRenderOverlay` → オニオンスキン描画
- `UndoManager` → ブラシストロークの Undo/Redo

---

## 4. 実装フェーズ

### Phase 1: PaintLayer データモデル (8-10h)
- `ArtifactPaintLayer` クラス
- フレームバッファ管理
- JSON 保存/復元
- `localBounds()` / `draw()` 実装

### Phase 2: BrushTool (8-10h)
- ブラシ描画 (円形/ソフト)
- ストローク記録 + Undo
- 消しゴムモード
- カーソルプレビュー (Overlay)

### Phase 3: フレーム管理 UI + ショートカット (4-6h)
- フレーム追加/削除/複製 UI
- 前/次フレームナビゲーション
- フレームインジケータ

### Phase 4: オニオンスキン連携 (0.5h)
- PaintLayer とオニオンスキンの統合

**合計工数:** ~25h

---

## 5. 依存関係

- `MILESTONE_PAINT_LAYER_RASTER_EDITING` (Phase 1 の前提)
- `MILESTONE_PEN_TOUCH_JOYSTICK` (Phase 2 の前提)
- オニオンスキン API (Phase 4 で利用)

---

## 6. ガードレール

- `QImage` を本流ストレージに使わない（`ImageF32x4_RGBA` を使用）
- 新規 W_OBJECT 派生は PaintLayer のみ
- BrushTool は既存 ToolType enum に追加
- ブラシカーソルは `Overlay.Composition` に描画
