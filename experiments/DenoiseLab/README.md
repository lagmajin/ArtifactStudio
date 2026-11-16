# Denoise Lab

**最終更新:** 2026-11-15
**ステータス:** rd0 実行済み（古典 6 候補 × 3 ノイズモデル。ゲート 2 件に不備を検出、承認待ち）

静止画・連番画像レイヤーに写り込んだ撮像ノイズを空間フィルタ（単一フレーム）でどこまで落とせるか、そのときに何を壊すかを研究する Python ラボです。既知の clean 画像に既知のノイズを注入し、「どれだけノイズが減ったか」と「どれだけ細部が残ったか」を同時に測ります。ここで得た結果は製品実装・既定値・UI の承認ではありません。

製品コード（Artifact / ArtifactCore / ArtifactWidgets）は変更しません。オフラインの NumPy / OpenCV のみで完結し、Artifact バイナリは起動しません。

評価計画は実行前に [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) へ登録済みです。`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従います。マイルストンは `docs/planned/MILESTONE_SPATIAL_DENOISE_RD_2026-11-15.md`。

## なぜ空間（単一フレーム）デノイズか

- 製品には `Artifact/src/Effects/Rasterizer/TemporalDenoiseEffect.cpp`（`frameCount` 1〜8 のフレーム履歴型）だけがあり、**1 枚の絵を空間フィルタで整える経路は無い**（grep による確認）。高 ISO の静止画、低照度素材、CG の未収束ノイズは時間方向の履歴を使えない。
- `M-IMG-1`（Still Image Layer Production Readiness）が本番経路を閉じる対象に静止画の色解釈・effect を挙げており、空間デノイズはその空白側にある。
- 既存ラボ（Glow / Blur / DoF / Sharpen / Film / Glitch / Outline / Retro / Matte / Upscale / Grading / Atmospheric）と重複しない。

## 対象と非対象

**対象**

- 合成チャート（決定論生成、1536×1024）と CC0 実写 1 枚
- 3 ノイズモデル（`fine` lumaσ5/chromaσ3、`strong` 12/12、`lowlight` 20/30）
- 古典 6 候補（`baseline_raw` / `median3` / `gaussian_s1` / `bilateral` / `nlm_light` / `nlm_strong`）
- 連番 5 フレーム（0.5px 移動）での時間安定性

**非対象（未検証として記録）**

- Artifact の GPU 経路、Composition Viewer、Render Queue、preview / final
- 既存 `Temporal Denoise` との統合、`ArtifactEffectFrameSampler` 経路
- D3D12 / Vulkan parity、OCIO、HDR、linear working space
- 学習型デノイズ（DnCNN / SwinIR / BM3D）、DirectML 推論
- 動画圧縮ノイズ、意図的なフィルムグレインの保持、scanner noise
- 製品の既定値・プロパティ範囲・UI・エフェクトカタログ

## 実行

Python 3.10 以降。`numpy` / `opencv-python-headless` が必要です。

```powershell
python experiments/DenoiseLab/denoise_lab.py --rd rd0
```

出力は `captures/rd0/`（`manifest.json`, `metrics.json`, `gates.json`, `fixtures/`, `review/`）。fixture とノイズは seed `20261115` 系で決定論生成します。

## ディレクトリ構成

| パス | 内容 |
|---|---|
| `EVALUATION_PLAN.md` | 事前登録済みの評価計画（目的・素材・条件・ゲート・証跡） |
| `denoise_lab.py` | fixture 生成、ノイズ注入、古典候補、指標測定、ゲート判定、manifest 出力 |
| `captures/` | 実行結果（rd0 …） |

## rd0 の結果（2026-11-15 実行）

全数値は `captures/rd0/metrics.json`、ゲートと判定は `gates.json`。チャート（1536×1024）の全画面 PSNR（luma、clean 基準）と、破壊の指標:

| 候補 | fine PSNR | strong PSNR | lowlight PSNR | 平坦σ比 worst（fine/strong/low） | 細線保持（fine/strong/low） | 彩度ノイズ比（fine/strong/low） | ms |
|---|---|---|---|---|---|---|---|
| `baseline_raw` | 33.35 | 24.63 | 18.69 | 1.000 / 1.000 / 1.000 | 1.003 / 1.008 / 0.989 | 1.000 / 1.000 / 1.000 | 0.7 |
| `median3` | 25.89 | 25.29 | 23.43 | 0.409 / 0.376 / 0.426 | 0.260 / 0.264 / 0.267 | 0.520 / 0.425 / 0.403 | 1.2 |
| `gaussian_s1` | 26.28 | 25.86 | 24.63 | 0.734 / 0.346 / 0.263 | 0.286 / 0.285 / 0.273 | 0.623 / 0.261 / 0.232 | 1.1 |
| `bilateral` | 39.42 | 26.77 | 19.02 | 0.312 / 0.741 / 0.961 | 1.000 / 1.008 / 0.986 | 0.164 / 0.494 / 0.932 | 6.1 |
| `nlm_light` | 42.36 | 24.78 | 18.68 | 0.998 / 0.999 / 1.002 | 1.005 / 1.007 / 0.989 | 0.210 / 0.424 / 1.000 | 0.23 s |
| `nlm_strong` | 41.08 | 35.32 | 20.50 | 0.167 / 0.994 / 0.997 | 0.994 / 0.999 / 0.991 | 0.559 / 0.156 / 0.358 | 0.45 s |

エッジ位置ずれは全候補・全ノイズで ≤ 0.01 px。実写（neon streets）の PSNR は fine 33.67 → bilateral 36.82 / nlm_light 33.88、strong 25.51 → median3 28.69 / nlm_strong 26.31、lowlight 20.00 → median3 25.45 / gaussian_s1 25.21。

**判定: 0 / 15 合格（候補 5 × ノイズ 3、`baseline_raw` は control）。** ただし不合格の内訳にゲート側の不備が含まれる（下記）。

### 分かったこと

- **固定 `h` の NLM は暗部でほぼ無効。** `flat_dark`（値 26）の σ 比は h=4 で 0.998、h=10 でも 0.994〜0.997 に対し、`flat_mid` / `flat_light` は 0.14 / 0.14。単一の平坦パッチでも再現（26: 5.80→5.43、128: 5.83→0.95）。`fastNlMeansDenoisingColored` の `h` が CIELAB L 上で定義されるため、同じノイズ量でも暗部では相対的に弱くなる。**画素値に依らない固定 h は製品経路にそのまま持ち込めない**（この結論は 8-bit sRGB 前提・CPU 実測）。
- **1px 細線と毛髪はエッジ保存系でしか残らない。** `median3` / `gaussian_s1` は線コントラストを 26〜29% まで落とす。`bilateral` / `nlm_*` は 0.99〜1.01 を維持。
- **必要な強さはノイズ量に依存する。** 個別最良は fine で `nlm_light`（+9.01 dB）、strong で `nlm_strong`（+10.69 dB）、lowlight で `gaussian_s1`（+5.94 dB）。lowlight では `bilateral` / `nlm_light` がほぼ no-op（σ 比 0.96 / 1.00）で、パラメータ自動決定が無いと破綻する。
- **NLM は時間安定性を悪化させる方向。** 連番の平坦平均 luma 標準偏差は fine で `baseline_raw` 0.032 に対し `nlm_strong` 0.091（2.84 倍、ゲート 1.25 倍）。空間フィルタ単体ではフリッカーを増やしうる。
- **実写での改善幅は合成チャートより小さい。** fine で `nlm_light` は +0.21 dB、`bilateral` は +3.15 dB。合成チャートの数値だけでは実素材の優劣を決められない。
- **性能差は 3 桁。** `bilateral` 6 ms、`median3`/`gaussian_s1` 約 1 ms、`nlm_*` 0.23〜0.45 s（1536×1024、CPU）。ゲートは無いが、プレビュー用途では NLM 系のコストが問題になる。

### ゲートの不備（承認待ち、変更は未適用）

ゲートは登録時のまま残し、判定の再現性を保っています。`gates.json` の `gate_review` に記録:

1. `ramp_max_adjacent_step_codes`（≤ 2 code）は **control 自身が超過**（3.6 / 5.8 / 11.3 code）。279 個の隣接差分の最大値なので残ノイズに支配され、ポスタリゼーションを測れていない。control 比、または median 差分・平坦連続長へ変更する提案。
2. `delta_e_improvement_min`（≥ 0.5）は **スケール誤り**。本ラボの OKLab ΔE は control でも 0.017 / 0.052 / 0.109 しかなく、どの候補も到達できない。40% 相対改善などの再スケール提案。

## 次の段階（未着手）

- rd1: ゲート改訂の承認後、適応 `h`・luma/chroma 分離・guided/bilateral 変種を比較し、ノイズ量推定の要否を判断する
- rd2: 実写追加、解像度依存、連番の時間安定性、GPU 実装コスト
- rd3: 採用／不採用と、製品実装する場合の受入条件（`Temporal Denoise` との関係整理を含む）

## 免責

本ラボの出力は研究用の測定値であり、製品の画質・性能・既定値を保証しません。実写素材の出所とライセンスは `experiments/GlowLab/assets/SOURCES.md` の記録に従います。数値の改善を視覚品質の証明として扱いません。人間による独立した目視レビューは未実施です。
