# Pencil Print Lab

**最終更新:** 2026-11-18
**ステータス:** RD0〜RD10 実行済み（fixture 9 件・baseline 24 出力・v1〜v6 と dither sweep・時間分析・content-lock・サブピクセル位相・整数シフト参照・解析的連続閾値場（IGN / R2）の比較まで実施。各段の blind シートを用意。gate 未設定、人手判定・順位付け・製品経路は未実施）

鉛筆画・版画風変換のオフライン R&D ラボ。線（line）・濃淡（tone）・紙／ハッチング（texture）を分離して評価する。製品アルゴリズムの移植ではない。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)、候補メモは `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md` の 16 番。

製品コード（Artifact / ArtifactCore / ArtifactWidgets）は変更しない。オフラインの NumPy / OpenCV / Pillow のみで完結し、Artifact バイナリは起動しない。

## セットアップ

```powershell
python -m pip install -r requirements.txt
```

## 実行

```powershell
python experiments/PencilPrintLab/generate_fixtures.py --output captures/rd0
python experiments/PencilPrintLab/generate_fixtures.py --output captures/rd0 --check
python experiments/PencilPrintLab/rd0_baseline.py --output captures/rd0
python experiments/PencilPrintLab/rd1_candidates.py --rd0 captures/rd0 --output captures/rd1
python experiments/PencilPrintLab/rd2_descriptor_safe.py --rd0 captures/rd0 --output captures/rd2
python experiments/PencilPrintLab/rd3_hdr_preserving.py --rd0 captures/rd0 --output captures/rd3
python experiments/PencilPrintLab/rd4_multi_dither.py --rd0 captures/rd0 --output captures/rd4
python experiments/PencilPrintLab/rd5_dither_sweep.py --rd0 captures/rd0 --output captures/rd5
python experiments/PencilPrintLab/rd6_temporal_deep.py --rd0 captures/rd0 --output captures/rd6
python experiments/PencilPrintLab/rd7_content_locked.py --rd0 captures/rd0 --output captures/rd7
python experiments/PencilPrintLab/rd8_subpixel_phase.py --rd0 captures/rd0 --output captures/rd8
python experiments/PencilPrintLab/rd9_integer_lock.py --rd0 captures/rd0 --output captures/rd9
python experiments/PencilPrintLab/rd10_continuous_threshold.py --rd0 captures/rd0 --output captures/rd10
```

出力は `captures/rd0/`（fixture・baseline・統計・空の gate 票）、`captures/rd1/`（v1 の6候補出力・統計・blind シート）、`captures/rd2/`（descriptor-safe v2 との比較・v2 blind シート）。候補は `line` / `tone` / `texture` の3成分とアブレーション。

`captures/rd2/` の `review/RD2_TABLES.md` に、v1 と v2 の descriptor 差・可視／近透明の絶対値・不透明 parity・時間安定性・実行時間を記録する。v2 は段階化を直色側で行い再 premultiply するため、近透明の 1.0 超えを解消する一方、不透明部の見た目が v1 と大きく異なる（特に HDR スポットを 1.0 に丸める）。RD1 の blind セットは v1 の履歴として保持し、v2 の証跡には使わない。

`captures/rd3/` の `review/RD3_TABLES.md` に、v1/v2/v3 の比較を記録する。v3 は量子化上限をデータから求め、各チャンネルを `min(q, max(1, 元の直色))` に制限するため、HDR の 1 超えを保ちつつ近透明の >1 を 0 に維持する。flat_highlight の v1 差は v2 1.47 に対し v3 0.32 に低減した。pp_b（Bayer 2 値化）は v1 との差が 1.0 のまま。RD1 / RD2 の blind セットは履歴として保持し、v3 の blind セットは `captures/rd3/` に生成する。

`captures/rd4/` の `review/RD4_TABLES.md` に、binary Bayer（v3）と多値 Bayer（v4）の比較を記録する。v4 の `bayer_multi` は Bayer 行列を丸め閾値として 4 / 8 段階に量子化する。binary の不透明 v3 差 1.0 に対し multi4 は 0.667、multi8 は 0.857 で、段階数を増やしても単純に差が減るわけではない。近透明 >1 と非有限値は v4 も 0 を維持。RD1〜RD3 の blind セットは履歴として保持し、v4 の blind セットは `captures/rd4/` に生成する。

`captures/rd5/` の `review/RD5_TABLES.md` に、dither 段階 3 / 4 / 5 / 6 / 8 と binary の sweep を記録する。binary と multi3 は可視／近透明の最大値 1.0、それ以外は 0.667〜0.8 で、全候補で近透明 >1 と非有限値は 0。binary からの不透明差は max で 0.667〜1.232、flat_highlight では multi5 が 0.55 と最も小さく、段階数と差は単調ではない。時間・性能も 3 run で記録したが、品質 gate は未設定で順位付けは行わない。sweep の blind セットは `captures/rd5/` に生成した。

`captures/rd6/` の `review/RD6_TABLES.md` に、整数位置整列 MAE、シーク相当 MAE、flicker proxy、フレーム単位の実行時間を記録する。per-step global MAE は候補間で同程度だが、整数位置へ再整列した MAE は binary 0.070〜0.449、multi4 0.043〜0.254、multi5 0.031〜0.107 と候補で異なる。これは dither パターンの画面固定性の差を示唆するが、知覚品質の判定ではない。temporal blind セットは `captures/rd6/` に生成した。

`captures/rd7/` の `review/RD7_TABLES.md` に、content-locked dither（v5）と screen-locked（v4）の比較を記録する。content-locked は既知の 0.5*k px オフセットで閾値をサンプルする制御実験であり、モーション推定ではない。偶数フレームの整列 MAE は 0.253→0.008 へ大幅低減したが、奇数フレームは 0.13 前後に留まる。これは 0.5px のサブピクセル offset が整数座標へ切り捨てられるためで、サブピクセル対応の閾値補間が課題。temporal blind セットは `captures/rd7/` に生成した。

`captures/rd8/` の `review/RD8_TABLES.md` に、隣接する Bayer 位相を補間する v6 の比較を記録する。奇数フレームの整列 MAE は nearest 0.127〜0.134 に対し interp 0.1266〜0.1267 で、わずかな改善に留まった。偶数フレームは nearest の方が小さい（0.005〜0.015 対 0.008〜0.021）。画面固定の control は従来どおり 0.25 程度の残差。補間による位相の連続化だけでは整列残差は十分に消えず、知覚影響は未判定。temporal blind セットは `captures/rd8/` に生成した。

`captures/rd9/` の `review/RD9_TABLES.md` に、整数シフト参照を記録する。frame 0 を整数 k px だけ `INTER_NEAREST` で平行移動した画像を参照とし、content-lock 側にも同じ k を与えることで、warp のボケを除いた dither 追従性を測る。整列 MAE は screen / content_nearest / content_interp がいずれも 0.149 前後で差がなく、RD6 の content-lock で見えた偶数フレームの改善は再現しなかった。一方 per-step MAE は screen 0.0024 に対し content-lock 0.254 と大きく、画像の動きに dither パターンが追従する分がフレーム間差として現れる。temporal blind セットは `captures/rd9/` に生成した。

**RD9 の測定方法的欠陥（RD10 で是正）:** RD9 の参照は効果適用前の生入力のシフトであり、frame 0 の出力のシフトではない。そのため測っていたのは dither の追従ではなく効果量そのもので、content-lock と screen-lock がほぼ同一に見えていた。RD10 では参照を frame 0 の出力の厳密整数シフト（replicate 帯を除外）に変更した。

`captures/rd10/` の `review/RD10_TABLES.md` に、解析的な連続閾値場（IGN / R2）と Bayer の比較を記録する。RD10 は参照可能な「参照＝frame 0 出力の移動」を導入し、2 つの系列（1 px/frame の整数系列＝補間なしで参照を作れるため blur なし、0.5 px/frame の既存系列＝参照の resample 自体が dither を low-pass するため blur 限界）で測る。整数系列の lock-aligned MAE の内側平均は、dither 単独の content-lock で bayer / ign / r2 いずれも 0.00000000（厳密一致）、screen-lock は 0.125〜0.148 だった。full 系の content-lock 内側平均 0.0175〜0.0227 は screen 固定の hatch texture 由来の混在である。0.5 px オフセットでの閾値コントラスト比は Bayer 補間 0.4728（dither 強度が半減）、IGN / R2 1.0000。画面上の per-step MAE は screen 固定 0.0011 に対し content 固定 0.10〜0.23 で、「画面固定 vs 内容固定」のトレードオフとして未判定。descriptor 差 5.96e-08・近透明 >1 は 0・非有限値 0。temporal blind セットは `captures/rd10/` に生成した。

## 既存資産との関係

- `experiments/OutlineLab/`: 輪郭抽出・線幅校正・時間安定性
- `experiments/RetroImageLab/`: 減色・ディザ・位相 study
- `ArtifactCore`: `Halftone`、`Posterize`、`Threshold`
- `experiments/WatercolorLab/`: premultiplied 空間処理と descriptor 契約の知見

## 解釈の制限

- 評価計画に沿って gate を固定するまで、数値は記述的な baseline に留まる
- 実作品を正解とは扱わない。合成 fixture の ground truth は応答の sanity check に限る
- HDR、OCIO、製品 preview / final、D3D12 / Vulkan parity、実アプリ経路、製品性能は未検証
