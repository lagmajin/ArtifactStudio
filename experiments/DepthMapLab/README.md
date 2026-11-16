# Depth Map Lab

**最終更新:** 2026-11-16
**ステータス:** rd0 / rd1 / rd2 実行済み（合成シーン 4 枚 + 動画 1 本 + 実写 1 枚、正規化 4 種 × 後処理 3 種）。rd3（2 本目のモデル比較）は未着手。

AI（単眼深度推定）で深度マップを作るための独立 R&D 環境です。解析的に正しい GT 深度を持つ合成シーンで、学習型モデルがシーン構造をどこまで復元するか、正規化や near/far の向きの扱いがどこで破綻するかを測ります。ここで得た結果は製品実装・既定値・UI の承認ではありません。

`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従い、評価計画を実行前に [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) へ登録済みです。マイルストンは [`docs/planned/MILESTONE_DEPTH_MAP_AI_RD_2026-11-16.md`](../../docs/planned/MILESTONE_DEPTH_MAP_AI_RD_2026-11-16.md)。

製品コード（Artifact / ArtifactCore）は変更せず、Artifact バイナリも起動しません。オフラインの NumPy / OpenCV / onnxruntime（CPU）で完結します。

## 対象と非対象

**対象**

- 学習型単眼深度モデル（ONNX、CPU）: 現在は Depth Anything V2 Small int8（固定 hash）
- classical baseline: 輝度 prior（明るい＝近い）、画面上下 prior（下＝近い）。下流用途でよくある誤用の下限
- 合成シーン 4 種（640×360、seed 固定、GT はカメラ z 深度）
- フレーム単位 / クリップ共通の percentile 正規化、near/far 極性の較正
- 指標: 順序精度、Spearman、scale-shift RMSE、δ1/δ2/δ3、輪郭 F1、境界帯誤差、前景欠落、背景侵入、時間安定性、推論時間

**非対象（未検証として記録）**

- メートル深度の絶対精度（相対深度であり尺度は不定）
- 実写の真値（`--photo` はプレビューと分布のみ）
- Artifact の GPU 経路、Composition Viewer、Render Queue、D3D12 / Vulkan parity
- OCIO / HDR / EXR、linear working space、alpha 付き入力
- DirectML / CUDA、VRAM、モデル配布の運用
- 深度マップを使った製品エフェクトの見た目（本ラボは入力マップの品質まで）

## 実行

Python 3.10 以降。`numpy` / `opencv-python-headless` が必要で、モデルを使う場合のみ `onnxruntime` を追加します（[`requirements.txt`](requirements.txt)）。

```powershell
# rd0: 合成 fixture と classical baseline のみ（モデル不要）
python experiments/DepthMapLab/depth_map_lab.py --rd rd0

# rd1: 固定モデルで比較 + 動画の時間安定性 + 実写プレビュー
python experiments/DepthMapLab/depth_map_lab.py --rd rd1 `
  --download-model `
  --onnx da2s=models/depth-anything-v2-small-int8.onnx `
  --onnx-preset da2s=da2_small `
  --video tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4 `
  --photo experiments/GlowLab/assets/flash-photo-plate.png

# rd2: 正規化 4 種 × 後処理 3 種（推論は 1 回、後処理だけを振る）
python experiments/DepthMapLab/depth_map_lab.py --rd rd2 `
  --onnx da2s=models/depth-anything-v2-small-int8.onnx --onnx-preset da2s=da2_small `
  --normalization p2_98 --normalization p1_99 --normalization p5_95 --normalization median_iqr `
  --refine none --refine guided --refine joint_bilateral `
  --video tests/ArtifactCore/fixtures/walkway-orbit-cc0.mp4
```

`--download-model` は Apache-2.0 の int8 重み（27 MB）を `models/` へ取得し、SHA-256 を検証します。モデルの出所・hash・前処理 preset は [`MODELS.md`](MODELS.md) を参照してください。重みはリポジトリに含めません。

その他のオプション: `--onnx NAME=PATH`（複数可）、`--onnx-preset NAME=PRESET`、`--photo`（複数可、GT なし）、`--video`（複数可）、`--max-frames`、`--save-raw`（float32 `.npy` も保存）。

出力は `captures/<rd>/`:

| パス | 内容 |
|---|---|
| `manifest.json` | 環境、seed、fixture hash、モデル hash / preset / 極性較正、正規化条件 |
| `metrics.json` | 候補 × fixture の全指標と分布、領域別誤差 |
| `gates.json` | 事前登録ゲート、baseline 実測、候補の合否と失敗項目 |
| `fixtures/`, `gt/` | 入力画像と GT near-bright 16-bit PNG |
| `maps/<candidate>/` | near PNG、turbo JPEG、6 倍増幅差分 |
| `review/contact-sheet.png` | 入力 / GT / 各候補 / 差分の一覧 |
| `video/<clip>/` | 時間指標 JSON、contact sheet（`.avi` は git 管理外） |

## ディレクトリ構成

| パス | 内容 |
|---|---|
| `EVALUATION_PLAN.md` | 事前登録済みの評価計画（目的・素材・条件・ゲート） |
| `MODELS.md` | モデルの出所・固定 hash・前処理 preset・極性の扱い |
| `depth_map_lab.py` | fixture 生成、baseline、ONNX 推論、指標、ゲート、contact sheet |
| `models/` | モデル重み（git 管理外） |
| `captures/` | 実行結果（rd0 / rd1 …） |

## rd0 / rd1 の結果

ゲートは合成 4 枚の平均。rd0（classical baseline）と rd1（モデル追加）は同じ fixture・同じ条件。

| 候補 | 順序精度 | Spearman | SI-RMSE | δ1 | 輪郭 F1 | 境界帯 MAE | 前景欠落 | 推論 |
|---|---|---|---|---|---|---|---|---|
| ゲート | ≥ 0.90 | ≥ 0.75 | ≤ 0.35 | ≥ 0.80 | ≥ 0.55 | ≤ 0.12 | ≤ 0.05 | — |
| `baseline_luma` | 0.104 | −0.785 | 0.661 | 0.169 | 0.583 | 0.483 | 0.996 | — |
| `baseline_vertical` | 0.682 | 0.552 | 0.903 | 0.296 | 0.000 | 0.303 | 0.156 | — |
| `da2s`（DA V2 Small int8） | **0.952** | **0.848** | **0.330** | 0.770 | **0.830** | **0.108** | **0.002** | 1.13 s |

`da2s` は δ1（0.770 < 0.80）だけを落とし、他はゲート内。ただし合格扱いにはしていない（下記の未達あり）。

fixture 別（`da2s`）:

| fixture | 順序精度 | SI-RMSE | δ1 | 輪郭 F1 | 境界帯 MAE |
|---|---|---|---|---|---|
| `scene_boxes` | 0.969 | 0.279 | 0.848 | 0.871 | 0.054 |
| `scene_corridor` | 0.955 | 0.222 | 0.988 | 0.604 | 0.060 |
| `scene_occlusion` | 0.937 | 0.532 | 0.787 | 0.925 | 0.183 |
| `scene_prior_trap` | 0.947 | 0.289 | 0.459 | 0.921 | 0.135 |

動画（`walkway-orbit-cc0.mp4`、24 フレーム、クリップ共通正規化）:

| 指標 | 値 |
|---|---|
| flow warp 後 平均 |Δ| | 0.0533（ゲート 0.05 に対し未達） |
| フレーム間 平均 |Δ| | 0.0446 |
| 静止領域 平均 |Δ| | 0.0061 |
| フレーム平均深度の範囲 | 0.081 |
| 推論時間（median） | 1.12 s/frame |

フレーム単位正規化でも flow warp 後 0.0529 でほぼ同じ。このクリップでは正規化の切り替えが flicker の主因ではない。

## rd2 の結果（正規化 × 境界後処理）

ゲートは rd1 と同じ事前登録値で、変更していない。`da2s__p2_98__none` は rd1 の `da2s` と全指標が一致した（リファクタで数値が動いていないことの確認）。

| 候補 | 順序精度 | SI-RMSE | δ1 | 輪郭 F1 | 境界帯 MAE | flow warp Δ |
|---|---|---|---|---|---|---|
| `p2_98` / none（rd1） | 0.952 | 0.330 | 0.770 | **0.830** | 0.108 | 0.0533 |
| `p1_99` / none | 0.952 | 0.330 | **0.859** | 0.833 | 0.112 | 0.0521 |
| `p1_99` / guided | 0.951 | 0.331 | 0.857 | 0.737 | 0.111 | **0.0486** |
| `p1_99` / joint bilateral | 0.952 | 0.330 | 0.859 | 0.750 | 0.110 | 0.0503 |
| `p5_95` / none | 0.951 | 0.330 | 0.757 | 0.826 | 0.106 | 0.0550 |
| `median_iqr` / none | 0.932 | 0.359 | 0.644 | 0.777 | 0.156 | 0.0446 |

`p1_99` 系の 3 候補が静止画の全ゲートを通った。`median_iqr` は時間安定性が最良（0.0446）だが静止画の 3 項目（SI-RMSE / δ1 / 境界帯）で落ちる。

- **δ1 の原因は percentile の切り詰めだった**。`p1_99` で `scene_prior_trap` の δ1 が 0.459 → 0.813 に上がり、平均も 0.770 → 0.859。暗い近景が画面の一部しか占めない構図で、2 / 98 percentile が近景の裾を切っていた。`p5_95` は逆に 0.757 へ悪化し、切り詰めの強さがそのまま δ1 に出る。
- **median-IQR は時間には強いが静止画の尺度が合わない**。順序精度 0.932 と Spearman 0.845 は保つが、SI-RMSE 0.359・δ1 0.644・境界帯 0.156 で落ちる。percentile の外れ値除去が実質的な役割を持っている。
- **境界後処理は静止画では損**。guided は境界帯 MAE をわずかに下げるだけ（`p1_99` で 0.112 → 0.111）で、輪郭 F1 を 0.833 → 0.737 へ落とす。遮蔽境界の `scene_occlusion` も 0.1849 → 0.1845 でほぼ動かず、「guided で遮蔽のにじみが取れる」という仮説は棄却。
- **但し guided は時間安定性をゲート内へ戻す**。`p2_98` 0.0533 → 0.0498、`p1_99` 0.0521 → 0.0486。移動輪郭のフレーム間ノイズを平滑化する効果はある。輪郭定義とのトレードオフとして記録し、静止画用の既定には採らない。
- **採用候補は `p1_99` / 後処理なし**。全ゲート通過かつ輪郭 F1 が最も高い。動画だけを見るなら guided を併用する選択肢が残る。

## 分かったこと

- **輝度 prior は完全に逆相関**（Spearman −0.785、前景欠落 99.6%）。近景を暗く、遠景を明るくした合成シーンでは「明るい＝近い」が全滅する。深度マップを輝度から作る経路は、この条件で使えない。
- **上下 prior は順序 0.68 まで行くが輪郭 F1 は 0**。床と壁の境界しか作れず、物体の前後関係を一切持たない。順序指標だけでは「使える深度」を判定できないことの実例。
- **モデルは順序と輪郭に強い**（順序 0.952、輪郭 F1 0.830、前景欠落 0.2%）。暗い近景・明るい遠景の `scene_prior_trap` でも順序 0.947 を維持し、輝度に依存していない。
- **δ1 が 0.770 で未達**。原因は `scene_prior_trap` の δ1 0.459 に集中している。順序は正しいのに、暗い近景パネルと明るい遠景壁の「近さの絶対量」が percentile 正規化と合わない。近景が画面の一部しか占めない構図では、2/98 percentile が近景側を潰す可能性がある。
- **最悪は遮蔽境界**（`scene_occlusion` の SI-RMSE 0.532、境界帯 MAE 0.183）。輪郭 F1 は 0.925 と高いのに境界帯誤差が大きい。輪郭の位置は合うが、手前の球の縁で深度が滑らかに混ざる（matte 的なにじみ）。
- **時間安定性はゲート直上で未達**（0.0533）。静止領域は 0.0061 と安定しており、残差は移動する輪郭に集中する。フレーム単位とクリップ共通正規化の差は小さく、正規化より推論そのもののフレーム間ノイズが支配的。
- **極性は較正が必要**。今回の int8 export は `higher=near`（Spearman 0.920）だったが、ONNX 深度モデルは inverse depth と depth が混在する。極性を fixture ごとに合わせると「品質」ではなく「出力契約の取り違え」を隠すため、1 回だけ較正して全 fixture に適用している。

## 未実施・制約

- 実写の GT 深度が無いため、実写はプレビューと分布のみ。精度は主張しない
- モデルは 1 本のみ。ベース / ラージ / metric 系との比較は未実施
- 合成シーンにレンズ、被写界深度、ノイズ、大気遠近が無い。実写の失敗モード（髪、ガラス、反射、暗部）は未評価
- 独立した人間の目視レビューは未実施。contact sheet は AI が確認した
- 製品経路（GPU、Viewer、Render Queue）との parity、性能予算は未検証
- 動画は 1 クリップ 24 フレームのみ。カット、露出変化、高速パンは未評価

## 次の候補（未着手）

- rd3: 2 本目のモデル（ベース、metric 系）を `p1_99` / 後処理なしの契約で同じゲートに載せる
- guided の半径・ε を振り、輪郭 F1 を落とさずに時間差分だけを下げられるか確認する
- 実写 2 枚以上で `p1_99` と `p2_98` の見た目差を確認する（GT は無いため数値合否には使わない）
- `ArtifactCore` の `DepthMap` / `DepthMeshGenerator` へ渡す際の契約（near=1、解像度、向き）の確認

## 免責

本ラボの出力は研究用の測定値であり、製品の画質・性能・既定値を保証しません。数値の改善を視覚品質の証明として扱いません。実写素材の出所とライセンスは `experiments/GlowLab/assets/SOURCES.md` の記録に従います。
