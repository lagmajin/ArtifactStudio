# Composition / Effect Format Path Memo — 2026-07-13

**Status:** Source-derived current-state memo  
**Scope:** Composition Viewer、layer-local rasterizer effect、mask / track matte、GPU blend、presentation / export boundary

## 1. 結論

今後の正規経路は次に固定する。

```text
asset decode / generator / layer renderer
  -> explicit decode to RGBA32F scene-linear
  -> canonical layer surface (RGBA32F, premultiplied alpha)
  -> guarded unpremult view when an effect formula requires straight RGB
  -> layer mask
  -> ordered layer-local effects
  -> effect-level masks
  -> track matte
  -> blend formula in straight scene-linear RGB
  -> Porter-Duff source-over into RGBA32F premultiplied accum
  -> composition final effects
  -> display tone/transfer conversion OR export encode
```

内部処理途中で `QImage`、`RGBA8_sRGB`、CPU readback / GPU re-uploadを暗黙に挟まない。Qt / OpenCV / encoded mediaとの変換は明示的な境界関数だけで行う。

## 2. 現在の実装経路

### 2.1 GPU blendを使用できるレイヤー

Composition Viewerは、表示中かつactiveなレイヤーにnon-Normal blendがある場合にGPU blend pathを選ぶ。CPU Rasterizer workを含むレイヤーも、CPUで処理したsurfaceをlayer targetへuploadした後、同じGPU blendへ合流する。

2026-07-13の接続修正後は、透明なcomposition backgroundもGPU blend pathの除外理由にしない。Accumをtransparent blackで開始し、checkerboard / Maya gradientはpresentation chromeとしてAccumの外側で描画する。またstandard blendのready判定をoptional track-matte PSOから分離し、device未準備時の遅延初期化を再試行可能にする。

根拠:

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:20731-20767`
- `layerHasCpuRasterizerWork()` はRasterizer effectまたはlayer maskを持つsurface-uploadレイヤーの診断に使う。

現在のGPU resourceは次の構成。

| Resource | 現在のformat | 意味 |
|---|---|---|
| `RenderPipeline.Layer` | `RGBA8_UNORM_SRGB` | graphics layer target |
| `RenderPipeline.LayerFloat` | `RGBA32_FLOAT` | blend前のstraight source |
| `RenderPipeline.Accum` | `RGBA32_FLOAT` | premultiplied accumulator |
| `RenderPipeline.Temp` | `RGBA32_FLOAT` | blend output / ping-pong |
| `RenderPipeline.MatteSource` | `RGBA8_UNORM` | CPU upload matte source |

根拠:

- `Artifact/include/Render/RenderConfig.ixx:12-15`
- `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:327-362`

各レイヤーは次の順で処理される。

```text
Layerへgraphics描画 (RGBA8_sRGB)
  -> convertLayerToFloat
     - sRGB texture samplingでRGBをlinear化
     - premultiplied RGBをalphaで割りstraight化
     - RGBA32F LayerFloatへ格納
  -> track matteをLayerFloatへ適用
  -> selected blend compute
  -> RGBA32F premultiplied Temp
  -> dispatch成功時だけAccum/Temp swap
```

根拠:

- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx:62-99`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:21962-22044`
- `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm:315-448`

### 2.2 Rasterizer effectを含むレイヤー

Rasterizer effectまたはlayer maskがあるレイヤーのeffect処理はCPU側surface pathへ寄るが、non-Normal blend自体は処理済みsurfaceのupload後にGPU compute pathへ合流する。

```text
QImageまたはImageF32x4_RGBA layer surface
  -> ImageF32x4RGBAWithCache
  -> enabled Rasterizer effectsをstack順にapplyConfigured(src, dst)
  -> LayerMask::applyToImage(cv::Mat)
  -> ImageF32x4_RGBA
  -> GPUTextureCacheManager upload
     - internal BGRA floatをtrue RGBAへchannel swap
     - SDR RGBを明示的にsRGB decode
     - `RGBA32_FLOAT` textureとして保持
  -> graphics sprite draw
```

根拠:

- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:477-555`
- `Artifact/src/Render/GPUTextureCacheManager.cppm:47-77`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:20749-20767`

この経路には次の損失がある。

1. `ImageF32x4_RGBA`という名称に反して、現在のCV storageはBGRA。
2. alpha modelとcolor spaceが型から判別できない。
3. effect処理自体はCPUで、GPU effect graphへはまだ統合されていない。

### 2.3 現在のeffect / mask順序

現在の`buildRasterizedSurfaceBuffer()`は次の順へ統一する。

```text
base surface -> layer masks -> Rasterizer effects
```

これによりDrop Shadow等はマスク後のalphaを入力として受ける。旧順序は`BUG_RASTERIZER_EFFECT_MASK_PIPELINE_ORDER_2026-07-02.md`に記録されている。

正規順序は原則として次にする。

```text
base surface -> layer masks -> ordered layer effects -> effect-level masks -> track matte -> blend
```

ただし、effectが明示的に「source-before-mask」を要求する場合はstage metadataで分類し、call siteの例外分岐にはしない。

### 2.4 Composition final effect

現在のComposition final Rasterizer effectはviewport imageを`QImage`として受け、OpenCV `CV_32FC4` / `ImageF32x4_RGBA`へ変換してeffect stackを適用し、再び`QImage`へ戻す。

根拠:

- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm:765-815`

これはpresentation後またはreadback後の8-bit imageへeffectを掛ける可能性があり、scene-linear合成のfinal effectとしては正規経路ではない。final effectはRGBA32F accum上で、display conversionより前に実行する必要がある。

## 3. Effect APIが守るformat契約

### 3.1 Storage contract

effect hostがeffect間で保持するsurfaceは次に固定する。

| Property | Required |
|---|---|
| Format | RGBA32F |
| Channel order | true RGBA |
| Color space | scene-linear |
| Stored alpha | premultiplied |
| Alpha range | `[0,1]` |
| RGB range | HDRなら`>1`可 |
| Exceptional values | NaN / Inf禁止 |

### 3.2 Formula contract

色補正、Multiply系演算、HSL、keying等、straight RGBを必要とするeffectはhost adapterで次を行う。

```text
canonical premult surface
  -> epsilon-guarded unpremultiply
  -> effect formula in straight scene-linear RGB
  -> effect mask / mix
  -> premultiply
  -> canonical surface
```

alphaがほぼ0ならhidden RGBを0へ落とす。effect実装内でBGRA/RGBA、sRGB/linear、straight/premultを推測しない。

### 3.3 Effect stage order

`EffectPipelineStage`の現在のenumは以下。

```text
PreProcess -> Generator -> GeometryTransform -> MaterialRender
-> Rasterizer -> LayerTransform
```

根拠: `Artifact/include/Effects/ArtifactAbstractEffect.ixx:63-69`

ただし現在のComposition Viewerが実際に画像effectとして順序実行しているのは主に`Rasterizer`である。ほかのstageは「enumがある」ことと「composition pathに接続済み」を同一視しない。

## 4. 目標GPU経路

長期的には`RenderPipeline.Layer`も`RGBA32_FLOAT`へ移し、graphics PSOをfloat target対応にする。

```text
Layer graphics/generator output: RGBA32F linear premult
  -> layer mask compute
  -> GPU effect graph (RGBA32F ping-pong)
  -> effect mask compute
  -> track matte compute
  -> blend compute (straight view for B(Cb,Cs))
  -> RGBA32F premult accum
```

移行中も`RGBA8_sRGB -> RGBA32F`変換passは許容するが、変換失敗時に旧SRVをblendへ渡してはいけない。requested mode失敗時にNormalやdirect spriteへ置換もしない。

## 5. CPU reference / fallbackの正しい形

CPU fallbackはGPUと同じ結果を返すreference compositorとして維持する。

```text
true RGBA float linear premult buffer
  -> same mask/effect/matte order
  -> same blend formula / opacity rule
  -> same clamp/HDR policy
```

`QImage` fallbackはUI表示・decode/encode境界だけに限定する。CPU referenceが必要でも、内部containerはtrue RGBAのfloat imageを使う。

## 6. 現在の主要ギャップ（優先順）

1. **P0:** `ImageF32x4_RGBA`の実storageがBGRAで、型名と契約が不一致。
2. **P1:** Composition final effectがQImage readback系で実行される。
3. **P1:** effect-level mask metadataはあるが、正規host合成への完全接続を確認できない。
4. **P1:** `ComputeMode::GPU/AUTO`と`supportsGPU()`は存在するが、全effectがRGBA32F GPU graphへ統合されているわけではない。
5. **P2:** Matte sourceは`RGBA8_UNORM`で、luma color-space policyを別途固定する必要がある。

## 7. 実装時の禁止事項

- 新しいeffect中間に`QImage`を追加しない。
- Qt `CompositionMode` / `QPainter`でblendやfallback合成を実装しない。
- effect内部でBGRA/RGBA swapを行わない。変換はboundary ownerだけが行う。
- `RGBA32F`から`RGBA8_sRGB`へ一時降格して再uploadしない。
- alpha representationを関数名・型・metadataなしで切り替えない。
- GPU effect失敗をNormal blendやdirect sprite成功として扱わない。

## 8. 参照

- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`
- `docs/technical/GPU_COMPOSITING_SPECIFICATION_2026-07-01.md`
- `docs/technical/IMAGE_FORMAT_CONVENTIONS.md`
- `docs/technical/MATTE_BLEND_COMPOSITING_ORDER_2026-07-03.md`
- `docs/bugs/BUG_RASTERIZER_EFFECT_MASK_PIPELINE_ORDER_2026-07-02.md`
- `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm`
- `Artifact/src/Render/GPUTextureCacheManager.cppm`
- `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`
- `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm`
