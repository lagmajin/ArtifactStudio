# プロダクションカラーシステム 詳細監査

**日付**: 2026-08-02
**調査範囲**: ソースコード直接読み込み（24ファイル）

---

## 全体アーキテクチャ

```
ArtifactCore (基盤層)
├── ColorSpace.ixx/.cppm         — 色空間 enum + 動的行列計算 + Bradford 色順応
├── ColorTransferFunction.ixx    — 16種の伝達関数（DaVinci Resolve 同等レベル）
├── ColorGamutConversion.ixx     — 11種の色域 + XYZ変換行列 + D60/D65 Bradford
├── ColorACES.ixx                — ACES IDT→RRT→ODT パイプライン
├── OCIOConfig.ixx/.cppm         — OCIO config（JSONベース。実ライブラリではない）
├── ColorLUT.ixx/.cppm           — 3D LUT 読み込み/書き出し
└── LUTWriter.ixx                — LUT ファイル出力

Artifact (アプリ層)
├── ArtifactColorScienceManager  — グローバル/コンポジション別設定 + LUT管理 + 変換キャッシュ
├── ArtifactOCIOManager          — OCIOConfig ↔ ColorScienceManager ブリッジ
├── ArtifactColorSciencePanel    — UI（OCIOプリセット/display/view コンボボックス）
├── ArtifactColorGradingEngine   — カラーグレーディングエンジン
├── ArtifactColorWheels          — カラーホイール
└── ArtifactColorNodeGraph       — カラーノードグラフ
```

---

## 1. ColorSpace（色空間） — 🟢 優

`ArtifactCore/include/Color/ColorSpace.ixx` + `.cppm`

| 項目 | 評価 |
|------|------|
| 色空間数 | 6種（Linear/sRGB/Rec709/Rec2020/P3/ACES_AP0/ACES_AP1） |
| 行列計算 | **動的計算**。色度座標から XYZ 行列をリアルタイムに生成 |
| 色順応 | **Bradford**。D60↔D65 のホワイトポイント変換を完全実装 |
| 実装品質 | 310行。constexpr 色度座標、手書き3x3逆行列、数値安定性チェック |

**コード品質**: `rgbToXyzMatrix()` が色度座標 {x,y} から XYZ プライマリ行列を計算し、Bradford 白色点変換を挟んでから逆行列でターゲット空間に変換。数値計算の教科書的な実装。

```cpp
// L143-161: 色度からXYZ行列を動的計算
Mat3 rgbToXyzMatrix(Chromaticity red, Chromaticity green, Chromaticity blue, Chromaticity white) {
    const auto primaries = { xr[0], xg[0], xb[0], ... };
    const auto scale = multiply3Vec(invert3(primaries), whiteXyz);
    return { primaries * scale }; // 白色点で正規化
}
```

---

## 2. ColorTransferFunction（伝達関数） — 🟢 優秀

`ArtifactCore/include/Color/ColorTransferFunction.ixx`

| 伝達関数 | 状態 | 備考 |
|----------|------|------|
| sRGB | ✅ | IEC 61966-2-1 完全 |
| Rec.709 | ✅ | OETF 完全 |
| Rec.2084 PQ | ✅ | SMPTE ST 2084。定数値が正確 |
| HLG | ✅ | ARIB STD-B67。sqrt + log ブランチ |
| ACEScc | ✅ | SMPTE ST 2065-4 |
| ACEScct | ✅ | toe 付き |
| DaVinci Intermediate | ✅ | Blackmagic 公式換算式 |
| Sony S-Log3 | ✅ | 2ブランチ toe + log |
| Canon Log 2 / Log 3 | 🟡 | enum定義のみ。実装なし |
| Cineon | 🟡 | enum定義のみ。実装なし |
| Gamma 2.2/2.4/2.6 | ✅ | 単純 pow |

**実装品質**: PQ の定数値（`kPQ_m1=2610/16384`, `kPQ_m2=2523/32*128` など）が SMPTE ST 2084 仕様書と完全一致。ACEScc も `(9.72-log2(x*0.5+0.000030517578125))/17.52` が正確。

---

## 3. ColorGamutConversion（色域変換） — 🟢 優秀

`ArtifactCore/include/Color/ColorGamutConversion.ixx`

| 色域 | 状態 |
|------|------|
| sRGB / Rec.709 | ✅ XYZ 行列 |
| Rec.2020 | ✅ XYZ 行列 |
| DCI-P3 | ✅ XYZ 行列 |
| Display P3 (Apple) | ✅ XYZ 行列 |
| ACES AP0 (ACES2065-1) | ✅ XYZ 行列 (D60) |
| ACES AP1 (ACEScg) | ✅ XYZ 行列 (D60) |
| AdobeRGB | ✅ XYZ 行列 |
| DaVinci Wide Gamut | ✅ XYZ 行列 (Blackmagic 公式値) |
| XYZ D65 / D60 | ✅ パススルー |
| D60↔D65 Bradford | ✅ 3x3 変換行列 |

**独自の強み**: DaVinci Wide Gamut の XYZ 行列が実装されている。これは Resolve ユーザーとの相互運用に直接使える。

---

## 4. ACESColorManager（ACES パイプライン） — 🟡 実用的

`ArtifactCore/include/Color/ColorACES.ixx`

| 機能 | 状態 | 備考 |
|------|------|------|
| IDT（入力変換） | ✅ | 5種（sRGB Linear/Encoded, Rec709, Rec2020, P3） |
| ODT（出力変換） | ✅ | 5種（SDR sRGB/Rec709/P3, HDR PQ/HLG） |
| RRT（レンダリング変換） | 🟡 | **簡易近似**。"ACES Filmic" 式。本物の RRT ではない |
| ACES Filmic 近似 | ✅ | `(x*(2.51x+0.03))/(x*(2.43x+0.59)+0.14)` |
| Soft Clip | ✅ | ガマット境界外の値をソフトに圧縮 |
| GPU行列取得 | ✅ | `getInputConversionMatrix()` / `getOutputConversionMatrix()` |

**注意**: コード内コメントに明記:
```
// ※ RRT+ODT の完全実装は非常に複雑。ここではカラースペース変換 + 伝達関数変換の簡易版。
```

本物の ACES RRT は Academy の CTL コード（数千行）で定義されており、それを実装するには ACES 1.3 Reference Implementation を C++ に移植する必要がある。

---

## 5. ColorScienceManager（アプリ層） — 🟢 充実

`Artifact/include/Color/ArtifactColorScienceManager.ixx`

| 機能 | 状態 |
|------|------|
| グローバル設定 | ✅ ColorScienceSettings（入力/作業/出力/LUT/HDR） |
| **コンポジション別設定** | ✅ CompositionColorSettings。useGlobalSettings フラグ |
| LUT 管理 | ✅ loadLUT / loadBuiltinLUT / clearLUT / hasActiveLUT |
| 変換キャッシュ | ✅ `map<pair<ColorSpace,ColorSpace>, function>` |
| HDR | ✅ isHDREnabled / setHDREnabled |
| 信号 | ✅ settingsChanged / lutChanged / compositionSettingsChanged |

---

## 6. 二重実装の問題 🟡

プロジェクト内に**2つの並列する色空間変換システム**が存在する:

| システム | 型 | ファイル | 色空間数 |
|----------|-----|----------|---------|
| `ColorSpace` + `ColorSpaceConverter` | 旧 | `Color/ColorSpace.ixx` | 6種（動的行列計算） |
| `Gamut` + `ColorGamutConversion` | 新 | `Color/ColorGamutConversion.ixx` | 11種（静的行列） |

```cpp
// OCIOManager.cppm L30-46 — 古い方を使っている
static ColorSpace mapOCIOColorSpaceToEnum(const QString& csName) {
    if (lower == "srgb") return ColorSpace::sRGB;     // 旧 enum
    if (lower == "rec709") return ColorSpace::Rec709;
    ...
}

// ColorACES.ixx L62 — 新しい方を使っている
Gamut inputGamut = inputGamutFromTransform(inputTransform);  // 新 enum
Gamut workingGamut = Gamut::ACES_AP1;
```

新旧のコードが混在している。特に `ColorSpaceConverter::getConversionMatrix()` は Bradford で行列を動的計算するが、`ColorGamutConversion::getConversionMatrix()` は事前計算された静的行列を使う。計算結果は同一だが、コードの一貫性がない。

---

## 7. GPU カラーパイプラインの不在 🟡

現在のカラー変換はすべて **CPU**（`Parallel::For` + `ColorSpaceConverter::applyGamma`）:
```cpp
// OCIOManager.cppm L303
ArtifactCore::Parallel::For(0, h, w*h, [&](int y) {
    // CPU上で float* row をループ
});
```

GPU コンピュートシェーダーを使ったカラー変換パイプラインは存在しない。Diligent Engine で GPU レンダリングしているのに、色変換だけ CPU 往復している。

---

## 全体評価

| レイヤー | スコア | 所見 |
|----------|--------|------|
| **伝達関数** | 🟢 95% | DaVinci Resolve 同等。PQ/HLG/ACEScc/S-Log3/DaVinci Intermediate 完備 |
| **色域変換** | 🟢 95% | 11種のガマット + Bradford色順応。DWG公式値まで実装 |
| **ACESパイプライン** | 🟡 70% | IDT+ODTは揃っている。RRTは簡易近似。本物が必要ならCTL移植 |
| **OCIO** | 🟡 65% | JSONベースの自前実装。実OCIOライブラリではない。config.ocio 読めない |
| **ColorScienceManager** | 🟢 85% | グローバル+コンポジション別+LUT+キャッシュ。よくできている |
| **GPU カラーパイプライン** | 🟠 20% | すべてCPU。"GPUレンダリング後にCPUで色変換"は帯域を浪費 |
| **コード一貫性** | 🟡 60% | 新旧2系統の色空間変換が混在。統合が必要 |

**総合**: 🟡 70% — 基礎は非常に堅固。伝達関数と色域変換は業界トップレベル。残課題は OCIO 実ライブラリ統合、RRT 完全実装、GPU 化、コード統合の4点。
