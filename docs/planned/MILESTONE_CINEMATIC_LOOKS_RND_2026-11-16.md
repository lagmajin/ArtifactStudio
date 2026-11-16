# Cinematic Looks 画像補正 R&D プロジェクト

**最終更新:** 2026-11-16
**ステータス:** In Progress（評価計画・Python source mirror・4実写ケースの rd0 応答測定・ブラインド接触シート・float 契約プローブを作成。人手視覚判定・品質 gate・製品 runtime 検証は未実施）

## 目的

既存の組み込み LUT とカラー補正経路を使った静止画の cinematic look を再現可能な条件で評価し、プリセットとして提供する場合の見た目・技術契約・受入条件を明らかにする。

本プロジェクトは R&D／テスト用プロジェクトであり、プリセットブラウザーの製品接続、新しい LUT や補正処理の実装、既定値変更を含まない。評価結果をもって後続の製品実装を別途判断する。

## 現状のコード確認

- `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp` は組み込み cinematic / vintage / cold / warm / contrast / desaturated / film-stock LUT と外部 LUT 選択、強度混合を実装している。
- `ArtifactCore/src/Color/ColorLUT.cpp` の `BuiltinLUTs` に LUT データ生成がある。ラボは 17³ 格子生成式と三線形 sampler を Python で mirror し、source hash を manifest に記録する。これは C++ 実装そのものの実行・一致テストではない。
- 2026-11-16 の mirror 監査で `ColorLUT::setValue` の格子点 clamp が Python mirror 2本で欠落していたため修正し、画像・統計・ブラインドシート・float probe を再生成した。例として warm LUT の `(0,0.98,0)` は格子点 clamp ありで green≈0.995、旧 mirror は1.0。source hash は現行 C++ を指すが、C++ 直接比較は未実施。
- `experiments/CinematicLooksLab/SOURCE_CONTRACT.json` に `ColorLUT.cpp` / `LUTEffect.cpp` の監査済み source hash と mirror 契約を pin し、変更検出時は harness を停止する。これで未レビュー source から stale mirror の rd0 を再生成しない。
- ソース上の cinematic は RGB 独立倍率（R×1.1、G×0.9、B×1.15）で、orange/teal の色相分離処理ではない。Kodak 2383 / Fuji 3510 も簡易的な channel power curve。`ColorLUT::apply` が入力・出力 RGB を [0,1] に clamp するため HDR >1 の保持契約もない。これらは実装の記述であり、目視品質の判定ではない。
- `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp` には Film / Photo / Creative 等のブラウザーと名称上の cinematic packs があるが、サムネイルはプレースホルダー。検索したコード内にダイアログ構築 callsite がなく、Apply は内部 `appliedId_` とバッジだけを更新し、effect 作成・Composition 反映・結果通知を行わない。LUT effect service への preset ID 対応付けも確認できない。
- `LUTEffect` は effect service から生成でき、layer/composition の CPU Rasterizer effect loop は `applyToCpuSurface` を呼ぶ。Render Queue の通常 Beauty 出力は GPU readback 後に composition final-effect helper を呼ぶため、この段は CPU effect 実行となる。マルチチャンネル出力分岐では同 helper を呼ばない。一方、final GPU post-process の effect-stack handler は stack の LUT entry を確認するだけで、コメント上は LUT upload を caller に委ねている。CPU 経路のソース追跡はできたが、実アプリでの画像到達・backend parity は未確認。
- ブラウザー24 preset ID と9 builtin LUT の source-hash 付き一覧を [`REGISTRY_AUDIT.json`](../../experiments/CinematicLooksLab/REGISTRY_AUDIT.json) に記録した。preset ID→LUT の明示的な mapping は見つからず、名称類似のみで割当しない。
- したがって検証対象は「補正機能の有無」ではなく、「現行補正候補の画質・色/alpha 契約・素材間の応答・CPU/GPU実経路」と「ブラウザーの preset 定義と実体の対応付け」である。

## 評価計画

必須の事前登録済み計画は [`../../experiments/CinematicLooksLab/EVALUATION_PLAN.md`](../../experiments/CinematicLooksLab/EVALUATION_PLAN.md) に置く。評価計画ルールは [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。

評価素材は deterministic chart と CC0 実写4点（夜景2、人物1、森林1）を確保し、人物／肌・屋外緑・夜景／点光源の3カテゴリを含む。全体画像と細部・肌・階調・高輝度・alpha の領域を統計とブラインド視覚レビューで評価する計画だが、現 rd0 は 8-bit RGB 応答統計とブラインド接触シートまでであり、HDR/alpha・人手判定・品質 gate は未実施。CPU Rasterizer のソース呼び出し経路は追跡済み、GPU/final の runtime parity は未検証。

9 builtin LUT × 6 float RGBA 入力 × intensity 0/0.5/1 の source-equation probe を `captures/rd0/float_contract_probe.json` に保存した。これは clamp・HDR 値・alpha の CPU 式 mirror 応答であり、C++ 差分一致、実画像の HDR 品質、alpha association の検証ではない。

## 段階と受入条件

1. **RD0 — harness と baseline:** deterministic chart と4点の CC0 実写素材（夜景2、人物1、森林1）で response characterization を取得済み。manifest/source hash、ROI、統計、通常シート、固定 seed のブラインド接触シートと別管理の対応表あり。人手判定、float HDR/alpha、C++ differential check、性能ゲートは残課題。候補ランキングは行っていない。
2. **RD1 — 既存 LUT 比較:** 人手 baseline review 後に threshold を固定し、cinematic を含む全候補を同条件で適用して、強度応答、色・階調、HDR/alpha、細部、処理時間を測る。ゲートと目視票を保存する。
3. **RD2 — 製品経路の追跡:** source-level CPU call chain は `EVALUATION_PLAN.md` に記録済み。Composition preview／final、Render Queue CPU／GPU variants の実画像到達・適用順・backend parity を実行証拠と区別して確認する。実行できなかった経路は未検証とする。
4. **RD3 — Look mapping recommendation:** ブラウザーの Look ID ごとに実在 LUT/effect、補正 recipe、未定義状態を対応表にし、採用候補・保留・不採用と根拠をまとめる。R&D 完了は製品機能追加の承認ではない。

完了条件は評価ルールの全必須項目が判定済みであること、画像・ROI・manifest・統計・視覚票・欠陥記録が再現可能であること、未検証項目を残したまま品質達成と表現しないこと、後続実装の範囲と受入条件を記録することである。

## 検証制約

リポジトリ指示により、ユーザーから明示依頼があるまでビルド、CMake configure、テスト起動は行わない。本プロジェクトの R&D 証跡では、画像評価、Artifact 契約テスト、実アプリ経路、性能計測の証明範囲を区別する。
