# MILESTONE: Distort & Warp Effects Completion

**日付**: 2026-08-04
**最終更新**: 2026-08-15
**現状**: 4効果がCPU+GPU実装済み（Liquify, Spherize, Wave, LensDistortion）。TurbulentDisplace GPUスタブ、TwistTransform/BendTransform 実装なし、Coreの DisplacementFunc 6種が未ブリッジ、CornerPin 旧パターン。AE互換の Bulge/Twirl/MeshWar/WaveWarp/Magnify/Ripple/PolarCoordinates 不在。2026-08-13 に `ArtifactCore::morphImages()` の CPU reference 基盤（対応制御点、進行率、bilinear/nearest、クロスディゾルブ）を追加。
**目標**: 全既存スタブの完成、Core DisplacementFunc→Composition ブリッジ、主要AE distortの実装、統一GPUヘルパー。

## 現行コード監査 (2026-08-15)

`ImageProcessing.Distortion` には Pinch/Bulge、Spherize、Twirl、Wave、Turbulent Displace 等の CPU displacement mapper と `morphImages()` があり、既存の Liquify／Spherize／Wave／Lens Distortion の CPU/GPU 実装も確認できる。`DisplacementMap`、`Kaleidoscope`、`OpticsCompensation`、Corner Pin、Drop Shadow 等は個別の effect／core 経路がある。

一方、Core の mapper 全種が Composition の通常 effect path に統一接続されている証拠、Turbulent Displace の専用GPU compute、Twist/Bend、主要AE distort の網羅、GPU/CPU parity、Image Morph のGPU／画像選択UI、実機受入は確認できない。現状は「既存 effect の部分実装＋CPU reference 基盤」であり、完了判定にはしない。

**進捗 (2026-08-13):** `ArtifactCore::morphImages()` のCPU referenceに加え、`Image Morph` エフェクトを追加。ターゲット画像パス、amount、対応制御点JSONを通常のエフェクトプロパティとして扱い、EffectService／Inspectorカタログから生成可能にした。GPUパス、画像選択UI、runtime受入確認は未完了。

## 現状マトリクス

| 効果 | CPU | GPU | パターン | 完成度 |
|------|-----|-----|---------|--------|
| LiquifyEffect | ✅ Parallel::For | ✅ HLSL (Pushのみ) | PIMPL+DualImpl | 85% |
| SpherizeEffect | ✅ | ✅ HLSL | PIMPL+DualImpl | 75% |
| WaveEffect | ✅ | ✅ HLSL | PIMPL+DualImpl | 65% |
| LensDistortionEffect | ✅ | ✅ HLSL | PIMPL+DualImpl | 75% |
| TurbulentDisplaceEffect | ✅ | ❌ applyCPU()委譲 | PIMPL+DualImpl | 55% |
| DisplacementMapEffect | ✅ | ❌ | 単一Impl | 45% |
| KaleidoscopeEffect | ✅ | ✅ 宣言あり | PIMPL | 70% |
| OpticsCompensationEffect | ✅ Core委譲 | ❌ | Direct apply() | 30% |
| ArtifactCornerPinEffect | ✅ cv::warpPerspective | ❌ | Direct apply() | 50% |
| TwistTransform | ❌ | ❌ | Header-only stub | 10% |
| BendTransform | ❌ | ❌ | Header-only stub | 10% |

---

## Phase 1: 既存スタブの完成

### 1.1 TurbulentDisplace GPU パス

**現状** (`Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm:207`):
```cpp
// GPUImpl::applyGPU が applyCPU を呼ぶだけのスタブ
void TurbulentDisplaceEffectGPUImpl::applyGPU(...) {
    cpuImpl_->applyCPU(input, output, roi, ctx);
}
```

**実装**: 2パス compute shader — ノイズ生成→バイリニアリマップ:

```hlsl
// TurbulentDisplaceCS.hlsl
RWTexture2D<float4> outputTexture : register(u0);
Texture2D<float4> inputTexture  : register(t0);

cbuffer Params : register(b0) {
    float amount;
    float size;
    int octaves;
    uint seed;
    float domainWarp;
    float2 invResolution;
}

float hash21(float2 p) {
    float2 q = frac(p * float2(127.1, 311.7));
    q += dot(q, q + 38.546);
    return frac(q.x * q.y);
}

float noise(float2 p) {
    // 簡易Perlin (octaves ループ)
    float value = 0;
    float amp = 1.0;
    float freq = 1.0;
    for (int i = 0; i < min(octaves, 8); i++) {
        float2 i0 = floor(p * freq);
        float2 f = frac(p * freq);
        f = f * f * (3.0 - 2.0 * f); // smoothstep
        value += lerp(
            lerp(hash21(i0), hash21(i0 + float2(1,0)), f.x),
            lerp(hash21(i0 + float2(0,1)), hash21(i0 + float2(1,1)), f.x),
            f.y
        ) * amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    return value;
}

[numthreads(16, 16, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    float2 uv = (float2(tid.xy) + 0.5) * invResolution;
    
    // Domain warp
    float2 warpUV = uv + domainWarp * noise(uv * size + 7.31) * 0.5;
    float2 displace = float2(
        noise(warpUV * size + 13.37 + seed * 127),
        noise(warpUV * size + 37.13 + seed * 257)
    ) * amount * invResolution;
    
    float2 sampleUV = uv + displace;
    outputTexture[tid.xy] = inputTexture.SampleLevel(pointSampler, sampleUV, 0);
}
```

GPU パスの配線（既存の dual-backend パターン準拠）:
1. `TurbulentDisplaceEffectGPUImpl::applyGPU()` のスタブを HLSL ディスパッチに置換
2. テクスチャアップロード、PSOビルド、ディスパッチ、リードバックは既存 `runCreativeCompute()` パターンを一般化した `runDistortCompute()` ヘルパーで共通化

### 1.2 DisplacementMap GPU パス

既存の `DisplacementMapEffect::applyCPU()` のバイリニアサンプリングロジックを HLSL 化。パラメータ:
- `maxHorizontalDisplacement`, `maxVerticalDisplacement`
- `horizontalChannel` (R/G/B/A/Luminance)
- `verticalChannel`
- `wrapAround: bool`

### 1.3 OpticsCompensation GPU パス

`ArtifactCore::makeOpticsCompensation()` (Coreの DisplacementFunc) はCPU側にある。GPU パスは HLSL で逆投影変換を直接実装:

```hlsl
// レンズ歪みの逆変換
float2 lensDistort(float2 uv, float2 center, float fov, float direction) {
    float2 delta = uv - center;
    float r = length(delta);
    float theta = atan(r * tan(fov * 0.00872665)); // deg→rad
    float rPrime = (direction > 0) ? theta / r : r / theta;
    return center + delta * rPrime;
}
```

### 1.4 完了条件

- [ ] TurbulentDisplace の GPU パスが CPU パスと画質一致（ランダムシード固定でピクセル比較）
- [ ] DisplacementMap の GPU パスが全4チャンネルモードで動作
- [ ] OpticsCompensation の GPU パスが fov=10°〜180° で正しい逆歪みを生成
- [ ] 3効果すべてが `supportsGPU() == true` を返す
- [ ] GPU/CPU フォールバックが正常に動作（GPU 失敗時自動CPU）

---

## Phase 2: TwistTransform + BendTransform 実装

### 2.1 TwistTransform

`Artifact/include/Effects/Transform/TwistTransform.ixx` (82行、純粋ヘッダ):
- `angle` プロパティと `ArtifactAbstractField` 参照は宣言済み
- `applyCPU()` が存在しない

実装:

```cpp
// TwistTransform.cppm
void TwistTransform::applyCPU(ImageF32x4_RGBA& input, ImageF32x4_RGBA& output,
                               const QRect& roi, EffectContext& ctx) {
    float cx = roi.width() * 0.5f;
    float cy = roi.height() * 0.5f;
    float angle = angle_.evaluate(ctx.frame, ctx.time);
    float maxR = std::sqrt(cx * cx + cy * cy);
    
    ArtifactCore::Parallel::For(roi.y(), roi.y() + roi.height(), [&](int y) {
        for (int x = roi.x(); x < roi.x() + roi.width(); ++x) {
            float dx = x - cx;
            float dy = y - cy;
            float r = std::sqrt(dx * dx + dy * dy);
            float factor = 1.0f - r / maxR;  // 中心で最大、端で0
            float theta = angle * factor * (3.14159265f / 180.0f);
            
            float srcX = cx + dx * std::cos(theta) - dy * std::sin(theta);
            float srcY = cy + dx * std::sin(theta) + dy * std::cos(theta);
            
            output.sampleBilinear(input, srcX, srcY, x, y);
        }
    });
}
```

### 2.2 BendTransform

`angle`, `direction` (H/V), `size` プロパティ。同様に CPU+GPU 実装。

### 2.3 完了条件

- [ ] TwistTransform が角度 45°/90°/180° の3点で正しい回転歪みを生成
- [ ] BendTransform が水平・垂直方向で正しい湾曲を生成
- [ ] 両効果が `ArtifactEffectService::availableEffects()` に登録される
- [ ] GPU パスも実装（Phase 1 の共通 `runDistortCompute()` 使用）

---

## Phase 3: Core DisplacementFunc → Composition ブリッジ

### 3.1 ブリッジすべき Core 関数

`ArtifactCore/include/ImageProcessing/Distortion.ixx` にある6つの DisplacementFunc:

| Core 関数 | Composition 化 | AE 相当 |
|-----------|---------------|---------|
| `makePinchBulge()` | PinchBulgeEffect | Bulge + Pinch |
| `makeTwirl()` | TwirlEffect | Twirl |
| `makeBilinearWarp()` | BilinearWarpEffect | Bezier Warp (4点) |
| `makeOffset()` | OffsetEffect | Offset |
| `makeScale()` | ScaleEffect | Transform (distort) |
| `makeNoiseDisplace()` | NoiseDisplacementEffect | Noise Displacement |

### 3.2 統一ラッパーパターン

OpticsCompensationEffect の薄いラッパー（57行）を参照パターンとし、全ブリッジに適用:

```cpp
class PinchBulgeEffect : public ArtifactAbstractEffect {
public:
    PinchBulgeEffect() {
        setDisplayName("Pinch/Bulge");
        setEffectID("pinchBulge");
        setPipelineStage(EffectPipelineStage::Rasterizer);
    }
    
    void applyCPU(ImageF32x4_RGBA& in, ImageF32x4_RGBA& out,
                  const QRect& roi, EffectContext& ctx) override {
        float amount = amount_.evaluate(ctx.frame, ctx.time);
        float radius = radius_.evaluate(ctx.frame, ctx.time);
        float cx = centerX_.evaluate(ctx.frame, ctx.time);
        float cy = centerY_.evaluate(ctx.frame, ctx.time);
        
        auto displacement = ArtifactCore::makePinchBulge(amount, radius, cx, cy);
        ArtifactCore::applyDisplacement(in, out, roi, displacement);
    }

private:
    AnimatableFloat amount_ = 0.0f;    // + = bulge, - = pinch
    AnimatableFloat radius_ = 100.0f;
    AnimatableFloat centerX_ = 0.5f;
    AnimatableFloat centerY_ = 0.5f;
};
```

GPU パスは `runDistortCompute()` 共通ヘルパーで全効果に一括適用。

### 3.3 完了条件

- [ ] 6効果すべてが Composition カタログに表示される
- [ ] 6効果すべてがレイヤーに適用可能
- [ ] 6効果すべてのプロパティが Inspector で編集可能
- [ ] 各効果の GPU パスが CPU パスと画質一致

---

## Phase 4: 主要 AE Distort 効果

### 4.1 実装優先度

| 効果 | 優先度 | 理由 |
|------|--------|------|
| Bulge（PinchBulge から分離） | P0 | 最も使われる歪み効果。PinchBulge を Pinch/Bulge 2効果に分割 |
| Wave Warp | P0 | AE の代表的歪み。方形波/三角波/ノコギリ波を追加 |
| Ripple | P1 | 同心円波紋 |
| Magnify | P1 | 球面拡大 |
| Polar Coordinates | P2 | 直交⇔極座標 |
| CC_ 系 | P3 | Cycore 互換効果群（ライセンス要確認） |

### 4.2 WaveWarp 実装

既存 `WaveEffect` を拡張（現在 1D sine/cosine のみ）:

```cpp
enum class WaveType {
    Sine, Cosine, Square, Triangle, Sawtooth, Noise
};

// applyCPU 内:
float waveFunc(float phase, WaveType type) {
    switch(type) {
    case WaveType::Sine:     return std::sin(phase * 2.0f * 3.14159f);
    case WaveType::Square:   return std::sin(phase * 2.0f * 3.14159f) > 0 ? 1.0f : -1.0f;
    case WaveType::Triangle: return 2.0f * std::abs(2.0f * (phase - std::floor(phase + 0.5f))) - 1.0f;
    case WaveType::Sawtooth: return 2.0f * (phase - std::floor(phase)) - 1.0f;
    }
}
```

### 4.3 完了条件

- [ ] Bulge / Pinch が独立した効果として登録
- [ ] WaveWarp が6波型 + 2D モードで動作
- [ ] Ripple が同心円+減衰で正しく波紋を生成
- [ ] Magnify が球面レンズ歪みを正しく生成

---

## Phase 5: 統一 GPU Distort ヘルパー + カテゴリシステム

### 5.1 統一 GPU ヘルパー

各効果が個別にコピペしている ~30行の GPU ボイラープレートを `runDistortCompute()` に統一:

```cpp
// ArtifactCore/include/Graphics/Effect/DistortComputeHelper.ixx
struct DistortComputeConfig {
    std::string_view shaderSource;   // HLSL ソース
    std::string_view entryPoint;     // "main"
    uint32_t threadGroupX = 16;
    uint32_t threadGroupY = 16;
    
    // 定数バッファ（効果ごとに異なる）
    const void* cbData;
    size_t cbDataSize;
};

// 戻り値: 成功/失敗
// 内部で: input→RW Texture upload → Create PSO → Dispatch → Readback
bool runDistortCompute(
    Diligent::IDeviceContext* ctx,
    const ImageF32x4_RGBA& input,
    ImageF32x4_RGBA& output,
    const QRect& roi,
    const DistortComputeConfig& config
);
```

### 5.2 効果カテゴリシステム

`ArtifactEffectService` にカテゴリ enum を追加:

```cpp
enum class EffectCategory {
    Color, Blur, Distort, Keying, Matte,
    Noise, Transition, Stylize, Generate,
    Light, Audio, Time, Utility, OFX, Unknown
};

struct EffectInfo {
    QString id;
    QString displayName;
    EffectCategory category;  // 新規
    QString iconPath;
    QString description;
    // ...既存フィールド
};
```

### 5.3 完了条件

- [ ] 全 distort 効果が `runDistortCompute()` を介してGPUディスパッチされる
- [ ] `EffectCategory` enum が全 ~100 効果で設定される
- [ ] GPU ボイラープレートの重複が0になる

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `Artifact/src/Effects/TurbulentDisplace/TurbulentDisplaceEffect.cppm` | GPU パス実装 |
| P1 | `Artifact/src/Effects/DisplacementMap/DisplacementMapEffect.cppm` | GPU パス追加 |
| P1 | `Artifact/src/Effects/OpticsCompensation/OpticsCompensationEffect.cppm` | GPU パス追加 |
| P1 | 新規 `Artifact/shaders/turbulentDisplaceCS.hlsl` | TurbulentDisplace compute |
| P2 | 新規 `Artifact/src/Effects/Transform/TwistTransform.cppm` | CPU+GPU 実装 |
| P2 | 新規 `Artifact/src/Effects/Transform/BendTransform.cppm` | CPU+GPU 実装 |
| P3 | 新規 `Artifact/src/Effects/Distort/PinchBulgeEffect.cppm` | Core→Composition ブリッジ |
| P3 | 新規 `Artifact/src/Effects/Distort/TwirlEffect.cppm` | 同上 |
| P3 | 新規 他4効果 | 同上 |
| P4 | `Artifact/src/Effects/Wave/WaveEffect.cppm` | WaveType 多様化 |
| P4 | 新規 `Artifact/src/Effects/Distort/RippleEffect.cppm` | 波紋効果 |
| P4 | 新規 `Artifact/src/Effects/Distort/MagnifyEffect.cppm` | 拡大効果 |
| P5 | 新規 `ArtifactCore/include/Graphics/Effect/DistortComputeHelper.ixx` | 統一GPUヘルパー |
| P5 | `Artifact/src/Service/ArtifactEffectService.cppm` | EffectCategory enum追加 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P2: Twist+Bend | **P0** | 小 | 空スタブ→即実装。ヘッダのみなので実装を書くだけ |
| P1: GPUパス3効果 | **P0** | 中 | TurbulentDisplace が最も需要大 |
| P3: Coreブリッジ6効果 | **P1** | 中 | 薄いラッパー。光学補償のパターン踏襲 |
| P5: 統一ヘルパー | **P1** | 中 | P1完了後。ボイラープレート排除 |
| P4: AE互換効果 | **P2** | 大 | 新規効果開発。WaveWarp/Ripple/Magnify |
