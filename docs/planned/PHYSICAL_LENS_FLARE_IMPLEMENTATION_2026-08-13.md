# 物理ベース・レンズフレア実装 手順書

**最終更新:** 2026-08-13
**元記事:** [Physically-Based Real-Time Lens Flare Rendering (Hullin et al. 2011) の実装解説](https://qiita.com/AngularSpectrumMTD/items/aeea1046c0ec5d4dd1d5)
**対象:** ArtifactStudio（DiligentEngine / DX12 / Vulkan / C++20 modules）

---

## 1. 結論（実装可否）

**実装可能。** ArtifactStudio の既存基盤と記事の技術が噛み合う。

### 根拠

| 記事の要求 | ArtifactStudio の既存資産 | 状態 |
|---|---|---|
| DX12 レンダラ | `DiligentDeviceManager`（DX12/Vulkan デュアル） | ✅ 実装済み |
| コンピュートシェーダでのポストエフェクト | `EdgeBloomEffect`（`Artifact/src/Effects/Glow/EdgeBloomEffect.cppm`）が `GpuContext` + `ComputeExecutor` + HLSL 埋め込み + `applyGPU` の完全なテンプレート | ✅ 実装済み・流用可 |
| 絞り羽根の多角形形状生成 | `ApertureShapeBlurEffect::makeBuiltInPsf`（`Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm:29-69`）が 5/6 枚羽根・回転・エッジ輝度を生成済み | ✅ 実装済み・流用可 |
| FFT 畳み込み（回折パターン） | `ApertureShapeBlurEffect::fftConvolve`（同 `:91-110`）が OpenCV DFT ベースで実装済み | ✅ 実装済み・流用可 |
| レンズフレア用シェーダ | `Artifact/shaders/lensFlareVS.hlsl` / `lensFlarePS.hlsl` が既に存在（ただし C++ から未登録） | ⚠️ 資産のみ・要配線 |
| エフェクト登録機構 | `ArtifactAbstractEffect` 継承 + `getProperties`/`setPropertyValue` + `ArtifactEffectService::createEffect` + CMakeLists 登録 | ✅ 実装済み |

### 実装スコープの判断

論文完全版（光束追跡で全ゴースト列挙 + フラウンホーファー/ASM 回折積分をゴースト数分反復）は、**特許データ由来のレンズ系定義と大量の前計算**が必要で、初期実装としては重い。記事の著者自身も「高速化は未着手」と述べている。

現実的な導入は **2 段階**:

1. **Phase 1（近似・即効）**: 既存の絞り羽根 + ゴースト + スターバーストを組み合わせた「レンズフレア」エフェクトを CPU で実装し、`EdgeBloomEffect` パターンで GPU 化。まず見た目を成立させる。
2. **Phase 2（物理ベース）**: 記事の光束追跡（フレネル界面 + 絞り + Sellmeier 分散）をコンピュートシェーダで実装し、特許データからレンズ系を読み込む。

本手順書は **Phase 1 を完全実装 + Phase 2 を詳細設計** までを対象とする。

---

## 2. 制約（AGENTS.md 遵守）

- **ビルド/CMake/テストはユーザー依頼時のみ**。本手順ではソース編集のみ行い、ビルドは行わない。
- **`QImage` 新規採用禁止**。中間は `ImageF32x4_RGBA`（float32 RGBA）を使う。
- **`QtCSS` / `setStyleSheet` 禁止**。プロパティ UI は既存の `AbstractProperty` 経由で自動生成されるため、本タスクでは UI コード不要。
- **新規 signal/slot 禁止**。エフェクトは既存の apply 経路で完結。
- **Diligent 低レベル（PSO/ラスタライザ）の変更は最小化**。`ComputeExecutor` の既存ラッパを使う。
- **C++20 modules**: `#include` は global module fragment（`module;` と `module X;` の間）にのみ置く。purview に `#include` を置かない。
- **ラインエンディングは LF**。

---

## 3. 事前調査で確認した既存資産（実装の土台）

| 資産 | パス | 用途 |
|---|---|---|
| エフェクト抽象 | `Artifact/include/Effects/ArtifactAbstractEffect.ixx` | 継承元。`apply` / `getProperties` / `setPropertyValue` / `supportsGPU` |
| エフェクト実装ベース | `Artifact/include/Effects/ArtifactEffectImplBase.ixx` | `applyCPU` / `applyGPU` を持つ Impl |
| GPU コンピュートテンプレート | `Artifact/src/Effects/Glow/EdgeBloomEffect.cppm` | `GpuContext` + `ComputeExecutor` + HLSL 埋め込み + `createTextureFromImage` + `readbackTexture` の完全パターン |
| 絞り羽根生成 | `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm:29-69` | `makeBuiltInPsf`（多角形 + 回転 + エッジ） |
| FFT 畳み込み | `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cppm:91-110` | `fftConvolve`（OpenCV DFT） |
| エフェクトファクトリ | `Artifact/src/Service/ArtifactEffectService.cppm` | `createEffect` に ID → インスタンスを追加 |
| エフェクト登録 | `Artifact/CMakeLists.txt`（`APP_MODULES` / `APP_IMPL`） | 新規 `.ixx` / `.cppm` を追加 |
| レンズフレアシェーダ（既存・未配線） | `Artifact/shaders/lensFlareVS.hlsl` / `lensFlarePS.hlsl` | Phase 2 で活用検討 |

---

## 4. Phase 1: 近似レンズフレアエフェクト（CPU → GPU）

### 4.1 ファイル構成（新規）

```
Artifact/
  include/Effects/Glow/PhysicalLensFlareEffect.ixx   # 公開インターフェース
  src/Effects/Glow/PhysicalLensFlareEffect.cppm      # 実装（CPU + GPU Impl）
```

> 命名は既存の `Glow/` 配下パターン（`EdgeBloomEffect` 等）に合わせる。`LensFlare` ではなく `PhysicalLensFlare` として、既存の未配線 `lensFlare` シェーダや今後追加する可能性のある別フレアと区別する。

### 4.2 公開インターフェース（`.ixx`）

`EdgeBloomEffect.ixx`（`Artifact/include/Effects/Glow/EdgeBloomEffect.ixx`）を雛形にする。

```cpp
module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <QString>
#include <QVariant>

export module Artifact.Effect.Glow.PhysicalLensFlare;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

export namespace Artifact {

class PhysicalLensFlareEffect : public ArtifactAbstractEffect {
private:
    // パラメータ（4.4 参照）
    float intensity_ = 1.0f;
    int apertureBlades_ = 6;
    float apertureRotation_ = 0.0f;
    float apertureSize_ = 1.0f;
    float ghostCount_ = 4.0f;
    float ghostSpacing_ = 1.0f;
    float ghostDispersion_ = 0.5f;
    float starBurstIntensity_ = 1.0f;
    float threshold_ = 0.85f;
    // 追加パラメータ...
    void syncImpls();
public:
    PhysicalLensFlareEffect();
    ~PhysicalLensFlareEffect() override;

    // getter/setter ...
    std::vector<AbstractProperty> getProperties() const override;
    void setPropertyValue(const UniString& name, const QVariant& value) override;
    bool supportsGPU() const override { return true; }
};

}
```

### 4.3 実装（`.cppm`）の骨格

`EdgeBloomEffect.cppm` の構造を踏襲する。

```
module; ... #include ...
module Artifact.Effect.Glow.PhysicalLensFlare;
import ...;

namespace Artifact {

// --- CPU Impl ---
class PhysicalLensFlareCPUImpl : public ArtifactEffectImplBase {
    // パラメータメンバ
    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        // 4.5 の CPU アルゴリズム
    }
};

// --- GPU Impl ---
class PhysicalLensFlareGPUImpl : public ArtifactEffectImplBase {
    // GpuContext / ComputeExecutor / paramsCB_ / outputTex_
    void applyCPU(...) override { cpuImpl_.applyCPU(src, dst); }
    void applyGPU(...) override {
        // 4.6 の GPU パス（EdgeBloomEffect の applyGPU を雛形に）
    }
};

// コンストラクタで setEffectID / setDisplayName / setPipelineStage / setCPUImpl / setGPUImpl / setComputeMode(AUTO)
// syncImpls / getProperties / setPropertyValue
}
```

### 4.4 プロパティ定義（`getProperties`）

記事のパラメータを AE 的な編集可能項目に落とす。各プロパティは `PropertyType::Float` / `Integer` + `setHardRange` で範囲を付ける（`DepthBokehEffect::getProperties` の `add` ラムダ方式が参考になる）。

| 表示名 | 内部キー | 型 | 範囲 | 意味 |
|---|---|---|---|---|
| Intensity | `intensity` | Float | 0..4 | フレア全体の強度 |
| Aperture Blades | `apertureBlades` | Integer | 0..12（0=円） | 絞り羽根の枚数 |
| Aperture Rotation | `apertureRotation` | Float | -180..180 | 絞り形状の回転 |
| Aperture Size | `apertureSize` | Float | 0.2..4 | ゴースト/ボケの大きさ |
| Ghost Count | `ghostCount` | Float | 0..12 | ゴーストの数 |
| Ghost Spacing | `ghostSpacing` | Float | 0..4 | ゴースト間隔 |
| Dispersion | `ghostDispersion` | Float | 0..2 | 色分散（虹色の広がり） |
| Starburst Intensity | `starBurstIntensity` | Float | 0..4 | 回折スターバースト強度 |
| Threshold | `threshold` | Float | 0..1 | フレア発生の輝度閾値 |

### 4.5 CPU アルゴリズム（Phase 1 の見た目を成立させる）

1. **入力からハイライト抽出**: 閾値 `threshold_` を超える輝度（`max(r,g,b) - threshold`）を正規化してマスクを作る。
2. **絞り羽根 PSF 生成**: `makeBuiltInPsf(size, apertureBlades_, apertureRotation_, edgeBrightness)` を流用（`ApertureShapeBlurEffect.cppm:29-69` をコピーまたは共通化）。`apertureSize_` で PSF サイズをスケール。
3. **ゴースト合成**: 画面中心を基準に、光軸対称位置へハイライトを配置。`ghostCount_` 個、`ghostSpacing_` で間隔を開け、`ghostDispersion_` で RGB チャンネルごとに位置をずらす（色分散）。
4. **スターバースト合成**: 絞り羽根のエッジに沿った回折線を放射状に描く。多角形の頂点方向（`apertureBlades_` 枚なら `apertureBlades_` 本、偶数枚なら `2*blades` 本）へ線状の減衰を加算。
5. **加算合成**: `dst = src + (ghost + starburst) * intensity_`。float32 なので HDR 値をそのまま保持（クランプしない）。

> この CPU 版はあくまで「見た目を先に成立させる」ための近似。論文の物理ベース（光束追跡 + 回折積分）は Phase 2。

### 4.6 GPU 化（Phase 1）

`EdgeBloomEffectGPUImpl::applyGPU`（`EdgeBloomEffect.cppm:127-147`）を丸ごと雛形にする。

1. `acquireSharedRenderDeviceForCurrentBackend` で device/context 取得。
2. `GpuContext` + `ComputeExecutor` を初期化。
3. HLSL 文字列を埋め込む（`kPhysicalLensFlareHlsl`）。`g_InputTexture` / `g_OutputTexture` / params cbuffer。
4. シェーダ内で上記 CPU アルゴリズムと同じロジックを per-pixel で実行。
5. `createTextureFromImage`（RGBA32F）で入力アップロード、`readbackTexture` で結果を `ImageF32x4_RGBA` へ戻す。

HLSL の要点:
```hlsl
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer LensFlareParams : register(b0) { /* 各パラメータ */ };

[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID) {
    // 入力ピクセル読み → ハイライト抽出 → ゴースト/スターバーストを
    // スクリーン中心基準で加算 → g_OutputTexture へ
}
```

### 4.7 ファクトリ登録

`Artifact/src/Service/ArtifactEffectService.cppm` の `createEffect` に追加:

```cpp
if (effectId == QStringLiteral("physical_lens_flare") ||
    effectId == QStringLiteral("Effect.Glow.PhysicalLensFlare")) {
    auto effect = std::make_unique<PhysicalLensFlareEffect>();
    effect->setEffectID(UniString::fromQString(effectId));
    effect->setDisplayName(QStringLiteral("Physical Lens Flare"));
    return effect;
}
```

あわせて、ファイル冒頭に `import Artifact.Effect.Glow.PhysicalLensFlare;` を追加する。

### 4.8 CMake 登録

`Artifact/CMakeLists.txt` の Glow 系ループ（`LiquidGlow` / `ResidualGlow` を登録している箇所、`Artifact/CMakeLists.txt:385-394`）を参考に、`PhysicalLensFlare` を `APP_MODULES`（`.ixx`）と `APP_IMPL`（`.cppm`）へ追加する。

```cmake
# PhysicalLensFlare
list(FIND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/PhysicalLensFlareEffect.ixx" _plf_module_idx)
if(_plf_module_idx EQUAL -1)
    list(APPEND APP_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/include/Effects/Glow/PhysicalLensFlareEffect.ixx")
endif()
list(FIND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/PhysicalLensFlareEffect.cppm" _plf_impl_idx)
if(_plf_impl_idx EQUAL -1)
    list(APPEND APP_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/src/Effects/Glow/PhysicalLensFlareEffect.cppm")
endif()
```

### 4.9 受け入れ基準（Phase 1）

- [ ] `physical_lens_flare` エフェクトが `ArtifactEffectService::createEffect` から生成される。
- [ ] プロパティが Inspector に表示され、値を変更すると `setPropertyValue` → `syncImpls` で反映される。
- [ ] CPU 適用でゴースト + スターバーストが見える。
- [ ] GPU 適用で CPU と同等の結果が出る（`ComputeMode::AUTO` で切替）。
- [ ] `QImage` / `QtCSS` / 新規 signal/slot を使っていない。

---

## 5. Phase 2: 物理ベース（論文準拠）詳細設計

Phase 1 でエフェクトの器と UI が成立した後、記事の物理ベースアルゴリズムを導入する。

### 5.1 レンズ系データモデル

記事の構造体を ArtifactStudio 側の表現に写す。

```cpp
// 特許データ → 内部表現（記事の PatentFormat → LensInterface に対応）
struct LensInterface {
    float3 center;      // 球面/平面の中心（z 軸上）
    float  radius;      // 球面/平面の半径（負値で凹面）
    float3 n;           // 屈折率 (n0, n1, n2)。n1 = 反射防止膜
    float  sa;          // 光軸からの公称半径（絞り相当）
    float  d1;          // コーティング厚 = λ/4/n1
    float  flat;        // 平面かどうか
};
```

Sellmeier 分散: `n(λ) = 1 + Σ B_n λ² / (λ² - C_n)`（B/C は材質定数）。

### 5.2 ゴースト列挙

- フレネル界面（レンズ面）が `n` 個のとき、2 回反射のゴースト数は `n(n-1)/2`。
- 各ゴーストの反射面ペアを列挙し、反射回数 2 回までのものを対象にする（高次は強度が微弱で無視）。

### 5.3 光束追跡（コンピュートシェーダ）

記事の「直線 vs 平面 / 直線 vs 球面」の交差判定を HLSL で実装:

```hlsl
// 球面交差: |d|²t² + 2(q·d)t + |q|² - r² = 0
// 判別式 B² - C < 0 なら交差なし
// t = sqrt(B²-C) * sgn(r·d.z) - B
```

- 各光束（等間隔グリッド）を追跡し、交差ごとに `r_rel = max(r_rel, r / r_surface)` を更新。
- センサ到達位置、絞り通過位置、光軸からの最大距離を記録。
- `r_rel > 1` はレンズ系から脱出（描画しないが補間に寄与）。

### 5.4 強度計算（コースティクス）

記事の面積ベース強度:

```
s = (a+b+c+d)/2
C = cos((θ1+θ2)/2)
S = sqrt((s-a)(s-b)(s-c)(s-d) - abcd·C)
I = S_base / Σ S_n
```

隣接する追跡結果のクアッド面積で「集光するほど輝度が高い」を再現。

### 5.5 回折パターン（テクスチャ事前計算）

- **スターバースト**: フラウンホーファー回折。開口パターン `T(x,y)` の強度周波数スペクトルを波長方向に積分。絞り羽根形状から事前計算テクスチャ生成。
- **ゴースト（リンギング）**: 角スペクトル法（ASM）。`I = F⁻¹[F[T]·H]`、`H = exp[i2π w z]`。記事は FrFT（Ozaktas）を推奨しているが、ASM で近似可。

これらは起動時または初回使用時に CPU でテクスチャ化し、GPU ではサンプリングのみ行う。

### 5.6 特許データ読み込み

- 米国特許データ（例: Nikon の `PN/5835272`）からレンズ系パラメータを抽出するパーサを追加。
- 入力形式は JSON/CSV で、`LensInterface` 配列へ変換。プリセットとして数種類（Nikon/Angenieux/Canon）を同梱。

---

## 6. 実装順序まとめ

| 順 | 作業 | 新規/変更 | 依存 |
|---|---|---|---|
| 1 | `PhysicalLensFlareEffect.ixx` 作成 | 新規 | `ArtifactAbstractEffect` |
| 2 | `PhysicalLensFlareEffect.cppm`（CPU Impl + 近似アルゴリズム）作成 | 新規 | 1, `makeBuiltInPsf` 流用 |
| 3 | GPU Impl（HLSL 埋め込み）追加 | 変更（同ファイル） | `EdgeBloomEffect` 雛形 |
| 4 | `ArtifactEffectService.cppm` にファクトリ追加 | 変更 | 1 |
| 5 | `Artifact/CMakeLists.txt` にモジュール登録 | 変更 | 1, 2 |
| 6 | Phase 2: レンズ系データモデル + 光束追跡 HLSL | 新規/変更 | 5 |
| 7 | Phase 2: 回折パターン事前計算 + 特許データパーサ | 新規 | 6 |

---

## 7. リスク・注意点

- **Phase 1 は近似**: 記事の物理ベース（光束追跡 + 回折積分）とは別物。論文の品質を求めるなら Phase 2 が必須。
- **パフォーマンス**: 記事のフル実装はゴースト数 × グリッド分割数の計算量。Phase 2 ではゴースト数・グリッド分割数を LOD 制御し、`EdgeBloomEffect` 同様に GPU で負荷を吸収する。
- **`makeBuiltInPsf` のコピー**: 現在 `ApertureShapeBlurEffect.cppm` 内の匿名名前空間関数。流用する場合は共通化するかコピーする。共通化すると `ApertureShapeBlurEffect` 側の変更も必要（影響範囲に注意）。
- **既存 `lensFlare` シェーダの扱い**: `Artifact/shaders/lensFlareVS/PS.hlsl` は別の描画パス（PUSHCONSTANT + オクルージョンサンプリング）向けで、`EdgeBloomEffect` 型のコンピュートパスとは構造が異なる。Phase 1 では無理に使わず、Phase 2 のスクリーンスペース合成で参考にする。
- **ビルドはユーザー依頼時のみ**: 手順書のコードは未ビルド。モジュール名・include の整合は実ビルド時に確認する。
