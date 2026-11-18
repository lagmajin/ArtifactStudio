# Watercolor Lab

**最終更新:** 2026-11-18
**ステータス:** RD0〜RD7 実行済み・現在保留（fixture・baseline・v1〜v5 の静止画比較・時間安定性・オフライン性能・各段の blind シートを用意。gate 未設定、人手判定・順位付け・製品経路は未実施。2026-11-17 に次の候補 16「鉛筆画・版画風変換」へ移行したため、以降の作業は `experiments/PencilPrintLab/` で行う。2026-11-18 に整列 MAE の参照が blur 限界である旨を監査で確認し、README の解釈の制限へ追記）

水彩画風変換のオフライン R&D ラボ。色のにじみ（bleed）、紙の凹凸（paper）、輪郭の溶け方（edge）を分離して評価するための土台であり、製品アルゴリズムの移植ではない。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)、候補メモは `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md` の 15 番。

製品コード（Artifact / ArtifactCore / ArtifactWidgets）は変更しない。オフラインの NumPy / OpenCV / Pillow のみで完結し、Artifact バイナリは起動しない。

## セットアップ

```powershell
python -m pip install -r requirements.txt
```

## 実行

```powershell
python experiments/WatercolorLab/generate_fixtures.py --output captures/rd0
python experiments/WatercolorLab/generate_fixtures.py --output captures/rd0 --check
python experiments/WatercolorLab/rd0_baseline.py --output captures/rd0
python experiments/WatercolorLab/rd1_candidates.py --rd0 captures/rd0 --output captures/rd1
python experiments/WatercolorLab/rd2_descriptor_fix.py --rd0 captures/rd0 --output captures/rd2
python experiments/WatercolorLab/rd3_v2_blind.py --rd0 captures/rd0 --output captures/rd3
python experiments/WatercolorLab/rd3_v2_blind.py --output captures/rd3 --check
python experiments/WatercolorLab/rd4_v3_comparison.py --rd0 captures/rd0 --output captures/rd4
python experiments/WatercolorLab/rd3_v2_blind.py --output captures/rd4/blind --check --manifest-name manifest_rd4_blind.json
python experiments/WatercolorLab/rd5_temporal_perf.py --rd0 captures/rd0 --output captures/rd5
python experiments/WatercolorLab/rd6_fast_bilateral.py --rd0 captures/rd0 --output captures/rd6
python experiments/WatercolorLab/rd3_v2_blind.py --output captures/rd6/blind --check --manifest-name manifest_rd6_blind.json
python experiments/WatercolorLab/rd7_tune_fast.py --rd0 captures/rd0 --output captures/rd7
python experiments/WatercolorLab/rd7_v5_comparison.py --rd0 captures/rd0 --output captures/rd7
python experiments/WatercolorLab/rd3_v2_blind.py --output captures/rd7/blind --check --manifest-name manifest_rd7_blind.json
```

出力は `captures/rd0/`。

- `fixtures/`: linear float RGBA `.npy` と sRGB 8-bit `.png`。alpha fixture は straight / premultiplied の両方。
- `fixtures_manifest.json`: seed `20261117`、解像度、ROI 座標、各ファイルの SHA-256。
- `baselines/`: 無処理（`baseline_raw`）と2つの伪水彩 control（均一ガウスぼかし、単一ノイズ付加）。
- `metrics.json`: 分布・ROI・差分・色・alpha・時間の記述統計。
- `manifest_rd0.json`: 環境、条件、出力 hash、計測時間、未検証項目。
- `review/`: full / ROI コンタクトシートと空のレビュー票・gate 票。

`captures/rd1/` には6候補（bleed / paper / edge の3成分とアブレーション）の出力、`metrics_rd1.json`、`manifest_rd1.json`、`review/rd1_blind_sheet.png`、`blind_key.json`、空の blind レビュー票を格納する。blind シートの ID と候補名の対応は `blind_key.json` にのみ記録する。

`captures/rd2/` には RD1 モデル（unpremultiply 先行）と v2 モデル（premultiplied 空間処理）の descriptor 比較を格納する。`review/DESCRIPTOR_COMPARISON.md` に v1 / v2 の復元 straight RGB 最大差と、opaque 領域の v1/v2 見た目 parity を記録する。

`captures/rd3/` は v2 モデルによる独立の blind run。`review/rd3_v2_blind_sheet.png`、`blind_key.json`、`review/CANDIDATE_BLIND_FORM.md` を揃え、`--check` で hash と ID 対応を検証できる。RD1 の blind key／シートは v1 の履歴として保持し、v2 の証跡には使わない。`metrics_rd3.json` の `descriptive_findings` には、可視領域は健全だが bleed 成分を含む候補で透明近傍の生 RGB が発散する観測を記録する。PNG preview は alpha=0 でも生 RGB を表示するため、目視では可視領域（alpha > 0.5）のみを判定する。

`captures/rd4/` は v3 モデルの比較 run。`watercolor_reference_v3.py` は bleed も alpha と同じカーネルで処理する（joint bilateral と premul／alpha の同一量子化）。`review/COMPARISON_RD4.md` に v1 / v2 / v3 の descriptor 差・可視領域最大値・近透明最大値・近透明 >10 チャンネル数・不透明 parity を記録する。v3 の blind セットは `captures/rd4/blind/` に別 run として生成し、`--manifest-name manifest_rd4_blind.json` で検証する。RD1 / RD3 の blind セットは履歴として保持する。

`captures/rd5/` は v3 の時間安定性とオフライン性能の run。`review/RD5_TABLES.md` に連番 8 フレームの per-step MAE と joint bilateral の実行時間・tracemalloc peak を記録する。temporal blind シートは候補名とフレーム番号を伏せた `review/rd5_v3_temporal_blind_sheet.png` と `temporal_blind_key.json` で管理する。モデル変更がないため RD4 の candidate blind セットは再生成していない。tracemalloc は Python heap のみで、OpenCV のネイティブ確保や製品性能は含めない。

`captures/rd6/` は高速な共有重み近似 v4 の比較 run。`watercolor_reference_v4.py` は局所エネルギー gate と分離可能 Gaussian で 4 チャンネルに 1 つの正規化重みを掛ける。`review/RD6_TABLES.md` に v3/v4 の descriptor 差・絶対値・不透明 parity・時間安定性・実行時間を記録する。v4 は約 12 倍高速で alpha 契約も維持するが、bilateral を含む候補では不透明部の見た目が v3 と最大 0.598 異なるため drop-in 置換ではない。モデル変更に伴い v4 の blind セットを `captures/rd6/blind/` に再生成し、以前の blind セットは履歴として保持する。

`captures/rd7/` はパラメータ調整済みの v5 比較 run。`rd7_tune_fast.py` が 80 通りのパラメータを v3 出力と比較し、`review/RD7_TUNING_TABLE.md` に結果を記録する。`watercolor_reference_v5.py` の `DEFAULT_PARAMS` は最良値（k=11 / sigma_space=1.0 / sigma_color=0.6）に固定済み。`review/RD7_TABLES.md` に v3/v4/v5 の比較を記録する。v5 は v3 比で不透明差を 0.598→0.484 に低減しつつ約 15 倍高速だが、依然として drop-in 置換ではない。v5 の blind セットは `captures/rd7/blind/` に再生成した。

## 解釈の制限

- 数値は baseline の記述であり、品質の合格を示さない。**閾値は RD0 後に別レビュー者が固定する**（`review/GATE_FORM.md`）。未記入項目は未評価。
- 実水彩を正解とは扱わない。合成 fixture の ground truth は応答の sanity check に限る。
- 時間安定性の連続フレームは 512×320 のみ。解像度比較は静止 fixture で行う。
- **整列 MAE（`integer_alignment_probe`）は blur 限界の値である。** 参照は処理済みの frame 0 出力を bilinear で 0.5 px/frame 平行移動したものであり、参照作成の resample 自体が low-pass するため、測られているのは「参照のボケとの差」であって「時間の追従の残差」ではない。キーの名前は整数整列を示唆するが 0.5×k は整数ではない。追従を blur なしに測るには、参照を補間なしの厳密シフト（整数移動系列）に作り直す必要がある（`docs/analysis/IMAGE_RD_ALIGNMENT_REFERENCE_AUDIT_2026-11-18.md`、 PencilPrintLab RD10 の方式）。
- HDR（1.0 超）、OCIO、製品 preview / final、D3D12 / Vulkan parity、実アプリ経路、製品性能は未検証。

## 既存資産との関係

`OutlineLab`（輪郭）、`RetroImageLab`（減色・ディザ）、`AnisotropicFlowBlur`（構造テンソルの方向場）、GlowLab の CC0 出所記録を再利用し、同じ機能を重複実装しない。
