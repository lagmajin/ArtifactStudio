# Blur Effects R&D Suite

**最終更新:** 2026-11-12
**ステータス:** In Progress

**進捗:** 2026-11-12 — 静的実装台帳、factory/catalog/test coverage の読解、deterministic fixture 69配列（空間18＋2.5D point 3＋時間列4×12）、Glow露出bracket点光源、custom PSF asset、6系統の spatial sweep matrix、offline image comparison CLI を用意。入力配列 sanity と asset/hash reference は確認済み。timing JSON は反復ごとのraw samplesを必須とし、解析器が指定統計量を算出する契約へ更新。Radial amount/quality、Anisotropic blur/noise/integration/adherence、Aperture PSF controlsは一因子ずつ変える。Reaction Diffusionは通常blur順位と分離。解析器は有限float32 RGBAとlinear scene-referred straight-alpha color contractを要求し、raw/visible parityとalpha誤差、visible energy/radius、hidden-RGB invariant比較、visible impulse PSF covariance/異方性、visible slanted-edge normal方向spreadを算出する。capture orderを3段階にし、CPU 88件／resident GPU適格候補14件を含む`capture_manifest.json`を生成。manifest生成スクリプトはeffect別property名・型・範囲、PSF hash、fixture/settings hash、unique output path、resident GPU適格性、計画件数の整合を検査する。さらに全入力fixture・補助asset・計画・README・解析ツールのSHA-256台帳とbundle identity hashを出力し、生成capture結果とmanifest自己参照は台帳対象から除く。GPU候補にはraw EXR pathとcapture record pathを付与し、`normalize_exr_capture.py` は明示的なchannel/color/alpha契約のsidecarを要求してfloat32 RGBA NPYへ正規化する。sidecarは実装環境の契約を保証するものではなく、operator assertionとして記録する。Vector BlurのGPU shaderはvelocityを使わず、CPUはHDRを0–1 clampする静的所見を追加し、GPU parity候補から除外。CLI renderはPNG/JPEG/videoのみ、Render QueueのEXR multi-channelはGPU限定でcolor/alpha契約未確認。EXR adapterは実ファイル未確認。Artifact effect capture、CLI execution、runtime 計測は未着手。

**予備測定:** 外部bundleの `run_reference_blur_study.py` でArtifact実装を呼ばない理想化PSF比較を追加。Gaussian、1〜3段box、disk/pentagon/hexagon/octagon/custom heart apertureをsigma 2/4/8/16/32 pxの45条件で測り、alpha-safe normalized kernelのvisible energy、重心、共分散、r95、Gaussianとの差を `reference_blur_study.json` に記録した。sigma 8ではdiskのspreadが約7.96/7.96 px、heartが約6.91/7.57 px。これは実装品質・知覚評価・CPU/GPU parity・速度の測定ではなく、候補特性を絞るための数値参照データである。

**Capture reproducibility:** run metadataにsource revision、実行binary SHA-256、build configuration、capture adapter、CPU/GPU/driver/thread countを必須化。analyzerはこれらのruntime provenance全体が一致した場合だけ異なるbackend/execution pathをparity比較可能とし、analyzer・capture manifest・bundle identityのhashを結果へ残す。異なるbuildやhostの数値比較はlook comparisonとして保持する。

**Temporal model reference:** `temporal_reference_study.json` の20条件では、sigma=3/lookback=4のcentered moving blurは時間spread 2.227 frames・centroid lag 0.332 px・trail幅 91.039 px、trailing moving blurは時間mean -1.597 frames・lag -19.703 px・trail幅 54.054 px。frame 11のfuture-missing moving条件はpast-only重みとなりlag -19.389 px・spread 55.432 px。zero-motion対照では全Time Blur設定でlag 0・patch内spread 16 pxとなり、時間平均だけで誤った空間smearが生じない基準を得た。opaque/transparentの同一trajectoryは応答値も同一。これらはTime Blur CPU式から作った数学的参照値で、Artifact captureの測定値ではない。

## 目的

既存のブラー系機能を、見た目・入力・画質・実行コストを同じ基準で比較できる R&D suite として整理する。空間畳み込み、方向場、レンズ／深度、時間履歴、生成的 stylize を再現可能な fixture で評価し、次の投資先を選べるようにする。

## 現状

- `docs/analysis/EFFECT_MAP_2026-07-16.md` は Gaussian Blur、Blur、Radial Blur、Anisotropic Flow Blur、Aperture Shape Blur をカタログ対象としている。実装・サービス・メニュー登録・実 HLSL 経路は別々に監査する必要がある。
- `Artifact/src/Effects/Blur/` に Gaussian、一般 Blur、Aperture Shape、Anisotropic Flow、Reaction Diffusion の実装がある。
- `Artifact/src/Effects/Rasterizer/` に Radial、Optical Flow、Vector、Time Blur がある。時間履歴の host 契約は `docs/planned/MILESTONE_TEMPORAL_EFFECTS_RD_2026-11-12.md` が対象としており、本計画では重複実装せず連携する。
- Aperture Shape と Anisotropic Flow の旧計画は superseded 表記を持つ一方、後者には現実装監査が追記されている。R&D 対象は現行ソースを基準に決める。
- Physical Motion Blur とカメラ DOF は独立した3D／時間露光・深度領域であり、2D blur benchmark と同一品質指標に混ぜない。

## 初期実装台帳（ソース静的確認）

| 対象 | 実装根拠 | CPU / GPU の確認 | R&D上の注意 |
|---|---|---|---|
| Gaussian Blur | `Artifact/src/Effects/Blur/GauusianBlur.cpp` | CPU と HLSL compute の両経路あり。HLSL は水平・垂直の分離カーネルと premultiplied alpha の計算を含む。 | 専用 contract tests あり。実パイプライン常駐経路と wrapper の upload/readback 経路を区別する。実行時 parity は未確認。 |
| Blur | `Artifact/src/Effects/Blur/BlurEffect.cpp`、`Artifact/include/Effects/Blur/BlurEffect.ixx` | CPU 実装に加え、適格設定はresident compositor spatial nodeを使う。legacy `applyGPU` wrapperはcomposition rendererから到達せず、blocking readbackを含む。 | `sigma<3`、strength>=0.999、premultiplied alpha、iterations×passes<=8 の場合だけresident GPU対象。それ以外はCPU。 |
| Radial Blur | `Artifact/src/Effects/Rasterizer/RadialBlurEffect.cpp` | CPU pixel 経路を確認。`syncImpls()` は CPU impl のみ設定。 | Zoom／Spin の意味を分け、端 clamp によるエネルギー集中を測る。 |
| Anisotropic Flow Blur | `Artifact/src/Effects/Blur/AnisotropicFlowBlurEffect.cpp` | Core `AnisotropicFlowBlur` を呼ぶ CPU impl のみ確認。 | Structure Tensor 前処理コストと方向場品質を分離する。 |
| Aperture Shape Blur | `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cpp` | CPU impl 内で OpenCV `filter2D` を用いる箇所を確認。 | GPU経路は未確認。PSF energy と alpha契約を試験する。 |
| Reaction Diffusion Blur | `Artifact/src/Effects/Blur/ReactionDiffusionBlurEffect.cpp` | CPU impl。Gaussian blur 後に反応拡散格子を評価する。 | 生成模様を測り、通常 blur の品質・速度順位には含めない。 |
| Optical Flow / Vector / Time Blur | `Artifact/src/Effects/Rasterizer/{OpticalFlowBlur,VectorBlur,TimeBlur}Effect.cpp` | Optical Flow／Vector は CPU と GPU wrapper、Time は CPU impl を確認。Vector GPU HLSL は `VelTex` を宣言するが参照せず、velocity がゼロ固定。 | Temporal history 契約は Temporal Effects R&D 側で検証する。Vector GPU は現状 parity candidate に数えず、CPU の HDR clamp とともに motion sequence で確認する。 |
| Camera DOF / Motion Blur | Camera/render pass 実装および `MILESTONE_PHYSICAL_MOTION_BLUR_2026-06-07.md` | 本台帳では dispatch と画素 parity を監査していない。 | 専用 depth/velocity fixture まで比較結果に混ぜない。 |

上表はソースから分かる範囲の初期分類であり、実行時の品質・速度は証明しない。factory branch、available-effects catalog、Inspector catalog の照合では上記 stable ID を確認した。ただし factory/catalog 登録は runtime での menu 表示や利用可能状態の証明ではない。

### factory / catalog / contract test の接続

`Artifact/src/Service/ArtifactEffectService.cpp` の create branch と available-effects catalog、および `Artifact/src/Widgets/InspectorEffectCatalog.cpp` の表示名を静的照合した。Gaussian は `effect.blur.gaussian`、一般 Blur は `blur`、他は `radial_blur`、`anisotropic_flow_blur`、`reaction_diffusion_blur`、`aperture_shape_blur`、`optical_flow_blur`、`vector_blur`、`time_blur` の stable ID で factory/catalog に登録されている。

| 既存 contract test | 読解で確認した assertion | R&D suite に残る不足 |
|---|---|---|
| `GaussianBlurEffectContractTest.cpp` | opaque impulse spread、sigma=0 identity、transparent hidden red のにじみ防止、clamp | HDR、境界 profile、複数寸法、数値精度マップ、GPU parity |
| `BlurEffect` | 専用 contract test file/target は確認できず | identity、alpha/HDR、半径ごとの CPU/GPU/fallback parity を新規契約化 |
| `ApertureShapeBlurEffectContractTest.cpp` | RGB spread、alpha／source保持、radius=0 identity、pentagon PSF差 | PSF energy profile、透明 RGB 対照、HDR、境界・寸法比較、GPU経路の確定 |
| `RadialBlurEffectContractTest.cpp` | zoom の画素期待値、center維持、zero identity、property clamp | spin profile、full-size radial energy、透明/HDR、CPU/GPU 比較 |
| `AnisotropicFlowBlurEffectContractTest.cpp` | RGB変化、uniform alpha保持、zero identity、property clamp | 方向場の向き／coherence fixture、edge retention 指標、HDR・透明境界 |
| `OpticalFlowBlurEffectContractTest.cpp` / `VectorBlurEffectContractTest.cpp` | zero blur、missing history fallback、property ranges | 実sampler・motion sequence の画素比較は Temporal Effects R&D と連携 |
| `TimeBlurEffectContractTest.cpp` | centered 時の `-1/+1` sample と重み、trailing 時の past-only | 実 sampler の順不同・seek等は Temporal Effects R&D の責務 |
| `ReactionDiffusionBlurEffectContractTest.cpp` | 寸法とsource alpha保持、simulation property clamp | deterministic output、seed／repeatability、pattern energy・差分 |

上記は source assertion の存在を確認しただけで、test は起動していない。CMake では各 contract target が登録済みで、Aperture は `ARTIFACT_BUILD_TESTS` または `ARTIFACT_ENABLE_IMAGE_EFFECT_TESTS` 条件内にある。

### 初期 sweep 設定表

| Effect ID | 最初の sweep | 比較を揃える方法 |
|---|---|---|
| `effect.blur.gaussian` | Inspector `Strength` (σ) 1 / 3 / 8 / 16 | metadataは実property名を保持し、impulse の95% energy radiusと処理時間を記録 |
| `blur` | Radius 2 / 8 / 16 / 32 px、Iterations 1 / 2 / 4、Strength 1、Mode Gaussian。Edge Preserving は別 sweep。 | Gaussian mode は実測95% radiusで比較し、iteration 数も別記する。 |
| `aperture_shape_blur` | radius 2 / 8 / 16 px、shape 0/1/2/3 = circle/pentagon/hexagon/custom PSF | radiusを揃えた比較と、実測 energy radius比較を分ける。custom PSF は入力画像も記録する。 |
| `radial_blur` | amount 0.25 / 0.5 / 1.0、quality 8 / 16 / 32、zoom / spin | 固有モデルの設定 sweep。blur radius換算値として表現しない |
| `anisotropic_flow_blur` | amount 5 / 10 / 25%、edge adherence 0 / 0.5 / 1 | direction-field fidelity と出力 energy radius を報告 |
| `optical_flow_blur` / `vector_blur` | default と 2つの品質点 | Temporal R&D の同一 motion sequence／履歴条件を使う |
| `time_blur` | centered と trailing、lookback 1 / 4 | 実 sampler の frame availability は Temporal R&D 側で確認 |
| `reaction_diffusion_blur` | default、iteration low/high | stylized output の repeatability と別枠で提示 |

時間履歴を使う Optical Flow／Vector／Time Blur は spatial impulse manifest へ混ぜず、Temporal Effects R&D と共有する `motion_frame_00..11.npy` 列を使う。初期比較は frame 6 を現在画像とし、過去・未来の参照可能frameを明示して出力する。Time Blur は centered/trailing と lookback 1/4、Vector は shutter 90/180 と samples 8/16、Optical Flow は blurAmount 0.5/1 と velocityScale 1/2 を一因子ずつ変える。これらは capture proposal であり、履歴store順とfuture-frame availabilityが再現できるhost adapterができるまで未実行の予定とする。Vector GPU wrapper は GPU parity capture を保留する。静的読解ではCPU `VectorBlur` が出力RGB/alphaを0–1へclampし、GPU shaderはゼロvelocityで現在画像だけを再サンプルする形に見えるため、HDR保持と実移動の両方を確認する。

外部bundleに `temporal_sweep_plan.json` と `temporal_capture_manifest.json` を追加。従来のopaque saturated-patch列に加えて、同じpatch位置のtransparent-black列とframe 6を12回複製したopaque/transparent静止対照列を用意し、Time Blur／Optical Flow Blur／Vector BlurのCPU 44 capture候補と、frame 6 random-accessおよびframe 11 future-missing sampler条件をhash付きで記録する。`analyze_temporal_capture.py` はmotion-axis trail幅／lagを計測し、Time Blurは実sampler availabilityに沿う厳密なweighted-frame参照とpixel差分を出す。`run_temporal_reference_study.py` は20通りのTime Blur条件で、正規化frame weights、実効時間spread、予想trail lag/spreadを計算して `temporal_reference_study.json` に記録する。これはCPU実装式に基づくモデル参照でありArtifact captureではない。画質用入力はcontract-testの12×1 fixtureと別で、未来frame preloadingは製品hostの供給可否を示さない。44件すべてpendingで、Artifact effect outputは未取得。

各 run metadata は effect ID、全 property 値、backend、実行経路（CPU reference / wrapper GPU / resident compositor GPU 等）、warm-up 数、測定反復数、timing statistic を必須にする。timing は ms 単位で setup、steady-state、upload、dispatch、readback、wait、独立測定の total wall time を記録する。stage の重複・包含を避けるため合算しない。未計測 stage は0とせず省略する。

## 実装意味論の横断確認

静的に確認できた処理意味論を、capture条件と解釈に反映する。これらはコード経路の確認であり、実出力の数値比較ではない。

| 対象 | 入力色・alpha処理 | 境界／サンプル | captureで分離する点 |
|---|---|---|---|
| Gaussian Blur | CPU/HLSLともRGBをpremultiplyしてalphaと畳み込み、出力時にunpremultiply。 | CPU OpenCVは既定境界（Reflect101）。HLSLは各軸を端pixelへclamp。CPUは6σ程度の奇数kernel、GPUは半径ceil(3σ)を最大64に制限。 | 同一設定でもCPU/GPUの境界・離散kernel・大σ上限が違う。中央impulse parityとborder impulse parityを別記録する。 |
| Blur | CPUは`convertBlurColorSpace()`でsRGB変換を行い、変換関数が各RGBを[0,1]へclampする。既定premultiplied modeではalphaも別にblur。 | CPUは`BORDER_REPLICATE`。GPU samplingも端clamp。CPUのwide blurはhalf-resolution経路がある。`EdgePreserving` branchは各iterationで通常のGaussian後にsigma 0.6倍のGaussianをもう一度適用する。 | HDR/negative fixtureでclippingを明示評価し、linear scene-referred 入力への色変換前提を確認する。Mode=0/1とiterationsを分け、Gaussianとsigmaだけで条件を同一視しない。 |
| Aperture Shape | RGB各planeを個別FFT convolution。alpha planeは入力をそのまま保持。highlightBoostは0.65超成分を追加する。 | `fftConvolve()`は画像寸法のDFTへ直接kernelを配置し、画像側をpadしないため数学的にはcircular convolution。対向端からのwrapをborder impulseで測る。 | alpha edge leakage、PSF energy normalization、custom mask、highlight boost、wraparoundを独立して評価する。 |
| Radial Blur | RGBとalphaを同じsample平均に含める。 | 中心からzoom/spin方向へquality個のsampleを足し、元pixelを含めてquality+1で平均。座標は整数化され、範囲外は端pixelへclamp。 | zoom/spin、中心位置、品質、端部へのsample集中とalpha edgeを評価する。 |
| Anisotropic Flow Blur | BGRA各channelを直接bilinear sampleし、premultiply/unpremultiplyしない。 | structure tensorで局所tangent角を出し、9点を線上sample。境界は座標clamp。`v_len`は計算されるがsamplingには使われない。 | `edgeAdherence`は未使用の`v_len`にしか入らないため現状出力に影響しないように見える。0/1設定をcaptureして確認し、hidden-RGB影響、HDR維持、9-tap固定品質も記録する。横断幅があると仮定して評価しない。 |
| Reaction Diffusion | RGBをGaussian blur後の模様でmixし、alphaは入力値を保持。seedは入力のBGRA grayscaleから作る。 | 初段GaussianとLaplacianはReflect101。模様は低解像度gridで反復し、拡大後に合成する。 | 通常のblur半径順位から分け、alpha edge、HDR/negative、Evolution固定時の決定性、模様の寄与を調べる。 |

この差のため、理想化参照カーネル結果はアルゴリズム形状の事前調査に限り、Artifact効果間の品質順位やbackend parityには使わない。比較レポートではeffect固有のalpha/boundary/color契約をmetadataに残し、契約が一致しないrunはparity pairにしない。

## R&D 軸

| 系統 | 代表対象 | 比較観点 |
|---|---|---|
| 空間・分離可能カーネル | Gaussian Blur、Blur | 半径、反復、境界、alpha／premultiplication、HDR、CPU/GPU parity |
| 中心・方向ベース | Radial Blur、Anisotropic Flow Blur | 中心位置、方向場、エッジ保持、端部の歪み |
| レンズ PSF | Aperture Shape Blur | PSF形状、エネルギー正規化、ハイライト形状、FFT／空間処理切替 |
| 局所運動推定 | Optical Flow Blur、Vector Blur | motion field、遮蔽、静止領域、場の品質と破綻 |
| 時間履歴 | Time Blur、Motion Blur | sampler／履歴契約、シーク、時間順、露光重み。Temporal Effects R&D と連携 |
| 生成的・stylized | Reaction Diffusion Blur | 決定性、反復数、模様と元画像の寄与、静止画／時間発展の区別 |
| 2.5D focus-offset | Layer 2.5D Depth of Field | CoC bokehと見なさず、depth差から作るoffset passの位置・opacity・重なりを別指標で評価 |

### 隣接する blur-derived トラック

Effect factory (`Artifact/src/Service/ArtifactEffectService.cpp`) には、通常の空間blurとは別の glow/bloom/halation 効果も登録されている。初期 impulse のσ順位へ混ぜず、Phase 1 の空間captureが動き出した後に独立した拡張matrixとして追加する。

| トラック | factory 登録名 | 専用入力と比較軸 |
|---|---|---|
| Glow / Bloom | Glow (`glow`)、Edge Bloom (`edge_bloom`)、Chromatic Glow (`chromatic_glow`)、Reactive Glow (`reactive_glow`)、Liquid Glow (`liquid_glow`)、Residual Glow (`residual_glow`)、Directional Glow / Streaks (`directional_glow`) | 暗部・中間調・HDR点光源を同じ位置に置く露出bracket、色違い点光源、細線。記録threshold後のenergy、halo半径・重心、色分離、streak方向、alpha契約。edge/threshold型は入力信号とhalo成分を別集計する。 |
| Film halation | Physical Halation (`physical_halation`) | HDR点光源と飽和色境界。threshold、spread、色移行を露出別に追い、通常glowとのenergy分布・色差を比較する。 |
| 生成的発光 | Luminescence Caustics (`luminescence_caustics`) | 固定seed/時間条件、暗部・HDR点光源、alpha境界。構造化模様の決定性と発光成分の寄与を独立評価し、Gaussian等のぼけ半径順位には入れない。 |
| 2.5D focus-offset smear | `layer.2_5d.depthOfFieldEnabled`, `focusDepth`, `focusRange`, `maxBlur` | `buildTwoPointFiveDRenderPasses()` は `min(maxBlur, abs(depth-focusDepth)/max(1,focusRange)*maxBlur)` を算出し、blur量が0.1 px超なら中央と±X/±Yへ同じspriteを5回、各1/5 opacityで描く。評価fixtureは高コントラスト点・斜線・近遠景の重なりとし、cross-shaped ghost offset、opacity和、深度に対するoffset応答を測る。通常のPSFやCoC bokeh順位には入れない。 |

この登録範囲はfactoryの静的確認であり、各効果の実行可能性、現行property schema、実GPU dispatch、画像出力を証明しない。拡張matrixを作る前に各効果の実property名・CPU/GPU経路・乱数／時間状態を個別確認し、共通評価項目を適用できないstylized出力は専用指標を定める。

2.5D Depth of Field は通常のeffect factory効果ではなく、layer transform supportからsprite draw passへ渡る。現在確認できる実装はGaussian/CoC filterではなく、最大5つの平行移動spriteを合成する近似である。Inspector tooltipもcomposition 3D cameraとは独立したlocal 2.5D lensと明記する。したがって、この機能のR&DはDOF画質評価と区別し、実出力で重ね描画・遮蔽・alphaの応答を確認する。

`dof_2_5d_sweep_plan.json` は中心pixelが異なる色の3画像を近景(+250)／合焦(0)／遠景(-250)の独立layerへ置く13 scene候補を定義する。focusDepth/range/maxBlurを一因子ずつ変え、depth=±250、focus=0、range=250、maxBlur=12なら実装式からoffset=12px、5 pass×0.2 opacity、合焦layerはoffset=0の単一passと予測する。これは式から導いた期待値であり、描画結果や遮蔽順序はcaptureで確認する。

最初の Glow/Bloom 実験対象として Edge Bloom と Chromatic Glow の `getProperties()` / CPU処理を読み、各々7項目・6項目の正確なproperty名とdefault/rangeを `glow_sweep_plan.json` に記録した。露出bracketの12点光源は warm/cyan/magenta × 0.25/1/4/16 で、両解像度のfixtureを使う。baselineと一因子low/high、256px resolution checkを含む32候補をCPU-firstで計画した。`analyze_glow_capture.py` は入力差分の正のRGB寄与から点光源ごとのhaloエネルギー、r95、重心、共分散、色度を出し、alpha差と出力端値数、解析対象ROI外縁のtail警告を添える。Edge BloomとChromatic GlowのCPU経路はRGBをclampし、出力halo寄与だけではHDR clippingを説明できないため `compare_outputs.py` の全画像比較と併用する。32候補はすべて未実施で、解析器も実captureではまだ動作確認していない。

## 最初の deliverable

### Phase 0 — 既存実装台帳

各対象の effect ID、CPU実装、HLSL実 dispatch、catalog 登録、アルファ契約、時間状態、未検証項目を記録する。`supportsGPU()` の宣言だけで GPU 実装済みと判定しない。

### Phase 1 — 決定論的画像セット

外部ファイルやデコーダに依存しない float RGBA fixture を用意する。impulse／点光源、checker／細線／高コントラスト斜線、半透明色境界と透明周辺、端まで届く形状、1.0超ハイライトと負値を含める。Flow／radial 比較には静止背景上を動く高彩度パッチを使い、Temporal Effects R&D の画像列と共有可能か確認する。

初期 asset は `blur-rd-fixtures/` に生成している（visualizations 作業領域）。`manifest.json` を入力仕様とし、linear unclamped straight-alpha RGBA float32 `.npy` を数値比較の正本、`blur-rd-contact-sheet.png` は display-clamped preview とする。fixture は impulse、HDR point、negative/HDR pixel、2px checker、slanted edge、alpha edge と hidden-RGB 対照、border impulse を256²/1024²で用意し、12-frame moving patch は256²で用意した。Glow/Bloom系向けに、暗背景上のwarm/cyan/magenta点光源を線形強度0.25/1/4/16で配した`glow_exposure_bracket`を256²/1024²に追加した。

`spatial_sweep_plan.json` は Gaussian、general Blur、Aperture Shape、Radial、Anisotropic Flow、Reaction Diffusion の設定 sweep と8種の fixture family を定義する。数値 sweep は一因子ずつ変更し、Radial の amount と quality を同時に変えていた confound を解消した。Reaction Diffusion は stylized output として通常blurの parity/radius順位から除外し、Evolutionを固定する。capture順は全設定を256px impulseで先に測り、代表設定の品質診断を行い、最後に選んだ12設定だけ1024pxへ拡大する3段階とし、CPU参照88件、resident GPU適格候補14件に絞った。`build_capture_manifest.py` がこの計画を`capture_manifest.json`へ展開し、各候補にfixture/settings hash、予想出力名、metadata templateを記録する。GPU候補はsource eligibility条件を満たすものだけ出力し、GPU dispatchを確認できた場合だけGPU結果として数える。capture filenameのsettings hashは、key順整列・compact UTF-8 JSON（NaN禁止）のSHA-256に固定した。Aperture Shape の custom shape 用には実装の built-in heart mask 式に合わせた `custom_psf_heart.png` を用意し、manifest SHA-256 との一致を確認した。effect capture は未実施。

初期 spatial benchmark は次の条件に固定する。

- linear float RGBA、256×256 と 1024×1024 の静止 fixture。
- impulse／point light、1px line pair、slanted edge、半透明の赤→透明境界、端に接した高輝度形状を用意する。
- Gaussian、Blur、Aperture Shape は impulse 出力の実測 visible-energy covariance から得る effective σ を主指標に合わせ、95% energy radius・major/minor σ・異方性も併記する。設定値（nominal radius/σ）が同じでも実効量は揃わない。Radial と Anisotropic Flow はこの一致条件に加えて固有の標準設定も記録する。
- CPU/GPU 比較では効果設定、色 descriptor、寸法を一致させ、入力の非破壊を確認する。
- warm-up と計測を分離する。cold setup、steady-state evaluation、upload、dispatch、readback を別欄にし、同期 wait/readback を伴う wrapper は製品 native render の性能値と混ぜない。
- 透明画素 RGB を変えて visible output が変化しないか検査する。HDR は負値と1超値を含み、clipping の有無を記録する。

Capture normalization must obey [`docs/technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md`](../technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md). `ImageF32x4_RGBA` has BGRA physical backing; `setFromRGBA32F` normalizes logical RGBA input to BGRA, while `toCanonicalRGBA32FC4` only swaps channel order. It does not convert alpha association or transfer. Before `.npy` export, require a fully specified source descriptor and use the descriptor-aware conversion path to produce linear scene-referred RGBA straight alpha. Unknown alpha/transfer metadata makes the capture invalid until the effect output contract is established. The external EXR normalizer records an explicit source-contract assertion but does not read Artifact descriptors; this descriptor-aware Artifact capture route remains unimplemented.

No direct Artifact effect-to-float32-NPY exporter was found in the inspected source. `Artifact.exe render` currently accepts PNG/JPEG and video formats. Render Queue supports GPU-only EXR multi-channel output with a 32-bit float option, and `readbackToMultiChannelImage()` swizzles the beauty texture from BGRA to logical R/G/B/A channels. Its AOV boundary is before composition final-image effects; alpha association also needs runtime confirmation. It is a candidate only for suitable layer-effect cases, not verified general capture infrastructure.

fixture の生成 script は `blur-rd-fixtures/generate_fixtures.py`。生成物の sanity check では69配列すべてが有限値、256²または1024² RGBA、alpha edge 対照の premultiplied visible pixels が一致し、HDR最大値8.0、negative最小値-0.4、両解像度の impulse が1 pixel、時間列が12枚であることを確認済み。これは Artifact effect の試験や runtime 性能検証ではない。

### Phase 1b — 出力比較 CLI

`blur-rd-fixtures/compare_outputs.py` は fixture 名と output dump 群を受け取り、raw RGBA RMSE／最大誤差に加え、visible premultiplied RGB RMSE／最大誤差とalpha誤差を分離して出す。alpha=0から新たに可視化した領域のRGB、unassociated RGB energy（透明RGB込みの診断値）、visible premultiplied energy／retention、visible energy centroid基準の95% radiusもJSON/CSVに出す。alpha-edge foreground bleedは隠しRGBを引かず、出力visible premultiplied RGBを元fixtureと比較する。別の `compare_hidden_rgb.py` はhidden RGBだけ異なる二つのfixture結果を比較し、premultiplied RGBとalphaの不変性を測る。単一インパルスではvisible positive-RGB energy centroid、2D covariance、major/minor spread、異方性比、主軸角を計算し、点像分布の形を記述する（perceptual scoreではない）。slanted-edge はvisible premultiplied luminanceをauthored edge normalに投影し、10–90% edge-spread幅、50%位置、bin profileをJSONに出す。同一入力に対するrun pairもraw RGBAとvisible RGB/alphaを分けて比較する。全runにeffect ID、正確なsettings、固定color contract、backend、execution path、warm-up／測定反復数、timing statisticが必要。parity候補はeffect/settings/color contractが一致し、backendとexecution pathが両方異なる場合だけとする。timing JSONはsteady-stateと独立測定total wall timeの反復配列を必須にし、各stage配列長を測定反復数と一致させる。解析器がmean / median / p95を算出し、raw samplesもJSONに保持する。stageを合算しない。CLIはrendererを実行せず、export済み`.npy`を読むoffline analyzerである。CLIの実行結果は未検証。

GPU EXR candidate normalization は `blur-rd-fixtures/normalize_exr_capture.py` が担う。manifest rowに紐づくEXRのみ受け付け、明示sidecarにchannel order、linear transfer、primaries、scene range、alpha association、float32、unclamped assertionを必須化する。premultiplied alphaはunpremultiplyし、alpha zeroのRGBを0にする。unknown contract、サイズ・dtype・finite値・alpha rangeの不一致は拒否する。入力EXRと出力NPYのhashをnormalization recordへ保存するが、capture statusやGPU dispatchの証明は更新しない。OpenCV local buildはOpenEXR supportを報告しているものの、実EXRの読込・alpha意味・チャンネル配置は未検証である。

### Phase 2 — 共通比較レポート

同じ寸法・入力・効果量で CPU reference と利用可能な GPU 経路を比較する。画像差分に加え、edge profile、alpha leakage、luma／energy変化、CPU時間、GPU dispatch時間、upload／readbackを分離して記録する。性能値は実測まで記載しない。

### Phase 3 — 品質・性能マップ

半径・解像度・品質設定ごとの画質差とコストを記録する。プレビュー／final の品質切替やアルゴリズム選択は、測定結果と既存 render contract の確認後に別判断とする。

## 成功条件

- blur family の実装範囲、登録状態、未検証事項が一つの台帳で追える。
- 代表入力で alpha、境界、HDR、エネルギー、エッジ保持の違いを比較できる。
- CPU／GPU parity と時間計測を混同せず、upload／readbackを含む実コストが分かる。
- 時間履歴系は sampler の順不同・seek・eviction 契約と連携する。
- 結果から次に実装・修正する具体的な一項目を選べる。

## 制約

- 初期段階では製品 UI、catalog、render path、effect API を変更しない。
- 子リポジトリは変更しない。
- ホットパスでは画像や GPU resource を毎フレーム再生成せず、fixture／計測は専用計測で行う。
- `QImage`、`QPainter`、Qt CompositionMode を画像処理・合成の新規経路に使わない。
- ビルド、テスト、CMake 実行は明示指示後に行う。

## 関連資料

- `docs/analysis/EFFECT_MAP_2026-07-16.md`
- `docs/planned/MILESTONE_TEMPORAL_EFFECTS_RD_2026-11-12.md`
- `docs/planned/MILESTONE_APERTURE_SHAPE_BLUR_2026-06-13.md`
- `docs/planned/MILESTONE_ANISOTROPIC_FLOW_BLUR_2026-06-13.md`
- `docs/planned/MILESTONE_REACTION_DIFFUSION_BLUR_2026-06-13.md`
- `docs/planned/MILESTONE_PHYSICAL_MOTION_BLUR_2026-06-07.md`
- `docs/technical/HOT_PATH_RULES.md`
