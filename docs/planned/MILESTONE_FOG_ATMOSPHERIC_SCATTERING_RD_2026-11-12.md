# Fog and Atmospheric Scattering R&D

**最終更新:** 2026-11-13
**ステータス:** In Progress（source audit、CPU積分収束・sample配置・shadow境界stress sweep／位置・幅誤差とpenumbra proxy・combined誤差・camera sequence時間残差とoutlier continuous-ray診断・outlier収束／可視ray母集団・spatial error field・shadowed world point direct-ray時間残差・AABB active-face post-hoc監査・X half-extent scenario/common-cohort・camera translation amplitude・quadrature factor decomposition sweep・aspect-ratio別signed factor追試・幅誤差±2%画像空間追試・ray-cast aspect-ratio静止／sequence画像追試・画像fixture、depth encoding感度診断、cloud/light fog積分と水平極限候補監査、惑星大気sample配置の太陽高度／camera高度robustness・局所fog froxel CPU proxy・light count sweep・log/linear depth distribution sweep・camera translation temporal CPU proxy・froxel XY spatial resolution / fine-volume stress sweep・hard-edged volume boundary・subsample injection・integration convergence decomposition・hard-box ray-interval oracle convergence・projected-box coverage negative result・per-froxel ray-box depth occupancy・multi-box overlap/transmittance sweep、GPU式mirror診断、球状大気single-scattering参照を作成。Composition host dispatch不在を再確認。GPU runtime capture・採用判断は未実施）

## 目的

フォグと大気散乱を、単純な距離減衰から深度・高さ・照明・散乱を扱う表現へ段階的に発展させるため、既存のCPU参照実装、Composition向けGPU試作、3D viewport／volume向け候補を同じ入力条件で比較する。画質、時間、メモリ、入力契約を測り、制作向けの採用方式と責務境界を決定する。

本書はR&D計画であり、GPU render pass接続、UI、既定値変更、オフライン出力への自動適用を承認するものではない。実験用コードは本番経路と分離し、採用判断後に個別の実装範囲を決める。

## 用語と対象

- **Uniform / distance fog:** 一様な消散係数で視線距離に応じた透過率を計算する。安価な基準方式。
- **Height fog:** 高さで密度が変わる媒質を視線に沿って積分する。指数高さ分布なら解析積分できる領域がある。
- **Volumetric fog:** froxel等の3D媒質グリッドに消散・散乱・照明を蓄積し、視線方向へ積分する。局所密度や影との連携を検討する方式。
- **Atmospheric scattering:** 惑星半径、大気層、Rayleigh/Mie係数、太陽方向を用いる広域の空・地平線輸送。Height fogと同じ設定・パスに暗黙統合しない。

## 現状のコード証拠

親リポジトリ内のサブモジュールは調査のみとし、このR&D作業では変更しない。

| 資産 | ソース上で確認できる責務 | 未確定・不足 |
|---|---|---|
| `ArtifactCore/include/Render/AtmosphereFog.ixx` / `ArtifactCore/src/Render/AtmosphereFog.cpp` | `AtmosphereFogSettings` と CPU `AtmosphereFogRenderer`。`evaluateFog()` はheight density、Rayleigh型位相、散乱・吸収をsample積分。`applyToImage()` はdepth/color `ImageBuffer`を処理する。 | 物理単位、深度画像のencoding、camera rayの構築元、HDR/color descriptor、alpha規約、制作render pathへの接続は本調査で確認できていない。`applyToPixel()` は画素高さを固定中間値にし、`applyToImage()` はdepth値を8-bit正規化として扱うため、`evaluateFog()` と同等なheight-resolved積分とは見なさない。 |
| `Artifact/shaders/heightFogCS.hlsl` | color/depth textureとnear/far、density、height falloff、ground、start distance、fog color、camera positionを受けるcompute shader。 | コメントと式が示す通りworld position reconstructionを行わず、camera heightを全画素の代表値に使う。Artifact/ArtifactCoreの非shader sourceで当該shader名の参照を検索したが利用host・dispatch接続は見つからない。品質・時間・GPU parityは未測定。 |
| `Artifact/shaders/fogHF.hlsli` | volumetric cloud/light shader向け`GetFogAmount()` / `GetFog()`。height fogの線積分、horizon/sky色、Henyey–Greenstein型位相、atmosphere transmittanceを扱う。 | Composition一般用の独立render passではない。weather/frame定数とcloud/light shader環境に依存するため、そのままの流用可能性は未確認。 |
| `Artifact/shaders/volumetricCloud_renderCS.hlsl` | cloud raymarchのshader内で、条件付きheight fogとrealistic sky atmospheric integrationを組み合わせるコードがある。 | shaderの存在だけではアプリ側でのlive pipeline/resource bindingや一般fog rendererの再利用性を証明しない。 |
| `docs/worklog/FOG_PROGRESS_2026-07-20.md` | compute height fog試作と未対応事項（world position、local volume、shadow/reprojection、pass登録/UI、GPU検証）を記録。 | 実機出力、定量比較、現在の接続状態は未確認。 |

## R&Dの問い

1. 正規化depthだけのheight近似と、inverse view-projectionによるworld position再構成を使うheight fogで、カメラ移動・傾斜地形・上下視線の誤差はどれほど違うか。
2. 指数高さ密度の解析積分と固定step raymarchで、同一の密度・距離・高さ条件における誤差とGPU時間はどう変わるか。
3. Uniform/height fogに照明散乱（Rayleigh近似またはHenyey–Greenstein位相）を加える価値は、追加コストとノイズ・帯状アーティファクトに見合うか。
4. 局所volume、遮蔽影、temporal reprojectionを必要とする用途でfroxel方式が必要になる境界はどこか。まずscreen-space height fogで解ける要件と分ける。
5. Production composition、3D viewport、cloud/volume rendererで共有すべき数学・入力契約と、分離すべき統合／品質責務は何か。

## 比較する候補

| ID | 候補 | 役割 | 主要計測 |
|---|---|---|---|
| F0 | Fogなし | 同一入力の基準画像 | 基準画像、基準時間 |
| F1 | Uniform analytic transmittance | 最小コストの視覚基準 | 距離曲線、色差、時間 |
| F2 | Analytic exponential height integral | 指数高さ分布の参照候補 | ray高度別誤差、CPU/GPU時間 |
| F3 | Fixed-step height raymarch | 解析式が使えない密度場の比較候補 | step数別誤差、時間、banding |
| F4 | Single-scattering height fog | F2/F3に太陽照明と位相関数を加えた候補 | 方向応答、輝度、時間、安定性 |
| F5 | Froxel local volumetric fog | 局所密度・ライト影が必要な場合の候補 | froxel解像度、時間、メモリ、reprojection ghosting |
| F6 | Planetary atmosphere LUT integration | 空・地平線の大気輸送候補。height fogと別評価 | LUT構築/参照コスト、太陽高度・視線角の誤差 |

F4〜F6は順に進める探索項目であり、すべてを本番機能へ導入する前提ではない。F5は既存cloud shaderの機能をComposition fogに転用できるという前提を置かず、独立した資源・統合設計が必要か評価する。

先行研究からは用途境界を分ける根拠が得られる。Bruneton–Neyretは地上から宇宙までの視点・太陽方向と多重散乱を事前計算し、実行時の大気輸送評価をLUT参照にする。一方、Hillaireはtransmittance LUTで各散乱sampleから太陽へ向かう二重raymarchを避け、sky-view LUTとcamera-frustum aerial-perspective volumeを別用途として扱う。Wronskiのfog方式はview-frustum volumeへ散乱光と媒質密度を蓄積し、forward/deferredどちらの描画にも適用する。したがって、惑星sky/aerial perspectiveと局所lighted fogは同じ「大気」機能名の下で一つの品質・資源モデルにまとめず、用途別に比較する。

## 決定論的入力・参照値

- 外部素材や乱数に依存しない線形float RGBAのscene-linear chartを用意する。黒／白の距離マーカー、細線、高輝度点光源、色票、alpha境界、地平線シルエットを同一画像に含める。
- 深度はcamera near/farとprojection方式をmanifestへ記録し、少なくとも平面、段差、深度不連続、上下に傾いた面を配置する。depth encodingとclear値を明示し、逆投影に必要な行列・clip-space規約を保存する。
- 高さ試験では同一形状をground levelの下／近傍／上へ配置し、水平視線・上向き・下向き、カメラ高度変更を独立軸として変える。
- 単一散乱試験では太陽方向を正面／横／逆光へ固定し、位相関数`g`と照度を独立に掃引する。F0〜F3では照明条件を固定し、散乱効果と消散効果を混同しない。
- 解析積分可能な指数媒質について、倍精度または閉形式の参照透過率`T = exp(-integral(sigma_t ds))`をfixture側で計算する。候補出力は同じ深度・媒質を使い、差分画像と区間別数値誤差を保存する。
- 各captureは候補ID、設定値、入力hash、出力hash、解像度、depth/projection/color/alpha契約、backend、GPU名、warm-up回数、raw timing samplesを記録する。測定統計量だけでなく全反復を保持する。

## 評価軸と採用ゲート

### 画質・数値

- scene-linear unclamped RGB誤差（平均、最大、p95）と参照透過率誤差を候補別に集計する。
- 深度段差周囲のhalo、遠景の帯状化、カメラ移動時のscreen-space swim、時間方向のflicker、temporal reprojection ghostingを画像・連番で目視確認する。
- fog color/alphaの合成結果はpremultiplied/straightを明記し、既存色管理境界での変換を記録する。display transform後の画像だけでHDR寄与を判定しない。
- 位相・散乱を評価するときは背景だけのF1/F2と同じ透過率で比較し、明るさ増加を単なる不透明度差と誤認しない。

### 性能・資源

- 代表解像度（例: 1280x720、1920x1080）ごとにwarm-up後のGPU時間を反復測定し、中央値とp95、raw samplesを保存する。候補間でbackend、画質条件、出力解像度を揃える。
- CPU setup、GPU pass、history/reprojection、追加texture/volume memoryを分けて計測する。froxel候補はgrid寸法とslice数を明示し、隠れたresource再生成がないことをソースとcaptureで確認する。
- フレームごとの大容量allocation、texture/staging再生成、readbackを避ける。測定diagnosticは明示R&D計測時のみ有効にし、通常frameに文字列生成やflushを入れない。
- D3D12とVulkanで候補契約を確認する。片方未検証ならbackend parityを主張しない。

## CPU 数値プロトタイプ（2026-11-12）

`tools/fog_height_integral_rd.py` は、地面より下で密度一定・上で指数減衰する媒質について、ground crossingで区分した閉形式積分とmidpoint固定step積分を比較する独立Pythonスクリプトである。距離は任意単位、`rho0=0.002`、falloff `0.01`、absorption `1.0`、scattering `0.65`、sun intensity `2.0`、位相角`cosTheta=0.5`を使用。散乱は視線方向の透過のみを含み、太陽経路の消散は含めないので、数値積分の制御比較であり大気照明モデルではない。

実行結果の最大相対透過率誤差（5 rayケース中の最大値）は次の通り。散乱の最大絶対誤差も併記する。

| Midpoint steps | Max relative transmittance error | Max scattering absolute error |
|---:|---:|---:|
| 8 | 2.52134% | 0.00163918 |
| 16 | 0.648684% | 0.000419814 |
| 32 | 0.162422% | 0.000105594 |
| 64 | 0.0406676% | 0.0000264389 |
| 128 | 0.0101708% | 0.00000661224 |

最大誤差はgroundから上向くrayで発生し、全区間が指数密度の水平rayでは閉形式とmidpoint値が一致する。これは同じrayの中で密度曲率が変わる領域とstep数の関係を示すだけであり、GPU性能や許容画質を示さない。R&D capture前の初期値選定に使い、3D GPU shaderの最終step数・品質基準へ直接転用しない。

同じスクリプトに、`ArtifactCore/src/Render/AtmosphereFog.cpp::phaseRayleigh()`の式mirrorと位相関数の球面積分diagnosticを加えた。標準Rayleigh位相`3(1+cos²θ)/(16π)`の積分は`1.0`、C++ source式`0.75(1+cos²θ) * 3/(16π)`は`0.75`となり、cosθ全域でsource式は標準式の0.75倍である。したがって他の設定を固定した単純比較では`evaluateFog()`のsingle-scattering項も同率で小さくなる。これはソース上の式差であり、見た目較正や既存出力の意図を確認せず変更を推奨するものではない。また`evaluateFog()`は`sunIntensity`を定数として乗じるが、sampleから太陽までの大気透過率評価はない。CPU numeric oracleは太陽経路消散を意図的に省いているため、CPU runtime parityでなくこの既知の簡略モデルを基準にし、散乱品質の採用判定では太陽透過を含む別参照と比較する。

## 固定step積分の事前基準付き広域収束sweep（2026-11-12）

5 rayの初期比較を一般化し、`tools/fog_height_convergence_sweep_rd.py`でcamera height 6種、ray elevation 7種、距離3種、ground density 3種、falloff 4種の直積1,512条件を評価した。near-groundの折れ点を跨ぐrayも含む。各ケースで閉形式積分を参照に、uniform midpointと、既知ground crossingで区間を分割して同じ総sample予算を区間長へ配分するmidpoint方式を8〜512 samplesで比較した。単位は任意のscene unit、absorptionは0.7。数値gateは候補実行前に、全ケースでtransmittance絶対誤差`<= 0.001`かつ参照T`>= 0.1`のケースで相対誤差`<= 1%`と事前固定した。このgateは数値積分一致のみに対する工学的受入値で、画質・実機精度の保証ではない。

| Method | Steps | p95 absolute T error | Max absolute T error | Max relative T error (reference T >= 0.1) | Gate |
|---|---:|---:|---:|---:|---|
| Uniform midpoint | 128 | 0.00004978 | 0.00157449 | 0.1693% | Fail |
| Uniform midpoint | 256 | 0.00001153 | 0.00039826 | 0.04282% | Pass |
| Ground-split midpoint | 128 | 0.00005377 | 0.00157449 | 0.1702% | Fail |
| Ground-split midpoint | 256 | 0.00001345 | 0.00039826 | 0.04282% | Pass |

両手法ともこのsweepでは最初にgateを通るsample数が256だった。step64時点のuniform midpointの最大絶対誤差はfalloff`0.005 / 0.02 / 0.08 / 0.2`の順に`0.00002182 / 0.00033311 / 0.00227427 / 0.00601578`で、高falloffほど同じstep幅で密度が急変することが主な難条件になった。ground crossingで機械的に分割しても全体worst caseは改善しなかった。worst caseは地面から上向くray（y0=0、+75°、500 units、density=0.02、falloff=0.2）で、境界から開始するため内部crossing splitの対象外である。内部crossingケースに限るとstep64のmax errorはuniform`0.00142022`、ground-split`0.00515370`となり、この単純な区間長配分は一部で悪化した。急な上空密度勾配へsampleを優先配分する設計ではなく、一般的な適応積分の有効性を否定する結果でもない。

結果は`docs/worklog/artifacts/fog-height-convergence-rd-2026-11-12/manifest.json`と全21,168行の`raw_cases.csv`に保存した。これはCPU倍精度式と独立Python midpointの比較であり、shader float誤差、実GPU時間、解像度別pixel cost、画面上のbanding・halo・時間安定性は測定していない。したがって256 samplesを製品step数として推奨せず、指数高さ媒質で解析積分が利用できる範囲では固定raymarchを採用する根拠にしない。

## 密度曲率に基づくsample配置とsingle-scattering追試（2026-11-12）

固定step sweepの高falloff条件を対象に、composite midpointの局所誤差近似`|f''| h^3`から、interval densityを`rho^(1/3)`に比例させるsample配置を事前に導いた。cell境界は累積`rho^(1/3)`積分を等分して逆算し、ground crossingでは区間を分ける。ground下の消散は一定密度なので解析的に処理し、上側区間へdensity sampleを寄せる。数値gateは前sweepと同一である。

消散のみの1,512条件では、uniform midpointとground-split midpointは256 samplesでgateを通過したが、`rho^(1/3)` weighted midpointは32 samplesで通過した。32 samples時の最大絶対T誤差は`0.00034297`、p95は`0.00007309`、最大相対T誤差（reference T>=0.1）は`0.08072%`。uniform midpoint 32の最大絶対誤差`0.02033185`に対し、weighted 32では約59倍小さい。16 samplesはmax absolute error `0.00137199`でgateを通過しない。これは既知の指数高さ媒質を使ったCPU数値積分だけの結果で、uniform 256対weighted 32のsample配置・誤差比較であり実行時間の8倍改善を意味しない。

次に同じ密度配置を単一散乱の視線積分へ適用した。camera height 3種×view方向3種×太陽高度5種の45条件で、analytic view/sun transmittanceを使うsingle-scattering integrandを比較した。基準はuniform midpoint 32,768 samplesとし、16,384との最大relative RGB L2差は`7.15e-9`だった。事前gate `relative RGB L2 <= 0.1%`に対し、uniform midpointは64 samples、density-weightedは32 samplesで初めて全45条件を通過した。32 samples時のworst relative RGB L2はuniform`0.2493%`、weighted`0.08741%`。weighted方式のworst caseはcamera y=2 m、上向きview、太陽高度80°で、density weightingだけでもview attenuationとsun-path attenuationの積を含むこの限定fixtureでは数値誤差を下げた。

このsingle-scattering結果もhigh-step CPU quadrature参照に対する数値比較に限る。重み則は密度のみを見ており、遮蔽shadow・local volume・多重散乱・時間変化の誤差分布には適合させていない。非uniform boundaryの逆写像は各cellで`log1p`等を必要とし、同じdensity sample数でもGPU instruction costは増える可能性がある。GPU時間、float精度、実画像、backend差は未測定のため、GPU候補の採用や既存shader変更の根拠とはしない。消散比較の全raw casesは`docs/worklog/artifacts/fog-height-sampling-distribution-rd-2026-11-12/`、散乱追試は`docs/worklog/artifacts/fog-scattering-sampling-rd-2026-11-12/`に保存した。

## Shadow visibility境界stress sweep（2026-11-12）

density-only sample配置の適用限界を調べるため、`tools/fog_shadow_sampling_rd.py`にcamera height 3種×view方向3種×太陽高度3種×binary shadow pattern 7種＝189条件を作成した。shadowはray長に対する区間として固定し、無遮蔽、ray長の4%幅を早期／中央／後期に置く条件、25〜30%幅の広いshadowを3位置に置く条件を含む。実際のoccluder geometryやshadow mapを使わず、visibility不連続だけを独立させた stress fixtureである。

各caseの基準はshadow境界とground crossingに区間分割を合わせた上でlit区間へuniform 16,384 midpoint samplesを置いた。8,192 samplesとの最大relative RGB L2差は`2.86e-8`。gateは既存single-scattering比較と同じrelative RGB L2`<= 1e-3`とし、189条件すべてで判定した。

| Method | Steps | Max relative RGB L2 | Gate |
|---|---:|---:|---|
| Uniform midpoint over full ray | 128 | 0.7376% | Fail |
| Uniform midpoint over full ray | 1,024 | 0.1154% | Fail |
| Uniform midpoint over full ray | 2,048 | 0.02886% | Pass |
| `rho^(1/3)` density-only placement | 128 | 1.0584% | Fail |
| `rho^(1/3)` density-only placement | 1,024 | 0.1807% | Fail |
| `rho^(1/3)` density-only placement | 2,048 | 0.06760% | Pass |
| Exact shadow/ground boundary split + density weighting | 32 | 0.08741% | Pass |

32-step density-only方式の最悪値はupward viewの早期25% shadow（camera y=2 m、sun elevation80°）で`7.414%`となり、無遮蔽caseだけで得た`0.08741%`より大きい。shadow間隔で集計すると、同じ32 stepsのmax relative errorはuniformで中央narrow shadow`2.4109%`、density-onlyでearly wide shadow`7.4144%`だった。均一配置も境界をstep cellで部分被覆するため誤差が大きく、density-only配置は影で寄与しない区間へsample予算を割く場合がある。shadow境界・ground境界へcell edgeを一致させ、lit区間のみに`rho^(1/3)` sampleを配る候補は32 stepsで全caseを通過した。

このshadow-aware結果は境界位置が完全に既知という楽観的上限であり、実rendererでshadow mapの不連続位置を前計算できることを示さない。shadow-aware boundary detectionの追加コスト・誤差、filteringされたpenumbra、視点移動時の時間安定性は未評価である。raw 5,103 case、shadow pattern別統計、script hashは`docs/worklog/artifacts/fog-shadow-sampling-rd-2026-11-12/manifest.json`と`raw_cases.csv`に保存した。ここから支持されるのは密度だけに基づくquadratureが遮蔽不連続の誤差を制御しない点までであり、実GPU fogやshadow品質の採用判断ではない。

### Shadow境界位置誤差への感度（2026-11-12）

上記の厳密境界候補を、ray長の0.1%、0.5%、1%、2%だけ区間ごと平行移動した推定境界で再評価した。189条件×9境界仮説（exact、各誤差幅のunder/overestimate）×7 sample数＝11,907結果を保存した。referenceと事前gateは前sweepと同一。underestimateでは真の遮蔽より早くlitと予測するためshadow内sampleが増え、overestimateでは真にlitな区間を予測shadowとして省略し、その寄与を失う。したがってstep数を増やしても境界誤差由来のbiasは消えない。

exactは32 samplesでgateを通過した。一方、位置を0.1%ずらしただけでも32 samplesのworst errorはunderestimate `0.7187%`、overestimate `0.8568%`で、最大2,048 samplesでもそれぞれ`0.2105%`、`0.7831%`となりgateを通過しなかった。2% overestimateは2,048 samplesでも最大`14.5524%`。このstress fixtureでは厳密境界候補の有効性がedge位置精度に強く依存する。数値はray方向へ同じ割合で平行移動する合成binary intervalの結果であり、実shadow-mapの位置誤差分布や実装可能な境界推定精度を示さない。完全なrawとoffset別集計は`docs/worklog/artifacts/fog-shadow-boundary-sensitivity-rd-2026-11-12/manifest.json`に記録した。

数値差を視覚領域へ結び付けるため、既存合成sceneの96×54版でF4 single scatteringのlinear RGB image fixtureを追加した。camera y=4 m、太陽高度45°、同一のsource/depth/rayを使用し、uniform 4,096 midpointをreference、2,048 midpointとの差でreference収束を確認した（relative image RGB L2 `7.09e-9`）。各pixelのview/sun transmittanceは解析式、散乱radianceのみを積分する。uniform 32、uniform 64、`rho^(1/3)` weighted 32のreferenceに対するwhole-image relative RGB L2はそれぞれ`3.87e-5 / 9.67e-6 / 2.80e-5`で、全て事前gate`1e-3`以内。weighted 32はuniform 32よりRMSEが`1.86e-5`から`1.35e-5`へ約27.6%減ったが、uniform 64のRMSE`4.65e-6`より大きい。全候補で最大channel差が`1e-3`を超えるpixelは0だった。高contrast edge mask（1,357 pixels）のRMSEはuniform32 `3.45e-6`、weighted32 `2.95e-6`、near-horizon mask（1,620 pixels）は`4.27e-6`対`3.97e-6`。HDR marker maskは77 pixelsで両32-step方式の差は約`1.06e-8`と小さい。

reference・候補・center cropをPFMとpreview PNGで保存し、raw linear absolute difference PFMとdisplay-only gain 4096の差分PNGを分けた。full image／center cropの目視では大域的な色・形状差は判別しにくく、増幅差分で見える残差は主に地平線・遠景silhouette近傍に限られた。これはopaque synthetic scene、単一太陽高度、単一解像度でのCPU画像評価である。透明境界・動くcamera sequence・実素材・production color management・GPU表示とのparityは対象外／未検証であり、sample配置の製品品質承認には使わない。captureと全region metricは`docs/worklog/artifacts/fog-scattering-sampling-image-rd-2026-11-12/manifest.json`に記録した。

### Penumbra proxyと遮蔽区間幅の推定誤差（2026-11-12）

追加sweepの条件を事前固定した。前段と同じ189 ray条件・媒質・shadow pattern・relative RGB L2 gate `<= 1e-3`を使い、penumbra幅をray長の0／0.2／1／4%、推定core幅誤差を真値比`-2% / -0.5% / 0 / +0.5% / +2%`とした。penumbraはcore外側の可視度がcubic smoothstepで0から1へ遷移する解析proxyである。幅誤差ありは固定128 samplesで感度比較し、誤差ゼロのみ32／128／512／2,048 samplesで収束を測った。baseline、`rho^(1/3)` density-only、推定profileのnonzero領域・knotに合わせ`rho^(1/3)*visibility^(1/3)`でsample配分する候補を同じgateで比較した。referenceは真のprofile境界とground crossingを区切る16,384-step midpoint、8,192-stepとの差は全189条件・全penumbra幅で最大`2.86e-8`だった。

penumbra 0%ではuniform／density-onlyが初めてgateを通るのは2,048 samples、厳密visibility-knot候補は32 samplesだった。4% penumbraではuniform／density-onlyとも128 samplesで通過する。幅推定誤差があるvisibility-knot候補は、hard shadowで±0.5%でも128 samples時の最大誤差が`0.488% / 0.494%`とgateを超え、±2%では`0.732% / 1.951%`だった。penumbra幅4%では同じ±2%の誤差でも最大`0.0130%`以下となり、128 samplesでgate内に入った。このproxyでは滑らかな遷移が区間幅誤差への感度を下げたが、遷移が幅推定誤差を常に吸収するわけではない。各条件のp95・最大値・raw radianceはartifact manifest/CSVに保存した。

本結果は区間幅が中心対称に変化する仮説であり、前段の境界位置ずれとの複合誤差は未評価。cubic proxyは光源サイズやoccluder geometryから導いたpenumbraではなく、shadow filtering、実画像、temporal stability、GPU時間・命令数も未計測である。visibility knotとprofileが既知の理想化候補である点も維持する。したがってこれは積分器の誤差感度を示すCPU数値結果で、production shadow/fog品質や方式採用の証拠ではない。

### Shadow中心位置とcore幅の複合推定誤差（2026-11-12）

位置ずれと幅誤差の単独sweepを受け、相互作用を同時評価した。前段と同じ189条件、4 penumbra幅（ray長0／0.2／1／4%）、5 center offset（0／±0.5／±2% ray長）、5 core幅誤差（0／±0.5／±2% relative）を直積にし、visibility-weighted候補を128 samplesで比較した。合計18,900結果。合否gateは各組み合わせの全189条件でrelative RGB L2 `<= 1e-3`。16,384-step referenceと8,192-step validationとの差は最大`2.86e-8`だった。offsetおよび幅誤差は[0,1]へclipするためray端に接するpatternではclip後の区間幅が指定誤差から変わる場合がある。raw CSVにはclip後の推定区間も保存した。

hard／0.2%／1% penumbraでは各25組み合わせ中、gateを通ったのはcenter・幅ともexactの1組だけだった。4% penumbraでは25組中14組が128 samplesで通過し、許容域はcenter offsetが−0.5%または0〜+0.5%に限られ、±2% center offsetは通らなかった。中心exactでも4% penumbraでcore幅誤差±2%を通過した。したがってこのfixtureでは広いsoft transitionが推定誤差を緩和するが、中心位置ずれが大きい場合は幅を正しく見積もっても頑健性を回復しない。過小／過大幅が位置ずれを常に相殺するような単純な傾向も見られず、組み合わせごとの判定表をartifactに残した。

本結果はcubic smoothstep visibilityと独立に設定した中心／幅誤差のCPU stress fixtureである。実shadowの同時誤差分布、幾何・ライトサイズ由来penumbra、shadow-map filtering、時間安定性、画像品質、GPU performanceは未検証であり、製品方式の採用判断に使わない。

### Shadow boundary errorの画像空間評価計画（2026-11-12）

数値ray sweepの誤差を画像へ投影するため、開始前に次の条件を固定する。素材は既存`fog_height_image_fixture_rd.py`のopaque scene-linear chartを96×54で再生成し、camera・FOV・source RGB・ray距離・mediumを全候補で共通にする。screen-space ellipse内の各rayにpixel座標で変化する正規化shadow coreを置き、真のpenumbra幅をray長の1%、candidateは中心をray長の+0.5%、core幅を+0.5%誤推定する。太陽高度は45°。参照はprofile knotとground crossingで区切ったuniform midpoint 4,096 samples、2,048 samplesを参照収束validationとし、relative RGB L2 `<= 1e-3`を数値積分gateとして事前固定する（画像品質承認gateではない）。

比較候補はuniform32、density-only weighted32、厳密profile-weighted32／128、誤推定profile-weighted128。PFMのlinear RGBを測定元、Reinhard+gamma PNGを目視用に分離し、全体とshadow interior、projected edge band、影外、near-horizon、HDR source、高contrast source-edgeをregion mask評価する。各領域のrelative RGB L2、RMSE、mean absolute RGB、p95・最大channel差を保存し、raw absolute-difference PFMとgain 4096 previewを出す。alpha、実shadow-map、連続フレーム、GPU/backend、処理時間・memoryはこのfixtureでは未評価として記録する。計画と入力hashは`tools/fog_shadow_image_rd.py`および出力manifestへ残す。

### Shadow境界推定誤差の画像fixture結果（2026-11-12）

96×54のopaque scene-linear chartにscreen-space ellipse（中心`(49,27)`、半径`(27,14)`）のshadow silhouette proxyを重ね、各shadowed rayのcore位置・幅を画素座標に応じて決定的に変えた。全候補でsource RGB、ray距離、medium、camera、太陽高度45°は同一。reference 4,096とvalidation 2,048のimage-relative RGB L2差は`7.06e-9`。PFMはlinear float RGB、PNGは共通Reinhard+gamma preview、差分PNGは線形絶対差へ4096倍をかけた表示専用である。

事前gate`1e-3`に対し、uniform32とdensity-only32はwhole imageではそれぞれ`6.82e-4`、`5.15e-4`で通過したが、projected shadow edge band（860 pixels）では`1.458e-3`、`1.095e-3`となり超過した。厳密profile-weighted32はwhole image`2.78e-5`、edge band`1.83e-5`、厳密profile-weighted128はそれぞれ`1.78e-6`、`1.42e-6`。中心+0.5%／幅+0.5%誤差のprofile-weighted128はwhole image`6.00e-5`、shadow edge band`1.25e-4`、interior`4.19e-5`でgate内だったが、厳密profile128より誤差が約34倍大きく、edge bandでは約89倍大きい。増幅差分の目視でも残差は主に楕円silhouette境界に集中した。これは意図したscreen-space shapeに沿った数値残差で、実shadowの見た目・許容性を証明しない。

全体・region画像、center crop、raw PFM差分、増幅previewと各regionのpixel分布統計は`docs/worklog/artifacts/fog-shadow-image-rd-2026-11-12/manifest.json`へ記録した。alpha、時間方向、実素材、実shadow-map、GPU/backend、性能は依然未検証であり、画像fixtureはsample配置の採用承認ではない。

### AABB core幅誤差±2%の画像空間追試結果（2026-11-12）

開始前登録した固定96×54 fixtureでcenter exact、core width error −2%/+2%、candidate 128 samplesを比較した。reference 4,096／validation 2,048の画像relative RGB L2差は両方`7.055e-9`。whole-image relative RGB L2は−2%=`7.482e-5`、+2%=`7.438e-5`。projected edge band（860 pixels）のrelative L2は`1.556e-4 / 1.546e-4`、absolute max-channel p95は`2.3166e-4 / 2.3062e-4`、maxは約`3.28e-4`で、全pixelでmax-channel差`1e-3`超過は0だった。shadow exterior（3,416 pixels）のrelative L2は両符号とも`2.315e-6`。このproxyではwhole-image平均的な指標よりedge bandの相対誤差が約67倍大きいが、閾値は設けていない。

共通Reinhard+gamma画像とgain-4096差分previewを目視すると差分は主に楕円shadow境界帯へ現れ、−2%/+2%の見た目はこの96×54表示では近かった。画像誤差vectorの符号分解ではwhole-image odd/even RGB RMSEが`3.575e-5 / 8.452e-7`、minus/plus残差vector cosineは`-0.99890`。edge bandでもodd/even=`8.010e-5 / 6.882e-7`、cosine=`-0.99987`で、誤差vectorの大半は幅誤差符号を反転すると反転した。これは人によるproduction-quality approvalではなく、screen-space ellipse proxyと静止画像の確認に限る。±符号間の誤差量は近いが、実AABB形状・shadow-map、時間ちらつき、alpha、GPU/backend、preview/final、performanceを評価せず、幅誤差の符号優劣や許容性を結論しない。PFM、全体／70×42 crop、gain-4096 raw/display差分、領域統計、符号vector分解、入力・script・output hashは`docs/worklog/artifacts/fog-shadow-width-signed-image-rd-2026-11-12/manifest.json`へ保存した。

### 体積固定AABB aspect ratioのray-cast画像追試結果（2026-11-12）

固定camera/sceneの96×54 ray-castで、体積33のAABB三形状すべてをslab intersectionから直接求めた。projected shadow pixel数はbaseline 896、X長/Z薄1,344、X短/Z厚700（全5,184 pixelsの17.3% / 25.9% / 13.5%）。shape reference imageをbaseline referenceと比べたlinear RGB relative L2はそれぞれX長/Z薄`1.326e-3`、X短/Z厚`1.482e-3`。silhouette XORは448／316 pixels、baseline/shape edge union上のrelative L2は`1.795e-3 / 2.835e-3`だった。同体積でも固定viewへ投影されたvisibility supportは同一でない。

reference 4096対validation 2048の最大whole-image relative RGB L2は`4.614e-9`。true intervalをそのまま使う128-step profile-weighted候補のreference差は3 shapeで`1.195e-6`前後、max-channel差`2.42e-6`以下、`1e-3`超過pixelは0だった。shape-reference間の全画面差はこのfixtureでは候補積分誤差より約3桁大きい。目視した共通tone-map referenceとgain-4096差分では、ellipseでなくray-castされた矩形状のsilhouetteと周辺散乱差が見えるが、表示は96×54の合成sceneであり、shapeの品質順位や実shadowの知覚差ではない。shape別PFM/preview、candidate-reference積分差、shape-baseline差、silhouette masks/region統計、scriptとinput hashは`docs/worklog/artifacts/fog-shadow-box-aspect-image-rd-2026-11-12/manifest.json`に保存した。

### Camera translation中のshadow quadrature時間残差評価計画（2026-11-12）

静止画像で誤差がshadow境界へ局在したため、横方向に移動するcameraとworld-fixed shadow-volume proxyで7-frame CPU sequenceを事前定義する。解像度は64×36、camera xは`-0.30`から`+0.30` world unitsまで0.10刻み、targetは固定。shadowはscene内の固定AABBとのray intersectionで決め、penumbra幅はray長の1%。candidateは前回同様center +0.5%／core幅+0.5%の誤差を与えた128-step profile-weighted積分、基準candidateは厳密profile 128-step、referenceはprofile/ground aligned 2,048-step、validationは1,024-step。

各frameのrelative RGB L2、shadow interior/edgeのp95・最大差を記録する。時間残差は各frameの`candidate - reference`画像を作り、隣接frame間でこの残差が同じscreen pixelにどれだけ変化するかを全画面と両frameのprojected-edge mask union上でRMSE/p95/max集計する。これはmotion-vector再投影をしないscreen-space proxyなので、production flicker許容gateは設定せず、参照自体のcamera-motion差と候補誤差の時間変化を分けて保存する。各frameのreference、exact、estimated PFM/previewと条件をartifactへ保存する。alpha、実shadow-map、GPU/backend、runtime性能は未検証とする。

### Camera translation sequenceの時間残差結果（2026-11-12）

64×36、7 frameのCPU sequenceでcamera xを−0.30から+0.30 world unitsへ0.10刻みで移動し、固定targetへ向け直した。world-fixed AABB `(-2.2,1.0,7.5)–(2.2,6.0,9.0)`と各camera rayのintersectionを使ってshadow intervalをframeごとに再計算した。reference 2,048／validation 1,024の最大per-ray relative RGB L2差は`6.35e-8`。厳密profile-weighted128の平均frame-to-frame residual RMSEは全画面`2.22e-8`、shadow edge union`1.11e-8`。中心+0.5%／幅+0.5%推定誤差候補は全画面`2.02e-6`、edge union`6.83e-6`で、edge上の時間残差は厳密profile候補より大きかった。誤差候補のper-frame image relative RGB L2は`1.77e-5–1.82e-5`の範囲だった。

時間残差は各frameの`candidate-reference`を隣接frame間で同じscreen pixel上比較した値であり、実sceneの画面変化そのものではない。motion-vector reprojection、実GPU temporal filtering、shadow-map rasterizationは使っておらず、合否閾値も設定しない。各frameのreference・candidate・absolute difference PFM/preview、per-frame region統計、連続frame residual統計とcapture hashは`docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/manifest.json`に保存した。この結果は固定AABBのCPU proxy内で境界推定誤差が時間方向へ変化する証拠に限る。

### 体積固定AABB aspect ratioのcamera translation評価計画（2026-11-12、実行前登録）

静止画で観察した等体積shape別silhouette差がcamera translationでどう推移するかを、既存7-frame camera sequenceへ拡張して測る。解像度64×36、camera x `-0.30…+0.30`（0.10刻み）、fixed target、既存camera scene/sun/medium/penumbraを維持し、center `(0,3.5,8.25)`の3 shape（baseline、X長/Z薄、X短/Z厚）をslab intersectionで個別に計算する。referenceは各frame/shapeのtrue interval 2,048 samples、validation 1,024、exact true-interval profile-weighted candidate 128 samplesとする。center/width推定誤差を入れず、shapeごとの静止・移動silhouetteと積分残差を切り分ける。

各frameでshape別shadow pixel count、edge mask、pairごとのmask XOR/IoU、world-scene reference imageのsame-screen-pixel motion RMSEを記録する。時間方向のquadrature residualはframe別`candidate-reference`を作り、その隣接差を全画面と両frame edge-unionでRMSE/p95/max集計し、reference motion量と別表にする。これはsame-screen-pixelの記述量でmotion compensationを含まない。数値gateやshape順位は置かず、reference-validation差が解釈を支えるか併記する。PFM/preview、frame/pair metrics、raw CSV、shape/camera/medium/hashを保存する。alpha、production flicker、実shadow-map、GPU/backend、runtime性能は未検証のままとする。

### 体積固定AABB aspect ratioのcamera translation結果（2026-11-12）

既存camera pathの7 frame（x=`−0.30…+0.30`、0.10刻み）、64×36、fixed target、同じscene/sun/mediumを使い、3 shapeをframeごとにslab ray intersectionした。reference 2,048／validation 1,024の最大whole-image relative RGB L2は`1.962e-8`、最大pixel relative L2は`6.347e-8`。projected shadow pixel数はbaselineが`418/418/418/440/418/418/418`、X長/Z薄`564/567/588/588/588/567/564`、X短/Z厚`299/299/299/322/299/299/299`。隣接silhouette IoUはbaseline最小`0.9500`、X長/Z薄最小`0.9643`、X短/Z厚最小`0.9286`（各6 pair）。shape reference imageのbaseline比relative L2はX長/Z薄`0.001333–0.001367`、X短/Z厚`0.001510–0.001554`で、shape別の投影差は全sequenceに保たれた。

true interval exact-profile 128 candidateのquadrature residualを隣接same-screen pixelで差分すると、mean RMSEは全画面でbaseline/X長Z薄/X短Z厚=`2.223e-8 / 2.250e-8 / 2.228e-8`、edge union=`1.115e-8 / 1.075e-8 / 1.237e-8`。一方reference image自体のsame-screen motion RMSEは全画面約`0.158`、edge union約`0.169–0.184`だった。従ってこのCPU fixtureでは、観測された通常のscreen-space画像移動と数値quadrature residualの時間変化は別の量で大きさも大きく異なる。silhouette移動とcandidate residualを分離できたが、screen-space差分はmotion compensationなしの記述量であり、production flicker評価ではない。frame capture、silhouette IoU/XOR、reference motionとresidual delta別のpair metrics、CSV、hashは`docs/worklog/artifacts/fog-shadow-box-aspect-camera-sequence-rd-2026-11-12/manifest.json`に保存した。

### 既知world point再投影による時間残差評価計画（2026-11-12）

screen-pixel差分のcamera-motion混入を切り分けるため、既存7-frame sequenceの中央frameで見える非sky surface pointをworld座標として採り、全隣接frameへ既知camera行列で再投影する。surface labelが4近傍で一致し、source RGB max-channel jumpが0.35未満の点だけを採用し、各隣接pairで画面内にありnearest surface label一致かつray距離差0.05 scene unit以内の点を残す。PFM scene-linear RGBをprojected coordinateでbilinear sampleする。比較量はworld pointにおける`candidate-reference` residualの隣接frame差分で、全点・shadow volume interior・volume silhouette edge・clear領域およびsurface別にRMSE、p95、maxを集計し、reference自体のappearance-motion deltaも併記する。合否閾値は置かず、厳密profile候補と推定境界候補の時間残差を比較する。これは既知geometry/calibrationの合成proxyでありoptical flowやproduction flicker測定とは扱わない。

### Known-world-point reprojectionの結果（2026-11-12）

中央frameから708のstatic non-sky world pointを抽出し、surface label不一致や4-neighbor source color jump `>= 0.35`の点を除外した。隣接frameごとに既知camera poseで再投影し、nearest surface label一致かつ距離差`<= 0.05`の点だけをbilinear sample比較へ含めた。6隣接pairの可視point数は619〜662、集計は3,885 point-pairである。shadow silhouette edge proxy群は264 point-pair。

同じworld pointで`candidate-reference`残差のframe差分を取ると、厳密profile候補は全点RMSE`6.28e-9`、p95 max-channel delta`2.34e-8`、最大`5.96e-8`。推定境界候補はRMSE`1.16e-6`、p95`6.72e-8`、最大`2.22e-5`。shadow edge proxyでは厳密候補RMSE/p95/maxが`8.72e-9 / 2.94e-8 / 5.60e-8`、推定候補は`4.06e-6 / 1.43e-5 / 2.22e-5`となった。p95に比べmaxが大きく、残差変化は少数の影境界対応点へ集中している。これは前回screen-space同位置比較よりcamera motionを分離したが、最大差は境界近傍で残り、局所的なprofile推定感度を裏付ける。

これは既知の合成geometry/camera calibrationを使った再投影であり、optical-flow品質や製品motion-vectorの性能ではない。bilinear sample、surface visibility tolerance、subpixel chart textureも誤差要因であり、知覚上のflicker判定やproduction acceptanceには使わない。raw 7,770 row correspondenceとsurface/region別集計は`docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/world_reprojection_manifest.json`および`world_reprojection_pairs.csv`に保存した。

### 時間残差外れ値の連続ray再評価計画（2026-11-12）

再投影統計の局所外れ値を切り分けるため、estimated candidateの最大channel residual delta上位16件からpair/world point重複を除いたoutlierを選ぶ。各pointを両camera poseから直接結ぶcontinuous rayで再評価し、AABBとの真のshadow interval、exact profile 128-step、推定center+0.5%／width+0.5% 128-step、profile-aligned 8,192-step referenceと4,096-step validationを同じrayで比較する。image-sequence PFMからのbilinear residual deltaとcontinuous-ray residual deltaの差をscreen sampling＋reference discretization寄与proxyとして保存し、4096/8192 referenceの収束差も明記する。ここでは局所原因の診断のみを行い、視覚gateや採用判断は追加しない。

### 時間残差上位点のcontinuous-ray切り分け結果（2026-11-12）

再投影残差上位16件を固定world pointから各cameraへ直接伸ばした連続rayで再評価した。各rayのshadow AABB intersection、exact profile 128-step、推定profile 128-step、8,192-step reference、4,096-step validationを計算した。continuous referenceのvalidation差は最大relative RGB L2`2.31e-11`で、top-point再評価では参照刻みの影響は小さい。

上位16件中11件はscreen-PFM bilinear reprojection residualがdirect continuous-ray residualの10倍を超えた。最悪の例はnear chart上のworld point`(1.6863, 7.1852, 12.1686)`、camera x=0.0→+0.1のpairで、真のintervalは両frameとも`[0.61634, 0.62790]`と同じなのに、screen-space temporal residualは最大channel`2.22e-5`、direct rayでは`3.34e-8`だった。このoutlierは積分候補の時間変化でなく、pixel-center rayとbilinearなscreen image error-fieldの不整合が主因である。

残るoutlierにはdirect rayでも大きな差が残った。near-chart world point`(-3.0610, 4.9003, 11.6939)`ではcamera移動によりtrue interval endが`0.71873→0.72763`へ動き、direct estimated residual delta最大`2.14e-5`、screen値`2.15e-5`となり、投影補間だけでは説明できないray interval変化と積分近似の寄与があった。top-16のscreen-direct差のmax-channel平均/p95/maxは`1.37e-5 / 3.26e-5 / 3.26e-5`。exact-profile候補では同じscreen-direct差maxが`7.22e-8`に留まった。

したがって前段のscreen-space temporal residualのmaxをそのまま「fog積分器が作るflicker」とは解釈できない。境界認識で候補誤差fieldが空間的に急変する影響と、実ray intervalが変わる条件を分けて扱う必要がある。今回のcontinuous-ray診断は上位外れ値16件の原因切り分けに限り、全populationの寄与比率や視覚的ちらつき閾値を示さない。pair/world point、両frameのtrue/estimated interval、screen/direct residualと誤差差分は`docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/temporal_outlier_manifest.json`および`temporal_outlier_continuous_ray.csv`に保存した。

### Continuous-ray外れ値のsample/推定誤差sweep計画（2026-11-12）

continuous-rayで積分近似の寄与が確認された点について、上位16 pair/world-pointを両frameに展開して最大32 raysを再評価する。reference 16,384、validation 8,192 midpointとし、estimated interval中心ずれ`0 / ±0.5% / ±2%` ray length、core幅誤差`0 / ±0.5% / ±2%`、sample count`32–4,096`を直積評価する。各組み合わせで全対象rayのrelative RGB L2 max/p95と`<=1e-3` gateを記録し、gateへ必要な最小step数を出す。これは上位outlierに条件付けたdiagnosticなので一般ray populationの合否推定には使わない。reference convergence差もray別に確認し、screen-PFM再投影誤差は持ち込まず直接積分値だけを比べる。

### Temporal outlierのcontinuous-ray convergence sweep結果（2026-11-12）

直前のtop-16 outlierを両camera frameのdirect rayへ展開し、32 rayでcenter offset 5種×core width error 5種×step数8種＝6,400候補を評価した。真値referenceは16,384 samples、8,192 validationとの差の最大relative RGB L2は`4.11e-11`。gateは従来通り全32 raysのmax relative RGB L2`<= 1e-3`。集計では最初にgateを通るstepと、そのstep以降の全swept stepでgateを維持するstable passを別々に出した。

center offsetが±2% ray lengthでは幅誤差をどの値にしても4,096 samplesまでgateを通らず、最大relative RGB L2は約`1.61–1.72%`へ収束した。サンプル増加で残るのは推定範囲のずれによるbiasである。center exactならcore width error±2%は32 samples時点からgateを維持し、4,096 samples時のmax errorは`1.29e-5`以下だった。この選択ray集合では、境界の中心位置誤差がcore幅誤差より強い制約となった。

center offset±0.5%・width error 0では初回32 samples時にgate内となるものの、後続stepで誤差が閾値を超え、4,096 samplesでも最大`0.103–0.104%`で僅かに未達だった。±0.5% centerと−2% widthの組み合わせは512 samplesからstable passとなる一方、同じcenterとexact widthではstable passしなかった。つまり低stepでの初回passは収束を示さず、誤差補償もparameter combination依存で一般化できない。manifestには全stepのmax/p95、first-passとstable-pass双方を記録した。

本sweepはscreen-space PFMを通さず、既知のsynthetic AABB intervalを使った上位outlier条件付き数値診断である。ray population全体、実shadow filter、視覚許容値、GPU性能の採用判断には外挿しない。6,400 rowとstep別集計は`docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/outlier_convergence_manifest.json`および`outlier_convergence_sweep.csv`に保存した。

### 可視shadow ray母集団の誤差×sample sweep計画（2026-11-12）

top temporal outlierに条件付けた前回結果を全画面母集団へ外挿しないため、同じ決定論的7-frame camera sequence（64×36、pixel-center ray、world固定synthetic AABB）から、最寄りscene surfaceより手前でAABBと交差する全rayを抽出する。全対象rayでcenter offset `0 / ±0.5% / ±2%` ray length × core relative width error `0 / ±0.5% / ±2%` の25組を128 samplesで評価し、exact interval baselineのstep収束を32/128/512/2,048 samplesで測る。推定誤差×sample countの相互作用は、frame/pixel位置・AABB silhouette edge/interior・nearest surface種別で層化した再現可能な代表subsetに25組×32/128/512/2,048を適用する。真のshadow interval/profileを積分側で用い、estimated intervalはsample allocation/profile knot配置だけに使う。reference 4,096 midpointとvalidation 2,048の差をray別に報告し、基準精度がgateに比して不十分なら結果を未確定とする。誤差はper-ray relative RGB L2で、全visible-ray populationとsilhouette edge部分集合についてp95/max・gate超過数を集計する。gateはprior stress studyから継承する全ray max `<=1e-3`で、製品の視覚許容値とはみなさない。frame別集計、層化subset、ray inventory、raw rowを保存する。入力fixtureは同sequence scriptからのcamera/scene/AABB/penumbra設定で、外部素材なし。screen image補間・GPU・製品shadow、実時間・視覚的ちらつきは対象外。候補評価前の計算量見積りで元の全直積が長時間化すると判明したため、変更を実測前に記録した。初回実行はreference出力前に中断し、成果物はない。

### 可視shadow ray母集団sweep結果（2026-11-12）

抽出した可視AABB rayは7 frame合計2,948本（frame別418/418/418/440/418/418/418）、うちsilhouette edge mask内548本、interior 2,400本。reference 4,096対validation 2,048の最大relative RGB L2差は`1.19e-9`で、事前gate`1e-3`より十分小さい。exact interval baselineは32 samplesで全2,948 rayがgate内（p95 `2.84e-6`、max `4.06e-6`）、128でmax `2.45e-6`、512で`1.71e-7`、2,048で`2.93e-9`だった。これは既知intervalを用いるCPU積分器のfixture上の収束である。

全populationの25通りcenter×width誤差gridは128 samplesで測定した。centerずれ±2% ray lengthでは全2,948 rayがgateを超え、p95/maxは最大`1.844% / 1.866%`。center exact・core width +2%では2,121/2,948 ray（72.0%）がgate超過し、p95/max `0.1423% / 0.1458%`。silhouette edge部分集合548 rayに限ると同条件max `0.0697%`でgate内、interior 2,400 rayは2,121 rayが超過した。したがってこのsceneの推定width誤差はsilhouette隣接画素よりshadow interiorで大きく表れた。層化subset 84 raysでは誤差×sample countも記録し、center exact・width +2%は32/128 samplesで15/84・17/84 rayが超過し、512/2,048ではgate内となった。このsample依存は選んだsubsetに限り、全populationの収束保証にはしない。

結果はrelative RGB L2の数値比較で、linear CPU radiance integrals。gateはstress-study用の数値閾値であり、視覚・時間安定性・性能の受入値ではない。sceneは合成AABBとray長の1% cubic smoothstepだけを持ち、実shadow map/penumbra filterを表さない。frame別全25組、region集計、subset、全ray inventoryと91,792 raw rowsは`docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/population_convergence_manifest.json`および`population_convergence_sweep.csv`に保存した。製品GPU経路、実画像、subpixel coverage、alpha、motion reprojection、性能は未評価。

### 可視shadow誤差fieldの空間分布計画（2026-11-12）

全画面raw rowから128-sampleの誤差fieldを再構成し、center ±2% ray length／width ±2%の固定4候補をframe別64×36 linear mapとして保存する。誤差指標は既存と同じper-ray relative RGB L2、表示mapは固定gain 50（2%で白）・shadow外blackとし、region/frame別p95/max・gate超過数・top座標を併記する。screen座標のframe差はcamera motionと投影shadow silhouetteの移動を含むのでflickerとして判定せず、誤差の空間的集中を示す診断だけに使う。候補・表示スケールは結果を見る前に固定し、実fog画像のvisual quality、temporal acceptability、GPU/性能の代替証拠にしない。

### 可視shadow誤差fieldの空間分布結果（2026-11-12）

raw CSVから全2,948 rayをframe別64×36 mapへ再配置した。4候補はcenter ±2%/width exact、center exact/width ±2%、全て128 samples。fixed preview mappingは`min(1, relative RGB L2 × 50)`（2%でwhite）で、PFM linear float32とPNGを保存した。全候補map 28枚（7 frame×4 candidate）とhashは`population_spatial_manifest.json`、画像群は同artifact directory内の`population_error_*`にある。

center exact・width ±2%では各方向とも2,121/2,948 rayが`1e-3` gateを超え、p95/maxは約`0.1423% / 0.1458%`。edge 548 rayのmaxは`0.0697%`でgate内、interior 2,400 rayのmaxは`0.1458%`であった。最大誤差のscreen位置は両方向ともframe 1付近の`(24,28)`、regionはshadow interior、surfaceはnear chart。surface別ではnear-chart ray 2,677本のmaxが`0.1458%`、ground ray 271本のmaxが`0.1001%`。center ±2%では全rayがgate超過し、maxはcenter −側`1.8073%`、+側`1.8660%`で、最大箇所は引き続きshadow interiorのnear-chart周辺だった。

誤差fieldの視覚検査では高誤差がshadow interiorに広く分布し、silhouette edgeだけへ局在してはいなかった。これはsynthetic fixture上でのsample allocation/profile knot位置誤差の分布で、実fog radiance画像の細部品質を示さない。camera移動sequenceの同screen座標差は意図した投影移動を含むためtemporal flickerの合否は付けていない。集計と全frame/per-region/top-coordinate統計はmanifestに記録し、製品shadow、視覚許容値、GPU出力、実時間安定性への外挿はしない。

### Shadowed world pointのdirect-ray時間残差計画（2026-11-12）

中心frameでAABB ray intervalを持つ各pixel-center rayとその最寄りsurface world pointをsource setにし、surface labelと距離差`<=0.05` scene unitを満たす隣接frame pairだけを採用する。対応world pointから各cameraへのcontinuous rayを再生成し、image/PFM bilinear samplingを介さずにtrue intervalでreference 4,096 samples、validation 2,048 samples、候補128 samplesのsingle-scattering integralを計算する。candidateはexact interval profile、center offset±2% ray length（width exact）、center exact・core width±2%の計5種。candidate−reference residualのworld-point/frame-pair差をRGB RMSE、max-channel p95/maxで集計し、source silhouette edge/interiorとnearest surface別に分ける。視覚的temporal acceptance閾値は設定せず、exact-profile residualとreference validation精度を対照にする。座標/可視性/intervalとraw pair rowsを保存する。結果を見る前に条件を固定し、視覚閾値、実shadow map、GPU実行の代替にしない。

### Shadowed world pointのdirect-ray時間残差結果（2026-11-12）

中心frameのshadowed point 440本から各隣接frame pairへ投影し、surface label一致かつdistance差0.05以内の418 correspondence/pairを確認した（6 pair、2,508 point-pair/method）。各point/frame rayはPFMやbilinear image samplingを介さず再生成し、reference 4,096対validation 2,048の最大relative RGB L2差は`1.19e-9`。exact interval profile 128のcandidate−reference temporal residual RMSEは`8.34e-9`、max-channel p95/maxは`1.45e-8 / 1.19e-7`だった。

center±2% ray-length errorのtemporal residual RMSEは約`3.11e-5`、max-channel p95は`1.41e-6` / `1.87e-6`、最大値は`3.27e-4` / `3.24e-4`。大きなmaxはsilhouette-edge subset（348 pair）にあり、interior（2,160 pair）のmaxは約`2.0–2.6e-6`に留まった。最大outlierはpair 2、source pixel `(41,10)`近辺のnear-chart silhouetteで、true intervalはprevious `[0.60672,0.61887]`、current `[0.60672,0.60834]`へ短縮した。direct ray上で幾何interval自体が大きく変化する場所にcenter-error residualが集まった。

center exact・core width±2%のtemporal residual RMSEは`1.15–1.20e-6`、max-channel p95は`2.32–2.40e-6`、maxは`1.12e-5`。edge maxは約`5.0–5.1e-6`。このsynthetic CPU fixtureでは位置誤差がsilhouetteの大きな直接残差を作り、幅誤差は一桁以上小さかった。ただし視覚的flicker閾値、製品のshadow filtering、subpixel coverage、実レンダラー/GPU性能は測っておらず、合否や実装採用の結論は出さない。12,540 raw pair rowsとframe/region/surface別統計は`population_temporal_manifest.json`と`population_temporal_direct_ray.csv`に保存した。

### Temporal outlierのAABB active-face探索計画（2026-11-12）

先行direct-ray temporal sweepの最大outlierを見た後の探索的mechanism auditとして、全2,508 worldpoint/frame pairでray-box slab intersectionのentry/exitを決めるactive AABB face（x/y/z × min/max）を復元する。candidate/gateは変更せず、center±2% width-exactの残差をentry/exit face切替・normalized interval length変化別に記述集計し、他candidateの同位置残差と対照する。最大点の境界面は誤差許容や採用条件の根拠にしない。結果を見ていない対照群と全対象をraw化し、仮説確認用の独立再実験が必要なexploratory findingとして扱う。

### Temporal outlierのAABB active-face探索結果（2026-11-12）

2,508 worldpoint/frame pairからslab active faceを復元したところ、exit faceがcamera間で切り替わったのは6 pairのみだった。center±2%候補の最大residual群はexit face切替を伴わず、entry `z:min`／exit `x:max`のまま、同一AABB面上の交点が移動していた。例のpair 2・source pixel `(41,10)`ではnormalized intervalが`[0.60672,0.61887]`から`[0.60672,0.60834]`に短縮し、center −2% candidate temporal max-channel residualは`3.27e-4`。そのため、先の「face切替が原因」という探索仮説はこのデータで支持されず、同じactive face上でray-volume overlap長が変わるnear-grazing geometryとの関連が見つかった。

ただしactive face不変群が2,502 pairと大半を占め、最大値もこの群に残る。位置誤差残差はedge群で顕著（p95約`2.94–2.95e-4`）だが、edge内の全点がoutlierではない。検定や因果同定ではなく、top residualとslab endpointを照合した探索的記述である。raw 2,508 pair、entry/exit face別・candidate別集計、top outlierは`population_boundary_face_manifest.json`と`population_boundary_face_audit.csv`に保存した。次の確認ではAABB edgeからの幾何距離とinterval幅・残差の関係を独立に変え、同じ挙動がbox尺寸・camera trajectory変更後も残るか再評価する。現結果から製品shadowのちらつきや許容値を推定しない。

### AABB X half-extent感度のdirect-ray追試計画（2026-11-12）

active-face post-hoc所見の幾何依存を調べるため、同じ中心frameの440 shadowed world point・7 camera positions・scene visibility条件を保ち、world-fixed AABBのX half-extentだけをbaseline比0.75/1.0/1.25倍する。各scenarioで全pointを再判定し、surface label一致・distance差`<=0.05`かつ両隣接frame rayがAABBにhitするpoint pairを採用、除外数も記録する。reference 4,096／validation 2,048／candidate 128 samples、候補はexact intervalとcenter offset±2% ray lengthのwidth-exactを固定。直接single-scattering integralのcandidate−reference temporal residual RMSE・p95/max、baseline source silhouette edge/interior別に比較し、slab active faceとinterval span変化も保存する。視覚合否閾値は設けず、各scenario固有のsynthetic AABB diagnosticとして解釈する。box寸法以外、candidate、sequence、scene、gate条件は変更しない。

scenario別有効pair数が変わるため、結果確認後のselection-bias auditとして、raw sweepに存在する3寸法共通のworldpoint/frame pair intersectionでcandidate統計を再集計する。これはpost-hoc同一support比較とし、除外を伴うscenario別統計と併記する。

### AABB X half-extent感度のdirect-ray追試結果（2026-11-12）

基準AABBから得た440 world pointを固定してX half-extentを0.75/1.00/1.25倍にした。全scenarioの最大reference 4,096対validation 2,048差は`1.21e-9`。有効な隣接frame point pairは0.75倍で1,848、baselineで2,508、1.25倍で2,640だった。候補個体数が変化するためscenario全体のsummaryだけを単純な同一母集団比較とはみなさず、rawには可視ray除外を残した。

center +2%候補の全有効pair temporal residual RMSE/max-channel maxは、0.75倍で`1.15e-6 / 1.95e-5`、baselineで`3.12e-5 / 3.24e-4`、1.25倍で`6.76e-7 / 1.51e-6`。baselineの最大outlier world point `(3.6164,0.0029,12.3616)`は0.75倍では該当pairのAABB hitが消えて除外され、1.25倍ではexit faceがx:maxからz:maxへ移り、同pair center +2% residualが`3.24e-4`から`1.28e-6`（約253分の1）へ減少した。intervalはbaselineの`[0.60672,0.61887]→[0.60672,0.60834]`から、1.25倍では両frameとも`[0.60672,0.72806]`に変わり、face構成とoverlap長の安定性が同時に変化した。これは先行outlierが特定のAABB geometryとgrazing overlapに強く条件付けられていることを支持する一方、縮小scenarioではrayを失うselection effectがあり、extent一般の改善と解釈できない。

全scenario共通の1,848 pairへ揃えたpost-hoc common-support集計では、center +2% residualのmaxは0.75倍`1.95e-5`、baseline`1.25e-6`、1.25倍`1.25e-6`。このintersectionからbaseline最大outlier自体が落ちているため、この共通cohort集計はoutlierを直接比較せず、baseline全体maxを小さいbox条件へ外挿しないためのselection-bias auditである。baseline outlier個別の1.25倍追跡はraw rowに保持した。

この追試はsynthetic box geometry sensitivityだけを示す。実遮蔽物、filter/penumbra、temporal perceptual threshold、GPU経路や性能への結論は出さない。scenario別raw 20,988 rows、valid pair数、各region summary、parameter/hashは`box_extent_temporal_manifest.json`および`box_extent_temporal_sweep.csv`に、post-hoc common-support 16,632 rowsは`box_extent_common_cohort_manifest.json`および`box_extent_common_cohort.csv`に保存した。

### Camera translation振幅のdirect-ray追試計画（2026-11-12）

AABB寸法感度から分離してcamera軌道依存を確認するため、box/scene/source pointを固定し、7-frame水平camera x trajectoryだけを基準列`(-0.3,…,0.3)`の0.5/1/2倍（端点±0.15/±0.30/±0.60 world unit）に変える。中心frame x=0のbaseline shadowed world point 440本を各scenarioへ再投影し、surface一致・距離差`<=0.05`・両frame AABB hitを満たすadjacent pairを測る。reference 4,096／validation 2,048／candidate 128 samples、exact intervalとcenter offset±2% width-exact候補を比較し、direct-ray candidate−reference temporal residualを全体・baseline silhouette edge/interior別に集計する。scenario別有効数と全scenario共通worldpoint/pair cohortを併記し、baseline max outlierの各軌道での可視性・intervalも追跡する。合否閾値は設けず、等frame間隔のsynthetic camera translation sensitivityとして解釈し、実速度・shutter・perceptual flicker・GPU/製品経路は対象外とする。

### Camera translation振幅のdirect-ray追試結果（2026-11-12）

基準camera trajectoryの0.5/1/2倍（x端点±0.15/±0.30/±0.60）を比較した。各scenarioで全440 source pointが可視・AABB-hitを保ち、隣接frameあたり418 correspondence、合計2,508 pair（共通supportも2,508）で母集団脱落はなかった。reference 4,096対validation 2,048の最大差は`1.20e-9`。exact interval profile 128のtemporal residual RMSEは各振幅で約`7.15–9.05e-9`、max-channel maxは約`1.19e-7`と安定した。

center +2%候補のall-pair temporal residual RMSE/max-channel p95/maxは0.5倍`2.30e-5 / 5.38e-5 / 2.02e-4`、1倍`3.12e-5 / 1.87e-6 / 3.24e-4`、2倍`4.83e-5 / 3.56e-6 / 3.99e-4`。RMSEと最大値は振幅増加とともに上がったが、p95は単調でなく、特に1倍では少数の大外れ値がaggregate RMSE/maxを支配する。位置誤差outlierは各振幅でsilhouette edgeに強く寄り、interior側にも2倍軌道では最大約`4.0e-4`の候補残差が現れた。これはbaseline source regionラベルに基づくproxy分類である。

baseline max point `(3.6164,0.0029,12.3616)`は3振幅すべてで追跡でき、center +2% residualは0.5/1/2倍で`1.22e-4 / 3.24e-4 / 3.82e-4`。previous rayのnormalized interval endはそれぞれ`0.61368 / 0.61887 / 0.62886`から、共通current frameでは`0.60834`へ移る。つまりcamera stepを広げると同じpointのray-volume overlap変化と残差が大きくなった一方、同一pointのsingle worst residualはall-population maxではなく、2倍軌道では別pointが最大になった。全trajectory、region/pair metrics、point intervalとraw 22,572 rowsは`camera_motion_temporal_manifest.json`および`camera_motion_temporal_sweep.csv`に保存した。

この結果は等frame間隔のsynthetic CPU sequenceでの数値residualのみであり、物理時間あたりの速度、perceptual flicker、motion blur、実shadow map、GPU/performance acceptanceは未検証。camera sweepはbox extentを変えないため、前回box sensitivityのselection effectとは区別する。

### 推定intervalのquadrature要因分解計画（2026-11-12）

camera-motion振幅sweepのcenter±2% candidateは、`weighted_cells()`がestimated intervalから作るprofile segmentation（knotsとblocked piece inclusion）とrho×visibility importance weightsの両方でsample配置を変える一方、`integrate_cells()`はtrue intervalを使ってradianceを評価している。どの要因がdirect-ray temporal residualを作るか、全3 camera amplitude、同じ440 world point・2,508 pair、±2% center candidateを保った2×2 factorial controlで分ける。profile segmentationはtrue/estimated interval、importance weightsはtrue/estimated visibilityの組を作り、integrand visibilityは常にtrue intervalとする。比較候補はtrue/true baseline、true/estimated、estimated/true、estimated/estimatedの各center offset。reference 4,096／validation 2,048／candidate 128を維持し、candidate−reference temporal residual RMSE・p95/maxをall/edge/interior別に報告する。閾値は設けず、factorial effectsはnonlinear interactionを含む記述比較として扱う。元のfull estimated/estimated raw値と再計算した同条件値が整合することも再現確認する。

### 推定intervalのquadrature要因分解結果（2026-11-12）

### Core幅誤差のquadrature要因分解計画（2026-11-12）

**事前登録:** center offsetを0に固定し、core幅推定誤差のみ−2%／+2%とする。既存camera-motion CPU fixtureの同じ3振幅、中心frameのshadowed world points、隣接frame pairを用い、true/estimated profile segmentation × true/estimated importance weightsの2×2 factorialを各幅誤差で評価する。reference 4096、validation 2048、candidate 128 steps、同じAABB・penumbra・ray条件を維持する。積分visibilityは常にtrue intervalで評価する。主要指標はtemporal residual RGB L2 RMSE、95th percentileおよび最大channel error、factor main effectsとinteraction。既存exact-width baselineとの一致も照合する。

このfixtureの目的は幅誤差に対するquadrature配置の数値感度を分離することであり、視覚品質の合否判定には使わない。画像目視・alpha／色契約・GPU/backend差・実時間性能はこのCPU実験では未評価として残す。素材は既存の合成scene/AABB fixture（各入力の識別情報とhashはmanifest参照）で、実sceneの正解画像ではない。実行日2026-11-12、再現手順は後続artifactのanalysis script実行。結果を見てから閾値を新設せず、許容差に基づく採否を主張しない。

### Core幅factorのregion層別追加解析計画（2026-11-12、exploratory）

登録済みwidth factorial rawに含まれる`source_region`で、segmentation main effect、interaction、combined shiftのRMSE／maxをedgeとinterior別に再集計する。新しい候補・閾値・合否判定は導入しない。regionはcenter frameのsilhouette maskから割り当てたsource-point labelをpair全体へ引き継いだものとするため、現フレーム上の境界位置を追跡する分類ではない。これは全体集計で隠れる差の探索的記述であり、独立標本の統計検定や視覚品質証明には用いない。

### Core幅誤差×candidate step収束の追加解析計画（2026-11-12）

既存の事前登録済み`outlier_convergence` rawからcenter offset=0、width error=−2%／+2%だけを抽出し、32 selected temporal-outlier points（previous/currentの32 rays）に対するcandidate steps 32/64/128/256/512/1024/2048/4096のrelative RGB L2 error p95/maxを記録する。reference=16,384、validation=8,192、既存gate=0.001は変更しない。あわせてwidth=0の同じray/stepをquadrature-only baselineとして差し引き、total errorとboundary-width寄与を混同しない。これは別の小ray cohortに対するpost-hoc接続解析であり、全populationの採否や視覚品質を示すものではない。

### Pair 3 のwidth残差幾何・factor follow-up計画（2026-11-12、exploratory case study）

前段で選択された最大width+2% paired contrast ray（outlier index 1、pair 3、current frame）を事後選択caseとして調べる。既存active-face監査からprevious/currentのtrue interval端点・active entry/exit faceを照合し、同じ2 framesをreference 16,384 samples、candidate 128/512/2,048/4,096で再評価する。width −2%/+2%それぞれでtrue/true、true segmentation/estimated weights、estimated segmentation/true weights、estimated/estimatedの4 cornerを保存し、各stepにおけるtemporal residual contrastを算出する。追加candidateや合否閾値は設けず、特定最大outlierの記述case studyとして扱い、母集団・因果一般化はしない。

### Selected outlier cohortへのwidth factor拡張計画（2026-11-12、exploratory）

pair 3のcase結果がその1 pairに特有かを確認するため、既存`temporal_outlier_manifest.json`で選択済みの全16 temporal pairsへ同じ2×2 width factorialを適用する。previous/currentの32 rays、width ±2%、candidate 128/512/2,048/4,096、reference 16,384、validation 8,192を固定する。各pairのtemporal residual max-channel分布のp95/maxと、同一stepのwidth=0 true/trueとの差（factor shift）を集計する。選択済みoutlierを再選別せず、gateも新設しない。screen-space temporal residualに基づく事後選択cohortなので独立population estimateや画像品質の合否とは扱わない。

### Selected cohortの重複依存構造監査計画（2026-11-12、exploratory）

width factorialの512 temporal rowsを、selected occurrence（16）、unique camera pair、unique world pointの3つのgroupingで再集計する。幅符号・step・4 factor cornerごとに各group内max-channel temporal residualの順位・最大値・group countを保存し、最大値が同一点反復・pair反復のどちらに支配されるかを照合する。新しいgate/推測統計は設けず、同じraw rowsのaggregation sensitivity診断として扱う。unique pointsも先行screen-space temporal outlier抽出で選ばれた7点に限られるため、全scene母集団への推論には使わない。

### 全440 source pointへのwidth factor分布追加解析計画（2026-11-12、post-hoc）

既存width factorial/effects rawがsource center frameから得た全440 shadowed world pointsを含むことを確認したため、新たな抽出seedなしで全点を再集計する。全3 camera amplitude・source_region・width ±2%・4 effect contrastを固定し、pair rowsから各world pointの最大absolute temporal RGB channel effectを取ってから、440 pointsのpoint-level p50/p95/maxを報告する。加えて先行screen-space outlierで選ばれた7 unique world pointsを、全440 pointのcombined-candidate-shift順位へ照合する。これは既存データのpost-hoc分布解析で、事前gate・閾値・因果推論を追加せず、synthetic CPU fixtureの範囲を越えて外挿しない。

### 全populationの区間幾何とwidth sensitivityの層別解析計画（2026-11-12、exploratory）

全440-point width factorial rawと実active-faceを再計算するslab classifierをpoint/pair/amplitudeでjoinする。 geometry predictorsはprevious/current normalized interval width、幅のsigned/absolute frame delta、entry/exit face、camera移動中のface switchとする。outcomeは両符号のcombined_candidate_shift max-channel absolute。各amplitude内の`abs(Δ normalized width)`をgeometry-only quartileへ分け、同じくface-switch有無でも集計し、各binでpair-levelおよびunique-point-max p50/p95/maxと件数を保存する。cutpointsはoutcomeを参照せずgeometry featureから作る。これはpost-hoc descriptive associationであり、point/pairの反復依存、synthetic AABB、source-frame固定regionという制約を明記し、因果・perceptual acceptanceには使わない。

### 全440 source pointへのwidth factor分布追加解析結果（2026-11-12）

source center frameでshadow volume intervalがある440 pointsのうち、各camera amplitudeの各隣接pairには418 valid correspondences（2,508 pair rows / amplitude）があり、point-level集計は最大440 source points全体を保持した。width +2% full combined-candidate-shiftの各pointについて6 adjacent-pair rows内の最大absolute channel effectを取り、そのp50/p95/maxはamplitude 0.5=`3.44e-8 / 7.59e-6 / 9.84e-6`、1=`6.98e-8 / 8.60e-6 / 1.13e-5`、2=`1.51e-7 / 1.02e-5 / 1.28e-5`だった。

region別ではinterior point tailがedge label群より大きかった。width +2%、amplitude 1のpoint-max p95/maxはsilhouette-edge 80 pointsで`4.14e-6 / 5.08e-6`、shadow-interior 360 pointsで`9.21e-6 / 1.13e-5`。amplitude 2でもedgeは`6.84e-6 / 7.69e-6`、interiorは`1.04e-5 / 1.28e-5`。regionはsource center-frame mask由来の固定world-point labelであり、現frame screen-edge区分ではない。

先行screen-space outlierの16 occurrencesは7 points全て全440へmatchしたが、幅誤差に対する順位は偏っていた。amplitude 1・width +2%の全population combined shift rankは、7点で`25, 64, 70, 71, 77, 78, 427`位／440。最大single-pair contrastだったpoint（source index 121）は25位で、topではあるが最大ではない。selected cohortで3 pair反復した同点が他の2 pairほど目立つ一方、outlier index 0の点は427位と低い。screen-space residualによる選択はwidth-only sensitivity cohortを代表せず、事前outlier群の点数や最大値だけで全440点のwidth-error分布を推定できない。

これは同一synthetic camera sceneの既存CPU factorial rawに対するpost-hoc complete-source-cohort再集計であり、独立scene validation、perceptual metric、GPU/runtime acceptanceではない。grouped point summariesとselected-point ranks、input hashesは`quadrature_width_population_analysis_manifest.json`およびCSVに保存した。

### 全populationの区間幾何とwidth sensitivity層別結果（2026-11-12）

#### 連続幾何量とwidth error tailの関連評価計画（2026-11-12、post-hoc exploratory）

- **目的・対象:** 上記440 source-point / 3-amplitude synthetic AABB camera sequenceで、frame間のnormalized entry／exit／width変化がwidth ±2%によるtemporal max-channel residualとどう対応するか評価する。
- **比較対象:** `quadrature_width_geometry_sensitivity.csv`の各pair/sign値。追加レンダー・新しい候補・閾値変更は行わない。
- **評価素材・条件:** 既存raw SHAはgeometry manifestに固定。2,508 valid pair rows / amplitude、4096 reference・2048 validation・128 candidate steps、width ±2%、同一synthetic scene。source-regionはcenter-frame由来。
- **測定:** continuous signed/absolute entry, exit, width deltaと最大channel temporal residualのSpearman順位相関をpair-levelとunique-point maxで計算する。residual上位1%/5%に対するgeometry feature上位群のenrichmentと、pair residualの点単位集約後の比較も出す。標本依存を記録し、descriptive associationに留める。
- **合否:** exploratoryのみ。既存数値gateを変えず、相関・enrichmentに品質合格の意味を与えない。目視・perceptual・GPU/runtime判定は未実施として維持する。
- **実行条件:** Python CPU post-hoc script、既存CSVのみ。GPUや製品hostは未実行。

#### Point-cluster bootstrapによる関連安定性評価計画（2026-11-12、post-hoc exploratory）

- **目的・対象:** 前項のexit/width移動とresidual順位相関が、同一点の隣接pair反復に支配されていないかを推定する。
- **比較対象:** `quadrature_width_geometry_sensitivity.csv`の既存6条件（3 amplitude × width sign ±2%）、2,508 valid pair rows/condition。
- **再標本化:** unique source point 440個を復元抽出し、抽出点に属する全pair rowをまとめて含めるcluster bootstrap。各条件2,000 replicate、seed `20261112`。95% percentile interval。相関が定義できないreplicate数も記録。
- **測定:** primary `abs(normalized exit delta)`対temporal max-channel residualのSpearman rho、secondary `abs(normalized width delta)`対residual。各replicateのpair-row-weighted correlationと、pointごとにresidual最大pair一つを選んだpoint-max correlationを併記する。
- **合否:** exploratoryのみ。閾値や採用判断に使わず、intervalはこの固定synthetic cohort内のpoint sampling感度として扱う。
- **実行条件:** Python CPU上の決定的post-hoc再集計。画像/知覚、GPU、runtimeの評価は対象外・未検証。

#### 体積固定AABB aspect-ratio sensitivity評価計画（2026-11-12、exploratory follow-up）

- **目的・対象:** X half-extentだけを変えた先行実験に対し、AABB中心と体積を固定してX/Z aspect ratioを変え、width +2% temporal residualとinterval geometry関連がbox形状を越えて再現するか評価する。
- **比較条件:** half-extentsを基準`(2.2, 2.5, 0.75)`、X長/Z薄`(3.3, 2.5, 0.5)`、X短/Z厚`(2.2/1.5, 2.5, 1.125)`とする。全条件で中心`(0, 3.5, 8.25)`とvolumeは同一。sun・scene・penumbra・source point（基準shape center frameの440点）は固定。
- **シーケンス・精度:** camera translation amplitude 0.5/1/2、7 frames、candidate 128、reference 4096、validation 2048、true interval visibility。center offsetは0、core width estimateだけ+2%。
- **評価:** valid pair rowsの数と世界点共通supportを記録し、shape別・amplitude別のpair residual p50/p95/maxとabs exit/width deltaのSpearmanを比較する。source pointは同一点の隣接pairで反復するため、pair値とunique point maxを分ける。loss of supportを品質改善と解釈しない。
- **合否:** exploratory。採用gate・一般scene閾値は設定しない。物理/視覚正解、perceptual flicker、GPU/runtimeの判断には使わない。
- **入力・再現性:** 既存camera/scene scriptsとそのhash、定数・形状・seedなしの決定論的実行をmanifestに記録する。GPU・製品hostなしのCPU post-hoc render。

#### Shape差のinterval / profile-knot機構post-hoc解析計画（2026-11-12、exploratory）

- **目的:** residual差がsupportの違いだけでなく、同じpoint/pairのinterval長・時間変位・profile-knot配置とも対応するか調べる。
- **入力:** `box_aspect_width_temporal.csv`のtrue intervalとwidth +2% temporal residual。3 shape共通supportとshape固有supportを分ける。
- **派生量:** previous/current interval幅・entry/exit変位、平均interval幅とray長の1% penumbra幅との比、width +2%で再構成するtrue/estimated `profile_knots`の最大変位とpenumbra比。
- **統計:** shape/amplitude別分布、pair Spearman、geometry quartile別residual、unique-point max。相互に代数依存する特徴量は独立原因として扱わない。
- **範囲:** 既存rawのpost-hoc解析のみ。新規積分、品質閾値、製品採用判定は行わない。目視・alpha・GPU/runtimeは未検証。

#### Aspect-ratio別quadrature segmentation × weight 2×2評価計画（2026-11-12、exploratory）

- **目的:** aspect ratioごとのwidth +2% temporal residualを、estimated profile segmentation（interval/knot配置）とimportance-weight allocationの2要因に分ける。
- **比較:** 同じ3体積固定shape、baseline-selected 440 world point cohort、camera amplitude 0.5/1/2、7-frame sequence、1% penumbra。
- **四隅:** true segmentation/true weights、estimated segmentation/true weights、true segmentation/estimated weights、estimated segmentation/estimated weights。estimated factorはwidth +2%、center offset 0。visibility integrationは常にshape固有true intervalを用いる。
- **数値条件:** 128 candidate steps、4096 reference、2048 validation。各frame-rayの四隅は同じreferenceを使う。supportはshapeごとのvalid pairと3-shape common support（前段定義）を併記する。
- **再現照合:** baseline shapeのtrue/true temporal RGB vectorと既存`quadrature_width_factor_temporal.csv`の同じamplitude/pair/pointを照合し、処理移植差を記録する。
- **測定:** temporal RGB residual vector、各要因contrast、2-way interactionとfull-corner residualを保存し、RGB RMSE・max-channel p50/p95/maxで集計する。要因contrastのvectorは相互作用を含む代数分解なので独立誤差として加算しない。区間幅変化Q1/Q4およびface-switch層を診断用に併記する。
- **評価上の位置付け:** 既存CPU synthetic fixtureの機序比較で、視覚品質の合否・実ジオメトリ一般化・製品host/GPU/performanceの採用判断には使わない。画像目視・alpha/backgroundは未評価と記録する。

#### Aspect-ratio factorのwidth誤差符号対称性評価計画（2026-11-12、exploratory follow-up）

- **目的:** +2%で大きかったfactor interactionとshape差が、同じ絶対量の−2% width errorでも再現するか、またRGB vectorが幅誤差符号の反転で対称／非対称か調べる。
- **素材・条件:** 同じ3 volume-preserving shape、baseline-selected 440 world points、camera amplitude 0.5/1/2、7 frames、1% penumbra、128 candidate / 4096 reference / 2048 validation。center offset 0、true interval visibility。
- **factor四隅:** true/true、estimated-segmentation/true-weights、true-segmentation/estimated-weights、estimated/estimated。−2%候補を新規実行し、+2%は前回`box_aspect_factor_temporal.csv`をhash固定入力として再利用する。
- **符号比較:** 対応pairごとに`plus-minus` vector差、even component`(plus+minus)/2`、odd component`(plus-minus)/2`を要因contrast別に集計する。vector単位で算出し、max-channel magnitudeを独立寄与として加算しない。
- **整合性:** minus/plus共通support、true/true vector一致、factor identityを検証する。3-shape common supportとshape固有supportを分ける。
- **合否・制限:** exploratory。視覚許容・因果判定・製品採用閾値を設けない。CPU synthetic single-scattering以外の画像目視、alpha、GPU/backend、performanceは未検証。

#### Aspect-ratio別quadrature factor結果（2026-11-12）

3 shape × 3 camera amplitude、20324 valid temporal pair rowsで2×2四隅を評価した。baseline true/true角は既存`quadrature_width_factor_temporal.csv`の同条件7,524 rowsとkey完全一致し、temporal RGB residual vector差のmax`0.0`、reference-validation summary差`0.0`。全factor contrastの代数reconciliation errorも`0.0`、reference/validation max relative RGB L2は先行shape sweepと同じ`1.20e-9`。

全supportでtrue/true基準からのsegmentation main-effect RGB RMSEはshape/amplitudeにより`4.27e-8`（X長/Z薄・0.5）から`3.28e-6`（X短/Z厚・2.0）まで動いた。importance-weight単独effectは全条件`5.3e-9–1.1e-8`で、他のeffectより小さい。segmentation×weight interactionはbaselineで`7.69e-7–9.93e-7`、X短/Z厚で`6.98e-7–2.38e-6`と無視できず、各termのscalar magnitudeを独立誤差として足すことはできない。estimated/estimatedの実corner temporal residualは、X長/Z薄でRMSE`3.92e-8–7.28e-8`、max-channel max`3.40e-7–3.44e-7`、X短/Z厚でRMSE`7.00e-7–2.89e-6`、max`5.40e-6–1.54e-5`だった。別量であるEE−TT full-effect contrastのRMSEはそれぞれ`4.21e-8–7.38e-8`、`7.03e-7–2.89e-6`。

同じ3-shape common support（amplitude 0.5/1/2で1,703/1,661/1,638 rows・294 points）でも、X短/Z厚のestimated/estimated実corner temporal residual p95は`1.73e-6 / 3.48e-6 / 8.85e-6`、segmentation effect RMSEは`8.93e-7 / 1.80e-6 / 3.34e-6`、weight effect RMSEは約`1.0e-8`だった。common-support width-delta Q1→Q4でもX短/Z厚のcorner p95はamplitude 1で`1.09e-7→6.09e-6`、2で`2.21e-7→1.38e-5`、Q4 segmentation RMSEは`3.60e-6 / 6.69e-6`。一方X長/Z薄のcommon-support Q1/Q4 p95はamplitude 1で`5.46e-8/6.00e-8`、2で`1.19e-7/1.20e-7`と近い。baseline common-supportでもQ1/Q4 p95は近似し、前段と同じくbaseline high-tailのsupport選択依存を確認した。

したがってこのfixtureのshape-sensitiveなwidth +2% temporal residualでは、importance配分よりprofile segmentation側の差が大きく、特にX短/Z厚とwidth変化上位群で現れる。ただしinteractionが大きい条件があり、face/interval/supportの同時変化もあるため、「segmentationが独立した唯一の原因」とは言えない。これはCPU single-scattering synthetic AABBの数値factor分解で、perceptual stability・実geometries・採用判断ではない。四隅・RGB vector contrastの20324 row raw、分布、common-support層、input hash、factor式、reconciliation結果は`box_aspect_factor_manifest.json`、`box_aspect_factor_temporal.csv`、`box_aspect_factor_baseline_reconciliation.json`に保存した。

#### Aspect-ratio factorのwidth符号対称性結果（2026-11-12）

既存+2% rawを固定入力にし、同じ3 shape・3 amplitude・true interval ray supportで−2%を新規計算した。sign pairごとに20,324 rows、合計40,648 rows。両符号のsupportは完全一致、true/true temporal RGBは全cohortで差`0.0`、factor identity error`0.0`。reference/validation最大差は`1.20e-9`。3-shape common supportは両符号ともamplitude 0.5/1/2で`1,703/1,661/1,638` pair-point rows・294 unique points。

estimated/estimated corner temporal residualのRMSEは+2%/−2%でbaseline `0.914/0.941e-6`、`1.148/1.198e-6`、`1.606/1.605e-6`、X長/Z薄`0.0392/0.0477e-6`、`0.0489/0.0567e-6`、`0.0728/0.0745e-6`、X短/Z厚`0.700/0.701e-6`、`1.393/1.429e-6`、`2.890/2.904e-6`（各振幅0.5/1/2）。common supportでもX短/Z厚amplitude 2のcorner p95は`8.85e-6 / 8.94e-6`、maxは`1.54e-5 / 1.56e-5`。width-delta Q1/Q4のcommon-support corner p95はX短/Z厚amplitude 2で+2%=`2.21e-7→1.384e-5`、−2%=`2.21e-7→1.394e-5`と同じshape-dependent tailを示す。

全cohortのestimated/estimated corner vectorは符号反転odd成分がeven成分より大きかった（例：X短/Z厚amplitude 2、odd/even RGB RMSE=`2.878e-6/3.275e-7`）。したがって符号を反転してもcorner residual magnitude分布は近い一方、RGB誤差方向の大部分は反転する。しかしsegmentation単独とinteractionの各contrastは符号非対称で、X短/Z厚amplitude 2のsegmentation RMSEは+2/−2で`3.28/3.73e-6`、interactionは`2.38/3.33e-6`。weight単独は両符号とも約`1e-8`のまま。component vectorの相殺を含むため、corner残差の対称性から各要因が対称とは言えない。

これは±2% width errorに対する決定論的CPU factor-vector sensitivityであり、符号優劣・perceptual flicker・一般sceneへの推奨を示さない。先行manifestでcommon supportが誤って0 rowsになっていた要約key生成をrawベースで修正し、shape間では`(pair_index, source_point_index)`で共通集合を作り直した。sign raw 40,648 rows、plus/minus support、component odd/even統計と訂正済みcommon-support集計は`box_aspect_factor_sign_manifest.json`および`box_aspect_factor_sign_temporal.csv`、再集計は`fog_shadow_box_aspect_factor_sign_summary_rd.py`に保存した。

#### 体積固定AABB aspect-ratio sensitivity結果（2026-11-12）

半寸法`(2.2,2.5,0.75)`のbaseline、`(3.3,2.5,0.5)`のX長/Z薄、`(2.2/1.5,2.5,1.125)`のX短/Z厚を比較した。box centerは全て`(0,3.5,8.25)`、box体積は全て`33.0 world unit³`。基準shapeの中心frameで選んだ同一440 world points、camera amplitude 0.5/1/2、width +2%・center exact・128 candidate stepsを使用し、reference/validationは4096/2048 steps。最大reference-validation relative RGB L2は`1.20e-9`。

supportの違いは大きい。baselineは各振幅2,508 pair/440 points、X長/Z薄は2,520 pair/420 points、X短/Z厚は振幅0.5/1/2で`1,784/1,740/1,716` pair・308 points。3 shape共通supportは`1,703/1,661/1,638` pair・294 points（baseline全pairの`67.9/66.2/65.3%`）。したがってscenario全体比較と同じpoint/pair比較を分けた。

全scenario supportのpair residual p95/maxはbaselineで振幅0.5/1/2=`1.28e-6/9.89e-6`、`2.32e-6/1.12e-5`、`4.11e-6/1.27e-5`。X長/Z薄では`3.17e-8/3.40e-7`、`6.36e-8/3.44e-7`、`1.27e-7/3.44e-7`。X短/Z厚では`1.72e-6/5.40e-6`、`3.40e-6/1.23e-5`、`8.19e-6/1.54e-5`だった。

さらに3 shape共通の294 pointsへsupportを揃えると、baseline対X長/Z薄対X短/Z厚のpair p95はamplitude 0.5で`4.18e-8 / 2.88e-8 / 1.73e-6`、1で`8.37e-8 / 5.75e-8 / 3.48e-6`、2で`1.69e-7 / 1.17e-7 / 8.85e-6`。X短/Z厚のp95はbaseline共通supportの約41〜52倍で、同じ294 point cohort内でもaspect ratioによりwidth-error temporal sensitivityが大きく変わった。一方、X長/Z薄ではbaselineと同程度か小さい値だった。これは単純な点の脱落だけでは説明できないが、この特定scene/照明/AABB条件での差である。

abs exit-deltaと残差のpair Spearman rhoもshape依存だった。全scenario supportでbaselineは`0.399–0.426`、X長/Z薄は`-0.007–0.020`、X短/Z厚は`0.490–0.568`。共通supportではbaselineが`0.001–0.014`、X長/Z薄が`0.006–0.036`、X短/Z厚が`0.511–0.578`。baseline共通support rhoが全baseline cohortより低いのは、3-shapeに残るsubsetがbaselineの高残差tailを保持しないselection構成による。よってsupport制限をscenario一般の統計として扱わない。X短/Z厚のcamera amplitude 2ではface-switch 48/1,716 pair、他shapeでは0〜6/2,520 pairだったため、AABB face切替も同時に変化し、aspect ratio単独の因果分離にはなっていない。

これは実ジオメトリ一般への外挿ではなく、体積一定でも区間形状と幅誤差感度が大きく変わるという反例的fixtureである。既存のsingle-box相関を汎用予測則に使えないことを示す。visual full/crop、alpha/background、GPU/backend parity、performance、製品host統合は未評価。raw 20,324 rows、support定義、scenario別/common-support統計、同一volume、reference差、source/input hashesは`box_aspect_width_manifest.json`と`box_aspect_width_temporal.csv`に保存した。

#### Aspect-ratio差のinterval / profile-knot post-hoc解析結果（2026-11-12）

同じ20,324 pair rawから、1%-of-ray penumbraに対するtrue interval寸法・camera間変位と、width +2%推定で動くprofile knotを再構成した。candidateの真値intervalからの最大knot displacementはpenumbra幅のshape別約`8.3%`（X長/Z薄）、`12.4%`（baseline）、`18.5%`（X短/Z厚、p50。p95は各`8.6/12.8/19.0%`）。このknot移動量は各rayの最大interval幅から代数的に定まる。一方、全support内でこの最大knot displacementとtemporal residualのSpearman rhoは3 shapeとも負（baseline`-0.454…-0.367`、X長/Z薄`-0.498…-0.475`、X短/Z厚`-0.372…-0.239`）。したがって「静的なknot誤差が大きいほど時間残差も大きい」という単純な関係は見られない。

形状内のframe間width変化量では応答が分かれた。abs normalized width delta（1%-penumbra単位）のpair rhoはbaseline`0.316–0.368`、X長/Z薄`-0.071…-0.015`、X短/Z厚`0.448–0.499`。width-delta四分位Q1→Q4の全support residual p95はbaselineでamplitude 0.5/1/2=`4.31e-8→6.80e-6`、`8.51e-8→7.85e-6`、`1.71e-7→9.30e-6`。X長/Z薄では同じ比較が`3.14e-8→3.27e-8`、`6.29e-8→6.64e-8`、`1.25e-7→1.30e-7`とほぼ横ばい。X短/Z厚は`5.38e-8→3.32e-6`、`1.09e-7→6.02e-6`、`2.20e-7→1.38e-5`とQ4側で増えた。

同じ294 unique pointsのcommon supportに限定してshapeごとに四分位を切り直しても、X短/Z厚ではQ1→Q4 p95がamplitude 0.5/1/2で`5.39e-8→3.32e-6`、`1.09e-7→6.09e-6`、`2.20e-7→1.38e-5`。X長/Z薄では各`2.87e-8→2.95e-8`、`5.46e-8→6.00e-8`、`1.19e-7→1.20e-7`でほぼ変わらない。baselineでもcommon-support Q1/Q4 p95は`4.24e-8/4.04e-8`、`8.43e-8/8.46e-8`、`1.69e-7/1.66e-7`へ揃い、full-cohortで見えた強い分離は消える。これは共通support構成がbaseline tailを除いていることを示し、baselineのquartile関係はsupport選択に敏感である。

以上は、X短/Z厚のこのfixtureでcamera移動に伴うtrue interval幅変化が大きいpairほど残差tailも高くなる関連と整合する一方、X長/Z薄では同様の時間変化がほぼないことを示す。knot-shift量単独では説明できず、shape変更はactive face・interval・visibility supportを同時に変えるため、原因の独立分離や知覚的flickerの説明ではない。派生feature、support別quartile、pair/point統計とhashは`box_aspect_geometry_analysis_manifest.json`およびCSVに保存した。目視・alpha・GPU/runtime・製品host評価は未検証のまま。

#### Point-cluster bootstrap結果（2026-11-12）

6条件 × 2,000 replicateの計12,000 cluster resampleを実行し、各replicateで2 geometry feature × 2 estimandを算出した（合計48,000相関）。すべて定義可能でundefinedは0。weighted Spearmanの順位にもbootstrap frequency weightを反映した。abs exit deltaのunique-point-max rho中央値と95% percentile intervalは、width −2%でamplitude 0.5/1/2=`0.483 [0.397, 0.556] / 0.560 [0.482, 0.629] / 0.626 [0.554, 0.690]`、width +2%で`0.511 [0.423, 0.585] / 0.550 [0.468, 0.624] / 0.630 [0.551, 0.691]`。対応するpair-row-weighted rhoも全条件で正で、medianはおよそ`0.401–0.428`、95%区間はおよそ`[0.328, 0.496]`。

abs width deltaのunique-point-max median rhoは−2%で`0.436 / 0.500 / 0.591`、+2%で`0.475 / 0.510 / 0.595`。各条件の95%区間は0を越えて正側にある。一方、振幅間およびwidth符号間の各区間は相当重なるため、振幅依存性や符号による差があるとは主張しない。これは固定synthetic scene内で点を標本単位とした関連安定性であり、scene間変動・空間相関・時間系列全体・視覚的flickerの確率的不確実性を含まない。

同じpoint-cluster seedの条件別rho・interval、未定義回数、script/input/output hashesはbootstrap manifest/CSVに保存した。R&D画像評価ルールに照らすと、現段階のCPU数値評価は区間幾何感度の補助証拠である。目視全体/crop、画像分布・差分の品質判定、alpha/background、GPU/backendとpreview/final parity、performance budgetは依然未実施で、フォグ製品統合・採用判断は未完了。

#### 連続幾何量とwidth error tailの関連評価結果（2026-11-12）

全15,048 raw rows（3 amplitude × 2 signs × 2,508 pair rows）を分析し、Spearman順位相関、各点のmax residualを取るunique-point集約、geometric feature上位5%とresidual上位1%／5%の重なりを算出した。width +2%で`abs(Δ normalized exit)`とresidualのpair-level rhoはamplitude 0.5/1/2で`0.402 / 0.402 / 0.426`、point-max集約では`0.511 / 0.552 / 0.629`。`abs(Δ normalized width)`のrhoはpair-levelで`0.366 / 0.384 / 0.410`、point-maxで`0.473 / 0.511 / 0.595`。entry移動は全振幅でほぼ無相関（pair rho -0.007〜-0.084）だった。width −2%でもexitのpair rhoは`0.400 / 0.411 / 0.429`、widthのpair rhoは`0.354 / 0.384 / 0.418`と同様の関連があった。

幅変化の大きさは残差tailの完全な予測指標ではない。width +2%時、`abs(Δ width)`上位5%に入る行がresidual上位5%を捕捉する割合は振幅0.5/1/2で`30.2% / 26.2% / 33.3%`。exit移動上位5%でも`31.7% / 24.6% / 35.7%`で、tailの大半は feature上位5%の外に残る。signed exit deltaはresidualと負方向にごく弱く相関（+2% pair rho -0.006〜-0.027）し、entry/widthの符号相関もほぼゼロ。したがって単調な符号付き移動規則というより、exit/width移動量が大きい一部pairで残差が高くなる記述的傾向と読むのが妥当である。amplitude 2のface-switchは64/2,508 pair（約2.6%）であり、本分析はこれを別因果変数として統制していない。

相関値には推測統計・p値を付けず、隣接pairの独立性を仮定しない。point-max値は各source pointから最大残差の1行を選ぶ感度分析で、無作為標本や信頼区間ではない。tail捕捉率も同じfixture・source cohort内の重なりで、別scene・screen-space edge・perceptual flicker予測へ一般化しない。CSVはfeatureごとのpair rawとpoint-max rawを収録し、入力hash・定義・制限はmanifestに保存した。

既存の全440 source-point width factorialを、各amplitudeの有効2,508 camera-pair rowsへ展開し、previous/current ray区間のnormalized幅・entry/exit変位・AABB active entry/exit faceと、width ±2%時のcombined candidate temporal max-channel shiftを対応付けた。各amplitude内で`abs(Δ normalized width)`の四分位に分け、face switch有無でも集計した。geometryを再計算した区間端点とfactor rawの最大不一致は`1.11e-16`で、再構成は数値誤差内で一致した。

全amplitudeで区間幅変化Q4はQ1–Q3よりpair-level tailが明確に高かった。width +2%のpair-level p95/maxは、amplitude 0.5でQ1=`4.33e-8 / 2.50e-6`、Q4=`6.77e-6 / 9.84e-6`、amplitude 1でQ1=`8.74e-8 / 2.43e-6`、Q4=`7.84e-6 / 1.13e-5`、amplitude 2でQ1=`1.76e-7 / 2.54e-6`、Q4=`9.31e-6 / 1.28e-5`。ただし同一pointが複数pairへ現れるため、unique-pointごとのgroup内最大も併記するとQ4 p95/maxは振幅0.5=`8.24e-6 / 9.84e-6`、1=`9.28e-6 / 1.13e-5`、2=`1.10e-5 / 1.28e-5`。四分位境界は下位3群が浮動小数精度程度のゼロ変化を含み、実質的にQ4が区間幅の変化を持つpair群を分ける構造だった。

active-face switchは稀少で、amplitude 0.5は2,508中4 pair、1は6 pair、2は64 pair。振幅2ではswitch群の+2% pair p50/p95/max=`3.27e-6 / 4.19e-6 / 4.28e-6`、no-switch群は`1.26e-7 / 4.01e-6 / 1.28e-5`だった。maxはswitchなし側にも存在するため、face switchだけでは最大感度を説明しない。これは同一synthetic AABB・source-frame point cohortでの記述的関連であり、独立scene、現frame screen edge、知覚的flicker、因果効果やGPU/runtimeを示さない。詳細な層別統計、全pair raw、cutpoints、input/script hashesは`quadrature_width_geometry_sensitivity_manifest.json`とCSVに保存した。

### Core幅誤差のquadrature要因分解結果（2026-11-12）

全3 camera amplitude、440 center-frame shadowed points、各振幅2,508 adjacent-frame pairで評価した（各候補7,524 temporal rows、factorial全体52,668 rows）。core幅のみを−2%／+2%振り、center offsetは0に固定した。reference/validation relative RGB L2の最大差は`1.1985e-9`。width-study true/true baselineは先行factor sweepの7,524対応値とmax-channel residual差`0.0`で一致した。

estimated segmentation・true weights角のtemporal residual RGB RMSEは、width −2%で振幅0.5/1/2に`1.94e-6 / 1.51e-6 / 2.31e-6`、width +2%で`3.61e-7 / 7.30e-7 / 1.63e-6`。true segmentation・estimated weights角は全振幅・両符号でおおむね`8e-9`〜`1.1e-8`とbaseline近傍だった。full estimated/estimated角は−2%で`9.41e-7 / 1.20e-6 / 1.60e-6`、+2%で`9.14e-7 / 1.15e-6 / 1.61e-6`。width +2%では単独segmentation main-effect RMSEが`3.63e-7 / 7.30e-7 / 1.63e-6`、width −2%では`1.94e-6 / 1.51e-6 / 2.31e-6`であり、weight main effectは約`8e-9`〜`1.1e-8`。幅誤差でも、このfixtureのglobal RMSEではsample budget配分よりinterval-driven knots／blocked-piece segmentationの変化が支配的だった。

interactionは無視できず、RGB vector RMSEは−2%で`1.97e-6 / 1.69e-6 / 2.14e-6`、+2%で`7.69e-7 / 8.29e-7 / 9.93e-7`。したがってfull cornerをsegmentationとweightの独立効果の単純和として予測できない。局所max、edge/interior別、perceptual評価では優劣の断定を避ける。完全なpercentile/max、region集計、input/source hash、再現条件はwidth factor manifestsとCSVに保存した。

これはsynthetic AABB・CPU single-scattering・128 candidate samplesでの数値感度であり、実sceneでの視覚flicker、実shadow filter、alpha/color contract、GPU/backend parity、性能を評価していない。採用判断は未実施であり、製品品質達成を示さない。

### Core幅factorのregion層別追加解析結果（2026-11-12）

事前に定めたexploratory集計では、各振幅2,508 pairのうち348がcenter-frame `silhouette_edge` label、2,160が`shadow_interior` labelだった。segmentation main-effect RGB vector RMSE（edge/interior）は、幅−2%で振幅0.5=`6.32e-7 / 2.07e-6`、1=`1.15e-6 / 1.56e-6`、2=`2.24e-6 / 2.32e-6`。幅+2%では0.5=`5.54e-7 / 3.21e-7`、1=`1.07e-6 / 6.60e-7`、2=`2.44e-6 / 1.46e-6`だった。つまりedge/interiorの大小関係は幅誤差の符号と振幅で変わり、global segmentation優位だけで局所挙動を代表できない。

segmentation effectのregion別maxは`1.50e-5`以下だった一方、interaction maxは幅−2%で最大`1.97e-5`（amplitude 1 interior、2 interiorでも`1.97e-5`）、幅+2%で最大`8.78e-6`。factorial interactionは局所ピークでmain effectと同程度以上になり得る。source-region cohortは固定center-frame由来で、pair間に同一pointを繰り返し含むため、独立画素標本の信頼区間とはみなさない。数字はpost-hoc領域集計であり、フレームごとのscreen-space boundary帯の挙動やperceptual flickerを測定していない。

### Core幅誤差×candidate step収束の追加解析結果（2026-11-12）

既存outlier-convergence fixtureの32 rays（16 selected world pointのprevious/current）で、center offset=0、width error±2%を抽出した。reference=16,384、validation=8,192の最大relative RGB L2差は`4.11e-11`。relative RGB L2 gateは事前登録済み`0.001`。幅±2%候補は32〜4,096 samplesの全段階でこのcohort gate内だったが、gate近傍の候補に数値的・視覚的な同等性を保証する意味はない。

32/128/512/2,048/4,096 samplesにおけるwidth −2%候補のp95/max per-ray relative RGB L2 errorは、`8.09e-4/9.05e-4`、`4.95e-4/9.04e-4`、`3.65e-5/3.75e-5`、`8.58e-7/1.00e-6`、`1.92e-7/1.93e-7`。width +2%では`8.08e-4/9.04e-4`、`4.97e-4/9.04e-4`、`3.66e-5/3.76e-5`、`7.91e-6/1.14e-5`、`9.17e-6/1.29e-5`だった。+2%は2,048→4,096でper-ray errorが低下せず、単一ray radianceには幅由来の残差が残る可能性を示す。この指標はtemporal residualではない。

同じray/stepのwidth=0 candidate RGBとの差をreference RGB normで割ったpaired per-ray contrastでは、width −2%のp95/maxが128で`4.96e-4/9.04e-4`、512で`3.66e-5/3.75e-5`、2,048で`8.59e-7/1.00e-6`、4,096で`1.92e-7/1.93e-7`へ縮小した。width +2%では128で`4.96e-4/9.04e-4`、512で`3.66e-5/3.75e-5`だが、2,048で`7.91e-6/1.14e-5`、4,096で`9.17e-6/1.29e-5`が残った。特にoutlier_01 current（pair 3）の+2% per-ray paired maxは`1.29e-5`。これは単一frame radiance差の有限step paired contrastであり、temporal residualや無限sample極限の残差床を示さない。次のpair-level 2×2 follow-upではprevious/current間にこのerrorが強く相殺されることを確認した。

本解析は既存rawのpost-hoc sliceで、新規にgateを選び直していない。32-ray outlier cohortは少数の選択点で、全population・画像知覚・GPU実行を代表しない。sample増加時のruntime／memory、screen-space画像差、実shadow filter経路は未計測。artifactと再現条件は既存`outlier_convergence_manifest.json`、`outlier_convergence_sweep.csv`およびそのanalysis scriptに記録されている。

### Pair 3 のwidth残差幾何・factor follow-up結果（2026-11-12）

最大single-ray +2% contrastを持つoutlier index 1／pair 3のworld point `(-3.061, 4.900, 11.694)`を事後選択し、active-face監査を再照合した。previous/currentともentry faceは`z:min`、exit faceは`x:min`で変化しない。normalized entryは`0.641360`で固定、exitは`0.718728→0.727627`へ移動し、true normalized interval幅は`0.077369→0.086267`（`+0.008898`）となる。これはcamera translation中にexit face位置が変わる幾何であり、active faceのswitchが原因という証拠ではない。

pair-level 2×2 width-factorを128/512/2,048/4,096 samplesで再計算した。width +2%、4,096 samples時のsingle current-ray relative RGB L2 errorはestimated segmentation/true weightsで`1.292e-5`、estimated segmentation/estimated weightsで`1.289e-5`。true segmentation/estimated weights角のerrorはほぼquadrature baselineで、weight errorだけでは説明されない。temporal residual max-channel absoluteは同じ順で`7.62e-8 / 7.67e-8`まで低く、previous/currentのper-ray errorが強く相殺される。すなわち前節の`1.29e-5`はtemporal residual床ではなく単一frame radiance差だった。width +2%のfull-factor temporal max-channelは128で`8.53e-6`、512で`1.57e-7`、2,048で`7.19e-8`、4,096で`7.67e-8`。128→512で大きく下がり、その先は約`1e-7` scaleで横ばいに見えるが、single pair・4 step valuesのみから極限残差とは断定しない。

この対照は、+2%側で観測した単一rayの誤差非対称性が同程度のtemporal residualを生むという仮説を否定する。原因候補はwidth estimateのscene/rayごとの偏りとframe間相関に分かれ、別のworld point群とscreen-space imageでの追試が必要。case-study raw 72 rows、temporal factorial 32 rows、manifestのinput/active-face hashesとsource scriptを`width_outlier_factor_case*` artifactsに保存した。これは最大outlierを選んだ事後case studyであり、一般化評価ではない。

### Selected outlier cohort width factor結果（2026-11-12）

16 selected temporal pairs（32 rays）をwidth factorialで再計算した。reference/validation relative RGB L2最大差は`4.11e-11`。16 rowは7 unique world pointsを含み、同一点が複数隣接camera pairへ再登場するため、16独立点として扱わない。

width +2% full estimated/estimated temporal max-channel p95/maxは128/512/2,048/4,096 samplesで`8.51e-6 / 1.58e-7 / 7.20e-8 / 7.67e-8`だった。この選択cohortではpair 3の問題が最大で、4096時の次点はpair 1=`5.07e-8`、3番目はpair 0=`3.93e-8`。この3 pairはすべて同じworld point `(-3.061, 4.900, 11.694)`の反復である。pair 3/1/0すべてentry `z:min`、exit `x:min`を保ち、normalized widthは順に`0.04681→0.05771`、`0.05771→0.06787`、`0.07737→0.08627`。したがって高いtemporal residualはface switchではなく、同じpointのcamera intervalごとのexit位置変化と共に観測された。これだけで幾何因果を証明はしない。

4096 samplesのwidth +2%ではestimated segmentation/true weightsのtemporal maxが`7.62e-8`、true segmentation/estimated weightsは`3.0e-12`級、estimated/estimated fullは`7.67e-8`だった。単一pair caseと同様、このfinite-step cohort内ではsegmentation側が主要な差で、weight allocation単独は小さい。128 samplesのfull max`8.51e-6`が512で`1.58e-7`へ下がり、その後は同一pointの3 intervalで約`4e-8`〜`8e-8`に留まる。selectionが過去のscreen-space residual上位に基づき、unique pointは7、最大残差を同一点反復が占めるので、母集団統計・一般的残差床・知覚的安定性へ一般化しない。

### Selected cohortの重複依存構造監査結果（2026-11-12）

同じ512 temporal rowsを16 selected occurrence／6 unique camera pairs／7 unique world pointsへ再集約した。width +2%・estimated/estimated・4096 samplesのselected occurrence上位は`7.67e-8`（outlier index 1、pair 3）、`5.07e-8`（index 2、pair 1）、`3.93e-8`（index 13、pair 0）。unique pair集約でも順にpair 3=`7.67e-8`、pair 1=`5.07e-8`、pair 0=`3.93e-8`となり、unique point集約後も同一world point `(-3.061,4.900,11.694)`のmax`7.67e-8`が首位だった。次のunique point maxは`2.28e-11`で、選択cohortの高いtailはこのpointに集中する。

したがって重複を除いても最大値は変わらないが、上位3 pairを3つの独立した幾何事例と解釈することはできない。これは上位選択されたworld pointとcamera intervalの反復感度を示すもので、通常caseの出現頻度・screen-space pixel比率・全scene error分布を推定しない。全符号・step・factor cornerのgroup keys、occurrence数、各集約maxはdependence manifest/CSVに保存した。

全3 camera amplitude、2,508 point/frame pair、center offset±2%でtrue/estimated profile segmentation（knotsとfully-blocked piece inclusion）×true/estimated importance weightsを比較した。reference/validation差のmaxは`1.20e-9`。estimated segmentation・estimated weights角の全15,048結果は前回camera-motion sweepの対応値とmax-channel residual差`0.0`で一致し、factor implementationを再現できた。

linear RGB temporal residual vectorのfactor contrastではsegmentation main effectが主成分だった。center +2%のvector RMSEはamplitude 0.5/1/2で`2.32e-5 / 3.13e-5 / 4.84e-5`、importance-weight main effectは`9.50e-9 / 9.05e-9 / 7.54e-9`。segmentation×weight interactionは`3.20e-6 / 1.76e-6 / 5.67e-7`。profile segmentationをestimatedにしてweightだけtrueにした角の時間残差はfull estimated/estimatedに近く、true segmentationのままweightだけestimatedにした角はexact true/true baseline近傍に留まった。少なくともこのfixtureと候補条件では、camera-motion residualの大半はimportance allocation値より、estimated intervalでknots/blocked piece inclusionを決めるsegmentation側に関連する。

最大segmentation effectはamplitude 0.5/1ではbaseline silhouette-edge groupにあり、それぞれ`2.02e-4 / 3.24e-4`。amplitude 2ではinterior-labeled pointにも`3.98e-4`まで現れた（region labelはsource center frame由来）。interaction maxは`5.55e-5 / 2.46e-5 / 1.92e-5`で、最大局所outlierでは小さくないため、effectを独立因果寄与として足し合わせない。factor contrastsは4 corner residual vectorの代数的分解で、非線形相互作用をinteraction項に含める。60,192 raw temporal factor-effect rows、corner別集計とreconciliationは`quadrature_factor_manifest.json`、`quadrature_factor_temporal.csv`、`quadrature_factor_effects_manifest.json`、`quadrature_factor_effects.csv`に保存した。

この結果はsynthetic AABB・CPU single-scattering・fixed 128 samplesの配置診断であり、実shadow filter、視覚的flicker許容、GPU runtime/performanceを示さない。幅誤差、subpixel coverage、実ジオメトリではsegmentationとallocationの相対寄与が変わる可能性があり、製品採用判断には外挿しない。

## 製品側のGPU入力・dispatch静的監査（2026-11-12）

既存shaderとrender hostを再検索し、fog試作を実行できる既存GPU capture経路があるか確認した。これはソース調査であり、shader compile、dispatch、GPU captureは行っていない。

- `Artifact/shaders/heightFogCS.hlsl` は `Artifact` 内で単独のshader資産であり、C++／module／CMake側にshader名のcompile・PSO・dispatch参照は見つからなかった。`Artifact/App/shaders` に同名shaderはない。したがって既存のComposition fog hostへの接続は確認できない。
- shaderの入力は `Texture2D<float4>` color、`Texture2D<float>` depth、`RWTexture2D<float4>` outputとnear/far、density等の定数buffer。depthをnormalized値としてnear/farに線形補間し、camera positionのYだけから高さ密度を得る。inverse view-projection、pixel ray、world positionは使わず、fog計算後もsource alphaをそのまま保存する。alpha保持だけではstraight/premultiplied RGB契約は定まらない。
- `ArtifactCompositionRenderController.cpp` のpreview slotはサイズ変更時に `createOffscreenDepthTexture()` でdepth targetを作り、render targetとともに3D previewで使う。`ArtifactIRenderer::createOffscreenDepthTexture()`は`D32_FLOAT` textureに`DEPTH_STENCIL | SHADER_RESOURCE` bind flagを付けるが、ここで作成・返却するのはDSV viewであり、Composition slotからfog用SRVを作成・bindする経路は確認できない。resourceがsample可能なbinding flagを持つことと、fog shader用SRV/dispatchがあることは区別する。
- `ArtifactCompositionRenderController.cpp`は3D cameraのview/projectionを`ArtifactIRenderer::set3DCameraMatrices()`へ渡し、rendererはmesh/particle側の行列状態として保持する。`ArtifactIRenderer.cpp`のenvironment skybox経路では保持行列からinverse view-projectionを計算して別shaderへ渡す箇所がある。一方、fog shader用camera constant/bindingは確認できないため、「行列がアプリに存在しない」のではなく「fogへ渡す契約・dispatchがない」と記録する。preview depthのsampling・projection/depth conventionは未確定であり、DSVをnormalized linear depthと見なしてはならない。
- `ArtifactIRenderer.cpp` にはOffline/Layer用 `TEX_FORMAT_D32_FLOAT` depth textureとSRVの作成箇所があるが、対象layer render用の資源であり、height-fog shaderとのbinding・camera matrix連携は確認できない。別経路の資源存在だけでComposition host契約を満たすとは判断しない。
- `globals.hlsli` にはinverse view-projectionによる `reconstruct_position()` があり、`aerialPerspectiveCS.hlsl` 等の別shaderで利用されている。ただしこのfog試作shaderでは未使用で、別shaderの実装をComposition fogの接続済み機能として扱えない。
- CPU `AtmosphereFogRenderer::applyToImage()` はRGB8相当の3 byte/pixel `ImageBuffer` を処理しalpha channelを持たない。`applyToPixel()` のdepth引数はnear/farと同じ距離単位として比較される一方、`applyToImage()`はdepth byteを0〜1へ正規化した後にfar planeを乗じる。製品側のdepth画像生成元・clear値・near/farと同じ規約かは未確認である。

**結論:** 現在のsource evidenceだけではF1/F2/F3の製品GPU比較を公平に実行できる既存adapterは特定できなかった。depth texture自体はshader-resource binding可能で、3D camera行列もrenderer内部にあるが、Composition previewからFog用SRVとmatrixをdispatchへ渡す経路は確認できない。次の有効な作業は、許可されたGPU capture・buildが可能な環境でpreview depthの実sample view、projection/depth規約、camera constant供給元を追跡し、既存render pathを変更せず独立captureできるか確認すること。adapterに必要な契約はcolor/depth SRV、depth clear/encodingと線形化式、pixel-to-world変換用matrix/clip規約、target extent/format、alpha convention、output ownershipである。契約が揃わなければrender passを新設せず、静的監査結果をもってhost接続未検証とする。

### 製品の`Atmospheric Depth`との責務区別（2026-11-12）

source auditで`builtin.atmospheric_depth`（`ArtifactAtmosphericDepthEffect`）を確認した。これはnamed depth input、または有効時のsource luma fallbackからnormalized depthを取り、near/far/curveで作るdepth-based color mixにGaussian-blurred highlights由来のglowを加えるRasterizer-stage effectで、alphaはsource値を保持する。`AtmosphereFogRenderer`はray・height-density・sample countを使う別のCPU volume-renderer APIであり、`heightFogCS.hlsl`も未接続shader資産である。名称にAtmosphericが共通するだけで、`builtin.atmospheric_depth`の出力をvolumetric fog baseline、depth texture契約、host integration evidenceとして扱わない。これはソース静的区別で、どちらもこのturnで実行比較していない。

## Depth encoding感度の合成診断（2026-11-12）

host auditでdepth契約が未確定だったため、既存の256×144合成scene rayとsurface距離を再利用し、fog評価前に深度値の解釈だけが距離復元へ与える影響を分離した。`tools/fog_depth_encoding_rd.py`は各rayの真のEuclidean距離とcamera-forward view-zから、linear Euclidean、linear view-z、一般的なforward-Z 0〜1 perspective device-depth式を作る。各値を`heightFogCS.hlsl`と同じ`near + depth * (far-near)`の線形decodeへ入れ、真のray距離とBeer–Lambert透過率との差を計算する。透過率比較は高さ密度をcamera y=4 m、ground=0、falloff=0.08で固定し、base density 0.002／0.008／0.02、absorption 0.7を使用した。shader固有の`distanceFactor * distance`式誤差は混ぜず、depth decodeだけを測る。

| 仮定した near/far | 入力encoding | 距離絶対誤差 p95 / max | density 0.008時の透過率絶対誤差 p95 |
|---|---|---:|---:|
| 0.1 / 120 | linear Euclidean distance | 0 / 0 | 0 |
| 0.1 / 120 | linear view-z | 32.8802 / 40.2921 scene units | 0.08782 |
| 0.1 / 120 | perspective device-zを線形depthとしてdecode | 108.7710 / 110.1537 scene units | 0.34338 |
| 0.1 / 1000 | perspective device-zを線形depthとしてdecode | 979.5622 / 979.7744 scene units | 0.94264 |

この結果は、normalised値という情報だけでは深度からfog距離を復元できず、perspective device-depthを線形distanceとして扱うとfar plane設定にも強く依存することを示す。linear view-zであっても画面外周ではEuclidean ray距離と一致せず、camera rayまたはworld positionを使う方式との差が残る。表の大誤差は明示した式を異なるencodingへ誤適用した場合の感度であり、Artifact/Diligentが現在そのencodingを使う証拠ではない。実hostのdepth format、reverse-Z有無、clear値、projectionとsampled channelは未確認なので、合成diagnosticは製品誤差・backend parity・画質評価として扱わない。全region集計・near/far別・density別の統計、encoding式、入力manifest hash、script hashは`docs/worklog/artifacts/fog-depth-encoding-rd-2026-11-12/manifest.json`に保存した。対象は距離契約の数値診断のため、画像出力やvisual-quality判定は行っていない。

## 決定論的Fog画像fixture（2026-11-12）

#### AABB shadow core幅誤差±2%の画像空間追試計画（2026-11-12、実行前登録）

- **目的:** 先行するAABB aspect-ratio／2×2 factor数値追試で使ったcore width ±2%が、既存96×54 linear RGB image fixture上のどの領域にどの程度現れるかを対応づける。screen-space proxyの画像測定であり、AABB形状別の実scene画像へ外挿しない。
- **比較対象と素材:** `fog_shadow_image_rd.py`と同じ固定opaque chart、pixel rays、depth/source colors、sun elevation 45°、penumbra 1%-of-ray、shadow ellipse/interval、scene-linear float32を使う。referenceはtrue intervalのprofile-aligned 4096 samples、validationは2048。candidateはcenter exact・core width error −2%/+2%・128 samplesの2条件。候補値を見る前に固定し、追加の品質閾値は作らない。
- **領域・測定:** 全画像、projected shadow interior/edge/exterior、near-horizon、HDR source、high-contrast edge proxyについてlinear RGB relative L2、RGB MAE/RMSE、absolute channel max・p95、max-channel差分>1e−3画素数を測る。画像全体と70×42 center cropを保存し、差分はraw PFMとgain 4096 previewで残す。原寸PFMを数値の根拠にし、同一tone-mapのPNGは目視補助に限る。
- **評価と解釈:** 指標に普遍的な合否閾値は置かず、2条件の位置・幅誤差研究に対する画像応答と失敗領域の記述比較とする。全体像・crop・差分を確認して欠陥候補を記録するが、screen-space ellipseは実shadow map/occluderでなく視覚品質の合格判定に使わない。
- **適用性・未検証:** 素材はopaqueのみなのでalpha評価は対象外（alphaを表現しないfixtureであることを明記）。この静止画比較では時間安定性、GPU/backend・preview/final parity、実アプリhost、性能/メモリは未検証のまま残す。実施日・input/output/script hashをmanifestに記録する。

#### 体積固定AABB aspect ratioのray-cast画像追試計画（2026-11-12、実行前登録）

- **目的:** 先行するdirect-ray時間残差で使った同一体積のbaseline／X長Z薄／X短Z厚AABBを、静止画像の視線rayと実slab intersectionで評価し、投影silhouetteとsingle-scattering画像への反映を調べる。
- **比較対象と素材:** `fog_height_camera_scene_rd.py`のscene-linear chartを96×54へ同条件でsampleする。cameraはx=0、y=4、target `(0,4,18)`、太陽高度45°、medium・source colors・depthを固定。AABB中心`(0,3.5,8.25)`、半寸法`(2.2,2.5,0.75)`、`(3.3,2.5,0.5)`、`(2.2/1.5,2.5,1.125)`で全shapeの体積を33に揃える。各rayはnearest scene surfaceまでのtrue AABB intervalだけ使い、penumbraをray長の1%とする。
- **測定:** 各shapeでprofile-aligned reference 4096／validation 2048／exact-interval profile-weighted candidate 128 samplesを生成する。候補−各shape referenceのquadrature誤差と、shape reference−baseline referenceの幾何画像差を別々に測る。全画像、shape別projected interior、baseline/shape silhouetteのXOR、binary edge unionでscene-linear relative RGB L2、RMSE、MAE、p95/max channel delta、`1e-3`超過pixel数を記録する。source、reference/candidate PFM・共通tone-map preview、gain-4096差分、crop、region masks/countsを保存する。
- **解釈:** これは固定camera・固定sceneのray-cast AABBとCPU single-scattering proxyで、実AABB比較がscreen-space ellipse proxyとどう違うかを見る記述評価である。画像差をshape間の品質順位や合否と解釈せず、uniform volumeでも投影遮蔽と寄与分布が変わりうる範囲を示す。
- **適用性・未検証:** 素材はopaqueでalphaを保持しない。時間安定性、実shadow map filtering、実host、GPU/backend・preview/final parity、性能/メモリは未評価のままとする。reference-validation誤差が積分精度を支持しなければshape差の解釈を保留する。

`tools/fog_height_image_fixture_rd.py` は、傾斜した近景／遠景チャート、地面checker、有限距離の空、HDR markerを持つ256×144の合成シーンを生成する。camera位置とFOVからrayを作り、各画素のopaque source colorとnormalized linear ray distanceを別々に出力する。depthはGPU hardware depthではなく`distance / 120`であり、alphaはopaqueとして省略している。

出力は `docs/worklog/artifacts/fog-atmosphere-rd-2026-11-12/` に保存する。PFMがunclamped linear float32 RGBの比較元、PNGはReinhard+gammaの目視用previewである。候補はF0 no-fog、F1 uniform analytic、F2 analytic exponential height、F3 midpoint 16/64 steps、F4 single-scattering height fogを含む。manifestには全ファイルhashとF2に対するlinear RGB誤差を記録した。

初回captureのsource auditで、fixture側が宣言したfalloff `0.08`ではなく、別モジュールのintegrator既定値`0.01`を使っていたことが判明した。`tools/fog_height_integral_rd.py` をfalloff/groundの明示引数にし、画像fixtureの全積分呼び出しから設定値を渡すよう修正してcaptureを再生成した。修正前の画像hashと誤差値は無効とする。

修正後のF3対F2 RMSEは16 stepsで`1.08e-5`、64 stepsで`6.74e-7`。F1 uniform対F2 heightは`0.02494`。これらは現在の256×144・density `0.008`・falloff `0.08`の線形float CPU fixture出力だけを比較する。

`Artifact/shaders/heightFogCS.hlsl`の式を同じfixture上で別途Python mirror化した。near=0、far=120、startDistance=0では`distanceFactor=distance/far`を求め、その後`opticalDepth=heightDensity*distanceFactor*distance`としているため、光学深度が距離の二乗に比例する。GPU試作式mirrorとheight-resolved解析消散参照のRMSEはcamera height 0／4／20／100で`0.04308 / 0.03813 / 0.01254 / 0.0000225`。式内のdistanceFactorを除いて線形ray距離にした「camera-height-only」診断でもRMSEは`0.02178 / 0.01980 / 0.007687 / 0.0000150`残り、カメラ高さだけで全rayを代表する誤差が分離して見える。camera height 100では媒質がほぼ消えるため両候補とも参照に近い。比較は固定depth/color画像のままcamera heightパラメータだけを変えており、シーンを高さごとに再投影した試験ではない。HLSL未コンパイル・未dispatchの式mirrorであり、GPU時間や実機出力ではない。距離二乗の挙動は現行source equationからの確認事項で、修正や実装推奨はR&D採用判断まで保留する。

camera height 4固定でdensity `0.002 / 0.008 / 0.02`、falloff `0.02 / 0.08 / 0.2`を掃引したshader式mirror対height-resolved参照のRGB RMSEは、density `0.002`で`0.01061–0.01136`、`0.008`で`0.03513–0.03814`、`0.02`で`0.07059–0.07794`だった。低密度ではfalloff差よりdistanceFactor項の影響が強く、高密度ほどsource式とworld-height積分の差が大きい。この掃引も同じscene depth/colorを固定した式比較であり、density/falloffの品質推奨値やGPU performanceを確定するものではない。

既存`AtmosphereFogRenderer::applyToPixel()`式も同じ入力・出力条件の別pairで確認した。ソース式をPythonで写したRGB8 depth/color mirrorと、同じdensity/falloff authoring値から作るBeer–Lambert height referenceを比較するとRGB RMSE `0.055996`、36,864画素中36,863画素で最大channel差が`1e-3`を超えた。source mirrorは`density * heightFalloff`を全rayで一定に使う一方、height referenceは`density * exp(-heightFalloff * max(height-ground,0))`を積分するため、この差はモデル式の差を示す。両者は同じRGB8入力・出力制約とfog設定を使い、source mirrorのmaxDistanceはfar planeに合わせて120とした。height referenceはBeer–Lambert消散のみで、`evaluateFog()`の方向性散乱は含めない。これはC++コードのruntime実行や散乱parityではなく、ソース式の独立Python mirrorである。元fixtureのHDR画像では524画素がRGB8 inputによりclipされ、ImageBufferにalpha channelはない。これらの制約も同じmanifestに記録した。

カメラ高とworld-space object heightを実際に変えるsynthetic scene fixtureとして`tools/fog_height_camera_scene_rd.py`を作成した。160×90 perspective viewのslanted near/far chart、checker ground、skyを、camera y=2／8／20 mごとに同じworld targetへ向け直して再描画する。piecewise analytic world-height Beer–Lambert referenceに対し、camera heightだけで密度を評価して線形距離積分する候補の全画面RGB RMSEは`0.01829 / 0.01677 / 0.03420`、`heightFogCS.hlsl` source equationそのままのmirrorは`0.03217 / 0.03800 / 0.04788`だった。y=20 mではground領域のRMSEがそれぞれ`0.04162`と`0.05766`に上がる。高い視点から地面を見るrayでは視線途中の密度変化をcamera-height定数で代表しにくい結果となった。camera-height linear候補はshaderの`distanceFactor * distance`項を除いた診断なので、source shader mirrorよりreferenceに近い。fixtureは実sceneのworld unitsやcamera host契約を再現せず、GPU shaderを実行していない。

同じsceneをdensity `0.002 / 0.008 / 0.02`、falloff `0.02 / 0.08 / 0.2`で独立sweepした。camera y=2 mでは全9条件のRMSE rangeがlinear camera-height候補`0.00292–0.03860`、source equation mirror`0.00866–0.06686`、y=8 mでは`0.00214–0.04665`と`0.00877–0.08760`、y=20 mでは`0.00479–0.06814`と`0.00788–0.11514`だった。camera y=20 m、density `0.02`、falloff `0.08`でground領域だけを見るとRMSEはlinear候補`0.08271`、source equation mirror`0.10087`。parameter応答は視点とsurface coverageに依存し、密度・falloffだけから採用境界を単一閾値にできない。manifestには27組のcamera×density×falloffについて全画面・near/far/ground/sky別のRMSE・最大channel誤差を保存した。全てsynthetic CPU mirrorであり、値はproduction設定推奨やGPU performance値ではない。

太陽経路透過の単独寄与を測る`tools/fog_sun_transmittance_rd.py`では、水平／上向き／groundwardの3 ray×camera y=2／8／20 m×太陽高度2／5／15／45／80°を評価した。view sampleごとの太陽経路はground crossingを区分した指数密度の閉形式積分で求め、view方向散乱は256-step midpointで積分する。正規化Rayleigh位相で太陽透過を入れたradiance対同じ式で太陽透過を無効にしたradianceの比は、水平ray・太陽高度2°でcamera y=2／8／20 mそれぞれ`0.1811 / 0.3475 / 0.6675`、80°では`0.9413 / 0.9632 / 0.9858`。camera y=2 mのgroundward rayでは2°時`0.1569`まで低下した。CPU `evaluateFog()`のsource phase係数・離散view attenuation・sun-path omissionをまとめたscattering-term equation mirrorは、正規化phase＋analytic sun-transmittance参照に対し45条件で`0.755–4.780`倍のradianceとなった。camera y=2 mのgroundward/2°では`4.780`倍、camera y=2 m horizontal/80°では`0.797`倍である。phase係数の0.75倍とsun-path attenuation欠如は別々の誤差方向を持ち、条件によって相殺も増幅もする。比較は散乱項だけで、C++ `evaluateFog()`最後の経験的horizon/fog blendは含めず、direction入力も共通化した式mirrorであってruntime parityではない。view積分stepを16〜1024で掃引し、4096-step midpointを参照にした45組のray/camera/sun条件では、256-stepの最大relative RGB L2誤差は`3.89e-5`、最大はcamera y=2 mの上向きray・太陽高度80°で生じた。同じrayの8192-step値に対しても256-stepの差は`3.90e-5`、4096-stepの差は`1.14e-7`で、参照刻み自体の収束も確認した。この誤差上限は解析的sun pathと平面指数媒質のCPU fixtureに限られ、shadow boundary、局所密度、GPU精度の保証ではない。平面媒質・固定fog tint/sun radianceのCPU参照であり、惑星曲率・地形shadow・GPU performanceやアプリruntime parityは含まない。

## cloud/light fog shader積分のCPU式監査（2026-11-13）

`tools/fog_shader_integral_rd.py`で`Artifact/shaders/fogHF.hlsli::GetFogAmount()`のheight integralをsource equation mirror化し、独立midpoint quadrature 65,536 stepと175条件で比較した。height start/end 10/40 m、origin y −20〜60 m、ray pitch −89〜89°、距離1〜1000 mの範囲でsource式の最大relative errorは`10.68%`。最大はorigin y=10 mの水平ray・1000 mで、source式の積分値893.22 m相当に対し、独立積分は1000 m相当だった。真に水平な場合にも`effectiveZ=max(abs(V.y),0.001)`が指数積分の指数・分母に使われ、密度がrayに沿ってわずかに減る扱いになることが差の原因である。診断用に真の水平極限と`expm1`を用いる解析候補式を同じ175条件で比較すると、quadratureに対する最大relative errorは`5.14e-7`まで下がった。この候補は数学的fixtureに限られ、GPU/HLSL動作や見た目の評価ではなく、production修正の推奨でもない。

開始距離のsource式は`startDistanceFalloff=saturate((distance-fog.start)/fog.start)`として全線積分へ係数を掛ける。fog.startより手前の区間を除くhard-cutoffとは別policyであり、exact height integralへsource fadeを適用した値とhard-cutoffの最大optical-depth差は`0.72`（origin y=−20 m、pitch=20°、distance=1000 m、start=60 m）だった。開始距離の意図された見た目・既存互換性は未確認であり、この差を単独で不具合とは判断しない。結果は`docs/worklog/artifacts/fog-shader-integral-rd-2026-11-12/manifest.json`に保存した。shaderは未コンパイル・未dispatchで、GPU性能、atmosphere LUT結合、host bindingの評価を含まない。

alpha合成契約の感度確認用に`tools/fog_alpha_compositing_rd.py`を追加し、straight RGBA入力をpremultipliedへ変換した決定論的21ケースを評価した。coverage alphaを保ちfogを被覆領域内へ限定する方式と、媒質の寄与をalphaにも含める方式は別のlayer semanticsで、同じ画像結果を意味しない。後者はalpha=0でもfog寄与を持つため、空の透明pixelを維持する処理には適さない。さらに、premultiplied RGBへfogを加えながら元alphaを保つnaive式では、transmittance 0.1時にalpha=0 pixelへ最大RGB `0.648`が残り、premultiplied合成時に透明pixelからfog色が漏れる。straight-alpha inputならRGB blend＋alpha保持を後段で正しくpremultiplyする経路が成立し得るため、GPU shaderのfloat4 input/outputだけではalpha conventionを断定できない。CPU `ImageBuffer`はRGB8でalphaなし、GPU shaderのhost bindingも確認できておらず、runtimeの正規契約や採用policyは未決定である。case表とblack/white/magenta背景へのpremultiplied composite値は`docs/worklog/artifacts/fog-alpha-compositing-rd-2026-11-12/manifest.json`に保存した。

## 球状大気 single-scattering CPU参照（2026-11-12）

`tools/planetary_atmosphere_scattering_rd.py` は球状惑星と大気shellのray/sphere交差を解き、view rayおよび太陽方向の光学深度、Rayleigh/Mie散乱、Rayleigh位相関数、Cornette–Shanks Mie位相関数を数値積分する。パラメータの基準はBruneton–Neyretの地球大気モデル（Rayleigh scale height 8 km、波長別Rayleigh係数、Mie scale height約1.2 km、g=0.76の代表値）に置く。Mie scattering/extinction比は0.9に設定した。同論文はこれらの大気係数・aerosol値の組み合わせでCIE clear-sky modelの測定fitを示すが、本fixtureはarbitrary sun radiance、単一散乱のみ、地面反射なし、狭いview setであり、その測定fitや論文方式を再現したものではない。係数の初期値を借りた式・収束のCPU参照として扱う。

球面上の位相関数積分diagnosticはcosθ区間を262,144分割し、Rayleigh `0.9999999991`、Cornette–Shanks Mie(g=0.76) `0.99999999825`（理想積分値1）となった。g sweepのg=0.9は`0.99999993334`である。以前の16,384分割ではg=0.9が`0.99998294`に留まったため、異方性が強いphaseでは角度積分の刻みも検算条件として保存する。96×54 capture、太陽高度18°・カメラforwardからの方位25°では、128×48積分を画像referenceとして32×16のRayleigh+Mie(g=0.76)がlinear RGB RMSE `0.02487`、64×24が`0.006648`。32×16でg=0（等方）にするとRMSE `0.3264`、Mie scatteringを切ると`0.4142`となり、太陽方向を視野内に入れるとMie位相差が強く表れた。前回の方位90°設定は太陽がカメラ視野外にあったため、前回版のMie差分は位相lookの評価値として採用しない。

異方性をg=0／0.5／0.76／0.9で比較するcaptureも追加した。同解像度32×16のg=0.76画像を基準にしたRMSEは、g=0で`0.3230`、g=0.5で`0.1385`、g=0.9で`0.2472`。この視野ではg=0.76がg=0.5よりg=0.9に対して近く見えるが、g=0.9はforward peakがより狭く、view/sun積分解像度32×16自体の誤差が混ざる。数値だけでg=0.76を最適値と結論しない。`mie_anisotropy_comparison_to_g076_32x16`は同一解像度同士、既存`linear_rgb_comparison_to_rayleigh_mie_g076_128x48`は128×48 referenceとの比較としてmanifest内で区別した。

太陽方向付近を含む6本のrayを個別に高密度化し、512×192を参照にした。最も厳しいsunward rayでの最大相対RGB誤差は、16×8で`38.1%`、32×16で`13.3%`、64×24で`3.49%`、128×48で`0.836%`、256×96で`0.167%`。128×48は全画素画像の収束を保証するものではないが、forward-scattering近傍では同設定の32×16画像比較より高いstep数が必要になる証拠となった。

view積分と太陽方向transmittance積分の誤差源を分けるため、地平線中心rayとsunward-upper rayで片方のstep数を固定した独立sweepを追加した。view側はsun steps=192固定・512×192結果基準、sun側はview steps=128固定・128×256結果基準で、最大相対RGB誤差を比較した。地平線rayのview steps 32／64／128／256では`5.87 / 1.48 / 0.354 / 0.071%`、sun steps 8／16／32／64／128では`7.78 / 2.66 / 0.790 / 0.200 / 0.041%`。sunward-upper rayではview側が`13.64 / 3.73 / 0.913 / 0.184%`、sun側が`3.60 / 1.05 / 0.284 / 0.070 / 0.014%`だった。今回の固定条件では地平線rayはsun-path積分、forward散乱近傍はview積分がより強い制約となる。これは選んだ2 rayのCPU数値収束であり、全画素の誤差上限やGPU時間を示さない。

view方向の同一sample budgetを保った診断候補として、放射方向外向きrayは`t=L*u²`、惑星中心へ向かうrayは`t=L*(1-(1-u)²)`へ寄せ、接線付近では`smoothstep(abs(radial_direction_y)/0.1)`でlinear mappingと滑らかにblendし、各u-cellのworld距離幅を積分weightとする`altitude_biased`配置を試した。uniform 32×16に対し、参照をuniform 512×192に固定した32-step ray誤差はzenithで`6.191%→0.269%`、sunward-upperで`13.642%→0.497%`へ低下した。center horizonはuniform `5.868%`に対してbiased `6.375%`、groundwardは`0.006%→0.016%`と小さく悪化した。全96×54画像ではbiased 32×16のRMSE `0.01204`、uniform 32×16は`0.02487`で、約52%低下した（双方ともuniform 128×48画像参照）。初期のhard angle thresholdは小さなpitchでsample配置が切り替わり、horizon proxyの最大相対誤差を一時約25%まで跳ねさせた。smoothstep blend後は最大`6.59%`となり不連続は解消したが、uniformの`4.76%`より残差は大きく、接線付近での安定性はなお弱い。
view方向の同一sample budgetを保った診断候補として、放射方向外向きrayは`t=L*u²`、惑星中心へ向かうrayは`t=L*(1-(1-u)²)`へ寄せ、接線付近では`smoothstep(abs(radial_direction_y)/0.1)`でlinear mappingと滑らかにblendし、各u-cellのworld距離幅を積分weightとする`altitude_biased`配置を試した。uniform 32×16に対し、参照をuniform 512×192に固定した32-step ray誤差はzenithで`6.191%→0.269%`、sunward-upperで`13.642%→0.497%`へ低下した。center horizonはuniform `5.868%`に対してbiased `6.375%`、groundwardは`0.006%→0.016%`と小さく悪化した。全96×54画像ではbiased 32×16のRMSE `0.01204`、uniform 32×16は`0.02487`で、約52%低下した（双方ともuniform 128×48画像参照）。初期のhard angle thresholdは小さなpitchでsample配置が切り替わり、horizon proxyの最大相対誤差を一時約25%まで跳ねさせた。smoothstep blend後は最大`6.59%`となり不連続は解消したが、uniformの`4.76%`より残差は大きく、接線付近での安定性はなお弱い。

この配置の条件依存性を測る追試として`tools/planetary_scattering_sampling_robustness_rd.py`で太陽高度2/18/75°、camera altitude 0.5/2/10 km、horizon・低角度・sun-aligned・zenithを含む重複なし51 ray組を比較した。sun積分48 step固定、uniform 128×48参照で、altitude-biased 32はuniform 32に対し36組で誤差が小さく、6組で大きく、9組で同等。median relative RGB L2は`0.00115`対`0.00410`、p95は`0.0241`対`0.1137`だった。一方uniform 64のmedianは`0.000861`、p95は`0.0257`で、biased32よりmedianが小さく、p95は近い。altitude-biased配置は全角度でuniformを上回るものではない。

主要outlierを`tools/planetary_scattering_sampling_convergence_rd.py`でsun積分192 step固定・uniform view 128/256/512/1024で追試した。1024×384参照に対するsun-aligned ray（太陽高度75°／18°、camera高度0.5 km）のrelative RGB L2は、altitude-biased 32で`0.532% / 0.470%`、uniform 32で`17.19% / 14.99%`、uniform 64で`4.83% / 4.05%`だった。horizon/high-sun rayではbiased32とuniform32が同じ`6.46%`、uniform64は`1.70%`。512×192から1024×384へ増やしたreferenceの相対変化はこれら3 rayで`0.054% / 0.034% / 0.011%`で、選択caseのreference収束は確認した。manifestは`docs/worklog/artifacts/planetary-atmosphere-sampling-robustness-rd-2026-11-13/`にある。これは54組のselected CPU raysと3 outlierだけで、全画面・全高度やGPU性能の保証ではない。

同じrayをcamera-right軸回りに−1°から+1°へpitchさせる5方向のcamera-motion proxyで、各方向のuniform 512×192を基準にした。sunward-upperではuniform 32×16の最大frame相対誤差`13.47%`・連続frame差分RMSE `0.001313`に対し、biasedは`1.24%`・`0.000203`。center horizonではuniformが`4.76%`・`0.03189`、biasedが`6.59%`・`0.04002`で、biasedはわずかに悪化した。したがってsunward rayのsample収束改善はcamera angular sweepでも残る一方、horizon付近はuniformが優位である。このray rotationは静的な大気場を通るcamera pitchの数値proxyで、rendered sequence、reprojection、実GPU temporal stability試験ではない。別カメラ高度、gの高い鋭いpeak、float/GPU実装での品質やper-sample mapping演算の実時間コストは未検証。Capture名は`rayleigh_mie_g076_altitude_biased_32x16`。

太陽高度を0°／5°／18°／45°／75°へ掃引した同じCPU sky fixtureでは、Rec.709重みを使ったtone mapping前の平均linear luminance proxyは`0.1136 / 0.4214 / 0.4957 / 0.3481 / 0.2629`。カメラ高度0.5／2／10／50 kmではそれぞれ`0.5778 / 0.4957 / 0.3929 / 0.2386`。低い太陽ほど長い斜め大気経路となり赤寄りの散乱が残り、高度上昇では光学深度が減って散乱radianceが低下する傾向がこの固定view条件で見えた。RGB primaries、solar radiance、sun disk、surface illuminationを較正していないため、Rec.709 weighted metricは物理輝度ではなくパラメータ応答を見るproxyである。

Captureとmanifestは `docs/worklog/artifacts/planetary-atmosphere-rd-2026-11-12/` にある。PFMはlinear radiance、PNGは同じReinhard+gammaのpreview。manifestは位相関数球面積分、6 rayのstep convergence、sun-elevation／camera-altitude radiance summaryを保持する。現状は黒い惑星surface radiance、sun disk、雲、地形shadow、多重散乱、spectral integrationを含まないため、主にsky angular responseとstep convergenceを比較する。

## 現時点の用途別候補（CPU/source evidenceに限定）

下表はR&Dを次段階へ進めるための候補整理であり、GPU実測・production採用の決定ではない。

| 用途 | 次に評価する候補 | 現時点の根拠 | 採用前に必要な証拠 |
|---|---|---|---|
| Composition / 2D postprocess | camera rayとworld positionをdepthから復元し、指数height densityを解析積分するF2を基準にする。world depthがない時だけF1を比較基準にする。 | synthetic world-height sceneでcamera-height-only候補のRMSEはcamera y=2/8/20 mで`0.01829/0.01677/0.03420`。`heightFogCS.hlsl` source式mirrorは`0.03217/0.03800/0.04788`。depth encodingの仮定を誤るとBeer–Lambert透過率p95差が大きくなる。 | 実Compositionのdepth format・clear値・clip convention・inverse view-projection、HDR/alpha契約を確定し、同一GPU imageをF0〜F3でcapture。GPU時間とresource lifetimeも記録する。 |
| Cloud / light shader内fog | 既存`fogHF.hlsli`のanalytic height integralと太陽LUT応答を基準にし、水平極限処理を独立候補として比較する。 | 175 CPU式条件のsource積分最大誤差は水平rayで`10.68%`。真の水平極限＋`expm1`候補は同じ数値参照に対して最大`5.14e-7`。太陽透過の影響はcamera高・太陽角で変わる。 | HLSLの水平・準水平ray capture、既存cloud/light visual baseline、GPU instruction/time、位相・太陽透過を一度に変えないfactor comparison。 |
| 3D viewport / 局所fogとshadow | 局所密度・複数light・遮蔽が要求される場合にF5 froxelを独立検証する。既存cloud shaderをそのまま一般viewport passと見なさない。 | 3-light局所fog CPU proxyではtile4×32 sliceがrelative RGB L2`0.00300`、tile4×64は`0.00306`で、当fixtureではsliceを倍にしても改善しなかった。tile8×64はcell数をtile4×64の1/4に減らす一方、誤差は`0.00727`へ増えた。既存shadow proxyでは境界位置・core幅誤差がcamera-motion residualを左右した。 | 実viewportのdepth/shadow入力、fog injection、temporal reprojection、複数camera motion、GPU memory/time、ghostingとbackend parity。 |
| Planetary sky / aerial perspective | F6 atmosphere LUT系を独立候補とし、height fogとは別の入力・品質予算にする。 | 球状大気single-scattering CPU fixtureでRayleigh/Mie位相、太陽角・高度・step収束を比較済み。32×16 altitude-biased sample配置はこの画像でuniform 32×16よりRMSEが約52%低い一方、horizon ray誤差はuniformよりわずかに高い。 | LUT生成／参照時間、複数散乱・地面反射・sun disk、全視野の視覚比較、カメラ高度と太陽角のcoverage、GPU memory。 |
| CPU renderer | `AtmosphereFogRenderer`はCPU reference/fallbackとして扱い、GPU Composition品質の代理にしない。 | `applyToImage()`のRGB8 depth/colorにはalphaがなく、`applyToPixel()`はheight-resolved ray oracleではない。`evaluateFog()`は独立ray積分で散乱式を持つがGPU pathの契約とは異なる。 | CPU runtimeを用途に残す場合は設定・色域・既存出力との互換性を別途比較し、GPU採用とは別判定にする。 |

現段階では、画質とCPU数値の比較はあるがGPU時間・memory・backend差の証拠がなく、production採用できる方式はまだ決めない。特にF2/F3、F4、F5、F6を共通の「fog quality」値で順位付けせず、上表の用途ごとに入力契約とGPU captureを埋める。

### F5局所fog / froxel CPU proxy（2026-11-13）

`tools/fog_local_froxel_proxy_rd.py`では96×54の同一sceneへGaussian局所密度3領域、point light 3灯、analytic AABB遮蔽2個を置き、direct raymarch 512 stepを参照に、256 stepで参照収束を検証した。256対512のrelative RGB L2は`7.11e-5`。direct32/64はそれぞれ`5.23e-4 / 1.96e-4`だった。

tile×depth sliceのfroxel gridを三線形sampleするCPU proxyは、tile2×64=`0.00249`、tile4×64=`0.00306`、tile8×64=`0.00727`、tile4×32=`0.00300`のrelative RGB L2。tile8×64のfar-surface maskはRMSE`0.00469`、relative RGB L2`0.01015`となり、深度のある局所fogではfull-image平均だけで粗いgridの誤差を見逃し得る。64 midpoint probeでdensity`>=0.003`かつどれかのlight segmentがAABBを横切るpixel（2,275 pixels）をまとめたshadow-influenced maskでは、tile2/4/8×64のrelative RGB L2が`0.00330/0.00402/0.00974`、direct32は`0.000721`だった。shadow mask内でfroxel errorが増える傾向は見えたが、maskはshadow boundary pixel単独ではなく、probe ray上で影響を受ける領域全体を示す。tile4では32から64 slicesへ増やしても当fixtureで誤差改善はなく、tile4×32はtile4×64のhalf slice数でわずかに低い誤差だった。これは特定配置の結果であり、一般的なslice数の優劣を意味しない。

memory arithmeticはgrid cell数と仮定bytes/cellを明示しており、tile4×32は10,752 cells、tile2×64は82,944 cells（7.71倍）。例示的なR16F density + RGBA16F sourceならそれぞれ107,520 bytesと829,440 bytes。これは実際のGPU resource layout、alignment、history、driver allocationを測った値ではない。scriptはinjection light-evaluation回数、pixel×slice積分数、三線形lookupの最大cell-read数も別々に保存する。CPU elapsed timeをGPU時間へ換算しない。

このproxyはisotropic phase、hard AABB shadow、固定3 point lights、synthetic Gaussian densityを使い、temporal reprojection、複数backend、実shadow-map filteringを含まない。PFM/PNG capture、差分表示gain 24、region別metric、memory算術と全条件は`docs/worklog/artifacts/fog-local-froxel-proxy-rd-2026-11-13/manifest.json`にある。sampled froxelの品質・費用tradeoffを形にした段階であり、GPU F5候補の採用を示さない。

### F5局所fog / light count sweep（2026-11-13）

`tools/fog_local_froxel_light_count_rd.py`で同じsceneの先頭1/2/3灯を比較した。512 step referenceに対する256 stepのrelative RGB L2は順に`4.44e-6 / 7.10e-5 / 7.11e-5`。direct32は`4.23e-5 / 5.21e-4 / 5.23e-4`。全画像relative RGB L2ではtile4×32が`0.00452 / 0.00339 / 0.00300`、tile4×64が`0.00445 / 0.00343 / 0.00306`、tile8×64が`0.00795 / 0.00566 / 0.00727`だった。この固定順序・sceneでは灯数増加でfroxel誤差が単調増加せず、誤差を直接的な灯数スケーリングとみなせない。

light evaluationの演算算術はlight countに比例する。tile4×32は10,752 cellのままなので1/2/3灯で10,752 / 21,504 / 32,256 evaluation、tile4×64は21,504 / 43,008 / 64,512、tile8×64は5,376 / 10,752 / 16,128。direct32は5184×32×N、すなわちN=1/2/3で165,888 / 331,776 / 497,664 light evaluation。memory算術は灯数を増やしても同じ仮定resource layoutのもとで不変。

shadow influence maskは64 midpoint probe、density閾値`0.003 / 0.0001 / 0.00001`の感度をmanifestへ記録した。閾値`0.003`では1/2/3灯で8 / 1,309 / 2,275 pixelであり、1灯のmaskは小さくregion誤差の代表性が低い。したがって主比較は全画像誤差とし、mask値は遮蔽配置に依存する診断とする。灯数は固定light配列のprefixであり、任意light選択、shadow map、実行時間、GPU memory/performanceは測っていない。詳細は`docs/worklog/artifacts/fog-local-froxel-light-count-rd-2026-11-13/manifest.json`。

### F5局所fog / log-linear depth distribution CPU sweep（2026-11-13）

`tools/fog_local_froxel_depth_distribution_rd.py`でtile4、3灯を固定し、log / linear view-Z分割と16 / 32 / 64 slicesを比較した。512-step direct基準に対する256-step validationはrelative RGB L2`7.11e-5`。全画像のrelative RGB L2はlog 16/32/64で`0.00531 / 0.00300 / 0.00306`、linearでは`0.01238 / 0.00500 / 0.00327`だった。このfixtureではlog配置が16/32/64すべてで低い全体誤差を示し、32 slices logはlinearより約40%低いが、64 slicesでは差が小さくなる。

view-Z帯別では、log配置がnear (`<20`) のrelative RGB L2を16/32/64 slicesで`0.00244 / 0.00156 / 0.00145`に保ち、linearでは`0.01367 / 0.00575 / 0.00238`だった。一方mid (`20–50`) 64 slicesはlog`0.00588`、linear`0.00574`、far (`>=50`) 64 slicesはlog`0.00380`、linear`0.00369`でlinearがわずかに低い。従ってlog配置の全体優位はnear regionに多くのsliceを割り当てるこの固定fixtureと画像寄与に依存し、遠景精度や別sceneでの一般優位を示さない。

同じslice数ならgrid cell数、light evaluation数、仮定memory算術はdistribution間で等しい。log配置はnear-to-farのbin幅が非一様（32 slicesで約`0.093`から`18.89` view-Z unit）、linearは一定幅（約`3.734`）。実depth discontinuity、production projection/depth convention、GPU filtering、時間安定性、GPU costは含まない。詳細なband counts/metricsは`docs/worklog/artifacts/fog-local-froxel-depth-distribution-rd-2026-11-13/manifest.json`。

### F5局所fog / camera translation temporal CPU proxy（2026-11-13）

`tools/fog_local_froxel_temporal_rd.py`で固定scene・固定3灯に対してcameraをX方向へ`-0.4, -0.2, 0, +0.2, +0.4`移動し、tile4×32 log gridを各frameで再構築する候補と、最初のgridをscreen coordinateへそのまま再利用するnegative controlを比較した。512-step direct referenceの256-step validation relative RGB L2はframeごとに`0.00056–0.00071`（center poseは`7.11e-5`）で、端poseのreference convergenceがcenterより低いため、そのposeでの差分はわずかに弱い基準となる。

再構築候補のreference比relative RGB L2は左から`0.01135 / 0.01109 / 0.00300 / 0.01079 / 0.01049`。screen-space froxel gridの固定camera sampling誤差はpose依存であり、中央frameのみの静止metricではsequenceを代表しない。初期grid流用controlは`0.01135 / 0.00959 / 0.00347 / 0.00952 / 0.01117`で、rebuildより良いframeも悪いframeもあった。これは補間／history filterやmotion-vector reprojectionを持たず、temporal reuseの有効性を示すものではない。

隣接frameの`candidate-reference` residual image差分RMSEはrebuildで`0.00064–0.00448`、stale controlで`0.00151–0.00392`、最大channel変化はrebuildで最大`0.0581`、staleで最大`0.0519`だった。差分はpose間のapproximation error driftを測り、実sceneのframe差そのものとは分けている。`candidate-reference`誤差は中央で小さく、左右poseで高くなるため、一定方向へ蓄積するghostingはこの短いsequenceだけでは評価できない。captureとframe別metricsは`docs/worklog/artifacts/fog-local-froxel-temporal-rd-2026-11-13/manifest.json`。

このCPU studyは単一方向の静的translation、screen-space再構築grid、analytic Gaussian medium、hard AABB visibilityのみで、GPU temporal reprojection、motion vector、jitter、disocclusion rejection、history clamp、回転・前後移動、動的light/volumeを含まない。従ってtemporal stabilityの合否やproduction history設計の判断には使わず、reprojectionを含むGPU実験前にpose依存空間誤差が残ることを示す診断とする。

### F5局所fog / XY tile幅とfine volume stress CPU sweep（2026-11-13）

`tools/fog_local_froxel_spatial_resolution_rd.py`で96×54、3 lights、log depth 32 slicesを固定し、XY tile幅`1/2/4/8/16`と、baseline Gaussian、sigma半減Gaussian、Xへ0.5 world unit移動したbaseline volumeを比較した。512-step direct referenceの256-step validation relative RGB L2はbaseline / fine / shiftedで`7.11e-5 / 2.86e-6 / 6.53e-5`。

baseline sceneの全画像relative RGB L2はtile1/2/4/8/16で`0.00141 / 0.00335 / 0.00300 / 0.00716 / 0.01555`。fog-only（source surfaceを黒にして散乱radianceだけを比較）のrelative RGB L2は`0.0252 / 0.0971 / 0.0926 / 0.1385 / 0.2974`。tile幅と誤差は厳密な単調関係ではなく、tile2がtile4をわずかに上回る例が引き続き見られた。tile4からtile1へ細分化するとbaseline fog-only誤差は`9.26%`から`2.52%`へ下がる一方、grid cell数は`10,752`から`165,888`（15.4倍）になる。

sigma半減volumeでは全画像metricはtile2/4で`0.000613 / 0.00106`と小さく見えるが、fog-only relative RGB L2は`0.483 / 0.911`に達し、tile8/16では`1.62 / 2.82`。fog signal自体が小さくsurface RGBで正規化したmetricが細いvolumeの消失を隠すこと、粗いgridがこのthin Gaussianの散乱寄与をほぼ再現できないことを示す。tile1でもfog-only誤差は`0.0294`。半幅は三軸すべてのsigmaを半分にしamplitudeは固定しているため総積分密度も下がるstress caseで、hard-edged volume境界を直接評価したものではない。

baselineをX方向へ0.5移動した条件はfog-only relative RGB L2がtile1/2/4/8/16で`0.0251 / 0.0991 / 0.0942 / 0.1405 / 0.3007`であり、今回の粗いworld-space offsetではphase感度に大差は出なかった。ただしpixel phaseを厳密に半pixel相当へ揃えた摂動ではない。tile1は多くのcell数を要してもfinite direct-to-grid sampling差が残るため、単にtile幅を縮めれば精度問題がなくなるわけではない。局所volumeを扱う候補では、適応解像度、volume-aware injection、解析的積分などを今後比較する価値がある（提案、未検証）。詳細な全画像/fog-only誤差、cell/memory arithmeticは`docs/worklog/artifacts/fog-local-froxel-spatial-resolution-rd-2026-11-13/manifest.json`。

### F5局所fog / hard-edged volume boundary CPU stress sweep（2026-11-13）

`tools/fog_local_froxel_hard_boundary_rd.py`でbinary density box（inside`0.035` / outside`0`、概ね4×8×4 world unit）を使い、centered、X +0.5、depth +0.5の三配置をtile1/2/4/8 pixel×log depth 16/32/64 slicesで比較した。各候補を同一512-step direct referenceへ照合し、surface込みとsurfaceを黒にしたfog-onlyのRGB RMSE / relative L2をmanifestへ保存した。

centered boxの全画像relative L2はtile1/2/4/8×32 slicesで`0.00241 / 0.00284 / 0.00463 / 0.00478`、fog-only relative L2は`0.316 / 0.395 / 0.592 / 0.547`。tile4×64でもfog-only relative L2`0.590`であり、depth sliceを倍にするだけではXY方向のhard boundary差を解消しなかった。tile4×32でboxをXへ0.5ずらすとfog-only relative L2`0.438`、depthへ0.5ずらすと`0.573`。depth +0.5ではtile4×64が`0.497`に改善するが依然大きく、配置とslice境界の影響を受ける。

fog-only relative L2は小さいfog reference energyに対して正規化されるため、surface RGB込みの誤差より大きくなり得る。manifest内のfog-only RMSEとfull-image RMSEも併記して評価する。今回のboxはCPU analytic binary densityでproduction volume rasterization、filtering、shadow/depth edgeとの合成は含まない。tile/sample配置によるhard boundary leakageとhaloの可能性を示すstress evidenceであり、productionでの漏れ量を主張するものではない。詳細は`docs/worklog/artifacts/fog-local-froxel-hard-boundary-rd-2026-11-13/manifest.json`。

### F5 hard-box direct referenceのray-interval oracle監査（2026-11-13）

前項の512-step uniform direct referenceが短いhard-box intersectionを十分解像するか確認するため、`tools/fog_local_froxel_hard_boundary_oracle_rd.py`でslab intersectionからray-box区間を求め、その区間内だけpoint-light scatteringをmidpoint quadratureした。密度区間とBeer-Lambert transmittanceはbinary boxに対して解析的に与え、区間内照明だけを数値積分する。

8/16/32/64/128-step interval quadratureを256-step oracleと比較したとき、128→256差は3配置のfull-image relative L2が`9.3–9.7e-6`、fog-only relative L2が`0.00153–0.00188`。従ってこのsceneでは256-step interval resultを収束targetとして使える。従来uniform512 raymarchとinterval256 oracleとの差はfull-image relative L2`0.000141–0.000149`、最大channel差`0.00202–0.00246`だった。boxと交差するpixel rayは配置ごとに60 / 65 / 60本で、全pixel一様stepではbox境界区間のsamplingが少数rayへ集中する。

同oracleを基準にしたtile2/4/8×32のfog-only relative L2はcentered配置で`0.402 / 0.590 / 0.548`、X shiftで`0.485 / 0.437 / 0.552`、depth shiftで`0.553 / 0.575 / 0.564`。tile4×64 depth shiftは`0.498`。以前のuniform512基準で見た同傾向がinterval oracleでも残るため、hard-box局所誤差はuniform referenceの収束不足だけでは説明されない。ただしfog-only relative値は小さい参照fog energyで正規化されるため、absolute RMSEとpixel coverageも合わせて読む。reference定義・全収束値・froxel比較は`docs/worklog/artifacts/fog-local-froxel-hard-boundary-oracle-rd-2026-11-13/manifest.json`。

### F5 projected-box coverage approximation negative result（2026-11-13）

`tools/fog_local_froxel_box_coverage_rd.py`でAABB 8頂点をscreenへ投影し、screen上の投影矩形overlap率と正確なview-Z区間overlap率を掛けてfroxel density coverageとする近似を実装し、ray-interval oracleと比較した。tile4×32 fog-only relative L2はcenter-sample baselineのcentered / X shift / depth shift=`0.592 / 0.438 / 0.573`に対し、coverage候補は`0.669 / 0.581 / 0.688`で全配置悪化。tile2/8×32およびtile4×64でも系統的な改善は得られなかった。

この方法はprojected boxをscreen-aligned rectangleに外接させ、XY/depth occupancyを分離積として仮定するため、froxel内の真のray-box交差形状・相関を失う。今回のsynthetic条件ではdensity coverageを平均化するだけの提案を支持しないnegative resultである。exact projected footprint、各froxel内box-ray coverage integral、volume-aligned/adaptive gridなどは別候補だが未検証。詳細なfull/fog-only誤差・cell算術は`docs/worklog/artifacts/fog-local-froxel-box-coverage-rd-2026-11-13/manifest.json`。

### F5 per-froxel ray-box depth occupancy CPU probe

前項のprojected-box rectangle案とは別に、`tools/fog_local_froxel_ray_box_coverage_rd.py`で各XY froxelのstratified subpixel rayとAABBの交差区間を求め、区間を各log-depth sliceへclipしてslice内occupancy fractionを注入した。交差区間内の照明もfraction-weightして平均し、ray-interval oracleと比較した。

tile4×32のcenter-sample baseline fog-only relative L2（center / X shift / depth shift=`0.592 / 0.438 / 0.573`）に対し、XY中心ray一本でdepth occupancyを正確にした候補は`0.437 / 0.322 / 0.459`へ改善した。grid cell数は同じ10,752であり、box/slice区間処理と点灯評価が追加される。XY ray数を2×2へ増やすとcentered / depth shiftは`0.428 / 0.449`と小幅改善するが、X shiftは`0.333`へ悪化した。4×4 / 8×8で一貫した追加改善はなく、評価ray-box testsは1×1の10,752から4×4の172,032、8×8の688,128へ増える。tile4×64も常に良くなるわけではなく、centeredは1×1で`0.464`とtile4×32より悪く、depth-shiftでは2×2で`0.444`。

この結果は、粗いdepth sampleでbox区間をまるごとmissする誤差をslice occupancy fractionが減らす可能性を示す。一方XY位置のdensity/lighting代表誤差、後段transmittance/scatteringの非線形近似は残る。screen-space AABB外接coverage negative resultと異なり、ray-box geometryを各rayで保つためこのsynthetic boxでは改善したが、任意密度volume、実GPU時間、shadow filter・depth discontinuityとの結合で有効とは未確認。結果、coverage算術、cell/memory費用は`docs/worklog/artifacts/fog-local-froxel-ray-box-coverage-rd-2026-11-13/manifest.json`。

### F5 multi-box additive density coverage CPU probe

`tools/fog_local_froxel_multi_box_coverage_rd.py`で2つのAABB密度volumeを重複、分離、depth-staggered overlapの3配置へ置いた。oracleは全ray/box区間endpointで積分区間を分割し、重なり中は各boxの一定密度を加算して照明を積分する。128対256 substeps/oracleのfull-image relative L2は`5.7e-6–1.21e-5`。

tile4×32、center-sample baselineのfog-only relative L2はoverlap / disjoint / staggered overlapで`0.457 / 0.631 / 0.706`。各XY cellのcenter rayでboxごとのslice occupancyを計算して密度加算する1×1 coverageは`0.350 / 0.474 / 0.387`へ改善した。2×2は`0.349 / 0.452 / 0.385`とやや改善、4×4は`0.373 / 0.462 / 0.390`で改善が戻った。全画像relative L2もbaseline`0.00260 / 0.00385 / 0.00419`からcoverage1×1`0.00155 / 0.00284 / 0.00278`へ低下した。

同manifestにはBeer–Lambert透過率を散乱RGBから独立して測るmetricもある。volume交差pixelのoptical-depth RMSEはcenter-sampleからxy1×1 occupancyでoverlap`0.0302→0.0176`、disjoint`0.0335→0.0249`、staggered overlap`0.0419→0.0219`へ低減。transmittance RMSEは同順に`0.0282→0.0164`、`0.0314→0.0233`、`0.0376→0.0203`。ただし最大absolute tau errorはoccupancy後も`0.051 / 0.069 / 0.114`で、staggered配置の極端ray誤差が残る。2×2ではmean RMSEが少し下がる場合があるが4×4は戻る。平均誤差だけでなくmaxと交差ray範囲を確認する。

各boxの密度occupancyを足し合わせる方法はこの2-box homogeneous additive-medium fixtureで動作し、重なりを含む複数volumeへ概念を拡張できた。ただしcoverage法はboxごとのintersection/light evaluationを行うためvolume数とXY ray数に応じて計算数が増え、XY sample増加は単調改善しない。異なるanisotropic phase、媒体ごとのalbedo/emission、shadow-map filtering、GPU runtimeは未検証。詳細は`docs/worklog/artifacts/fog-local-froxel-multi-box-coverage-rd-2026-11-13/manifest.json`。

### F5局所fog / XY subsample injection CPU probe（2026-11-13）

`tools/fog_local_froxel_injection_sampling_rd.py`でhard-edged box三配置、tile4/8、32 depth slicesを固定し、各froxelのXY injectionを1×1 / 2×2 / 4×4評価点の平均にした。cell数と仮定memoryは不変で、field/light evaluation数のみ1/4/16倍になるproxyである。

tile4 centered boxのfog-only relative RGB L2は`0.592 / 0.537 / 0.502`（1×1 / 2×2 / 4×4）と改善したが、light evaluationは`32,256 / 129,024 / 516,096`へ増えた。depth-shift boxは`0.573 / 0.503 / 0.509`で2×2が改善、4×4はやや戻る。一方X-shift boxは`0.438 / 0.454 / 0.458`とsubsampleで悪化。tile8では2×2 / 4×4が全3配置で1×1よりfog-only誤差を増やした。全画像指標も同様に条件依存であり、subsampleの一様な解決効果は確認できない。

この比較はfield sample平均と後段のBeer-Lambert integrationの非線形性を保持した簡易proxyで、CPU/GPU時間を計測していない。subsampleはgrid memoryを変えずに計算評価を増やす算術案だが、改善量が配置依存で小さく、コストに対して安定した解決策とは示せなかった。box被覆率のanalytic integration、volume-aligned grid、adaptive local resolutionなどは候補仮説として残るが未検証。詳細は`docs/worklog/artifacts/fog-local-froxel-injection-sampling-rd-2026-11-13/manifest.json`。

### F5局所fog / froxel representation vs integration error CPU sweep（2026-11-13）

`tools/fog_local_froxel_integration_convergence_rd.py`でhard box三配置、tile4×32、tile8×32、tile4×64のgridを一度構築し、各slice内ray integrationだけを1/2/4/8 substepsへ増やした。substep=1は従来のlog-bin geometric midpoint sampleを維持し、追加substep条件は各slice内を等しいview-Z間隔で評価する。512-step direct referenceと比較する際にgrid constructionは固定なので、sampling/integration寄与の変化を切り分ける。

centered boxのtile4×32 fog-only relative L2はsubsteps 1/2/4/8で`0.592 / 0.614 / 0.612 / 0.611`、tile4×64は`0.590 / 0.601 / 0.600 / 0.600`。substep増加でdirect referenceへのfog-only誤差は改善せず、full-image RMSE変化も僅少だった。tile8×32はfog-only`0.547 / 0.543 / 0.543 / 0.543`と少し改善するが、全体差は限定的。X/depth-shiftでも同様に1→8 substepsの差は小さく、fog-only相対誤差は配置次第で増加または減少した。

従ってこのstress fixtureでは、主要誤差は1 sample/sliceの単純な積分不足より、既に構築された粗いgrid representationとinterpolationの寄与が大きい。ただしsample integrationを細分するとgrid近似誤差との相殺が外れ、direct referenceに対する誤差が増える場合もあるため「substeps増で誤差が減らない=純粋な空間誤差」と断定はしない。CPU operation countも1/2/4/8倍に増える。`manifest.json`は全image/fog-only RGB RMSEとrelative L2を保存する。製品GPUでは実測しておらず、hard box proxyの診断である。

### R&D通過条件

- F1は解析透過率参照と数値誤差が一致し、near/far・start distance・無効depthの境界条件が明文化される。
- F2はheight/ray directionを変えた全fixtureで参照積分に対する誤差上限を設定し、F3はstep数を増やした際の収束を示す。具体的な許容値はcapture前に解像度・precisionを基に確定し、結果を見てから都合よく変更しない。
- 高さを変えた物体と視線でcamera-height定数近似の限界を可視化し、F2/F3で改善するケースを特定する。
- F4〜F6は追加コストと視覚上の改善を同一条件で示す。改善を定量・視覚の両方で確認できない候補は採用推奨にしない。
- 完了レポートに候補ごとの品質・時間・memory、backend、未確認項目、用途別推奨を記載する。試作結果を本番統合済みと誤認させない。

## フェーズ

### Phase 0 — 契約監査（静的、shader契約とComposition host未確定）

上表のソースを照合し、各rendererのdepth encoding、coordinate space、color/alpha、near/far、camera matrix、現行host bindingを確認する。確認できない項目は推測で補完せず、R&D adapterの入力要件として列挙する。

### Phase 1 — CPU numeric oracleとfixture（数値積分の原型あり）

`tools/fog_height_integral_rd.py`で地面交差を含む5 rayと閉形式透過率のCPU数値比較およびCPU source phase正規化audit、`tools/fog_height_convergence_sweep_rd.py`で1,512条件×2 midpoint方式の事前gate付き収束sweep、`tools/fog_height_sampling_distribution_rd.py`で`rho^(1/3)`配置を含む1,512条件の消散積分比較、`tools/fog_scattering_sampling_distribution_rd.py`で45条件のsingle-scattering sample配置追試、`tools/fog_shadow_sampling_rd.py`で189条件のshadow visibility境界stress sweep、`tools/fog_shadow_boundary_sensitivity_rd.py`で境界位置誤差感度11,907結果、`tools/fog_shadow_penumbra_rd.py`でsmoothstep penumbraとshadow幅誤差の12,096結果、`tools/fog_shadow_combined_error_rd.py`で位置・幅複合誤差の18,900結果、`tools/fog_shadow_image_rd.py`で96×54 shadow境界誤差画像fixture・region metric・crop/difference capture、`tools/fog_shadow_camera_sequence_rd.py`で7-frame camera translationのshadow誤差時間residual計測、`tools/fog_shadow_reprojection_rd.py`でknown-world-point再投影とtemporal residualを7,770 pair rows測定、`tools/fog_shadow_temporal_outlier_rd.py`で上位16件をcontinuous ray再評価、`tools/fog_shadow_outlier_convergence_rd.py`で上位点32 raysのcenter/width誤差・step sweepを6,400結果測定、`tools/fog_scattering_sampling_image_rd.py`で96×54 linear RGB F4 fixture・region metric・visual difference capture、`tools/fog_sun_transmittance_rd.py`で太陽高度・camera height別の解析的sun-path transmittance比較、`tools/fog_height_image_fixture_rd.py`でF0〜F4のlinear float fixture・GPU式mirror・RGB8 CPU式mirror、`tools/fog_height_camera_scene_rd.py`でcamera y=2／8／20 mのscene再描画とdensity/falloff 3×3 sweep、`tools/fog_alpha_compositing_rd.py`でalpha semantics比較、`tools/fog_depth_encoding_rd.py`でlinear Euclidean／linear view-z／仮定上のforward-Z device-depth解釈比較を実施済み。次は実際の製品camera/depth transform、alpha-bearing runtime境界、GPU capture経路を独立評価する。既存`AtmosphereFogRenderer`とのコード実行比較はCPU moduleの入力境界（RGB8、alphaなし、normalized depth）と画像fixtureの契約を踏まえて別途行う。`applyToPixel()`のdepth-only approximationをheight-aware oracleとして扱わない。

### Phase 2 — GPU height fog比較（source-only）

`heightFogCS.hlsl`のsource equation mirrorは実施済み。2026-11-12の静的host検索でも当該shaderのcompile/PSO/dispatch参照は確認できず、Composition preview depthのsample view・depth encoding・camera matrixもshaderへの公開契約としては特定できなかった。次にGPU capture可能環境で対象resourceとmatrix規約を確認し、既存capture経路の有無を判定する。F1/F2/F3を同一入力で比較し、camera-height近似とworld-position再構成の差、depth精度、HDR/alpha維持を記録する。適切なhost matrixまたはdepthが得られなければ、render passを新規接続せず必要契約を文書化して停止する。

2026-11-13の実行環境監査では、`dxc` / `glslangValidator` / `ShaderMake`のstandalone commandはPATH上にない。Artifact/ArtifactCoreのsource検索でも`heightFogCS.hlsl`のproduct compile/PSO/dispatch callerは見つからなかった。一方`ArtifactCore::GpuContext::CompileShader()`はDiligent runtime APIとして存在するが、初期化済み`IRenderDevice`が必要でshader単体compile CLIの代わりにはならない。したがってshader実行・GPU時間計測には既存GPU hostか専用harnessが必要であり、現在は後者もない。リポジトリ指示により子repo編集とbuild/CMake/test実行はユーザーの明示指示なしに行わないため、この区間はCPU式mirrorまでを確認範囲とする。GPU計測へ進む場合の作業単位は、既存render hostを利用できるかの確認、必要なら子repo内のR&D専用harness追加、明示許可後のconfigure/build/dispatch/captureである。本番render passやUIへの接続はこの計画から自動的に含めない。

### Phase 3 — 散乱・体積方式のfeasibility（CPU single-scattering/F5 proxyあり）

F4ではsingle scatteringと位相関数を追加し、影なし／単純なshadow visibilityの範囲を比較する。局所volume・ライト影・時間安定性が必要と確認された場合に限ってF5のfroxel grid/reprojectionを小さな独立fixtureで評価する。F6の大気LUTはplanetary sky用途が明確になった場合に独立評価し、height fogの代替扱いをしない。

### Phase 4 — 判断と実装提案

比較結果から、Composition/postprocess、3D viewport、cloud/volumeごとの推奨候補、共有できる媒質/積分数学、別々に保つべき統合責務、GPU資源予算をまとめる。本番pass、設定UI、保存形式、既定動作を変える提案は個別に切り出す。

## 参照資料

- `docs/worklog/FOG_PROGRESS_2026-07-20.md` — 既存height fog試作と未対応事項。
- `docs/REFERENCE_REPOSITORIES.md` — Wronski/Frostbite froxel資料、各種大気散乱参照先の既存棚卸し。
- `docs/technical/HOT_PATH_RULES.md` — render hot pathのallocation、resource再生成、診断規則。
- [Precomputed Atmospheric Scattering (Bruneton & Neyret, Eurographics 2008)](https://onlinelibrary.wiley.com/doi/abs/10.1111/j.1467-8659.2008.01245.x) — spherical atmosphere、Rayleigh/Mie、multiple scatteringのprecompute方式。今回のCPU参照で使う地球大気スケールと位相関数の初期条件も参照。
- [A Scalable and Production Ready Sky and Atmosphere Rendering Technique (Hillaire, Eurographics 2020)](https://sebh.github.io/publications/egsr2020.pdf) — transmittance、sky-view、aerial-perspective、multiple-scattering LUTを分ける方式と、single-scattering shadowingのコスト境界。
- [Volumetric Fog: Unified Compute Shader Based Solution to Atmospheric Scattering (Wronski, SIGGRAPH 2014)](https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf) — view-frustum volumeへfog density／in-scattering／lightを蓄積し、forward/deferred双方から適用する参考。
- `ArtifactCore/include/Render/AtmosphereFog.ixx` / `ArtifactCore/src/Render/AtmosphereFog.cpp` — CPU参照実装。
- `Artifact/shaders/heightFogCS.hlsl` / `Artifact/shaders/fogHF.hlsli` / `Artifact/shaders/volumetricCloud_renderCS.hlsl` — GPU shader候補とcloud/light内の既存fog利用。
- `tools/fog_height_integral_rd.py` / `tools/fog_height_image_fixture_rd.py` — CPU height-fog数値・画像比較。
- `tools/fog_height_camera_scene_rd.py` — camera heightとworld-space object heightを再投影するheight-fog比較。
- `tools/fog_sun_transmittance_rd.py` — local single scatteringへのanalytic sun-path transmittance寄与比較。
- `tools/fog_shader_integral_rd.py` — cloud/light fog shaderのheight integralを独立quadrature・start-distance policyと比較。
- `tools/fog_alpha_compositing_rd.py` — coverage-preserving / medium-alpha fog合成のRGBA semantics fixture。
- `tools/fog_depth_encoding_rd.py` / `docs/worklog/artifacts/fog-depth-encoding-rd-2026-11-12/manifest.json` — depth encodingの仮定別に距離・透過率decode感度を測るCPU diagnostic。
- `tools/fog_height_convergence_sweep_rd.py` / `docs/worklog/artifacts/fog-height-convergence-rd-2026-11-12/manifest.json` / `raw_cases.csv` — exponential height integrationのground-crossing条件・密度・falloff別収束sweep。
- `tools/fog_height_sampling_distribution_rd.py` / `docs/worklog/artifacts/fog-height-sampling-distribution-rd-2026-11-12/manifest.json` / `raw_cases.csv` — `rho^(1/3)` nonuniform sample placementとuniform midpointの消散積分比較。
- `tools/fog_scattering_sampling_distribution_rd.py` / `docs/worklog/artifacts/fog-scattering-sampling-rd-2026-11-12/manifest.json` / `raw_cases.csv` — 同じsample配置則をlocal single-scattering積分で追試。
- `tools/fog_scattering_sampling_image_rd.py` / `docs/worklog/artifacts/fog-scattering-sampling-image-rd-2026-11-12/manifest.json` — single-scattering候補のlinear RGB full image、region統計、crop、差分capture。
- `tools/fog_shadow_sampling_rd.py` / `docs/worklog/artifacts/fog-shadow-sampling-rd-2026-11-12/manifest.json` / `raw_cases.csv` — binary shadow interval境界がsingle-scattering sample配置へ与える感度。
- `tools/fog_shadow_outlier_convergence_rd.py` / `docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/outlier_convergence_manifest.json` — top temporal outlier raysに対するboundary error × step convergence結果。
- `tools/fog_shadow_temporal_outlier_rd.py` / `docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/temporal_outlier_manifest.json` — top temporal residual pointをcontinuous ray reference/candidateで再評価。
- `tools/fog_shadow_reprojection_rd.py` / `docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/world_reprojection_manifest.json` — 既知world pointをframe間再投影したcandidate-reference時間残差統計。
- `tools/fog_shadow_population_convergence_rd.py` / `population_convergence_manifest.json` / `population_convergence_sweep.csv` — 全visible shadow rayのcenter/width推定誤差とsample数の階層sweep。
- `tools/fog_shadow_population_spatial_rd.py` / `population_spatial_manifest.json` — 全ray populationのscreen-space relative error field、fixed-scale PFM/PNG map、frame/region/top-coordinate集計。
- `tools/fog_shadow_population_temporal_rd.py` / `population_temporal_manifest.json` / `population_temporal_direct_ray.csv` — shadowed world pointを隣接frameへ再投影しcontinuous rayだけで評価したtemporal residual。
- `tools/fog_shadow_population_boundary_face_rd.py` / `population_boundary_face_manifest.json` / `population_boundary_face_audit.csv` — temporal outlierのAABB active entry/exit faceとinterval変化のpost-hoc記述監査。
- `tools/fog_shadow_box_extent_temporal_rd.py` / `box_extent_temporal_manifest.json` / `box_extent_temporal_sweep.csv` — AABB X half-extent 0.75/1/1.25倍でのdirect-ray temporal residual sensitivity。
- `tools/fog_shadow_box_extent_common_cohort_rd.py` / `box_extent_common_cohort_manifest.json` / `box_extent_common_cohort.csv` — box寸法ごとに異なるvalid pairを揃えるpost-hoc common-support集計。
- `tools/fog_shadow_camera_motion_temporal_rd.py` / `camera_motion_temporal_manifest.json` / `camera_motion_temporal_sweep.csv` — AABB寸法固定でcamera translation振幅を変えたdirect-ray temporal residual sweep。
- `tools/fog_shadow_quadrature_factor_rd.py` / `quadrature_factor_manifest.json` / `quadrature_factor_temporal.csv` — true/estimated profile segmentationとimportance weightsの2×2 temporal factorial、prior sweepとのexact reconciliation。
- `tools/fog_shadow_quadrature_factor_effects_rd.py` / `quadrature_factor_effects_manifest.json` / `quadrature_factor_effects.csv` — factorial residual vectorのsegmentation/weight main effect・interactionのpost-hoc代数分解。
- `tools/fog_shadow_quadrature_width_factor_rd.py` / `quadrature_width_factor_manifest.json` / `quadrature_width_factor_temporal.csv` — core幅±2%誤差のsegmentation/allocation 2×2 temporal factorial。
- `tools/fog_shadow_quadrature_width_factor_effects_rd.py` / `quadrature_width_factor_effects_manifest.json` / `quadrature_width_factor_effects.csv` — width factorial residual vectorのsegmentation/weight main effect・interactionのpost-hoc代数分解。
- `tools/fog_shadow_width_outlier_factor_case_rd.py` / `width_outlier_factor_case_manifest.json` / `width_outlier_factor_case.csv` / `width_outlier_factor_case_temporal.csv` — pair 3 selected outlierのactive-face幾何と幅2×2 factor step convergence case study。
- `tools/fog_shadow_width_outlier_factor_cohort_rd.py` / `width_outlier_factor_cohort_manifest.json` / `width_outlier_factor_cohort.csv` / `width_outlier_factor_cohort_temporal.csv` — 16 selected temporal pairs（7 unique points）のwidth factor step convergence follow-up。
- `tools/fog_shadow_width_outlier_dependence_rd.py` / `width_outlier_dependence_manifest.json` / `width_outlier_dependence.csv` — selected width-factor temporal rowsのoccurrence／unique-pair／unique-world-point集約感度。
- `tools/fog_shadow_width_population_analysis_rd.py` / `quadrature_width_population_analysis_manifest.json` / `quadrature_width_population_analysis.csv` — 全440 source pointsのwidth effect point-max distribution・region層別集計・selected outlier point ranks。
- `tools/fog_shadow_width_geometry_sensitivity_rd.py` / `quadrature_width_geometry_sensitivity_manifest.json` / `quadrature_width_geometry_sensitivity.csv` — 全440 source points・3 camera amplitudeでの区間幾何とwidth ±2% temporal sensitivityの四分位／active-face post-hoc層別。
- `tools/fog_shadow_width_geometry_association_rd.py` / `quadrature_width_geometry_association_manifest.json` / `quadrature_width_geometry_association.csv` — 連続entry/exit/width変位とresidualのSpearman順位相関、point-max感度集約、upper-tail overlap。
- `tools/fog_shadow_width_geometry_cluster_bootstrap_rd.py` / `quadrature_width_geometry_cluster_bootstrap_manifest.json` / `quadrature_width_geometry_cluster_bootstrap.csv` — 6条件におけるpoint-cluster bootstrap 2,000回の順位相関安定性と95% percentile interval。
- `tools/fog_shadow_box_aspect_width_rd.py` / `tools/fog_shadow_box_aspect_summary_rd.py` / `box_aspect_width_manifest.json` / `box_aspect_width_temporal.csv` — 同一volume・centerの3 AABB aspect ratio、3 camera amplitudeでのwidth +2% residual・geometry感度とshape共通support比較。summary toolはdurable rawからmanifestを再構築する。
- `tools/fog_shadow_box_aspect_geometry_analysis_rd.py` / `box_aspect_geometry_analysis_manifest.json` / `box_aspect_geometry_analysis.csv` — aspect-ratio rawからtrue interval変位、1%-penumbra比、width +2% profile-knot displacementとresidualのshape/common-support層別解析。
- `tools/fog_shadow_box_aspect_factor_rd.py` / `box_aspect_factor_manifest.json` / `box_aspect_factor_temporal.csv` — 3 aspect ratio・3 camera amplitudeにおけるwidth +2% segmentation/importance-weight 2×2 factor分解とRGB interaction。
- `tools/fog_shadow_box_aspect_factor_reconcile_rd.py` / `box_aspect_factor_baseline_reconciliation.json` — 新factor sweepのbaseline true/true角と先行width-factor rawの全7,524 temporal vector・reference検証差の再現照合。
- `tools/fog_shadow_box_aspect_factor_sign_rd.py` / `tools/fog_shadow_box_aspect_factor_sign_summary_rd.py` / `box_aspect_factor_sign_manifest.json` / `box_aspect_factor_sign_temporal.csv` — width ±2%のshape別2×2 factor rawとRGB even/odd・sign差・修正済みpair-point common-support統計。
- `tools/fog_shadow_camera_sequence_rd.py` / `docs/worklog/artifacts/fog-shadow-camera-sequence-rd-2026-11-12/manifest.json` — 固定world-space shadow volumeに対するcamera translation中のper-frame誤差・時間残差。
- `tools/fog_shadow_image_rd.py` / `docs/worklog/artifacts/fog-shadow-image-rd-2026-11-12/manifest.json` — shadow位置・幅推定誤差のscreen-space region別linear RGB差分とcapture。
- `tools/fog_shadow_width_signed_image_rd.py` / `docs/worklog/artifacts/fog-shadow-width-signed-image-rd-2026-11-12/manifest.json` — center exact・core width ±2%の96×54 linear RGB image-space比較、edge regions、signed vector分解、cropと差分capture。
- `tools/fog_shadow_box_aspect_image_rd.py` / `docs/worklog/artifacts/fog-shadow-box-aspect-image-rd-2026-11-12/manifest.json` — 同体積3 AABBのtrue slab-intersection silhouette、shape別reference/candidate画像、region統計と差分capture。
- `tools/fog_shadow_box_aspect_camera_sequence_rd.py` / `docs/worklog/artifacts/fog-shadow-box-aspect-camera-sequence-rd-2026-11-12/manifest.json` — 3等体積AABBを7-frame camera pathで追い、silhouette mask motion、reference image motion、quadrature residual deltaを分離。
- `tools/fog_shadow_combined_error_rd.py` / `docs/worklog/artifacts/fog-shadow-combined-error-rd-2026-11-12/manifest.json` / `raw_cases.csv` — shadow中心位置・core幅誤差を同時に摂動したpenumbra quadrature比較。
- `tools/fog_shadow_penumbra_rd.py` / `docs/worklog/artifacts/fog-shadow-penumbra-rd-2026-11-12/manifest.json` / `raw_cases.csv` — cubic smoothstep penumbra幅と遮蔽core幅の推定誤差がsample quadratureへ与える影響。
- `tools/fog_shadow_boundary_sensitivity_rd.py` / `docs/worklog/artifacts/fog-shadow-boundary-sensitivity-rd-2026-11-12/manifest.json` / `raw_cases.csv` — 推定shadow境界をray長の0.1〜2%ずらしたときのsingle-scattering積分誤差。
- `tools/planetary_atmosphere_scattering_rd.py` — 球状惑星Rayleigh/Mie single-scattering比較、sun elevation sweep。
- `tools/planetary_scattering_sampling_robustness_rd.py` / `tools/planetary_scattering_sampling_convergence_rd.py` — 太陽高度・camera altitudeをまたぐsampling候補robustnessとselected-ray収束確認。
- `tools/fog_local_froxel_proxy_rd.py` — Gaussian local density、3 lights、AABB shadowを使うfroxel tile/depth CPU image proxyとoperation/memory算術。
- `tools/fog_local_froxel_light_count_rd.py` — 同じsynthetic sceneで1/2/3 point-light prefixの画質、shadow-mask閾値感度、light-evaluation算術を比較するCPU-only driver。
- `tools/fog_local_froxel_depth_distribution_rd.py` — tile4/3-lightの局所fog fixtureでlog/linear view-Z bin配置とslice数を比較し、奥行き帯別誤差・bin幅・費用算術を記録するCPU-only driver。
- `tools/fog_local_froxel_temporal_rd.py` — 静的局所fog sceneのcamera translation sequenceにおけるgrid再構築／初期grid流用のframe誤差とcandidate-reference residual driftを比較するCPU-only driver。
- `tools/fog_local_froxel_spatial_resolution_rd.py` — XY tile幅とGaussian volumeのscale/position stressを比較し、全画像とfog-only誤差およびcell/memory算術を記録するCPU-only driver。
- `tools/fog_local_froxel_hard_boundary_rd.py` — binary box densityでhard volume境界のtile/depth resolutionと配置感度を比較するCPU-only driver。
- `tools/fog_local_froxel_hard_boundary_oracle_rd.py` — ray-box slab intervalでhard-boundary散乱referenceを構築し、uniform raymarch収束とfroxel誤差を監査するCPU-only driver。
- `tools/fog_local_froxel_box_coverage_rd.py` — projected AABB screen-rectangle × view-Z overlap率によるfroxel density近似をray-interval oracleと比較するCPU-only negative-result probe。
- `tools/fog_local_froxel_ray_box_coverage_rd.py` — stratified XY raysと厳密ray/AABB区間からslice occupancyを作るCPU-only probe。
- `tools/fog_local_froxel_multi_box_coverage_rd.py` — additive densityの2-box重複/分離条件でcenter-sampleとray-box slice coverageを比較するCPU-only probe。
- `tools/fog_local_froxel_injection_sampling_rd.py` — hard box volumeに対するfroxel内XY injection 1×1/2×2/4×4平均の画質と評価数を比較するCPU-only driver。
- `tools/fog_local_froxel_integration_convergence_rd.py` — grid構築を固定し、log-depth slice内ray integration substep数を増やしてrepresentation/integration誤差寄与を診断するCPU-only driver。
- `docs/worklog/artifacts/fog-atmosphere-rd-2026-11-12/manifest.json` / `docs/worklog/artifacts/planetary-atmosphere-rd-2026-11-12/manifest.json` — capture契約、hash、誤差／輝度summary。

## 実行制約・状態

- この作業ではサブモジュールを編集せず、ビルド、CMake、テスト、GPU captureを実行しない。
- CPU fixtureの生成・解析は独立Python toolで継続する。製品host dispatchやGPU captureに必要なビルド／テスト／CMake実行は、AGENTS.mdの実行制限に従って別途明示指示を受けてから行う。
- **現状態:** Phase 0のsource auditと仮定別depth encoding感度診断、Phase 1の事前gate付きCPU積分収束・sample配置・shadow境界stress sweep、single-scattering quadrature比較、Fog画像fixture・カメラ高別scene proxy・alpha数式fixtureと既存CPU/GPU式mirror、Phase 3の球状大気single-scattering参照・太陽高度／惑星カメラ高度sweep、全visible shadow-ray誤差×sample sweep、spatial error field、shadowed world point direct-ray temporal residualとactive-face post-hoc診断、AABB extent sensitivity/common-support、camera translation amplitude、center offsetおよびcore幅のquadrature segmentation/allocation factor decompositionを実施。製品host接続、GPU runtime／performance、multiple scattering／LUT品質、実sceneのdepth契約、alpha convention、実素材での視覚評価は未検証。
