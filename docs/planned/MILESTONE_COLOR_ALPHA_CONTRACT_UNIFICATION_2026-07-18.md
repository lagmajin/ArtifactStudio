# Milestone: Repository-Wide Color / Alpha Contract Unification

**ステータス:** In Progress
**日付:** 2026-07-18
**対象:** `ArtifactStudio`, `Artifact`, `ArtifactCore`, `ArtifactRenderer`

## 目的

色とアルファの意味を call site の暗黙知から分離し、入力、生成、effect、mask / matte、
blend、preview、export の全経路を同じ契約へ統一する。

このマイルストーンの正規内部表現は次のとおり。

| 項目 | 正規値 |
|---|---|
| channel order | true RGBA |
| storage | `RGBA32F`（将来の `RGBA16F` は型付き互換surfaceとしてのみ許可） |
| working color | project working primaries + scene-linear transfer |
| 既定working primaries | Linear sRGB / Rec.709, D65 |
| alpha | premultiplied |
| alpha range | `[0, 1]` |
| RGB range | finite。HDR処理中は `1` 超を許可 |
| transparent pixel | alphaがepsilon以下ならRGBも0 |

`sRGB`、`QImage`、OpenCV BGR/BGRA、straight alpha、encoded media は境界表現であり、
内部合成標準にはしない。

## 背景と現状

既存の次の文書は、上記の最終方向をすでに定義している。

- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`
- `docs/technical/GPU_COMPOSITING_SPECIFICATION_2026-07-01.md`
- `docs/analysis/COMPOSITION_EFFECT_FORMAT_PATH_MEMO_2026-07-13.md`

ただし実装には次の不一致が残る。

1. `ImageF32x4_RGBA` の実メモリが多くの経路でBGRAとして扱われている。
2. `FloatColor` だけでは primaries、transfer、alpha modelを判別できない。
3. `QImage::Format_ARGB32_Premultiplied` と `CV_32FC4` の往復がeffect hot pathに残る。
4. CPU effectがencoded RGB / straight alphaを暗黙に前提とする箇所がある。
5. graphics layer targetは`RGBA8_UNORM_SRGB`、accum / tempは`RGBA32F`である。
6. primitive、gradient、sprite、effect surfaceでdecode / premultiplyの担当が一致しない。
7. preview、RAM preview、software render、external renderer、exportが同じ変換を共有していない。
8. sRGB transfer関数のローカル実装が複数存在する。

2026-07-18時点の機械的な参考値:

| パターン | 件数 | ファイル数 |
|---|---:|---:|
| `ImageF32x4_RGBA` | 656 | 189 |
| `setFromCVMat()` | 88 | 72 |
| `toQImage()` | 82 | 37 |
| `Format_ARGB32_Premultiplied` | 92 | 31 |
| sRGB decode helper / local implementation | 34 | 7 |
| sRGB encode helper / local implementation | 50 | 9 |

これらはすべてが違反という意味ではない。境界利用と内部利用を分類するための監査母集団である。

## 用語と型の契約

### CanonicalSurface

内部のlayer / effect / compositor surfaceは、少なくとも次のdescriptorを持つ。

```text
PixelStorage      = RGBA32F
ChannelOrder      = RGBA
ColorPrimaries    = project working primaries
TransferFunction  = Linear
AlphaMode         = Premultiplied
Range             = SceneReferred
```

生の`cv::Mat`、`QImage`、`void*`、format情報のないSRVをcanonical surfaceとして渡さない。

### Color value

汎用の`FloatColor`へ異なる意味を詰め込まない。

- UI picker / serialized 8-bit color: `EncodedSrgbStraightColor`
- 内部計算色: `LinearWorkingStraightColor`
- 合成済みpixel: `LinearWorkingPremulColor`
- mask / opacity: colorではなくlinear coverage scalar

互換期間中は物理的に別classへ即時置換せず、API名とadapterで意味を固定してよい。
新規APIで意味のない`FloatColor`引数を増やさない。

### Alpha

1. external inputはstraight / premultipliedのどちらでもよいがmetadataを必須とする。
2. canonical ingressで一度だけpremultiplyする。
3. effect式がstraight RGBを必要とする場合、hostがguarded unpremultiply / repremultiplyする。
4. opacityはalphaへ一度だけ適用する。RGBへの反映はpremultiply段階だけで行う。
5. mask / matteでcoverageを変更するときはpremultiplied RGBとalphaを同時に変更する。
6. alphaがepsilon以下ならhidden RGBを0にする。

### Color space

1. `scene-linear`だけでなくworking primariesも必ず指定する。
2. 既定はLinear sRGB / Rec.709 D65とする。
3. ACEScg等はproject working spaceとして明示選択された場合だけ使う。
4. input transfer decodeはingressで一度だけ行う。
5. display / output transfer encodeはegressで一度だけ行う。
6. blend、blur、glow、exposure、lightingはscene-linearで実行する。
7. HSL / HSV、legacy artistic blend等でencoded-domain互換が必要な場合はeffect metadataで宣言する。

### Gradient

新規canonical gradientはworking-space linear RGBで補間する。

既存projectの見た目を維持する必要がある場合は、project versionまたはgradient metadataで
`LegacyEncodedSrgbInterpolation`を明示し、CPU / GPU / effect有無で同じ補間方式を使う。
`QGradient`をcanonical generatorとして使用しない。

## 唯一許可する境界

変換は名前付きboundary APIだけに集約する。

1. asset / video decode ingress
2. generator / UI color ingress
3. Qt preview / thumbnail interop
4. OpenCV compatibility adapter
5. GPU resource upload / readback
6. display transform
7. export encode
8. external renderer protocol

推奨する責務名:

```text
decodeAssetToCanonical(...)
decodeUiSrgbColorToWorking(...)
wrapLegacyBgraForEffect(...)
convertLegacyBgraToCanonical(...)
makeStraightEffectView(...)
restorePremultipliedCanonical(...)
transformCanonicalForDisplay(...)
encodeCanonicalForOutput(...)
```

`convertToFloat()`、`toImage()`、`upload()`のように色・alpha変換の有無が分からない名前は
新規境界APIでは使用しない。

## 所有権

| 責務 | 所有先 |
|---|---|
| surface descriptor / alpha・transfer enum | `ArtifactCore` |
| transfer / primaries変換の唯一実装 | `ArtifactCore` color modules |
| canonical CPU image | `ArtifactCore` image modules |
| effect host adapter | `Artifact` effect pipeline |
| layer target / GPU resource format | `Artifact` renderer bridge |
| compute blend式 | `ArtifactCore` graphics pipeline |
| display transform | `Artifact` presentation path |
| export変換 | `Artifact` render queue / `ArtifactRenderer` |
| Qt / OpenCV compatibility | 明示adapter。core hot pathの所有物にしない |

`libs/DiligentEngine`はこの契約のownerではない。application側resource / shader / bindingで
解決できないことを確認するまで変更しない。

## 移行フェーズ

### Phase 0: 契約凍結と再現fixture

1. 本文書をrepository-wide migration authorityとする。
2. 現在のgradient planeへno-op effectを追加してもpixelが変わらないfixtureを固定する。
3. primary / grayscale / alpha ramp / transparent edge / HDR値のfixtureを固定する。
4. preview、GPU blend、CPU reference、export、external rendererの観測点を揃える。
5. format / transfer / alphaが不明なboundaryを診断ログへ出す。

報告されたgradient問題の完了条件:

- effectなし / no-op effectありの差が規定誤差以内。
- endpoint、midpoint、透明endpointでCPU / GPUが一致。
- layer opacity `0`, `0.5`, `1`で色相が変化しない。

### Phase 1: 型付きmetadataと変換API

1. `SurfaceColorDescriptor`相当を`ArtifactCore`へ定義する。
2. transfer関数を`ColorTransferFunction`へ集約し、ローカルpow実装を廃止する。
3. boundary conversionを一箇所へ集約する。
4. debug buildで不正なdescriptor組合せ、NaN / Inf、alpha範囲を検査する。
5. cache keyへformat / primaries / transfer / alpha modelを含める。

この段階では`ImageF32x4_RGBA`の物理layoutをまだ変更しない。

### Phase 2: Canonical CPU image

1. true RGBAのcanonical float imageを導入する。
2. legacy BGRA-backed `ImageF32x4_RGBA`は明示compatibility adapter経由に限定する。
3. `setFromCVMat()`へchannel / transfer / alpha metadataなしで入る呼び出しを廃止する。
4. effect hostをcanonical premultiplied surfaceへ切り替える。
5. OpenCV effectはstraight viewまたはBGRA viewをadapterから受け、結果をcanonicalへ戻す。

既存classのlayoutを一度に反転しない。producer / consumerの組をmigration tableで移し、
二重channel swapを防ぐ。

### Phase 3: Gradient / primitive / generated layer統一

1. UI / serialized colorをrenderer ingressでworking linearへ変換する。
2. solid、gradient、shape、text、particle、generatorを同じcolor-value契約へ寄せる。
3. gradient CPU fallbackとGPU shaderで同じ補間規則を共有する。
4. opacityのRGB / alpha二重適用を禁止する。
5. generated layerをeffectの有無で別の色経路へ切り替えない。

### Phase 4: GPU layer / effect surface統一

1. graphics layer targetを`RGBA32F linear premultiplied`へ移す。
2. 関連graphics PSOをfloat target対応にする。
3. `LayerFloat`変換passは移行完了後に削除する。
4. mask、matte、effect ping-pong、accum、tempを同じdescriptorへ揃える。
5. GPU失敗時はcontract-equivalent CPU referenceを使うかfail-closedにする。

### Phase 5: Preview / composition / final effect統一

1. Composition ViewerとPreview Pipelineの重複effect pathを共通hostへ寄せる。
2. composition final effectをdisplay変換前のfloat accum上へ移す。
3. RAM preview cacheはcanonicalまたはdescriptor付きencoded cacheとして保存する。
4. checkerboard、Maya gradient、selection overlayはpresentation chromeとしてaccumへ混ぜない。

### Phase 6: Import / display / export / external renderer

1. OIIO / video decodeでinput metadataを保持する。
2. metadata不明inputの既定解釈を一箇所で決め、ログへ出す。
3. display transformをviewport presentationへ一度だけ適用する。
4. exportはcodecのstraight / premult要件へ明示変換する。
5. `ArtifactRenderer` job schemaへworking / output colorとalpha metadataを含める。
6. preview / export / external rendererのpixel parityを確認する。

### Phase 7: 強制とcleanup

1. hot pathの`QImage` / implicit `cv::Mat`変換を機械検査する。
2. local sRGB helper、ad hoc channel swap、意味不明な`FloatColor`境界を禁止する。
3. legacy adapterの利用箇所をallowlist化する。
4. `IMAGE_FORMAT_CONVENTIONS.md`をlegacy boundary referenceとして更新する。
5. migration完了後に古いcompatibility pathを削除する。

## 互換性方針

色の正規化は既存projectの見た目を変える可能性があるため、保存形式にpipeline versionを持たせる。

```text
colorPipelineVersion = 1  // legacy implicit sRGB/BGRA behavior
colorPipelineVersion = 2  // typed canonical contract
```

旧projectは読み込み時にlegacy metadataを明示化する。黙って新しい補間・transferへ再解釈しない。
新規projectはversion 2を既定とする。

## 検証マトリクス

最低fixture:

1. red / green / blue / white / gray patch
2. sRGB 0.5とlinear 0.5を区別できるpatch
3. alpha `0`, epsilon近傍, `0.5`, `1`
4. transparent edgeにhidden RGBがある入力
5. linear / radial / conical gradient
6. gradient + no-op / blur / glow / color correction
7. Normal / Add / Multiply / Screen / HSL blend
8. mask / alpha matte / luma matte
9. HDR `>1`値
10. image / video / text / solid / shape / generated layer

比較対象:

- direct GPU vs effect path
- GPU vs CPU reference
- Composition Viewer vs Preview
- preview vs export
- in-process renderer vs `ArtifactRenderer`

許容誤差は経路ごとに明示する。8-bit encoded boundaryを含む比較とfloat内部比較を同じ閾値にしない。

## 禁止事項

- 新規内部surfaceを`QImage`で表現しない。
- 新規合成を`QPainter` / Qt CompositionModeで実装しない。
- format情報なしで`cv::Mat`をcanonical APIへ渡さない。
- effect内でBGRA/RGBAを推測しない。
- sRGB encode / decodeをローカル実装しない。
- opacityをRGBとalphaへ別々に重複適用しない。
- display変換済みpixelをcompositionへ戻さない。
- failure fallbackで異なるalpha / color契約の経路へ黙って切り替えない。
- 子repo、Diligent fork、全effectを一括変更しない。

## Definition of Done

1. すべてのrender / effect surfaceがdescriptorを持つ。
2. true RGBA / scene-linear / premultipliedが内部で一貫する。
3. transfer、channel reorder、premultiplyは名前付きboundaryだけで発生する。
4. gradientへeffectを追加してもno-opなら色が変わらない。
5. CPU / GPU / preview / export / external rendererが検証誤差内で一致する。
6. legacy projectはversioned compatibilityで見た目を維持する。
7. debug diagnosticsがformat / color / alpha違反を特定できる。
8. hot pathから不要な`QImage`往復と8-bit中間surfaceがなくなる。

## 関連文書

- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`
- `docs/technical/GPU_COMPOSITING_SPECIFICATION_2026-07-01.md`
- `docs/technical/IMAGE_FORMAT_CONVENTIONS.md`
- `docs/analysis/COMPOSITION_EFFECT_FORMAT_PATH_MEMO_2026-07-13.md`
- `Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md`
- `docs/planned/COLOR_SYSTEM_ANALYSIS_2026-04-17.md`

## 今回の作業範囲

2026-07-18の初期実装:

1. `Graphics.SurfaceColorContract`を追加し、storage、channel order、primaries、
   transfer、alpha mode、rangeを一つのdescriptorへ集約した。
2. `ImageF32x4_RGBA`へdescriptor保持と明示入力overloadを追加した。
3. `RenderConfig`のmain RTVとfloat pipelineへdescriptorを付与した。
4. gradient shaderでsRGB入力をlinearへ一度だけdecodeするよう修正した。
5. gradient opacityをRGBへ事前乗算せず、alphaへ一度だけ適用するよう修正した。
6. `Image.UploadConversion`を追加し、descriptor駆動のchannel reorder、
   guarded unpremultiply、transfer decode / encodeを一箇所へ集約した。
7. GPU texture cacheを`RGBA32F linear straight` uploadへ統一した。
8. PrimitiveRenderer2Dのfloat image uploadを`RGBA8 sRGB straight`へ統一した。
9. texture cache keyへsurface descriptorを含めた。
10. raw pixel変換を`Image.SurfacePixelConversion`へ分離し、GPU uploadと
    `ImageF32x4_RGBA::toQImage()`で共有した。
11. QImage loader fallbackをBGRA / sRGB / premultiplied境界として明示した。
12. project rootへ`colorPipelineVersion`を保存し、新規projectはversion 2、項目のない
    旧projectはversion 1として復元するようにした。未知のversionはfail-closedにした。
13. 共通pixel変換の契約fixtureへsRGB decode、RGBA/BGRA reorder、premultiplied
    unpremultiply、透明画素のhidden RGB除去を固定した。
14. straight-alpha入力でもalphaがepsilon以下ならRGBを0へ正規化するようにした。
15. `colorPipelineVersion`をprojectから各compositionへ伝播し、composition snapshotにも
    保存するようにした。旧snapshotはversion 1、新規compositionはversion 2を既定とする。
16. layer surface cache keyへ`colorPipelineVersion`を含め、異なるpipeline世代のsurfaceを
    再利用しないようにした。
17. Solid2D / SolidImageのgradient cache keyへfill type、両endpoint、angle、reverse、
    center、scale、offsetを含め、gradient編集後の古いeffect surface再利用を防止した。
18. version 1をencoded-sRGB補間、version 2をlinear-light補間として、Solid2D / SolidImageの
    CPU effect入力とGPU直描画へ同時に接続した。
19. CPU gradient生成を共有関数へ集約し、linear / radial / conical / repeat / mirrorの
    座標式をGPU shaderと揃えた。対象effect経路ではQt gradient painterを使用しない。
20. GPU側でrepeat / mirrorがconical分岐へ落ちていた判定を修正した。
21. version 2の透明endpointは補間前にhidden RGBを0へ正規化した。
22. built-in test runnerへversion別CPU gradient midpointと透明endpointの契約fixtureを追加した。
23. identity effectを通したgradient pixelsとsurface descriptorが完全一致するno-op fixtureを
    built-in test runnerへ追加した。
24. QImage→OpenCV境界のformat判定を`qImageCvMatSurfaceDescriptor()`へ集約し、RGBA8888を
    含むfallback変換をBGRA / sRGB / straightとして明示した。
25. composition final effect、matte再入場、LOD downsampleでsurface descriptorを保持した。
26. RGB888 / grayscale等をeffectへ渡す前に`normalizeQImageForCvEffectBoundary()`で
    ARGB32へ明示正規化し、OpenCVの1/3/4 channel分岐をeffect hostから排除した。
27. 静止画とQImage動画フレームのlayer入口をARGB32 premultipliedへ明示正規化し、
    `BGRA / sRGB / premultiplied` descriptorを付与した。
28. effect hostは完全指定された入力descriptorを出力へ必ず再確定し、CPU強制実行も
    同じ契約入口を通すよう統一した。
29. 誤った完全指定descriptorを返す擬似effectをfixtureへ加え、hostが入力契約へ戻しつつ
    画素を不変に保つことを固定した。
30. Text layerの2つのARGB32 premultiplied raster入口へ
    `BGRA / sRGB / premultiplied` descriptorを付与した。
31. CPU動画frameのRGB24 / RGBA8 / BGRA8 / RGBA32F入口へ、推測可能なstorage、
    channel order、alpha modeだけを部分descriptorとして明示した。raw FFmpeg metadataからの
    transfer / primaries推測は行っていない。
32. FFmpegのRGB変換後frame metadataをRGB / full rangeへ正規化し、元frameの
    primaries / transferを保持した。hardware downloadは共通変換へ集約し、direct GPU frameの
    color metadata欠落も補完した。
33. 今回触れた2つのdecoder実装でFFmpeg includeをglobal module fragmentへ移し、
    module purview内includeを解消した。
34. blur / distortion / keying系のCPU producerと、入力imageが同一scopeにあるGPU readbackの
    計12箇所で入力surface descriptorの明示継承を追加した。
35. color correction / dithering / kaleidoscope / kuwaharaのGPU readback成功後に、
    入力surface descriptorを明示再付与した。
36. glow系8effectのCPU producerとGPU readbackへ入力surface descriptorの明示継承を追加した。
37. rasterizer系10effectのCPU producerとGPU readbackへ入力surface descriptorの明示継承を追加した。
38. noise / edge / wipe / shadow / blur / bevel / satin / liquify / rain等の直接producer
    14箇所へ入力surface descriptorの明示継承を追加した。
39. colorama / photo filter / selective color / tritone / mosaic / sharpenのGPU readback helperへ
    descriptor引数を追加し、pixel取り込み時点で契約を確定した。

未実施:

- 既存189ファイルの`ImageF32x4_RGBA` producer / consumer分類
- effect hostのcanonical surface移行
- GPU layer targetのRGBA32F化
- gradient + no-op effectのGPU readback parityと実画面での実測
- EXR / codec export時のworking primaries・alpha mode指定

submodule gitlink、build設定は変更していない。ビルド、テスト、CMakeも実行していない。
