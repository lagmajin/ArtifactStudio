# Image Light Effects R&D Notes

**最終更新:** 2026-11-10

## Scope and status

This note records the independent image captures and code-path findings for Glow and still-image Light Leak. The experiments are Python/OpenCV SDR references. They do not establish production GPU performance or pixel parity. ArtifactCore has analytic CPU still-image Light Leak fields for edge wash, anamorphic streak, and organic lobes, and Artifact has a corresponding resident-filter effect with normalized streak source X/Y and angle controls. The `ArtifactCore` and `ArtifactEffectsGlow` targets compiled successfully on 2026-11-10, and the resident HLSL body compiled as both D3D12 DXIL and Vulkan 1.1 SPIR-V. A full `ArtifactUiTest` build using MSVC 14.52 completed successfully in `out/build/x64-Debug-Agent`; D3D12 is disabled in that configuration and Vulkan is enabled. `capture_ui_test.py` produced a valid Japanese startup UI capture at `captures/light-leak-app-ja-rd.png`. The English startup capture was a flat gray image and is invalid. No dedicated Light Leak application fixture exists, so the successful UI capture does not show the effect. Actual GPU dispatch and CPU/GPU pixel parity remain unverified.

## Glow: soft-knee multiscale look

`glow_lab.py` compares the current fixed-threshold stacked-Gaussian approximation with a linear-light soft-knee candidate. On the deterministic neon plate, mean encoded RGB was 0.03217 source, 0.03435 current approximation, and 0.05571 candidate. Means are descriptive only; the candidate preserves colored sources while adding near and broad halo.

`glow_pyramid_lab.py` compares four full-resolution Gaussian scales against a full-resolution near scale plus 1/2, 1/4, and 1/8 downsample/blur/upsample planes. At 960x540 the linear bloom-field RMSE was 0.0002605, relative energy error 0.0498%, and maximum field delta 0.003405. At 641x359 and 320x180 RMSE was 0.0001388 and 0.0005769. Captures are under `captures/glow-pyramid/`.

These are CPU/OpenCV image comparisons, not GPU measurements. GPU parity, resource reuse, HDR, high-resolution memory, GPU odd-dimension dispatch, border behavior, and alpha expansion remain unverified.

## Light Leak: shape and scale

`light_leak_lab.py` compares normalized-coordinate edge wash, anamorphic streak, and organic gate leak. It reports linear-light leak deltas in upper-left, center, and lower-right regions at 960x540, 641x359, and 320x180. In the generated dusk fixture, edge-wash region values remained approximately 0.199–0.200 upper-left, 0.006 center, and 0.0147 lower-right across the three sizes. The large-scale geometry was therefore stable in this limited resampling study.

The same models were applied to frame 20 of the CC0 walkway footage recorded in `tests/ArtifactCore/fixtures/README.md`. At shared strength 1.0 the bright ice scene showed strong edge color; 0.55 was a more restrained comparison. Source mean RGB was 0.59232; strength 0.55 output means were edge wash 0.61866, streak 0.60407, and organic 0.61787. Captures are in `captures/light-leak-cc0-frame20-soft/` and `captures/light-leak-still-effect-reference/`. These are look references, not recommended universal settings.

An additional strength sweep captured the same frame at 0.30 and 0.80. At 0.30, edge-wash linear-light delta in the normalized upper-left region was 0.0450 and organic lower-right delta was 0.0213; at 0.80 these were 0.1081 and 0.0537. The 960x540, 641x359, and 320x180 measurements stayed close for each strength, so normalized placement is stable in this resampling sample. Visually, edge wash colors a broad part of the bright sky, the streak reads as a horizontal sky band, and organic keeps more localized lower-edge patches. At 0.80, color wash was already conspicuous on snow/sky; 0.30 remained more restrained. This comparison does not select one universal default. Captures are in `captures/light-leak-cc0-frame20-sweep-030/` and `captures/light-leak-cc0-frame20-sweep-080/`.

GPU parity, temporal continuity, HDR, color-managed input, broader aspect ratios, and automatic source placement remain unverified.

## Light Leak: anamorphic streak angle

`light_leak_lab.py` now sweeps the anamorphic beam axis from -12° to +12° on a winter bridge frame and a synthetic neon plate with the same source coordinates and strength. The winter capture makes the beam direction follow the bridge/horizon at +12°, while the neon plate shows an unmistakable diagonal flare. The existing approximately 2.578° default remains close to horizontal. On the winter frame, the center-region linear-light delta changed from 0.03800 at the default to 0.03853 at +12°, showing mainly a redistribution of the streak rather than a large energy increase; on neon it changed from 0.06384 to 0.06582. Default-angle Python output was byte-identical to the prior capture (max channel delta 0 across 921,600 channel samples).

The selected control is `Streak Angle` in degrees, clamped to [-45°, +45°]. The CPU implementation computes sine/cosine once before the image loop; the resident filter reads P5 and uses the same degree range. MSVC 14.52 `ArtifactEffectsGlow` build and DXIL/Vulkan 1.1 shader compilation passed after this change. The full app target subsequently built. Current app startup capture is available, but no Light Leak scene fixture has been added; runtime effect rendering and CPU/GPU parity remain unverified.

## Straight alpha and linear-premultiplied contract

`surface_alpha_contract.py` provides independent SDR reference conversions from straight sRGB RGBA to linear-premultiplied RGBA and compositing over an sRGB checkerboard. Glow and Light Leak alpha fixtures use straight-alpha input and zero hidden RGB where alpha is exactly zero. Both compare source-alpha preservation, coverage clipping, and effect-energy alpha expansion. Their `*-premult-contract.png` captures contrast deliberately misreading straight RGB as premultiplied with valid linear-premultiplied output and a six-times amplified visual difference.

The maximum SDR `linear premultiplied RGB <= alpha` violation after valid conversion was 0 for both effects. With the Glow reference updated to extract straight-color highlights but blur premultiplied light energy, misinterpreting straight RGB as premultiplied caused maximum display differences of 0.32848 for Glow and 0.66347 for Light Leak. Preserving source alpha hid the outside-matte halo after compositing; energy-based expansion affected 216,069 fully transparent Glow pixels and 214,792 Light Leak pixels above alpha 0.01.

These values apply to the current generated SDR fixtures. HDR scene-referred values, other primaries, half-float formats, effect masks/mix, and real GPU surfaces remain unverified.

## 2026-11-10 — Cut reset ramp for adaptive flash strength

**仮説:** カット位置でEMAを即時resetするとstrength段差が大きい。前shot状態は破棄しつつ、短い時定数で新shotのtargetへ追従させれば、段差と尾引きを両立して減らせる。

**方法:** `flash_exposure_cut_lab.py` を更新し、CC0動画の冒頭16フレームから低キー生成静止画16フレームへ切り替える既知位置カットを用いて、未平滑、未reset tau 0.75秒、cut時reset後tau 0/0.1/0.25/0.5秒のstrength応答を比較した。同じflash progressを保ち、strength制御だけを変えた。出力は `captures/exposure-cut-rd2`。

**結果:** target strengthは約0.452から2.3へ変化。未reset EMAの切替後初フレーム誤差1.689、切替後16フレーム平均誤差0.942。reset後tau 0.1秒は初回strength段差を1.85から0.90（約51%減）にし、切替後平均target誤差0.122。tau 0.25秒では段差0.43、平均誤差0.372。tau 0.5秒では段差0.23、平均誤差0.713。短いreset ramp群は未reset EMAよりこの評価窓で早く追従した。

**暫定判断:** 既知cut境界で前shot状態を破棄し、tau 0.1秒程度で追従する方式は即時resetと長いEMAの中間を作れる。tau 0.1秒はこの素材・fps・人工カット条件での候補であり、美的に最適な値とは言えない。自動cut検出は引き続き未評価。

**未検証:** 自動cut検出閾値と誤検出率、実写の滑らかな露出変化との識別、複数素材での最適reset ramp、flashの知覚的連続性。

## Production integration findings

## 2026-11-10 — Moving source coordinate-scale correction

**観察:** `flash_specular_lab.py` の旧光方向は、球法線を半径145pxで正規化する一方、画面上の光源差を全画像幅/高さで割っていた。既定の約900px水平移動に対し、光方向が球面上で必要以上に小さく変わる単位不整合だった。

**変更:** 光源の水平/垂直差と高さを球半径と同じpixel-scale座標へ統一し、`--light-height-px` を追加した。既知法線の2.5D panelでは移動光源サンプル、Lambert応答、距離減衰を使い、球面Blinn–Phong specularとは別の寄与として時系列へ含めた。point radius 0とarea radius 180pxの24frame sequenceを `captures/specular-motion-point-rd4` / `captures/specular-motion-area-rd4` に出力した。

**結果:** 座標修正後、flash envelope >0.2の区間における球specular重心の移動pathはpoint 60.9px、area 60.5px（旧モデルはいずれも約38px）。同じ区間でarea sourceはpointに比べて平均specular peakが0.554から0.372へ約33%低下し、可視specular面積率は6.1%から8.0%へ増えた。接触シートでも点光源の狭く明るいハイライトと、面光源の広く低いハイライトを確認した。

**暫定判断:** 光源位置変化が表面法線へ及ぼす応答の座標スケール不整合を修正できた。有限面光源は局所specular peakを抑えて広げる制御として有効な見た目候補。ただしチャート固定法線・Blinn–Phong近似であり、物理校正や実写写真の法線推定ではない。ハイライト重心の移動は光源軌跡と一致せず、視線方向と面法線に依存する。

**未検証:** 複数高さ・光路・roughnessの組合せ、実写に対するdepth/normal推定、遮蔽を含む影との整合、area sample数増加時の収束。

## 2026-11-10 — Traveling front with known-surface illumination

**目的:** soft exposure frontを単独の画面マスクとしてではなく、既知のsurface上のlocal diffuse responseと組み合わせたときの見た目を比較する。実写では幾何を推測せず、radial frontだけを適用する。

**方法:** `flash_surface_front_lab.py` を追加した。合成surface chartに対してflat-depth/radial front、既知パネル/球depthでmodulateしたfront、local Lambert lighting、両者のlinear-light合成を比較する。CC0 walkway動画のframe 24にはradial frontだけを重ねる。capture/metricsは `captures/surface-front-rd7`。

**結果:** strength 0.6で合成chartのmean linear-luma deltaはfront 0.0222、surface light 0.0421、combined 0.0709、新規clip 0%。depth-front strength 0.18/0.36/0.72でflat frontとの差のmean absolute linear-lumaは0.00092/0.00229/0.00428。既知チャートでは全条件で新規clip 0%だった。一方、CC0 snow frameのsource p95 linear-lumaは0.799。strength 0.4でp95 0.992・新規clip channel率9.43%、0.6でp95 0.996・率11.12%。この高輝度素材では小strengthでも大きなheadroomを消費した。

**修正履歴:** 最初のcaptureではchartのlinear RGBをsRGBと誤認して入力したためsurface照明が過大だった。色変換を直し、sRGB RGBAからlinear RGBへ明示変換後にsurface lightingを計算するcaptureへ置き換えた。depth front比較も当初directional modeで呼び出し、実装上depth modulationがradialのみに適用されるため差がゼロだった。chart比較をradialへ改め、flat対既知depthの差を再計測した。

**暫定判断:** 既知geometryではsoft frontとlocal surface responseの加算が可能で、軽いdepth modulationは前線をわずかに変える。ただしこの試験progress/深度rangeでは視覚差が小さい。強いdepth変調はより見えるが、任意写真に適用するには深度推定と遮蔽検証が必要。high-key実写ではこのglobal strength規則が過剰なので、前のheadroom実験と時間応答を結合する必要がある。実写に架空のsurface normalを当てない。

**未検証:** 動画全区間でのfront/clip/temporal continuity、複数の高key実写、real-photo depth estimator品質、動く光源とshadowの時間整合、HDR output。

## 2026-11-10 — Corrected depth-dependent front arrival direction

**確認した問題:** `flash_fill`は既定のwhite=near depth mapを内部で反転し、大きい値を遠距離としていたが、距離補正でその値を減算していたため、遠い面ほどfrontを早く到達させていた。説明上の「farther regions later」と式の符号が逆だった。

**修正・確認:** depth補正を`distance += depth_front * depth_front_strength`に変更した。256×256の均一な板で同じscreen-space半径にある対称2点へnear white / far blackの深度だけを割り当て、flat frontでは両点の差が`-5.2e-7`、depth-front strength .5ではnear pointがfar pointより0.00396明るくなった。既知surface chartとCC0 walkway frameも再レンダーした。更新captureは`captures/depth-front-sign-rd1`と`captures/surface-front-depth-sign-rd1`。後者でstrength .36のknown-depth対flat mean absolute luma差は0.00142、新規clipは0%。

**制約:** これは深度の符号と到達方向を検証しただけで、推定depthの正確さ、遮蔽、実空間の光輸送を検証していない。旧`surface-front-rd*`やdepth-front captureで得た視覚傾向は逆向きの時間差を含むため、新captureを参照する。実写に使うdepth mapは依然として未提供である。

## 2026-11-10 — Foreground-leading fill on the portrait plate

**方法:** `flash-photo-plate.png`からGrabCutで人物の粗いforeground matteを取り、white=nearの人物領域と手作りの背景距離勾配からdepth proxyを作った。強度4.8 / scatter1.5 / coupling1 / veil .0015625 / global fill .015 / source (.08,.5) / peak1の同一lookで、flat frontとdepth-front strength .36をprogress .16/.20/.24/.32/.44/.60/1.0で比較。比較画像は`captures/photo-depth-rd1/strong-fill-flat-vs-depth.jpg`、depth mapとmask、metricsも同じcapture folderに保存。

**結果:** progress .24の人物領域mean linear-luma flash deltaはflat .00239からdepth .00707へ増加し、同時点の背景は.00412から.00420でほぼ維持された。progress .32では人物 .00940→.01121、背景 .00674→.00653。progress .44では人物はほぼ一致し、背景は.01286→.01207と遅れて明るくなる。終端はflat/depth共に人物.03503、背景.03888/.03788。最大の新規clipped-channel率はdepth側で0.0506%。目視でもfront初期に人物が少し先行して照らされ、終端ではflatとの差が収束する。

**暫定判断・制約:** Depth frontは被写体を早く照らし、その奥の背景を遅らせる時間art-directionとして有効なlook候補。ただしGrabCut silhouetteは人物輪郭しか分けず、背景depthは手作り勾配で、人物の顔・衣服・手足の立体、髪の遮蔽、wet streetの正確な距離は復元しない。比較では均一なimage-space veil/PSFのままなので、光が人物の輪郭で遮られる現象は再現しない。次は品質を評価できる実写depth sourceまたは動画の時間安定なdepth mapで、subject segmentationの破綻と光漏れを調べる。

## 2026-11-10 — Moving flash front with cast-shadow cue

**目的・方法:** `flash_shadow_animation_lab.py` を追加し、合成known-surface chartに沿ってflash sourceを左から右へ移動させ、深度modulatedのradial front、局所diffuse照明、翻訳＋Gaussian blurによるcast-shadow cueを7段階で比較した。geometry+shadow結果ではfront deltaとsurface-light deltaを混合し、shadow maskはsource imageではなくflash寄与だけに掛けた。normal lookのstrength 0.8と、見え方を確認する強いprobe設定を `captures/shadow-animation-rd1` / `rd2` に記録した。

**結果:** probe設定（front strength 2.2、surface strength 2.6、shadow opacity .78）では、shadow領域内の平均flash linear-luma deltaは全サンプル平均0.0126、隣接領域は0.0444で、影側は隣接側の約28%だった。平均絶対輝度差はfront-onlyとgeometry+shadow間で0.0215。新規clip率は全時刻で0.08%未満。通常strength .8では差が小さく、接触シート上で影の寄与を読み取りにくかったため、数値を見せるprobe値と初期look値を分けた。

**暫定判断・限界:** 前線だけでなく、既知形状への局所照明と移動する影をflash deltaへ適用する経路は、暗い床面上の影保持を定量的に示せた。一方、出力は合成チャート専用の2.5D画像空間近似であり、現実の遮蔽投影・面法線・inverse-square光量を再構成しない。パネル／球の既知geometryを使い、任意写真へ深度や法線を推測適用していない。低strengthで十分な視覚差が出ない点も残る。次は影softness/高さと前線速度を分けて比較し、光源移動に対する影方向・長さの連続性を評価する。

**前線・影パラメータ追試:** 前線時間（0.45/0.78/1.10秒）、影高さ（55/92/150px）、影ぼけ（8/18/32px）を独立sweepした初回出力は `captures/shadow-parameter-sweep-rd1`。frontのlinear-luma deltaが0.01を超える面積はprogress .52で96.95%/78.04%/54.87%、progress .72で100%/100%/79.99%。短い前線時間は同一progressで広く照らす。初回の影region平均値は高さ変更によってROI内mask位置・面積も変わっていたため、パラメータ比較の結論には使わない。

**固定ROI幾何計測:** `measure_shadow` を追加し、地面ROIをx=130..849、y=604..719に固定。各時点で影面積（opacity >= 0.5）、重心、影長（行profile >= 最大値の50%）、後端10–90%幅を測り、地面ROIのshadow matte接触シートを出力した。再計測は5時点、`captures/shadow-metric-validation-rd1`。固定softness 14pxではheight 16/32/52pxに対し平均影長19/33/53px、影面積7,903/15,606/25,263px、重心y=616.9/622.9/631.9px。固定height 28pxではsoftness 6/14/24pxに対し後端10–90%幅16/35/52px。softnessが広がるにつれ境界幅が増え、threshold面積は13,944/13,678/13,100pxへ少し縮小した。進行中の光源移動を含めても、指定した2.5D変位距離と影長、blur softnessと境界幅の順序関係は一致した。

**制約:** 上の幾何計測は作成したrectangle matteを投影関数で平行移動しGaussian blurした既知チャートだけで検証した。ROIとprofile thresholdの再現性は示すが、現実場面の遮蔽形状・射影・影の物理量を検証しない。現行shadow transformは簡易画像空間モデルで、地面方向や光源高さを3Dカメラ校正から求めない。front timingも同じradial fieldのtime normalizationを変えた比較であり、レンズ露光実測値ではない。

**Headroom follow-up:** frame 24のCC0 snow/bridge画像では、strength 0.1/0.2/0.3/0.4/0.6/0.8/1.2/1.6の新規clip channel率が7.27/7.94/8.65/9.43/11.12/13.07/17.09/21.16%。新規clip target 8%の最高候補は0.2で、p95 linear lumaはsource 0.799から0.987になった。selected resultは `captures/surface-front-rd8/flash-surface-front-cc0-clip-limited.png` に保存した。この選択はprogress 0.56の単一frameだけに対する制約付き候補で、美的な最適値や動画shot全体の固定strengthを証明しない。次は連続frame上でframeごとのheadroom変化、strength EMAの追従、カット区切りを一緒に計測し、pumpingと残留clipを評価する。

## 2026-11-10 — Clip-limited adaptation on continuous footage

**方法:** `flash_exposure_video_adaptation_lab.py` を追加した。CC0 walkway fixtureの連続60frame（15fps、約0–3.93秒）を最大寸法240pxで処理し、progressを0.38から0.70へ進めた。各frameでstrength候補0.05/0.1/0.15/0.2/0.25/0.3/0.4/0.6/0.8を実レンダーし、新規clip channel率8%以下の最大値をtargetにする。framewise選択、最初のframeで決めたshot固定strength、EMA tau 0.1秒/0.25秒を比較。captureとmetricsは `captures/exposure-video-adaptation-rd1`。

**結果:** framewise targetは0.8から最終0.25まで推移し、隣接strength差平均0.0263、最大0.20。clip target超過は0/60、平均新規clip channel率7.43%。最初の候補0.8に固定すると47/60frameでtarget超過、平均clip率9.27%、最大11.996%。EMA tau 0.1秒は隣接strength差平均0.0172/最大0.0973に抑え、clip超過0/60、平均clip率7.48%。tau 0.25秒はさらに0.0113/0.0479へ平滑化した一方、9/60frameでtargetを超え、最大8.19%。平均flash luma deltaはframewise 0.1357、固定0.1524、EMA .1 0.1363、EMA .25 0.1373。

**暫定判断:** この4秒素材ではshot先頭のheadroom値を固定する方法は後半に過露出となった。EMA .1秒はframewise制御の光量に近く、strength変動を下げながら上限内を保った。EMA .25秒はより滑らかだが上限を一部超え、clip制約を守るには非対称追従（strengthを下げる側を速くする）または安全余白が必要になる可能性がある。候補グリッドと低解像度SDRによる結果であり、実時間・美的優位の確証ではない。

**制限:** 1つの連続snow/bridge shotのみでcut resetなし。隣接flash luma差はカメラ/物体運動を補償していない。4秒の結果から汎用shot固定値や自動exposureを推奨しない。

**Control refinement:** coarse strength gridを5回の二分探索で補間し、framewise clip limitの境界強度を推定する比較を追加した。通常EMAは目標clip境界へ遅れて追従し、tau 0.1秒でも35/60frame、tau 0.25秒では40/60frameで8%を超えた。安全余白2%（6% clip target）＋上昇tau .25秒/下降tau .05秒の非対称EMAは8%超過0/60frameだったが、平均strengthは0.259、平均flash luma deltaは0.120まで下がり、後半はstrengthがほぼゼロとなる。ハードなclip制約をそのままtargetにEMAするか、余白を大きくしすぎると、ちらつきか効果消失のどちらかになる。

**Highlight shoulder alternative:** shot先頭で選んだstrength 0.8を固定し、highlight rolloffだけを0.78/0.85/0.90/0.95/1.0からclip上限を満たす最小値へ適応させた。8%超過は0/60frame、平均clip channel率7.29%、最大7.99%。mean linear-luma deltaは0.14885で、framewise strength選択の0.13899より約7.1%高く光量を維持した。rolloff平均0.853、最大隣接差0.07。contact sheetは `captures/exposure-video-adaptation-rd5`。

**暫定判断:** 高key素材ではstrengthを下げ続けるより、光のfill強度を保ちながらflash中のhighlight shoulderを強くする方が、この4秒fixtureではclip制約と平均照度の両立に有望だった。rolloffの離散候補は局所的な見た目変化を生む可能性がある。別素材、motion-compensated temporal flicker、HDR、より広い進行範囲で再検証が必要。

**Rolloff continuity follow-up:** rawの離散rolloff列には隣接差0.05〜0.07の単発切替があった。clip targetに2%の余白を置いた連続二分探索と非対称EMA（rolloff上昇tau .08秒、低下tau .25秒）を追加した。固定strength 0.8のまま、60frameで最大隣接rolloff差は0.0204、clip率最大6.12%（8% target超過0frame）、平均clip率5.92%、mean linear-luma delta 0.14558。raw adaptive-rolloffはclip率最大7.99%、mean delta 0.14885、rolloff step最大0.07。平滑版は平均光量をraw比約2.2%下げる代わりに、clip余裕を広げてパラメータ段差を小さくした。全60frameで測った候補rolloff 0.78/0.85/0.90/0.95/1.0のnew-clip値は単調非増加だった。capture/metricsは `captures/exposure-video-adaptation-rd7`。

**視覚確認:** contact sheet上で雪面・橋の形状を保ちながらhighlightsのrolloffが緩やかに変化した。これは縮小SDRの目視で、動き補償したちらつき評価や実時間プレビューではない。

## 2026-11-10 — Low-key/high-key exposure-range comparison

**方法:** `flash_exposure_video_adaptation_lab.py` に `--synthetic-exposure-stops` を追加し、同じCC0 60frameへlinear-light一様露出を適用して−2 stopsと0 stopsを比較した。strength候補の上限を既存Flash Fill default付近の5.0まで拡張した。0 stops用captureは `captures/exposure-video-adaptation-bright-rd2`、−2 stopsは `captures/exposure-video-adaptation-dark-rd3`。これは同じ映像の露出レンジ比較で、低照度撮影のsensor noiseや色応答を再現しない。

**結果:** 平均source p95 linear lumaは−2 stopsで0.186、0 stopsで0.742。暗条件ではframewise selectorが全60frameでstrength上限5.0まで到達し、最大newly clipped channel率0.10%、平均flash luma delta 0.216。rolloffは常にbase .78で、clip headroomがstrength上限を決めなかった。接触sheetでは暗部を保ちながら雪面/橋のfront fillが見える。対して0 stopsではframewise strength range .277–1.406、平均.654、全フレームで8%以下を維持。固定strength 1.406は後半59/60frameで超過。固定strength＋rolloff最大1.0でも11/60frameで8%を超えた。framewise strength＋adaptive/smoothed rolloffはこの露出条件ではstrengthだけでclip targetを満たすためrolloffがbase .78のままであり、強度制御に追加改善をもたらさなかった。

**暫定判断:** clip率だけで低キー素材の適切な光量は決められない。暗条件はstrength上限5.0でまだclip headroomが十分あり、目標の見た目やsubject-relative brightnessなど別の美的controlが必要。高key条件はshot頭で選んだfixed strengthを後半へ持ち越せず、rolloffだけでもclipを完全に抑えられない。素材全体を見るshot-level strength envelope、またはframewise strengthとrolloffの併用をさらに評価する。

**未検証:** 実際の暗所カメラnoise、異なる低キー実写、strength >5.0の効果範囲、複数素材の視覚評価、実時間動画再生。

## 2026-11-10 — Interactive preview strength baseline

**方法:** `flash_preview_strength_sweep.py` を追加し、対話previewの既定位置・色温度・散乱・rolloffを固定したまま、CC0 walkwayの連続60frameをstrength 0.4 / 0.8 / 1.2 / 2.3で比較した。代表5フレームを接触シート化し、各frameのsourceに対して新たに1.0へ達したRGB channel比率と平均linear-luma増分を計測した。結果は `captures/preview-strength-sweep-rd1`。

**結果:** strength 0.4 / 0.8 / 1.2 / 2.3 の平均新規clip率はそれぞれ1.77% / 3.04% / 4.89% / 10.39%、最大は3.60% / 7.12% / 11.99% / 20.92%。8%を超えたフレームは0 / 0 / 16 / 34。平均linear-luma増分は0.06825 / 0.08428 / 0.09980 / 0.13834。接触シートでは2.3で氷面と橋の明部が広く白く持ち上がる。0.8はfrontを視認できる範囲を残しつつ、候補中で8%の新規clip proxyを全frameで守ったため、standalone previewの初期strengthを2.3から0.8に下げた。

**暫定判断・限界:** 0.8はこの高key冬景色fixtureに対する作業開始値であり、全素材に対する美的最適値や物理ストロボの露出校正値ではない。新規clip率はハイライト保持のproxyであり、顔・肌・暗所・異なる画面比率での見た目評価は未実施。strengthを下げても現行screen-space flashは幾何や遮蔽を推定せず、照明の物理的正しさは別課題として残る。

## 2026-11-10 — Real-footage spatial path sweep

**方法:** `flash_spatial_path_sweep_lab.py` を追加し、同一CC0 walkway動画のprogress 0.18/0.36/0.54/0.72/0.90に対応する5フレームへ、既定strength 0.8でradial top-center、radial left-mid、radial lower-left、directional left-to-right、directional top-to-bottomを適用した。softness・色温度・scatter・rolloffなどはpreview既定値で固定した。capture/metricsは `captures/spatial-path-sweep-rd1`。

**結果:** 5 sample平均linear-luma deltaは上記順に0.0939/0.0888/0.0884/0.1039/0.1061。delta >0.01の画素coverageは94.9%/90.0%/90.0%/91.9%/96.1%、平均新規clip channel率は3.41%/2.51%/2.33%/3.17%/3.60%。最大sample clip率は6.60%/5.28%/4.35%/8.37%/8.46%。この高key素材では放射源を左下へ移すほど画面全体のwashとclipが減る一方、directional前線は明るさcoverageを広く保ち、最大clip率が8%を少し超えた。

**Preview handoff:** standalone `flash_video_preview.py` にDirection X/Y controlsを追加し、preset schema内に方向ベクトルを保存・読込できるようにした。旧presetは既定方向を使い、入力ベクトルは正規化、zero vectorは拒否する。directional modeの左右・上下preset snapshotが異なる画像を生成することを確認（matched snapshot mean absolute pixel difference 2.97/255、max 34/255）。Radial modeでは既存Source X/Y controlsを使う。

**制約:** これはCC0冬景色の5 matched framesであり、camera/scene geometry、flash ground truth、motion-compensated temporal評価を含まない。directional pathは実写における実際のフラッシュ光源位置と同等ではなく、方向別のlook controlである。顔・室内・暗所・複数光源などで同じ候補を評価するまで、default変更は行わない。

**露出レンジ追試:** 空間sweepへ `--synthetic-exposure-stops` を追加し、同じ5 frame/pathを0 stop strength 0.8、−2 stop strength 0.8/2.4で比較した。0 stop metricsは `captures/spatial-path-sweep-bright-rd2`、低key候補は `captures/spatial-path-sweep-dark-rd1` と `...dark-strength24-rd1`。−2 stop / strength .8では各pathの平均luma delta .0215–.0327、front coverage 59.1–71.0%、new clip 0%。strength 2.4ではradial path .0688–.0706、coverage 73.3–74.8%、directional path .1069–.1080、coverage 74.0–74.3%、全pathでnew clip 0%。contact sheetでは雪面や橋を保ちながら暗い画面にfrontが見える一方、directionalの照明は画面広域を均等に持ち上げる。

**暫定判断:** この合成露出レンジでは、暗素材にstrengthを上げてもclip headroomは十分あり、bright素材ではstrength .8でclipが平均2.3–3.6%発生する。露出に応じたstrength調整は有望だが、-2 stopは一様linear gainの画像加工でsensor noise・black level・色応答を再現しない。2.4を暗所用preset/defaultに昇格させず、実際の低照度CC0素材と別の明るい素材で視覚比較する。

## Production integration findings

- `Artifact.Effect.Glow` currently has separate CPU and GPU implementations. Both use fixed-threshold sampling and preserve the center pixel alpha. The GPU implementation owns a source upload, output texture, staging readback, `Flush`, and `WaitForIdle`; that self-contained route must not be used as a resident per-frame integration path.
- The resident raster effect plan passes a single source texture through bounded nodes. A true multiscale Glow must preserve the original surface until final composite and reuse downsample/blur/accumulation resources across frames. Chaining ordinary Gaussian nodes does not represent the tested pyramid.
- `ArtifactCore::Image.SurfacePixelConversion::convertSurfacePixelsInto` converts descriptor-aware BGRA/RGBA inputs to a reusable `Rgba32LinearStraight` buffer; it now also supports reusable `Rgba16LinearPremultiplied` and `Rgba32LinearPremultiplied` outputs. A descriptor-targeted float32 overload provides the return conversion so a CPU effect can write pixels in the host's transfer/channel/alpha contract.
- The existing ArtifactCore OpenCV Glow functions are legacy BGRA operations and do not implement the new descriptor-aware soft-knee pyramid.
- `Artifact::FilmLightLeakEffect` is preset data; `ArtifactCore::LightLeakTransition` is a two-frame video transition. Neither is the still-raster Light Leak effect studied here.
- The tested looks are analytic screen-space fields, so a first app candidate can be a single-pass resident filter with normalized coordinates and no blur pyramid. It still needs descriptor-aware linear-light math, explicit alpha policy, exposed placement/look controls, and CPU/GPU capture comparison. The existing `DirectionalGlowEffect` samples the input along long streak kernels and is not a drop-in replacement for these authored leak fields.
- `ArtifactCore::applyStillLightLeak` exposes the captured edge-wash, anamorphic-streak, and deterministic organic-lobe fields for linear-premultiplied `CV_32FC4` inputs. `Artifact::StillLightLeakEffect` converts through the descriptor-aware Core buffer API, and its resident shader carries the same three analytic fields and alpha policies. The inspector catalog exposes Look, Strength, and Alpha Policy. The latest Core and `ArtifactEffectsGlow` target compiled successfully, including the organic-look changes. `ArtifactEffectService.cpp` also compiled in an earlier isolated selected-file build. The full app target remains blocked by the unrelated MSVC C1116 described above; no app capture or pixel-parity measurement has been run. Organic gate leak is now a deterministic sum of normalized-coordinate soft lobes, implemented with matching field equations in Python, ArtifactCore CPU, and the resident HLSL shader. Its screen amount is lower than the edge/streak looks to limit snow-field washout.



## 2026-11-10 — Soft-knee Glow look and SDR headroom sweep

**方法:** `glow_look_sweep_lab.py` でCC0 walkway frame 20にwarm-near、balanced、dream-wideの3 parameter setを適用し、通常のlinear additive結果とdisplay headroom曲線を並べた。表示値>=0.999をnear-white proxyとして計測し、sourceから増えた割合も別に出した。Captureは `captures/glow-look-sweep-cc0-frame20/`。

**結果:** sourceのnear-whiteは0.0623%。通常加算の新規near-whiteはwarm-near 20.9199%、balanced 26.8065%、dream-wide 24.3762%。headroom候補では各1.1918%、1.8963%、1.7147%だった。warm-near/balanced/dream-wideのcenter linear-luma deltaは、加算で0.20418/0.24314/0.22870、headroomで0.16295/0.19398/0.18271。edge deltaは同順で加算0.15054/0.19356/0.19281、headroom0.12135/0.15590/0.15555。contact sheetではheadroom版が橋梁・氷面の局所コントラストをより残し、warm-nearが最も控えめだった。

**暫定判断:** この高key SDR素材では、soft-kneeにheadroom-aware shoulderを加えるとnear-white増加を大幅に抑えながらGlowの広がりを残せる。warm-nearは比較候補として採用し、普遍的defaultとはしない。headroom式は表示上限1.0に対するSDR look/reference専用で、HDR scene-linear Core演算へそのまま移植しない。HDR/表示変換の境界で再設計が必要。

**Cross-material follow-up:** deterministic neon fixtureで同じsweepを取り、通常加算の新規near-whiteは全候補0.1635%、headroomはwarm-near 0.0004%、balanced 0.0118%、dream-wide 0.0000%。center linear-luma deltaは通常0.01056/0.01534/0.01582、headroom0.00988/0.01434/0.01483。暗いネオンではhaloの見え方を保ちつつ、SDR clip proxyの低下が成立した。Captureは `captures/glow-look-sweep-neon-headroom-rd/`。

**未検証:** 肌・他の暗部・alpha素材での見た目、HDR/異なるtransfer function、時間連続性、ArtifactCore/app effectへの統合、resident GPU実装とCPU/GPU parity、処理時間。

## 2026-11-10 — Deterministic organic Light Leak candidate

**変更:** Python streakのscreen amountを0.74からCore/HLSLと同じ0.8へ揃えた。乱数coarse fieldのresize/blurに依存していたorganic referenceを、正規化座標上の複数Gaussian lobe、下端pooling、amber/magenta/blue寄与を重ねる解析式へ置き換えた。同じfield係数をPython、`ArtifactCore::applyStillLightLeak`、`StillLightLeakEffect` resident HLSLへ反映し、Look enumにOrganic Gate Leakを追加した。Organicのscreen amountは0.34、Edge Wash/Streakは0.8とし、雪景色での白飛びを抑える比較候補にした。

**Capture:** CC0 walkway frame 20、strength 0.55の接触シートを `captures/light-leak-organic-analytic-rd/` に保存。source mean RGB 0.59232に対しOrganicは0.63277、Edge Washは0.61866、Streakは0.60479。Organicのlinear-light deltaはtop-left/center/bottom-rightで0.02412/0.01299/0.03341（960x540）、0.02417/0.01299/0.03346（641x359）、0.02415/0.01329/0.03376（320x180）。正規化配置はこの素材・縮尺群で安定し、下端に色が残りつつ雪面全体への過剰なwashを旧organic候補より弱めた。

**暫定判断:** 乱数textureやblur pyramidを使わない決定的なorganic形状は、single-pass resident shaderとCPU参照の共有に向く。今回の0.34量と色配合は一つのCC0冬景色に対する見た目候補であり、万能な既定値ではない。Python captureは設計式の独立確認であって、C++ CPU/GPU parity captureではない。

**Alpha capture:** `light_leak_alpha_lab.py --look organic` の透明plateではpreserve時のalpha維持とclip policyのcomposite外漏れ0.00000000を確認。energy expansionは完全透明画素224,183個にcoverageを作り、linear-premultiplied契約違反0.00000000。Captureは `captures/light-leak-organic-alpha-rd/`。

**未検証:** 変更後のC++ build、GPU実行capture、CPU/GPU差分、別の明暗・画面比率・alpha素材、HDR、時間変化でのorganic形状。


## 2026-11-10 — Glow pyramid workspace capacity reduction

**変更:** `SoftKneeGlowWorkspace`から独立した`blurScratch`を除き、levelごとのdownsample matrixを必要寸法に割り当てる構造へ変更した。level 1/3は最大1/2解像度の同じbuffer、level 2は最大1/4解像度の別bufferを使う。OpenCV `GaussianBlur`はsource/destinationを同一Matに指定してin-place処理する既存利用形に合わせ、blur用の3枚目のhalf-resolution RGB planeをなくした。

**算定:** 3840x2160、CV_32FC3 planeで、full-resolution 2枚 + half-resolution 1枚 + quarter-resolution 1枚は約219.4 MiB。従来のfull 2枚 + half 3枚の約261 MiBから約41.6 MiB減る。これはMat容量の理論値であり、OpenCV内部scratchや他の画像bufferを含まず、runtime allocation/profiling結果ではない。

**実施:** 変更後の`ArtifactCore` target compileはexit 0。**未検証:** OpenCV各kernelでのin-place動作を含む画素一致、odd dimensions、Mat再利用、処理時間・peak RSS。

## Core conversion groundwork

`ArtifactCore/include/ImageProcessing/OpenCV/Glow.ixx` now declares `applySoftKneeMultiScaleGlow`, which accepts linear-premultiplied RGBA and a caller-owned four-level workspace. It performs soft-knee seed extraction, fixed-scale downsampling, Gaussian blur, linear upsampling, and source-alpha preserve or energy expansion. The existing `Glow.cpp` is already in the Core source manifest, so no CMake registration was added. `glow_alpha_lab.py` now uses the same premultiplied-seed and pyramid model for its transparent fixture; its display conversion remains SDR-clamped.

`ArtifactCore/include/Image/SurfacePixelConversion.ixx` adds `Rgba16LinearPremultiplied` and `Rgba32LinearPremultiplied` input-conversion targets. `convertSurfacePixelsToDescriptorInto` provides the return float32 path for a fully specified target descriptor, including RGBA/BGRA order, transfer encoding, and straight/premultiplied alpha. Both APIs accept persistent workspace buffers, so steady-size frames retain byte capacity. Artifact's standard Glow does not call these Core APIs yet.

`Artifact::StillLightLeakEffect` is registered as `still_light_leak` in Effect Service and Inspector Effect Catalog. Its CPU implementation uses persistent surface conversion buffers and a persistent OpenCV result matrix. Its GPU implementation is an analytic `GpuGenericResourceKind::Filter` resident shader using normalized UV fields; it does not add staging readback or blocking waits. Edge wash, streak, and organic are offered with matching CPU/Core/shader field equations; the organic look uses a reduced screen amount. The user-facing Look and Alpha Policy are integer properties pending a richer choice-property control.

The Core pyramid workspace has two full-resolution RGB float planes, one half-resolution RGB float plane, and one quarter-resolution RGB float plane. Level 1 and 3 reuse the half-size buffer; level 2 uses the quarter-size buffer. Gaussian blur is performed in place, removing a separate half-size blur plane. At 3840x2160 the fixed workspace is approximately 219.4 MiB (down from the previous 261 MiB estimate) before source, destination, conversion buffers, or OpenCV internal scratch. This is a static capacity estimate, not a measured allocation or runtime result; OpenCV internal allocations and processing time remain unprofiled, so the CPU reference is not yet suitable for unbudgeted interactive playback.

On 2026-11-10, `cmake --build build --target ArtifactCore ArtifactEffectsGlow --config Debug --parallel 4` completed with exit 0 and compiled the latest Core and Glow effect code. The selected-file `ArtifactEffectService.cpp` compile had also succeeded earlier. An earlier full-app build in another configuration did not complete, but an MSVC 14.52 Ninja build in `out/build/x64-Debug-Agent` later succeeded and deployed current `Artifact.exe` and `ArtifactUiTest.exe`. `capture_ui_test.py` captured a valid Japanese startup view; the English startup capture was blank gray. No test target was run.

An alternate `x64-Debug-ClangXX-MsvcAbi` configure completed and selected Vulkan; D3D11/D3D12 support was disabled in that configuration. Its first full `ArtifactUiTest` build compiled dependencies and much of ArtifactCore, then stopped at `ArtifactCore/include/...` modules with four visibility errors for Qt `qsizetype`. This remains a separate toolchain/module integration issue. The successful current MSVC 14.52 app build also has D3D12 disabled and Vulkan enabled. No dedicated Light Leak composition fixture is available yet, so the app startup capture validates launch/localization only; independent Python images remain the only verified Light Leak effect captures. Runtime GPU dispatch and CPU/GPU parity are still unverified.

## 2026-11-10 — Neon-scene Light Leak strength and look sweep

**方法:** Existing deterministic neon still fixture from `glow_lab.py` was passed through `light_leak_lab.py` at shared strength 0.30, 0.55, and 0.80. The capture set shows Edge Wash, Streak, and Organic Gate Leak on the same plate. Normalized-coordinate response was also measured at 960x540, 641x359, and 320x180. Captures: `captures/light-leak-neon-scene-sweep-030/`, `captures/light-leak-neon-scene-rd/`, and `captures/light-leak-neon-scene-sweep-080/`; source: `captures/light-leak-neon-source.png`.

**結果:** At strength .55, the 960x540 linear-light delta in top-left/center/bottom-right was Edge Wash .12420/.00362/.00832, Streak .14802/.00002/.00000, Organic .04276/.01808/.05980. Organic is the only one of the three that visibly carries colored light into both the center and lower-right field; Streak remains localized near its top-left source, while Edge Wash gives a broad but strongly corner-weighted wash. Organic output mean RGB rose from .09844 source to .16797/.22152/.26181 at strengths .30/.55/.80. Regional deltas stayed close across all three sizes (largest reported difference between 960x540 and 320x180 was below .002 linear-light). On this dark neon plate the current Organic amount reads as a broad atmospheric tint at .55 and stronger, rather than a small optical gate artifact. The .30 capture retains more black negative space. Saved 8-bit sRGB outputs showed no newly near-white channels at threshold .999 for these strengths; this is only a display-clipping proxy, not an HDR or pre-quantized measurement.

**暫定判断:** Keep the three looks distinct: Edge Wash for edge/corner contamination, Streak for a localized anamorphic source, Organic for frame-wide gate color. For the current Organic screen equation, .30 is the better starting candidate on this dark neon plate; .55 remains viable on the snowy high-key scene but spreads more color across the frame. A single universal strength default is not supported by these two unlike materials. This evidence suggests adding material-aware presets or more explicit strength control, but the app's current integer property UI and fixed organic amount should be reviewed before changing the production default.

**制約:** One synthetic neon plate and one CC0 winter plate, one fixed analytic field placement, 8-bit display captures, and no physical film scan or real lens-leak reference. These captures validate the Python candidate only; they do not establish Artifact CPU/GPU image parity or user preference. Check portrait/skin, daylight, gradients, alpha edges, and multiple leak positions before revising Core/HLSL coefficients.

## 2026-11-10 — Anamorphic streak source-position control

**方法:** `light_leak_lab.py` now accepts normalized `--streak-position X Y`. The default remains (0.01, 0.105); the default-position capture was byte-compared against the previous CC0 snow capture and the maximum pixel delta was 0. Position sweeps used (0.50, 0.24) on snow, and (0.50, 0.25)/(0.88, 0.45) on the neon plate, each at strength .55. Captures: `captures/light-leak-streak-origin-snow-rd/`, `captures/light-leak-streak-origin-center-rd/`, and `captures/light-leak-streak-origin-right-rd/`.

**結果:** With the source moved to (0.50, 0.24), the snow plate's streak linear-light delta changed from default top-left/center/bottom-right .08006/.00002/0.00000 to .01757/.03800/0.00000, placing the strongest contribution across the bridge and center instead of the upper-left edge. On neon, the center source (0.50,0.25) yielded .04598/.06384/.00001; the right source (0.88,0.45) yielded .00003/.03855/.01301. Regional results were stable across 960x540, 641x359, and 320x180. Visual captures show position control lets the streak follow a scene's bright subject instead of always originating off the top-left edge.

**Artifact integration:** Added normalized `streakSourceX/Y` fields to `ArtifactCore::LightLeakParameters`, clamped them to [0,1], and used them in the CPU streak source and beam equations. `StillLightLeakEffect` now exposes `Streak Source X/Y` float properties and forwards them to both CPU parameters and resident GPU generic parameters P3/P4; HLSL uses matching default offsets and source coordinates. The default coordinates reproduce the previous independent capture exactly in the Python reference. `ArtifactCore` and `ArtifactEffectsGlow` targets compiled successfully with MSVC on 2026-11-10. Extracted the actual resident HLSL with the production parameter prelude and compiled it with `dxc` as both D3D12 DXIL (`cs_6_0`) and Vulkan 1.1 SPIR-V successfully. A full app build and Japanese startup UI capture then succeeded; no Light Leak image fixture has been integrated into the app, so effect rendering and CPU/GPU parity remain unverified.

**暫定判断・制約:** A movable streak is a meaningful spatial control with negligible field-equation complexity, but it remains a one-axis rotated soft beam and is not an optical simulation. This changes only the anamorphic streak; Edge Wash and Organic ignore these coordinates. Validate CPU/GPU image parity and actual inspector editing before treating the controls as production verified. The app exposes X/Y as generic float properties because the current property implementation has no dedicated two-dimensional source-position control here.

## Acceptance gates before promoting these candidates to standard effects

1. Connect the new Core Glow routine to a descriptor-aware CPU reference using reusable conversion buffers and explicit host transfer/alpha behavior.
2. Compare the Core output against PNG references using the same fixtures. Check alpha policy, finite values, and the SDR premultiplied invariant.
3. Implement the same visual contract in Artifact's resident GPU route. Glow's multiscale pyramid requires persistent resize-time resources and multiple passes; do not replace it with one large per-pixel kernel.
4. Capture CPU/GPU output differences on even/odd dimensions and transparent edges. Record timing and resource creation/reuse before making the candidate the standard effect.
5. Do not claim production parity until a Light Leak fixture exercises the application render path. A supported MSVC app build and Japanese startup capture are available; the alternate Clang/MSVC ABI build still has Qt `qsizetype` visibility errors. Add or reuse an isolated fixture before judging effect output, then compare Core CPU and Vulkan GPU renders.

For Still Light Leak, additionally compare the Core CPU API, Artifact CPU reference, and resident GPU output on the CC0 frame 20 and transparent plate at even and odd sizes. Record difference metrics, clipping/alpha behavior, dispatch time, and steady-size CPU/GPU resource reuse. The captures produced so far are independent Python references, not app captures.

## 2026-11-10 — Full-sequence low-key temporal comparison

**方法:** `flash_video_lab.py` に方向ベクトル指定 (`--direction X Y`) と線形露出オフセット (`--synthetic-exposure-stops`) を追加し、CC0 walkway fixture全60フレーム (360×640, 15fps, 3.93秒) でradial top-centerとdirectional left-to-rightを強度0.8/2.4で比較した。4条件とも露出は-2 stops、Farneback動き補償指標を有効化。captureは `captures/full-temporal-dark-radial08`、`full-temporal-dark-radial24`、`full-temporal-dark-directional08`、`full-temporal-dark-directional24`。

**結果:** 全条件でclipped channel率は0%。平均linear-luma増分はradial 0.0196→0.0626、directional 0.0289→0.0950。motion-compensated flash-contribution mean absolute delta (59 frame pairs平均) はradial 0.00175→0.00581、directional 0.00279→0.00978で、高強度側が約3.3〜3.5倍。最大値はそれぞれ0.00880→0.03124、0.01442→0.05515。動き補償valid fractionは全条件で平均約99.26%。したがってこの条件では高強度は黒つぶれを避けたまま画面を強く持ち上げるが、フラッシュ寄与の時間変化も増やす。これは意図した前線の進行を含む指標であり、ちらつきそのものの合否ではない。

**制約:** -2 stopsはlinear-light一様減光で、センサーnoise、ISO、トーンカーブ変化を模擬しない。Farnebackは近似で、遮蔽・細部・照明変化の残差が混じる。実写1素材・固定presetのため、自然なちらつき評価には複数の動き／露出条件と、前線を除いた静止領域での時間変動測定が必要。

## 2026-11-10 — Interactive light-response controls

**変更:** 独立動画previewにRadial Falloff、Highlight Threshold、Highlight Rangeのtrackbarを追加し、操作中の描画とPキーのJSON preset保存に反映した。これで光源からの距離減衰と、実シーンの明部からレンズ散乱を生成する閾値・幅をフレーム単位で試せる。従来のpreset schemaを維持し、既定値も既存値を使う。

**確認:** プレビューの再描画cache keyに3値を含め、slider変更で再レンダーされる実装を確認。Python構文確認は実施した。対話UIの実画面操作・preset往復保存は未確認。

**次に見ること:** 高keyと-2-stop低keyの双方でthreshold/range/falloffの組合せを比較し、clip率、光源近傍と前線後方のluma差、highlight spillの形を記録する。

## 2026-11-10 — Exposure-relative highlight seeding

**方法:** `flash_response_knob_sweep_lab.py` でCC0 walkwayのframe 24、progress 0.56、strength 0.8を使い、standard exposureと線形-2 stopsでradial falloff 0/2.7/7.0、fixed highlight profile 3種、フレーム内luma percentile profile 3種を比較した。captureは `captures/response-knob-sweep-cc0-rd3`。この結果から動画全60フレーム用の `flash_video_lab.py --highlight-percentiles THRESHOLD_PCTL RANGE_END_PCTL` を追加し、p70–p95、p85–p99を固定threshold .12/range .75と比較した。captureは `captures/lowkey-highlight-fixed12`、`lowkey-highlight-percentile70_95`、`lowkey-highlight-percentile85_99`。

**結果:** 静止frameの固定threshold .28/.50は-2 stopsで散乱がほぼ消え、固定threshold .12/range .75でもscatter on-minus-off linear-luma deltaは0.00053だった。source frame内のp70–p95は0.01486、p85–p99は0.00503で、クリップはいずれも0%。この差分はscatter on/off間のhighlight shoulder相互作用を含み、純粋なbloom量ではない。全60 framesではfixed、p70–p95、p85–p99の平均flash luma deltaが0.01990/0.02882/0.02281、motion-compensated flash-contribution deltaが0.001775/0.002507/0.002082。p70 thresholdの各frame値は0.13391〜0.14415 (mean 0.13934, std 0.00294)、seeded pixel率は約30%。低keyでもハイライト散乱を残せる一方、p70–p95はfixedよりフラッシュ寄与とその時間変化を約45%/41%増やした。

**制約・判断:** percentileは各frameごとに再計算されるため、カット境界や強い構図変化で値が動く可能性がある。1本の固定カメラ素材だけで、-2 stopsは均一linear scalingでsensor noiseを再現しない。relative thresholdは露出レンジ対応の候補として保持し、既定化は保留する。複数素材・実センサーnoise・shot単位の閾値平滑化比較を追加し、前線による変化と不自然な明滅を分けて評価する。

## 2026-11-10 — Shot-local highlight threshold smoothing

**方法:** `flash_video_lab.py` に`--highlight-smoothing-seconds`を追加。フレームごとのpercentile threshold/rangeを時定数tauの因果EMAで平滑化する。60-frame低key比較でp70–p95、tau=0.4秒/1.0秒をinstant p70–p95およびfixed threshold .12と比較した。captureは`captures/lowkey-highlight-percentile70_95-ema0p4`、`...ema1p0`。

**結果:** instant EMAなしに対しtau .4/.1.0秒ではthresholdのフレーム差平均が0.00031から0.000174/0.000121、最大差が0.001779から0.000293/0.000199へ低下。平均flash luma増分は0.02882から0.02871/0.02863、クリップ率は全条件0%。motion-compensated flash-contribution deltaは0.002507から0.002503/0.002494とほぼ維持された。この固定ショットでは0.4秒が閾値変動を下げつつ光量を保つ候補となった。

**独立preview:** `flash_video_preview.py` と`flash_preset.py`にFixed / Per-frame Percentile / EMA mode、threshold/range-end percentile、smoothing time controlsを追加。EMAは連続する前向きフレーム間で状態を継続し、シーク、逆方向step、設定変更では表示中フレームのpercentileを初期値にして再開する。旧presetは既定値で読み込む。

**制約:** EMA比較は1本の固定ショットで行い、カットまたぎの露出追従は未評価。カット後にしばらく旧shotのthresholdが残るため、実用時はcut時resetまたはshot boundaryごとの再初期化が必要。対話previewの画面操作・preset往復保存は未確認。-2 stopsはsensor noiseを含まない。

## 2026-11-10 — Reset percentile EMA at a known cut

**方法:** `flash_video_lab.py`へ`--highlight-reset-frames`を追加し、選択範囲に対する既知カットindexでpercentile EMAを目標値へ再初期化できるようにした。`flash_video_preview.py`にはsource timestamp指定の`--highlight-reset-seconds`を追加。`flash_cut_fixture_lab.py`でCC0 walkway先頭16 frameを使い、frame 15を同構図の線形-2 stops版へ切替えて以後反復する32-frame / 15fpsのcontrolled exposure-cut AVIを生成。固定threshold、instant p70–p95、tau .4秒EMA継続、tau .4秒EMA + frame16 resetを比較した。inputは`captures/highlight-cut-rd1/known-exposure-cut.avi`、生成設定は隣接JSON、各結果は同フォルダ内の4 strategy directory。

**結果:** cut frameのp70 targetは0.141。EMA継続は旧shot threshold 0.49887を保持し、cut後平均threshold error 0.13557、seed pixel約3.48%となった。frame16 resetは最初からtarget 0.141、post-cut threshold error 0、seed pixel約30.04%。post-cut平均linear-luma増分はEMA継続0.03287、reset 0.04615。すべてclip率0%。同じ低key shotでは境界resetによってhighlight seedingが即時に新露出へ追従し、EMAの遅れによる散乱不足を解消した。

**制約:** これは実scene cutではなく、同じフレームを反復したsynthetic exposure cutであり、境界時刻は既知として与えた。自動cut検出と、内容・色・露出が同時に変わる素材でのreset判断は未評価。resetは時間的な境界を越えたと明示できる場合に使う。

## 2026-11-10 — Opt-in grayscale-histogram cut reset

**方法:** `flash_cut_detection.py` に32-bin grayscale histogramとBhattacharyya距離の共通cut scoreを実装。動画レンダーと対話プレビューへ任意の`--detect-highlight-cuts`を追加し、距離がthreshold以上になった連続forward frameでpercentile EMAをresetする。既定threshold 0.35。cut fixture generatorはpre/cut/post距離をJSONへ記録する。

**検証:** CC0 walkwayの通常60-frame素材で隣接histogram距離max 0.01373、p95 0.01249、検出0件。controlled exposure cutは事前max 0.01009、境界frame16で0.67170、境界後0。threshold 0.35でframe16だけを検出し、手動reset版に対して32全frameのhighlight thresholdとmean output-luma metricsが一致した。

**制約:** histogramだけのheuristicで、明るさ変化やflash/strobeもcutと誤認しうる一方、似たグレースケール分布のscene cutは見逃しうる。1本の短い自然素材とsynthetic exposure cutのみなので既定では無効とし、内容・色・露出が同時に変わる複数素材でprecision/recallを確認してから採用判断する。対話previewでの実画面・連続再生確認は未実施。

## 2026-11-10 — Spatial supplement for matched-histogram cuts

**方法:** `flash_cut_detection.py`に160px以下 grayscale thumbnailのnormalized mean-absolute spatial differenceを追加。自動resetはhistogram距離>=0.35またはspatial差分>=0.12で判定し、動画評価・対話previewが共有する。fixture generatorにpost-cut source frame指定を追加し、同じCC0 walkwayのframe 0–12からframe 55へ切替える構図カットを作った。

**結果:** 既存CC0の自然な単一60-frame shotではcombined detectorの検出は0件 (max histogram 0.02132、max spatial 0.03154)。構図cut fixtureでは境界距離がhistogram 0.11908 (hist-onlyしきい値未満)、spatial 0.19608となり、spatial補助によりframe13を検出。spatialしきい値を1.0にして補助を止めると検出しなかった。-2 stops exposure cutではhistogram 0.68845、spatial 0.28015でframe16を検出。構図cutの自動reset結果は手動frame13 resetと全32-frameのthresholdおよびmean output-luma metricsが一致した。

**制約:** CC0素材に自然なscene cutはなく、構図カット評価は同一shot内の遠い2フレームをつないだcontrolled fixture。連続カメラパンは小さい空間差分だったが、rapid motion / flash / strobeはspatialしきい値を越える可能性がある。異なるhistogramでも画素位置だけが変わる実scene cutsの代表性は未検証。自動検出はopt-inのまま維持し、UI実画面での連続再生確認も未実施。

## 2026-11-10 — Highlight scatter coupled to the traveling front

**目的:** 進行する光の範囲外にあるハイライトもpulse開始と同時に散乱する既存挙動を、画像全体へ均一に適用する場合と、前線の到達に合わせて発生させる場合で比較する。

**方法:** `flash_fill_lab.py` の`flash_fill`に`scatter_front_coupling`を追加し、0で従来のglobal bloom、1で前線マスク連動、0.5で中間を選べるようにした。独立比較器`flash_scatter_front_lab.py`でGlowLabの1536×1024ポートレートplateを使い、radial source (.08,.5)、strength 2.4、scatter 2.0、front duration .72秒を固定して、4時点 (.25/.45/.65/.85) と3段階couplingをレンダーした。SDR luma/clip、元のglobal版との差、全alpha保持とfinite値を確認した。captureは`captures/scatter-front-rd1`。

**結果:** coupling 1の元版に対するmean absolute linear-luma差はprogress .25/.45/.65/.85で0.000629/0.000917/0.000613/0.000091。global版のmean linear-luma増分は0.01455/0.04347/0.07724/0.08063、front-gated版は0.01398/0.04254/0.07662/0.08054。newly clipped channelはglobal版0.0058–0.0599%、gated版0.0003–0.0548%。このlookでは全体露出とveilが支配的で、front-gated bloomの視覚・測定差は小さい。接触sheetでは人物・濡れた路面を大きく崩さず光量が進むが、gatingによる説得力の明確な改善は示せていない。

**暫定判断・制約:** scatter/front couplingはart-direction候補として独立実験に追加するが、既定値は変更しない。効果差が弱いため、次は露出veilから散乱成分を分離した診断表示、源ハイライトごとのspill、異なる光源位置と暗部素材で比較し、前線外に不自然なbloomが残るかを評価する。ポートレートplateは校正済みカメラ素材ではなく、物理的な光路・遮蔽・レンズPSFの証明にはならない。動画でのちらつき、HDR、対話previewの実操作も未検証。

## 2026-11-10 — Veiling-light balance and component diagnosis

**目的・変更:** 前回のscatter/front coupling差が小さかった原因を測るため、`flash_fill`に任意の成分返却を追加。元画像上へ独立合成したexposure、highlight scatter、veilと、線形光の合成結果を出せる。veil係数`veil_gain` (既定0.025)を露出強度から独立させ、`flash_fill_lab.py` / `flash_video_preview.py`の操作ノブとJSON presetに通した。`flash_fill_decomposition_lab.py`は成分接触sheet、luma/clip測定、veil-gain sweepを生成する。captureは`captures/fill-decomposition-rd1`。

**結果:** strength 2.4のportrait plateで、既定veil gain 0.025時のveil単独mean linear-luma deltaはprogress .18/.36/.54/.72/.90で0.00601/0.02636/0.05143/0.07821/0.06352。対応する合成deltaは0.00699/0.02951/0.05776/0.08822/0.07137で、veilが概ね89%を占めた。progress .54でveil gainを0/.00625/.0125/.025とした合成deltaは0.00633/0.01919/0.03205/0.05776、newly clipped channel率は各0.0375/0.0379/0.0382/0.0392%。見た目では0.025が人物・濡れた路面を強い一様veilで洗い、.00625/.0125は局所露出の階調を比較的残す。既定値を変更せず、候補レンジ0.00625–0.0125を次の動画比較対象とする。9-frame、8fps、1秒の低veil進行を`captures/veil-restrained-progression-rd1`へ生成した。

**制約・次:** veilは現状、front/hazeから作る画像空間の広い加算成分であり、空気散乱や実レンズの校正値ではない。portrait plate一枚だけでは美的・一般化を決められず、動画連続性もこのturnでは測定していない。次は同一の連続素材でveil gain 0/.00625/.0125/.025を比較し、移動補償指標、固定ROIの変化、人物と背景の保持を計る。CC0動画を用いたpreview snapshotは従来presetとveil gain .0125 presetの双方から生成できた。対話UIでのtrackbar実操作とPキー保存往復は未確認。

## 2026-11-10 — Source-gated scatter before PSF spread

**変更:** `scatter_front_coupling`を静止画CLI/対話sandbox、動画renderer、対話video preview、preset schemaへ通した。従来のcoupling 1は作成済みbloom画像をfront maskで切っていたため、画像光学上は不自然な境界になりうる。実装を修正し、coupling 1では局所flash energyでhighlight sourceをweightしてから同じGaussian-mixture PSFへ通す。これにより光源のscatterは前線より先へ柔らかく広がる。0は既存global bloomと同一、0.5は両方式を線形混合する。既定値は0のまま。

**動画比較:** CC0 walkway 60 frameへstrength .8、scatter 3.0、veil gain .00625、synthetic exposure -2 stops、p70–p95 EMA .4秒を適用。source-gated方式のcoupling 0/.5/1に対し、平均flash linear-luma増分は0.05296/0.04239/0.03178、明るくなった画素率86.36/81.70/63.84%、newly clipped channel率は全て0%。motion-compensated flash-contribution delta平均は0.005355/0.004294/0.003310、最大は0.020426/0.017579/0.014641。前線連動は未到達のハイライト源からの散乱を抑え、出力光量とその時間変化を減らした。比較画像は`captures/scatter-front-source-gated-rd1/scatter-front-source-gated-comparison.png`。

**ポートレート確認:** 1536×1024 plateでstrength .8、scatter 3.0、veil .00625、highlight threshold .06/range .22を6時点で比較。coupling 1の平均luma増分はprogress .18/.32/.48/.64/.80/.94で0.00067/0.00242/0.00497/0.00836/0.01060/0.00756。progress .64ではglobal coupling 0の0.01102に対し約24%低く、newly clipped channelは0.0043%。接触sheetではsource gatingが未到達の街灯・路面反射からの先行fogを抑えるが、全体の絵の差は控えめ。capture/metricsは`captures/scatter-front-portrait-rd2`。

**暫定判断・制約:** source-before-PSF方式は旧post-bloom maskより光の広がりとして筋が通るが、どの coupling 値が美的に正しいかは素材・光路次第で、全て画像空間の近似。PSFは実レンズ計測でなく、遮蔽・奥行き・sensor bloomも再現しない。結び付け度はpreview knobとして残し、既定化は保留する。生成画像では連続60frame評価を実行したが、対話UIでの手操作・preset roundtripは未実施。次は人物plateのveilをさらに控えた条件と、孤立した点光源のある動画でflare spillを見て、前線境界からの散乱幅と光量を調整する。

## 2026-11-10 — Full-resolution terminal fill and pulse hold

**方法:** decomposition toolをstrength、highlight threshold/range、source-gated scatter、global fill、front duration、highlight rolloff、base veil gainをCLIで指定できるようにした。GlowLab portrait plate 1536×1024でradial source (.08,.5)、scatter 3、coupling 1、threshold .06/range .22、rolloff .78を使い、progress .60/.72/.84/.96を比較。strength 2.4ではveil gains 0/.003125/.00625/.0125/.025、strength 4.8では0/.0015625/.003125/.00625/.0125/.025をprogress .72で比較。両者の連番でframes=9 / 8fps / 1秒とし、peak .78のdecayとpeak 1.0のholdを比べた。capturesは`captures/final-fill-highres-rd1`、`final-fill-strength48-rd1`、`final-fill-strength48-progression-rd1`、`final-fill-strength48-hold-rd1`。

**結果:** strength2.4 / veil .00625のprogress .72でsource-plus-exposure / scatter / veilのmean linear-luma deltaは.00853/.00174/.01955、combined result .02974、p95 .0828、new clip channel .0361%。同じ光量積となるstrength4.8 / veil .003125ではdirect exposure .02366、scatter .00174、veil .01955、combined .04475、p95 .1272、new clip .0973%。strength4.8 / veil .0015625ではcombined mean delta .03498、p95 .1147、new clip .0950%。veil .00625では.06429/.1528で、見た目に広いfogが目立つ。強めのdirect exposureとveilを別調整すると、均一fogを増やさず局所光量を保てる候補が得られた。

**終端の時間応答:** strength4.8 / veil .0015625 / peak .78 sequenceのmean linear-luma deltaはprogress .75で.03705、.875で.02863、1.0で.01960と、光が全体へ届いた後に暗く戻る。peak 1.0へ移したhold sequenceでは.75で.03705、.875で.03918、1.0で.03918。終端のp95は.1260、new clipは.1178%で一定。接触sheetでは全域に光が届いた状態を最後まで保持し、通常peak .78より「塗りつぶし完了」が読みやすい。これはpulse-tailを選ぶ時間デザインであり、一発の自然な閃光の減衰を模擬する場合はpeak .78の方が合う素材もある。

**暫定判断・制約:**静止plate上ではstrength4.8 + veil .00156–.003125 + source-gated scatter + shoulder .78を、局所ディテールを保つstrong-fill候補とする。終端保持は明示的なlook候補であり、default化しない。plateは校正済み実カメラ素材ではなく、depth・shadow・色管理・HDRを検証していない。高解像度静止画の目視/数値と9-frame sequenceだけで、連続動画、実時間UI操作、動き補償評価はない。次はこの候補をCC0連続素材と人物/点光源を含む別素材で比較し、光量を保ったまま局所コントラスト、subject motion、終端のhold/decayを分けて判断する。

## 2026-11-10 — Veil gain across a continuous moving shot

**方法:** `flash_video_lab.py`へ`--veil-gain`を追加し、CC0 walkwayの全60 frame / 3.93秒 / 15fpsを同一設定で4回処理した。出力を240px、radial source (.50,.14)、strength .8、scatter .6、highlight p70–p95 EMA tau .4秒、synthetic exposure -2 stopsに固定し、veil gain 0/.00625/.0125/.025を比較。motion-compensated metricsをFarnebackで計測し、各条件の代表frameを`veil-video-gain-comparison.png`へ合成した。各runのmetricsは`captures/veil-video-gain-*`。

**結果:** veil gain順に、60frame平均flash-contribution mean linear-luma deltaは0.01592/0.01891/0.02190/0.02788、平均brightened pixel fractionは65.27/72.52/75.20/78.33%。平均motion-compensated flash-contribution deltaは0.001606/0.001803/0.002010/0.002438、最大値は0.006770/0.007994/0.009241/0.011763。すべて新規clipped-channel率0%。gain .0125はveilなしより平均光量を約37.5%上げ、動き補償後のflash差分を約25.2%上げた。gain .025は同じ指標を約75.2%/51.8%上げた。接触sheetでは前線が進む明るさを保ちながら、.025は雪面と橋桁の局所コントラストを最も強く洗う。

**判断・制約:**この低key化された単一CC0ショットでは0.00625–0.0125を控えめな比較候補、.025を強いveil lookとして分けられる。動き補償差は意図したfrontの時間進行も含み、ちらつきだけの測定ではない。-2 stopsは一様linear減光でsensor noiseなし、240px出力、scene cutなし、静止ROIの評価なし。係数の汎用既定化は保留し、次は同一素材の別source positionと高key条件、カメラ/被写体運動のある素材で局所コントラスト・固定ROI変化を確認する。対話UIの連続操作は未確認。
## 2026-11-10 — Native-resolution continuous flash fill and endpoint hold

**方法:** CC0 walkwayを元解像度360×640、全60 frame / 15fps / 3.93秒で処理し、Farneback動き補償指標を記録した。第一比較はstrength 4.8 / scatter 3 / front coupling 1 / veil .0015625 / global fill .015 / source (.08,.5) / attack .78 / radial falloff 2.7 / rolloff .78 / frame p70–p95 threshold EMA .4秒で、peak .78 decayとpeak 1.0 hold、通常露出と合成-2 stopsを比較。第二比較は低key strength 1.2 / 2.0と通常露出 strength .8で、scatter 1.5 / veil .0005 / global fill .005 / peak 1.0を比較した。比較一覧は`captures/final-fill-video-native-rd1/comparison-overview.jpg`、各条件のJSONは両`final-fill-video-native-rd*`配下。

**結果:** strength 4.8は低keyでもhold終端mean linear-luma delta .2365、全60 frame平均の動き補償flash contribution delta .00967。decayは同じ終端で.02121まで落ち、平均.01061。source/output saturationの分離計測を追加した後、低key strength 4.8は終端で1.18%のchannelを新規飽和させ、全frame最大2.79%。通常露出ではhold終端mean delta .3418、source既存飽和0.50%に対し出力48.92%、新規飽和48.42%で、比較画像でも強い白飛びとなった。低key holdのstrength 1.2 / 2.0は終端mean delta .0567 / .0802、動き補償平均delta .00235 / .00346。strength 1.2の新規飽和は全frame 0%、strength 2.0は新JSONを参照する。通常露出strength .8は終端mean delta .1296、source既存飽和0.50%、新規飽和9.74%（全frame最大10.75%）。`clipped_channel_fraction`はabsolute output rateのまま残し、`source_clipped_channel_fraction`と`newly_clipped_channel_fraction`を別キーで出す。動き補償差分はfrontの進行を含むため、ちらつき量そのものではない。

**暫定判断・制約:**終端を保持するlookには`peak_time=1.0`が機能するが、静止plate向けstrength 4.8を動画へ流用すると光量過多になりやすい。低key側のstrength 1.2は飽和のない控えめな候補、2.0は新規クリップ指標を含めて条件別確認する候補として扱う。素材ごとにsource exposureと光量を合わせる。単一の雪面・橋梁ショットであり、人物・点光源・室内、深度遮蔽、カメラ由来flare、sensor noise、HDR/色管理は未検証。

## 2026-11-10 — Relative-depth model for subject-leading flash fill

**実装:** optional tool `flash_depth_estimate.py`を追加した。Depth Anything V2 Small int8 ONNXをrevision `4472b7362082ad9968fee890ca0f1e5aca36b93d`へpinし、Apache-2.0 model card、27,258,801-byte expected size、SHA-256を検証する。onnxruntime 1.31.1 / CPUExecutionProviderで1536×1024 portrait plateの推定は0.85–0.91秒。モデル本体は明示的な`--download-model`時だけLocalAppData cacheへ取得し、repoへweightsを複製しない。relative raw map、white=near PNG、color preview、metadataを保存する。

**確認:** model previewでは人物、手前の壁、足元の路面が明るい側、木々・遠景が暗い側となり、粗いGrabCut silhouetteより連続したscene depthが得られた。CC0 walkway frame 0/24/59でも空・遠景、橋、近景の路面/氷が順序付けられた。strength4.8のportrait plate flat対depth (.36)ではprogress .24の人物平均flash deltaが.00239→.00542、背景が.00412→.00445。progress .32は人物.00940→.01048、背景.00674のまま。progress .44では人物値がflatとほぼ収束し、遠景を含む背景は.01286→.01166。終端のdepth new clipped-channel fractionは0.0357%。比較画像は`captures/depth-anything-portrait-rd1/flash-fill/flat-vs-depth-anything.jpg`、推定preview/metricsは同folder parentにある。

**暫定判断・制約:** image-spaceのradial falloffを実測metric light transportに置き換えるものではないが、実写内容に応じた人物先行と遠景遅れを手作業proxyより滑らかにart-directできる。relative depthはモデル推定であり、髪・暗い被写体・反射面の境界を含む。CC0動画は3 sampled frameのみで、60-frame深度安定性・flow-warp temporal consistency・遮蔽は未検証。小型モデルの単画像CPU推論を確認した段階で、動画の最終 look 決定やdefault強度の採用はしない。
