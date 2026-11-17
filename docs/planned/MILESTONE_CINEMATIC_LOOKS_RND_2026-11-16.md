# Cinematic Looks 画像補正 R&D プロジェクト

**最終更新:** 2026-11-17
**ステータス:** In Progress（LUT9種、rd0/rd1/rd2 source-mirror応答・full/ROI blind sheetsを5素材で記録済み。3 familyの名前付き選択、Composition concrete restore分岐（同種effect instance suffix対応）、Tritone CPU式に沿ったHLSL/premultiplied-alpha wrapperとLUT CPU alpha wrapperを実装。Looks Browserは`teal_orange`のLayer / Composition適用を暫定接続。rd2ではCPU/GPU mirror差0、Tritone alpha sweep差は最大1.2e-7。LUT fixture mirror差は全9 presetで2.3e-16未満。ビルド・runtime未検証。人手視覚判定・品質 gate・C++ differentialも未実施）

## 目的

既存の組み込み LUT とカラー補正経路を使った静止画の cinematic look を再現可能な条件で評価し、プリセットとして提供する場合の見た目・技術契約・受入条件を明らかにする。

本プロジェクトは R&D／テスト用プロジェクトであり、新しい LUT や補正処理の実装、既定値変更を含まない。2026-11-17のユーザー明示依頼による製品フォローアップとして、Looks Browserの`teal_orange` IDだけを既存Tritone `TealAndOrange` presetへ暫定接続した。これは名前一致による製品仮説で、既存sourceに公式mappingがあったわけではない。残り23 IDはApply不可のままとし、正式mappingと品質採用判断は保留する。

## 暫定結論（rd0・画質採否前）

- LUT 補正の基本 effect 自体は存在し、Inspector の effect catalog から選択中 layer または composition の stack へ追加するソース経路がある。`Preset` / `LUT File` / `Intensity` property がある。製品フォローアップで`Preset`は既存Integer enum chooserへ接続した。Looks browserは別inventoryであり、`teal_orange`以外の正式mappingは未設定。
- `ColorBalanceEffect` (`effect.colorcorrection.colorbalance`) は Inspector catalog / Effect Service に登録された CPU/GPU effect で、`Custom / Neutral / CoolShadows / WarmHighlights / Cinematic` を持ち、初期値は Cinematic。`TritoneEffect` (`effect.colorcorrection.tritone`) も登録済み CPU/GPU effect で、`Custom / Cinematic / TealAndOrange / WarmGold / ColdBlue` を持ち、初期値は Cinematic。今回の製品フォローアップで両方の `Preset` Integer property に既存 enum chooser 用 tooltip を追加したが、画面表示は未確認。設定式・元評価時のソース hash は `REGISTRY_AUDIT.json` に記録した。
- LUT 関連は三つの別機能として扱う。`LUTEffect` の内蔵9 preset は effect property として存在する。外部 `.cube/.3dl` library/browser は Color Science Panel 側に別途あり、旧 `MILESTONE_LUT_BROWSER_2026-06-16.md` の2026-07監査では一覧・選択 preview・Load/Clear・drop入口が部分実装（同 milestone は OCIO integration に superseded）とされる。24件の`ArtifactLooksPresetBrowser`は別inventoryである。初期監査ではcallsiteとmappingがなかったが、2026-11-17にInspectorのAdd Effect pickerから起動できるcallsiteと`teal_orange`の暫定Tritone mappingを追加した。これは他IDsのmappingやLUT browser統合を意味しない。
- `ColorBalanceEffect` に `Cinematic` preset（shadow/midtone/highlight balance）、`TritoneEffect` に `Cinematic` と `TealAndOrange` preset（tonal color mix）、`LUTEffect` に9個の内蔵 LUT preset があり、いずれも Inspector catalog / service factory から追加可能。従ってシネマティックな画像補正部品・effect preset は既に部分導入されている。各 `Preset` は製品フォローアップで既存 enum chooser 用 tooltip metadataを追加した。Looks browserはInspector pickerから開けるが、24 ID中1件だけ暫定mappingされ、画質検証も未完了である。
- Looks Browserは24 ID中、`teal_orange`だけを既存Tritone `TealAndOrange`へ暫定mappingし、InspectorのAdd Effect pickerから起動可能にした。Intensityは新規effectの`Effect Mix`に設定した状態でLayerまたはCompositionへ追加し、Undo上は単一の追加操作になる。成功時はdialogを閉じ、同じ画面での連続クリックによる重複追加を防ぐ。他IDはmapping未設定としてApply不可。実runtime動作・Undo/redo・見た目は未確認。サムネイルはplaceholderで、previewは実画像処理結果を表さない。
- product / visual acceptance は保留。LUT と effect extension のブラインドレビュー、3 family の C++ differential／実アプリ画像測定、alpha/HDR runtime capture、Composition save/reload、Render Queue 適用確認、品質 gate が残っている。LUT CPU wrapperをsource-levelで補正したがruntime未検証。Color Balance は既存の premultiplied-aware control lane。Tritone CPU/GPU wrapper を同契約へ更新し、rd2 source mirrorで挙動を確認したが実runtime未検証。**修正前のrd0**ではTritone CPU/GPU source equation差があり、式mirrorの強度0.5/1で素材ごとに約7〜16 RGB codes MAE、GPU式は強度0でも飽和色を変化させる候補を検出した。**修正後のrd1/rd2 source-mirror**では5素材×Cinematic 0/0.5/1・TealAndOrange 1の量子化コード差が0。rd2のpremultiplied-alpha probeはstraight RGBとの差がTritone CPU/GPUとも最大約1.2e-7。旧差分と新一致は別のsource-mirror版であり、shader/C++実行差の判定ではない。
- rd0時点ではComposition deserializerがLUT / Color Balance / TritoneのConcrete effectを生成せず、base `ArtifactAbstractEffect`へ落とす経路を確認した。`captures/rd0/findings.json` の `RD0-COMPOSITION-LUT-RESTORE-01` と `RD0-COMPOSITION-COLOR-EFFECT-RESTORE-01` はこの修正前sourceの監査履歴であり、後続製品変更で3 factory分岐と同種effect instance suffix対応を追加した。修正後のComposition project round-trip/renderは未実行。

この結論はソース監査と Python source mirror に基づく暫定整理であり、実アプリの画面確認や画質受入を代替しない。

## 製品実装フォローアップ（2026-11-16 / 17）

ユーザーの別途明示依頼に基づき、R&D の品質採否とは分けて次の製品コード接続を追加した。

- LUT / Color Balance / Tritone の `Preset` Integer property に既存 enum chooser が読む選択肢 tooltip を付け、それぞれの組み込み preset を名前で選べるようにした。
- Composition effect deserializer に LUT / Color Balance / Tritone の concrete effect factory 分岐を追加し、JSON の各 property を既存復元ループで concrete instance に適用するようにした。同種effectへ追加される `-2` 以降のinstance suffixも既知の3 typeで認識する。
- 2026-11-17にはLUT CPU wrapperもPremultiplied descriptorを読み、straight RGBでlookup/mix後に再premultiplyする処理を追加した。9 LUT×8色×6 alpha×3 strengthのsource mirrorは最大2.3e-16未満、旧直接RGB式は最大1.321154を記録した（`experiments/CinematicLooksLab/captures/rd1_lut_alpha_wrapper/float_contract_probe.json`）。これは Looks browser の24 ID mapping、色品質の採用、CPU/GPU parity、runtime alpha/HDR 対応を確定するものではない。C++ module integration、ビルド、Composition save/reload（特に複数の同種effect）、画面、レンダー経路の runtime 確認は未実施であり、R&D の品質ゲートは引き続き未完了とする。
- Looks Browserは`teal_orange`のみをTritone `TealAndOrange`へ暫定mappingする。名前から着想した1件の製品仮説であり、公式preset mappingや画質受入ではない。残り23 IDのmappingと実UI/Previewは未確認。
- Looks BrowserのInspector生成callsiteを追加し、選択中CompositionまたはLayerへ適用できるようにした。Tritone effectの設定値は追加前に構成するため、Undo履歴はeffect追加1件となる。callsiteの実UI表示、Layer/Compositionのsave/reload、Undo/redoとrenderは未検証。
- `contract_probe.py` のrun IDを出力先から設定できるようにし、更新wrapperのrd3 mirrorを [`experiments/CinematicLooksLab/captures/rd3_lut_alpha_wrapper/float_contract_probe.json`](../../experiments/CinematicLooksLab/captures/rd3_lut_alpha_wrapper/float_contract_probe.json) に独立記録した。更新後の最大straight-RGB mirror誤差は2.3e-16未満、旧式は最大1.321154。これは証跡識別とsource-equation再現性を改善するもので、製品C++実行を検証していない。
- effect source pin は画像応答式だけでなく source 全体の SHA-256 を対象にする。関連式とmetadataの差分を再監査した後、`SOURCE_CONTRACT.json` / `EFFECT_SOURCE_CONTRACT.json` を更新し、`rd1_effect_extension` と `rd2_effect_extension` を別runとして生成した。rd2はpremultiplied-alpha wrapper変更後。`rd1_lut_alpha_wrapper`のfloat probeも個別保存し、旧 capture は元のsource/hashに結び付いた履歴として保持する。
- R&Dで検出したTritoneのCPU/GPU差に対し、`TritoneEffect.cpp` のHLSLを `TritoneProcessor::applyPixel` のRec.709 luma、softness全幅のweight、色ごとのtarget clamp、`masterStrength * colorMix`、CPUと同じluma保持へ揃えた。rd1 source mirrorでは5素材×4設定の8-bitコード差0を記録した。これはソース上の式対応であり、shader compile・実GPU差分・alpha/HDR/runtimeの証拠ではない。
- 追加のsource auditでTritone GPU CPU-fallbackにもpremultiplied alpha対応が必要と分かったため、GPU wrapperと同じunpremultiply/process/re-premultiply処理に揃えた。premultiplied flag追加で76 byteになっていたGPU `ParamsCB` にpaddingを戻し、既存`static_assert`の80 byte契約とHLSL cbuffer配置を一致させた。これはsource-level修正で、コンパイルとGPU runtime buffer layoutは未確認。`EFFECT_SOURCE_CONTRACT.json` pinを更新し、既存rd2 captureは旧source hashの履歴として保持する。

## 現状のコード確認

- `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp` は組み込み cinematic / vintage / cold / warm / contrast / desaturated / film-stock LUT と外部 LUT 選択、強度混合を実装している。追加の Color Balance / Tritone Cinematic lane も effect service factory、Inspector catalog、CPU/GPU implementation に登録済み。preset 設定式と source hash は `REGISTRY_AUDIT.json` / `EFFECT_SOURCE_CONTRACT.json` に記録し、追加2 family の source-mirror 画像応答・blind sheet を `captures/rd0_effect_extension/` に生成した。
- 一般の `Apply LUT` effect は `InspectorEffectCatalog` に登録され、`ArtifactEffectService` が `effect.colorcorrection.lut` ID で factory 作成する。`ArtifactInspectorWidget::addSelectedEffectToCurrentTarget` は factory から生成し、選択中の layer または composition stack へ追加する。`LUTEffect` には Custom と9 builtin preset があり、既定 preset は Cinematic、強度は1.0。今回の製品フォローアップで `Preset` に enum chooser 用 tooltip を追加した。`LUTEffectContractTest.cpp` には LUT の CPU 契約テストがあるが tooltip enum chooser を直接確認するテストは検索範囲内で見つからず、Composition round-trip テストは IBK を対象として LUT は含まない。コード上の値は0=Custom、1=Cinematic、2=Vintage、3=Cold、4=Warm、5=HighContrast、6=LowContrast、7=Desaturated、8=Kodak2383、9=Fuji3510。これは適用導線・property editor 選択の静的ソース確認で、画面操作・見た目の受入・実 runtime parity の証拠ではない。
- 既存の汎用 effect preset file API は effect ID、enabled/mix/mask state と property 値を JSON 保存し、既存 effect object への再適用に対応する。`LUTEffect` の `Preset`、`LUT File`、`Intensity` も保存対象で、load は effect ID と owned property payload を検証後に `setPropertyValue` へ戻す。これは個別 effect のファイル保存／復元 API の静的根拠で、専用 cinematic look library、Looks browser 接続、プロジェクト保存・再読込や runtime の実行証拠ではない。根拠 source hash は `experiments/CinematicLooksLab/REGISTRY_AUDIT.json` に追加した。
- `ArtifactCore/src/Color/ColorLUT.cpp` の `BuiltinLUTs` に LUT データ生成がある。ラボは 17³ 格子生成式と三線形 sampler を Python で mirror し、harness 自身・source・source contract の hash を manifest に記録する。各素材のブラインド順についても導出 seed と shuffle 方法を保存する。これは C++ 実装そのものの実行・一致テストではない。
- 2026-11-16 の mirror 監査で `ColorLUT::setValue` の格子点 clamp が Python mirror 2本で欠落していたため修正し、画像・統計・ブラインドシート・float probe を再生成した。例として warm LUT の `(0,0.98,0)` は格子点 clamp ありで green≈0.995、旧 mirror は1.0。source hash は現行 C++ を指すが、C++ 直接比較は未実施。
- `experiments/CinematicLooksLab/SOURCE_CONTRACT.json` に `ColorLUT.cpp` / `LUTEffect.cpp` の監査済み source hash と mirror 契約を pin し、変更検出時は harness を停止する。これで未レビュー source から stale mirror の rd0 を再生成しない。
- alpha保持の根拠となる `ImageSurfaceView.ixx` も source pin に追加した。mutable row indexing は reference member を持つ channel proxy を返すため、LUTEffect の `auto pixel = row[x]` と RGB member 書き込みは backing buffer に反映される。これは静的型定義の確認で、runtime 実行証拠ではない。
- ソース上の cinematic は RGB 独立倍率（R×1.1、G×0.9、B×1.15）で、orange/teal の色相分離処理ではない。Kodak 2383 / Fuji 3510 も簡易的な channel power curve。`ColorLUT::apply` が入力・出力 RGB を [0,1] に clamp するため HDR >1 の保持契約もない。これらは実装の記述であり、目視品質の判定ではない。
- 初期監査時は`ArtifactLooksPresetBrowser.cpp`にFilm / Photo / Creative等のinventoryがある一方、thumbnailはplaceholder、生成callsiteなし、Applyは内部IDとbadge更新のみだった。2026-11-17にInspector picker callsiteと`teal_orange`の暫定Tritone apply pathを追加した。残り23 IDは未mappingで、thumbnail / sample previewも実effect結果ではない。
- 外部 LUT library/browser は ArtifactColorSciencePanel / ArtifactColorScienceManager を中心とする別経路で、旧 LUT Browser milestone の2026-07監査では directory scan、一覧、選択 preview、load/clear、drop入口の部分実装が記録されている。これはユーザー持込み LUT の workflow であり、9 builtin cinematic LUT の名前付き preset browser でも24件 Looks catalog の apply bridge でもない。現状の関連文書は [MILESTONE_LUT_BROWSER_2026-06-16.md](MILESTONE_LUT_BROWSER_2026-06-16.md)（superseded）と [MILESTONE_OCIO_INTEGRATION_2026-06-16.md](MILESTONE_OCIO_INTEGRATION_2026-06-16.md) を参照。
- `LUTEffect` は effect service から生成でき、layer/composition の CPU Rasterizer effect loop は `applyToCpuSurface` を呼ぶ。Render Queue の通常 Beauty 出力は GPU readback 後に composition final-effect helper を呼ぶため、この段は CPU effect 実行となる。マルチチャンネル出力分岐では同 helper を呼ばない。一方、final GPU post-process の effect-stack handler は stack の LUT entry を確認するだけで、コメント上は LUT upload を caller に委ねている。`Artifact/src` と `Artifact/include` の callsite 検索では `updateFromEffectStack` 呼び出しは見つからず、`updateFromColorLUT` の唯一の呼び出しは `ArtifactIRenderer` の viewport display LUT upload（独立した display transform 経路）だった。したがってこれらは一般 `LUTEffect` の GPU 適用を示さない。関連3 source の SHA-256 と callsite 監査範囲を `REGISTRY_AUDIT.json` に追加した。CPU 経路のソース追跡はできたが、実アプリでの画像到達・backend parity は未確認。
- ブラウザー24 preset ID と9 builtin LUT の source-hash 付き一覧を [`REGISTRY_AUDIT.json`](../../experiments/CinematicLooksLab/REGISTRY_AUDIT.json) に記録した。preset ID→LUT の明示的な mapping は見つからず、名称類似のみで割当しない。
- 利用可能性を混同しないこと: `Apply LUT` の一般 effect は既にInspector / Effect Serviceに存在する。Looks Browserには別inventoryの24 IDがあり、Inspectorからの入口は追加済みだが、effectへ暫定接続したのは`teal_orange` 1件のみである。元の`REGISTRY_AUDIT.json`は接続前のsource hash付き監査snapshotとして保持し、後続実装は別項目で記録する。
- したがって検証対象は「補正機能の有無」ではなく、「現行補正候補の画質・色/alpha 契約・素材間の応答・CPU/GPU実経路」と「ブラウザーの preset 定義と実体の対応付け」である。

## 評価計画

必須の事前登録済み計画は [`../../experiments/CinematicLooksLab/EVALUATION_PLAN.md`](../../experiments/CinematicLooksLab/EVALUATION_PLAN.md) に置く。評価計画ルールは [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。

評価素材は deterministic chart と CC0 実写4点（夜景2、人物1、森林1）を確保し、人物／肌・屋外緑・夜景／点光源の3カテゴリを含む。LUTEffect の9 preset lane に加えて Color Balance / Tritone Cinematic lane と Tritone TealAndOrange を5素材 source mirror で測定し、Tritone CPU/GPU式も独立比較した。別 ID のblind full/ROI sheets、126 crops、空欄票を追加。HDR/alpha runtime 画像評価、人手判定、品質 gate は未実施。GPU/final runtime parity は未検証。

chart の灰色 ramp ROI では RGB channel spread と水平 luma の reversal / flat-step 率を、smooth / color gradient では中央行・列の unique code level、flat-step / reversal 率、最大隣接 step を候補別に記録する。加えて8色の既知 flat patch 中心で RGB code delta と8-bit sRGB code 値由来の OpenCV HSV hue / saturation 応答を記録する。hue は低彩度時に未定義とする。rd0 baseline の灰色 ramp は mean / p95 channel spread 0、luma reversal 率0、flat-step 率約0.430。smooth gradient の中央水平線は211 unique RGB triplet、最大隣接 step 1 code。これらは説明指標であり、知覚色差、banding の合否閾値や美的品質の証明ではない。定義と制約は `experiments/CinematicLooksLab/EVALUATION_PLAN.md` に記録した。

9 builtin LUT × 6 float RGBA probe 入力 × intensity 0/0.5/1 の source-equation probe を `captures/rd0/float_contract_probe.json` に保存した。加えて同一 straight RGB の alpha sweep と、synthetic fixture の8色 × 6 alpha patch × 3 intensity のmirror比較も記録した。rd0時点ではComposition QImage経路がpremultiplied descriptorをeffectへ渡す一方、LUTEffectはdescriptorを考慮せず出力QImage conversionはunpremultiplyするため、fixture mirrorの最大RGB差はhighContrast 1.000、lowContrast 1.321、vintage 0.881などだった。これは修正前sourceの履歴値である。2026-11-17にLUT CPU wrapperを修正し、rd3 source-equation mirrorでは全9 LUTのalpha>0 patchでstraight RGBとの最大差が2.3e-16未満。C++差分一致、実画像HDR品質、背景合成後のruntime画質は未検証。修正前後の値は`captures/rd0/findings.json`と`experiments/CinematicLooksLab/captures/rd3_lut_alpha_wrapper/float_contract_probe.json`に分けて記録した。

Color Balance CPU / HLSL equation mirror は5つの8-bit素材、strength 0.5 / 1.0 の全出力でbyte差0となった（source-equation証拠のみ）。修正前Tritone CPU / HLSL mirror はsource式が異なり、Cinematic strength 0.5/1.0で素材ごとのMAEは約10.6〜16.0 RGB codes、TealAndOrange 1.0は約6.9〜14.0。旧GPU式はstrength 0でも5素材の62〜100%のpixelを変化させ、飽和色 `(0.8,0.2,0.1)` を `(0.4112,0.3512,0.3412)` へ動かした。これはsource-mirror値でありC++実行比較ではない。現行HLSLはCPU式へ寄せ、CPU/GPU wrapperのpremultiplied-alpha処理も揃えた。rd1/rd2 source mirrorを再生成したが、GPU実行比較は未実施。根拠とcase別p95/maxは `captures/rd0_effect_extension/metrics.json`、open finding は修正前sourceに対する履歴として保持する。

Composition側`deserializeEffect()`のConcrete factory欠落はsource監査で検出され、後続フォローアップでLUT / Color Balance / Tritoneのfactory分岐と、同一型の`-2`以降instance ID認識を追加した。これはComposition restoreのsource path修正であり、project round-trip、非既定property保持、実レンダーは未検証。Layer側は引き続きEffect Service factoryを使う別経路である。受入ではCompositionとLayerへ同種effectを複数追加し、非既定preset / intensityで保存・再読込してConcrete type・全property・レンダー結果を確認する。

Composition / Render Queue の後続確認向けに、8色・6固定 alpha 値・水平 alpha ramp を持つ synthetic straight-alpha fixture [`experiments/CinematicLooksLab/assets/alpha-association-chart.png`](../../experiments/CinematicLooksLab/assets/alpha-association-chart.png) と SHA-256 manifest を追加した。評価計画には preview/final・CPU/GPU の capture 条件と複数背景の確認手順を記録したが、アプリへの適用・capture は未実施。

ブラインド視覚レビュー用に全95 ID（5素材 × 19候補）の空欄票 [`experiments/CinematicLooksLab/captures/rd0/review/BLIND_REVIEW_FORM.md`](../../experiments/CinematicLooksLab/captures/rd0/review/BLIND_REVIEW_FORM.md) を追加した。画像 harness は5素材すべての定義済み ROI 統計と full-resolution ROI crops を出し、blind full-sheet と18 ROI sheet、342個の候補名なし ID crops を生成する。manifest に harness SHA-256 と素材ごとの shuffle seed／方法を保存し、今回再生成後も harness hash 一致と5ケース分の seed を確認した。候補票に先立って無処理baselineのみを記録する [`experiments/CinematicLooksLab/captures/rd0/review/BASELINE_REVIEW_FORM.md`](../../experiments/CinematicLooksLab/captures/rd0/review/BASELINE_REVIEW_FORM.md) も追加した。両票は未記入で、candidate gate・人手レビュー・品質判定はまだ行っていない。

追加 effect extension は、baseline を含む35 blind IDs（5素材 × 7候補）、18 ROI contact sheets、126 full-resolution candidate-name-free crops を別ディレクトリに生成した。blind key は別 JSON。CPU source-mirror outputs のみが blind set に含まれ、GPU HLSL mirror outputs は `metrics.json` に CPU との source-equation deltas として記録した。AI triage は全5 full sheets と6選択ROI sheetのみ、未採点・gate未設定である。blank manual review form は [`experiments/CinematicLooksLab/captures/rd0_effect_extension/BLIND_REVIEW_FORM.md`](../../experiments/CinematicLooksLab/captures/rd0_effect_extension/BLIND_REVIEW_FORM.md)。

## 段階と受入条件

### 評価項目ごとの証拠状態

| 必須項目 | 現在の証拠 | 判定状態 / 次の確認 |
|---|---|---|
| 効果の正しさ | LUTEffect、Color Balance、Tritone CPU/GPU の pinned Python source mirrors、float probe | 三 family の応答式を同一素材で characterization 済み。Tritone CPU/GPU式の差を source-level で検出。C++ differential と製品 runtime は未実施 |
| プロジェクト保存・再読込 | Effect Preset file API のソース監査、Composition / Layer の別個の serializer・restore factory 経路 | Composition LUT / Color Balance / Tritone concrete restore分岐とinstance suffix対応を追加済み。実project round-trip、property保持、output captureは未実施 |
| 画質・細部 | deterministic chart、CC0 写真4点、ROI、三 family のsource-mirror contact sheets、baseline-only form、AI baseline preflight、95 ID + 35 ID の空欄レビュー票 | AI preflightはbaseline画像の記述だけでhuman review/gateではない。baseline reviewと人手candidate reviewは未実施。contact-sheet triageは未採点・非gate。baseline review後、candidateを開く前にgateを固定し、高解像度ROI cropsでconfirmatory比較する |
| 色・階調 | sRGB 8-bit 画像応答、全体／ROI の分布・差分統計、chart 灰色軸・グラデーション診断と既知8色 patch の RGB code delta・HSV hue / saturation 応答 | 応答測定済み。補助指標は候補応答の説明に限る。目標画像がないため品質 gate・ランキングは未確定 |
| HDR・alpha | LUT / Color Balance / Tritone CPU/GPU float equation probes、alpha sweep、source-level findings、synthetic straight-alpha fixture | 更新wrapperのsource mirrorではalpha 0.1におけるstraight RGBとの差はColor Balance CPU/GPU最大2.4e-7、Tritone CPU/GPU最大1.2e-7、LUT全9 preset最大2.3e-16未満。修正前Tritone GPU差はrd0の履歴finding。現行Tritone CPU/GPU mirrorはstrength 0でもHDR/負値をclampし、NaN/Inf入力から非有限RGBを出す。C++/runtime画像・複数背景確認は未実施 |
| 時間安定性 | LUT が時間不変である実装構造 | 同一静止入力の処理決定性は説明可能。内容変化時の見た目安定性は対象外・未検証 |
| 経路・解像度 | Composition / Render Queue CPU・GPU 経路のソース監査、Tritone CPU/HLSL equation comparison | Tritoneの式差はsource-mirrorで定量化済み。preview/final の実画像到達、backend 差、解像度比較はruntime未検証 |
| 性能 | NumPy/Python reference timing | 製品性能ではない。1080p/4K の実行時間・メモリ gate は未実施 |
| 採否・受入条件 | ブラウザー ID と builtin LUT の registry audit | preset 対応付け・採用候補の判断は未完了。品質 gate の確定後に recommendation を記録 |

この表の「準備済み」は画像品質の合格を示さない。全必須項目に判定根拠が揃うまでは R&D を In Progress とする。

1. **RD0 — harness と baseline:** deterministic chart と4点の CC0 実写素材（夜景2、人物1、森林1）で三 family の source-mirror response characterization を取得済み。manifest/source hash、ROI、統計、通常シート、固定 seed の blind contact sheets と別管理の対応表あり。人手判定、C++ differential check、実アプリ HDR/alpha、性能ゲートは残課題。候補ランキングは行っていない。
2. **RD1 — 既存 effect preset 比較:** 同一素材で LUT Cinematic、Color Balance Cinematic、Tritone Cinematic、Tritone TealAndOrange の CPU式応答・strength sweep・alpha/float probe と Tritone GPU HLSL式を記録済み。Tritone CPU/GPU予測差を finding に分離した。次は人手 baseline review 後に visual / technical thresholds を固定し、実 runtime parity を確認する。どの候補もまだ採用・不採用判定していない。
3. **RD2 — 製品経路の追跡:** source-level CPU call chain は `EVALUATION_PLAN.md` に記録済み。Composition preview／final、Render Queue CPU／GPU variants の実画像到達・適用順・backend parity を実行証拠と区別して確認する。実行できなかった経路は未検証とする。
4. **RD3 — Look mapping recommendation:** ブラウザーの Look ID ごとに実在 LUT/effect、補正 recipe、未定義状態を対応表にし、採用候補・保留・不採用と根拠をまとめる。R&D 完了は製品機能追加の承認ではない。

完了条件は評価ルールの全必須項目が判定済みであること、画像・ROI・manifest・統計・視覚票・欠陥記録が再現可能であること、未検証項目を残したまま品質達成と表現しないこと、後続実装の範囲と受入条件を記録することである。

## 検証制約

リポジトリ指示により、ユーザーから明示依頼があるまでビルド、CMake configure、テスト起動は行わない。本プロジェクトの R&D 証跡では、画像評価、Artifact 契約テスト、実アプリ経路、性能計測の証明範囲を区別する。
