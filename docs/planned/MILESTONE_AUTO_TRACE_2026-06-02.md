# M-MOTION-6 Auto-Trace / Vectorize Raster (2026-06-02)

日付：2026-06-02
最終更新：2026-08-15
目標：ラスタ画像をベクターパスに自動トレースする機能。ロゴのベクター化、イラストのパス抽出に使用する。

## 2026-08-15 現行コード監査

`ShapePath`／`ShapeLayer`、MaskPath、OpenCV の画像処理基盤は存在するが、文書で想定する `ArtifactCore/src/Trace/AutoTrace.cppm`、`cv::findContours()`／`approxPolyDP()` を使う Auto-Trace エンジン、チャンネル／しきい値 UI、トレース結果の Shape／Mask 出力導線は確認できなかった。`KeyframePatternGenerator` の trajectory → keyframe 変換や診断用 `TraceRecorder` は、画像輪郭の Auto-Trace とは別機能である。

判定: **Phase 1〜3 は未着手。既存の Shape／Mask／OpenCV 基盤を利用できるが、輪郭抽出・Bezier 近似・UI・Undo 統合が未実装。**

---

## Goal

- 画像レイヤーを選択 → Auto-Trace 実行 → シェイプレイヤーとしてパス生成
- After Effects の「自動トレース」と同程度の品質
- マスクにも出力可能（既存 MaskPath 形式）

---

## Definition of Done

- [ ] **トレース実行** - 選択レイヤーのアルファチャンネル/輝度/RGBチャンネルから輪郭パスを抽出
- [ ] **チャンネル選択UI** - Alpha / Luminance / Red / Green / Blue から選択
- [ ] **しきい値調整** - 0〜255 のしきい値スライダ。プレビュー即時反映
- [ ] **許容値・最小領域** - 近似許容値と最小領域ピクセル数
- [ ] **出力先選択** - 新規シェイプレイヤー / 選択レイヤーのマスク
- [ ] **ベジェ最適化** - 抽出パスをベジェ曲線に近似 (Ramer-Douglas-Peucker + Curve fitting)
- [ ] **OpenCV 活用** - `cv::findContours()` + `cv::approxPolyDP()` をベース

---

## Implementation Phases

### Phase 1: 輪郭抽出エンジン

**ファイル**: `ArtifactCore/src/Trace/AutoTrace.cppm` (新規)

**完了条件**:
- [ ] OpenCV の `cv::findContours()` で輪郭抽出
- [ ] チャンネル選択と2値化（しきい値処理）
- [ ] 階層的輪郭（穴）対応
- [ ] 最小領域フィルタ

```cpp
class AutoTraceEngine {
public:
    struct Config {
        enum Channel { Alpha, Luminance, Red, Green, Blue };
        Channel channel = Alpha;
        int threshold = 128;
        float tolerance = 1.0f;    // approxPolyDP 許容値
        int minArea = 10;          // 最小領域ピクセル数
        bool invert = false;
    };
    
    struct ContourPath {
        std::vector<QPointF> points;  // 生輪郭点列
        bool isHole;                  // 穴かどうか
    };
    
    struct TraceResult {
        std::vector<ContourPath> contours;
        int width, height;
    };
    
    TraceResult trace(const ImageF32x4RGBA& image, const Config& cfg);
};
```

### Phase 2: ベジェ曲線近似

**完了条件**:
- [ ] Ramer-Douglas-Peucker アルゴリズムで点列削減
- [ ] 削減後の点列をベジェ曲線でフィッティング（最小二乗法）
- [ ] `ShapePath` 形式への変換

### Phase 3: UI

**ファイル**: `Artifact/src/Widgets/Trace/ArtifactAutoTraceDialog.cppm` (新規)

**完了条件**:
- [ ] レイヤー右クリック → 「自動トレース」メニュー
- [ ] ダイアログ: チャンネル選択・しきい値・許容値・最小領域
- [ ] プレビュー（元画像 + トレース結果のオーバーレイ）
- [ ] 出力先選択（新規シェイプレイヤー / マスク）
- [ ] 実行後はシェイプレイヤーが作成され、タイムラインに表示

---

## Dependencies

- OpenCV (`cv::findContours`, `cv::approxPolyDP`, `cv::threshold`)
- ShapePath / ShapeLayer (シェイプ出力先)
- MaskPath (マスク出力先)

---

## Total Estimate

| Phase | 時間 |
|---|---|
| Phase 1: 輪郭抽出エンジン | 4-6h |
| Phase 2: ベジェ曲線近似 | 4-6h |
| Phase 3: UI | 4-6h |
| **合計** | **12-18h** |
