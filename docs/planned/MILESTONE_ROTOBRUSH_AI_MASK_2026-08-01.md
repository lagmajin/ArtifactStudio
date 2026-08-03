# Rotobrush級 AIマスク 実装マイルストーン

**日付**: 2026-08-01
**ベース**: AE Rotobrush 3.0 / RunwayML / SAM (Segment Anything Model)
**現状**: `OnnxDmlLocalAgent`（ONNX Runtime + DirectML）稼働中。`OpenCVRotoBrushEngine` 一部実装あり。AI基盤は整っている。
**狙い**: ONNX Runtime で SAM 等のセグメンテーションモデルを推論し、ブラシストローク1回でオブジェクトを自動マスクする

---

## Rotobrush とは

AEの Rotobrush 3.0（2024以降）は以下のパイプライン:
1. ユーザーが前景/背景をブラシで塗る
2. AIがピクセル単位の前景/背景セグメンテーションを実行
3. 時間方向にマスクを伝播（オプティカルフロー or フレーム間 attention）
4. 結果をマスクパス（ベジェ）に変換可能

---

## Phase 1: SAM 統合による静止画セグメンテーション

### Step 1.1 — SAM ONNX モデルローダー
**新規**: `ArtifactCore/include/AI/Segmentation/SAMEngine.ixx`

```cpp
export module Core.AI.Segmentation.SAMEngine;

export namespace ArtifactCore::AI::Segmentation {

struct SAMPoint {
    float x, y;   // 画像正規化座標 (0-1)
    int label;    // 1 = foreground, 0 = background
};

struct SAMBox {
    float x1, y1, x2, y2; // 正規化座標
};

struct SAMResult {
    int width, height;
    std::vector<float> logits;   // width*height*3 (low-res)
    std::vector<float> masks;    // width*height (binary)
    float iouScore = 0.0f;
    float stabilityScore = 0.0f;
};

class SAMEngine {
public:
    SAMEngine();
    ~SAMEngine();

    // モデル読込
    bool loadImageEncoder(const QString& onnxPath);
    bool loadMaskDecoder(const QString& onnxPath);

    // 画像エンコード（重い処理。1回だけ実行しキャッシュ）
    bool encodeImage(const ArtifactCore::ImageF32x4_RGBA& image);

    // セグメンテーション
    SAMResult segment(const std::vector<SAMPoint>& points);
    SAMResult segmentWithBox(const SAMBox& box);
    SAMResult segmentMultiPoint(
        const std::vector<SAMPoint>& foregroundPoints,
        const std::vector<SAMPoint>& backgroundPoints);

    // キャッシュ
    void clearCache();

    // 情報
    bool isImageEncoded() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace
```

### Step 1.2 — SAM 推論の流れ

```
ユーザーが前景クリック → SAMEngine::segment(foregroundPoint)
  ↓
Image Encoder (ViT) で画像を特徴ベクトルに変換（重い。1回目のみ）
  ↓
Prompt Encoder でクリック位置/ボックスをエンコード
  ↓
Mask Decoder でマスク予測 (256×256 low-res → bilinear upsample)
  ↓
Sigmoid で 0-1 float マスク出力
  ↓ 
IoU Score / Stability Score 計算
```

### Step 1.3 — VP 統合
**変更**: `ArtifactCompositionRenderController.cppm`

```
Rotobrush ツール:
- ブラシストロークで前景（緑） or 背景（赤）を塗る
- リアルタイムで SAM 推論 → マスクをVPオーバーレイ表示
- SAM 結果を LayerMask::addMaskPath() に変換
```

---

## Phase 2: 時間伝播（Frame-to-Frame Propagation）

### Step 2.1 — オプティカルフロー伝播
既存の `MotionTracker` + `TrackingMethod::OpticalFlow` を使用。

```cpp
class MaskPropagator {
public:
    // 前フレームのマスクを現在フレームにオプティカルフローで伝播
    DeepImage propagateMask(
        const ImageF32x4_RGBA& prevFrame,
        const ImageF32x4_RGBA& currentFrame,
        const DeepImage& prevMask  // 前フレームのアルファマスク
    );
};
```

### Step 2.2 — 伝播マスクのリファイン
伝播後にSAMで再セグメンテーション（プロンプト = 前フレームのマスク）:

```cpp
SAMResult refineMask(
    const DeepImage& propagatedMask,
    float confidenceThreshold = 0.5f
);
```

---

## Phase 3: マスク → ベジェパス変換

### Step 3.1 — 輪郭抽出 + ベジェ化

```cpp
class MaskToPathConverter {
public:
    // 連続アルファマスク → ベジェパスのリスト
    std::vector<MaskPath> convert(
        const std::vector<float>& alphaMask,
        int width, int height,
        const MaskConversionParams& params
    );
};

struct MaskConversionParams {
    float simplificationTolerance = 1.5f;  // 頂点削減（RDP法）
    float cornerThreshold = 10.0f;        // コーナー検出角度
    int minPathVertices = 6;              // 最小頂点数
    bool closedPath = true;
    bool autoKeyframes = true;
};
```

**アルゴリズム**:
1. OpenCV `findContours()` で輪郭抽出
2. Ramer-Douglas-Peucker で頂点削減
3. コーナー検出 → アンカーポイント配置
4. 残りの頂点をベジェ近似（最小二乗法）
5. `MaskVertex { position, inTangent, outTangent }` のリスト出力
6. `MaskPath::addVertex()` で逐次追加

---

## Phase 4: 最適化

### Step 4.1 — バッチ推論
全フレームの画像エンコーディングを先行実行し、マスクデコードだけフレームごとに実行。

### Step 4.2 — 低解像度推論 + アップスケール
240p でSAM推論 → bilinear + guided filter でフル解像度にアップスケール。
推論時間: 1920×1080 → 480×270 で約16倍高速化。

### Step 4.3 — キーフレームのみSAM推論
全フレームをSAM推論せず、キーフレーム（10フレームおき）のみSAM。
中間フレームはオプティカルフロー伝播 + クロスフェード。

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `ArtifactCore/include/AI/Segmentation/SAMEngine.ixx` | 新規 | SAM エンジン インターフェース |
| P1 | `ArtifactCore/src/AI/Segmentation/SAMEngine.cppm` | 新規 | ONNX 推論実装 |
| P1 | `ArtifactCore/include/AI/Segmentation/MaskRefiner.ixx` | 新規 | マスク精錬（CRF/guided filter） |
| P1 | `ArtifactCore/src/AI/Segmentation/MaskRefiner.cppm` | 新規 | 実装 |
| P2 | `ArtifactCore/include/AI/Segmentation/MaskPropagator.ixx` | 新規 | 時間伝播 |
| P2 | `ArtifactCore/src/AI/Segmentation/MaskPropagator.cppm` | 新規 | 実装 |
| P3 | `ArtifactCore/include/AI/Segmentation/MaskToPathConverter.ixx` | 新規 | マスク→ベジェ変換 |
| P3 | `ArtifactCore/src/AI/Segmentation/MaskToPathConverter.cppm` | 新規 | 実装 |
| P3 | `ArtifactCompositionRenderController.cppm` | 変更 | Rotobrush ツール + VP オーバーレイ |
| P4 | `ArtifactCore/src/AI/Segmentation/SAMEngineOptimizer.cppm` | 新規 | バッチ/低解像度最適化 |

---

## 検証チェックリスト

- [ ] SAM モデル（ViT-B）が ONNX 変換され、`loadImageEncoder/loadMaskDecoder` で読み込める
- [ ] 前景クリック1回で妥当なセグメンテーションマスクが得られる
- [ ] 前景/背景の複数クリックでマスクが改善される（multi-point prompt）
- [ ] バウンディングボックス指定で動作する
- [ ] 画像エンコードがキャッシュされ2回目以降は高速
- [ ] マスクが `MaskPath` に変換され、ペンツールで編集可能
- [ ] フレーム間伝播でマスクが追従する（単純移動シーン）
- [ ] 低解像度推論 + upsample で品質劣化が許容範囲内（PSNR > 35dB）
- [ ] VP上でRotobrushプレビューがリアルタイム（>10fps）に表示される
