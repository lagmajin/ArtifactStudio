# Session Progress: 2D Tracker + Path Text (2026-06-08)

## Path Text — ほぼ完了

### 完了済み
- ✅ `ArtifactCore/src/Text/GlyphLayout.cppm` — `layoutOnPath()` 実装完了
- ✅ `ArtifactCore/include/Text/GlyphLayout.ixx` — `layoutOnPath()` 宣言済み
- ✅ `ArtifactCore/include/Shape/ShapeGroup.ixx` — `addOperator(ShapeOperatorType)` 追加済み
- ✅ `ArtifactCore/include/Text/TextStyle.ixx` — `ParagraphStyle::PathBinding` 構造体済み
- ✅ `Artifact/include/Layer/ArtifactShapeLayer.ixx` — shape operator API 追加済み
- ✅ `Artifact/src/Layer/ArtifactShapeLayer.cppm` — shape operator 統合完了 (buildProcessedPainterPaths, localBounds対応, JSON serialize)
- ✅ `Artifact/include/Layer/ArtifactTextLayer.ixx` — **変更残っている**
  - `import Shape.Types` 追加済み
  - Path text API 宣言済み (`setPathSegments`, `pathStartOffset`, `pathEndOffset`, `pathAlignToPath`, `pathReverse`)
  - `TextLayoutMode::Path` 追加済み
- ✅ `Artifact/src/Layer/ArtifactTextLayer.cppm` — `applyColorToSelectorRange()` 実装済み

### 未完了 (やること)
- ❌ **ArtifactTextLayer::Impl に pathSegments_ メンバ追加** — 前回セッションで編集に失敗した部分。
  - `perGlyphMode_` の直後に以下を挿入する:
    ```cpp
    // Path text data
    std::vector<ArtifactCore::BezierSegment> pathSegments_;
    bool pathAlignToPath_ = true;
    bool pathReverse_ = false;
    double pathStartOffset_ = 0.0;
    double pathEndOffset_ = 0.0;
    ```
  - CacheKey に `pathSegments` フィールド追加
  - `lastCacheKey_->pathSegments == o.pathSegments` 比較も追加
- ❌ **AxrtifactTextLayer の Path API 実装** — `setPathSegments`, `pathSegments`, `setPathStartOffset`, `pathStartOffset`, `setPathEndOffset`, `pathEndOffset`, `setPathAlignToPath`, `pathAlignToPath`, `setPathReverse`, `pathReverse` の全メソッド実装
- ❌ **updateImage() に Path mode 分岐** — `layoutMode_ == TextLayoutMode::Path` の場合、`TextLayoutEngine::layoutOnPath()` を呼び出してグリフ配置
- ❌ **toJson/fromJson に pathSegments のシリアライズ追加**

## 2D Tracker — Core 実装完了、UI 未着手

### 完了済み
- ✅ `ArtifactCore/include/Tracking/MotionTracker.ixx` — 全API宣言完了
- ✅ `ArtifactCore/src/Tracking/MotionTracker.cppm` — 全実装完了 (1151行)
  - TrackPoint, TrackFrame, TrackResult
  - MotionTracker (全メソッド)
  - TrackerManager (Singleton)
  - OpticalFlow 名前空間関数
  - JSON シリアライズ/デシリアライズ
  - ECC ベースの Planar トラッキング
  - 統計・解析・補正機能 (smoothTrack, removeOutliers, filterByConfidence)
- ✅ `ArtifactCore/src/Tracking/CameraTracker.cppm` — 3D Camera Tracker 実装

### 未完了
- ❌ **Tracker UI** — 新規ファイル `Artifact/src/Widgets/Tracking/ArtifactTrackerUI.cppm` (CompositionEditor 上の Inner Box / Outer Box オーバーレイ)
- ❌ **Phase 3: 結果適用** — トラック結果 → 位置キーフレーム/Nullレイヤー書き出し
- ❌ **Phase 4: 複数点トラッキング** — 2〜4点同時、射影変換

## その他変更済みファイル
- `ArtifactPr/src/ArtifactPrEditorEngine.cppm` — saveProject/loadProject に validateProjectHealth 追加
- `docs/planned/MILESTONES_BACKLOG.md` — 各種マイルストーン追加
- `docs/planned/NEXT_PHASE_ROADMAP.md` — ロードマップ更新
- `Artifact` — 128ファイル変更 (+6574/-3644 lines)
- `ArtifactCore` — 39ファイル変更 (+1659/-673 lines)
