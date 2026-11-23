# Retro Image Lab Research Notes

**最終更新:** 2026-11-11

## 実験 070: OldTV CPU/GPU source equation とHDR clamp差

### 問い

ArtifactのCPU OldTVはCore adapterへ、GPUは独立HLSLへ分岐する。自然alpha素材上で、GPU HLSLのjitter・scanline・noise・alpha sample・saturateを式モデル化した時、Coreのfringe-onlyモデルとどこに差が出るか。特に0〜1外のfloat値でclamp差がどの程度現れるか。

### 条件と方法

- 実験069と同じprovenance manifest / SHA-256検証済みCC0 cutout 2点を使用。
- Artifact HLSLの整数hash、row jitter、scanline、noise、alpha同一座標sample、RGB saturateをNumPy uint32/float32で実装。GPU実機ではなく式のCPU translation。
- CPU比較側はCore OldTVのfringe=.3のみを適用し、curve/flicker/scanlineを無効化したsource-equation isolation。HLSL側は該当shader全体を使うため、これは同設定のCPU/GPU parity比較ではない。
- RGB domains: normalized 8-bit values `[0,1]`、synthetic HDR `[0,2]`、synthetic negative/HDR `[-.5,1.5]`。後二つは元のvisible RGBをscale/offsetした値域probeで、alpha coverageと素材構造を維持。
- Straight linear-value source-overをblack/mid-gray/white背景に適用。色転送・alpha associationは契約未確認のため計測仮定。
- boundary ROIのCPU-vs-GPU RGB MAE/P95/max、alpha changed rate、HLSL low/high clip count、hidden-RGB pair差を出力。GPU hashは座標全体、詳細RGB統計はROI/alpha regionで計算。
- 再実行: `python tools/core_oldtv_natural_alpha_study.py`。JSONは `captures/core-oldtv-natural-alpha/core-oldtv-natural-alpha-metrics.json`。

### source-equation probe結果

| 素材 / RGB domain | Core fringe-only vs GPU HLSL edge RGB MAE | P95 | max | GPUで変わるvisible alpha比 | HLSL RGB clip low / high画素数 |
|---|---:|---:|---:|---:|---:|
| Clover full `[0,1]` | .09475 | .55808 | .99774 | .5011% | 2,375,641 / 0 |
| Macro `[0,1]` | .07611 | .30751 | .99486 | .2102% | 1,167,916 / 59 |
| Clover full `[0,2]` | .19998 | 1.00000 | 1.67059 | .5011% | 2,375,641 / 29,081 |
| Macro `[0,2]` | .22878 | .78039 | 1.99093 | .2102% | 1,167,784 / 1,179,193 |
| Clover full `[-.5,1.5]` | .32179 | .89608 | 1.50000 | .5011% | 5,125,901 / 168 |
| Macro `[-.5,1.5]` | .20960 | .50000 | 1.50000 | .2102% | 2,460,396 / 450,851 |

- HLSL row jitterは両画像で約6.36〜6.38%のrowに発生し、int cast後の観測shiftは−4〜+4px。alphaはGPUでRGBと一緒にsampleされるため、可視destinationの0.21〜0.50%で値が変化した。Core modelはalpha destination値を維持。
- 8-bit domainでもnoiseがblack/暗部側へ値を押すためHLSLのlow saturateが作動した。8-bit入力だからclampは無関係、とは言えない。数は透明pixelのRGBを含む全channel samplesであり、可視/edgeの欠陥pixel数ではない。
- HDR/negative domainではsaturateによるclipが増え、source-domain差も拡大した。ただし実製品callerがHDR/negative値を渡すかは未確認。

### 限界と判断

- この数値はsource-equation model間の差であり、画像品質評価やGPU実機parityではない。CPU側がfringe-only、GPU側がHLSL full effectという非対称条件で、whole-effect visual rankingやCPU/GPU誤差の合否には使えない。
- HLSL `hash`のuint32式は移植したが、GPUコンパイラのfloat演算順序・texture format・dispatch/runtimeを検証していない。CPU側のCore Perlin flicker/curvature/scanlineはこの比較へ含まれない。
- 8-bit alpha/RGB fixtureはsource-overの比較値を得るため使う。straight linear compositingとsynthetic HDR offsetは仮定であり、production alpha/color contractの証拠ではない。CC0素材は2点だけ。
- Core/Artifact source修正は行わない。次はCoreのNoiseGenerator既定permutation依存を解消せず、まずcallerとSurfaceColorContractから実入力range・transfer・alpha associationを特定する。parity改善を論じるならCore既定出力も同じ時間条件でモデル化し、source-equation完全比較の後に許可されたruntime captureを検討する。

## 実験 071: GPU OldTV alpha sampling差の背景合成への伝播

### 問いと比較

HLSL GPU modelはjitter後の座標からRGBAすべてをsampleする。alphaの変化は元の背景との合成でどの程度見えるか。GPU出力RGBを固定してalphaだけdestination値へ戻すcounterfactualを、GPUモデル出力と比較する。

### 手順

- 実験069/070と同じ2点のCC0素材、provenance hash検証済み。
- RGB normalized `[0,1]`。Artifact HLSLのGPU equation model（jitter/scanline/noise/saturate）を使用。
- straight-alpha source-overをblack / 0.5 mid-gray / white背景で合成。`GPU alpha held at destination` は出力RGBを変えずalphaだけ入力alphaへ戻したcounterfactual。
- ROIは元alphaのpartial-alphaを含む輪郭帯。表示用sheetではmid-gray合成、元画像・Core fringe-only・GPU model・GPU alpha-held・alpha差分×4を並べた。
- 出力: `captures/core-oldtv-natural-alpha/core-oldtv-gpu-alpha-composite-sheet.png` と同metrics JSON内の `straight-alpha composite sensitivity probes`。

### 結果

| 素材 | 背景 | edge composite MAE | P95 | max | >1e-6差 pixel比 |
|---|---|---:|---:|---:|---:|
| clover full | black | .00729 | 0 | .72264 | 3.90% |
| clover full | mid-gray | .00645 | 0 | .50000 | 4.65% |
| clover full | white | .01918 | 0 | 1.00000 | 4.65% |
| macro | black | .01114 | 0 | .99697 | 4.53% |
| macro | mid-gray | .00437 | 0 | .49697 | 4.68% |
| macro | white | .00825 | 0 | .97485 | 4.68% |

- visible destination alpha change率はfull .501%、macro .210%。edge compositeで1e-6超の差は約3.9〜4.7%に現れ、平均に対して最大差が大きく局所化した。P95=0は差分がROI pixel数の5%未満であるためであり、差が無いことを意味しない。
- 背景によりMAE/maxが変わる。alpha移動は暗背景へhalo/coverage差、明背景へ別のedge差として現れるため、背景1色の合成結果だけでは代表できない。
- contact sheetではGPU hash-jitter / scanline look自体がCore fringe-onlyと大きく異なる。alpha-held列との小さな差分画像はalpha policyのcounterfactual成分を分離する補助であり、製品で使うべき画像を示さない。

### 限界と次の判断

- SurfaceColorContractはcanonical linear-premultiplied surfaceとstraight surfaceの両方を表現でき、layer blend accumulatorからstraight viewport outputへの変換shaderも存在する。しかし、このOldTV GPU入力がどのalphaMode/transfer/domainで供給されるかは、このsource reviewだけでは確定できない。ここでのstraight linear compositingはcounterfactual。
- GPU式モデルとcounterfactualは実device outputではない。素材は2点。acceptance threshold、承認済みlook、runtime/backend capture、performanceは未評価。
- Core/Artifact変更は行わない。実callerのsurface descriptorを追跡できるまで、alpha移動を不具合または改善とは判定しない。次はOldTVが受け取る入力生成/texture uploadのalpha contractをコード経路で確認する。

## 実験 072: Image LayerからOldTV GPU uploadまでのalpha契約を追跡

### 調査範囲

自然cutoutの実測条件を製品経路へ結び付けるため、Image Layerの非連番画像decode、in-memory QImage入力、input interpretation、effect GPU uploadのdescriptor伝播をsource reviewした。Artifact.exe / build / testは使用していない。

### 確認できた経路

| 経路 | pixels / alpha handling | SurfaceColorDescriptorの状態 | 確認範囲 |
|---|---|---|---|
| Image Layer async still-image reader | OpenImageIO `FLOAT` decodeのRGBAを `setFromRGBA32F` へ渡す。alpha association変換なしで受け取る。Source metadataは `oiio:UnassociatedAlpha` を読み、既定値1（unassociated）。input transform時に metadata がassociatedならunpremultiply→transform→premultiply。 | `setFromRGBA32F` defaultでalphaMode / transfer / range不明。Input transformが無い場合もUnknownのまま。 | `loadImagePairViaAsyncReader` / `loadImagePairFromMemory`、`SourceImageMetadata::fromSpec`、`applyInputInterpretation`。実ファイルのOIIO metadata値は素材依存。 |
| Image Layer in-memory `setFromQImage` / sequence frame | QImageを `Format_ARGB32_Premultiplied` に変換し、そのBGRA値をfloatへ。 | sRGB transfer + Premultiplied + DisplayReferredを明記。 | `toFrameBuffer`。 |
| Artifact OldTV GPU input | `runCreativeCompute` はsurface dataをBGRAでuploadする。`makeGpuImageUploadBuffer`はdescriptorを保持するが、HLSLへalphaMode/transfer/rangeを渡す定数はない。HLSLはraw RGBAをjitter sampleし、RGB scanline/noise/saturate後もsampled alphaを出力。 | upload時 descriptorは利用可能だがshaderは参照しない。未知alphaModeも入力可能。 | `ArtifactOldTVEffect::apply` → `runCreativeCompute` → `makeGpuImageUploadBuffer` → `kOldTVComputeHlsl`。 |
| Render/blend surface contract | canonical accumulatorはlinear premultipliedとして定義。layer-to-float shaderは入力rgbをalphaで割ってstraight化しsaturate。viewport shaderはpremultiplied BGRA accumulatorをstraight RGBAへ変換。 | 変換境界は明示されるが、effectごとの入力descriptor一致は別問題。 | `SurfaceColorContract.ixx`、`LayerBlendComputeShader.ixx`。 |

### 結論

- OldTV GPU effect境界のalpha associationは一意に確定しない。一般のstill decodeではsource metadataとinput interpretationに依存し、descriptorがUnknownのままのケースがあり得る。in-memory QImage経路は明示premultiplied。したがって前実験のstraight-alpha合成は一般的なcounterfactualであり、すべてのImage Layer入力に対応する実契約ではない。
- GPU shaderはdescriptor-awareではない。CPU bridgeもImageF32x4からVideoFrameへ生channel値をcopyし、alpha associationを正規化せずCore処理する。source associationが混在すれば、fringe/jitterの色縁差も入力表現で変わりうる。
- premultiplied sourceでの妥当な入力例は別途parameterized modelを設ける必要がある。unknown descriptorを見ただけで全still素材をstraight、またはpremultipliedと断定しない。
- 本件では処理を変更しない。次のモデル比較ではsame visible cutoutをstraight / premultiplied value representationでそれぞれ符号化し、sampling後のRGB-alpha invariantと背景合成感度を評価する。実入力例の結論にはcaller固有のmetadata記録かruntime captureが必要。

## 実験 073: OldTV GPU式をstraight / premultiplied入力値で比較

### 問いと条件

OldTV GPU shaderが同じRGBA座標をjitter sampleする性質は、straightとpremultipliedで共通か。scanline/noiseを含むHLSL式を両representationに同じく適用し、alpha-zero hidden RGBの合成差とpremult invariantを調べる。入力representationは比較parameterであり、製品経路の選択を主張しない。

- 実験069〜072と同じSHA検証済みCC0素材2点。
- Straight: decoded RGBそのまま。Premultiplied: RGB×alpha、alphaは同じ。GPU hash jitter、scanline、noise、saturateを含む既存NumPy式モデル。
- Straight outputはstraight source-over、premult outputはpremult source-overとしてblack/mid-gray/whiteへ合成。
- alpha-zero RGBだけ異なるpairを作り、premult encodingではalpha=0で両variantのRGBが0になることを確認。
- 数値JSONの `alpha association parameterization probes` に集約。

### 観測

| 素材 | encoding | 可視alpha変化率 | GPU出力でalpha=0かつRGB非zero | hidden-RGB pairのedge合成差（MAE / changed ratio） |
|---|---|---:|---:|---:|
| clover full | straight | .501% | 795,517 px | 0 / 0 |
| clover full | premultiplied | .501% | 794,982 px | 0 / 0 |
| macro | straight | .210% | 404,330 px | 0 / 0 |
| macro | premultiplied | .210% | 396,948 px | 0 / 0 |

- GPU shaderはRGBとalphaを同じjitter座標からsampleするため、straight encodingでもalpha-zero sampleのRGBはalpha-zeroと一緒に出力され、standard straight composite上のhidden-RGB pair差は0になった。これはCore fringe (.3)がRGB channelだけを動かしalphaをdestinationに残す前実験と異なる。
- premult入力では入力RGB≤alphaを満たすが、GPU noiseはalphaでscaleされずRGBへ加算される。このモデルではoutputの約397k〜795k画素でalpha-zero pixelに非zero RGBが残り、premult invariant (RGB≤alpha) 違反画素も約397k〜795kだった。これはsource-overのpremultiplied合成で透明pixel色が残り得ることを示す式上のリスクで、実device artifactの発生を意味しない。
- 可視edgeにおけるGPU出力対入力のpremult composite MAEは背景別に約.014〜.029、変更pixel比は約78〜79%。GPU全体のjitter / scanline / noise変化を含むため、alpha-association単独の誤差とは解釈しない。

### 限界と判断

- Source-equation modelのみ。OpenImageIO実loaderが個々のファイルをどのassociationで出力するか、input interpretationによるworking surface、OldTV後にどのalpha modeでcompositeされるかはsource metadataとcallerに依存する。Runtime image captureなし。
- HLSLがdescriptorを参照しないことは確認済み。descriptor preservationはupload boundaryで行われるが、shader内RGB/noise計算はalphaModeに応じて変わらない。
- Core/Artifact sourceは変更しない。実素材でinput descriptorとpost-effect descriptorを同じcaptureへ記録し、premult invariant violationが本当にcompositorへ届くか確認できるまでは不具合判定を保留する。

### 下流 descriptor consumer の追加source追跡

- `ArtifactCoreCreativeEffect::apply` はsurfaceのRGBA channel値を `MultiChannelImage` へそのままコピーしてCore処理し、戻りも `DeepCopy` で元surface descriptorを引き継ぐ。CPU経路でもalpha associationの正規化は行わない。
- `ImageF32x4_RGBA::toQImage` の `convertSurfacePixels(Rgba8SrgbStraight)` はPremultiplied descriptorの場合だけunassociateし、Straight/UnknownはRGB値をそのまま扱う。alpha=0では出力RGBを0にする。
- `LayerBlendComputeShader` の `layerToFloatShaderText` はdescriptorを引数に取らず、alphaでRGBを割ってstraight化する。alphaが極小ならRGBを0にし、その後saturateする。
- 以上から、CPU bridge出力のdescriptor保持と各consumerのalpha前提には非対称がある。ただし、このOldTV出力が当該 `layerToFloatShaderText` 経路へ実際に接続されるcaller経路は未追跡であり、製品不具合とは断定しない。
- 次に必要なのはeffect outputから具体的なblend/upload consumerまでのcaller追跡、または許可されたruntime captureでdescriptorとpixelを同時観測すること。現段階ではlab内のsource-contract懸念として記録し、実装は変更しない。

## 実験 078: 固定paletteの遷移とsource errorを既存VOA写真で補う

### 対象と利用範囲

前回の固定palette試験はcutout / illustration / pixel-art atlas中心だったため、連続色・肌・グリーン背景を含む写真素材を追加して同じ指標を測る。素材は既存 `eval-corpus/voa/132819fcfd733c00.jpg`（540×960、2秒位置のVOA Learning English video frame、SHA-256一致）。

- `tests/Artifact/fixtures/keying/README.md`とcorpus manifestをsource-of-recordにする。VOA Learning Englishの[公式利用案内](https://learningenglish.voanews.com/p/6861.html)は教育用を含む本文・写真・動画のpublic domainを案内しているが、対象[Commons file page](https://commons.wikimedia.org/wiki/File:Filming_with_the_green_screen.webm)はlicense review caveatを残しているため、実験素材をクリア済みCC0とは扱わない。
- この素材は内部 exploratory useだけ。contact sheetや派生画像を再配布しない。
- 3 fixed palettes ×2 distance domain ×Bayer strength0/.5/1、計18条件をphoto上で追加。既存3 CC0 assetも合わせた72 conditionを再実行。
- Full opaque photoなのでalpha-edge/partial-alpha ROIは存在せず、visible image全体のpalette occupancy、隣接遷移、source RGB errorを評価。alphaの自然素材知見は先行3点のCC0 cutout/illustrationへ限定する。
- RGBは現行ラボ同様sRGB code assumptionで読み、RMSEはsRGB decode後linear RGB。source metadataを実行時ICC変換せず、このPNG/JPEG code valueをそのまま比較。

### 観測

| Palette / distance | Strength 0: used colors / effective colors / transition rate / linear RMSE | Strength 1: used colors / effective colors / transition rate / linear RMSE |
|---|---|---|
| Game Boy / sRGB | 4/4 / 3.1 / 15.0% / .3589 | 4/4 / 3.5 / 65.9% / .3771 |
| Game Boy / linear | 4/4 / 3.2 / 15.4% / .3585 | 4/4 / 3.3 / 51.0% / .3681 |
| CGA / sRGB | 4/4 / 3.2 / 12.2% / .3276 | 4/4 / 3.4 / 46.9% / .3829 |
| CGA / linear | 4/4 / 2.9 / 10.6% / .2650 | 4/4 / 2.8 / 31.1% / .3130 |
| PICO-8 / sRGB | 14/16 / 5.9 / 20.5% / .1801 | 15/16 / 6.9 / 76.2% / .2057 |
| PICO-8 / linear | 16/16 / 7.4 / 21.0% / .1351 | 16/16 / 7.3 / 66.7% / .1685 |

`used/effective colors`は実palette色数とentropy-derived effective count。transition rateは水平隣接visible pixelのpalette indexが切り替わる比率。RMSEは写真への忠実度の説明量でありretro quality scoreではない。

- Photoは全4色が使われたGame Boy/CGAでもeffective color countが3前後に集中。PICO-8は距離domainにより、no-ditherでも14色と16色へusageが分かれた。
- Strength 0→1で全6 palette/domain組のtransition rateが増え、linear RGB RMSEも増えた。最大のtransition increaseはPICO-8/sRGBで20.5%→76.2%。
- Linear distanceのRMSEは6組すべてでsRGB distanceより低かったが、これはRMSE自体がlinear RGBで定義されているため、palette lookの優位性を示さない。
- FeatherのCGA/sRGBでは遷移増加と同時にRMSEが少し下がったのに対し、このphotoの全組はstrength 1で誤差増加。dither応答は素材とpaletteに依存する。

### 判断と限界

- 連続色写真でも固定paletteのdomain選択は色分布と局所遷移を大きく変える。cutout、soft-alpha illustration、pixel-art atlas、写真の4系統を同じ共通指標で並べられるようになった。
- VOA素材は1 frameで license review caveatが残り、分布の代表とはみなさない。CC0 photo assetsが追加されるまでは公開・再配布素材の品質結論へ広げない。
- Core変更やpalette既定値変更は行わない。実行条件は `python tools/palette_occupancy_transition_study.py`、結果は `captures/palette-occupancy-transitions/`。

## 実験 079: 適応palette median-cutのalpha重みと色domain

### 問いと条件

固定paletteではなく画像ごとに色を抽出する場合、透明度をpalette学習時にどう扱うか、sRGB/linearどちらの色domainで箱を分割するかを比較する。

- `tools/adaptive_palette_weighting_study.py`。SHA-256検証済みCC0素材3系統（white clover cutout、silver feather illustration、pixel-art hair atlas）、4/8/16色。
- 独立median-cut: visible RGB8のunique colorを作り、最大RGB range軸を重み付き中央値で反復分割、各boxを重み付きcentroidで代表させる。weightはvisible-pixel frequencyまたはsource alpha coverage。palette assignmentは構築と同じsRGB/linear Euclidean domain。ordered ditherなし。
- fixed PICO-8/sRGB/no-ditherを比較基準として追加。palette色をsource alphaへ戻し、alphaが完全一致することを確認。黒/灰/白へlinear-light合成しsRGB表示したvisible/partial-alpha/edge ROI誤差を記録する。
- 再実行: `python tools/adaptive_palette_weighting_study.py`。JSONと3枚のcontact sheetは `captures/adaptive-palette-weighting/`。

### 観測

- 36 adaptive条件＋3 fixed-PICO比較を生成。全3素材で元alphaを完全保持し、要求palette色数をすべて使用した。
- Silver featherの4色条件ではalpha coverage重みがpartial-alpha/mid-gray表示RMSEをvisible-frequency .0360から.0266へ下げた。16色sRGBでは.0084から.0091へ僅かに上がる。重み付けの効果はpalette容量によって変わる。
- Cloverの16色linearはalpha weightで.0234から.0269へ増加。pixel hair atlasはpartial-alpha pixelが全体の.223%だけで、同条件4/8/16色のalpha-weightとfrequencyの結果が概ね一致した。
- 16色adaptive sRGBのsilver featherはpartial-alpha/mid-gray RMSE .0084、fixed PICO-8は.0478。これはこの一素材/この指標でのsource fidelity差であり、palette lookやretro品質順位ではない。
- 適応4色linearはclover partial-alpha RMSE .0473、sRGBは.0707だった一方、silver featherではlinear .0366、sRGB .0360とほぼ同じ。色domainも素材依存。
- Contact sheetを目視確認。銀羽根では適応paletteが微細な明暗/羽軸を残し、PICO-8固定色は紫・青など別の色調を与える。誤差最小化だけでは意図するretro paletteの選択を代替しない。

### 限界・判断

- RGB code valuesをsRGBと仮定し、embedded ICC/gAMAは適用していない。median-cutはこのラボで明示した研究アルゴリズムで、製品/Coreの実装を模していない。色差は知覚均等でなく、dither・時間安定性・複数画像を跨ぐpalette一貫性・速度は評価外。
- alpha weightingの普遍的推奨は出ない。特定素材用paletteの候補生成には使えるが、Coreへ一般既定値や変更を反映する根拠には不足する。実製品へ進めるなら、artist-authored paletteとの使い分けと、同一素材を跨ぐpalette安定性を先に評価する。

### 追加測定: 背景合成後の表示コード誤差

- black / mid-gray / whiteの3背景で、linear-light source-over後にsRGBへencodeする経路と、sRGB code値で直接source-overする経路を分けて測定した。alphaはsource/outputで共通。
- 3つのCC0素材のPICO-8/linear/no-dither partial-alpha ROIで、linear-light経路の表示sRGB code RMSEは、clover .08170/.06576/.05023、silver feather .05204/.04894/.04551、pixel hair .11493/.01955/.00820（black/gray/white）。同じ条件の直接sRGB code合成では各素材とも背景間で同値（約.06605/.04807/.07363）。
- これは合成後の数値誤差がtransfer curveを通して背景依存になることを示す。source-over直後のlinear RGB誤差はalphaが同じなら背景項が相殺される。表示code値の誤差とlinear値の誤差を混同しない。
- 指標は元画像との差の記述で、背景ごとの品質順位ではない。合成式のcounterfactual比較でありArtifactStudioの実際のpreview/export合成経路を示さない。VOA写真は完全不透明のため本追加測定もCC0のpartial-alpha領域に限定する。

## 実験 080: 適応palette再学習によるパン中の色ちらつき

### 問いと条件

実験079で素材ごとのadaptive paletteが静止画像の誤差を下げ得る一方、各frameで色割当てを再学習すると同じworld pixelの出力色が時間的に変化するかを確認する。

- `tools/adaptive_palette_temporal_stability_study.py`。同じ3 SHA-verified CC0素材を固定sourceとして使用。各画像で利用可能なpan travelを覆う重複square cropを蛇行順に配置（clover 45、silver feather 4、hair atlas 24 crop）。crop間隔は最大crop幅の半分で、overlap範囲は素材形状により異なる。
- 8色、alpha-coverage weighted、sRGB median-cutを各cropで学習する方式と、source全体から一度だけ学習して全cropへ共有する方式を比較。dither/resamplingは無効、alphaを完全保持。
- 隣接cropのworld-coordinate overlap内で、同じsource pixelのRGB8 palette出力変化・>1/255 changed-visible-pixel比を測定。alpha-edge/partial-alpha ROIも個別集計。共有paletteは決定的point mappingの対照。
- Cloverには可視pixelがないcropが1個あり、empty cropではfull-source paletteへfallback。ここはlocal palette再学習条件でないことをJSONへ明記した。
- 再実行: `python tools/adaptive_palette_temporal_stability_study.py`。JSONと各素材の比較sheetは `captures/adaptive-palette-temporal-stability/`。

### 観測

- Per-frame paletteの隣接frame changed-visible-pixel比（正規化sRGB差 >1/255、各overlap平均）はclover .6082、silver feather .3108、pixel hair atlas .3726。shared paletteは全て0で、同じ入力pixelへの決定的mappingを維持した。
- 同一入力world pixelのper-frame RGB8 RMSE（adjacent overlap平均）はclover 4.011 code、silver feather 2.734、pixel hair 5.443。alpha-edge ROIではそれぞれ6.446 / 2.646 / 5.706 code。shared paletteは全ROIでRMSE 0。
- Contact sheetではcrop位置が移るにつれてfeatherのlocal palette tone/outputが揺れる一方、shot-shared paletteは安定することを目視確認した。画像が静止画の連続cropであるため、自然なscene motionや露出変化は混ざらない。

### 限界・判断

- これはpalette learningとpoint mappingの決定的モデルであり動画処理のruntime captureではない。changed-pixel率は時間ちらつきの数値代理で、知覚的flicker合否ではない。3素材のみ、sRGB code assumption、dither・resize・codecなし。
- Per-frame paletteは静止素材ごとの色適応に有効でも、動画像ではpalette flickerを生む可能性がこのモデルで示された。Coreへ移すならshot-wide palette、固定palette、または時間正則化が候補になるが、どれを採るべきかは仕様/画質評価がなく決められないため製品変更はしない。

## 実験 081: Palette slot対応とEMA平滑化の時間安定性・source error

### 問いと条件

実験080ではframe-local palette更新による同一pixelの色変化を確認した。palette色を時間方向に平滑化すると変化を減らせるか、source fidelityの変化と合わせて比較する。

- `tools/adaptive_palette_temporal_smoothing_study.py`。実験080と同じ3 CC0 still-pan sequence、8色alpha-weighted sRGB median-cutを使用。
- 各frame paletteを前frameのpalette slot順へ最小コストone-to-one RGB matchingで整列し、その後にEMA factor .25/.50を適用。Raw per-frame paletteとfull-source shot-shared paletteも比較。EMAはsRGB palette code value上の補間。
- 同一world-coordinate overlapのRGB8 RMSE / >1 code changed-pixel率を測定。mid-gray背景へのlinear-light composite後の表示sRGB source RMSEもvisible/partial-alpha/edge ROI別に評価する。
- 再実行: `python tools/adaptive_palette_temporal_smoothing_study.py`。JSONと各素材sheetは `captures/adaptive-palette-temporal-smoothing/`。

### 観測

- EMA .25で同一world pixelの平均changed-visible比はsilver feather .3108→.0091へ減り、visible overlap RGB8 RMSEは2.734→1.309 code、alpha-edge RMSEは2.646→1.240。source mid-gray composite RMSEはvisible .0185→.0186、partial-alpha .0179→.0180と僅かに増えた。
- CloverではEMA .25のchanged比が.6082→.6083で下がらなかったが、overlap RMSEは4.011→3.273、alpha-edge RMSEは6.446→2.559へ低下。EMA .50はchanged比.7588、RMSE4.042とrawより悪化した。
- Pixel hair atlasではEMA .25でchanged比が.3726→.5348、overlap RMSE5.443→7.107へ増加。shot-shared paletteのsource visible RMSEは.0233、raw per-frameは.0216、EMA .25は.0273。
- Shot-shared paletteは3素材全てで同一pixelのmapping変化0。source visible RMSEはclover .0354（raw .0221）、silver .0181（raw .0185）、hair .0233（raw .0216）。安定性とsource fidelityの関係も素材ごとに異なる。
- Contact sheetではsilver featherのEMA出力差は小さく視覚的にも落ち着く一方、palette adaptationによる見えの差も残ることを確認。承認済みlook targetはない。

### 限界・判断

- Slot assignmentはpalette色間の最小総距離による本研究の数学的対応付けで、色のsemantic identityを保証しない。EMAはsRGB補間、実動画ではなく同じstill image crop、知覚flicker・motion/resize/dither・runtime/performance未評価。
- EMA効果は素材依存で、同じ係数がsilverでは安定、hairでは不安定だった。shot-sharedも静止source全体の色分布へ適応する選択で、複数sceneにまたがるpalette契約やGPU履歴所有者は定めていない。Core実装や既定値変更の根拠には不足し、現段階ではLab内研究に留める。

## 実験 082: EMA補間domainとworld-locked Bayer併用

### 問いと条件

実験081で見たEMAの素材差について、palette interpolation domain (sRGB/linear) とordered ditherが時間変化/source fidelity tradeoffを変えるかを分けて測る。

- `tools/adaptive_palette_domain_dither_study.py`。同じ3 CC0 pan sequence、8色、alpha-weighted median cut、slot alignment、EMA .25、per-frame/shot-shared baseline。
- EMA palette interpolationだけをsRGB code値／linear lightで切り替える。nearest palette assignmentは全条件でsRGB固定。両EMAにBayer 4×4 variantを追加し、source RGBを `0.5 * median nearest-neighbor palette spacing` だけmatrixに応じて摂動する。matrix phaseは各crop originを加えたworld/source座標固定。
- source errorはmid-grayへのlinear-light composite後のdisplay sRGB。temporal outputは隣接crop overlapの同一world pixelsでRGB8 RMSEと正規化code delta >1/255比を測る。
- Bayer振幅は実装探索用counterfactualであり、既存製品値や推奨係数ではない。
- 再実行: `python tools/adaptive_palette_domain_dither_study.py`。JSONと3素材sheetは `captures/adaptive-palette-domain-dither/`。

### 観測

- Silver featherではEMA sRGB/linearが同値（changed .0091、visible overlap RMSE1.309 code、edge RMSE1.240）。Bayer併用後もchanged .0092、RMSE1.295/edge1.214とほぼ同じだが、source display RMSEはvisible .0186→.0207、partial .0180→.0199へ増えた。
- Hair atlasではlinear EMAがsRGB EMAよりchanged .5348→.5147、overlap RMSE7.107→6.886へ小幅低下したが、source visible RMSEは.0273→.0274と改善しない。Bayer併用はchangedをsRGB .5990 / linear .5817へ上げ、source visible RMSEも.0340/.0339へ増やした。
- CloverではsRGB EMAのchanged .6083に対しlinear EMA .6225。Bayerはそれぞれ.6091/.6221。linear interpolationによるalpha-edge RMSE低下は小さいが、source visible RMSE .0255→.0252の範囲。
- Contact sheetでsilverの各EMA/dither rowを目視確認。small crop previewsでは色domain差が微小であり、これのみを根拠に見た目の改善とは判定しない。

### 限界・判断

- Ditherはpalette entryからの最近傍距離中央値を振幅尺度にする独自モデルで、画面固定matrixではなくsource/world-locked。実装のordered dither式と同等とは主張しない。ROIは3素材で、画質判定者/閾値、実video motion、resize、GPU/runtime/performanceなし。
- EMA interpolation domainは同じslot pathでも素材ごとの差が小さいか逆転した。SilverのBayer併用はadjacent RMSEを僅かに下げるがchanged ratioはほぼ同じで、source errorは増加。Hairでは時間変化とsource errorが悪化。今回の係数/行列式をCoreへ反映する根拠はなく、実製品targetを定めたlook/temporal評価を先に行う。

## 実験 083: Bayer振幅とworld/screen phaseがpalette EMAの時間変化へ与える影響

### 問いと条件

実験082のordered ditherはpalette spacingの0.5倍だけだった。振幅を広げ、screen/crop-fixedとsource/world-fixed Bayer phaseがpan中のsame-pixel output stabilityを分けるか測定する。

- `tools/adaptive_palette_dither_phase_sweep.py`。実験080–082と同じ3 CC0素材、8色alpha-weighted sRGB median-cut、slot-aligned EMA .25 pan sequence。
- Strength .25/.5/1.0 × phase source/world-locked vs crop/screen-locked。Perturbation amplitudeは`strength × median nearest-neighbor palette spacing`（spacing/perturbationともsRGB code domain、RGB channelsへscalarで加算）、4×4 Bayer。Per-frame no-dither / EMA no-dither / shot-shared no-ditherをcontrolとする。
- source errorはlinear-light mid-gray composite後のdisplay sRGB、時間変化は同一world-pixel overlapで>1 code changed ratio・RGB8 RMSEを測る。alpha保持を各frameで確認。
- JSONには全crop originとBayer phase `(x mod 4, y mod 4)`を記録。silver featherの4 crop originは全て`(0,0)` phaseであり、このassetではsource/screen phase条件が同じになる。
- 再実行: `python tools/adaptive_palette_dither_phase_sweep.py`。結果と3 contact sheetは `captures/adaptive-palette-dither-phase-sweep/`。

### 観測

- 振幅定義を修正: sourceはsRGB code valuesでBayer摂動するため、palette spacingもsRGB RGB8-normalized distanceで測るよう統一した。linear RGB spacingを使った初回数値は無効で、この項目の記載値は修正後の再実行結果。
- CloverではEMA no-dither changed ratio .6083、source/world Bayerはstrength .25/.5/1で.6090/.6091/.6091。screen phaseでは.6112/.6234/.6433へ増え、strength1/screen adjacent RGB8 RMSEは4.449（EMA baseline3.273）。source visible RMSEはworld strength1で.0282、no-dither .0255。
- Pixel hair atlasではsource/world ditherもstrengthとともにchanged ratioを.5491/.5990/.6858へ上げ、screen phaseは.5762/.6424/.8070へ上げた。strength1/screen RGB8 RMSE14.983でEMA no-dither7.107、source visible RMSE.0489でEMA no-dither .0273。
- Silver featherでは全crop origin mod4が等しいためsource/screen phase結果が同じ。Bayer strength .25/.5/1のchanged ratio .0090/.0092/.0100とEMA no-dither .0091はほぼ同一だが、visible source RMSEは.0186から.0192/.0207/.0259へ増えた。
- Hair atlasのlocal crop originsには複数mod4位相があるため、screen-fixed phaseがworld-fixedよりchanged ratioを更に押し上げた。位置合わせ済みのstatic image cropでは位相差のみを分離し、object motionは含まない。
- Contact sheetを確認。Bayer強度増加に伴うhair atlasのpixel色変化が目立つ。dither texture自体が意図したlookかどうかの承認基準はない。

### 限界・判断

- Dither amplitudeとRGB scalar perturbationは独自探索モデルで、production Bayer quantizerと同じ処理ではない。screen-locked phaseはcrop座標resetで代理し、実transform/view mappingは扱わない。素材数3、視覚acceptance、GPU/runtime/performanceは未評価。
- このmodelではBayerがpalette update flickerを一貫して抑えず、特にpixel-art hairでsource error/temporal changesを増加させた。strength/phaseをCoreへ反映する根拠にはならない。次はpalette smoothing plus resize/scale phase interactionを測り、static source-coordinate conclusionの限界を確認する。

## 実験 084: BOX縮小phaseとadaptive palette EMAの時間安定性

### 問いと条件

実験083はfull-resolution cropのpalette remapでBayer phaseを見た。実suiteのpixelationはBOX縮小→nearest拡大を行うため、source sampling gridのphase変化がpalette再学習とshot-shared mappingへどう伝わるか確認する。

- `tools/adaptive_palette_resize_phase_study.py`。同じ3 SHA-verified CC0素材、384² source crop、各scaleで9位置の水平pan。scale .5/.75、8色alpha-weighted sRGB median-cutを低解像度側へ適用し、元サイズへnearest upscale、alphaはsource値を保持。
- Phase-locked pathはcrop originをscale denominator周期（.5なら2px、.75なら4px）へ揃える。Phase-cycling pathは同じpan travel・同じ端点で途中originをsource pixel単位でずらし、resampling grid phaseを変える。y originは両path共通。
- Per-frame palette, slot-aligned sRGB EMA .25, scale-specific whole-source shared paletteを比較。同じsource world-coordinate overlapのRGB8 RMSE/changed ratio、mid-gray composite後source display sRGB RMSEを測る。
- RGBA素材はRGB/alpha planeを独立にBOX縮小するstraight-resize model。Quantizationはlow-res pixelへ適用してからnearest upscaleする。RetroImageLab `apply_retro`のdownscale-then-nearest-upscale順をpalette mappingと組み合わせた研究modelだが、product runtimeやGPU parityではない。Ditherはphase原因を分けるため無効。
- 再実行: `python tools/adaptive_palette_resize_phase_study.py`。scale別contact sheetとmetrics JSONは `captures/adaptive-palette-resize-phase/`。

### 観測

- Phase-lockedでは同じworld footprintのBOX samplesが揃い、3素材×2 scalesのshot-shared palette temporal changed ratioは全て0。Phase-cyclingでは固定paletteでもresampling inputが変わり、source-pixel flicker proxyが発生した。
- Hair atlas / .5ではper-frame changed ratio .2395→.9401、EMA .1169→.7910、shot-shared 0→.6865（locked→cycling）。shot-shared visible source errorは.1418→.1491。palette smoothingではsampling-phase由来の変化を取り除けない。
- Hair atlas / .75ではper-frame .0125→.6219、EMA .0017→.3827、shot-shared 0→.3418。scale-locked EMAでは高安定だった条件も、phase-cyclingで変化が大幅に戻る。
- Silver feather / .5ではper-frame .0479→.4781、EMA .0011→.4512、shot-shared 0→.3521。Silver / .75ではper-frame .0014→.4903、EMA .0013→.3600、shot-shared 0→.2206。
- Clover / .5 phase cyclingではper-frame .6498、EMA .5784、shot-shared .2764。Clover / .75では.6583/.5431/.1665。phase-locked同scaleのshot-sharedは0。
- Phase-lockedでもEMAの改善は一定ではない。Clover/.75のchanged ratioはper-frame .2941からEMA .3810へ増え、Silver/.5はEMA changed .0011でもadjacent RMSE .35から.58 codeへ上昇（量子化値が1 code未満動くpixel changesを減らす一方、残るchanged pixelsの平均距離が増える）。changed ratioとRMSEを併記する必要がある。
- Silver feather/.75のphase比較sheetを確認。phase-locked rowsは同じ細部samplingが続き、phase-cyclingはsource cropの変位とsampling位相が切り替わる。素材の静止panモデルで、scene motionはない。

### 限界・判断

- Source/image cropsのみ、実写動画・moving object・subpixel motion・dither・GPU/runtime未評価。phase cyclingは整数source-pixel crop-origin perturbationで、製品のtransform samplingを完全には再現しない。straight RGB BOX縮小のため他alpha resize policyは含まない。
- Palette更新だけでなくBOX resampling phaseも時間変化へ寄与する。Coreへ進める際はpalette stateだけでなくdownscale grid originをworld/layer/output座標にどう固定するかを別仕様として定義し、実callerとpreview/export経路で検証する必要がある。今回は製品コード変更なし。

## 実験 085: Alpha associationと縮小phaseの相互作用

### 問いと条件

実験084でstraight RGB/alpha BOX縮小に限定した。透明RGBの混入を抑えるpremultiplied縮小は、縮小paletteのedge fidelityとpan時のphase由来変化をどう変えるか。

- `tools/adaptive_palette_alpha_resize_study.py`。実験084と同じ3 SHA-verified CC0素材、384px crops、9位置pan、scale .5/.75、phase locked/cyclingを使用。
- StraightはRGBとalphaを別々にBOX縮小。PremultipliedはRGB×alphaとalphaを縮小後、低解像度RGBをunassociateしてからpalette学習／mappingする。どちらもnearest upscale後にoriginal full-resolution alphaを復元する。
- 各associationで8色alpha-weighted sRGB median-cutのper-frame、slot-aligned EMA .25、full-source shot-sharedを比較。temporal差は隣接panの同一world pixelで測る。
- 最初のcropではstraight/premultiplied出力差を、linear-light source-overでblack/mid-gray/whiteへ合成して境界別に測る。Ditherなし。
- 実行: `python tools/adaptive_palette_alpha_resize_study.py`。metrics/contact sheetは `captures/adaptive-palette-alpha-resize/`。

### 観測

- Hair atlas/.5のphase-cycling shot-shared changed-visible ratioはstraight .6865、premultiplied .6355。EMAは.7910→.7129。一方、Clover/.5 shot-sharedも.2764→.1976と低下したが、Silver feather/.75は.2206→.2220でほぼ同じ。associationの時間効果は素材／scale依存。
- Hair atlas/.75ではphase-locked per-frame changed ratioがstraight .0125、premultiplied .1901とpremultiplied側が大きい。shot-shared自体は同phaseで0だが、local palette再学習は縮小色分布の変化を拾う。
- 同じ最初のcrop・shot-shared paletteでもedge RGBはstraight/premultipliedで異なる。Hair atlas alpha-edge raw RGB8 RMSEはscale .5/.75で73.285/83.498 code、mid-gray linear composite edge sRGB RMSEは.06741/.04837。Silverはraw約22 code、Cloverは約18–25 code。associationの差はalpha境界に限定されず、透明pixel RGBが低解像度filterに入る範囲へ及ぶ。
- Final alphaは両方式で元alphaとbit-exact一致。黒/灰/白背景を分けたJSON指標は背景色による表示差を含む。
- Contact sheetを確認。Silver feather/.75は縮小後の羽根の明るい細線が両方式で概ね維持され、premultiplied rowの部分alpha輪郭に小さな差がある。表示背景はmid-gray。

### 限界・判断

- Python/Pillow modelであり、premultiplied RGBをBOX前にuint8へ丸める。Artifact/Coreのfilter、GPU精度、製品alpha規約との一致は未確認。適応palette学習も組み合わせたため、結果を一般的なresize品質順位とは扱わない。
- Premultipliedは一部のphase-cycling変化を下げたが、全素材・scaleで優位ではない。Core変更の根拠は未成立。次はpaletteなしの単純 resize と透明edge fixtureでassociation差だけを分離する。

## 実験 086: Paletteなしalpha resize associationの切り分け

### 問いと条件

実験085では適応paletteが色差へ介在した。BOX縮小→nearest復元だけにし、straight/premultiplied RGB associationと、source alpha保持／縮小alpha復元を独立に比較する。

- `tools/alpha_resize_association_isolation_study.py`。実験084/085と同じ3 SHA-verified CC0 RGBA素材、384px crop、9位置pan、scale .5/.75、phase fixed/cycling。
- RGBはstraightまたはRGB×alphaをBOX縮小しnearest復元。Premultipliedはfilter後にalphaでunassociate。Alphaはsourceのfull-resolution値を保持する条件と、BOX縮小したalphaをnearest復元する条件を比較。
- 8-bit sourceに対するlinear-light composite sRGB RMSEをblack/mid-gray/white背景別・visible/partial/alpha-edge別に記録。alpha changed ratio、edge RGB8 RMSE、同一world visible RGB changed ratio/RMSEも測定。
- 実行: `python tools/alpha_resize_association_isolation_study.py`。metrics JSON/contact sheetsは `captures/alpha-resize-association-isolation/`。

### 観測

- Phase-locked panでは3素材×2 scales×2 associationすべて、same-world RGB changed ratioは0。phase cyclingではpremultipliedが全6 asset/scaleでchanged ratioを低下。例: silver feather/.75はstraight .5434→premultiplied .4922、pixel hair/.5は.8420→.7898。これはsampling phaseが変わるpanだけの数値。
- Output alphaの変更はRGB flicker proxyを変えなかった（どちらも同じRGB mapを使用）。Source-alpha保持ならalpha difference 0だが、BOX→nearest alphaは銀羽根/.5のサンプルcropで33.64%のpixels、clover/.5で1.30%、hair/.5で6.23%のalpha codeを変更した。
- 最初のphase-locked cropのmid-gray composite alpha-edge RMSEでは、clover straight/.5がsource-alpha保持 .0325、resize-alpha .0248、premultipliedは.0094→.0154。hair/.5はstraight .1030→.1062、premultiplied .0769→.0941。縮小alphaの良否は素材とassociationで反転した。
- Silver featherはpartial alphaが広い: .5 resize-alphaでalpha codeが変化する画素はcropの33.64%。それでもmid-gray edge RMSE差はstraight .0341→.0348と小さい。全体RMSEだけではalpha変化の空間的な見え方を伝えにくい。
- Contact sheetsはsource-alpha保持条件のscale違いを灰色背景で確認。正確なsource-error比較は背景別/ROI別metrics JSONを参照。

### 限界・判断

- Pillow uint8 BOXを使う独立モデル。production Core resize順、premultiplied filter precision、preview/export、知覚評価、GPU parity/performanceは未確認。CC0素材3点だけで一般化しない。
- alpha associationはphase-cycling RGB安定性を改善した一方、alpha復元を変えるとsource fidelityは一律に改善しない。Coreで採用判断するには処理の目的（元alpha厳密保持か、area-resampled coverageか）と実際のfilter contractが必要。次は色とalphaを制御したsynthetic edge chartで、細線・gap・部分alphaのfilter footprintを掃引する。

## 実験 087: Hidden RGBとalpha復元を制御したresize chart

### 問いと条件

実験086のCC0素材は色、hidden RGB、edge geometryが交絡していた。alpha-zero RGBを既知値に固定したsynthetic chartで、straight/premultiplied RGB resizeとsource/resized alphaの効果を分ける。

- `tools/alpha_resize_synthetic_edge_chart.py` が768×512 RGBA fixtureを決定的に生成。hard colored blocks、antialiased ellipse/diagonal、1–8px strand、1/2/4/8px gap、4段のpartial alphaを含み、alpha=0のRGBを全て(9,240,17)へ上書き。
- `tools/alpha_resize_synthetic_study.py` はBOX→nearest resize only（palette/effectなし）、scale .25/.375/.5/.75/1、straight/premultiplied RGBとsource alpha保持/resized alpha復元のfactorial比較を実施。
- source-overはlinear lightでblack/mid-gray/whiteへ合成し、whole imageおよびhard/curve/strand/gap ROIのvisible/partial/alpha-edge sRGB RMSEを記録。alpha changed ratioも計測。
- 実行: `python tools/alpha_resize_synthetic_edge_chart.py`、`python tools/alpha_resize_synthetic_study.py`。fixtureは `fixtures/alpha-resize-edge-chart.png`、metrics/sheetsは `captures/alpha-resize-association-isolation/`。

### 観測

- Scale .5のwhole image mid-gray alpha-edge RMSEはstraight/source-alpha .0332、straight/resized-alpha .0543、premultiplied/source-alpha .0077、premultiplied/resized-alpha .0503。hidden RGB色がvisible/partial edge footprintへ混ざるためstraight RGB resizeで色誤差が大きく、premultiplyで大幅低下した。source alpha保持がこのchartのreferenceとの誤差を抑えた。
- Scale .25/.375/.75でもpremultiplied/source-alpha条件がalpha-edge RMSE最小: .0114/.0093/.0059。Scale1はstraight/source-alphaが0、premultiply/source-alphaはuint8 premultiply/unassociate丸めにより.0008。Identity scaleで完全同値にならないのはこのlabの量子化手順上のartifact。
- Source alphaに対しBOX→nearest alpha復元が変えたpixel ratioはscale .25/.375/.5/.75で9.84/8.50/8.35/4.77%。これは線・曲線・半透明sampleを含む全chartの率で、差分が一様な領域占有率を意味しない。
- Gap ROIのmid-gray edge errorもscale .5でstraight/source .0243、premultiplied/source .0062。hidden RGBが異色の細い空間に色fringeとして広がる機序を支持する。contact sheetをblack/gray/white合成で目視し、細い白/紫strandと赤青gapが縮小率で消失・結合することを確認した。

### 限界・判断

- Synthetic geometryは実写品質やユーザーの見た目嗜好を表さない。Pillow BOX uint8 modelで、Coreの実filterやalpha contractを示さない。scale1の微差もモデルのrounding artifact。
- このchartではsource-alpha保持/premultiplied RGBがbest source fidelityだが、実素材3点のalpha errorは別傾向を示した。Core変更前に正規resize APIのRGB association・alpha policy・input/output descriptorをソースで追跡する必要がある。今回はコード変更なし。

## 実験 088: Core RGBA resizeのalpha契約ソース監査

### 問いと範囲

実験085–087のモデル差がCoreのどのresize APIに対応するか、既存実装とcall siteを読み取り専用で確認する。ビルド／テスト／アプリ起動は行わない。

- `ArtifactCore/src/Image/ImageF32x4_RGBA.cpp::ImageF32x4_RGBA::resize` は `cv::resize(impl_->mat_, resized, cv::Size(width, height))` を呼ぶ。interpolation flagは省略されるのでOpenCV default (`INTER_LINEAR`) が使われ、4 channelを同一cv::Matとしてresizeする。alpha-specific processing、premultiply/unpremultiply、alpha restoreはこの関数本体にない。
- Resize後は `impl_->mat_` のみを置換し、`colorDescriptor_`を更新しない。従ってalpha association metadataは入力descriptorのまま保持される。behaviorはstraight/premultiplied別処理ではなく、channelwise interpolation。
- Descriptor defaultは`unknownBgra32Float()`で、alphaModeはUnknown。`SurfaceColorContract.ixx`は別途Unknown/Straight/Premultiplied/Coverageを定義する。resize関数はdescriptorを参照しない。
- 読み取れたcall site例: `LayerMatte.cpp` はサイズ不一致のmatteをcopy後resize。`ParametricComposition.cpp` はresolved inputをtarget sizeへresize。これらはresize APIを使う事実で、各実データのdescriptor/hidden RGBや見た目の問題を証明しない。
- Embedded Parametric Composition Image/Matte bindingは`ImageF32x4_RGBA`値を直接copyし、`DeepCopy()`とcopy ctorはdescriptorを保持する。SourceLayer resolver (`Artifact/src/Composition/ArtifactParametricCompositionResolver.cpp`) は `source.toQImage()`→任意で`QImage::scaled(...SmoothTransformation)`→RGBA8888→no-descriptor `setFromRGBA8()` bridgeを経由し、結果alphaMode/transferはUnknownとなる。targetSizeが同じならCore resize自体は追加で通らない。この経路はresampling＋metadata boundaryの事実だが、visible fringe等の実障害の証拠ではない。
- `toQImage()`自体は`convertSurfacePixels(..., Rgba8SrgbStraight)`を呼ぶため、出力pixel contractは8-bit sRGB straight。SourceLayer resolverでこれをQImage scaleし、戻りの`setFromRGBA8()`ではその既知契約をdescriptorへ再設定していない。Metadata Unknownとpixel encoding不明は同義ではない。
- さらに`SurfacePixelConversion`のsRGB outputはalpha<=1e-6のRGBをzeroする（Unknown/Straight/Opaqueのdirect-sRGB branch）。したがってSourceLayer resolverのQImage scale前には完全transparent pixelの任意hidden RGB値は残らない。透明RGBを変えた実験089をこの特定のQImage経路へそのまま当てはめるのは不適切。部分alpha RGB edge filteringと実scaled outputは未検証。
- ArtifactImageLayerのOIIO float decode pathsも`setFromRGBA32F()` no-descriptor overloadを使い、derived-image cache decodeは`setFromBGRA32F()` no-descriptor overloadを使う。Core API既定descriptorはUnknown。各ファイルのalpha encodingがstraightかpremultipliedかは呼び出しだけでは確定しない。
- ただしArtifactImageLayerは別の`SourceImageMetadata::alphaAssociated` fieldをOIIO specから読み取り/保存し、`applyInputInterpretation()`でOCIO変換の前にunpremultiply、後にpremultiplyする。ImageF32 descriptorとasset source metadataは別契約。条件（設定されたinput color space/transfer）によってはこの解釈処理を通らない。
- Core `ImageImporter::loadRawImageViaOIIO` は`oiio:UnassociatedAlpha=1`をread configへ設定し、RawImage pixel valuesをimplicit premultiplyしない方針をコメントで明記する。これはこのCore importer経路の仕様であり、Artifact側の別OIIO loaderやcacheが同じ条件を使う証拠ではない。
- 同じ`ArtifactImageLayer.cpp`の`toFrameBuffer(QImage)`経路はQImageを`Format_ARGB32_Premultiplied`へ変換し、`legacyOpenCvBgra32Float(sRGB, Premultiplied)` descriptorを明示している。従ってunknown-alpha状態は全image layer pathに共通ではなく、取り込み経路により異なる。
- 参照: `ImageF32x4_RGBA.cpp:289-294`, `ImageF32x4_RGBA.cpp:61-63`, `SurfaceColorContract.ixx:39-59`, `LayerMatte.cpp:126-131`, `ParametricComposition.cpp:1374-1378`。

### 観測・判断

- Core implementation behavior is confirmed by source: default OpenCV linear channelwise resize; no alpha association policy; descriptor carried through unchanged.
- これは実験087のBOX modelとはfilter kernelが異なり、結果の数値を移植できない。Premultiplied dataならchannelwise linear interpolationは数式上associative representationを保つが、straight dataのalpha edgeでは透明RGB混合の影響があり得る。どのcall siteがどのalphaModeを実際に渡すかは別問題。
- APIを変える前にcallerが渡す実descriptor、straight/premul invariants、alpha coverage要件とresize用途（matte / image / composition input）を個別に調べる必要がある。現時点でCore変更の根拠は未成立。

### 限界・次

- Source auditのみ。OpenCV runtime version/flags, boundary behavior, pixel output, caller fixture, GPU equivalence/performanceは実行確認していない。
- 次はMatteとParametric Compositionのresize直前descriptor生成/伝播を追跡し、alphaModeが既知か、straight/premultipliedが混在するかをcallsite単位で表にする。変更しない。

## 実験 092: 正確なscene motionでgrain座標の安定性を分離

### 問いと条件

既存の動画grain研究ではscreen-fixedとmotion-alignedの違いを扱ったが、flow推定誤差と座標方針そのものが混ざっていた。正確な整数移動・カメラpan・occlusionを与え、座標方針だけを制御比較する。

- `tools/grain_motion_coordinate_study.py` は768×384 procedural worldを384×256 cameraで32 frame、毎frame x=2px panする。SHA-256検証済みCC0のcloverとsilver-feather spriteを異なる整数経路で移動し、交差時はfeatherがcloverを隠す。
- 同じsigma=.025のmonochrome Gaussian grainをscreen-fixed、world background + sprite material fixed、frame-varying screen fieldで適用。grain residualは同じframeのclean outputを差し引いて算出する。
- 隣接frameのscreen座標、同一world背景座標、sprite material座標のresidual MAEを測る。clipされた値、occluded領域、移動sprite端の対応しないpixelを除外。featherはvisible intersectionが少ないpairがある。
- 再実行: `python tools/grain_motion_coordinate_study.py`。Metrics/contact sheetは`captures/grain-motion-coordinate/`。
- Contact sheetを確認。3方針×frame 00/16/31のscene表示で、grainは意図通り画面、素材、frameごとに変化する。表示は合成sRGB model。

### 観測

| grain座標方針 | screen residual MAE | 同一world背景MAE | clover material MAE | feather material MAE |
|---|---:|---:|---:|---:|
| screen-fixed | 1.97e-7 | .028238 | .028387 | .028059 |
| world-and-object-fixed | .028022 | 7.13e-7 | .000055 | .000177 |
| frame-varying | .028210 | .028206 | .028281 | .028329 |

- 固定座標では該当座標のresidualがほぼ不変となり、他方針の座標では概ねnoise差分規模へ変化した。これは実験設定の再現性確認で、独立した知覚発見ではない。
- Feather material MAEは21 frame-pairで集計され、valid pixel数はpairごとに51〜842。cloverは31 pair、1524〜1529 pixels。低support pairがあるfeather平均の精度は相対的に弱い。

### 限界・判断

- exact integer transformsであり optical-flow誤差を含まない。sprite alpha合成はsRGB code-spaceの簡略model。grain以外の画質、実写の動き、粒状感の好み、時間周波数の製品lookは評価しない。
- screen-fixed / content-fixed / frame-varyingは異なる意味の選択肢で、普遍的な優劣を示さない。CoreまたはArtifact実装へ変更は加えない。実装判断にはgrainの意図（sensor/display noiseか素材 textureか）と承認済みlookが必要。

## 実験 093: Grain temporal correlation と周期的refreshの時間応答

### 目的・評価計画

- **目的:** 固定grainと毎frame独立grainの両端だけでなく、中間の時間相関や周期refreshが時間方向の応答をどう変えるかを記述する。grainのamount・spatial spectrum・座標追従ポリシーを別問題として扱う。
- **baseline:** 同一の静止合成RGB sceneからgrain residualを差し引き測定する。scene自体は正解lookではない。候補はheld、AR(1) rho=.95/.75/.50/.00、8-frame hold-and-refresh。全てfield varianceを概ね1、additive sigma=.025へ揃えた。
- **素材・条件:** 256×192 procedural smooth color field、120 frame、NumPy float32、seed 20261111。scene base RGBはclip-safe `[.12,.82]`、sRGB-like code-value model。alphaなし。
- **統計:** 各frame sigma、lag 1/2/4/8/16/32のresidual MAEとfield autocorrelation、frame 0とのcorrelation。Lag MAEの95% CI half-widthはframe-pair間の標準誤差表現で、空間位置を独立sampleとした信頼区間ではない。
- **目視:** 生成contact sheet上でframe 0/1/7/8/30/60/119を同一表示条件で確認。screen縮小sheetで全体を確認、等倍詳細/flicker適否の判定者間レビューは未実施。
- **合否:** 知覚的受容閾値・approved look・判定者基準がないため設定しない。数値はalgorithmic temporal responseで、flicker品質の合格判定ではない。
- **必須項目の状態:** 効果応答/時間挙動は評価。画質・色transferはsynthetic code-spaceのみ、alphaはopaque RGBのため未適用。motion、実写、subpixel、HDR、Core/GPU/backend、preview/export、性能・memory、製品操作経路は未検証。実際のRetroImageLab presetのcorrelationはこの別modelから推定しない。
- **再現:** `python tools/grain_temporal_correlation_study.py`。Metricsとvisual sheetは`captures/grain-temporal-correlation/`（ignored generated outputs）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

| policy | lag-1 MAE | lag-1 correlation | lag-8 MAE | lag-8 correlation | lag-32 correlation |
|---|---:|---:|---:|---:|---:|
| held | 0 | 1.000 | 0 | 1.000 | 1.000 |
| AR(1) .95 | .2523 | .9497 | .6538 | .6620 | .1914 |
| AR(1) .75 | .5644 | .7492 | 1.0700 | .0987 | -.0012 |
| AR(1) .50 | .7983 | .4992 | 1.1263 | .0032 | -.0008 |
| AR(1) .00 (independent) | 1.1289 | -.0007 | 1.1288 | -.0004 | -.0006 |
| refresh every 8 | .1328 | .8821 | 1.1291 | -.0021 | -.0006 |

- 平均field sigmaは各方式で.9967–1.0000。従ってframe差の変化はほぼfield variance差ではなくtemporal correlation policyに対応する。
- AR(1) .95/.75/.50のlag-1 correlationは設定値付近となり、lagを伸ばすと相関が落ちる。独立方式は各lagでほぼzero correlation。
- 8-frame hold方式はblock内lag-1 residual MAEが低く、block境界を跨ぐlag-1は約1.129。Contact sheetでもframe 0/1/7は同じfield、frame 8から新field。これは更新周期による不連続を示す。
- AR(1) .95でもframe 0とのcorrelationはframe 32で約.190、frame 119で約.0013。隣接frameをなめらかにすることと長期的な粒子配置保持は別軸。
- Sheet確認: smooth synthetic base上でgrainが見えるが、frame別tileの縮小は弱いflickerを隠す。sheetは時間の連続再生を示さない。

### 判断・限界

- 時間相関係数またはrefresh周期は、grain amount・粒径・screen/content coordinateと独立した候補parameterになり得る。ただし本実験はその設計空間の数値確認であり、最適値や製品既定を選ぶ証拠ではない。
- White Gaussian fieldと静止sceneのみ。実写motion、object-following grain、scene cut、sRGB/linear差、clipping、spatial-frequency違いとの交互作用、知覚閾値・長時間loopの好みは未評価。Contact sheetのみで連続再生評価はしていない。
- Core/Artifact変更は行わない。Core反映判断に進むには、意図するlookと時間基準を明示し、複数素材/動き上で連続再生の目視と閾値を事前定義した比較、および実Core経路/性能確認が必要。

## 実験 094: Grainとpalette quantizationの順序・Bayer位相相互作用

### 目的・評価計画

- **問い:** 現suiteのquantization→grain順序でgrain residualはどの程度palette外へ出るか。grain→quantizationへ順序を逆転するとgrainは離散paletteへの移動へどう変わるか。Bayer origin 16通りの影響と素材依存を測る。
- **source確認:** `retro_lab.py::apply_retro`はordered quantization → channel offset → scanlines → grain → clipの順。対象print相当ではscanline=0 / aberration=0なのでquantizationとgrainだけを比較し、他のsuite効果は混ぜない。
- **baseline/候補:** baselineは実suiteのquantize(source) + grain。同じnoise fieldを使うcounterfactualはquantize(clip(source+grain))。candidateは製品提案ではなく順序感度の対照。
- **素材:** SHA-256/provenance確認済みCC0 clover cutout、silver feather cutout、pixel hair cutout。各々中央square crop→Lanczos 256×256。roleは葉edge/自然texture、羽毛fiber/highlight、方向性hair texture。source hash/manifest/licenseはmetrics JSONへ保存。
- **条件:** seed=34091、同一Gaussian fieldを全順序/位相へ適用、sigma=.035。実presetと同じprint相当level 12 / dither .7のほか、4/8 scalar-level stress caseを追加。Bayer 4×4 origin全16通り。RGBはsRGB-like code valuesで処理し、source alphaを変更しない。
- **指標:** paletteからのscalar channel距離、baseline quantized outputとの差としてのgrain residual RMS、alpha-weighted visible change ratio、edge ROI metric、順序間MAE、全位相range。これらはalgorithmic descriptionであり色差/見た目の正解ではない。ROI maskはsource luma gradientのP85 thresholdかつalpha>.98。
- **目視:** representative print conditionの4位相についてsource / 両順序 / 8倍difference contact sheetを同一表示で確認。thumbnail reviewのみ。等倍pixel crop・独立判定者・連続再生なし。
- **合否:** approved targetとperceptual thresholdがないため未設定。品質順位を付けない。
- **未検証:** Core/Artifact実行経路、GPU/backend parity、HDR/ICC transform、background-dependent alpha appearance、runtime/memory、motion/time stability、approved visual criteria。製品統合完了とは扱わない。
- **再現:** `python tools/grain_quantizer_interaction_study.py`。Metrics/sheetは`captures/grain-quantizer-interaction/`（ignored generated outputs）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

print相当・phase (0,0)の可視alpha-weighted linear-independent code-value統計:

| 素材 | 現suite quantize→grain: grain RMS / palette距離 | grain→quantize: residual RMS / palette距離 | 順序間visible MAE | edge visible MAE |
|---|---:|---:|---:|---:|
| Clover | .00425 / .02044 | .00542 / ~0 | .00092 | .03011 |
| Silver feather | .01084 / .01794 | .01484 / ~0 | .00448 | .02857 |
| Pixel hair | .00878 / .01950 | .01185 / ~0 | .00326 | .02878 |

- 現suite順の出力はnoiseをquantizer後に加えるため、量子化palette値から外れる（平均scalar channel distance .0179–.0204）。逆順はquantize後の色集合へgrainで直接距離を加えるのではなく、noiseでpalette binの境界を越え、出力自体は再びlevel格子上に残る。従ってpalette distanceはほぼzeroだが、grain RMSは素材により大きくなる場合がある。
- levelを12から4へ減らすと、全素材平均の順序差visible MAEはprint相当 .00289 → 8 levels .00380 → 4 levels .00458へ増加。同一grain係数でもquantizer bin幅との関係で順序感度が変わる。
- Bayer 16位相での順序差rangeは素材/levelにより異なる。print相当でclover .00091–.00094、feather .00444–.00448、hair .00326–.00332。4-levelでは位相rangeがやや広がり、feather .00683–.00717、hair .00502–.00540。
- Representative sheetでgrainが画面全体へ加わる現suite順と、paletteの閾値移動として作用する逆順の差を確認。葉の暗背景やfeather highlightで見える差の大きさは素材依存。
- Alpha changed ratioは全条件0。これはRGB-only modelで元alphaを明示保持した結果。

### 判断・限界

- Grain stageの順序はpalette adherenceとgrainの実効分布を結び付ける重要なrecipe parameter。Core側へ知見だけ反映し、実装を変更する根拠ではない。現行suiteの意図するlookを選ぶtarget reference/acceptanceなしに順序変更はしない。
- Scalar equal-channel levelsはchromatic learned/fixed paletteを再現しない。center crop 3点、sigma1点、grain monochrome Gaussian、sRGB-like numeric modelである。edge thresholdは画像毎のP85で、同じ物理ROIではない。等倍crop/person preference/temporal effect/Core性能は未評価。
- 外部素材source/licensingを含むJSONと比較sheetはローカルignored capturesに保存。tracked研究ノートには派生素材を追加しない。

## 実験 095: Pixelation前後のgrain配置とalpha edge合成

### 目的・評価計画

- **目的:** suiteのBOX縮小/nearest復元、quantization、scanline、grainのうち、grainを出力解像度で加える現行手順と、grainをresize前へ移すcounterfactualを比較する。alpha cutout境界と背景合成まで見る。
- **source確認:** `retro_lab.py::apply_retro`はRGBをBOX downsample→nearest upsampleし、ordered quantization→channel offset→scanline→grain→clipを通す。元source alphaを最後に戻す。今回print/handheld/arcade設定を使いaberration=0に固定し、縮小率・quantizer・scanline・grain sigmaは各presetの値を使用。
- **baseline / candidate:** baselineはresize→quantize→scanline→grain。candidateはgrainをsource RGBへ先に加えた後、resize→quantize→scanlineする。noise realizationとseedを同じにする。candidateは製品提案ではなく順序応答の診断比較。
- **素材:** SHA-256/provenance検証済みCC0 clover cutoutとsilver-feather cutoutを中央square cropしLanczosで256²化。source alpha planeを維持。pixel hair はopaque/別の役割で今回のalpha edge比較から除外。
- **条件:** seed 78123、同一monochrome Gaussian noise field、grain sigma handheld .025 / arcade .015 / print .035、scale .5/.75/1.0。palette levels/dither/scanlineはpreset値、sRGB code-value処理。出力RGBのlow/high clip率、source alphaから作るedge/partial masks、black/mid-gray/white背景でlinear-light合成後のsRGB-code差を測る。
- **効果応答指標:** output minus clean full suite baselineをgrain insertionの実効差とする。これは加算grainと、それによりquantizer binが変化した成分を両方含む。visible/edge/flat RMS、水平/垂直相関を測る。量子化経路との交互作用があるため、理想的なraw noise residualとは呼ばない。
- **目視:** 2素材×3 presetのsource / 現行 / pre-resize / 8倍order差contact sheetを同一nearest倍率で確認。研究者がsheetレビュー。個別1:1 crop、独立判定者、approved targetなし。
- **合否:** 知覚閾値・承認lookなし。結果記述に限り、pass/failやpreset rankingはしない。
- **未検証:** hidden RGBを除去した場合の差の分離、ICC/HDR、実アプリpreview/export、GPU/Core parity、性能/メモリ、実写motion、複数判定者による画質評価。
- **再現:** `python tools/grain_pixelation_alpha_interaction_study.py`。Metrics/contact sheetは`captures/grain-pixelation-alpha/`（ignored captures）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

| 素材 / preset | 現行 visible effective RMS | pre-resize effective RMS | 現行 edge RMS | pre-resize edge RMS | mid-gray edge composite MAE: current / pre |
|---|---:|---:|---:|---:|---:|
| Clover / handheld | .02513 | .02457 | .02302 | .02131 | .00328 / .00188 |
| Clover / arcade | .01499 | .01343 | .01324 | .01119 | .00201 / .00143 |
| Clover / print | .03437 | .04182 | .02938 | .03659 | .00499 / .00491 |
| Feather / handheld | .02401 | .02854 | .02344 | .02829 | .00857 / .00368 |
| Feather / arcade | .01444 | .01451 | .01402 | .01389 | .00513 / .00329 |
| Feather / print | .03169 | .04393 | .03054 | .04187 | .01189 / .01179 |

- Alpha changed ratioは全6素材/preset×orderで0。black/gray/white合成後のedge差は素材・preset・背景で異なり、一貫してどちらかが小さくなるわけではない。例として白feather+handheldのmid-gray edge MAEは現行 .00857 / pre-resize .00368だが、clover+handheldは現行 .00328 / pre .00188。printではgrain/noiseが量子化境界を変えるためeffective RMSが両素材でpre-resize側の方が大きい。
- Scale=.5/.75のpre-resize conditionではgrainがBOX filterを通り、細かなnoiseが空間平均化されてからnearest blockへ展開される。一方scale=1のprint比較ではresize平均化はないため、順序差は主にgrainがquantizer入力を変えるか出力へ直接加わるかの応答となる。
- Sheetでcutout outlineのblockificationと、羽根の高輝度edgeにnoiseが入る位置が順序により違うことを確認。8倍差分表示は差の場所を強調するもので、実表示の見た目を示す倍率ではない。

### 判断・限界

- Grain spatial scaleは処理解像度とstage位置に依存し、amount sigmaだけでは同じlookを保証しない。これは現行suiteの順序感度であり、Coreへの実装変更を直接提案するものではない。
- Pre-resize noiseはalpha-zero pixelのhidden RGBにも加算され、そのRGBがBOX filterに入る。従ってcounterfactual差にはgrainの低域化とhidden RGB edge mixingの両方が含まれる。透明外側RGBだけにnoiseを入れない別条件は未評価で、寄与の分離は今後の実験課題。
- 2つのCC0素材・中央crop・1 seed・各preset1条件。色domain、alternative alpha-aware resize、実Core/GPU出力、look target、性能は評価していない。背景composite MAEはsource clean pipelineとの記述差で、境界品質合否ではない。

## 実験 096: Alpha-zero hidden RGBとgrain配置の相互作用を分離

### 目的・評価計画

- **目的:** 実験095で交絡したpre-resize grainの低域化とalpha-zero hidden RGB mixingを区別する。可視RGBとalphaを厳密に固定し、alpha=0 pixelのRGBだけをblack/magenta/greenへ置換して処理結果を比較する。
- **baseline / pair:** Hidden blackをcontrol、magentaおよびgreenを各々pair候補とする。処理前に全variantでvisible RGB (`alpha>0`) とalphaが同一である。自然素材の元hidden RGBを改変した合成counterfactualであり、デコーダが実際に返す値の代表とはしない。
- **経路:** grain-off reference、現suite（BOX/nearest→quantize→scanline→grain）、grain-before-resize（grain→BOX/nearest→quantize→scanline）。suiteのprint/handheld/arcade相当scale、levels、dither、scanline、grain量を使う。元alphaを維持。
- **素材:** SHA/provenance検証済みCC0 cloverとsilver feather、中央square crop→256² Lanczos。edge maskはsource alphaから生成。
- **評価:** black/mid-gray/white backgroundsへlinear-light合成後のwhole/edge/P95 sRGB-code MAEを測る。Grain-induced hidden-RGB sensitivityはgrain付きpairwise MAEとgrain-off pairwise MAEとの差の絶対値。これは絶対値同士の差であり、signed RGB error changeではない。alpha変化率も記録する。
- **目視:** source alphaをmid-grayへ合成した比較sheetでclean black/colored hidden、現suite、pre-resize、感度変化×512表示を確認。倍率512は差の場所を見せるための表示だけで、測定値のscaleではない。研究者がthumbnail sheetを確認。
- **合否:** quality thresholdなし。目的は因果要素の切り分けであり、画質rankingは行わない。
- **未検証:** 実source decodeのhidden RGB値、decoder/alpha-mode別の実 caller値、ICC/HDR、GPU/Core equivalence、製品画面、性能、独立目視評価。
- **再現:** `python tools/grain_hidden_rgb_interaction_study.py`。Metrics/provenance/contact sheetは`captures/grain-hidden-rgb/`（ignored）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

black↔magenta pairのmid-gray composite edge MAE（grain-off / suite-post-resize / grain-before-resize）:

| 素材 / preset | clean | suite後grain | resize前grain |
|---|---:|---:|---:|
| Clover / handheld | .009479 | .009514 | .009328 |
| Clover / arcade | .004300 | .004303 | .004241 |
| Clover / print | 0 | 0 | 0 |
| Feather / handheld | .000335 | .000333 | .000332 |
| Feather / arcade | .000135 | .000134 | .000134 |
| Feather / print | 0 | 0 | 0 |

- Black↔green差も同方向で小さい（clover handheld .004784/.004801/.004688）。alpha=0 RGB感度はcloverで強く、featherで小さい。scale=1.0 printでは隣接pixelを混ぜるresizeがないため、alpha-zero色はvisible compositeへ寄与せず、全hidden-fill比較がzeroとなる。
- Suite後grainはquantization/resize後に加わるため、hidden-color pair sensitivityはgrain-offとの差が小さい（clover handheldのmagenta case +.000035 edge MAE）。grainをresize前へ移すとpair sensitivityの変化は同例 −.000151。どちらもhidden RGBの基礎感度 .009479より小さく、素材・scale依存。
- Alphaは構築上全て保持（changed ratio 0）。mid-gray合成sheetで同じvisible object silhouetteを保ったまま、hidden fillだけに由来するedge差が見える。×512 difference tileは小差の位置を誇張表示している。

### 判断・限界

- 実験095のpre-resize差にはalpha-zero RGBが寄与し得ると確認したが、hidden RGBの影響方向/大きさはsource・hidden fill・scaleに依存する。pre-resizeはgrain振幅やquantizer境界も変えるため、効果全てがhidden RGBに帰属するわけではない。
- Core反映の根拠にはしない。source decoderが何を返すかを知るには許可された実素材decode経路とalpha descriptor別採取が必要。製品でhidden RGB正規化を行う場合も、現行経路契約を確認してから別判断とする。
- 2素材・人工hidden fills・1 random field・短いpreset条件。visible RGB/alpha同一という統制は強いが、natural RGB distributionの正解や知覚受容性を示さない。運用上のhidden color canonicalizationが正しいと結論しない。

## 実験 097: Grain量とBayer周期成分の相対energy

### 目的・評価計画

- **問い:** 現行suiteのquantize→grain順序で、noise追加はBayer由来の周期成分自体を変えるか、それとも残差全体に対する相対energyだけを薄めるか。入力素材、palette/dither値、Bayer位相、grain sigma、seedを交差させる。
- **baseline:** 同じinput/levelsに対するno-dither quantized output。Bayer templateは同一input・levels・phaseのdithered output minus no-dither output。処理応答の数値基準でありtarget imageではない。
- **sources:** 256² synthetic flat mid-gray、smooth unclipped color ramp/soft mound、SHA/provenance検証済みCC0 clover cutoutとsilver feather center crop。自然素材はalpha>250/255のopaque interiorだけを統計maskにする。
- **条件:** level/dither 8/.5、12/.7、24/.35。Bayer shifts (0,0),(1,0),(0,1),(1,1)。grain sigma 0/.0075/.015/.025/.035。16 independent fixed Gaussian field seeds per condition。Mono noiseをquantized RGBへ加えclip。Seed countは各条件同じ。
- **指標:** 出力残差 `(grain output - no-dither quantized baseline)`をBayer templateへscalar least-squares projectionしbetaを算出。Bayer coherent energy share = projected template energy / total residual energy。Residual RMSとclip率も記録、seed間mean/p05/p95/stdを出す。これは選択したbaseline/template上のsignal decompositionで、perceptual visibility scoreではない。
- **目視:** Flat mid-gray print-like caseのsigma 0/.0075/.015/.025/.035でcontact sheetを同倍率比較。研究者がthumbnailでreview。高倍率周期cropや視認試験、独立 reviewerは未実施。
- **合否:** 承認look・thresholdなし。効果応答の記述のみ。
- **未検証:** 知覚masking閾値、moving footage/temporal interaction、alpha edge composites、ICC/HDR、Core/GPU parity、preview/export、性能・memory、human panel。
- **再現:** `python tools/grain_bayer_masking_study.py`。provenance/metrics/sheetは`captures/grain-bayer-masking/`（ignored outputs）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

12-level / dither .7でBayer phase 4種を平均したcoherent-energy share（sigma 0 / .015 / .035）:

| Source | sigma 0 | sigma .015 | sigma .035 | projection beta at .035 |
|---|---:|---:|---:|---:|
| Flat mid-gray | 1.000 | .948 | .772 | 1.000 |
| Smooth color ramp | 1.000 | .862 | .534 | 1.000 |
| Clover | 1.000 | .867 | .547 | 1.001 |
| Silver feather | 1.000 | .864 | .538 | .998 |

- Sigmaを増やすとBayer templateへのprojection betaはほぼ1のまま保たれた一方、独立grainがtotal residual energyを増やしcoherent Bayer shareが下がった。従ってこのsignal modelではgrainは周期成分の振幅を消すというより、相対的なenergy比を下げる。
- 24-level/.35 ditherではBayer templateのinput-relative RMSが小さいため、同じgrain sigmaでもshareがより急に低下した。例: smooth rampでsigma .015時share .430、.035時.121。8-level/.5ではflat fieldでそれぞれ.975/.880。Effect amount単体ではなくpattern amplitudeとの比で結果が変わる。
- 16 seedのBayer beta平均は多くのsource/configで目立つbiasなく1付近。clip-safe synthetic controlでも同様。Seed intervalのp05/p95/stdはJSONにある。素材alpha maskとphaseの影響で自然cutoutはsynthetic controlよりseed/phase rangeが少し大きい。
- Contact sheetのflat controlはsigmaに伴いfine grainが増し、4×4 periodic fieldは残ったまま全体の細かなenergyが上がる。画面縮小ではpattern視認性を判定できない。

### 判断・限界

- GrainがBayerの周期振幅を直接弱める、とはこのmodelから言えない。coherent share低下は残差分母の増加で説明でき、知覚上のmasking率ではない。Bayer grain `dither` adjustmentやgrain係数変更をCoreへ移植する判断には使わない。
- Bayer templateは`Q_dither(source)-Q_no_dither(source)`というphase/input別の差分であり、全てのpattern contrastを一般的に抽出するものではない。Natural textureが4×4成分を含む場合の基底分離、知覚的視認閾値、色channel間の相関を評価していない。
- 2 natural assetsと2 synthetic control、mono Gaussian grain、RGB code-value domain。現在の通常設定で見えるかどうかは等倍display、approved look、observer studyで別途評価が必要。

## 実験 098: Full suite presetの出力解像度感度

### 目的・評価計画

- **目的:** 現行4 presetのパラメータを固定して出力解像度だけを変え、同じnormalized display frameへ縮小した場合のlook差と各effect寄与を記述する。 grainだけを扱った実験090/091をfull suiteへ広げる。
- **比較基準:** 各素材/presetの256²実行結果をreferenceとする。承認lookや品質正解ではない。128/512/1024²の結果をLanczosで256²へ表示正規化する。
- **素材:** 512² deterministic multiscale chart (smooth color ramps、highlight、形状境界、1px diagonal/horizontal lines)と、provenance/SHA確認済みCC0 clover cutout・silver feather cutout・pixel-hair cutoutの中央square crop。自然alphaはlinear-light mid-grayへ先に合成してopaque入力にし、解像度比較中のalpha resamplingを対象外にする。
- **実行条件:** Pillow Lanczosで素材を128/256/512/1024にsRGB code-value resizeし、現行`PRESETS`を`apply_retro`でseed 60419固定実行。grain-off / aberration-off / scanline-off counterfactualも同解像度で描画。
- **評価指標:** 256 view上の全体/center ROI linear RGB MAE/RMSE/P95/threshold pixel ratio、nativeとnormalized outputのluma spectrum、grain normalized-view RMS、native/normalizedの各effect ablation差。PSNR/SSIM、人間acceptance thresholdは設定していない。
- **目視:** 全4 source×4 preset、列128/256/512/1024の同表示サイズcontact sheetとsource別シートをnearest倍率で比較。研究者が各source行を確認。independent reviewer/approved targetなし。
- **必須項目の状態:** 空間/効果応答とresolution dependenceを記録。RGB processingはsuiteのsRGB code assumption。alphaは事前linear-light compositeしているためsuite alpha edgeは未評価。時間安定性はstatic stillのため対象外、GPU/Core/preview/exportと性能は未検証。
- **合否:** resolution-invariance requirementがないためpass/failなし。数値は256基準からの出力変化量で、どの解像度が良いかは示さない。
- **再現:** `python tools/full_suite_resolution_sensitivity_study.py`。JSON/sheet/provenanceは`captures/full-suite-resolution/`（ignored outputs）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

全体normalized-frame linear RGB MAE（128 / 512 / 1024 output対256基準）:

| 素材 | Handheld | Arcade | Print | VHS |
|---|---:|---:|---:|---:|
| Synthetic chart | .037 / .031 / .031 | .042 / .038 / .039 | .042 / .032 / .031 | .045 / .035 / .035 |
| Clover | .045 / .038 / .037 | .042 / .037 / .038 | .052 / .045 / .045 | .046 / .037 / .036 |
| Feather | .052 / .045 / .044 | .053 / .047 / .048 | .058 / .047 / .046 | .055 / .045 / .045 |
| Pixel hair | .054 / .046 / .046 | .048 / .043 / .044 | .060 / .045 / .044 | .054 / .046 / .046 |

- Tableの値は各presetで順に128 / 512 / 1024。1024と512は多くの組合せで近いが、全解像度は256基準と完全一致せず、画像素材により差の大きさが変わった。これはsource resampling・fixed pixel grid・量子化・clippingを合算した描写差。
- Normalized-view grain RMSは全素材/presetで128 outputが1024より約3.6〜4倍大きい（handheld .0226 vs .0060、arcade .0137 vs .0042、print .0315 vs .0079、VHS .0360 vs .0094）。output-sized imageを256表示へ縮小すると、1024側のpixel grainが高周波としてfilterされる。
- Aberration-off ablationのnormalized MAEは素材/presetにより128から1024へ低下する傾向。例: synthetic VHS .0088→.0022、clover arcade .0115→.0007。`chromatic_shift` amountはsample coordinate上のpixel offsetなので同じ係数でもnormalized frameに対する比率はresolutionに反比例する。
- Scanline-off ablationのnative RGB8 MAEは同一preset内でほぼ維持される一方、pattern周期は2 output rows fixed。normalized displayではrow pitchが128 outputで約4 display pixels、1024で.5 display pixelsへ変わるため、同じnative mean changeでも線の密度/表示応答は同一lookを意味しない。
- 4×4 Bayer matrixとgrainもoutput pixel格子へ固定される。Contact sheetsでは低解像度のpixelation blocks、scanline、chromatic displacementがnormalized表示で大きく見え、hair/feather細部は解像度・source roleごとに差がある。

### 判断・限界

- 既存presetは単一output sizeにnormalized invarianceする契約を持たない。pixel amountをresolution scalingする変更は既存lookを変えるため、target display contractを決めずにCore/RetroImageLab recipeへ反映しない。
- この比較ではinput自体を各解像度へLanczos/sRGB codeで再標本化するため、出力effectだけでなくsource sampling差も含む。特に128/1024でdetailのsource sampling条件が異なる。256はreference baselineに過ぎない。
- 自然cutoutはmid-grayへflattenされalpha boundaryを測らない。suite runtime、GPU/backend、preview/export scaling、HDR/ICC、性能/VRAM、出力サイズの実利用分布は未評価。結果は製品統合や画質合否の証拠ではない。

## 実験 099: Full suiteのnatural alpha cutout解像度・association応答

### 目的・評価計画

- **目的:** 実験098のopaque composite解像度比較を自然alpha cutoutへ拡張し、suite effectのedge差、alphaの正確な保持、source/input resize association (straight/premultiplied)依存を同じ解像度内で比較する。
- **素材:** provenance/SHA検証済みCC0 clover、silver feather、pixel hair cutoutを中央square cropし512²へLanczos標準化。元pixel valuesはsRGB code valuesで扱う。
- **source resize条件:** 128/256/512/1024へstraight（RGB各planeとalphaを独立Lanczos）またはpremultiplied（RGB×alphaとalphaをLanczos、filter後unassociate）でresizeする。両方式は同じalpha filterを共有する。PremulもsRGB code values上のfloat filterであり、linear-premul production contractとは主張しない。
- **suite:** 現行4 preset/seed 81327を`apply_retro`へ通す。suite内pixelationにも同じalpha policyを指定。各解像度・associationのresize済みRGBAそのものを同一native-grid baselineにし、baselineとcandidateをlinear-lightでblack/mid-gray/whiteへcompositeしてwhole/visible/partial/edge MAE・P95・maxを測る。
- **alpha確認:** suite出力alphaが該当source-resized alpha planeとpixel-exact一致することを調べる。alpha-resampling方式間の見た目差は別途paired output comparison。
- **目視:** straight policyの全asset×preset×resolutionをnative composite後256へ同じLanczos表示したsheet、256pxでstraight/premultiplied policy comparison sheetを確認。研究者が全体/edge表示をレビュー。approved target/独立reviewerなし。
- **合否:** perceptual threshold・承認lookなし。baselineからのMAEは処理差を記述するだけ。
- **未検証:** actual Artifact decoder alpha descriptor/hidden RGB、ICC/HDR、GPU/Core path、display scaling、performance/VRAM、独立評価者、製品preview/export。
- **再現:** `python tools/full_suite_alpha_resolution_study.py`。metrics/provenance/sheetsは`captures/full-suite-alpha-resolution/`（ignored）。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

256px / straight-source-resize / mid-gray compositeでのfull preset出力とsame-size resized input baselineのalpha-edge sRGB-code MAE:

| 素材 | Handheld | Arcade | Print | VHS |
|---|---:|---:|---:|---:|
| Clover | .01415 | .01630 | .00635 | .01850 |
| Silver feather | .03339 | .04290 | .01630 | .03570 |
| Pixel hair | .02190 | .02425 | .00580 | .02610 |

- Background依存がある。例: clover/handheld 256px edge MAEはblack .02117、mid-gray .01415、white .01015。これは合成後display-code差であり、単一背景のedge値を一般化できない。
- 256px handheldでstraight vs premultiplied source-resize pairのmid-gray edge MAEはclover .00694、feather .00146、pixel hair .00852。Suite出力を比較すると各々.02847/.00910/.04013となった。Pixel hair/handheldのsame-size baseline edge MAEもstraight .0219 / premult .0361と反転差があり、association差が素材/preset responseへ伝播する。
- Source resized alphaは両associationで同じLANCZOS alpha planeを使い、suite後も全48 source/preset/size records×2 policyでalpha changed ratio=0。よってpaired RGB composite差はalpha-coverage変更ではなくRGB filtering/processingの差。
- Association間のedge差はresolutionとpresetで変動し、全素材に一貫したwinnerはない。例: clover handheldではstraight lower at 128/256/512、1024でpremul lower。Feather handheldは全4サイズでstraight lower。これはsource hidden RGB、filter/scale、preset作用が組み合わさったspecific comparison。
- Contact sheetsで羽根のpartial-alpha fiberとpixel-hair cutout boundaryへ、association×resolutionでRGB texture/edgeが変わることを確認。Sheetはmid-gray display viewで、background metricsは3色別JSONを参照。

### 判断・限界

- Alpha planeのpixel-exact保持は、edge RGB/alpha compositeの一致やquality correctnessを保証しない。Source resize alpha mode、hidden RGB、suite internal pixelationを併記する必要がある。
- Association差はsRGB-code float filterのcounterfactualで、source decoderがstraight/premultipliedどちらを返すか、Core runtimeでの適用契約の証拠ではない。Core変更やalpha-policy default変更には使わない。
- Sourceを512²に標準化してから4 sizeへresampleするため元のsource resolution/detailはcropに依存。Color management、HDR、GPU/backend、性能、visual acceptanceは未評価。

## 実験 090: Pixel固定grainと画像比率固定grainの解像度スケール

### 問いと条件

実験006ではwhite/coarse grainの空間相関を記録したが、同じ素材を異なる出力解像度へ出す場合、固定pixel粒径とframe-relative粒径がどう変わるかは未整理だった。

- `tools/grain_resolution_scale_study.py`。3 SHA/provenance確認済みCC0素材（clover cutout、silver feather、pixel hair atlas）の中央square cropを256/512/1024へLanczos resize。
- 各解像度に同じ入力sigma=.035のmonochrome additive noiseをsRGB code valuesへ加え、alphaはresized source値をbit-exact保持。
- Pixel-fixed modeは各出力pixelへindependent white noise。Relative modesはoutput幅の1/2/4%間隔のGaussian fieldをbilinear拡大し、各fieldのsigmaを.035へnormalize。
- FFT radial median frequencyをcycles/pixelとcycles/imageで記録。隣接pixel correlation、visible residual RMS、black/gray/white合成後のROI別残差も保存。
- 実行: `python tools/grain_resolution_scale_study.py`。Metrics/contact sheetは `captures/grain-resolution-scale/`。

### 観測

- Pixel-fixed white grainのspectral medianは256/512/1024で102.8/204.1/408.3 cycles/image。Cycles/pixelはほぼ不変で、出力画素数に比例してgrain粒子数が増える。
- Relative 2% modeのmedianは16.4 cycles/imageを全3解像度で維持（nominal cellは5/10/20px）、adjacent correlationは.942/.984/.995。1% modeも26.6/31.9/31.8 cycles/image、4%は8.6/8.6/7.3と概ねframe-relativeなfrequencyを維持した。端数gridとfinite cropで完全一致はしない。
- Input sigmaを揃えてもclipping後visible residual RMSは素材・解像度ごとに約.030–.037へ変動。Alpha-edge mid-gray composite residualにも素材差があり、全体sigmaだけで表示強度を記述できない。
- Silver featherのcontact sheetを確認。同じ表示タイルサイズへ縮小して並べるとrelative grainは画像占有率を揃えたざらつき、pixel-fixedは元の出力samplingに応じた細かな粒子として見える。好みの優劣基準は設定していない。

### 限界・判断

- 相対grainはbilinear-upsampled white fieldの探索モデルで、film emulsion/製品grain recipeの忠実度を主張しない。Still素材3点、sigma1点、mono noiseのみ。人による品質評価・時間安定性・Core/GPU経路・性能は未測定。
- Grain amountだけでなく空間scaleもlookを決める独立parameter候補。ただし固定px/relativeどちらを採るかは意図する表示・export契約次第であり、Core既定やRetroImageLab presetは変更しない。

## 実験 091: Linear-light表示強度を揃えたgrain粒径比較

### 問いと条件

実験090では全方式に同じ入力sigmaを与えたため、粒径による統計差と、合成後の表示強度差が混在し得る。表示後の数値強度を揃えた状態でも空間周波数差が残るかを調べる。

- `tools/grain_equal_display_strength_study.py` が実験090の3 CC0素材、256/512/1024解像度、pixel-fixed/1%/2%/4% relative grain fieldを再利用。
- 各条件で同じseedのunit-RMS noise fieldを作り、sRGB code-value additive amplitudeをbinary search。Source-over linear-light Rec.709 luma difference RMSをvisible pixels上でtarget .015へ揃える。
- Source/output alphaは完全同一。linear-lightでの一様background項はdifferenceを取るとalphaが一定のため厳密に相殺する。従ってこの指標はblack/gray/whiteで同じ値になる。可視画素限定のpreclip channel fractionとmatched sigmaも記録。
- 実行: `python tools/grain_equal_display_strength_study.py`。結果は `captures/grain-resolution-scale/grain-equal-display-strength-metrics.json` と素材別sheet。

### 観測

- 全素材×解像度×粒径でmatched RMSはtarget .015へ到達（pixel quantizationによるごく小さな差を含む）。見かけのluma strengthを揃えても、noise spectrumは実験090と同じくpixel-fixedはoutput pixelsに応じて周波数が増え、relative fieldはcycles/imageがおおむね一定のまま。sigmaの調整だけでは粒径の違いを消さない。
- 必要code-value sigmaは素材・resolution・fieldにより約.0139–.0220。Cloverは2% fieldがpixel-fixedより高いsigmaを要する場合がある（256: .0220 vs .0198）。Silver featherは.0140–.0155、hair atlasは.0141–.0169程度。強度係数の厳密な補正値はgrain modelとsource materialに依存する。
- 可視pixel channel sampleのpreclip fractionはclover約2.4–4.3%、hair約2.8–5.9%、white feather約14.0–17.7%。高輝度featherでは同じlinear-luma RMSへ揃える際にもchannel clippingが多く、線形amplitude scalingがtexture distributionを保つとは限らない。
- Background metric equalityは人間の色知覚が背景に無関係という意味ではない。固定alpha下、linear-light RGB/luma deltaから一様background項が代数的に消えるという評価量の性質。

### 限界・判断

- RMS .015は比較用の数値基準で、approved lookまたは知覚等価の閾値ではない。sRGB code noise、mono channel共通、静止cutout3点、field seed差を含む。display spatial frequency、視距離、動き・Temporal noise、GPU/Core性能は未評価。
- Equal-strength comparisonはgrain amountとspatial scaleを独立軸で記述するのに有効。ただしclippingと材質差があるため、単一のsigma補正式をCoreへ持ち込む根拠ではない。実験090/091からCore/preset変更なし。

## 実験 089: Core OpenCV linear RGBA resize式のhidden RGB感度

### 問いと条件

実験088でCore resizeのdefaultがchannelwise OpenCV linearと確認した。同じfloat32 linear interpolation modelで、alpha=0画素のRGBだけを変えるhidden-RGB感度を比較する。

- `tools/core_opencv_alpha_resize_model.py` はPython OpenCV 4.13.0の`cv2.resize(..., INTER_LINEAR)`をCV_32FC4相当のfloat32配列に適用する。source chartはalpha以外が同一で、alpha=0 RGBだけを(9,240,17)または(240,9,230)にする。
- Straightは4 channelを独立resize。PremultipliedはRGB×alphaをresize、output RGBをalphaでunassociate。Input/output alphaはどちらも同じlinear interpolationで縮小される。
- 保存domainをencoded sRGB numeric value / linear-lightへ分け、black/mid-gray/whiteへのlinear-light合成後にhidden-pair間のRGB/alpha差を測る。output scale .25/.375/.5/.75/1。
- 実行: `python tools/core_opencv_alpha_resize_model.py`。Metricsとscale .5 sheetは `captures/core-opencv-alpha-resize/`。

### 観測・判断

- scale .25–.75のstraight hidden-pair mid-gray composite sRGB RMSEはstored encoded valuesで.0086–.0125、linear-light preprocessingで.0063–.0089。RGB max differenceは.9059まで到達し、見えないsource RGBがlinear filter footprintへ混ざるモデル結果。
- Premultipliedでは全scale・両domainでRGB max differenceとblack/gray/white composite差が0。Alphaはpair間で同一。scale1ではstraightのvisible composite差0だがalpha-zero output RGBは違うためraw RGB max差は残る。
- 結果はhidden RGB感度の解析的期待と整合する。Core direct resize APIはdescriptorを見ずassociation補正しない。だが実Parametric SourceLayer bridgeでは`toQImage()`がalpha≈0 RGBをzeroするため、このhidden-pair differenceをそのまま製品経路へ帰属できない。部分alpha fringeや実QImage scaleの品質は未確認。Core itselfは未実行で、Python cv2 wheelのversion/buildもCore OpenCVと一致保証なし。
- Contact sheetはscale .5の4 domain/association rowを確認。premultiplied outputは3背景いずれでもhidden pair差が見えず、straightはedge/strand周辺で緑/紫のfringeが変化する。

### 限界・次

- Equation-level CPU model; no callsite fixture/runtime output, no embedded OpenCV version parity, no performance. OpenCV INTER_LINEAR may differ from Pillow BOX experiments;数値の直接比較はしない。
- Parametric SourceLayer resolverのQImage bridgeはalpha/transfer descriptorをUnknownとして再導入し、Core generic resizeのinput semanticsも明示されない可能性がある。次に画像importerとlayer framebuffer descriptor contractを読み、未知descriptorが通常なのか、このresolverだけのmetadata lossなのかを確認する。Core/app変更はまだ行わない。

## 実験 077: Bayer遷移率とalpha合成色誤差の関係

### 目的・追加測定

実験076のpalette occupancy / transition rateに、quantized RGBを元画像へ戻すsource-error指標を加える。transition rateが増えることは、元画像との誤差や見た目の改善と同じ意味かを確認する。

- 同じ3 SHA-verified CC0素材、54 palette/domain/strength conditionを使用。現行 `nearest_palette` と元alphaを使い、出力alphaが全画素一致することを確認。
- RGB sourceとpalette outputをsRGB値と仮定してlinearへdecodeし、visible / partial-alpha / boundary上のRGB MAE/RMSEを計算。alpha composite後の差分もlinear-lightで集計。
- alphaが同じなので、固体背景に対するlinear source-over deltaは背景項が相殺される。この計算上の性質をJSONにも記録した。
- 元PNGのgAMA/chromaticity/ICC metadataはJSONへ記録するが、suite本体同様にcolor-managed transformへ適用せず、明示的なsRGB-code assumptionを使う。

### 観測

| 素材 / palette / domain | dither 0: edge RMSE / transition rate | dither 1: edge RMSE / transition rate |
|---|---:|---:|
| silver feather / PICO-8 / sRGB | .0651 / 16.7% | .1171 / 65.3% |
| silver feather / PICO-8 / linear | .0636 / 17.5% | .0954 / 40.6% |
| silver feather / CGA / sRGB | .3015 / 13.4% | .2946 / 48.1% |
| clover full / CGA / sRGB | .2640 / 43.7% | .2659 / 82.8% |
| pixel hair atlas / CGA / linear | .1483 / 12.8% | .1706 / 25.0% |

- Feather / PICO-8ではtransition増加とalpha-composited edge RMSE増加が同時に起きた。一方Feather / CGA / sRGBではtransitionが増えてもRMSEは僅かに低下した。Clover / CGA / sRGBはstrength .5でRMSE .2521へ下がるが、strength 1では.2659へ戻る。応答は単調ではない。
- 同じalphaを維持するためblack/gray/whiteの固体背景間で、linear-light composite deltaは数値上同じになる。背景ごとの違いを評価するには色空間、alpha、または背景そのものを変える必要がある。
- density/occupancyはpatternの記述に、source-error指標は入力からの数値距離の記述に役立つが、どちらも好ましいretro lookの判定ではない。

### 限界・判断

- ErrorはsRGB transfer assumptionのlinear RGBで、知覚色差ではない。Feather PNGにはgAMA=0.45454とchromaticityがあるがcolor-managed transformは未実施。Cloverは9080-byteのsRGB ICCを埋め込むが、これも適用せずsuiteが読むcode valueを比較した。profile metadataを厳密適用した場合の色差は未測定。
- palette color outputに元alphaを戻す独立suite model。Artifact/Core実装、審美評価、人間acceptance、画面/動画安定性、性能は未評価。
- Bayer強度既定値変更、Core API追加の根拠は得られていない。palette look選択はoccupancy、transition、alpha composite deltaを併記して比較する。

## 実験 076: 固定palette occupancyとBayer隣接遷移を自然alpha素材で測る

### 問いと条件

実験016では合成opaque fixtureのRMSEとpalette color数を測った。色数だけではpaletteの大半が主色1色に偏っている状態を見落とすため、visible/partial-alpha領域のoccupancy entropyと近傍色遷移を調べる。

- 3 palettes（Game Boy風 / CGA風 / PICO-8風）×2 distance domains（sRGB / linear）×Bayer 4×4 strength 0/.5/1。
- SHA-256検証済み素材3点: CC0 clover cutout、CC0 silver feather illustration、CC0 pixel-art hair atlas。3つは別source family。透過率とpartial-alpha率が異なる。
- 現行 `nearest_palette` の色割当てを使用し、source alphaをそのまま出力へ戻す。visible occupancy、palette utilization、entropy-effective colors、dominant share、alpha-weighted occupancy、partial-alpha occupancy、水平/垂直隣接色遷移を記録。
- 再実行: `python tools/palette_occupancy_transition_study.py`。conditions JSONとalpha合成crop sheetは `captures/palette-occupancy-transitions/`。

### 観測

- PICO-8 / sRGBのsilver featherではstrength 0→1で使用色が8→9/16、visible horizontal transition rateが17.1%→66.3%、alpha-edge rateが16.7%→65.3%へ増加した。同じlinear domainではtransition rateが18.0%→41.9%だった。
- Hair atlasのCGA strength 1ではsRGB domainのvisible horizontal transition rate 66.5%、linear domainは33.4%。両方で4色使われたが、effective occupancyもsRGB 3.05 / linear 2.06と異なる。palette color数だけでは分布とpattern差を表せない。
- CloverのCGA / linear / strength 0は4色のうち3色が使われる一方、visible pixelの99.99%が単一色だった。sRGBでは4色すべてが使われ、dominant shareは95.69%。「palette color使用数」と実質的な色分布は別指標だった。
- strengthを上げた時にtransition rateが増える条件が多いが、その増加量はasset・palette・distance domainに依存した。transitionの多さはBayer pattern頻度を示す記述量で、好ましいretro appearanceやbandingの改善を示さない。
- 全54 conditionでalpha値は完全保持。結果はvisible/partial alphaの元coverageで集計し、透明背景画素をpalette occupancyの分母へ含めない。

### 解釈・限界

- Fixed paletteの評価では、少なくとも使用color数、dominant/effective occupancy、近傍transitionを併記すると、1色支配や高頻度checker patternを見落としにくい。
- 3素材のみで、clover/featherはillustration、hairはpixel-art atlas。自然写真の色分布や一般的なpixel artを代表しない。4×4 Bayerのみ、時間変動・CPU/GPU・performance・人手のlook acceptanceも未評価。
- palette/domain/Bayerは探索用の見た目選択として維持する。現Core APIやpreset既定値へ反映する根拠にはしない。

## 実験 075: 全retro presetの色domainを段階別に切り替える

### 問いと条件

現行4 presetはresize、quantize、chromatic interpolation、scanline、grainをsRGB code valueで処理する。1段ずつlinear lightへ変えた時、どの工程が出力を最も動かし、その影響はpresetと素材でどう違うかを調べる。

- `apply_retro` の現行出力をbaselineとする。各工程を個別にlinearへ切替えた5条件と全工程linearの計6条件を加える。4 preset（handheld / arcade / print / vhs）をすべて比較。
- dither/levels/strength、scanline係数、grain sigma、chromatic shift、seedは同じ数値で固定。grainはseed 79を全条件で共有。
- 3素材: SHA-256検証済みCC0 full clover、同CC0 macro cutout、CC0 opaque hair/fur texture。clover 2点は同一pack由来なので、独立したライセンス/素材系統は2つ。
- baseline再現が各素材×presetでpixel-exactであることを確認してから比較。alpha-plane一致も全variantで検証。
- 出力: `python tools/full_suite_color_domain_study.py`。条件別JSONとpreset別contact sheetは`captures/full-suite-color-domain/`。

### 観測

| Preset / domain toggle | Clover full linear RGB RMSE / changed pixel | Macro cutout | Hair/fur texture |
|---|---:|---:|---:|
| handheld: quantize linear | .0148 / 13.5% | .0380 / 41.2% | .0105 / 85.4% |
| handheld: grain linear | .0157 / 11.1% | .0145 / 32.5% | .0156 / 78.1% |
| print: quantize linear | .0113 / 13.5% | .0277 / 42.4% | .0102 / 84.8% |
| print: grain linear | .0218 / 11.7% | .0211 / 36.2% | .0212 / 77.6% |
| print: all linear | .0236 / 13.5% | .0344 / 42.6% | .0198 / 87.2% |

`changed pixel`は現行sRGB recipeとの差をlinear RGBへ変換し、alpha composite後に1/255を超えた全画素比。clover/macroでは同一のalpha coverageを使った。alphaは全ケースで完全保持。

- Quantizer domainはプリセット色の割当てを広範囲で変えた。ditherのないhandheldでもlevels変更は働くため、sRGBからlinearへの切替えだけで全素材の13.4〜87.4%画素が閾値を超えて変化した。
- 同じgrain sigma数値をlinearへ移すと、特にprint/vhsでlinear RGB RMSEがquantizer toggleより大きい素材があった。これはdomainに対して振幅を再調整していないためで、linear grainが悪い／良いという比較ではない。
- printはscale=1、scanline=0、aberration=0のためresize/chromatic/scanline domain toggleはbaselineと一致した。vhsのresize toggleもscale=1でno-op。該当stageが無効な時の差分ゼロを確認できた。
- 全linear条件の差は個別stage差の単純和ではなく、変換順序と量子化後の入力値が後段へ伝播した結果。各stageの実行domainを別々に決める必要がある。

### 判断と限界

- 色domainの統一linear化を採用する根拠にはしない。見た目変更量を決める主要因はpalette quantizationとgrain振幅で、同じ係数値をdomain間で比較すること自体が表現差を含む。
- これは現行suite出力との差分調査であり、linear RGB RMSEは見た目の良さを採点しない。目標lookやgrainの知覚振幅が未定義なのでpresetを変更しない。
- 2点のtransparent cutoutは同一CC0 packであり、hair/furはopaque texture 1点。写真、他のalpha matte、異なるpalette設計を一般化しない。GPU/Core parity、runtime、性能も未調査。

## 実験 074: OldTV GPU出力を透明layer中間面で往復するalphaモデル

### 問いとsource経路

OldTV GPU式がpremultiplied値を出した場合、Composition Viewのlayer intermediateと`LayerToFloat`変換を通すと、前のsource-over counterfactualで見えたalpha-zero RGBやRGB>alphaがどう扱われるか。前のsource追跡で未確認だったcaller境界を、呼び出し順と値域モデルで更新する。

- `prepareGpuLayerForBlend`は`layerSRV`を先に`convertLayerToFloat`へ渡し、その後に計画済みdeferred GPU raster passを実行する。`layerToFloatShaderText`はalphaでunassociateしsaturate、alphaが極小ならRGB=0にする。
- `buildGpuRasterEffectPlan`はPointwise/Spatial domainと対応nodeを要求する。OldTVは別の`apply`経路にcompute shaderを登録するが、この計画nodeを実装していない。したがって同変換をOldTVの生effect bufferへ直接適用する経路とは確認できない。
- OldTV式の出力値をstraightまたはpremultiplied producerとしてパラメータ化し、透明targetへのassociation適合source-over後に`LayerToFloat`式を適用。実callerのblend stateを示すruntime計測ではない。
- provenance manifestのSHA-256を確認したCC0 cutout 5点。実行: `python tools/core_oldtv_layer_roundtrip_study.py`。metrics: `captures/core-oldtv-layer-roundtrip/core-oldtv-layer-roundtrip-metrics.json`。

### 観測

- Straight producerをtransparent premultiplied targetへ描き、alphaで割って戻すモデルは5素材すべてでdirect straight compositeと完全一致（edge MAE=0、変更pixel比=0）。alpha-zero hidden RGBは`LayerToFloat`のalpha guardで0へ落ちる。
- Premultiplied producerでは、HLSL式のunscaled noiseによりalpha-zero RGBやRGB>alphaが発生する。premultiplied source-overを直接評価するcounterfactualとのedge差はMAE .00017〜.00064、変更pixel比7.7〜25.7%。このedge ROIはalpha-zero領域を含む。
- partial-alphaだけに限定するとMAE .000028〜.000058、最大差は約.005、変更pixel比2.0〜2.9%。この差は`LayerToFloat`側のstraight化後saturateにより、premult invariantを破る値が切られるモデルに対応する。

### 更新した解釈と限界

- 共通Composition View経路の`LayerToFloat`は、effect descriptor不明の生image consumerではなく、source-over後のpremultiplied layer render targetをstraight working surfaceへ戻す境界として使われる。前記source追跡だけからeffect output descriptor mismatchを製品不具合とみなすのは不適切。
- premultiplied noise counterfactualのedge差の大半は、alpha=0なのにRGBがnonzeroという不正premult値を直接premult compositeした場合に出る。layer roundtripはこの透明RGBを破棄する。部分alpha差は小さいが、意図した見た目か・当該効果の実blend conventionは未確定。
- GPU HLSLのNumPy式とsource-over数式のみ。Artifact.exe、GPU dispatch、実callerのeffect mode / blend state / descriptor capture、画質基準、性能は未確認。Core/Artifact実装は変更しない。
- 実行コード: `tools/core_oldtv_layer_roundtrip_study.py`。このsource traceは実行時の経路到達性やdevice出力の代用ではない。

## 実験 069: OldTV fringe のhidden-RGB境界感度を自然cutoutで測る

### 仮説

OldTV fringeはalpha planeを書き換えなくても、透明画素のRGBを横ずらし時に可視領域へ取り込み、半透明境界の合成色を変える。synthetic edgeだけでなく、provenance確認済みの自然cutoutでも同じ機序が見えるかを調べる。

### 手順

- `eval-corpus/cc0-white-clover/manifest.json`に登録された2つのCC0 clover cutoutを使用。実行時に各ファイルのSHA-256をmanifestと照合した。
- 各素材から、alpha-zero RGBだけが異なりvisible RGBAとalpha planeが完全一致する一様色pairと9×11 checker pairを生成。
- `OldTVEffect.cpp`のfringe式をPythonで再現し、fringe 0 / .3 / 1.0を適用。curve、flicker、scanlineは0相当。
- black / mid-gray / white背景へstraight source-overをnormalized sRGB code spaceで計算し、whole / visible / edge / partial-alpha ROIごとにpair差を測定。全2素材×2 pattern×3 fringe=12条件。
- `tools/core_oldtv_natural_alpha_study.py`を実行。ROI contact sheetとmetrics JSONを `captures/core-oldtv-natural-alpha/` に生成。

### 観測結果

| 素材 | fringe | hidden pattern | mid-gray edge MAE | P95 | 1/255超の差があるedge pixel比 |
|---|---:|---|---:|---:|---:|
| `2e325727cf8369e7.png` (1017×1807) | .3 | uniform | .01979 | .01569 | 17.32% |
| 同上 | .3 | checker | .01993 | .01569 | 17.34% |
| 同上 | 1.0 | uniform | .07234 | .76822 | 42.18% |
| 同上 | 1.0 | checker | .07473 | .79565 | 42.24% |
| `f347ea575b04c894.png` (1037×1347) | .3 | uniform | .00583 | 0 | 14.12% |
| 同上 | .3 | checker | .00585 | 0 | 14.43% |
| 同上 | 1.0 | uniform | .02240 | .10588 | 31.42% |
| 同上 | 1.0 | checker | .02303 | .10588 | 32.38% |

- 12条件すべてでalpha出力の最大code差は0。これはalpha保持を示すが、RGB境界の不変性は示さない。
- fringe .3でもedge pixelの14〜17%に1/255超のpair差があった。二つ目の素材のP95が0なのは、差がedge samplesの5%未満に局在しているためで、MAEとchanged ratioでは差が観測されている。
- 隠れRGB由来のR/Bサンプルは可視destinationで検出され、fringe増加とともに増えた。量と画素位置は素材alpha輪郭に依存する。
- contact sheetでは、自然な葉や三つ葉の輪郭にhidden RGB由来の色fringeが出ることを確認した。F=1差分は4倍表示。
- 初期GPU座標probeはalpha変化率を過小計上し、GPU shader output equationも含んでいなかったため、GPU比較としては実験070で置換した。実験069のhidden-RGB pairとCore fringe測定は有効なまま。

### 解釈と制約

- 実素材でも「alpha plane保持」と「transparent-edge color invariance」は別の性質であることを確認した。評価では全画面平均だけでなくedge ROIと変更pixel比を併記する。
- 経路照合でCPU/GPUの実装差を確認した。`ArtifactEffectService` は `builtin.old_tv` を `ArtifactOldTVEffect` に登録する。Artifact版のCPU処理は `ArtifactCoreCreativeEffect` bridgeを通ってCore `OldTVEffect` を呼び、Fringe parameterを含む。Artifact GPU処理は別HLSLへ分岐し、固定hash-jitterとscanlineを適用、alphaはRGBと同じsource coordinateからsampleする。HLSLにはFringe parameterがない。追加probeはsampling coordinateのみをモデル化し、scanline/noise outputとruntimeを含まない。
- Coreテスト `CreativeImageEffectContractTest.OldTvFringeOffsetsRedAndBlueInOppositeDirections` はsynthetic rampでchannel shiftを確認し、`OldTvChangesRgbWithoutChangingAlpha` 等はalpha不変を確認するが、自然cutoutのhidden-RGB invarianceやcomposite resultはテストしていない。Artifact `RenderQueueLayerImageIntegrationTest` のGPU OldTV契約はalpha保持・jitter・scanline・CPU/GPU差を検査する一方、fringe parameter parityは扱わない。テストは実行していない。
- これはsource-derived Pythonモデルであり、Core/Artifact runtime、実際の製品caller、GPU parity、性能、望ましい画質の判定ではない。VideoFrameのalpha associationと当該フレームの入力由来は未確認。straight source-overのnormalized sRGB code spaceは測定上の仮定。二素材の結果を一般画像へ一般化しない。
- 現時点ではCoreのfringe policy変更を正当化しない。次はHLSLのscanline/noiseを含む値モデルを同じreal cutoutで仕上げ、HDR/negative inputにおけるCPU/GPU clamp差を測る。その後で実callerの入力・alpha associationを追う。

## 研究目的

静止画をゲーム機風・印刷物風に変換する際、量子化方式、固定パレット、ordered ditheringが画質と見た目に与える影響を分けて評価する。ArtifactStudioへの移植を前提にせず、まず再現可能な見た目の根拠を集める。

## 実験 001: 色量子化とordered dithering

### 仮説

1. 画像ごとに作る適応パレットは色誤差を抑えやすいが、特定ハードウェアを模倣する固定パレットの再現には向かない。
2. Bayer ditheringは階調帯を視覚的に分散する一方、元画像からの画素誤差を必ず増やす。
3. Bayer 4×4と8×8は同じ量子化段数でも粒度が違うため、RMSEだけで好みや用途を選べない。

### 手順

- Pillow 12.2.0 / NumPy 2.4.3 / Python 3.14.2。
- 320×200の合成RGBチャート（平坦背景、幾何形状、複数色、水平細線）を入力。
- 4階調のBayer 4×4 / 8×8を、ディザ強度0 / 0.5 / 1.0で比較。
- Game Boy風4色、CGA風4色、PICO-8風16色をnearest-colorとBayer変調ありで比較。
- 指標は元画像に対する線形RGBの画素RMSE。表示用比較シートは `output/research-fixture_quantization-study.png`。

### 観測結果

| 方式 | 設定 | RGB RMSE |
|---|---:|---:|
| Bayer 4×4 | 強度 0.0 | 0.09174 |
| Bayer 4×4 | 強度 0.5 | 0.10675 |
| Bayer 4×4 | 強度 1.0 | 0.13284 |
| Bayer 8×8 | 強度 0.0 | 0.09174 |
| Bayer 8×8 | 強度 0.5 | 0.10578 |
| Bayer 8×8 | 強度 1.0 | 0.13442 |
| Game Boy風 palette | nearest | 0.31729 |
| Game Boy風 palette | Bayer 4×4 | 0.32811 |
| CGA風 palette | nearest | 0.26722 |
| CGA風 palette | Bayer 4×4 | 0.32067 |
| PICO-8風 palette | nearest | 0.06581 |
| PICO-8風 palette | Bayer 4×4 | 0.12659 |

- Bayerの強度を上げるほどRMSEが増加した。ディザは誤差低減手法ではなく、制約された階調の空間分布を変える表現手法として扱う。
- この入力はPICO-8 paletteの色と偶然よく一致するため、そのRMSEを一般画像へ一般化できない。
- 4×4と8×8のRMSEは近いが、比較画像では周期パターンの大きさと視覚的なざらつきが異なった。
- 固定パレットの選択がディザの有無より結果を大きく左右した。自動パレット化と固定パレット模倣は別モードとして扱うのがよい。

### 解釈と限界

- RMSEは知覚色差、バンディングの目立ち、レトロらしさを測らない。次段では複数の写真・グラデーション・細線チャートを使い、視覚評価と色差指標を加える。
- この実験はsRGB値を線形RGBとして扱う簡易指標である。物理的な線形光誤差や知覚均等色空間の評価ではない。
- 現在の8-bit画像処理実験はCoreのfloat/HDR画像契約を検証していない。
- PICO-8 paletteは実験内の代表値であり、公式ハードウェアの厳密な表示モデルを保証しない。

### Coreへの反映判断

現時点ではCoreへ移植しない。ArtifactCoreには既に `Halftone`、`Old TV / CRT` と `CurvesSettings::posterize(int levels)` がある。コード上、posterize factoryはmaster curveに段差点を作る汎用階調処理であり、固定paletteへの最近傍割当てや空間ディザ機能は示していない。今回の知見は効果統合よりも、palette量子化とordered ditheringの仕様分離・評価方法にある。Core API追加を検討する前に、既存Layer Effect/GPU経路と接続する価値、alpha保持、端数レベル、CPU/GPU parityを定義する。

## 再現

```powershell
python retro_lab.py fixtures/quantization-fixture.png --preset print --research-sheet --output output/research-result.png
```

## 実験 002: 透明境界とretro処理

### 仮説

1. straight alpha画像のRGBをalphaと独立に縮小すると、完全透明画素の隠れた色が半透明境界へ混ざる。
2. RGBだけをずらして元alphaを固定する色収差は、透明境界の色縁を作る。alphaも一緒にずらす方法やチャンネルごとのalphaずらしは異なる結果になる。
3. 粒状感をalphaで制限しないと、透明画素の隠れRGBを変えても通常の合成結果には現れず、半透明領域では色変化として現れる。

### 手順

- 完全透明画素をmagenta、opaqueな領域を赤、alpha rampを橙、半透明領域を青にした256×128 fixtureを生成。
- 50%へBOX縮小後、BILINEARで元サイズへ戻す手順を比較。RGBをそのままresizeする方法と、RGB×alphaをresizeしてからalphaで除算する方法を比較。
- RGB channel offsetは元alpha固定、共通shift alpha、channelごとのshift alphaを比較。
- 白・黒・緑の背景へ合成した比較画像と、元alpha ramp上での白背景MAE、橙領域のblue-channel偏差を計測。
- 比較画像は `captures/alpha-edge-study.png`。

### 観測結果

| 処理 | 白背景 edge MAE | 橙edge blue偏差 |
|---|---:|---:|
| Source | 0.00000 | — |
| straight RGB縮小 | 0.00371 | 0.06646 |
| premultiplied RGB縮小・unpremultiply | 0.00434 | 0.05402 |
| RGB shift・元alpha固定 | 0.01664 | — |
| RGB shift・共通shift alpha | 0.04739 | — |
| RGB shift・channel別alpha shift | 0.02999 | — |
| alphaを考慮せずgrain適用 | 0.04206 | — |

- このfixtureではpremultiplied処理が橙境界へのmagenta由来blue混入を小さくしたが、edge全体の白背景MAEはstraight処理よりわずかに大きい。単一の汎用誤差値だけでは、特定のhalo色と輪郭変化を評価しきれない。
- RGB shift時にalphaを単純に共通移動すると境界位置が変わり、edge MAEが最も大きくなった。alpha保持と色ずれを一体で扱う場合にも、どのチャンネルのcoverageを採るかを仕様化する必要がある。
- alpha=0のpixelに残るRGBは直接合成されない。問題となるのは、その値を補間してalpha>0領域へ持ち込む処理と、半透明pixel上でRGBを変更する処理である。

### Coreとの関係

ArtifactCoreは `ImageF32x4_RGBA` のCPU backingをBGRAとし、surface descriptorでstraight / premultiplied、transfer functionを管理する。`SurfacePixelConversion` は変換時にpremultiplied入力をunpremultiplyし、透明pixel RGBを0にしてから処理し、出力契約に従って再premultiplyする。今回の研究画像は8-bit sRGBであり、このCoreのfloat/transfer contract全体を検証していない。既存Coreに色収差や画像縮小があるため、現段階ではCoreコードを変更せず、次はlinear light、descriptor alpha mode別のround trip、既存effect経路での境界挙動を確認する。

### 限界と次の確認

- 透明境界のfixtureは単純な2D図形であり、写真やアンチエイリアス済み素材の代表とは限らない。
- PillowのBILINEAR/BOXはArtifactCoreのresize/GPU sampling実装と一致する保証がない。
- MAEはperceptual metricではない。alpha mode、色転送、background別の視覚評価を維持して比較する。
- channelごとのalpha shiftは試作上の仮説であり、製品仕様として推奨しない。

## 実験 003: sRGB code valueとlinear light

### 仮説

1. 同じ数値係数を使う量子化・減光でも、sRGB符号値に直接適用する場合とlinear lightに適用する場合で出力は大きく異なる。
2. scene/display color処理の意味を保つには、処理ごとに意図するtransfer domainを選ぶ必要がある。全retro処理を一律linear化するのが正解とは限らない。
3. 固定paletteの距離計算もtransfer domainで割当てが変わる。

### 手順

- `fixtures/color-space-fixture.png`: 384×192のgrayscale ramp、RGB色相ランプ、平坦色見本、水平細線を含む合成画像。
- 4段階posterize、各2行ごとの0.65減光、PICO-8風16色のnearest-color割当てを、sRGB code value空間とlinear light空間で個別実施。
- 出力をsRGB表示値へ戻し、入力をsRGB decodeしたlinear RGBを基準にlinear RGB RMSEを計測。
- 比較画像は `captures/color-space-study.png`。差分画像はposterizeの表示code値差を3倍にして表示する。

### 観測結果

| 処理 | sRGB codeで処理 | linear lightで処理 |
|---|---:|---:|
| 4段階 posterize RMSE | 0.10633 | 0.08305 |
| 0.65 scanline減光 RMSE | 0.15801 | 0.09008 |
| PICO-8 nearest RMSE | 0.12547 | 0.11035 |

- このfixtureとlinear-RGB RMSEではlinear処理の誤差が3項目すべてで小さかった。これはlinear domainで処理する方がこの指標に適合するための傾向を示すが、見た目のレトロ感や知覚品質の優劣を直接証明しない。
- sRGB codeとlinear lightのposterize画像の平均絶対表示code差（3倍表示前）は約0.15709で、処理domainを変えると見た目の段差位置が大きく変わった。
- 減光はlinear domainでは光エネルギーを係数倍するため、出力sRGB code valueを同じ係数で直接減らす処理より明るく見える領域が多い。CRT/scanline表現としてどちらを望むかは視覚設計上の選択になる。
- 固定paletteへのnearest割当ても距離空間により異なる色が選ばれる。hardware paletteの離散色そのものはsRGBで定義しておき、距離計算domainを明示的な選択肢として扱うのが妥当。

### Coreとの関係・判断

`Graphics.SurfaceColorContract` の `SurfaceColorDescriptor` はtransfer function、primaries、alpha mode、rangeを保持し、`SurfacePixelConversion` はtransferをlinearへdecode／encodeする。したがってCore側の基盤契約は既に存在する。一方、今回の試験は単純な8-bit sRGB fixtureであり、既存Layer Effectがdescriptorを参照して処理domainを選んでいるか、GPU shaderが同じdomainを使うかは確認していない。retro効果の見た目上の目的もあるため、無条件にCoreをlinearizeする変更は行わない。今後Coreへ反映する候補は、既存効果が意図するworking domainを明記し、CPU/GPU parityを比較する検証fixtureとする。

### 限界と次の確認

- RMSEは線形RGBであり、知覚色差、bandingの目立ち、retro表現の好みは評価しない。
- 同じ入力、同じ係数を使ったdomain差の分離実験であり、実写、透明画像、HDR、広色域を網羅しない。
- sRGB transferのroundtripだけを扱い、Display-P3、Rec.2020、alpha premultiplicationは含めない。
- 次は実写と滑らかなgradient素材で、色差／banding指標と目視比較を併用し、既存Core effectの実際のworking domainをコード追跡する。

## 実験 004: 色ずれの補間・端処理・subpixel量

### 仮説

1. wrapは画像端の反対側の色を混ぜ、端に不自然な色を作る。clampとreflectも端の見え方が異なる。
2. nearest移動は0.5 pixelを跨いだ時に出力位置が跳ぶ。bilinearはsubpixel量を連続的に反映する。
3. bilinear interpolationのsRGB code / linear light選択も境界色へ影響する。

### 手順

- `fixtures/chromatic-fixture.png`: 320×160、端の単色pixel、暗いfield、強いRGB縦bar、微細走査線、色gradientを含むfixture。
- Rを+shift、Bを-shiftする単純な水平色ずれを適用。amount 2.0 / 2.49 / 2.5 nearest、2.5 bilinearを比較。
- 2.5 bilinearでclamp / wrap / reflectを比較し、clamp / sRGB補間に対する全体および左右8px境界のlinear RGB mean absolute deltaを測定。
- 比較画像は `captures/chromatic-shift-study.png`。

### 観測結果

| 設定 | 全体linear delta | 端8px linear delta |
|---|---:|---:|
| nearest 2.0 / clamp / sRGB | 0.00176 | 0.00423 |
| nearest 2.49 / clamp / sRGB | 0.00176 | 0.00423 |
| nearest 2.5 / clamp / sRGB | 0.00203 | 0.00887 |
| bilinear 2.5 / clamp / linear | 0.00097 | 0.00450 |
| bilinear 2.5 / wrap / sRGB | 0.00163 | 0.03263 |
| bilinear 2.5 / reflect / sRGB | 0.00241 | 0.04814 |

baselineのbilinear 2.5 / clamp / sRGBはdelta 0。2.49から2.5へのnearest閾値で全体差は0.00176から0.00203、端差は0.00423から0.00887へ増えた。wrapとreflectはこのfixtureの端8pxに対して、clamp bilinearより大きな差を作った。

### 既存実装のコード調査

- Core `ChromaticAberrationEffect::process` はAmountから整数shiftをroundし、R/Bを反対方向へ移動し、端をclampする。既存Core契約テストは0.49 pixelでは無移動、0.5 pixelでは1 pixel移動する丸め境界を固定している。
- Artifactの静止画像CPU実装も半径方向のoffsetを最近傍roundしてclampする。GPU shaderも整数Load座標へround/clampしており、補間方式の意図は揃っている。
- Artifactのこのeffectの直接GPU適用経路は入力・出力・staging resource等を作り、readback後に`WaitForIdle()`を呼んでいる。通常のGPU空間effect経路との責務・呼び出し頻度は別途確認が必要。

### 解釈・Core反映判断

- このfixtureでは、見た目の選択としてclamp以外の端処理を選ぶ根拠がなく、wrapは特に反対端色の混入が明瞭だった。Retro Labの既定channel offsetをwrap / integer roundingからclamp / bilinearへ変更した。
- Coreの0.5 pixel閾値は既存仕様とテストに裏付けられている。下位互換性を保ったまま変更する根拠はなく、Coreは変更しない。
- Artifact CPU/GPUは現在nearestで揃えている。これをbilinearへ変えると既存画像の変化とCPU/GPU parity範囲を確認する必要がある。今回は実験の数値だけでproduction変更しない。
- GPUのresource生成／readback／同期待機は性能上の別調査候補としてInsightに記録した。今回の画作り研究には含めない。

### 限界・次の確認

- 水平固定shiftであり、実際のArtifactは中心からのradial shift。radial distortionの画面位置別pixel分布は未比較。
- 端8px平均は知覚上のseam評価ではなく、fixture依存の差分指標。
- sRGB / linear補間は一つのpixel stepを比較しただけで、alpha/premultiplied colorや広色域は対象外。
- 次はradial field上で補間とshift clampを比較し、Coreのchannel-plane effectとArtifactのRGBA effectの契約差を整理する。

## 実験 005: radial chromatic aberrationのoffset分布

### 仮説

1. radial色ずれは中心近傍でほぼゼロ、外周ほど大きくなるため、固定方向channel shiftとは明確に違う画面分布になる。
2. centerを動かすと大きいoffsetを受ける画面領域が偏る。
3. nearest samplingは円形edgeや斜め線に段差を出し、bilinearとの差が高コントラスト形状で観察できる。

### 手順

- `fixtures/radial-chromatic-fixture.png`: 256×256のHSV hue wheel、同心円、中心を通る水平・垂直線を含む。
- Artifact CPU/GPU実装と同じradial距離の式を使い、Rを外向き、Bを内向きにamount 8pxずらす。
- center 50/50と35/65、nearestとbilinear、sRGBとlinear補間を比較。固定水平bilinear shiftも比較対象にする。
- baselineはcenter 50/50 radial bilinear sRGB。全体と外周8px帯のlinear RGB mean absolute deltaを測定。
- 比較画像は `captures/radial-chromatic-study.png`。

### 観測結果

| 設定 | 全体linear delta | 外周8px delta |
|---|---:|---:|
| radial nearest / center 50-50 / sRGB | 0.00427 | 0.00446 |
| radial bilinear / center 50-50 / linear | 0.00080 | 0.00076 |
| radial bilinear / center 35-65 / sRGB | 0.02489 | 0.01891 |
| directional bilinear / 8px / sRGB | 0.07069 | 0.05876 |

radial bilinear sRGB baselineとの差は0。中心50/50では計算上の平均offset 4.37260px、最大8.08122px。中心35/65では平均4.82044px、最大10.47407px。amountが画面上の厳密な最大pixel移動ではなく、正規化半径に掛ける係数であることを示す。

### 既存実装との対応

- Artifact CPUと通常GPU spatial shaderは、`distance / (max(width,height) * 0.7)` を正規化半径とし、中心からの方向へred/blueを反対向きに移動、nearest整数Load、clampを使用する。
- Core `ChromaticAberrationEffect` はangleに沿った一様parallel shiftでありradial fieldではない。両APIは同名の色ずれでも画面分布が異なる。
- Coreの「Amount」はinteger pixel移動量へroundされる。Artifactの`redShift`/`blueShift`はradial正規化式に対する上限scaleであり、外周位置に応じた実offsetとなる。

### 解釈・Core反映判断

- radialとdirectionalの差はこのfixtureでnearest/bilinear差より大きく、両者は別のlookとして選べるべき性質である。Core fixed-shiftをradial化する変更根拠にはならない。
- center変更はoffset場全体を組み替えるため、大きな視覚差を作る。artifact側のcenter propertyは意味のあるlook controlである。
- CPU nearestとbilinearの差は小さいが、細い放射線・写真の文字など未検証素材で結果が変わる可能性がある。現在CPU/GPUがinteger Loadで揃う契約を優先し、今回production samplingを変更しない。
- Coreにradial modeを追加する場合は別mode／別APIとして扱い、既存一様shiftの後方互換を保つ必要がある。現時点では別modeの利用需要やCore effectを呼ぶ実際の静止画導線を確認できていないため追加しない。

### 限界・次の確認

- high-contrast hue wheelのため、実写での色縁の自然さ、centerの視覚的最適位置、amount上限は判断できない。
- Core / Artifact rendererでのbackend captureやCPU/GPU parityを測定していない。
- 次は写真・細文字素材とalpha境界を含むfixtureを使い、radial量の知覚評価と画像外サンプルの扱いを確認する。

## 実験 006: film grainの輝度包絡・空間粒度・時間安定性

### 仮説

1. film-like midtone envelopeは一様noiseより暗部・明部のgrain量を減らす。
2. monochromeとindependent RGB noiseでは色ノイズの性質が変わる。
3. 固定seedは同じ入力frame間で粒子配置を完全に固定し、seed更新はフレーム間flickerを作る。粗いnoise fieldはneighbor相関を上げる。
4. Core `NoiseType::FilmGrain` のamount依存を式から確認し、量の応答が線形か二次かを定量化する。

### 手順

- `fixtures/grain-fixture.png`: 320×200、sRGB code値0.04 / 0.16 / 0.32 / 0.50 / 0.68 / 0.84 / 0.96の灰色帯と赤／青のcolor swatch。
- amount 0.08、別seedで一定monochrome、midtone weighted monochrome、Core式、独立RGB、4px/8px低解像noiseを生成。
- 各gray bandの出力perturbation標準偏差を測り、neighbor相関と独立RGB間相関も計測。
- 同一seedを2frameへ適用する場合と異なるseedで適用する場合のframe間RGB RMSを比較。
- 比較画像は `captures/grain-study.png`。

### 観測結果

| gray level | constant mono σ | midtone envelope σ | Core式 σ |
|---:|---:|---:|---:|
| 0.04 | 0.08031 | 0.01213 | 0.00097 |
| 0.50 | 0.07904 | 0.07916 | 0.00647 |
| 0.96 | 0.07978 | 0.01328 | 0.00105 |

| 構造指標 | 結果 |
|---|---:|
| 固定seed frame差分RMS | 0.00000 |
| seed変更 frame差分RMS | 0.08327 |
| 独立RGB R-G correlation | -0.00453 |
| white noise horizontal correlation | -0.01243 |
| 4px coarse noise horizontal correlation | 0.92573 |

- 一定grainは全階調にほぼ同じsigmaを足す。midtone envelopeは0.5付近を維持し、0.04 / 0.96を弱めた。
- 固定seedでは同一画像の連続frameのgrainが静止し、seedを変えるとamount 0.08条件で大きなframe差が発生した。
- 独立RGBは色粒状を生み、monochromeは同じnoiseを全色へ加えるためluma寄りの粒状となる。4px coarse fieldは隣接相関が高く、fine white noiseとは別のtextureである。
- Retro Lab本体の現行grain処理は固定seed由来の独立pixel白色noise fieldをRGB共通で加算し、luminance envelopeは持たない。frameごとに同じseedで呼ぶと時間方向は固定される。
- Coreのコードは`RandomStream::gaussian(mean, stddev)`でstddev=`amount`のnoiseを作り、その後`grainAmount = amount * 4 * lum * (1-lum)`を乗算する。したがって出力sigmaは`amount² * 4 * lum * (1-lum)`。amount 0.08 / mid-gray時の理論sigmaは0.0064、実験式の実測は0.00647。標準偏差ベースでamountを解釈するmidtone modelの0.07916に対して約1/12.5。

### Core反映判断

CoreのNoise API文書はamountをintensity (0..1)とだけ説明し、FilmGrainで二乗する意図やamount応答を明記していない。一方、二次応答は意図的な低強度カーブかもしれず、公開APIの互換挙動でもある。内部callsiteは本体検索で見つからず、`generateNoiseTexture()`だけが内部で`addNoise()`を呼ぶ。今回の根拠は静的式とPython統計モデルであり、実C++出力を実行していないため、Coreを変更しない。次のCore反映候補はamountの意味をσとstrengthのどちらにするか仕様確認した上での回帰契約である。

### 限界・次の確認

- fixtureは一様grayの帯が中心で、実写の輝度・textureとgrainの相互作用を測っていない。
- σとcorrelationは合成式の分布統計であり、film grainの物理再現、印刷dot、perceptual matchを証明しない。
- Coreの`cv::cvtColor` grayは入力domainのlinearizeを行わず、fixtureのgray valuesはsRGB code values。wide-gamut/linear inputの扱いは未検証。
- 次は実写portrait/sky/flat graphicでmidtone weightingとcoarse grainを目視比較し、grain scaleを画像解像度に対してpxとrelative-sizeのどちらで定義するか決める。

## 実験 007: retro effect orderとscreen phase

### 仮説

1. pixelate前後のBayer／scanline／grainは異なる空間周波数を残し、順序変更は同一素材でも結果を大きく変える。
2. half-resolution pixelation前に加えたgrainは縮小平均で弱まり、後から加えたgrainより粒状感が減る。
3. Bayer matrixのoriginとscanlineの奇偶phaseは画素格子に固定され、1px phase変更でも同じ画像の量子化結果が変わる。

### 手順

- `fixtures/color-space-fixture.png`: 前実験で使用した384×192のgradient、flat color、水平細線fixtureを再利用。
- pixelateはBOX downscaleを0.5、NEAREST upscaleを1.0で実施。Bayerは4×4、12階調相当、strength 0.7。scanlineは奇数行を0.65倍。grainは固定seed 481、σ=0.035のRGB共通white noise。
- 現行順（pixelate → Bayer → scanline → grain）を基準に、各処理をscale前／quantize前後へ動かしたvariantを比較。
- Bayer matrixをx/yで0 / 1 / 2 pixel roll、scanlineの暗い行をodd/evenで反転。
- baselineとの差をlinear RGB MAE、1/255を超えて変化したpixel比率で記録。grain sigmaはpixelate前後のnoise fieldを直接測定。
- 比較画像は `captures/pipeline-order-study.png`。

### 観測結果

| 変更 | baseline比linear MAE | 変化pixel比率 |
|---|---:|---:|
| scanlineをpixelate前へ | 0.08268 | 94.07% |
| Bayerをpixelate前へ | 0.01398 | 80.88% |
| grainをpixelate前へ | 0.02248 | 91.74% |
| grainをBayer前へ | 0.01863 | 88.76% |
| scanlineをBayer前へ | 0.00692 | 49.07% |
| grainをscanline前へ | 0.00293 | 37.31% |
| Bayer phase x+1 | 0.01909 | 56.33% |
| Bayer phase y+1 | 0.01921 | 54.12% |
| Bayer phase x+2,y+2 | 0.00245 | 8.81% |
| scanline dark phaseをoddからevenへ | 0.16413 | 97.80% |

- σ=0.03496のfull-resolution grainは半縮小・nearest拡大後σ=0.01749となった。独立noiseを2×2平均する縮小が粒状の振幅を下げる。
- この入力ではscanlineをscale前へ置いた場合の差が最大。半分へ縮小する際にscanline周期も縮小samplingと相互作用し、final-resolution scanlineとは違う帯域になる。
- Bayerのx/yを1pxずらすと画像の半数以上のpixelが1/255超変化した。2,2 phaseはBayer4の対称性とこのfixtureの分布により変化が小さいがゼロではない。
- scanlineの暗行を反転すると全画面の明暗格子がずれ、今回のfixtureでは最大差となった。位相を明示しないcrop／tile運用では切り出し位置ごとにlookが変わりうる。
- 効果順のMAEは「どちらが正しいか」ではなく、順序がlookの一部であることを示す。retro presetを組む際にはorderとphaseをpreset契約として固定すると再現しやすい。

### 現行ラボとの対応・Core判断

- `apply_retro` はpixelate相当のresizeを先に行い、その後Bayer、channel offset、scanline、grainを処理する。Bayerのoriginは出力左上、scanlineのdark rowは奇数yで、grainは同じseedを使う呼び出しで固定される。
- 今回の値は静止画ラボのsRGB code-value処理とPillow BOX/NEAREST縮小に限る。ArtifactCoreのGPU/backend画素結果を測っておらず、Core APIへの移植根拠にはしない。
- scanline／Bayer phaseの安定性は既存の高品質な比較表現要件であり、現行ラボのframe origin固定を維持する。production codeは変更しない。

### 限界・次の確認

- 1つの合成fixtureと1つの縮小比だけであり、奇数寸法、0.75 scale、実写、alpha境界、別Bayer matrixは未確認。
- MAEとchanged pixel ratioは差分量を示すだけで、画質・好み・bandingの優劣は測らない。
- 次はsource crop offset／odd dimensionsを使い、frame origin固定とsource-relative phase固定のどちらが用途に適するか確認する。

## 実験 008: odd-size crop/tileでのBayer・scanline phase

### 仮説

1. Bayerとscanlineをcropごとにphase 0から始めると、odd tile offsetでframe全体処理とパターンがずれて内部seamができる。
2. 0.5 / 0.75のpixelation scaleやodd output dimensionsでも、最終output frameのoriginを各tileへ渡せばscreen phaseは全体処理と一致する。

### 手順

- `fixtures/color-space-fixture.png` の左上383×191を使用。output dimensionを意図的に奇数にし、scale 0.5 / 0.75で全体pixelate（BOX downscale後NEAREST upscale）。
- pixelate済みの画像を127×63 tileへ分け、tile内Bayer 4×4 / scanline処理を比較。tile-localは各tileでorigin=(0,0)へreset、source-relativeはtile left/topをphase originとして使用。
- Bayer dither後にscanlineを適用。内部tile seamから両側2pixelの帯をseam ROIとし、source値と同じsRGB code domainでMAE、変化pixel率、最大code差を比較。
- 比較画像は `captures/crop-phase-study.png`。

### 観測結果

| scale | reduced size | phase方式 | seam code MAE | whole code MAE | changed pixel ratio |
|---:|---:|---|---:|---:|---:|
| 0.50 | 192×96 | tile-local reset | 0.07672 | 0.05891 | 49.68% |
| 0.50 | 192×96 | source-relative | 0.00000 | 0.00000 | 0.00% |
| 0.75 | 287×143 | tile-local reset | 0.07594 | 0.05904 | 51.19% |
| 0.75 | 287×143 | source-relative | 0.00000 | 0.00000 | 0.00% |

tile-localの最大code差はいずれのscaleでも0.40909。source-relativeは全画素で全体処理と一致した。output frameは383×191、tile寸法127×63で、crop開始座標の奇数／Bayer周期との非整合を含む。

### 解釈

- Bayer/scanlineを画像全体のscreen effectとして見る場合、phaseは処理単位のcrop originではなくoutput-frame coordinateから導出する。tile内でpatternを再起動するとtile幅／高さがperiodの倍数でない場所にphase discontinuityが出る。
- 今回のsource-relativeはframe座標を明示的にtileへ渡した。全体処理と完全一致したのは両演算がpixel-localで、scaleを先に全体へ適用したためである。
- 独立tileごとにresizeを行うケース、tileごとにscale roundingが変わるケース、GPU shaderでdispatch originが違うケースは測定していない。現結果をArtifact/Core実装の parity証明へ一般化しない。
- 表現上patternをtileごとに再開したい素材もありえるため、local phaseは誤りと断定せず、frame-lockedか tile-lockedかをeffect contractとして選ぶ必要がある。

### Core反映判断・次の確認

Coreへscreen effect APIを追加・変更する段階ではない。静止画ラボの比較処理はcrop seamを避ける場合にframe originを渡す必要性を示すが、production rendererのtiling/dispatchが存在するか未確認である。次はArtifactのDiligent spatial/compute経路でdispatch座標、tile bounds、effect coordinatesの関係を調べ、実際にcrop単位のphase resetが起きる経路があるか確認する。

## 実験 009: Chromatic Aberration GPU適用経路の到達性

### 問い

`ChromaticAberrationGPUImpl::applyGPU` の同期readbackが通常のCompositionフレーム描画で実行されるか、それとも別のCPU surface用経路に限られるかをコード上の呼出し経路から確認する。

### 確認方法

- `ChromaticAberrationEffect.ixx` のGPU空間ノード登録と、`ArtifactCompositionRenderController.cpp` の `buildGpuRasterEffectPlan` およびレイヤー描画適用箇所を追跡。
- `ArtifactAbstractEffect::applyConfigured` / `applyToCpuSurface` とCompositionのCPU surface builder呼び出し箇所を追跡。
- `ChromaticAberrationEffect.cpp` の単体GPU実装、および `ArtifactRenderLayerPipeline.cpp` の空間ノード実装を照合。

### 確認結果

- `ChromaticAberrationEffect` は `supportsGPU() == true` と `gpuRasterEffectDomain() == Spatial` を宣言し、`appendGpuSpatialNodes()` で固定容量の `GpuSpatialEffectStack` にノードと4個のパラメーターを登録する。
- Compositionのラスター効果計画はRasterizerかつ非CPU設定の各効果についてdomain別ノードを収集し、レイヤー結果処理はspatial passを `RenderPipeline::applySpatialEffect()` へ渡す。このeffectの通常適用先はこのGPU resident経路である。
- CompositionのCPU surface builderは効果ごとに `applyToCpuSurface()` を呼ぶ。GPU実装へ落とさないようコメントでCPU境界を明示しており、GPU対応効果では `applyCPUOnly()` を選択する。従ってこの経路も単体GPU readbackを呼ばない。
- 単体 `ArtifactAbstractEffect::apply()` はGPUモードなら `gpuImpl_->applyGPU()` を直接呼ぶ。Chromatic Aberration側ではここで入力・params・readback textureを生成し、毎回 `Flush()` / `WaitForIdle()` 後にCPUへreadbackする。
- 通常Composition経路では、Chromatic Aberration shaderは既存の入力SRV、scratch UAV、再利用されるexecutor/parameter bufferを用い、出力をGPU内で後続処理へ渡す。単体GPU実装のCPU readbackを含まない。
- コード検索で確認できた `applyConfigured()` の直接呼び出しはテスト箇所のみ。production側の代表的なComposition CPU経路は `applyToCpuSurface()`、GPU経路はplan/spatial dispatchである。

### 解釈・限界

- 通常のComposition previewについて、単体GPU `applyGPU` のresource生成・同期readbackがsteady-state frame pathへ到達する根拠は今回の呼出し経路では見つからなかった。前回Insightの「フレーム経路へ届くか未確定」は、この通常経路に関しては否定できる。
- 公開APIの外部利用、個別fixture/toolの実行、特殊な呼び出し元を全バイナリで網羅したものではない。従って単体GPU APIがアプリ全体で一切実行されないとは断定しない。
- Shader出力のCPU/GPU parityや実測性能は調べていない。build/test/profileは実施していない。

### Core反映判断・次の確認

本体変更は不要。今回確認した通常Composition経路はGPU residentであり、懸念していた同期readbackを踏まない。次のCore候補はコード変更ではなく、RetroImageLabの結果とComposition空間effectの色ずれサンプリング（座標丸め、境界処理、alpha/transfer domain）を同一fixtureとrender outputで比較できる環境が用意できた時に、見た目と画素差を個別に測ること。

## 実験 010: alpha境界の色ずれと隠れRGB感度

### 仮説

1. straight RGBを直接bilinear samplingすると、alpha=0画素の隠れRGBが半透明領域へ混ざり、出力alphaを維持しても結果が変わる。
2. 色チャンネルをpremultiplyしてsamplingし、移動先のcoverageでunpremultiplyすれば、透明画素の隠れRGB変更に対してshifted channelが不変になる。
3. 隠れRGB感度と入力に対する境界MAEは別々に測る必要があり、感度ゼロだけで最良の色ずれを意味しない。

### 手順

- 実験002と同じ256×128 synthetic RGBA fixtureを使い、赤チャンネルを+3.5px、青を-3.5px、bilinear/clampでsamplingする。alphaは元の画素位置を維持し、色ずれに伴って輪郭を移動させない。
- straight RGB sampling、premultiplied channel sampling後に各shift位置のalphaでunpremultiply、透明画素のhidden RGBを0にしたstraight samplingを比較。
- 白・黒・緑背景の合成結果を作成。元alphaが0.01〜0.99のedge ROIにおける3背景のcomposite MAEと、alpha=0領域をmagentaからgreenへ変更したときのshifted R/Bの平均絶対変化を測定。
- `retro_lab.py --alpha-chromatic-study`、比較画像 `captures/alpha-chromatic-study.png`。

### 観測結果

| 方式 | 3背景 edge composite change MAE | visible hidden-RGB感度 |
|---|---:|---:|
| straight sampling | 0.01405 | 0.00966 |
| premultiplied sampling / shifted-alpha unpremultiply | 0.01355 | 0.00000 |
| hidden RGBを0にしたstraight sampling | 0.01465 | 対照処理 |

visible hidden-RGB感度は、alpha=0画素をmagentaからgreenへ差し替えた二つの出力を3背景へ合成し、全pixel/RGB平均絶対差で測定。edge composite change MAEは処理前入力との差で、効果による変化量を表し品質の合否値ではない。このfixtureではpremultiplied経由がhidden-RGB変更に対して不変となり、変化量もstraightより僅かに小さかった。hidden RGBを先に0へ正規化したstraight samplingもhidden値には依存しないが、出力変化量は0.01465だった。

### 解釈・限界

- premultiplied経由の手順は「移動元から色を採る一方で出力alphaは固定する」契約を保ち、サンプリングされた透明coverageは色の正規化にだけ利用している。色とalphaを同時に動かす処理とは異なる。
- この比較はsRGB符号値でpremultiplicationとbilinear補間を行う8-bit fixtureで、linear-light処理、実写素材、HDR、GPU backend parityを測っていない。
- 1つの固定horizontal shiftであり、放射状ずれ場、端pixel、zero-coverage除算閾値、異なるalpha edge形状を網羅しない。
- MAEの僅差は画質優位の根拠として弱い。明確な結果は隠れRGB感度の差であり、見た目の判断は比較画像と複数素材で続ける必要がある。

### Core反映判断・次の確認

Core変更は行わない。隠れRGBに不変なsamplerが有効な候補だが、production effectのalpha契約、transfer domain、CPU/GPU一致を同じfixtureで調べるまでは共通APIへの移植根拠にしない。次はshift距離と素材種を広げる。

## 実験 011: textured alpha cutoutでの色ずれ距離スイープ

### 仮説

1. antialiasedな有機輪郭と内部の細かな色変化がある素材でも、transparent hidden RGBがstraight samplingのshift結果へ混入する。
2. shift量が増えるほど、hidden RGBの差が表示alphaのある領域へ届く幅が広がり、visible sensitivityが上がる。
3. premultiplied channel samplingとshift位置alphaによるunpremultiplyは、shift量に関わらずhidden RGBへ不変であり、出力alphaを維持する。

### 手順

- seed 21で384×256の植物葉群風synthetic cutoutを生成。RGBは複数の周期構造と独立noiseで内部変化をつけ、楕円の有機輪郭を周期perturbationし、約1.5px幅のsoft alpha edgeを作る。alpha=0の外側RGBはmagentaとした。入力は `fixtures/photo-alpha-fixture.png`。
- 赤・青チャンネルを反対方向へ0.5 / 1.5 / 3.5 / 7px shift。bilinear/clamp、sRGB code domainを使い、straight、premultiplied/unpremultiply、zero-hidden-RGB straightの3方式を比較。各方式は元alphaを維持する。
- 3背景で合成。edge composite change MAEは処理前との変化量をedge ROIで計測し、visible hidden-RGB sensitivityはhidden RGBをmagentaからgreenへ変更した前後の3背景合成RGB平均絶対差を全画面で計測。
- 実行: `python retro_lab.py fixtures/color-space-fixture.png --alpha-photo-sweep`。比較画像は `captures/alpha-photo-chromatic-sweep.png`。

### 観測結果

| shift | straight edge change MAE | premultiplied edge change MAE | zero-hidden straight edge change MAE | straight visible sensitivity | premultiplied visible sensitivity |
|---:|---:|---:|---:|---:|---:|
| 0.5px | 0.02128 | 0.00274 | 0.00861 | 0.00026 | 0.00000 |
| 1.5px | 0.06004 | 0.01672 | 0.02181 | 0.00115 | 0.00000 |
| 3.5px | 0.09770 | 0.03234 | 0.03324 | 0.00404 | 0.00000 |
| 7.0px | 0.11558 | 0.03898 | 0.03898 | 0.00941 | 0.00000 |

このfixtureではshift量を増やすとstraight samplingのvisible hidden-RGB感度が単調に上がった。premultiplied方式は全距離で測定上ゼロ。zero-hidden-RGB straightはhidden colorを除くため3.5px以降premultiplied方式と同じedge change値になった。

### 解釈・限界

- 生成画像は内部textureと有機形状を持つが実写写真ではない。結論はsynthetic素材内の距離依存性に限る。
- edge change MAEはeffectなし画像との差であり、色ずれ表現を望ましく評価する指標ではない。方式間の数値差はサンプリング結果の変化量としてのみ扱う。
- texture noise seedは固定され、方向は水平、shiftは赤青対称、transferはsRGB code value。実写の複雑なalpha matte、linear-light sampling、非対称RGB shiftは未調査。
- 元alphaを維持した比較であり、channel別alpha移動やcoverage付きRGBA出力の仕様を支持しない。

### Core反映判断・次の確認

Core変更は行わない。透明RGBに依存しないsamplerへの候補根拠は増えたが、実写、異なるmatte、linear/premultiplied descriptor、GPU shader parityを調べてから共通化を判断する。次のソース調査は実験012へ記録。

## 実験 012: Chromatic Aberration CPU/GPUのsampling contract照合

### 確認した事実

- `ChromaticAberrationCPUImpl::applyCPU` とComposition用 `kChromaticAberrationShader` は、center、最大寸法の0.7倍を使う距離正規化、赤を外向き・青を逆向きにずらす式を同じ形で持つ。
- CPUは色descriptorのchannelOrderを見てR/B offset先を選び、GPU shaderはBGRA upload/SRV値に`.bgra`を適用して対応する。CPU/GPUとも座標に`+0.5`を加えてinteger coordinate化し、画素範囲へclampする。CPUはfloat4 viewから該当channelを読み、GPUはinteger `Load` を使うため、両方nearest samplingでありbilinear interpolationではない。
- 両方とも出力を入力pixelから始め、RとBだけを移動元sampleで上書きする。Gとalphaは元の出力pixel由来で、alphaを色channelと共に移動しない。
- スタンドアロンGPU shaderも同様のinteger load/clampを使うが、通常Composition経路は別実装の空間ノードである（実験009）。

### 解釈・未確認事項

- 実験011のbilinear alpha-aware手順は、現行Chromatic Aberrationのsampling仕様の再現ではなく、隠れRGBに対する異なる設計案の挙動である。Coreへ反映する場合、既存effectの見た目をそのまま置き換えるとは限らない。
- nearest samplingでもalphaを動かさずRGBのみ移すため、straight-alpha画像の透明source pixelに非ゼロhidden RGBがあると、元pixel alphaが非ゼロの場所にその色が採られうる。surface conversionや前段の描画がtransparent RGBをどう正規化するか、実際のComposition textureで起きるかは今回未確認。
- source上の算術・descriptor分岐を照合しただけで、実行時CPU/GPU pixel parity、DX backend差、出力alpha mode、実素材のedge結果は未測定。build/test/profileは未実行。

### Core反映判断・次の確認

Core変更なし。まずalpha=0に異なるRGBを持たせたfloat32 RGBA配列fixture（値はsRGB code valueを0..1化）を作り、現行CPU nearest/clamp式とGPU shader式に対応するalgorithm-level出力差、およびalpha composite差を参照画像で可視化する。Production renderを使ったCPU/GPU parityはビルドと実行が認可された時点で別途確認する。

## 実験 013: 現行radial nearest/clamp式のhidden RGB漏れ範囲

### 手順

- 実験011と同じ384×256の有機輪郭fixtureをfloat32 RGBA配列へ変換。alpha=0のRGBをmagenta、同じ画像の比較コピーではgreen、正規化コピーではzeroにする。alphaやvisible foreground RGBは共通。
- CPU実装にある距離正規化、半径方向R/B逆向きoffset、`coordinate + 0.5`からの整数化、clampをfloat配列モデルへ写し、shift量2 / 8 / 16を比較する。alphaは元pixel位置を保持する。
- visible affected ratioはalpha>0 pixelのうち、magenta/green hidden-RGB版のshifted R/B差が1/255を超えるpixel数。edge affected ratioはalpha 0.01..0.99に限定。visible composite sensitivityは3背景合成後の全画面RGB MAE。
- 比較画像 `captures/production-chromatic-alpha-study.png`、入力 `fixtures/production-chromatic-alpha-fixture.png`。CLI `--production-chromatic-alpha-study`。

### 観測結果

| shift parameter | visible affected pixels | edge affected pixels | max normalized R/B delta | visible composite sensitivity | zero-hidden sanitization composite delta |
|---:|---:|---:|---:|---:|---:|
| 2px | 1.711% | 68.132% | 1.00000 | 0.00120 | 0.00120 |
| 8px | 5.759% | 94.505% | 1.00000 | 0.00710 | 0.00710 |
| 16px | 10.977% | 97.602% | 1.00000 | 0.01504 | 0.01504 |

float32 modelではshift parameterが増えるほど、hidden RGBが影響するvisible pixel領域と合成差が拡大した。affected ratioは全visible pixelを分母にする。edge affected ratioはedge pixel数を分母とする。zero-hiddenとgreen-hiddenの差がmagenta/green差と同じ値になったのは、このfixtureの透明部を各版で一定色にしたため。

### 解釈・限界

- これはソースにあるradial nearest/clamp式をPython上のfloat配列へ写したalgorithm-level model。C++関数、HLSL compiler、実GPU、Diligent resource upload、Compositionの実サーフェスは実行しておらず、CPU/GPU parity検証ではない。
- Fixtureのsource RGBは8-bit生成後にfloat32へ変換し、sRGB code valueを0..1で保持。linear-light値ではなく、transfer conversionやpremultiplied descriptor経路は評価しない。
- hidden RGB差は意図的に極端なmagenta/green。surface contractが透明RGBを0に正規化するなら、当該入力は実際のproduction frameへ届かない可能性がある。
- 数値は色ずれの品質評価ではなく、透明RGB差に対するalgorithm sensitivityである。nearest samplingでもR/Bだけを移しalphaを保持する契約では、hidden colorが可視alphaへ届く可能性を示す。

### Core反映判断・次の確認

Core変更なし。コード上のalpha処理懸念を測定可能なmodelへ落とせたが、実際の入力境界でtransparent RGBが保たれるかは未確認。次のboundary調査は実験014へ記録。GPU/runtime parityは明示的なbuild/render実行許可後に行う。

## 実験 014: transparent RGBの正規化境界をソース追跡

### 参照した既存資料

- `docs/INDEX_GENERATED.md` の画像format/channel order項目。
- `docs/technical/IMAGE_FORMAT_CONVENTIONS.md` と `docs/technical/IMAGE_CHANNEL_ORDER_CONTRACT_2026-10-16.md`。

### 確認した事実

- `ArtifactCore/include/Image/SurfacePixelConversion.ixx::convertSurfacePixelsToDescriptorInto` はsourceのalphaを読み、alphaが`1e-6`以下ならRGBを0へ置き換える。Premultiplied sourceなら先にunpremultiplyするが、alphaが閾値以下ならRGBを0にする。targetがPremultipliedならlinear decode/encode後にalphaを掛ける。
- この正規化はそのconversion APIの経路に限られる。`ArtifactCore/src/Image/ImageF32x4_RGBA.cpp::setFromCVMat` のCV_32FC4 no-descriptor overloadはcloneし、descriptor overloadはchannel orderをBGRAへ整えるが、この処理内でalpha=0 RGBはzero化しない。
- `ArtifactCore/src/Image/GpuImageUpload.cpp::makeGpuImageUploadBuffer` はBGRA descriptor/storage/strideを検証し、CPU bufferを`sourceData`として渡す。pixel copy、alpha normalization、hidden-RGB cleanupは行わない。
- `ImageF32x4RGBAWithCache` のGPU texture creation/updateはこのupload helperを利用する。
- `ArtifactImageLayer::loadImagePairViaAsyncReader` は通常の非CMYK decodeで、OpenImageIOからRGBA channel mapを作り、float pixel列へ読み、`normalizeNonFiniteRgba32F`後に`setFromRGBA32F`へ渡す。このnormalize関数はNaN/Infを有限値に置き換えるだけでalphaとRGBの関係は見ない。
- Prefetch採用時、`adoptPrefetchResult` は`result.floatImage`があれば`cacheBuffer_`としてそのまま選び、QImage経由の`toFrameBuffer()`を使わない。QImageへpremultiplied変換するのは別の`toFrameBuffer(QImage)`入口。
- `applyInputInterpretation()` はcolorSpaceとtransferFunctionがともに空なら即returnする。変換する場合、source metadataがassociated alphaと示すときだけunpremultiply/OCIO transform/premultiplyを実施する。
- `Artifact/src/Layer/ArtifactImageLayer.cpp::toFrameBuffer(QImage)` はQImageを`Format_ARGB32_Premultiplied`へ変換した後、premultiplied descriptor付きでCVMatをImageF32へ設定する。このQImage経路は一般CV_32FC4直設定と異なるが、今回、Qt conversion後のedge bytesを実行測定していない。
- `ArtifactImageLayer::draw` は`currentFrameBuffer()`を `renderer->textureForImage()` へ渡す。`PrimitiveRenderer2D::textureForImage(ImageF32)` は`convertImageForUpload(...Rgba8SrgbStraight)`を通す。別の`GPUTextureCacheManager::acquireOrCreate(ImageF32)`も`Rgba32LinearStraight`へ同じconversion APIを使う。
- 両upload targetの `convertSurfacePixelsInto` はdirect sRGB 8-bit fast pathとgeneral float pathを持つが、両方alpha<=1e-6の画素のRGBを0にする。従って、これらのCPU surface→GPU texture ingressでは、source float bufferにhidden RGBがあってもeffectに渡るtexture値ではcleanupされる契約となる。

### 解釈・限界

- Coreにはalpha=0のRGBを確実にzero化する明示conversion経路が存在する一方、全ImageF32 setter/GPU upload経路に普遍的な正規化保証があるわけではない。
- 通常async image decode後のCPU cacheBufferまではhidden RGBを保持できるが、確認した通常ImageLayer sprite uploadとfloat texture-cache uploadは、texture生成前にalpha<=1e-6のRGBを消す。従ってその2経路を通る画像のComposition Chromatic Aberrationでは、当該source hidden RGBがeffect samplingへ届く可能性は低い。
- direct `makeGpuImageUploadBuffer`を使う単体GPU effectなど他のupload経路はこのzero-alpha cleanupをしない。これは実験009で通常Composition経路から分離したstandalone applyGPU実装に当たる。
- これは静的ソース調査で、conversion関数やQt経路を実行していない。実rendererの到達性、アルファ合成後のGPU pixel、CPU/GPU parityは未測定。

### Core反映判断・次の確認

Core変更は不要。upload conversion自体がalpha<=1e-6のRGBを除去する契約を確認できた。次はこのcleanupをモデルに適用し、現行radial nearest/clamp式へ渡る前後のsensitivityを比較する（実験015）。実読込とGPU textureをruntimeで検証する作業は、build/render実行が許可された後に別途行う。

## 実験 015: upload前alpha-zero cleanup後のeffect sensitivity

### 手順

- 実験013の同じfloat32 sRGB-code fixtureとradial nearest/clamp modelを使用し、input branchをalpha<=1e-6のRGB=0 cleanupあり／なしに分ける。
- magenta hidden RGB版とgreen hidden RGB版の3背景composite sensitivityをshift parameter 2 / 8 / 16で比較。cleanupありは`SurfacePixelConversion`のalpha-zero branchだけをモデル化し、transfer/channel conversionやGPU texture executionは行わない。
- `--production-chromatic-alpha-study` がcleanup前後を両方出力する。captureは `captures/production-chromatic-alpha-study.png`。

### 観測結果と解釈

| shift parameter | cleanup前 visible composite sensitivity | alpha-zero cleanup後 sensitivity |
|---:|---:|---:|
| 2px | 0.00120 | 0.00000 |
| 8px | 0.00710 | 0.00000 |
| 16px | 0.01504 | 0.00000 |

modelではalpha-zero RGBだけをcleanupするとhidden color差が全て消えた。これはソース上のconversion規則と一致するアルゴリズムmodelで、`PrimitiveRenderer2D::textureForImage`が実際に生成したDiligent textureのpixel readbackを示すものではない。

### Core反映判断・未確認

Core変更は不要。通常のCPU image surface uploadには既存cleanupがあるため、新しい共通APIを追加する根拠は得られなかった。実際のlayer renderへ届くGPU SRVの値、既にGPU上にある独自source、CPU/GPU parityは未確認。専用のrender fixtureで実測するにはビルドと描画実行が必要なため、今回は実行していない。

## 実験 016: 固定paletteの距離domainとBayer強度

### 目的・比較計画

- **目的:** 固定Game Boy風/CGA風/PICO-8風paletteで、sRGB符号値距離とlinear RGB距離の割当て、およびBayer 4×4 strengthが色誤差とpalette使用へどう効くか調べる。
- **baseline:** 同一palette/domainのdither strength 0.0をbaselineとする。入力に対するlinear RGB RMSEは補助指標で、視覚品質の正解とは扱わない。
- **素材:** `quantization-fixture.png` (320×200のchart)、`color-space-fixture.png` (384×192のgradient/chart)、`radial-chromatic-fixture.png` (256×256のhue/value ring)を使用。すべて合成fixtureで実写ではない。3画像ともRGB opaqueでalpha経路を評価しない。
- **設定:** 3 palettes × 2 distance domains × dither strength 0.0/0.5/1.0。Bayer 4×4をRGB全channelの比較入力へ同じthresholdで加え、palette nearestで選ぶ。palette本体はsRGB表示値として固定し、linear domainでは入力とpalette色をlinearへdecodeして距離計算し、出力paletteは同一値を返す。
- **指標:** sRGB decode後のlinear RGB RMSE、baselineから変わったpixel比、出力で実際に使ったpalette color数。閾値・合否判定は設けず探索的に比較する。
- **環境:** Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。CLI `--palette-domain-study`。画像全体比較は `captures/palette-quantization-study.png` / `captures/palette-gradient-study.png` / `captures/palette-radial-study.png`。

### 観測結果: strength 0のlinear RMSE (sRGB distance → linear distance)

| fixture | Game Boy風 | CGA風 | PICO-8風 |
|---|---:|---:|---:|
| quantization chart | 0.30712 → 0.30684 | 0.30463 → 0.28465 | 0.09104 → 0.08759 |
| gradient chart | 0.28033 → 0.27566 | 0.42149 → 0.30636 | 0.12547 → 0.11035 |
| radial hue/value | 0.33219 → 0.31312 | 0.39931 → 0.35102 | 0.17432 → 0.14639 |

今回の9組ではlinear-distance nearestのRMSEが全て小さかった。ただしこれは測定metricがlinear RGB errorであることと同じ領域を測っている。製品のretro lookとして優れることは意味しない。palette選択domainが全入力で同じpixel割当てを作るわけでもなく、比較画像でcolor blockの変化を確認できる。

### Bayer strengthの応答

- strength 1.0では全9組で同domainのstrength 0よりlinear RMSEが増加した。増分はfixture/palette/domainにより約0.0032〜0.0848。
- monotonicとは限らない。gradient chartのCGA/sRGBではstrength 0.5のRMSE 0.40977がno-dither 0.42149より小さく、strength 1.0では0.43206へ増えた。従ってdither strengthを上げ続けても改善／悪化が単調になると仮定しない。
- dither strength 1.0でno-ditherから変わったpixelは約0.142〜0.495。PICO-8 fixtureで使った色はquantization chartがsRGB/linear distanceで11/13色、gradient chartが15/16色、radial chartが14/16色になり、ditherは実使用palette色数も増やした。
- Bayerはpalette間の選択を空間的に変える。色数の増加は設定色数の増加ではなく、固定palette内の使われる色が増えることを示す。

### 解釈・未検証項目

- sRGB-vs-linear距離の優劣は色誤差metric上の結果。palettesを物理的な色差単位で定義していないため、perceptual ΔE、hardware appearance、ユーザーの好みを示さない。
- 手元の素材は3つとも人工fixture。実写、alpha edge、HDR/wide gamut、palette自体の異なるtransfer contract、Bayer 8×8は対象外。
- 時間安定性は静止画像のみで未評価。動画像でphaseが固定された場合のshimmerも確認していない。
- 実ArtifactStudioのGPU/preview/export経路・performance・CPU/GPU parityは未評価。測定はPython/Numpy静止画像labのみ。
- 完了判定や製品品質ゲートではなく、parameter responseの探索結果である。Coreへ反映する成果なし。

### Core反映判断・次の確認

Core変更は行わない。palette modeではdistance domainを明示的なlook選択として扱う価値があるが、既存Core APIへ足す根拠や受入条件は不足。次は固定palette色の具体的なpalette occupancy/transition cropを見比べ、gradientのbandingとflat-region patternの観察量を補う。frame-timeはscope外のため動画の時間安定性を主張しない。

## 実験 017: 階調化のbanding代理指標とBayer tradeoff

### 目的・比較計画

- **目的:** 滑らかなgray/color rampで階調数、量子化domain、Bayer 4×4強度が帯幅・局所pattern・誤差へ与える変化を観察する。
- **baseline:** 各domainと階調数におけるdither 0.0をbaselineとする。合成元rampは線形に生成し、8-bit sRGB RGB PNGへ保存するため、fixture自体に8-bit丸めがある。
- **素材:** `fixtures/banding-fixture.png`、512×128、上半分がneutral gray ramp、下半分がR上昇/G非線形上昇/B下降のcolor ramp。外部素材なし。全caseは同じfixtureを使用。
- **設定:** 4 / 8 / 16階調 × sRGB-code / linear-light quantize × Bayer 4×4 strength 0.0 / 0.5 / 1.0。RGB channelへ同一thresholdを適用。出力はopaque RGB、float演算後の8-bit表示。
- **指標:** sRGB decode後linear RGB RMSE (gray/color)、grayの各rowにおける平均unique code数、最長同値runの平均pixel数、隣接横stepのうち下向きstepの割合。閾値は設けず探索値として報告する。RMSEは画質合格指標ではない。
- **実行条件:** 2026-11-10、Windows/PowerShell、Python 3.14.2、NumPy 2.4.3、Pillow 12.2.0。`python retro_lab.py fixtures/color-space-fixture.png --quantization-banding-study --output output/banding.png`。gallery: `captures/quantization-banding-gray-study.png`, `captures/quantization-banding-color-study.png`。

### 観測結果

| domain / levels | dither | gray RMSE | color RMSE | gray mean longest run (px) | gray reverse-step ratio |
|---|---:|---:|---:|---:|---:|
| sRGB / 4 | 0.0 | 0.115709 | 0.132357 | 170.0 | 0.000% |
| sRGB / 4 | 0.5 | 0.127582 | 0.146307 | 121.0 | 12.182% |
| sRGB / 4 | 1.0 | 0.160457 | 0.177537 | 69.0 | 24.609% |
| linear / 4 | 0.0 | 0.087350 | 0.089907 | 228.0 | 0.000% |
| linear / 4 | 0.5 | 0.097365 | 0.100237 | 195.5 | 9.687% |
| linear / 4 | 1.0 | 0.124454 | 0.128636 | 146.5 | 20.303% |
| sRGB / 8 | 0.0 | 0.049440 | 0.050921 | 74.0 | 0.000% |
| sRGB / 8 | 0.5 | 0.054804 | 0.056521 | 53.0 | 11.693% |
| sRGB / 8 | 1.0 | 0.069609 | 0.072968 | 33.0 | 24.462% |
| linear / 8 | 0.0 | 0.038924 | 0.039865 | 152.0 | 0.000% |
| linear / 8 | 0.5 | 0.043439 | 0.044456 | 130.5 | 10.372% |
| linear / 8 | 1.0 | 0.055267 | 0.056401 | 94.5 | 21.575% |
| sRGB / 16 | 0.0 | 0.022989 | 0.024446 | 34.0 | 0.000% |
| sRGB / 16 | 0.5 | 0.025804 | 0.027504 | 27.0 | 11.644% |
| sRGB / 16 | 1.0 | 0.031922 | 0.033796 | 17.0 | 22.211% |
| linear / 16 | 0.0 | 0.018595 | 0.018838 | 104.0 | 0.000% |
| linear / 16 | 0.5 | 0.020714 | 0.021065 | 87.5 | 9.883% |
| linear / 16 | 1.0 | 0.026270 | 0.026740 | 62.5 | 21.673% |

Bayer strength増加は全6組でgray/color RMSEを増やし、gray最長runを短くした一方、grayに下向きの隣接stepを約9.7–24.6%生じさせた。見た目では長い一様帯を模様へ分散するが、模様と反転stepも新しいtextureとして現れる。linear quantizationはこのfixtureで全階調数・全strengthにおいてRMSEが小さかったが、4/8/16 levelsのlongest runはsRGB量子化より長く、band幅の代理値は誤差と一致しない。unique gray code数は設定階調数と一致した。

### 共通評価項目と限界

- **効果の応答:** 適用。階調数は4/8/16、dither strengthは0/0.5/1を固定し、RMSE・run・reverse-stepを比較。parameter sweep内の中立値はstrength 0。出力は入力に対して無処理identityではない。
- **画質・細部:** 適用。2枚の全体galleryを同寸法で保存し目視。定量的なbanding合否閾値、独立判定者、実写・細線cropは未設定／未実施。run長とreverse-stepはbandingの代理指標のみ。
- **色とalpha:** 色transfer domainは比較対象。opaque RGBのみ。alpha、premultiplied、HDR/wide gamut、display transformは対象外。
- **時間安定性:** 静止画像labのため未評価。frame間dither shimmerを主張しない。
- **経路と解像度:** 512×128 synthetic fixtureのみ。ArtifactStudio preview/export、CPU/GPU/backend、他解像度は未評価。
- **性能:** 実装候補のruntime・GPU memory・production performanceは未評価。Python実験自体の時間を測定していない。
- **完了・採用判定:** 探索実験でありquality gateでもCore採用判断でもない。採用には実画像と拡大cropの評価条件、許容するpattern、alpha/transfer契約、アプリ経路と性能の受入条件が必要。

### Core反映判断・次の確認

Core変更なし。次は自然画像と緩やかな影を含む画像で、run指標だけでは拾えないtexture/contour損失を評価する。必要ならperceptual banding指標を導入するが、先に対象・判定手順・閾値根拠を定める。

## 実験 018: landscape fixtureでのposterize・Bayerと細部

### 目的・比較計画

- **目的:** 実写に近い要素（低周波の空・光の勾配、重なる丘、細い草、家の輪郭と窓）を持つ固定sceneで、階調化とBayerが全体toneと局所detailへ与える影響を比較する。素材は権利・再現性を固定しやすい合成sceneで、実写とは区別する。
- **baseline:** `fixtures/natural-quantization-fixture.png` の8-bit RGB source、無処理。候補は同一sourceを使う。
- **素材・固定条件:** 384×256、seed 23の丘陵scene。空色と広いsun glow、重なるhill、520本のseed固定line、家・窓を含む。生成されたopaque sRGB PNG。display transformなし。全条件で同じ寸法・画像を使う。
- **設定:** posterize 8 / 16 levels × sRGB-code / linear-light × Bayer 4×4 strength 0 / 0.5 / 1。比較ケース12にsource baseline 1件を加える。
- **指標:** linear RGB RMSE (scene全体)、linear luminanceの2D forward-difference gradient magnitudeに対するMAE、sourceとの差があるpixel率。いずれもscene-wide summaryでありperceptual scoreではない。閾値・合否は定めず探索データとして扱う。
- **評価領域と手順:** gallery全体とhouse/grass crop `(240,118)-(336,214)` を保存。cropは2倍nearest enlargement。画像全体とcropを同じ出力画像条件で目視し、dither texture、hill/roof contour、window/grass legibilityを確認する。判定者: この研究を実施したCodex、独立評価者なし。
- **実行条件:** 2026-11-10、Windows/PowerShell、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。`python retro_lab.py fixtures/color-space-fixture.png --natural-quantization-study --output output/natural.png`。capture: `captures/natural-quantization-study.png`, `captures/natural-quantization-detail-crop.png`。

### 測定結果

| domain / levels | dither | linear RGB RMSE | luma gradient MAE |
|---|---:|---:|---:|
| sRGB / 8 | 0.0 | 0.038432 | 0.004070 |
| sRGB / 8 | 0.5 | 0.043515 | 0.043800 |
| sRGB / 8 | 1.0 | 0.052013 | 0.078338 |
| linear / 8 | 0.0 | 0.043801 | 0.005923 |
| linear / 8 | 0.5 | 0.049183 | 0.060272 |
| linear / 8 | 1.0 | 0.060004 | 0.111353 |
| sRGB / 16 | 0.0 | 0.015582 | 0.003131 |
| sRGB / 16 | 0.5 | 0.017555 | 0.019790 |
| sRGB / 16 | 1.0 | 0.022077 | 0.033554 |
| linear / 16 | 0.0 | 0.019591 | 0.003634 |
| linear / 16 | 0.5 | 0.022333 | 0.026257 |
| linear / 16 | 1.0 | 0.027476 | 0.047705 |

All candidate cases changed essentially every source pixel (8 levels: 100%; 16 levels: 99.91–100%), so changed-pixel ratio offers little discrimination here. At both level counts and every strength, sRGB quantization had lower scene-wide linear RMSE than linear quantization; this differs from the simple ramp and palette assignment results in experiments 017/016 and shows fixture dependence. Dither increased both RMSE and luma-gradient MAE monotonically across these tested strengths. Visual inspection: strength 0 preserves broad clean contours but creates visible broad sky bands at 8 levels; strength 0.5 breaks up bands while adding visible 4×4 texture; strength 1 overlays stronger texture on house/grass and reduces fine-line legibility. The 16-level versions retain more source detail. The detail sheet contains 13 cases in a 3-column layout, so the last tile occupies the first cell of its final row.

### 共通評価項目・限界・採用

- **効果の応答:** 適用。2 level counts × 2 domains × 3 strengths; neutral comparison is dither 0.0, and source baseline is unprocessed.
- **画質・細部:** 適用。whole-scene gallery and 2× house/grass crop visually inspected. Acceptance threshold, defect severity scale, independent reviewer, and real photographs were not established. Gradient MAE is only a structural proxy.
- **色・alpha:** sRGB and linear operation domains were compared. Opaque RGB only; alpha association, HDR/wide gamut, and multiple composite backgrounds are out of scope.
- **時間安定性:** static still fixture; temporal flicker/shimmer unverified and not claimed.
- **経路・解像度:** single 384×256 Python implementation/output. ArtifactStudio preview/export, GPU/backend parity, and alternate resolution are unverified.
- **性能:** production time/memory budgets and Python sweep runtime are unmeasured.
- **合否・採用判断:** exploratory, no pass/fail threshold or product acceptance. Do not select a domain based only on this one scene or RMSE. No Core change; no effect behavior is proposed for integration yet.
- **次:** obtain representative real imagery under permitted usage, define crops and visual acceptance before final comparisons, and assess a higher-quality/blue-noise pattern or error diffusion against Bayer if a smoother dither look is desired. Runtime, alpha, and performance remain separate implementation gates.

## 実験 019: Bayer matrix sizeとFloyd–Steinberg誤差拡散

### 目的・計画

- **目的:** Bayer 4×4/8×8の周期patternと誤差拡散の非周期patternを同素材で比較し、空のband concealment、輪郭/細部のtexture、局所誤差を確認する。
- **baseline:** 実験018と同じ無処理source image、および各domainの8-level no-dither output。
- **素材と固定条件:** `fixtures/natural-quantization-fixture.png`、384×256、seed 23、opaque sRGB RGB。実験018と同じ丘・空・grass strokes・house scene。
- **設定:** quantization 8 levels固定。sRGB / linear domainごとにno-dither、Bayer 4×4 strength 0.5/1、Bayer 8×8 strength 0.5/1、Floyd–Steinberg RGB error diffusionを比較。Bayerは行/列に依存しない通常のtile repeat。Floyd–Steinbergは左→右の単方向raster scanで、誤差を右7/16、下左3/16、下5/16、下右1/16へ拡散するreference implementation。
- **評価region:** whole frame `(0,0)-(384,256)`、sky `(0,0)-(384,125)`、hill boundary `(0,132)-(384,214)`、house/grass detail `(240,118)-(336,214)`。各regionのlinear RGB RMSEとlinear-luma forward-gradient magnitude MAEを算出。比較画像は全体と3× detail crop。
- **閾値・判定:** exploratory comparison。受け入れ閾値なし。Codexがgalleryとcropを目視し、Bayer periodicity、contour texture、Floyd scan-direction streaksを確認。独立評価者なし。
- **実行条件:** 2026-11-10、Windows/PowerShell、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。`python retro_lab.py fixtures/color-space-fixture.png --dither-pattern-study --output output/dither-pattern.png`。captures: `captures/dither-pattern-study.png`, `captures/dither-pattern-detail-crop.png`。

### sRGB-domainの代表値

| method | whole RMSE | sky RMSE | hill RMSE | detail RMSE | whole gradient MAE | detail gradient MAE |
|---|---:|---:|---:|---:|---:|---:|
| no dither | 0.038432 | 0.037413 | 0.041410 | 0.044800 | 0.004070 | 0.002434 |
| Bayer 4×4, 0.5 | 0.043515 | 0.041649 | 0.047186 | 0.051577 | 0.043800 | 0.052959 |
| Bayer 8×8, 0.5 | 0.043688 | 0.041975 | 0.047400 | 0.051760 | 0.043453 | 0.052688 |
| Floyd–Steinberg | 0.051942 | 0.053697 | 0.051431 | 0.056573 | 0.051599 | 0.050180 |

At Bayer strength 0.5 the two matrix sizes were close in region RMSE (whole difference 0.000173; detail 0.000183), but their visible pattern scale differs. Bayer 1.0 raises errors for both matrix sizes. In this fixture Floyd–Steinberg had larger RMSE than Bayer 0.5 in all four reported regions, while detail gradient MAE was slightly smaller than Bayer 4×4 0.5 (0.050180 vs 0.052959). These are not perceptual rankings. Linear-domain no-dither whole RMSE was 0.043801 and Floyd–Steinberg was 0.059797; Bayer 4×4/8×8 strength0.5 were 0.049183/0.049518. Linear-domain Floyd’s region RMSE was larger than its sRGB counterpart for whole/hill/detail, but sky was 0.056256 vs 0.053697. Full output metrics are printed by the documented command.

### 目視・限界・採用判断

- Bayer 4×4 shows a finer, readily visible regular texture over the house and sky. Bayer 8×8 shifts that texture to a coarser period but does not remove its grid character.
- Floyd–Steinberg removes a fixed 4×4/8×8 tile, but the crop shows directional/serpentine-looking local structure from its left-to-right error propagation; small house/window edges acquire grain. The whole image also shows row-wise texture in smooth regions.
- **効果の応答:** 8 levels and two domains; no-dither provides the per-domain neutral comparison. Matrix size and pattern are explicit.
- **画質・細部:** whole and 3× detail gallery visually inspected; no quality threshold, real image, or independent reviewer. Gradient MAE and RMSE are auxiliary proxies only.
- **色・alpha:** opaque RGB sRGB input and code/linear arithmetic domains; alpha association, HDR/wide gamut, and compositing are untested.
- **時間安定性:** still image only. Error diffusion can be frame-content/scan-order sensitive, but temporal shimmer was not evaluated.
- **経路・解像度:** Python reference at one resolution only; no ArtifactStudio preview/export, GPU parity, tiling, crop-origin behavior, or backend comparison.
- **性能:** sweep completed in about 2.4s on the current Python environment, but this is a single measurement including image generation/output, not a controlled benchmark. Floyd–Steinberg implementation uses serial per-pixel Python loops; do not infer production feasibility.
- **採用:** no Core change and no default-pattern recommendation. To consider integration, first define acceptable look, multi-image visual criteria, crop/tile phase contract, alpha behavior, temporal policy, and target-path performance budget. Floyd–Steinberg parallelization would change implementation behavior and needs separate design/evaluation.

## 実験 020: 1px/frame移動時のdither phase stability

### 目的・比較計画

- **目的:** dither patternをframe座標へ固定するか、画像contentの移動へ追従させるかで、被写体を位置合わせした後に残る模様変化を比較する。screen-lockedとcontent-lockedのどちらが正解かはlook contractに依存するため、合否を決めず差を測る。
- **baseline:** 同一sceneの8-level no-dither。既知の1px translationを正確に補償すれば0差になる対照。
- **素材:** 実験018/019と同じ`fixtures/natural-quantization-fixture.png`。384×256、seed 23、opaque sRGB RGB。全sceneを水平+1px/frameで8 frames移動。borderはclampし、MAEでは左右各8px相当の端を除く。垂直運動、fractional motion、変形、scene editは対象外。
- **比較:** 8-level sRGB quantize。no-dither、Bayer 4×4 screen-locked/content-locked、Bayer 8×8 screen-locked/content-locked (strength 0.5)、Floyd–Steinbergを各frameで再実行。content-locked Bayerは既知translation分だけmatrix originを逆向きに補償。Floyd–Steinbergは左→右のraster scanを各frameでx=0から再開。
- **指標:** 隣り合う出力frameを+1px位置合わせしたlinear RGB MAEの8 frame sequence平均/最大、およびframe 0へ位置合わせしたdriftの平均/最大。0がperfectly translation-covariant result。MAEはtexture stabilityの差分量で、美的優劣や知覚閾値ではない。
- **目視:** 8 frame全scene stripとhouse/grass crop stripを同じscale/表示条件で作り、dither patternがscreenまたはcontentのどちらへ固定されるか確認。判定者はCodexのみ、閾値なし。
- **実行条件:** 2026-11-10、Windows/PowerShell、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。`python retro_lab.py fixtures/color-space-fixture.png --temporal-dither-study --output output/temporal.png`。captures: `captures/temporal-dither-study.png`, `captures/temporal-dither-detail-strip.png`。

### 観測結果

| method | mean aligned frame-to-frame MAE | max frame-to-frame MAE | mean drift from frame 0 | max drift |
|---|---:|---:|---:|---:|
| no dither | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| Bayer 4×4 screen-locked | 0.029047 | 0.029061 | 0.018800 | 0.029071 |
| Bayer 4×4 content-locked | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| Bayer 8×8 screen-locked | 0.028509 | 0.028530 | 0.018404 | 0.028507 |
| Bayer 8×8 content-locked | 0.000000 | 0.000000 | 0.000000 | 0.000000 |
| Floyd–Steinberg recomputed per frame | 0.034528 | 0.034889 | 0.033420 | 0.034604 |

No-dither and content-locked Bayer are exactly covariant to this integer translation after alignment, confirming the coordinate compensation and metric control. Screen-locked Bayer leaves about 0.0285–0.0291 mean per-frame residual and 0.0184–0.0188 mean drift; the object crosses a fixed threshold pattern. Floyd–Steinberg has the largest residual in this case (0.034528 mean per-frame and 0.033420 mean drift), because the moving content is re-quantized against a frame-left scan origin and accumulated error changes. The contact sheets visually show stable texture attached to the object for content-locked variants and shifting texture over the object for screen-locked variants.

### 共通評価項目・限界・判断

- **効果の応答:** applicable; pattern phase and 1px translation response were directly compared, with no-dither neutral control.
- **画質・細部:** whole frame and detail strips visually inspected. No perceptual acceptability threshold or external evaluator; numeric residual is not a flicker quality score.
- **色・alpha:** sRGB arithmetic, opaque RGB only. Alpha, linear/HDR path, and compositing were not studied.
- **時間安定性:** applicable only to 8 integer horizontal translations of a synthetic still. Fractional movement, vertical phase, motion vectors, moving subregions, scene cuts, animation and subjective viewing were not tested.
- **経路・解像度:** standalone Python at 384×256. No preview/export, GPU/backend, tiled dispatch or production temporal behavior was measured.
- **性能:** not a controlled performance benchmark; Floyd–Steinberg remains serial and reruns per frame. No production budget evaluation.
- **判断:** neither screen-lock nor content-lock is universally correct. A product effect must specify coordinate space (frame/output vs source/layer), phase origin under crop/tile, and behavior under transforms. No Core change; do not infer existing Core behavior from this reference model.

## 実験 021: 製品DitheringEffectとRetroImageLab設定のsource crosswalk

### 目的・方法

- **目的:** 実験016–020の設定・結論をArtifactStudioの既存DitheringEffectへ誤って直結しないよう、製品パラメータ、CPU実装、GPU resident shader、別のGPU adapter、frame適用経路をソースで照合する。
- **方法:** read-only source trace。参照箇所は `Artifact/include/Effects/Dithering/DitheringEffect.ixx`、`Artifact/src/Effects/Dithering/DitheringEffect.cpp`、`Artifact/src/Effects/ArtifactAbstractEffect.cpp`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`、`Artifact/include/Effects/EffectHostContract.ixx`。ビルド、テスト、GPU実行、画像captureは行わない。
- **合否:** source-contract auditのため画像品質合否なし。確認事実、条件付きリスク、未検証runtime経路を分ける。

### 確認した契約と差

| 項目 | 製品DitheringEffectのsource上の動作 | RetroImageLabとの関係 |
|---|---|---|
| 主な値 | Algorithm=Bayer4×4、Color Count=16、Amount=1、Pattern Scale=1 | Labはper-channel 8/16 levelsやdither strength 0/0.5/1を使い、同名値ではない |
| channel quantize | `levelsPerChannel=max(2,sqrt(colorCount))`、floor/round式で各RGBを独立quantize。Color Count=16はおよそ4区間/channelで、16 levels/channelではない | palette color countとper-channel step countを混同しない |
| Bayer Amount | thresholdを `(1-Amount)` で中央値0.5へ縮める。Amount=1はthreshold patternが消えたno-dither量子化、Amount=0で最大pattern振幅 | Lab strengthを製品Amountに移すなら単純同値でなく逆方向変換が必要。ただし量子化式も異なる |
| error-diffusion Amount | diffusion residualへAmountを掛ける。Amount=0でerror propagationなし、Amount=1で最大 propagation | 同一EffectのAmountがordered branchとdiffusion branchで逆向きの応答になる |
| Bayer position | CPU ordered branchはx/yを各frame surface内の局所座標で使う。GPU resident shaderは同じwidth/height正規化式を使い、global origin入力は見当たらない | Labの固定4px Bayerやexplicit frame/content originとは周期・phase contractが異なる。product patternはsurface coordinate基準に見えるが、transformed layerとの望ましい関係は設計判断 |
| GPU fast path | `appendGpuSpatialNodes`はBayer2×2/4×4のみを登録し、8×8/16×16とdiffusionはfalseを返す | これらは通常のComposition resident raster plan対象外。source上はCPU referenceへ退避する設計 |
| CPU error buffer | FS/Atkinson/Sierra/Stucki branchは呼出しごとに`std::vector<float>(width*height*3)`を確保し、順次raster scanする | per-frame使用時の大容量確保候補。1920×1080で約23.7 MiB、3840×2160で約94.9 MiB。実頻度/計測値は未確認。`HOT_PATH_RULES.md`観点で性能・workspace再利用を調査候補として残す |

### GPU source divergence risk (runtime unverified)

- Resident generic HLSLはBayer2×2と4×4のrank table、CPUと同形のcoordinate formula・threshold remap・quantize式を持つ。
- 同じeffect file内のstandalone `DitheringEffectGPUImpl::kDitheringHlsl` はalgorithm 0/1だけを分岐処理し、2/3およびdiffusion IDsにquantize branchがない。従って、このadapterへBayer8×8/16×16やdiffusionを直接渡すと、shader source上はinput RGBをそのまま出すように読める。またBayer2×2では4×4 tableの要素indexを流用しており、CPUのcanonical rank tableと異なるthresholdになる可能性が高い。これはsource式からの推論でGPU runtimeでは未検証。
- 通常CompositionではBayer2×2/4×4だけがresident nodeに受理され、その他はplan対象外。CPU surface builderはCPU implementationを持つeffectをCPU-only適用する。別の `applyConfigured` adapter interfaceやDAG nodeも存在するため、他の呼出し経路がこのGPU adapterへunsupported IDを渡さないとは、今回のsource traceだけでは証明していない。
- GPU adapterには入力texture生成、readback、`Flush()`、`WaitForIdle()`もある。Composition resident fast pathとの利用差・実到達性をruntime traceで確認していないため、performance issueと断定しない。

### 結論・次の判断

このsource crosswalkだけでは製品出力のvisual parityを証明しない。実験017–020のlook比較は研究素材として有効だが、現在のDitheringEffectに対する性能・品質・GPU parity acceptanceには直接転用できない。CoreのDithering APIは今回の検索範囲に見つからず、実在のEffectはArtifact moduleにある。コード変更は行わない。次に実装候補へ進むなら、まず実アプリで選択可能なAlgorithm/ComputeModeごとの呼出し経路、CPU↔resident GPUのpixel parity、per-frame error buffer allocationを許可された実行環境で確認し、amountの共通意味とphase originの要件を定める。build/testはAGENTS.mdにより明示許可がないため未実施。

## 実験 022: 製品Bayer実装式のsource-derived比較

### 目的・方法

- **目的:** 実験021で特定したCPU Bayer、Composition resident HLSL、standalone adapter HLSLの差が、同一画素でどの程度の出力差になるか再現可能な形で可視化する。
- **方法:** `retro_lab.py --product-dither-source-audit` がソース式をPython/NumPy上でモデル化。seed固定の384×256合成丘陵fixture、Color Count=16（4 level/channel）、Amount=0、Pattern Scale=1を各方式で共有。Bayer2×2/4×4ではCPU・resident・standaloneを、8×8/16×16ではCPUとstandalone IDs 2/3を比較。差分画素率はRGBいずれかの差が1e-6を超えた画素の比率、誤差は正規化RGB値で算出。
- **実行条件:** 2026-11-10、Windows/PowerShell、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。実行: `python experiments/RetroImageLab/retro_lab.py experiments/RetroImageLab/fixtures/color-space-fixture.png --product-dither-source-audit --output experiments/RetroImageLab/output/product-dither-audit.png`。比較capture: `captures/product-dither-source-audit.png`。

### 観測結果

| 比較 | 差分画素率 | mean absolute RGB code difference | 最大差 |
|---|---:|---:|---:|
| Bayer2×2 resident vs CPU | 0.0000 | 0.0000 | 0.0000 |
| Bayer2×2 standalone vs CPU | 0.8522 | 0.3770 | 1.0000 |
| Bayer4×4 resident vs CPU | 0.0000 | 0.0000 | 0.0000 |
| Bayer4×4 standalone vs CPU | 0.7756 | 0.1621 | 0.5000 |
| Bayer8×8 standalone unsupported ID vs CPU | 1.0000 | 0.1343 | 0.3706 |
| Bayer16×16 standalone unsupported ID vs CPU | 1.0000 | 0.1342 | 0.3735 |

ソース式モデルではresidentの2方式はCPUと完全一致した。standalone 2×2/4×4の位相・threshold indexingは出力差が大きく、unsupported 8×8/16×16 IDsはモデル上量子化を行わず入力RGBを返した。captureでも2×2 standaloneの過剰な閾値、4×4のphase差、unsupported IDsの無処理が見て取れる。

### 限界・判断

- この結果はソース式を転記した数値モデルの実行結果であり、C++ CPU関数・HLSLのコンパイル実行・GPU readback・runtime reachabilityを検証していない。resident/CPU一致はruntime parityの証明ではない。
- Opaque synthetic stillの単一解像度と単一設定のみ。alpha、他Amount/PatternScale、float境界差、backend差、dispatch境界、performance、実写品質は未評価。
- standalone adapterがアプリ上でどのAlgorithm/ComputeModeから到達するかは未確認。従って現時点ではsource上の潜在差であり、ユーザーに見える不具合とは断定しない。
- **判断:** 統合や修正を先行させない。次の製品検証では実アプリの適用経路を確認したうえで、許可されたruntime画像比較でCPU/resident/adapterを同一入力・同一設定で照合する。Artifact/Coreコード変更なし。

## 実験 023: provenance付きretro suite corpusとalpha保持baseline

### 目的・評価計画

- **目的:** 合成scene一枚ごとの個別スクリプトから進み、同じ固定プリセットを複数の実素材へ適用し、入力識別情報、alpha挙動、全体比較を一括で残す。
- **対象:** RetroImageLab内の4つの研究用プリセット（handheld、arcade、print、vhs）。現時点では静止画・sRGB display RGB値の研究用近似で、製品rendererの再現ではない。
- **比較対象:** 同一の入力RGBAを未処理baselineとし、RGB差分・RMSEは効果量を記述する補助値。見た目の正解や品質合否を示すものとしては扱わない。
- **素材計画:** 現在は独立生成した合成RGBA fixture一枚のみ。実写・写真、細線、髪/葉、複雑な輪郭、HDR素材は未収録。次の段階で出所/利用条件を確認した素材を複数追加する。実写の合否評価には使わない。
- **条件:** fixture 384×256 RGBA、固定seed 23、PillowのRGBA decode、プリセット別に固定された効果式、dark gray背景で比較contact sheet。元画像SHA-256、元mode、寸法、alpha有無、非opaque比をmanifestへ記録。
- **判定者・合否:** 研究者による全体/crop目視と対象固有criteriaを後続素材追加時に設定。今回はbaseline動作確認であり、画質合格判定なし。重大欠陥は透明pixelのalpha保持破壊と大きな輪郭/細部破綻を候補として、まだ正式閾値を決めていない。
- **実行条件:** Windows/PowerShell、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。CPU NumPy/Pillowのみ。GPU/backend、preview/export、性能予算は対象外の代用ではなく未検証。

### 実装・再現

`retro_lab.py --suite-corpus <source-dir> --corpus-dir <tracked-input-copy> --output <path>` で、source directory下のPNG/JPEG/TIFF/WebPをSHA-256由来の名前でcorpusへcopyし、`manifest.json`、4 presetごとのbaseline/result横並びsheetと`metrics.json`を出力する。`--make-suite-fixture` は人工RGB/alpha controlを作る。任意の`--corpus-provenance` JSONでファイル名ごとのsource originとlicense/permissionを記録でき、欠落時は`unknown`と表示する。外部素材のdownloadはしない。生成物/corpusは`.gitignore`対象。

再現コマンド:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --suite-corpus eval-inputs --make-suite-fixture --corpus-dir eval-corpus --corpus-provenance eval-inputs/provenance.json --output output/suite-corpus.png
```

### 観測

fixture manifest: `eval-corpus/manifest.json`、384×256 RGBA、non-opaque alpha ratio 0.113322、SHA-256 `4ac182abe84ba2bc1b35d6986fef29331017eede77cb1b9e4a7002b37254f70a`。provenance fileを作らず実行したためsource/licenseは`unknown`となる。alpha差分率は4 preset全て0。RGB RMSE（normalized RGB）はhandheld 0.119798、arcade 0.068210、print 0.150271、vhs 0.026328。変更画素率は99.86%以上だが、これはfixture全域へ効果を適用するプリセットの動作確認であり品質指標や望ましさを意味しない。全体sheetは`output/suite-corpus/suite-*.png`にあり、各素材のbaseline/resultを同一表示サイズで横並びにした。比較生成を確認した。

### 限界・次の判断

- 素材出所は自作の決定的fixtureで、実写のテクスチャ/実際のcamera noiseはない。エッジ・透明部は人工図形の部分alphaであり、hair/fur/foliage cutoutの代表性はない。
- 金属色、肌、暗部gradient、near-white highlight、alpha hidden RGB、複数background合成、寸法変化/拡大縮小、低解像度入力、時間変化を含むsuiteは未完了。
- Handheld/arcade/print/vhsはlab内固定レシピであり、PICO-8等のハードウェア/製品仕様の正確な再現とは主張しない。性能・GPU経路未検証。
- **判断:** このrunnerは素材追加後の比較可能性とalpha非破壊を確認するR&D基盤として採用。画質優劣や製品統合の根拠にはしない。実写評価、正式な見た目基準、性能/製品経路の確認を継続し、Coreへの反映は独立して有益と検証された小さな知見が出てから個別に判断する。

## 実験 024: 現行RetroImageLab presetの透明境界 hidden RGB 感度

### 目的・評価計画

- **目的:** 実験023で使ったrunnerが実際の`apply_retro` recipeと同一かを確認し、縮小処理のあるプリセットで完全透明部のhidden RGBが半透明edgeに漏れるかを測る。
- **baseline/candidate:** 可視RGBAを同一に保ち、alpha=0のRGBだけをmagentaとgreenへ変えたペアがhidden-color control。各pairに現行handheld/arcade/print/vhs presetを適用。
- **入力/条件:** 192×128 RGBAの合成hard/soft edge fixture。PillowでRGBA作成、現行preset値、seed 23、sRGB code values、元alphaを保持。結果を黒/白/中間灰色に合成。edge regionは`0.02 < alpha < 0.98`。SHA-256は`fixtures/alpha-hidden-suite-fixture.png`を再生成して取得できる。
- **評価:** hidden-color pairの出力RGB・合成後の全体/edge MAE、alpha完全一致、黒/白/灰背景の全体contact sheet。数値はchannel平均のnormalized RGB絶対差。判定者は研究実施者、合格閾値は探索前に製品品質基準がないため未設定、品質合格とはしない。
- **実行条件:** 2026-11-10、Windows/PowerShell、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。CPU NumPy/Pillow。GPU・本体runtime・性能は未計測。

### 先行訂正

実験023の最初のrunner版は同名4 presetの独自仮recipeを使っており、実際の`PRESETS` + `apply_retro`とは一致していなかった。実験023で記録した4 recipeのRMSE値はこのためpreset成果の比較根拠にはしない。今回`retro_suite`を修正して`PRESETS`を直接渡し、runnerと既存コマンドの実処理を一致させた。実験023のalpha保持の確認も人工control上の範囲に限る。

### 観測結果

再現コマンド:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --alpha-hidden-suite-study --output output/alpha-hidden.png
```

| preset | scale | hidden RGB edge MAE | black composite edge MAE | alpha exact |
|---|---:|---:|---:|---:|
| handheld | 0.50 | 0.051826 | 0.006044 | yes |
| arcade | 0.75 | 0.060064 | 0.008171 | yes |
| print | 1.00 | 0.000000 | 0.000000 | yes |
| vhs | 1.00 | 0.092381 | 0.016092 | yes |

現行handheld/arcadeでは`apply_retro`の縮小段階がRGBをBOX filterし、その後original alphaを再付与する。hidden RGB pairだけを変えても可視alpha自体は不変だが、edgeの出力RGBとcomposite pixelに差が出た。printはscale=1でpixel単位処理だけなのでこの条件のhidden-RGB差なし。VHSはhidden領域のnoise RGBが異なるが、alphaとの合成deltaは半透明edgeで発生する。黒/白/灰背景のdelta MAEは同一で、これは同じpremultiplied source deltaを不透明背景へover合成したとき背景色が差分式から相殺されるため。capture`output/alpha-hidden-suite-study.png`では黒/白背景の色縁を目視比較できる。

### 判断・限界・次

- **確認された研究結果:** scale<1のRGBだけをBOX縮小して元alphaを保持する現在のPython presetでは、完全透明画素のRGBが半透明edgeの最終compositeへ影響する。少なくともこのfixtureではhidden-RGB不変性契約を満たさない。
- **未確認:** 実写/実素材での重大度、source resize時のhidden RGB慣行、異なるalpha filter/縮小率、premultiplied処理候補の画質、製品/Coreの同様の問題、下流のfilter/compositor挙動。
- **判断:** 独立ラボ内では縮小前にalpha-aware premultiplied resamplingする候補を次に比較する価値がある。ただしretro pixel artではstraight RGB縮小が意図的なedge lookとなる場合もあり、無条件置換しない。製品コードやCore変更は行わない。実写RGBA cutoutを追加し、base/候補を複数背景で目視してから採否を決める。

## 実験 025: alpha-premultiplied source downscaleは同一silhouetteを保証しない

### 目的・条件

- **目的:** 実験024で発見したhidden RGB影響の候補対策として、scale presetのstraight RGB縮小を、RGBA premultiply → BOX縮小 → nearest拡大 → unpremultiplyへ差し替える方法を比較する。候補を自動採用せず、副作用も観測する。
- **入力:** 192×128の単色RGBA soft edgeと透明穴。source alphaは維持し、各scale presetへ同じfixtureを適用。Handheld scale=0.5、Arcade scale=0.75。測定edgeは`0.02 < alpha < 0.98`。
- **比較:** (a) effect無効相当のresize-only RGBと入力edge RGBのMAE、(b) effect有効時のstraight/premultiplied出力差を黒/白合成後のedge MAEで比較、(c) alpha exact preservation。
- **実行:** 2026-11-10、Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0、CPU only。capture `output/alpha-resize-suite-study.png`。再現は`python retro_lab.py fixtures/color-space-fixture.png --alpha-resize-suite-study --output output/alpha-resize.png`。

### 観測

| preset | straight vs premultiplied RGB edge MAE | black/white composite edge MAE | alpha保持 | resize-only edge RGB MAE straight / premul |
|---|---:|---:|---|---:|
| handheld | 0.055890 | 0.009555 / 0.009555 | exact | 0.000000 / 0.065384 |
| arcade | 0.059999 | 0.012910 / 0.012910 | exact | 0.000000 / 0.050208 |

単色sourceに対しstraight RGBのBOX縮小はedge色をそのまま保つ一方、候補premultiplied経路は低解像度へalphaもdownscaleし、alpha-zero cell近傍ではunpremultiply後もRGB=0となり得る。そのRGBをnearestで元サイズへ戻しながらoriginal alphaを保持するため、元alphaが正のedgeに暗いRGBが置かれ、resize-only edge errorが増えた。つまりhidden RGB感度の低減を狙うalpha-aware downscaleであっても、pixelated RGBと元silhouetteを組み合わせる現在のlookには単純置換できない。候補間differenceのblack/white composite MAEが同じなのは、両方とも同じalphaでover合成する差分式の性質。

### 判断・次

- **採否:** 現行presetへpremultiplied経路を一律適用しない。実験024のhidden RGB漏れと、本実験の透明穴/細い縁の欠落を同時に扱う必要がある。
- **未検証:** 写真cutout上の見た目、別再構成filter、alphaをpixelateする別look、透明部のRGB fill/dilation、subpixel resampling。GPU、runtime、性能も未評価。
- **次:** realistic alpha assetが加わるまでrecipe変更は保留。候補軸として「original alphaを保持しつつRGBはalpha-aware lowpass + edge-aware reconstruction」「pixelated alphaも出力」「hidden RGB dilation」を別々に比較し、lookが変わることを明示してから選ぶ。

## 実験 026: hidden RGB dilationは影響を除くがedge RGB lookを変える

### 目的・条件

- **目的:** straight RGB縮小のhidden-color漏れに対し、完全透明pixelのRGBだけを半径8px以内の近傍alpha>0 pixel RGBで伝播充填してから通常縮小する候補を、straight baselineとpremultiplied候補へ追加比較する。
- **入力/条件:** 実験024/025と同じ192×128単色RGBA soft edge+透明穴、可視部同一・alpha-zero RGBがmagenta/greenのpair。実際のhandheld scale=0.5 / arcade scale=0.75、seed23、alphaは元画像を保持。black/white compositing。edge=`0.02<alpha<0.98`。dilationは8回の8-neighbor平均伝播で実装。
- **評価/合否:** hidden-RGB pair間の合成edge MAE、straight baseline比のoutput RGB edge MAE、alpha完全一致。合格閾値なし。性能/GPU/backend/実写品質は未検証。
- **再現:** `python retro_lab.py fixtures/color-space-fixture.png --alpha-rgb-dilation-study --output output/alpha-dilation.png`。capture=`output/alpha-rgb-dilation-study.png`。Windows/Python 3.14.2、NumPy 2.4.3、Pillow 12.2.0。

### 観測

| preset | mode | hidden RGB black/white composite edge MAE | RGB edge delta vs straight | alpha |
|---|---|---:|---:|---|
| handheld | straight | 0.006044 / 0.006044 | 0.000000 | exact |
| handheld | dilation 8px | 0.000000 / 0.000000 | 0.032491 | exact |
| handheld | premultiplied | 0.000000 / 0.000000 | 0.080397 | exact |
| arcade | straight | 0.008171 / 0.008171 | 0.000000 | exact |
| arcade | dilation 8px | 0.000000 / 0.000000 | 0.042912 | exact |
| arcade | premultiplied | 0.000000 / 0.000000 | 0.079896 | exact |

Dilationはこのpairに対してhidden-RGB感度を測定精度内で除いたが、edge RGBはstraight baselineから変化し、その変化量はpremultiplied候補より小さかった。contact sheetではmagenta/green fringeの消失とpixelated edge RGBの違いを確認できる。これはアルゴリズム特性の証拠で、望ましい品質という意味ではない。black/white間のdeltaが同じなのは同一alpha合成の差分式による。

### 判断・限界

- **暫定採否:** 半径8px dilationを現行presetへ採用しない。hidden-RGB不変性の候補として残すが、edge RGB変化はvisible lookの変更である。
- 一つの合成geometry・均一foreground色のみ。カール状の実写髪/葉、細い穴、色の異なる前景近傍、source edge fringing、他dilation半径は未評価。8回平均伝播は距離の厳密な最近傍色を計算していない。
- 次は複数のforeground色/背景色の合成fixtureで複合edgeを作り、距離変換nearest-color dilation、alpha-aware area resampling、現在の処理を同じcropで比較する。実写素材が未追加の間はCore/product変更に反映しない。

## 実験 027: 最近傍visible RGB dilationと平均伝播を複色edgeで比較

### 目的・条件

- **目的:** 実験026の8回平均伝播と、半径8px内の最小ユークリッド距離にあるalpha>0 pixelのRGBを透明pixelへコピーする最近傍dilationを区別し、複色勾配edgeで比較する。
- **入力:** 192×128 RGBA、赤/緑が横方向に変わり青が縦方向に変わるvisible gradient、soft diagonal alpha edge、透明穴2領域。透明部のみhidden RGBをmagenta/greenの2通りに変え、可視RGB/alphaは同一。Handheld scale=0.5、Arcade=0.75、seed23、元alphaを維持。黒/白背景を合成。edge mask=`0.02<alpha<0.98`。
- **方法:** straight (現行)、nearest visible RGB radius8、mean propagation 8 iterations、premultiplied downsampleを実preset `apply_retro`で比較。hidden-pair composite edge MAEとstraight出力との差分edge MAEを測る。合否閾値なし、研究者が全体sheetを確認。
- **実行:** Windows / Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0、CPU only。再現: `python retro_lab.py fixtures/color-space-fixture.png --alpha-rgb-dilation-study --output output/alpha-dilation.png`。Capture=`output/alpha-rgb-dilation-study.png`。

### 観測

| preset | method | hidden RGB composite edge MAE (black/white) | RGB edge delta vs straight | alpha |
|---|---|---:|---:|---|
| handheld | straight | 0.006066 / 0.006066 | 0.000000 | exact |
| handheld | nearest radius8 | 0 / 0 | 0.035806 | exact |
| handheld | mean 8 rounds | 0 / 0 | 0.035806 | exact |
| handheld | premultiplied | 0 / 0 | 0.081862 | exact |
| arcade | straight | 0.008076 / 0.008076 | 0.000000 | exact |
| arcade | nearest radius8 | 0 / 0 | 0.042564 | exact |
| arcade | mean 8 rounds | 0 / 0 | 0.042491 | exact |
| arcade | premultiplied | 0 / 0 | 0.079410 | exact |

この勾配fixtureでも両dilationは入力hidden RGBによる差をなくし、edge RGB lookをstraightから変更した。nearestとmeanはArcadeでわずかに異なるが、本ケースでは優劣の根拠にならない。両者は後段の縮小とquantizationで出力が近くなり、fixtureが局所的な複色や細かいalpha切抜きではないため差を分離しきれていない。

### 判断・限界・次

- 今回のnearest法は厳密な最近傍色コピーをアルゴリズム上実装したが、探索範囲8pxを越える透明穴は元hidden RGBが残る可能性がある。伝播実装より1回のbounded neighborhood scanを使い、距離tieは走査順で決める。
- Dilationはstraight baselineとの差があり、alphaを保持した同じlookとは言えない。製品/Coreへの反映なし。
- 複色gradientだけでは、近接する複数objectの境界やhair-like細線の色を代表しない。次は明確に異色の複数shapeと1–8pxの透明gapを持つstress chartで最近傍色混合/色漏れと距離制限の影響を分離する。実写評価は引き続き未実施。

## 実験 028: 対照的な隣接objectと細線に対するnearest dilation

### 目的・条件

- **目的:** 実験027より厳しい色所有権stressとして、赤・青の隣接object、金色/赤/青の細線、透明gap 1/2/4/8pxを配置し、straight RGB resizeと半径8px最近傍visible RGB dilationのhidden-color感度を比較する。
- **入力:** 192×128 RGBA合成chart。高解像度2×でrectangles/strandsのalphaを描いてLANCZOS縮小し、赤と青のobject間に指定幅の透明gapを設定。透明画素のhidden RGBだけmagenta/greenの2色へ変更したpair。色・形・alphaはpairで同一。Handheld 0.5x、Arcade 0.75x、seed23、元alpha維持。Black compositeをcontact sheetに表示し、測定はedge(alpha 0<alpha<255)とgap領域で行う。
- **比較/合否:** hidden-RGB pairのcomposite RGB MAEとalpha完全一致。数値閾値なし。研究者が同じ表示拡大率の全体sheetを確認。GPU/runtime/performance未検証。
- **再現:** `python retro_lab.py fixtures/color-space-fixture.png --alpha-dilation-stress-study --output output/alpha-stress.png`。入力は`fixtures/alpha-dilation-stress/alpha-gap-{1,2,4,8}px.png`、captureは`output/alpha-dilation-stress-study.png`。Windows/Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。

### 観測

| gap | preset | straight edge hidden-RGB MAE | straight gap MAE | nearest edge/gap MAE | alpha |
|---:|---|---:|---:|---:|---|
| 1 | handheld | 0.014275 | 0.000223 | 0 / 0 | exact |
| 1 | arcade | 0.010584 | 0.000011 | 0 / 0 | exact |
| 2 | handheld | 0.018497 | 0.003867 | 0 / 0 | exact |
| 2 | arcade | 0.015073 | 0.011096 | 0 / 0 | exact |
| 4 | handheld | 0.018027 | 0.002938 | 0 / 0 | exact |
| 4 | arcade | 0.017115 | 0.018087 | 0 / 0 | exact |
| 8 | handheld | 0.017595 | 0.001890 | 0 / 0 | exact |
| 8 | arcade | 0.016786 | 0.011547 | 0 / 0 | exact |

指定範囲内nearest dilationでは全てのgap幅で、可視edgeの出力compositeがhidden RGB選択に依存しなかった。straight RGB resizeでは1〜8px全ケースにedge差が残り、gap差は特にArcadeの2/4/8pxで増えた。alphaは全て厳密保持。目視sheet上で色付きobjectとstrandsのsilhouetteは保たれるが、8px nearest探索を越える透明領域では未知のhidden RGB残存があり得る。nearest候補の透明gap内RGB色は不可視alpha=0であり、後続resize filterが追加されれば再露出する可能性がある。

### 判断・限界・次

- bounded nearest dilationは1〜8px gap stressにおけるhidden RGB非依存策として、平均伝播と同程度に有効だった。ただし同じalpha-edge RGB lookを保存する証明ではなく、実写品質/製品要件の採用判定でもない。
- Chartの物体は矩形・人工細線で、曲線状hair/fur、斜めに交差する色境界、半透明色付きfringe、ノイズ、RGB HDR、極端な穴径は未評価。背景はmetricでは黒だけ（前研究の背景差分式が同値のため）で、白/市松目視はこの実験では未実施。
- 次は実写真/許可されたcutoutに同じhidden-color変換を適用し、hairline cropと穴・輪郭を検査する。素材の出所/利用条件を記録できるまで画像品質結論とCore反映は保留。

## 実験 029: 実写グリーンスクリーンでRetroImageLab presetを比較

### 素材と権利情報

- **素材:** 既存の`tests/Artifact/fixtures/keying/voa-green-screen-frame-2s.jpg`を読み取り専用で使用。540×960 JPEG、RGB、alphaなし。
- **由来:** 隣接README `tests/Artifact/fixtures/keying/README.md`によると、VOA Learning English “Filming with the green screen” (2023-07-07)を2秒地点でframe抽出し540×960へresize。READMEに記載されたSHA-256: `132819FCFD733C007EDBD6CD1E9C276C3DD0B8B1B56230989E67EF01A107D1A3`（実ファイルhashも一致）。
- **利用条件の留保:** 同READMEはVOA制作物をpublic domainとする根拠を記す一方、Wikimedia Commons source pageに“license needs review”とあり、VOAのthird-party licensed content除外も記録している。本実験は既存test fixtureの内部評価用途に限定。画像をRetroImageLabへ複製・再配布していない。出典メタデータを`captures/voa-suite-manifest.json`に記録。

### 評価条件

- **目的:** 合成素材では不足していたcamera texture、実物のhair strands、skin/highlights、実スタジオ背景、字幕を含むframeで、同じ実装の4 presetのlookと変化量を観察する。
- **baseline/candidate:** 元JPEGと`PRESETS` + `apply_retro`のhandheld/arcade/print/vhs。変更pixel比・normalized RGB RMSEは変化量指標で、品質合否値ではない。
- **ROI:** source pixel box `(x=300, y=195, width=200, height=250)`。presenter face/hair/shoulderとgreen-screen boundaryを含む。cropはnearest 2×拡大、同一表示倍率。
- **設定/環境:** 4 presetは既定値、seed 23、Pillow RGB decode、sRGB code values、CPU NumPy/Pillow、2026-11-10 / Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0。preview/export、GPU、製品runtime、処理時間は未計測。Still imageのためtemporal stabilityはこの実験では未評価。
- **判定:** 研究実施者が全体とROI contact sheetを確認。素材は一例のみで正式な受入閾値なし、画質合格判定なし。

### 実行と観測

全体corpus再現:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --suite-corpus tests/Artifact/fixtures/keying --corpus-dir eval-corpus/voa --corpus-provenance eval-inputs/provenance.json --output output/voa-suite.png
```

詳細crop再現:

```powershell
python retro_lab.py fixtures/color-space-fixture.png --suite-detail-image tests/Artifact/fixtures/keying/voa-green-screen-frame-2s.jpg --suite-detail-box 300 195 200 250 --output output/voa-detail.png
```

| preset | whole-frame RGB RMSE | ROI RGB RMSE | whole-frame changed pixels | ROI changed pixels |
|---|---:|---:|---:|---:|
| handheld | 0.104585 | 0.077428 | 0.993688 | 0.999300 |
| arcade | 0.129975 | 0.099538 | 0.994641 | 0.999460 |
| print | 0.043905 | 0.044148 | 0.985062 | 0.999700 |
| vhs | 0.136469 | 0.091659 | 0.994790 | 0.999540 |

ROI capture: `captures/voa-suite-detail-crop.png`; whole frame sheets: `captures/voa-suite-{handheld,arcade,print,vhs}.png`; metrics/identity: `captures/voa-suite-metrics.json`, `captures/voa-suite-manifest.json`。目視上、handheldは粗いlevel/scanline変化が強く髪の細線を単純化し、arcadeはより細かいordered grid、printは4つで最小の全体RMSEだがpalette shiftが見られ、vhsはnoise/chromatic displacementで髪輪郭を色分離する。これはlook差の記述であり、printを最良と判定する根拠ではない。ROI RMSEがhandheldで全体より低くても、数値だけで細部保持の良否を断定できない。

### 評価ルール項目・限界・次

- **Effect response:** 既定presetごとの出力を確認。パラメータsweep/中立値/単調応答は未評価。
- **Quality/detail:** 全体とhair/face/green edge ROIを確認。背景全域、衣服texture、字幕の専用cropは未評価。正式なperceptual thresholdなし。
- **Color/alpha:** sourceはJPEG RGB、alphaなし。alpha handling対象外（opaque入力のため）。sRGB仮定でHDR/transfer metadataは未評価。
- **Temporal:** 静止画のため未評価。sequenceを使用していない。
- **Path/resolution:** Python still CPU runnerのみ、540×960。ArtifactStudio preview/final、Core/GPU/backend parity未評価。
- **Performance:** 未計測。
- **判断:** 実写1例で4 presetの見た目差を確認できたが、利用条件の留保と単一frameの範囲を越える品質一般化はしない。Coreへ転記できるalgorithmic resultなし。次は利用条件を確認できる複数のreal still / transparent cutoutを確保し、許諾情報とhashをmanifestへ記録してsmooth gradient、hair/foliage edge、dark textureのcropを増やす。

## 実験 030: 実写動画上でgrainのseed固定/フレーム変化を比較

### 素材・権利・抽出

- **source:** VOA Learning English, “Filming with the green screen” (2023-07-07)。[Wikimedia Commons file page](https://commons.wikimedia.org/wiki/File:Filming_with_the_green_screen.webm)は14.268秒、1080×1920、VOA提供、public-domain表示を記載する一方、license review neededも表示する。権利留保を実験029と同じく維持し、外部へ配布しない。
- **識別:** source WebM SHA-1 `7f491bfd0e770207c894a37e6897445dac898f3f`（Commons structured metadataと一致）、SHA-256 `90a6e00f4436293842b3a7036043b347c3641e26687aee1515673a9ab0a0e872`、5,404,709 bytes。元動画はOS tempに一時保持、抽出frameは`eval-inputs/`下のignored local data。source動画自体・抽出frameはtracked labへ追加しない。
- **抽出:** FFmpeg 9.0-full_build.www.gyan.dev、2.0秒開始から1.1秒、fps=10、scale=540×960、passthrough、11 frame (0.1秒間隔)、audioなし。コマンド: `ffmpeg -ss 2.0 -i source.webm -t 1.1 -vf fps=10,scale=540:960 -fps_mode passthrough -start_number 0 frame-%02d.png`。各frame SHA-256は`captures/video-grain-temporal-manifest.json`。
- **実行:** `python retro_lab.py fixtures/color-space-fixture.png --video-grain-frames eval-inputs/voa-sequence --output output/voa-temporal.png`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0、CPU only、seed base=23。

### 評価計画と方法

- **問い:** 現行4 presetのgrainが、同じ画素座標へseed固定noiseを足す場合とframeごとにseedを変える場合で時間変化をどう変えるか。
- **分離方法:** 各frameで、grainなし同一preset出力をbaselineとして、grainあり出力との差をnoise residualと定義。scene motion/compressionを評価から差し引き、adjacent frame residualのmean absolute normalized RGBを比較する。プリセット固有grain量は変更しない。fixed seed=23、frame-varying seed=24..34。
- **ROI:** source pixels x=295..509,y=190..444はhair/face付近のscreen-space window。静止green maskは全frameでG>1.25RかつG>1.12B、追加clipping-safe範囲を満たす共通pixel集合（全画像の0.8459%）。同ROIはmotion trackingしていない。全体も測る。
- **合否:** perceptual threshold未設定。residual差分はalgorithmic temporal varianceであり、全画像のflicker評価や品質合格とはしない。研究者がfirst/lastの全preset contact sheetを確認。
- **他の評価項目:** RGB opaque sequenceのためalpha対象外。GPU/backend/runtime、性能予算は未計測。Temporalを適用項目として調べた。

### 観測

| preset | fixed residual adjacent MAE (green / hair) | varying residual adjacent MAE (green / hair) | residual sigma fixed / varying |
|---|---:|---:|---:|
| handheld | 0.000201 / 0.000563 | 0.020893 / 0.020013 | 0.021081 / 0.021112 |
| arcade | 0.000392 / 0.000589 | 0.012717 / 0.012099 | 0.012809 / 0.012826 |
| print | 0.000393 / 0.001060 | 0.029342 / 0.026508 | 0.028791 / 0.028826 |
| vhs | 0.000651 / 0.000911 | 0.033783 / 0.031993 | 0.033568 / 0.033613 |

Source adjacent frame RGB MAEは0.053940。static green common mask上でもgrain-free baselineの隣接差は約0.0134–0.0227あり、元映像の変化/圧縮noiseは無視できない。一方、noise residualを分離するとseed固定時の差は全presetで概ね0.0002–0.0011、seed変更時は0.0121–0.0338。両seed方式のnoise sigmaは各preset内でほぼ一致するため、差はamountではなくgrain fieldの時間変化に由来する。Contact sheetではfirst/last ROI、fixed/varying出力を同倍率で確認した。

### 判断・限界・次

- 現行presetと同じgrain強度を用いてseedだけをframeごとに変えると、grain成分は明確に時間変化する。現行呼び出し側が毎frame同じseedを維持するかは別のruntime経路問題であり、このlabモデルからは推測しない。
- sourceは1秒の縦動画、同一人物の短い発話/動作。frame extraction/delivery/cadenceは再現可能だが、元の抽出frame fixtureと同じtool/resize経路ではない。Commonsのlicense-review caveatにより derived crops/contact sheetもgit管理から除外した。
- 固定画面grainが望ましいか、grainをobjectへ追従させるべきかはlook要件次第。seed固定は画面固定noiseであり、content-locked grainではない。
- 次は粒状感の許容criteriaを決め、より長い/複数motion sequenceでsource motionとgrain residualを別々に測る。Core移植やseed policy変更の根拠にはまだ使わない。

## 2026-11-11: Motion-aligned grain needs a raw residual metric

### 再現条件

- **関連:** `experiments/RetroImageLab/retro_lab.py::video_grain_space_study`、`captures/video-grain-space-study.png`、`captures/video-grain-space-manifest.json`。
- **素材:** VOA green-screen縦動画から2.0秒開始、10fpsで11 frame、540×960に抽出した同一セット。元ページはVOA素材にPublic Domain表記を含む一方、Commonsにlicense-review caveatがあるため、素材とderived cropは配布せずローカルgit ignore対象とした。
- **実行:** `python retro_lab.py fixtures/color-space-fixture.png --video-grain-space-frames eval-inputs/voa-sequence --output output/video-grain-space-study.png`。OpenCV Farneback前後flow、forward-backward誤差<1.5px、境界外を除外。髪ROIはsource pixel x=295..509,y=190..444、共通green maskは各frameの色閾値とclipping-safe条件の積集合。seed=7。
- **評価対象:** 同seedのraw grayscale Gaussian grain (sigma 0.025)をscreen座標固定する場合と、前frameのgrain fieldをbackward flowでbilinear warpしsigma正規化して素材へ追従させる場合。sourceに加えてからclipし、動き補正した過去残差と現frame残差のMAEを測る。preset側はfirst/last visual context用。

### 評価項目・観測

- **Effect/quality:** grain coordinate policy。flow平均量は髪ROI 4.91px、stable green 1.07px/frame。flow forward-backward有効率は髪0.872、green0.972。
- **Temporal:** raw-field motion-aligned MAEは髪ROIでscreen-fixed 0.02318、content-locked 0.000917、有効画素平均18,423。共通greenで0.02193対0.001067、有効画素平均4,046。screen-spaceでは逆にcontent-lockedの隣接差が大きい（髪0.01039、green0.01130）一方screen-fixedはほぼ0。この結果は空間policyの定義と整合し、grain量をほぼ同等sigma 0.0238/0.0234に揃えた。
- **Preset residual:** presetごとのgrain=0との差分は8-bit出力上でほぼ全ROIが0（有効画素0）だった。presetは量子化後にgrainを足すため、8-bit再量子化で差分が消えることが原因。従ってpreset別motion MAEはinvalidとして解釈しない。数値policy比較はraw diagnostic fieldに分離した。
- **Color/alpha:** RGB opaqueのためalpha評価対象外。source clippingがあるfieldはclip後実残差で集計。
- **Path/performance:** OpenCV + Python + CPU lab modelのみ。実アプリ、GPU経路、実時間性能は未測定。
- **判定:** 定量結果は解析式とflow warpの実装確認に限る。光学flow推定の正確さ、遮蔽、hair motion、映像品質やどちらのlookが望ましいかの人間評価は未確立。11 frame/1秒、単一被写体のため一般化しない。

### 結論と次

- flowで追従させたraw grainは素材座標で安定し、screen-fixed grainは画面座標で安定するという、設計上期待する振る舞いをこの短い系列で数値確認できた。
- 既存presetへの結論やCore移植判断には使わない。次は長い複数motion素材で、flow誤差/遮蔽を目視確認し、raw grain size/spectrumの条件も揃えた上で知覚比較する。

## 2026-11-11: Crop-local resizing changes retro pixel phase and sampling support

### 再現条件

- **関連:** `experiments/RetroImageLab/retro_lab.py::crop_resize_phase_study`、`captures/crop-resize-phase-study.png`、先行実験008。
- **評価目的:** Bayer 4×4 quantizationと2行周期scanlineを含むpixelationで、global resize→crop処理とcrop別resize→phase補正処理が同じ出力になるか調べる。対象はscreen/frame-locked phaseとtile-local low-resolution resamplingの境界挙動。Core出力再現ではない。
- **合否:** 各候補が全体処理と同じ処理を表す条件では、sRGB code valueの完全一致（MAE=0、changed pixel=0）を契約確認として要求する。この閾値は処理実装の一致に限定し、知覚画質の合否ではない。
- **素材/条件:** seed=91の515×257連続勾配+sinusoidal detail+Gaussian texture人工fixture。Pillow BOX downsample/NEAREST upscale、scale 0.5と0.75、Bayer4×4、12 levels、dither strength 0.7、scanline gain 0.65。Odd 173×85 crop (0.5)とeven 172×86 crop (0.75)、それぞれ12/9 tiles。比較referenceは全体縮小後にeffectを全体適用。
- **実行:** `python retro_lab.py fixtures/color-space-fixture.png --crop-resize-phase-study --output output/crop-resize-phase.png`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / CPU only。素材画像は関数内で決定的に合成。比較画像は `captures/crop-resize-phase-study.png`。

### 評価項目・観測

| scale / crop | candidate | seam MAE | tile interior MAE | whole MAE | changed pixels | max code error |
|---|---|---:|---:|---:|---:|
| 0.50 / 173×85 | tile-local resize + local phase | 0.080814 | 0.060029 | 0.061307 | 60.06% | 0.377273 |
| 0.50 / 173×85 | global resize + local phase | 0.079847 | 0.058812 | 0.060105 | 56.07% | 0.377273 |
| 0.50 / 173×85 | global resize + output-coordinate phase | 0 | 0 | 0 | 0% | 0 |
| 0.50 / 173×85 | tile-local resize + output-coordinate phase | 0.005878 | 0.005625 | 0.005640 | 20.82% | 0.090909 |
| 0.75 / 172×86 | tile-local resize + local phase | 0.005854 | 0.004897 | 0.004941 | 18.01% | 0.090909 |
| 0.75 / 172×86 | global resize + local phase | 0.002781 | 0.002025 | 0.002060 | 7.45% | 0.090909 |
| 0.75 / 172×86 | global resize + output-coordinate phase | 0 | 0 | 0 | 0% | 0 |
| 0.75 / 172×86 | tile-local resize + output-coordinate phase | 0.004596 | 0.003691 | 0.003733 | 13.95% | 0.090909 |

MAE列はseam帯4px / seamを除いたtile interior / wholeの順。変更画素はRGB最大差が1/255を超えたpixel率。

- **効果の正しさ:** helper `ordered_quantize_with_origin` は当初x originしか受けずy Bayer phaseを暗黙に0へresetしていた。両軸originを追加後、先行実験008を再実行して既知結果（source-relativeがglobalと0差）を保持することを確認した。
- **画質と細部:** sheetはfull-scale contact sheetで確認。tile-local resizeはseamだけでなくtile interiorにも差が残るため、filter supportと縮小・NEAREST再構成gridが全域へ影響する。品質の主観評価・閲覧者判定なし、合格ではなく差分観察。
- **色/alpha:** RGB人工fixture、sRGB code-value処理。alpha/HDR/linear-light対象外。Core float color descriptorの保証なし。
- **時間安定性:** 未検証。静止画像のcrop分割のみ。
- **経路/解像度:** Python/Pillow CPUモデルのみ。Core OpenCV resize、GPU shader、dispatch tile、preview/final rendererで未検証。
- **性能:** 未計測。cropごとの追加処理/overlapのruntime costを推定しない。

### 結論・Core判断・次

- crop前に全体を同じ条件でresizeし、その出力を分割する場合、Bayerとscanlineの両方へoutput-frame originを渡すことで全体処理と完全一致した。
- tileごとにresizeする場合、screen phaseを渡しても全体resizeとは一致しない。crop境界のfilter supportとreduced extentの丸めが影響し、今回の人工fixtureではscale 0.5/0.75とも画素差が残った。
- Coreには既存`ImageF32x4_RGBA::crop`があるが、これはROI cloneでありphase-aware effect APIではない。`resize`はOpenCV `cv::resize`を直呼びする。現段階でCore変更はしない。GPU effectの実dispatchがtile-local resizeをする証拠やRetroSuiteのproduction coordinate contractはまだ確認していないため、現在の候補結果を製品欠陥として扱わない。
- 次はadditional crop offset/alignment、scale factor、BOX/BILINEAR variantsを合成素材で振り、品質判断に必要ならpermit済み実写素材を使う。その後、実render pathでtile-local resizeとeffect phaseの組合せが存在するか追跡してからCore採用条件を決める。

## 2026-11-11: OpenCV float resize filter and tile-local crop comparison

### 再現条件と評価計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::opencv_crop_resize_study`、`captures/opencv-crop-resize-study.png`、Core `ImageF32x4_RGBA::resize`。
- **対象:** Coreのresize implementationを意識したCPU algorithm modelにて`INTER_AREA`とOpenCV既定`INTER_LINEAR`のdownscale、`INTER_NEAREST` upscale、crop-local resize、screen-coordinate Bayer/scanline phaseを比較する。Core runtime behaviorやretro product featureの評価ではない。
- **素材:** 515×257 synthetic RGBA float32 field（smooth gradients、sinusoidal detail、sinusoidal texture、alpha=1）。sRGB transfer decode/encodeはせずnormalized code valuesをCV_32Fのまま処理。scales 0.5/0.75、crop 173×85 odd / 172×86 even、Bayer4×4 16 levels、amount0.65、scanline gain0.72。
- **baseline / 合否:** 同じOpenCV interpolationでwhole-frame resize→effectをbaselineとし、global resize後にtilesへ分けるケースの正確一致に限りRGB MAE=0をcontract acceptanceとする。tile-local resize比較は探索値で合否閾値なし。resize-only MAEも測りfilter差と後段処理を分ける。
- **実行:** `python retro_lab.py fixtures/color-space-fixture.png --opencv-crop-resize-study --output output/opencv-crop.png`。Python/OpenCV/CPUのみ。画像は `captures/opencv-crop-resize-study.png`。

### 観測

| scale / method | resize-only tile vs global RGB MAE | tile-local resize + output phase whole RGB MAE | AREA vs LINEAR whole output MAE |
|---|---:|---:|---:|
| 0.50 | AREA 0.009945 / LINEAR 0.010342 | AREA 0.008565 / LINEAR 0.008939 | 0.000904 |
| 0.75 | AREA 0.005002 / LINEAR 0.004820 | AREA 0.004349 / LINEAR 0.004164 | 0.000669 |

- phase resetを含むtile-local処理はwhole RGB MAEがscale0.5で約0.0514、scale0.75で約0.0050。output-coordinate phaseへ補正すると低下するが、resizeだけでも差が残るため0にはならない。
- global resize後にtile分割しoutput-coordinate phaseを使う候補は両scaleでwhole RGB MAE=0。
- AREA対LINEARの平均差は小さいが最大RGB errorは両scaleで0.066667。quantization等の後段処理後の見た目にも差があるため平均MAEのみでは同等性を判定しない。
- Alphaは1.0固定のためalpha保持や透明境界は未検証。Temporal、知覚品質、HDR/transfer space、GPU/backend/runtime、処理時間/memoryは未検証。

### Coreとの関係と判断

- **コード上の事実:** `ArtifactCore/src/Image/ImageF32x4_RGBA.cpp::resize` calls `cv::resize(impl_->mat_, resized, cv::Size(width, height))` with default interpolation (`INTER_LINEAR`). `Artifact/src/Effects/AutoMosaicEffect.cpp` downsamples ROI with explicit `INTER_LINEAR`, then upscales with `INTER_NEAREST`.
- **判断:** `INTER_AREA`候補は同一synthetic imageで別のpixel lookを生むが、品質優位性や意図されたCore behaviorを示す根拠ではない。Core API/実装を変更しない。AutoMosaicはchild repo実装であり変更していない。
- **次:** これはPython/OpenCV算法モデルの比較。Core/API/runtime一致やproduction経路の証明ではない。実製品のresize semanticsが課題になる場合は、透明edge素材とvalid preview/final captureで実経路を比べ、採用前に性能も測る。

## 2026-11-11: Alpha pixel-art edges expose crop-local resize and hidden-RGB sensitivity

### 再現条件・評価計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_pixel_art_crop_study`、`fixtures/alpha-pixel-art-crop-fixture.png`、`captures/alpha-pixel-art-crop-study.png`。
- **入力識別:** deterministic generated 515×257 RGBA PNG, SHA-256 `5f5b7f7252380f7502962fa1f789802ce187ae6634e883428aa4674ead72000c`, 5,601 bytes。外部素材なし。hard-edge blocks、1px details、partially transparent diagonal/circle edges、alpha=0 pixels with alternating magenta/green hidden RGB。Partial-alpha fraction 0.9203%, fully transparent fraction 73.1306%, edge ROI 4.2756%.
- **目的/対象:** normalized code value上のpixel resampleでcrop-local supportとorigin、後段Bayer/scanline、alphaを元のcoverageへ戻したときのvisible edge差を分ける。Dithered imageの見た目をcomparison baselineから同じ式で作る。Crop origin補正はpolicy candidateであって品質優位 claimではない。
- **方法:** OpenCV 4.13.0 float32 RGB only, no transfer-function conversion; `INTER_AREA` or `INTER_LINEAR` downsample followed by `INTER_NEAREST` upscale; factor 0.5 / 0.75; fixed crop 173×85; 12-level Bayer4 amount0.55, scanline gain0.75. Compare whole-frame resize with tile-local reset and tile-origin phase. Restore original source alpha for every path. Compare visible composited RGB over black, 0.5 gray, and white; edge ROI is cv2 dilation (5×5, one iteration) of partial-alpha pixels. Hidden-RGB sensitivity compares magenta/green hidden values while visible RGBA remains identical.
- **合否:** No perceptual threshold is established. Global processing serves as the algorithmic reference, but its match is not a product acceptance condition. Alpha max difference 0 is guaranteed by explicitly restoring source alpha and only verifies this model setup.
- **実行環境:** Python 3.14.2, OpenCV 4.13.0, NumPy 2.4.3, Pillow 12.2.0, CPU. `python retro_lab.py fixtures/color-space-fixture.png --alpha-pixel-art-crop-study --output output/alpha-pixel-art.png`.

### 観測

| scale / filter | policy | composited edge MAE | changed edge pixels | hidden-RGB sensitivity edge MAE |
|---|---|---:|---:|---:|
| 0.50 AREA | tile-local phase | 0.07306 | 55.61% | 0.01801 |
| 0.50 AREA | tile-origin phase | 0.03961 | 43.74% | 0.01708 |
| 0.50 LINEAR | tile-local phase | 0.08085 | 54.29% | 0.01509 |
| 0.50 LINEAR | tile-origin phase | 0.04055 | 33.27% | 0.01403 |
| 0.75 AREA | tile-local phase | 0.07639 | 55.15% | 0.01366 |
| 0.75 AREA | tile-origin phase | 0.03246 | 34.35% | 0.01403 |
| 0.75 LINEAR | tile-local phase | 0.07929 | 52.75% | 0.01262 |
| 0.75 LINEAR | tile-origin phase | 0.03388 | 25.39% | 0.01308 |

- Black/gray/white composited MAE values are identical because all policies restore identical alpha: in a difference between two composites over the same opaque background, the background terms cancel algebraically. The contact sheet is retained across all 3 backgrounds because contrast affects visual visibility, not this numeric difference.
- Tile-origin phase reduces visible edge difference from global processing in every tested pairing but does not eliminate it. Resize support and tile scale rounding still differ. Hidden-RGB sensitivity does not improve consistently: at scale0.75, origin correction raises sensitivity slightly for both kernels.
- **色・alpha:** Source alpha is preserved by construction. This does not validate Core alpha-mode descriptors, premultiplied inputs, hidden-RGB handling contracts, or GPU paths. Hidden RGB did affect output edge after RGB resampling, then original alpha restoration.
- **Temporal:** Not applicable to still image; sequence shimmer untested.
- **経路/性能:** Python OpenCV CPU model only. Core runtime and GPU/backend paths untested; no timing or memory data.
- **画質:** Contact sheet shows hard panel and circle edges plus thin diagonal over three backgrounds. This is visual inspection of synthetic asset only; no independent reviewer or acceptance criterion.

### Core判断・次

- Existing lab recipe (and this isolated model) intentionally resize RGB independently, then restore original alpha. Results show why crop-local phase correction is only part of parity: it reduces differences but cannot make separately resized crops equivalent to a whole-frame resize.
- No Core code change. Core resize default and actual product call path were traced previously, but not executed here; the synthetic result does not demonstrate a product defect.
- Next compare transparent-color dilation/premultiplied RGB choices on this chart as distinct looks, then determine whether any actual RetroSuite call path crops before resizing. Keep quality selection open until realistic photographic alpha assets or a user-approved reference and runtime captures exist.

## 2026-11-11: Alpha resize policies trade hidden-color invariance against edge look

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_resize_strategy_study`、`captures/alpha-resize-strategy-study.png`、前実験の`fixtures/alpha-pixel-art-crop-fixture.png`。
- **問い:** 実preset縮小のstraight RGB（baseline）、radius-8 nearest-visible dilation、8-round mean dilation、premultiplied RGB/alpha縮小のどれがhidden RGB感度と元edge lookをどう変えるか。
- **比較条件:** 前実験の515×257人工RGBA chart（SHA-256は前項を参照）、alpha=0 RGBだけmagenta/greenを反転するvisible-identical pair、handheld (scale 0.5) / arcade (scale 0.75)、seed37固定、実`apply_retro` recipe（各presetのpalette/Bayer/aberration/scanline/grain）を使用。alphaはsourceから復元し黒/灰/白背景へsRGB code value上で合成。edge ROIはpartial alpha境界を5×5 dilation。出力時間・memoryは未計測。
- **合否:** quality thresholdや承認lookなし。alpha difference 0は`apply_retro`がalpha restoreする実装契約の確認のみ。nearest/dilated/premultipliedに採用順位を付けず、edge-changeとhidden-pair sensitivityを併記する。

### 観測

| preset | policy | vs straight edge composite MAE | hidden-RGB composite edge MAE | alpha max error |
|---|---|---:|---:|---:|
| handheld | straight | 0 | 0.01309 | 0 |
| handheld | nearest dilation | 0.01268 | 0 | 0 |
| handheld | mean dilation | 0.01276 | 0 | 0 |
| handheld | premultiplied | 0.02825 | 0 | 0 |
| arcade | straight | 0 | 0.01121 | 0 |
| arcade | nearest dilation | 0.00928 | 0 | 0 |
| arcade | mean dilation | 0.00930 | 0 | 0 |
| arcade | premultiplied | 0.01956 | 0 | 0 |

- Three opaque backgrounds had identical numerical deltas because each policy restores the same alpha plane; contact sheet retained all backgrounds for visual contrast inspection.
- Nearest/mean dilation are nearly indistinguishable after these preset scale+palette stages on this fixture; this does not prove the heuristics equivalent for other shapes. Their visible edge look changes from straight baseline. Premultiplied gives zero hidden-RGB dependence but its edge look changes more on this chart.
- Fully transparent-region RGB remains different outside what the bounded dilation fills (whole-image output RGB pair MAE is nonzero for nearest/mean), while composited edge sensitivity was 0 for this fixture. This distinction matters if later operations sample those transparent colors again.
- **色/alpha:** Alpha plane exactly restored. Composition metrics are output-code-space; linear-light compositing, premultiplied input descriptors, HDR and colored semitransparent fringes not tested.
- **Temporal:** Static still; temporal consistency not applicable/tested.
- **Path:** This is the actual RetroImageLab `apply_retro` model, not Artifact/Core production runtime or GPU parity.
- **Quality:** Full contact sheet visually inspected at three backgrounds; no independent evaluator or target reference, so no pass/fail on appearance.

### 判断・Coreへの反映

- No Core code change. Results support keeping alpha resize as an explicit look choice in the independent lab rather than silently replacing straight processing globally.
- If product behavior changes later, acceptance must separately specify visible edge tolerance from the chosen straight reference, hidden-RGB pair sensitivity, exact alpha preservation, and backgrounds; this synthetic chart result alone is not an integration approval.
- The next experiment adds multiple transparent foreground colors, 1/2/4/8px gaps and hair-like translucent strands; compare these policies on them and then add at least one clearly licensed photographic asset or approved reference before selecting a recipe.

## 2026-11-11: Resize policy behavior diverges on multicolor gaps and translucent strands

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_resize_multicolor_study`、`fixtures/alpha-resize-multicolor-fixture.png`、`captures/alpha-resize-multicolor-study.png`。
- **問い:** 色の異なる近接形状・細い透明gap・半透明strandを持つ決定chartで、straight / nearest dilation / mean dilation / premultipliedのhidden RGB感度と縁lookを4 preset横断で比較する。縮小なしpresetをnegative controlとして加える。
- **入力:** 480×320 deterministic synthetic RGBA、seed 53、SHA-256 `2b5151918a25d568dca0b3cdec8ded93c6df92c6324fbf95a74e15713f660720`。opaqueな対照形状は1/2/4/8 source-pixel gap、下部に22本のalpha 92–210の1〜2px曲線strand。透明部のRGBはmagenta/green checker、pair側はgreen固定。実写・外部参照なし。
- **比較:** 現行`apply_retro`のhandheld (0.5)、arcade (0.75)、print/vhs (1.0)。seed固定、各presetのpalette/Bayer/aberration/scanline/grain込み。source alphaは元のalphaへ復元。edgeはpartial-alphaを7×7 dilationしたROI。隣り合う形のgap ROIは後段出力寸法へnearestで写像。black/gray/white sRGB code-value compositeをcontact sheet化。
- **合否:** 承認look/referenceも品質閾値もなし。alpha exactnessのみ実装契約として記録し、look差とhidden-pair差を用途領域別に並記する。性能/memory未計測。

### 観測

| preset | policy | straight比 edge RGB MAE | hidden RGB edge MAE | hidden composite edge MAE | alpha max error |
|---|---|---:|---:|---:|---:|
| handheld | straight | 0 | 0.22002 | 0.00928 | 0 |
| handheld | nearest | 0.19257 | 0 | 0 | 0 |
| handheld | mean dilation | 0.19092 | 0 | 0 | 0 |
| handheld | premultiplied | 0.28905 | 0 | 0 | 0 |
| arcade | straight | 0 | 0.21749 | 0.00647 | 0 |
| arcade | nearest | 0.20136 | 0 | 0 | 0 |
| arcade | mean dilation | 0.19985 | 0 | 0 | 0 |
| arcade | premultiplied | 0.29138 | 0 | 0 | 0 |
| print | all four | 0 | 0.23179 | 0 | 0 |
| vhs | all four | 0 | 0.21749 | 0.00345 | 0 |

- Dilation policies produced zero hidden-RGB pair delta in the tested edge and all four gap ROIs after the scale presets, but shifted visible edge RGB substantially from straight. Premultiplied also removed hidden-pair edge sensitivity while changing edge RGB more. No policy can be selected from leakage reduction alone.
- `print` and `vhs` have scale 1.0, so `alpha_resize` has no resize to affect; all four policies match within each preset. `print` has zero hidden composite edge delta despite nonzero uncomposited RGB delta; vhs has visible leakage from later processing. They must not be merged into one preset result.
- black/gray/white composite difference values are equal because all policies restore the exact same alpha; background contrast remains represented in visual panels.
- The synthetic strand ROI is valid only for this fixture; it does not model real hair mattes, motion, or human quality preference. No photographic alpha asset/reference has been tested yet.
- **色/alpha:** sRGB code-value compositing; HDR, linear-light comp, premultiplied-input descriptors and other alpha conventions untested.
- **Temporal:** Still-image only; not applicable. **Path/performance:** Python lab `apply_retro`; no Artifact/Core runtime/GPU parity, timing, or memory evidence.
- **Visual review:** full contact sheet reviewed at three backgrounds; no independent evaluator or acceptance look, so appearance remains unjudged.

### 判断・Coreへの反映

- No Core changes. This wider synthetic control makes the tradeoff stronger, not a product-level adoption case: scale presets suppress hidden-color dependence under dilation while visibly altering edges. Scale-1 controls verify that resize policies should have no effect when no resize occurs.
- **次:** inspect larger per-gap/per-strand crops and repeat with at least one reproducibly sourced image with explicit license/permission or an approved reference; then set a visible-edge acceptance method before judging an alpha-resize recipe. Runtime and performance remain outstanding if product integration is ever considered.

## 2026-11-11: Isolated resize sweep exposes scale-dependent transparent-edge leakage

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_resize_gap_scale_study`、`captures/alpha-resize-gap-scale-study.png`。
- **問い:** Full presetのposterization/aberration/grainを除外し、RGBA policy＋Pillow BOX downscale/NEAREST upscaleだけにした時、gap-widthと縮小率でhidden RGB依存とvisible RGB差がどう変わるか。
- **入力/条件:** experiment 037と同じ480×320 fixture、SHA-256 `2b5151918a25d568dca0b3cdec8ded93c6df92c6324fbf95a74e15713f660720`、seed71、scale 0.25/0.375/0.5/0.75/1.0。RGBはstraight / radius-8 nearest dilation / 8-round mean dilation / premultiplied RGB+alphaとしてresizeし、alphaはoriginal planeへrestore（scale1はresize no-op）。評価edgeはpartial-alpha領域7×7 MaxFilter dilationとopaque-support 3×3 morphological boundaryの和。gap ROIはsource座標に固定。Hidden pairではalpha=0 RGBだけを入れ替える。composite MAEは黒背景式`RGB * alpha`で評価。
- **判定設計:** look targetやthresholdなし。filter-only modelでの数値比較とcrop目視を行い、full preset結果とは別証拠として扱う。CPU/Pillow、時間/memory未計測。

### 観測

| scale | straight hidden composite edge MAE | nearest / mean / premultiplied hidden composite edge MAE | nearest vs straight edge RGB MAE | premultiplied vs straight edge RGB MAE |
|---:|---:|---:|---:|---:|
| 0.25 | 0.04639 | 0 / 0 / 0 | 0.18401 | 0.33373 |
| 0.375 | 0.04092 | 0 / 0 / 0 | 0.19731 | 0.32126 |
| 0.50 | 0.01822 | 0 / 0 / 0 | 0.21252 | 0.31198 |
| 0.75 | 0.01715 | 0 / 0 / 0 | 0.22468 | 0.30111 |
| 1.00 | 0 | 0 / 0 / 0 | 0 | 0 |

- Straight gap hidden composite MAE varied by gap/scale from 0 to 0.054. Nearest/mean/premultiplied gave 0 within tested edge/gap ROIs. Fully transparent pixels themselves are invisible under alpha composite; their relevance is their influence on filtered visible samples.
- At scale1 all methods bypass resize and are identical. This resize-only model differs from experiment 037's full preset at scale1 print/vhs, where non-resize processing propagated hidden RGB.
- Nearest/mean substantially change edge RGB from straight on this chart (MAE about 0.18–0.22); premultiplied about 0.30–0.33. Expanded crops visually show color/brightness changes in thin strands, particularly premultiplied, but no independent preference or approved reference exists.
- Experiment 037's raw gap metric included fully transparent uncomposited RGB, so it is not a visible-output measure. This experiment adds composited gap values; prior raw metrics remain separately labeled.
- **色/alpha:** source alpha restored exactly; compositing in sRGB code values. Linear-light compositing, premultiplied input contracts and HDR remain untested.
- **Temporal/path/performance:** still-image CPU model only; temporal stability, Core/GPU parity and timing/memory untested.
- **Visual review:** 2× enlarged crops at fixed display size inspected across policies and scales. Synthetic evidence only; no independent judge or acceptance threshold.

### 判断・次

- No Core change. The isolated resize step explains visible hidden-RGB sensitivity for scale<1 but not leakage in full scale-1 presets; those paths require separate stage-level tracing. No strategy is selected because leak suppression changes hair-like strand appearance.
- **訂正:** experiment 037の初版はprint/vhsを誤って同じ値として要約していた。再計測ではprint straight edge composite MAE 0、vhs 0.00344694。experiment 038のstage attributionが差を説明する。
- **次:** inspect whether RGB channel offset or grain reintroduces edge dependence in scale-1 presets and separate each stage; obtain permission-cleared realistic alpha artwork before making product-quality claims. VOA fixture provenance remains caveated and is not used as licensed evidence.

## 2026-11-11: Channel-offset leakage rises with absolute shift

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_chromatic_offset_sweep`、`captures/alpha-chromatic-offset-sweep.png`。
- **目的:** stage attributionで判明したVHS channel offsetのhidden-color propagationが、offset magnitudeと符号にどう応答するか、同じvisible source pairで測る。
- **入力:** experiment 037 synthetic multicolor 480×320 RGBA、alpha=0 RGBのみ(13,243,31)に変更したpair。seed97。preset VHS quantization (48 levels)を通した後、channel offsetだけを0/.25/.5/1/2/4/-2 pxで変更。scanlines/grain無効。composite背景はmid-gray、edge ROIはpartial alphaを7×7 dilation。alpha-plane exactness確認。
- **合否:** accepted lookやthresholdなし。straight/zero-offsetを比較基準とし、漏れ量とvisible変化を別指標で示す。結果は人工素材・CPU modelに限定。

### 観測

| signed offset | hidden composite edge MAE | visible composite edge MAE vs 0 | edge RGB MAE vs 0 | alpha exact |
|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | 0 | yes |
| +0.25 | 0.000289 | 0.000789 | 0.01529 | yes |
| +0.5 | 0.000579 | 0.001578 | 0.03055 | yes |
| +1 | 0.001156 | 0.003157 | 0.06110 | yes |
| +2 | 0.003645 | 0.007508 | 0.11654 | yes |
| +4 | 0.011900 | 0.018759 | 0.21013 | yes |
| -2 | 0.003670 | 0.007506 | 0.11639 | yes |

- Pair leakage and visible change both rise with absolute shift through 4px on this chart. At ±2px the two signs are similar in aggregate MAE, but detail crops show opposite colored fringe direction. The curve is not assumed linear and no acceptable leakage/look threshold is established.
- Black/gray/white composite-difference values are algebraically equal because both variants retain identical alpha; mid-gray is used for rendered evidence. Full crop sheet visually inspected.
- Temporal: still image, not applicable. Color-domain: existing channel sampler interpolates sRGB code values. Linear-light sampling, Core/GPU path, performance/memory and realistic licensed alpha art remain untested.

### 判断・次

- No Core changes. This identifies shift distance as a controllable lab tradeoff: lower offset reduces leakage and visible edge change, but the desired VHS look is unapproved. Next inspect direction-specific channel footprints / clamp behavior and compare a permission-cleared natural alpha asset before recommending a default.

## 2026-11-11: VHS channel offset is the first visible hidden-RGB propagation stage

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_hidden_stage_attribution`、`captures/alpha-hidden-stage-study.png`。
- **目的:** experiment 037で同一視していたscale-1 print/vhsのhidden color差を再確認し、quantization・chromatic channel offset・scanline・grainのprefixを足した時のedge合成差を分ける。
- **入力:** experiment 037 synthetic 480×320 RGBA chart、alpha=0 RGBのみ(13,243,31)へ変更したvisible-identical pair。seed83。edge ROIはalpha partial pixelsの7×7 dilation、compositeはsource alphaを用いblack/gray/white code-value background。
- **方法:** `apply_retro`のstage prefixを固定設定で順に実行: identity (colors256), preset quantization (dither含む), channel offset, scanlines, grain。print/vhs各preset値を使い、scale=1。全prefixの出力を3背景contact sheetへ保存。
- **合否:** threshold/approved lookなし。差が出る段の同定だけを目的とし、観測はこのchart・seed・setting限定。

### 観測

| preset | stage | hidden composite edge MAE | stage-added delta | alpha exact |
|---|---|---:|---:|---:|
| print | identity | 0 | 0 | yes |
| print | quantize | 0 | 0 | yes |
| print | channel offset (0 amount) | 0 | 0 | yes |
| print | scanlines | 0 | 0 | yes |
| print | grain | 0 | 0 | yes |
| vhs | identity | 0 | 0 | yes |
| vhs | quantize | 0 | 0 | yes |
| vhs | channel offset (2px amount) | 0.003645 | +0.003645 | yes |
| vhs | scanlines | 0.003474 | -0.000171 | yes |
| vhs | grain | 0.003449 | -0.000025 | yes |

- This confirms experiment 037 correction: print has zero visible pair delta; VHS is .003447 after all stages. The VHS channel-offset stage first exposes hidden RGB into semi-transparent edge samples; scanline and shared-seed grain slightly attenuate this aggregate metric rather than introduce it.
- sRGB channel sampling and original alpha restore; no linear-light or GPU path. Full contact sheet visually inspected; synthetic material and no independent evaluator. Temporal behavior irrelevant for this still. Performance/memory unmeasured.

### 判断・次

- No Core change. An alpha-aware resize cannot fix a later spatial channel sampler. Next sweep offset amount/sign and sample direction on the same chart to measure leakage versus visible aberration before considering any algorithm change.

## 2026-11-11: CC0 clover cutouts confirm smaller alpha-resize look deltas

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_resize_real_corpus_study`、`captures/alpha-resize-real-corpus-study.png`、4 preset contact sheets `captures/cc0-clover-{handheld,arcade,print,vhs}.png`。
- **素材:** OpenGameArtの[Clover Cutouts - Improved and Expanded!](https://opengameart.org/content/clover-cutouts-improved-and-expanded) (Croomfolk)。配布pageはCC0・public-domain cut-out PNGと記載し、archive内READMEもCC0を明記。archive SHA-256 `44ceed28b10eceee48cebe563bfb9f3c4bcf7a8e1617858c2f6b88cfe41ec69b5`。素材ZIP/PNG/provenance manifestはignored `eval-inputs/cc0-white-clover/`に保存し、各PNG SHA-256は同ディレクトリおよび次表に記録。用途・作者のendorsementを示唆しない。

| asset | size | SHA-256 | transparent ratio | partial-alpha ratio | edge ROI ratio |
|---|---:|---|---:|---:|---:|
| clover_close.png | 1436×1436 | `8052d08ef1901b3658845d6728ca98331d8c0ba200344f021b8407d6bcd63c8e` | .52269 | .02530 | .04561 |
| clover_close2.png | 1436×1436 | `214259e22c9db14f855f260927e92f7121e82472b3b2552d3351da504fb1ca9c` | .53573 | .02181 | .03882 |
| clover_full.png | 1017×1807 | `2e325727cf8369e7e8eafd7a05e905578c8746c9ebf0637785a9592975415523` | .86372 | .00545 | .03846 |
| trifolium_repens_macro.png | 1037×1347 | `f347ea575b04c894bd33fcacfe9d698f273cefa809a0ce5ea50d9d2df837cb0a` | .56786 | .00977 | .05329 |
| vegetation_clover_02a.png | 451×769 | `3548803a33d8936f174cad65e975aec6b24598a7922feda8ebf2d82f392eb928` | .67379 | .01109 | .08031 |

- **比較:** input pairはalpha=0 RGBのみmagenta/greenに置換。4 policy (`straight`, radius-8 nearest, 8-round mean, `premultiplied`)を実`apply_retro` handheld(.5) / arcade(.75)へ通し、seed109固定。edge ROIはbinary alpha 3×3 morphological boundaryとpartial-alpha 7×7 dilationのunion。黒/灰/白へsRGB code valuesで合成し、straightとの差とvisible-identical hidden pair差を測る。元alphaを厳密保持するかbyte比較。基準look/reference/thresholdなし。CPU labのみ、時間/memory未計測。

### 観測

| preset | policy | 5素材 hidden composite edge MAE 平均 / 最大 | straightからのedge composite MAE 平均 / 最大 | alpha exact |
|---|---|---:|---:|---:|
| handheld | straight | .00312 / .00784 | 0 / 0 | yes |
| handheld | nearest | 0 / 0 | .00283 / .01067 | yes |
| handheld | mean dilation | 0 / 0 | .00282 / .01053 | yes |
| handheld | premultiplied | 0 / 0 | .01566 / .02739 | yes |
| arcade | straight | .00285 / .00843 | 0 / 0 | yes |
| arcade | nearest | 0 / 0 | .00324 / .01206 | yes |
| arcade | mean dilation | 0 / 0 | .00322 / .01189 | yes |
| arcade | premultiplied | 0 / 0 | .01033 / .01654 | yes |

- Unlike the synthetic chart, nearest/mean edge-look change is small on average (about .0028–.0032 composite code-value MAE over the ROI), while hidden-pair edge sensitivity reaches 0 for all five assets. Premultiplied also removes pair sensitivity but has a larger straight-look delta. These measurements do not select a preferred look.
- The no-provenance `suite-corpus` whole-image RGB RMSE is not the edge-quality measure here: transparency distribution and hidden RGB can dominate whole-image differences. All five source alpha planes were preserved exactly by every policy. Contact sheet shows clover leaves / flower cutouts on mid-gray; same-asset comparisons are visible, but there is no approved reference or independent evaluator.
- **Temporal:** Still assets; not applicable. **Color/alpha:** sRGB code-value composite, no HDR/linear-light/premultiplied-input descriptor test. **Path/performance:** Python `apply_retro`, not Core runtime/GPU; time and memory not measured.

### 判断・次

- No Core change. The licensed real cutouts weaken the synthetic chart's large visible-edge-change concern for nearest/mean dilation, but only for these five green/white clover images and two scale presets. A larger and taxonomically varied alpha corpus is still needed before defaults or product changes.
- **次:** add feather/fur/metallic translucent edges from distinct permission-cleared sources, inspect enlarged real-source edge crops, and test with alpha input contracts / actual product route if an integration candidate emerges. Keep the VOA input marked rights-review-pending.

## 2026-11-11: Existing hidden RGB in a clover cutout leaks only near its edge

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_hidden_source_residual_study`、`captures/alpha-hidden-source-residual-study.png`。
- **素材:** CC0 `vegetation_clover_02a.png` from the pack recorded in experiment 041; 451×769, SHA-256 `3548803a33d8936f174cad65e975aec6b24598a7922feda8ebf2d82f392eb928`。alpha=0 pixels: 233,682, unique hidden RGB colors 27,332, all-zero RGB ratio .00434, mean hidden RGB `(203.53,163.81,170.75)`.
- **問い/手順:** original hidden RGB versus zeroed alpha=0 RGB, then only zero near hidden pixels (within 8px Chebyshev distance of alpha>0 support) or only deep hidden pixels. Compare all 4 resize policies through 4 actual lab presets; seed127. Edge ROI is binary-alpha 3×3 morphological boundary union partial-alpha 7×7 dilation. Composite using preserved source alpha over black (other opaque backgrounds produce same pair-difference equation).
- **合否:** No reference look or threshold. Only visible pair sensitivity and alpha exactness. Still image; CPU lab; product route/GPU and timing/memory untested.

### 観測

| preset | policy | original hidden RGB → zero, edge composite MAE | near-hidden-only change | deep-hidden-only change | alpha exact |
|---|---|---:|---:|---:|---:|
| handheld | straight | .010301 | .010301 | 0 | yes |
| handheld | nearest | 0 | 0 | 0 | yes |
| handheld | mean dilation | 0 | 0 | 0 | yes |
| handheld | premultiplied | 0 | 0 | 0 | yes |
| arcade | straight | .010743 | .010743 | 0 | yes |
| arcade | nearest | 0 | 0 | 0 | yes |
| arcade | mean dilation | 0 | 0 | 0 | yes |
| arcade | premultiplied | 0 | 0 | 0 | yes |
| print | all policies | 0 | 0 | 0 | yes |
| vhs | all policies | .016979 | .016979 | 0 | yes |

- In handheld/arcade, this asset's existing nonblack transparent RGB affects the output edge only when it lies within 8 source pixels of visible support; all four policies eliminate that measured effect after scaling. Deep hidden pixels are outside the sampler footprint in this case.
- Print has no spatial chromatic offset and shows zero. VHS is scale1 so alpha resize policies are bypassed; its channel offset still yields .016979 for every policy. This matches the stage attribution pattern in synthetic chart, now with a real asset whose hidden RGB was already present in the source.
- The midgray sheet visually inspected; display thumbnails are overview only. No independent reviewer/approved target, and no quality threshold. Only one source asset was selected because the other four pack assets are overwhelmingly zero RGB under alpha0.
- **色/alpha:** exact alpha restoration; sRGB code-value composite. HDR, linear-light, premultiplied input descriptors untested. **Temporal:** not applicable. **経路/性能:** product/GPU parity, preview/final and time/memory untested.

### 判断・次

- No Core change. The real source demonstrates a practical distinction: the stored hidden colors in this cutout cause a measurable edge change for straight resize, while deep padding does not. Dilation/premul eliminate the measured source-residual effect here, but VHS demonstrates that a later spatial sampler can reintroduce it regardless of resize policy.
- **次:** inspect source-alpha conventions and hidden-color preprocessing in any actual Core callers; then evaluate more species/materials with distinctive alpha mattes. No product default is selected from this one clover cutout.

## 2026-11-11: Source-derived Core chromatic-shift model exposes a contract tradeoff

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::core_chromatic_alpha_source_model`、`captures/core-chromatic-alpha-model-study.png`。
- **調査対象:** `ArtifactCore/src/Graphics/Effect/ChromaticAberrationEffect.cpp` lines 34–54 rounds `amount×cos/sin` to integer shifts, moves Red and Blue in opposite directions, clamps sample coordinates, copies full-frame temporary R/B planes back, and never reads/writes Alpha. `ArtifactCore/include/Channel/Channel.ixx` declares an Alpha plane; `VideoFrame` stores channel planes separately. `VideoFrameMetadata` does not declare alpha association in this interface. Existing tests at `tests/ArtifactCore/ColorVibranceEffectTest.cpp` 1833 and 1860 verify RGB processing without optional Alpha and alpha remains at original pixel positions; the OldTV test at 2443 verifies RGB output independent of optional alpha-plane values. They do not test a transparent-hidden-RGB pair.
- **Model:** Python implementation copies those integer round/shift/clamp RGB equations, source-derived against the inspected code; plus a reference variant that outputs 0 only when the sampled source alpha is exactly 0. The reference is a candidate study model, not an accepted algorithm. Inputs: CC0 clover asset SHA from experiment 042 plus the 480×320 multicolor synthetic chart. Amount 0/1/2/4 pixels horizontal and 2 pixels vertical; alpha preserved, black composite, binary/partial edge union. No stochastic processing.
- **合否:** no approved effect look or alpha sampling contract; report leakage and visible difference separately. This is a source-derived Python CPU model, not C++ execution/runtime parity, GPU parity, or performance measurement.

### 観測

| input | Core-model amount/angle | hidden-pair edge composite MAE | alpha-aware-reference pair MAE | visible delta vs reference |
|---|---:|---:|---:|---:|
| CC0 vegetation clover | 0px/0° | 0 | 0 | 0 |
| CC0 vegetation clover | 1px/0° | .006449 | 0 | .006449 |
| CC0 vegetation clover | 2px/0° | .018613 | 0 | .018613 |
| CC0 vegetation clover | 2px/90° | .010795 | 0 | .010795 |
| CC0 vegetation clover | 4px/0° | .049142 | 0 | .049142 |
| Synthetic multicolor chart | 1px/0° | .029527 | 0 | .029527 |
| Synthetic multicolor chart | 2px/0° | .030064 | 0 | .030064 |
| Synthetic multicolor chart | 2px/90° | .031046 | 0 | .031046 |
| Synthetic multicolor chart | 4px/0° | .034814 | 0 | .034814 |

- Suppressing fully transparent source samples removes dependence on their RGB in this model, while the resulting edge composite change equals the hidden-pair delta. This is an explicit look alteration, not a free quality fix. The amount curve differs between the real clover and the chart due to their alpha/color geometry.
- Signed direction is reflected by angle; this pass samples horizontal/vertical shifts, not all angles, border-only ROI, or partial-alpha input premultiplication. Crops inspected at same thumbnail dimensions; no independent reviewer or approved target.
- **Core candidate status:** alpha mode is not identified in VideoFrame metadata, and current tests intentionally cover RGB-with/without-alpha equivalence. Before a Core API or default changes, define the input alpha association and edge-look contract, then add targeted C++ contract coverage and runtime/GPU evidence. No Core code or tests were changed/run in this study.

### 判断・次

- No Core change yet. The model confirms the exact current integer-shift sampler can expose hidden RGB on this CC0 source, but does not establish how upstream `VideoFrame` callers encode RGB/alpha or whether a changed effect is desirable. Existing optional-alpha tests are not hidden-RGB invariance tests.
- **次:** invocation/import path and pixel association have partial source tracing in the subsequent rasterizer study; experiment 062 now covers a Core-equivalent angle/rounding/clamp sweep. The `VideoFrame` CPU effect's actual alpha association, runtime pixels, GPU parity, and preferred edge look remain unresolved; keep lab-only until those contracts are established.

## 2026-11-11: Core chromatic angle response is quantized before RGB channel sampling

### 再現条件と計画

- **関連:** `tools/core_chromatic_angle_study.py`、`fixtures/core-chromatic-angle-sweep.png`、`fixtures/core-chromatic-angle-sweep.json`、`captures/core-chromatic-angle/core-chromatic-angle-metrics.json`、`core-chromatic-angle-sheet.png`。
- **目的:** Core `ChromaticAberrationEffect` のround-to-integer amount/angle responseを、visual output前にvector量子化・方向誤差として分離し、端clampとalpha保持をstress chartで確認する。
- **Source equation:** `ArtifactCore/src/Graphics/Effect/ChromaticAberrationEffect.cpp` に合わせ、R shift=`round(amount*cos(angle)), round(amount*sin(angle))`、Bは逆vector、Gと出力alphaはpixel-local保持、source coordinateはimage boundsへclamp。roundingはC++ `std::round` のhalf-away-from-zero。
- **Sweep:** amount 0/.25/.5/.75/1/1.5/2/4/8/16/32/50px。各amountで0–359°を1°刻み。加えて48 preview cases (amount .5/1/1.5/2/4/50、angle 0/30/45/60/90/120/180/270)。fixtureは256×192 RGBA8、alpha-zero hidden-RGB checker、opaque/partial-alpha RGB line/rectangle、斜線、上下左右border contactを含む。
- **Metrics:** distinct integer vectors、zero-vector angle比、理想continuous vectorとの差、非zero時の方向差、clamp source sample率、alpha差、edge/whole composite MAE/P95、alpha-zero hidden-RGB pair差。表示/比較compositeはsRGB code-value domain、linear-lightではない。
- **制限:** Python source-derived CPU equation model only。Core C++ execution、float/HDR input、GPU backend、性能、ユーザー承認lookは未検証。数値は外観合否ではない。

### 観測

| Amount | 1° sweepでのunique shift vectors | zero-shift angles | 最大方向誤差 |
|---:|---:|---:|---:|
| .25px | 1 | 100.0% | — (全vector zero) |
| .5px | 5 | 98.9% | — (非zeroはcardinal方向のみ) |
| .75px | 8 | 0% | 41.0° |
| 1px | 8 | 0% | 30.0° |
| 1.5px | 12 | 0% | 25.0° |
| 2px | 16 | 0% | 14.4° |
| 4px | 32 | 0% | 8.0° |
| 8px | 64 | 0% | 4.3° |
| 16px | 120 | 0% | 2.3° |
| 32px | 224 | 0% | 1.1° |
| 50px | 320 | 0% | .6° |

- 0.5px/45°はR shift=(0,0)で効果が消える一方、0.5px/0°と90°は各1px移動する。1pxの30°/45°/60°は同じ(1,1) vectorになり、角度は連続応答ではない。
- amount増加に伴いvector数が増えて最大方向誤差は減るが、50pxでは広い境界領域をclampする。fixtureでR/Bのいずれかのsource coordinateがclampされる率は50px/0° .195、45° .294、90° .260。alphaは48 preview全てで完全保持。
- alphaを保ってもRGB shiftはalpha-zero hidden RGBに依存する。0.5px/0°のhidden-pair edge MAEは.0307、45°はzero shiftのため0。50px/45°は.0949。これは合成fixtureのcode-value measurementであり、望ましい縁や製品差を示さない。
- Contact sheetでsubpixel amountのangle dead-zone、整数vectorの角度段差、最大amountのhidden-color/checker露出を目視確認した。

### 判断・次

- UI amountが小さい範囲では、round-to-integer後にangleによってeffectが完全に消える角度領域があり、同じamountでも向きの応答差が大きい。これは現在のsource behaviorの記述で、smoother bilinear samplingへの変更提案ではない。
- Maximum amountは単なる色ずれ強度だけでなく、clamped border sample領域とhidden-RGB exposureも増やす。Core API・既定値は変更しない。次に進むには実際にeffectを使う caller のalpha associationと出力lookを確定し、必要ならユーザーが許可したC++ runtime/contract検証を別計画にする。

## 2026-11-11: Border clamp の可視影響は被写体の配置で変わる

### 再現条件と計画

- **関連:** `tools/core_chromatic_boundary_study.py`、`fixtures/core-chromatic-boundary-control.png` / `.json`、`captures/core-chromatic-boundary/core-chromatic-boundary-metrics.json`、`core-chromatic-boundary-sheet.png`。
- **Fixture:** 同じ非対称RGBグラデーション・色識別stripe・4x supersampled partial-alpha輪郭を256×192 canvas上の中央、左、右、上、下、左上へ配置した6ケース。透明画素には隠れRGB checkerを保持。外部素材なし。
- **比較:** amount 1/4/16/50px × angle 0/45/90/135度の96条件。Core式と等価な整数R/B移動・bounds clampを実行し、clamp外sampleのみRGB=0にする診断counterfactualと比較。alphaはpixel-localのまま。sRGB code-valueでblack/mid-gray/whiteに合成し、全体・object・edge・partial-alpha・clamp destinationを集計する。
- **制限:** Python source-derived modelのみ。C++/GPU、HDR、性能、承認look、実素材は未評価。RGB=0は境界差の局在を見るためのcounterfactualであり、提案するedge policyではない。

### 観測

- 96条件全てでalphaは完全一致。最大amount 50px/45度では全6配置でcanvas画素の53.8%にいずれかのR/B clampがあるが、中央配置はobject/partial-alpha輪郭とclamp領域の交差が0。端配置ではclamp領域にobject画素が3,207–3,372、partial-alpha画素が468–554含まれた。
- 同じ50px/45度でmid-gray compositeのclamp-vs-zero-outside edge MAEは中央0、左.1516、右.2051、上.1340、下.1888、左上.1657（0..1 normalized code value）。差分channelはshift方向に応じてRまたはBで現れた。これは2つの式の差であり、品質評価ではない。
- Clamp destinationが画面端に多数存在しても、被写体が中央なら透明背景しか重ならず可視輪郭への影響はない。一方、被写体が端へ触れると同じ境界規則が半透明を含むobjectへ作用する。配置・amount・angleを一緒に記録する必要がある。
- Fixture hashをmanifestと照合。96 case、29,881数値項目はfinite。contact sheetはsourceと4方向のamount 50を6配置で目視確認。

### 判断・次

- clampはcanvas boundsに対して安定した現行契約であり、この研究だけで端のRGBを0化したり別policyへ変更する根拠にはならない。端に触れる素材を含む評価fixtureを今後のchromatic比較へ追加する。
- 次は利用条件の明確な写真/透過素材で中央・端配置のcropを評価し、実callerのalpha associationが確認できる場合のみCore runtime調査を提案する。現段階でCoreの追加変更なし。

## 2026-11-11: Quantized no-op chromatic calls can bypass frame-sized buffers

### 根拠と実装

- **関連:** experiment 062、`tools/core_chromatic_angle_study.py` のallocation model、`ArtifactCore/src/Graphics/Effect/ChromaticAberrationEffect.cpp`。
- **Source fact:** 非zero shift経路は`width*height` floatの`std::vector`をR/B用に2本作成し、各channelを全pixel samplingした後、`std::copy`でplaneへ戻す。temporary capacityは`8 bytes/pixel`。sampling+copy trafficはsource-levelで`32 bytes/pixel`、vector value-initializationのzero storesが実体化する場合さらに`8 bytes/pixel`となる。compiler/runtime trafficと壁時計時間は計測していない。
- **Example estimate:** 1920×1080でtemporary 15.82 MiB、sampling+copy 63.28 MiB、value-initを含むsource-level nominal upper estimate 79.10 MiB/frame。3840×2160ではtemporary 63.28 MiB、sampling+copy 253.13 MiB、同upper estimate 316.41 MiB/frame。これらはsource arithmeticであってDRAM転送測定ではない。
- **Observed identity region:** experiment 062ではAmount .25pxの全360角度とAmount .5pxの356/360角度がround後shift=(0,0)。effect方程式上、R/B/G/Aはいずれも変化しない。
- **Change:** `ChromaticAberrationEffect::process`でRGB planeの有無を確認しinteger R shiftを確定した後、(0,0)ならframe dimensions取得・一時vector確保・sampling/copyより先にreturnする。nonzero vectorの式とsample behaviorは変更しない。User authorized findings to be reflected in Core; this is a semantic no-op path only。
- **Verification limit:** ソース差分とPython source-derived mathのみ確認。ユーザー提供AGENTS.mdがbuild/testを明示要求まで禁止しているため、CMake/build/testおよびC++ runtime計測は未実施。最適化による実機性能改善量は未確認。

### 判断・次

- 丸め後にeffectがidentityとなる角度/Amount領域ではfull-frame scratchの確保を省ける。Amount=.5pxのほぼ全角度のように一部がzero vectorになる条件も対象となる。
- これは壁時計性能を保証しない。nonzero shiftsは依然2 full-frame vectorsと全pixel処理を通る。実際のthroughput/alloc profiler確認とCore contract coverageは、build/test許可がある時に別途行う。

## 2026-11-11: Rasterizer alpha descriptor survives into chromatic sampling but is not consulted

### 再現条件と計画

- **関連:** `Artifact/src/Effects/Rasterizer/ChromaticAberrationEffect.cpp`、`Artifact/src/Render/ArtifactCompositionViewDrawing.cpp::buildRasterizedSurfaceBuffer`、`experiments/RetroImageLab/retro_lab.py::rasterizer_chromatic_alpha_source_model`。
- **確認できた経路:** QImage layer surface は `normalizeQImageForCvEffectBoundary` と `qImageCvMatSurfaceDescriptor` を通り、`ARGB32` は straight、`ARGB32_Premultiplied` は premultiplied と記述される。一方、still image の通常ロードはOpenImageIOからRGBA floatを取得し、`setFromRGBA32F(data,w,h)` を呼ぶ。このoverloadは`unknownRgba32Float()`を設定する。RGBA→BGRA reorderはRGB値を保持し、alpha associationでRGBを正規化しない。ICC profileはsourceMetadataにbyte数のみ保持され、loaderはICCを適用しないと明示的にwarningを記録する。`applyInputInterpretation` は明示またはnamed source metadataに基づく色空間/transfer変換を行う場合に、metadata alphaAssociatedがtrueならunpremultiply→変換→premultiplyする。**重要:** Rasterizerの汎用float-buffer overloadは直接 `ImageF32x4RGBAWithCache current(surface)` を包むが、Composition Viewのstill `ArtifactImageLayer` + Rasterizer経路は `imageLayer->toQImage()` を呼び、そのQImageを`buildRasterizedSurfaceBuffer`へ渡す。したがって上記float loader値がそのままRasterizer効果へ届くとは限らない。
- **効果の動作:** CPU は descriptor の alpha mode に関わらず RGB sample を読み、出力 alpha は元の pixel のまま。HLSL も RGB のみ別座標から読み、alpha は現在 pixel から保持する。Contract test は RGBA32F の alpha=0 hidden-color pair と descriptor matrix を網羅しない。コード読解であり、実際の OIIO format metadata が alpha-associated と宣言される素材全てを証明するものではない。
- **再現:** experiment 044 と同じ CC0 clover asset/model; straight-alpha cutout と現在の radial sampler 条件を使用。

### 判断・次

- Still-image float loader bufferにはhidden RGBが残り得るが、通常のComposition View Rasterizer entryより手前に`ImageF32x4_RGBA::toQImage()`がある。unknown-alpha/unknown-transfer descriptorも`Rgba8SrgbStraight`変換のdirect branch対象であり、alpha<=1e-6のRGBを0にしてからQImageを作る。よってこのstill-image pathではsource alpha0 RGBの効果への到達は抑えられる見込み。実OIIO decoder値でのruntime captureは未確認。
- これは「合成前に隠れRGBを一律消すべき」という主張ではなく、source alpha association と効果 sampler の契約が分離しており、fully-transparent sample RGB suppression が候補の見た目を変えることを示す。alpha-aware reference は採用仕様ではない。
- **次:** PNG chunk/source residualはexperiment 046、raw-source sampler感度はexperiment 047/048に記録。still ImageLayerのQImage cleanup境界とtransparent-sample fallbackはexperiment 049で追試。実OIIO loader/runtimeは起動しておらず、モデルのC++契約testも未実行。Core/Artifact実装変更なし。

## 2026-11-11: Clover hidden RGB is concentrated at the cutout boundary

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::alpha_hidden_distance_residual_study`、`captures/alpha-hidden-distance-study.png`、素材とCC0 provenanceはexperiment 042と同じ。CLI: `--alpha-hidden-distance-study eval-inputs/cc0-white-clover`。
- **測定:** Pillow で保存PNGをRGBA8 decodeし、可視alpha>0 supportを3x3 square kernelで反復dilateしてalpha=0 ringを作成。1/2/4/8/16/32/64px累積距離を測定。RGB residual は透明画素RGBをblack baselineと比較したmean channel code value、changed ratioはRGB tupleに少なくとも一つ非zeroがある率。画像シートでは各距離内のsource RGBをorangeで重ねた。
- **境界:** `cv2.dilate` の反復をChebyshev distance（square neighborhood）とし、Pillow上の保存RGBA値に限る。OIIO/Artifact import値、alpha mode metadata、合成後の製品captureではない。評価閾値・承認lookなし。

### 観測

| alpha=0 の visible support 距離 | pixels (累積) | nonblack RGB ratio | mean RGB code value | p95 RGB code value |
|---:|---:|---:|---:|---:|
| 1px | 4,587 | .9991 | .4938 | .6340 |
| 2px | 8,539 | .9991 | .5226 | .6719 |
| 4px | 15,859 | .9990 | .5485 | .6745 |
| 8px | 28,987 | .9989 | .5671 | .6732 |
| 16px | 50,633 | .9987 | .5895 | .6810 |
| 32px | 83,727 | .9985 | .6173 | .7150 |
| 64px | 133,507 | .9980 | .6543 | .7765 |

- alpha=0 total 233,682 px; 전체 alpha0의 .99566에 nonblack RGB. Hidden RGB levelは距離とともにむしろ明るくなる。8px帯だけで28,987 pixelsあり、前の radial model（center .5/.5, R8/B8）でpair composite MAE .02327だった結果と整合する高い境界残差がある。
- Previous experiment 042 found deep-hidden-only changes at this sampled preset amounted to 0, while near-hidden-only matched the entire .02327 edge delta. Current distance report is source-side distribution evidence; it does not independently identify exact sampler footprints or establish a universal 8px cutoff.
- **色/alpha:** byte-domain values。**Temporal:** still, N/A。**経路/性能:** Pillow source analysis only; no OIIO/app render, backend parity, or timing. Capture is labeled diagnostic overlay, not candidate output/reference.

### 判断・次

- This cutout contains strongly colored transparent padding directly around its visible matte, so the asset's source residual is a plausible driver of Rasterizer radial edge leakage. It does not imply all straight-alpha assets behave similarly, nor that blackening the fringe is a preferred look.
- **次:** when a compatible OIIO runtime is available, decode this same hash and compare alpha0 RGB/association metadata against Pillow; then feed imported pixels through exact radial CPU model for the actual decoded values. No product/Core change from this one asset.

## 2026-11-11: Actual cutout RGB produces measurable radial fringe under the source model

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::rasterizer_chromatic_alpha_source_model` experiment 044, expanded with multi-background metrics; `captures/rasterizer-chromatic-alpha-model-study.png`.
- **素材:** experiment 042 の同じ CC0 vegetation clover PNG (SHA-256 `3548803a33d8936f174cad65e975aec6b24598a7922feda8ebf2d82f392eb928`) を Pillow でRGBA8 decode。PNG chunk audit はIHDR bit-depth 8/color-type 6 (RGBA)、iCCPあり、tRNSなし、text metadataなし。PNG RGBA sample はunassociated alpha形式のstraight sampleとして解釈する。
- **比較:** Rasterizer CPU/HLSL source-derived radial nearest/clamp model; red=blue 2/8px, center .5/.5。現行 source RGB model と、sampling位置のalphaが0なら sample RGBを0にする候補を比較。black/midgray/white/green背景へ出力をalpha compositeし、edge ROIとvisible ROIを別測定。候補はaccepted lookではない。
- **不確実性:** Python modelとPillow decoder。OpenImageIO Python binding欠如でArtifact実loader、OCIO変換後の実際のpixel数値、GPU readback parity、performanceは未検証。入力source association metadata のOIIO値も未採取。

### 観測

| radial red/blue shift | background | hidden-pair edge MAE | alpha=0 sample suppressionによるvisible MAE |
|---:|---|---:|---:|
| 2px/2px | black | .004262 | .001049 |
| 2px/2px | white | .004262 | .001049 |
| 8px/8px | black | .023270 | .005855 |
| 8px/8px | white | .023270 | .005855 |

- Midgrayとgreenも同一値。出力alphaをそのまま保つ2候補の差分は、straight-alpha合成式で背景寄与が相殺されるため、同じpixel domainで同値となる。複数背景は実装差やlinear/display transformを通した検証ではなく、この合成不変条件を確認する対照。
- 8px edge leakageは前 experiment 046 のsource-sideで強いalpha0 fringeが8pxまで存在するという分布と一致する。ただしこのcorrelationは実Artifact decode/output measurementではない。
- **色/alpha:** sRGB-like normalized code sampleとlinear arithmetic composite model。ICC profileの展開/色変換は反映せず、HDR/partial-alpha input association matrixなし。**Temporal:** still。**Output path/performance:** CPU Python model only、実時間/メモリ未測定。

### 判断・次

- CC0 cloverでは、alpha0 sampleを抑制した場合のvisual changeはedge-only leakageより小さく、visible領域全体ではR8/B8でも平均.005855。一方これを許容差と見なす基準はない。現行効果の隠れRGB感度と候補の見た目変化の両方を提示することが必要。
- **次:** 可能なら Artifactが使うOpenImageIO runtimeで同hashをdecodeし、source metadata/float alpha-zero RGBを採取してこの表を置き換える。runtimeが得られるまでは lab-onlyで保持し、実装変更根拠にしない。

## 2026-11-11: Transfer domain changes the displayed hidden-RGB fringe metric

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::rasterizer_chromatic_transfer_domain_study`、`captures/rasterizer-chromatic-transfer-domain.png`、CC0 asset hashはexperiment 042。
- **ICC:** Pillow は埋め込み3144-byte ICC profileを `sRGB IEC61966-2.1` と認識。PNG RGBA8 samplesをまずsRGB code valuesとして扱い、次にIEC sRGB transfer formulaでlinearへdecode。ここでは ICC LUTを実変換するのではなく、認識したstandard profileのsRGB transfer characteristicを使う。
- **Sampler:** Rasterizer radial nearest/clamp source modelでR=B=2/8, center .5/.5。各domainで元のRGBA (current) とalpha0 RGBを先にblack化したpairを効果へ入力。alphaは保持し、black/midgray/whiteへ合成。linear domainは合成後にsRGB encodeし、edge ROIとvisible ROIのmean display-code MAEを測る。Black display sheetを目視で確認。
- **受入:** Approved look/referenceと閾値なし。色統計はdisplay-code MAE補助指標であり単独採用判断に使わない。Artifact OIIO/OCIO/runtime/GPU経路、処理時間、HDR、alpha descriptorは未検証。

### 観測

| processing / display domain | R=B shift | background | edge hidden-pair display MAE | visible suppression display MAE |
|---|---:|---|---:|---:|
| sRGB code / sRGB code composite | 2px | black / midgray / white | .004262 | .001049 |
| sRGB code / sRGB code composite | 8px | black / midgray / white | .023270 | .005855 |
| linear-light / sRGB display | 2px | black | .005529 | .001361 |
| linear-light / sRGB display | 2px | midgray | .002438 | .000600 |
| linear-light / sRGB display | 2px | white | .001999 | .000492 |
| linear-light / sRGB display | 8px | black | .025367 | .006371 |
| linear-light / sRGB display | 8px | midgray | .020082 | .005070 |
| linear-light / sRGB display | 8px | white | .019276 | .004871 |

- In the sRGB code-value calculation, changing flat background does not change delta because alpha is held fixed and the same background term cancels. After linear-light composite followed by nonlinear sRGB encode, display-code delta depends on background; at R8/B8 edge MAE ranges .01928–.02537. This does not claim that one transfer domain is preferred; it shows the reported fringe score depends on display pipeline.
- Source ICC profile says standard sRGB, but the raw-pixel source model does not reproduce Artifact's OCIO interpretation settings, OIIO color conversion, or GPU presentation. Difference sheet shows same cutout processed in two domains at R8/B8 over black; no independent viewer or approved reference.
- **Alpha:** preserved exactly; fully transparent pixels are excluded from visible composite ROI. **Temporal:** still, N/A. **Path/performance:** Python model, no runtime parity or timing.

### 判断・次

- Report the color domain and composite background with every alpha fringe metric. The earlier multi-background equality is valid only for the code-domain compositing branch; the linear-light branch disproves its use as a universal statement.
- **次:** inspect actual `ArtifactImageLayer::applyInputInterpretation` defaults and OIIO/OCIO code paths for a default still layer, then, only if a runtime/API is already available, capture identical source through the actual effect path. No Core behavior change from this model alone.

## 2026-11-11: Still-image QImage boundary removes alpha-zero source RGB before Rasterizer

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::image_layer_rasterizer_boundary_study`、`captures/image-layer-rasterizer-boundary.png`。
- **製品コード:** `ArtifactCompositionViewDrawing.cpp` has a direct float-buffer Rasterizer overload, but its still `ArtifactImageLayer` branch with Rasterizer work calls `imageLayer->toQImage()`, then `applySurfaceAndDraw(QImage)`. That path calls `buildRasterizedSurfaceBuffer(QImage)`, creates `ImageF32x4_RGBA`, and applies effects via `effect->applyToCpuSurface`. `ImageF32x4_RGBA::toQImage()` requests `Rgba8SrgbStraight`; `SurfacePixelConversion`'s direct-sRGB branch zeros RGB when alpha <= 1e-6 for alpha modes Unknown, Straight, and Opaque. The still OIIO loader's generic RGBA32F overload produces Unknown alpha and transfer descriptor unless a later interpretation transform changes it. Existing `SurfacePixelConversionTest.EverySrgbAlphaByteCodePreservesColoredStraightPixels` covers explicit straight RGBA8 (alpha 0 output RGB is zero); StillImageLayer round-trip coverage records ICC bytes and exact loaded float values, but does not assert the alpha-zero `toQImage`→effect path.
- **Model:** Pillow RGBA8 source from CC0 clover pack. Apply exact source-derived nearest radial coordinate field at R2/B2 and R8/B8. Compare raw-source counterfactual with a `toQImage` proxy that zeros RGB where alpha<=1e-6, and a separate candidate that falls back to the destination R/B when a source sample has alpha=0. Measure alpha-composited edge MAE and source sample-alpha categories. It does not run C++.
- **受入:** no approved look or threshold. Leakage pair invariance and effect-vs-no-shift look change are reported separately. CPU/GPU, preview/export parity, runtime capture, HDR, timing, memory, and exact OIIO decode all remain unverified.

### 観測

| R/B | raw alpha0 hidden-pair edge MAE | after `toQImage` pair MAE | cleanup vs raw edge MAE | current effect vs no-shift edge MAE | alpha0 fallback candidate effect vs no-shift |
|---:|---:|---:|---:|---:|---:|
| 2/2 | .004262 | 0 | .004262 | .010009 | .005443 |
| 8/8 | .023270 | 0 | .023270 | .033434 | .010524 |

- Sample-alpha histogram among visible edge output pixels: R2 red source samples alpha0 .0787 / partial .2216 / opaque .6997; blue alpha0 .0265 / partial .1702 / opaque .8032. R8 red alpha0 .2642 / partial .1815 / opaque .5543; blue alpha0 .0718 / partial .1280 / opaque .8002. These frequencies explain why alpha0 sanitization can remove pair dependence while the sampling operation still changes colors at the matte edge.
- The `toQImage` proxy exactly removes pair sensitivity for RGB changes confined to alpha0 pixels. A different candidate, which retains the destination channel value instead of reading a fully-transparent sample, reduced the model's edge delta versus no-shift for this asset; it remains an artist-visible change and not a selected behavior. Crops are nearest-size contact sheet on black and are diagnostic only.
- **Correction to experiments 044/047/048:** their raw-PNG radial sampler metrics are source sensitivity/counterfactual evidence, not measurements of this actual still-image Rasterizer entry. The inspected Composition View route has an alpha-zero cleanup boundary before the effect; the remaining question is whether fully-transparent samples should produce black-channel gaps and how semi-transparent samples should behave. Generic direct-float effect callers may have a different boundary.

### 判断・次

- Do not change Core's image-loading policy based on raw hidden RGB: the ordinary still-layer render path already zeros alpha0 RGB before its Rasterizer pass. A sharper R&D candidate is output sampling at opaque/partial/transparent alpha boundaries; define semantics for each and compare independently with the existing behavior.
- **次:** build an alpha-matte chart with opaque edge, 8-bit partial ramp, and alpha-zero fringe; measure sample categories and candidate policies at several radial shifts on black/midgray/white. Then statically trace non-Composition-View callers of the direct float-buffer overload to determine whether any need separate analysis. No build/test/product change in this study.

## 2026-11-11: Radial channel shifts increasingly sample transparent and partial-alpha pixels

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::rasterizer_alpha_sample_policy_study`、`fixtures/alpha-resize-suite-fixture.png`、`captures/rasterizer-alpha-sample-policy.png`。
- **Fixture:** 480x320 generated RGBA chart, SHA-256 `2b5151918a25d568dca0b3cdec8ded93c6df92c6324fbf95a74e15713f660720`; 1/2/4/8px opaque-color gaps, 1px bars, 22 varied-color translucent curved strands, hostile magenta/green RGB below alpha0. `toQImage` boundary proxy zeros hidden RGB before effect. Deterministic source, no random seed.
- **Policies:** current radial sample (R outward/B inward); alpha0-fallback to destination channel; nonopaque-fallback for any sampled alpha<1; coverage-blend `dst + sampleAlpha*(sample-dst)` independently for R/B. All retain destination alpha. R/B shifts 2/4/8 px about center. Edge ROI = binary-alpha boundary union 7x7 dilation of partial-alpha pixels; separate strand ROI y=205..299 and 1/2/4/8px gap ROIs. Images composited over black/white in normalized sRGB code values.
- **判定:** no approved target/look, acceptance threshold, human reviewer, or selected policy. Report source-sample category and candidate-vs-current appearance deltas only. This is a source-derived CPU model, not C++/GPU execution. Temporal is N/A for still chart; runtime/performance, descriptor matrix, HDR, OCIO, preview/export parity are unverified.

### 観測

| R/B | red sample alpha0 / partial / opaque | blue sample alpha0 / partial / opaque | alpha0 fallback vs current edge MAE | partial fallback vs current edge MAE | coverage blend vs current edge MAE | current vs no-shift edge MAE |
|---:|---:|---:|---:|---:|---:|---:|
| 2/2 | .285 / .526 / .190 | .204 / .540 / .256 | .02510 | .02830 | .02613 | .02830 |
| 4/4 | .379 / .463 / .158 | .305 / .470 / .224 | .03667 | .04129 | .03847 | .04322 |
| 8/8 | .500 / .358 / .141 | .464 / .371 / .165 | .05255 | .06127 | .05657 | .06321 |

- As amount grows, the current radial field samples source pixels with alpha0 or partial coverage more often on the edge ROI. The 8px setting reads alpha0 for about half of red and 46% of blue edge samples; fewer than 17% of blue samples are opaque.
- At R8/B8 the current effect changes edge composite from no-shift by .06321 MAE. The three reference policies differ from current by .05255-.06127; this magnitude means they materially redefine the look on this adversarial chart. It does not tell which output is better.
- Gap ROIs are direction/geometry dependent: for current effect at R8/B8, no-shift MAE is .13980/.03887/.15264/.10444 for 1/2/4/8px gap regions; policy deltas also vary with shift and gap. On strand ROI at R8/B8, alpha0 fallback, partial fallback, coverage blend differ from current by .03358/.04774/.04011 MAE. Black and white code-domain composites yielded same per-pixel RGB-delta metric because output alpha is held fixed; they are a composite check, not independent evidence.
- Post-boundary hidden-RGB pair max RGB difference is exactly 0 for all shifts because alpha0 RGB is normalized before sampling. Alpha remains destination alpha in every policy. Contact sheet reviewed as diagnostic, not as quality verdict.

### 判断・次

- A single `alpha > 0` gate is not neutral: it replaces sampled channel values across broad portions of edge support and can remove intended chromatic displacement. Partial-alpha fallback and coverage blend differ even more on this fixture. Keep the candidates as distinct artistic responses; do not fold them into a generic “alpha-aware fix.”
- **次:** compare the same policy matrix on the licensed clover and at least one distinct permitted feather/fur/hair cutout; inspect 1:1 edge crops and identify which effects are localized to gaps versus translucent strands. Then trace direct-float callers (video/text) separately. No Core change from synthetic output alone.

## 2026-11-11: Alpha sample policy responses vary across natural cutouts

### 再現条件と計画

- **関連:** `experiments/RetroImageLab/retro_lab.py::natural_alpha_sample_policy_study`、`eval-inputs/cc0-white-clover/provenance.json`、`captures/natural-alpha-sample-policy.png`。
- **入力:** OpenGameArt “Clover Cutouts - Improved and Expanded!” のCC0パックから5枚。元ZIP SHA-256 `44ceed28b10eceee48cebe563bfb9f3c4bcf7a8e1617858c2f6b88cfe41ec69b5`。由来・許諾記録は各素材のprovenance JSONを参照。全5枚が同じ植物素材パックで、独立した異種素材群ではない。
- **手順:** RGBA8値を正規化し、alpha=0画素のRGBを0にしてImageLayer `toQImage`境界のモデルとする。Artifact Rasterizer由来のnearest/clamp radial座標場でR/Bを各2/4/8pxずらす。現行channel sample、透明sample時の出力channel fallback、非opaque sample時のfallback、sample alphaによるcoverage blendを比較。alphaは元の出力位置の値を維持する。目視シートは黒背景、縮小表示。R8/B8のedge ROIとpartial-edge ROIで、候補と現行の合成RGB MAE/p95を素材ごとに計測する。
- **判定:** 承認済みreference/lookも許容閾値もない。差分は画質順位でなく見た目の変更量。Python/Pillowモデルのみで、C++/GPU/OIIO/OCIO runtime、preview/export parity、性能は未検証。色は正規化RGBA8 code値のalpha合成。

### 観測（R8/B8）

| 素材 | edgeでの赤sample alpha=0率 | 青sample alpha=0率 | alpha0 fallback vs 現行 edge MAE | partial fallback vs 現行 edge MAE | coverage blend vs 現行 edge MAE |
|---|---:|---:|---:|---:|---:|
| `clover_close.png` | .2130 | .0076 | .01173 | .02601 | .01820 |
| `clover_close2.png` | .2430 | .0047 | .01349 | .02863 | .02070 |
| `clover_full.png` | .2231 | .1719 | .02198 | .02699 | .02468 |
| `trifolium_repens_macro.png` | .2611 | .0456 | .02077 | .02597 | .02473 |
| `vegetation_clover_02a.png` | .2642 | .0718 | .02291 | .02493 | .02364 |

- 全5枚で赤側R8 sampleの約21–26%はalpha=0だが、青側は約0.5–17%と素材形状ごとの差が大きい。alpha0 fallbackのedge差分も.0117–.0229、partial fallbackは.0249–.0286、coverage blendは.0182–.0247で一定の順序にはならない。
- `vegetation_clover_02a.png` のsource alpha0 pixelの99.6%に非黒hidden RGBがあるが、これは境界モデルで正規化されてからsamplingされる。上記候補差分はhidden RGB差の測定ではない。
- Contact sheetでは近接葉のR8候補差は控えめに見える一方、全草体・花・別葉形でも同一差分とは限らない。縮小シートなので細い縁の1:1品質判定には使わない。

### 判断・次

- 素材単位でedge応答を記録する必要がある。alpha0/partial fallbackは相互にも現行にも異なる応答で、単一のalpha-aware方針へまとめる根拠はない。
- **次:** 植物以外の許諾明確な羽毛/毛/髪の透過カットアウトを追加し、同じ測定と1:1 cropを比較する。多様な素材で安定した見た目目標が定まるまではCoreへ反映しない。

## 2026-11-11: Feather cutouts separate hard-edge and translucent response

### 再現条件と計画

- **関連:** `retro_lab.py::natural_alpha_sample_policy_study`、`eval-inputs/cc0-feathers/provenance.json`、`captures/natural-alpha-sample-policy.png`、`captures/feather-alpha-sample-policy-metrics.txt`。
- **入力と出所:**
  - `feather_simple_sprite.png` — cat5isalive, OpenGameArt [Feather](https://opengameart.org/content/feather), CC0、150×162 RGBA、SHA-256 `2f5553d69c5930a042260fa9f293648ad59b0ff3777e561a1d1c1b3352df6379`。
  - `feather_silver.png` — way-21, OpenGameArt [Silver Feather](https://opengameart.org/content/silver-feather), CC0、500×500 RGBA、SHA-256 `f24d2454ffa602c0bff531d0679432ce2a6e90f328293651349f5f756ba31ffd`。出所ページにあるクレジット “xavier sautereau” もprovenanceに保持。
  - `feather_pixel_26.png` — Hiross, OpenGameArt [Simple Feather](https://opengameart.org/content/simple-feather), CC0、128×128 RGBA、SHA-256 `2a747dc134458077706460fbcce1d78e893866de954c5c52bd48946b140c17be`。ページの説明は26×27pxのピクセルアート。実ファイルは複数アイコンを含むシートで、自然画像ではなく輪郭ストレス素材として扱う。
- **手順:** CC0素材のalpha=0 RGBを0にした`toQImage`境界モデルを入力とする。現行radial R/B samplingとalpha0 fallback、nonopaque fallback、coverage blendをR2/4/8で比較。destination alphaを保持し、edge ROIで候補対現行の合成RGB MAE/P95を黒/中灰/白背景について併合測定。合成と統計はRGBA8 normalized code-value domain。画像はR8/B8出力を黒背景合成し、縮小接触シートで確認。
- **評価計画上の限界:** 承認済み目標・判定者・閾値がないため合否判定なし。候補差分は画質評価ではなく見た目応答の記述。RGB/alpha descriptor、sRGB/linear運用、HDR、Artifactの実C++/GPU/preview/export経路、時間安定性、性能/メモリは未検証。静止画像のためtemporal評価は対象外。全体は縮小sheetで確認し、銀色羽根のfiber detailは下記の1:1 cropで別途確認。

### 観測（R8/B8）

| 素材 | edge red sample alpha=0 | edge blue sample alpha=0 | alpha0 fallback vs current edge MAE / P95 | partial fallback MAE / P95 | coverage blend MAE / P95 |
|---|---:|---:|---:|---:|---:|
| `feather_pixel_26.png` atlas | .4953 | .4225 | .04163 / .38754 | .04914 / .40941 | .04622 / .40326 |
| `feather_silver.png` translucent feather | .0609 | .0178 | .00042 / .00000 | .02070 / .12500 | .00301 / .01569 |
| `feather_simple_sprite.png` hard-edge icon | .7407 | .0417 | .00069 / .00000 | .00069 / .00000 | .00069 / .00000 |

- Silver feather is the only input here with extensive semitransparent feather fibers (68,543 partial-alpha pixels). Its alpha0 fallback is nearly identical to current by the aggregated metric, but partial fallback changes the boundary substantially more; coverage blend is intermediate. The contact sheet also shows the partial-fallback rendition suppresses much of the red/blue fringe over those semitransparent strands. This is an observed response, not proof that less chromatic shift is preferable.
- The pixel-art atlas has no transparent-edge photographic content and its large candidate deltas largely reflect the deliberately sharp small sprites under an 8px shift. Its P95 is high even though the metric is aggregated over all pixels/backgrounds; this reinforces retaining tail values with the mean.
- The hard-edge icon has no partial alpha. Despite a high rate of alpha0 samples in red direction, the three candidate edge MAEs are all .00069 and P95 is zero at R8/B8, because the affected color changes contribute little under the destination alpha/ROI aggregation. Sample-alpha frequency alone therefore does not predict visible difference magnitude.
- **等倍詳細:** `--natural-alpha-sample-detail eval-inputs/cc0-feathers --natural-alpha-detail-asset feather_silver.png --natural-alpha-detail-box 130 120 220 220` writes `captures/natural-alpha-sample-detail.png`. The crop is taken after calculating each full-frame R8/B8 sampler field, so cropping does not reset the effect center. At 220×220 source pixels, over black/gray/white, the crop MAE is .000069 for alpha0 fallback, .022491 for partial fallback, and .002588 for coverage blend; P95 is respectively 0/.135640/.013548 on each fixed-alpha code-value composite. In 1:1 visual review, partial fallback removes much of the cyan/magenta fiber fringe, while current and alpha0 fallback largely overlap; coverage blend retains a weaker fringe. This describes the candidates and is not a quality ranking.

### 判断・次

- The feather set confirms that alpha0 frequency alone is a weak proxy for visible change: semi-transparent strand structure and destination alpha materially affect the output. Keep policy responses separate; do not select one globally from these descriptive metrics.
- **次:** Add one source-documented fur/hair cutout with a clearly permitted alpha matte. Only after setting a review question and acceptance threshold should this be used to propose a Core behavior change. The code now loads the explicit files listed in each corpus `provenance.json`, validates per-input SHA-256 when present, and can produce a pixel-exact crop panel for a specified input ROI.

## 2026-11-11: Grain dominates the high-frequency spectrum on a hair texture crop

### 再現条件と計画

- **目的・対象:** 4つのRetroImageLab静止画プリセットが細い髪筋状のテクスチャをどの空間周波数帯へ再配分するかを記述し、grain寄与をgrain-off対照から分ける。画像は人物のhair alpha matteではなくopaqueな手描き/texture素材。
- **入力:** kleenstudios, OpenGameArt [Hair Texture](https://opengameart.org/content/hair-texture-0), CC0。`eval-inputs/cc0-hair-fur/hair_fur_texture.png` 1024×1024 RGBA、alpha unique value count=1 (fully opaque)、SHA-256 `deb0bc3a37ee22f57512dfba65806a5c3853cd7b13e9dea46a985f4cd5ece691`。`eval-inputs/cc0-hair-fur/provenance.json` と `captures/hair-fur-suite-manifest.json` に由来/hashを記録。
- **条件・手順:** suite presetsの固定設定、seed 7。256×256の3 ROI（primary=(320,320)、upper-left=(64,64)、lower-right=(704,704)）。linear sRGBの係数0.2126/0.7152/0.0722でluminance化し、平均除去＋2D Hann windowのFFTを取る。`rfft2`片側面はHermitian対称性に従いDC/Nyquist X列を1、その他X列を2倍して全周波数面のpowerへ補正。radial frequency(cycles/pixel)をlow [0,.08)、mid [.08,.20)、fine [.20,.35)、very-high [.35,.5]に集計し、full-plane windowed power、band fractions、linear-luminance variance、gradient energy、baseline normalized-spectrum L1を記録。directional anisotropyは.08–.5 cycles/pixelで`abs(sum(P*exp(2iθ))/sum(P))`、併せてfrequency-axis dominant angleを度数で記録。strand-continuity diagnosticは、各ROIのbaseline gradient magnitude P90をfixed thresholdにし、baseline dominant frequency normalから±22.5°に入るgradient pixelsを列方向runとして測る。run lengthと8px以上run内のpixel比を記録する。これは線維追跡やhair strand segmentationではない。各presetはgrain=0対照とfixed-preset actualを測定。JSONは `captures/texture-frequency-suite-metrics.json`、3 ROI×baseline/actualのsheetは `captures/texture-frequency-suite-rois.png`。元画像hashは `captures/hair-fur-suite-manifest.json`。
- **判定:** 意図された出力の承認reference/判定者/合格閾値はないため、統計は質感再配分の記述であり画質順位でない。FFTは3固定ROI・single-channel linear luminanceで、非定常画像をHann windowで扱う近似。RGB/chroma別、他ROI/解像度、preview/export、GPU/backend、時間安定性、性能/メモリは未検証。Alphaは全opaqueなので境界評価は対象外。

### 観測

- **測定補正:** 前版の実装は`rfft2`片側powerをそのままradial bandへ合計していた。Hermitian対称な負X側のpowerが抜けるため、前版のexperiment 053の帯域fraction表は superseded であり引用しない。下表とJSONはone-sided X weightsで再計算した値。

| ROI | Baseline directional anisotropy / frequency-axis | handheld grain-off / actual high-band | arcade grain-off / actual | print grain-off / actual | vhs grain-off / actual |
|---|---:|---:|---:|---:|---:|
| primary (320,320) | .8606 / 0.2° | .1604 / .3007 | .2392 / .3959 | .3061 / .5547 | .1385 / .5993 |
| upper-left (64,64) | .8857 / 0.5° | .1498 / .3058 | .2328 / .3790 | .2669 / .5448 | .1147 / .5815 |
| lower-right (704,704) | .8780 / 0.3° | .1705 / .2864 | .1872 / .3819 | .2477 / .5062 | .1229 / .6078 |

High-band fraction is fine + very-high. Across the three ROIs, the baseline has a strongly horizontal frequency axis (consistent with vertically aligned strands). Actual preset directional anisotropy falls to .284/.318/.418 for handheld, .035/.031/.085 for arcade, .044/.034/.048 for print, and .033/.011/.029 for VHS. Grain-off anisotropy remains higher in some cases (handheld .549/.682/.662; exact values in JSON), so both spatial resizing/quantization and grain affect orientation concentration.

- Baseline-normal gradient energy fraction is .829/.850/.835 for the three ROIs. With grain enabled it falls to .406–.460 handheld, .325–.336 arcade, .294–.316 print, and .293–.311 VHS. On the primary ROI, column-run mean length / share of aligned edge pixels in runs ≥8 is baseline 3.67/.481; handheld grain-off 8.39/.747 vs actual 1.68/.109; arcade .1.31/.068 vs 1.21/.000; print 1.70/.143 vs 1.33/.000; VHS 1.60/.128 vs 1.31/.000. The increase in grain-off handheld run length likely reflects nearest-neighbor block expansion; it is not evidence of original strand recovery. Across ROIs, all actual presets reduce the ≥8 column-run share to ≤.124. Treat this as a fixed-threshold directional-contiguity diagnostic, not strand identity or quality score.

- Actual preset high-band fraction exceeds its grain-off counterpart in all 12 ROI/preset comparisons. The gap remains strongest for VHS (~+.46 to +.49) and print (~+.26 to +.28). This means high-frequency-energy increase cannot be interpreted as preserved hair strands: seeded grain supplies much of that band energy.
- Primary-ROI actual linear-luminance variance remains handheld .0000467, arcade .0000083, print .0000336, VHS .0000313; these low-level scalar values depend on this dark input and are not cross-content quality scores.
- Whole crop RGB RMSE is handheld .04040, arcade .02114, print .03884, VHS .03769; changed-pixel ratio is 99.75–100%. These are effect-size descriptors. Whole-image RMSE from the corpus runner is .03985/.02052/.03864/.03701 respectively, alpha changed-pixel ratio 0 for all four.
- The 2x nearest detail sheet shows handheld's coarse resampling and palette create visible vertical block structure; arcade keeps a denser pattern; print and VHS add fine-grained texture. The frequency-axis measure shows whether energy is directionally concentrated, but the broad band aggregates do not establish strand correspondence; the three ROI differences and diagnostics cannot rank these looks.

### 判断・次

- For hair/fur imagery, compare directional structure and strand continuity separately from isotropic grain. Keep grain-off as a diagnostic ablation; it is not a candidate preset by itself.
- **次:** repeat on a second independently sourced hair/fur texture and test a second resolution; validate this thresholded run proxy against manually identified strand segments before using it to compare candidate quality. Establish a visual target and reviewer/threshold before considering recipe or Core changes. No Core change from this single opaque texture study.

## 2026-11-11: Grain also raises high-frequency opponent-chroma energy

### 再現条件と計画

- **関連:** `retro_lab.py::texture_frequency_suite_study`、experiment 053と同じCC0 Hair Texture、`captures/texture-frequency-suite-metrics.json`、`captures/texture-frequency-suite-rois.png`。
- **目的:** 053のsingle-channel luminance解析を補い、色の変化とchromatic high-frequency textureを輝度・粒状感と分けて記述する。これは承認された色目標に対する画質判定ではない。
- **条件:** 同じ入力hash `deb0bc3a37ee22f57512dfba65806a5c3853cd7b13e9dea46a985f4cd5ece691`、同じ3つの256×256 ROI、4 preset、seed 7を使用。各RGB出力をlinear sRGBへ変換し、opponent axesをR−GおよびB−(R+G)/2として計算。各軸のmeanを除去してHann-window FFTし、Hermitian補正後の合算powerからfine+very-high (0.20–0.5 cycles/pixel)割合を求める。chroma delta RMSとP95はsource baselineとの差を同じlinear opponent axes上で測る。luminance deltaはlinear luminance MAE/P95。
- **評価計画上の限界:** opponent axesはlinear RGB上の記述量であり、CIELAB/ΔEや知覚均等な色差ではない。入力はopaqueで暗い手描きtexture一枚のみ。判定者、目標画像、受入閾値、RGB素材・肌色・彩度の異なる素材、display transform、実製品preview/export/GPU経路、性能は未検証。結果はプリセット順位・色品質の合格を示さない。

### 観測

3 ROI平均（chroma high-band fractionはgrain-off / actual、他はactual）。

| Preset | Chroma high-band fraction off / actual | Opponent chroma RMS | Chroma delta RMS vs source | Linear luminance delta MAE vs source |
|---|---:|---:|---:|---:|
| handheld | .1642 / .2611 | .01176 | .00801 | .00404 |
| arcade | .2842 / .3349 | .00687 | .00314 | .00189 |
| print | .3413 / .4184 | .00988 | .00610 | .00410 |
| VHS | .2163 / .4704 | .00796 | .00428 | .00381 |

- Actual outputのchroma high-band割合は全presetでgrain-offより上昇した。増分はhandheld +.0969、arcade +.0507、print +.0772、VHS +.2541。VHSの増加が最も大きいが、これは色ノイズの量を示す記述値で、髪色の忠実度や視覚品質ではない。
- Chroma deltaとluminance deltaはプリセットごとに異なり、luminance統計だけでは色応答を表せないことを確認した。現状の固定ROI/単一入力で見た差であり、素材一般へは外挿しない。
- 指標定義はJSONと画像sheetに保存。Contact sheetはnearest-neighbor拡大表示用で、色差の目視合否に使用していない。

### 判断・次

- 周波数解析ではlinear luminanceとopponent chromaを別々に保持する。高周波比率だけを増やすことを細部保持とは扱わない。
- **次:** 第二の独立CC0髪/毛テクスチャと別解像度で再測定し、色応答は複数の明度・色相を持つ素材で確認する。人の目標とレビュー基準が定まるまでpreset/Core変更は提案しない。

## 2026-11-11: Hair-texture chroma response replicates across a second source and ROI scale

### 再現条件と計画

- **関連:** `retro_lab.py::texture_frequency_suite_study`、experiments 053/054、`eval-inputs/cc0-hair-fur/hair_0_dugi.jpg`、`eval-inputs/cc0-hair-fur/provenance.json`、`captures/hair-frequency/dugi/`、`captures/hair-frequency/dugi-128/`。
- **入力と由来:** Dugi, OpenGameArt [Hair texture](https://opengameart.org/content/hair-texture), direct file `https://opengameart.org/sites/default/files/hair_0.jpg`; source page lists GPL-3.0/GPL-2.0/CC0 as selected license alternatives, and says the 1024×1024 seamless brown human-hair texture was made from the creator's own hair photo. Downloaded JPEG SHA-256 `14a6a0fb8e28a3950c48d2f7d99ce8113a578fba42e96686f136d5de79feb82a`; source hash, dimensions, origin, and license choice are recorded in local provenance/manifest.
- **条件:** Prior 256×256 crop protocol (primary=(320,320), upper-left=(64,64), lower-right=(704,704)), then repeat with 128×128 ROIs at the same top-left coordinates. Same four fixed presets, seed 7, same linear luminance/opponent chroma metrics and grain-off ablation. Reproduce from the lab directory with `python retro_lab.py eval-inputs/cc0-hair-fur/hair_0_dugi.jpg --texture-frequency-image eval-inputs/cc0-hair-fur/hair_0_dugi.jpg --texture-frequency-box 320 320 256 256 --seed 7 --output captures/hair-frequency/dugi/analysis-output.png` and change the box dimensions/output folder to `128 128` / `dugi-128` for the second run. Captures are separated by source and ROI size; full-frame ordinary output was removed after each run.
- **評価計画上の限界:** JPEG source is compressed; input shows a photographed brown hair patch and is not a matte or strand-labeled reference. The 128/256 results are different spatial supports and frequency-bin resolutions, not direct scale-equivalent measures. No approved look, reviewer, pass threshold, other hues/bright hair, preview/export/GPU path, temporal test, or performance measurement. No perceptual color difference is claimed.

### 観測

3 ROI means; luminance and chroma high-band values are grain-off / actual, followed by actual opponent chroma delta RMS vs source and linear-luminance delta MAE. These are descriptive metrics, not preset rankings.

| ROI size | Preset | Luma high band off / actual | Chroma high band off / actual | Chroma delta RMS | Luma delta MAE |
|---|---|---:|---:|---:|---:|
| 256² | handheld | .1102 / .2293 | .1229 / .1488 | .01591 | .00771 |
| 256² | arcade | .1501 / .1975 | .2432 / .2597 | .00848 | .00650 |
| 256² | print | .0978 / .2723 | .2806 / .3180 | .01184 | .00929 |
| 256² | VHS | .0537 / .3192 | .1275 / .2267 | .00891 | .00876 |
| 128² | handheld | .1527 / .2764 | .1535 / .1834 | .01568 | .00740 |
| 128² | arcade | .1994 / .2544 | .2978 / .3148 | .00839 | .00635 |
| 128² | print | .1421 / .3179 | .3303 / .3683 | .01173 | .00921 |
| 128² | VHS | .0759 / .3851 | .1556 / .2678 | .00909 | .00862 |

- Actual chroma high-band fraction exceeds grain-off at both ROI sizes for all four presets. The 256² increase is +.016–.099; at 128² it is +.017–.112. This repeats the direction of experiment 054 on a second independent hair photo, while the magnitude varies by content and ROI scale.
- 256² to 128² changes the absolute band fractions for every mode. That is expected to depend on support, window, and frequency sampling; do not compare these numbers as if the band estimator were resolution invariant.
- The baseline directional anisotropy varies .152–.433 among the 256² ROIs, unlike the strongly aligned painted texture in experiment 053. This is evidence the prior continuity proxy's baseline axis and thresholds are content-sensitive; low run support after presets cannot be treated as a universal hair-retention score.
- The actual-output contact sheet was visually inspected at overview scale. It shows handheld's coarse palette blocks against finer arcade/print/VHS texture; this is a descriptive visual observation only, without a target or quality verdict.

### 判断・次

- Across two distinct hair-texture sources, enabled grain raises measured high-frequency chroma energy. The amount is content-dependent; keep chroma and luminance analysis separate and retain grain-off ablation.
- The fixed P90 directional run proxy is not calibrated across sources: baseline alignment strength and run statistics differ materially. Do not use it to accept/reject a preset without labeled strand segments and a broader validation set.
- **次:** Add a brighter/non-brown source or an authored multicolor hair chart, then manually label local strand directions in selected crops; use that to assess whether a more stable directional metric is feasible. No recipe/Core change follows from these two textures.

## 2026-11-11: Strand-run proxy is orientation-biased and its adaptive threshold admits grain

### 再現条件と計画

- **関連:** `retro_lab.py::texture_frequency_suite_study`、`fixtures/directional-strand-control.png`、`fixtures/directional-strand-control.json`、`captures/hair-frequency/directional-control/texture-frequency-suite-metrics.json`、experiments 053/055。
- **目的:** P90 threshold・列方向run proxyが、既知の線方向で何を測るかを較正する。併せてdominant-frequency normalからstrand axisを推定し、二値edge maskを垂直方向へ回転整列する候補を比較する。
- **入力:** 合成1024×1024 RGB chart、SHA-256 `7b43194a7c04f42c1794d3ff79d77c0d6498afa0562ae49404117553ee4b295a`。暗灰色背景に2px幅/高コントラストの平行線を3つの256² ROIへ配置。primary=(320,320)は縦線・周期12、upper-left=(64,64)は横線・周期12、lower-right=(704,704)は+45°斜線・周期17。乱数なし。生成条件とライセンス不要のsynthetic controlであることはfixture JSONに記録。
- **手順:** seed 7、4 preset、既存の固定baseline-P90/方向±22.5° edge maskを測定。新しい方向整列版ではdominant frequency normalから得たstrand angleを垂直化する回転をOpenCV nearest-neighborで適用し、8px以上のaligned run内pixel比を算出。別案として各出力自身のgradient P90を使うrelative-threshold runも記録する。現行固定列・fixed baseline threshold・回転aligned・per-image thresholdを相互比較する。再実行コマンド: `python retro_lab.py fixtures/directional-strand-control.png --texture-frequency-image fixtures/directional-strand-control.png --texture-frequency-box 320 320 256 256 --seed 7 --output captures/hair-frequency/directional-control/analysis-output.png`。
- **限界:** 強い高コントラスト直線だけの幾何学controlで、実毛の毛束/交差/曲率/blur/alphaを再現しない。rotation clipping・nearest-neighbor aliasingがある。承認look、human evaluator、閾値、製品実行経路、性能は未評価。

### 観測

| ROI / known strand direction | Baseline frequency normal | Original fixed-column ≥8 ratio | Rotated fixed-threshold ≥8 ratio |
|---|---:|---:|---:|
| primary / vertical | 0.0° | 1.000 | 1.000 |
| upper-left / horizontal | 90.0° | 0.000 | 1.000 |
| lower-right / +45° diagonal | 135.0° | 0.000 | 0.729 |

- 固定列runは縦線のみを長runとして測定する。推定strand axisへ回転すると横線も1.0になるが、斜線は0.729に留まる。方向差を修正できる一方、warp/aliasingで完全一致しない。
- Preset outputsではbaseline P90固定thresholdのrun比がほぼ消えた（全ROI/presetで0–.022）。細線のgradientがthreshold未満に落ちたためで、連続性が消えたのかedge強度だけが落ちたのかをこの値だけでは分離できない。
- Per-image P90 thresholdは適応したが、grain/quantization edgeをrunとして誤計上した。例: horizontal chart arcade=.853、diagonal chart VHS=.387、対して同じ方向の baselineは1.000/.729。threshold再正規化だけでは解決しない。
- 同じ追加軸を Dugi / kleenstudios の実毛textureにも適用した。baselineのfrequency normalは両素材の3 ROIで約0°に揃い、rotated/fixed proxyは元の固定列値に近い（Dugi .066–.147、painted texture .448–.496）。これはその素材の多くの毛筋が縦向きであったためで、一般化証明ではない。
- **判断:** 回転整列版は幾何学方向バイアスを減らす診断だが、threshold/信号強度依存は残る。per-image P90値はgrainで偽陽性になるため採用判定には使わない。どちらも品質スコアやstrand correspondence metricとしては未成立。

### 次

- Experiment 061で実素材ROIの概略方向ラベルを付けたが、pixelwise centerline/幅/可視性annotationではない。次はsmooth hair, crossed strands, low contrast, grainを含む実画像cropで個別strand centerline/edge幅/欠落/false-positiveを手動labelし、閾値を候補比較前に固定する。それまでは研究記述専用とし、preset/Coreに反映しない。

## 2026-11-11: Multicolor strand chart separates hue loss from source-coordinate displacement

### 再現条件と計画

- **関連:** `tools/multicolor_strand_study.py`、`fixtures/multicolor-strand-control.png`、`fixtures/multicolor-strand-control.json`、`captures/multicolor-strand/multicolor-strand-metrics.json`、`captures/multicolor-strand/multicolor-strand-sheet.png`。
- **目的:** 赤/緑/青/黄/マゼンタ/シアンの細い線束で、レトロpresetの色分離と線幅依存を記述する。位置ぴったりの色サンプルと、横色ずれを部分的に許す局所サンプルを並べ、色変化とsampling alignmentの影響を混同しにくくする。
- **Fixture:** deterministic 768×384 RGB opaque chart、SHA-256 `6c3fe9f99c9d199448b1521d8fadc6e5f33e81ea7d67ae77e9ee8a22ee916ec8`。背景RGB 22、6色を8px周期で縦線として配置、各線幅1/2/4pxの3行。alphaは適用外。外部素材ではない。
- **手順:** `python tools/multicolor_strand_study.py`をラボrootから実行。seed 7でbaseline、全4 actual preset、各preset grain=0対照を測定。linear sRGBをopponent axes (R−G, B−(R+G)/2)へ変換。各色・線幅ごとに source-position line pixels を測る厳密samplingと、同じsource line sampleごとに±2pxのx内でlinear luminance最大pixelを取るlocal-peak samplingを比較。6クラスのsource-center nearest-color identity accuracy、6色のpairwise opponent-chroma距離median保持率・最小距離、各色内RMSを保存する。これはΔEや知覚均等色差ではない。
- **限界:** 人工の単純な縦線・高彩度・暗背景のみ。local-peakは空間ずれを部分補正するがgrain/noiseを選ぶ可能性がある。チャンネルごとの動きやline correspondenceを分離していない。承認target、reviewer、受入閾値、実毛・自然色・alpha・別背景、GPU/preview/export、temporal、性能評価は未実施。スコアはpresetの画質順位ではない。

### 観測

Local-peak samplingのgrain-off / actualを「pairwise chroma distance retention / nearest source-color accuracy」で示す。1.0 accuracyは、このサンプル群が6つのsource色中心に最近傍分類された割合である。

| Preset | Width | grain-off retention / accuracy | actual retention / accuracy |
|---|---:|---:|---:|
| handheld | 1px | .205 / .833 | .179 / .709 |
| handheld | 2px | .934 / .833 | .799 / .751 |
| handheld | 4px | .908 / 1.000 | .916 / .970 |
| arcade | 1px | .422 / .417 | .422 / .392 |
| arcade | 2px | .380 / .333 | .381 / .309 |
| arcade | 4px | .796 / 1.000 | .764 / .993 |
| print | 1px | .991 / 1.000 | .981 / 1.000 |
| print | 2px | 1.083 / 1.000 | 1.046 / 1.000 |
| print | 4px | 1.088 / 1.000 | 1.069 / 1.000 |
| VHS | 1px | .626 / .333 | .538 / .326 |
| VHS | 2px | .626 / .333 | .544 / .321 |
| VHS | 4px | .967 / .833 | .923 / .760 |

- Baselineは全幅でretention/accuracy 1.0。printは全幅で色class identityが1.0、distance retention .981–1.069。1.0を超える距離はsource色中心間隔より測定値が広がっただけで、品質向上とはしない。
- 厳密source-position samplingは1px handheld/arcade/VHSでaccuracy .167–.500へ低下したが、局所ピークではhandheld .709、arcade .392、VHS .326。位置一致だけの測定が横ずれを色損失として数える一方、局所peakもgrainに敏感であり、両方の読み分けが必要。
- 4px線ではhandheld/arcadeのlocal-peak accuracyが.970/.993まで上がるが、VHSは.760。1–2px線と4px線で色分離が異なる。線幅条件を記録しない色統計はこの差を隠す。
- Grain-onとoffではhandheld 1px accuracy .833→.709、VHS 4px .833→.760へ低下。seed付きgrainはこのfixtureでclass中心付近の分布も広げる。これは現行settingでの測定で、主観的なgrainの良し悪しではない。
- 比較sheetをoverviewで確認。handheld/arcadeの細線でsource maskと出力の一致が弱まり、VHSで色 fringeが見える。ただし縮小表示であり、目視合否ではない。

### 判断・次

- 色を維持できたかを測る時はsource-coordinate-only値と小さい局所登録値を併記する。局所最大値だけではgrain/noiseのfalse positiveを含む。
- **次:** 明/中間背景と交差線はexperiment 058で追加した。次は斜め/低彩度色と実素材cropの手動color labelを増やし、測定窓とサンプル選びを事前固定する。これらができるまではrecipe/Coreへ反映しない。

## 2026-11-11: Local contrast sampling handles bright backgrounds better than brightness maximum

### 再現条件と計画

- **関連:** `tools/multicolor_strand_study.py::make_context_chart` / `run_context_study`、`fixtures/multicolor-strand-context-control.png`、`fixtures/multicolor-strand-context-control.json`、`captures/multicolor-strand/multicolor-strand-context-metrics.json`、`captures/multicolor-strand/multicolor-strand-context-sheet.png`。
- **目的:** 前実験057の暗背景・独立縦線を拡張し、dark/mid/light背景、縦線、横交差線、交点で局所color-sampling法を比較する。色中心の正解は合成入力のsource hue labelsである。
- **Fixture:** deterministic opaque RGB 768×1152 chart、SHA-256 `da9dd6b54914df3ecd8a278f2116931fd89e239bc185d511222d02017985213c`。背景はRGB22/128/232。赤/緑/青/黄/マゼンタ/シアンの1/2/4pxストランド、縦周期8px・横周期32px。各交点では縦線を先に描き、横線色を上書きする。縦単独・横単独・overlap maskを分け、交点は上の横色classで照合。
- **手順:** `python tools/multicolor_strand_study.py`を再実行。seed 7、baseline、4 actual preset、grain-off controlを各背景/線幅/領域へ適用。各方向の法線に沿って、source-coordinateサンプル、local luminance maximum±2px、local contrast maximum±2pxを比較。contrastはそのストランド位置から法線方向へ4px離れたpixelをlocal referenceとし、candidateとのabsolute linear-luminance差が最大の位置を選ぶ。交点referenceは完全な背景ではなく、下側の縦色を含む可能性があるので独立集計する。
- **評価限界:** RGB opponent-axis color distance/nearest source-color classであり、ΔEでも人の見えでもない。±2px窓、4px reference、8px/32px pitchは本synthetic chart用。暗/中/明の一様背景と直交線だけで、自然な明暗、texture、低彩度、斜め・曲線、実画像、alpha、製品経路/性能は未検証。判定者・目標・閾値なし。

### 観測

以下はprint actualの1/2/4pxを平均したnearest-class accuracy / pairwise chroma-distance retention。local contrast peak samplingを用いる。

| 領域 | dark RGB22 | mid RGB128 | light RGB232 |
|---|---:|---:|---:|
| vertical strands | 1.00 / 1.03 | .83 / .81 | .96 / .89 |
| horizontal strands | 1.00 / 1.03 | .85 / .81 | .97 / .90 |
| overlap intersections | 1.00 / .96 | 1.00 / .95 | 1.00 / .94 |

- **brightest-pixel failure:** print/light背景のvertical lineで、local luminance maximumはwidth 1/2/4pxに対しaccuracy .171/.176/.207、distance retention 0/.000/.000を示した。背景が線より明るいため、選択窓が背景を選び色がcollapseする。横線でもaccuracy .167–.201、retention 0–.001。source-coordinate strict measurementではprintのaccuracyは1.0・retentionは.975–.992なので、失敗はpreset出力よりsampling ruleにある。
- contrast-peak samplingは同じprint/light vertical linesでaccuracy .919/.974/.987、retention .821/.940/.900、horizontal linesで.943/.977/.988、retention .917/.900/.894に戻る。つまり暗線/明線の両方を扱える可能性はあるが、local reference/window選択に依存し万能ではない。
- Light背景vertical arcadeではcontrast peak accuracyが線幅1/2/4で.420/.687/.879だった一方、strict positionsは.167/.718/.750。許容samplingは細線の色を拾う場合があり、同時に位置保持差の意味を変える。
- Print intersectionsはcontrast-peak identity .991–1.000。overlay ruleが既知なのでsource top-colorとの一致を測れたが、自然なalpha-overlapやsemi-transparent strandsを表さない。
- Contact sheetをoverview確認し、3背景の色見え・交差を記録。縮小画像であり品質合否ではない。

### 判断・次

- brightness maximumを色保持用の一般的な位置合わせとして使わない。local contrast peakは背景輝度差への適応候補だが、4px offset、±2px幅、交点の基準色に依存するので現在は記述用の補助値。
- **次:** 低彩度色、斜め線、部分透明度はexperiment 059で追加した。交差順の反転や複数幅の隙間、実素材の手動labelsと目標基準を調べるまではpreset/Coreに採用しない。

## 2026-11-11: Strand geometry response varies with line width, pitch, and pixel phase

### 再現条件と計画

- **関連:** `tools/multicolor_strand_study.py::make_geometry_chart` / `run_geometry_study`、`fixtures/strand-geometry-control.png`、`fixtures/strand-geometry-control.json`、`captures/strand-geometry/strand-geometry-metrics.json`、`captures/strand-geometry/strand-geometry-sheet.png`。
- **目的:** 色相変化と分けて、既知の明線の太さ、空隙、pixel-grid位置が縮小・量子化presetの細線応答へ与える影響を記録する。
- **Fixture:** deterministic opaque RGBA 768×576 chart、48 cell (64×48px)。sRGB8 line color (226,196,132)、暗背景(18,20,24)、幅1/2/3/4px、pitch 6/10/16px、位相0/0.25/0.5/0.75 pitch。すべて外部素材なし。
- **手順:** seed 7、baseline・4 actual preset・4 grain-off control。known source line maskと補集合の背景を分離し、linear-sRGB luminanceで線RMSE、背景RMSE、線対背景contrast保持率、1/255超過画素率を測る。phase感度は幅/pitch固定時の4位相間range。再現コマンド: `python tools/multicolor_strand_study.py`。
- **限界:** 垂直な一定色の直線、暗い一様背景のみ。metricはsource画像との差であり、意図されたpixel art lookへの品質評価ではない。自然なhair、斜線/曲線、alpha edge、review基準、製品/GPU経路、時間安定性、性能、受入閾値は未検証。

### 観測

- Fixture manifestとPNG SHA-256が一致。9処理条件×48 cell、各幅/pitchの4位相群108個を記録し、全4461数値がfinite。alphaは全条件で不変。
- Actual presetの全cell平均線RMSE / 背景RMSE / contrast retention / 幅-pitchごとの位相RMSE range平均:

| Preset | Line RMSE | Background RMSE | Contrast retention | Phase RMSE range |
|---|---:|---:|---:|---:|
| handheld | .31641 | .04239 | .41992 | .07207 |
| arcade | .27238 | .05087 | .52200 | .04424 |
| print | .08213 | .00545 | .98985 | .00711 |
| VHS | .20616 | .08073 | .57607 | .00505 |

- **解釈:** このchart上では線幅/周期の条件をまとめて平均すると、細線消失や背景変化を隠す。handheld/arcadeで位相に伴う線RMSE変動が比較的大きく、VHSは平均位相rangeが小さい一方で背景RMSEが大きかった。したがって単一のphase平均では効果の違いを表せない。これは固定条件の合成チャートでの記述で、presetの優劣ではない。
- Contact sheetを目視。太さ、疎密、phaseごとに細線の残り方とVHS色ずれが異なる。

### 判断・次

- 幾何学的な細線応答を比較する時はwidth/pitch/phaseごとの値を保持し、色・交差・alpha実験と混ぜない。phase sensitivityはsampling/scaleの診断候補だが、実画像のstrand tracking指標としては未検証。
- 次は、利用条件が明確な実画像cropに手動centerline/width/visibility labelsを付け、自然な髪や繊維で同じ測定が意味を持つか確認する。recipe/Coreへの採用根拠はまだない。

## 2026-11-11: Approximate strand-axis labels hold across two CC0 textures, but anisotropy does not

### 再現条件と計画

- **関連:** `tools/hair_roi_orientation_study.py`、`eval-inputs/cc0-hair-fur/provenance.json`、`captures/hair-frequency/manual-roi-orientation/manual-roi-orientation-metrics.json` と4 contact sheets。
- **目的:** 既存のsynthetic geometry findingsを2つの独立した髪テクスチャへ限定的に接続し、概略の毛筋方向がpreset後にも保たれるか、directional coherenceと高周波比率がどう変わるか測る。
- **入力/由来:** Dugi `hair_0_dugi.jpg` (SHA-256 `14a6a0fb8e28a3950c48d2f7d99ce8113a578fba42e96686f136d5de79feb82a`) と kleenstudios `hair_fur_texture.png` (SHA-256 `deb0bc3a37ee22f57512dfba65806a5c3853cd7b13e9dea46a985f4cd5ece691`)。OpenGameArt CC0選択の根拠・source URLは既存provenance JSONに記録。どちらも1024²でopaque。
- **Manual labels:** 各素材の(64,64)、(384,384)、(704,704)をupper-left/center/lower-right ROIとし、128²と256²を測定。両sourceは目視でdominant strand tangentが概ね65–115° (Dugi)、70–110° (kleenstudios)の範囲にあると記録した。独立annotator agreementはない。これはROI-level dominant axisで、strand centerline segmentationではない。
- **条件/指標:** seed 7、baseline・4 presetのgrain-off/actual。Hann windowed linear-sRGB luminance FFT、片側スペクトルをHermitian-weightし、0.03–0.35 cycles/pixel帯域のsecond angular momentからdominant frequency axis (axial 0–180°)とanisotropyを算出。high-band fractionは0.20–0.50 cycles/pixel。baseline方向との差はaxial absolute delta。source baselineは比較参照で、正解画像ではない。
- **実行:** `python tools/hair_roi_orientation_study.py`。出力sheetは256²/128²それぞれDugi・kleenstudios別。パネルはNN拡大で、差分計算は元のcrop解像度。
- **限界:** opaque texture 2点、濃色hair/手描きhair textureのみ。ROI-level目視annotationは粗く、判定者1名相当、受入閾値なし。partial alpha/hair matte、自然光色、時間安定性、Artifact preview/export/GPU、性能は未評価。数値は既存preset出力の記述で、見た目の順位ではない。

### 観測

12 ROIすべてでbaseline dominant frequency axisは、vertical strandに期待される水平法線に近く、anisotropyは0.65以上 (Dugi) / 0.92以上 (kleenstudios)。actual presetの12 ROI平均:

| Preset | Baseline axisからの角度差 mean / max | Anisotropy retention vs baseline | High-band fraction delta |
|---|---:|---:|---:|
| handheld | 1.06° / 4.19° | .643 | +.229 |
| arcade | .72° / 1.57° | .664 | +.254 |
| print | 2.63° / 6.81° | .329 | +.360 |
| VHS | 3.52° / 8.93° | .274 | +.411 |

- Directional axis stays near the source axis on this hand-labeled set, but coherence weakens: print/VHS mean anisotropy retention is .329/.274, while their high-band fractions rise +.360/+.411. Added grain and chromatic/scanline structure can raise high-frequency power while reducing dominant directional concentration; high-band increase therefore does not mean strand detail was preserved.
- Scale changes matter. For print/VHS, mean angle deviation is larger on 128² than 256² windows (3.91°/5.11° vs 1.34°/1.93°); spectral support/resolution differs, so this is a window-size sensitivity result, not proof of output scale instability.
- Contact sheets reviewed at overview size. VHS and print appearance changes are visible, but this was not a blinded quality review and no approved target exists.

### 判断・次

- For hair-texture diagnostics, keep orientation, anisotropy, high-band energy, and grain-off ablation as separate outputs. A stable dominant axis does not establish preserved individual strands; high-band energy alone is especially misleading under grain/scanline treatments.
- Next add manually traced individual centerlines and visibility/width labels on crops, with a second reviewer or repeat-label agreement, before setting any strand-continuity threshold. No Core/library change is supported yet.

## 2026-11-11: Angled translucent strands expose crossing and alpha behavior

### 再現条件と計画

- **関連:** `tools/multicolor_strand_study.py::make_alpha_crossing_chart` / `run_alpha_crossing_study`、`fixtures/multicolor-strand-alpha-control.png`、`fixtures/multicolor-strand-alpha-control.json`、`captures/multicolor-strand/multicolor-strand-alpha-metrics.json`、`captures/multicolor-strand/multicolor-strand-alpha-sheet.png`。
- **目的:** 彩度2段階、方向3種、半透明の細線交差で、各presetのalpha保持と、透明合成後の見えの差を記述する。
- **Fixture:** deterministic 768×512 RGBA chart、SHA-256 `0d7cdc4f170e63bfd99c3cce9490d6f15421049181ea6f0d63f083fb0ed868e8`。飽和色/低彩度色の2行、vertical/+45°/−30°の3列、line width 3px/pitch 24px。背景はalpha 0、base strand alpha 128、crossing strand alpha 160。交点はbaseの上へ crossingをlinear-light source-overで合成。alpha-zero領域にmagenta/green/blueの隠れRGB checkerを置く。外部素材・licenseは使わない。
- **手順:** `python tools/multicolor_strand_study.py` をラボrootから実行。seed 7、baseline、4 actual preset、grain-off controlを測定。出力をlinear-lightでsRGB-code black/mid-gray/white背景へ合成し、alpha/hidden-RGB差と、チャート全体・線・partial-alpha・境界・交点・alpha-zeroのRGB MAE/RMSE/P95/change ratioを集計する。彩度/方向ごとのcell contextも記録する。
- **限界:** 合成した単純な線・既知の交差順のみ。実毛、複雑なpremultiplied input、自然なedge coverage、human-approved target、製品経路、性能、品質閾値は未評価。統計は見えの順位や知覚品質スコアではない。

### 観測

- 4 actual presetすべてでalphaは完全一致（最大code差0、変更pixel比0）。一方alpha-zeroのhidden RGBはhandheld .993、arcade .989、print .985、VHS .996のpixel比で変化した。alpha-zero単独pixelのcomposite RGB差はblack/mid/whiteのどの背景でも0。
- alphaが完全一致するため、linear-light合成した出力RGB差は3背景で同一となった。背景を増やしたが、この条件では背景に依存する可視差は発生しなかった。
- Whole boundary+neighbor RMSEはhandheld .07995、arcade .09181、print .02356、VHS .12025。交点RMSEはそれぞれ.11281/.11898/.04217/.16375。これはbaselineとの差の大きさであり、品質や改善の順位ではない。
- VHS diagonal cellのboundary/intersection RMSEは飽和+45° .1488/.2262、−30° .1423/.2266、低彩度+45° .1145/.1642、−30° .1105/.1503。方向による差が見えるが、色・角度・source-over構成の限定されたsynthetic条件から一般化できない。
- Fixture SHA-256をmanifest記載値と照合し一致。contact sheetを目視し、6 cellと3 composite background rowを確認。

### 判断・次

- この入力ではalpha plane保存とhidden RGB変更が両立し、alpha-zero RGB変更は直接のcomposite表示差を生まなかった。partial-alphaの境界と交点にはRGB差が残るため、alpha-zeroだけの検査では透明素材の端の影響を説明できない。
- 次は利用条件が明確な実RGBA素材から、低彩度・斜め・交差領域を手動labelしたcropを作る。現状はCore実装やpreset変更の判断根拠にしない。

## 2026-11-11: A CC0 pixel-art hair sprite exposes sparse partial-alpha edge behavior

### 再現条件と計画

- **関連:** `tools/hair_cutout_alpha_study.py`、`eval-inputs/cc0-hair-cutouts/`、`eval-inputs/cc0-hair-cell-study/`、`captures/hair-frequency/hair-cell-alpha-metrics.json`、`hair-cell-alpha-policy.png`、`hair-cell-alpha-detail.png`。
- **出所:** thecilekli, OpenGameArt [LPC Summer Hair Female](https://opengameart.org/content/lpc-summer-hair-female)。配布ページはCC0を明記し、白色hair assetと4px brushを説明している。取得したatlas `summer_hair_female.png` は832×1344 RGBA PNG、SHA-256 `937588aa69a5f5b752b6eca53740706a9a8c32ff44f725705bf68ec960b7e6ff`。
- **評価crop:** 個別spriteセル `[704,1152,64,64]` を切り出し、sampler centerが評価対象spriteの中心と一致するよう別corpusへ保存。crop SHA-256 `2c4a142db933dbb6caa1d2ebbd23983eea057f5d1259e587b027f438ac3e6e2e`。64×64でalpha-zero 3334px、partial-alpha 24px、opaque 738px。
- **計画/手順:** radial RGB chromatic samplerのcurrent / alpha-zero fallback / partial-alpha fallback / coverage-blendをR/B 2/4/8pxで比較。全edgeとpartial-edge ROI、現行alphaで合成した黒/中灰/白の差を集計する。compositeはnormalized sRGB code-valueで、linear-light合成ではない。`python tools/hair_cutout_alpha_study.py`でprovenance hashを検証し、JSONと2 contact sheetsを再生成する。
- **評価範囲:** 64pxのゲーム用pixel-art hair spriteであり、写真のhair matteや個別hair strandsではない。transparent shape境界、極少数のAA coverage、skin/hair内部の描画pixelを含むが、実写の毛先やfine strand alphaには外挿しない。評価はEffect responseの記述で、承認target・判定者合意・合否閾値なし。Artifact/GPU経路とperformanceは未検証。

### 観測

- crop内のpartial-alphaは0.59% (24/4096px)に留まる一方、定義済みedge ROIは688px。R8でsource alpha-zeroを読むsample比はred .487、blue .241となり、channel directionで異なった。
- R2でalpha0/partial/coverage policyはcurrentとのedge MAEがすべて.03156、P95 .30980。R4ではedge MAE .04879/.05079/.05006、partial-edge MAE .00448/.01248/.00956 (alpha0/partial/coverage)。R8ではedge MAE .06762/.07101/.06977、partial-edge MAE .01698/.03052/.02558。
- 64×64詳細compositeでは、alpha0/partial/coverage候補のcurrentとの差MAEは.01768/.01825/.01805で、black/mid-gray/whiteで一致した。output alphaがsource alphaのままだからである。各detail P95は0だったがMAEは非ゼロで、画素差が少数pixelに集中するとP95だけでは見逃す。
- Contact sheetのnearest-neighbor拡大でsprite全体を確認。縮小表示を品質合否には使わない。
- **Atlasの評価単位に関する修正:** 最初に全832×1344 atlasを直接runnerへ渡すとradial sampler中心が全シート中央になり、個々のsprite位置からの方向/offsetを代表しない。評価対象をcell cropへ限定した。atlas全体の集計値はこの問いへの証拠に採用しない。

### 判断・次

- Sparse partial alphaでもedge ROI全体の候補差は大きくなり得るが、その多くはalpha0 hidden RGBや色shiftで、alpha coverageそのものではない。partial-edge P95=0と非ゼロMAEも両立したため、平均・分位点・局所表示を併記する。
- このpixel-art cropはchromatic-aberration alpha contractのstress inputにはなるが、より良い見た目を決めない。Core変更はしない。次は複数hair sprite形状と写真hair/featherのcontinuous alpha edgeを同条件で並べ、pixel-art用と自然素材用の契約が共通化できるか調べる。

## 2026-11-11: Chromatic RGB shift は1つのscratch planeを順次再利用できる

### 再現条件と計画

- **関連:** `tools/core_chromatic_workspace_study.py`、`captures/core-chromatic-workspace/core-chromatic-workspace-metrics.json`、`ArtifactCore/src/Graphics/Effect/ChromaticAberrationEffect.cpp`。
- **問い:** R/Bの独立shift結果を維持したまま、現行2 plane scratchから共有1 planeへ置き換えられるか。Core source上でR planeはshift後に元channelへcopyし、その後B planeを読むため、channel自体は変更されない。R/B channelはVideoFrameが個別に保持する。
- **比較:** clamp付きinteger samplingを模したfloat32 CPUモデルで、dual scratchとR-shift→copy→scratch再利用→B-shift→copyを比較。解像度1×1、1×13、17×1、17×13、256×192、640×360、1920×1080。7方向・距離のshiftを適用し、計49条件。乱数seed 20261111。
- **範囲:** RGB値の厳密一致とG保持、およびソース上のscratch capacityのみ。Pythonモデルであり、C++実行、allocator/frame-time計測、GPU path、スレッド実行性能は未評価。

### 観測とCore反映

- 49条件すべてでdual-bufferと共有bufferのRGB出力がfloat32で完全一致し、G planeも全件一致した。
- 非zero shiftではscratchのピークcapacityはpixelあたり8 bytes (2×float) から4 bytes (1×float) となり50%減る。1920×1080では16,588,800 bytes (15.82 MiB) から8,294,400 bytes (7.91 MiB)、差は7.91 MiB。値はscratch capacityだけで、frame/channel・allocator metadataを含まない。
- `ChromaticAberrationEffect.cpp` の処理を一つの `std::vector<float>` へ変更し、R shift/copy後に同じscratchでB shift/copyする。先行のzero-vector early returnは維持。画像値のC++ runtime比較やbuild/testは未実施。vectorはvalue-initializeされるため確保時のzero fillも残るが、旧2 vectorに対して一面分減る。
- RとBが独立planeである現在のVideoFrame構造では、Rの結果copyはB入力を変更しない。将来channel aliasingを導入する場合はこの前提を再評価する。
- **呼び出し経路の静的確認:** repository内で`CreativeEffectManager::applyAll`の定義と宣言は見つかるが、Artifact/Coreからの呼び出しsiteは検索で特定できなかった。したがってこのChromaticAberrationEffectの実際の呼び出し頻度・製品経路・workspace所有者は未確定。呼び出し頻度不明はhot pathとして扱う規則に従い、1 planeの確保は残したまま扱う。

### 判断・次

- この小変更はsource-orderと数値モデルが一致し、最大scratch容量を半減するためCoreへ反映した。ただし1 allocation/frameは残存するので、ホットパスのworkspace再利用完了を意味しない。頻度を見たallocator/profiler検証は未実施。
- 次は`CreativeEffectManager`の製品接続がないのか、別target/外部利用者に委譲されているのかを広いtarget mapから確認し、フレーム間workspaceの安全な所有者を決める。build/test許可がないため今回は実行しない。GPU parityや色品質の変更は加えていない。

## 2026-11-11: Core Posterize の小数Levelsとextended input domain

### 再現条件と計画

- **関連:** `tools/core_posterize_domain_study.py`、`captures/core-posterize-domain/core-posterize-domain-metrics.json` / `core-posterize-domain-sheet.png`、`fixtures/core-posterize-domain-manifest.json`、`ArtifactCore/src/Graphics/Effect/PosterizeEffect.cpp`。
- **Source equation:** `n=max(2, levels)`、各RGB値は `floor(value*(n-1)+0.5)/(n-1)`。sourceには明示clampがない。式をfloat32で模し、SDR [0,1]、negative/>1 [-.5,1.5]、HDR [0,4] ramp（各2049点）とLevels 2/2.5/3/4/4.5/8/16/64を比較。各levelでhalf-step境界の隣接float32値も調べる。
- **追加source比較:** ArtifactのsetterはLevelsをfinite [2,64]へclampするがinteger化しない。Artifact CPU Posterizeとstandalone GPU shaderは同じ非clamp式。PointwiseEffectFusion resident HLSLは `saturate(color.rgb)` を量子化前に行うため、extended inputでsource-level差がある。`SurfaceColorDescriptor`にはLinear / SceneReferred float surface契約があり、Posterize CPUはRGB値を式へ渡しdescriptorを出力へ保持する。ただし実際のPosterize callerが各descriptor/rangeを使うかは未追跡。
- **制限:** Python source-derived modelのみ。Core VideoFrameの値域、Artifact descriptor付きsurfaceの実caller範囲、実際に到達するGPU path、runtime parity、visual targetは未確定。誤差統計は同一domainの数値記述で品質評価ではない。

### 観測

- 24 ramp条件すべて単調。float32で正確に表現できるhalf-step 456点の全てで、`floor(x+.5)`側への量子化を確認した（各levelでは10–263点）。
- SDR [0,1]で整数Levels 2/3/4/8/16/64は出力range内。小数Levels 2.5では出力max 1.3333、4.5では1.1429となり、2049点中各1点 (0.0488%) が1を超えた。Levels値が非整数のまま分母に使われるため、white endpointを1に写像しない。
- 同じ小数Levelsの入力domain依存は大きい。2.5-level quantizerは[-.5,1.5] rampで[-.6667,1.3333]へ、[0,4]では[0,4]へ出力。実装はnormalized SDR専用のclampをしておらず、extended-value quantizationとしても振る舞う。
- SDR [0,1]内ではsource CPU式とresident saturate式は24条件すべて一致。extended domainでは差が出て、Levels=2.5時に[-.5,1.5]で8.35%のsampleが非一致（最大差.6667）、[0,4]で58.32%が非一致（最大差2.6667）。Levels=4.5ではそれぞれ28.60%/67.84%が非一致。これはresident式だけが入力を[0,1]へ飽和させるためで、runtimeで実際にどの経路を通るかは未確認。
- Contact sheetを確認し、橙の段階出力と入力勾配の関係を3 input rangeで表示。extended rangeは各domain内で線形表示正規化しており、viewerのHDR appearanceではない。

### 判断・次

- 小数Levelsによるnormalized endpoint overshootとextended-input route divergenceはsource factsだが、Core VideoFrameの値域、Artifact descriptor付きsurfaceの実caller範囲・許容HDR quantizationは未確定。単純clampはscene-referred値を壊す可能性があるためCore変更は保留し、仕様を推測しない。
- 次はArtifactのColorDescriptor/CPU-GPU route contractと実callerで、Posterizeへ渡るLevels・channel domain・到達shaderが定義されているかを確認する。Artifact.exe起動/build/testなしではcaller色域の実態や見た目の最終判断は未解決。

## 2026-11-11: Core OldTV scanline density はpixel row依存

### 再現条件と計画

- **関連:** `tools/core_oldtv_scanline_resolution_study.py`、`captures/core-oldtv-scanline/core-oldtv-scanline-metrics.json` / `core-oldtv-scanline-sheet.png`、`ArtifactCore/src/Graphics/Effect/OldTVEffect.cpp`。
- **Source equation:** `sl = 1 - abs(sin(sy * scanlineDensity * 2)) * 0.2`。curvature=0、flicker=0、fringe=0、constant imageに固定し`sy=y`として、240/480/720/1080/2160行でdensity 0/.125/.25/.5/.75/1を測定。CPUモデルは画素row上でsource式を評価。
- **評価:** gainのmin/max/mean、離散rowでのdark minima数、normalized-frame minima比、profileを240行へ正規化座標で線形写像したRMSEを記録。contact sheetは各profileを横へ描き、heightごとに比較。
- **制限:** source-derived 1D model。実caller、preview/final縮小、表示倍率、画質目標、他設定との結合、実行時間は未検証。normalized-y RMSEは解像度感度の記述値で、品質評価ではない。

### 観測

- 非zero densityの連続式上の絶対sin周期は `π/(2*density)` pixel rows。例: density .5は3.142 rows、1.0は1.571 rows。density 1は周期が2 pixel未満のため、実際の整数row samplingでaliasする。
- density .5の離散dark minimaは240行で76個、2160行で687個。density .125では19から172個、density .25では38から344個。同じ値のまま高さを9倍にすると、dark band数も約9倍になる。
- 240行profileをnormalized yで参照にしたRMSEはdensity .5で高さ480–2160に対し.075–.076、density .125で.078–.086。同一settingでも縦解像度が変わるとnormalized-frame内のpatternは一致しない。
- multiplier自体は常に0.8–1.0内。contrast幅は解像度で変えず、周期数と位置密度がpixel gridに追随する。
- Contact sheetを確認し、解像度に比例してnormalized frame内の走査線本数が増え、density=1では整数rowの高密度samplingになることを目視した。

### 判断・次

- `Scanline Density`がpixel pitchを指定するか、画面高さに対するline countを指定するかは名称だけでは確定できない。解像度正規化は既存lookを変えるため、現時点でCoreを変更しない。
- 次は実際のeffect適用経路と画像を表示する際のscale relationを確認する。もし解像度非依存のlookが設計意図なら、line count基準かphysical pixel pitch基準かを決めた後に仕様比較する。

## 2026-11-11: OldTVノイズ成分は異なる時間座標を同じFlicker parameterで駆動する

### 再現条件と計画

- **関連:** `tools/core_oldtv_temporal_sampling_study.py`、`captures/core-oldtv-temporal/core-oldtv-temporal-metrics.json` / `core-oldtv-temporal-sampling-sheet.png`、`ArtifactCore/src/Graphics/Effect/OldTVEffect.cpp`、`ArtifactCore/src/Math/NoiseGenerator.cpp`。
- **Source facts:** `globalNoise`は`perlin(time*50)`、row jitterは`perlin(y*.5,time*10)`、grainは`perlin(x*10,y*10,time*60)`。3振幅とも同じ`flic`値に比例し、係数はそれぞれ1, 2, .05。NoiseGeneratorは各軸のlattice indexに`floor(coord)&255`を用いる。
- **評価:** 固定cadenceを`time=n/(A/B)`としてFPS 23.976 (24000/1001)、24、25、29.97 (30000/1001)、30、50、59.94 (60000/1001)、60、120で比較。各成分のcoordinate stride/frame、連続domainの256-lattice wrap、full-coordinate wrapを保証するframe数を算出。Perlin数値や画像値は生成しない。
- **数学的条件:** temporal multiplierをk、rational FPSをA/Bとすると、coordinate wrap条件は`k*B*N/A = 256*m`。full-wrapを保証する最小整数frame数は`N=256A/gcd(kB,256A)`。spatial multiplierをa/b (lowest terms)とすると、整数pixel格子での保証repeatは`256b/gcd(a,256b)` pixel。
- **制限:** 再生clockが`n/FPS`であること、permutationが不変であることを仮定した座標分析。full 256-lattice wrapは保証周期であり、スカラーPerlin出力の最小周期を証明しない。aliasing、可視flicker、時間滑らかさ、jitter量、grain品質、製品の実time供給は未計測。

### 観測

- 24fpsではglobal flickerが2.083 lattice units/frame、row jitterが.417、grainが2.5。60fpsでは各々.833/.167/1.0。1 frameで複数latticeを進むことは疎な座標samplingの根拠だが、Perlin出力のalias/flickerが見えると断定しない。
- continuous 256-lattice wrapはglobal 5.12s、row jitter 25.6s、grain 4.267s。24fpsでfull coordinate wrapを保証するframe数はglobal/row jitterが3072 (128s)、grainが512 (21.333s)。60fpsでは1536/1536/256 frames (25.6/25.6/4.267s)。
- 23.976fps (24000/1001)ではframe strideがglobal 2.0854、row .4171、grain 2.5025 lattice units。integer frameでfull-coordinate wrapを保証するのはglobal 122,880 frames (5125.12s)、row 614,400 (25,625.6s)、grain 102,400 (4270.933s)。continuous wrap後にsample phaseがずれるため、整数FPSより保証frame periodが長くなり得る。
- 空間方向ではgrainの`x*10,y*10`が連続座標25.6pxでwrapし、整数pixel samplingではx/y各128px後にrepeatを保証する。row jitterの`y*.5`は512 rowの空間repeatを保証する。これは格子由来の保証であり、目視可能なtexture tileを観測した結果ではない。permutationによってscalar出力の基本周期がより短い可能性もある。
- 同じFlicker値がglobal brightness、行位置、grainへ異なる座標速度・振幅係数で作用するため、parameter sweepだけでは各成分を独立調整できない。
- Contact sheetはFPSとcomponentごとの座標stride/full-wrap frame countを表示。rendered frame比較ではない。

### 判断・次

- この厳密座標分析はtimebase間の実ノイズ差や視覚的flickerを証明しない。repeat保証も実際のplayback time sourceが一定cadenceの場合に限り、scalar Perlinのfundamental periodより長い可能性がある。Coreのparameter構成やnoise式は変更しない。
- 次はNoiseGenerator実装に合わせたpermutationを同じC++ toolchainから読み出せる、許可されたoffline routeがあるか確認し、同一frame sequenceのactual Core output比較を計画する。ビルド禁止のままなら、この座標表までをsource-derived evidenceとして保持する。

## 2026-11-11: OldTV channel fringe はhidden RGBをalpha境界へ持ち込む

### 再現条件と計画

- **関連:** `tools/core_oldtv_alpha_boundary_study.py`、`fixtures/core-oldtv-alpha-hidden-rgb-control.png` / `.json`、`captures/core-oldtv-alpha/core-oldtv-alpha-metrics.json` / `core-oldtv-alpha-boundary-sheet.png`、`ArtifactCore/src/Graphics/Effect/OldTVEffect.cpp`。
- **Fixture:** 同じvisible RGBAのままalpha-zero RGBだけを一様magenta/green、または対になったchecker色へ変える人工RGBA。代表controlは512×192 atlasで、256×192画像2枚を横並び（SHA-256 `18f678dde1ef7634cf3f3144e9ce4b0055995cfc7305d32f5b42ab852965f1d8`）。評価解像度128×96、256×192、512×384。外部素材なし。
- **Source equation isolation:** curvature=0、flicker=0、scanline=0でfringeだけ0/.1/.3/.5/1。`splitX=fringe*5*abs(x-cx)/cx`、R sampleは`int(x-splitX)`、Bは`int(x+splitX)`をbounds clamp。C++ castを模してtrunc toward zero。G/Alphaはpixel-localのまま。各出力pairをblack/mid-gray/whiteへsRGB code-value alpha compositeし、whole/boundary/partial-alpha ROIのMAE/P95/差分率を算出する。
- **入力契約仮定:** 透明外のRGBをstraight colorとして合成表示するモデル。VideoFrame interfaceにはこのeffect入力のalpha associationがなく、premultiplied入力の実挙動を主張しない。
- **制限:** Python source-derived model。Core runtime、実caller、alpha association、GPU経路、見た目合否、性能は未検証。数値差はhidden-RGB感度の記述であり、望ましい色縁や品質の順位ではない。

### 観測

- 30条件すべてでvisible source RGB pairは一致し、出力Alphaも完全保持（max code差0、変更pixel比0）。fringe=0のhidden-pair composite差は全背景・ROIで0。
- 256×192/uniform hidden pair、fringe=.3ではboundary MAE .00839、P95 .01029 normalized code。mid-grayで閾値1/255超のpixelはboundary ROIの21.39%。fringe=1ではboundary MAE .02518、P95 .05002、同changed率33.04%。
- 同じ256×192/checker pair、fringe=1はblack boundary MAE .02598、P95 .05098。隠れRGBがcheckerの場合もsource sampling差がpartial-alpha boundaryへ届く。
- fringe=.3でuniform pairのboundary MAEは128×96 .00757、256×192 .00839、512×384 .00859。edge-local differenceは解像度を上げても概ね残り、whole-frame averageでは面積拡大により薄まる可能性がある。
- 実際に同じfringe=.3のblack whole-frame MAEは各解像度で.00196/.00102/.00052へ約半減する一方、partial-alpha ROI MAEは.01301/.01387/.01430。全体平均だけだと局所edge感度を弱く見積もる。
- Contact sheetはmid-gray合成のsource pair、fringe=.3/1出力、×4 pair-difference表示、white合成を比較。背景RGBはalphaが同一なら理論上delta invariantで、floating-point rounding由来の微小差のみ。

### 判断・次

- Alphaを変えないことだけではOldTVの透明縁がhidden RGBに不変とは言えない。channel fringeを使うtransparent cutoutでは、alpha境界ROIと複数背景の評価を含める必要がある。
- 「hidden RGBを読むままにするか」「境界で別の色処理をするか」は望ましいfringe look、入力alpha association、上流のRGB拡張処理が未確定なのでCore変更しない。次は利用条件が明確な自然RGBA cutoutで同じpaired-hidden-color試験を行い、実caller契約を追う。

## 2026-11-11: Standalone suite のCPU時間とtrace対象メモリは解像度とeffect構成で増える

### 再現条件と計画

- **関連:** `tools/full_suite_cpu_cost_study.py`、`captures/full-suite-cpu-cost/full-suite-cpu-cost-metrics.json`、`full-suite-cpu-cost-chart.png`、`retro_lab.py::apply_retro`。
- **問い:** ArtifactStudioを起動しない研究runner内で、現在の4 presetの処理時間とPythonからtraceできる一時メモリは出力解像度とどう変わるか。どのstageを無効化したcounterfactualで全体時間が変化するか。
- **条件:** 128/256/512/1024 squareの決定的opaque synthetic gradient、各preset、seed 31039。各条件で2 warm-up後に9回 `perf_counter_ns` でend-to-end `apply_retro` を計測し、出力は timed region 内で破棄。tracemallocは代表1 callのbaseline超peak/retainedを別計測。256/1024ではgrain、chromatic aberration、scanlines、scale/pixelationを1段ずつ無効化したpipelineを各2 warm-up+9回比較する。
- **評価計画/制約:** median/p95 wall time、tracemalloc peak/retained、RGBA8 input/output byte数とfloat32 RGB frame byte数を記録。stage差は中間配列・cache/allocation変化も含むnon-additive counterfactualであり、単独stageの厳密CPU時間ではない。性能予算も合格閾値もないため記述研究とする。
- **環境/未確認:** Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0、Windows 11、Intel Family 6 Model 183、24 logical CPUs。1台・1 processの反復で、OS scheduling/background loadの影響あり。tracemallocはnative/Pillow全体のRSS/GPU/VRAMを保証せず、Artifact/Core/GPU、decode/IO、production caller、threading、実機user latencyは未確認。
- **実行:** `python tools/full_suite_cpu_cost_study.py`。artifactはignored `captures/full-suite-cpu-cost/` に保存。

### 観測

| Preset | 128px median | 256px median | 512px median | 1024px median | 1024px traced peak |
|---|---:|---:|---:|---:|---:|
| handheld | 1.51 ms | 6.56 ms | 28.64 ms | 116.94 ms | 76.01 MiB |
| arcade | 2.17 ms | 8.67 ms | 30.53 ms | 123.01 ms | 76.01 MiB |
| print | 0.57 ms | 3.53 ms | 17.39 ms | 76.34 ms | 40.04 MiB |
| vhs | 1.74 ms | 7.12 ms | 34.53 ms | 141.33 ms | 76.01 MiB |

- 1024px p95はhandheld 119.04ms、arcade 148.38ms、print 78.05ms、VHS 142.56ms。各preset内で128→1024は画素数64倍、median時間は約51–78倍。これは当該Python環境の観測で、計算量上限や他環境の予測値ではない。
- 1024pxでprintだけtraced peakが40.04 MiB、他presetは76.01 MiB。現行recipeでprintはresize/aberration/scanline stageが無効なため、trace対象の最大同時配列群と処理時間も小さい。本観測はPython allocation peakでありprocess RSSではない。
- 256px/1024px counterfactualでaberration無効化はmedian時間をhandheld約46%/31%、arcade約62%/34%、VHS約52%/41%低下させた。printではaberrationが無効のため256px差+0.94ms、1024px差−1.23msとなり、単純なstage加算が成立しない例も得た。
- grain無効化は256pxで22–52%、1024pxで14–31%低下。scanline/scaleの効果はpreset/サイズで不安定で、1024px handheldなどでは無効化側が遅い測定もあり、個々のstage費用として解釈できない。

### 判断・次

- standalone suiteでは解像度とrecipeが研究runnerコストを大きく左右し、1024pxではaberration/grainのcounterfactualが主要な時間差だった。ただし製品Coreへ最適化を移す根拠にはならない。
- 次は必要ならこのrunnerの同一Python環境で入力形状・thread affinity等の再現性を独立に調べる。Artifact.exe起動、C++ build/test、Core/GPU profilingは未実施。Core変更なし。

## 2026-11-11: Full suite の色数は量子化後のgrain/offsetで大きく増える

### 再現条件と計画

- **関連:** `tools/full_suite_tonal_response_study.py`、`captures/full-suite-tonal-response/full-suite-tonal-response-metrics.json`、`full-suite-tonal-response-sheet.png`、`full-suite-tonal-response-ramp-quantizer-vs-suite-1to1.png`、`retro_lab.py::apply_retro`。
- **問い:** 現行suiteのquantizerとfull recipeで、素材タイプ・低勾配・勾配/細部・alpha境界ごとの色数とtone分布、入力との差がどう変わるか。4 presetの良し悪しを順位付けしない。
- **比較対象:** 同じdecoded RGBA8 inputをbaselineとし、quantizer-onlyを後段のchannel offset/scanline/grainより前に採取し、full suiteは`apply_retro`そのものを通す。入力との差分は応答の記述に限り、正解画像・承認済みlookとして扱わない。
- **素材/条件:** existing manifestsと照合したCC0素材10点（white-clover cutout 5、feather 3、hair/fur 2）とsynthetic opaque grayscale ramp/control。native dimensions、fixed seed 31041、現行4 preset。SHA-256をdecode前に検証。provenance URL/license/hashはJSON各recordに保持。
- **領域:** source基準visible、3x3 alpha boundary band、partial-alpha、linear-luma neighbor sum <.004のsmooth low-gradient、>.025のgradient/detail。pixel countも記録。定義はsource maskでありsemantic object segmentationではない。
- **指標:** output unique RGB8 colors、linear Rec.709 luma min/median/p95/max、alpha-coverage-weighted 32-bin luma entropy、channel low/high clipping ratio、linear RGB MAE/RMSE/P95/1÷255超pixel比、alpha exact-match、黒/中灰/白へnormalized code-value合成後の差。Entropyと色数は分布記述で品質指標ではない。色域変換はdecoded RGBをsRGBと仮定し埋込ICCは適用しない。
- **共通必須項目の適用判定:** tone response/parameter recipeは適用。全体/局所表示と統計を確認。色/alphaは適用（3背景合成、alpha保持）。時間安定性は対象がstill-only suiteのため対象外（time-varying stageなし）。経路/解像度は製品pathの比較対象外・未検証。性能はこの実験では未測定。Python CPU時間の別studyは実行済みだがbaseline/Core/GPU予算がないため性能合格は未判定。実写真、高bit-depth/HDR、profile-aware decode、display transform、人手承認済target、製品preview/exportは未検証。
- **合否/判定:** approved appearance target、rater、許容閾値なし。descriptive studyであり合格/不合格やpreset品質順位を付けない。細部表示用rampは元pixel 1:1。多数source/presetのcontact sheetは128px tileの概観だけで、微細画質合否には使わない。
- **実行環境:** Python 3.14.2、NumPy 2.4.3、Pillow 12.2.0、Windows 11、seed 31041。`python tools/full_suite_tonal_response_study.py`。Artifact.exeなし。

### 観測

- 11 input × 4 preset = 44 case。hash verificationを通過。全opaque/alpha assetでfull suite output alphaは入力と完全一致。
- synthetic rampのquantizer-only smooth ramp unique colorsはhandheld 56、arcade 35、print 28、VHS 54。一方full-suite全RGB unique colorsは3,876/8,598/2,425/3,948。quantizer後のgrain、channel offset、scanlineが値を増やすため、full suiteのunique RGB色数をpalette budgetや実効階調数の代用にできない。
- CC0 10素材のquantizer-only visible unique RGB8 colorsは preset間・asset間で4–4,976、full-suiteは217–169,209。固定color countでもBayer、resize、色分布、下流effectで実出力の色数が異なる。可視色数の多さは品質向上を意味しない。
- assetごとに4 presetのmedian smooth low-gradient linear RGB RMSE（baselineとの差）は hair/fur opaque .003–.006、hair JPEG .009–.012、clover close .041–.062、clover full .020–.022、silver feather .066–.279。素材内容がpreset差より強く影響し得る。数値は誤差量であって正解からの品質誤差ではない。
- 全11素材でpreset別median region RMSEはprintがsmooth .0407/detail .0440/alpha-edge .0430/partial .0472、handheld .0527/.0727/.1222/.1420、arcade .0615/.0833/.1414/.1544、VHS .0471/.0692/.1537/.1479。透明境界/partial-alphaの応答はsmooth regionより増える場合がある。printが小さい差であることを「優れたlook」とは判断しない。
- alpha preservedのため、同一code-value compositeの3背景で、background median RMSEはpresetごとにほぼ一致（handheld .0542/.0545/.0551、arcade .0642/.0647/.0656、print .0410/.0412/.0415、VHS .0487/.0492/.0504）。小差はfloat合成・量子化由来。実製品のlinear/premultiplied合成を証明しない。
- 1:1 ramp sheetではflat patchにgrain grain-like fluctuation、scanline modulation、pixelationが重なる様子を目視した。Contact sheetでは各素材のsilhouette/textureとpresetの大差を概観したが、thumbnailは細部判定に不適。

### 判断・次

- quantizerの設定color countは量子化段だけの仕様であり、grain/色ずれ/scanlineを含むfull suiteの最終色数やsmoothnessと同義ではない。既存suite処理もR&D runnerも変更不要で、Core反映の根拠なし。
- 次は実写写真の利用許諾とICC条件が明確な素材を追加し、低照度/skin/skyなど自然な滑らかtoneで量子化後grain量・scanline・offsetの交互作用を、人手で承認した表示条件と目標を定めて評価する。現時点では写真とvisual acceptance未検証、製品path/performance未検証のため研究は継続。

## 2026-11-11: CC0写真でskin・night sky・cloud highlightのsuite応答を確認

### 再現条件と計画

- **関連:** `tools/full_suite_photo_tone_study.py`、`eval-inputs/real-tone/provenance.json`、`captures/full-suite-photo-tone/real-photo-suite-tone-metrics.json`、`real-photo-source-sheet.png`、`real-photo-suite-contact-sheet.png`、`real-photo-suite-detail-crops-1to1.png`、`retro_lab.py::apply_retro`。
- **問い:** portrait skin、night sky、backlit cloud、blue sky photoに現行4 presetを適用した際、quantizer-onlyとfull suiteでshadow/midtone/highlight、滑らかなtone、detailがどう変わるか。
- **素材と権利:** 5写真の個別source URL、author、CC0 status、download SHA-256、EXIF条件、ICC条件をmanifestへ記録。William Stitt / Unsplash portraitはCommons page上でUnsplash 2017-06-05前のCC0とされるが、Commons importはreview-needed、被写体のmodel/publicity rightは確認できない。Acagastyaの高ISO B&W portraitもCC0だがCommonsがpersonality-rights warningを表示する。両portraitは本lab内の内部R&Dに限定し、製品・demo素材として使用しない。Milky Way、Tyrol backlit cloud、Ontario cloud/blue-sky photoは各Commons file pageでCC0を確認。
- **入力条件:** `ImageOps.exif_transpose`、embedded ICCがあるportrait/Nikon画像はPillow ImageCmsでsRGB変換、profile無しはsRGBと仮定。長辺1024pxへ比例Lanczos縮小、cropなし。seed 31111。適用recipeは現行PRESETS。Quantizer-onlyはscaleがあるpresetではBOX縮小＋NEAREST復元後、preset color count/Bayer strengthを適用。full suiteは同じ画像を`apply_retro`へ渡す。
- **評価領域:** scene-linear Rec.709 lumaによるwhole、<.02 shadow、.02–.08 deep shadow、.08–.6 midtone、≥.8 highlight、隣接luma差sum <.004 smooth、>.025 detail。各maskのsource pixel countを保存。空領域はmetric `null` として扱い0誤差にしない。
- **測定:** unique RGB8 colors、linear Rec.709 luma min/median/P95/max、32-bin luma entropy、channel clipping ratios、inputとの差linear RGB MAE/RMSE/P95・1/255超過率。alphaはopaque sourceのみで合成比較対象外。固定still/seedであり時間安定性は対象外。製品decode/preview/export/GPU/backend/performanceは対象外・未検証。
- **視覚確認:** overview sheetはaspect-preserving縮小で全体傾向のみ。detail cropはstudy resolution上の256×256 pixelを1:1に表示。display transform/校正画面なし。baseline差分は応答の記述で、ground truth/approved retro target/人手嗜好ではない。許容閾値も設定していないため合否・品質順位を作らない。
- **再現:** `python tools/full_suite_photo_tone_study.py`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / Windows 11。Artifact.exeは起動していない。

### 観測

| Photo | Input luma range/median | Preset | Whole RGB RMSE | Smooth-tone RMSE | Detail RMSE | output unique RGB8 |
|---|---|---|---:|---:|---:|---:|
| color face portrait | .021–.928 / .311 | handheld | .0829 | .0698 | .0815 | 15,714 |
|  |  | arcade | .1047 | .0860 | .1014 | 110,129 |
|  |  | print | .0556 | .0465 | .0541 | 5,890 |
|  |  | VHS | .0753 | .0587 | .0821 | 100,968 |
| high-ISO B&W portrait | 0–1 / see metrics JSON | handheld | .0631 | .0253 | .1067 | 6,726 |
|  |  | arcade | .0840 | .0357 | .1391 | 85,544 |
|  |  | print | .0373 | .0163 | .0625 | 256 |
|  |  | VHS | .0600 | .0241 | .1096 | 86,977 |
| Milky Way | 0–1 / .015 | handheld | .0355 | .0174 | .0950 | 15,588 |
|  |  | arcade | .0443 | .0211 | .1126 | 61,351 |
|  |  | print | .0233 | .0146 | .0460 | 8,367 |
|  |  | VHS | .0362 | .0163 | .1075 | 61,884 |
| backlit cloud | 0–1 / .330 | handheld | .1044 | .0589 | .1364 | 22,818 |
|  |  | arcade | .1432 | .0827 | .1834 | 110,144 |
|  |  | print | .0596 | .0331 | .0729 | 6,095 |
|  |  | VHS | .1015 | .0536 | .1424 | 105,908 |
| blue sky/clouds | .035–.392 / .149 | handheld | .0491 | .0453 | .0572 | 3,829 |
|  |  | arcade | .0540 | .0491 | .0674 | 19,218 |
|  |  | print | .0392 | .0367 | .0446 | 2,014 |
|  |  | VHS | .0423 | .0390 | .0526 | 15,652 |

- 同一sourceでもsuite responseはrecipeと領域で変わる。Milky Way handheldはwhole RMSE .0355だがdetail .0950、smooth .0174。一方color portrait handheldはwhole .0829、smooth .0698、detail .0815。high-ISO B&W handheldはwhole .0631、smooth .0253、detail .1067。撮影時grainを含むsourceでもsmooth/detail responseは同じ方向に動かず、whole averageだけでは局所の応答を代表しない。
- backlit cloudのhighlight ROIは104,872px。full-suiteのhighlight内channel values ≥1の比率はhandheld .1814、arcade .0604、print .2289、VHS .0903。cloudのsourceが既にluma max 1のsceneであり、grain clipping・元画像clip・表示変換を分離していないため、比率は追加の出力clippingを直接意味しない。
- handheld face quantizer-only whole unique colors 53からfull suite 15,714、Milky Way 106→15,588、backlit cloud 66→22,818、blue sky 15→3,829へ増加。後段grain/channel offset/scanlineのため、final unique colorsは設定palette sizeではない。printも量子化後のgrain等を含むため同様。
- Contact sheet overviewと5素材の1:1 cropsを目視。handheldは顔の皮膚・雲・空の低勾配に目立つ段階帯を作り、その後のgrain/scanlineが段差内へ変動を加える。high-ISO B&W portraitは入力の写真粒状とpreset grainを画素差だけで分離できない。高輝度雲は各presetで階調と局所detailの表現が違い、Milky Wayはshadowと星/灯りdetailの差が分かれて見える。これは当該素材・1024px条件の観察に限り、どのlookが望ましいかは未決。
- `shadow luma <.02` はportrait/blue skyでpixel count=0。該当ROIの統計は未定義。各reportは`null`として保持し、0誤差と混同しない。

### 判断・次

- 自然写真を追加したことでstylized/transparent texture以外の連続tonal materialも評価できるようになった。差分や色数から良否を断定するapproved targetは依然ない。Coreへの変更根拠なし。
- 次は異なる肌色・照明の複数portraitをmodel/publicity statusまで確認して増やし、あわせてより多い昼空/夕空/夜景素材を集める。今回の2 portraitは内部R&Dのみとする。人手で承認されたretro appearance/display transformと許容条件が決まるまでは記述研究を続ける。製品のICC import、GPU/runtime、performanceは未検証。

## 2026-11-11: 写真suiteの入力解像度依存を比較

### 再現条件と計画

- **関連:** `tools/full_suite_photo_resolution_study.py`、前項の5写真manifest、`retro_lab.py::apply_retro`、`captures/full-suite-photo-resolution/photo-resolution-suite-metrics.json`、input/output overview、normalized detail crops。
- **問い:** 同じsceneを長辺512/1024/2048pxでsuiteへ渡すと、表示サイズを揃えたときの出力差・texture記述値・Python CPU時間はどう変化するか。
- **入力と処理:** 前項と同じ5枚、hash確認、EXIF orientation、埋込ICCからsRGB変換（無ICCはsRGB仮定）、比例Lanczos縮小、現行4 preset、seed 31111。各resolutionを処理し、512px input dimensionsと一致するpixel寸法へLanczosで正規化した。よって同一resolution内のsuite実行と共通display縮小の両方を含み、Coreや製品decode経路を表さない。
- **差分の分離:** reference 512入力からの入力縮小差、reference 512 suite出力からのfull output差、各resolution inputと同resolution outputのeffect residualを個別記録。領域maskはreference inputから作り、whole/shadow/deep-shadow/midtone/highlight/smooth/detailにlinear RGB MAE/RMSE/P95とchanged-pixel ratioを測定。空領域はnull。
- **性能・texture:** 1 warm-up後に各photo/preset/resolutionを3回測り、Python `perf_counter_ns` median/P95を保存。512px共通表示でlinear-luma variance、radial high-frequency power share、median spectral radius、水平neighbor correlation、unique RGB8色数を記録。これらは品質点数ではない。
- **視覚確認:** aspect-preserving 512px display overviewおよびsubjectのnormalized cropを確認。detail cropは同じsource比率領域を256px tile内へaspect-preserving fitするため、native 1:1 pixel表示ではない。承認target、calibrated display、合否thresholdはない。
- **再現:** `python tools/full_suite_photo_resolution_study.py`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / Windows 11。Artifact.exeは起動せず、build/test/CMake/Core/GPUも未実行。

### 観測

- 5 photos × 4 presets = 20 recordを3解像度で完了。全source hashを検証し、全20 caseでsuite output alphaが入力alphaと一致。photo dimensionsはそれぞれ512 `[512,341]`、1024 `[1024,683]`、2048 `[2048,1365]`。
- 共通512表示におけるsuite outputの512基準RMSEは1024でmedian .04894 / max .10038、2048でmedian .05039 / max .10203。input resamplingのみのRMSEは1024 median .00150 / max .00239、2048 median .00121 / max .00193。今回の差分は単純な入力Lanczos縮小量より大きいが、測定にはresolution依存のsuite処理と共通表示縮小が含まれ、単一stageへ帰属できない。
- 素材別の2048 whole-image RMSE range（4 preset）はcolor portrait .0557–.0740、B&W portrait .0373–.0607、Milky Way .0231–.0316、backlit cloud .0596–.1020、blue sky .0328–.0393。sceneにより表示正規化後の差量が異なる。これは誤差量であり見た目の良否ではない。
- 2048のregion RMSE中央値はwhole .05039、smooth .03115、detail .07269、midtone .06445、highlight .12851（highlight有効16 case）。highlight差は明るい雲等で大きくなり得るが、領域ごとの画素内容とpreset compositionに依存し、閾値や好ましいlookを示さない。
- Python suiteのcase median CPU時間は512px 17.51ms、1024px 76.62ms、2048px 330.39ms。概ね画素数の4倍ごとに約4.4倍の処理時間となる観測だが、3回・単一Windows機・Python実装の測定であり、変動、memory、Artifact/Core/GPU性能、実製品budgetについて結論しない。
- 200px overviewではresolution列間の大まかなscene/preset外観は近い一方、normalized cropではcloud/skyのtexture、肌・髪・B&W portraitのgrainや境界が変わるのを確認。cropは表示fitでありnative-pixel sharpness比較ではない。固定seedでも同じnormalized lookを保証しない例だが、原因stageを分ける専用counterfactualは未実施。
- `512`と比較したスペクトル指標・unique colorsはmetric JSONに全20 case分を保存。これらから品質順位は付けていない。

### 判断・次

- 固定pixel parameterのsuiteは512/1024/2048の入力条件を変えると、512px表示へ縮小した後にも色・texture差が残る。入力縮小単独の差は小さいが、この計測だけではgrain、scanline、pixelation、quantization、chroma displacement、resampling相互作用の寄与を特定しない。
- CPU時間は独立したPython runnerの観測値として記録し、製品予算や実装選択へ外挿しない。Core反映の根拠やpreset優劣は確立していない。
- 次は、表示寸法を固定した中でsuiteの各stageを一つずつoffにするcounterfactualと、resolution parameterをdisplay-scaleへ追従させる仮説を同一source/cropで比較し、目視基準と許容条件を事前定義する。製品要件が決まるまでは記述研究を続ける。

## 実験 104: 実写suiteの解像度driftをstage ablationで比較

### 再現条件と計画

- **関連:** `tools/full_suite_photo_stage_isolation_study.py`、前実験の5写真provenance、`retro_lab.py::apply_retro`、`captures/full-suite-photo-stage-isolation/photo-stage-isolation-metrics.json` とvariant別overview/detail crop。
- **問い:** 同じ実写・presetを512/1024/2048pxで処理したとき、どのstageを無効にすると512px表示後の解像度drift patternが変わるか。
- **素材・入力:** 前実験と同じ5枚をSHA-256照合。EXIF向き補正、埋込ICCからsRGB変換、未埋込はsRGB仮定。比例Lanczos resize後、4現行preset・seed 31111で実行。全variantを512px入力と一致する寸法へLanczos正規化。
- **Counterfactual:** 1段ずつ `pixelation: scale=1`、`quantization_dither: colors=256/dither=0`、`channel_offset: aberration=0`、`scanlines=0`、`grain=0`。量子化とBayer ditherは同一stage関数内の同時停止であり個別効果へ分けない。full suiteも同じrunで再生成。
- **比較量:** (1) 同じ解像度でfullとablation出力の差、(2) 各pipeline variant内で512px出力から見た1024/2048px drift、(3) fullとablationのsigned resolution-drift image field差を、whole/shadow/deep-shadow/midtone/highlight/smooth/detail ROI別にlinear RGB MAE/RMSE/P95/changed-pixel ratioで記録。512 reference input由来maskを使い、空ROIはnull。3番は「stageを止めた時にresolution responseがどれだけ変わるか」のcounterfactual proxyで、加算可能なstage寄与ではない。
- **視覚確認・判定:** 全source/preset/variant/resolutionのaspect-preserving共通512px overviewと、各素材の固定normalized subject cropを確認。承認look・calibrated display・observer thresholdがないため記述研究であり、品質合否やpreset順位を作らない。alphaはopaque素材のexact passthroughを確認。静止画のためtemporal stabilityは対象外。性能測定はなし。
- **再現・境界:** `python tools/full_suite_photo_stage_isolation_study.py`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / Windows 11。Artifact.exe起動、build/test/CMake、Core/GPU実行なし。製品decode/display path、memory/performance、human preferenceは未検証。

### 観測

- 5写真 × 4 preset = 20 case、6 pipeline variants（full + 5 ablations）、3解像度を完了。合計360回のsuite処理。すべてのcase/variant/resolutionでalpha passthrough一致。
- 2048px表示の「fullとablation間のsigned resolution-drift field RMSE」中央値（5素材）は以下。単位はlinear RGB、品質誤差ではなくdrift patternの差量。

| Preset | Pixelation off | Quantization+dither off | Channel offset off | Scanlines off | Grain off |
|---|---:|---:|---:|---:|---:|
| handheld | .02455 | .01811 | .01551 | .03623 | .01994 |
| arcade | .01217 | .00749 | .01979 | .05786 | .01139 |
| print | .00000 | .02585 | .00000 | .00000 | .02976 |
| VHS | .00000 | .00479 | .02744 | .03089 | .03206 |

- 0のstageは当該presetで元から無効（handheld/arcadeだけscale<1、printはscanline/offset無効、printとVHSはscale=1）。ablation proxyがpreset構成と整合するcontrolとなった。
- stage停止時のresolution-response差はpreset依存。scanline停止はarcade .05786、handheld .03623で大きい。VHSはgrain .03206とscanline .03089、printはgrain .02976とquantization+dither .02585。channel offsetはVHS .02744、arcade .01979、handheld .01551。これらは各preset有効量・素材内容・stage相互作用込みで、一般化可能なstage順位ではない。
- 2048px時に同一resolution full-vs-ablation差RMSE中央値を全20 caseで集計すると、scanline .03075、grain .00554。grainの瞬間的な出力差は小さめでも、grain無効時のdrift signature差はpreset別 .01139–.03206。これはgrain sampling/filter/resamplingとの相互作用候補を示すが、因果stageを特定した結果ではない。
- 目視overviewでは大まかなscene/toneは近いvariantもあるが、normalized cropでportrait・sky/cloud・night-skyの粒状感、row modulation、edge chromaとtextureがvariant/resolution間で変化した。tileは共通512pxへ縮小し表示fitした領域cropなのでnative-pixel sharpnessを示さない。

### 判断・次

- 実写でもstage ablationによりresolution responseが変わることを観測した。preset別傾向は現行parameterの反応で、pixel固定量をdisplay-scaleへ正規化する設計が望ましいか、どのlookが正しいかまでは決められない。Core反映の根拠なし。
- 次はdominant候補のscanline/grain/channel offsetについて、fixed-pixelとnormalized-display-space parameterのpaired counterfactualを作り、同じ入力・crop・display寸法で差を比較する。閾値・人手基準がないため、その結果も引き続き記述的に扱う。

## 実験 105: 512px表示座標へparameterを追従させる仮説

### 再現条件と計画

- **関連:** `tools/full_suite_photo_display_scale_hypothesis_study.py`、実験104のstage ablation、`retro_lab.py::apply_retro`、`captures/full-suite-photo-display-scale/photo-display-scale-hypothesis-metrics.json`。
- **問い:** 固定pixel parameterが作る解像度driftを、effectのpixel spacingを512px共通表示座標へ追従させるcounterfactualで変えられるか。
- **素材/入力:** 前実験と同じhash検証済み写真5点、EXIF向き、ICC→sRGB、無profileはsRGB仮定、比例Lanczosの512/1024/2048入力、4 preset、seed 31111。比較出力は512px inputと同寸法へLanczos正規化。
- **研究式仮説:** `pixelation`はscaleをlong-edge解像度比で割る、`dither`は既存Bayer4×4 thresholdを512座標でsample、`channel_offset`は横幅比でpixel shiftを拡大、`scanlines`は512座標の交互行phaseをsample、`grain`はseed固定の512座標fieldをnearest mappingしてgrain粒度とresolution間field alignmentを保つ。単独5 variantと全stage追従variantを作成。grain hypothesisは粒径だけでなく同じseedのspatial field mappingも含む。
- **完全性gate:** 各single-stage variantとall-scaled variantが、全20 photo/presetの512px出力で現行`apply_retro`とRGB8完全一致することをassert。各解像度でsource/output alpha passthroughも確認。
- **比較・合否:** 各variantの512px自身をreferenceにしたresolution drift、fixed-pixel出力との差、signed drift fieldの差を領域別linear RGB MAE/RMSE/P95/changed-pixel ratioで測定。512 referenceへの一致を「一貫性proxy」とし、望ましいappearanceやquality oracleと扱わない。overview/detail cropはaspect-preserving display fit。承認target、calibrated display、observer thresholdなし、したがって記述研究のみ。still imageなので時間安定性は未対象、performance/memory未測定。
- **境界:** `apply_retro`のhigher-resolution production behaviorを変更せず、現行式を写した独立Python研究モデル。実装式・GPU/Core/decoder/export/display scale、HDR、motion footage、temporal grain、人手嗜好は未検証。
- **再現:** `python tools/full_suite_photo_display_scale_hypothesis_study.py`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / Windows 11。Artifact.exe起動・build/test/CMake/Core/GPU実行なし。

### 観測

- 20 photo/preset case × 7 variants（現行fixed-pixel、5単独hypothesis、all scaled）× 3解像度を完了。全normalized variantが512pxで現行suiteと完全一致するgateを通過。全variant/resolutionでalpha exact-match。
- 2048pxで固定pixelの512px-reference driftはmedian RMSE .05039、最大 .10203。all-stage display-scale仮説はmedian .01978、最大 .04162となり、20/20 caseで固定pixelより512px referenceに近かった。これは512px appearanceとの一致率であり、画質改善を証明しない。
- 単独仮説別の2048 drift median/max、fixed-pixelよりdriftが小さいcase数:

| Hypothesis | Median RMSE | Max RMSE | 20 casesでlower drift |
|---|---:|---:|---:|
| Pixelation scale | .04852 | .10067 | 15 |
| Bayer/dither coordinate | .04782 | .10300 | 5 |
| Channel offset distance | .04497 | .09886 | 15 |
| Scanline row phase | .03783 | .07534 | 15 |
| Grain spatial field | .04099 | .10067 | 20 |
| All stages together | .01978 | .04162 | 20 |

- presetごとのall-scaled average whole-image RMSE (fixed→candidate) はhandheld .04991→.02421、arcade .06117→.02522、print .04301→.02074、VHS .05525→.01806。視覚sheetでは大きなtone/preset characterは維持される一方、grain/row/pixel textureが解像度に応じて変わった。これは研究式表示で目視した差で、承認looksとの比較ではない。
- dither単独は全20 case中5件のみlower driftで、改善はprintに集中し、arcadeの一部はdriftがわずかに増加。scanline単独はactiveなhandheld/arcade/VHSで各5素材ともlower drift。grain単独は20件すべてlower driftだが、seed fieldを解像度間で対応づけた条件を含む。
- 素材縮小自体は残っており、stage parameter正規化の影響は素材・presetに依存。all-stage結果が各single-stage改善の和と一致する保証はない。

### 判断・次

- 固定512pxのlookを安定化させる数値proxyとしては、複数のdisplay-space mappingが効き、組合せhypothesisは本5写真/4 presetでdriftを下げた。ただし512pxを基準lookとする製品要件・人手選好はなく、受容すべき高解像度lookは未決。Coreへ移植する根拠はまだない。
- 次はgrain fieldのseed/coordinate mappingと振幅を分離するcounterfactualを行い、単独の粒径追従と跨解像度のfield alignmentがそれぞれどれだけproxyを動かすか調べる。その後、もし製品要件がresolution consistencyなら、approved display/view scaleとhuman acceptanceを定義してから実装候補を評価する。

## 実験 106: Grain粒径・field alignment・振幅を因子分解

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_factor_study.py`、実験105のdisplay-scale式、`retro_lab.py::apply_retro`、`captures/full-suite-photo-grain-factor/photo-grain-factor-metrics.json` と6 variant別overview/detail crop。
- **問い:** 512-coordinateへ他のpixel-space stageを正規化した条件で、grainのcell size、resolution間field alignment、sigma倍率は512px基準へのdriftと実際のgrain residualへ別々にどう作用するか。
- **素材/条件:** 同じSHA-256検証済み写真5点、4 preset、seed 31111、512/1024/2048。ICC/EXIF対応は実験105と同じ。5つのnon-grain stageは実験105のhypothesis mappingに固定。全grain variantの512px出力は現行suiteとRGB8完全一致するgateを通過すること。
- **6条件:** native-pixel grainとdisplay-cell grainの2粒径 × nominal sigma / resolution-ratio sigma compensation × nominal-field same seed / size-derived independent field。Native pixel modeは現suiteのfull-size iid Gaussian field。Display cellは512座標fieldをnearest mapping。independent条件は512のseedだけbaselineと共有し、それ以外の解像度は決定的なsize-specific seedを使う。sigma補償は入力long-edge比（1/2/4倍）をsigmaへ乗算し、仮説stress条件とする。
- **効果測定:** grainを含む結果と同じnormalized non-grain pipelineとの差からvisible RGB residualを測り、whole/shadow/deep-shadow/midtone/highlight/smooth/detailのlinear RGB MAE/RMSE/P95/change ratio、luma residual RMS/variance、radial high-frequency power、lag 1/2/4/8水平相関、RGB channel endpoint occupancyを保存。併せて512px referenceへの解像度driftを測る。RGB endpoint比はsource/他stage由来も含むproxyで、grainだけのclipping率ではない。
- **視覚・判定:** 6条件×5写真×4 preset×3解像度をaspect-preserving overview/cropで確認。固定512px appearanceはlook targetではない。display calibration、独立observer、thresholdなし。Still画像の跨解像度field比較であり時間方向grain安定性ではない。性能/メモリ/Core/GPU経路は未計測・未確認。
- **再現:** `python tools/full_suite_photo_grain_factor_study.py`。Artifact.exe起動なし、build/test/CMake/Core/GPU実行なし。測定は独立Python equation model。

### 観測

- 20 photo/preset case × 6 grain variants × 3解像度を完了。各variantの512px出力は全20 caseで現行suiteとRGB8完全一致し、全variant/解像度でalpha exact-match。
- Nominal sigmaでnative iid grain residual luma RMS中央値は512px .02249、1024px .01012、2048px .00546。512表示へ downsample すると高解像度の独立pixel grain振幅が落ちる挙動を観測。
- 512-coordinate display-cell grainは同じsigmaのまま、residual RMS中央値が1024px .01770、2048px .01721。粒の空間尺度は512px表示で相対的に保持される一方、fixed-pixel referenceとのdriftは2048 median .01978となり、native-pixel nominal .03276より20/20 caseで低かった。これは512pxへの見た目一致を増やすproxyで、品質向上を示さない。
- 粒径を同じdisplay-cellに固定したaligned-fieldとsize-specific independent-fieldを比較すると、grain residual RMSは2048で中央値 .01721 と .01720でほぼ同じだが、reference driftはaligned .01978、independent .03853。aligned側のdriftが全20 caseで低く、grain fieldの空間対応自体が解像度間差へ寄与する結果となった。random textureの反復・resolution間一貫性が望ましいかは未決。
- Sigmaをlong-edge比で増幅したnative-pixel variantは2048 residual RMS .02059まで戻るが、512px基準driftはmedian .04106となりnominal .03276より全20 caseで増えた。display-cell aligned sigma増幅はresidual RMS .06624、drift .05450で、nominal cell .01721/.01978より全20 case悪化。2048のoutput channel値1.0 occupancyはamplitude-scaled cell条件で最大 .00365（0.365%）。この比はgrain-only clipping率ではないが、sigmaの単純倍率補償が過大なpixel responseを作り得るstress evidence。
- Grain residualのradial high-frequency fraction中央値はnative nominal .8253、display-cell aligned nominal .7887、display-cell independent nominal .7995。これは設定したcutoffとselected photos上のtexture記述値で、知覚上の粒状感の序列ではない。
- Overview/cropではcell grainがsky/cloud/skin上で大きめに連続し、amplitude-scaled caseは顔・暗空・雲に強いノイズが出るのを確認。意図したretro lookか欠陥かはapproved targetなしでは判定しない。

### 判断・次

- 512 display-spaceで粒径を保つモデルは、downsample後のnominal grain residualと512-reference driftを保った。一方、seed field alignmentは同じ粒径/RMSでもdriftを大きく変え、振幅を解像度比で単純増加する案はdriftとendpoint occupancyを悪化させる傾向だった。いずれもPython modelと5写真/4 presetに限り、Coreへ反映する根拠や視覚的推奨値ではない。
- 次はresolved scene上でnoise residualが低勾配/細部・明部/暗部へどう分布するかを調べ、clipping・quantizationとの相互作用を分ける。Motion footageでのgrain temporal coherenceは別途未検証。

## 実験 107: GrainのROI別quantization-grid crossingとclamp excursion

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_roi_interaction_study.py`、実験106の6 grain variants、`retro_lab.py::apply_retro`、`captures/full-suite-photo-grain-roi/photo-grain-roi-interaction-metrics.json`。
- **問い:** Grainはsource-tone/detail ROIごとにどれくらい後段の等間隔preset gridを跨ぐか、またRGBを0/1 clampへ押すか。
- **素材/条件:** 実験102–106と同じSHA照合済み写真5点、4 preset、6 grain variant、512/1024/2048、seed 31111。pixelation/Bayer/channel offset/scanlineは実験105の512-coordinate mapping。region maskは512入力のlinear luma / gradientで定義し、native寸法へnearest mapping。
- **内部観測点:** pixelation→ordered quantization→channel offset→scanlines後、grain直前のfloat RGBを再構成。noiseを加算し、pre-clamp excursionと[0,1] clamp後のcode binsを別測定。
- **bin crossing definition:** presetの`colors-1`等分code-value gridへgrain直前RGBとgrain後/clamp RGBをnearest-roundし、bin indexが変わったRGB component/pixel比率を記録する。channel offset/scanlineが既にquantizer gridを外れている出力を、grain後に実quantizerが再実行される意味ではない。画素全体の色数・品質やbanding合格を表す指標でもない。
- **clamp metrics:** raw `base+grain <0` / `>1`のRGB component比、clamp後のexact 0/1 component比。ROI shadow/highlightはsource-luma maskでありsemantic dark/light object segmentationではない。endpoint値はgrain以外のsource/stageからも生じ得る。
- **gate/判定:** grain加算式再構成はdisplay-scale modelの全解像度出力RGB8と一致。全variantの512px出力もcurrent `apply_retro`と一致。360 conditionを計測。画像所見は同一variant/sceneの実験106 normalized overview/detail cropsと照合。承認target、display calibration、人手評価、thresholdなし、よって記述研究。alphaはopaque inputで保持。time stability/performance/memory/Core/GPUは未検証。
- **再現:** `python tools/full_suite_photo_grain_roi_interaction_study.py`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / Windows 11。Artifact.exe起動、build/test/CMakeなし。

### 観測

- 5写真 × 4 preset × 6 grain variants × 3解像度 = 360 ROI sets。式RGB8 parity gateと512px current-suite gateを全caseで通過。
- 2048px・nominal native-pixel grainのpreset-grid component crossing中央値（5 photo）はhandheld 8.71%、arcade 20.58%、print 18.53%、VHS 79.14%。VHS grain sigma .04と48-level gridが組み合わさり、grid proxyが高い。これはgrain前後のnearest-bin assignmentの変化であり、VHS出力がその比率だけ誤色/欠陥である意味ではない。
- Nominal grainの2048 source shadow ROIではclamp前low excursion component比中央値はhandheld 28.91%、arcade 13.21%、print 21.32%、VHS 16.66%。source highlight ROIのhigh excursionは各々16.44%、0.58%、16.79%、5.99%。値はsource mask内RGB componentsがraw sumで0/1を越えた割合。色ずれ/scanline後の値分布を含み、ユーザー可視破綻や追加clipping pixel数とは同じでない。
- Nominal grainの2048 smooth/detail component bin crossing中央値（smooth/detail）はhandheld 4.42/8.82%、arcade 19.47/21.46%、print 18.28/19.31%、VHS 79.11/79.12%。source edge/detailだけでなく、preset階調数・grain量・後段stageの値が crossing に影響するproxyである。
- Long-edge倍率でsigmaを増やした2048 native-pixel conditionsではwhole bin crossingがhandheld 46.58%、arcade 71.68%、print 71.19%、VHS 94.65%。highlight high excursionも16.44→21.73%、0.58→8.82%、16.79→33.64%、5.99→26.35%。他stage・clampと相互作用するため、「sigmaを解像度比で補えばgrain強度が同じになる」という単純仮説はこのmodelで支持されない。
- Display-cell nominalとnative-pixel nominalの全体bin crossing中央値はpresetごとにほぼ同じ（例: VHS .7915/.7914）。粒径差はbin crossing率単独には現れにくく、空間スペクトル/field alignmentの差と併せて読む必要がある。
- 実験106のdetail cropsではamplitude-scaled grainが顔/夜空/雲へ強いtextureを加える様子を目視。ROI統計との一致は応答の確認であり、どのtextureが望ましいかの判定ではない。

### 判断・次

- Grainはquantizer後段なので、設定palette countだけでは最終出力の階調grid残存率を保証しない。特にshadowのlow excursionとhighlightのhigh excursionは素材・preset・grain sigmaで変わる。clipping率proxyを視覚品質と等置しない。
- 次はseed違いの複数field/多seedでROI統計のばらつきを測り、preset差と確率noise差を分ける。続いてgrainをenable/disableでpaired renderし、source-tone・detail・clipごとに差分を抽出する。Core変更の根拠や受入thresholdはまだない。

## 実験 108: 現行suiteのgrainを16 seedsでROI別に測定

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_seed_variance_study.py`、実験107のbin/clamp ROI診断、`retro_lab.py::apply_retro`、`captures/full-suite-photo-grain-seed-variance/photo-grain-seed-variance-metrics.json` とoverview/detail crops。
- **問い:** 現行固定pixel suiteのgrain出力はseedにどれだけ依存するか。入力が512pxか2048pxかで、共通512px表示へ正規化後のseed varianceはどう変わるか。
- **入力と条件:** 同じprovenance/SHA検証済み写真5点、EXIF方向補正、embedded ICC→sRGB、untaggedはsRGB仮定。現行4 `PRESETS` と未変更`apply_retro`、long-edge 512/2048px、連番seed 31111–31126の16個。実験105のdisplay-coordinate equation modelではなく、現在suite関数を直接実行。各解像度のgrainless baselineは同一presetでgrain=0。
- **正規化/ROI:** 出力を各素材の512px inputと同じ寸法へLanczos縮小。ROI maskは同512入力のwhole/shadow/deep-shadow/midtone/highlight/smooth/detailを使う。
- **測定:** seedごとのgrainless差linear RGB MAE/RMSE/P95/change ratio、RGB endpoint occupancy。16 outputをstackしcommon-displayのper-pixel linear RGB std、Rec.709 luma std、P95をregionごとに測定。stdの定義は16 seed間のunbiased sample standard deviation。
- **視覚・判定:** 各source/presetに両解像度×4 selected seeds (31111, 31116, 31121, 31126) のoverview/detail cropを保存し確認。16 sequential deterministic seedsはconfidence intervalや全noise realizationの保証ではない。静止画像間のseed variationはtemporal flickerではない。approved target、observer、thresholdなしのため記述研究。Opaque sourceでalpha保持確認。性能、memory、Core/GPUは未測定。
- **再現:** `python tools/full_suite_photo_grain_seed_variance_study.py`。Python 3.14.2 / NumPy 2.4.3 / Pillow 12.2.0 / Windows 11。計640 seeded outputs + 40 grainless baseline calls。Artifact.exe起動なし、build/test/CMake/Core/GPU実行なし。

### 観測

- 5写真 × 4 preset × 2解像度 = 40 case、各16 seedで計640出力。全出力alphaは入力と一致。
- 共通512表示上のwhole-image seed-ensemble linear luma std RMS中央値（5写真）は、512→2048入力でhandheld .01858→.00421、arcade .01065→.00245、print .02788→.00623、VHS .03010→.00671。2048/512比は各preset .223–.230。現在suiteのwhite-noise grainは高解像度render後に縮小するとseed依存振幅が約77%下がる挙動を観測。source resamplingや他の固定effectはbaselineにはあるが、跨seed分散はseedで変わるgrain出力成分を反映する。
- Grainless baselineとの差のwhole-image linear RGB RMSE中央値も512→2048でhandheld .01938→.00476、arcade .01109→.00292、print .02908→.00666、VHS .03132→.00708。
- ROI別luma std RMSでも同じ縮小傾向。例えばprint highlight ROIは512 .06172、2048 .01443、VHS highlightは.07289→.01687。shadow ROIの有効source数は3/5で、残りの空maskはnullのまま。
- Seed視覚sheetでは同じscene/preset内でseedごとにtextureがわずかに変わり、共通512表示へ縮小した2048結果では差が減る。高sigmaのprint/VHS grainがhandheld/arcadeよりseed stdの大きいことはpreset値と整合するが、preset品質順位ではない。
- 16 consecutive seedは再現可能な有限集合であり、randomized independent hardware sampleではない。seed間stdはstill-image output variabilityであり、時間方向のanimation flicker/temporal coherenceを表さない。

### 判断・次

- 現行native-pixel white-noiseは固定seedで再現可能だが、同じsigmaでもoutput resolution後のcommon-view seed spreadは大きく変化する。これは実際のsuite式で確認した確率noiseの解像度応答で、表示scale追従が望ましいかはまだ未判定。
- 次はgrain on/offのpaired image residualをROIごとに画像差分として保存し、source compositionとquantization/clampによる応答をseed内で分離する。その後motion sequenceが必要ならtemporal seed/coordinateの連続性を別実験で扱う。Core変更の根拠・画質合格基準なし。


## 実験 109: Grain有無のpaired residualとROI分布

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_paired_residual_study.py`、実験108のseed ensemble、`retro_lab.py::apply_retro`、`captures/full-suite-photo-grain-paired-residual/`。
- **問い:** 現行suiteにおいてgrainを有効にしたとき、grainless同条件出力からのsigned residualがseed・解像度・source tone/detail ROIごとにどう変わるか。
- **条件:** 実験102–108と同じSHA照合済み5写真、4 preset、512/2048入力、selected seeds 31111/31116/31121/31126。160 grain-on出力を各々同preset/size grain=0出力と比較し、双方を同じ512入力寸法へLanczos正規化。
- **ROI/指標:** 512入力のsource luma/gradientで定義したwhole、shadow、deep shadow、midtone、highlight、smooth、detailの7領域。linear RGB誤差、signed linear Rec.709 luma mean/RMS/P95、正負residual比率、RGB8 changed-pixel比、0/255 code state遷移を記録。RGBA alpha passthroughを確認。
- **画像:** paired on/off sheetsとsigned-luma heatmapを全写真/preset/size/seedで出力。Heatmapは固定×8表示gain（red正、blue負）で、飽和し得るため定量源ではない。
- **制約:** grain-offもpixelation/quantization/channel offset/scanlineは有効。差分は最終clamp/RGB8変換との相互作用を含む。still seed群でありtemporal flickerではない。approved target、閾値、人手評価、性能/Core/GPU/product pathは未検証。
- **再現:** `python tools/full_suite_photo_grain_paired_residual_study.py`。160 paired cases。Artifact.exe起動なし、build/test/CMake/Core/GPU実行なし。

### 観測

- Whole-image median linear-luma residual RMS（512→2048入力、共通512表示）はhandheld .01858→.00472、arcade .01069→.00266、print .02793→.00652、VHS .03014→.00678。高解像度入力由来grainの出力差は、512pxへ縮小したとき小さくなる。これは現状のnative-pixel grainとLanczos縮小の特性で、見た目の優劣ではない。
- 512pxで中央値RGB8 changed-pixel比はhandheld .900、arcade .929、print .936、VHS .977。2048px入力ではそれぞれ .875/.839/.929/.939。grainが値を変えた画素比率であり、変化が知覚可能・望ましい比率ではない。
- 512px midtone ROIのluma RMSはhandheld .02728、arcade .01565、print .04161、VHS .04425。smooth ROIは.01484/.00850/.02065/.02363、detail ROIは.02531/.01444/.03806/.04087。写真5点×seed4点の中央値で、detail領域へのgrain反応がsmoothより大きかった。
- 512px highlight ROIではVHS以外のsigned luma mean中央値が負方向（handheld -.00775、arcade +.00067、print -.00943、VHS +.00154）。小さいhighlight ROIのRGB clamp/endpoint状態遷移が影響し得る。source mask由来の中央値であり、独立したgrain biasやtone curveの証明ではない。
- signed residual画像は低周波の色調変化というよりseed依存の赤青speckleとして現れ、grain量の大きいprint/VHSで強かった。2048入力の縮小表示では高周波成分が弱く見えた。×8 heatmap表示による所見であり、キャリブレーション済み鑑賞評価ではない。
- 全160出力でalpha一致。全4画像sheetとJSON reportを保存した。

### 判断・次

- paired residualとseed ensembleを合わせると、grain強度だけでなく出力縮小と細粒相関が、512表示での振幅を大きく左右する。ROI差はsmooth/detail・midtone/highlightで一様ではないため、単一grain量だけの比較では挙動を説明しにくい。
- これは既存Python suiteのモデル研究であり、ArtifactCore反映の仕様・品質判断には未到達。次は実サイズの表示条件と知覚比較、または短い動画像でtemporal coherenceを評価する。Core変更なし。


## 実験 110: Grain residualに対する表示縮小率とresampling filterの寄与

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_resample_filter_study.py`、実験109 paired residual、実験108 seed variance、現行`retro_lab.py::apply_retro`。
- **問い:** Grain差分が512表示で弱まる主因を、入力解像度と出力resampling kernelに分けて比較する。
- **素材/条件:** provenance SHA照合済み写真5点、4 preset、long-edge 512/1024/2048、selected seed 31111/31116/31121/31126。512 native出力に加え、1024/2048の各出力を512入力寸法へBOX/BILINEAR/BICUBIC/LANCZOSの各filterで縮小。計180 source/preset/scale/filter groups、720 paired renders。
- **pairing/測定:** 各grain-on seedを同一size/presetのgrain=0画像と同じfilterで縮小してpaired residualを測定。7 ROI（512 inputで定義したwhole/shadow/deep shadow/midtone/highlight/smooth/detail）でlinear-light luma/RGB RMS、luma P95/signed mean、RGB8 changed-pixel比。4 seed間のper-pixel luma stdも算出。alphaをresize前にexact-match確認。
- **色/表示:** PillowでRGB8へ変換後、encoded sRGB code value上で各resamplerを実行し共通512-input寸法で比較。Linear-light resamplingとは異なる。grain residualには後段clamp/RGB8との非線形相互作用が含まれる。
- **判定/未検証:** approved target、人手評価、閾値なしの記述研究。製品preview/export、linear-light filter、temporal grain、Core/GPU/性能は未検証。
- **再現:** `python tools/full_suite_photo_grain_resample_filter_study.py`。Artifact.exe起動なし、build/test/CMake/Core/GPU実行なし。

### 観測

- 512表示全体のmedian linear-luma paired residual RMSは、512 native .02250。Lanczosで縮小した1024/2048入力では.01012/.00541。解像度を1024から2048へ倍増したときのmatched residual比は、BOX .522、BILINEAR .559、BICUBIC .535、LANCZOS .530で、全80 photo/preset/seedペアで2048側が低かった。
- 同じscaleでresampler差も再現した。2048ではmedian RMSがBOX .00595、BILINEAR .00432、BICUBIC .00497、LANCZOS .00541。全80 matched pairsでBOX residual > BILINEAR、median ratio 1.427。Lanczosもbilinearより全80組で高く、median ratio 1.280。これは現suite grain出力の非線形clamp/8-bit値をfilter処理した結果であり、filterの一般品質順位ではない。
- 1024でもfilter順序は同様：BOX .01134、BILINEAR .00756、BICUBIC .00929、LANCZOS .01012。BOX/BILINEAR median ratio 1.536。従って2048での半減傾向に対し、filter選択は同じscaleで約1.1〜1.4倍の差を作った。
- 512 seed4条件のwhole-image median luma RMS .02250から1024/Lanczos .01012、2048/Lanczos .00541へ低下。seed-spread RMSも512 .02247、1024/Lanczos .01005、2048/Lanczos .00510。これらのseedは4つの決定的still sampleであり、時間方向flicker/確率母集団推定ではない。
- Lanczos whole-image residual内のdetail/smooth ROI median RMSは512 .03396/.01259、1024 .01494/.00566、2048 .00773/.00309。defined source-gradient領域ではdetail側がsmooth側より高い応答を示した。
- 全720 paired renderでalpha一致。overview/detail sheetと有限値JSONを保存。

### 判断・次

- このPython photo suiteでは、縮小倍率を上げる効果がgrain residualを約半減させ、filter選択も一貫した追加差を生んだ。現行の高解像度grain差減衰はLanczosだけでは説明できず、filter間差を考慮してもscale依存性が残る。
- Coreへ反映すべき仕様はまだ特定されていない。次は同一線形light noise planeの理論的kernel responseと現suite residualを照合し、quantization/clamp由来の差を分離する。実サイズ表示での人手比較と製品経路確認が必要。Core変更なし。


## 実験 111: 制御noise planeでresampler・transfer・RGB8量子化を分離

### 再現条件と計画

- **関連:** `tools/grain_controlled_plane_resample_study.py`、実験110のfull-suite resampler比較、実験109のpaired residual。
- **問い:** 白色grainの縮小RMSが、resampling kernel、linear/sRGB transfer空間、8-bit量子化でどのように変わるか。
- **fixture:** 空間texture/stageのない一様linear-light grayscale planeにiid Gaussian noiseを加算。base linear値 .18/.50/.82、sigma .01/.05、long-edge 1024/2048を512表示へ縮小。BOX/BILINEAR/BICUBIC/LANCZOS、seed 7331–7338。1152 condition records（384 filter/plane/seed組×3 domain）。
- **domain:** (1) unclamped linear float filter、(2) linear input clamp→sRGB transfer→float filter→decode、(3) 同経路へRGB8 quantizeを追加してからfilter/decode。sRGB RGB8のno-grain baselineは同じbaseをsRGB transfer・8-bit quantize・filter・decodeして用い、基準値のrounding biasをpaired residualへ混ぜない。
- **metrics:** post-resize linear residual RMS/P95/signed mean、input sigmaとのRMS比、resize前linear planeのclamp occupancy。seedは独立な決定seed群だが、8回はtail/confidence推定ではない。visualはseed7331のfixed×20 heatmap。
- **判定/限界:** 制御モデル上のkernel/domain診断で、retro lookの品質正解や推奨filterを定めない。Pillow mode F/RGBのfilterを製品/GPU実装と同一視しない。空間分布・写真内容・temporal grain・HDR・performanceは未検証。Artifact.exe/build/test/CMake/Core/GPU不使用。
- **再現:** `python tools/grain_controlled_plane_resample_study.py`。

### 観測

- unclamped linear-float white noiseはscale doublingで予想どおり振幅が約1/2となった。2048→512でoutput/input sigma ratioはBOX .2500、BILINEAR .1643、BICUBIC .2036、LANCZOS .2234。1024→512では各々.4997/.3127/.4042/.4466。これはPillowの各filter kernelが独立white-noise入力を平均する応答で、full suite値の原因をすべて説明するものではない。
- Transferだけをlinear-floatと比較するとbase=.50/sigma=.05でRMS比はおよそ1.00–1.02に留まるが、dark base=.18/sigma=.05では2048条件でfilterごとに約1.09–1.17へ増えた（BILINEAR .1643→.1917）。transferの曲率と入力clampとの相互作用はdark/large-noise条件で無視できない。
- RGB8 quantization追加の影響は入力sigmaが小さく縮小倍率が大きいと増える。2048/base=.50/sigma=.01/BILINEARではoutput/input sigma ratioがsRGB float .1644からsRGB RGB8 .4002へ増加。8-bit出力のquantization floorにより、縮小後のgrain residualがunclamped linear予測より大きく残るstress caseだった。sigma=.05/base=.50では同filter ratio .1674→.1883と差が小さくなる。
- base=.18/sigma=.05/2048/BILINEARではlinear .1643、sRGB float .1917、sRGB RGB8 .2033。8-bit quantizationとsRGB transferはともに寄与するが、dark/strong caseでtransfer/clamp寄与のほうが大きい。このconditionのinput low-clamp pixel比は約0.0155%で、全画像の大半がclampされているわけではない。
- grainなしbaseを対応domainでpairedし直すと、sRGB量子化baselineの定数biasは除かれる一方、noiseへの量子化応答は残った。RMSのみでは非線形歪みや視覚品質を判定しない。
- 全1152 records finite、heatmap sheetを生成。

### 判断・次

- 実験110で見たscale/filter差は、制御white noiseだけでもkernel平均化により再現する。full-suite固有の追加差はquantizer後段grain、RGB8化、dark-tone transfer/clamp等の相互作用と整合する。小さいsigma/大きい縮小ではRGB8 quantizationがgrain応答を保持・変形させうるため、「downsampleは常にwhite-noise RMSを理論kernel比へ落とす」と製品へ一般化できない。
- Core変更を支える目標画質・product pathの証拠はまだない。次は実suiteのpaired source residualにsRGB-float counterfactualを適用し、どの素材/preset/ROIで8-bit quantization floorが支配するかを照合する。


## 実験 112: Full-suite grain residualにおける最終RGB8量子化のcounterfactual寄与

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_quantization_counterfactual_study.py`、実験109 paired residual、実験110 resampler sweep、実験111 controlled plane。
- **問い:** 現suiteのgrainを最終RGB8 conversion前で保持したcounterfactualと比べ、current RGB8 outputがdisplay-scale paired grain residualをどのpreset/ROIで変えるか。
- **素材/条件:** 既存provenance/SHA検証済み写真5点、PRESETS 4つ、long-edge 1024/2048、BILINEAR/LANCZOS縮小で512px入力寸法へ出力。selected seed 31111/31116/31121/31126。80 groups/320 grain-on paired rendersと各groupのgrain=0 control。source luma/gradientで定義した7 ROI。
- **再現/parity gate:** `apply_retro`のstage順をPython上でpost-grain clampまで再現。そこから`np.uint8(rgb*255)`を適用した出力が現行`apply_retro` RGB8とgrain-on/off全caseで完全一致することを先にassert。このgate以降のsRGB float counterfactualはアプリに存在するpathとは限らない。
- **比較経路:** 共通512 display上で、pre-byte float sRGB channelsをPillow mode-Fで縮小する経路と、現行のtruncating RGB8化後Pillow RGB resize経路を比較。各々同domainのgrain=0 outputをbaselineとし、linear Rec.709 luma residual RMS/P95/mean、RGB8/float RMS比、およびpaired residual差RMSを測定。float filter後は[0,1]へclamp。quantization difference heatmapは固定×64 display gainで飽和し得る。
- **評価状態:** output RGB8 parity、alpha passthroughは全条件で確認。意図した見た目、人手評価、合否threshold、GPU/Core/preview/export、性能・memory、temporal coherenceは未検証。
- **再現:** `python tools/full_suite_photo_grain_quantization_counterfactual_study.py`。Artifact.exe起動、build/test/CMake/Core/GPUなし。

### 観測

- Whole-image median paired luma RMSのRGB8/float比（全20 photo/preset×4 seed）は、1024/Lanczos 1.002、1024/Bilinear 1.014、2048/Lanczos 1.029、2048/Bilinear 1.055。強い2:1縮小の2048条件ではRGB8化による増分がBilinearで相対的に大きかったが、絶対のpaired RMS差とは区別する。
- 2048/Bilinearのpreset別whole-image median ratioはhandheld 1.152、arcade 1.160、print 1.039、VHS 1.018。2048/Lanczosでは1.075/1.086/1.020/1.008。presetのgrain sigmaはhandheld .025、arcade .015、print .035、VHS .04であり、相対quantization responseの順はsigma順にならない。stage値域・内容・clamp/filterとの相互作用が残る。
- 2048/BilinearのROI median ratioはsmooth 1.069、detail 1.058、midtone 1.058、deep shadow 1.032、highlight 1.083。Lanczosではwhole 1.029、smooth 1.050、detail 1.028、midtone 1.030、deep shadow 1.014、highlight 1.042。highlight有効標本は64/80 pairsで他ROIより少なく、source mask領域の混合/画素数制約がある。
- 2048/Bilinearのquantization interaction luma RMS（2経路のpaired residual差分）のmedianはwhole .00139、highlight .00312。これはquantization前後のnoise response差であり画質誤差ではない。sRGB float RMSはwhole .00391、RGB8 .00432だった。
- 1024ではfloat/byte差が小さくpresetごとに符号/大きさが変わり、LanczosではVHS whole median ratio .999、Bilinearではhandheld 1.040、arcade 1.051。byte化はあらゆる条件でgrain差分を単純に増幅するわけではない。
- 同一scene/detail cropsではfloatとRGB8出力の大局像は似ている。×64差分heatmapは微小な対応残差を視認するための表示で、飽和/増幅を含み、客観画質の合否画像ではない。
- 全grain-on/offでRGB8 parity exact、alpha exact-match。Finite JSON、paired output sheet、quantization-interaction heatmapを保存。

### 判断・次

- 実験111のcontrolled planeで見たsmall-grain/strong-downsample quantization感度はfull suiteでも確認できたが、増分はpresetとROI依存で、1024では小さく、2048/Bilinearのhandheld/arcadeで強まった。controlled noiseの結果を写真へ直接移せないことも確認された。
- このcounterfactualは`apply_retro`の実RGB8 outputに対する説明用で、float sRGB pathを採用する根拠ではない。実Core/GPUのbuffer precision・resize経路・user-facing display scaleと一致する証拠、approved appearance target、人手判定がないためCore変更なし。次はArtifactCore等を変更せず、grain amount/scaleを別factorとして同じparity-gated replayで分けるか、suite内のpresets比較を拡張する。


## 実験 113: Full-suite grain amount × 解像度のfactor response

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_amount_resolution_study.py`、実験112のRGB8-parity-gated float replay、実験108/109/110のseed・paired residual study。
- **問い:** Grain sigmaだけを変えたとき、512表示でのgrain residualはinput resolutionとpresetに対してどれだけ比例するか。
- **素材/条件:** SHA検証済み同5写真、4現行preset、input long-edge 512/1024/2048、current preset grain sigmaの0.5/1/1.5/2倍、selected seeds 31111/31116/31121/31126。5×4×3×4=240 source/preset/size/factor groups、各4 seedsで960 cases。512寸法へ表示正規化し、1024/2048はLanczos。
- **paired baseline:** 同じsource/preset/sizeのgrain=0 suite出力。grain以外のstage/値を固定し、7 source luma/gradient ROIでlinear-light Rec.709 luma residual RMS/P95/signed mean、RGB8/float RMS比、quantization interaction RMS、changed pixelと表示後端点channel比を記録。
- **parity:** Grain-off baseline全60 source/preset/sizeと、nominal factor=1の各source/preset/size/seedで`render_prebyte_float`の`np.uint8(float*255)`が`apply_retro` RGB8とexact match。Nominal alpha exact matchも確認。sigma counterfactualは同一 replay式でgrain scalarのみ変えるが、変更sigmaそのものはcurrent preset APIの既定値ではない。
- **visual/制約:** selected seedのfloat/RGB8 overview/detail crops。表示端点率は512共通viewで測り、native pre-resize clipping率ではない。approved target、人手評価、thresholdなし。temporal grain、GPU/Core、性能、display calibration未検証。Artifact.exe/build/test/CMakeなし。
- **再現:** `python tools/full_suite_photo_grain_amount_resolution_study.py`。

### 観測

- 512 view上のRGB8 paired grain luma RMSはsigma倍率へほぼ比例した。たとえば512入力handheldはfactor .5/1/1.5/2で.00931/.01858/.02794/.03742（nominal比 .501/1/1.504/2.014）。同VHSは.01507/.03014/.04560/.06151（.500/1/1.513/2.041）。5写真×4 seedの中央値で、endpoint・8bit変換による小さなずれを含む。
- 2048入力では縮小後応答が相対的に圧縮された。handheldは.00272/.00472/.00685/.00899（nominal比 .577/1/1.452/1.906）、arcade .00172/.00266/.00373/.00484（.647/1/1.403/1.823）、print .00352/.00652/.00953/.01250（.540/1/1.461/1.917）、VHS .00353/.00678/.01007/.01339（.521/1/1.485/1.976）。sigmaを半分にしたときの応答は全presetで半分より高く、factor2でも倍未満。強い縮小のsmall-grain quantization floor/clamp相互作用と整合するが、これだけで原因を一意に確定しない。
- 2048/Lanczosでのmedian RGB8/float response ratio（4 presetのpreset-level中央値）はfactor .5/1/1.5/2で1.153/1.047/1.019/1.009。quantization interaction RMS絶対値中央値は各factorで約.00141とほぼ一定で、grain responseが大きくなるにつれ相対寄与が小さくなった。これは実験111のcontrolled-plane small-signal挙動および実験112のpreset差をfull suite factor sweepで補足する。
- 512 native、1024/Lanczosではfactor別の RGB8/float ratio中央値は概ね1近傍（512 .996–1.002、1024 .997–1.026）。2048でもLanczosに限定した結果であり、実験112でBilinearがより高いRGB8差を示したことと矛盾しない。
- 2048のsmooth/detail response (factor1→2) はhandheld .00403→.00757/.00581→.01130、arcade .00237→.00433/.00345→.00648、print .00565→.01112/.00856→.01686、VHS .00605→.01199/.00913→.01809。source-defined detail ROIでsmoothより強いgrain residualが続く。これはROI別出力応答の説明で、知覚上の望ましさではない。
- 240 groups/960 seed casesを出力。全nominal RGB8 replay gateとalpha gateを通過し、全4 factor×3 resolution列のoverview/detail sheetとfinite JSONを保存。

### 判断・次

- sigma変更はfull suiteのgrain residualを概ね連続的に動かしたが、512→2048縮小後はlow sigma域の増減が比例しにくく、RGB8 quantizationの相対寄与が大きい。単純に解像度比でsigmaを補償する根拠にはならず、Bilinear/Lanczosでも応答が異なる。
- この研究はPython static photo modelに限定され、grain sigmaの製品推奨やCore変更を支持しない。次は同一短尺motion sequenceでseed/coordinateを固定したときのframe-to-frame grain consistencyを独立評価し、still noise responseとtemporal flickerを分ける。


## 実験 114: 写真panにおけるgrain座標方針の時間安定性

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_motion_policy_study.py`、実験092のscreen/world/object coordinate診断、実験093のsynthetic temporal correlation、実験108–113の写真grain paired response。
- **問い:** 動く写真sceneに対して、現在の同seed screen-fixed grain、毎frame seed更新grain、素材座標へ固定したgrain fieldで、grain-only residualの時間安定性はどう異なるか。
- **素材/sequence:** provenance manifestを参照しSHA-256一致を検証した写真3点（portrait、high-ISO monochrome portrait、sun/clouds）。EXIF方向補正、embedded ICC→sRGB、untaggedはsRGB仮定。long edge 1280にLanczos縮小後、中心付近から512×512 cropを24枚切り出し、source xまたはyを2pxずつ移動。4現行preset、base seed 8411、frame seed step 977。
- **policy:** `screen-fixed`はcurrent `apply_retro`をseed 8411で全frameに適用。`frame-varying`はseed `8411+977×frame`。`source-locked`は同じGaussian delta planeをsource座標でcropし、現suiteのnon-grain stagesを通したprebyte float値へ加算。後者は既存APIの挙動ではなく座標固定仮説を調べるcounterfactual。
- **paired residual/時間整列:** 各frameごとに同crop/presetのgrain=0 outputをbaselineとし、grain-on RGB8との差をsRGB decode後のlinear-light RGB/luma residualとして計算。隣接frameをscreen同座標、および共通source overlapで比較。双方grain-off lumaが.05–.95の画素のみ有効とし、RMS/P95/correlationとpixel countを出力。grain-on residualへ非grain差分を直接混入しない。
- **視覚/parity:** source-lockedのlocal Gaussian delta planeをcurrent `apply_retro` grainと同seed・同cropで比較しRGB8 exact matchを12/12 source/presetで確認。24×3 policiesのcontact sheetをsource/presetごとに出力し、代表portrait sheetを目視確認。Opaque sourcesでalpha passthroughを全frame/policyに確認。
- **判定/制約:** 864 frame records、1,656 screen/world adjacent-pair records。approved target、observer、acceptance thresholdなしの記述研究。整数translationのみでsubpixel、rotation、occlusion、scene cut、motion-estimation errorsなし。grain fieldはnon-grain stages後段に適用したcounterfactual。Python/PillowのみでArtifact runtime、Core/GPU、preview/export、性能/memory/display calibration未検証。
- **再現:** `python tools/full_suite_photo_grain_motion_policy_study.py`。Artifact.exe起動、build/test/CMake/Core/GPUなし。結果は`captures/full-suite-photo-grain-motion-policy/grain-motion-policy-study.json`と同folderの12 contact sheets。

### 観測

- 276隣接pair（3素材×4 preset×23 pair）の中央値で、screen-fixedはscreen整列luma residual delta RMS .00300、correlation .9965。source/world整列ではRMS .05168、correlation .0018。画面に貼り付いたfieldがviewport上でほぼ一定である一方、素材が通過すると別grain位置が現れる。
- Frame-varyingはscreen/source両整列でRMS約.0519、correlation約.002。各隣接frameで独立なseedを使うため、両座標系でgrain residualが更新される。
- Source-lockedはsource/world整列RMS .000352、correlation .9998。screen整列ではRMS .05201、correlation .0044であり、grain field自体が素材と共に移動する想定と整合する。screen-aligned source-lockedの差分には、動く画像内容とsuite stageのscreen phase等も残るため、単純なgrain flicker値とは読まない。
- Source-locked world-aligned median RMSはpreset別にhandheld .000015、arcade .000757、print .002227、VHS .000031。sigmaだけで単純には並ばず、RGB8 quantization、clamp、各preset stageとの相互作用を含む。説明的な値で品質順位ではない。
- Visual contact sheetではpanに伴う内容移動が見え、screen-fixed / source-lockedは粒状パターンの座標関係が対照的に見えた。これは縮小sheetでの定性的所見であり、知覚受容性テストではない。

### 判断・次

- 実験092/093で別々に確認していた座標固定とtemporal correlationを、SHA検証済み写真および現行4 presetのgrain residualで接続した。screen-fixedとsource-lockedはいずれも自分の固定座標系において非常に高い残差相関を示す。従って「固定seedならtemporal stable」という主張には座標系を明記する必要がある。
- どの座標方針がretro lookとして望ましいかは未判定。素材移動とgrainの相対挙動、pixel-art fixed screen texture、カメラ移動等の製品意図と受入画像を定める必要がある。Coreに必要な仕様やGPU pathの根拠はなく、変更なし。


## 実験 115: 移動RGBA cutoutのgrainとalpha edge応答

### 再現条件と計画

- **関連:** `tools/grain_motion_alpha_edge_study.py`、実験092の移動clover/feather、実験095/096のgrain-alpha/hidden RGB静止画比較、実験114の写真pan座標比較。
- **問い:** Grainをscene合成後へ置く場合とRGBA layer上で合成前に適用する場合で、動く半透明edge・object外背景・object座標時間差はどう変わるか。
- **素材/条件:** SHA-256/provenanceを確認したCC0 `clover_full`, `silver_feather`, `pixel_hair_atlas`。alphaを維持して96×96へLanczos縮小/透明canvas中央配置。256×192 scene、20 frame、integer move x+=4/y+=1。neutral-grayとdeterministic spatial-texture背景。現行4 preset、seed48117、frame-vary seed increment431。
- **policy/paired reference:** `screen-fixed`/`frame-varying`は各frameでspriteを背景へ合成し、current `apply_retro`をopaque composite全体へ適用、同frame/presetのgrain=0 sceneを基準にする。`layer-material-fixed`はRGBA spriteへcurrent `apply_retro`を一度適用してから同一背景へ移動合成し、同spriteのgrain=0処理を基準にする。各route内のpaired residualはgrain insertionの応答を測るが、stage orderが異なるためroute間の画質順位ではない。
- **regions/statistics:** source alpha `>=250` interior、`0<alpha<250` partial pixels、MaxFilter/MinFilter 5×5によるvisible edge band、all-visible、outside-object。same-frame RGB8 on/offをdecodeしてlinear-light RGBからRec.709 lumaを算出し、RMS/P95/changed-pixel ratio。24 groups×20 frame×3 policiesのper-frame records、隣接frame residualをobject-local座標で揃えたRMS/correlation。
- **alpha/color:** layer on/off 48条件でalpha plane exact。RGBA合成はPillow alpha_compositeのencoded sRGB values。linear-light composite、premultiplied asset import、HDRは未評価。alpha-zero hidden RGBへのgrain leakage/consumer pathは別studyが必要。
- **visual/evaluation:** 24 group別overviewと2× sprite edge cropを生成。Feather/neutral-gray/handheld detail sheetを確認。approved target、独立observer、thresholdなしの記述研究。integer translationのみ。Artifact runtime/Core/GPU、preview/export、性能/memory未検証。
- **再現:** `python tools/grain_motion_alpha_edge_study.py`。Artifact.exe起動、build/test/CMake/Core/GPUなし。JSONと48 sheets/cropsは`captures/grain-motion-alpha-edge/`。

### 観測

- 24 groups（3 source×2 background×4 preset）、1,368 adjacent-pair records、alpha exact gate 48。各policyの456 object-aligned pairsを集約したmedian luma residual delta RMS/correlationは、screen-fixed .03416/−.0181、frame-varying .03346/.00076、layer-material-fixed .000286/.99980。layer fieldはsprite座標に固定される一方、scene-space fieldは移動spriteに対して同じgrain sampleを維持しない。これは実験条件上の挙動で知覚品質順位ではない。
- 全preset/group/frameのmedianを集計すると、partial-alpha ROIのgrain-on/off residual luma RMSはscreen-fixed .02275、frame-varying .02291、layer-material-fixed .01028。visible 5×5 edge bandでは.02183/.02187/.00974。encoded-sRGBモデルではlayer grainが後段のRGBA合成でalphaと関連付けられ、partial edgeで相対的に小さい差となった。preset別partial-alpha medianはhandheld .01895/.01862/.00752、arcade .01144/.01137/.00453、print .02993/.02943/.01329、VHS .03201/.03197/.01229（各行screen-fixed/frame-varying/layer-material-fixed順）。
- partial-alpha maskのRGB8 changed-pixel比medianは.9530/.9545/.5640。scene全体へ適用した方針はobject外背景にもgrain residualを持ち（median RMS .02107）、layer-material方針のobject外residualは構造上0となった。grain量が同じでも、layer/composition境界のどちらへ挿入するかで空間coverageが異なる。
- Layer alphaはon/offで完全一致。contact sheetではgrain配置とedge coverageの差を確認したが、calibrated continuous playbackやobserver panelは行っていない。

### 判断・限界・次

- 実験092のopaque interior motion maskおよび実験114の写真pan結果に、alpha edgeとlayer/composition挿入位置の証拠を加えた。grain field自体は各座標系で時間安定していても、半透明素材をgrainが覆う範囲は合成の前後で変わり得る。
- encoded-sRGB合成、異なるstage順、layerのgrainを固定seedで一度処理する設定、approved look不在が視覚選好の解釈に影響するため、数値を製品方針へ移さない。次はsensor/composition grainかmaterial textureかの意図、source layerのalpha-mode/content条件、native scaleの連続目視を明確にする。現時点でCore変更は支持されない。


## 実験 116: GrainのRGB channel covarianceとluma/chroma response

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_color_covariance_study.py`、実験111–113のquantization/scale応答、実験114–115のgrain coordinate/composite coverage。
- **問い:** grainの投入RGB powerを同等に保ったとき、common-channel monochrome、独立RGB、luma-null chroma noiseは現行suiteの後段処理と512表示でluma/chroma residualをどう変えるか。
- **素材/条件:** 既存provenance/SHA照合済み自然写真5点（portrait、high-ISO B&W、Milky Way、clouds/sun、blue sky/clouds）、4 current preset、input long-edge 512/2048、seeds 31111/31116/31121/31126。EXIF向き補正、embedded ICC→sRGB、untaggedはsRGB仮定。2048 outputをencoded-sRGB RGB8 Lanczosで512入力と同じ寸法へ縮小。5×4×2=40 source/preset/size groups、各4 seeds・3 variantsで480 variant records。
- **variants:** `current-monochrome`はcurrent `apply_retro`と同じseeded scalar Gaussian fieldをRGB全channelへbroadcast。`independent-rgb`は同sigmaで3 channel独立Gaussian。`zero-luma-chroma`はRec.709重みに対しsRGB code-value additive domainでluma-nullとなる直交RGB基底へ2 independent Gaussian planeを射影し、平均channel input varianceをmono/independentと合わせる。全てsRGB code値の同一grain stageへ加え、noise追加以外のcurrent stage/preset値を固定。
- **pairing/統計:** 同size/presetのgrain=0 current suite outputとpaired。reference512 sourceのlinear-luma/gradientからwhole/shadow/deep shadow/midtone/highlight/smooth/detail 7 ROIを定義。RGB8後sRGB decodeでlinear RGB/luma、chroma axes `R-G` と `B-(R+G)/2` のRMS、luma signed mean/P95、chroma P95、changed-pixel比を記録。各seed recordにpixel countを保持。crop座標は既存の5-photo studyに合わせたnormalized detail crop。
- **parity/visual:** `render_prebyte_float`へmono 2D deltaを渡し、current `apply_retro` RGB8とsource/preset/size/seed全160条件でexact matchをassert。20 overview/detail sheetsを生成しportrait/VHS detail sheetを確認。alphaはopaque RGB写真のため対象外。HDR、human preference、display calibration、product path、Core/GPU、性能/memoryは未検証。
- **合否/再現:** approved appearance target、perceptual observer、thresholdなしの記述研究。`python tools/full_suite_photo_grain_color_covariance_study.py`。Artifact.exe起動、build/test/CMake/Core/GPUなし。JSONと20 overview + 20 detail JPEGは`captures/full-suite-photo-grain-color-covariance/`。

### 観測

- 160 current-mono parity gateが全て一致。512 input/512 displayのwhole-image median luma/chroma RMSは、current-mono .02250/.00576、independent-RGB .01685/.03061、zero-luma-chroma .00213/.03344。投入段のchannel RMSを揃えても出力luma/chroma配分は大きく異なる。
- 480 records全てでpre-clamp input field統計も有限値として保存。各variantのmean per-channel code-noise RMS medianはcurrent-mono .02992、independent-RGB .03000、zero-luma-chroma .03002と近い。一方code-domain Rec.709 luma RMSは.02992/.02250/9.04e-10、chroma-axis RMSは0/.03969/.04334。chroma-only nullは投入配列上で1e-6以下のmax-absolute gateを通過。
- 2048 inputを512へLanczos縮小したwhole-image median luma/chroma RMSは、current-mono .00541/.00212、independent-RGB .00422/.00704、zero-luma-chroma .00148/.00768。強い縮小後もnoise covariance差がlinear-light residualへ残る。これは5素材・4 preset・4 seedsのmedianであり、各noise realizationのconfidence intervalや知覚均等を示さない。
- 512表示smooth/detail ROIのluma RMSはcurrent-mono .01259/.03396、independent-RGB .00943/.02547、zero-luma-chroma .00152/.00254。2048入力ではそれぞれ .00309/.00773、.00262/.00595、.00118/.00167。source gradientで定義したdetail側のeffect responseが大きいが、良否や見えやすさとは同じではない。
- zero-luma-chromaの入力fieldはsRGB-code additive domainでluma重みに直交する。一方出力luma residualは0にならず、sRGB decodeのtransfer、clamp、RGB8 truncation等の後段非線形性が残る。独立RGBのlinear lumaが同sigma monoより小さいのもchannel covarianceから予測できるが、出力値はcurrent suite stageとのinteractionを含む。
- 20 source/preset overviewと20 crop sheetを生成。portrait/VHS cropでは512 inputのfine noiseと2048→512のattenuationを視認でき、独立/chroma variantは色grainが増えた。小縮小sheetでの作者reviewのみであり、native scale連続鑑賞・色差好み評価ではない。

### 判断・限界・次

- Grain amountとspatial scaleだけでなくRGB channel covarianceも独立応答軸である。現在のcommon-channel grainはlinear RGB channel差を比較的小さく保つ傾向があるが、quantization・chroma offset・source色とのinteraction後も自然なlookであるかは未確定。
- 変種は同じ期待pre-clamp per-channel code-noise varianceを持つだけで、post-clamp RGB RMS、知覚強度、写真grainの物理モデルを一致させていない。independent/chroma modeをpresetやCore APIへ足す根拠にはならず、Core変更なし。
- 次に必要なのは、承認済みの参照lookまたは対比較観察条件を先に定め、同一display scale・局所detailで色grain量を知覚等価に調整したうえでblind preference/defect reviewを行うこと。必要性が確認されるまでは既存mono grainのままとする。


## 実験 117: Current mono luma RMSへ合わせるgrain covariance校正

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_equal_luma_calibration_study.py`、実験116のchannel covariance、実験112/113のRGB8・sigma応答。
- **問い:** 投入noise varianceではなく、共通512表示で得られるcurrent monoのwhole-image luma RMSへ別channel covarianceを合わせると、必要sigmaと残るchroma/headroom差はどうなるか。
- **data:** 実験116と同じSHA検証済み写真5点、4 preset、input long-edge 512/2048。各source/preset/sizeでtrain seeds 31111/31116、held-out seeds 31121/31126。40 fit groups、計160 seed/preset/sizeのcurrent mono RGB8 replay。
- **fit:** 目標は各fit groupのcurrent `apply_retro` mono noise-on minus noise=0の、共通512表示 whole-image linear Rec.709 luma RMS（2 train seedsのmedian）。independent-RGB / code-domain luma-null chromaをpreset sigmaの1/2/4倍で各train seed測定し、zero-responseを含むpiecewise-linear interpolation。目標がprobe rangeでbracketされない場合はfactor4上限を採用、extrapolateしない。ROI maskはreference512入力のsame luma/gradient rulesを使用。
- **held-out:** fit固定後、各validation seed自身のcurrent mono RMSをそのseedの目標として比較（train medianは併せて記録）。whole/shadow/deep shadow/midtone/highlight/smooth/detailでlinear luma/chroma, P95, changed ratio, final RGB8 endpoint channel occupancyを計測。160/160 mono replayがcurrent outputとexact match。
- **visual/evaluation:** 20 source/preset overviewと20 detail cropsを出力。目標は数値RMS一致の評価用基準で、知覚等価ではない。approved image、observer、許容範囲なし。RGB8 display endpointはnative pre-clamp clipping原因を示さない。HDR/alpha/Core/GPU/product path/performance未検証。
- **再現:** `python tools/full_suite_photo_grain_equal_luma_calibration_study.py`。JSONは`captures/full-suite-photo-grain-equal-luma-calibration/`。

### 観測

- Independent RGBは40/40 fit groupでtargetをbracket。校正sigma factor median 1.322（P10/P90 1.254/1.355）。held-out 80ケースでは512入力luma RMS/seed mono target比median 1.001、2048では.995。matched lumaを得てもchroma-axis RMSは512 .04072（mono .00576）、2048 .00911（mono .00212）と高い。endpoint channel比medianは512 .04363（mono .03700）、2048 .000103（mono .000233）。
- Chroma-only factor1/2/4ではtargetをbracketできたfit groupが7/40に留まり、factor4をcapにした。held-out luma target ratio medianは512 .532、2048 .711。Chroma RMSは512 .12539、2048 .02734。512 endpoint channel比.08491（mono .03700）となり、未達条件を抱えたまま色差・endpoint occupancyが増えた。
- **校正上の修正:** 初回集計ではheld-out candidateをtrain targetと比較していた。最終script/reportはcandidateごとにheld-out seed自身のcurrent-mono RMSを使って再実行した。従って上記値はseed対応のpaired target ratioである。

### 判断・次

- Independent channel noiseはsigmaを約1.32倍にすることで本測定のwhole luma RMSを別seedで概ね合わせたが、chroma residualはmonoより大きく残った。Luma-null chroma noiseはfactor4以内では大半のconditionでluma targetに届かない。
- Factor4 capにより、chroma-onlyの応答・必要sigmaは未確定。さらにnoiseを増やす価値があるかはheadroomを伴うため、factor8を追加probeし、bracket可能性とheldout chroma/clamp proxyを調べる。まだCore変更を支持しない。


## 実験 118: Chroma-only grainの出力luma/headroom frontier

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_chroma_headroom_study.py`、実験117のfit/held-out validation。
- **問い:** code-domain luma-null chroma grainのsigma上限をfactor4からfactor8へ延ばすと、current mono output-luma targetへ到達するか。到達する場合、linear chroma residualとRGB8 endpoint occupancyはいくら増えるか。
- **手順:** 同じ5写真×4 preset×512/2048=40 group。実験117のfactors1/2/4の2 train-seed responsesへfactor8 responseを追加し、factor0/1/2/4/8 curveを補間。validation seeds 31121/31126ではfitを変えず、factor4 fixedとcalibrated candidateを同seedのcurrent mono outputとpaired比較。common 512 view、同grain-off baseline、7 ROI、whole RGB8 endpoint channelsを使用。
- **結果量/画像:** 40 calibration rows、240 validation records、source/preset別20 overview +20 detail sheets。alphaはopaque sourceのため対象外。endpoint occupancyはRGB8表示値のcode 0/255率で、native pre-clamp grain clippingの直接計測ではない。知覚基準なしの記述研究。
- **再現:** `python tools/full_suite_photo_grain_chroma_headroom_study.py`。基準sourceとして実験117のignored JSONを読み、出力は`captures/full-suite-photo-grain-chroma-headroom/`。

### 観測

- Factor8までにtraining targetをbracketしたのは34/40 groups。中央値calibration factorは5.856、P10/P90は3.579/8.0。6未達groupはface portrait/arcade/512・2048、high-ISO B&W/arcade/512、clouds-sun/arcade/512、blue-sky-clouds/handheld/2048・arcade/2048。これらはfactor8でもtraining targetに届かず、factor8 capを保持した。
- Held-out 80ケースのcalibrated luma RMS/同seed mono target median ratioは.969（absolute ratio error中央値.0308）。解像度別では512 .965、2048 .972。whole-image RMSは概ね揃えられるgroupが増えたが、34 bracket groups限定でtarget一致を保証するわけではない。
- Calibrated chroma-axis RMS medianは全80で.07166、mono同seed median .00307。512では.18397（mono .00576）、2048では.03400（mono .00212）。目的lumaへ近づけても色差は大幅に残る。
- Calibrated RGB8 endpoint channel occupancy medianは全80で.00636、mono .000562。512では.12668（mono .03700）、2048では.000020（mono .000233）。入力/解像度で応答が逆方向にもなりうる。display RGB8端点は色のclip発生源をgrainだけに特定しない。
- Factor4 fixed candidateのwhole luma ratio medianは.550、chroma RMS .05783、endpoint .00130。校正候補はluma matchを改善した一方、chroma RMS/endpointを増やした。Detail sheetでは512 portrait cropに強い色grainが見えた。calibrated monitorや連続表示、人の好み比較ではない。

### 判断・限界・次

- Code-domain luma-null chroma noiseは小sigmaではoutput luma responseが弱く、mono luma targetへ寄せるためsigmaを上げるとchroma residualが大きくなる。さらに強い出力条件は可能でも、等lumaは同じlookを意味しない。今回のluma RMS targetはcalibration statisticであってperceptual equivalence criterionではない。
- Clipping/headroomをgrain起因と断定できるnative pre-clamp channel excursionは未計測。高sigmaを製品採用する根拠もない。色grainのblind preference/defect review、局所強度上限、alpha/HDR、Core/GPU pathが未検証のためCore変更なし。現行monoを維持する。


## 実験 119: Grain pre-clamp crossingとRGB8 endpoint遷移の帰属

### 再現条件と計画

- **関連:** `tools/full_suite_photo_grain_preclamp_attribution_study.py`、実験117–118のluma校正grain variant。
- **問い:** grain前にすでに0/1端点だったchannelと、grain後に新たに範囲外へcrossしたchannel、最終RGB8でgrain-offから端点へ移ったchannelを区別できるか。
- **条件:** provenance/SHA照合済み写真5点×4 preset×512/2048入力×held-out seed 31121/31126×4 variant（current mono、luma校正independent RGB、chroma×4、luma校正chroma）=320 records。七つのsource-defined ROIを測定。
- **計測:** grain直前float、後・clip前float、clip後native RGB8を保存。`pregrain>0 && preclamp<0`または`pregrain<1 && preclamp>1`をgrainによる新規range crossingと定義。別にgrain-off RGB8との0/255 endpoint新規/消失とnet変化をpaired計測。2048結果は共通512 displayにも縮小。
- **gate/限界:** mono float replayとcurrent `apply_retro` RGB8の80条件exact parity。2 held-out seedsのみ、知覚欠陥基準なし。RGB8 endpoint遷移はvisible clipping defectとは限らず、suite内encoded-sRGB Python replayに限る。Artifact.exe、build/test/CMake、Core/GPUなし。
- **再現:** `python tools/full_suite_photo_grain_preclamp_attribution_study.py`。JSON/map sheetsは`captures/full-suite-photo-grain-preclamp-attribution/`。

### 観測

- 320 records完了、mono RGB8 parity 80/80。grain前endpoint channel率medianは512で.01438、2048で.01452（全variant共通）。
- Native floatの新規range crossing median（512/2048）はmono .000135/.000148、luma校正independent RGB .000532/.000559、chroma×4 .04449/.04483、luma校正chroma .09348/.08347。
- Grain-off native RGB8 endpoint率medianは.01438/.01452。512/2048それぞれ、grain-onで新規端点へ移ったchannel率はmono .000201/.000202、independent RGB .000667/.000607、chroma×4 .04605/.04647、校正chroma .09501/.08524。既存端点から外れた割合は約.00635–.00700あり、net endpoint率変化は同じ variant間でも新規率と一致しない。
- Current mono native RGB8 endpoint率は.03700/.04000でgrain-offより大きいが、新規float crossing中央値は約.014%。よってRGB8端点の増加だけでgrainがclipしたと判断できない。2048→512表示ではendpoint率が大幅に落ちるため、測定解像度も併記が必要。

### 判断・限界・次

- Clamp前float tapとgrain-off paired RGB8 endpoint遷移を併用すると、stage既存端点、grain-induced range crossing、quantized endpointの三種類を別々に追跡できる。色grain校正候補は等luma校正でもnative headroomを大きく消費するという既存の数値所見を補強する。
- 2 seedの固定holdout中央値であり、確率的分布や知覚閾値の証拠ではない。画像の正常な黒/白を含む0/255 endpointは欠陥を意味しない。ユーザー承認target、表示条件、human review、alpha/HDR、製品経路の検証がないため、Core変更は支持されずcurrent monoを維持する。


## 実験 120: Grain前stageでのblack/white endpoint provenance

### 再現条件と計画

- **関連:** `tools/full_suite_photo_stage_endpoint_provenance_study.py`、実験119のpregrain endpoint finding、`retro_lab.py::apply_retro`。
- **問い:** 現行suiteの0/1 float RGB endpointはgrain前のどのstageで導入・解除されるか。RGB8 0/255 endpointはどのstageへ遡れるか。
- **条件:** SHA/provenance照合済み実写5点×current 4 preset×long-edge 512/1024/2048、seed31111。grainを0にしてpixelation→quantization/dither→channel offset→scanlineの現行順を保持。input、各stage出力、pre-clamp、clip、truncating RGB8相当をfloat channel/pixel any channel両方で計測。
- **transition:** 連続stage間で新規endpoint、解除endpoint、net endpoint ratioをRGB channel母数でpaired算出。512入力表示のstage mapsでは赤=黒endpoint channelがあるpixel、青=白endpoint、黄=同一pixelに黒/白endpointが共存。
- **gate/制約:** grain-off float replay対現行`apply_retro(..., grain=0)`のRGB8完全一致60条件。endpointはpaletteの意図的な出力値も含み、clip artifactや視覚欠陥を示さない。single seed、5 photos、opaque SDR、Python/Pillowのみ。human review、approved target、Core/GPU、性能未検証。
- **再現:** `python tools/full_suite_photo_stage_endpoint_provenance_study.py`。JSONと20 source/preset endpoint mapsは`captures/full-suite-photo-stage-endpoints/`。

### 観測

- RGB8 parityは60/60。現行5×4条件のstage別median float endpoint channel比は、512入力でquantize/dither後1.654%、channel offset後1.445%、scanline後1.438%。1024では1.622%→1.397%→1.387%、2048では1.713%→1.463%→1.452%。grainは無効である。
- 全60条件をまとめた連続stage transition medianではpixelation後→quantization/dither後に1.670% channelが新規endpointとなる。quantizerは`step=1/(colors-1)`のpalette値を出力し0/1を含む。channel offset後にはわずかにendpoint解除が起き、scanlineが有効なpresetでは暗行による白endpoint解除が観測される。scanline無効のprintではそのstageの値は変わらない。
- 端点比はpreset依存。512入力でquantization後/scanline後の5写真medianはhandheld 18.68%/12.50%、arcade 1.18%/.75%、print 9.44%/9.44%、VHS .22%/.22%。暗いportrait/sceneなど一部sourceのendpoint率が高く全体medianを押し上げる。2048でも大枠は同様だが、grain前endpoint mapは実画像のhighlight形状に沿って局所的に生じる。
- Grain-off pre-clamp medianは実験119の値と一致し、stage起源をquantization/ditherおよび後続scanlineの一部endpoint解除まで遡れた。これらはendpoint occupancyの起源であって、grain clip診断と区別される。

### 判断・限界・次

- 0/255 RGB8の存在を「clipping」とまとめると、意図的palette endpoint、stageで範囲端へ写像された値、clampで新たに範囲外から戻った値を混同する。今後のclamp系研究ではpre-clamp excursionとendpoint occupancyを別メトリックとして記録する。
- Quantizerの0/1出力はこの実装で明示される挙動だが、望ましいretro lookか否かは受入参照・目視判定がないため未確定。Core変更なし。次はdither量/palette levelごとのendpoint遷移を調べ、quantizationのintentional endpointと後段offset/scanlineの解除・再導入がどの画像領域で起きるかを因子分解する。
