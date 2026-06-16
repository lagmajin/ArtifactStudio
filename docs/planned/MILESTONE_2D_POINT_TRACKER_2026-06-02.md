# M-MOTION-1 2D Point Tracker (2026-06-02)

日付：2026-06-02
目標：モーションデザイナーが最も頻繁に使う「画面上の特徴点を追跡し、位置データとして出力する」2Dポイントトラッカーを実装する。

---

## Goal

- ユーザーが指定した追跡点（Tracker Point）を、フレーム間で自動追跡する
- 追跡結果をレイヤーの位置プロパティに適用（またはNullレイヤーに書き出し）
- 選択した複数点を同時に追跡可能（4点＝平面追跡まで想定）

---

## Definition of Done

- [x] **トラッカー領域UI** - Composition Editor 上に2つの矩形（特徴領域・探索領域）をドラッグ配置可能
- [x] **追跡実行** - 前方/後方/全フレーム追跡。Normalized Cross-Correlation ベース
- [x] **結果の適用** - 追跡データをレイヤーの位置(keyframe)またはNullレイヤーに書き出し
- [x] **手動修正** - ずれたキーフレームを手動で微調整し、再追跡可能
- [ ] **複数点追跡** - 2〜4点の同時追跡。4点で平面トラッキングとして利用可能
- [ ] **アンカーポイント自動設定** - 追跡開始フレームでアンカーポイントを追跡点中心に設定

---

## Design Concept

After Effects / Mocha の2Dトラッカーを参考に、シンプルな実装を優先する。

```
Track Point UI:
┌─────────────────────────────────┐
│  Inner Box (特徴領域)            │
│  ┌───────┐                      │
│  │ 追跡点  │                     │
│  └───────┘                      │
│  Outer Box (探索領域)            │
│  ┌───────────────────────┐      │
│  │                       │      │
│  │   Inner Box を含む     │      │
│  │   探索範囲             │      │
│  │                       │      │
│  └───────────────────────┘      │
└─────────────────────────────────┘
```

### データフロー

```
Frame N         Frame N+1        Frame N+2
   │                │                │
   ▼                ▼                ▼
[Template] → [Search Window] → [Search Window]
   │         NCC一致位置      NCC一致位置
   │                │                │
   ▼                ▼                ▼
TrackData[N]   TrackData[N+1]   TrackData[N+2]
   │                │                │
   └────────────────┴────────────────┘
                    ▼
          [Apply to Layer Position]
          [または Null に書き出し]
```

---

## Analysis: 既存アセット

- 3D Camera Tracker (`ArtifactCameraTrackerTool`) は存在するが、2Dポイントトラッキング用ではない
- OpenCV はプロジェクトに組み込み済み → `cv::matchTemplate()` / `cv::calcOpticalFlowPyrLK()` が利用可能
- NCC ベースの実装なら OpenCV だけで完結する

---

## Implementation Phases

### Phase 1: トラッカーコア (OpenCV ベース)

**ファイル**: `ArtifactCore/src/Tracking/MotionTracker.cppm` (既存 MotionTracker に統合 — 新規ファイルなし)
**インターフェース**: `ArtifactCore/include/Tracking/MotionTracker.ixx` (TrackingMethod::NormalizedCrossCorrelation 追加)

**実装方針**: 新規 `PointTracker` クラスではなく、既存の `MotionTracker` に NCC テンプレートマッチングパスを追加。
理由: `MotionTracker` は既にフレームバッファ、TrackResult、前方/後方追跡、export API 等のインフラを備えており、
新規クラスを作ると UI 統合時に二重管理になる。

**完了条件**:
- [x] NCC テンプレートマッチング: `cv::matchTemplate(TM_CCOEFF_NORMED)` によるマッチング (Impl::computeNCCMatch)
- [x] サブピクセル精度推定: 3x3 近傍 parabolic fit による subpixel 位置推定
- [x] 前方・後方追跡に対応: trackForward / trackBackward に NCC パスを統合
- [x] `TrackingMethod::NormalizedCrossCorrelation` enum 追加 (MotionTracker.ixx)

```cpp
class PointTracker {
public:
    struct Config {
        int templateWidth = 32;   // 特徴領域サイズ
        int searchWidth = 64;     // 探索領域サイズ
        bool subPixel = true;
    };
    
    struct TrackPoint {
        float2 position;  // 追跡結果位置
        float confidence; // 0.0〜1.0
        bool success;     // 追跡成功/失敗
    };
    
    // テンプレート設定
    void setTemplate(const ImageF32x4RGBA& frame, float2 center, const Config& cfg);
    
    // 追跡実行
    TrackPoint track(const ImageF32x4RGBA& nextFrame);
    
    // 複数点同時追跡
    std::vector<TrackPoint> trackMultiple(
        const ImageF32x4RGBA& nextFrame, 
        int count);
};
```

### Phase 2: トラッカーUI (Diligent ネイティブ)

**ファイル**:
- `Artifact/include/Widgets/Render/ArtifactPointTrackerGizmo.ixx` (新規 — Gizmo クラス宣言)
- `Artifact/src/Widgets/Render/ArtifactPointTrackerGizmo.cppm` (新規 — 描画 + マウス操作)
- `Artifact/include/Tool/ArtifactToolManager.ixx` (ToolType::PointTracker 追加)
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderOverlay.ixx` (drawTrackerPointOverlay 宣言追加)
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` (drawTrackerPointOverlay 実装)
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (描画フック + マウス分岐追加)

**実装方針**: Diligent/D3D12 の `ArtifactIRenderer` を使用してネイティブ描画。Qt QPainter や QImage は不使用。
TransformGizmo パターンに倣い、ToolType::PointTracker の early-return ブロックでマウスを捕捉。

**完了条件**:
- [x] Composition Editor 上にトラッカー領域をオーバーレイ表示 (ArtifactPointTrackerGizmo::draw — Diligent ネイティブ)
- [x] Inner Box (黄色実線) / Outer Box (水色破線) の描画 + 中心赤十字
- [x] Inner Box のドラッグリサイズ・移動 (角・辺・内部のヒットテスト)
- [x] CompositionRenderController への描画フック統合 (renderOneFrameImpl 内)
- [x] マウス Press/Move/Release の PointTracker 分岐追加
- [x] モーションパス軌跡描画 (drawPolyline + dot — tracker 参照同期あり)
- [x] 「Analyze Forward / Backward / All」メニューアクション (LayerMenu)
- [x] トラッキング結果プレビュー（軌跡描画でフレーム単位で位置確認）
- [x] 手動修正: PathPoint ヒットテスト + ドラッグ補正 (applyCorrection 即時適用)

### Phase 3: 結果適用

**ファイル**: `Artifact/src/Tool/ArtifactPointTrackerTool.cppm` (新規)
**パターン**: ArtifactCameraTrackerTool に倣った静的メソッドクラス。

**完了条件**:
- [x] 追跡結果 → 位置キーフレームとして書き出し (AnimatableTransform3D::setPosition)
- [x] 追跡結果 → Nullレイヤーの位置プロパティとして書き出し (ArtifactLayerFactory::createNewLayer + appendLayerTop)
- [x] アンカーポイントを追跡点に自動設定 (初回フレームで setAnchor)
- [x] 全トラッキングポイントを個別 Null レイヤーに一括書き出し (applyAllTrackingPoints)
- [x] 手動修正モード: PathPoint ヒットテスト + ドラッグで applyCorrection を即時適用

### Phase 4: 複数点・平面トラッキング

**完了条件**:
- [ ] 2〜4点の同時トラッカー
- [ ] 4点の位置から射影変換行列を計算 (`cv::getPerspectiveTransform`)
- [ ] 平面トラッキング用のコーナーピンUI
- [ ] コーナーピンエフェクトへの結果書き出し

---

## Dependencies

- OpenCV (`cv::matchTemplate`, `cv::calcOpticalFlowPyrLK`, `cv::getPerspectiveTransform`)
- ArtifactCompositionEditor (オーバーレイ描画)
- ArtifactAbstractLayer (位置プロパティ書き出し)

---

## Total Estimate

| Phase | 時間 |
|---|---|
| Phase 1: トラッカーコア | 6-10h |
| Phase 2: トラッカーUI | 8-12h |
| Phase 3: 結果適用 | 4-6h |
| Phase 4: 複数点・平面 | 6-10h |
| **合計** | **24-38h** |