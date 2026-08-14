# 画像バッファ精度 監査メモ

**最終更新:** 2026-08-13

レンダリングの画像バッファが「十分か」を、ソースコード（`.ixx` / `.cppm`）を一次情報として検証し、弱点を深掘りしたメモ。

## 結論

GPU 合成本線（accum/temp/float 中間）は **RGBA32F で AE 32bpc 級に十分**。ただし「レイヤー入口」「マット」「最終表示」「CPU パス」が 8bit に固定され、精度の非対称がある。特に **レイヤー単体ラスタの 8bit sRGB 固定** と **HDR 表示の実質 SDR 固定** が明確な弱点。

---

## バッファ構成の現状（確認済み）

### メイン中間（合成用）— 十分
`RenderConfig`（`Artifact/include/Render/RenderConfig.ixx`）:

| 定数 | 値 | 用途 |
|---|---|---|
| `PipelineFormatF32` | `RGBA32_FLOAT` | デフォルト合成フォーマット |
| `PipelineFormatF16` | `RGBA16_FLOAT` | 省メモリフォールバック |
| `PipelineFormat` | `PipelineFormatF32` | デフォルト |
| `LinearColorFormat` | `PipelineFormat` | float 線形 |
| `MainRTVFormat` | `RGBA8_UNORM_SRGB` | スワップチェーン/即時描画 RTV |

`RenderPipeline::createTextures`（`ArtifactRenderLayerPipeline.cppm:1040-1123`）:

| バッファ | フォーマット | バインド |
|---|---|---|
| `accum` | `format`（= RGBA32F） | RT + SRV + UAV |
| `temp` | `format`（= RGBA32F） | RT + SRV + UAV |
| `layer` | `MainRTVFormat`（RGBA8 sRGB） | RT + SRV |
| `layerFloat` | `format`（= RGBA32F） | RT + SRV + UAV |
| `emission` | `format`（= RGBA32F） | RT + SRV |
| `normal` | `format`（= RGBA32F） | RT + SRV |
| `velocity` | RGBA16F | RT + SRV |
| `objectId` / `materialId` | RGBA16F | RT + SRV |
| `albedo` | `format`（= RGBA32F） | RT + SRV |
| `matteSource` | RGBA8_UNORM | SRV |
| `screenSpaceGI` | RGBA16F | SRV + UAV |
| 深度 | D32_FLOAT | DSV |

---

## 弱点（深掘り）

### 弱点 1: レイヤー入口が 8bit sRGB 固定（最重要）

- `layer`（レイヤー単体ラスタライズ先）は `RenderConfig::MainRTVFormat` = **RGBA8_UNORM_SRGB**（`ArtifactRenderLayerPipeline.cppm:1055`）。
- 合成前に `layerFloat` へ昇格する経路がある（`ArtifactCompositionRenderController.cppm:11032`、`11427`、`convertedBackgroundToFloat ? layerFloatSRV : layerSRV`）。
- **しかしこの昇格は「8bit 量子化済みの値を float で再表現」するだけ**。HDR（>1.0）や float 精度を持つソース（EXR/HDR フッテージ、float ラスタ、高精度グレーディング）は、`layerRTV` へ書く時点で **8bit sRGB にクランプ・量子化** され、精度は回復しない。
- つまり合成本線は float でも、**入口で既に 8bit に潰れている**。これが最大の非対称。

**影響**: HDR フッテージを 8bit で読み込み直して合成する、float 精度のエフェクトをレイヤー単体に適用する、といったケースで精度劣化。

**改善案**: `layer` のフォーマットを `PipelineFormat`（RGBA32F）に統一し、最終表示だけ sRGB に落とす。あるいは「float ソース/エフェクトありのレイヤーだけ layerFloat に直接ラスタ」する分岐を追加。

### 弱点 2: premultiplied alpha × sRGB の変換整合性（潜在バグ）

- `MainRTVColor` = `encodedSrgbRgba8Premultiplied()`、`PipelineColor` = `canonicalLinearPremultiplied()`（`RenderConfig.ixx:25-28`）。
- `layer`（sRGB premultiplied 8bit）→ `layerFloat`（linear premultiplied float）への変換があるが、**premultiplied alpha を sRGB 空間で乗算した値をそのまま linear にデコードすると、エッジで色ずれ（ハロー/黒縁）が生じる**。
- 正しくは「unpremultiply → sRGB decode → linear で premultiply」の順が必要。現状この順序が守られているか未確認（**要検証**）。

**影響**: 半透明エッジ・アンチエイリアス境界・フェザーマスク周辺で、合成結果の色が僅かに濁る/ずれる。

**改善案**: layer→layerFloat 変換シェーダで unpremultiply→decode→premultiply を明示。または合成を全て linear 非 premultiplied で行う設計に統一。

### 弱点 3: HDR 表示が実質 SDR 固定

- `hdrDisplayEnabled()` / `setHDRDisplayEnabled()` は `std::atomic_bool` フラグとして存在（`RenderConfig.ixx:14-15`）。
- しかし `MainRTVFormat` は **constexpr `RGBA8_UNORM_SRGB` 固定**（`:17`）で、`hdrDisplayEnabled` がこれを変える経路が存在しない。
- スワップチェーンを `RGBA16F` / `R11G11B10F` / `RGB10_A2` に切り替える実体も grep で見つからない。
- → **HDR 表示はフラグのみで、バッファは SDR 8bit 固定**。OCIO/HDR 色変換（PQ/HLG、ACES）は実装済みだが、表示側が 8bit sRGB なので HDR を真正面から出せない。

**影響**: HDR モニタ・HDR 出力ワークフローで、合成は float でも表示が SDR に潰れる。

**改善案**: HDR 表示時はスワップチェーンフォーマットを float/10bit に切り替え、`MainRTVFormat` を動的化（constexpr → 実行時解決）。ただし Diligent スワップチェーン再生成と OS HDR 設定連携が必要。

### 弱点 4: マットと CPU パスが 8bit 固定

- `matteSource` は RGBA8_UNORM（`ArtifactRenderLayerPipeline.cppm:1072`）。luma/alpha 計算の正確性を優先した意図はあるが、**float マット / HDR マットの精度が出ない**。
- CPU 側は `ImageF32x4_RGBA` が float 対応（`setFromRGBA32F` / `rgba32fData` あり、`ImageF32x4_RGBA.cppm:463-501`）だが、readback / upload の入口は `QImage::Format_RGBA8888` を多用（`ArtifactIRenderer.cppm:95`、`:1773`、`:2377`）。
- → **CPU 合成・ソフトウェアパスは実質 8bit**。GPU は float だが、フォールバックで CPU に落ちると精度が変わる。

**影響**: GPU 未対応のエフェクト・ソフトウェア合成・プレビューで、float 精度が維持されない。

**改善案**: CPU パスの readback を `readbackTextureViewToImageAsync`（RGBA16F/32F 対応済み）経由に寄せ、`ImageF32x4_RGBA` を主表現にする。

### 弱点 5: 深度は十分だが、補助 AOV の精度に非対称

- 深度 D32_FLOAT は十分。
- `velocity` / `objectId` / `materialId` は RGBA16F（`ArtifactRenderLayerPipeline.cppm:1095-1110`）。モーションブラー/オブジェクト ID 用途なら妥当。
- ただし `albedo` / `normal` / `emission` が 32F なのに `velocity` / `objectId` が 16F という非対称は、**将来のデファード/GBuffer 拡張で精度条件が揃っていない**点で注意。

---

## 総評

| 層 | 精度 | 判定 |
|---|---|---|
| 合成本線（accum/temp/layerFloat） | RGBA32F | 十分（AE 32bpc 級） |
| レイヤー入口（layer） | RGBA8 sRGB | **弱点（HDR/float ソースが入口で潰れる）** |
| マット | RGBA8 | 弱点（float/HDR マット不可） |
| 最終表示 / スワップチェーン | RGBA8 sRGB | **弱点（HDR 表示が実質 SDR 固定）** |
| CPU パス | QImage RGBA8888 | 弱点（GPU と精度非対称） |
| 深度 | D32F | 十分 |

**最大の課題は「入口（レイヤーラスタ）と出口（表示）が 8bit sRGB、中間だけ float」というサンドイッチ構造**。HDR ワークフローと float 精度のグレーディングを本気でやるなら、入口の `layer` と出口のスワップチェーンを float/10bit に引き上げる必要がある。加えて premultiplied alpha × sRGB の変換順序（弱点 2）は、見た目に直結する潜在バグなので優先的に検証すべき。

## 主要ファイル

| ファイル | 役割 |
|---|---|
| `Artifact/include/Render/RenderConfig.ixx` | フォーマット定数（MainRTVFormat / PipelineFormat） |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:1040-1123` | 中間バッファ確保 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:10991-11040` | layerRTV ラスタ → layerFloat 昇格 |
| `Artifact/src/Render/ArtifactIRenderer.cppm` | 深度/readback/upload（8bit 入口） |
| `ArtifactCore/src/Image/ImageF32x4_RGBA.cppm` | CPU float バッファ（float 対応済み） |
