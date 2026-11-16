# Cinematic Looks 画像補正 R&D プロジェクト

**最終更新:** 2026-11-16
**ステータス:** In Progress（評価計画・Python source mirror・rd0 応答測定を作成。視覚判定・品質 gate・製品経路検証は未実施）

## 目的

既存の組み込み LUT とカラー補正経路を使った静止画の cinematic look を再現可能な条件で評価し、プリセットとして提供する場合の見た目・技術契約・受入条件を明らかにする。

本プロジェクトは R&D／テスト用プロジェクトであり、プリセットブラウザーの製品接続、新しい LUT や補正処理の実装、既定値変更を含まない。評価結果をもって後続の製品実装を別途判断する。

## 現状のコード確認

- `Artifact/src/Effects/ColorCorrection/LUTEffect.cpp` は組み込み cinematic / vintage / cold / warm / contrast / desaturated / film-stock LUT と外部 LUT 選択、強度混合を実装している。
- `ArtifactCore/src/Color/ColorLUT.cpp` の `BuiltinLUTs` に LUT データ生成がある。ラボは 17³ 格子生成式と三線形 sampler を Python で mirror し、source hash を manifest に記録する。これは C++ 実装そのものの実行・一致テストではない。
- `Artifact/src/Widgets/ArtifactLooksPresetBrowser.cpp` には Film / Photo / Creative 等のブラウザーと名称上の cinematic packs があるが、サムネイルはプレースホルダーで、確認したソース範囲では `LUTEffect` や組み込み LUT の適用接続はない。
- したがって検証対象は「補正機能の有無」ではなく、「現行補正候補の画質・色/alpha 契約・素材間の応答・実アプリ経路」と「ブラウザーの preset 定義と実体の対応付け」である。

## 評価計画

必須の事前登録済み計画は [`../../experiments/CinematicLooksLab/EVALUATION_PLAN.md`](../../experiments/CinematicLooksLab/EVALUATION_PLAN.md) に置く。評価計画ルールは [`../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)。

評価対象計画には人物／屋外／夜景素材を含めるが、現在確保済みなのはチャートと CC0 夜景素材2点のみで、人物・屋外カテゴリは未達である。全体画像と細部・肌・階調・高輝度・alpha の領域を統計とブラインド視覚レビューで評価する計画だが、現 rd0 は 8-bit RGB 応答統計までであり、HDR/alpha・人手レビュー・品質 gate は未実施。

## 段階と受入条件

1. **RD0 — harness と baseline:** deterministic chart と2点の CC0 夜景素材で response characterization を取得済み。manifest/source hash、chart ROI、統計、レビューシートあり。素材カテゴリ不足、float HDR/alpha、C++ differential check、性能ゲートは残課題。候補ランキングは行っていない。
2. **RD1 — 既存 LUT 比較:** cinematic を含む全候補を同条件で適用し、強度応答、色・階調、HDR/alpha、細部、処理時間を測る。ゲートと目視票を保存する。
3. **RD2 — 製品経路の追跡:** LUT effect が Composition preview と final 出力まで到達するかをソース／契約／実行証拠ごとに分離して確認する。実行できなかった経路は未検証とする。
4. **RD3 — Look mapping recommendation:** ブラウザーの Look ID ごとに実在 LUT/effect、補正 recipe、未定義状態を対応表にし、採用候補・保留・不採用と根拠をまとめる。R&D 完了は製品機能追加の承認ではない。

完了条件は評価ルールの全必須項目が判定済みであること、画像・ROI・manifest・統計・視覚票・欠陥記録が再現可能であること、未検証項目を残したまま品質達成と表現しないこと、後続実装の範囲と受入条件を記録することである。

## 検証制約

リポジトリ指示により、ユーザーから明示依頼があるまでビルド、CMake configure、テスト起動は行わない。本プロジェクトの R&D 証跡では、画像評価、Artifact 契約テスト、実アプリ経路、性能計測の証明範囲を区別する。
