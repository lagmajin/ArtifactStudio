# 2D Particle Shape Target

**最終更新:** 2026-11-19
**ステータス:** In Progress


既存の 2D Particle Layer に、放出した粒子を選択した Shape または Text layer の形へ集めるオプションを追加する。独立した Particle Layer にはせず、Particle System の専用設定として実装する。既定は無効とし、無効時の既存出力を維持する。初回対象は静止 geometry とし、粒子の色・サイズ・寿命・乱数 seed は既存設定を使う。

## 現状調査

- `ArtifactParticleLayer` はレイヤー設定・JSON 保存・Particle 専用 PropertyGroup を所有する。通常の Property Widget ではなく、この専用グループへ設定を追加する。
- `ArtifactParticleGenerator` は粒子の固定ステップ更新と安定した `Particle::id` を所有する。
- Form Particle の `LayerMap` は外部画像を読む機能であり、Shape/Text geometry のサンプラーとしては再利用できない。
- `ArtifactShapeLayer::nativeShapePaths()` と `ShapePath::triangulate()` は geometry 取得口を提供する。穴を含む複数輪郭を扱う `triangulate()` を Shape のターゲット点作成に使える可能性がある。座標変換と fill rule の実際の意味は実装時に確認する。
- `ArtifactTextLayer::textGlyphGeometry()` は shaped glyph snapshot を返す。現行 software text renderer は `glyphPathForCode()` と glyph transform で輪郭を作るため、新規サンプラーも同じ helper を使う。Text サンプラーは fill path と style/glyph override の stroke outline を union して点を作る。GPU の shaped glyph index 経路との一致、stroke を含む見た目の一致は未検証。emoji/color bitmap は除外する。
- Shape/Text layer への参照解決・座標変換は Particle Layer のコールドパスで行い、Generator の固定ステップに composition や Qt shape API を持ち込まない。実行時の変更通知接続は追加しない。geometry revision が利用できない場合は評価フレーム境界で限定的に再生成する方式を検討し、毎粒子・毎 substep の geometry 取得は禁止する。

## 実装契約

- 保存値: `enabled`（既定 false）、target `LayerID`、吸引強度。参照切れ・自己参照・unsupported type・空 geometry・outline 非対応文字のみの場合は no-op。
- geometry を Particle Layer-local 座標へ変換して、ArtifactCore `ArtifactArray` で最大 4096 点の bounded target array を cold path で準備する。emitters へ複製する点数は最大 65536 点（保存・UI 上限の1024 emitters時。各 emitter 最低1点）とし、emitters が多い場合は決定論的な先頭サンプルを共有する。候補点は三角形面積に比例して deterministic に配置し、点生成は target ID / geometry revision / transform / seed により再現可能にする。Text sampler は呼び出し側の pre-sized buffer を使う。ShapePath の既存三角形分割が返す `std::vector` は互換のため一時作業領域として保持する。
- Shape は `nativeShapeFillPathsForParticleTarget()` から、legacy fill または multi-content/ordered stack の visible・opacity・merge 済み fill geometry を取得する。stroke-only 領域は対象外で、line や open path は no-op となる。multi-content の可視 fill と穴が実描画に一致するかは画像評価で確認し、stroke-only に誤って吸引されないことも確認する。
- 各粒子の行先は emitter seed と粒子 ID から選択し、粒子は固定 timestep 内で既存速度・減衰と競合しにくい bounded attraction acceleration を受ける。フレーム中の allocation、shape/text 呼び出し、target vector の再構築は禁止。
- Shape/Text の content revision が変化した場合だけ layer-local point cache を再構築する。Shape の三角形分割結果と面積累積値は ArtifactArray に保持し、transform 変更時は bounded 点列の座標変換だけをやり直す。geometry API・アニメーション走査・三角形分割はキャッシュ無効時だけ実行し、steady state では描画経路から早期 return する。Composition の `layerById` による参照解決と content revision、原点 transform の確認は描画評価ごとに行う。初期実装では keyframe 付き legacy geometry、multi-content geometry、Shape operator、Text と、target/particle または親レイヤーの animated transform は無効扱いとする。
- 新規 signal/slot、ReactiveEvents、QImage サンプリング、3D Particle 挙動変更は導入しない。
- 保存/再読込後、disabled 設定で既存挙動が変わらないこと、seed と直接 seek/連続再生が一致することを受入条件にする。

## 画像評価計画

### 目的と比較

放出粒子が指定 geometry 内へ収束し、穴・輪郭外へ漏れず、時間・backend 間で安定して見えるかを評価する。比較 baseline は同一 scene・seed・設定で target disabled にした現行 Particle Layer。geometry 内部の粒子中心率と穴領域率は形状マスクから算出する。意図的な attraction のない baseline は視覚的な正解画像とは見なさない。

### 素材と条件

- 320×180 transparent composition。白い穴付き Shape、細線を含む Text、空 Shape、移動/回転 target、削除された参照を含める。
- Text の family・weight・Qt/font backend/version・shaped glyph snapshot の識別情報を記録する。評価素材は合成生成し、出所を記録する。
- 30 fps、seed 固定、同じ emitter / particle count / transforms、フレーム 1–90。PNG 全体・評価 crop・内部/穴/輪郭マスクを保存する。探索キャプチャの seed と素材は記録し、合否用の許容差は最終比較前に確定する。
- Working color space、display transform、alpha が straight/premultiplied のどちらで比較用 PNG に出るか、量子化と出力経路を記録する。目視は黒・白の両背景で行う。
- D3D12 と Vulkan の Composition View および Render Queue を対象とする。直接 seek と連続再生を同一 frame で比較する。利用不能な backend は未検証とする。

### 判定項目と合格条件

- **機能/正しさ:** disabled 出力は同じ条件の baseline と一致する。参照切れ・self-reference・空 geometry で crash、非有限値、不要な吸引がない。target enabled では放出終了後に geometry 内粒子中心率が baseline より増加する。
- **形状/細部:** 生成された Shape target point の穴領域率は 0% とする。粒子は目的地へ向かう途中で穴を横切り得るため、フレーム 30/60/90 の粒子中心穴率は baseline との相対変化として測り、終端の穴 leak を目視確認する。輪郭 crop を 100% と 400% で目視する。Text では fine stroke の欠落・過密と fill/stroke target の違いを記録する。Shape は multi-content の可視 fill、merge、opacity、stroke-only を別ケースにし、実際に描画される可視領域との一致を確認する。
- **色/alpha:** RGB/alpha min/max・mean・p50/p95/p99、非有限値、輪郭境界帯の alpha 分布を記録し、黒/白背景 composite の fringe を確認する。
- **時間安定性:** target/seed 固定時の粒子対応を ID で揃えたフレーム差と、粒子中心率の推移を確認する。direct seek / continuous の位置差を計測する。
- **経路/解像度:** 320×180 の Composition View と Render Queue の結果を比較する。backend ごとの差分、pixel/alpha の最大差と p95 を保存する。
- **性能:** particle update time と CPU process working set を同粒子数 baseline に対して測る。target cache 構築と layer preparation の時間・working set差を別記録する。working set は GPU memory ではなく、cache単体の容量も直接示さない。
- 数値許容幅は baseline 取得後、候補の最終比較より前に、指標・領域・判定根拠とともに記入して固定する。固定前の画像は探索用で合否根拠にしない。必須項目の未検証は合格扱いにしない。

### 実施した評価

2026-11-19 に `ArtifactParticle2DPlayground` を Ninja Debug build し、Diligent Vulkan と D3D12 の headless capture を取得した。NVIDIA GeForce RTX 4070 Ti、320×180、30 fps、seed `20261118`。最終fixtureはOrdered Shape Stack（外側・内側ellipseのPath、Merge Paths/Subtract、Fill/Stroke）で、Shape/Text/Particle/Empty ShapeのLayerIDを固定してbackend間のdestination samplingも揃えた。GPU render alphaは穴中心(154,121)=0、ring上(154,95)=184、native target pathは穴中心を含まない。旧legacy subtractive fixtureの測定値は描画 parityを満たさなかったため現行レポートから除外した。各backend画像と集計は `output/particle-shape-target/{vulkan,d3d12}/` と `output/particle-shape-target/backend_comparison.json`。

- disabled default と明示 disabled の frame 90 PNG は完全一致（異なる画素 0、最大 channel 差 0）。frame 60の連続再生とreset後直接seekではparticle ID mismatch 0、位置差0 px。PNG差はbaseline 7画素（最大channel差1）、Shape/Textはともに0画素差。
- Shape frame 90 のparticle centerは外接楕円内でbaseline 5/360からtarget 297/360へ増加し、実描画visible mask内は5/360から287/360へ増加した。穴楕円内はtarget 25/360、frame 240では0/360。frame 240は外接楕円内356/360、visible mask内359/360。rendererの穴中心alpha=0、ring sample alpha=184、native target pathの穴中心contains=false。frame90/240の100%/400% cropを目視確認した。粒子は移動途中に穴領域を通過し得るため中間フレームは過渡値であり、終端で穴内0/360になった。
- Text `A8` の静的 sampling は4096点。frame90でglyph bounds内264/360、実描画alpha mask内171/360。frame240では359/360、mask内346/360。Arial regular 400、Qt 6.10.2、exact match=true。layer alpha bounds `[230,98]-[306,151]` と sampler bounds x=229.92–304.09、y=97.91–151.80は整合した。shaped glyph backend・日本語・emoji・複数行・細線保持 parity は未確認。
- 初回測定で Text 三角形分割が空となりサンプリングが 0 点だったため、輪郭内の決定論的な bounded rejection sampling をフォールバックに追加した。上限は要求 capacity 最大4096点、試行回数は要求点数の64倍。今回の `A` は4096点すべて取得した。細く複雑な字形で全数に達する保証はなく、継続評価が必要。
- `--check-shape-target-persistence` は現行Ninja/Debugバイナリでも再実行して成功。既定値、enabled/target ID/strength の JSON round trip、および shapeTarget フィールドを持たない旧 JSON の既定値を確認した。layer の実プロジェクト保存・再読込までは未検証。
- 参照切れ・self-reference・空 Shape の有効ターゲットは、それぞれ frame 90 の baseline と完全一致（差分画素 0、最大 channel 差 0）。未対応 type、animated/multi-content/stroke-only のケースは未実施。
- PNG frame90/240 RGBA統計は `output/particle-shape-target/vulkan/image_statistics.json` に保存した。frame90 alpha zero/partial/full counts: baseline 48839/5943/2818, Shape 48871/5476/3253, Text 48778/5882/2940。RGB meanは baseline (27.3540,20.8727,14.9380)、Shape (27.6568,21.8211,17.0709)、Text (27.7707,21.4505,15.6977)。8-bit PNG値域内でalpha min/max 0/255、RGB非有限値なし。frame90/240を白背景合成し、transparent PNGを黒背景上でも確認した。背景合成は保存PNGから行ったものでアプリ別レンダー経路ではない。
- **性能探索 (2026-11-19):** Ninja Debug、360 live particles、30 fps fixed timestep、12 warm-up + 5 batch × 24 measured steps。`ParticleSystem::update()`のbatch平均中央値はVulkan baseline 916.14µs、Shape 1019.24µs（+11.3%）、Text 1024.75µs（+11.9%）。D3D12はbaseline 914.69µs、Shape 1028.33µs（+12.4%）、Text 1015.73µs（+11.1%）。5 batchesは単一process run内で、閾値・run間信頼区間・Release build比較は未実施。測定区間中のsampled process working-set peak growthはbaseline 4096 bytes、Shape/Text 0 bytes。Shape/Text preparationでは `renderToImage()` frame 0（target cache refresh + software layer drawを含む）のelapsedがVulkan 1025.51ms / 719.50ms、D3D12 1025.71ms / 721.10ms。prep前後のprocess working-set差はVulkan baseline/Shape/Text +524288/+421888/+4096 bytes、D3D12 +524288/+638976/+4096 bytesだった。これはプロセス全体のCPU working setであり、target cache割当だけを分離せず、8-step間隔samplingは短いpeakを捉えない。GPU memoryとtarget point cache容量は未測定。厳密な性能合否・memory budget評価は未完了。
- Parent Visual Studio generatorではMSVC C1116（`Animation.Transform3D` IFC import / `_Stop_callback_base::_Do_attach`）が再発した。Ninja Debug構成は再configureで`HAS_ATL=TRUE` / `D3D12_SUPPORTED=TRUE`となり、playgroundと両backend captureまで成功。capture deviceはNVIDIA RTX 4070 Ti、Vulkan API 1.4.341。最終的なD3D12 captureは成功したため、以前のD3D12 exit 3は古いNinja configurationに限る。
- Vulkan/D3D12は、固定LayerIDを用いた最新fixtureでShape/Textの30/60/90/180/240およびframe60 direct-seek PNGが全画素一致（RGBA最大差0）。baselineはframe60で2 pixel、direct seekで5 pixel（最大差1）、他baselineフレームは一致。Shape/Textのframe90/240粒子中心も同index 360粒すべて位置差0px。以前のUUID未固定captureではShape/Text粒子位置がbackend間で異なったが、fixture LayerIDがランダムだったためseed条件が揃っていない比較として破棄した。
- Render Queue parity、Composition View、未対応type、animated geometry/transform、stroke-only、非stack multi-content、複数emitter、複数font素材、性能閾値・Release比較、GPU memoryとtarget cache容量は未検証。閾値は候補画像比較前に固定できていないため、今回の値は探索/応答の定量記録であり、品質合格を示すものではない。必須評価の未検証が残るため In Progress を維持する。

探索結果から Shape attraction と静的 glyph attraction がVulkan/D3D12 offscreen経路で動作し、disabled parity・deterministic seek・固定fixtureでのbackend parityを確認した。一方、画像評価ルールで必須の製品UI/Render Queue、広い素材マトリクス、性能判定値とmemory budgetが未確定のため、ステータスは In Progress のままとする。

## 実装先

- Product settings / serialization / dedicated particle property group: `Artifact/src/Layer/ArtifactParticleLayer.cpp` と対応 interface。
- Fixed-step attraction and deterministic destination selection: `Artifact/src/Generator/ArtifactParticleGenerator.cpp` と対応 interface。
- 変更対象は 2D Particle 経路に限定する。`Artifact` は submodule なので、親の gitlink 更新前に子を個別に確認する。
