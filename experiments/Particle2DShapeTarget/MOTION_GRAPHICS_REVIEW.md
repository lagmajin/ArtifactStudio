# 2Dパーティクルのモーショングラフィックス評価

**最終更新:** 2026-11-20

## モーショングラフィックス試作の進捗

2026-11-19に `ArtifactParticle2DPlayground` をDebugでビルドし、Vulkan / NVIDIA GeForce RTX 4070 Tiで1920×1080、30 fps、150フレームのA8タイトルをオフスクリーン取得した。非表示ターゲットから4096点をサンプルし、画面座標はx=719..1201、y=365..715。粒子発生点を中央に置いた条件では吸引は働いたが、frame 59の生存粒子4657個の中心からの平均距離が102pxで、字形は読めなかった。キャプチャは `output/particle-motion/vulkan/title-measured/`、連番解析は同階層の `review/`。

色と形の診断結果: 以前の `renderToQImage()` キャプチャはBGRA読戻し指定によりRGBが反転していた。背景色も期待値 `(53,66,85)` に対して `(85,66,53)`、砂では `(53,47,39)` に対して `(39,47,53)` と赤青が逆だった。Playgroundはcompositionを各フレームへ進め、`renderer.renderFrame()` 後に既存のRGBA `readbackToImage()` を呼ぶ経路へ変更した。同条件の修正画像では背景・砂の色が正しく、Vulkanで確認できた。初期の青い砂画像は粒子GPU色の証拠として扱わない。

Emitter色設定の抜けも修正した。Emitter既定の `colorMid` は白で、タイトル・砂の試作は `colorStart` と `colorEnd` だけを変更していたため、粒子寿命の中間に白へ寄っていた。色を変化させないEmitterでは中間色を開始色に揃えた。タイトルは青灰背景に対する視認性を上げるため明るい青と氷色にし、終端opacityも下げ切らない。Vulkanのframe 149画像ではA8の輪郭と穴が見える。タイトル粒子数はframe 59 / 149とも4347、画面内全数、中心距離平均はframe 59で102.5px、frame 149で150.1px。輪郭の画面占有範囲は約482×351px、画面全幅の約25%。サイズカーブを寿命全体で維持したv20ではA8が明瞭になり、frame 149で4347個、範囲約576×417pxを確認した。形状は判読できるが、粒が太く重なり気味で、黒白背景・連続再生を含む最終採用品質の確認は残る。最新画像は `output/particle-motion/vulkan/title-size-held-v20/review/source149.png`。

シミュレーション経路の確認: captureは各出力フレームで`composition.goToFrame()`を呼び、GPU Particle Layerのdrawは`ParticleSystem::goToFrame(frame, fps, simulationReusable)`を使う。30 fpsの連番では120 Hz固定刻みの境界を保ち、初期化後は前状態から再開できる実装であり、各フレームが毎回ゼロから再生されるわけではない。前の記述「すべて独立シークで、連続再生を評価していない」は誤りとして撤回する。今回のcaptureは連番での決定論的シミュレーションを測るが、実アプリの再生時計・ドロップフレームやD3D12での状態は未検証。30 fpsから60 fps等の異なるstep条件、frame60の直接シークとの粒子状態・画像一致も別途確認が必要。

未検証の改善仮説: 文字ターゲット点から一定距離だけ外側へ粒子の初期配置を作り、発生源を分散させると、集合中の軌跡と完成形を両立しやすい可能性がある。粒子へ固定のターゲット点を割り当てる現方式では、点の再分配・細線被覆・穴の保持も次の比較項目とする。現A8は判読できるが占有面積と密度が小さく、採用品質に達していない。D3D12、連続再生と直接シークの状態比較、文字の黒白背景目視は未完了。

制作シーンは、砂が流れて散る素材と、粒子が文字や図形を形づくるタイトル演出から始める。既存の2D Particle Layerを使い、タイトルは放出・集合・静止、砂は流入・流れの分岐・消散を一続きのショットとして評価する。評価はオフスクリーンの試験プログラムで行う。

| ショット | 現行比較 | 採用条件 |
|---|---|---|
| タイトル | Playgroundの `A8` と穴付きリング、Shape Targetの有効・無効 | 集合の向きが読める。静止区間で穴と細線が読めて粒の揺れや輪郭抜けが目立たない。直接シークで同じ粒子状態に戻る。 |
| 砂の流れ | Playgroundの既存Wind Leavesを風の挙動比較に利用し、砂の初回試作を以後の見た目の基準として保存 | 流れの主方向と粒径差が読める。細粒がちらつかず、粗粒と同じ塊に見えない。発生位置と消散位置で急な切れが見えない。 |

固定seedで30 fpsの連続フレームと直接シークを採取する。既存タイトルfixtureの320×180に加え、制作確認は1920×1080で行う。変更前後で解像度、シーン、seed、露出、表示変換、色空間とalpha形式を揃える。比較の識別情報としてソースcommit、設定、画像hash、GPUとbackendを残す。既存の素材は手続き生成で、外部参照を正解画像として使わない。

制作タイトルの初回試作は `--capture-particle-title output/particle-title/<backend>`。Arial Boldの「A8」を塗り領域のサンプルで中央に配置し、非表示の文字レイヤーへ青系の粒を集める。放出はframe 60まで、収録はframe 149まで。輪郭、文字穴、集合後の残留運動は連続フレーム取得後に判定する。

レビュー担当者は全画面、100%・400%の文字穴／輪郭／砂の流れの端、黒白背景での合成を確認する。全体と各領域のRGB・alpha分布、穴内粒子中心数、静止区間のフレーム差、粒径別のalpha被覆を記録する。明暗合成に共通して見える縁や、静止区間の形を崩す揺れは採用を止める欠陥とする。彩度の高い粒と淡い粒を含め、HDR・半透明は別に記録する。

D3D12とVulkanのオフスクリーン描画で比較し、タイトルのシーク整合と無効時の同一性を別途確認する。1080pで更新・描画の中央値と95パーセンタイル、ピークGPUメモリを計測する。Vulkanのv29/v30を探索baselineとして固定し、次の砂候補を取得する前に次の数値ゲートを適用する。差分画素はRGBA8 sRGB出力を左上の背景RGBと比較し、RGBいずれかが10/255を超えた画素数を1920×1080で割った値とする。bboxは同じ差分画素から得る。

| 領域・フレーム | 受入れゲート | 現行基準の値 |
|---|---|---|
| 流れの本体 frame 59 | 差分画素率8–20%、bbox幅70%以上・高さ60%以上 | v29/v30: 9.82%、幅89.6%、高さ79.0% |
| 終端の余韻 frame 149 | 差分画素率0.5–3%、bboxが存在し、下端から24px以上の空きがある | v30: 1.78%、bbox x=0..1752 / y=599..1080、下端余白0pxで不合格 |
| フレーム間の減衰 | frame 60..100の連続rate設定が単調非増加で100で0。画像は複数フレームを等倍で目視し、粒の移動をちらつきと誤認しない | v30のJSONでは設定条件成立。画像はレビュー済み |

これは砂ショットの探索ゲートで、モーショングラフィックス全体の品質合格を単独で証明しない。タイトルは字形・穴・細線の目視、30 fps逐次状態とframe60直接シークの粒子ハッシュ／画像一致を別ゲートにする。性能は同期readback込み画像取得時間を製品再生fpsの代用にしない。D3D12が有効なビルドを用意できた場合に同一条件で再測定し、実アプリ経路の更新中央値/p95とGPUピークメモリを別に記録する。HDR・透明背景への合成はこの不透明背景ショットでは適用外とし、RGB8表示での半透明粒と暗背景の縁を目視する。

初回の数値ゲートを画像解析スクリプトへ実装し、保存済みVulkan画像に適用した。v29はframe59本体が9.82%で合格、frame149終端は0.045%で不合格。v30は差分画素率だけならframe59が9.82%、frame149が1.78%で範囲内だった。その後の400%目視でframe149の粒子が下端に切られていると判明したため、終端ゲートへ「下端から24px以上の空き」を追加し、以降の候補比較前に閾値変更理由を記録した。改訂ゲートではv30終端は下端余白0pxで不合格、v29は終端粒子がほぼなく不合格。最新レポートは各`review/sequence_report.json`の`predeclared_acceptance_gates`にある。数値範囲の合格は目視レビューやリアルタイム性能の合格を意味しない。

v30の目視レビュー（Vulkan、1920×1080、暗褐色背景）: frame59は広い斜めの砂流、frame90/120/149は下方向へ移るフェードとして読める。ただしframe149の粒子bboxはy=1080まで達し、画面下端で流れが切れているため手動の画面端ゲートは不合格。frame59の400% cropでは細粒・粗粒の径差は明瞭だが粒は円形の点群で、速度筋や砂粒の流動感は弱い。したがって現状は数値ゲート合格・見た目は未採用。

v31候補は fine/coarse gravity を180/260から135/195 px/s²へ下げる一要因比較として実施した。結果は本書の「v31 砂候補のheadless診断結果」に記録し、Vulkan/D3D12ともframe149終端ゲート不合格だった。重力低下だけでは下端クリップと余韻面積超過を解消できず、v31を採用しない。次の候補を設定する場合も、砂の主方向・粒径差を保ち、終端余白24pxと差分率0.5–3%を満たす一因子比較から始める。VelocityAligned billboardとstretchEnabledを用いる短い速度方向ストリーク（stretchFactor=1.1）は別案として記録済みだが、今回のv31には混ぜていない。GPU shader変更は行わない。

従来のタイトル証跡 `output/particle-shape-target/` では、320×180のリング5フレームが両backendで画素一致し、frame 240には穴内の粒子中心が1/360残った。制作解像度のA8試作は上記のとおり評価中。HDRとGPUメモリは未評価。生成したキャプチャは `output/` に置く。

Vulkanの `text_A8_f0240.png` と白背景合成を確認した。白背景では文字の輪郭が薄く、右端に寄った文字と左側のリングが視線を分ける。制作確認用ショットでは文字を中央に置き、参照用リングは別出力にして、文字を形づくる粒の動きと背景上での可読性を確認する。

## 砂の初回試作

Playgroundの `--capture-sand-flow <output-directory>` は、背景付き1080p PNGを30 fpsで150枚出力する。細粒の流れを上向き、粗粒を下向きに分け、粒径と色にも差をつけた。タイトルはframe 60で放出停止。砂はframe 60からframe 100まで放出率を線形に落として収録する。Vulkan / RTX 4070 Ti のframe 59は2,128個、画面内2,128個の生存粒子で、修正RGBA読み戻し画像の差分範囲はx=85..1045、y=288..669。ただし目視では流れが疎で、粗粒が左側に偏り、粒径差はあるが砂の流線として連続して読めない。砂ショットは作り直しが必要。2026-11-19のv22では細粒を3.2–4.8px、粗粒を4.8–7pxへ拡大したが、frame 59でも粒の流れは左上の小さな斜線に限られ、砂としての量感と画面構成は不足している。画像 `output/particle-motion/vulkan/sand-size-held-v22/review/sand_0059_source.png`。その後のv23は広い上部帯に留まり、v24/v25は放出中に縦方向へ流れるが粒数約10,800、読み戻し込み中央値約107 msで負荷が高かった。v26では粒数上限を下げ、画面中央寄りの長い発生線を使った。frame 59で生存5,359個、画面内3,768個、背景との差分範囲x=4..1265 / y=117..926。描画・readback中央値は放出中33.1 ms (p95 55.6 ms)、停止後55.6 ms (p95 58.9 ms)。全体に広がったが粒の密度は低く、frame 149には画面内粒子29個で流れがほぼ抜ける。候補画像 `output/particle-motion/vulkan/sand-centered-v26/review/sand_0059_source.png`。v23..v25の粒数と結果はPlaygroundのcapture_measurements.json / sequence_report.jsonに保存される。v26は生存5,359個 / 画面内3,768個で中央値33/56 ms、v27は生存7,787個 / 画面内5,519個で、frame 59の差分範囲x=7..1312 / y=117..953、放出中44.3 ms (p95 76.1 ms)、停止後78.0 ms (p95 82.5 ms)。v27は粗粒と細粒の差が見やすくなったが、点が散開し砂の連続した面には見えず、frame 149には画面内29個しか残らない。画像 `output/particle-motion/vulkan/sand-dense-v27/review/sand_0059_source.png`。これらの時間はフレームごとの同期画像readbackを含み、リアルタイム再生の性能値とは同一視しない。原因調査ではEmitterのdirectionSpreadが3D円錐として速度を散らし、turbulenceAmplitudeもZ速度を加えることを確認した。2D撮影では粒子が奥行きへ離れてパース上の表示サイズが落ちていた。frame 59のrender sample 24粒ではv27のzが-88.4..124.1、v28/v29はすべて0だった。v28ではdirectionSpread=0、XYだけのvelocityRandom、turbulenceAmplitude=0としてZを固定。生存7,787個でもframe 59の背景差分が閾値10/255を超える画素は377,077個まで増え、粒の面が読めるようになった。v29で粒数を5,340個へ抑えると、同じ画素数は203,717個、色差範囲x=0..1720 / y=117..970。画像 `output/particle-motion/vulkan/sand-planar-light-v29/review/sand_0059_source.png`。v29の描画・同期readback中央値は放出中30.3 ms (p95 52.5 ms)、停止後52.2 ms (p95 54.9 ms)。frame 149は画面内14個で流れが抜ける。2Dの奥行き固定で見た目を維持したままv27より負荷を下げられたが、30 fps再生の予算内とは確認できていない。v30ではframe 60..100で連続Emitter rateを線形に落とし、Burst countも減衰させた。frame 59/75/90/99/100/120/149の画素差10/255超は203,717 / 257,065 / 236,865 / 212,696 / 208,895 / 142,172 / 36,875、画面内粒子は3,788 / 4,692 / 4,321 / 3,846 / 3,766 / 2,497 / 614。減衰は連続し、frame149も余韻が残る。放出時間延長と粒子蓄積により全体の同期readback中央値は約49 ms、停止後約69 msへ増加した。30 fps実時間性能の根拠には使えない。確認画像 `output/particle-motion/vulkan/sand-planar-fade-v30/review/sand_0149_source.png`。現行共通コードでのタイトル回帰確認ではframe149で4347個、背景差分範囲588×430、放出中中央値16.2 ms、停止後30.2 msだった。画像 `output/particle-motion/vulkan/title-regression-v31/review/title_0149_source.png`。D3D12でのheadlessキャプチャは初期化できず終了コード3となった。調査ではbuildの`ARTIFACT_AGILITY_SDK_DIR`が空で、実行ファイル隣に`D3D12Core.dll`も存在せず、生成済みコンパイルコマンドも`D3D12_SUPPORTED=0`だった。現在のbuild設定ではD3D12 rendererが無効なので、D3D12比較は未検証として残す。元のv8 `review/` およびBGRA読戻しのv9画像は色比較に使わない。

コード確認で判明した初期化漏れ: 新規Particle Layerは既定Emitterを持つため、試験用Emitter追加前に `clearEmitters()` が必要だった。現Playground fixtureはこれを行う。EmitterごとのRender SettingsもEmitter追加後に設定する。キャプチャ計測も修正し、sandで全Emitterの画面内particle数を数えるようにした。

## 連番の測定

Playgroundのビルド後、両backendのキャプチャと集計をまとめて実行できる。

```powershell
./experiments/Particle2DShapeTarget/run_motion_review.ps1 -Executable ./out/build/x64-Debug-Agent/bin/Debug/ArtifactParticle2DPlayground.exe
```

出力は実行ごとに `output/particle-motion/<run-id>/<backend>/<shot>/` に保存される。各ショットの `review/` に連番レポート、元色の代表フレーム、固定座標の100%・400%クロップが入る。

各backendのキャプチャ取得後、Pillowが入った環境で実行する。タイトルの場合は `title` とタイトルの出力先を指定する。キャプチャ時の `capture_measurements.json` から、描画と読み戻しの中央値・95パーセンタイルを放出中と放出後で計算する。PNG保存時間は計測外。背景付きRGBA出力の粒子範囲は、PNGの左上画素色を背景色としてRGB差分から求める。黒白合成は透明背景の素材にだけ適用し、現在の不透明背景ショットは元の色のままレビューする。

```powershell
python experiments/Particle2DShapeTarget/analyze_motion_sequence.py sand output/particle-sand-flow/d3d12 output/particle-sand-flow/d3d12/review
```

全150フレームの入力hash、alpha分布、背景色との差分範囲、前フレームとの平均alpha差と、代表5フレームの元色レビュー画像を保存する。フレーム差には粒の意図した移動も含まれるため、時間方向の異常を探す手掛かりとして用いる。不透明背景ショットには黒白合成を行わない。

## v31 評価のビルド状況

2026-11-20、評価用ソースコピーでCMake configureとParticlePlaygroundターゲットの全コンパイル工程が完了した。コピー内だけで `ArtifactAbstractLayerImpl.cpp` のmodule partition宣言をexportして `ArtifactAbstractLayerLayoutRouting.cpp` のincomplete type障害を回避したが、元のArtifact submoduleは変更していない。

通常リンクは `ArtifactCompositionMenu.cpp.obj` と `ArtifactProjectManagerControls.cpp.obj` の `ArtifactWidgets::FloatColorPicker::staticMetaObject` unresolved externalで停止した。利用側のコンパイル定義には `WIDGET_LIBRARY_DLL_IMPORT` があり、ArtifactWidgets側の公開CMake定義も `INTERFACE WIDGET_LIBRARY_DLL_IMPORT` だが、利用側objectは同symbolをDLL import thunk (`__imp_...`) でなく直接参照していた。`ArtifactWidgets.dll` は実体をexportし、対応するimport libraryには `__imp_...` のみがあることをdumpbinで確認したため、DLL/link libraryの未生成ではなくconsumer側のimport参照形式との不整合である。さらにMSVC 19.44で一時コピーの `FloatColorPicker.ixx` を `WIDGET_LIBRARY_DLL_EXPORT` なし（宣言マクロが `dllimport` になる条件）で再コンパイルし、そのIFCを消費するmodule interfaceを作成したところ、consumer objectは `__imp_?staticMetaObject...` を参照した。通常生成IFCからは直接参照が出るため、MSVC 19.44ではconsumer向けのimport属性を含むIFC生成が参照形式を変えることを確認した。これは隔離したコンパイラ実験であり、正規CMake/Ninja生成への反映やArtifactWidgets submoduleの修正・通常製品リンク成功を示すものではない。粒子経路だけを診断するため、隔離コピーで `/FORCE:UNRESOLVED` を加えたリンクを一度実行し、通常のArtifactStudio実行ファイルとしては使わず、headless capture後に破棄する前提で評価した。したがって以下は実アプリ品質保証ではなく、リンク障害の影響範囲を除外できるか未確認の診断証跡である。VS 18 Insiders 19.51では別途 `ArtifactAbstractLayerFractureSupport.cpp` のC1116も確認した。

### v31 砂候補のheadless診断結果

両backendで1920×1080、30 fps、150フレームを取得し、既存 `analyze_motion_sequence.py` を実行した。seed・設定はPlaygroundのv31試験ソースに固定され、出力は再生成可能な隔離コピー内 `out/particle-v31-isolated/output/<backend>/sand-v31/` に置いている。両backendともframe59本体ゲートは合格、frame149余韻ゲートは不合格だった。

capture条件、source commit、実行ソース／binary hash、backend別の代表画像・解析JSON hashは [`v31_diagnostic_manifest.json`](v31_diagnostic_manifest.json) に記録した。画像・全フレーム解析JSON・binaryは再生成可能な出力としてGit追跡しない。

| 指標 | Vulkan | D3D12 | 判定 |
|---|---:|---:|---|
| frame59差分率 / bbox | 9.665% / x=0..1719, y=117..895 | 9.664% / x=0..1719, y=117..895 | 本体ゲート合格 |
| frame149差分率 / bbox | 3.347% / x=0..1907, y=537..1080 | 3.347% / x=0..1907, y=537..1080 | 終端ゲート不合格、下端余白0px（必要24px） |
| frame60 sequential/direct | 5430粒子、state hash一致、image差分0px | 5430粒子、state hash一致、image差分0px | 各backend内で一致 |
| 更新+readback中央値/p95、放出中 | 29.53 / 40.84 ms | 29.97 / 41.95 ms | readback込み。30 fpsの33.33msをp95が超過 |
| 更新+readback中央値/p95、放出後 | 39.00 / 43.74 ms | 40.61 / 45.02 ms | readback込み。再生fps性能とはみなさない |

frame59/60/149のbackend間RGBA比較では差分画素数がそれぞれ55,409 / 56,664 / 18,294、最大channel差43 / 45 / 48、全channel平均差0.081 / 0.083 / 0.029（8-bit値）だった。全体の見た目は両backendで揃い、粒子数・frame60状態hashも一致するが、全150枚のbackend間ピクセル同一性は要求せず未検証。目視ではframe59の二方向流れと粒径・色分けは読める一方、粒子は円形spriteのまま。frame149は残粒子が下端へ密集し、画面で切れるため採用条件を満たさない。v31（fine gravity 135 / coarse 195 px/s²）はv30より重力を下げても下端クリップを解消せず、終端差分率もゲート上限3%を超えたため、次候補の採用根拠にはしない。

**結論:** v31は探索候補として不採用。これは診断リンク版によるheadless比較であり、通常リンク、実アプリParticle Inspector / Composition View、GPU実再生の性能、全150フレームのbackend差分は未検証。正規リンクの修復後に同一ソース・同一seedで再取得し、終端下端余白と30 fpsの時間条件を満たす候補を改めて選ぶ。

### v33 砂候補のheadless評価結果

v31のframe149は終端差分率3.347%、下端余白0pxだった。v33では放出レート・方向・seedを保ったまま、fine/coarse emitterの寿命を2.0–2.4秒、重力をそれぞれ115/155 px/s²へ変更した。frame59では最古粒子も寿命上限に達しないため本体範囲を保ち、放出後の古い粒子を減らす狙いである。隔離コピーの候補ソースをビルドし、Vulkan / D3D12で同じ150フレームを取得した。

| 指標 | Vulkan | D3D12 | 判定 |
|---|---:|---:|---|
| frame59差分率 / bbox | 9.073% / x=0..1719, y=117..860 | 9.073% / x=0..1719, y=117..860 | 本体ゲート合格 |
| frame149差分率 / bbox | 0.527% / x=0..1749, y=510..1013 | 0.527% / x=0..1749, y=510..1013 | 終端ゲート合格、下端余白66px |
| frame60 sequential/direct | 5430粒子、state hash一致、image差分0px | 5430粒子、state hash一致、image差分0px | backend内で一致 |
| frame59 / 60 / 149 backend差分 | — | 52,792 / 54,073 / 3,338画素、最大差57 / 54 / 53、全channel平均差0.079 / 0.081 / 0.0054 | 代表画像で外観は近似。全150枚の完全一致は未検証 |
| 更新+readback中央値/p95、放出中 | 39.32 / 52.96 ms | 43.53 / 54.72 ms | readback込み。30 fps実再生の性能証明ではない |
| 更新+readback中央値/p95、放出後 | 23.00 / 41.91 ms | 24.62 / 44.48 ms | readback込み。30 fps実再生の性能証明ではない |

目視ではframe59の幅広い斜めの砂流とframe149の疎な残像を確認し、後者は画面下端で切れない。v33は宣言済みの画像ゲートを両backendで通過した。

初回のcaptureは通常リンクを回避した診断ビルドで取得したが、その後ArtifactWidgetsのstatic data exportを修正し、通常リンクの同一Playground targetで両backendを再取得した。以下は通常リンク版の結果である。frame59の画面全体は幅広い斜めの砂流として読め、frame149の粒子は下端で切れない。全150フレームのbackend間完全一致は要求せず、frame59/60/149の代表画像差分を計測した。同期readback込み時間は30fpsの実再生性能を証明しない。

| 指標 | Vulkan | D3D12 | 判定 |
|---|---:|---:|---|
| frame59差分率 / bbox | 9.073% / x=0..1719, y=117..860 | 9.073% / x=0..1719, y=117..860 | 本体ゲート合格 |
| frame149差分率 / bbox | 0.527% / x=0..1749, y=510..1013 | 0.527% / x=0..1749, y=510..1013 | 終端ゲート合格、下端余白66px |
| frame59 / 60 / 149 backend差分 | — | 52,792 / 54,073 / 3,338画素、最大差57 / 54 / 53、全channel平均差0.079 / 0.081 / 0.0054 | 代表画像は近似。一致ではない |
| 更新+readback中央値/p95、放出中 | 49.02 / 62.93 ms | 49.36 / 64.07 ms | readback込み。製品再生性能ではない |
| 更新+readback中央値/p95、放出後 | 23.87 / 45.24 ms | 23.17 / 44.04 ms | readback込み。製品再生性能ではない |

通常リンクの `ArtifactParticle2DPlayground.exe` はMSVC 19.44でリンクでき、キャプチャはQt/OpenCVのdebug runtime DLL探索パスを設定してoffscreen実行した。ArtifactWidgetsの変更は `FloatColorPicker.ixx` だけで、module IFC内の `Q_OBJECT` 静的 `staticMetaObject` と公開APIを明示exportする。実アプリのParticle Inspector / Composition View UI、GPU実再生fps、HDR／透明背景合成は未検証であり、粒子形状は円形spriteのまま。したがってv33はPlayground上の砂候補として採用可能だが、Shape Target spin-offのUI・製品統合完了とはみなさない。

条件、source / executable hash、backend別の代表画像と解析JSON hashは [`v33_diagnostic_manifest.json`](v33_diagnostic_manifest.json) に記録した。生成画像・全フレーム解析・実行ファイルは `out/` 以下に置き、Gitには追加しない。

### v31 UI確認の試行

許可された範囲で既存の `out/build/x64-Debug-Agent/bin/Debug/Artifact.exe` を起動したが、これはv31診断実行ファイルではない。未保存の一時 `Comp1`（1920×1080、30 fps）へメニューから `2D Particle 1`（Form Particle Layer）を追加し、選択状態を確認した。InspectorにはLayer/Templateタブとレイヤー情報が表示されたが、Shape Target項目は見つからなかった。この古い実行ファイルで当該項目が未実装なのか、別の専用編集面にあるのかは判定できていない。Composition Viewではグリッドと選択レイヤーの変換ハンドルを観察したが、粒子画像は確認できなかった。したがって、これは既存ビルドでの限定的なUI観察であり、v31診断実行ファイルのUI評価ではない。詳細はmanifestに記録した。
