# Cinematic Looks 画像補正 R&D プロジェクト

**最終更新:** 2026-11-16
**ステータス:** In Progress（評価計画・Python source mirror・4実写ケースの rd0 応答測定・ブラインド接触シート・float 契約プローブを作成。人手視覚判定・品質 gate・製品 runtime 検証は未実施）

## 目的

既存の組み込み LUT とカラー補正経路を使った静止画の cinematic look を再現可能な条件で評価し、プリセットとして提供する場合の見た目・技術契約・受入条件を明らかにする。

本プロジェクトは R&D／テスト用プロジェクトであり、プリセットブラウザーの製品接続、新しい LUT や補正処理の実装、既定値変更を含まない。評価結果をもって後続の製品実装を別途判断する。

## 暫定結論（rd0・画質採否前）

- LUT 補正の基本 effect 自体は存在し、Inspector の effect catalog から選択中 layer または composition の stack へ追加するソース経路がある。`Preset` / `LUT File` / `Intensity` property があるが、`Preset` は名前付き chooser ではなく integer index 入力。
- 24件の Looks preset browser は別 inventory で、呼び出し箇所・effect 適用・builtin LUT との ID mapping が確認されていない。従って Film / Cinematic などの表示名だけから既存 LUT を割り当ててはならない。
- 既存 `Cinematic` は RGB 独立倍率（R×1.1、G×0.9、B×1.15）の単純な LUT。teal/orange の色相分離や、映画フィルム特性を再現するとの画質根拠はない。現時点では「既存の簡易 LUT effect は使えるが、検証済みのシネマティック Look browser/preset system は未導入」と表現するのが正確。
- product / visual acceptance は保留。ブラインドレビュー、alpha runtime capture、実 Composition / Render Queue の適用確認、品質 gate が残っている。alpha fixture source-equation mirror は highContrast / lowContrast / vintage / fuji3510 / kodak2383 などに alpha 依存差を示すが、runtime 欠陥の確定ではない。
- 追加の静的監査で、Composition effect stack の LUT は JSON に保存される一方、Composition deserializer が LUTEffect を生成せず base ArtifactAbstractEffect に落とす経路を確認した。base effect は LUT implementation と LUT 固有 property を持たない。従って Composition に追加した LUT が project reload 後に無効化される可能性が高い。layer 側は Effect Service factory で concrete effect を再生成する別経路。差は `captures/rd0/findings.json` の `RD0-COMPOSITION-LUT-RESTORE-01` に source evidence 付きで記録したが、project round-trip/render は未実行。

この結論はソース監査と Python source mirror に基づく暫定整理であり、実アプリの画面確認や画質受入を代替しない。

## 現状のコード確認

- `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp` は組み込み cinematic / vintage / cold / warm / contrast / desaturated / film-stock LUT と外部 LUT 選択、強度混合を実装している。
- 一般の `Apply LUT` effect は `InspectorEffectCatalog` に登録され、`ArtifactEffectService` が `effect.colorcorrection.lut` ID で factory 作成する。`ArtifactInspectorWidget::addSelectedEffectToCurrentTarget` は factory から生成し、選択中の layer または composition stack へ追加する。`LUTEffect` には Custom と9 builtin preset があり、既定 preset は Cinematic、強度は1.0。ただし `Preset` は enum option metadata のない Integer property で、shared property editor は名前付き chooser ではなく一般整数入力にする。コード上の値は0=Custom、1=Cinematic、2=Vintage、3=Cold、4=Warm、5=HighContrast、6=LowContrast、7=Desaturated、8=Kodak2383、9=Fuji3510。これは適用導線・property editor 選択の静的ソース確認で、画面操作・見た目の受入・実 runtime parity の証拠ではない。
- 既存の汎用 effect preset file API は effect ID、enabled/mix/mask state と property 値を JSON 保存し、既存 effect object への再適用に対応する。`LUTEffect` の `Preset`、`LUT File`、`Intensity` も保存対象で、load は effect ID と owned property payload を検証後に `setPropertyValue` へ戻す。これは個別 effect のファイル保存／復元 API の静的根拠で、専用 cinematic look library、Looks browser 接続、プロジェクト保存・再読込や runtime の実行証拠ではない。根拠 source hash は `experiments/CinematicLooksLab/REGISTRY_AUDIT.json` に追加した。
- `ArtifactCore/src/Color/ColorLUT.cpp` の `BuiltinLUTs` に LUT データ生成がある。ラボは 17³ 格子生成式と三線形 sampler を Python で mirror し、harness 自身・source・source contract の hash を manifest に記録する。各素材のブラインド順についても導出 seed と shuffle 方法を保存する。これは C++ 実装そのものの実行・一致テストではない。
- 2026-11-16 の mirror 監査で `ColorLUT::setValue` の格子点 clamp が Python mirror 2本で欠落していたため修正し、画像・統計・ブラインドシート・float probe を再生成した。例として warm LUT の `(0,0.98,0)` は格子点 clamp ありで green≈0.995、旧 mirror は1.0。source hash は現行 C++ を指すが、C++ 直接比較は未実施。
- `experiments/CinematicLooksLab/SOURCE_CONTRACT.json` に `ColorLUT.cpp` / `LUTEffect.cpp` の監査済み source hash と mirror 契約を pin し、変更検出時は harness を停止する。これで未レビュー source から stale mirror の rd0 を再生成しない。
- alpha保持の根拠となる `ImageSurfaceView.ixx` も source pin に追加した。mutable row indexing は reference member を持つ channel proxy を返すため、LUTEffect の `auto pixel = row[x]` と RGB member 書き込みは backing buffer に反映される。これは静的型定義の確認で、runtime 実行証拠ではない。
- ソース上の cinematic は RGB 独立倍率（R×1.1、G×0.9、B×1.15）で、orange/teal の色相分離処理ではない。Kodak 2383 / Fuji 3510 も簡易的な channel power curve。`ColorLUT::apply` が入力・出力 RGB を [0,1] に clamp するため HDR >1 の保持契約もない。これらは実装の記述であり、目視品質の判定ではない。
- `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp` には Film / Photo / Creative 等のブラウザーと名称上の cinematic packs があるが、サムネイルはプレースホルダー。検索したコード内にダイアログ構築 callsite がなく、Apply は内部 `appliedId_` とバッジだけを更新し、effect 作成・Composition 反映・結果通知を行わない。LUT effect service への preset ID 対応付けも確認できない。
- `LUTEffect` は effect service から生成でき、layer/composition の CPU Rasterizer effect loop は `applyToCpuSurface` を呼ぶ。Render Queue の通常 Beauty 出力は GPU readback 後に composition final-effect helper を呼ぶため、この段は CPU effect 実行となる。マルチチャンネル出力分岐では同 helper を呼ばない。一方、final GPU post-process の effect-stack handler は stack の LUT entry を確認するだけで、コメント上は LUT upload を caller に委ねている。`Artifact/src` と `Artifact/include` の callsite 検索では `updateFromEffectStack` 呼び出しは見つからず、`updateFromColorLUT` の唯一の呼び出しは `ArtifactIRenderer` の viewport display LUT upload（独立した display transform 経路）だった。したがってこれらは一般 `LUTEffect` の GPU 適用を示さない。関連3 source の SHA-256 と callsite 監査範囲を `REGISTRY_AUDIT.json` に追加した。CPU 経路のソース追跡はできたが、実アプリでの画像到達・backend parity は未確認。
- ブラウザー24 preset ID と9 builtin LUT の source-hash 付き一覧を [`REGISTRY_AUDIT.json`](../../experiments/CinematicLooksLab/REGISTRY_AUDIT.json) に記録した。preset ID→LUT の明示的な mapping は見つからず、名称類似のみで割当しない。
- 利用可能性を混同しないこと: `Apply LUT` の一般 effect は既に Inspector / Effect Service に存在するが、別の Looks preset browser は callsite と effect 接続がなく、その24 ID はいずれも LUT mapping 未定義。`REGISTRY_AUDIT.json` に各ソース hash と両者の境界を記録した。
- したがって検証対象は「補正機能の有無」ではなく、「現行補正候補の画質・色/alpha 契約・素材間の応答・CPU/GPU実経路」と「ブラウザーの preset 定義と実体の対応付け」である。

## 評価計画

必須の事前登録済み計画は [`../../experiments/CinematicLooksLab/EVALUATION_PLAN.md`](../../experiments/CinematicLooksLab/EVALUATION_PLAN.md) に置く。評価計画ルールは [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。

評価素材は deterministic chart と CC0 実写4点（夜景2、人物1、森林1）を確保し、人物／肌・屋外緑・夜景／点光源の3カテゴリを含む。全体画像と細部・肌・階調・高輝度・alpha の領域を統計とブラインド視覚レビューで評価する計画だが、現 rd0 の画像応答統計は 8-bit RGB とブラインド接触シートまでであり、HDR/alpha の画像評価・人手判定・品質 gate は未実施。CPU Rasterizer のソース呼び出し経路は追跡済み、GPU/final の runtime parity は未検証。

chart の灰色 ramp ROI では RGB channel spread と水平 luma の reversal / flat-step 率を、smooth / color gradient では中央行・列の unique code level、flat-step / reversal 率、最大隣接 step を候補別に記録する。加えて8色の既知 flat patch 中心で RGB code delta と8-bit sRGB code 値由来の OpenCV HSV hue / saturation 応答を記録する。hue は低彩度時に未定義とする。rd0 baseline の灰色 ramp は mean / p95 channel spread 0、luma reversal 率0、flat-step 率約0.430。smooth gradient の中央水平線は211 unique RGB triplet、最大隣接 step 1 code。これらは説明指標であり、知覚色差、banding の合否閾値や美的品質の証明ではない。定義と制約は `experiments/CinematicLooksLab/EVALUATION_PLAN.md` に記録した。

9 builtin LUT × 6 float RGBA probe 入力 × intensity 0/0.5/1 の source-equation probe を `captures/rd0/float_contract_probe.json` に保存した。加えて同一 straight RGB の alpha sweep と、synthetic fixture の8色 × 6 alpha patch × 3 intensity の mirror 比較も記録した。Composition QImage 経路は premultiplied descriptor を effect に渡す一方、LUTEffect は descriptor を考慮せず、出力 QImage conversion は unpremultiply する。fixture mirror の output unpremultiply 後最大RGB差（単純[0,1]clip 前／後）は highContrast 1.000 / 1.000、lowContrast 1.321 / 0.746、vintage 0.881 / 0.395、fuji3510 0.146 / 0.146、kodak2383 0.239 / 0.080。これは9 LUT全体からruntime候補を選ぶ診断値で、C++ 差分一致、実画像 HDR 品質、背景合成後の runtime 画質は未検証。全候補の値は `captures/rd0/findings.json` に記録した。

Composition / Render Queue の後続確認向けに、8色・6固定 alpha 値・水平 alpha ramp を持つ synthetic straight-alpha fixture [`experiments/CinematicLooksLab/assets/alpha-association-chart.png`](../../experiments/CinematicLooksLab/assets/alpha-association-chart.png) と SHA-256 manifest を追加した。評価計画には preview/final・CPU/GPU の capture 条件と複数背景の確認手順を記録したが、アプリへの適用・capture は未実施。

ブラインド視覚レビュー用に全95 ID（5素材 × 19候補）の空欄票 [`experiments/CinematicLooksLab/captures/rd0/review/BLIND_REVIEW_FORM.md`](../../experiments/CinematicLooksLab/captures/rd0/review/BLIND_REVIEW_FORM.md) を追加した。画像 harness は5素材すべての定義済み ROI 統計と full-resolution ROI crops を出し、blind full-sheet と18 ROI sheet、342個の候補名なし ID crops を生成する。manifest に harness SHA-256 と素材ごとの shuffle seed／方法を保存し、今回再生成後も harness hash 一致と5ケース分の seed を確認した。票は未記入で、候補名との照合・人手レビュー・品質判定はまだ行っていない。

## 段階と受入条件

### 評価項目ごとの証拠状態

| 必須項目 | 現在の証拠 | 判定状態 / 次の確認 |
|---|---|---|
| 効果の正しさ | Python source mirror、float probe、source hash pin | 応答式の参考確認のみ。C++ differential 契約確認は未実施 |
| プロジェクト保存・再読込 | Effect Preset file API のソース監査、Composition / Layer の別個の serializer・restore factory 経路 | Composition-level LUT restore の source-level failure candidate を検出。実 project round-trip と output capture は未実施 |
| 画質・細部 | deterministic chart、CC0 写真4点、ROI、通常／ブラインド sheet、95 ID の空欄レビュー票 | 人手の等倍・拡大レビューは未実施。欠陥と好みを分けた票が必要 |
| 色・階調 | sRGB 8-bit 画像応答、全体／ROI の分布・差分統計、chart 灰色軸・グラデーション診断と既知8色 patch の RGB code delta・HSV hue / saturation 応答 | 応答測定済み。補助指標は候補応答の説明に限る。目標画像がないため品質 gate・ランキングは未確定 |
| HDR・alpha | float source-equation probe、alpha 0/0.1/0.25/0.5/1 sweep、source-level alpha path finding、synthetic runtime fixture | fixture は準備済み。C++/runtime画像、HDR画質、黒・白・中間色背景への合成確認は未実施 |
| 時間安定性 | LUT が時間不変である実装構造 | 同一静止入力の処理決定性は説明可能。内容変化時の見た目安定性は対象外・未検証 |
| 経路・解像度 | Composition / Render Queue CPU・GPU 経路のソース監査 | preview/final の実画像到達、backend 差、解像度比較は未検証 |
| 性能 | NumPy/Python reference timing | 製品性能ではない。1080p/4K の実行時間・メモリ gate は未実施 |
| 採否・受入条件 | ブラウザー ID と builtin LUT の registry audit | preset 対応付け・採用候補の判断は未完了。品質 gate の確定後に recommendation を記録 |

この表の「準備済み」は画像品質の合格を示さない。全必須項目に判定根拠が揃うまでは R&D を In Progress とする。

1. **RD0 — harness と baseline:** deterministic chart と4点の CC0 実写素材（夜景2、人物1、森林1）で response characterization を取得済み。manifest/source hash、ROI、統計、通常シート、固定 seed のブラインド接触シートと別管理の対応表あり。人手判定、float HDR/alpha、C++ differential check、性能ゲートは残課題。候補ランキングは行っていない。
2. **RD1 — 既存 LUT 比較:** 人手 baseline review 後に threshold を固定し、cinematic を含む全候補を同条件で適用して、強度応答、色・階調、HDR/alpha、細部、処理時間を測る。ゲートと目視票を保存する。
3. **RD2 — 製品経路の追跡:** source-level CPU call chain は `EVALUATION_PLAN.md` に記録済み。Composition preview／final、Render Queue CPU／GPU variants の実画像到達・適用順・backend parity を実行証拠と区別して確認する。実行できなかった経路は未検証とする。
4. **RD3 — Look mapping recommendation:** ブラウザーの Look ID ごとに実在 LUT/effect、補正 recipe、未定義状態を対応表にし、採用候補・保留・不採用と根拠をまとめる。R&D 完了は製品機能追加の承認ではない。

完了条件は評価ルールの全必須項目が判定済みであること、画像・ROI・manifest・統計・視覚票・欠陥記録が再現可能であること、未検証項目を残したまま品質達成と表現しないこと、後続実装の範囲と受入条件を記録することである。

## 検証制約

リポジトリ指示により、ユーザーから明示依頼があるまでビルド、CMake configure、テスト起動は行わない。本プロジェクトの R&D 証跡では、画像評価、Artifact 契約テスト、実アプリ経路、性能計測の証明範囲を区別する。
