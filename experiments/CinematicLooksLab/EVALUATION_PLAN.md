# Cinematic Looks Lab — 事前登録済み画像評価計画

**最終更新:** 2026-11-16
**ステータス:** 4点の実写素材を含む rd0 応答測定と、float source-equation probe を取得済み。ブラインド接触シートを生成。実アプリ alpha 確認、判定票・数値 gate は未確定。

本計画は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う。目的は既存のカラー補正部品と、未接続の Looks プリセットブラウザー案を評価することであり、製品 UI の実装・採用を承認するものではない。

## 1. 目的と対象

静止画に対する「シネマティックなルック」を、再現可能な画像素材と条件で比較する。候補ルックの見栄えだけでなく、色域・階調・HDR 値・alpha・細部に対する応答、強度調整、素材間の頑健性を評価し、後続のプリセット提供方式（既存 LUT effect の整理、Look 定義の追加、ブラウザー接続）の要件を決める。

### 調査済みの実装境界

- `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp` は `BuiltinLUTs::cinematic()`、`vintage()`、`cold()`、`warm()`、`highContrast()`、`lowContrast()`、`desaturated()`、`kodak2383()`、`fuji3510()` および外部 LUT の選択を持つ。
- 一般の `Apply LUT` effect (`effect.colorcorrection.lut`) は Inspector effect catalog と `ArtifactEffectService` factory に登録されている。`ArtifactInspectorWidget::addSelectedEffectToCurrentTarget` は同 factory から effect を作成し、選択中 layer または composition の stack に追加する。`LUTEffect` は Custom + 9 builtin preset を持ち、初期値は Cinematic / intensity 1.0。ただし `Preset` は enum metadata を持たない Integer property であり、shared property editor は一般整数 editor を選ぶため、preset 名の chooser は確認できない。これは適用導線・property control のソース監査であり、画面操作・Looks browser との接続・品質受入を意味しない。
- `ArtifactCore/src/Color/ColorLUT.cpp` に組み込み LUT が定義され、`Artifact/include/Effects/ColorCorrection/LUTEffect.ixx` に effect のプリセット／強度プロパティがある。
- `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp` の別系統ブラウザーは名前、カテゴリ、パック、強度などを表示するが、サムネイルはプレースホルダー生成で、確認した範囲では LUT または effect 適用へ接続されていない。
- 実際の Composition／Render Queue 適用経路と、どの候補 LUT が最終画像へ到達するかはソース宣言だけでは確認完了として扱わず、本 R&D の統合確認で検証する。

### 対象

- 既存候補: 無処理 baseline、builtin cinematic、vintage、cold、warm、high contrast、low contrast、desaturated、Kodak 2383、Fuji 3510。各 LUT を同じ入力へ適用し、effect 強度 0 / 0.5 / 1.0 を比較する。
- オプション候補: Looks ブラウザーの「Teal & Orange」「Amber Dusk」「Muted Indie」などは、実体が既存 LUT と重複するかを調べるための名前上の候補に留める。コードにない補正式や見た目を既存機能とみなさない。
- 資材: 決定論チャート、`experiments/GlowLab/assets/SOURCES.md` の CC0 夜景写真2点、および `assets/SOURCES.md` の CC0 人物・森林写真を使用する。人物／肌、屋外緑・枝葉、夜景／点光源の3カテゴリを満たす。入力写真は見た目の ground truth ではない。
- 評価対象は静止画の全体および ROI。連番画像は今回対象外（時間変化を持たない LUT でも、フレームごとに内容が変わると見た目が揺れる素材依存性は別途残る）。

### 対象外

- 未接続ブラウザーの製品 UI、ユーザー向けプリセットの保存・配布・管理、および既定値変更
- 新しいシネマティック効果／LUT アルゴリズムの実装、外部 LUT のライセンス評価と配布
- 動画再生・時間的一貫性の製品保証、GPU/D3D12/Vulkan parity、Render Queue の画質・性能受入
- 任意の LUT を「映画フィルムの物理的正解」とみなすこと。既知の目標画像がなければ、ルックの良否は基準化された視覚レビューと技術的破綻の有無で評価する。

## 2. 比較条件と評価素材

| 素材 | 役割 | 参照としての扱い |
|---|---|---|
| 決定論チャート（グレー階調、skin-tone 近似パッチ、色相パッチ、細線、エッジ、グラデーション、暗部／高輝度パッチ） | 色・階調・細部の応答と破綻を測る | 入力画像は既知。ただし cinematic 出力の正解ではない |
| ライセンス記録済み静止画（最低3枚: 人物／肌、屋外緑・空、夜景・点光源） | 素材依存の見た目と局所的問題のレビュー | 入力のみ基準。映画ルックの ground truth ではない |
| 無処理 baseline | 全候補の同条件 control | LUT の完全一致出力ではない。中立強度 0 では入力保持契約の確認に利用 |
| 承認済み目標ルック画像（用意できた場合のみ） | 特定ルックの目標との差を測る | 出所・承認者・変換条件が記録された場合のみ限定的 reference とする |

初回 baseline では素材、生成 seed、寸法、SHA-256、ROI の座標と画素数に加え、生成 harness の SHA-256 を manifest に確定する。ブラインド順序は case ごとの導出 seed と shuffle 方法を記録する。画像を評価結果に合わせて差し替えない。

## 3. 固定画像条件

- 主要な LUT 応答は ArtifactCore `ColorLUT.cpp` の 17³ table generation / trilinear sampler を mirror した Python harness で測定し、source SHA-256 を manifest に記録する。これは C++ 実行結果の直接測定ではなく、source mirror の differential contract が未実装のため数値一致の証明ではない。
- 画像 harness は sRGB 8-bit RGB のみ。別の float source-equation probe は >1.0 RGB と alpha 0 / 0.1 / 0.25 / 0.5 / 1 を扱うが、C++ differential test、実画像 fixture、実アプリでの HDR 画質・透明エッジ評価は未実施。
- 表示用画像は固定した sRGB 変換で生成する。統計は可能な限り LUT 入出力の作業値で取り、表示変換後の統計と混同しない。
- 乱数を使う場合は seed を固定。LUT 適用自体は決定論的であることを記録する。
- 全候補で同一入力、解像度、ROI、alpha、色条件を使う。候補順は固定 seed で並べ替えた `review/blind/` の接触シートで隠し、対応表 `blind_review_key.json` は分離する。判定者と日付は人手レビュー後に記録する。
- レビュー手順・採点定義・全95ブラインド ID の空欄票を [`captures/rd0/review/BLIND_REVIEW_FORM.md`](captures/rd0/review/BLIND_REVIEW_FORM.md) に用意した。票の保存前に `blind_review_key.json` を開かず、技術欠陥と美的好みを分けて記録する。これはレビュー実施や品質 gate 確定の証拠ではない。

## 4. 必須評価・測定

| 項目 | 方法／領域 | 合否の決め方 |
|---|---|---|
| 効果の正しさ | LUT 定義の再現性、強度 0 / 0.5 / 1 の応答、同一設定の反復結果、中立時の入力保持 | 強度 0 は許容誤差内で入力と一致。中間強度は LUT 出力との線形混合契約をコードから確認し、単調な強度遷移を検証。ゲートは実測 baseline 前に固定 |
| 画質・細部 | 全体、肌、細線、髪・葉などの細部、点光源の等倍／拡大目視 | 色にじみ、banding、輪郭崩れ、ディテール消失、不要な clipping の重大欠陥がないこと。美的選好のみで合否を決めない |
| 色・階調 | RGB／luma／色差分布、色相・彩度パッチ、灰色軸の色かぶり、暗部／highlight の ROI | 各候補の応答を記録。入力値との差の小ささは品質基準にしない。色域外・NaN/Inf・予期しないクリップのゲートは baseline と実装 domain を確認後、候補比較前に確定 |
| HDR・alpha | >1.0 highlight、alpha 0 / 中間 / 1。透明画像を黒・白・中間色へ合成して境界を確認 | LUT が RGB のみ変更し alpha を維持する契約を確認。alpha 0 RGB は表示結果と別記。複数背景で halo／色縁を確認 |
| 時間安定性 | 効果自体は時間不変のため処理固有の flicker は対象外。静止 LUT を同じフレーム内容へ適用した決定性を確認 | 時間変化がないことは処理設計から説明する。内容変化に対するルック知覚の安定性は製品合否対象外として未検証 |
| 経路・解像度 | standalone LUT 契約と、実アプリの Effect Service / Composition preview / final 経路を個別に記録 | standalone 結果から製品経路 parity を推定しない。実アプリ経路は未実施なら未検証 |
| 性能 | 代表解像度 1080p / 4K の実行時間、処理回数、ピーク作業メモリ。ウォームアップ後複数回の中央値・p95 | 入力・環境・候補・測定手順を記録。閾値は rd0 baseline から候補比較前に確定 |

### 統計と視覚レビュー

- RGB / luma / alpha の min、max、mean、median、分位点、ヒストグラム、NaN/Inf 数、入力 domain を超えた画素率を全体および各 ROI で記録する。
- 入力との差分（MAE / RMSE / 最大・p95 / 閾値超過率）は LUT の応答量の記述に使い、正解への近さや画質合格の証明には使わない。
- 灰色軸の色偏差、色パッチの色相・彩度変化、エッジ周辺のチャンネル別 overshoot、グラデーションの段差を効果固有指標として検討する。評価空間・単位・許容値は baseline 後、候補比較の前に固定する。
- `metrics.json` は chart の既知 achromatic ramp 内部で RGB channel spread と水平 luma の reversal / flat-step 率、smooth / color gradient の中央行・列で 8-bit code の unique levels・flat-step / reversal 率・最大隣接 step を記録する。既知の8色 flat patch では中心画素の RGB code delta と、8-bit sRGB code 値から求めた OpenCV HSV hue / saturation 応答も記録する。hue は低彩度時に未定義とし、これらは候補応答の補助記述値であって、知覚色差・品質判定・gate ではない。gradient の曲線圧縮も意図的な LUT 応答として起こり得るため、人手の等倍確認と合わせて読む。エッジ overshoot 等の指標・品質閾値は引き続き未確定とする。
- 目視レビューは全体画像と ROI crop、同じ倍率・表示変換で行い、候補名を隠した順序でレビューする。各 case / ROI の blind contact sheet は full sheet と同じ ID・順序を使い、full-resolution crop は `captures/rd0/review/blind/crops/` に候補名でなく case / ROI / blind ID を付けて保存する。人物・夜景・森林の ROI も chart と同様に metrics / crops へ含める。画像全体・crop・数値・レビュー票を保存する。

### rd0 のコード経路監査

- `BuiltinLUTs::cinematic()` の実処理は各 RGB channel に独立して倍率（R×1.1、G×0.9、B×1.15）を掛けるだけで、コメントにある orange/teal の色相分離を実装していない。`kodak2383()` と `fuji3510()` もソースコメント上「簡易エミュレーション」であり、各 channel の power curve が中心である。名前や説明を映画フィルムの特性再現の根拠にしない。
- `ColorLUT::apply(float&, float&, float&)` は LUT lookup の前後で RGB を有限値チェックしつつ [0,1] に clamp する。`LUTEffect` の中間 intensity は元の float 値と clamp 済み LUT 結果を線形混合する。このため >1 highlight を LUT full intensity で保持する契約はソース上存在せず、HDR 対応を主張できない。人手画質 gate を満たしても HDR／working-space 対応は別評価が必要。
- `contract_probe.py` と `captures/rd0/float_contract_probe.json` に、9 builtin LUT × 6 float RGBA probe 入力 × intensity 0/0.5/1 の source-equation mirror を記録した。通常 probe には範囲外値・非有限値を含む。例: cinematic に `(4.0, 0.5, -0.25, 0.37)` を与えると intensity 0 は入力 RGB 保持、0.5 は `(2.5, 約0.475, -0.125)`、1 は `(1, 約0.45, 0)`。全強度で alpha は0.37のまま。これはコード式を翻訳した測定値で、実 C++ 実行との一致や色空間上の HDR 品質を示さない。
- Mirror audit で `ColorLUT::setValue` の格子点書き込み clamp を見落としていたため、Python mirror 2本を修正し `rd0` の画像・統計・contact sheet と float probe を再生成した。影響確認例は warm LUT の `(0, 0.98, 0)` green 出力で、格子点 clamp を含む結果は約0.995、旧 mirror は1.0へ飽和した。以降の rd0 は格子点 clamp を反映する。C++ differential evidence はまだない。
- `SOURCE_CONTRACT.json` は `ColorLUT.cpp` と `LUTEffect.cpp` の監査済み SHA-256 を pin する。どちらかが変わると画像 harness／float probe は source mirror が古い可能性を示して停止する。pin 更新には source の再監査、mirror 更新、証跡再生成を伴わせる。これは stale-source 防止であり C++ 実行一致の検査ではない。
- `LUTEffectCPUImpl` が `auto pixel = row[x]` で読み取った要素へ書く操作は、`ImageSurfaceView.ixx` の `ColorFloat4Channels<Component>` が channel を参照メンバーとして持つ proxy であるため backing buffer へ反映される。この view 定義も `SOURCE_CONTRACT.json` に pin した。これにより「alpha を維持し RGB を書く」という static source claim の参照先を明示するが、実 C++ 実行の証明ではない。
- Alpha association は単に A channel が変わらないだけでは不十分。`qImageCvMatSurfaceDescriptor` は `Format_ARGB32_Premultiplied` を sRGB/Premultiplied として float effect buffer に渡し、`LUTEffectCPUImpl` は descriptor を見ず premultiplied RGB を直接 LUT 入力にする。後段 `ImageF32x4_RGBA::toQImage` は premultiplied buffer を sRGB straight-alpha output へ unpremultiply する。したがって LUT が `F(alpha*C)` と `alpha*F(C)` で一致しない場合、同じ色の見え方が alpha に依存する候補がある。source-equation probe は straight color `(0.8,0.2,0.1)` を alpha `0 / 0.1 / 0.25 / 0.5 / 1` で全9 LUT・intensity 0/0.5/1について比較する。synthetic fixture の8色 × 6 alpha patch × 3 intensity の最大 per-channel RGB 差（output unpremultiply 後、0/1 clip 前／後）は highContrast `1.000000 / 1.000000`、lowContrast `1.321154 / 0.745686`、vintage `0.880769 / 0.395216`、fuji3510 `0.145529 / 0.145529`、kodak2383 `0.238881 / 0.079738`。cold は `0.040196 / 0.004902`、cinematic `0.043922 / 約0`、warm `0.138824 / 約0`、desaturated は約 `1e-7`。すべて alpha byte 0 を除外したsource-equation mirror 値で、after-clip は単純 [0,1] clamp（transfer encoding／8-bit quantization を含まない）。これは実画像、背景合成、C++ 実行一致、runtime 品質ではなく、runtime 優先順位のための診断値である。finding `RD0-ALPHA-ASSOCIATION-01` に全 Look の最大値を保存した。
- 実経路確認に使う synthetic RGBA 素材 `assets/alpha-association-chart.png` を生成した。8色の行それぞれに固定 alpha byte `0,26,64,128,192,255` の小パッチと、同じ RGB 値を保った水平 alpha 0..255 ramp を持つ。色・行・byte 条件・SHA-256 は隣接 manifest に記録し、再生成は `python experiments/CinematicLooksLab/make_alpha_association_fixture.py` で行う。PNG の入力 RGB は straight/unassociated。これは表示 ground truth ではなく、レビュー／runtime capture 用入力であり、まだ Artifact 経路へ適用していない。
- `contract_probe.py` は fixture の SHA-256、寸法、全色行の固定 alpha patch 値を manifest と照合してから、9 builtin LUT × 8色 × 6 alpha patch × intensity 0/0.5/1 の straight／premultiplied 経路を source-equation mirror で比較する。`float_contract_probe.json` の `alpha_fixture_patch_probe` に RGB差と、alpha byte 0 を除外した Look ごとの最大差を保存する。例えば alpha byte 26（約0.102）の入力では vintage の output clip 前最大 RGB 差が約0.881、clip 後が約0.395（skin approximation）、lowContrast は約1.321 / 約0.746（amber）。これはソース式上の診断値であり、C++ 実行・PNG decoder・Composition 合成の評価でも品質 gate でもない。
- **Alpha runtime protocol (未実行):** fixture を同一プロジェクトへ読み込み、全9 built-in LUT を strength 0 / 0.5 / 1 で適用する。source-mirror が強い alpha 差を示す highContrast / lowContrast / vintage / fuji3510 / kodak2383 を優先候補、他4種を対照として含める。出力は effect なし baseline と共に Composition preview、Composition final、Render Queue の利用可能な CPU/GPU 経路で lossless PNG capture し、8色 × alpha patch / ramp の RGB 出力を比較する。黒・白・50% gray・saturated magenta の背景上で合成表示を保存する。各 capture にアプリ revision、effect ID、strength、経路/backend、出力 hash、row/ROI、色管理設定を記録する。暫定期待は、同じ straight RGB の見える色が opacity だけで変わらないこと。ただし source-equation mirror と C++/runtime capture の一致、および preview/final/backend 間の差を確認するまでは gate を確定しない。
- 上記を `captures/rd0/findings.json` の `RD0-ALPHA-ASSOCIATION-01` として記録した。severity は視覚・runtime 影響確認まで未確定。gate を lock せず、人手レビューと path-specific output capture の対象候補とする。
- `ArtifactEffectService` は `effect.colorcorrection.lut` に `LUTEffect` を生成する factory を持つ。`LUTEffect` は `BuiltinLUTs` または custom LUT を保持し、CPU の float RGBA effect として RGB に LUT を適用し、alpha を書き換えない実装である。これはクラスと実装のソース確認であり、実アプリからの呼び出し実行を証明しない。
- `ArtifactPresetManager::effectToPresetJson` / `applyPresetJsonToEffect` と `ArtifactEffectService::saveEffectPreset` / `loadEffectPreset` は、effect id と properties（LUT `Preset` integer、`LUT File` string、`Intensity` float を含む）を保存し、既存 effect object へ再適用する汎用 per-effect file API を提供する。loader は対象 effect id と owned property payload を検証する。これはコード上の保存／復元契約であり、実際の round-trip、プロジェクト保存、named Looks library または browser 接続は未検証。監査した `ArtifactPresetManager.cpp` の hash は [`REGISTRY_AUDIT.json`](REGISTRY_AUDIT.json) に記録する。
- `ArtifactLooksPresetBrowser` はコード上の構築 callsite が見つからない。ダイアログ内の Apply は `appliedId_` とバッジを更新するだけで、effect 作成・Composition 反映・結果通知を行わず、公開 API は `appliedPresetId()` getter のみ。preset 定義／placeholder thumbnail から `LUTEffect` への対応付けも確認されていない。したがってブラウザー上の Film / Cinematic 名称が LUT の cinematic / Kodak / Fuji と一致するとは扱わない。
- **Composition-level LUT persistence finding (runtime 未確認):** Inspector は composition effect stack へ `effect.colorcorrection.lut` を追加できる。composition `toJson()` は id と properties を保存するが、`ArtifactAbstractComposition.cpp::deserializeEffect` は LUT ID を factory 生成せず、列挙された数種以外を base `ArtifactAbstractEffect` として復元する。base は LUT `Preset` / `LUT File` / `Intensity` を持たず、CPU/GPU implementation も作らないので、source path 上は saved LUT が project reload 後に LUT として動作しない。layer effect restore は `ArtifactEffectService::createEffect` を通じて concrete type を作成し、property を戻す別経路である。根拠は `captures/rd0/findings.json` の `RD0-COMPOSITION-LUT-RESTORE-01`。これは静的な経路比較で、project round-trip/render 実行はしていない。製品修正・実 runtime 確認には該当 project workflow の明示依頼が必要。
- CPU のソース経路は確認できた。`ArtifactCompositionViewDrawing` の layer rasterizer loop と composition final-effect loop は、Rasterizer stage の effect に `applyToCpuSurface` を呼ぶ。`LUTEffect` は CPU implementation を登録し GPU support は false なので、この呼び出しは `applyCPU` に到達する。Render Queue の通常 Beauty 出力では GPU readback 後にも composition final-effect helper が呼ばれるため、このエフェクト段はCPU上で実行される。マルチチャンネル出力分岐には同 helper 呼び出しがない。これはソース上の経路確認で、実行時の画像到達・出力一致を証明しない。
- `ArtifactFinalPostProcess::updateFromEffectStack` は LUT stack entry を見つけても実 LUT を upload せず、コメントで caller に `updateFromColorLUT` を直接呼ぶよう求める。`ArtifactIRenderer` の viewport display LUT upload は独立した display transform 経路であり、layer/composition `LUTEffect` の GPU parity を証明しない。Render Queue にも GPU render variant があるため、CPU final-effect helper の呼び出し有無・適用順・最終出力の一致は別途追跡・実行確認が必要。
- Looks browser は Inspector の一般 `Apply LUT` effect とは別の preset inventory のまま。実在の `LUTEffect` は effect service から生成可能だが、ブラウザー24 preset ID と LUT preset の対応付けは確認されていない。各登録箇所のソース hash と状態は `REGISTRY_AUDIT.json` に記録した。色空間／transfer function、HDR／alpha association も未検証のままとする。

## 5. ゲート確定と成果物

rd0 は素材・入力条件・baseline 測定のみを作成し、候補の視覚ランキングを行わない。rd0 の後に指標・ROI・許容差・性能予算を `captures/rd0/gates.json` に理由付きで確定する。確定後に候補比較 rd1 を行う。閾値を変更した場合、変更理由・旧新値・承認者・再実行範囲を記録する。

`captures/rdN/` に manifest、全測定値、gate 判定、通常／ブラインドのレビュー画像と crop、差分画像、環境・実行条件・判定票を保存する。素材は assets に格納し、取得元・利用条件・version/hash を記録する。

## 6. 実行条件、進捗と完了条件

- 初期研究はオフライン・CPU の決定論的 fixture と既存 `ColorLUT` 実装の評価を第一段階とする。GPU／Composition 経路は別の統合検証段階として明記し、未実施のまま製品 parity を主張しない。
- Python 画像統計 harness を用いる場合、依存（numpy、OpenCV、Pillow 等）と version を requirements / manifest に記録する。Artifact のテストやビルドを実行する場合は、別途明示依頼を得る。
- **現時点:** `captures/rd0/` に deterministic chart と CC0 実写4点（夜景2、人物1、森林1）の LUT 応答、分布統計、通常／ブラインド接触シート、source hash を記録済み。float source-equation probe も記録した。これは baseline/response characterization であり品質比較ではない。ブラインド人手判定、実画像での HDR/alpha と複数背景確認、事前 gate、1080p/4K 性能条件、実アプリ経路は未実施。
- R&D 完了には全必須評価の結果、採否判断、制限、後続実装の受入条件が必要。R&D 完了は製品 UI 導入や新規 LUT 実装を意味しない。
