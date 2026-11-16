# AI 深度マップ推定 R&D — 事前登録済み画像評価計画

**最終更新:** 2026-11-16
**Status:** 候補モデルの実行前に登録。rd0 で classical baseline を測定し、ゲートは本計画の値をそのまま固定する。候補結果を見た後のゲート変更はしない。

`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う。オフライン・決定論の CPU スタディであり、Artifact バイナリを起動しない。製品パリティ、GPU 経路、既定値の採用を証明しない。

## 1. 目的と対象

**目的:** 静止画 1 枚から単眼深度マップを作る経路について、(a) 学習型モデルの出力がシーン構造をどの程度正しく復元するか、(b) どの正規化・向きの扱いが下流用途（DoF の焦点面、視差変位、フォグ、フラッシュ距離減衰）で破綻するかを、既知深度の合成シーンで切り分ける。単に「それらしいグレースケール画像」が出ることは合格根拠にしない。

**対象**

- 学習型単眼深度モデル（ONNX、CPU 推論）。rd0 は classical baseline のみ、rd1 以降で Depth Anything V2 Small int8 を実行する
- classical baseline（学習なし）: 輝度 prior（明るい＝近い）、画面上下 prior（下＝近い）。下流用途でよくある誤用の下限を示すための control
- 合成シーン（解析的に正確な z-depth が既知、seed 固定）と、GT を持たない CC0 実写のプレビュー
- 正規化: フレーム単位 percentile 正規化、動画のクリップ共通 percentile 正規化
- 指標: 順序精度、Spearman、scale-shift 整合 RMSE、δ1/δ2/δ3、輪郭 F1、境界帯誤差、前景欠落・背景侵入、時間安定性、推論時間

**対象外（未検証として記録）**

- メートル深度の絶対精度（モデルは相対深度であり、尺度は不定）
- 実写の真値（GT 深度が無いため、実写はプレビューと分布のみ）
- Artifact の GPU 経路、Composition Viewer、Render Queue、D3D12 / Vulkan parity
- OCIO / HDR / EXR、linear working space、alpha 付き入力
- DirectML / CUDA 推論、VRAM、モデル配布の運用
- 深度マップを使った製品エフェクトの見た目（本ラボは入力マップの品質まで）

## 2. 比較対象と役割

| 項目 | 役割 | 正解として使えるか |
|---|---|---|
| 合成シーンの解析 GT z-depth | 正解 | Yes（レンダリングした幾何のみ。実写の真値ではない） |
| `baseline_luma` | 明るい画素を近いとする control | No |
| `baseline_vertical` | 画面下を近いとする control | No |
| 学習型モデル（rd1） | 候補 | No |

baseline は「学習なしでどこまで行けてしまうか」を示す下限であり、合格対象ではない。学習型候補が baseline を上回ること自体を合格根拠にしない（ゲートは絶対値で先に固定する）。

## 3. 評価素材

`depth_map_lab.py` が `seed=20261116` で決定論生成する。各 fixture は `image`（8-bit sRGB 入力）、`depth_z`（float32、カメラ z 深度）、`disparity = 1/depth_z` を保持し、SHA-256 を manifest に記録する。

| fixture | 内容 | 主に落とす手法 |
|---|---|---|
| `scene_boxes` | 床面、暗い近景ボックス、中景ボックス、球、明るい遠景壁 | 輝度 prior（近景が暗い） |
| `scene_corridor` | 床面、左右に並ぶ柱、奥の壁 | 上下 prior、遠近の取り違え |
| `scene_occlusion` | 近景球・細い棒が中景ボックスと遠景壁を隠す | 輪郭の甘いモデル、境界帯の誤差 |
| `scene_prior_trap` | 暗い近景パネルと明るい遠景壁、中景ボックス | 輝度 prior の完全な失敗 |

合成シーンは実写のレンズ、被写界深度、センサーノイズ、大気遠近を含まない。テクスチャは深度と無相関になるよう手続き生成し、輝度だけで深度を当てられないようにしてある。実写は `experiments/GlowLab/assets/` の CC0 素材のみを `--photo` でプレビューに使う（出所と hash は `experiments/GlowLab/assets/SOURCES.md` に従う）。

## 4. 固定画像条件

- 入力: 8-bit sRGB、640×360、alpha なし。合成シーンのカメラは y=1.6、FOV 52°、注視方向 -Z
- GT: float32 のカメラ z-depth。比較は `1/depth` の相対深度（disparity）で行う
- 正規化: 予測・GT とも 2 / 98 percentile を 0 / 1 に写す（near = 1）。動画はフレーム単位とクリップ共通の両方を比較する
- 向き: モデル出力の near/far 極性は `scene_boxes` の GT との Spearman 符号で 1 回だけ較正し、その結果を全 fixture に適用する。fixture ごとの向き調整はしない
- モデル入力: preset ごとの短辺サイズ、multiple、mean / std を manifest に記録する。RGB、CPU 単一セッション
- 乱数: fixture 生成とペア抽出のみ `seed=20261116`。候補処理に乱数は入れない
- 出力: near-bright PNG、turbo プレビュー、増幅差分、contact sheet、metrics / gates / manifest JSON

## 5. 必須評価項目と事前登録ゲート

| 項目 | 領域・手順 | ゲート |
|---|---|---|
| 順序精度 | 4px 間引き画素から固定 seed で 40,000 ペア。GT 差 > 0.03 のペアのみ | ≥ 0.90 |
| Spearman | 4px 間引き画素の順位相関 | ≥ 0.75 |
| scale-shift RMSE | 予測に最小二乗で scale+shift を適合し、GT 標準偏差で正規化 | ≤ 0.35 |
| δ1 | closeness（near=1）に +0.1 して深さ比へ変換、median 比で尺度を合わせ `max(r,1/r) < 1.25` | ≥ 0.80 |
| 輪郭 F1 | GT / 予測それぞれ Sobel 上位 4% を輪郭とし、2px 許容で precision / recall | ≥ 0.55 |
| 境界帯 MAE | GT 輪郭の 2px 膨張帯での |予測 − GT| | ≤ 0.12 |
| 前景欠落 | GT > 0.75 かつ予測 < 0.25 の画素率 | ≤ 0.05 |
| 背景侵入 | GT < 0.25 かつ予測 > 0.75 の画素率 | ≤ 0.05 |
| 時間安定性 | 連続フレームの flow warp 後 average |Δ|（動画実行時のみ） | ≤ 0.05 |
| 性能 | 1 枚あたり推論秒（単発、記録のみ） | ゲートなし |

ゲートはいずれも合成シーン 4 枚の平均で判定する。1 項目でも未測定のまま候補の合格を主張しない。`gates.json` に baseline 実測と候補の合否、失敗した項目を残す。閾値を変更する場合は、変更理由・前後・承認者・影響する結果を記録し、比較をやり直す。

## 6. 記録する画像統計

- 予測・GT の min / max / mean / p5 / p50 / p95、0 / 1 に張り付いた画素率、非有限値数
- 順序精度、Spearman、scale-shift RMSE、δ1 / δ2 / δ3、median absolute relative error
- 輪郭 F1（precision / recall）、境界帯 MAE と符号付き bias
- 前景欠落、背景侵入
- 近景 / 中景 / 遠景の領域別 MAE
- 動画: フレーム単位・クリップ共通正規化の時間差分、flow warp 後差分、静止領域差分、フレーム平均深度の変動
- 推論時間、入力解像度、モデル入力サイズ

全画面平均と領域別を併記し、平均だけで局所破綻を隠さない。

## 7. 実行条件と証跡

- Python 3.14.2、NumPy 2.4.3、OpenCV 4.13.0、onnxruntime 1.31.1（CPU）
- 実行: `python experiments/DepthMapLab/depth_map_lab.py --rd rd0`
- 成果物: `captures/<rd>/` の `manifest.json`、`metrics.json`、`gates.json`、`fixtures/`、`gt/`、`maps/`、`review/contact-sheet.png`
- モデル重みはリポジトリに含めない（`models/` は git 管理外）。入手方法と固定 hash は `MODELS.md`
- 判定者: 実行主体は AI エージェント。独立した人間の目視レビューは、実施した場合のみその旨を記録する

## 8. 比較と判定

- 入力、GT、各候補の near-bright マップ、増幅差分を同じ表示条件で並べて確認する
- 数値は順序精度と輪郭 F1 を主、scale-shift RMSE と δ1 を従として読む。平均値の改善だけで視覚品質を断定しない
- 失敗は場所（近景 / 遠景 / 輪郭）、再現条件、影響、重大度とともに記録する
- 動画のフレーム単位正規化は、フレーム間でスケールが動くため時間差分が過大に出る。クリップ共通正規化との差を flicker の証拠として扱う

## 9. 完了条件

rd0（合成 fixture と classical baseline、ゲート確定）、rd1（学習モデル 1 本の比較）、rd2（動画の時間安定性）、rd3（採用 / 不採用と後続実装の受入条件）の結果を残す。未達・未検証はそのまま明示し、製品統合完了とは表現しない。
