# Blur Effects R&D Suite

**最終更新:** 2026-11-12
**ステータス:** In Progress

**進捗:** 2026-11-12 — 静的実装台帳、factory/catalog/test coverage の読解、deterministic fixture 28配列（256²/1024²の静止16＋時間12）、custom PSF asset、6系統の spatial sweep matrix、offline image comparison CLI を用意。入力配列 sanity と asset/hash reference は確認済み。timing JSON は反復ごとのraw samplesを必須とし、解析器が指定統計量を算出する契約へ更新。Radial amount/quality、Anisotropic blur/noise/integration/adherence、Aperture PSF controlsは一因子ずつ変える。Reaction Diffusionは通常blur順位と分離。解析器は有限float32 RGBAとlinear scene-referred straight-alpha color contractを要求する。capture orderを3段階にし、CPU 88件／GPU候補30件を含む`capture_manifest.json`を生成。manifest生成スクリプトはfixture/settings hash、unique output path、計画件数の整合を検査する。GPU候補にはraw EXR pathとcapture record pathを付与し、`normalize_exr_capture.py` は明示的なchannel/color/alpha契約のsidecarを要求してfloat32 RGBA NPYへ正規化する。sidecarは実装環境の契約を保証するものではなく、operator assertionとして記録する。CLI renderはPNG/JPEG/videoのみ、Render QueueのEXR multi-channelはGPU限定でcolor/alpha契約未確認。EXR adapterは実ファイル未確認。Artifact effect capture、CLI execution、runtime 計測は未着手。

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
| Blur | `Artifact/src/Effects/Blur/BlurEffect.cpp` | CPU 実装と GPU implementation wrapper を持つ。GPU stack 容量制約時は CPU fallback する旨の実装コメントあり。 | factory/catalog ID `blur` は登録済み。専用 `BlurEffect` contract test は確認できず、fallback と半径ごとの経路も含め R&D contract を追加候補とする。 |
| Radial Blur | `Artifact/src/Effects/Rasterizer/RadialBlurEffect.cpp` | CPU pixel 経路を確認。`syncImpls()` は CPU impl のみ設定。 | Zoom／Spin の意味を分け、端 clamp によるエネルギー集中を測る。 |
| Anisotropic Flow Blur | `Artifact/src/Effects/Blur/AnisotropicFlowBlurEffect.cpp` | Core `AnisotropicFlowBlur` を呼ぶ CPU impl のみ確認。 | Structure Tensor 前処理コストと方向場品質を分離する。 |
| Aperture Shape Blur | `Artifact/src/Effects/Blur/ApertureShapeBlurEffect.cpp` | CPU impl 内で OpenCV `filter2D` を用いる箇所を確認。 | GPU経路は未確認。PSF energy と alpha契約を試験する。 |
| Reaction Diffusion Blur | `Artifact/src/Effects/Blur/ReactionDiffusionBlurEffect.cpp` | CPU impl。Gaussian blur 後に反応拡散格子を評価する。 | 生成模様を測り、通常 blur の品質・速度順位には含めない。 |
| Optical Flow / Vector / Time Blur | `Artifact/src/Effects/Rasterizer/{OpticalFlowBlur,VectorBlur,TimeBlur}Effect.cpp` | Optical Flow／Vector は CPU と GPU wrapper、Time は CPU impl を確認。wrapper は native GPU residency を意味しない。 | Temporal history 契約は Temporal Effects R&D 側で検証する。 |
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

各 run metadata は effect ID、全 property 値、backend、実行経路（CPU reference / wrapper GPU / resident compositor GPU 等）、warm-up 数、測定反復数、timing statistic を必須にする。timing は ms 単位で setup、steady-state、upload、dispatch、readback、wait、独立測定の total wall time を記録する。stage の重複・包含を避けるため合算しない。未計測 stage は0とせず省略する。

## R&D 軸

| 系統 | 代表対象 | 比較観点 |
|---|---|---|
| 空間・分離可能カーネル | Gaussian Blur、Blur | 半径、反復、境界、alpha／premultiplication、HDR、CPU/GPU parity |
| 中心・方向ベース | Radial Blur、Anisotropic Flow Blur | 中心位置、方向場、エッジ保持、端部の歪み |
| レンズ PSF | Aperture Shape Blur | PSF形状、エネルギー正規化、ハイライト形状、FFT／空間処理切替 |
| 局所運動推定 | Optical Flow Blur、Vector Blur | motion field、遮蔽、静止領域、場の品質と破綻 |
| 時間履歴 | Time Blur、Motion Blur | sampler／履歴契約、シーク、時間順、露光重み。Temporal Effects R&D と連携 |
| 生成的・stylized | Reaction Diffusion Blur | 決定性、反復数、模様と元画像の寄与、静止画／時間発展の区別 |
| 深度・レンズ | Camera DOF / Depth Bokeh | CoC、foreground/background 分離、絞り形状。別 fixture として接続 |

## 最初の deliverable

### Phase 0 — 既存実装台帳

各対象の effect ID、CPU実装、HLSL実 dispatch、catalog 登録、アルファ契約、時間状態、未検証項目を記録する。`supportsGPU()` の宣言だけで GPU 実装済みと判定しない。

### Phase 1 — 決定論的画像セット

外部ファイルやデコーダに依存しない float RGBA fixture を用意する。impulse／点光源、checker／細線／高コントラスト斜線、半透明色境界と透明周辺、端まで届く形状、1.0超ハイライトと負値を含める。Flow／radial 比較には静止背景上を動く高彩度パッチを使い、Temporal Effects R&D の画像列と共有可能か確認する。

初期 asset は `blur-rd-fixtures/` に生成している（visualizations 作業領域）。`manifest.json` を入力仕様とし、linear unclamped straight-alpha RGBA float32 `.npy` を数値比較の正本、`blur-rd-contact-sheet.png` は display-clamped preview とする。fixture は impulse、HDR point、negative/HDR pixel、2px checker、slanted edge、alpha edge と hidden-RGB 対照、border impulse を256²/1024²で用意し、12-frame moving patch は256²で用意した。

`spatial_sweep_plan.json` は Gaussian、general Blur、Aperture Shape、Radial、Anisotropic Flow、Reaction Diffusion の設定 sweep と8種の fixture family を定義する。数値 sweep は一因子ずつ変更し、Radial の amount と quality を同時に変えていた confound を解消した。Reaction Diffusion は stylized output として通常blurの parity/radius順位から除外し、Evolutionを固定する。capture順は全設定を256px impulseで先に測り、代表設定の品質診断を行い、最後に選んだ12設定だけ1024pxへ拡大する3段階とし、CPU参照88件、GPU候補30件に絞った。`build_capture_manifest.py` がこの計画を`capture_manifest.json`へ展開し、各候補にfixture/settings hash、予想出力名、metadata templateを記録する。GPU fallback時は実行経路を実際に記録し、GPU dispatchを確認できた場合だけGPU結果として数える。capture filenameのsettings hashは、key順整列・compact UTF-8 JSON（NaN禁止）のSHA-256に固定した。Aperture Shape の custom shape 用には実装の built-in heart mask 式に合わせた `custom_psf_heart.png` を用意し、manifest SHA-256 との一致を確認した。effect capture は未実施。

初期 spatial benchmark は次の条件に固定する。

- linear float RGBA、256×256 と 1024×1024 の静止 fixture。
- impulse／point light、1px line pair、slanted edge、半透明の赤→透明境界、端に接した高輝度形状を用意する。
- Gaussian、Blur、Aperture Shape は impulse の95% energy radiusを揃える。Radial と Anisotropic Flow はこの一致条件に加えて固有の標準設定も記録する。
- CPU/GPU 比較では効果設定、色 descriptor、寸法を一致させ、入力の非破壊を確認する。
- warm-up と計測を分離する。cold setup、steady-state evaluation、upload、dispatch、readback を別欄にし、同期 wait/readback を伴う wrapper は製品 native render の性能値と混ぜない。
- 透明画素 RGB を変えて visible output が変化しないか検査する。HDR は負値と1超値を含み、clipping の有無を記録する。

Capture normalization must obey [`docs/technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md`](../technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md). `ImageF32x4_RGBA` has BGRA physical backing; `setFromRGBA32F` normalizes logical RGBA input to BGRA, while `toCanonicalRGBA32FC4` only swaps channel order. It does not convert alpha association or transfer. Before `.npy` export, require a fully specified source descriptor and use the descriptor-aware conversion path to produce linear scene-referred RGBA straight alpha. Unknown alpha/transfer metadata makes the capture invalid until the effect output contract is established. The external EXR normalizer records an explicit source-contract assertion but does not read Artifact descriptors; this descriptor-aware Artifact capture route remains unimplemented.

No direct Artifact effect-to-float32-NPY exporter was found in the inspected source. `Artifact.exe render` currently accepts PNG/JPEG and video formats. Render Queue supports GPU-only EXR multi-channel output with a 32-bit float option, and `readbackToMultiChannelImage()` swizzles the beauty texture from BGRA to logical R/G/B/A channels. Its AOV boundary is before composition final-image effects; alpha association also needs runtime confirmation. It is a candidate only for suitable layer-effect cases, not verified general capture infrastructure.

fixture の生成 script は `blur-rd-fixtures/generate_fixtures.py`。生成物の sanity check では28配列すべてが有限値、256²または1024² RGBA、alpha edge 対照の premultiplied visible pixels が一致し、HDR最大値8.0、negative最小値-0.4、両解像度の impulse が1 pixel、時間列が12枚であることを確認済み。これは Artifact effect の試験や runtime 性能検証ではない。

### Phase 1b — 出力比較 CLI

`blur-rd-fixtures/compare_outputs.py` は fixture 名と output dump 群を受け取り、入力 fixture に対する RMSE／最大誤差、元画像で alpha=0 から output で新たに可視化した領域の premultiplied RGB、positive RGB energy と alpha-weighted energy retention、source energy centroid 基準の95% energy radiusを JSON/CSV に出し、slanted-edge の中心 line profile と同一入力に対する run ペアの RMSE／最大誤差を JSON に出す。全 run に effect ID、正確な settings、固定 color contract、backend、execution path、warm-up／測定反復数、timing statistic が必要。parity 候補は effect/settings/color contract が一致し、backend と execution path が両方異なる場合だけとする。timing JSON は steady-state と独立測定 total wall time の反復配列を必須にし、各 stage 配列長を測定反復数と一致させる。解析器が mean / median / p95 を算出し、raw samples も JSON に保持する。stage を合算しない。CLI は renderer を実行せず、export 済み `.npy` を読む offline analyzer である。CLI の実行結果は未検証。

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
