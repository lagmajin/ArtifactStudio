# Deepコンポジット 実装マイルストーン

**日付**: 2026-08-01
**ベース**: Nuke Deep Compositing / Foundry Deep EXR 仕様
**現状**: `DepthMask`（深度マスク計算）と `OpenExr`（空スタブ）が存在。本格的な Deep コンポジットは不在。

---

## Deepコンポジットとは

通常の画像がピクセルあたり {R, G, B, A} の4値を持つ「Flat 画像」であるのに対し、
**Deep 画像**はピクセルあたり「奥行き方向の複数のサンプル」を持つ。

```
Flat Pixel:   {R, G, B, A}
Deep Pixel:   [ {R, G, B, A, Z, ZBack} × Nサンプル ]
              ↑ 手前のオブジェクトから順にN個の色+深度サンプル
```

**メリット**: 
- 被写界深度（DoF）が出しやすい
- ボリュームとの合成が簡単
- 深度によるオブジェクト前後関係の自然な遮蔽
- EXR 2.0 Deep でファイル保存

---

## Phase 1: Deep Pixel データ構造 + 基本演算

### Step 1.1 — DeepPixel 型定義
**新規**: `ArtifactCore/include/Image/Deep/DeepPixel.ixx`

```cpp
export module Core.Image.Deep.Pixel;

export namespace ArtifactCore::Image::Deep {

struct DeepSample {
    float r, g, b, a;   // RGBA（premultiplied推奨）
    float z;             // 深度（カメラからの距離）
    float zBack;         // 背面深度（ボリュームサンプルの厚み）
};

class DeepPixel {
public:
    DeepPixel() = default;

    // サンプル管理
    void addSample(const DeepSample& s);
    void addSample(const FloatRGBA& color, float z, float zBack = 0.0f);
    void clear();
    int sampleCount() const;
    const DeepSample& sample(int i) const;
    DeepSample& sample(int i);
    void removeSample(int i);

    // Zソート（手前→奥）
    void sort();

    // Z範囲でフィルタ（near～farの範囲外サンプルを削除）
    void clipZ(float nearZ, float farZ);

    // マージ（2つのDeepPixelを合成）
    void merge(const DeepPixel& other);

    // 情報
    bool isFlat() const { return sampleCount() <= 1; }
    float minZ() const;
    float maxZ() const;

private:
    std::vector<DeepSample> samples_;
};

} // namespace ArtifactCore::Image::Deep
```

### Step 1.2 — DeepImage 型
**新規**: `ArtifactCore/include/Image/Deep/DeepImage.ixx`

```cpp
export module Core.Image.Deep.Image;

export namespace ArtifactCore::Image::Deep {

class DeepImage {
public:
    DeepImage();
    DeepImage(int width, int height);
    ~DeepImage();

    int width() const;
    int height() const;
    void resize(int w, int h);

    DeepPixel& pixel(int x, int y);
    const DeepPixel& pixel(int x, int y) const;
    DeepPixel* data() const;

    // Flat 画像へフラット化（Z=一定位置で平面描画する場合）
    void flatten(ImageF32x4_RGBA& out) const;

    // Flat画像 → Deep画像（全ピクセル単一サンプルとして変換）
    void fromFlat(const ImageF32x4_RGBA& flat, float z = 0.0f);
    void fromFlatWithDepth(const ImageF32x4_RGBA& rgba,
                            const ImageF32x4_RGBA& depth);

    // ボリュームとの合成
    void compositeVolume(const ImageF32x4_RGBA& volumeRGBA,
                          float volumeZ, float volumeThickness);

    // 情報
    size_t totalSampleCount() const;
    float memoryMB() const;

private:
    int width_ = 0, height_ = 0;
    deep_unique_ptr<DeepPixel[]> pixels_; // PIMPL for large allocation
};

} // namespace
```

### Step 1.3 — Deep 合成エンジン
**新規**: `ArtifactCore/include/Image/Deep/DeepCompositor.ixx`

```cpp
export module Core.Image.Deep.Compositor;

export namespace ArtifactCore::Image::Deep {

enum class DeepBlendMode { Over, Under, Add, Max };

class DeepCompositor {
public:
    // Deep over Deep 合成（手前→奥の自然な遮蔽を維持）
    static void compositeOver(DeepImage& dst, const DeepImage& src);

    // Deep over Flat（奥行き情報を使ってFlat画像を合成）
    static void compositeFlatOver(DeepImage& dst,
                                   const ImageF32x4_RGBA& srcFlat,
                                   float srcZ);
    static void compositeFlatUnder(DeepImage& dst,
                                    const ImageF32x4_RGBA& srcFlat,
                                    float srcZ);

    // Flat over Deep（Deep 背景の上に Flat 前景を合成）
    static ImageF32x4_RGBA compositeDeepUnderFlat(
        const DeepImage& srcDeep,
        const ImageF32x4_RGBA& foreground);

    // ホールドアウト（Deep画像の特定深度範囲をマスク）
    static void holdout(DeepImage& image, float nearZ, float farZ);

    // 深度マット（DepthMaskCalculator と同等のことを Deep で実行）
    static ImageF32x4_RGBA depthMatte(const DeepImage& image,
                                       float nearZ, float farZ);

    // Deep 被写界深度（DoF） — Z値に応じてブラーを変える
    static DeepImage depthOfField(const DeepImage& image,
                                   float focalZ, float fStop);

    // 全Deepサンプルをアルファでノーマライズ
    static void normalizeAlpha(DeepImage& image);
};

} // namespace
```

---

## Phase 2: GPU 高速化

### Step 2.1 — Deep Pixel を GPU バッファに格納
- DeepImage は可変長サンプルを持つため、通常の2Dテクスチャに格納できない
- GPU上では「フラットなサンプル配列 + ピクセルごとのオフセットテーブル」で表現

```cpp
struct DeepImageGPU {
    // 全サンプルをフラットに並べた StructuredBuffer
    std::vector<DeepSample> flatSamples;
    // ピクセル(x,y) が flatSamples のどこから何サンプルかを示すテーブル
    std::vector<uint32_t> sampleOffsets;  // [y*width+x] = offset
    std::vector<uint16_t> sampleCounts;   // [y*width+x] = count
    int totalSamples;
    int width, height;
};
```

### Step 2.2 — HLSL コンピュートシェーダー
**新規**: `shaders/DeepComposite.hlsl`

```
// Deep over Deep をGPUで実行
[numthreads(8, 8, 1)]
void CSDeepCompositeOver(uint3 dtid : SV_DispatchThreadID) {
    int idx = dtid.y * width + dtid.x;
    // src と dst のサンプル配列をマージ（Zソート維持）
    // アルファ乗算。不透明になったらそれより奥はカット
}
```

---

## Phase 3: EXR 2.0 Deep 入出力

### Step 3.1 — Deep EXR 書き込み
既存の `OpenExr` クラスを拡張（現状は空の PIMPL のみ）。

**変更**: `ArtifactCore/include/Image/OpenEXR.ixx`

```cpp
class OpenExr {
public:
    // ... existing Flat EXR methods ...

    // Deep EXR
    bool writeDeep(const QString& path, const DeepImage& image,
                    const ExrWriteOptions& options = {});
    bool readDeep(const QString& path, DeepImage& outImage);

    // Multi-part EXR（Beauty + Depth + AOV を1ファイルに）
    bool writeMultiPart(const QString& path,
                         const std::map<QString, ImageF32x4_RGBA>& parts);
    bool readMultiPart(const QString& path,
                       std::map<QString, ImageF32x4_RGBA>& outParts);
};

struct ExrWriteOptions {
    bool halfPrecision = true;   // 16bit float で圧縮
    int compression = 4;         // 0=none, 4=piz, 6=b44
    bool tiled = false;
    int tileSize = 64;
};
```

### Step 3.2 — 実装（OpenEXR C++ ライブラリ）
vcpkg に `openexr` が既にあればリンクするだけ。なければ追加が必要。

---

## Phase 4: レンダーパイプライン統合

### Step 4.1 — 3DレイヤーからのDeep出力
既存の `ArtifactIRenderer` はDiligent Engine経由で深度チャンネルをレンダリングできる。この深度バッファを使って DeepImage を構築する。

```
ArtifactIRenderer の既存 AOV:
  Red, Green, Blue, Alpha, Depth, NormalX/Y/Z, VelocityX/Y,
  ObjectId, MaterialId, AlbedoR/G/B, Emission

→ この Depth AOV を使って DeepImage::fromFlatWithDepth() を呼ぶだけで
  Flat → Deep 変換が完了する
```

### Step 4.2 — 描画パイプライン追加
```cpp
// CompositionRenderController::Impl に追加
bool deepCompositeEnabled_ = false;
DeepImage deepAccumBuffer_;

void renderDeepFrame() {
    // 各3Dレイヤーを順にレンダリング
    for (auto& layer : layers) {
        if (!layer->is3D()) continue;
        // Layer → RGBA + Depth にレンダリング
        ImageF32x4_RGBA rgba = renderLayerToRGBA(layer);
        ImageF32x4_RGBA depth = renderLayerToDepth(layer);
        // Deep画像に変換
        DeepImage deepLayer;
        deepLayer.fromFlatWithDepth(rgba, depth);
        // 合成
        DeepCompositor::compositeOver(deepAccumBuffer_, deepLayer);
    }
    // 2DレイヤーをDeepの上に合成
    for (auto& layer : layers) {
        if (layer->is3D()) continue;
        ImageF32x4_RGBA rgba = layerToRGBA(layer);
        DeepCompositor::compositeFlatOver(deepAccumBuffer_, rgba, 0.0f);
    }
    // 最終的なFlat画像に戻す
    ImageF32x4_RGBA finalBeauty;
    deepAccumBuffer_.flatten(finalBeauty);
}
```

---

## Phase 5: Deep ノード（Nuke互換）

以下のノード（エフェクト）を実装:

| ノード | Nuke 相当 | 説明 |
|--------|-----------|------|
| `DeepMerge` | DeepMerge | Deep over Deep 合成 |
| `DeepFromImage` | DeepFromImage | Flat→Deep変換 |
| `DeepToImage` | DeepToImage | Deep→Flatフラット化 |
| `DeepTransform` | DeepTransform | Deepピクセルの空間移動 |
| `DeepRecolor` | DeepRecolor | 深度に応じて色変更 |
| `DeepHoldout` | DeepHoldout | 深度範囲マスク |
| `DeepCrop` | DeepCrop | 深度範囲でのトリミング |

---

## ファイル一覧

| フェーズ | ファイル | 新規/変更 | 内容 |
|---------|----------|----------|------|
| P1 | `ArtifactCore/include/Image/Deep/DeepPixel.ixx` | 新規 | DeepSample + DeepPixel |
| P1 | `ArtifactCore/src/Image/Deep/DeepPixel.cppm` | 新規 | DeepPixel 実装 |
| P1 | `ArtifactCore/include/Image/Deep/DeepImage.ixx` | 新規 | DeepImage（2D配列） |
| P1 | `ArtifactCore/src/Image/Deep/DeepImage.cppm` | 新規 | DeepImage 実装 |
| P1 | `ArtifactCore/include/Image/Deep/DeepCompositor.ixx` | 新規 | 合成エンジン |
| P1 | `ArtifactCore/src/Image/Deep/DeepCompositor.cppm` | 新規 | 合成エンジン実装 |
| P2 | `ArtifactCore/include/Image/Deep/DeepImageGPU.ixx` | 新規 | GPU バッファ |
| P2 | `ArtifactCore/src/Image/Deep/DeepCompositorGPU.cppm` | 新規 | GPU 合成 |
| P2 | `shaders/DeepComposite.hlsl` | 新規 | HLSL シェーダー |
| P3 | `ArtifactCore/include/Image/OpenEXR.ixx` | 変更 | Deep EXR 追加 |
| P3 | `ArtifactCore/src/Image/OpenEXR.cppm` | 変更 | Deep EXR 実装 |
| P4 | `Artifact/src/Render/ArtifactIRenderer.cppm` | 変更 | Deep 出力パス |
| P4 | `ArtifactCompositionRenderController.cppm` | 変更 | Deep 合成統合 |
| P5 | `Artifact/include/Effects/Deep/DeepFromImageEffect.ixx` | 新規 | Flat→Deepノード |
| P5 | `Artifact/src/Effects/Deep/DeepFromImageEffect.cppm` | 新規 | 実装 |

---

## 検証チェックリスト

- [ ] DeepPixel の Zソートが正しい（手前→奥）
- [ ] DeepImage のメモリ効率（100万サンプルで < 50MB）
- [ ] Deep over Deep 合成で3Dオブジェクトの前後関係が正しい
- [ ] Flat over Deep で2Dテキストが3Dオブジェクトの手前に正しく表示
- [ ] DoF シミュレーションで手前/奥が適切にボケる
- [ ] Deep EXR 書き込み + Nuke で読み込み可能
- [ ] Multi-part EXR が正しく出力される
- [ ] Nuke の DeepMerge / DeepFromImage との相互運用性
