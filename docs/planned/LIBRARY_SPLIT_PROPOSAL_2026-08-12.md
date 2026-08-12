# ライブラリ分割提案（第2段階）

**最終更新:** 2026-08-13
**ステータス:** In Progress（P1/P2/P3/P4/P5/P6/P7/P8/P9/P10/P11/P12/P13/P14/P15/P16/P17/P18/P19/P20/P21/P22/P23/P24/P25/P26/P27/P28/P29/P30/P31/P32/P33/P34/P35/P36/P37/P38/P39/P40/P41/P42/P43/P44/P45/P46/P47/P48/P49/P50/P51/P52/P53/P54/P55/P56/P57/P58/P59/P60/P61/P62/P63/P64/P65/P66/P67/P68/P69/P70/P71/P72/P73/P74/P75/P76/P77/P78/P79/P80/P81/P82/P83/P84/P85/P86/P87/P88/P89/P90/P91/P92/P93/P94/P95/P96/P97/P98/P99/P100/P101/P102/P103/P104/P105 CMake分割実装済み・ビルド未検証）
**対象:** `Artifact/CMakeLists.txt`、`Artifact/src/Effects/`、`Artifact/src/Render/`、`ArtifactCore/CMakeLists.txt`

## 1. 結論

次に進める分割は、`ArtifactEffects` をエフェクトパック単位へ分ける案を第一候補とする。

`ArtifactRender` は重要な候補だが、現時点では `DiligentImmediateSubmitter` が
`PrimitiveRenderer3D` に依存するなど、デバイス基盤と描画実装の境界がまだ一方向ではない。
先にここを機械的に分けると、C++20 module の依存循環や Diligent のリンク依存を増やすリスクがある。

## 2. 現在の分割状態

`Artifact/CMakeLists.txt` には既に次の target がある。

| Target | 役割 | 依存 | 現状の規模・所見 |
|---|---|---|---|
| `ArtifactAsset` | Asset UI/model integration | `ArtifactCore` | 小規模。直ちに追加分割する価値は低い |
| `ArtifactColor` | OCIO / color-management app layer | `ArtifactCore` | 中規模。単独責務が比較的明確 |
| `ArtifactRenderSupport` | render scheduler / queue / preview support | `ArtifactCore` | `ArtifactRender` とは別責務になっている |
| `ArtifactRender` | Diligent device、shader、submitter、primitive、IRenderer | `ArtifactCore`、Diligent | 約29 source。内部結合が強い |
| `ArtifactEffectContract` | effect base / context / frame sampler | `ArtifactCore`、`ArtifactRender`、`ArtifactRenderSupport` | 分割の安定した土台 |
| `ArtifactEffectsRasterizer` | stateless Rasterizer operators | `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract` | 30 interface / 30 impl。P4で追加 |
| `ArtifactCoreAudio` | Core Audio domain + direct Audio consumers | `ArtifactCore` | 43 module / 30 impl。P5で追加 |
| `ArtifactCoreAI` | Core AI contracts and agents | `ArtifactCore` | 33 module / 6 impl。P6で追加 |
| `ArtifactCoreVideo` | Core Video + direct frame consumers | `ArtifactCore` | 31 module / 20 impl。P7で追加 |
| `ArtifactCoreMedia` | Core Media sources + thumbnail decoder | `ArtifactCore`、`ArtifactCoreVideo` | 16 module / 12 impl。P8で追加 |
| `ArtifactCoreComposition` | Core composition buffers / pre-compose / template contracts | `ArtifactCore`、`ArtifactCoreMedia` | 9 module / 8 impl。P9で追加 |
| `ArtifactCoreUI` | Core input operators / shortcuts / layout contracts | `ArtifactCore` | 19 module / 8 impl。P10で追加 |
| `ArtifactCoreData` | Core data tables / CSV / type inference contracts | `ArtifactCore` | 12 module / 0 impl。P11で追加 |
| `ArtifactCoreCommand` | Core command sessions / actions | `ArtifactCore` | 8 module / 2 impl。P12で追加 |
| `ArtifactCoreAsset` | Core asset files / database / manager | `ArtifactCore`、`ArtifactCoreData` | 12 module / 7 impl。P13で追加 |
| `ArtifactCoreAcoustic` | Acoustic synthesis and telemetry registry | `ArtifactCore` | 8 module / 0 impl。P14で追加 |
| `ArtifactCoreShape` | Shape geometry + vector/text layout facades | `ArtifactCore` | 15 module / 3 impl。P15で追加 |
| `ArtifactCorePlatform` | OS / process / shell utilities | `ArtifactCore` | 6 module / 4 impl。P16で追加 |
| `ArtifactCoreThread` | background task / ticker / thread helpers | `ArtifactCore` | 5 module / 2 impl。P17で追加 |
| `ArtifactCoreAnalyze` | image analysis / optical flow / time remap | `ArtifactCore` | 6 module / 5 impl。P18で追加 |
| `ArtifactCoreTracking` | motion / planar / camera tracking | `ArtifactCore` | 3 module / 2 impl。P19で追加 |
| `ArtifactCoreIPC` | IPC / render-farm shared memory transport | `ArtifactCore` | 3 module / 3 impl。P20で追加 |
| `ArtifactCoreNLE` | OTIO / non-linear editing contracts | `ArtifactCore` | 2 module / 2 impl。P21で追加 |
| `ArtifactCorePlayback` | playback clock / state | `ArtifactCore` | 2 module / 1 impl。P22で追加 |
| `ArtifactCorePreview` | preview quality / settings | `ArtifactCore` | 2 module / 2 impl。P23で追加 |
| `ArtifactCoreExport` | Lottie export contracts | `ArtifactCore` | 3 module / 2 impl。P24で追加 |
| `ArtifactCoreVST3` | VST3 interfaces / loader | `ArtifactCore` | 1 module / 1 impl。P25で追加 |
| `ArtifactCoreLocalization` | locale formatting | `ArtifactCore` | 1 module / 1 impl。P26で追加 |
| `ArtifactCoreCoordinate` | coordinate profiles | `ArtifactCore` | 1 module / 1 impl。P27で追加 |
| `ArtifactCoreEvent` | event bus / event diagnostics | `ArtifactCore` | 3 module / 2 impl。P28で追加 |
| `ArtifactCoreFile` | file metadata / type detection | `ArtifactCore` | 3 module / 2 impl。P29で追加 |
| `ArtifactCorePlugin` | plugin contracts / registry | `ArtifactCore` | 3 module / 2 impl。P30で追加 |
| `ArtifactCoreControl` | MIDI / OSC external control | `ArtifactCore` | 3 module / 2 impl。P31で追加 |
| `ArtifactCoreDatabase` | database contracts / storage | `ArtifactCore` | 2 module / 1 impl。P32で追加 |
| `ArtifactCoreMask` | depth / roto / path / volume masks | `ArtifactCore` | 4 module / 4 impl。P33で追加 |
| `ArtifactCoreConfiguration` | layered configuration / app settings | `ArtifactCore` | 4 module / 3 impl。P34で追加 |
| `ArtifactCoreText` | font / shaping / glyph layout contracts | `ArtifactCore`、`ArtifactCoreShape` | 9 module / 5 impl。P35で追加 |
| `ArtifactCoreGenerate` | test image / starfield generators | `ArtifactCore` | 2 module / 2 impl。P36で追加 |
| `ArtifactCoreSimulation` | OpenVDB / pyro simulation contracts | `ArtifactCore` | 2 module / 2 impl。P37で追加 |
| `ArtifactCoreTrack` | layer track / NCC tracker | `ArtifactCore` | 2 module / 1 impl。P38で追加 |
| `ArtifactCoreSource` | source abstraction | `ArtifactCore` | 1 module / 1 impl。P39で追加 |
| `ArtifactCoreProject` | project metadata / visitor contracts | `ArtifactCore` | 2 module / 0 impl。P40で追加 |
| `ArtifactCoreScene` | scene node / simulation settings | `ArtifactCore`、`ArtifactCoreComposition` | 2 module / 1 impl。P41で追加 |
| `ArtifactCoreRig` | 2D rig contracts | `ArtifactCore`、`ArtifactCoreExport` | 1 module / 1 impl。P42で追加 |
| `ArtifactCoreGrid` | grid system | `ArtifactCore`、`ArtifactCoreConfiguration` | 1 module / 0 impl。P43で追加 |
| `ArtifactCoreColorCollection` | color grading collection | `ArtifactCore` | 1 module / 1 impl。P44で追加 |
| `ArtifactCoreSound` | sound track / type contracts | `ArtifactCore` | 2 module / 0 impl。P45で追加 |
| `ArtifactCoreSequence` | sequence group / step sequencer contracts | `ArtifactCore` | 2 module / 0 impl。P46で追加 |
| `ArtifactCoreMaterial` | material contracts | `ArtifactCore`、`ArtifactCoreScene` | 1 module / 1 impl。P47で追加 |
| `ArtifactCoreEnvironment` | environment variable access | `ArtifactCore` | 1 module / 1 impl。P48で追加 |
| `ArtifactCoreLight` | IES light profile | `ArtifactCore` | 1 module / 1 impl。P49で追加 |
| `ArtifactCoreCrowd` | boids swarm contract | `ArtifactCore` | 1 module / 0 impl。P50で追加 |
| `ArtifactCoreDomain` | layer domain types | `ArtifactCore` | 1 module / 0 impl。P51で追加 |
| `ArtifactCoreFileSystem` | directory builder contract | `ArtifactCore` | 1 module / 0 impl。P52で追加 |
| `ArtifactCoreIcon` | SVG-to-icon contract | `ArtifactCore` | 1 module / 0 impl。P53で追加 |
| `ArtifactCoreVST` | VST host/effect contracts | `ArtifactCore` | 2 module / 0 impl。P54で追加 |
| `ArtifactCoreNetwork` | collaboration websocket / RPC transport | `ArtifactCore`、`ArtifactRender` | 3 module / 3 impl。P55で追加 |
| `ArtifactCoreCollaborate` | collaboration protocol | `ArtifactCore`、`ReactiveEvents` | 1 module / 0 impl。P56で追加 |
| `ArtifactEffectsResidual` | remaining independent effects | `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract` | 5 module / 5 impl。P57で追加 |
| `ArtifactRenderSupportContracts` | render context / ROI / foundation contracts | `ArtifactCore` | 4 module / 0 impl。P58で追加 |
| `ArtifactColorPalette` | color palette persistence / manager | `ArtifactCore` | 1 module / 1 impl。P59で追加 |
| `ArtifactColorNode` | color node / node graph | `ArtifactCore` | 2 module / 2 impl。P60で追加 |
| `ArtifactColorSettings` | color settings contract | `ArtifactCore` | 1 module / 1 impl。P61で追加 |
| `ArtifactColorScience` | color science / LUT manager | `ArtifactCore` | 1 module / 1 impl。P62で追加 |
| `ArtifactColorManagement` | color management helpers | `ArtifactCore` | 1 module / 1 impl。P63で追加 |
| `ArtifactColorGrading` | color grading engine | `ArtifactCore` | 1 module / 1 impl。P64で追加 |
| `ArtifactEffects` | 個別 visual effects | `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract` | CMake上は202 interface / 101 impl。次の主対象 |

`Artifact` 本体はこれらをすべてリンクし、`ArtifactGlobalEffectManager` と OFX host はアプリ側に残している。
この配置は、plugin/service 依存をライブラリへ逆流させない点で妥当であり、現段階で manager を移動しない。

## 3. 第1候補: ArtifactEffects のパック分割

### 根拠

- `Artifact/src/Effects/` には約110個の cppm がある一方、`ArtifactEffects` の CMake file-set は101 cppmに留まり、アプリ側に残る実装もある。
- `Rasterizer` が40 cppm、`ColorCorrection` が16 cppm、`Glow` が9 cppmで、単一 target に偏っている。
- エフェクト間の相互 import は静的検索で確認できず、共通依存は主に次の契約へ収束する。
  - `Artifact.Effect.Abstract`
  - `Artifact.Effect.ImplBase`
  - `Artifact.Effect.Context`
  - `Artifact.Render.DiligentDeviceManager`
  - `Image.ImageF32x4RGBAWithCache`
- したがって、個別エフェクトを複数 target に移しても、責務境界を新規に発明する必要が比較的少ない。

### 提案する target 構成

最初から細かく分けず、次の3パックを目安にする。

```text
ArtifactEffectContract
        ├── ArtifactEffectsColor       (ColorCorrection, LiftGammaGain, WhiteBalance)
        ├── ArtifactEffectsSpatial     (Blur, Glow, Keying, Distort, Generate, etc.)
        ├── ArtifactEffectsTemporal    (Rasterizer の frame/history/time 系)
        └── ArtifactEffectsRasterizer  (残存 Rasterizer の stateless operator)
```

`ArtifactEffectsSpatial` は暫定的な残余パックであり、実装後の include/import 集計でさらに分ける。
`Rasterizer` ディレクトリ全体をそのまま temporal にするのではなく、`Frame*`、`Feedback`、
`Echo`、`Time*`、`Trail*`、`Strobe` など履歴・時間状態を保持するものだけを temporal 候補とする。

### 先行パイロット

最初の変更単位は、既に実装と CMake 登録が揃っている色補正18組だけにする。

- 対象 interface: `ColorCorrection` 配下の次の16組と、トップレベルの `WhiteBalanceEffect` / `LiftGammaGainEffect`
  - `BrightnessEffect`, `ChannelMixerEffect`, `ColoramaEffect`, `ColorBalanceEffect`
  - `ColorWheelsEffect`, `CurvesEffect`, `ExposureEffect`, `FillEffect`
  - `GradientRampEffect`, `GrayscaleEffect`, `HueAndSaturation`, `InvertEffect`
  - `LevelsEffect`, `PhotoFilterEffect`, `SelectiveColorEffect`, `TritoneEffect`
  - `WhiteBalanceEffect`, `LiftGammaGainEffect`
- 対象実装: 上記18組に対応する `Artifact/src/Effects/` の cppm
- 対象外: `AutoExposureEffect`、`HDRDisplayEffect`、`ShadowHighlightEffect` は interface のみで実装がなく、P1へ混ぜない
- 依存: `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract`
- 実装済み: `Artifact/CMakeLists.txt` に `ARTIFACT_EFFECTS_COLOR_MODULES` / `ARTIFACT_EFFECTS_COLOR_IMPL` と `ArtifactEffectsColor` を追加
- 実装済み: 18組を `ArtifactEffects` から除去し、`Artifact` のリンクへ `ArtifactEffectsColor` を追加
- 互換性: `ArtifactEffects` を直ちに削除せず、残りの effects のみを保持する暫定 target とする
- 成功条件: `Artifact` が両方の target をリンクし、既存の effect API と保存形式を変更しない（CMake source/link 静的確認済み、ビルド未検証）

`ArtifactServiceEffect` は `BrightnessEffect`、`ColorWheelsEffect`、`WhiteBalanceEffect`、
`LiftGammaGainEffect`、`InvertEffect` などを直接 import して factory を作る。したがって、
P1ではサービスや manager を移動せず、`Artifact` が `ArtifactEffectsColor` をリンクした状態で
既存の factory import が解決できることを境界条件にする。

このパイロットで確認するのは、機能追加ではなく module file-set の分離、target 間の BMI 参照、
静的ライブラリのリンク取りこぼし、DLL 境界の export 要否である。現在確認できた色補正経路は
静的な直接 factory 参照であり、個別 effect の自動登録テーブルを移動する設計ではない。

## 4. 第2候補: ArtifactRender の二層化

`ArtifactRender` は次の方向で検討する。ただし、先に import graph を整理する。

```text
ArtifactRenderDevice
  Config / DeviceManager / ShaderManager / RenderCommandBuffer / IRenderSubmitter

ArtifactRenderPipeline
  PrimitiveRenderer2D/3D / OffscreenRenderer2D / ArtifactIRenderer
  / FinalPostProcess / MotionBlurPass / GPUTextureCacheManager
```

現状は `PrimitiveRenderer3D` が `PrimitiveRenderer2D` を import し、
`DiligentImmediateSubmitter` が `PrimitiveRenderer3D` を import するため、
submitter を device 側へ単純移動してはいけない。まず submit API と renderer 実装の境界を確認し、
device-only target に renderer 型が漏れないことを証明する必要がある。

## 5. 第3候補: ArtifactCore の domain target 化

`ArtifactCore` は単一 STATIC target で、PUBLIC link に Qt、OpenCV、SDL2、Boost、Vulkan、TBB、
OpenImageIO、OpenColorIO、glm、EnTT、magic_enum などが並ぶ。依存の広さは分割候補の根拠になるが、
現時点で source の domain 境界だけを見て分割すると、module import と PUBLIC include の伝播を誤って切る可能性が高い。

Core 分割は次の順序で行う。

1. 各 domain の interface import と外部ライブラリ利用を静的に表にする。
2. `Utils` / `Time` / `Math` / 基本 `Image` を最小基盤として依存方向を固定する。
3. `Audio`、`Video`、`AI`、`Graphics` のような外部依存が強い domain を別 target 候補にする。
4. Qt Widgets を必要とする Core module を先に特定し、非GUI Core という名前だけで分離しない。

## 6. 実施順と停止条件

| Phase | 作業 | 停止条件 |
|---|---|---|
| P0 | effect の module/import と CMake source 対応表を固定 | 対応漏れ、重複 file-set、effect 間 import が残る |
| P1 | `ArtifactEffectsColor` を追加 | module scan またはリンク境界で欠落が出る |
| P2 | spatial / temporal を順に追加 | 保存・生成 API が特定 pack の実装へ暗黙依存する |
| P3 | `ArtifactRender` の import graph を再監査 | device-only と pipeline の一方向性が証明できない |
| P4 | Core domain target の設計だけを作成 | 外部依存と PUBLIC 伝播の表がない |

P1/P2/P3/P4のCMake分割は実装済みだが、ビルド、CMake再生成、テストはまだ実行していない。
`Artifact/CMakeLists.txt` の静的確認では、Color pack は interface 18 / impl 18、Temporal pack は
interface 14 / impl 14、Spatial pack は interface 34 / impl 34である。対象ファイルの存在、
`ArtifactEffects` からの除去、`Artifact` からのリンク追加、MSVC/Win32定義の追加を確認した。
通常の CMake / build / runtime 検証は、ユーザーの明示許可後に行う。

### P2: ArtifactEffectsTemporal

P2として、履歴・時間依存が明確な14組を `ArtifactEffectsTemporal` へ移した。

- 対象: `Echo`、`Feedback`、`FrameBlend`、`FrameAverage`、`FrameAccumulation`、`FreezeFrame`
- 対象: `TemporalDenoise`、`TemporalMedian`、`TemporalSmear`、`TimeBlur`、`TimeWarp`
- 対象: `TrailFade`、`SlitScan`、`Strobe`
- 対象外: `LightTrails`、`MotionTrail`、`Deflicker`、`FilmDamage`、`PosterizeTime` など、時間名を持つが状態契約の確認が不足するもの

P2も `ArtifactEffectsColor` と同じく、契約・Render target への依存を維持し、`Artifact` から明示的にリンクする。
Rasterizer全体を移動していないため、残りの effect pack は引き続き `ArtifactEffects` に保持される。

### P3: ArtifactEffectsSpatial

P3として、空間的な画像処理で依存が閉じている34組を `ArtifactEffectsSpatial` へ移した。

- Glow: `DirectionalGlow`、`Glow`、`EdgeBloom`、`ChromaticGlow`、`ReactiveGlow`、`LiquidGlow`、`ResidualGlow`、`PhysicalHalation`、`LuminescenceCaustics`
- Keying / Blur: `LumaKey`、`ChromaKey`、`DifferenceKey`、`IBKKeyer`、`AnisotropicFlowBlur`、`ApertureShapeBlur`、`ReactionDiffusionBlur`
- Distort / Generate: `DisplacementMap`、`TurbulentDisplace`、`SimpleRain`、`RadioWaves`、`LensDistortion`、`Wave`、`AddNoise`、`AutoMosaic`
- Other spatial processing: `Kaleidoscope`、`Dithering`、`Kuwahara`、`Bevel`、`LinearWipe`、`Liquify`、`Mosaic`、`Spherize`、`Sharpen`、`FindEdges`

各対象は `ArtifactEffects` の元の file-set に存在すること、対象間に直接の effect module import がないことを静的に確認した。
`ArtifactEffectsSpatial` は他パックと同じく `ArtifactCore`、`ArtifactRender`、`ArtifactEffectContract` に依存する。
この移動後も `ArtifactEffects` には interface 35 / impl 35を残し、複合 effect、OFX host、依存確認が未完了の effect は移さない。

### P4: ArtifactEffectsRasterizer

P4として、Temporal packに含めなかったRasterizer配下の30組を
`ArtifactEffectsRasterizer`へ移した。対象は `Bricks`、`ChromaticAberration`、
`ChromaticRelief`、`DataMosh`、`Deflicker`、`DifferenceMatte`、`DropShadow`、
`Edge`、`FilmDamage`、`Ghost`、`Glitch`、`Glow`、`Halftone`、`HexGrid`、
`InnerShadow`、`Kaleidoscope`、`LightTrails`、`MotionTrail`、`OpticalFlowBlur`、
`PixelSort`、`PosterizeTime`、`RadialBlur`、`Satin`、`ScreenShake`、`Stripes`、
`Stroke`、`VectorBlur`、`VectorFlowGlitch`、`Vignette`、`Voronoi`である。

各インターフェースの直接 import は共通の effect contract / property module に限られ、
別の個別 effect module への直接依存は静的検索で確認されなかった。Temporal packとの重複はなく、
P4後の `ArtifactEffects` 残存は interface 5 / impl 5（`TimeDisplacement`、`Noise`、
`OpticsCompensation`、`RadialShadow`、`SurfaceFX`）となる。

### P5: ArtifactCoreAudio

`ArtifactCore` のAudio domain 43 module / 30 implementationを
`ArtifactCoreAudio`へ分離した。Audio moduleを直接importしていた
`Media.Encoder.FFmpegAudioDecoder` と `Particle.System` も同じpackへ含め、
分離後の`ArtifactCore`本体からAudio packへのmodule import edgeを静的に0件へした。
`ArtifactCoreAudio` は基盤moduleを利用するため `ArtifactCore`へ依存し、Artifact側の各Core依存targetへ
明示的にリンクしている。

### P6: ArtifactCoreAI

AI系33 module / 6 implementationを `ArtifactCoreAI`へ分離した。Core内の非AI moduleから
AI moduleへの直接importは0件で、AI packは `ArtifactCore`への一方向依存となる。
Artifact側ではAI moduleを利用するアプリ本体へ `ArtifactCoreAI`をリンクし、既存のAI APIと
agent実装の所有targetだけを切り離した。

### P7: ArtifactCoreVideo

Video系31 module / 20 implementationを `ArtifactCoreVideo`へ分離した。`Codec.FFmpegVideoDecoder`を含む
Video module群はVideo packへ閉じ込め、Media controller/decoderはP8のMedia packへ整理した。
分離後のCore本体からVideo packへのmodule import edgeを静的に0件へし、Artifact側ではRender targetと
アプリ本体へ明示リンクしている。

### P8: ArtifactCoreMedia

Media系16 module / 12 implementationを `ArtifactCoreMedia`へ分離した。Mediaを直接importする
`Codec.FFmpegThumbnailExtractor`も同じpackへ含め、Video frameを利用するMedia controller/decoder群は
Media側へ整理した。`ArtifactCoreMedia`は基盤Coreと`ArtifactCoreVideo`へ依存し、Artifact側では
Asset、Render、アプリ本体へ明示リンクしている。

### P9: ArtifactCoreComposition

Composition系9 module / 8 implementationを `ArtifactCoreComposition`へ分離した。Core内の
非Composition moduleからComposition moduleへの逆importは0件で、buffer、pre-compose、template、
final effectの契約を独立packへ閉じ込めた。Artifact本体へ明示リンクしている。

### P10: ArtifactCoreUI

UI系19 module / 8 implementationを `ArtifactCoreUI`へ分離した。Core内の非UI moduleからUI moduleへの
逆importは0件で、input operator、shortcut、selection、layout契約を独立packへ閉じ込めた。
Artifact本体へ明示リンクしている。

### P11: ArtifactCoreData

Data系12 moduleを`ArtifactCoreData`へ分離した。Data domainにはimplementation sourceがなく、CSV、
table、cache、type inferenceのmodule interfaceを独立packへ閉じ込めている。

### P13: ArtifactCoreAsset

Asset系11 moduleに`Utils.AssetManager`とそのimplementationを加えた12 module / 7 implementationを
`ArtifactCoreAsset`へ分離した。`Asset.DataAssetFile`をData packからAsset packへ戻し、Asset domainの
管理DB consumerを同じtargetへ閉じ込めた。Asset targetは`ArtifactCoreData`へ依存し、ArtifactAssetと
アプリ本体へ明示リンクしている。

### P12: ArtifactCoreCommand

Command系7 moduleにUIの`InteractiveActions`を加えた8 module / 2 implementationを
`ArtifactCoreCommand`へ分離した。UI側の逆importをconsumer同梱で閉じ、`ArtifactCoreUI`は
Command packへ依存する一方向構成にした。Artifact本体へ明示リンクしている。

### P14: ArtifactCoreAcoustic

Acoustic系7 moduleに`Artifact.Diagnostic.Registry`を加えた8 module / 0 implementationを
`ArtifactCoreAcoustic`へ分離した。Acoustic telemetryを直接保持するregistryを同じpackへ移し、
Core本体からAcousticへの逆importを解消した。現在のArtifact側利用者は静的検索で確認されなかった。

### P15: ArtifactCoreShape

Shape系のprimary module 12組に、Shapeを直接利用する`IO.VectorExport`、`IO` facade、
`Text.GlyphLayout`、`Text.TextAnimator`を加えた15 module / 3 implementationを
`ArtifactCoreShape`へ分離した。Artifact本体へ明示リンクしている。

### P16: ArtifactCorePlatform

Platform系6 module / 4 implementationを `ArtifactCorePlatform`へ分離した。Core内の逆importはなく、
OS、process、power、shell、system usageのutility境界を独立packへ閉じ込めた。現行Artifact側の
有効な利用者は静的検索で確認されなかったため、不要なlink伝播は追加していない。

### P17: ArtifactCoreThread

Thread系5 module / 2 implementationを `ArtifactCoreThread`へ分離した。唯一のCore内利用者である
`Media.ImageSequenceSource`はMedia pack側にあるため、`ArtifactCoreMedia → ArtifactCoreThread`の
一方向依存を追加した。

### P18: ArtifactCoreAnalyze

Analyze系5 module / 4 implementationに`Time.TimeRemap`を加えた6 module / 5 implementationを
`ArtifactCoreAnalyze`へ分離した。TimeRemapがAnalyzeのOpticalFlowを利用するため、時間再マップを
同じpackへ閉じ込め、Artifact本体へ明示リンクしている。

### P19: ArtifactCoreTracking

Tracking系3 module / 2 implementationを `ArtifactCoreTracking`へ分離した。Core内の逆向き参照はなく、
Transformへの一方向依存だけを持つため、Artifact本体へ明示リンクしている。

### P20: ArtifactCoreIPC

IPC系3 module / 3 implementationを `ArtifactCoreIPC`へ分離した。Core内の逆向き参照はなく、
Image型を利用する共有メモリ・render-farm transportの境界として、Artifact本体へ明示リンクしている。

### P21: ArtifactCoreNLE

NLE系2 module / 2 implementationを `ArtifactCoreNLE`へ分離した。VideoからNLEへの参照だけが確認できたため、
Video targetからNLE targetへの一方向リンクを追加した。

### P22: ArtifactCorePlayback

Playback系2 module / 1 implementationを `ArtifactCorePlayback`へ分離した。MediaからPlaybackへの参照だけが確認できたため、
Media targetからPlayback targetへの一方向リンクを追加した。

### P23: ArtifactCorePreview

Preview系2 module / 2 implementationを `ArtifactCorePreview`へ分離した。Core内の逆向き参照は確認されなかったため、
Artifact本体へ明示リンクして利用境界を固定した。

### P24: ArtifactCoreExport

Lottie Export系3 module / 2 implementationを `ArtifactCoreExport`へ分離した。Core内の逆向き参照はなく、
Rigへの一方向依存だけを持つため、Artifact本体へ明示リンクしている。

### P25: ArtifactCoreVST3

VST3系1 module / 1 implementationを `ArtifactCoreVST3`へ分離した。ArtifactのVST hostがVST3 interfaceを利用するため、
Artifact本体へ明示リンクしている。

### P26: ArtifactCoreLocalization

Localization系1 module / 1 implementationを `ArtifactCoreLocalization`へ分離した。ArtifactのProject Memo UIが利用するため、
Artifact本体へ明示リンクしている。

### P27: ArtifactCoreCoordinate

Coordinate系1 module / 1 implementationを `ArtifactCoreCoordinate`へ分離した。Serializationへの一方向依存を保ち、
Artifact本体へ明示リンクしている。

### P28: ArtifactCoreEvent

Event系3 module / 2 implementationを `ArtifactCoreEvent`へ分離した。UIとPlaybackがEvent.Busを利用するため、
それぞれのtargetからEvent targetへの一方向リンクを追加した。

### P29: ArtifactCoreFile

File系3 module / 2 implementationを `ArtifactCoreFile`へ分離した。AssetがFile.TypeDetectorを利用するため、
Asset targetからFile targetへの一方向リンクを追加した。

### P30: ArtifactCorePlugin

Plugin系3 module / 2 implementationを `ArtifactCorePlugin`へ分離した。Core内の逆向き参照は確認されなかったため、
Artifact本体へ明示リンクして利用境界を固定した。

### P31: ArtifactCoreControl

Control系3 module / 2 implementationを `ArtifactCoreControl`へ分離した。MIDI / OSC外部制御の依存はCore、Property、Utilsへ収束するため、
Artifact本体へ明示リンクしている。

### P32: ArtifactCoreDatabase

Database系2 module / 1 implementationを `ArtifactCoreDatabase`へ分離した。Core内の利用者は確認されなかったため、
Artifact本体へ明示リンクして独立境界を保った。

### P33: ArtifactCoreMask

Mask系4 module / 4 implementationを `ArtifactCoreMask`へ分離した。UIのRotoMaskEditorだけがCore Maskを参照するため、
UI targetからMask targetへの一方向リンクを追加した。

### P34: ArtifactCoreConfiguration

Configuration系3 module / 2 implementationとApplication.AppSettingsを同じ `ArtifactCoreConfiguration`へ移した。
AIとAssetが設定moduleを利用するため、それぞれのtargetからConfiguration targetへのリンクを追加した。

### P35: ArtifactCoreText

Font 3 moduleとText 6 module、Text実装5件を `ArtifactCoreText`へ統合した。Shapeに暫定配置されていた
GlyphLayout / TextAnimatorもText packへ戻し、Shape targetからText targetへの一方向リンクを追加した。

### P36: ArtifactCoreGenerate

Generate系2 module / 2 implementationを `ArtifactCoreGenerate`へ分離した。Core内の逆向き参照は確認されなかったため、
Artifact本体へ明示リンクして独立境界を保った。

### P37: ArtifactCoreSimulation

Simulation系2 module / 2 implementationを `ArtifactCoreSimulation`へ分離した。Geometryへの一方向依存を維持し、
Artifact本体へ明示リンクした。

### P38: ArtifactCoreTrack

Track系2 module / 1 implementationを `ArtifactCoreTrack`へ分離した。Image型への依存をCoreに閉じ込め、Artifactの
CompositionRenderController利用に対応して明示リンクした。

### P39: ArtifactCoreSource

Source系1 module / 1 implementationを `ArtifactCoreSource`へ分離した。Memory / Utilsへの依存だけを持つleaf境界として、
Artifact本体へ明示リンクした。

### P40: ArtifactCoreProject

Project系2 moduleを `ArtifactCoreProject`へ分離した。MetadataCollectorを内部契約として同じpackに置き、ArtifactのProject統計利用へ
明示リンクした。

### P41: ArtifactCoreScene

Scene系2 module / 1 implementationを `ArtifactCoreScene`へ分離した。CompositionからSceneへの一方向参照をtarget linkへ反映した。

### P42: ArtifactCoreRig

Rig系1 module / 1 implementationを `ArtifactCoreRig`へ分離した。ExportからRigへの依存をtarget linkへ反映した。

### P43: ArtifactCoreGrid

Grid系1 moduleを `ArtifactCoreGrid`へ分離した。Application設定を含むConfiguration packからGridへの依存をtarget linkへ反映した。

### P44: ArtifactCoreColorCollection

ColorCollection系1 module / 1 implementationを `ArtifactCoreColorCollection`へ分離した。Color grading collectionをCore本体から切り離し、
Artifact本体へ明示リンクした。

### P45: ArtifactCoreSound

Sound系2 moduleを `ArtifactCoreSound`へ分離した。逆向き参照のない音声型契約としてArtifact本体へ明示リンクした。

### P46: ArtifactCoreSequence

Sequence系2 moduleを `ArtifactCoreSequence`へ分離した。逆向き参照のないsequence契約としてArtifact本体へ明示リンクした。

### P47: ArtifactCoreMaterial

Material系1 module / 1 implementationを `ArtifactCoreMaterial`へ分離した。SceneがMaterialを利用するため、Scene targetからMaterial targetへの一方向リンクを追加した。

### P48: ArtifactCoreEnvironment

EnvironmentVariable系1 module / 1 implementationを `ArtifactCoreEnvironment`へ分離し、Artifact本体へ明示リンクした。

### P49: ArtifactCoreLight

Light系1 module / 1 implementationを `ArtifactCoreLight`へ分離した。逆向き参照のないIES profile境界としてArtifact本体へリンクした。

### P50: ArtifactCoreCrowd

Crowd系1 moduleを `ArtifactCoreCrowd`へ分離した。Graphics / Math / Particle依存をCoreに閉じ込め、Artifact本体へリンクした。

### P51: ArtifactCoreDomain

Domain系1 moduleを `ArtifactCoreDomain`へ分離した。layer domain typeのleaf契約としてArtifact本体へ明示リンクした。

### P52: ArtifactCoreFileSystem

FileSystem系1 moduleを `ArtifactCoreFileSystem`へ分離した。DirectoryBuilderのleaf契約としてArtifact本体へ明示リンクした。

### P53: ArtifactCoreIcon

Icon系1 moduleを `ArtifactCoreIcon`へ分離した。SVG-to-icon変換契約としてArtifact本体へ明示リンクした。

### P54: ArtifactCoreVST

VST系2 moduleを `ArtifactCoreVST`へ分離した。VST host/effect契約をCore本体から切り離し、Artifact本体へ明示リンクした。

### P55: ArtifactCoreNetwork

Network系3 module / 3 implementationを `ArtifactCoreNetwork`へ分離した。RenderのRPC利用とArtifactWorkerのRPC client利用に対応し、
Render / Worker / Artifact本体からNetwork targetへの明示リンクを追加した。

### P56: ArtifactCoreCollaborate

Collaborate系1 moduleを `ArtifactCoreCollaborate`へ分離した。ReactiveEventsを変更せず、既存のReactive.Events moduleを
利用する一方向のprotocol packとしてArtifact本体へ明示リンクした。

### P57: ArtifactEffectsResidual

分割後に残ったTimeDisplacement、Noise、OpticsCompensation、RadialShadow、SurfaceFXの5 module / 5 implementationを
`ArtifactEffectsResidual`へ明示化した。既存の `ArtifactEffects` 名はaliasとして維持し、利用側の互換性を保った。

### P58: ArtifactRenderSupportContracts

RenderSupportのContext、ROI、Foundation、PerformanceMonitorの4 moduleを
`ArtifactRenderSupportContracts`へ分離した。Scheduler/Controller等の実装はSupport本体に残し、
RenderSupport、Render、EffectContractから契約packへの依存を明示した。

### P59: ArtifactColorPalette

ColorPaletteManagerの1 module / 1 implementationを `ArtifactColorPalette`へ分離した。Core serializationとColor.Floatだけを利用する
独立packとしてArtifact本体へ明示リンクした。

### P60: ArtifactColorNode

ArtifactColorNode / NodeGraphの2 module / 2 implementationを `ArtifactColorNode`へ分離した。NodeGraph→Nodeの依存を同一packへ閉じ、
Artifact本体へ明示リンクした。OCIO、Science、Management、Gradingは既存ArtifactColorに残した。

### P61: ArtifactColorSettings

ArtifactColorSettingsの1 module / 1 implementationを `ArtifactColorSettings`へ分離した。Color.LUTだけを利用する設定packとして、
ArtifactColor本体とArtifact本体へ明示リンクした。

### P62: ArtifactColorScience

ArtifactColorScienceManagerの1 module / 1 implementationを `ArtifactColorScience`へ分離した。Core Color science/LUT/ACES契約に依存し、
ArtifactColorのOCIO managerから参照される一方向packとした。

### P63: ArtifactColorManagement

ArtifactColorManagementの1 module / 1 implementationを `ArtifactColorManagement`へ分離した。Core ColorSpace/GamutConversionへ依存する
軽量なmanagement helper packとしてArtifactColor本体へリンクした。

### P64: ArtifactColorGrading

ArtifactColorGradingEngineの1 module / 1 implementationを `ArtifactColorGrading`へ分離した。Core Color/Parallelへ依存するgrading packとして
ArtifactColor本体へリンクした。

### P65: ArtifactEffectsKeying

LumaKey / ChromaKey / DifferenceKey / IBKKeyer の4 module / 4 implementationを、既存のSpatial packから
`ArtifactEffectsKeying`へ分離した。共通の`ArtifactEffectContract`、Render、Core依存に限定し、アプリ本体から
直接リンクする構成とした。

### P66: ArtifactEffectsBlur

AnisotropicFlowBlur / ApertureShapeBlur / ReactionDiffusionBlur の3 module / 3 implementationを、既存のSpatial packから
`ArtifactEffectsBlur`へ分離した。共通の`ArtifactEffectContract`、Render、Core依存に限定し、Spatial packの再構築範囲を縮小する境界とした。

### P67: ArtifactEffectsGenerate

SimpleRain / RadioWaves の2 module / 2 implementationを、既存のSpatial packから`ArtifactEffectsGenerate`へ分離した。
共通の`ArtifactEffectContract`、Render、Core依存に限定した小規模なgenerator packとした。

### P68: ArtifactEffectsDistort

DisplacementMap / TurbulentDisplace の2 module / 2 implementationを、既存のSpatial packから`ArtifactEffectsDistort`へ分離した。
共通の`ArtifactEffectContract`、Render、Core依存に限定したdistortion packとした。

### P69: ArtifactEffectsStylize

Kaleidoscope / Dithering / Kuwahara / Bevel の4 module / 4 implementationを、既存のSpatial packから
`ArtifactEffectsStylize`へ分離した。共通のEffect、Image、Property、GPU compute、Render依存を持つstylize packとした。

### P70: ArtifactEffectsGlow

DirectionalGlow、Glow、EdgeBloom、ChromaticGlow、ReactiveGlow、LiquidGlow、ResidualGlow、PhysicalHalation、LuminescenceCausticsの
9 module / 9 implementationを、既存のSpatial packから`ArtifactEffectsGlow`へ分離した。Glow系のImage/Property、GPU compute、Render依存を同一packに閉じ込めた。

### P71: ArtifactEffectsOptics

LensDistortion / OpticsCompensation の2 module / 2 implementationを、Spatialおよびresidual effect targetから
`ArtifactEffectsOptics`へ集約した。両者が共有するImageProcessing.Distortion境界をtarget構成にも表現した。

### P72: ArtifactEffectsWave

Wave の1 module / 1 implementationを、既存のSpatial packから`ArtifactEffectsWave`へ分離した。GPU compute、Image、Renderを中心とする独立した波形変形packとした。

### P73: ArtifactEffectsFilters

LinearWipe / Liquify / Mosaic / Spherize / Sharpen / FindEdges の6 module / 6 implementationを、既存のSpatial packから
`ArtifactEffectsFilters`へ分離した。共通のImage、Property、GPU compute、Render依存を持つ画像フィルタpackとした。

### P74: ArtifactEffectsNoise

AddNoise の1 module / 1 implementationを、既存のSpatial packから`ArtifactEffectsNoise`へ分離した。GPU compute、Image upload、Renderを中心とする独立packとした。

### P75: ArtifactEffectsAutoMosaic

AutoMosaic の1 module / 1 implementationを、既存のSpatial packから`ArtifactEffectsAutoMosaic`へ分離した。FaceDetection、CvUtils、Property、Core Parallelを中心とするCV処理packとした。

### P76: ArtifactEffectsMotion

OpticalFlowBlur / VectorBlur / VectorFlowGlitch / LightTrails / MotionTrail の5 module / 5 implementationを、Rasterizer packから
`ArtifactEffectsMotion`へ分離した。Effect.Context、Image、Property、GPU compute、Renderを共有するmotion/flow packとした。

### P77: ArtifactEffectsDigital

DataMosh / Glitch / FilmDamage / Deflicker の4 module / 4 implementationを、Rasterizer packから`ArtifactEffectsDigital`へ分離した。
Effect.Context、Image、Property、Core Parallelを共有するデジタル破損・安定化packとした。

### P78: ArtifactEffectsPatterns

Bricks / HexGrid / Halftone / Stripes / Voronoi の5 module / 5 implementationを、Rasterizer packから`ArtifactEffectsPatterns`へ分離した。
画像生成、Property、Core Parallel、GPU compute、Renderを共有するpattern packとした。

### P79: ArtifactEffectsChromatic

ChromaticAberration / ChromaticRelief の2 module / 2 implementationを、Rasterizer packから`ArtifactEffectsChromatic`へ分離した。
画像、Property、Core Parallel、GPU compute、Renderを共有するchromatic packとした。

### P80: ArtifactEffectsShadows

DropShadow / InnerShadow の2 module / 2 implementationを、Rasterizer packから`ArtifactEffectsShadows`へ分離した。
画像、Property、Core Parallel、GPU compute、Renderを共有するshadow packとした。

### P81: ArtifactEffectsContextual

DifferenceMatte / Edge / Ghost / PixelSort の4 module / 4 implementationを、Rasterizer packから`ArtifactEffectsContextual`へ分離した。
Effect.Context、Image、Property、Core Parallelを共有するcontext-aware raster packとした。

### P82: ArtifactEffectsTemporalContext

PosterizeTime / ScreenShake の2 module / 2 implementationを、Rasterizer packから`ArtifactEffectsTemporalContext`へ分離した。
Effect.Context、Image、Property、Core Parallelを共有する時間・フレームコンテキストpackとした。

### P83: ArtifactEffectsFinishing

RadialBlur / Satin / Stroke / Vignette の4 module / 4 implementationを、Rasterizer packから`ArtifactEffectsFinishing`へ分離した。
画像、Property、Core Parallel、GPU compute、Renderを共有する仕上げ処理packとした。

### P84: Legacy rasterizer duplicate ownership cleanup

`Effects/Rasterizer/GlowEffect` と `Effects/Rasterizer/KaleidoscopeEffect` は、canonicalなGlow/Stylize packと同じ module名を別実装として持っていたため、Rasterizer targetから除外した。ファイル自体は削除せず、canonical pack側だけがmoduleを提供する構成にした。FinishingのSatin interfaceは実在する`Effects/Rasterizer/SatinEffect.ixx`へ修正した。

### P85: Residual effect ownership closure

residualに残っていたTimeDisplacementを`ArtifactEffectsDistort`、RadialShadowを`ArtifactEffectsShadows`、NoiseEffectを`ArtifactEffectsNoise`へ統合した。SurfaceFXは独立したgraphics-surface契約を持つため`ArtifactEffectsSurfaceFX`へ分離した。全 focused packをresidual source listから明示除去し、分割済みsourceの二重コンパイルを防いだ。

### P86: ArtifactEffectsResidual compatibility umbrella

residual sourceが0/0になったため、`ArtifactEffectsResidual`を空のSTATIC targetではなくINTERFACE umbrellaへ変更した。既存の`ArtifactEffects` aliasは維持し、全focused effect packへ委譲する互換link入口とした。

### P87: Spatial/Rasterizer compatibility umbrellas

`ArtifactEffectsSpatial` と `ArtifactEffectsRasterizer` も source 0/0 になったため、空のSTATIC targetからINTERFACE umbrellaへ変更した。従来のtarget名を維持し、それぞれのfocused pack群へtransitive linkする互換入口とした。

### P88: Focused pack ownership checker

`scripts/check_source_manifests.py` にArtifact focused packの検査を追加した。各packのmodule/implementation件数一致、source path存在、pack間の重複を検出し、CMakeのsource ownershipをmanifest検査と同時に静的検証できるようにした。

### P89: Focused pack reachability audit

22 focused STATIC packすべてがArtifact本体またはSpatial/Rasterizer/Residualの互換INTERFACE umbrellaからlink到達可能であることを静的確認した。未リンクpackは0件だった。

### P90: Checker target integration

rootの`check_source_manifests` custom targetのコメントを、manifestだけでなくfocused pack ownershipも検査することが分かる表記へ更新した。既存のconfigure/build手順から追加検査が実行される経路を維持した。

## 7. 未検証事項

- effect class の利用が静的ライブラリの object pull-in に依存していないか
- `ArtifactGlobalEffectManager`、Python API、Effect Service が個別 effect の link order に依存していないか
- `ArtifactEffectContract` の DLL 境界で effect field / sampler の ABI が安定しているか
- `ArtifactEffects` の現在の全 source が CMake の `APP_MODULES` / `APP_IMPL` から重複なく除去されるか
- P4のRasterizer packをリンクした場合の module BMI 参照とリンク順
- 各パックをリンクした場合の実際の増分ビルド時間とバイナリサイズ
- `ArtifactCoreAudio` のMSVC module reference、Qt Multimedia / FFmpeg link、静的ライブラリの循環解決
- `ArtifactCoreAI` のoptional ONNX / llama / Python link、MSVC module reference、アプリ本体のlink順
- `ArtifactCoreVideo` のFFmpeg / Media module reference、Render targetのBMI解決、リンク順
- `ArtifactCoreMedia` と `ArtifactCoreVideo` のmodule reference、thumbnail / asset / render link順
- `ArtifactCoreComposition` の保存・pre-compose利用側におけるmodule referenceとlink順
- `ArtifactCoreUI` のshortcut / layout利用側におけるmodule referenceとlink順
- `ArtifactCoreData` とAsset利用側のmodule referenceおよびlink順
- `ArtifactCoreCommand` とUI.InteractiveActionsのmodule referenceおよびlink順
- `ArtifactCoreAsset` とArtifactAsset / Data packのmodule referenceおよびlink順
- `ArtifactCoreAcoustic` のDiagnostic registry利用とmodule reference
- `ArtifactCoreShape` のShape / Text / IO facade利用側のmodule referenceとlink順
- `ArtifactCorePlatform` のOS条件分岐とmodule reference
- `ArtifactCoreThread` とMedia.ImageSequenceSourceのmodule referenceとlink順
- `ArtifactCoreAnalyze` とTimeRemap利用側のmodule referenceとlink順
- `ArtifactCoreTracking` のTransform依存、module referenceとlink順
- `ArtifactCoreIPC` のImage依存、module referenceとlink順
- `ArtifactCoreNLE` とVideo利用側のmodule referenceとlink順
- `ArtifactCorePlayback` とMedia利用側のmodule referenceとlink順
- `ArtifactCorePreview` のmodule referenceとlink順
- `ArtifactCoreExport` のRig依存、module referenceとlink順
- `ArtifactCoreVST3` とVST host利用側のmodule referenceとlink順
- `ArtifactCoreLocalization` とProject Memo利用側のmodule referenceとlink順
- `ArtifactCoreCoordinate` のSerialization依存、module referenceとlink順
- `ArtifactCoreEvent` とUI/Playback利用側のmodule referenceとlink順
- `ArtifactCoreFile` とAsset利用側のmodule referenceとlink順
- `ArtifactCorePlugin` のProperty/Utils依存とmodule reference
- `ArtifactCoreControl` のMIDI/OSC外部依存とmodule reference
- `ArtifactCoreDatabase` のdatabase backend依存とmodule reference
- `ArtifactCoreMask` とUI.RotoMaskEditor利用側のmodule referenceとlink順
- `ArtifactCoreConfiguration` とAI/Asset/Application設定利用側のmodule referenceとlink順
- `ArtifactCoreText` のFont/Text module生成、Shape依存、Artifact Render利用側のlink順
- `ArtifactCoreGenerate` のgenerator module referenceとlink順
- `ArtifactCoreSimulation` のGeometry/OpenVDB依存とlink順
- `ArtifactCoreTrack` とCompositionRenderController利用側のlink順
- `ArtifactCoreSource` のMemory/Utils依存とlink順
- `ArtifactCoreProject` とProject statistics利用側のlink順
- `ArtifactCoreScene` とComposition利用側のmodule referenceとlink順
- `ArtifactCoreRig` とExport利用側のmodule referenceとlink順
- `ArtifactCoreGrid` とConfiguration/Application利用側のmodule referenceとlink順
- `ArtifactCoreColorCollection` とColor/Effect利用側のlink順
- `ArtifactCoreSound` のmodule referenceとlink順
- `ArtifactCoreSequence` のmodule referenceとlink順
- `ArtifactCoreMaterial` とScene利用側のmodule referenceとlink順
- `ArtifactCoreEnvironment` のArtifactApplicationManager/AppMain利用側のlink順
- `ArtifactCoreLight` のIES profile利用側のlink順
- `ArtifactCoreCrowd` のGraphics/Math/Particle依存とlink順
- `ArtifactCoreDomain` のlayer type利用側のlink順
- `ArtifactCoreFileSystem` のDirectoryBuilder利用側のlink順
- `ArtifactCoreIcon` のSVG/Qt依存とlink順
- `ArtifactCoreVST` のVST利用側のmodule referenceとlink順
- `ArtifactCoreNetwork` のRPC/WebSocket module reference、Render/Workerのlink順
- `ArtifactCoreCollaborate` のReactive.Events module referenceとlink順
- `ArtifactEffectsResidual` の5 effect module間依存、Diligent link順、既存alias解決
- `ArtifactRenderSupportContracts` とRender/EffectContractのmodule referenceとlink順
- `ArtifactColorPalette` のSerialization/Color依存とlink順
- `ArtifactColorNode` のNodeGraph→Node参照とlink順
- `ArtifactColorSettings` のColor.LUT参照とlink順
- `ArtifactColorScience` とArtifactColor.OCIOManagerのmodule referenceとlink順
- `ArtifactColorManagement` のColorSpace/GamutConversion参照とlink順
- `ArtifactColorGrading` のColor/Parallel参照とlink順
- `ArtifactEffectsKeying` のGPU keyer実装、Render/EffectContract参照とlink順
- `ArtifactEffectsBlur` のImageProcessing/Parallel参照、Render/EffectContract参照とlink順
- `ArtifactEffectsGenerate` のImage/Parallel参照、Render/EffectContract参照とlink順
- `ArtifactEffectsDistort` のImage/Parallel参照、Render/EffectContract参照とlink順
- `ArtifactEffectsStylize` のImage/Parallel/GPU compute参照、Render/EffectContract参照とlink順
- `ArtifactEffectsGlow` のImage/Property/GPU compute、Particle、Render/EffectContract参照とlink順
- `ArtifactEffectsOptics` のSpatial/residualからの重複除去、ImageProcessing.Distortion参照とlink順
- `ArtifactEffectsWave` のGPU compute、Image/Render参照とlink順
- `ArtifactEffectsFilters` の6 filter module、GPU compute/Render参照とlink順
- `ArtifactEffectsNoise` のImage upload/GPU compute/Render参照とlink順
- `ArtifactEffectsAutoMosaic` のFaceDetection/CvUtils参照とlink順
- `ArtifactEffectsMotion` のRasterizerからの重複除去、Effect.Context/GPU compute/Render参照とlink順
- `ArtifactEffectsDigital` のRasterizerからの重複除去、Effect.Context/Image/Parallel参照とlink順
- `ArtifactEffectsPatterns` のRasterizerからの重複除去、Image/Property/Parallel/GPU compute参照とlink順
- `ArtifactEffectsChromatic` のRasterizerからの重複除去、Image/Property/Parallel/GPU compute参照とlink順
- `ArtifactEffectsShadows` のRasterizerからの重複除去、Image/Property/Parallel/GPU compute参照とlink順
- `ArtifactEffectsContextual` のRasterizerからの重複除去、Effect.Context/Image/Property/Parallel参照とlink順
- `ArtifactEffectsTemporalContext` のRasterizerからの重複除去、Effect.Context/Image/Property/Parallel参照とlink順
- `ArtifactEffectsFinishing` のRasterizerからの重複除去、Image/Property/Parallel/GPU compute参照とlink順
- P86 `ArtifactEffectsResidual`がarchiveを生成せず、既存aliasから全focused packへ伝播すること
- P87 Spatial/Rasterizer umbrellaがarchiveを生成せず、旧target名からfocused packへ伝播すること
- P88 checkerが全focused packの件数一致・重複0・path存在を検証すること
- P89 focused pack 22件がArtifactのlink graphから到達可能であること
- P90 root `check_source_manifests` targetがownership checkerを実行すること
- P84 legacy Glow/Kaleidoscope moduleの非二重定義とSatin interface path
- P85 residual source ownershipが4 module / 4 implementationだけに縮退すること、focused packとの重複がないこと
- P91 RadialBlurの重複moduleを`ArtifactEffectsFinishing`の所有経路だけに整理すること
- P92 checkerがfocused packのmodule名衝突とinterface/implementation対応を検証すること
- P93 checkerが全22 focused packのtarget wiring（`add_library` / `target_sources`）を検証すること
- P94 legacy RadialBlurがRasterizer / Residualのsource listから除外されること
- P95 checkerが`Artifact`から全focused pack targetへ到達できることを検証すること
- P96 checkerがSpatial / Rasterizer / Residual umbrellaのfocused pack集約を検証すること
- P97 checkerがbase effect sourceのfocused/residual ownership漏れを検証すること
- `ArtifactCoreLocalization` は `Localization.cppm`（`Core.Localization`）がbase `ArtifactCore`側の `include/Utils/Localization.ixx` を参照する特殊なcross-pack module referenceであり、`ArtifactCoreModuleReferences.cmake` に明示登録されている。実configure / MSVC module生成による解決は未検証。
- P98 checkerがfocused sourceのbase list所属と`APP_MODULES/APP_IMPL`除去を検証すること
- P99 checkerが全focused packの共通直接依存を検証すること
- P100 checkerがfocused targetのprivate implementation / public C++ module file setを検証すること
- P101 checkerがArtifactCore 52 pack variableのtarget/source wiringとpath存在を検証すること
- P102 ArtifactCoreAcoustic / ArtifactCorePlatformを親Artifactのlink graphへ追加し、Core pack全体の到達性を検証すること
- P103 checkerがArtifactCore全52 packの基盤直接依存とmodule file setを検証すること
- P104 checkerがArtifact / ArtifactCore合成target link graphの循環を検証すること
- P105 checkerがArtifactCore pack内のinterface / implementation module名重複を検証すること
- `ArtifactCoreModuleReferences.cmake` に stale path が2件ある（CloudAgentの`include/`欠落、NoiseImageGeneratorの存在しない`Generator.ixx`）。子リポジトリ編集方針に従い、修正は明示承認待ち。

## 8. Configure-time source scan の削減

`ArtifactCore` と `Artifact` は、各 `cmake/*Sources.cmake` を明示 source manifest として導入した。
両 CMakeLists は manifest を読み込むだけとし、`include/` と `src/` の source 用
`GLOB_RECURSE` を撤去した。ArtifactCore では、加えて全 `.cppm` の `export module` 判定用内容スキャンも撤去した。

- manifest は現行の分類結果である module 824件 / implementation 442件を保持する
- Artifact manifest は現行の分類結果である module 573件 / implementation 476件を保持する
- public `.cppm` と module partition は manifest の `ARTIFACTCORE_MODULE_SOURCES` に明示登録する
- 新しい `.ixx` / `.cppm` / `.cpp` は manifest に明示追加する。未登録ファイルは通常のCMake configureで自動的に target へ入らない
- `ArtifactCore` の既存 source を新規追加しただけでは、CMakeが全 `.cppm` を読んで再分類しない
- `Artifact` / `ArtifactCore` の source を新規追加しただけでは、CMakeが source tree 全体を再帰列挙しない

`scripts/check_source_manifests.py` と root の `check_source_manifests` target を追加した。
これは通常ビルドには含めず、CIまたはsource追加後に実行して、未登録・削除済み source を報告する。

```powershell
cmake --build <build-dir> --target check_source_manifests
```

この変更は configure-time の探索・内容読取りを除くものであり、公開 module interface を変更した場合の
BMI依存再コンパイルまでは抑制しない。CMake configure / build は未実行である。
