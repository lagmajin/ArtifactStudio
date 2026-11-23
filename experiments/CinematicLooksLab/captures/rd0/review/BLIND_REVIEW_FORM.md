# rd0 ブラインド画像レビュー票

**最終更新:** 2026-11-16
**状態:** 未記入テンプレート。品質判定・gate はまだ確定していない。

## ブラインド素材

- [Chart contact sheet](blind/chart_contact_sheet.png)
- [Neon streets contact sheet](blind/neon_streets_contact_sheet.png)
- [Street lights contact sheet](blind/street_lights_contact_sheet.png)
- [Portrait contact sheet](blind/portrait_contact_sheet.png)
- [Forest contact sheet](blind/forest_contact_sheet.png)

### ROI contact sheets (same blind IDs and ordering)

**Chart:** [gray ramp](blind/chart__roi_gray_ramp_contact_sheet.png) · [skin patches](blind/chart__roi_skin_patches_contact_sheet.png) · [color patches](blind/chart__roi_color_patches_contact_sheet.png) · [fine lines](blind/chart__roi_fine_lines_contact_sheet.png) · [step edges](blind/chart__roi_step_edges_contact_sheet.png) · [dark / highlight](blind/chart__roi_dark_highlight_contact_sheet.png) · [smooth gradient](blind/chart__roi_smooth_gradient_contact_sheet.png) · [color gradient](blind/chart__roi_color_gradient_contact_sheet.png)

**Neon streets:** [intersection lights](blind/neon_streets__roi_intersection_lights_contact_sheet.png) · [dark buildings](blind/neon_streets__roi_dark_buildings_contact_sheet.png)

**Street lights:** [lamp / glare](blind/street_lights__roi_lamp_and_glare_contact_sheet.png) · [lower sign lights](blind/street_lights__roi_lower_sign_lights_contact_sheet.png)

**Portrait:** [face / skin](blind/portrait__roi_face_skin_contact_sheet.png) · [hair / face](blind/portrait__roi_hair_and_face_contact_sheet.png) · [bright window](blind/portrait__roi_bright_window_contact_sheet.png)

**Forest:** [sunlit canopy](blind/forest__roi_sunlit_canopy_contact_sheet.png) · [deep foliage](blind/forest__roi_deep_foliage_contact_sheet.png) · [fine branches](blind/forest__roi_fine_branches_contact_sheet.png)

Full-resolution, candidate-name-free crops use `blind/crops/<case>__<roi>__<blind-id>.png` naming.

## 手順

1. `blind/` の候補名を伏せた contact sheet を見る。対応表 `../blind_review_key.json` は、記入を終えて票を保存するまで開かない。
2. 各ケースで全体を同じ表示倍率で見てから、上記の対応する ROI contact sheet を確認する。特定候補の細部を拡大するときは `blind/crops/` の blind ID crop を使い、同一 ID の全候補で倍率と表示変換を揃える。表示環境、倍率、レビュー日、判定者を下欄へ記録する。
3. まず技術的欠陥を記録し、次に見た目の好みを記録する。入力素材にもともとある特徴と、候補処理で増えた特徴を区別する。入力画像は cinematic 出力の正解画像ではない。
4. 全ケースの記入を保存した後にのみ対応キーを開き、Look ID と照合する。美的選好だけで合否を確定しない。

## 評点

- **技術品質:** 0=重大な破綻、1=明確な懸念、2=許容可能、3=問題を確認せず、N/A=評価不能。
- **ルックの一貫性:** 1=意図不明／素材を損なう、2=弱いまたは素材依存、3=明瞭だが対象を保つ、4=明瞭で素材にも適合。
- **好み:** 1〜5。主観値で、品質合否には使わない。
- 欠陥がある場合は場所（全体または ROI）、症状、重大度（軽微／中／重大）を必ず記す。判断できない項目は N/A として理由を記す。

## レビュー条件

- 判定者:
- 日付（YYYY-MM-DD）:
- OS / 表示装置・色モード:
- 表示倍率:
- 表示アプリと version:
- メモ:

## ケース票

各ケースの各候補について、技術品質・ルックの一貫性・好み・欠陥と場所を記録する。必要なら同じ表を複製して判定者ごとに分ける。

### Chart

| ID | 技術品質 (0–3/N/A) | 一貫性 (1–4) | 好み (1–5) | 欠陥・場所・重大度 / メモ |
|---|---:|---:|---:|---|
| CH-01 | | | | |
| CH-02 | | | | |
| CH-03 | | | | |
| CH-04 | | | | |
| CH-05 | | | | |
| CH-06 | | | | |
| CH-07 | | | | |
| CH-08 | | | | |
| CH-09 | | | | |
| CH-10 | | | | |
| CH-11 | | | | |
| CH-12 | | | | |
| CH-13 | | | | |
| CH-14 | | | | |
| CH-15 | | | | |
| CH-16 | | | | |
| CH-17 | | | | |
| CH-18 | | | | |
| CH-19 | | | | |

### Neon streets

| ID | 技術品質 (0–3/N/A) | 一貫性 (1–4) | 好み (1–5) | 欠陥・場所・重大度 / メモ |
|---|---:|---:|---:|---|
| NE-01 | | | | |
| NE-02 | | | | |
| NE-03 | | | | |
| NE-04 | | | | |
| NE-05 | | | | |
| NE-06 | | | | |
| NE-07 | | | | |
| NE-08 | | | | |
| NE-09 | | | | |
| NE-10 | | | | |
| NE-11 | | | | |
| NE-12 | | | | |
| NE-13 | | | | |
| NE-14 | | | | |
| NE-15 | | | | |
| NE-16 | | | | |
| NE-17 | | | | |
| NE-18 | | | | |
| NE-19 | | | | |

### Street lights

| ID | 技術品質 (0–3/N/A) | 一貫性 (1–4) | 好み (1–5) | 欠陥・場所・重大度 / メモ |
|---|---:|---:|---:|---|
| ST-01 | | | | |
| ST-02 | | | | |
| ST-03 | | | | |
| ST-04 | | | | |
| ST-05 | | | | |
| ST-06 | | | | |
| ST-07 | | | | |
| ST-08 | | | | |
| ST-09 | | | | |
| ST-10 | | | | |
| ST-11 | | | | |
| ST-12 | | | | |
| ST-13 | | | | |
| ST-14 | | | | |
| ST-15 | | | | |
| ST-16 | | | | |
| ST-17 | | | | |
| ST-18 | | | | |
| ST-19 | | | | |

### Portrait

| ID | 技術品質 (0–3/N/A) | 一貫性 (1–4) | 好み (1–5) | 欠陥・場所・重大度 / メモ |
|---|---:|---:|---:|---|
| PO-01 | | | | |
| PO-02 | | | | |
| PO-03 | | | | |
| PO-04 | | | | |
| PO-05 | | | | |
| PO-06 | | | | |
| PO-07 | | | | |
| PO-08 | | | | |
| PO-09 | | | | |
| PO-10 | | | | |
| PO-11 | | | | |
| PO-12 | | | | |
| PO-13 | | | | |
| PO-14 | | | | |
| PO-15 | | | | |
| PO-16 | | | | |
| PO-17 | | | | |
| PO-18 | | | | |
| PO-19 | | | | |

### Forest

| ID | 技術品質 (0–3/N/A) | 一貫性 (1–4) | 好み (1–5) | 欠陥・場所・重大度 / メモ |
|---|---:|---:|---:|---|
| FO-01 | | | | |
| FO-02 | | | | |
| FO-03 | | | | |
| FO-04 | | | | |
| FO-05 | | | | |
| FO-06 | | | | |
| FO-07 | | | | |
| FO-08 | | | | |
| FO-09 | | | | |
| FO-10 | | | | |
| FO-11 | | | | |
| FO-12 | | | | |
| FO-13 | | | | |
| FO-14 | | | | |
| FO-15 | | | | |
| FO-16 | | | | |
| FO-17 | | | | |
| FO-18 | | | | |
| FO-19 | | | | |

## キー照合後の記録

- 対応表を開いた日時:
- 技術欠陥として gate 審議へ送る項目（候補 ID / Look / 条件 / 根拠）:
- 美的好みとしてのみ記録する項目:
- 判定者間で一致しなかった項目と次の確認:
- 数値 gate の提案（根拠、適用範囲。根拠がなければ「未確定」）:

## ブラインド票の完了後に行う別評価

この節は上の95件をすべて採点し、対応キーを照合した後に扱う。alpha runtime 評価はブラインド画像の好み・品質評点へ混ぜない。

- Runtime 入力: [`alpha-association-chart.png`](../../../assets/alpha-association-chart.png)。色行、固定 alpha 値、ramp、SHA-256 は同じ assets フォルダーの [`alpha-association-chart.json`](../../../assets/alpha-association-chart.json) を参照する。
- 手順: [`EVALUATION_PLAN.md`](../../../EVALUATION_PLAN.md) の「Alpha runtime protocol」に従い、利用可能な Composition / Render Queue 経路と複数背景の出力を個別記録する。
- 実施状態: 未実施。fixture の存在や source-equation probe を runtime 視覚判定とみなさない。
