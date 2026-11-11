# Image Light Effects R&D Notes

**最終更新:** 2026-11-11

## 2026-11-11 — Edge Bloom contour response and Reactive Glow saturation response

**方法:** Installed-effect CPU captures used the same 960×540 synthetic neon plate and matched Threshold (.65), Radius (8), amount/intensity (.35), and independent source pixels. Edge Bloom varied only Edge Boost (.15/.8); Reactive Glow varied only Saturation Weight (.1/.8), with Reaction .8 and Tint Mix .18 fixed. Four outputs, property readback, common-source hashes, and an aspect-preserving contact sheet are in `captures/glow-edge-reactive-role-probe-neon-cpu-rd3/`; profile: `glow_edge_reactive_role_probe_rd3.json`.

**結果:** Raising Edge Boost from .15 to .8 increased full-frame mean linear-luma change from .00106 to .00218 and newly clipped channels from .152% to .494%. Raising Reactive Saturation Weight from .1 to .8 increased the corresponding mean change from .000275 to .00155 and clipping from .025% to .451%. Alpha remained identical in all captures. At the contact-sheet's amplified difference scale, Edge Bloom reads as a more neutral contour/edge lift, while Reactive Glow adds more source-colored halo around the saturated ring, text, and tube. This is a role-separation signal on this fixture, not proof of aesthetic quality or transfer to photos.

**判断・制約:** The paired controls provide independent ways to tune contour response and saturation-driven color response, but the stronger settings also raise local clipping and no candidate is accepted. The captures use the installed CPU reference on an SDR synthetic plate, not the final Viewer/GPU path; HDR, photographic transfer, temporal behavior, and performance remain unverified. No production defaults changed.

**再現:** Run `glow_capture_product_suite.py` with `glow_edge_reactive_role_probe_rd3.json` and the profile's `captures/light-leak-neon-source.png` input, then generate the review with `glow_product_capture_review.py --capture-prefix edge-reactive-rd3-`.

## Scope and status

This note records the independent image captures and code-path findings for Glow and still-image Light Leak. Python/OpenCV studies remain SDR references and do not establish production GPU performance. ArtifactCore has analytic CPU still-image Light Leak fields for edge wash, anamorphic streak, and organic lobes, and Artifact has a corresponding resident-filter effect with normalized streak source X/Y and angle controls. The resident HLSL compiled as D3D12 DXIL and Vulkan 1.1 SPIR-V. The full `ArtifactUiTest` target builds in `out/build/x64-Debug-Agent`; D3D12 is disabled and Vulkan enabled in that configuration. Japanese GPU fixture captures now show successful blend dispatch and close pixel agreement with the installed-effect CPU reference after fixing the UV coordinate domain. The saved GPU image is cropped from diagnostic Accum RT; it is not the final Composition Viewer presentation. Final frame readback and visible Viewer output remain unresolved.

## 2026-11-11 — Chromatic Glow dispersion role on photographic practicals

**方法:** The installed Chromatic Glow CPU effect was captured on two CC0 photographs: a 1536×1025 aerial neon city and a 576×1280 paired street-light scene. Each plate used an identical source capture across its dispersion sweep and verified readback of the requested effect properties. An initial sweep used Tint Mix .65/1.0 and therefore attenuated or removed the shifted red/blue channels. Source inspection confirmed the property equation: at Tint Mix 1.0 the shifted samples are blended fully back to the unshifted green bloom. A Dispersion 0/1/2 Intensity control on a deterministic synthetic probe changed the installed CPU output strongly between Intensity 0 and 4, so the effect route itself was active. The corrected practical-light sweep fixed Tint Mix at 0, Threshold .62, Radius 24, Intensity .4, Angle 0 and compared Dispersion 0/1/2. A separate bulb-detail setting used Threshold .90 and Intensity .8. All captures, hashes, state readback, contact sheets and ROI measurements are stored in `captures/glow-chromatic-dispersion-cc0-city-cpu-rd{1,2,3,4}/`, `captures/glow-chromatic-dispersion-cc0-streetlight-cpu-rd{1,2}/`, `captures/glow-chromatic-bulb-detail-cpu-rd1/`, and `captures/glow-chromatic-dispersion-probe-cpu-rd1/`.

**結果:** With Tint Mix 1.0, changing Dispersion from 0 to 2 produced byte-identical product PNGs on both photographs and the synthetic probe; this is consistent with the property equation, not a broken effect route. With Tint Mix 0 and Radius 24, Dispersion 2 versus 0 changed the aerial-city full-frame RGB mean by .289 8-bit code and its manually selected intersection-practical ROI by 1.085 codes, with .599% newly clipped channels and alpha MAE 0. On the paired-bulb street-light photo, the corresponding difference was .073 full-frame code and .447 in the bulb ROI, with .061% new clipping and alpha MAE 0. Threshold .90 / Intensity .8 raised the bulb-ROI D2-vs-D0 change to .674 code while new clipping remained .071%. The color ring is clear in amplified difference sheets but remains subtle in the unamplified capture; the source already contains strong lens beams.

**判断・制約:** Chromatic Glow remains a distinct optional spectral role, but this round does not establish a visually strong or accepted photographic preset. Tint Mix interacts directly with Dispersion and can cancel its main visible feature; property wording/response deserves review before changing defaults. Measurements are 8-bit installed CPU reference captures with manual ROIs. GPU parity, final Viewer output, float-stack behavior, HDR, temporal stability, and performance remain unverified. Product code was not changed because the tested photo candidates were subtle and have not passed visual acceptance.

**再現:** Profiles: `glow_chromatic_dispersion_cc0_city_rd{1,2,3}.json`, `glow_chromatic_dispersion_practical_tint0_rd1.json`, `glow_chromatic_dispersion_cc0_streetlight_rd1.json`, and `glow_chromatic_dispersion_bulb_detail_rd1.json`. Generate the diagnostic source with `glow_chromatic_probe_plate.py`; run `glow_chromatic_dispersion_capture_review.py` with `--roi-profile city`, `streetlight`, or `probe` on each capture folder.

## 2026-11-11 — Reactive-spectrum recipe with retained channel dispersion

**方法:** The existing ordered `reactive-spectrum` recipe uses Reactive Glow then Chromatic Glow over the shared Glow foundation. After confirming the Tint Mix interaction above, `glow_suite_reactive_spectrum_chromatic_rd4.json` captured the same two-specialist order and foundation on the CC0 city plate with Chromatic Dispersion 2, Radius 12, Intensity .20, Tint Mix 0. This made the spectral delta more distinct but exceeded the preregistered local clipping gate: full-frame new clip .677%, central-intersection ROI 3.636%. A restraint round changed only Chromatic Intensity to .15 (`glow_suite_reactive_spectrum_chromatic_rd5.json`); it passed at .539% full and 2.903% max ROI, with alpha MAE 0. Both rounds' stage captures, source hashes, 1:1 ROI sheets and x5 differences are in `captures/glow-suite-reactive-spectrum-chromatic-rd{4,5}/review/`.

**判断・制約:** The lower-intensity recipe is a headroom-qualified exploration candidate, not an accepted look. In the 1:1 unamplified image the added color separation remains subtle, so the difference sheet is useful for locating the response but cannot establish aesthetic value. The capture is an ordered 8-bit PNG handoff, not a single float-intermediate stack or final Viewer output. GPU parity, HDR and timing are open.

## 2026-11-11 — Light Leak exposure transfer on portrait

**方法:** The same low-light portrait used in the suite was deterministically exposed by +2 stops in linear light. Its PNG was captured through the installed Still Light Leak CPU reference at strength .15 and .30, source (.01,.015), angle 2.58°, for Edge Wash, Streak, and Organic. Both strength runs used an identical fixture-input hash, exactly matching the exposure-transformed source pixel-for-pixel; each state sidecar identifies `compute=0` and `captureSource=Installed CPU reference`. `light_leak_exposure_transfer_review.py` compares linear-luma deltas in the existing manual face ROI, an upper-frame ROI, and a practical-context ROI, plus newly clipped channels and alpha. Evidence is in `captures/light-leak-portrait-plus2-app-cpu-rd1/`.

**結果:** All six captures preserve alpha exactly and add no clipped channel relative to the +2 stop input. Edge Wash spreads strongly into the face ROI: mean linear-luma delta .00869 at .15 and .01729 at .30, versus .00280 / .00577 in the practical-context ROI. At strength .30 the rendered sheet visibly warms much of the wall and subject, so this is a broad-wash stress response, not a portrait-safe candidate. Streak keeps its strongest visible signature along the top edge at this source placement; face deltas stay .00023 / .00050, while the practical ROI receives .00070 / .00141. Organic stays more localized and subdued, with face .00111 / .00232 and practical context .00177 / .00346.

**判断・制約:** On this brighter exposure, the tested .15 settings reduce spill but do not establish an accepted look. Edge Wash remains composition-wide, Streak depends on source placement, and Organic's contribution is subtle. The input is an exposure transform rather than another photograph; ROI masks are manually selected rectangles; measurements are SDR CPU reference output. GPU parity, final Viewer output, HDR behavior, temporal stability, and performance are not established.

**再現:** `python experiments/GlowLab/light_leak_exposure_transfer_review.py --input experiments/GlowLab/captures/glow-suite-portrait-exposure-rd1/portrait-plus2stop.png --captures experiments/GlowLab/captures/light-leak-portrait-plus2-app-cpu-rd1 --output experiments/GlowLab/captures/light-leak-portrait-plus2-app-cpu-rd1/review`

## 2026-11-11 — CC0 night-photo Light Leak review

**素材:** `assets/SOURCES.md` records two CC0 1.0 source plates and checksums: a 4496×3000 aerial neon-city photo by JINDONG H, proportionally reduced to 1536×1025 for the working capture, and a 576×1280 street-light photo by Reshmamohamed. Wikimedia Commons records both source licensing and authorship; the Unsplash image predates its 2017 license change.

**方法:** The independent field model rendered Edge Wash, Streak, and Organic at strength .35 on the city image, with the Streak source at (.93,.50), angle 2.6°, and additional angle checks at -8/-3/0/3/8°. The registered `still_light_leak` app fixture rendered all three looks through the installed CPU reference on both plates. The street-light capture used strength .25 and Streak source (.28,.30), aligned near the twin bulbs. Contact sheet and encoded RGB/alpha metrics are in `captures/light-leak-cc0-product-review-rd1/`; original product outputs, input copies, sidecars, and independent model captures remain in sibling files/directories.

**目視確認:** On the aerial city, Edge Wash creates a heavy warm upper-left veil at this strength; Streak adds a thin horizontal pink-white band through the center intersection, and Organic produces a broad purple/amber cast over much of the city. These are distinct signatures, but their current strength/field values are too assertive for a subtle default candidate on this image. On the vertical lamp photo, a source point placed near the bulbs creates a broad horizontal streak through both lamps and the pole; it competes with the real lamp rays. Edge Wash colors the upper-left sky, while Organic reads as a wide magenta wash. Source placement therefore needs both position and beam-shape/extent review; simply snapping a streak origin to a practical can make the effect less photographic.

**測定・制約:** All six installed-effect captures preserved alpha exactly (alpha MAE 0). Their source-to-output encoded RGB MAE ranged 12.12–24.40 on 8-bit RGB, indicating visible changes but not quality. CPU request and registered effect are recorded in state sidecars. This is CPU installed-effect output, not final Viewer or GPU parity. The independent model is not Artifact output. The two photos provide very different scene geometry but do not represent scanned film-leak references; no audience preference, broad camera/lens validity, transparent-edge behavior on these real photos, temporal stability, or per-frame cost was tested. No effect coefficients or product defaults were changed in this review.

## 2026-11-11 — Glow suite transfer to a CC0 aerial night photo

**方法:** `glow_suite_recipe_profiles_portrait.json`のGlow specialist 10枝をCC0 aerial neon-city imageへ同値で転用し、1つのGlow foundationから各枝を分岐してCPU product captureした。rd1はportrait向けfoundation gain 2.5のまま適用したため、交差点と道路のハイライトが大きく飽和した。比較土台を修正するため、rd2ではGlow gainをneon比較で使った1.0に変更し、他のbranch parameterは据え置いた。全枝が同一foundation pixelsを入力にしていることを検査し、交差点、明るい道路、赤色practical、暗い建物の4 ROIを1:1で比較した。画像、manifest、captureごとのstateとmetricsは`captures/glow-suite-recipes-cc0-neon-cpu-rd2/`、ROI contact sheetとJSONは`.../review/`。

**結果:** 全10枝のalpha MAEは0。foundationに対する全画面newly-clipped channel fractionはVolumetric .23%、Halation .29%、Caustics .39%、Reactive .67%、Edge 1.08%、Directional 1.45%、Optical 1.59%、Glint 1.61%、Chromatic 2.67%、Liquid 2.84%。特に交差点ROIではChromatic 8.63%、Liquid 9.08%まで上がるため、全画面指標だけでは見逃す。Halationは照明領域に広い赤成分を足し、Causticsは街路・建物境界へ細かいridgeを作る。Volumetricはこの1枚・source座標の設定では控えめで、存在感が弱い。ChromaticとLiquidは違いが見える一方、明るい舗装と車列に対し色分離・歪みが強く、portrait candidateをそのまま採用できない。

**判断・制約:** rd2の値は異なるGlow役割の候補を示すが、素材専用の強度としては未調整であり採用profileではない。rd1 captureは共通Glow foundation自体が強すぎるため評価対象から外す。これらはCPU installed-effect outputを8-bit PNGへ書き戻して枝に再入力する比較proxyであり、単一のfloat stack、Viewer/GPU parity、HDR headroom、実行時間やallocationは示さない。Residual Glowはstill画像で時間履歴を評価できないので除外。入力写真はCC0で、出典・加工・hashは`assets/SOURCES.md`に記録した。

## 2026-11-11 — Ordered multi-effect Glow suite recipes

**評価計画:** `SUITE_EVALUATION_PLAN.md`へordered stackの目的、対照、素材、4つの固定ROI、1%全画面 / 3% ROIの追加clip review gate、alpha条件、未検証経路を撮影前に追記した。CC0 aerial night photoではGlow foundation gain 0.10 / 0.25 / 0.50 / 1.00を撮り、gain 0.10で全画面新規clip .44%、交差点ROI 2.40%。さらに0.05を追加した結果は全画面 .25%、交差点1.42%で、後続レシピ用の共通基盤に選んだ。数値は254/255以上となった新規channel割合であり、HDR clipや美的品質の代替ではない。

**組合せ:** 同じGlow foundationから5つの順序付き2-effect recipeをCPU product referenceで撮影した。contour-streak = Edge Bloom → Directional Glow、reactive-spectrum = Reactive Glow → Chromatic Glow、film-optical = Physical Halation → Optical Glow、refractive-ridge = Liquid Glow → Luminescence Caustics、point-rays = Volumetric Shine → Glint Star。各段のPNG handoff、input/output hash、property、state、capture時間を`glow_suite_ordered_stack_capture.py`がmanifest化し、元画像 / 基盤 / 各段の全体・固定ROI contact sheetと、前段・基盤・原画像に対する測定を`glow_suite_ordered_stack_review.py`で保存した。設定は`glow_suite_ordered_stacks_cc0_neon_rd2.json`と`glow_suite_ordered_stacks_portrait_rd1.json`。

**結果 — CC0夜景:** gain 0.05基盤からの最終追加clipはcontour-streak全画面1.23% / 交差点ROI6.17%、reactive-spectrum .94% / 4.82%、film-optical .70% / 3.56%、refractive-ridge .70% / 3.45%、point-rays .52% / 2.62%。事前gateを満たしたのはpoint-raysだけ。各専門effectを重ねると道路交差点の強い光で局所clipが増え、full-frame平均ではその上昇を隠す。Point-raysはclip gateを満たすが、このplateでは点光源の視覚的な星形・ray役割が弱く、品質候補としては未合格。

**結果 — portrait stress plate:** AI生成portraitにportrait foundation gain 2.5を使った別captureでは、全recipeの全画面追加clipは0.05%以下、顔・街灯・shopfront・dark-coatの全ROIが3%以内で、face ROIとdark coatに新規clipはなかった。一方、specialist段のfull-frame encoded RGB MAEは概ね0.00002–0.00100で控えめ。光学差は見えるものの、point-raysは顔ROIで平均luma差0、streetlampでもVolumetric .00052 / Glint .00190と特に弱い。顔素材での光漏れを抑えつつ、効果ごとの役割が縮小表示や1:1 cropで明瞭かを追加確認する必要がある。

**判断・制約:** 「suite」としての互換性を単一の汎用Glow係数で説明できず、素材ごとに基盤gainとspecialistのheadroomが異なる。現roundではどのstackも見た目の最終採用には至らず、CC0夜景では4/5が局所clip gate超過、portraitでは効果署名が弱い組がある。captureは独立したEffect CPU reference出力を順に8-bit PNGへ再入力したstack proxyであり、1回のfloat effect stack、Composition Viewer、GPU parity、HDR、実写skin、時間安定性、steady-state性能を証明しない。Residual Glowは静止画stackに含めず、複数frameのhistory評価を別途要する。

## 2026-11-11 — Ordered Glow suite restraint and material transfer

**調整:** CC0 city rd2でclip review gateを超えた4組について、Glow foundation .05、effect順序、radius/shapeなどを固定し、出力を重ねるspecialistのIntensity/Amountを個別に下げたrd3を取得した。contour-streakはEdge Amount .20→.10 / Directional Intensity .18→.07、reactive-spectrumはReactive .15→.10 / Chromatic .20→.10、film-opticalはHalation .15→.10 / Optical .18→.10、refractive-ridgeはLiquid/ Caustics .15→.10。point-raysは対照recipeとして保持した。

**結果 — CC0 city:** 5/5 recipeが事前定義した追加clip review gate（full frame 1%、各ROI3%）を通過。foundationからの追加clipは全画面で.42–.55%、central intersectionで2.17–2.86%。alpha MAEは全て0。branchごとの変化はcity plateで残り、Chromatic/Reactiveはcrosswalkと道路エッジに色分離、Film/Opticalは広い薄い赤暖色拡散、Liquid/Causticsは道路面と境界に揺らぎ/細線を足す。ただしfull-frame contact sheetでは差が小さく、彩度・detailへの影響は等倍差分で見る必要がある。

**結果 — portrait:** portrait向けGlow gain2.5を使った同順序captureでは5組すべて全画面clip <.05%、全ROI clip <3%、顔とdark-coat ROIは新規clipゼロ。顔への平均luma差は全組で0.0005以下。一方、各specialist段のfull-frame encoded RGB MAEは概ね.00002–.00100で、point-raysのVolumetric Shineは .00002、Glintは .00006。clip抑制は成立したが、人物上でのrole visibilityは一部弱い。

**結果 — CC0 street-light photo:** foundation .05と同じ5 recipeを縦型576×1280写真へ転用し、ray sourceを左のbulbへ合わせた。5組ともfull-frame追加clip <.05%、ROI <.3%、alpha差0。元写真に既に強い光条が写っているため、新しいpoint-raysは光条を大きく伸ばさず、bulb周辺へ小さなring-like haloを追加した。街灯の光線形状が素材側にある場合は、Volumetric/Glintを重ねることで二重化する可能性がある。

**判断・制約:** intensity restraintは3種類のplateでclip増加を減らしたが、見た目の最終合否ではない。cityでは明瞭な署名と穏当なclipを両立する見込みがある一方、portraitのpoint-raysは弱く、街灯写真では既存rayとの重複を観察した。素材別にGlow foundation、effect role、source point、既存光条を組み合わせて選ぶ必要がある。記録は`glow_suite_ordered_stacks_cc0_neon_rd3.json`、`glow_suite_ordered_stacks_portrait_rd1.json`、`glow_suite_ordered_stacks_cc0_streetlight_rd1.json`および各`captures/glow-suite-ordered-stacks-*/review/`。このCPU PNG-chainでは単一float stack、Viewer/GPU、HDR、temporal/persistence、performance/allocationを検証していない。製品係数/既定値は変更していない。

## 2026-11-11 — Still Light Leak horizontal source placement

**方法:** 同じ低照度ポートレート上で登録済み`still_light_leak`のCPU出力を撮影し、streak source Xを.01/.25/.50/.75/.99へ移動した。source Y=.015、strength .35、angle 2.58°を固定。captureごとに同じfixture-input hash、`compute=0`、`captureSource=Installed CPU reference`のstate sidecarを確認した。既存の手動face ROI `(0.37, 0.08)–(0.49, 0.25)`と上部8%を使ってlinear-luma delta、新規clip、alpha差を計測した。contact sheetとmetricsは`captures/light-leak-placement-portrait-source-x-app-cpu-rd1/review/`、元captureは同名folderに保存した。

**結果:** face ROI / 上部の平均linear-luma delta比はX=.01/.25/.50/.75/.99で`.007/.221/.624/.019/.000`。顔ROIの平均ΔLは`.000716/.028923/.077363/.001996/.000002`。シートでもsource X=.50付近では発生源近傍の暖色haloが顔にかかり、左右端へ動かすと顔から離れる。5条件すべて全画面new-clipped-channel fraction 0%、alpha MAE 0。横位置は縦位置と独立に顔へのspillを大きく変えることが、登録済みCPU出力でも確認できた。

**判断・制約:** この写真では左上のX=.01–.25または右側X=.75–.99が顔を避ける配置候補で、X=.50は顔へのspillが大きい。ただし暗所ポートレート1枚と矩形ROIの結果であり、一般向け推奨/defaultにはしない。GPU、最終Viewer、別人物位置、動画中のsource追従は未評価。Streak source X/Yは既に製品propertyとして存在するため、今回は実装・製品値を変更していない。レビューscriptは`light_leak_source_x_review.py`。

## 2026-11-11 — Glow suite GPU role captures

**方法:** GPU対応登録8系統（Glow, Edge Bloom, Reactive Glow, Chromatic Glow, Liquid Glow, Directional Glow, Luminescence Caustics, Volumetric Shine）を同じ960×540 synthetic neon入力、各CPU role-candidateと同じproperty値、ja UIで既存`ArtifactUiTest`実行ファイルから撮影した。8件すべてstate sidecarは`compute=1`、`supportsGPU=1`、`captureSource=Accum RT`を記録し、source PNG hashはCPU captureと一致した。`gpuNodes`はspatial-node数であり、Liquid Glowは0、他7件は1。統合contact sheetとROI metricsは`captures/glow-suite-role-candidates-neon-gpu-rd1/review/`。

**結果:** 8系統それぞれでGPU可視出力を取得できた。CPU referenceとの比較proxyのfull-frame encoded RGB MAEは.0295–.0405、effect-delta linear-luma MAEは.0028–.0073、alpha MAEは0。発光文字ROIの1:1表示には一部の光学輪郭で位置/幅差が見られ、家族ごとのGPU出力確認は進んだが、数値上の差をそのままparity合否とは扱わない。

**capture経路の制約:** source inspectionでCPU出力は`applyToCpuSurface`の画像サイズ結果、GPU出力は`Accum RT`の`afterImage`からalpha非ゼロ範囲をcropし、`Qt::SmoothTransformation`でsource sizeへscaleした診断PNGと確認した（`Artifact/src/AppMain.cpp`のGlowSuite fixture）。したがって解像度は一致しても、GPU sideにはviewport preview crop / resamplingが含まれ、RGB/luma differenceは厳密なEffectバッファparityの測定にならない。Preview画像は役割が現れるかを見る探索証拠に限る。CPU専用のResidual Glow、Physical Halation、Optical Glow、Glint Starは今回のGPU capture set外。GPU timingはavailableでなく、性能根拠でもない。次は同じsource座標系のGPU effect output readbackが得られる経路を用意するか、preview crop transformを根拠付きで逆変換して比較する。製品効果値・既定値は変更していない。

**関連レビュー:** `glow_suite_gpu_suite_review.py`が8 captureのstate/source hashを検証し、全画面と5 neon ROIのCPU/GPU differenceをcontact sheet/JSON化する。`glow_suite_cpu_gpu_review.py`は単一effectの比較用。両者のCPU/GPU image metricsは診断値で、美的合否は1:1画像を別に確認する。

## 2026-11-11 — Glow suite evidence scorecard

**内容:** `glow_suite_scorecard.py`は登録12系統の共通role-candidate profile、4素材上の14 fixed ROI matrices、CPU alpha/clipping/responses、GPU宣言状態とdiagnostic capture有無を集約する。全12系統のCPU role evidenceはportrait / neon / CC0 walkway / high-key walkwayにあり、固定ROIごとのalpha MAEは0。GPU diagnostic captureは8系統を確認し、Residual Glow / Physical Halation / Optical Glow / Glint StarはCPU-only。CSVはneon local maximum clipping、portrait face ROI response、high-key walkway local maximum clipping、GPU node count、family-specific open gateを横並びにする。`captures/glow-suite-scorecard-rd1/`。

**読み方:** これは研究範囲と実装ゲートのindexであり、効果間の美的ランキングやpreset acceptanceではない。複数系統は共有の光源輪郭に反応するため、full-frame MAEや差分cosineだけで役割の重複を決めず、固定ROIの1:1 imageとmaterial transferを併用する。GPU diagnostic previewは正確なeffect-buffer parityではなく、全12候補の aesthetic acceptance も未完了。scorecardから得たclipやluma値だけで product defaults を変更していない。

## 2026-11-11 — Glow suite scope and capture-route correction

**方法:** A shared CC0 walkway frame 20 was used for independent Glow look sweeps and Light Leak looks. The deterministic neon plate was used for the current-versus-soft-knee Glow comparison. The Light Leak app fixture was captured from the supplied low-light portrait using CPU compute, strength 0.30, source (0.74, 0.19), angle -8°, and all three looks. Outputs are under `captures/glow-suite-independent-rd1/`, `captures/glow-suite-cc0-frame20-rd1/`, `captures/glow-suite-light-leak-cc0-frame20-rd1/`, and `captures/glow-suite-light-leak-app-cpu-ja-rd1-*`.

**確認:** The independent Light Leak comparison distinguishes a broad edge wash, a directional streak, and an organic local leak on the same CC0 still. On the portrait app capture, the streak crosses close to the subject's face, so source placement is a composition decision rather than a generic default. The Glow look-sweep code reports newly clipped channel fractions of 20.92–26.81% for additive looks and 1.19–1.90% for headroom looks on the walkway frame; these are model metrics, not production output or quality thresholds. Visual inspection also shows the headroom candidate remains substantially brighter than source in this high-key frame.

**Capture tooling finding:** `capture_ui_test.py` used the Glow flag in the Light Leak fan-out predicate. It could label Light Leak screenshots as a Glow run and never execute the Glow fixture. The predicate is corrected and Glow input validation is separate. Retrying against the existing executable returned `invalid or black Glow render`; the current executable did not produce the source-level Glow fixture output. A rebuild is required before claiming app Glow verification.

**App capture preparation:** The `ArtifactUiTest` source fixture and Python wrapper now accept a registered effect ID, a numeric property JSON object, compute mode, and a source plate for one effect per run. The fixture checks that each named property exists and reads back the requested value, and saves a separate input capture plus effect output/state. It has not been compiled or run. Only the existing Light Leak app path was captured successfully in this turn.

**判断と未検証:** This supports treating Glow as a suite of distinct families (bloom, edge, reactive, chromatic/spectral, liquid, residual/halation, directional/optical) alongside Light Leak, rather than choosing one generic Glow look. It does not rank or accept any family. The five implemented Glow variants still lack same-input app captures. Alpha, GPU parity, temporal behavior for residual looks, and performance remain unverified here.

**ソース経路監査:** `EdgeBloomEffect`、`ReactiveGlowEffect`、`ChromaticGlowEffect`、`DirectionalGlowEffect` はCPU/GPU implを登録しresident shaderも持つ。`LiquidGlowEffect` はCPU implに加えresident generic shaderを登録するが、専用GPU implは登録していない。`ResidualGlowEffect` はCPU implのみ。これはソース登録状態であり、実行時にどの経路が選ばれたかの検証結果ではない。

## 2026-11-11 — Transparent-input alpha review and Directional Glow parity fix

**方法:** `glow_suite_alpha_fixture.py`を使い、合成neon plateから半透明輪郭を持つ960×540 fixtureを生成した。初回`rd1`はalphaが0/255のみで部分alphaがなかったため、半透明試験として無効扱いにした。Gaussian featherを加えた`rd2`はpartial-alpha画素9.0702%、完全不透明0.0087%、透明約90.92%。既存`ArtifactUiTest`のinstalled-effect CPU経路で12 effectを個別captureし、`glow_suite_alpha_review.py`でstraight sRGB出力を線形premultiplied合成契約に沿って黒・中間灰・白へ合成して評価した。

**結果:** 12 effect中11 effectは入力alphaを完全保持した。Directional Glowだけはalpha MAE 0.1609 (8-bit)、最大差42、変更画素1.874%で、zero-alpha pixelにRGBが残る割合も0.348%だった。実装照合ではCPUがstreakのRGBAを加算して出力alphaを飽和させる一方、通常GPU HLSLとresident HLSLはRGBのみを加算しalphaを保持していた。これは透明入力で表面化するCPU/GPUの意味差である。

**変更と限界:** `Artifact/src/Effects/DirectionalGlowEffect.cpp`の2つのHLSL variantをfloat4 streakへ変更し、CPU側と同じくstreak alphaを強度乗算後に加算・飽和するよう修正した。RGBの処理は維持した。修正後GPU capture、shader compile、CPU/GPU parityは未確認である。修正前のrd2 captureが不一致の証拠であり、rd1は無効データとして除外する。

**再現:** fixture生成は`glow_suite_alpha_fixture.py`、出力レビューは`glow_suite_alpha_review.py`。入力、12 effectのcapture、接触sheet、JSON指標は`captures/glow-suite-alpha-transparent-neon-rd2/`および`captures/glow-suite-alpha-transparent-neon-cpu-rd2/`にある。

## 2026-11-11 — Shared-plate Glow family model comparison

**方法:** `glow_suite_lab.py`に、標準multi-scale halo、EdgeBloom、ReactiveGlow、ChromaticGlow、LiquidGlow、4-frame ResidualGlow、luminance P95/P99 soft-threshold bloomの独立CPUモデルを実装した。モデル式は該当Artifact CPU実装の大枠に沿わせ、製品出力ではないことをcontact sheetとJSONへ明記する。合成neon、CC0 walkway frame 20、低照度portraitを960×540で使い、Glow各モデルに同じΔ強度0.20を適用した画像、detail crop、alpha/finite/new-clipping/入力空間luma差を保存した。CC0 frame 17–20をResidual modelのsequence windowとし、current-only controlも出した。画像・設定・source hashは各出力の`glow-suite-metrics.json`にある。

**結果:** 強度scaleを適用しない最初のCC0モデル比較では新規clipped-channel率が約17.5–30.6%となり、高key雪面では比較条件として過剰だった。共通Δ scale 0.20では同素材の新規クリップは全familyで約0.0023%となった。Portraitでは同scaleの新規クリップ0%、一方でfixed 0.62 thresholdを超える入力空間lumaは0.14%に留まり、同じthresholdはCC0雪面の52.1%を超える。p90/p95/p99はportrait 0.174/0.203/0.345、CC0 0.864/0.891/0.944。したがってfixed threshold modelは暗部portraitでほとんどglow seedを作らず、高key雪面では広範囲をseedにする。全ての表示モデルでalphaはsourceと完全一致し、有限値だった。Residualの4 frame sequence対current-only RGB MAEは1.08e-5で、この窓では時間残光の視覚差がほぼなく、temporal acceptanceに使えない。

**追加比較:** Luminance percentile P95 bloom model（共通scale .20）はportrait平均入力空間luma差0.00253、fixed multi-scale haloは0.000038、P99 modelは0.00081で、3者とも新規clipped-channel 0%。CC0 walkwayではP95 model .01044、P99 .00482、fixed multi-scale halo .01668、new clipはいずれも約0.0023%。P95/P99は暗い素材で光源抽出を増やし、高key素材では固定threshold .62の広いseed領域を減らすが、候補の美的優位や動画での安定性は未判定。

**判断・制約:** 異なる用途の画像結果をひとつの「Glow強さ」で順位付けせず、各familyの役割別にthreshold/exposure responseと適正量を探す必要がある。Percentile extractionは有望な比較候補だが、global percentileがフレームごとに変わるとflickerする可能性があり、sequence smoothingが必要か未検証。現在のmodelsはnormalized SDR inputをそのまま入力空間で閾値化し、HDR、color-managed linear pipeline、製品CPU/GPU parity、実時間性能、より長い動き系列は未評価。比較画像は`captures/glow-suite-{synthetic,cc0-frame20,portrait}-soft-rd1/`。

## 2026-11-11 — Frame-percentile threshold response on a continuous CC0 clip

**方法:** CC0 walkwayの60 frame / 15fpsを連続処理し、fixed .62、framewise P95、P95をtau=.4秒のEMAで平滑化した3経路を比較した。soft-knee extraction、multi-scale blur、共通blend .20を固定。入力、出力、各frame threshold/clip/contribution、Farneback current-to-previous flowでwarpしたglow contribution差分、6時点のcontact sheetを保存。JSONと画像は`captures/glow-threshold-temporal-rd1/`。

**結果:** P95 thresholdのframewise標準偏差は.001686、EMA後は.001081（36%減）、平均値は.90067/.90088。60 frameのflow-aligned glow residualはfixed .62=.001249、raw P95=.000547、EMA=.000544。最大new clipped-channel率はfixed .62で12.17%、raw P95で1.45%、EMAで1.46%。この滑らかな単一ショットではEMAのflow residual改善は小さく、しきい値のばらつきを抑えながらP95ルックをほぼ保った。

**判断・制約:** この素材ではP95モデルが雪面を広くglow source扱いするfixed .62より低いcontribution/clipになり、暗いportrait上ではfixed .62より多くの実光源を拾う候補となった。閾値相対化は露出適応の候補だが、P95自体が意図する「上位5%」の光を作るため、空/雪/肌など意味的な発光選択はできない。1 clipだけで、明滅光、ショットカット、人物・カメラ運動、光源の出入り、色管理、GPU/product経路は未検証。EMAはカット後に閾値を引きずる可能性がある。

## 2026-11-10 — App-registered Light Leak CPU look comparison

**方法:** A deterministic 960x540 gradient plate with a bright point and horizon is injected into an isolated UiTest composition. The fixture installs the production `StillLightLeakEffect`, sets source (0.50, 0.25), angle 12°, strength 0.55, opaque alpha policy, then passes the same image-layer CPU buffer through that installed object's `applyToCpuSurface`. The normal Japanese app UI and its separate viewport readback are also saved. Captures: `captures/light-leak-app-ja-cpu-final-fixture-input-cpu.png` and `captures/light-leak-app-ja-cpu-final-{edge-wash,streak,organic}-cpu.png`.

**結果:** All three effect captures decode as 960x540 RGBA, have nonzero pixel range, and differ pairwise. Mean absolute encoded RGB deltas from the input were Edge Wash .075803, Streak .068156, and Organic .106329. Pairwise MAE was Edge Wash/Streak .095777, Edge Wash/Organic .092890, and Streak/Organic .138029. The images preserve the bright source and show distinct edge wash, directional anamorphic band, and broader organic color spill. The fixture state marker confirms controller/composition identity, one visible active layer, a valid 960x540 image buffer, and one installed effect. The UI-language capture remains Japanese.

**制約・次:** These are the installed effect object's CPU-reference results, not proof that Composition Viewer rendered its layer effect. The state marker rules out effect insertion, enabled state, and Rasterizer-stage recognition as the immediate cause. A post-render UI capture is now saved, but the viewer canvas is empty while the separate controller readback shows the unmodified source. Trace which composition-editor surface is visible and which target `requestCurrentFrameImageAsync` reads before treating either capture as an end-to-end effect result. GPU mode has no valid effect capture. English and the remaining configured locales have not been exercised for this fixture.

## 2026-11-10 — GPU Light Leak route diagnosis

**方法:** GPU modeで Edge Wash / Streak / Organicを個別に起動し、通常ブレンド構成でもGPU合成を要求する `ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL=1` を加えた。初回は起動遅延中に取得され、`.viewport.png` は元画像とcheckerboardを示し、3つの出力SHA-256も同一だった。fixtureに限定してブレンドPSOのcold-start待ちを追加し、Edge Washを再取得した。キャプチャは `captures/light-leak-ready-gpu-{edge-wash,streak,organic}-gpu` の各frame-debugとLayer RT / Accum RT。Streak / Organicの一部ファイルはcapture helperのlook suffixが重なった名前になっている。

**結果:** 3 looksすべてのframe-debugで `path=gpu-blend`、`gpuBlendReady=1`、`gpuBlendInitAttempts=1`、`dispatch=2`、`failed=0`、`directFallback=0` を記録した。Layer RTは入力fixtureに近く、Accum RTには入力とLayer RTにない色の広がりが目視できる。letterboxを除いてAccum RTを960×540へリサイズし、入力と単純比較したencoded RGB MAEはEdge Wash 19.27/255、Streak 21.13/255、Organic 28.20/255（色空間変換を補正していない参考値）。キャプチャ上でもEdge Washは広い周辺色、Streakは斜めの強い帯、Organicは下側へ広がる色として差が見える。つまり3 looksのGPU blend中間出力差を記録できた。一方 `requestCurrentFrameImageAsync` がfixture内の20秒待機でも完了せず、`.viewport.png`、効果後state marker、最終画面キャプチャは生成されなかった。GPU nodeの実行結果とCPU参照のpixel parityを確定する証拠にはまだ不足する。Vulkan logical device同時生成不可のDiligent ERRORが2件続いているが、因果関係は未検証。

**結論・未検証:** 起動時pipeline不在という以前の条件はfixture待機後に解消した。GPU blend合成は成功し、Accum RTは入力と異なるが、最終readbackが止まる位置を調べる必要がある。次はRT previewをreadback完了の依存にせず保存し、フレーム番号・出力サイズ・GPU effect pass成功状態をfixture markerへ先に記録する。そのうえでLayer RT / effect後layerFloat / Accum RT / finalPresentを対応づけ、CPU referenceと色空間を揃えて比較する。Vulkan deviceエラーとの関連も切り分ける。

## 2026-11-10 — Light Leak GPU/CPU UV parity and captured output

**方法:** Independent Japanese UiTest fixture runs the same 960x540 source and StillLightLeak settings through the installed CPU effect and the resident GPU generic node. GPU composition is enabled with `ARTIFACT_COMPOSITION_GPU_BLEND_NORMAL=1`; the fixture exports the alpha-bounded Accum RT image because async and sync final-present readback both wait indefinitely in this capture scenario. The GPU composition route records `gpuBlendReady=1`, one GPU node, `dispatch=2`, `failed=0`, and `directFallback=0`. Captures are `captures/light-leak-parity-gpu-{edge,streak,organic}-v2-*.png` and `captures/light-leak-final-cpu-{edge-wash,streak,organic}-cpu.png`; the side-by-side sheet is `captures/light-leak-cpu-gpu-parity-v2.png`.

**結果:** Initial RGB MAE was .02694 Edge Wash, .06347 Streak, and .02470 Organic. Source pixels were identical; inspection showed the GPU shader evaluating normalized viewport coordinates while CPU used image-local coordinates. The controller now maps viewport UV to the 2D layer's source UV using the inverse global transform and source dimensions, passing an affine map to the resident shader. For 3D layers, where this affine map cannot represent perspective, the raster plan falls back to the established CPU path. After the change, full-frame RGB MAE fell to .009416 Edge Wash, .011437 Streak, and .006495 Organic. Excluding a 2px border, MAE is .008918, .011208, and .005988; p99 absolute channel error is .019608, .027451, and .019608. The largest residuals lie on the cropped output boundary. Visual look placement now aligns closely, including the diagonal anamorphic streak. Metrics and capture names are recorded in `captures/light-leak-cpu-gpu-parity-v2-metrics.json`.

**制約・次:** These are diagnostic Accum RT crops, not final-present or on-screen Composition Viewer captures. The remaining MAE includes composition/crop/color-path differences and has not been isolated to a single stage. The affine mapping assumes the raster target UV corresponds to the current viewport transform and the layer is 2D. Validate transform changes (zoom, pan, rotation, nested transforms), alpha edges, aspect ratios, and 3D CPU fallback separately before treating parity as general. The capture helper's current final readback waits remain unresolved.

## 2026-11-10 — CC0 winter plate app capture and effect-delta comparison

**方法:** The Light Leak UiTest fixture now accepts `--light-leak-input`, loads a still plate at its native dimensions, and creates the composition at the same dimensions. The plate is frame 20 from the 360×640 CC0 walkway fixture documented in `tests/ArtifactCore/fixtures/README.md`. CPU and GPU captures use the same source PNG SHA-256 and default effect settings (strength .55, streak source .50/.25, angle 12°). Each output is saved at 360×640; GPU state confirms the Accum RT source and active GPU node. Side-by-side captures are `captures/light-leak-realplate-cpu-gpu.png`, with measurements in `captures/light-leak-realplate-cpu-gpu-metrics.json`.

**結果:** On the snow/bridge photo, the three looks remain visually distinct at the app level: Edge Wash warms the upper-left and side, Streak adds a broad diagonal sky beam, and Organic adds distributed magenta/amber edge spill. Direct encoded RGB output MAE is .019508 / .019965 / .016626 (Edge Wash / Streak / Organic). However, the unprocessed GPU Layer RT already differs from the source by MAE .02622 after diagnostic crop and resize, so direct output MAE is not a valid effect-only parity score. Comparing `(Accum RT - Layer RT)` against `(CPU effect - input)` gives approximate effect-delta MAE .00670 / .00670 / .00761, with p95 absolute errors .01961 / .01961 / .02353. The capture crops/resizes GPU RTs to source dimensions; the effect-delta calculation uses an independent bilinear resize and is an approximate, not linear-light, decomposition.

**暫定判断・制約:** The source-space mapping stays visually aligned on a portrait plate with a substantially different aspect ratio than the earlier 960×540 plate. This is stronger evidence for aspect-ratio behavior, but does not verify zoom, pan, rotated layers, nested transforms, or 3D. On this high-key snow/sky material, Edge Wash and the lower Organic spill can visibly tint broad areas; .55 should be treated as a comparison setting rather than a universal default. Next look-dev pass should capture a restrained strength sweep and a low-key portrait against the same plate before recommending presets. The Composition Viewer final-present/readback limitation still prevents an on-screen render acceptance claim.

## 2026-11-10 — Light Leak strength and practical-source placement sweep

**方法:** The independent `light_leak_lab.py` reference captured all three looks at strengths .20, .35, and .55 on the CC0 winter walkway frame and a low-key night portrait plate. Contact sheets are `captures/light-leak-winter-strength-rd1.png` and `captures/light-leak-portrait-strength-rd1.png`; region, mean-luminance, and newly-clipped-pixel measurements are stored beside them in matching JSON files. The night portrait has a visible practical lamp near normalized (.74, .19). A streak sweep moved its source to that lamp and compared -8°, 0°, and +8° at strength .30. The -8° / .30 candidate was then captured through both installed CPU and GPU app routes with `--light-leak-input`; the app capture and independent reference are assembled in `captures/light-leak-portrait-streak-source-rd1.png`.

**結果:** All strength variants had zero newly clipped pixels on these two SDR plates. Linear mean-luma lift increases monotonically with strength. At .20, lift spans .00487–.00862 on the winter scene and .00829–.01432 on the night portrait; at .55 it spans .01253–.02368 and .02121–.03855. The snow/sky image reveals broad color tinting at .55, while .20–.35 is more restrained. On the night portrait, aligning the Streak source with the practical lamp avoids the previous arbitrary top-left origin and gives a controllable diagonal beam; -8° directs it below the face. CPU app and GPU app captures are visually close, and the GPU dispatch state reports the resident generic node active. The app CPU result differs from independent Python encoded RGB by MAE .02382; after subtracting the GPU pre-effect Layer RT from Accum RT, the GPU-vs-CPU effect delta MAE is .01831 (p95 .03137). The effect-delta value is approximate because the RT crop/resize and encoded PNG math do not reproduce the internal linear-premultiplied surfaces exactly.

**暫定判断・制約:** .20–.35 are useful restrained starting points for these two plates, not a global default recommendation. A practical-light source position improves the streak's causal reading, but source detection remains manual, and the generated portrait does not replace evaluation on licensed production footage. The streak currently crosses near the subject's upper body at -8°; subject-aware occlusion or masking is not part of this analytic source-space field. Continue with more real plates and user-controlled source placement before choosing a preset.

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

The selected control is `Streak Angle` in degrees, clamped to [-45°, +45°]. The CPU implementation computes sine/cosine once before the image loop; the resident filter reads P5 and uses the same degree range. MSVC 14.52 `ArtifactEffectsGlow` build and DXIL/Vulkan 1.1 shader compilation passed after this change. The full app target subsequently built. A Japanese Light Leak scene fixture now captures the registered effect's CPU output; the visible Composition Viewer render remains unverified and CPU/GPU parity remains unverified.

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

An alternate `x64-Debug-ClangXX-MsvcAbi` configure completed and selected Vulkan; D3D11/D3D12 support was disabled in that configuration. Its first full `ArtifactUiTest` build compiled dependencies and much of ArtifactCore, then stopped at `ArtifactCore/include/...` modules with four visibility errors for Qt `qsizetype`. This remains a separate toolchain/module integration issue. The successful current MSVC 14.52 app build also has D3D12 disabled and Vulkan enabled. A dedicated Light Leak UiTest fixture now writes the app effect's CPU reference output and state marker; a post-render main-window capture still has a blank Composition Viewer, while the controller readback resembles the source. Therefore app effect CPU reference is captured, but visible Viewer rendering is not validated. Runtime GPU dispatch and CPU/GPU parity are still unverified.

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

## 2026-11-11 — Light Leak source placement around a portrait subject

**方法:** `light_leak_subject_placement_lab.py` でポートレートplateを固定し、独立source-derived anamorphic streakのstrength 0.35、angle 2.58°を保ったままnormalized source Yを0.01/0.06/0.11/0.16/0.21へ移動した。顔ROIは画像を見て手動指定した `(0.37, 0.08)–(0.49, 0.25)`。5条件のfull-frame contact sheet、各画像、SHA-256、face ROIと上部frameの線形luma delta、新規clip、alpha保持を記録した。出力は`captures/light-leak-placement-portrait-rd1/`。

**結果:** source Y=.01では顔ROIの平均ΔL 0.000472、上部全体0.104536で、顔ROI/上部比0.005。Y=.06では比0.378となり、Y=.11/.16/.21では2.05/2.60/3.10となる。contact sheetでも、既存既定に近いY=.11はバンドが被写体の顔を横切り、.01は頭上に留まる。全条件の新規clipは0%、alphaはsourceと一致した。

**判断・制約:** ポートレート上の初期配置候補はsource Y=.01（同一plate内）だが、効果を無効化せず顔への光を避ける判断は画面構図依存であり、汎用defaultとして採用しない。face ROIは手作業の矩形で、人物検出や視線/肌色保護ではない。source Y sweepだけであり、streak angle、source X、強度、動き、別の人物位置、GPU/アプリ出力とのparityは未評価。このcaptureは独立モデルの配置研究で、製品効果の出力ではない。

### 追試 — 登録済みStill Light Leakの製品キャプチャ

**方法:** 同一ポートレート、strength .35、angle 2.58°、source X .01を固定し、登録済み `still_light_leak` のCPU出力をsource Y=.005/.015/.025/.035/.045/.055で撮影した。GPUは .015 と .055 を撮影し、frame-debug stateとAccum RTを確認した。独立captureと同じ手動face ROI・上部ROIでCPU出力を測定した。製品キャプチャ、contact sheet、JSONは `captures/light-leak-placement-portrait-app-cpu-rd1/`、GPU captureは `captures/light-leak-placement-portrait-app-gpu-rd1/`。

**結果:** CPUの顔ROI/上部平均linear-luma delta比は順に .0027/.0067/.0193/.0530/.1285/.2720。新規clipは全条件0%、alpha MAEも0。独立モデル側の比との差は全位置で0.001未満となり、配置の転換点は近似だけでなく登録済みCPU効果でも再現した。Y=.015は顔への寄与が小さく、Y=.035から増え始め、.055では顔ROIのΔLが上部平均の約27%に達する。GPUの両captureは `compute=1`、`gpuBlendReady=1`、`captureSource=Accum RT` を記録し、同じく .015は頭上、.055は顔へ近づく帯を目視確認した。CPU/GPUの各出力から対応するfixture-input captureを引いてlinear-light effect deltaを比べた近似RGB MAEは .00438/.00432、effect deltaの平均linear lumaはCPU .01172/.01303、GPU .01506/.01634。直接出力のencoded RGB MAEは .02094/.02057だった。

**判断・制約:** この素材でのsource Y=.005–.015を「頭上のリーク」を狙う初期配置の比較候補として残すが、全素材向けdefaultにはしない。測定は1枚のポートレート、手動矩形、CPU referenceを中心とし、人物マスク・別人物位置・動き・肌色別評価は未実施。GPU delta差分も診断Accum RTからの近似で、ビューポートsampling・画像source座標差が含まれる。最終Viewer表示との厳密なCPU/GPU parity測定ではない。Metricsは`captures/light-leak-placement-portrait-app-gpu-rd1/cpu-gpu-effect-delta-metrics.json`。

## 2026-11-11 — Still Light Leak Streak angle on a street-light photograph

**方法:** 登録済み`still_light_leak`を`ArtifactUiTest` installed CPU reference経路で撮影。CC0縦長street-light photo（576×1280）上でStreak strength=.25、source=(.50,.12)を固定し、angle -24/-12/0/+12/+24°を比較。各runでは3 lookを撮影したが、角度はStreakの比較要因。全source fixture PNGのhash一致、state sidecarのCPU referenceを確認した。Contact sheetとROI metricsは`captures/light-leak-streetlamp-angle-cpu-rd1/review/`。

**結果:** 手動ROIのlamp band（x=.20–.80, y=.055–.19）平均linear ΔLは、angle -24/-12/0/+12/+24°で.07852/.09590/.09995/.09626/.07849。下側background bandは.04031/.03320/.03220/.03455/.04395。全角度でfull-frameの新規clipped channelは0%、alpha MAEは0。Contact sheetでは0°は街灯列を横切る水平帯、±12°は帯を斜めに傾け、±24°はlamp band寄与が約21%低い一方で下側backgroundへの寄与が増えた。

**判断・制約:** 単一写真と手動矩形では0°が測定したlamp bandの応答を最大化し、強度.25ならclipを増やさない。ただしこれは美的合格や光源への自然な整列を意味しない。画面上部を横断する帯が残り、±角度は光源を横切る範囲と周辺spillのtradeoffになる。source座標、角度、strengthは既存のArtifactCore / Artifact controlsで表現できるため、現時点ではAPIやdefaultを変更しない。別の光源配置、別露出、画像上の実際のビーム角、GPU/final Viewer、HDR、時間応答は未検証。

## 2026-11-11 — Registered Glow-family CPU product baselines

**方法:** `ArtifactUiTest`を現行ソースでbuildし、汎用Effect ID fixtureから登録12系統を同一条件でCPU撮影した。入力は1536×1024低照度ポートレートと960×540合成ネオン。Glowだけはportraitで`glowGain=2.5` / `baseSigma=10`を指定し、他は登録既定値。`glow_product_capture_review.py`が各capture sidecarとstateを基にfull-frame入力/出力/4倍差分を並べ、SHA-256、encoded RGB MAE、linear-luma差、新規clip、alpha差を記録する。captureは`captures/glow-suite-product-portrait-cpu-rd1/`と`captures/glow-suite-product-neon-cpu-rd1/`。

**結果:** 同じneon入力上のencoded RGB MAEはGlow .00233、Edge Bloom .01241、Reactive .00706、Chromatic .00197、Liquid .00300、Residual .00153、Directional .00091、Physical Halation .00000（修正前）、Luminescence Caustics .00197、Optical .00168、Volumetric Shine .00063、Glint Star .00052。全12でalpha MAE 0。Edge Bloom / Reactiveはネオン文字・輪郭への寄与が最も強く、Directionalは薄い筋、Causticsは輪郭を強調する。portraitでは複数の光学系統が既定値でかなり控えめで、同じ係数によるfamily間ランキングは成立しない。これらはinstalled effectのCPU reference outputであり、通常のViewer最終frame・GPU結果・美的合否ではない。

**Core修正・再撮影:** この比較で`PhysicalHalationEffect`が両plateで完全no-opと分かった。`ArtifactCore/include/ImageProcessing/Halation.ixx`では広域red diffusionを`highlights`へ書き、その直後に同じ配列をgreen/blue diffusionで上書きしていた。red層を既存destination bufferへ出力し、GB層は元のhighlight fieldを使い、水平passの一時vectorを2 pass間で再利用するよう修正。現行target buildは成功し、post-fix default neon captureはMAE .00016、tuned `(threshold=.4, spread=24, intensity=3, redDiffusion=2.5)`はMAE .00725、new clip .2189%、alpha差0。強設定は広い赤い帯になるためlook候補として不採用、defaultも素材・用途を広げて再評価する。

**Hot-path再確認:** `PhysicalHalationCPUImpl::applyCPU`は呼び出しごとに`PhysicalHalation`を構築し、`PhysicalHalation::process`は全frame相当の`source`、`highlights`、`temp` vectorを毎回生成する。`temp`を2 diffusion pass間で共有しても、steady-stateの大容量確保3回は残るため、[HOT_PATH_RULES.md](../../docs/technical/HOT_PATH_RULES.md)を満たさない。再利用workspaceの所有権・並行full/draft処理との安全性を決め、allocation/resource計測するまでは、画質修正済みでもCore integration quality gateは未完了。HalationはGPU実装もなくCPU経路だけである。

**GPU経路の状態:** 同じneon上でEdge Bloom GPU captureを試したが、`frameDebugSnapshot`の`previews`配列が空でAccum RT出力を得られず、wrapperは画像を成功扱いしなかった。consoleにはDiligent Vulkan instance/deviceのsingleton errorが出た。CPU/GPU parityは未検証のまま保持し、diagnostic resource/routeの解決後に再実施する。

## 2026-11-11 — Physical Halation softness response

**変更:** `ArtifactCore.ImageProcessing.Halation`の`Settings.softness`は公開propertyから渡されていたが、highlight extractionでは未使用だった。threshold周辺にsoft-knee contributionを入れ、softness=0はhard thresholdを維持し、正の値はthreshold下側から滑らかに反応させる。二層diffusion修正後の結果バッファ・共有scratchを維持する。

**確認:** 合成neonの同一入力でdefault threshold .8 / spread 15 / intensity .5 / red diffusion 2を撮影。softness=.5のCPU product referenceはencoded RGB MAE .00092、平均linear ΔL .00024、変更channel率4.64%、新規clip 0%、alpha差0。softness=0はMAE .00016、ΔL .00003、変更channel率0.95%、新規clip 0%、alpha差0。soft-kneeで応答はMAE比約5.8倍となり、輪郭まわりに控えめなred diffusionが見える。capturesとreview JSONは`captures/glow-suite-product-neon-cpu-rd3/`。

**制約:** 1枚の合成plateだけの入力応答比較で、肌や実写点光源への色移り、SDR/HDR、GPU route、フレーム連続性、時間/allocator性能は未検証。softnessのdefault .5を美的に採用した判断ではない。Core Halationは現在CPU-onlyであり、GPU優先方針に沿う統合には別途GPU pathとparity評価が要る。

## 2026-11-11 — Restrained Glow candidates at portrait face detail

**方法:** 同じportrait CPU product captureから顔まわりを同一ROI `(0.32, 0.02)–(0.55, 0.30)` で切り出し、input / output / 5倍差分を並べた。Edge Bloom Edge Boost .15/.30、Reactive Glow、4-ray Glint、Optical Glow primary-only / secondary-only / restrained stackを対象とする。capture stemとROIを固定するreview helperは`glow_portrait_detail_review.py`。contact sheetとROI別linear luma差、新規clip、alpha差は`captures/glow-suite-restrained-portrait-cpu-rd1/portrait-face-detail-contact-sheet.png`および対応するJSON。

**結果:** ROI内の平均linear ΔLはEdge Bloom .15/.30で.00102/.00187、Reactive .00033、Glint .00006、Optical primary/secondary/stackで.00066/.00044/.00087。face ROIでは全profileの新規clipとalpha差が0。差分表示はEdge Bloomの肌・髪・背景への広がりが他候補より強いこと、Optical secondaryの光学shapeがprimary haloと異なることを示す。GlintとReactiveはこの顔plateではほぼ控えめで、neon plateでの役割と分けて評価する必要がある。

**暫定判断・制約:** Edge Boost .15は.30より顔領域の広がりを抑えた候補だが、顔ROIの矩形は手動であり、主観的な肌質/色再現の合格を数値だけで決めていない。接触sheetの文字はOpenCV標準fontのため一部記号が代替表示される。全てArtifactUiTestの登録effect CPU reference captureであり、GPU・最終Viewer・HDR・時間安定性・処理コストは未確認。still Light LeakはGlow suiteに混ぜず、edge wash / streak / organicの別lookとして評価を継続する。

## 2026-11-11 — Detail ROI matrix and practical-local restraint

**方法:** 登録済み12 Glow系統の同一role-candidate CPU capturesを、portrait顔とneonのmagenta ring、emissive text、cyan/amber practical、horizontal tubeの合計72 ROIで集計した。すべて同じplate source/output hash、normalized ROI、1:1 input/output/5倍差分cropを対応付け、線形luma、channel clip、alphaを記録した。reviewerは`glow_role_detail_matrix.py`。次にEdge Bloom、Reactive Glow、Optical Glowの抑制候補をneon plateで製品CPU経路から撮影し、ring/text/tubeでbaselineと比較した。profile / review tool / outputsは`glow_practical_restraint_profiles.json`、`glow_practical_restraint_review.py`、`captures/glow-suite-practical-restraint-rd1/`。

**結果:** 元のrole-candidateはhorizontal tube cropで新規clipがEdge Bloom 5.628%、Reactive 5.408%、Optical 5.648%。Edge BloomをAmount .25 / Edge Boost .15へ下げるとring/text/tubeで3.740/2.787/5.628%から0.314/2.729/0%へ下がり、チューブの細いhaloを残す。threshold .95も3 ROIのclipを0%にするが、文字のmean ΔLは.00507であり、役割強度とのtradeoffを次素材で見る。Reactive intensity .25ではring/text/tubeのclipが3.614/2.765/5.408%から0/.617/0%へ下がり、発光源の周囲は依然見える。Opticalの低secondary + threshold .50はhorizontal tube clipを0%に抑えながらmean ΔL .00413を残すが、emissive text clipは2.778%でbaseline .2.787%とほぼ同じ。threshold .65ではtubeのmean ΔLが.00022まで下がり、実質的にtube roleを失う。72 baseline ROIすべてでalpha差0。顔ROIでCausticsはnew clip .002%と局所輪郭変化が出たが、評価はこの1 plateに限定される。

**暫定判断・制約:** Edge Bloom / Reactive / Opticalは別々の制御軸により役割とclipを調整でき、ひとつの全体MAEだけでは明るい実用光源の飽和を見落とす。現時点のrole-candidateを共通defaultに採用しない。ROIは固定された合成chart / 単一portraitの手動座標で、異なる色域・肌・露出、最終Viewer、GPU、HDR、時間応答、性能/リソースは未検証。Opticalのsecondary balanceやEdge Bloomのthresholdと明るい実素材への反応を追加確認し、familyごとの評価基準は複数素材で決める。

## 2026-11-11 — Third-plate CC0 Glow suite and high-key response

**方法:** CC0素材 `walkway-orbit-cc0.mp4`（4秒excerpt、15fps、360×640、出典とCC0条件は`tests/ArtifactCore/fixtures/README.md`）からzero-based frame 20を抽出した。source/video SHA-256、frame index/time、寸法、PNG hashをmetadataへ保存。登録Glow 12 effectを既存 `glow_role_candidate_profiles.json` の同じ役割候補値でArtifactUiTest CPU product pathから撮影し、入力・出力hash、各全体指標、contact sheetを出力した。app fixture inputはopaque alphaを持つBGRA PNGだが、抽出PNGとのRGB値は全画素一致した。続けてbright-sky / bridge / ice-and-water / dark-waterの4 ROIへ同じ判定を適用した。結果は `captures/glow-suite-role-candidates-walkway-cpu-rd1/` と、全portrait/neon/walkway ROIを含む `captures/glow-suite-role-detail-matrix-rd1/detail-roi-metrics.json`。

**結果 — 共通候補の高key失敗:** neon/portraitのrole候補は明るい空・雪氷上で過大だった。全frameの新規clipped-channel率はGlow 54.18%、Optical Glow 52.75%、Edge Bloom 31.48%、Liquid 31.78%、Chromatic 26.86%、Directional 26.45%、Reactive 16.47%、Residual 12.91%、Caustics 6.17%、Physical Halation 4.34%、Glint 13.81%、Volumetric Shine 0.691%。全12でalpha差0。これは同じ係数を材質間で共通採用できないことの明確な反例であり、アプリの8bit PNG clipping統計はHDR合否を示さない。

**高key別候補:** 12系統ごとにthreshold / intensity等を抑えた初期profile `glow_highkey_restraint_profiles.json` を作り、同じframe/sourceで再撮影した。ここでの新規clipped-channelは8bit RGB PNGで、result channel >=254 かつ source channel <254 の割合。全frameのclipはGlow .5 gainで32.76%、Edge Bloom 1.251%、Reactive .457%、Chromatic 1.214%、Liquid 2.648%、Residual .726%、Directional 2.648%、Physical Halation .690%、Caustics 1.315%、Optical .372%、Volumetric .036%、Glint .228%。Glowを除く11系統はrole candidateより低いが、Edge Bloom / Reactive / Chromatic / Volumetric / Glintの一部は画像上かなり控えめになる。Physical Halationはclip .69%でも画面全体に目立つ赤いcastを出す。数値改善だけでは美的合格にならない。

**ROIでの役割比較:** baseline role candidateとhigh-key restraintのfixture RGBは全画素一致し、12 effect × 4 ROI = 48組を同じ位置で比較した。Edge Bloomのbright-sky mean ΔLは.03992→.00024、clipは7.00→0%; bridgeでは.25653→.03315、35.51→2.37%。Reactive bridgeはΔL .08363→.00751、clip 10.94→.71%。Optical bridgeは.23402→.00663、clip 36.33→.58%。それぞれのhigh-key値は広い空のspillとclipを抑えるが、実用光源周囲のhalo量も下げる。標準Glowはbridge clip 39.84→22.79%、ice/water 63.79→38.39%に留まり、gain .5 profileはまだ高key素材を保護できていない。対比較sheetと指標は `captures/glow-suite-highkey-role-comparison-rd1/`。

**Glow base gain追試:** highkey plate上のgain .05/.10/.20/.50を比較した。全frame RGB MAEは.01664/.03301/.06305/.12466、平均linear ΔLは.02460/.04979/.09819/.19663、新規clipは.726/2.601/10.535/32.764%。.05は0.5の約45分の1のclipを保つが、全画面に薄いhalo/veilが残る。局所ROIと別のhigh-key素材、肌・点光源では未検証で、Glowの採用値として確定していない。capture/reviewer/manifestは `glow_highkey_base_glow_sweep.json`、`captures/glow-suite-highkey-base-glow-sweep-rd1/`。

**暫定判断・制約:** 今回のwalkway frameは高key・雪面反射・空・暗い水面を同居させたstress plateで、全Glowの品質順位や汎用値を定めるground truthではない。静止frameのみではResidual履歴、Liquidのphase安定、temporal flickerを評価できない。全結果は現行ArtifactUiTestのinstalled-effect CPU reference PNGであり、最終Composition Viewer、GPU、HDR線形浮動小数出力、性能/allocationsは未検証。次にfamily別の役割可視性とclip境界を追加写真・別露出で測り、GPU実装の有無とeffect-specific受入範囲を分けてからArtifactCore / Artifactへ採用候補を選ぶ。

**ROI対比較の追記:** 元のrole candidateと高key抑制profileを4つの同一ROIで直接並べた。Edge Bloom / Reactive / Opticalは高key空とbridgeのクリップを下げたが、bridge上のhalo寄与もそれぞれ大きく低下した。標準Glowはgain .5でもbridge / ice-waterのクリップが22.79% / 38.39%残った。各値と等倍cropは `captures/glow-suite-highkey-role-comparison-rd1/`。したがって「clipを減らす」だけではfamilyの役割を保った合格とできず、次は色分散と歪みのような署名効果を細部ROIで分けて測る。

## 2026-11-11 — Chromatic separation and liquid distortion sweep

**方法:** 合成neonの同じArtifactUiTest fixture input（960×540 RGBA。role-candidate captureの `app-role-glow-glow-cpu-fixture-input.png`）で、Chromatic GlowのDispersion 0/.75/1.5/3/6とLiquid GlowのDistortion 0/1.5/3/6/12をinstalled-effect CPU pathから撮影する。各family内のThreshold、Radius、Intensity、Tint Mix / Flow Scale、Phase、角度は固定し、違いを一つのパラメータに限定する。プロフィール、各captureのhashとproperty stateは `glow_family_signature_sweeps_neon.json` と `captures/glow-suite-family-signature-neon-cpu-rd2/` に保存。初回は全画面UI captureをfixture入力と誤指定したため、その出力（`...-rd1/`）は無効扱いで隔離し、今回の比較に使わない。

**制約:** Chromaticの全体luma差は色分離量を直接表さず、Liquidの静止画は位相・時間安定を判定しない。細い文字とtube ROIの視覚確認、RGB channel別のエッジ位置／色差指標を併用するまで数値順位や候補採用をしない。CPU reference captureのみでGPU parity、Composition Viewer、HDR、処理時間、hot-path allocationは未評価。

**結果:** 有効captureは入力RGB hashがすべて一致し、計8条件（Chromatic 3、Liquid 5）でalpha MAEは0。Chromatic dispersion 0/.75/1.5は全体RGB MAE .00119/.00120/.00120、平均linear ΔLは各.00068、new clipは約.175%でほぼ同じ。text 1:1差分では低強度の暖色／青系の色縁が見え、定量ではRGB channel mean absolute deltaがわずかに再配分されるが、現captureのdispersion範囲で強い分離signatureとは言えない。Liquid distortion 0/1.5/3/6/12は全体MAE .00241→.00251、tube ROIのmean absolute RGB deltaは3.549→3.241（sourceの直線光そのものがゆがむのでなく、halo帯に周期的なうねりが増す）。増幅差分のtube cropでは12で最も明瞭なwave signatureが現れ、clipは全候補でROI 0%、alpha差0。Distortion=0でもglow haloは残るので、flow変形とhalo強度は別々に判断できる。

**capture再試行:** Chromatic 3/6のeffect-suite出力は2回とも黒画像判定となり、state sidecar／effect outputを残さないため、family sweepの結論から除外した。全10プロフィールの有効／失敗を統合したcapture manifestは`family-signature-manifest.json`。初回の全画面UI素材誤指定分も無効である。参照画像と指標は`captures/glow-suite-family-signature-neon-cpu-rd2/`、固定ROI reviewerは`glow_family_signature_roi_review.py`。

## 2026-11-11 — Portrait context for the shared Glow recipe suite

**方法:** 1536×1024のAI生成低照度portrait plate（顔、暗いコート、道路、街灯、店舗光）へ、portrait用Glow profile `glowGain=2.5 / layerCount=4 / baseSigma=10`を一度適用した。出力を共通foundationとして、Edge Bloom / Reactive / Chromatic / Liquid / Directional / Physical Halation / Luminescence Caustics / Optical Glow / Volumetric Shine / four-ray Glintの10 specialistへ枝分かれさせ、各枝を`ArtifactUiTest` installed-effect CPU reference pathで撮影した。全枝入力はfoundationとpixel-identical、SHA-256 `eef66fe4315ab3957dc60fd31707a964ebeb2625ed860b90deee5283c10988ef`。一画像ではhistoryを示せないためResidual Glowは除外した。recipe profileは`glow_suite_recipe_profiles_portrait.json`、capture/manifestは`captures/glow-suite-recipes-portrait-cpu-rd1/`。

**結果:** 10枝すべて取得され、各枝のalpha MAEは0。full-frame encoded RGB MAEはReactive .00025、Chromatic .00051、Liquid .00075、Directional .00027、Halation .00017、Caustics .00215、Optical .00047、Volumetric .00012、Glint .00028（Edge Bloom .00183）。Contact sheetでは全体の差は小さくても、ROIでeffect signatureが分かれた。街灯contextにはChromaticの青／橙fringe、Directionalの細い横streek、Causticsの周囲texture、Glintの4-ray形状が現れ、Opticalは複数成分のhalo/streekを足した。顔ROIのclipは全枝0で、Chromaticの変化は0、他の大半も微小。一方Causticsは顔ROIで最大線形luma差 .25491を含む細かな模様が目視でき、顔肌には不自然になり得る。店舗灯ROIのnew-clipped-channel率は全体平均より高く、Chromatic 1.965%、Liquid 2.030%、Directional 1.112%、Optical 1.097%だった。したがって同じ処理が全体では穏やかでも、点光源に近いROIでclip/headroomを確認する必要がある。全体contact sheetは`portrait-recipe-contact-sheet.png`、face / lamp / shop / dark-coatの1:1比較とmetricsは同ディレクトリの`portrait-recipe-*-roi-*`および`portrait-recipe-roi-metrics.json`。

**判断・制約:** 同じGlow基盤から分岐しても専門effectの署名が残ることを、人物・街灯素材の製品CPU経路で確認した。これは色管理された実写、ground truth、GPU、最終Composition Viewer、またはfloat保持の同一stackではない。各枝は8-bit PNG foundationを再importする比較proxyであり、候補値は採用presetではない。fixture画像はAI生成R&D plateで、CC0撮影素材ではない。次は別露出の写真素材とbounded ordered float stackで同じfamily差・局所clip・肌ROIを再確認する。

## 2026-11-11 — Chromatic Glow dispersion extension to 2.0

**方法:** 前回と同じ960×540 synthetic neon fixture inputで、Chromatic GlowのDispersionだけを0 / 1.5 / 1.75 / 2.0 / 2.5へ変更し、Threshold .6、Radius 7、Intensity .65、Tint Mix .25、Angle 0を固定した。`ArtifactUiTest` installed-effect CPU reference pathを使用し、入力hash、property state、画像を保存した。profileは`glow_chromatic_dispersion_neon_rd3.json`、出力は`captures/glow-suite-chromatic-dispersion-neon-cpu-rd3/`。

**結果:** Dispersion 0 / 1.5 / 1.75 / 2.0は有効な960×540 RGBA captureとなり、入力RGB hashは一致、alpha差は0。Dispersion 0との差のRGB MAEはtext ROIで0 / .2498 / .2910 / .3313 (8-bit)、ring ROIで0 / .0824 / .0963 / .1101、horizontal-tube ROIは全候補0。2.0でのtext ROIチャンネル別差はR .4699、G 0、B .5239で、変化がred/blue成分に限られる。sourceに対する既存clipはtext ROI 2.544%で全候補ほぼ不変、ring ROIは.348%から.313%へ下がった。画像上も2.0までのdispersion増加は控えめで、独立した強い色縁signatureを示す根拠には足りない。

**失敗の原因:** `ChromaticGlowEffect::setDispersion`が値を[0,2]にclampし、capture fixtureは要求値のreadbackが一致しない場合に出力を受け入れない。よって2.5は画像処理後の黒出力ではなく、property-range validationでrejectされた。3 / 6のcapture欠落も上限値より大きい要求値と一致する。Vulkan singleton errorは成功captureにも記録されており、このrejectの原因ではない。

**次:** 現在の[0,2]範囲で色分離signatureが弱いことを踏まえ、Chromatic Glowを現行レシピの必須要素とみなさず、他のスペクトル／屈折役と区別できるかを写真plateの1:1 cropで確認する。上限拡張は別のAPI/UX判断として扱い、変更する場合はproperty metadataとCPU/GPU係数を揃えてから製品キャプチャをやり直す。現時点でChromatic Glowの候補値やdefaultは採用しない。

## 2026-11-11 — Chromatic Glow Tint Mix response at fixed dispersion

**方法:** Dispersion 2.0、Threshold .6、Radius 7、Intensity .65、Angle 0を固定し、Tint Mix 0 / .25 / .5 / 1.0を同じ960×540 neon fixtureでArtifactUiTest installed-effect CPU reference pathから撮影した。入力とproperty stateを各capture sidecarで確認し、text / tube / ring ROIをinput・output・6x source-differenceで並べた。profileは`glow_chromatic_tintmix_neon_rd1.json`、レビューコードは`glow_chromatic_tintmix_review.py`。

**結果:** 4 captureは同一入力hashでalpha MAE 0。Tint Mix 0.25をreferenceにしたtext ROIの8-bit RGB MAEは0 / .1104 / .1102 / .3313（Tint Mix 0 / .25 / .5 / 1.0）。sourceに対するtext ROIのチャンネル別mean absolute deltaはMix 0でR/G/B = 5.277/5.495/4.828、Mix 1では5.290/5.495/4.744で、Mix全域でhalo responseと色分離の差はわずかだった。全候補のtext ROI new clippingは2.544%で同一、tube ROIは変化なし。Contact sheetでもMixによる明瞭な1:1色縁の優位は認められず、今のproperty候補範囲ではTint MixがChromatic Glowのfamily signatureを解決していない。

**制約・次:** 数値は8-bit encoded RGB product CPU captureの固定ROI測定で、美的ground truthやGPU parityを示さない。現在のDispersion上限2とTint Mix範囲では明瞭なsignatureを示せていない。次は同じ色分散候補を別の実写・点光源素材で確認し、必要ならeffect実装のチャンネルずらし設計とproperty semanticsをソースベースで再評価する。採用値やdefaultは変更しない。Contact sheetとmetricsは`captures/glow-suite-chromatic-tintmix-neon-cpu-rd1/`。

## 2026-11-11 — Suite recipe branches from a shared Glow foundation

**方法:** 同じ960×540 neon fixtureに標準Glow（gain 1.0、4 layers、sigma 6.0）を1度適用し、その出力hashを共有して4つの枝へ分けた。枝は抑制Edge Bloom、Chromatic Glow、Liquid Glow、Luminescence Causticsを各1回追加する。各段階は`ArtifactUiTest`の登録Effect CPU reference pathで処理し、recipe manifestへ入力・出力hash、property sidecar、exe hashを残す。結果画像は`captures/glow-suite-recipes-neon-cpu-rd1/recipe-branches-contact-sheet.png`。

**結果:** 全枝が同じfoundation出力（SHA-256 prefix `13768251a2`）を入力し、全体new-clipped-channelはEdge Bloom .0255%、Chromatic .0879%、Liquid .0723%、Caustics .1393%。全てalpha MAE 0。目視ではEdge Bloomは輪郭halo、Chromaticは文字周りの暖色／青系edge、Liquidはhorizontal tube周囲の波形、Causticsは光輪とtubeのridgeをそれぞれ足した。ひとつのGlowへ重ねても署名が同じhaloへ溶け込まず、effect familyを用途別の選択肢として組み合わせる余地がある。

**制約:** fixtureは一回につき登録Effect ID一つだけをlayerへ適用する。そのため今回はGlow出力PNGを次effectのfixture sourceとして再importするchain proxyであり、8-bit intermediate quantizationが入る。実際のordered float stack、最終Composition Viewer、GPU parityは未確認。これを採用recipeと呼ばず、次にbounded ordered multi-effect fixtureでsame-pipeline comparisonする。Profileとcapture runnerは`glow_suite_recipe_profiles_neon.json`、`glow_suite_recipe_capture.py`。

## 2026-11-11 — High-key cross-check of the Glow recipe suite

**方法:** CC0 `walkway-orbit-cc0.mp4` frame 20（360×640）にGlow gain .05 / sigma 10の共通基盤を作り、10個の非Residual系specialist effectをそれぞれhigh-key restraint profileで追加した。共通base output hashをmanifestで照合し、入力・出力・増幅差分を並べた。capture / profile / metricsは`captures/glow-suite-recipes-walkway-cpu-rd1/`と`glow_suite_recipe_profiles_highkey.json`。

**結果:** 全9枝と基盤でalpha MAE 0。基盤の新規clipped-channelは.726%。追加effect後の同基盤に対するfull-frame new clipはEdge Bloom 2.034%、Reactive 1.738%、Chromatic 3.133%、Liquid 6.121%、Directional 5.787%、Physical Halation 1.446%、Caustics 1.962%、Optical .955%、Volumetric .064%、Glint .466%。Liquid / Directional / Chromaticは単体high-key candidateより上がるケースがあり、個々に抑えたprofileでもhalo foundation上ではheadroomを共有して消費する。Halationはclip 1.45%ながら広い赤cast、Causticsは雪・水面に細かなridge、VolumetricとGlintはこのframeでかなり控えめ。したがってsuite候補はeffectごとの単体値を足し合わせず、共通基盤込みの素材別budgetを要する。

**重要ROI:** ROI別のnew clipはLiquidがbright sky 5.53% / bridge 5.40% / ice-water 6.50% / dark water 3.12%、Directionalが同4.10 / 5.52 / 5.90 / 3.32%、Chromaticが2.30 / 3.39 / 2.43 / 1.31%まで上がった。Full-frame平均だけでは特定ROIのhigh-key飽和を見逃す。4つの固定ROI画像とmetricsは同capture folder内の`recipe-*-roi-comparison.png`、`recipe-roi-metrics.json`。

**制約:** 静止画・CPU reference・8-bit PNGを中間再読込するchain proxyであり、actual ordered float stack、Composition Viewer / GPU、moving residual、SDR/HDR mapping、性能・allocationは未検証。高keyの1 plateだけから普遍的なclip上限や採用profileは決めない。

## 2026-11-11 — Twelve-effect Glow suite role candidates

**範囲:** Glowを単一の汎用ルックとして扱わず、登録済み12系統（Glow、Edge Bloom、Reactive、Chromatic、Liquid、Residual、Directional、Physical Halation、Luminescence Caustics、Optical Glow、Volumetric Shine、Glint Star）それぞれが担う視覚役割を調べる。Still Light Leakは関連する光学トラックだがGlowの一系統とは数えず、別の3ルックとして保持する。

**方法:** `glow_role_candidate_profiles.json` に各系統の役割ベース候補値を記録し、同一ポートレート1536×1024と合成ネオン960×540に対し、登録Effect IDのCPU reference outputを12系統すべて撮影した。各captureでfixtureが設定値を受理したことをstate sidecarで確認し、同一plateのsource hash、出力hash、encoded RGB MAE、linear-luma差、新規clip、alpha MAEを `glow_product_capture_review.py` で集計した。全体contact sheetと同一位置の等倍cropも保存した。Volumetric Shineの光源位置とGlintの4-ray形状は別候補として実アプリcaptureを追加。出力は`captures/glow-suite-role-candidates-{portrait,neon}-cpu-rd1/`と`captures/glow-suite-role-candidates-rd2/`。

**確認:** 全12のCPU candidate outputでalpha MAEは0。neon上ではEdge Bloom MAE .00665 / 新規clip 1.019%、Reactive Glow .00366 / .985%、Optical Glow .00575 / .680%で、強い役割が見える一方、明るい細線に対するclip管理が必要。portraitのEdge Bloomはcandidate値でも全RGB channelの差が1/255を超え、顔・背景に広いhazeを作る。細部cropでBase Glow、Reactive tint、Liquidの波打ち、Caustic ridge、Optical layer、Directional streakの形状が異なることを確認したが、候補が美的に合格したとは判定していない。

**第2候補:** 光源座標をネオンのmagenta practical中心へ合わせたVolumetric Shineは、neon MAE .00079 / new clip .036%、portrait MAE .00015 / new clip 0%で依然控えめだった。Glintを4 rays・長いray・抑えたsoftnessへ変更するとneon上で明瞭な星形になり、MAE .00563 / new clip .680%; portraitではMAE .00059 / new clip .027%。出力は異なるGlowの役割を増やすが、点光源の飽和と構図依存を次の評価に含める。

**判断・制約:** 12系統に同一strengthを当てた順位付けはしない。現状は役割候補と素材依存を調べる段階で、accepted presetはまだない。Residualは単一still captureでhistory/decayを評価できない。GPU/最終Viewer経路、linear/HDR条件、動画安定性、effect別性能・一時resource計測も未評価。NeonのChromatic candidate連続capture中に1回だけ`0xC0000005`で終了したが、同じ条件の即時再captureは成功し、原因は未確定。candidate sidecarsはその成功captureを保持する。

## 2026-11-11 — Four-effect intensity response and Optical Glow component isolation

**方法:** neon plateと他の設定を固定し、Edge Bloom Amount=.25/.40/.55、Reactive Glow Intensity=.25/.40/.55、Optical Glow Primary Intensity=.35/.55/.70、4-ray Glint Intensity=.5/1.0/1.5を登録済みEffect CPU referenceで撮影した。Opticalではprimary haloのみ、afterglow/streak/atmosphereのみ、抑えた複合profileも追加した。設定は`glow_role_intensity_sweep_neon.json`と`glow_optical_component_sweep_neon.json`、出力・contact sheet・測定JSONは`captures/glow-suite-role-intensity-neon-cpu-rd1/`と`captures/glow-suite-optical-components-neon-cpu-rd1/`。

**結果:** 各強度3段階でencoded RGB MAEは単調増加し、alpha MAEは全条件0。source RGBが254未満から254以上へ移ったchannelの割合は、Edge Bloomで .130/.462/1.019%、Reactiveで .026/.127/.985%、Glintで .160/.543/.680%。低い候補でも各形状は確認でき、Edge .25、Reactive .25–.40、Glint .5の範囲ではclipが抑えられた。Optical Primary Intensityを.35/.55/.70に変えてもnew clipは約.678%のまま。構成要素を分けるとprimary-only .668%、secondary-only .511%、抑えた複合 .676%であり、clipは単一Intensityよりhalo・streak・afterglowそれぞれの加算と強いneon lineに分散して由来する。secondary-onlyはclipが少し減る代わりにlayered haloが弱まる。

**判断・制約:** Edge、Reactive、Glintは強度を下げることで形状を保ったまま局所clipを減らせる候補が得られたが、採用閾値はまだ確定していない。OpticalはPrimary Intensityだけを下げてもこの素材のline clippingを解決しないため、component balanceかhighlight headroomの設計比較が必要。clip値はSDR encoded PNGで新規に254以上となったchannel率であり、HDR内部値や視覚的な合否を直接示さない。全結果はneon一枚のCPU referenceで、portrait再確認、GPU parity、最終Viewer、性能測定は未実施。

## 2026-11-11 — Edge Bloom edge-response restraint across two plates

**方法:** Edge BloomのThreshold .80、Threshold Softness .15、Radius 8、Amount .55、Tint Mix .20を固定し、Edge Boostだけを.15/.30にしてportraitとneonの同サイズ製品CPU captureを取得した。比較対象は同条件のrole candidate Edge Boost .75。測定と全体contact sheetは`captures/glow-suite-edgebloom-edgeboost-rd1/`。

**結果:** portraitの顔ROI (0.37,.08)–(.49,.25) のmean linear-luma deltaはEdge Boost .15/.30/.75で .00184/.00365/.00966、full-frameは .00111/.00205/.00529。channel差>1/255の割合は .15で24.32%、.30で72.64%となり、.75 candidateでは全channelが変化していた。newly clipped channelはportrait .006/.009%で、neon .137/.152%。neon contact sheetでは.15でも細いbright contour周りにhaloが残り、背景テクスチャ寄与を下げられた。alpha MAEは全て0。

**判断・制約:** Edge Boost .15は広いportait hazeを大きく減らしつつneon輪郭の候補を残す比較値。ただし濃度・細部・skin responseの美的合格判定ではなく、CPU referenceのみ。GPU resident shader、等倍cropのskin halo、他の高テクスチャplate、processing timeを確認してからdefault変更を決める。

## 2026-11-11 — Cross-plate view of five Glow suite recipes

**方法:** 各plate固有のshared Glow foundationと二つのspecialistを順番に加えた最終CPU outputについて、CC0 aerial city central intersection、portrait streetlamp context、CC0 street-light twin bulbsを同じrecipe順に並べた。各tileはfoundation、recipe final、foundationとの差分を5倍した表示で、source profile / final output hash / ROI / clip gateをJSONに保存する。Scriptとatlasは`glow_suite_crossplate_atlas.py`、`captures/glow-suite-crossplate-atlas-rd1/`。

**観察:** 5 recipe（contour-streak、reactive-spectrum、film-optical、refractive-ridge、point-rays）はcity intersectionの拡大差分で別の寄与位置・色の変化を示し、city版は全て登録済みの1% full-frame / 3% ROI incremental-clip review gate内、alpha差0。portraitのstreetlamp contextとtwin-bulb street-lightでは相対差分が小さく、特にpoint-raysは素材に焼き込まれたビームと重なりやすい。film-opticalのcity差分は暖色寄与を示すが、広い赤castの許容は別途目視が必要。recipeごとのclip gate数値は`SUITE_EVALUATION_PLAN.md`と各plate metrics JSONにある。

**判断・制約:** 今回は三つの素材・異なるROI・各plate別foundation calibrationを通して5 recipeを一望できるようにした。8-bit PNG handoff、固定静止画、installed CPU referenceであり、同一強度や同一foundationのクロスマテリアル比較ではない。atlasは差分を5倍表示するため見え方の採用判定には使わず、元plate・通常表示・等倍ROIと合わせて人が確認する。GPU/final Viewer、float-preserving stack、HDR、時間応答、性能は未検証。現段階ではrecipeを製品presetとして採用しない。

## 2026-11-11 — High-key headroom for five Glow suite recipes

**方法:** CC0 `walkway-orbit-cc0.mp4` frame 20（360×640、橋・氷・明るい空・暗水面）で5つの二段specialist recipeを同じGlow foundation（gain .05、4 layers、baseSigma 10）から順次撮影した。最初は単体系high-key restraint profileを両専門effectへ転用（rd1）。次にclip超過recipeのamplitude controlsだけを25%へ（rd2）、それでも1% full-frame gateを超えた2 recipeだけさらに半分へ（rd3）。geometry、threshold、source/angleなどの構造設定は固定。capture profileは`glow_suite_ordered_stacks_highkey_rd{1,2,3}.json`、CPU installed-effect PNG handoff captureとROI reviewは`captures/glow-suite-ordered-stacks-highkey-cpu-rd{1,2,3}/`。

**結果 — 単体用抑制値をレシピへ転用したrd1:** foundation自身の新規clipped-channelはfull .726%（bridge ROI 1.08%）。foundationとの差分でcontour-streakはfull 10.25% / max ROI 12.94%、reactive-spectrum 7.02 / 7.35%、film-optical 3.40 / 4.21%、refractive-ridge 8.90 / 10.66%、point-rays .62 / .67%。全5 recipe alpha MAE 0。単体effectを個別に抑えたprofileを共通foundation上でそのまま重ねると、4 recipeが1% full / 3% ROI review gateを大きく超える。

**結果 — amplitude 25%のrd2:** reactive-spectrum .85% full / .79% max ROI、film-optical .31 / .50%、point-rays .62 / .67%で数値gate内。contour-streak 1.68 / 2.12%、refractive-ridge 1.50 / 1.50%は全ROI gate内だがfull-frame gateを超過。全てalpha MAE 0。

**結果 — 2 recipeをさらに半分にしたrd3:** contour-streak .707% full / .990% max ROI、refractive-ridge .611 / .661%となり、残る2 recipeも数値gateへ入った。rd2値を維持した3 recipeと合わせ、五つそれぞれにhigh-key-specific headroom候補を得た。ROI sheetでは5倍差分に輪郭・屈折の応答が残るが、通常表示の高キー雪面では役割の認識性が弱い。これは単一画像・一組の手動ROIで、役割を美的に合格とした判定ではない。

**判断・制約:** 役割ごとに違う組み合わせrecipeを持つsuiteは、single universal strengthではなく素材別amplitude calibrationを要するという結果。今回の1% full / 3% ROIは以前登録したclip review gateであり、視覚品質やHDR値域の契約ではない。全て静止8-bit CPU PNG chain proxyで、float-preserving ordered stack、final Viewer、GPU parity、HDR headroom、時間安定性、処理時間/allocationsは未検証。明るい素材上で光源roleが弱い場合、clipが少ないことだけを理由にpreset採用しない。次は高キー背景と実用光源が共存する写真素材でrecipeごとのrole legibilityを測り、適用判断と強度候補を確認する。

## 2026-11-11 — Point-rays role on a clean synthetic night plate

**方法:** baked-in beamsのない決定的な夜景plate `captures/glow-point-rays-probe-rd1/point-rays-probe.png` を作り、Glow foundation後にVolumetric ShineとGlint Starを順に適用した。rd1の弱い出力を見てCPU実装を確認すると、Volumetric Shineのray sampleを`settings.samples`で平均するため、低いintensityでは細い点光源の寄与が小さい。製品コードは変更せず、rd2ではrayLength .85 / intensity 4 / decay .94 / samples 32 / threshold .22とし、Glintはintensity 1、length 40、width 1、softness .55へ調整。capture、ROI sheet、5x difference、metricsは`captures/glow-suite-point-rays-probe-rd2/`。

**結果:** rd1では両specialistの役割が通常表示でほぼ読めなかったが、rd2では主灯から斜めの広がりとGlintの星形が等倍ROIおよび全体表示で判別でき、暗い建物ROIへの変化は0。shared foundationとの差で新規clipはfull .0244%、primary-emitter ROI .4848%、その他ROI 0%、alpha MAE 0。点光源の役割を分離するfixtureとして有効な探索候補になった。

**判断・制約:** rd2は合成plate上の役割可視性を示す探索候補で、写真上の美的採用値ではない。Streak感の方向性や見え方はsource位置・構図依存。captureはCPU installed-effect referenceの8-bit PNG handoffで、GPU parity、最終Viewer、HDR、性能/割当、実写実用灯での適合性は未検証。Volumetric Shine実装にはフル画像scratch copyがあるため、性能レビューは別ゲートとして維持する。

## 2026-11-11 — Point-rays transfer and intensity range on a photographic practical

**方法:** 同じCC0 street-light photo、Glow foundation（gain .05 / 4 layers / sigma 6）、source position (.28,.30)で二つの確認を実施した。synthetic probeの強いrd2値はrayLength .85 / intensity 4 / decay .94 / 32 samples / threshold .22、Glint intensity 1。比較ではVolumetric Shineの強度だけを.5 / 1 / 2へ動かし、rayLength .6、decay .90、40 samples、threshold .45、およびGlint intensity .5など他条件を固定。キャプチャ、sidecar、固定ROI、metricsは`captures/glow-suite-point-rays-streetlight-transfer-rd2/`と`captures/glow-suite-point-rays-streetlight-intensity-sweep-rd1/`。

**結果:** 強いrd2 transferは主灯のspillと光条を明瞭にする一方、元画像に焼き込まれた複数のbeamと重なり、見た目が過剰。full-frame new clip .420%、max fixed ROI 2.787%、alpha MAE 0。固定geometryの強度.5 / 1 / 2ではfull-frame new clip .058 / .067 / .087%、max ROI .471 / .598 / .860%、alpha差0。ROI sheetでは暗い空のluma変化はほぼなく抑制できたが、既存beamのある写真なので合成probeほど役割の新規性を判別できない。低〜中強度は安全な比較域でも視覚的なPoint Raysの差が弱く、高強度はclip gate内でも全体の光学演出を支配した。

**判断・制約:** この実写では単一の強度値をsuite共通候補にしない。source flaresがないか弱い実写practical、異なる位置・画面占有率で役割を分けて見る必要がある。8-bit CPU PNG chain、固定画像のため、GPU/Viewer、HDR、temporal stability、performance/allocation、品質採用は未確認。ライブラリ／アプリへのpreset変更なし。

## 2026-11-11 — Point-rays role on a low-flare portrait practical

**方法:** 既存のAI生成低照度ポートレート（顔、暗色coat、単一の街灯とshopfront practicals、強い焼き込みbeamなし）を使い、既存suiteのGlow foundationとsource位置 (.74,.19) を維持した。Volumetric Shineのintensity .5 / 1 / 2を、rayLength .6・decay .90・40 samples・threshold .45で比較し、Glintは同じ抑制profileへ固定した。さらにsynthetic probeの長いray geometryを移しintensity 4、次に2.4、2.0と強度だけを落として局所clip gateを調整した。capture / ROI / metricsは`captures/glow-suite-point-rays-clean-portrait-{sweep,strong,intermediate,intermediate2}-rd1/`。

**結果:** 弱〜中強度 sweepはfull-frame追加clip .024–.04%、全固定ROIが3%以内だが、通常表示ではpoint-rays roleはかなり控えめ。長いray geometryのintensity 4は街灯ROIで4.954% clip、2.4は3.261%で局所gateを超過。intensity 2.0 / Glint .5はfull .037%、街灯ROI 2.992%、shopfront .812%、face / dark-coat 0%、alpha差0で数値gateを通った。1:1 sheetでは街灯周囲に寄与は出るが、stronger ray appearanceの美的受容を確定できるほど明確ではない。

**判断・制約:** 低flare写真でも高可読なrayへ上げると光源直下がclip budgetを使い切る。現時点ではintensity 2.0長rayを「clip gate内の比較候補」として保持するだけで、採用presetではない。人物一枚、AI生成素材、installed CPU reference、8-bit PNG handoff。GPU / final Viewer、HDR、動画時系列、性能・割当、他の顔・光源構図への転用は未検証。製品コードは変更していない。

## 2026-11-11 — Five ordered Glow recipes on the same low-light portrait

**方法:** 同じ1536×1024 portraitと同一 Glow foundationから、5つの二段recipeを各々順序付きCPU PNG handoffで撮影した。輪郭＋方向（Edge Bloom → Directional）、反応＋色分散（Reactive → Chromatic）、フィルム光学（Halation → Optical Glow）、屈折リッジ（Liquid → Caustics）、点光＋グリント（Volumetric Shine → Glint Star）。Point Raysだけは直前のlong-ray intensity 2.0 / Glint .5候補へ更新し、残る4 recipeは既存portrait設定を固定。capture manifest、5 recipe contact / ROI sheets、x5差分、測定値は`glow_suite_five_recipes_clean_portrait_rd1.json`と`captures/glow-suite-five-recipes-clean-portrait-rd1/`。

**結果:** 5 recipe全てのalpha MAEは0、full-frame incremental new clipは0.025–0.048%、全fixed ROIのnew clipは3%以内。顔・dark-coatのnew clipは全recipe 0%。ROI別mean linear-luma deltaでは、contour-streakはshopfront .00872、reactive-spectrum .00976、film-optical .00787、refractive-ridge .00791に寄与。point-raysはstreetlamp .07534、streetlamp-context .00323、shopfront .01292で光源周辺に集中し、face .00004 / dark coat .00002とほぼ変化しない。full-frame contact sheetの縮小表示では5 recipe間の差は小さいが、各recipeのROI・段別x5差分では寄与位置が分かれる。

**判断・制約:** 同じ素材・基盤上で5組の役割位置とclip/alphaを揃えて比較できたが、全体一覧だけでは低照度の差を視認しにくい。ROI応答は補助的根拠で、5 recipe全ての美的採用を意味しない。AI生成portrait一枚、CPU reference、8-bit handoffのため、second-photo transfer、final Viewer/GPU parity、HDR、時間安定性、性能/allocations、float-preserving stackは未検証。製品統合・既定値変更なし。

## 2026-11-11 — Pairwise spatial overlap of five ordered Glow recipes

**方法:** 共通Glow foundationから分岐した5 recipeの最終出力を同一のfoundation出力との差として linear light で比較し、full frameと固定ROIごとにsigned RGB、sigma 6 broad luma、sigma 3 residual、R-G/B-G chromaのcosine行列とL2 responseを作った。Capture manifestのsource/output hash・ordered pixel input・2段statusを検査し、同じROIのfoundation / recipe final / x5差分をnative crop scaleで横並びにする`glow_suite_recipe_overlap_review.py`を追加。評価対象はlow-flare portraitの5 recipeとCC0 neon-cityのrd3 5 recipe。結果は各captureの`review/overlap/`内JSON、band別CSV、`recipe-role-atlas-*.jpg`。

**観察:** low-flare portraitではstreetlamp ROIのbroad-luma recipe pair cosineが.941–.998、streetlamp-contextで.912–.995、shopfrontで.871–.992と高い。CC0 cityではcentral-intersectionで.964–.993、bright-roadsで.963–.993、dark-city-blocksで.902–.980。red-practicalsでは.776–.949まで広がり、帯域差が大きくなった。portraitのface / dark-coatでは一部recipeのdeltaが非常に小さく、Point Raysと他役の方向類似も低下する一方、街灯に対するL2応答はPoint Raysが目立って大きい。native-scale ROI atlasのportrait shopfrontでは、5x差分にContour / Reactiveの灯具輪郭、Film-Opticalの暖色spill、Refractiveの輪郭・テクスチャ、Point Raysの斜めlineがそれぞれ表れたが、final表示では区別が控えめ。CC0 cityのred-practical atlasではfinal差と5x差分の双方がかなり弱く、recipe profileの役割強度が足りない可能性がある。

**判断・制約:** 共通の明るい入力位置にhaloを作るため、滑らかにぼかした輝度寄与はrecipe間で似やすい。これは視覚的な冗長性の証明ではなく、cosineは素材の光源形状・閾値・強度の影響を受ける。fine/chroma band、L2振幅、各ROIの通常表示を合わせて見て、suite memberの役割が十分区別されるか確認する。2枚の静止画、CPU installed-effect、8-bit PNG handoffであり、float stack / Viewer / GPU / HDR / temporal / performanceは未検証。


## 2026-11-11 — Structural recipe signatures and refractive-ridge intensity on CC0 city

**方法:** CC0 aerial city上の5 ordered recipeで、強度を固定したままDirectional length/angle、Chromatic dispersion/angle、Halation/Opticalの広がりと色、Liquid distortion/scale、Caustics scale/color shift、Point Rays lengthなどの構造値を変えた。profileは`glow_suite_structural_signature_cc0_neon_rd1.json`。captureとnative ROI atlasは`captures/glow-suite-structural-signature-cc0-neon-rd1/`。次に構造値を固定し、refractive-ridgeのLiquidとCaustics intensityを同時に.10/.125/.15へ変えた。profileは`glow_suite_refractive_ridge_intensity_cc0_neon_rd1.json`、captureは`captures/glow-suite-refractive-ridge-intensity-cc0-neon-rd1/`。

**結果:** 構造trialの5 recipeは全てalpha MAE 0、full-frame追加clip .38–.52%、各ROI 3%以下。都市の赤い実用灯では最終画像のrecipe差は引き続き弱い一方、屈折recipeは明るい道路と交差点で細部・色差のresponseが増えた。refractive intensity .10/.125/.15のfull-frame new clipは.449/.571/.693%、最大ROIは2.258/2.845/3.424%。従って.10と.125は既定3% ROI review gate内、.15は交差点で超過。alpha MAEはいずれも0。pairwise cosineはstrength sweep間で1.000となり、これは同じsignatureを線形に拡大した結果で、新たな役割差の増加ではない。

**判断・制約:** .125はclip数値を通り、native x5 differenceでは道路・交差点の微細responseが確認できるが、通常表示のfinal ROIでLiquid ridge / Causticsの見た目は明瞭ではない。赤実用灯ROIへの寄与も小さい。視覚採用ではなく次回比較用のheadroom candidateに留める。.15は現ROI boundを超えるため保留。構造値だけの変更では都市素材の5役の可読性を十分に解決しなかった。全captureはinstalled CPU effectsの8-bit PNG ordered chain proxyであり、GPU、final Viewer、HDR、float-preserving stack、時間挙動、性能は未検証。


## 2026-11-11 — Five Glow recipes transferred to a controlled neon fixture; Point Rays source alignment

**方法:** 既存の決定的 `glow_lab.make_fixture` 由来の960×540 neon plate（magenta ring、白い中心emitter、cyan/amber point、cyan text、長いamber tube、dark field）を用い、CC0 city用の5 ordered structural recipeとshared Glow foundationをそのまま適用した。ROIはfixture形状に合わせた `synthetic_neon` profileをreviewerへ追加。設定・capture・metricsは`glow_suite_structural_signature_synthetic_neon_rd1.json`、`glow_suite_ordered_stack_review.py`、`captures/glow-suite-multirole-synthetic-plate-rd1/`。次にVolumetric Shineのsourceをring中心(.271,.38)へ整列し、rayLength .85 / decay .94 / threshold .22 / 32 samples固定でintensity 2.0（Glint .5）を撮影。さらに同条件でintensity .5 / 1.0を比較した。profilesは`glow_suite_point_rays_synthetic_neon_rd1.json`、`glow_suite_point_rays_synthetic_neon_intensity_rd1.json`、captureは`captures/glow-suite-point-rays-synthetic-neon-{rd1,intensity-rd1}/`。

**結果:** city用5 recipeを制御fixtureへ転用すると、全recipeでalpha MAE 0、full-frame incremental clip .038–.121%、全ROI 3%以内だったが、通常表示では4 recipeの役割差が控えめで、point-rays profileはsource (.52,.58)がfixtureの白色emitter (.271,.38)を外していた。sourceを合わせてintensity 2.0にするとring中心に放射光・starが通常ROI表示で認識できた。ただしtextや別の発光物にもradial streakが広がる。max ROI clipはring 2.672%、text 2.788%、alpha MAE 0。intensity .5 / 1.0はfull clip .171 / .191%、ring linear-luma delta .00203 / .00342。最大ROI clipはどちらもtext 2.787%で、2.0でも同じtext周辺がclip上限に近い。

**判断・制約:** suite recipeは別plateへ数値だけ転用せず、Point Raysならemitter source positionをplateごとに合わせる。2.0は星形の視認性を示す比較候補だが、radiating streakが他の点光源・textへspillし美的合格ではない。.5/.1.0は穏やかだが役割が弱い。残る4 recipeも独立した特性別strengthと視覚レビューが必要であり、共通Glow foundationを重ねるだけでsuiteの多様性は成立しない。全てinstalled CPU reference / 8-bit PNG ordered chain、固定synthetic fixtureの結果。GPU、final Viewer、HDR、float stack、実写転用、時間安定性、性能は未確認。


## 2026-11-11 — Identity-input comparison and Glow zero-gain behavior

**方法:** Specialist-only ordered branches were captured on the exact same synthetic-neon input with `foundation: null`, which now means the unchanged input image is the branch start. The capture utility and reviewer were extended to represent this explicitly, verify stage input pixels, and measure recipe output against source. A separate `glowGain=0` probe was retained under `captures/glow-suite-multirole-synthetic-neon-gain0-probe-rd1/` for diagnosis, then clearly renamed so it is not mistaken for an identity baseline. Valid identity-only five recipe results are in `glow_suite_structural_signature_synthetic_neon_identity_rd1.json` and `captures/glow-suite-multirole-synthetic-neon-identity-rd1/`.

**結果:** The identity capture confirms the five restrained transferred recipes preserve alpha and stay under the existing 1% full / 3% ROI incremental-clip review gates; full-frame added clip ranges 0.000–0.066%, max ROI ranges 0.000–1.287%. At the same specialist settings, adding the shared Glow foundation at gain .05 changes the five final RGB images only slightly (encoded MAE 0.000146–0.000152; mean RGB delta within the text ROI 0.64–0.70 code). This means the weak visual differentiation on this fixture is primarily a specialist-profile/role-visibility issue, not evidence that the shared foundation dominates it.

**実装事実と評価上の修正:** `Artifact/src/Effects/Glow/GlowEffect.cpp` CPU path multiplies the threshold mask by `glowGain_` only when `glowGain_ > 0`; at zero it skips that multiplier, so zero is not a neutral identity. The installed CPU capture sidecar accepts and reports zero, but the image differs from input (encoded RGB MAE .002438, alpha MAE 0). This documents the observed CPU path only; GPU parity for zero remains unverified. The product implementation was not changed. The valid comparison therefore starts recipes directly from input and does not route through Glow with gain zero.

**判断・制約:** A shared Glow foundation is optional in the suite model; it is not required to make specialist combinations. The matched capture indicates the current low-contrast result persists with or without the small (.05) foundation. The identity fixture is synthetic and the recipe values remain exploratory; four roles are still too weak in normal view. The gain-zero code path may merit an independent product behavior review before any future UI contract is inferred. All specialist capture is installed CPU reference with 8-bit PNG stage handoff; GPU/final Viewer, HDR, float-preserving multi-effect stack, temporal behavior, and performance remain open.


## 2026-11-11 — Specialist-forward Glow suite on textured emissive materials

**方法:** `glow_suite_material_fixture.py`で決定的960×540 plateを生成した。従来fixtureのring/text/point/tubeは維持し、負空間に暗いレンガ壁とwet pavementの低輝度波紋を重ね、bright emissionを焼き込まず屈折対象の素材感を加えた。Input hashはcapture manifestに記録。既存のspecialist-forward 5 recipe（各2 specialist、共通Glowなし、Point Rays位置整列）を同一入力へ撮影。recipe/ROI/metricsは`glow_suite_specialist_forward_material_rd1.json`、`captures/glow-suite-specialist-forward-material-rd1/`。追加で各ROIのレンズ効果spillを測れる`material_neon` ROI profileをreviewerへ追加した。

**結果:** 5 recipeすべてalpha MAE 0、full-frame new clip .154–.247%、max fixed ROI 2.747–2.787%。発光text ROIは共通して2.8%近くclipし、現review bound内でも余裕が少ない。wet-road ROIのmean linear-luma deltaはcontour .000010、reactive .000018、film-optical .000026、refractive-ridge .000066、point-rays .001301。brick-wall ROIもpoint-rays .000287、ほかはほぼ0。1:1 contact sheetではpoint-raysの放射尾がroadへ伸びる一方、残り4種のtexture応答は控えめ。refractive-ridgeの5x差分はring/text/tube輪郭に色付きridgeを示すが、normal outputではそのsignatureはまだ弱い。

**判断・制約:** suite専門recipeは共通Glow抜きでも素材に応じて反応し、Point Raysとrefractive detailの空間応答差を測れた。ただし強度を上げたprofileでも、現在の素材で強く読めるのは主にpoint-raysで、text ROIはclip予算上限近く。屈折効果はテクスチャの存在だけで視覚合格にならず、normal-view legibility・色縁・明るい細線保持を見ながらfamily-specific intensityを再調整する。fixtureは暗い壁／路面と発光図形の決定的synthetic plateであり実写transferではない。CPU installed-effect PNG chain、GPU/Viewer/HDR/float stack/performance未確認。製品コード・default変更なし。


## 2026-11-11 — Eleven standalone Glow-family candidates on textured emissive material

**方法:** 同じ`glow-suite-material-fixture-rd1/source.png`から共通Glowなしで11 registered family candidateを個別分岐 captureした。従来の`glow_role_candidate_profiles.json`を使い、Volumetric sourceだけ白emitter位置(.271,.38)に合わせた。静止画ではhistory behaviorを検証できないためResidual Glowは除外。single-stage recipeを支援するようordered stack reviewerを一般化し、source・identity・effect output contact sheet、fixed ROI、stage delta、clip/alphaを生成。profileとoutputは`glow_suite_standalone_families_material_rd1.json`、`captures/glow-suite-standalone-families-material-rd1/`。

**結果:** 全11出力でalpha MAE 0。既存候補値ではGlow / Chromatic / Liquid / Physical Halation / Caustics / Volumetric / Glintの7つが現clip review gate内（full-frame .000–.174%、max ROI .000–2.787%）。Edge Bloomは1.020% full / 5.646% max ROI、Reactive Glow .985 / 5.410%、Directional Glow .681 / 5.555%、Optical Glow .680 / 5.648%でgate超過。1:1 ROI sheetではEdge Bloomが輪郭halo、Directionalがline-streak、Glintが局所星形、Causticsが輪郭ridgeの差分を出す一方、Chromatic / Halation / Caustics / Volumetricのcurrent valuesはnormal-viewのrole legibilityが低い素材領域がある。Full-frame overviewは11候補を同じ素材・identity inputから並べるgalleryとして有用だが、縮小一覧だけでは差を判断できない。

**判断・制約:** 11 familyは同じ「Glow amount」値を共有するものではなく、形状・色・時間挙動が異なる専門look候補である。現candidate tableは高強度role explorationを含むため、4候補はheadroom tuneが必要。7つの数値gate passも美的合格ではない。Residual Glowは別途multi-frame fixtureで評価する。単一synthetic plate、CPU installed effects、8-bit PNG handoffのみ。GPU/Viewer/HDR/float stack/performance・実写transfer未検証。製品default / library codeに反映なし。

## 2026-11-11 — Edge Bloom edge-boost sweep on textured emissive material

**方法:** textured emissive material plate上でEdge Bloom単体系を比較。threshold .8、softness .15、radius 8、Amount .55、Tint .2を固定し、Edge Boostのみ .15 / .30 / .75 とした。Glow foundationなしのidentity inputから分岐。8-bit CPU captures、ROI review、clip/alpha metricsは`glow_suite_edgebloom_material_edgeboost_rd1.json`と`captures/glow-suite-edgebloom-material-edgeboost-rd1/`。

**結果:** alpha MAEは全条件0。Edge Boost .15 / .30 / .75のfull-frame追加clipは .138 / .147 / 1.020%、max ROIは2.787 / 2.787 / 5.646%。.30はリング・チューブの応答が.15より明瞭で、レンガ壁のlinear-luma deltaは.00068（changed 93.6%）だった。.75では壁delta .00151、changed 100%となり、チューブROIも5.646% clipでgate超過。通常表示では.30の局所haloを識別できるが、文字縁はやや柔らかい。画像は`captures/glow-suite-edgebloom-material-edgeboost-rd1/review/ordered-stack-edgeboost-0p30-roi-sheet.jpg`。

**判断・制約:** Edge Boost .30をこのfixtureにおける数値・視覚の比較候補として記録する。採用presetではない。強いEdge Boostは背景textureまで応答を広げ、局所clipを増やす傾向が見えた。synthetic material一枚、installed CPU reference、8-bit PNG。写真への転用、GPU/final Viewer、HDR、時間安定性、性能/割当は未確認。製品ライブラリ／アプリの変更なし。

## 2026-11-11 — Chromatic Glow as a distinct suite role on textured emissive material

**方法:** 同じ960×540素材プレートとinstalled CPU referenceで、Chromatic GlowのDispersion 0 / 1 / 2をTint Mix 0と.25の二条件で比較した。Threshold .8、Radius 8、Intensity .65、Angle 0は固定。さらにTint Mix 0、Dispersion 2のままIntensity .50と.65を、各強度に一致するDispersion 0 baselineと比較した。capture profileは`glow_suite_chromatic_material_tintmix{0,025}_rd1.json`と`glow_suite_chromatic_material_intensity050{,_baseline}_rd1.json`。素材用ROIを`glow_chromatic_dispersion_capture_review.py`に追加し、ring/text/points/tube/brick/wet-roadを記録した。

**結果:** Tint Mix 0のD2 vs D0 red-blue responseはemissive-text ROIで平均0.801 8-bit code、Tint Mix .25では0.601。linear-luma変化は小さく、色縁は差分画像で読み取りやすい。Intensityを.65から.50へ下げたmatched D2 vs D0比較では、text ROI new clipが3.181%から2.774%へ下がり、red-blue responseは.619 code残った。full-frame new clip .113%、ring .234%、tube 0%、brick .060%、alpha MAE 0。画像は`captures/glow-suite-chromatic-material-intensity050-rd1/review/chromatic-intensity-0.50-text-comparison.jpg`。

**判断・制約:** Tint Mix 0 / Dispersion 2 / Intensity .50をこのsynthetic material plate上のChromatic specialist比較候補として記録する。色分散という独自役割は数値差と拡大ROIで確認でき、既存3% ROI clip review boundも通るが、通常画像での色縁は控えめであり、美的採用ではない。素材は決定的synthetic plate、8-bit installed CPU captures、manual ROI。写真転用、GPU / final Viewer、HDR、performance・allocationは未確認。製品値／presetは変更していない。

## 2026-11-11 — Seven standalone Glow-family roles on one CC0 city photo

**方法:** provenance-recorded `cc0-neon-streets-1536.jpg`を共通入力にし、各familyをidentity inputから1 effectだけ適用してArtifactUiTest installed CPU pathで撮影した。Edge Bloom、Reactive Glow、Chromatic Glow、Directional Glow、Physical Halation、Luminescence Caustics、4-ray Glintの7設定を使用。source / output hash、properties sidecar、全体contact sheet、交差点・上部灯火・下部サインの3つの手動等倍ROIを保存。profileは`glow_suite_family_city_singletons_rd1.json`、captureとreviewは`captures/glow-suite-family-city-singletons-rd1/`。

**結果:** 7 captureは同じ入力hash、1536×1025、alpha MAE 0。full-frame new clipはEdge .688%、Reactive .421%、Chromatic .796%、Directional 1.068%、Halation .185%、Caustics .422%、Glint 1.160%。3 ROIの最大new clipはそれぞれ3.190%、2.231%、3.863%、4.652%、.920%、1.645%、5.345%。交差点ROIでは赤い拡散(Halation)、輪郭の細い構造(Caustics)、長い筋(Direction)、色分離(Chromatic)が差分像で異なる。縮小contactだけでは差が弱く、source / output / x4差分の等倍ROIが役割の判断に必要。contactは`review/family-city-singletons-contact.jpg`、詳細は`review/family-city-singletons-intersection-practicals.jpg`など3 ROI sheet。

**判断・制約:** 同じ写真でも各familyは空間応答と色方向の異なる候補を作り、Glowを単一強度で代表させないスイート評価の根拠が増えた。ただしEdge / Chromatic / Directional / Glintは交差点ROIで現行3% review boundを超え、Directional / Glintはfull-frame 1%も超過。7つとも採用ルックではない。全てinstalled CPU referenceの8-bit画像で、通常viewの嗜好判定、final Viewer、GPU parity、HDR、時間安定性、性能・allocationは未確認。Residual Glowはmulti-frame capture未実施。

**追加確認:** Directional Glow Intensity .15ではcentral practical ROI clip 3.046%で基準をわずかに超えたため、.14へ下げた出力を追加取得した。`.14`のproduct image、source PNG、state sidecar、frame diagnosticsは生成され、sidecarは`compute=0`とIntensity `.14`を記録。画像測定ではfull-frame clip .632%、最大ROI 2.878%、alpha MAE 0。capture wrapperは別のUI screenshotを得られず終了コード1となったため、この候補は出力ファイルとsidecarによる参考測定に留め、完全なcapture成功とは扱わない。証跡は`captures/glow-suite-family-city-directional-streak-rd2/`。

## 2026-11-11 — Street-light plate transfer of the standalone family

**方法:** 出典記録済みCC0 `cc0-street-light-night.jpg`（576×1280、SHA-256 `b01e663da4cada43f95b936d88ed377244680b56c41929d2d621d087d7224eb8`）から7系統を個別に撮影: Edge Bloom、Reactive Glow、Chromatic Glow、Directional Glow、Physical Halation、Luminescence Caustics、4-ray Glint。Glow基盤を重ねず、city lineupの抑制設定を転用したroundと、形状・しきい値・色設定を固定して各系統の振幅だけ2倍にしたvisibility probeを比較。両方ともArtifactUiTest installed CPU output。手動ROIは左右ランプの正規化座標left [.195,.325,.315,.395]、right [.585,.325,.705,.395]。全体contact、各ランプinput/output/5×signed RGB difference、source/output hash、alpha・clip・ROI RGB MAEを保存。profileは`glow_suite_family_streetlight_singletons_rd1.json`と`glow_suite_family_streetlight_rolepop_rd1.json`、出力は同名の`captures/glow-suite-family-streetlight-*-rd1/`。

**結果:** 全captureでalpha MAEは0。抑制roundではfull-frame new clipped-channelが0.10%未満だが、lamp ROIはfamily別に差がある: Chromatic 4.37% / 3.88%、Directional 3.70% / 3.34%、Edge 3.61% / 3.25%、Glint 3.41% / 3.10%、Reactive 3.12% / 2.84%、Caustics 2.46% / 1.06%、Halation 0.41% / 0.47%（left/right）。2倍roundのlamp ROIはChromatic 6.19% / 5.39%、Directional 4.88% / 4.28%、Edge 4.77% / 4.17%、Glint 4.61% / 3.99%、Reactive 4.21% / 3.66%、Caustics 4.02% / 1.30%、Halation 0.72% / 0.71%。2倍でもwhole-frame新規clipは0.14%未満だが、全体画像ではロール差が縮小時に弱い。等倍ROI差分ではChromaticの青/cyan fringe、Halationの赤い拡散、Causticsの局所構造を確認。EdgeとReactiveはこの素材上で似た広がり、Directional/Glintも通常全体表示で署名が弱い。

**判断・制約:** 夜景cityでの強度は別の光源露出にそのまま移せず、全体平均clipだけだと小さな飽和光源のheadroomを隠す。2倍は役割を見やすくする一方、複数familyのlamp ROIでclipが増えるため採用値ではない。ROI clipはこの入力の局所測定であり、汎用合否閾値にはしない。別途撮影した`flash-photo-plate.png`のportrait transferは、`assets/SOURCES.md`に出典記録が見当たらないため、このCC0評価と採用判断から除外した。次はfamilyごとにthreshold/exposure responseを探し、native-scale signature visibilityとROI headroomの境界を比較する。CPU 8-bit、最終Viewer/GPU、HDR、時間安定性、性能/allocationは未検証。
