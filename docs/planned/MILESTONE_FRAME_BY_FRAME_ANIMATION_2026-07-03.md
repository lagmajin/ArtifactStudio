# M-FBF-1 Frame-by-Frame Animation Milestone

**作成日:** 2026-07-03
**最終更新:** 2026-08-15
**ステータス:** Paint Layer／Brush／Onion Skin 基盤は実装済み、専用 Timeline 編集と統合検証が未完了
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

## 2026-07-25 現状確認

当初の「未実装」記載から進展しており、`ArtifactPaintLayer` はフレーム単位の `ImageF32x4_RGBA` バッファ、new/remove/duplicate、stroke適用、消しゴム、直近stroke undo、JSONプロパティ保存を持つ。`ArtifactBrushTool` とツールバー／オプションバーのブラシ・消しゴム導線も存在する。Composition Editor にはOnion Skinの有効化、フレーム数、opacity設定と非同期キャプチャ・オーバーレイ描画がある。

未完了または未検証なのは、Paint LayerのTimeline上のフレーム追加／削除／複製／PageUp/PageDownナビゲーション、フレームインジケータ、stroke UndoManagerとの統合、Onion SkinがPaint Layerの前後フレームを正しく参照すること、実機描画性能と保存復元である。判定は「PaintLayer／Brush／Onion Skin基盤は実装済み、専用Timeline編集と統合検証が残る」とする。

## 2026-08-15 現行コード監査

- `ArtifactPaintLayer` はフレーム単位の `ImageF32x4_RGBA`、frame add/remove/duplicate、Brush／Eraser／Clone、stroke undo、JSON 保存／復元を持つ。
- `ArtifactBrushTool` と Composition Editor の brush／eraser／clone／clear 導線、Layer Panel の frame-by-frame capability 表示、Paint Layer onion-skin overlay 経路を確認した。
- ただし、Dope Sheet／Timeline での専用 cel 操作、PageUp／PageDown の専用ナビゲーション、フレームインジケータ、UndoManager の一般経路との統合、前後フレーム参照の runtime parity は未確認。
- paint buffer の表示側に QImage 変換境界が残っており、編集用 float buffer と表示／renderer 経路の性能・保存復元は未検証。ビルド／テストは実行していない。

判定: **Frame-by-frame のデータ／ブラシ／Onion Skin 基盤は実装済み。専用 Timeline workflow、Undo／runtime parity、表示性能検証は pending。**
