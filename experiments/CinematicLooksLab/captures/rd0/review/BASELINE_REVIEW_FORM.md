# rd0 baseline-only review

**最終更新:** 2026-11-17  
**状態:** 未記入。候補画像は含めず、candidate review前にbaselineの状態と判定条件を定めるための票。

## 目的と手順

この票は入力素材と無処理baselineだけを確認する。Looks候補、blind contact sheet、`blind_review_key.json`は開かない。baselineに元からあるノイズ、圧縮、露出・色偏りを記録し、後で候補由来の変化と混同しないようにする。

各画像は同じ表示アプリ、倍率、色管理条件で確認する。全体の次に下表のbaseline ROIを等倍で確認し、項目ごとに「確認方法」「許容する変化」「重大欠陥」「根拠」を決める。値を決められない場合は未確定のままにする。好みだけで数値gateを作らない。

## baseline画像

| ケース | baseline全体 | baseline ROI |
|---|---|---|
| Chart | [chart baseline](../images/chart__baseline_i0.00.png) | `chart__baseline_i0.00__roi_<gray_ramp/skin_patches/color_patches/fine_lines/step_edges/dark_highlight/smooth_gradient/color_gradient>.png` |
| Neon streets | [neon streets baseline](../images/neon_streets__baseline_i0.00.jpg) | `neon_streets__baseline_i0.00__roi_<intersection_lights/dark_buildings>.png` |
| Street lights | [street lights baseline](../images/street_lights__baseline_i0.00.jpg) | `street_lights__baseline_i0.00__roi_<lamp_and_glare/lower_sign_lights>.png` |
| Portrait | [portrait baseline](../images/portrait__baseline_i0.00.jpg) | `portrait__baseline_i0.00__roi_<face_skin/hair_and_face/bright_window>.png` |
| Forest | [forest baseline](../images/forest__baseline_i0.00.jpg) | `forest__baseline_i0.00__roi_<sunlit_canopy/deep_foliage/fine_branches>.png` |

## レビュー条件

- 判定者:
- 日付 (YYYY-MM-DD):
- OS / 表示装置 / 色モード:
- 表示アプリとversion:
- 表示倍率:
- ICC / display transform:
- メモ:

## baselineの既存特性

| ケース / ROI | 入力にもともとある特性・欠陥 | 確認方法 / 場所 | 候補評価で誤帰属しないための注意 |
|---|---|---|---|
| Chart / gray ramp | | | |
| Chart / skin & color patches | | | |
| Chart / edges, lines & gradients | | | |
| Chart / dark & highlight | | | |
| Neon streets / lights & dark buildings | | | |
| Street lights / glare & sign | | | |
| Portrait / face, hair & window | | | |
| Forest / canopy, foliage & branches | | | |

## candidate review前に固定する基準

| 項目 | 対象ROI / 測定条件 | 合格条件・許容差 | 重大欠陥 | 根拠 / 判定者 |
|---|---|---|---|---|
| 画質・細部 (banding, halo, edge/detail loss) | | | | |
| 色・階調 (gray axis, clipping, hue behavior) | | | | |
| alpha / 複数背景 | | | | |
| source-mirror / C++ / GPU parity | | | | |
| 1080p / 4K 性能・memory | | | | |

## gate確定記録

- candidateの確認を始める前に全基準を固定した日時:
- gate記録先 (`captures/rd0/gates.json`):
- 未確定のまま候補比較へ進む項目と理由:
- このbaseline reviewだけでは判定できない項目:

