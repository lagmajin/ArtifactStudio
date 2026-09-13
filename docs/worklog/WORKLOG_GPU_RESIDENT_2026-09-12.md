**最終更新:** 2026-09-13

# 作業ログ：GPU常駐化（2026-09-12）

## やったこと

描画系effectのGPU常駐化を2経路で進めた。CPU実装はすべて残置（参照・フォールバック用）。

### A. 専用Kind（`GpuSpatialEffectKind` 追加＋ `RenderPipeline` 直書き）

| effect | Kind | 方式 |
|---|---|---|
| Voronoi | `Voronoi` | CPU鏡写しHLSL（新規） |
| Bricks | `Bricks` | CPU鏡写しHLSL（新規） |
| Kaleidoscope（正規品） | `Kaleidoscope` | 旧式 `kKaleidoscopeHlsl` 流用 |
| Lens Distortion | `LensDistortion` | CPU式鏡写しHLSL（bilinear） |
| Chroma Key | `ChromaKey` | YCbCr matte＋despill HLSL |

### B. 汎用Generic（`Kind::Generic`＋共有レジストリ＋汎用PSOキャッシュ）

設計：`docs/analysis/GENERIC_RESIDENT_GPU_DESIGN_2026-09-12.md`

| effect | 方式 | params |
|---|---|---|
| Halftone（正規品） | 既存 `kHalftoneComputeHlsl` 登録 | なし |
| Glitch | 既存 `kGlitchComputeHlsl` 登録 | なし |
| OldTV | 既存 `kOldTVComputeHlsl` 登録 | なし |
| Glow（正規品） | `kGlowResidentHlsl`（cbuffer除去＋g_P*化） | 7 |
| EdgeBloom | `kEdgeBloomResidentHlsl`（同上） | 7 |
| ChromaticGlow | `kChromaticGlowResidentHlsl`（同上） | 6 |
| ReactiveGlow | `kReactiveGlowResidentHlsl`（同上） | 6 |
| LiquidGlow | `kLiquidGlowResidentHlsl`（新規、分離ガウス） | 6 |
| LuminescenceCaustics | `kCausticsResidentHlsl`（新規、Sobel→分離ガウス） | 6 |
| DirectionalGlow | `kDirectionalGlowResidentHlsl`（cbuffer除去＋g_P*化） | 8（上限） |
| VolumetricShine | `kVolumetricShineResidentHlsl`（新規、radial） | 8（上限） |

計16effect。Lens Distortion は既存の9パラメータを固定16-slotノードへ渡し、Chroma Key は12パラメータを専用CSへ渡す。両方とも旧式GPUのstaging readback経路を常駐構成から外した（Chroma Keyのchoke/matte blurは追加alpha面が必要なため従来経路）。

### 契約・基盤の変更

- `Artifact/include/Effects/ArtifactAbstractEffect.ixx`：Kind追加（`Voronoi/Bricks/Kaleidoscope/Halftone/RasterGlow/Generic/LensDistortion/ChromaKey`）、ノードに `genericKey`、16-slot固定パラメータ、`GpuGenericShaderRecord`、FNV-1aキー、レジストリ宣言、`gpuGenericKey()` 仮想。
- `Artifact/src/Effects/ArtifactAbstractEffect.cppm`：レジストリ実体。
- `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`：専用shader×5、汎用prelude＋PSOキャッシュ＋Generic分岐。Lens Distortion と Chroma Key の専用CS、定数バッファ、PSOを追加し、linear-premultiplied RGBAを直接処理する。

## 確定した約束事

- body側に独自 `cbuffer X : register(b0)` 禁止（preludeと衝突）。paramsは `g_P*`、標準は `g_Width/g_Height/g_Time/g_Frame`（時刻は0埋めTODO）。
- コスト超過・表現不可は `appendGpuSpatialNodes` で `false`（LiquidGlow radius>8、DirectionalGlow Custom等）。
- C++ `(int)` 切捨て＝HLSL `int()`（共に0方向丸め）。borderは基本clamp、Sobel系のみreflect101。

## 対象外判定

- `ResidualGlow`：フレーム履歴の状態持ち。単一フレーム常駐の対象外。
- `PhysicalHalation`：coreの2層拡散が同一バッファで互いを消す（既定でno-op化）。core修正の判断待ち。→ `Insight.md` 記録済み。
- `OpticalGlow`：9params超過＋巨大ブラー＋overscan。ペイロード拡張時に再検討。
- `Rasterizer::*` 系（Kaleidoscope/Halftone等）：importゼロ・Service未登録のデッドコード。触っていない。

## 未確認（ビルド許可後）

1. ビルド（D3D12/Vulkan両backendのPSOコンパイル）。
2. CPU／旧GPU／常駐の3者画素差分とtolerance確定。
3. 未登録key・コンパイル失敗時のCPUフォールバック動作。
4. `g_Time/g_Frame` 配線、`runCreativeCompute` 一本化（Phase 2）。

## 2026-09-13 — Lens Distortion のGPU常駐化

- **確認できた事実:** 旧 `LensDistortionEffectGPUImpl::applyGPU` は入力／出力textureを毎回作成し、staging textureへcopy後に `Flush()`／`WaitForIdle()`／CPU readbackを行っていた。CPU式は radial quadratic、tangential、invert、zoom、transparent edges、bilinear の単一フレーム処理で、共有常駐ノードに表現できる。
- **対応:** `GpuSpatialEffectKind::LensDistortion` と専用CSを追加し、9パラメータを16-slot固定ノードから定数バッファへ渡す。`LensDistortionEffect` をRasterizer stageへ移し、mix=1・mask/regionなしのGPU raster planで実行可能にした。入力と出力は canonical linear-premultiplied RGBA のまま扱い、透明端はゼロRGBAで出力する。
- **価値／懸念:** composition GPU chain内ではCPU実行、texture再生成、staging readback、GPU待機を避けられる。旧GPU実装は外部／非常駐呼び出し用に残るため、全呼出し経路のreadback除去は未完了。GeometryTransformからRasterizerへのステージ変更は既存のeffect順序に影響し得る。
- **次に確認すること:** ビルド許可後にD3D12/Vulkan双方でCPU／旧GPU／常駐GPUの3者画素差分、透明境界、center/zoom/invert、effect順序を比較する。sRGB ingressがlinearize済みであることも同時に確認する。

## 2026-09-13 — Chroma Keyの常駐化とpremultiplied境界

- **確認できた事実:** Coreの`processChromaKey`は入力RGBをstraightとして処理し、最終alphaだけを更新していた。旧GPU HLSLも同じ前提で、staging readbackと`WaitForIdle()`を含んでいた。
- **対応:** Coreはcanonical premultiplied入力をalphaで明示的にunpremultiplyしてkey/despillし、最終RGBをmatteでpremultiplyして出力するよう更新。専用`ChromaKey` CSは同じYCbCr／despill／view式を常駐実行する。`choke`／`matteBlur`は追加alpha面が必要なため、該当設定では従来GPU／CPU経路へfail-closedする。
- **価値／懸念:** 半透明素材のキー色判定と透明境界の色漏れをcanonical契約に揃えつつ、通常のkey/despill連鎖ではCPU実行・readback・GPU waitを除去できる。Coreの直接keyer呼び出しがstraight入力を前提にしていた場合は見た目が変わるため、互換境界のfixture確認が必要。
- **次に確認すること:** ビルド許可後に半透明green-screen、clip／despill各モード、matte view、choke／blur fallback、D3D12/Vulkan画素差分を確認する。
