# FFT 回折グレア（光芒・まつ毛・ハロ）実装 手順書

**最終更新:** 2026-08-13
**元記事:** [ポストエフェクトクエスト - 波動と回折とレンズと使われしシェーダ -](https://qiita.com/AngularSpectrumMTD/items/77f71b24b21cbc3d2a52)
**対象:** ArtifactStudio（DiligentEngine / DX12 / Vulkan / C++20 modules）

---

## 1. 結論（実現可否）

**実現可能。ただし、前回のレンズフレア記事より実装コストが高い。**

### 根拠

| 記事の要求 | ArtifactStudio の既存資産 | 状態 |
|---|---|---|
| コンピュートシェーダでのマルチパス処理 | `ComputeExecutor`（`ArtifactCore/src/Graphics/SandGPUCompute.cppm` が複数パス dispatch の実例） | ✅ 実装済み・流用可 |
| float 中間テクスチャ | `ImageF32x4_RGBA`（RGBA32F）+ `RenderConfig::PipelineFormatF32` | ✅ 実装済み |
| 二値化（threshold） | `EdgeBloomEffect` の HLSL に同等ロジック | ✅ 実装済み |
| FFT 畳み込み | CPU 側に OpenCV `fftConvolve`（`ApertureShapeBlurEffect.cppm:91-110`） | ✅ 実装済み（CPU） |
| **GPU FFT（butterfly + groupshared）** | なし。CPU の `AudioAnalyzer` に Radix-2 FFT があるのみ | ❌ **未実装（新規）** |
| エフェクト登録機構 | `ArtifactAbstractEffect` + `ArtifactEffectService` + CMakeLists | ✅ 実装済み |

### 最大の新規実装ポイント

記事のコアは **GPU FFT（butterfly 演算 + `groupshared` 共有メモリの複素 FFT）**。ArtifactStudio のシェーダ資産（`Artifact/shaders/`）にも GPU FFT は存在せず、新規に HLSL を書く必要がある。ここが本タスクの主な作業量。

### 実装スコープの判断

記事の実装は **折り返し雑音（エイリアシング）を許容**している（著者自身が「4 倍拡張（zero-padding）をしていないので折り返し雑音が出る。groupshared のサイズがやばいから」と明記）。ArtifactStudio に本格導入するなら、この問題を解決するか、CPU FFT とのハイブリッドで回避する必要がある。

現実的な導入は **2 段階**:

1. **Phase 1（低コスト・即効）**: CPU FFT（OpenCV `dft`）で光芒 + ハロを先に実装し、見た目を検証。
2. **Phase 2（本格）**: GPU FFT コンピュートシェーダを新規実装し、高速化。

---

## 2. 制約（AGENTS.md 遵守）

- **ビルド/CMake/テストはユーザー依頼時のみ**。
- **`QImage` 新規採用禁止**。中間は `ImageF32x4_RGBA`。
- **`QtCSS` / `setStyleSheet` 禁止**。プロパティ UI は `AbstractProperty` 経由で自動生成。
- **新規 signal/slot 禁止**。エフェクトは既存 apply 経路で完結。
- **Diligent 低レベル（PSO/ラスタライザ）の変更は最小化**。`ComputeExecutor` を使う。
- **C++20 modules**: `#include` は global module fragment にのみ。
- **ラインエンディングは LF**。

---

## 3. 既存資産（実装の土台）

| 資産 | パス | 用途 |
|---|---|---|
| エフェクト抽象 | `Artifact/include/Effects/ArtifactAbstractEffect.ixx` | 継承元 |
| エフェクト実装ベース | `Artifact/include/Effects/ArtifactEffectImplBase.ixx` | `applyCPU` / `applyGPU` |
| GPU コンピュートテンプレート | `Artifact/src/Effects/Glow/EdgeBloomEffect.cppm` | `GpuContext` + `ComputeExecutor` + HLSL 埋め込み + `createTextureFromImage` + `readbackTexture` |
| マルチパス compute 実例 | `ArtifactCore/src/Graphics/SandGPUCompute.cppm` | 複数 `ComputeExecutor` + 複数 dispatch |
| CPU FFT 畳み込み | `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm:91-110` | `fftConvolve`（OpenCV `dft`） |
| 絞り羽根形状 | `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm:29-69` | `makeBuiltInPsf` |
| エフェクトファクトリ | `Artifact/src/Service/ArtifactEffectService.cppm` | `createEffect` 登録 |
| エフェクト登録 | `Artifact/CMakeLists.txt` | `APP_MODULES` / `APP_IMPL` |

---

## 4. Phase 1: CPU FFT で光芒 + ハロを先に成立させる

### 4.1 ファイル構成（新規）

```
Artifact/
  include/Effects/Glow/DiffractionGlareEffect.ixx   # 公開インターフェース
  src/Effects/Glow/DiffractionGlareEffect.cppm      # 実装（CPU Impl、GPU は Phase 2）
```

### 4.2 公開インターフェース（`.ixx`）

`EdgeBloomEffect.ixx` を雛形にする。プロパティは記事のパラメータ + 拡張:

```cpp
module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.DiffractionGlare;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class DiffractionGlareEffect : public ArtifactAbstractEffect {
private:
    // 記事の ComputeParameters に対応
    float lambdaR_ = 633e-9f;   // 波長 R
    float lambdaG_ = 532e-9f;   // 波長 G
    float lambdaB_ = 466e-9f;   // 波長 B
    float glareIntensity_ = 0.8f;
    float threshold_ = 0.8f;
    // 拡張: グレア種別と開口形状
    int   glareMode_ = 0;        // 0=光芒(絞り羽根) 1=まつ毛 2=ハロ
    int   apertureBlades_ = 8;   // 光芒時の羽根枚数
    float apertureRotation_ = 0.0f;
    void syncImpls();
public:
    DiffractionGlareEffect();
    ~DiffractionGlareEffect() override;

    // getter/setter ...
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return false; } // Phase 1 は CPU のみ
};

}
```

### 4.3 CPU アルゴリズム（Phase 1）

記事の「エフェクトをかける」プロセスを OpenCV で再現する:

1. **ハイライト抽出（二値化）**: 入力画像の輝度 `(r+g+b)/3 > threshold` を白、それ以外を黒にする。閾値は `threshold_`。
2. **開口画像生成**:
   - 光芒: `makeBuiltInPsf(size, apertureBlades_, apertureRotation_, edgeBrightness)` を流用（絞り羽根形状）。
   - まつ毛: まつ毛状の線画像を生成（記事も「適当」と述べてる簡易版）。
   - ハロ: ゾーンプレート（同心円のモノクロ濃淡）を生成。
3. **FFT 畳み込み**: `fftConvolve(highlight, aperture)` を実行（`ApertureShapeBlurEffect.cppm:91-110` の関数を流用）。
4. **波長スケーリング（色収差）**: R/G/B で異なる波長（`lambdaR_`/`lambdaG_`/`lambdaB_`）に応じて開口像を拡縮し、チャンネルごとに畳み込む。記事の `mainSpectrumScaling` 相当。
5. **強度正規化 + 底上げ**: 記事の `mainRaiseBottomRealImage` 相当で、振幅が小さい部分を `glareIntensity_` で底上げ。
6. **加算合成**: `dst = src + glare * glareIntensity_`。

### 4.4 受け入れ基準（Phase 1）

- [ ] `diffraction_glare` エフェクトが `createEffect` から生成される。
- [ ] 光芒（絞り羽根の放射状バースト）が見える。
- [ ] ハロ（光源周りの同心円）が見える。
- [ ] 波長スケーリングにより色収差（虹色の縁）が再現される。
- [ ] プロパティが Inspector に表示され、値変更が反映される。

---

## 5. Phase 2: GPU FFT コンピュートシェーダ実装（本格）

### 5.1 GPU FFT の新規実装（最大の作業）

記事の HLSL（butterfly 演算 + groupshared）を ArtifactStudio の `ComputeExecutor` パターンに移植する。

#### 5.1.1 シェーダファイル（新規）

```
Artifact/shaders/diffraction_fft_ROW.hlsl
Artifact/shaders/diffraction_fft_COL.hlsl
Artifact/shaders/diffraction_ifft_ROW.hlsl
Artifact/shaders/diffraction_ifft_COL.hlsl
Artifact/shaders/diffraction_glare_common.hlsli   # 複素数演算 + butterfly 共通部
```

#### 5.1.2 複素数演算（共通 `.hlsli`）

記事の HLSL をそのまま移植:

```hlsl
float2 complex_conjugate(float2 c) { return float2(c.x, -c.y); }
float  complex_sqr(float2 c) { return c.x*c.x + c.y*c.y; }
float  complex_norm(float2 c) { return sqrt(complex_sqr(c)); }
float2 complex_add(float2 a, float2 b) { return float2(a.x+b.x, a.y+b.y); }
float2 complex_sub(float2 a, float2 b) { return float2(a.x-b.x, a.y-b.y); }
float2 complex_mul(float2 a, float2 b) {
    return float2(a.x*b.x - a.y*b.y, a.y*b.x + a.x*b.y);
}
float2 complex_div(float2 a, float2 b) {
    float2 c = complex_mul(a, complex_conjugate(b));
    float sqr = complex_sqr(b);
    return float2(c.x/sqr, c.y/sqr);
}
float2 complex_polar(float amp, float phase) {
    return float2(amp*cos(phase), amp*sin(phase));
}
```

#### 5.1.3 Butterfly 演算

記事の `ComputeSrcID` / `ComputeTwiddleFactor` / `ButterflyWeightPass` / `ButterflyPass` / `mainFFT` を移植。`groupshared` は float3 × 2 の複素バッファ:

```hlsl
groupshared float3 ButterflyArray[2][LENGTH];
```

> 注意: `LENGTH` / `BUTTERFLY_COUNT` / `ROW` / `INVERSE` は記事では `param.hlsl` を動的生成して注入している。ArtifactStudio では、2 の累乗サイズに固定するか、マクロで渡す。`EdgeBloomEffect` は HLSL を文字列埋め込みしているが、FFT はサイズ依存マクロが多数あるため、**外部 `.hlsl` ファイル + コンパイル時マクロ**の方が管理しやすい（`SandGPUCompute` が外部シェーダ参照の実例）。

### 5.2 マルチパス実行

記事の `ExecuteGlareCommand` のパイプラインを `ComputeExecutor` 複数個で再現:

| 記事のコマンド | ArtifactStudio 側 |
|---|---|
| `ExecuteCopyCommand` | `copyCS`（input → working） |
| `ExecuteBinaryThresholdCommand` | `thresholdCS` |
| `ExecuteClearCommand` | `clearCS` |
| `ExecuteFFTCommand` / `ExecuteIFFTCommand` | `fftCS_ROW` → `fftCS_COL`（実部/虚部の 2 テクスチャ） |
| `ExecuteCalcurateAmplitudeCommand` | `ampCS` |
| `ExecuteCalcMaxMinCommand` | `maxminfirstCS` → `maxminsecondCS` |
| `ExecuteDivideMaxAmpCommand` | `divByMaxAMPCS` |
| `ExecuteRaiseRICommand` | `raiseRICS` |
| `ExecuteSpectrumScalingCommand` | `spectrumScalingCS` |
| `ExecuteMultiplyCommand` | `mulCS` |
| `ExecuteAddCommand` | `AddCS` |

各 CS は `SandGPUCompute` のパターン（`build` → `createShaderResourceBinding` → `setTextureView` → `dispatch`）で実装する。

### 5.3 実部/虚部の 2 テクスチャ管理

FFT は複素数なので、実部と虚部を別テクスチャ（RGBA16F or RGBA32F）で持つ。記事は `sourceImageR`（実部）/ `sourceImageI`（虚部）を分けている。ArtifactStudio も同様に、エフェクト内部で実部/虚部の作業テクスチャを確保する。

### 5.4 折り返し雑音の対策（画質向上）

記事は zero-padding なしで折り返し雑音を許容している。ArtifactStudio では以下を検討:

- **zero-padding（4 倍拡張）**: FFT 前に画像を 2×2 に拡張（残りはゼロ埋め）→ FFT → 畳み込み → 逆 FFT → 中央を切り出し。メモリ 4 倍だが画質が改善。
- **CPU ハイブリッド**: 折り返しが問題になる場合、CPU FFT（OpenCV `dft` は zero-padding 済み）へフォールバック。

### 5.5 波長スケーリング（色収差）

記事の `mainSpectrumScaling` を移植。R/G/B の波長比でスペクトルを拡縮:

```hlsl
float ratioRG = lambdaG / lambdaR;
float ratioRB = lambdaB / lambdaR;
// uvR を基準に uvG = uvR * ratioRG, uvB = uvR * ratioRB でサンプリング
```

これは Phase 1 の CPU 版でも同じロジックを使う。

---

## 6. エフェクト登録（両 Phase 共通）

### 6.1 ファクトリ登録

`Artifact/src/Service/ArtifactEffectService.cppm` の `createEffect` に追加:

```cpp
if (effectId == QStringLiteral("diffraction_glare") ||
    effectId == QStringLiteral("Effect.Glow.DiffractionGlare")) {
    auto effect = std::make_unique<DiffractionGlareEffect>();
    effect->setEffectID(UniString::fromQString(effectId));
    effect->setDisplayName(QStringLiteral("Diffraction Glare"));
    return effect;
}
```

ファイル冒頭に `import Artifact.Effect.Glow.DiffractionGlare;` を追加。

### 6.2 CMake 登録

`Artifact/CMakeLists.txt` の Glow 系ループ（`Artifact/CMakeLists.txt:385-394`）を参考に `DiffractionGlare` を追加:

```cmake
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/DiffractionGlareEffect.ixx" _dg_module_idx)
if(_dg_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/DiffractionGlareEffect.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/DiffractionGlareEffect.cppm" _dg_impl_idx)
if(_dg_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/DiffractionGlareEffect.cppm")
endif()
```

Phase 2 ではシェーダファイル（`.hlsl` / `.hlsli`）も CMake のシェーダリストへ追加する。

---

## 7. プロパティ定義（`getProperties`）

| 表示名 | 内部キー | 型 | 範囲 | 意味 |
|---|---|---|---|---|
| Glare Mode | `glareMode` | Integer | 0..2 | 0=光芒 1=まつ毛 2=ハロ |
| Intensity | `glareIntensity` | Float | 0..4 | グレア強度 |
| Threshold | `threshold` | Float | 0..1 | 光源抽出閾値 |
| Wavelength R | `lambdaR` | Float | 380e-9..780e-9 | 赤の波長（色収差） |
| Wavelength G | `lambdaG` | Float | 380e-9..780e-9 | 緑の波長 |
| Wavelength B | `lambdaB` | Float | 380e-9..780e-9 | 青の波長 |
| Aperture Blades | `apertureBlades` | Integer | 0..12 | 光芒時の羽根枚数 |
| Aperture Rotation | `apertureRotation` | Float | -180..180 | 羽根回転 |

> 波長のデフォルトは記事と同じ 633/532/466nm。単位はメートル（`633e-9`）で内部保持するが、UI 表示は nm に変換する方が親切（実装時に検討）。

---

## 8. 実装順序まとめ

| 順 | 作業 | 新規/変更 | 依存 |
|---|---|---|---|
| 1 | `DiffractionGlareEffect.ixx` 作成 | 新規 | `ArtifactAbstractEffect` |
| 2 | `DiffractionGlareEffect.cppm`（CPU Impl、OpenCV `fftConvolve` 流用） | 新規 | 1, `fftConvolve` |
| 3 | 光芒 + ハロの CPU アルゴリズム（波長スケーリング含む） | 変更（同ファイル） | 2 |
| 4 | `ArtifactEffectService.cppm` にファクトリ追加 | 変更 | 1 |
| 5 | `Artifact/CMakeLists.txt` にモジュール登録 | 変更 | 1, 2 |
| 6 | Phase 2: 複素数演算 + butterfly HLSL 作成 | 新規 | 5 |
| 7 | Phase 2: FFT/IFFT の 4 シェーダ + マルチパス `ComputeExecutor` | 新規/変更 | 6 |
| 8 | Phase 2: GPU Impl（`applyGPU`）追加 | 変更（`.cppm`） | 7 |

---

## 9. リスク・注意点

- **GPU FFT は 2 の累乗サイズ前提**: フレームが 2 の累乗でない場合、パディングか `AudioAnalyzer` の Radix-2 を参考に任意サイズ対応を検討。
- **`groupshared` メモリ制限**: 記事が zero-padding を諦めた理由。4K（4096×4096）では groupshared が破綻するため、サイズ上限を設けるか、タイル分割 FFT が必要。
- **折り返し雑音**: zero-padding なしでは画像端からもエフェクトが出る。AE 的な品質なら CPU FFT（OpenCV は zero-padding 済み）で担保し、GPU は高速プレビュー用と割り切る選択肢もある。
- **複素テクスチャのフォーマット**: 記事は `R16G16B16A16_FLOAT`。ArtifactStudio は `RenderConfig::PipelineFormatF32`（RGBA32F）が既定なので、まず 32F で実装し、必要なら 16F に落とす。
- **既存の未配線 `lensFlare` シェーダとの関係**: 前回手順書の物理レンズフレアとは別系統。本エフェクト（FFT 回折グレア）は「光源の回折パターン」、レンズフレアは「レンズ内部の反射」で、重複しない。
- **ビルドはユーザー依頼時のみ**: 手順書のコードは未ビルド。モジュール名・シェーダコンパイルは実ビルド時に確認する。

---

## 10. 関連ドキュメント

- 物理レンズフレア手順書: `docs/planned/PHYSICAL_LENS_FLARE_IMPLEMENTATION_2026-08-13.md`（本記事の上位概念。回折パターンはレンズフレアのスターバーストとしても再利用できる）
- 既存の絞り羽根/FFT 資産: `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm`
