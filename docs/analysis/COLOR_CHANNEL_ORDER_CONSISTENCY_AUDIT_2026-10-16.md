# 画像チャンネル順の一貫性監査

**最終更新:** 2026-10-17

### 一括移行の追加確認（2026-10-16）

- YUV420変換、FFmpeg動画／画像連番出力、OpenEXR連番出力でBGRA backingをRGBAとして解釈していた経路を修正した。YUV入力はdescriptorを見て論理RGBを読む。FFmpegはRGBA staging frameの行strideに従いチャンネルを配置し、OIIO連番出力は再利用scratchを使う。
- OFXはRGBAコンポーネントを広告しているため、host ABIのsrc/dstをBGRA backingから明示変換し、プラグイン出力をBGRAへ戻す。scratch vectorはインスタンス内で容量を再利用する。
- OCIO CPU view-transformでBGRAデータをRGBAとして処理する問題を、処理前後のin-place R/B swapで解消した。matrix fallbackもdescriptor channel orderに従う。AutoColorMatcherのraw float APIはBGRAを契約としてRGB統計・適用を行う。
- `FilmRepair::frameLuma()`のraw channel係数もBGRA順へ修正した。
- Coreの共通`Distortion`処理（bilinear/nearest sampling、morph、displacement）をdescriptor-awareにし、FloatRGBAの論理色と実 backing orderを分離した。morph/displacement出力はsource descriptorを保持する。
- Core image processorsのAnamorphic Flare、Broadcast Colors、Edge Echo、Leave Color、Threshold、Luma Keyについて、raw BGRA画素のRGB係数、色設定、出力channel位置を論理色順に修正した。
- 同じraw `float4*`群のEmboss、Halftone、Duotone、Strobe Lightもluma、CMYK separation、論理色設定の入出力をBGRA順へ揃えた。
- Effect CPU fallbackのRadio Waves、Pixel Sort / Pixel Sort Pro、Inner Shadow、Surface FX、Simple Rainに残った色設定・hue演算のRGB/BGRA取り違えを修正した。Core Physical HalationもBGRA lumaと赤拡散channelを修正した。
- Glow CPU出力setterの暗黙RGBA扱いとVolumetric ShineのBGRA輝度／warmth tint順も修正した。
- OIIO画像読み込みはchannel namesからR/G/B/Aを選び、格納順がBGRA等でも論理RGBでCPU backingへ正規化する。名前がない場合は既存の先頭チャンネル順をfallbackとして維持する。
- OIIOの汎用画像書き出しもchannel namesから論理R/G/B/A順へ並べ、出力channel数に合わせたImageBufを経由する。内部順がBGRAでも出力ラベルとの不一致を避け、4ch超の入力を3/4ch出力へ読み込む際のサイズ不一致も避ける。
- GPU画像uploadはBGRA descriptorを直接参照し、RGBA/BGRA swizzle用の全画素scratchを作らない。RGBA descriptorやRGBA target指定はupload境界で拒否し、CPU側の正規化漏れを隠さない。
- Effect外の共有CPU matte経路、viewport exposure compute shader、Render Farm共有バッファ受信もBGRA順を維持するよう修正した。matteのluminance計算は論理RGBAへ展開し、viewport shaderは論理RGBで演算してBGRA値で保存し、共有buffer受信は送信payloadと同じBGRA setterを使う。
- Coreのsingle-pass compute shader生成器はBGRA texture入力をpixel functionへ論理RGBAとして渡し、戻り値をBGRAで保存する。標準brightness/contrast/saturation/grayscale stageのRGB計算をresident値順から独立させた。現在のrepository内にこの公開生成APIのcallerは見つからず、変更後のshader compilation/runtimeは未確認。
- CoreのGPU Histogram（輝度/RGB/clip statistics）、Scope Parade/Vectorscope/Waveform、3D LUT、Edge Echo、GPU Track MatteもBGRA image inputを論理RGB演算へ渡すよう修正した。LUT/Edge Echo outputはBGRA resident textureへ戻し、Track Matteはmatteの色解析だけlogical RGBA化してlayerへのscalar mask適用は成分順を維持する。
- Parametric Compositionの画像JSONは内部backingを`bgra32f` keyで保存し、旧`rgba32f` keyは互換のRGBA入力として読む。Render Farm shared-memory frame packetのmagicをBGRA payload版へ更新し、旧protocol peerとの混在を明示的に拒否する。
- 追加のGPU shader走査で、Inner Shadow resident shaderだけが入力BGRAをraw RGBAとして合成し、結果もswizzleせずresident textureへ保存していた。入力を`.bgra`で論理RGBAへ展開し、出力を`.bgra`でresident BGRAへ戻すよう修正した。CPU実装はBGRA backingの3色成分を順序不問で合成している。
- Paint frame JSONとEffect Preset image JSONは`channel_order: BGRA`を付けて内部backingを保存し、metadataがないlegacy payloadは従来のRGBAとして読む。still image derived cacheはraw BGRA payloadをBGRA setterで復元し、cache versionを上げて旧cacheを再生成させる。
- 追加CPU raw-buffer監査でDeband/Deblockのluma係数、Beauty Studioのskin RGB抽出・warmth出力、TriChromaticShiftのfloat4 componentを論理RGBへ合わせた。
- 追加監査でFilmRepairのOpenCV BGRA分割luma、Cinematic Lens Flareの色加算、Energy Zapのwarmth色軸、Spherizeの赤青分散軸、Spill Killer Proの論理RGB補正、Depth Relight / Atmospheric Depthの深度・法線・霧色をBGRA物理順へ修正した。Creative EffectsのGPU Glitch/Old TV/Halftone shaderもresident BGRA順を明示した。
- Add NoiseとBlur/Gaussian BlurのGPU出力はshaderがBGRA値を書いているのにreadback descriptorをRGBAとしており、赤青をもう一度swapする状態だったためBGRA descriptorへ修正した。Gaussian Blur CPU fallbackもchannelwise処理前後の不要なBGRA↔RGBA swapを除いた。
- Core OpenCV Glowの縦方向glow用4-channel正規化helperは出力をBGRAとして後段処理するのに3ch BGR入力をRGBA化していたため、BGRA helperに改め、BGR/gray入力の変換をBGRAに揃えた。
- Rasterizer Glowのresident GPU shaderはCPU fallbackがBGRA順で計算する一方、GPU入力をraw `.rgb`として輝度評価・保存していたため、入力を論理RGBAへ読み出して出力をBGRAで保存するよう修正した。Film Damage / Film Grunge、Heatwave / Texturize Motion、Reflection Composer、Glowの暖色係数も論理R/Bに結び付けた。
- Core Solid Fill / Gradient Rampのraw `float*` overloadは論理RGBA参照を物理BGRA index 0/1/2へそのまま渡していた。R/B参照を逆にして、色設定が意図した論理色へ適用されるよう揃えた。
- 追加producer/fallback確認でNoise Layerのcolor mapping、Paint Layerの筆色合成、Lens Distortion CPU fallbackに論理RGBAからraw BGRAへの書き込み漏れがあった。R/B書き込みを入れ替え、Lens DistortionのGPU出力順とも揃えた。
- Noise Layerに続く画像producer監査でSand Simulation LayerもFloat32 backingへmaterialのR/G/Bをそのまま書いていた。GPU simulation readback後とCPU fallbackの両方をBGRA mappingへ揃えた。
- Screen Shakeのclamp/mirror CPU経路はlogical `FloatRGBA`をBGRA raw bufferへ直接書き、wrap経路（共有BGRA displacement）と色が異なっていた。論理B/G/Rを物理0/1/2へ格納するよう直した。
- OpenCV software compositorの`Format_RGBA8888`結果はRGBA順で生成されるが、canonical BGRA setterをdescriptorなしで呼んでいた。RGBA descriptorを明示してCPU backingへ正規化するよう修正した。
- Scene-light liftはBGRA backingをRGBA一時Matへswizzle後、BGRA前提のsetterへ渡していた。3色成分へ同一係数をかける順序不変処理なのでraw BGRAを直接更新し、全画面channel swapと一時画像を除いた。
- Drop Shadow / Satin / Stroke CPU effectは色設定を`cv::Vec4f(R,G,B,A)`としてBGRA backingへ書いていた。matteをBGRAの`(B,G,R,A)`で生成し、誤っていた「ImageF32x4_RGBAはRGBA backing」というコメントも修正した。
- `ImageF32x4_RGBA::setColorDescriptor()`がFloat32/BGRA backingを保つようstorage/orderを正規化し、`ImageF16x4`もconstructorとdescriptor setterでFloat16/BGRAを固定した。Filmstripのpreview pixel APIと表示変換のdescriptor不一致もこの不変条件で解消した。
- Video LayerのRGBA32Fフレーム表示変換がRGBAをBGRA前提のOpenCV→QImage helperへ直接渡していた。表示境界でRGBA→BGRAを明示してからhelperへ渡す。
- ArtifactCreativeEffects内のWire Removal / Dust Repair GPU shaderもBGRA resident textureの論理RGB readとBGRA output storeを明示した。前景、clean plate、temporal neighborの色処理を修正し、scalar maskは成分順不変のため維持した。
- これらは静的確認のみ。build/test/runtime/非対称色画素比較は未実施。OFX初回・拡大フレーム時scratch容量拡張は発生し得る。画像入出力、すべてのCPU fallback、render producerと表示の全経路を含む完了判定はまだできない。

**決定:** 2026-10-16のユーザー指示により、CPU Float32 / Float16 image backingとArtifact image-pipeline GPU float値はBGRAへ統一する。GPU resource formatはDiligentのRGBA16F/32Fを使うが、値順はshader contractでBGRAとする。正本は[IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md](../technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md)。

### 一括移行の最新実装状況（2026-10-16）

- `ImageF32x4_RGBA` / `ImageF16x4` のfloat backingとdescriptorをBGRAへ固定し、descriptor更新でもphysical orderは変わらない。論理pixel APIとRGBA入力setterは論理色を保つ。API名に残る`RGBA`は互換名で、生配列順は常にBGRA。
- `makeGpuImageUploadBuffer()`の既定targetとimage cache upload/readbackをBGRAに変更した。BGRA CPU backingは並べ替えずRGBA16F/32F textureへ転送し、cache readbackもBGRA setterへ直接渡す。外部RGBA向けsetterは別の明示境界として残る。
- `Artifact/src/Effects`の58 GPU effect modulesをBGRA値順へ移行した。59 upload callsitesはBGRAを明示し、画像入力readは論理RGBAへ読み替え、画像出力writeはBGRA値として保存し、readbackはBGRA setterを使用する。CPU fallbackの全raw-index演算は未監査。
- Compositionの最終readbackは、accumulator→straight RGBA変換が成功した場合とBGRA accumulatorを直接使うfallbackの場合でchannel orderを保持し、同期capture・非同期capture・RAM previewの各readbackへ明示する。診断previewもlayerRT=RGBA、accumRT=BGRAを指定する。共通renderer readbackはRGBA/BGRAを受け取り、QImageおよびnative RGBA8 buffer境界でRGBAへ正規化する。全renderer呼び出し元を静的検索し、AOVはRGBAを明示した。runtime確認は未実施。
- 高精度GPU image readbackから`ImageF32x4_RGBA`を作る経路もcanonical BGRA setterを使うよう修正した。MultiChannelImageではbeauty/albedoのraw BGRA readbackだけR/Bをlogical RGBAへ正規化してからchannel抽出し、normal/vector/scalar AOVは意味成分の順序を保つ。AOV resource producerとの実データ整合は未検証。
- QImageへ直接readbackするAPIにもsource order overloadを追加し、OffscreenCompositionRenderer、GPU Render Queue、Export PreRenderの既知のBGRA image-pipeline sourceからのreadbackはBGRAを明示した。引数なしAPIはlegacy RGBA境界として維持し、他callerはsource targetの色順を追加確認する。
- アプリ本体と既存conversion suiteから独立起動できる`ArtifactCorePixelChannelOrderStandaloneTest`を追加した。R/G/B/Aが異なるbinary-exact色をRGBA setterから投入し、Float32 raw BGRA配列、論理pixel API、Float16の各half bit pattern、Float16→Float32往復、descriptor setter後の不変条件、GPU uploadのzero-copy BGRA descriptorを個別に検査する。RGBA GPU upload targetは失敗することも検査する。ArtifactCoreだけをリンクする専用実行ファイルとして登録した。テストは作成したが未構成・未ビルド・未実行。
- `LayerBlendComputeShader.ixx`の40 shader literalsと合成accumulatorをBGRA値順に変更した。通常のlayer sourceは既存のRGBA sampling semanticsで読み、BGRA accumulatorへ保存する。viewport/readback boundaryでは専用compute shaderでpremultiplied BGRA accumulatorをstraight RGBAへ書き出す。
- CoreのDuotone / Halftone / ColorCurves compute shaderもBGRA image input/outputへ移行し、CPU fallbackで明確にBGRA raw indexを使っていたluminance・channel-select箇所を修正した（TimeWarp、Temporal Median / Denoise、Optical Flow Blur、Pixel Sort、Displacement、Chromatic Relief、Add Noise、Sharpen、Glow群など）。
- Bevel CPU luminanceとDirectionalGlow / ChromaticGlow GPU luminanceで赤・青係数の誤割当を修正した。これは色順序移行とは別に確認できた赤青差の原因。
- Composition rendererのReveal、FXAA、Chromatic Aberration、Chroma Key shaderを確認し、論理RGB演算とBGRA resident storeを明示した。Sharpenなど順序不変な処理は生BGRAのまま処理する。調整レイヤーのaccumulatorをspriteへ渡す箇所にも明示的なBGRA→straight RGBA presentation boundaryを置いた。
- Offscreen composition exportでBGRA float raw bufferからRGBA8888 QImageへ書き出すchannel mappingを明示した。出力メモリへ4 byte RGBAとして書き、little-endian `QRgb` wordの並びに依存しない。
- 追加監査で見つかった漏れを修正した。Liquid Glow / Luminescence Causticsのresident GPU shader、Deflicker CPU luminance、Radial Shadow / Rim Lightの色加算、Kaleidoscope CPU出力、NCC trackerのOpenCV grayscale変換をBGRA順へ合わせた。
- これらはコード静的確認のみ。未移行CPU raw-index箇所、その他のGPU pointwise/spatial shaderやrenderer間の順序契約、画素一致は残課題であり、build/test/runtime/pixel parityは未実施。したがってアプリ全体の移行完了とはまだ扱わない。

### 実装進捗（2026-10-16・追記）

- BGRAへの全体移行へ方針を変更。GPU float resource名はRGBAのままでも、Artifact image-pipeline値はBGRA順で扱い、CPU↔GPUのchannel reorderをなくす。旧RGBA shader/upload adapterは外部境界または未移行経路の一時互換層として扱い、全shader移行後に不要な変換を除去する。
- ChromaKey / DifferenceKey / LumaKey / IBK のCPU処理へsurface channel orderを渡すようにした。RGBA/BGRAを論理RGBとして計算し、出力は前景画像のbacking orderを保つ。IBKは前景とclean plateのorderを別々に扱い、clean plate生成では入力frameごとのorderを論理RGBAへ合わせてmedianを取る。
- 旧 `ChromaKey::process(ImageF32x4_RGBA&)` もdescriptor-aware処理へ接続した。channel orderがUnknownまたは非対応の場合は処理器が失敗を返し、Effect wrapperは元画像へ戻る。
- `ChromaSpreadGlow` の旧「ImageF32x4_RGBAは常にBGRA」前提を除去した。OpenCV処理前にcanonical BGRAへ変換し、処理後は元descriptorのchannel orderへ戻す。
- これらはコード静的確認のみ。build/test/runtime/pixel parityは未実施。Float32/Float16画像の生成・変換・全CPU/OpenCV effect・renderer・preview/export経路をBGRAへ移行する作業が残る。

### 実装進捗（2026-10-16）

- `makeGpuImageUploadBuffer()` は、storage と RGBA/BGRA order がsurface descriptorで一致しない入力を拒否するようにした。Unknown orderをRGBAとみなしてコピーする曖昧な動作をなくした。
- `ImageF32x4_RGBA::setFromGpuRGBA32F()` は外部RGBA shader/API readback用の明示adapterとして残る。Artifact image-pipelineのBGRA-native GPU surface readbackの正規経路にはしない。
- 上記は一括移行前の記述。現在のimage effectとimage cache uploadはBGRAを直接転送する。未移行shader・外部RGBA API・viewport表示境界は個別に順序を明示する。
- Curves、ColorWheels、LiftGammaGain、ChannelMixer、Levels、Tritone、Colorama、PhotoFilter、SelectiveColor、HueAndSaturation、Grayscale、ChromaticAberration、Sharpen、Brightness、Exposure、ColorBalance、Fill、GradientRamp、Invert、BlackAndWhite、Vibrance、ColorTint、Posterize、Threshold のGPU upload/readbackを共通境界へ移した。
- ColorCorrection CPU effectの論理色処理をdescriptor-aware `ImageSurfaceView`へ移した。Fill / GradientRamp processorにも画素単位の論理RGBA APIを追加。Chromatic Aberration CPUも赤・青チャンネルをdescriptorの論理色として選ぶ。
- WhiteBalance、FindEdges、DirectionalGlow、BevelもBGRA GPU値順へ移行した。FindEdgesのCPU luminanceはtyped viewから論理RGBで計算する。
- Wave、Kaleidoscope、Mosaic、Liquify、LensDistortionもGPU upload/readbackを共通境界へ移した。
- `IMAGE_FORMAT_CONVENTIONS.md` とRender Format Contractを、CPU Float32/Float16 backing順BGRA・Artifact image-pipeline GPU float値順BGRAへ更新した。resource format名とshader値順は区別し、旧true-RGBA CPU canonical記述は明示的にsupersedeした。
- ArtifactのEffect 58 modulesでBGRA upload/readbackとshader値順を移行した。CPU/OpenCV raw-index境界、Effect CPU fallback、全renderer/preview/exportの論理色一致は未完了。build/test/runtime検証は未実施。

## 1. 調査対象・スナップショット

| 項目 | 内容 | 根拠 | 状態 |
|---|---|---|---|
| 対象 | ArtifactStudio / Artifact / ArtifactCore の画像形式、effect、GPU upload/readback 境界 | 下記 Evidence Index | 静的調査 |
| branch / commit | 親 `main` / `3cf5e3ea`、Artifact `main` / `a35f233e`、ArtifactCore `main` / `a9567adc` | `git branch --show-current`, `git rev-parse --short HEAD` | 確認済み |
| dirty state | 親、Artifact、ArtifactCore に既存の追跡対象変更あり | `git status --short --untracked-files=no` | 確認済み・保持 |
| 実行検証 | build / CMake / tests / app runtime は実行していない | この調査の実行履歴 | 未確認 |

## 2. 情報の信頼度モデル

- **採用済み要求・仕様:** ユーザー指示および採用済み設計契約。
- **実装観測事実:** 現在のコードから確認した宣言・分岐・データ受け渡し。
- **検証済み事実:** build、test、runtime、画素比較で実測した結果。本調査にはなし。
- **設計案:** 一貫性を実現するための候補。採用決定ではない。
- **仮説 / 未確認:** 実行条件や対象素材を特定できず、コード静的確認に留まるもの。
- **廃止済み・旧情報:** 現行コードより古い記述、または現在のdescriptorモデルと矛盾する記述。

## 3. 正本・決定根拠

| 優先 | 情報源 | 権限範囲 | 根拠・留意点 |
|---|---|---|---|
| 1 | `docs/technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md` | CPU/GPU image channel-order正本 | CPU backingとArtifact GPU image values BGRA。移行中であり完了を意味しない。 |
| 2 | `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` | render/storage/alpha/color contract | CPU Float32/Float16 backingとArtifact GPU image values BGRA。 |
| 3 | `SurfaceColorDescriptor`, `ImageSurfaceView`, upload API | 実装の型・境界観測 | external surfaceはchannel orderとalpha/transfer metadataを持つ。ImageF32/ImageF16のbackingはBGRA固定。GPU uploadはBGRAのみ受け取り、descriptor mismatchを拒否する。 |
| 4 | `docs/technical/IMAGE_FORMAT_CONVENTIONS.md` | OpenCV/Qt/GPU境界の説明 | Artifact CPU float imagesをBGRAに統一。任意のOpenCV matrixはproducer-defined。 |
| 5 | 各 effect の生ポインタ upload / readback | call site の実装観測 | descriptorを尊重する保証があるとは限らない。コード観測だけで実行時に色が反転すると断定しない。 |

## 4. 不変条件・設計制約・保護機構

| ID | 不変条件 / 制約 | 根拠 | 現状 |
|---|---|---|---|
| INV-001 | CPU内部画像はlinear-premultiplied RGBA32F semantics、物理backing順BGRA。Float16も同じorder。 | 2026-10-16 channel-order decision | 仕様決定済み、実装移行未完了。 |
| INV-002 | Artifact CPU float image backingはBGRA。外部OpenCV/QImage/GPU順序はdescriptorかnamed boundaryで明示し、型だけから推測しない。 | `SurfaceColorDescriptor`, typed `ImageSurfaceView` | 基盤APIあり。default factory/callsiteはRGBA前提が残る。 |
| INV-003 | Artifact GPU image-pipeline float values are BGRA over RGBA16F/32F resources; CPU upload/readback preserves BGRA. | `makeGpuImageUploadBuffer()`, image shader contracts | Effect / cache / LayerBlend paths migrated; other image shader families and parity remain incomplete. |
| INV-004 | CPU effectも入力順序とalpha modelに従う。RGBA前提の処理へBGRAを暗黙に渡さない。 | color effect parity milestone | 一部effectにdescriptor-aware処理あり。全effect監査は未完了。 |
| INV-005 | ホットパスで新たな大容量確保・コピー・画像変換を増やさない。 | `docs/technical/HOT_PATH_RULES.md`、AGENTS.md | canonical BGRAの `makeGpuImageUploadBuffer()` はsourceを直接参照し、order mismatch変換scratchを持たない。 |
| INV-006 | 子リポジトリの編集はユーザーの明示依頼が必要。サブモジュール変更・build/test/CMakeは別指示なしに行わない。 | root `AGENTS.md` | ユーザーの継続指示に基づきArtifact/ArtifactCoreのコードを変更。サブモジュール・build/test/CMakeは未実施。各repoに既存変更あり。 |

## 5. 依存関係・影響範囲

```text
asset / QImage / OpenCV / generated image
  -> ImageF32x4_RGBA + SurfaceColorDescriptor
  -> CPU effect OR GPU effect upload
  -> Artifact image-pipeline GPU float texture (target value order: BGRA)
  -> GPU readback + output descriptor
  -> next effect / renderer / display / export
```

- `ArtifactCore` は `SurfaceColorDescriptor`, `ImageSurfaceView`, typed RGBA/BGRA views、`GpuImageUploadBuffer` を所有する。GPU uploadはBGRA descriptorのsurfaceだけを受け付け、値を変えずtextureへ渡す。
- `ImageF32x4_RGBA::rgba32fData()` は常にBGRA backingを返す。RGBA入力は各setterでBGRAへ正規化され、descriptor setterでもstorage/orderは固定される。`toCanonicalRGBA32FC4()` は外部RGBA API境界であり、Artifact GPU effect uploadは`makeGpuImageUploadBuffer(surfaceView(), Bgra)`を使う。
- `Artifact` はeffect hostと多数のCPU/GPU effect wrapper、`GPUTextureCacheManager` を持つ。effect間readbackが次段のdescriptorとして再利用されるため、input uploadだけ直しても経路全体の整合は保証されない。
- 現在 `Artifact/src/Effects` で `rgba32fData()` を参照するファイルは116。`makeGpuImageUploadBuffer()` または `setFromGpuRGBA32F()` を使うeffectファイルは58。GPU用RGBA32Fテクスチャを宣言するファイルは59。これらは監査母集団であり、すべての使用が誤りという意味ではない。
- UI preview、composition render、external renderer、exportも同じ画像型と変換境界を共有するが、本監査では各下流経路の画素一致までは追跡していない。

## 6. 既知の不具合・再発防止知識

| ID | 観測された問題 / リスク | 根拠 | 原因の確度 | 予防 / 回帰確認 |
|---|---|---|---|---|
| REG-001 | GPU effectによって色が変わる、または赤青が入れ替わる可能性。 | Effect内の直接upload候補は共通境界へ移行済み。ImageF32/ImageF16 backingはBGRA固定。 | 直接upload候補は解消したが、CPU/GPU画素一致と全表示経路は未確認。 | 非対称RGBA入口とBGRA setter入口から同値画像を生成し、全effect CPU/GPU両経路・readback後の値を比較。 |
| REG-002 | チャンネル順がeffectごとのローカル実装に分岐している。 | Effectの直接upload候補はBGRA共通境界を利用する。CPU/OpenCVのraw index処理と全renderer境界は追加分類が必要。 | 実装観測事実。 | 共通BGRA upload/readback契約を維持し、論理RGB計算はdescriptor-aware helperまたは明示的swizzleを使う。 |
| REG-003 | legacy説明とdescriptor-aware外部surface実装を混同し、誤った一律swizzleを追加する危険。 | ImageF32/ImageF16は固定BGRA。RGBA sourceは入力setterでBGRAへ正規化し、external `ImageSurfaceView`には個別descriptorが残る。 | 内部画像クラスの順序は固定。外部surface callerの分類と全境界確認は継続。 | raw surface/API boundaryを監査し、内部画像外の順序変換をproducerで明示する。 |
| REG-004 | `setFromCVMat(mat)` の無descriptor overloadはOpenCV BGR/BGRA入力をcanonical BGRAとして解釈する。producerが異なる順序を渡すと誤色になる。 | `ImageF32x4_RGBA.cppm` の無descriptor overloadは `unknownBgra32Float()` を設定。明示descriptor overloadはRGBAをBGRAへ正規化する。 | 規約は実装で確認済み。全callerのproducer分類は未完了。 | 無descriptor setter利用を列挙し、OpenCV標準外の入力には実順を示すdescriptorか名前付きadapterを要求する。 |

## 7. 受入基準・検証マトリクス

| ID | 基準 | 確認方法 | 期待結果 | 現状 |
|---|---|---|---|---|
| ACC-001 | CPU Float32/Float16 backingとArtifact GPU image-pipeline値がBGRAで一致する。 | API/type contract review、descriptor assertion、asymmetric pixel round-trip | CPU↔GPU ingress/egressで並べ替えなしに論理色が保たれ、外部RGBA境界だけ明示変換される。 | Pending。移行未完了。 |
| ACC-002 | 同値のRGBA/BGRA入力が全ての対象effectで同じ論理色を出す。 | 非対称画素のCPU/GPU境界回帰試験 | 赤青反転なし。alphaも一致。 | Pending。build/test未実施。 |
| ACC-003 | effect chainの各readback descriptorが実メモリ順序に一致する。 | upload→shader→readbackを複数effectで追跡 | 次段effectが同じ論理色を受け取る。 | Pending |
| ACC-004 | 画像UI、Composition、preview、exportで色が一致する。 | 同一fixtureの代表経路画素比較 | boundary以外にchannel reorderが発生しない。 | Pending |
| ACC-005 | 修正でホットパスの追加大容量確保・同期readbackを増やさない。 | allocation/profile計測とcode review | bounded buffer再利用。新規確保の理由・頻度を説明できる。 | Pending |
| ACC-006 | 旧プロジェクトと既存保存データで色の読み込み互換性を保つ。 | RGBA/BGRA descriptor付きfixtureのload/save round-trip | descriptorを保つか、定義済みboundaryで正規化される。 | Pending |

## 8. 古い情報・矛盾・暫定対策

1. 2026-10-16以前の `RENDER_FORMAT_CONTRACT` とcolor/alpha milestoneにあるtrue-RGBA CPU backingの記述は superseded。新しい正本は `IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md`。
2. `ImageF32x4_RGBA` / `ImageF16x4` のcanonical backingはBGRAへ変更済みだが、すべての生成・変換・effect経路が実データ順とdescriptorを保つ証拠はない。RGBA入口は明示境界として残り、全callerとraw-index処理の監査が必要。
3. Effect modulesとLayerBlend shader群はBGRA値順へ移行したが、Artifact全体のGPU pointwise/spatial/render shaderまで完了した証拠はない。残る画像shaderは意味型（color、mask、vector、AOV）を確認して個別に移行する。
4. `ColorCorrectionEffectContractTest.cpp` は作業ツリー上で変更済み。作業ツリーに存在するが、その変更の由来・実行結果は未確認であり、受入済み・成功済みの証拠として扱わない。

## 9. 変更候補

| 候補 | 内容 | リスク / 判断点 |
|---|---|---|
| C-1 | `ArtifactCore`のtyped surface APIをGPU image境界として確立し、BGRA-native float upload/readbackを可能にする。 | upload/cacheとLayerBlendの主要境界を変更済み。Composition readbackへのchannel-order taggingを追加した。renderer全体のorder taggingとruntime動作は未確認。 |
| C-2 | `Artifact`のeffect hostとshaderをBGRA値順へ移す。 | Effect 58 modulesとLayerBlend群を変更済み。CPU fallbackとpointwise/spatial/render shadersに残件。 |
| C-3 | CPU effectのchannel-order依存処理をtyped viewまたはdescriptor-aware helperへ移す。 | 既存のCV/OpenCV依存とalpha associationを維持する必要がある。 |
| C-4 | `setFromCVMat(mat)`、raw `float*`、直接texture `sub.pData` の全call siteを分類し、暗黙metadataを段階的に除去する。 | 大規模作業。機械的な一括置換はしない。 |
| C-5 | format conventions文書を現行descriptor実装に合わせて改訂する。 | `docs/technical/IMAGE_FORMAT_CONVENTIONS.md` は今回改訂済み。docs inventoryは自動生成のため手編集しない。 |
| C-6 | effect代表例から段階導入し、RGBA/BGRA同値性、CPU/GPU parity、全chain結果を受入条件にする。 | build/test/runtime実行にはユーザー指示が必要。 |

## 10. 証拠索引

- 採用済み契約: `docs/planned/MILESTONE_COLOR_ALPHA_CONTRACT_UNIFICATION_2026-07-18.md`、`docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`、`docs/technical/HOT_PATH_RULES.md`
- 現状説明（legacy注意）: `docs/technical/IMAGE_FORMAT_CONVENTIONS.md`
- Color correctionの先行修正記録: `docs/planned/MILESTONE_COLOR_PIPELINE_PARITY_HARDENING_2026-08-21.md`
- 型・変換境界: `ArtifactCore/include/Graphics/SurfaceColorContract.ixx`、`ArtifactCore/include/Image/ImageSurfaceView.ixx`、`ArtifactCore/include/Image/GpuImageUpload.ixx`、`ArtifactCore/src/Image/GpuImageUpload.cppm`、`ArtifactCore/include/Image/ImageF32x4_RGBA.ixx` (`setFromGpuRGBA32F`)、`ArtifactCore/src/Image/ImageF32x4_RGBA.cppm`
- renderer upload: `Artifact/src/Render/GPUTextureCacheManager.cppm`
- effect例: `Artifact/src/Effects/Rasterizer/ChromaticAberrationEffect.cppm`、`Artifact/src/Effects/Sharpen/SharpenEffect.cppm`、`Artifact/src/Effects/ColorCorrection/CurvesEffect.cppm`、`Artifact/src/Effects/ColorCorrection/ColorWheelsEffect.cppm`、`Artifact/src/Effects/LiftGammaGainEffect.cppm`
- 画素契約試験候補（未実行）: `tests/Artifact/ColorCorrectionEffectContractTest.cpp`
- 調査コマンド: `rg -l "rgba32fData\\(\\)" Artifact/src/Effects --glob '*.cppm'`、`rg -l "makeGpuImageUploadBuffer" Artifact/src/Effects --glob '*.cppm'`、`rg -l "TEX_FORMAT_RGBA32_FLOAT" Artifact/src/Effects --glob '*.cppm'`
