# Fog and Atmospheric Scattering R&D

**最終更新:** 2026-11-12
**ステータス:** In Progress（source audit、CPU積分／画像fixture、GPU式mirror診断、球状大気single-scattering参照を作成。GPU runtime capture・採用判断は未実施）

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

## 決定論的Fog画像fixture（2026-11-12）

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

alpha合成契約の感度確認用に`tools/fog_alpha_compositing_rd.py`を追加し、straight RGBA入力をpremultipliedへ変換した決定論的21ケースを評価した。coverage alphaを保ちfogを被覆領域内へ限定する方式と、媒質の寄与をalphaにも含める方式は別のlayer semanticsで、同じ画像結果を意味しない。後者はalpha=0でもfog寄与を持つため、空の透明pixelを維持する処理には適さない。さらに、premultiplied RGBへfogを加えながら元alphaを保つnaive式では、transmittance 0.1時にalpha=0 pixelへ最大RGB `0.648`が残り、premultiplied合成時に透明pixelからfog色が漏れる。straight-alpha inputならRGB blend＋alpha保持を後段で正しくpremultiplyする経路が成立し得るため、GPU shaderのfloat4 input/outputだけではalpha conventionを断定できない。CPU `ImageBuffer`はRGB8でalphaなし、GPU shaderのhost bindingも確認できておらず、runtimeの正規契約や採用policyは未決定である。case表とblack/white/magenta背景へのpremultiplied composite値は`docs/worklog/artifacts/fog-alpha-compositing-rd-2026-11-12/manifest.json`に保存した。

## 球状大気 single-scattering CPU参照（2026-11-12）

`tools/planetary_atmosphere_scattering_rd.py` は球状惑星と大気shellのray/sphere交差を解き、view rayおよび太陽方向の光学深度、Rayleigh/Mie散乱、Rayleigh位相関数、Cornette–Shanks Mie位相関数を数値積分する。パラメータの基準はBruneton–Neyretの地球大気モデル（Rayleigh scale height 8 km、波長別Rayleigh係数、Mie scale height約1.2 km、g=0.76の代表値）に置く。Mie scattering/extinction比は0.9に設定した。同論文はこれらの大気係数・aerosol値の組み合わせでCIE clear-sky modelの測定fitを示すが、本fixtureはarbitrary sun radiance、単一散乱のみ、地面反射なし、狭いview setであり、その測定fitや論文方式を再現したものではない。係数の初期値を借りた式・収束のCPU参照として扱う。

球面上の位相関数積分diagnosticはcosθ区間を262,144分割し、Rayleigh `0.9999999991`、Cornette–Shanks Mie(g=0.76) `0.99999999825`（理想積分値1）となった。g sweepのg=0.9は`0.99999993334`である。以前の16,384分割ではg=0.9が`0.99998294`に留まったため、異方性が強いphaseでは角度積分の刻みも検算条件として保存する。96×54 capture、太陽高度18°・カメラforwardからの方位25°では、128×48積分を画像referenceとして32×16のRayleigh+Mie(g=0.76)がlinear RGB RMSE `0.02487`、64×24が`0.006648`。32×16でg=0（等方）にするとRMSE `0.3264`、Mie scatteringを切ると`0.4142`となり、太陽方向を視野内に入れるとMie位相差が強く表れた。前回の方位90°設定は太陽がカメラ視野外にあったため、前回版のMie差分は位相lookの評価値として採用しない。

異方性をg=0／0.5／0.76／0.9で比較するcaptureも追加した。同解像度32×16のg=0.76画像を基準にしたRMSEは、g=0で`0.3230`、g=0.5で`0.1385`、g=0.9で`0.2472`。この視野ではg=0.76がg=0.5よりg=0.9に対して近く見えるが、g=0.9はforward peakがより狭く、view/sun積分解像度32×16自体の誤差が混ざる。数値だけでg=0.76を最適値と結論しない。`mie_anisotropy_comparison_to_g076_32x16`は同一解像度同士、既存`linear_rgb_comparison_to_rayleigh_mie_g076_128x48`は128×48 referenceとの比較としてmanifest内で区別した。

太陽方向付近を含む6本のrayを個別に高密度化し、512×192を参照にした。最も厳しいsunward rayでの最大相対RGB誤差は、16×8で`38.1%`、32×16で`13.3%`、64×24で`3.49%`、128×48で`0.836%`、256×96で`0.167%`。128×48は全画素画像の収束を保証するものではないが、forward-scattering近傍では同設定の32×16画像比較より高いstep数が必要になる証拠となった。

view積分と太陽方向transmittance積分の誤差源を分けるため、地平線中心rayとsunward-upper rayで片方のstep数を固定した独立sweepを追加した。view側はsun steps=192固定・512×192結果基準、sun側はview steps=128固定・128×256結果基準で、最大相対RGB誤差を比較した。地平線rayのview steps 32／64／128／256では`5.87 / 1.48 / 0.354 / 0.071%`、sun steps 8／16／32／64／128では`7.78 / 2.66 / 0.790 / 0.200 / 0.041%`。sunward-upper rayではview側が`13.64 / 3.73 / 0.913 / 0.184%`、sun側が`3.60 / 1.05 / 0.284 / 0.070 / 0.014%`だった。今回の固定条件では地平線rayはsun-path積分、forward散乱近傍はview積分がより強い制約となる。これは選んだ2 rayのCPU数値収束であり、全画素の誤差上限やGPU時間を示さない。

view方向の同一sample budgetを保った診断候補として、放射方向外向きrayは`t=L*u²`、惑星中心へ向かうrayは`t=L*(1-(1-u)²)`へ寄せ、接線付近では`smoothstep(abs(radial_direction_y)/0.1)`でlinear mappingと滑らかにblendし、各u-cellのworld距離幅を積分weightとする`altitude_biased`配置を試した。uniform 32×16に対し、参照をuniform 512×192に固定した32-step ray誤差はzenithで`6.191%→0.269%`、sunward-upperで`13.642%→0.497%`へ低下した。center horizonはuniform `5.868%`に対してbiased `6.375%`、groundwardは`0.006%→0.016%`と小さく悪化した。全96×54画像ではbiased 32×16のRMSE `0.01204`、uniform 32×16は`0.02487`で、約52%低下した（双方ともuniform 128×48画像参照）。初期のhard angle thresholdは小さなpitchでsample配置が切り替わり、horizon proxyの最大相対誤差を一時約25%まで跳ねさせた。smoothstep blend後は最大`6.59%`となり不連続は解消したが、uniformの`4.76%`より残差は大きく、接線付近での安定性はなお弱い。

同じrayをcamera-right軸回りに−1°から+1°へpitchさせる5方向のcamera-motion proxyで、各方向のuniform 512×192を基準にした。sunward-upperではuniform 32×16の最大frame相対誤差`13.47%`・連続frame差分RMSE `0.001313`に対し、biasedは`1.24%`・`0.000203`。center horizonではuniformが`4.76%`・`0.03189`、biasedが`6.59%`・`0.04002`で、biasedはわずかに悪化した。したがってsunward rayのsample収束改善はcamera angular sweepでも残る一方、horizon付近はuniformが優位である。このray rotationは静的な大気場を通るcamera pitchの数値proxyで、rendered sequence、reprojection、実GPU temporal stability試験ではない。別カメラ高度、gの高い鋭いpeak、float/GPU実装での品質やper-sample mapping演算の実時間コストは未検証。Capture名は`rayleigh_mie_g076_altitude_biased_32x16`。

太陽高度を0°／5°／18°／45°／75°へ掃引した同じCPU sky fixtureでは、Rec.709重みを使ったtone mapping前の平均linear luminance proxyは`0.1136 / 0.4214 / 0.4957 / 0.3481 / 0.2629`。カメラ高度0.5／2／10／50 kmではそれぞれ`0.5778 / 0.4957 / 0.3929 / 0.2386`。低い太陽ほど長い斜め大気経路となり赤寄りの散乱が残り、高度上昇では光学深度が減って散乱radianceが低下する傾向がこの固定view条件で見えた。RGB primaries、solar radiance、sun disk、surface illuminationを較正していないため、Rec.709 weighted metricは物理輝度ではなくパラメータ応答を見るproxyである。

Captureとmanifestは `docs/worklog/artifacts/planetary-atmosphere-rd-2026-11-12/` にある。PFMはlinear radiance、PNGは同じReinhard+gammaのpreview。manifestは位相関数球面積分、6 rayのstep convergence、sun-elevation／camera-altitude radiance summaryを保持する。現状は黒い惑星surface radiance、sun disk、雲、地形shadow、多重散乱、spectral integrationを含まないため、主にsky angular responseとstep convergenceを比較する。

### R&D通過条件

- F1は解析透過率参照と数値誤差が一致し、near/far・start distance・無効depthの境界条件が明文化される。
- F2はheight/ray directionを変えた全fixtureで参照積分に対する誤差上限を設定し、F3はstep数を増やした際の収束を示す。具体的な許容値はcapture前に解像度・precisionを基に確定し、結果を見てから都合よく変更しない。
- 高さを変えた物体と視線でcamera-height定数近似の限界を可視化し、F2/F3で改善するケースを特定する。
- F4〜F6は追加コストと視覚上の改善を同一条件で示す。改善を定量・視覚の両方で確認できない候補は採用推奨にしない。
- 完了レポートに候補ごとの品質・時間・memory、backend、未確認項目、用途別推奨を記載する。試作結果を本番統合済みと誤認させない。

## フェーズ

### Phase 0 — 契約監査（静的）

上表のソースを照合し、各rendererのdepth encoding、coordinate space、color/alpha、near/far、camera matrix、現行host bindingを確認する。確認できない項目は推測で補完せず、R&D adapterの入力要件として列挙する。

### Phase 1 — CPU numeric oracleとfixture（数値積分の原型あり）

`tools/fog_height_integral_rd.py`で地面交差を含む5 rayと閉形式透過率のCPU数値比較およびCPU source phase正規化audit、`tools/fog_sun_transmittance_rd.py`で太陽高度・camera height別の解析的sun-path transmittanceを加えたlocal single-scattering比較、`tools/fog_height_image_fixture_rd.py`でF0〜F4のlinear float fixture・GPU式mirror・RGB8 CPU式mirror、`tools/fog_height_camera_scene_rd.py`でcamera y=2／8／20 mのscene再描画とdensity/falloff 3×3 sweepを実施済み。`tools/fog_alpha_compositing_rd.py`ではalpha semanticsの数式比較を行った。次は実際の製品camera/depth transform、alpha-bearing runtime境界、GPU capture経路を独立評価する。既存`AtmosphereFogRenderer`とのコード実行比較はCPU moduleの入力境界（RGB8、alphaなし、normalized depth）と画像fixtureの契約を踏まえて別途行う。`applyToPixel()`のdepth-only approximationをheight-aware oracleとして扱わない。

### Phase 2 — GPU height fog比較（source-only）

`heightFogCS.hlsl`のsource equation mirrorは実施済み。次に実際の入出力契約・dispatch hostを読み、許可された既存capture経路があるか確認してからGPU capture adapterの可否を決める。F1/F2/F3を同一入力で比較し、camera-height近似とworld-position再構成の差、depth精度、HDR/alpha維持を記録する。適切なhost matrixまたはdepthが得られなければ、render passを新規接続せず必要契約を文書化して停止する。

### Phase 3 — 散乱・体積方式のfeasibility（CPU single-scattering参照あり）

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
- `tools/fog_alpha_compositing_rd.py` — coverage-preserving / medium-alpha fog合成のRGBA semantics fixture。
- `tools/planetary_atmosphere_scattering_rd.py` — 球状惑星Rayleigh/Mie single-scattering比較、sun elevation sweep。
- `docs/worklog/artifacts/fog-atmosphere-rd-2026-11-12/manifest.json` / `docs/worklog/artifacts/planetary-atmosphere-rd-2026-11-12/manifest.json` — capture契約、hash、誤差／輝度summary。

## 実行制約・状態

- この作業ではサブモジュールを編集せず、ビルド、CMake、テスト、GPU captureを実行しない。
- CPU fixtureの生成・解析は独立Python toolで継続する。製品host dispatchやGPU captureに必要なビルド／テスト／CMake実行は、AGENTS.mdの実行制限に従って別途明示指示を受けてから行う。
- **現状態:** Phase 0のsource audit、Phase 1のCPU積分・Fog画像fixture・カメラ高別scene proxy・alpha数式fixtureと既存CPU/GPU式mirror、Phase 3の球状大気single-scattering参照・太陽高度／惑星カメラ高度sweepを実施。製品host接続、GPU runtime／performance、multiple scattering／LUT品質、実sceneのdepth契約、alpha convention、実素材での視覚評価は未検証。
