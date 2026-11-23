# Atmospheric Glow Blur R&D

**最終更新:** 2026-11-12
**ステータス:** In Progress

**進捗:** 2026-11-12 — CC0 neon / streetlight plate の出典・元hashを照合し、個別利用記録のないportrait plateを除外。960×540を主解像度、480×270 / 1920×1080を解像度比較とするoffline CPU studyを作成。float32 HDR synthetic chart、CC0写真2枚、straight alpha edge、moving source 7 framesの11 capture caseを生成した。source/model/component NPY、review PNG、固定ROI、scene-linear領域統計、黒/灰/白背景alpha composite、amount=0 identity、再現性・非有限値diagnostic、tool/input/output hashをmanifestへ記録。さらに3素材×5因子、計66のone-factor response measurementとcontact sheetを保存し、各解像度でsigmaを960px基準から画角比例させる。保存済み`glow` installed CPU captureと同一のcaptured inputを使うSDR look comparison 20組も追加。現行Glow CPU式のraw-sRGB equation mirrorは歴史的captureと最大1 code差、RMSE約0.0014–0.0018 codeで一致し、source式の読み違いではないことを強く裏付けた。固定mask transfer ablationではencoded-sRGBの方がscene-linearより歴史的PNGとの数値差が小さいが、画質判断ではなく転送空間感度の観測。同じsource mask・HDR入力・総scene-linear輝度追加量に揃え、single Gaussian・3-scale Glow型・local/broad/veilのPSF応答を比較するoffline制御を追加。local/broad/veilはGlow型よりr95が25.02→74.41pxに伸び、同RMSのsingle Gaussianよりr50が28.02→9.22pxと中心側に寄る一方、微弱な広い裾が細部ROIへ到達した。これは合成PSFの形状差であり、品質・知覚優位性ではない。さらに3組の等エネルギー方式対照をA/B化した別ブラインドパケットを生成し、方法キーは独立フォルダーへ隔離。歴史的Glow対候補の既存4組パケットとともに、第三者レビューの回答は未取得。source-matched current runtime、HDR/float製品capture、GPU parity、performance、合否閾値も未実施。

## 目的と仮説

「大気グローぼかし」は提案段階の名称であり、既存 `glow` / bloom や通常 blur と異なる画像応答があるかを調べる。R&D中は製品エフェクト名・パラメータ・実装方式を確定しない。

主仮説は、閾値で抽出した高輝度光源の広い低周波散乱成分を、弱い局所ハローと連続的に組み合わせると、光源の形や背景細部を保ちながら空気中へ光が広がる印象を作れる、というもの。対立仮説は、既存 Glow / bloom に弱い広半径を足した look と視覚的に区別できず、別エフェクトを設ける価値がない、または単なる veil が画像を洗うだけ、というもの。いずれも未検証である。

### 対象

- 静止画像に対する高輝度 source selection、複数スケールの空間散乱、元画像との合成応答。
- Glow、通常 blur、既存 Atmospheric Fog / Scattering R&D の成果を比較・再利用できる範囲の把握。
- CPU数式／fixture段階から開始し、実アプリ capture 経路が利用可能と証明できた後に限って GPU preview / final 出力を評価する。

### 対象外

- 体積光輸送、遮蔽・shadow map、depth-aware scattering、空気密度場、時間変化を新たに実装すること。
- Lens flare / streak、色収差、ハレーション、カメラ応答の物理再現。
- 製品コード、effect catalog/UI、既定値の変更、GPU shader / render-host 追加。

## 比較対象と証拠の役割

同一の入力と表示条件で次を比較する。

1. 入力 baseline（恒等応答、合成 ground truth ではない）
2. 現行 `glow` / 適切な bloom 候補
3. 通常 Gaussian / multi-scale blur の光源選択なし対照
4. Atmospheric Glow Blur の合成試作候補：bright-source threshold + 正規化した局所／広域PSF散乱 + 独立に記録した低周波 veil 成分

合成fixtureからの解析式は応答特性・数値回帰の参照であり、実写の物理的正解ではない。既存実装の画像は製品ベースラインであり、承認済み目標画像として扱わない。目標画像が後から指定された場合は出所・承認者・用途を明記する。

GlowLab の `experiments/GlowLab/RESEARCH_NOTES.md` には既存 flash-fill look で veil が合成輝度増分の約89%を占めた測定がある。これは異なる効果・素材の既存観測で、本R&D候補の予測値や合否閾値には流用しない。veil成分を独立測定し、単なる露出上昇に退化しないか確認する。

## 評価素材と再現条件

既存データを優先し、取得時に入力ファイルSHA-256・寸法・色/alpha契約・ライセンス情報・元fixtureの生成版をcapture manifestへ記録する。ファイルが見つからない、契約を特定できない、または利用条件が不明なら別素材で置き換えた事実を明示する。

- **合成応答chart:** 非対称なHDR点光源2個（色・強度違い）、明暗階段、細線、checker、平坦領域、image edge近傍光源。deterministic float32 scene-linear fixtureを新規生成する場合は固定seedとgenerator hashを保存。
- **既存実写候補:** GlowLab の出所記録済みCC0街灯写真とneon plateを使用する。肖像plateは個別LICENSE/READMEで利用条件を確認できなかったため除外した。固定ROIでは街灯の光源/空、neonの交差点/実用光源/道路細部を確認する。顔領域は現在のfixture matrixに含まれない。
- **alpha fixture:** opaque edge、半透明発光物、straight/premultipliedの対応ペア、fully-transparent hidden-RGB stress。背景黒・中間灰・白の3色で合成表示する。hidden RGBは表示指標に混入させずrepresentation invariant診断として分離する。
- **時間fixture:** 静止光源、横移動する小光源、露出が段階変化する光源の決定的7-frame sequence。静止入力の候補出力がフレームごとに変わらないことも確認する。

一次計測は少なくとも960×540 scene-linear float32 RGBA。解像度依存を確認するため同一画角の480×270と1920×1080を追加し、可能なら4K処理予算は実行機材・GPU・メモリのbaseline取得後に定める。float32入力、scene-linear RGB、露出固定、同一の表示変換、フレーム番号、parameter値、出力形式をmanifestへ固定する。アルゴリズム乱数は使わない。画像とcropは線形計測データ（PFM/EXR等の契約が検証された形式）と同一Reinhard + sRGBのreview用PNGを分離する。

## 事前登録する比較行列

- Threshold: 無効/低/高（HDR radiance unitで入力分位点と対応づけ、実測baseline前に数値を固定）
- Scatter PSF: Gaussian対円形aperture参照、局所/広域の半径比、広域成分比。PSFは正規化し、bright source単独応答を記録。
- Amount: 0をidentity対照、弱・中・強の3候補。実装候補のbaselineを取得してから、最終比較の前に採用値を固定。
- Composite: source-add、highlight-replacement/soft-knee、veil独立の成分表示を対照に含める。比較目的は各成分の識別で、先に最良方式を仮定しない。
- 一度に一要因を変更する。その他の設定は固定し、全候補の設定JSONを保存する。

## 評価・合否計画

判定者は担当実装者と別のレビュー者を含める。少なくとも全体画像と固定ROI cropを1:1、同一表示で確認し、数値集計だけで合格にしない。以下の数値閾値は、現時点ではbaseline未取得のため意図的に空欄である。**R&D の最終候補比較前**に、baseline画像・ROI mask・測定空間を固定して担当者が許容差を数値記入し、日時と根拠をこの文書へ追記する。閾値未記入の項目は未評価であり、R&D合格にできない。

| 必須項目 | 領域・測定 | 合格条件の確定方法 | 重大欠陥 |
|---|---|---|---|
| 効果の正しさ | bright source ROIと周辺ringの線形輝度、PSF重心/covariance/r50/r95、source-off/threshold境界近傍、amount=0応答 | baseline capture後に許容誤差・応答単調性を数値化。amount=0は定義された恒等契約の数値精度を先に記録 | source位置ずれ、threshold微小変化による不連続、非有限値 |
| 画質・細部 | 顔、細線、文字、実用光源、平坦部、画像端。edge spread、局所contrast保持、banding/noise、haloと光源energyを別々に測る | 固定ROI・測定尺度を先に定め、候補の目視blind comparisonと統計を併記 | source形状消失、文字/顔の視認性阻害、端の不自然な漏れ、顕著なring/banding |
| 色・alpha | RGB分布/clip率/色差、alpha分布、3背景合成後の境界帯差、straight/premultiplied pair | baseline取得後にHDR値域と色空間を明記して許容差を記入。hidden RGB invariantは別契約 | fringe、色相偏り、意図しないalpha変化、premultiplication由来の縁 |
| 時間安定性 | 7-frameの対応ROI、source motion整列後の光源重心/halo半径、残差frame差 | sequence baselineからmotion-compensation手順と閾値を固定。時間状態を使わない方式は静止再現性も記録 | flicker、shimmer、動きに追従しないghost/trail |
| 経路・解像度 | 480p/960p/1080p、preview/final、CPU/GPU実出力のpaired capture。backendが使えない場合は未検証 | source/build/runtime provenanceを一致させたpaired capture。許容差はcapture前に記入 | 解像度でlookが大幅変化、previewとfinalの見た目不一致 |
| 性能 | 同じ入力、warm-up、実行回数と反復raw sampleを保存。median/p95処理時間、peak GPU memory、allocation/transfer | baselineと候補を同一実機・buildで測定後、目標予算と許容回帰を固定 | interactive予算超過、frame中の重い非zero allocation/resource再生成 |

### 画像統計

- 全体・source ROI・halo ring・source edge・細部ROI・alpha boundaryごとにRGB/輝度/alphaのmin/max/mean/median/p95/p99、非有限値、HDR clip率を記録する。
- baselineとの差は同一入力・linear scene-referred値でMAE/RMSE/p95/max/閾値超過率を保存する。ただし入力との差が大きいこと自体は品質欠陥を意味しない。
- 効果固有統計としてsource energy保持率、halo ring energy、veil面積比、PSF centroid/spread、slanted-edgeの法線方向spread、細線contrast保持率を測る。
- 有効な基準画像がない look 比較に参照誤差の合格値を設定しない。PSNR/SSIMは補助であり単独合否に使わない。
- NPY/EXR等の元計測値、差分、mask、ROI座標/画素数、preview変換、設定、source hash、tool versionを証跡に含める。

## 実行経路・性能条件

- 初期は外部fixture上のCPU prototypeと数式比較に限定し、Artifact実出力とは呼ばない。
- 実アプリcaptureに進む前に、source revisionと一致したbinary、対象effect stageを通る経路、linear float RGBA export、alpha/color descriptor、GPU/backend/driverを検証する。PNG screenshotのみではHDR・alpha・backend parityの証拠にしない。
- previewはinteractive解像度と品質tierを、finalはcomposition解像度と正式出力経路を別々に記録する。GPU非対応環境は未検証として残す。
- baselineとcandidateを同じGPU/backend・解像度・入力・warm-up・iterationで実行し、全iterationのraw時間、median/p95、GPU memory、texture allocation/readbackの有無を記録する。baseline取得後にinteractive/final予算をms/MPまたはframe timeで明記し、capture matrixを完了する前に合否閾値を確定する。
- 計画にないテスト/build/CMake、製品ファイル編集は行わない。現在の許可範囲は評価計画の整備とfixture/オフラインR&Dであり、製品実装・アプリ起動・capture runtimeは別途明示指示が必要。

## 現在の状態とゲート

- 既存のGlow variant R&D、Blur Effects R&D Suite、Fog and Atmospheric Scattering R&Dを索引で確認した。Glowの派生として重複しないか、fog/volumeのtransportを誤って範囲に含めないかを分ける。
- Fog R&Dは2026-11-12時点でComposition host dispatch不在を静的再確認しており、runtime captureは未実施。新しいeffectから既存 fog shaderを接続可能と仮定しない。
- 製品capture adapter、Artifact実出力baseline、別判定者レビュー、実測performanceは未作成/未実施。offline fixtureとsource-equation outputsはこのR&Dの再現可能な探索証跡だが、製品経路・品質受入れの証拠ではない。
- 実ファイル確認済み素材は `experiments/GlowLab/assets/SOURCES.md` 記録のCC0 streetlight (`b01e663d…7224eb8`) とneon working plate (`8791bd59…7aa319d4`)。portrait plateは個別ライセンス記録が見つからないため除外した。両写真はRGB JPEGをsRGB transfer assumptionでlinearizeし、960×540をcontain letterbox、480p/1080pも同じ画角にする。embedded ICCはstudyで適用せず、manifestに有無を記録する。
- **rd1 offline result (2026-11-12):** `experiments/AtmosphericGlowBlurRD/atmospheric_glow_blur_lab.py` が、480×270 / 960×540 / 1920×1080におけるsynthetic HDR + 2 CC0 photo = 9 cases、960×540 alpha edge 1 case、480×270 moving-source sequence 7 frames、計11 caseを生成。raw float32 input/outputとsource seed/local/broad/veil成分、Reinhard+sRGB review PNG、固定 synthetic / photograph ROIのchannel・luma分布、baseline-modelとの差分、黒/灰/白背景composite、sequence screen-space residualを `experiments/AtmosphericGlowBlurRD/captures/rd1/rd1_manifest.json` とNPY群へ保存。manifestはinput/tool/output SHA-256と `rd1_manifest.sha256` を持つ。
- **Historical installed Glow SDR comparison (2026-11-12):** 既存`experiments/GlowLab/captures/glow-suite-foundation-neon-gain-sweep-cpu-rd1/product-suite-manifest.json`はstable ID `glow`のinstalled CPU outputをgain `.10/.25/.50/1.00`、layerCount 4、baseSigma 6で記録し、同じ1536×1025 captured input PNG hash `04484ab8…f488ad65`と各出力PNGを持つ。profileの元CC0 source hashは`8791bd59…7aa319d4`。記録されたArtifactUiTest.exe SHA-256 `180f9c4a…07d2c490`は同じworkspace pathのbinary hashと一致するが、capture recordにsource revisionがないため、現行sourceに一致するbinaryとは証明できない。既存ファイルだけを読む`compare_installed_glow_sdr.py`で4 product gains × 5 offline candidate amounts (.10/.25/.50/.70/1.00) の20組を同じcaptured PNG入力・同一ROIで比較した。diag pairのwhole-image encoded RGB MAEは1.53 / 2.65 / 4.31 / 6.92 8-bit code、sRGB decode後のsigned mean linear-luma deltaは+.00222 / +.00685 / +.01359 / +.02375 (gain=.10/.25/.50/1.00)。.25 pairのcentral-intersection ROI encoded MAEは6.98 code。candidate amountとGlow gainは意味が等価ではなく、この誤差は差の規模のみを示す。接触画像は同一SDR input、product output、offline candidate、8-bit absolute diff×8を並べる。capture source revision不明・8-bit PNGなので、これは歴史的installed-effectのlook comparisonであり、現行source parity・HDR品質・知覚優位性ではない。
- **Glow CPU equation reconstruction and transfer-domain control (2026-11-12):** `Artifact/src/Effects/Glow/GlowEffect.cpp::GlowEffectCPUImpl::applyCPU` は入力descriptorを読む変換を行わず、BGRA float RGBのluma `.114B+.587G+.299R` をthreshold `.6` / `THRESH_TOZERO`、maskへ`glowGain`を乗算・1へ上限、source RGBへmaskを乗算して4つのGaussianを適用する。sigmaは`baseSigma + i*sigmaGrowth`、各alpha weightは`baseAlpha*alphaFalloff^i`、最後にalpha weight和でnormalizeし、sourceを加算してRGB `[0,1]` clip、alphaをsourceから戻す。`Artifact/src/Layer/ArtifactImageLayer.cpp::toFrameBuffer(QImage)` はQImageをsRGB/Premultiplied descriptorのBGRA floatへ変換するため、opaque RGBA8 sourceではsRGB code valuesをGlow CPUへ渡すモデルが妥当な候補になる。`compare_installed_glow_sdr.py` はその式をOpenCV float32で再現し、同じcaptured inputと4 product gainsで比較。raw sRGB code modelのwhole RGB MAEは約`1.9e-6–3.2e-6` code、RMSEは`0.00138–0.00178` code、max差1 code。decoded-linear input modelではgain .10→1.00のMAEが`0.70→4.85` codeへ増えた。現binary hash一致とequation output一致はsource revisionの空白を補完するruntime provenanceではないが、このhistorical opaque CPU captureが現在読めるCPU式に極めて近い出力をしたことを示す。
- 同ツールのtransfer-space ablationは、candidateのsmooth source-selection maskをlinear lumaから共通生成し、PSF convolution/compositeだけscene-linearまたはencoded sRGBで行う対照。product gain=.10/.25/.50/1.00でwhole-image encoded RGB MAEはscene-linear`1.529/2.651/4.308/6.915`code、encoded-space`0.664/1.604/2.998/5.190`code。central-intersection ROIでもencoded-space側は`1.901/4.359/7.718/12.243`codeで、各条件のscene-linear値より小さい。この結果はGlow式へ色空間を合わせると数値差が縮まる感度を示すだけで、encoded-space candidateが画質上正しいという主張ではない。今後のlook評価では、候補のspatial kernel差と色空間差を別実験にし、正式候補の色・alpha契約を先に定義する。
- **Blind SDR review packet (2026-11-12):** `experiments/AtmosphericGlowBlurRD/build_blind_review_packet.py` creates four method-anonymized A/B full-frame comparisons, three fixed ROI crops per pair, and a CSV response form from saved historical product PNGs and offline scene-linear candidate arrays. A/B ordering uses recorded seed 20261112; identity key is isolated under `captures/blind-review-sdr-rd1-key/`, outside the packet directory. Packet manifest SHA-256 is `baa3d98aed4ef1e9e961fbec2da9efa13c27488d8b72a90fa46f6d9f24304874`. The answer template and mask-key instructions are prepared, but no independent reviewer response is available. The comparison remains exploratory: equal numeric gain/amount values are not semantically matched, and 8-bit SDR cannot support HDR, runtime, or final acceptance conclusions.
- **Matched-energy scatter-profile control (2026-11-12):** `experiments/AtmosphericGlowBlurRD/compare_scatter_profiles.py` uses one deterministic 960×540 HDR chart, one smooth threshold mask, one selected source seed, scene-linear Rec.709 luma, and OpenCV Reflect101 for four kernels: single Gaussian at multiscale-Glow RMS radius, three-scale Glow-like (.5/.3/.2 at 5/9.5/14px), single Gaussian at candidate RMS radius, and local/broad/veil (.455/.245/.08 at 5/24/60px). Each applied kernel is scaled to the same total added luma energy as the multiscale reference (`879.786`). Impulse PSF r50/r95 are Glow-like multiscale`8.25/25.02px`, single Gaussian with candidate RMS`28.02/58.14px`, and local/broad/veil`9.22/74.41px`. The last model's veil contributes`0.2371` luma energy in the synthetic fine-detail ROI; ROI luma standard deviation changes by`-1.04e-6`. This provides analytical evidence that a weak veil can produce a sparse long tail while keeping a brighter central response at matched integrated energy. It is one synthetic, source-selected, offline CPU equation study. It does not test perception, unthresholded full-frame Gaussian blur, product/runtime parity, or a final quality acceptance. Outputs and input/model/tool hashes are in `experiments/AtmosphericGlowBlurRD/captures/scatter-profile-rd1/scatter_profile_manifest.json`.
- **Matched-energy blind comparison packet (2026-11-12):** `experiments/AtmosphericGlowBlurRD/build_scatter_blind_review_packet.py` creates A/B full-frame and fixed-ROI crops for multiscale Glow-like vs local/broad/veil, candidate-RMS single Gaussian vs candidate, and Glow-RMS Gaussian vs multiscale. The three pairs use the same input/mask/energy-normalized equations and are more controlled than the historical-product packet, but still use one synthetic source and one Reinhard+sRGB display preview. The method key is isolated under `captures/blind-review-scatter-profile-rd1-key/`. Packet manifest SHA-256 is `1f333e968928b9a163523c4701cfb8bc66eb0bd0963cfe6ae179233834c9c8e0`. No reviewer answers have been received; do not infer distinguishability or preference from the analytical PSF metrics.
- **CC0 photographic scatter response and blind packet (2026-11-12):** `experiments/AtmosphericGlowBlurRD/compare_scatter_profiles_photographs.py` runs the same four fixed kernels on the existing license-recorded neon and streetlight plates. For each image, scattered total scene-linear Rec.709 luma is normalized to that image's multiscale Glow-like output. Existing GlowLab ROIs are retained. In streetlight `sky_field_ring`, added luma sum is `0.0026` (multiscale) vs `1.9381` (local/broad/veil); lamp-core luma standard-deviation changes are `+0.0703/+0.0531` vs `+0.0722/+0.0554` (left/right). On neon, fine-detail ROI luma standard-deviation changes are `+0.01629` vs `+0.01370`. Full-frame mean display-luma changes under the saved Reinhard+sRGB preview are only `+0.000306` (neon) and `+0.000011` (streetlight), but local ROI display luminance still differs; therefore the blind form asks perceived brightness separately from atmospheric character. These are regional output responses, not quality scores. Float arrays and source/ROI/settings/output hashes are in `captures/scatter-photo-rd1/scatter_photo_manifest.json` (SHA-256 `742bc0fcb1d45c7f4b5d18439ac0f1ceca0eeee08415084e55a2419a833987d2`). `build_photo_scatter_blind_review.py` creates six method-anonymized pairs and fixed practical-light/detail crops; both A/B side and stimulus order are shuffled from recorded seed 20261112. The key is isolated under `captures/blind-review-photo-scatter-rd1-key/`. The CSV separately records perceived brightness; its packet manifest SHA-256 is `d3b1f559d5222bc7d14550cd39a3c1cba27567d3d64b59e1348d90ed774cdebe`. Independent review remains pending.
- rd1のscene-linear CPU equation diagnosticsでは amount=0の最大RGB差0、alpha差なし、同一静止入力の反復差0、全candidateの非有限値なし。alpha edge fixtureをstraight RGBからpremultiply/unassociateした対では入力可視RGB最大差`1.19e-7`、候補可視RGB最大差`4.77e-7`だった。fully transparent画素へ無効なHDR hidden RGB `(5,4,2)`を置くstressではalphaは維持されたが、隣接半透明物体の合成後RGB RMSE/最大差は`4.27e-6` / `6.89e-5`。これは現offline式がalpha-awareでなくhidden RGBをsource selectionに使う感度であり、Artifact挙動・許容性の根拠ではない。候補veil/source-seed luma energy比は、設定した比率に沿いsynthetic/neon/streetlightで約0.08002/0.08004/0.08001。これはmodel上の成分比で、物理散乱や視覚的優位性の証明ではない。7 frameのscreen-space残差RGB RMSEは隣接差`0.01535`前後だが、motion compensationなし・acceptance thresholdなしのためflicker判定には使わない。
- 66候補のone-factor sweepではthreshold / broad fraction / broad sigma / veil fraction / amountを一因子ずつ変更し、他の基準値を固定した。neonのcentral-intersection ringにおけるcandidate対baseline model MAEはbroad fraction 0→1で`0.000498→0.001477`、broad sigma 12→72pxで`0.000329→0.001588`。streetlightのlamp-ring MAEはthreshold 0.45→1.10で`0.003138→0.017791`、broad sigma 12→72pxで`0.002089→0.009636`。これらはROIにおける候補差の応答値で、好ましさ・許容値・知覚弁別の順位ではない。sweep JSONと各因子contact sheetを保存したが、独立blind reviewと合否判定はしていない。
- 960×540探索用Reinhard+sRGB接触画像では、neon / streetlightのbaseline modelとcandidate modelは全体表示で非常に近く、差分画像では主に発光源ROIへ差が出た。linear RGB MAEはwhole imageでneon`0.00391`、streetlight`0.000179`。固定ROIではneon central intersection ring`0.0247`、streetlight left/right lamp ring`0.00704 / 0.00607`、streetlight fine-detail領域`0.000101`。これは独立式model間の差分で、知覚上の識別性・品質改善を示すスコアではない。previewは担当者による探索的な表示確認のみで、別判定者のblind reviewは未実施。比較画像: [synthetic HDR](../../experiments/AtmosphericGlowBlurRD/captures/rd1/synthetic-hdr-960x540.comparison.review.png), [CC0 neon](../../experiments/AtmosphericGlowBlurRD/captures/rd1/cc0-neon-960x540.comparison.review.png), [CC0 streetlight](../../experiments/AtmosphericGlowBlurRD/captures/rd1/cc0-streetlight-960x540.comparison.review.png)。差分paneは線形絶対差×8を同じpreview変換で表示する。
- `baseline_model` は独立式の固定threshold multi-Gaussianで、installed Artifact Glowのcaptureではない。既存GlowLab製品CPU PNG captureは8-bit handoffであり、scene-linear HDR比較へ使わない。したがって本データから候補の見た目が製品Glowより優れる、または異なるとは主張しない。目視したreview previewも単純Reinhard+sRGBの探索用で、別判定者によるblind reviewではない。
- 未実施: Artifact runtime baseline/candidate capture、目的別image-quality judgement、final閾値確定、Artifact alpha/color descriptor上でのhidden-RGB stress、GPU/backend、preview/final route、実測performance/メモリ、別判定者blind review。offline modelではhidden-RGB stressを実施したが、Artifact実行時契約の確認にはならない。次工程は、capture routeのfloat32 linear export・alpha descriptor・現source一致binaryを確認して製品baselineを取得すること。routeが確立しなければoffline fixtureの範囲を維持し、製品品質として評価しない。
- **採用ゲート:** Glow/bloom baselineとの差を別判定者が識別でき、veilでは説明しきれない局所source-to-halo応答を示し、重大欠陥なし、全必須評価を実施し、performance予算を満たすこと。条件未達または未検証があれば不合格ではなく「未判定」とし、製品採用を主張しない。
- R&D完了は製品effect追加の承認ではない。採用時も実装設計・API/UI・GPU経路は別マイルストーンにする。

## 関連資料

- [Image Effect R&D Evaluation Rules](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)
- [Blur Effects R&D Suite](MILESTONE_BLUR_EFFECTS_RD_2026-11-12.md)
- [Fog and Atmospheric Scattering R&D](MILESTONE_FOG_ATMOSPHERIC_SCATTERING_RD_2026-11-12.md)
- [Glow Variants milestone](../done/MILESTONE_GLOW_VARIANTS_2026-06-13.md)
- `experiments/GlowLab/RESEARCH_NOTES.md`（既存 look 実験。条件の異なる結果を本R&Dの合否に流用しない）
