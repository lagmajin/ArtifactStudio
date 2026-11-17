# Cinematic Looks Lab — 事前登録済み画像評価計画

**最終更新:** 2026-11-17
**ステータス:** 修正前rd0、Tritone更新後rd1/rd2、LUT premultiplied-alpha wrapper更新後の`rd1_lut_alpha_wrapper`とrun-ID修正済みrd3 source-mirrorを記録済み。製品コードに3 familyの名前付きpreset選択、Composition concrete-effect restore（同種effectのinstance suffix対応）、Tritone CPU式対応HLSL/alpha wrapper、LUT CPU alpha wrapperを追加。LUT alpha fixtureのsource-mirror差は全9 LUTで最大2.3e-16未満。shader/C++実行、Composition/runtime、品質gate、人手判定、HDR/alpha実経路は未確認。

本計画は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う。目的は既存のカラー補正部品と、別inventoryのLooksプリセットブラウザーを評価することである。ブラウザーの製品接続はユーザーの別途明示依頼により部分実装したが、R&D評価計画自体は製品UIや品質の採用判定を行わない。

## 1. 目的と対象

静止画に対する「シネマティックなルック」を、再現可能な画像素材と条件で比較する。実際に適用可能な候補はLUTEffect、ColorBalanceEffect、TritoneEffectの3 effect familyとして別々に扱い、24-ID Looks browser inventoryと分離する。`teal_orange`の暫定接続は既存Tritone応答を同browserから適用する1経路を作るが、他23 IDは未mappingである。候補ルックの見栄えだけでなく、色域・階調・HDR値・alpha・細部に対する応答、強度調整、素材間の頑健性を評価し、後続のpreset提供方式と残るbrowser integration要件を決める。

### 調査済みの実装境界

- `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp` は `BuiltinLUTs::cinematic()`、`vintage()`、`cold()`、`warm()`、`highContrast()`、`lowContrast()`、`desaturated()`、`kodak2383()`、`fuji3510()` および外部 LUT の選択を持つ。
- 一般の `Apply LUT` effect (`effect.colorcorrection.lut`) は Inspector effect catalog と `ArtifactEffectService` factory に登録されている。`ArtifactInspectorWidget::addSelectedEffectToCurrentTarget` は同 factory から effect を作成し、選択中 layer または composition の stack に追加する。`LUTEffect` は Custom + 9 builtin preset を持ち、初期値は Cinematic / intensity 1.0。製品フォローアップで `Preset` に既存enum chooser用tooltipを加えたが、画面表示と保存再読込は未確認。これは適用導線・property control のソース監査であり、LUT effect と Looks browser inventory 間の完全対応・品質受入を意味しない。
- `ArtifactCore/src/Color/ColorLUT.cpp` に組み込み LUT が定義され、`Artifact/include/Effects/ColorCorrection/LUTEffect.ixx` に effect のプリセット／強度プロパティがある。
- `ColorBalanceEffect` (`effect.colorcorrection.colorbalance`) は Custom / Neutral / CoolShadows / WarmHighlights / Cinematic を持ち、既定は Cinematic。`ColorBalanceSettings::cinematic()` は shadow/midtone/highlight 係数、range、preserve-luma を設定する。effect catalog、service factory、CPU/GPU implementation は存在する。Presetには名前選択metadataを追加したが、画面未確認。
- `TritoneEffect` (`effect.colorcorrection.tritone`) は Custom / Cinematic / TealAndOrange / WarmGold / ColdBlue を持ち、既定は Cinematic。`TritoneSettings::cinematic()` は shadow/midtone/highlight 色、balance、softness、color mix を設定する。effect catalog、service factory、CPU/GPU implementation は存在し、Presetには名前選択metadataを追加したが画面未確認。CPU/GPU wrappers は premultiplied descriptor時にunpremultiply/process/repremultiplyする実装へ更新した。rd2 source-mirror probeで確認するが、製品runtimeでのalpha応答は未確認。
- `ColorGrader::createCinematic()` / `createFilmLook()` API も存在するが、検索範囲で product callsite は見つからないため、接続済み effect preset として扱わない。
- Composition / Render Queue の実適用・保存復元はソース宣言だけでは完了扱いにせず、本 R&D の統合確認で検証する。LUT / Color Balance / Tritone の Composition deserializer concrete factory 欠落はrd0時点の監査findingとして `captures/rd0/findings.json` に残し、後続コードでLUT / Color Balance / Tritone factoryと同種effect instance suffix対応を追加した。project round-tripは未実行。
- `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp` のブラウザーは名前、カテゴリ、パックなどを持つ。サムネイルはplaceholder生成で、after側はsample previewとして表示する。2026-11-17の製品フォローアップでInspector Add Effect pickerからdialogを起動できるようにし、`teal_orange`だけTritone `TealAndOrange`へ暫定mappingした。Effect Service factoryでConcrete effectを作り、Effect Mixを設定してから現在のCompositionまたはLayerへ単一Undo追加を行う。残り23 IDは未mappingでApply不可。sourceはコード経路の存在を示すだけで、呼び出し到達、runtime、画質、Undo/redo、保存復元は未評価。
- 実際の Composition／Render Queue 適用経路と、どの候補 LUT が最終画像へ到達するかはソース宣言だけでは確認完了として扱わず、本 R&D の統合確認で検証する。

### 対象

- 取得済み `rd0` LUT lane: 無処理 baseline、builtin cinematic、vintage、cold、warm、high contrast、low contrast、desaturated、Kodak 2383、Fuji 3510。各 LUT の intensity 0 / 0.5 / 1.0 は同じ入力に対して測定済み。
- Color Balance lane: baseline と Cinematic preset の Strength 0 / 0.5 / 1.0 を同一素材で評価する。0 は baseline を兼ね、CPU effect の no-op branch を別の float probe で確認する。0.5 / 1.0 の5素材画像と統計を `captures/rd0_effect_extension/` に生成済み。
- Tritone lane (修正前rd0): CPU / GPU の source equation を別々に mirror し、Cinematic Master Strength 0 / 0.5 / 1.0 と TealAndOrange 1.0 を5素材で測定。CPU / GPU の式差、float domain、alpha association probe、画像・統計を同じ extension に保存した。製品HLSLはその後CPU `applyPixel` 式に合わせて変更したため、rd0結果は修正前GPU式の履歴であり、rd1/rd2でsource-mirrorを再生成したが、現行GPU応答の実行根拠にはしない。0.5 / 1.0 は LUT intensity と同じアルゴリズム・保証とはみなさない。
- 追加 lane は effect ID 別に source equation mirror / source hash pin を用意した。CPU source outputs を独立した blind contact sheet、18 ROI sheets、126 full-resolution crops、空欄票に含めた。人手レビューと Look 対応キーの照合は未実施。
- オプション候補: Looks ブラウザーの「Teal & Orange」「Amber Dusk」「Muted Indie」などは、実体が既存 LUT と重複するかを調べるための名前上の候補に留める。コードにない補正式や見た目を既存機能とみなさない。
- 資材: 決定論チャート、`experiments/GlowLab/assets/SOURCES.md` の CC0 夜景写真2点、および `assets/SOURCES.md` の CC0 人物・森林写真を使用する。人物／肌、屋外緑・枝葉、夜景／点光源の3カテゴリを満たす。入力写真は見た目の ground truth ではない。
- 評価対象は静止画の全体および ROI。連番画像は今回対象外（時間変化を持たない LUT でも、フレームごとに内容が変わると見た目が揺れる素材依存性は別途残る）。

### 対象外

- Looks Browserの残る23 ID mapping、プリセット保存／配布／管理、thumbnail生成、実画像preview、および既定値変更。`teal_orange`の暫定Effect Service適用経路は別途追加済みで、runtimeと品質は本評価計画で未検証。
- 新しいシネマティック効果／LUT アルゴリズムの実装、外部 LUT のライセンス評価と配布
- 動画再生・時間的一貫性の製品保証。D3D12/Vulkan と Render Queue の実 runtime acceptance はオフライン段階では未実施とするが、CPU/GPU の source equation parity は対象に含め、実測差を open finding として記録する。
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

- LUT 応答は ArtifactCore `ColorLUT.cpp` の 17³ table generation / trilinear sampler を mirror した Python harness で測定し、source SHA-256 を manifest に記録する。Color Balance / Tritone は `cinematic_effects_extension.py` が core processor 式と effect preset 値を別途 mirror し、6 source file の hash pin に記録する。いずれも C++ 実行結果の直接測定ではなく、数値一致の証明ではない。
- 写真／チャート画像は sRGB 8-bit RGB のみで、source processor が入力値に対して transfer conversion を行わないため、そのまま正規化値を式へ渡す。extension の float probe は unit-range／negative／HDR／NaN／Inf 入力、strength 0 / 0.5 / 1 と alpha association を CPU/GPU source equation で調べる。これは HDR 実画像、C++ differential test、実アプリの透明エッジ評価ではない。
- 表示用画像は固定した sRGB 変換で生成する。統計は可能な限り LUT 入出力の作業値で取り、表示変換後の統計と混同しない。
- 乱数を使う場合は seed を固定。LUT 適用自体は決定論的であることを記録する。
- 全候補で同一入力、解像度、ROI、alpha、色条件を使う。候補順は固定 seed で並べ替えた blind contact sheet で隠し、対応表 `blind_review_key.json` は分離する。判定者と日付は人手レビュー後に記録する。LUT 95 ID の票に加え、Color Balance / Tritone extension の35 ID（5素材 × baselineを含む7候補）を生成済み。
- レビュー手順・採点定義・全95ブラインド ID の空欄票を [`captures/rd0/review/BLIND_REVIEW_FORM.md`](captures/rd0/review/BLIND_REVIEW_FORM.md) に用意した。票の保存前に `blind_review_key.json` を開かず、技術欠陥と美的好みを分けて記録する。これはレビュー実施や品質 gate 確定の証拠ではない。
- rd2 color-effect extensionの4 full contact sheets（chart / forest / neon streets / portrait）をblind IDのままAI視覚triageし、見える色偏りとcontact-sheet倍率の限界を [`captures/rd2_effect_extension/AI_VISUAL_TRIAGE_2026-11-17.md`](captures/rd2_effect_extension/AI_VISUAL_TRIAGE_2026-11-17.md) に記録した。keyを開かず、候補順位・合否・品質gateは決めていない。人手レビューの代替ではない。

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
- **Gate order:** 探索的なrd0/rd1/rd2 source-mirror結果とAI contact-sheet triageは、応答のcharacterizationであり採否比較ではない。baseline-only画像を記述した[`AI baseline preflight`](captures/rd0/review/AI_BASELINE_PREFLIGHT_2026-11-17.md)もhuman reviewやgateの代替ではない。候補を見る前に、[`captures/rd0/review/BASELINE_REVIEW_FORM.md`](captures/rd0/review/BASELINE_REVIEW_FORM.md)でinput/無処理baselineだけを確認し、既存欠陥・表示条件・測定領域を記録する。そのbaseline evidenceをもとに技術/視覚/性能gateを`captures/rd0/gates.json`へ理由付きで固定してから、blank candidate formsの採点・key照合・confirmatory比較に進む。baseline reviewとgate固定が済むまではblind candidate formsを採点せず、gateを候補結果に合わせて後付けしない。

### rd0 のコード経路監査

- `BuiltinLUTs::cinematic()` の実処理は各 RGB channel に独立して倍率（R×1.1、G×0.9、B×1.15）を掛けるだけで、コメントにある orange/teal の色相分離を実装していない。`kodak2383()` と `fuji3510()` もソースコメント上「簡易エミュレーション」であり、各 channel の power curve が中心である。名前や説明を映画フィルムの特性再現の根拠にしない。
- `ColorLUT::apply(float&, float&, float&)` は LUT lookup の前後で RGB を有限値チェックしつつ [0,1] に clamp する。`LUTEffect` の中間 intensity は元の float 値と clamp 済み LUT 結果を線形混合する。このため >1 highlight を LUT full intensity で保持する契約はソース上存在せず、HDR 対応を主張できない。人手画質 gate を満たしても HDR／working-space 対応は別評価が必要。
- `contract_probe.py` と `captures/rd0/float_contract_probe.json` に、9 builtin LUT × 6 float RGBA probe 入力 × intensity 0/0.5/1 の source-equation mirror を記録した。通常 probe には範囲外値・非有限値を含む。例: cinematic に `(4.0, 0.5, -0.25, 0.37)` を与えると intensity 0 は入力 RGB 保持、0.5 は `(2.5, 約0.475, -0.125)`、1 は `(1, 約0.45, 0)`。全強度で alpha は0.37のまま。これはコード式を翻訳した測定値で、実 C++ 実行との一致や色空間上の HDR 品質を示さない。
- Mirror audit で `ColorLUT::setValue` の格子点書き込み clamp を見落としていたため、Python mirror 2本を修正し `rd0` の画像・統計・contact sheet と float probe を再生成した。影響確認例は warm LUT の `(0, 0.98, 0)` green 出力で、格子点 clamp を含む結果は約0.995、旧 mirror は1.0へ飽和した。以降の rd0 は格子点 clamp を反映する。C++ differential evidence はまだない。
- `SOURCE_CONTRACT.json` は `ColorLUT.cpp` と `LUTEffect.cpp` の監査済み SHA-256 を pin する。どちらかが変わると画像 harness／float probe は source mirror が古い可能性を示して停止する。pin 更新には source の再監査、mirror 更新、証跡再生成を伴わせる。これは stale-source 防止であり C++ 実行一致の検査ではない。
- `LUTEffectCPUImpl` が `auto pixel = row[x]` で読み取った要素へ書く操作は、`ImageSurfaceView.ixx` の `ColorFloat4Channels<Component>` が channel を参照メンバーとして持つ proxy であるため backing buffer へ反映される。この view 定義も `SOURCE_CONTRACT.json` にpinした。LUTEffectはこのproxyを通じ、Premultiplied descriptorの場合はstraight RGBで処理して再premultiplyし、その他のdescriptorでは従来どおり格納RGBを処理する。これはsource上の経路で、実C++実行の証明ではない。
- 旧実装のalpha association finding (RD0-ALPHA-ASSOCIATION-01) は単にA channelが変わらないだけでは不十分であることを示した。`qImageCvMatSurfaceDescriptor` は `Format_ARGB32_Premultiplied` を sRGB/Premultiplied として float effect buffer に渡し、`LUTEffectCPUImpl` は旧実装でdescriptorを見ず premultiplied RGB を直接 LUT 入力にしていた。2026-11-17の製品フォローアップで、descriptorがPremultipliedの場合にのみRGBをunpremultiplyし、straight値へLUT/strength mixを適用後に再premultiplyする処理へ更新した。alpha<=0は入力画素を保持する。後段 `ImageF32x4_RGBA::toQImage` は premultiplied buffer を sRGB straight-alpha output へ unpremultiply する。旧wrapperは `F(alpha*C)` を評価していたため同じ色の見え方がalphaに依存した。現行wrapperは `alpha*F(C)` と同等のstraight-domain処理を行う。source-equation probe は straight color `(0.8,0.2,0.1)` を alpha `0 / 0.1 / 0.25 / 0.5 / 1` で全9 LUT・intensity 0/0.5/1について比較する。synthetic fixture の8色 × 6 alpha patch × 3 intensity の最大 per-channel RGB 差（output unpremultiply 後、0/1 clip 前／後）は highContrast `1.000000 / 1.000000`、lowContrast `1.321154 / 0.745686`、vintage `0.880769 / 0.395216`、fuji3510 `0.145529 / 0.145529`、kodak2383 `0.238881 / 0.079738`。cold は `0.040196 / 0.004902`、cinematic `0.043922 / 約0`、warm `0.138824 / 約0`、desaturated は約 `1e-7`。すべて alpha byte 0 を除外したsource-equation mirror 値で、after-clip は単純 [0,1] clamp（transfer encoding／8-bit quantization を含まない）。これは修正前のsource-mirror値で、実画像、背景合成、C++実行一致、runtime品質を示さない。更新後の9 LUT×8色×6 alpha×3 intensity probeは `captures/rd1_lut_alpha_wrapper/float_contract_probe.json` に保存し、最大差2.3e-16未満、旧direct-RGB式の最大差1.321154を記録した。
- 実経路確認に使う synthetic RGBA 素材 `assets/alpha-association-chart.png` を生成した。8色の行それぞれに固定 alpha byte `0,26,64,128,192,255` の小パッチと、同じ RGB 値を保った水平 alpha 0..255 ramp を持つ。色・行・byte 条件・SHA-256 は隣接 manifest に記録し、再生成は `python experiments/CinematicLooksLab/make_alpha_association_fixture.py` で行う。PNG の入力 RGB は straight/unassociated。これは表示 ground truth ではなく、レビュー／runtime capture 用入力であり、まだ Artifact 経路へ適用していない。
- `contract_probe.py` は fixture の SHA-256、寸法、全色行の固定 alpha patch 値を manifest と照合してから、9 builtin LUT × 8色 × 6 alpha patch × intensity 0/0.5/1 の straight／premultiplied 経路を source-equation mirror で比較する。`float_contract_probe.json` の `alpha_fixture_patch_probe` に更新wrapperと旧direct-premultiplied式の差を並べ、alpha byte 0を除外したLookごとの最大差を保存する。rd3では出力先名をmanifest run IDにも反映し、`captures/rd3_lut_alpha_wrapper/float_contract_probe.json`へ別runとして記録した。更新後最大差は2.3e-16未満。旧式ではalpha byte 26（約0.102）のvintage約0.881、lowContrast約1.321。これはsource式診断値であり、C++実行・PNG decoder・Composition合成の評価でも品質gateでもない。
- **Alpha runtime protocol (未実行):** fixture を同一プロジェクトへ読み込み、LUT / Color Balance / Tritone を各 native strength 0 / 0.5 / 1 で適用する。LUT は修正後も複数presetと対照presetを含め、source mirrorとruntime出力の整合を確認する。Color Balance は premultiplied-aware control、Tritone Cinematic / TealAndOrange は alpha 候補として比較する。出力は Composition preview、Composition final、利用可能な Render Queue CPU/GPU 経路で lossless PNG capture し、8色 × alpha patch / ramp の RGB を複数背景へ合成して比較する。各 capture に revision、effect ID、preset、strength、path/backend、hash、ROI、color settings を記録する。C++ differential／runtime一致と経路差を確認するまで gate は確定しない。
- **Float/HDR runtime protocol (未実行):** float RGBA fixture に black, mid-gray, white, HDR >1, negative-channel, alpha 0 / 0.1 / 0.25 / 0.5 / 1 を用意し、Color Balance / Tritone の strength 0 / 0.5 / 1 を CPU / GPUで capture する。Tritone source mirror は strength 0 でも HDR 1.5 を1、負値を0へ clamp し、NaN/Infを sanitize しない挙動を示す。これは契約候補の根拠で、期待仕様・C++一致・実出力は未確定。関連 finding は `RD0-TRITONE-HDR-CLAMP-01`。
- 旧wrapperの問題を `captures/rd0/findings.json` の `RD0-ALPHA-ASSOCIATION-01` として記録した。source-level修正後もseverityと視覚影響はruntime確認まで未確定。gateをlockせず、人手レビューとpath-specific output captureを残す。
- `ArtifactEffectService` は `effect.colorcorrection.lut` に `LUTEffect` を生成する factory を持つ。`LUTEffect` は `BuiltinLUTs` または custom LUT を保持し、CPU の float RGBA effect として RGB に LUT を適用し、alpha を書き換えない実装である。これはクラスと実装のソース確認であり、実アプリからの呼び出し実行を証明しない。
- `ArtifactPresetManager::effectToPresetJson` / `applyPresetJsonToEffect` と `ArtifactEffectService::saveEffectPreset` / `loadEffectPreset` は、effect id と properties（LUT `Preset` integer、`LUT File` string、`Intensity` float を含む）を保存し、既存 effect object へ再適用する汎用 per-effect file API を提供する。loader は対象 effect id と owned property payload を検証する。これはコード上の保存／復元契約であり、実際の round-trip、プロジェクト保存、named Looks library または browser 接続は未検証。監査した `ArtifactPresetManager.cpp` の hash は [`REGISTRY_AUDIT.json`](REGISTRY_AUDIT.json) に記録する。
- `ArtifactLooksPresetBrowser` の初期監査時は生成callsiteがなく、Applyが内部IDとbadgeだけを更新していた。2026-11-17の更新後はInspector Add Effect pickerにcallsiteがあり、`teal_orange`のみTritone `TealAndOrange`をCompositionまたはLayerへ追加する暫定pathがある。追加effectはfactory生成後にPresetとEffect Mixを設定してからundoable addを行うため、部分適用時の別々のproperty Undoは発生しない。他23 IDは未mapping、`appliedPresetId()`は成功後だけIDを返す。mappingは名前一致に基づくproduct仮説で、正式仕様・runtime・画質受入の証拠ではない。 Film / Cinematic名称をLUT名との同一性と扱わない。
- **Composition-level LUT persistence finding (修正sourceあり、runtime未確認):** rd0監査ではInspectorから追加したcomposition `effect.colorcorrection.lut` が、reload時にbase `ArtifactAbstractEffect`へ復元されるsource経路を見つけた。`RD0-COMPOSITION-LUT-RESTORE-01` はその修正前snapshot。後続変更で `deserializeEffect` にLUT / Color Balance / TritoneのConcrete factory分岐と`-2`以降の同種instance suffix認識を追加した。source上の修正は存在するが、Composition project round-trip、property保持、effect render出力は未実行。layer側のEffect Service factory経路とは引き続き別に扱う。
- CPU のソース経路は確認できた。`ArtifactCompositionViewDrawing` の layer rasterizer loop と composition final-effect loop は、Rasterizer stage の effect に `applyToCpuSurface` を呼ぶ。`LUTEffect` は CPU implementation を登録し GPU support は false なので、この呼び出しは `applyCPU` に到達する。Render Queue の通常 Beauty 出力では GPU readback 後にも composition final-effect helper が呼ばれるため、このエフェクト段はCPU上で実行される。マルチチャンネル出力分岐には同 helper 呼び出しがない。これはソース上の経路確認で、実行時の画像到達・出力一致を証明しない。
- `ArtifactFinalPostProcess::updateFromEffectStack` は LUT stack entry を見つけても実 LUT を upload せず、コメントで caller に `updateFromColorLUT` を直接呼ぶよう求める。`ArtifactIRenderer` の viewport display LUT upload は独立した display transform 経路であり、layer/composition `LUTEffect` の GPU parity を証明しない。Render Queue にも GPU render variant があるため、CPU final-effect helper の呼び出し有無・適用順・最終出力の一致は別途追跡・実行確認が必要。
- Looks browser は Inspector の一般 `Apply LUT` effect とは別の preset inventory のままである。実在の `LUTEffect` は effect service から生成可能だが、24 ID中`teal_orange`だけを別familyのTritone `TealAndOrange`へ暫定接続し、browser IDとLUT presetの対応付けはない。他23 IDは未mapping。各登録箇所の初期監査hashと後続製品変更hashは `REGISTRY_AUDIT.json` に記録した。色空間／transfer function、HDR、alpha associationの実runtime挙動は未検証のままとする。LUT CPU source wrapperはpremultiplied対応へ更新済み。

## 5. ゲート確定と成果物

rd0 は素材・入力条件・baseline測定とsource応答characterizationを作成し、品質ランキングを行わない。既存rd1/rd2は式・応答の開発用mirrorであり、採否比較・品質gate evidenceではない。次はbaseline-only reviewで既存画像特性と表示条件を記録し、その後に指標・ROI・許容差・性能予算を `captures/rd0/gates.json` へ候補画像を見る前に理由付きで確定する。確定後のcandidate reviewをconfirmatory比較として扱う。閾値を後で変更した場合、変更理由・旧新値・承認者・再実行範囲を記録し、影響するconfirmatory比較をやり直す。

`captures/rdN/` に manifest、全測定値、gate 判定、通常／ブラインドのレビュー画像と crop、差分画像、環境・実行条件・判定票を保存する。素材は assets に格納し、取得元・利用条件・version/hash を記録する。

## 6. 実行条件、進捗と完了条件

### 製品実装フォローアップ（2026-11-16 / 17）

別途明示依頼により、LUT / Color Balance / Tritone の `Preset` を既存 enum chooser で名前選択できる metadata、Composition effect deserializer の concrete effect 分岐、Tritone HLSLをCPU `TritoneProcessor::applyPixel` と同じ画素式へ寄せる修正を追加した（`Artifact/src/Effects/ColorCorrection/*Effect.cpp`, `Artifact/src/Composition/ArtifactAbstractComposition.cpp`）。rd2ではTritone CPU/GPU wrappersのpremultiplied-alpha処理もColor Balanceと同様のunpremultiply/process/repremultiplyへ揃え、更新source pinでsource mirrorを別出力した。後続source auditでGPU wrapperのCPU fallbackにも同じalpha処理を追加し、premultiplied flag挿入で不足していた`ParamsCB` paddingを戻して80 byteのHLSL cbuffer境界へ合わせた。source pinは更新したが、既存rd2 captureは修正前source hashに結び付く履歴として保持する。2026-11-17にはLUT CPU wrapperもpremultiplied descriptorに対応し、straight RGBでLUTとstrength mix後に再premultiplyする処理を追加した。9 LUT×8色×6 alpha×3 strength probeは `captures/rd1_lut_alpha_wrapper/float_contract_probe.json` に保存し、更新後最大差2.3e-16未満、旧direct-RGB式の最大差1.321154を記録した。source mirrorはshader compileやC++/GPU differential、Composition round-trip、HDR/runtime、画質品質ゲートを代替しない。

- 初期研究はオフライン・CPU の決定論的 fixture と既存 `ColorLUT` 実装の評価を第一段階とする。GPU／Composition 経路は別の統合検証段階として明記し、未実施のまま製品 parity を主張しない。
- Python 画像統計 harness を用いる場合、依存（numpy、OpenCV、Pillow 等）と version を requirements / manifest に記録する。Artifact のテストやビルドを実行する場合は、別途明示依頼を得る。
- **現時点:** `captures/rd0/` にLUT 9種、`captures/rd0_effect_extension/` に修正前Color Balance / Tritone応答、`captures/rd1_effect_extension/` にTritone式更新後、`captures/rd2_effect_extension/` にTritone alpha wrapper更新後、`captures/rd1_lut_alpha_wrapper/` にLUT alpha wrapper更新後のsource-mirror応答を記録済み。rd2は同一5素材・ROIでstrength sweep、float domain、zero-strength、alpha association probeとblind画像を出力する。source equation mirrorの一致はC++/GPU実行差分ではない。ブラインド人手判定、HDR実経路、複数背景合成、Composition save/reload、preview/final/backend parity、1080p/4K性能、根拠付き品質gateは未実施。
- R&D 完了には全必須評価の結果、採否判断、制限、後続実装の受入条件が必要。R&D 完了は製品 UI 導入や新規 LUT 実装を意味しない。
