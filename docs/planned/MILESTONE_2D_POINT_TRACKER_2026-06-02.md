# M-MOTION-1 2D Point Tracker (2026-06-02)

日付：2026-06-02
ステータス：実装フェーズ文書へ移管（進捗は [`MILESTONE_2D_POINT_TRACKER_2026-06-16.md`](MILESTONE_2D_POINT_TRACKER_2026-06-16.md) を参照）
目標：モーションデザイナーが最も頻繁に使う「画面上の特徴点を追跡し、位置データとして出力する」2Dポイントトラッカーを実装する。

> この文書は初期設計・見積もりとして保持する。実装済み判定や未完了項目の更新は 2026-06-16 版に集約する。

---

## Goal

- ユーザーが指定した追跡点（Tracker Point）を、フレーム間で自動追跡する
- 追跡結果をレイヤーの位置プロパティに適用（またはNullレイヤーに書き出し）
- 選択した複数点を同時に追跡可能（4点＝平面追跡まで想定）

---

## Definition of Done

- [ ] **トラッカー領域UI** - Composition Editor 上に2つの矩形（特徴領域・探索領域）をドラッグ配置可能
- [ ] **追跡実行** - 前方/後方/全フレーム追跡。Normalized Cross-Correlation ベース
- [ ] **結果の適用** - 追跡データをレイヤーの位置(keyframe)またはNullレイヤーに書き出し
- [ ] **手動修正** - ずれたキーフレームを手動で微調整し、再追跡可能
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

**ファイル**: `ArtifactCore/src/Tracking/PointTracker.cppm` (新規)

**完了条件**:
- [ ] `PointTracker` クラス: テンプレート画像 + 探索領域を受け取り、次フレームでの位置を返す
- [ ] NCC (Normalized Cross-Correlation) によるマッチング
- [ ] サブピクセル精度推定 (parabolic fit / cv::minMaxLoc + 周辺重み付け)
- [ ] 前方・後方追跡に対応

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

### Phase 2: トラッカーUI

**ファイル**: `Artifact/src/Widgets/Tracking/ArtifactTrackerUI.cppm` (新規)

**完了条件**:
- [ ] Composition Editor 上にトラッカー領域をオーバーレイ表示
- [ ] Inner Box / Outer Box のドラッグリサイズ・移動
- [ ] 追跡開始フレームの設定UI
- [ ] 「Analyze Forward / Backward / All」ボタン
- [ ] トラック結果プレビュー（フレーム単位で位置確認）

### Phase 3: 結果適用

**完了条件**:
- [ ] 追跡結果 → 位置キーフレームとして書き出し
- [ ] 追跡結果 → Nullレイヤーの位置プロパティとして書き出し
- [ ] アンカーポイントを追跡点に自動設定
- [ ] 手動修正モード: 特定キーフレームの位置をマウスでドラッグ修正 → そこから再追跡

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
