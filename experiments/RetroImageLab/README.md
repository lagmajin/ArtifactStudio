# Retro Image Lab

**最終更新:** 2026-11-11

ArtifactStudio本体とは独立して、静止画向けレトロ画像表現を試すPythonラボです。ここで得た見た目や知見を本体へ移す作業は別途判断します。

## 初期テーマ

- パレット削減と色数制限
- ordered dithering（Bayer）
- scanline / CRT風の明暗変調
- RGB channel offset
- 印刷物／紙の微細な粒状感
- 固定パレット（Game Boy風 / CGA風 / PICO-8風）と適応パレットの比較

まずは各処理を個別に評価できる状態を作り、最後に組み合わせます。元画像は変更せず、結果は `output/` に保存します。

## セットアップ

Python 3.10以降を用意し、このディレクトリで実行します。

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

## 実行

```powershell
python retro_lab.py input.png
python retro_lab.py input.png --preset handheld --scale 0.5
python retro_lab.py input.png --preset print --colors 12 --dither 0.7
python retro_lab.py input.png --preset print --research-sheet
python retro_lab.py input.png --alpha-study
python retro_lab.py fixtures/color-space-fixture.png --alpha-chromatic-study
python retro_lab.py fixtures/color-space-fixture.png --alpha-photo-sweep
python retro_lab.py fixtures/color-space-fixture.png --production-chromatic-alpha-study
python retro_lab.py fixtures/color-space-fixture.png --color-space-study
python retro_lab.py fixtures/color-space-fixture.png --palette-domain-study
python retro_lab.py fixtures/color-space-fixture.png --quantization-banding-study
python retro_lab.py fixtures/color-space-fixture.png --natural-quantization-study
python retro_lab.py fixtures/color-space-fixture.png --dither-pattern-study
python retro_lab.py fixtures/color-space-fixture.png --temporal-dither-study
python retro_lab.py fixtures/chromatic-fixture.png --chromatic-study
python retro_lab.py fixtures/radial-chromatic-fixture.png --radial-chromatic-study
python retro_lab.py fixtures/grain-fixture.png --grain-study
python retro_lab.py fixtures/color-space-fixture.png --pipeline-study
python retro_lab.py fixtures/color-space-fixture.png --crop-phase-study
python retro_lab.py fixtures/color-space-fixture.png --crop-resize-phase-study --output output/crop-resize-phase.png
python retro_lab.py fixtures/color-space-fixture.png --opencv-crop-resize-study --output output/opencv-crop.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-pixel-art-crop-study --output output/alpha-pixel-art.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-resize-strategy-study --seed 37 --output output/alpha-resize-strategy.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-resize-multicolor-study --seed 53 --output output/alpha-resize-multicolor.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-resize-gap-scale-study --seed 71 --output output/alpha-resize-gap-scale.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-hidden-stage-study --seed 83 --output output/alpha-hidden-stage.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-chromatic-offset-sweep --seed 97 --output output/alpha-chromatic-offset.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-resize-real-corpus eval-inputs/cc0-white-clover --seed 109 --output output/alpha-resize-real.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-hidden-source-residual eval-inputs/cc0-white-clover --seed 127 --output output/alpha-hidden-source.png
python retro_lab.py fixtures/color-space-fixture.png --core-chromatic-alpha-model eval-inputs/cc0-white-clover --output output/core-chromatic-alpha.png
python tools/full_suite_cpu_cost_study.py
python tools/full_suite_tonal_response_study.py
python tools/full_suite_photo_tone_study.py
python tools/full_suite_photo_resolution_study.py
python tools/full_suite_photo_stage_isolation_study.py
python tools/full_suite_photo_display_scale_hypothesis_study.py
python tools/full_suite_photo_grain_factor_study.py
python tools/full_suite_photo_grain_roi_interaction_study.py
python tools/full_suite_photo_grain_seed_variance_study.py
python retro_lab.py fixtures/color-space-fixture.png --rasterizer-chromatic-alpha-model eval-inputs/cc0-white-clover --output output/rasterizer-chromatic-alpha.png
python retro_lab.py fixtures/color-space-fixture.png --product-dither-source-audit
python retro_lab.py fixtures/color-space-fixture.png --suite-corpus eval-inputs --corpus-dir eval-corpus --output output/suite-corpus.png
python retro_lab.py fixtures/color-space-fixture.png --suite-corpus eval-inputs --make-suite-fixture --corpus-dir eval-corpus --output output/suite-corpus.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-hidden-suite-study --output output/alpha-hidden.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-resize-suite-study --output output/alpha-resize.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-rgb-dilation-study --output output/alpha-dilation.png
python retro_lab.py fixtures/color-space-fixture.png --alpha-dilation-stress-study --output output/alpha-stress.png
python retro_lab.py fixtures/color-space-fixture.png --suite-detail-image tests/Artifact/fixtures/keying/voa-green-screen-frame-2s.jpg --suite-detail-box 300 195 200 250 --output output/voa-detail.png
python retro_lab.py fixtures/color-space-fixture.png --video-grain-frames eval-inputs/voa-sequence --output output/voa-temporal.png
python retro_lab.py fixtures/color-space-fixture.png --video-grain-space-frames eval-inputs/voa-sequence --output output/video-grain-space-study.png
python tools/full_suite_color_domain_study.py
python tools/palette_occupancy_transition_study.py
```

結果は `output/<入力名>_<プリセット>.png` に出力します。`--help` でオプションを確認できます。
`--research-sheet` を指定すると、Bayer 4×4 / 8×8、各強度、固定パレットの比較シートとRGB RMSEを同じ出力先に作成します。実験条件・観測値・限界は [RESEARCH_NOTES.md](RESEARCH_NOTES.md) に記録します。
`full_suite_color_domain_study.py` はprovenance manifestを検証した3点のCC0素材へ現行4 presetを適用し、resize／quantization／chromatic interpolation／scanline／grainのsRGBとlinear domainを個別および全体で比較します。結果は`captures/full-suite-color-domain/`に出力します。
`palette_occupancy_transition_study.py` はSHA-256検証済みCC0 cutout／feather／pixel-art素材3点と、license review caveat付きの内部専用VOA写真1点で、固定palette 3種、距離domain 2種、Bayer強度3段階のoccupancy、隣接色遷移、alpha composite後のsource errorを測ります。partial-alpha領域は黒・灰・白背景に対してlinear-light合成後にsRGB表示する場合とsRGB code値で直接合成する場合も比較します。PNG/JPEG color profileは記録しますが、現行suiteの解釈に合わせてsRGB code値として処理します。これはラボの式比較で、ArtifactStudioの実合成経路を評価しません。VOA由来の派生画像は再配布しません。結果は`captures/palette-occupancy-transitions/`に出力します。
`python tools/adaptive_palette_weighting_study.py` は同じCC0素材3系統に、visible-pixel頻度／alpha coverage重みとsRGB／linearの決定的median-cut palette (4/8/16色)を適用し、固定PICO-8との誤差・透明縁・接触シートを比較します。これは独立した研究用median-cut実装で、Pillow/Core/Artifactの量子化式ではありません。結果は`captures/adaptive-palette-weighting/`へ出力します。
`python tools/adaptive_palette_temporal_stability_study.py` はCC0素材3系統を重複cropでパンし、8色alpha-weighted paletteを各cropで再学習する方式とsource全体で共有する方式について、同一source座標のpalette出力変化を測ります。素材ごとにcrop数は異なります。動画runtimeではなく、palette更新だけによる時間差のsource-derived modelです。結果は`captures/adaptive-palette-temporal-stability/`です。
`python tools/adaptive_palette_temporal_smoothing_study.py` は前述のpan cropでper-frame palette、slot対応後のEMA 0.25/0.50、shot-shared paletteを比較し、重なり領域の色変化とmid-gray合成source errorを測ります。EMAは研究候補であり、動画runtime・知覚品質・GPU実装の評価ではありません。結果は`captures/adaptive-palette-temporal-smoothing/`です。
`python tools/adaptive_palette_domain_dither_study.py` は同じpan sequenceでEMA .25をsRGB/linearで補間し、Bayer 4×4のworld-locked source perturbation有無も比較します。dither amplitudeはpalette間隔に対するexploratory modelで、製品係数ではありません。結果は`captures/adaptive-palette-domain-dither/`です。
`python tools/adaptive_palette_dither_phase_sweep.py` はEMA .25に対するBayer振幅をpalette間隔の.25/.5/1.0倍で測り、source/world-lockedとcrop/screen-locked phaseの差を比較します。結果は`captures/adaptive-palette-dither-phase-sweep/`です。
`python tools/adaptive_palette_resize_phase_study.py` はCC0素材の水平panをscale 0.5/0.75でBOX縮小・nearest拡大し、crop-local palette、EMA .25、shot-shared paletteを比較します。scale周期にoriginを固定した場合とorigin位相を巡回させた場合を分け、同一world pixelの出力変化を測ります。結果は`captures/adaptive-palette-resize-phase/`です。

`python tools/adaptive_palette_alpha_resize_study.py` は同じ位相条件でstraight／premultiplied alpha resizeを比較し、palette temporal stabilityと透明境界のRGB差を黒・灰・白へのlinear-light合成で測ります。結果は`captures/adaptive-palette-alpha-resize/`です。

`python tools/alpha_resize_association_isolation_study.py` はpalette処理を除き、straight／premultiplied BOX縮小・nearest復元だけを測ります。元alpha保持と縮小alpha復元、phase固定／巡回panを比較し、背景合成後のedge errorと同一画素の変化をJSONへ出します。結果は`captures/alpha-resize-association-isolation/`です。

`python tools/alpha_resize_synthetic_edge_chart.py` は色・alpha・hidden RGBを制御したhard edge、曲線、細線、gapのfixtureを生成します。続けて `python tools/alpha_resize_synthetic_study.py` を実行すると、paletteなしでstraight/premultiplied RGBとsource/resized alphaの組合せを5 scale比較し、ROI別の合成誤差を記録します。

実験088は `ImageF32x4_RGBA::resize` と既存call siteのread-only source auditです。Coreのresize policyを記述しますが、実行・変更・品質判定は含みません。

`python tools/core_opencv_alpha_resize_model.py` はCoreの `cv::resize` default linear/channelwise式をfloat32配列でモデル化し、alpha-zero hidden RGB sensitivityをstraight/premultiplied、encoded/linear domainで比較します。Core runtime実測ではありません。

`python tools/grain_resolution_scale_study.py` は同一CC0 cropを256/512/1024pxで出力し、white grainのpixel固定と、1/2/4%画像幅で粒径を相対固定した条件を同じ投入sigmaで比較します。周波数・隣接相関・alpha合成誤差・contact sheetは`captures/grain-resolution-scale/`に保存します。

`python tools/grain_equal_display_strength_study.py` は同じ粒径条件のsigmaを探索し、linear-light Rec.709 luma差分RMSを0.015に揃えて空間スケールの違いを比較します。素材ごとの必要sigmaと可視画素のclipping率も出力します。

`python tools/grain_motion_coordinate_study.py` は32フレームの既知カメラpan・独立sprite移動・相互occlusionを持つ合成sceneで、grainがscreen / world-background / object-materialのどの座標に固定されるかを比較します。SHA-256検証済みCC0素材を使い、baselineとの差分からgrain residualだけを測定します。結果は`captures/grain-motion-coordinate/`に出力します。これは座標挙動の制御実験であり、製品runtimeやlookの選好評価ではありません。

`python tools/grain_temporal_correlation_study.py` はheld、AR(1)時間相関、毎frame独立、8-frame周期refreshを120 synthetic framesで比較します。matched field varianceのlag別residual MAE/correlationとcontact sheetを`captures/grain-temporal-correlation/`へ出力します。数値は時間挙動の記述で、知覚品質の合否ではありません。

`python tools/grain_quantizer_interaction_study.py` は、suiteのquantization→grain順とgrain→quantization counterfactualを、CC0 cutout・feather・pixel-hair素材、print相当・4/8階調stress条件、Bayer 4×4の全16位相で比較します。palette distance、grain residual、輪郭差、alpha保持を測り、provenance付きJSONと比較sheetを`captures/grain-quantizer-interaction/`へ出力します。これは現行suiteの順序・位相感度の記述で、推奨lookや製品Core経路の判定ではありません。

`python tools/grain_pixelation_alpha_interaction_study.py` は、handheld/arcade/print相当の縮小率・quantize・scanline・grain設定で、現行pixelation後grainと縮小前grainをCC0 alpha cutout 2点で比較します。背景別edge合成差、effective grain分散/空間相関、clip、alpha保持を記録します。結果は`captures/grain-pixelation-alpha/`へ出力します。縮小前noiseが透明RGBからedgeへ混ざる影響も含むcounterfactualであり、製品品質判定ではありません。

`python tools/grain_hidden_rgb_interaction_study.py` は、同一CC0 cutoutのvisible RGBとalphaを固定し、alpha-zero RGBだけblack/magenta/greenへ置換します。grain-off、現suite順、resize前grainの各hidden-color sensitivityと、black/gray/white合成後のedge差を記録します。provenance付きJSON/sheetは`captures/grain-hidden-rgb/`へ出力します。人工hidden-color対照であり、実decoder値や製品方針の証拠ではありません。

`python tools/grain_bayer_masking_study.py` は、現suite順でBayer量子化のno-dither出力との差分templateを作り、grainを加えた残差中のcoherent Bayer energy shareを測ります。synthetic controls 2点とCC0 cutout 2点、3 palette条件、4 phase、5 sigma、16 seedを比較します。これは知覚的masking閾値ではなく、`captures/grain-bayer-masking/`へsignal decompositionとsheetを出力します。

`python tools/full_suite_resolution_sensitivity_study.py` は現行4 presetを128/256/512/1024pxで実行し、出力を共通256px表示へ正規化して比較します。synthetic detail chartとprovenance付きCC0素材3点、grain/aberration/scanline ablationを含み、normalized error・spatial spectrum・各effect差を`captures/full-suite-resolution/`へ出力します。256pxは比較基準であり品質の正解ではありません。

`python tools/full_suite_alpha_resolution_study.py` はCC0 alpha cutout 3点×4 preset×4解像度をstraight/premultiplied source resizeで実行し、suite出力と同一native-resolution入力をlinear-light合成後にedge/partial/whole別評価します。alpha保持と背景別差も記録し、`captures/full-suite-alpha-resolution/`へprovenance・metrics・2種sheetを出力します。Association結果はsRGB-code resize modelで、製品decoder contractやquality rankingではありません。

`python tools/full_suite_tonal_response_study.py` は既存manifestのSHA-256を検証したCC0 clover/feather/hair-fur 10点とsynthetic ramp controlを現行4 presetへ通します。quantizer-onlyとfull-suiteを分け、smooth/detail/alpha-edge regionsのRGB色数・luma分布・入力差、alpha合成差を記録します。thresholdもapproved targetもない記述研究で、品質順位・製品経路の一致は主張しません。結果は`captures/full-suite-tonal-response/`です。

`python tools/full_suite_photo_tone_study.py` はprovenance.jsonに固定した写真5点をSHA-256確認し、ICCをsRGBへ変換（未埋込はsRGB仮定）、EXIF向き補正後、長辺1024pxへ比例縮小して現行4 presetを比較します。低輝度・中間調・highlight・smooth/detail別にquantizer-only/full-suiteの色分布と入力差を記録し、source/output overviewと1:1 cropを`captures/full-suite-photo-tone/`へ保存します。被写体の権利確認が取れていないportraitは内部研究用途限定で、製品/demo・再配布には使いません。品質閾値、承認target、製品経路の評価ではありません。

`python tools/full_suite_photo_resolution_study.py` は同じ5写真・4 presetを長辺512/1024/2048pxで処理し、全結果を512px入力と同じ寸法へLanczos正規化して、入力縮小だけの差・suite出力の差・同一解像度でのeffect residualを分離します。seed 31111、各条件3回のPython CPU時間、shadow/midtone/highlight/smooth/detail別の差、texture descriptor、alpha passthroughをJSONへ記録し、aspect-preserving overviewとnormalized cropを`captures/full-suite-photo-resolution/`へ保存します。実際のpreview/export、GPU、memory、承認済みlookや品質閾値は評価しません。

`python tools/full_suite_photo_stage_isolation_study.py` は同じ5写真・4 preset・3解像度で、pixelation、quantization+dither、channel offset、scanline、grainを一段ずつ無効化したcounterfactualをfull suiteと比較します。同一解像度のablation差、各variant内のresolution drift、signed drift patternの変化を領域別JSONへ記録し、variantごとのoverviewとsubject cropを`captures/full-suite-photo-stage-isolation/`へ保存します。stageは非線形に相互作用するため、結果は加算可能な単独stage寄与や品質順位ではありません。Artifact/Core/GPU経路、性能、承認targetは評価しません。

`python tools/full_suite_photo_display_scale_hypothesis_study.py` は現行固定pixel suiteと、pixelation/Bayer周期/channel offset/scanline周期/grain粒度を512px表示座標へ個別または一括で追従させるPython式counterfactualを比較します。全variantが512pxでは現行suiteとRGB8完全一致しない限り処理を中止します。各解像度のdrift・signed drift patternを測り、overview/cropを`captures/full-suite-photo-display-scale/`へ保存します。512px referenceへの近さは解像度一貫性proxyで、画質や望ましいlookの判定ではなく、製品/Core実装でもありません。

`python tools/full_suite_photo_grain_factor_study.py` はgrain粒径（native pixel/512-display cell）、解像度間seed-field alignment（共通field/sizeごとfield）、sigma（nominal/長辺比補償）を因子分解し、他stageは実験105の512-coordinate mappingへ固定します。grainless controlとの差、残差スペクトル、隣接相関、clipping proxyと512px基準driftを保存します。全variantの512px RGB8 parity gateを通し、outputsは`captures/full-suite-photo-grain-factor/`へ記録します。sigma補償は探索条件であり、推奨値や品質基準ではありません。

`python tools/full_suite_photo_grain_roi_interaction_study.py` はgrain直前float RGBとnoiseを分け、実写5点・preset・grain variant・解像度ごとにsource shadow/highlight/smooth/detail ROIでclamp前low/high excursion、uniform preset-grid bin crossing、output endpoint比を集計します。grain加算式とdisplay-scaled model出力のRGB8一致、512px current-suite一致を確認し、metrics JSONを`captures/full-suite-photo-grain-roi/`へ保存します。bin crossingは後段色ずれ/scanlineを含む診断格子の変化率で、品質欠陥率ではありません。

`python tools/full_suite_photo_grain_seed_variance_study.py` は現行`apply_retro`を5写真×4 preset×512/2048px×16 deterministic seedsで実行します。同条件grainless出力との差、共通512px表示におけるseed間per-pixel std、tone/detail ROI別のseed spreadとendpoint occupancyを計測し、selected seedのoverview/detail cropsとmetricsを`captures/full-suite-photo-grain-seed-variance/`へ保存します。16個の固定seedは確率的confidence intervalではなく、still-imageのseed variationはtemporal flickerを証明しません。

`python tools/grain_bayer_masking_study.py` は、Bayer-dithered outputを同levelsのno-dither outputと対にし、その差分templateへのcoherent projectionがsuite grain追加でどう変わるかを4 source types・3 palette条件・4 Bayer位相・5 grain sigma・16 seedsで記述します。seed統計JSONとflat-field sheetは`captures/grain-bayer-masking/`です。coherent energy shareは信号分解proxyで、知覚maskingの閾値や品質判定ではありません。
`--alpha-study` は合成alpha境界の縮小、色ずれ、grainを比べる合成fixtureと比較画像を作ります。サンプルの比較画像は `captures/` にあります。
`--alpha-chromatic-study` は透明画素に残る隠れRGBを変えたとき、色チャンネルのbilinear shiftが変化する度合いをstraight/premultipliedの各samplingで比較し、3背景への合成とalpha edge MAEを出力します。
`--alpha-photo-sweep` は細かな色変化とアンチエイリアス輪郭を持つ合成foliage素材を生成し、shift量0.5 / 1.5 / 3.5 / 7pxで同じ比較を行います。生成した入力は `fixtures/photo-alpha-fixture.png` に保存されます。
`--production-chromatic-alpha-study` は現行Chromatic Aberrationのradial nearest/clamp座標式をfloat配列上でモデル化し、透明部のhidden RGB差が色channel移動と合成結果へ漏れる範囲を2 / 8 / 16pxで測ります。これはshader/production runtime実行ではなく、ソース式を写したalgorithm-level modelです。
`--color-space-study` は入力画像をsRGB code valueとlinear lightの両方で量子化・減光・固定palette化し、linear RGB RMSEを比較します。
`--palette-domain-study` はGame Boy風 / CGA風 / PICO-8風の固定paletteをsRGB距離またはlinear RGB距離で選び、Bayer dither強度0 / 0.5 / 1のRMSE・palette使用色数・no-dither時からの変更pixel率を比較します。
`--quantization-banding-study` は512×128のgray/color rampを4 / 8 / 16階調、sRGB / linear-light量子化、Bayer 4×4強度0 / 0.5 / 1で比較し、linear RGB RMSEとgray rampの同値run・逆行stepを測ります。探索用の代理指標であり、banding品質の合否判定ではありません。fixtureと比較画像は `fixtures/` と `captures/` に保存します。
`--natural-quantization-study` はseed固定で生成する丘陵fixture（滑らかな空、陰影、草の細線、小屋）を無処理baselineと比較します。8 / 16階調、sRGB / linear-light、Bayer 4×4強度0 / 0.5 / 1の全体galleryと小屋・草の2倍cropを保存し、linear RGB RMSE・明度gradient MAE・変更pixel率を出力します。合成素材の探索評価で、実写や製品品質の合否ではありません。
`--dither-pattern-study` は8階調に揃え、無処理source、no-dither、Bayer 4×4/8×8のstrength 0.5/1、Floyd–Steinberg誤差拡散をsRGB / linear domainで比較します。scene全体・空・丘境界・家と草の各regionのlinear RGB RMSEとluma gradient MAEに加え、全体galleryと家・草の3倍cropを出力します。Floyd–Steinbergは逐次CPUの研究用referenceであり、production性能を示しません。
`--temporal-dither-study` は同じ丘陵sceneを1pxずつ8フレーム平行移動し、sRGB 8階調でno-dither、Bayer 4×4/8×8の画面固定／素材追従phase、毎frame再計算するFloyd–Steinbergを比較します。既知の移動を位置合わせして残るframe-to-frame MAEとframe 0からのdriftをlinear RGBで測り、全体のframe stripと家・草のcrop stripを保存します。これは整数pxの合成still sequenceであり、実アプリの時間安定性や動画品質評価ではありません。
`--chromatic-study` は入力画像で色ずれのnearest / bilinear、subpixel閾値、端処理、sRGB / linear補間を比較します。
`--radial-chromatic-study` はradial offset場、center位置、固定方向shift、nearest / bilinear、transfer domainを比較します。
`--grain-study` は一定量／midtone weighting／Core FilmGrain式、mono/RGB noise、粗さ、seed固定／変更時の空間・時間差を計測します。
`--pipeline-study` はpixelate、Bayer、scanline、grainの適用順とBayer／scanline位相を比較します。
`--crop-phase-study` はodd output寸法をcropへ分け、各cropでphaseをresetする場合とframe originを引き継ぐ場合をscale比別に比較します。
`--crop-resize-phase-study` は連続値のsynthetic fieldをscale 0.5/0.75、odd/even crop寸法で独立縮小し、全体縮小baselineとのBayer/scanline位相差とresize kernel差を分離して測ります。人間による製品品質判定やCore/GPU parityではありません。
`--opencv-crop-resize-study` はCore CPUのfloat image `cv::resize`に寄せたOpenCV `INTER_AREA` / `INTER_LINEAR` downscaleと`INTER_NEAREST` upscaleを人工RGBA chartで比較し、global/crop-local resizeとBayer/scanline phase、resize-only差を別々に記録します。CPU算法モデルでありCore runtime/API/GPU parityや品質合否ではありません。
`--alpha-pixel-art-crop-study` は決定的に生成するhard/soft edge、細線、hidden RGB付きRGBA pixel-art chartをOpenCVでpixelateし、global resizeとcrop-local resize＋phase補正を3背景合成後のwhole/edge差で比較します。縮小後alphaは入力alphaを保持し、実Core出力や画質合否の証明とは区別します。
`--alpha-resize-strategy-study` は同じ再現chartへ実lab presetの縮小処理を適用し、straight / nearest-visible RGB dilation / mean dilation / premultiplied RGB縮小を比較します。straight baselineからの縁変化とhidden RGB pairへの感度を3背景合成で測り、透明pixelの残留RGBも記録します。
`--alpha-resize-multicolor-study` は複数色の形、1/2/4/8px透明gap、細い半透明strandの固定chartを生成し、同じ4方式を4 presetすべてで比較します。縮小のないprint/vhsは対照条件となり、edge/gapのhidden RGB感度、straightからのRGB差、alpha保持、3背景合成を記録します。これはsynthetic controlであり、実写やCore/GPU品質の証明ではありません。
`--alpha-resize-gap-scale-study` は最終preset処理を除き、BOX縮小→NEAREST復元だけを0.25/0.375/0.5/0.75/1.0で比較し、全4 gap ROIとpartial-alpha edgeのhidden RGB差を測ります。1px-gapおよびstrand領域の2倍拡大cropを同倍率・中間灰色背景で保存し、filter単独効果とpreset後の効果を区別します。
`--alpha-hidden-stage-study` はscale1のprint/vhsに量子化、channel offset、scanline、grainを順に加え、hidden RGB pairによるedge composite差がどの段で現れるか3背景で追跡します。stage prefixの比較でありGPU/runtimeまたは他素材への一般化ではありません。
`--alpha-chromatic-offset-sweep` はVHS相当の量子化後、signed channel-offset量0/0.25/0.5/1/2/4pxを比較し、alpha=0のhidden colorがedgeへ漏れる値とzero-offsetからのvisible edge変化を測ります。1px gapとstrand cropを可視/hidden-pairで同倍率表示します。
`--alpha-resize-real-corpus` はprovenance.jsonにsourceと利用条件を持つPNG RGBA素材を対象に、handheld/arcadeの4 resize policyを比較します。alpha edge ROIに限定したstraight差・hidden RGB pair composite差・alpha exactnessを黒/灰/白で記録し、midgrayの比較sheetを生成します。suite corpus runnerのwhole-image RGB RMSEは透明pixelを含むためedge品質指標と区別します。
`--alpha-hidden-source-residual` はCC0 clover pack中の`vegetation_clover_02a.png`で、元の透明部RGBとalpha=0を黒へ正規化した入力を比較します。透明padding全体・alpha境界から8px以内・8pxより遠い領域だけ変更した3 pairを実4 preset/4 resize policyに通し、どの距離のhidden RGBがvisible edgeへ影響するか記録します。
`--core-chromatic-alpha-model` はCore `ChromaticAberrationEffect.cpp`の整数round、Red/Blue逆向きシフト、clamp式をsourceから写したPython CPUモデルです。CC0素材とsynthetic chartでhidden-RGB pair差を計測し、fully-transparent sampleのRGBを0へ抑えた参考変種と比較します。C++/GPU runtime parityやアルファモードの採用根拠ではありません。
`--rasterizer-chromatic-alpha-model` はArtifact Rasterizerの中心基準radial RGB sample座標（CPU/HLSL共通形）をsourceから写し、hidden-RGB pair感度とalpha=0 sample抑制のvisible deltaを測ります。実GPU/CPUコードの実行、CPU/GPU parity、import時alpha associationの検証ではありません。
`--alpha-hidden-distance-study` はprovenance付きCC0 clover PNGの保存RGBAを、alpha=0画素の可視supportからの距離ごとに集計・可視化します。Pillow decode値の解析であり、OpenImageIOやArtifactの実ロード値ではありません。
`--rasterizer-chromatic-transfer-domain` はICC付きCC0 cloverの同じRGBAをsRGB code valueまたはsRGB transfer decode後のlinear-lightでradial samplerへ通し、表示合成後のhidden-RGB感度とsuppression候補の変化を比較します。Artifact実ローダーの色管理を再現するものではありません。
`--image-layer-rasterizer-boundary` は still-image Rasterizer 経路の `ImageF32x4_RGBA::toQImage()` 境界でalpha=0 RGBを消す処理をsource-derivedにモデル化し、raw-source counterfactualとのリーク差と半透明sample比率を比較します。C++/GPU runtimeは実行しません。
`--rasterizer-alpha-sample-policy` は固定のmulticolor alpha chartで、現行radial RGB sample、alpha=0時の出力位置フォールバック、半透明も含む非opaqueフォールバック、sample-alpha coverage blendを比較します。R2/4/8、gap/strand ROI、alpha category、black/white合成を記録します。候補は参考モデルです。
`--natural-alpha-sample-policy <corpus-dir>` は `provenance.json` に列挙したPNG/JPEG/WebPを個別に、同じalpha sample方針・R2/4/8設定で比較します。各入力の `source_sha256` が記録されていれば実行時に照合します。サンプル分類、edge/partial-edge差分、接触シートを出し、素材間の平均に集約しません。既存コーパスは `eval-inputs/cc0-white-clover` と `eval-inputs/cc0-feathers` です。
`--natural-alpha-sample-detail <corpus-dir> --natural-alpha-detail-asset feather_silver.png --natural-alpha-detail-box 130 120 220 220` は、全画像座標でR8/B8のサンプル場を計算してから入力pixel座標のROIを切り出し、現行/候補を黒/灰/白背景で1:1表示します。ROIは素材ごとに指定し、既定値は銀色羽根用です。
`--crop-resize-phase-study` は連続値のsynthetic fieldをscale 0.5/0.75、odd/even crop寸法で独立縮小し、全体縮小baselineとのBayer/scanline位相差とresize kernel差を分離して測ります。人間による製品品質判定やCore/GPU parityではありません。
`--product-dither-source-audit` は製品DitheringEffectのCPU式、Composition向けresident HLSL、standalone GPU adapter HLSLを同一fixtureと設定でソース式からモデル化して比較します。C++/GPU実行やruntime parityの検証ではありません。
`--suite-corpus <directory>` はユーザーが用意したPNG/JPEG/TIFF/WebP素材に固定4プリセットを適用し、baselineとの横並びcontact sheet、簡易RGB指標、入力SHA-256・寸法・alpha比率を記録します。入力画像は `--corpus-dir` へハッシュ名でコピーします。`--corpus-provenance provenance.json` は `{ "image.png": { "source_url_or_origin": "...", "license_or_permission": "..." } }` 形式で由来・利用条件を記録できます。指定がない項目は `unknown` と表示します。取得・downloadは行いません。
`--texture-frequency-image <image> --texture-frequency-box X Y W H` は指定ROIと追加2領域のbaseline/4 presetを比較します。linear-luminance FFTの`rfft2`片側面をHermitian重みで全周波数面へ補正し、radial band power、directional anisotropy、variance、gradient energy、固定列run、推定strand軸へROI maskを回転したrunをJSONへ保存します。baseline固定gradient thresholdに加え、grainの誤検出を観察するper-image P90 threshold ablationも記録します。linear RGB上の2軸 opponent chromaについてもhigh-band power、anisotropy、source baselineとの差を測ります。どのrun値もhair strand追跡や品質合否を証明する指標ではなく、chroma軸も知覚均等色差ではありません。既知の縦/横/斜め線コントロールは`fixtures/directional-strand-control.png`とJSONにあります。
`python tools/multicolor_strand_study.py` は6色/交差/半透明strand chartに加え、線幅1/2/3/4px、pitch 6/10/16px、pixel phase 4条件の幾何学chartも生成します。線maskと背景を分けたlinear-luminance RMSE、contrast retention、位相rangeを測定します。各チャートの統計はeffect差の記述用で、知覚均等色差や品質スコアではありません。出力は`captures/multicolor-strand/`と`captures/strand-geometry/`です。

`python tools/hair_roi_orientation_study.py` は provenance 記録済みCC0 hair texture 2点の既定ROIを128²/256²で比較し、baseline方向からの軸ずれ、方向異方性、高周波比率を測ります。ROI-levelの概略方向ラベルであり、個々の毛筋を追跡する精度や品質判定を主張しません。数値と4枚のcontact sheetは`captures/hair-frequency/manual-roi-orientation/`です。

`python tools/hair_cutout_alpha_study.py` はCC0 hair sprite atlasの個別64² cellを別corpusとして切り出し、radial chromatic samplerのalpha policyをR/B 2/4/8pxで比較します。atlas全体ではsampler centerが各spriteと一致しないため、対象セルを分離して評価します。由来・crop座標・hashは`eval-inputs/cc0-hair-cell-study/provenance.json`、出力は`captures/hair-frequency/hair-cell-alpha-*`です。

`python tools/core_chromatic_angle_study.py` はCoreのR/B integer-shift source equationsをamount 0–50px、全角度1°刻みで解析し、vector quantization、direction error、edge clamp率、alpha/hidden-RGB応答を記録します。これはC++実行やGPU parityではありません。manifestとsheet/metricsは`fixtures/core-chromatic-angle-sweep.*`と`captures/core-chromatic-angle/`です。
同じstudyのallocation modelはframe-sizeごとの2 full-frame `vector<float>`容量とsource-level trafficを計算します。実機時間を測ったものではありません。Coreには丸め後shift vectorが(0,0)の場合のno-op returnを追加し、2 vector確保と全pixel処理を避けます。
`--make-suite-fixture` はライセンス不要の人工RGB/alpha fixtureをcorpus入力先へ作成します。実写画像の代替ではなく、再現可能なsynthetic controlです。
`--alpha-hidden-suite-study` は可視部分が同一で完全透明部分のRGBだけが異なる2入力を作り、現行4プリセット（実際の`apply_retro`）を通した後、黒・白・中間灰色背景に合成して輪郭への漏れを測ります。
`--alpha-resize-suite-study` は縮小するhandheld/arcadeについてstraight RGB縮小とalpha-premultiplied RGB縮小を比較し、同じ量子化前処理のedge誤差と、実preset出力を黒・白背景へ合成した差を記録します。
`--alpha-rgb-dilation-study` は完全透明画素のRGBを半径8px内の最近傍opaque色、または8回平均伝播で埋めてから縮小し、straight／premultiplied候補とhidden RGB感度、straight baselineからのedge RGB変化を比較します。両dilationは探索用の固定条件で品質推奨値ではありません。
`--alpha-dilation-stress-study` は赤/青の隣接形状と髪状の細線を1/2/4/8pxの透明gapで分け、hidden RGB色を変えたstraight/nearest dilation結果のedge・gap合成差を測ります。
`--suite-detail-image` と `--suite-detail-box X Y W H` は指定した実画像ROIをbaselineと4つの実presetで同倍率比較し、ROI RGB RMSEと変更pixel比を出力します。sourceは読み取りのみです。
`--video-grain-frames` は連番PNGに実presetを適用し、grainなし同一frame出力との差分をgrain成分として分離します。seed固定とframeごとにseedを変える場合の隣接frame grain residualをwhole/静止green-screen/hair ROIで比較します。入力動画の取得・frame抽出はこのrunnerの範囲外です。
`--video-grain-space-frames` は連続frameからFarneback optical flowを推定し、screen-fixed grainと素材座標へ追従させるgrainを比較します。preset出力のgrain残差は8-bit量子化段差で消える場合があるため、coordinate policyの数値比較は生grainのdiagnostic fieldで行い、presetは画面参照として出力します。

量子化研究の入力fixtureは `fixtures/quantization-fixture.png` です。再現する場合:

```powershell
python retro_lab.py fixtures/quantization-fixture.png --preset print --research-sheet
```

色空間の比較には `fixtures/color-space-fixture.png` を使います:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --preset print --color-space-study
```

色ずれの比較には `fixtures/chromatic-fixture.png` を使います:

```powershell
python retro_lab.py fixtures/chromatic-fixture.png --preset print --chromatic-study
```

radial色ずれの比較には `fixtures/radial-chromatic-fixture.png` を使います:

```powershell
python retro_lab.py fixtures/radial-chromatic-fixture.png --preset print --radial-chromatic-study
```

粒状感の比較には `fixtures/grain-fixture.png` を使います:

```powershell
python retro_lab.py fixtures/grain-fixture.png --preset print --grain-study
```

retro effectの順序比較には `fixtures/color-space-fixture.png` を使います:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --preset print --pipeline-study
```

crop／tile位相の比較にも `fixtures/color-space-fixture.png` を使います:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --preset print --crop-phase-study
```

Core chromatic-aberration の角度量子化・画面端 clamp の source-derived study は、Artifact.exe を起動せずPythonモデルで再現できます:

```powershell
python tools/core_chromatic_angle_study.py
python tools/core_chromatic_boundary_study.py
python tools/core_chromatic_workspace_study.py
python tools/core_posterize_domain_study.py
python tools/core_oldtv_scanline_resolution_study.py
python tools/core_oldtv_temporal_sampling_study.py
python tools/core_oldtv_alpha_boundary_study.py
python tools/core_oldtv_natural_alpha_study.py
python tools/core_oldtv_layer_roundtrip_study.py
```

生成fixture、条件別JSON、contact sheetは各study名の `fixtures/` と `captures/` に出力されます。OldTV natural-alpha studyはprovenance manifestのSHA-256を検証したCC0 cutoutを使い、Core fringe-onlyとArtifact GPU HLSL式、straight/premultiplied両alpha表現、背景合成counterfactualを記録します。layer-roundtrip studyは5点のCC0 cutoutでstraight/premultiplied producer値を透明layer中間面とLayerToFloat式へ通すcounterfactualを測ります。GPU側はPython式モデルであり、製品実行・GPU品質・性能の計測ではありません。

## 独立性

- ArtifactStudioのCMake、Qt、サブモジュールに依存しません。
- Python依存はこのラボ専用の `requirements.txt` に限定します。
- 入力／出力と実験コードはこのディレクトリ内に置きます。
- 巨大な素材や生成物はGitへ追加せず、必要ならローカル素材を使います。


### Grain paired residual (experiment 109)

`python tools/full_suite_photo_grain_paired_residual_study.py` はSHA照合済み5写真×4 preset×512/2048入力×4 selected seedsの160出力を、同条件grain=0 baselineとpaired比較します。7 ROIごとのsigned linear-luma/RGB差分、RGB8 changed-pixel比、alpha保持を記録し、paired imagesと固定×8 signed residual heatmapsを`captures/full-suite-photo-grain-paired-residual/`へ保存します。高解像度出力は512px入力寸法へ縮小するため、seed間still画像の差分であってtemporal flickerや品質評価ではありません。Artifact.exe不要です。


### Grain resampling filter (experiment 110)

`python tools/full_suite_photo_grain_resample_filter_study.py` はSHA照合済み5写真×4 preset×512/1024/2048入力を選定4 seedsで処理し、512表示へ縮小するBOX/BILINEAR/BICUBIC/LANCZOSの差をgrain-off paired residual/ROI統計で比較します。overview/detail cropsとJSONを`captures/full-suite-photo-grain-resample-filter/`へ保存します。Pillow RGB8 encoded-value resampling上のsuite研究で、linear-light/product pathや知覚品質の順位を示しません。Artifact.exe不要です。


### Controlled grain resampling (experiment 111)

`python tools/grain_controlled_plane_resample_study.py` は一様linear-light planeにGaussian grainを加え、linear float、sRGB float、sRGB RGB8の3経路でBOX/BILINEAR/BICUBIC/LANCZOS縮小応答を比較します。8 deterministic seeds、複数base値/sigma、1024/2048→512条件を含み、metricsと×20 residual heatmapを`captures/grain-controlled-plane-resample/`へ出力します。synthetic kernel diagnosticで、製品filterの同等性や推奨画質を示しません。Artifact.exe不要です。


### Grain RGB8 quantization counterfactual (experiment 112)

`python tools/full_suite_photo_grain_quantization_counterfactual_study.py` は5写真×4 preset×1024/2048入力×BILINEAR/LANCZOS×4 seedsを測ります。suiteのpost-grain float replayがcurrent `apply_retro` RGB8と完全一致するparity gate後、最終RGB8 conversion前後のdisplay-space grain residualを7 ROI別に比較し、overview/detail cropと×64 interaction heatmapを`captures/full-suite-photo-grain-quantization-counterfactual/`へ保存します。Float経路は説明用counterfactualで、製品path/品質優位性を示しません。Artifact.exe不要です。


### Grain amount × resolution (experiment 113)

`python tools/full_suite_photo_grain_amount_resolution_study.py` は5写真×4 preset×512/1024/2048×grain sigma倍率0.5/1/1.5/2×4 seedsを固定条件で比較します。Nominal float replayと現行`apply_retro` RGB8の完全一致をgateし、同条件grain-offとの差を7 ROIでfloat/RGB8経路別に測定します。overview/detail sheetsとJSONは`captures/full-suite-photo-grain-amount-resolution/`に保存します。Sigma変更は研究counterfactualであり製品推奨値ではありません。Artifact.exe不要です。


### Grain motion policy (experiment 114)

`python tools/full_suite_photo_grain_motion_policy_study.py` はprovenance/SHA照合済み写真3点から24フレームの整数camera-panを作り、4 presetでscreen-fixed seed、frame-varying seed、source/world-locked Gaussian fieldを比較します。同一frame grain-offとの差分を線形light RGB/lumaで測り、隣接フレームをscreen座標と共通source座標で整列したRMS/P95/correlationを記録します。source-locked deltaのsuite RGB8 parity gate、contact sheetsとJSONは`captures/full-suite-photo-grain-motion-policy/`に保存します。source-lockedはPython実験counterfactualであり、runtime、知覚品質や製品仕様を決めません。Artifact.exe不要です。


### Grain motion at alpha edges (experiment 115)

`python tools/grain_motion_alpha_edge_study.py` はSHA-256検証済みRGBA clover/feather/hair素材を20 frameで移動し、現行4 presetのgrainをscene合成後へ適用するscreen-fixed/frame-varying方針と、合成前layer materialに適用する方針を同一frame grain-off対照と比較します。opaque interior、partial alpha、visible alpha edge、object exteriorを別領域にして、alpha保持、RGB8 changed ratio、linear-light residual統計、object座標の時間差を記録します。neutral/texture背景のcontact sheetsと2x edge crops、provenance付きJSONは`captures/grain-motion-alpha-edge/`に保存します。compositeはencoded-sRGB Pillow modelで、製品合成経路や優れた方針を示しません。Artifact.exe不要です。


### Grain color covariance (experiment 116)

`python tools/full_suite_photo_grain_color_covariance_study.py` はprovenance/SHA照合済み5写真、4 preset、512/2048入力、4 seedsで、現行common-channel mono noise、同sigmaのindependent RGB noise、Rec.709 weightsに対しsRGB code domainでluma-nullなchroma noiseを比較します。grain-off対照からlinear-light luma/chroma residualを全体と7 ROIで集計し、current mono replay RGB8を160条件でexact gateします。overview/detail sheetsとfinite JSONは`captures/full-suite-photo-grain-color-covariance/`へ保存します。変種は色方向のcounterfactualで、luma/chromaの知覚等価や製品推奨を示しません。Artifact.exe不要です。


### Equal-output-luma grain calibration (experiment 117)

`python tools/full_suite_photo_grain_equal_luma_calibration_study.py` は5写真×4 preset×512/2048入力で、current mono grainのwhole-image luma RMSを目標にindependent RGB/chroma-only variantのsigma倍率を訓練seed2個とfactor1/2/4の応答から補間します。別seed2個でluma一致度、chroma RMS、P95、RGB8端点率を検証し、current mono replay 160条件をexact gateします。全条件のcalibration/validation recordsと20 overview/detail sheetsを`captures/full-suite-photo-grain-equal-luma-calibration/`へ保存します。出力RMS一致は知覚等価を意味しません。Artifact.exe不要です。


### Chroma-only grain headroom frontier (experiment 118)

`python tools/full_suite_photo_grain_chroma_headroom_study.py` は実験117のfactor1/2/4訓練応答へfactor8を追加し、luma-null chroma noiseでmono luma RMSを達成できる範囲を調べます。訓練seedでfitし、別seed2個を同seed mono RMSへ対比。factor4固定との差、chroma residual、RGB8 endpoint channel率を記録します。overview/detail sheetsとJSONは`captures/full-suite-photo-grain-chroma-headroom/`です。8xは探索境界で、推奨値ではありません。Artifact.exe不要です。


### Grain pre-clamp attribution (experiment 119)

`python tools/full_suite_photo_grain_preclamp_attribution_study.py` は5写真×4 preset×512/2048入力×2 holdout seedsで、mono、出力luma校正RGB、chroma factor4、校正chromaを比較します。grain直前/直後のnative floatを読み、既存stage endpoint、新規の0未満/1超crossing、grain-off/on間のRGB8 endpoint新規/消失、native/common-display endpointを7 ROIで分離します。mono RGB8 parity 80条件、crossing map 20点、JSONを`captures/full-suite-photo-grain-preclamp-attribution/`に保存します。RGB8 endpointはgrain由来clipと同義ではありません。Artifact.exe不要です。


### Stage endpoint provenance (experiment 120)

`python tools/full_suite_photo_stage_endpoint_provenance_study.py` は同じprovenance/SHA照合済み実写5点×現行4 preset×512/1024/2048入力のgrain-off計60条件で、input、pixelation、quantization+dither、channel offset、scanline、clip、RGB8変換各段の0/1 float endpointとRGB8 0/255 endpointを集計します。stage間のendpoint新規/解除遷移、512入力のendpoint map、現行grain-off RGB8との60条件exact parityを記録し、JSON/sheetsを`captures/full-suite-photo-stage-endpoints/`に保存します。端点はpalette値として意図される場合もあり、clipping defectを意味しません。Artifact.exe不要です。
