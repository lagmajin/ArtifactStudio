# 空間デノイズ R&D（静止画・連番）

**最終更新:** 2026-11-15
**ステータス:** In Progress（rd0 実行済み。古典 6 候補 × 3 ノイズモデルで 0/15 合格。ゲート 2 件の不備を検出し承認待ち）
**マイルストン ID:** M-IMG-DENOISE-RD-1
**ラボ:** `experiments/DenoiseLab/`

## 目的

静止画レイヤーおよび連番画像レイヤーに写り込んだ撮像ノイズ（高 ISO、低照度、CG の未収束ノイズ）を**空間フィルタ（単一フレーム）**でどこまで落とせるか、そのときに細部・階調・色・時間安定性をどれだけ壊すかを、既知の clean 画像への再構成誤差とディテール保持で切り分ける。方式選定と後続実装の要否判断の材料を出す。製品実装・既定値・UI・カタログ追加を承認しない。

## なぜ今か

- 製品には `Artifact/src/Effects/Rasterizer/TemporalDenoiseEffect.cpp`（`frameCount` 1〜8 のフレーム履歴型）だけがあり、**1 枚の絵を空間フィルタで整える経路が無い**（grep による確認。実機の描画結果は未確認）。高 ISO の静止画や低照度素材は時間方向の履歴を使えない。
- `M-IMG-1`（Still Image Layer Production Readiness）が静止画の effect 経路を本番受入の対象にしている。空間デノイズはその空白側にある。
- 既存ラボ（Glow / Blur / DoF / DetailSharpening / Film / Glitch / Outline / Retro / Matte / Upscale / ColorWheelGrading / AtmosphericGlowBlur）と重複しない。

## 現行実装の確認済み範囲

- `Artifact/src/Effects/Rasterizer/TemporalDenoiseEffect.cpp` は `strength` / `frameCount` / `varianceThreshold` / `motionAdaptive` を持つフレーム履歴型で、CPU 実装は `TemporalDenoiseCPUImpl`。空間（単一フレーム）のノイズ除去ではない。
- `Artifact/src/Effects/` に denoise 系ディレクトリは無く、`AddNoise`（逆方向）と `TemporalDenoiseEffect.cpp` のみが名前として一致する。
- これらはソース検索による確認であり、実機での画質・速度は未検証である。

## 対象 / 非対象

| 区分 | 内容 |
|---|---|
| 対象 | 合成チャート 1536×1024、CC0 実写 1 枚、3 ノイズモデル（luma/chroma 分離ガウス）、古典 6 候補、連番 5 フレームの時間安定性 |
| 対象外 | 製品 GPU 経路、Composition Viewer、Render Queue、`Temporal Denoise` との統合、D3D12/Vulkan parity、OCIO/HDR/linear working space、学習型デノイズ（DnCNN/SwinIR/BM3D）と DirectML、動画圧縮ノイズ、フィルムグレイン保持、製品の既定値・UI・カタログ |

対象外は「未検証」として記録し、合否の根拠にしない。

## 進め方

| 段階 | 内容 | 状態 |
|---|---|---|
| R0 | fixture 生成、ノイズモデル固定、古典 control の計測、ゲート確定、証跡整備（rd0） | 実行済み（結果は `captures/rd0/`） |
| R1 | ゲート改訂後、適応 `h`・luma/chroma 分離・guided/bilateral 変種の比較、ノイズ量推定の要否判断（rd1） | 未着手（ゲート改訂の承認待ち） |
| R2 | 実写追加・解像度依存・連番の時間安定性・GPU 実装コスト（rd2） | 未着手 |
| R3 | 採用／不採用判断と後続実装の受入条件（rd3、`Temporal Denoise` との関係整理を含む） | 未着手 |

各段階の条件・素材・指標・ゲートは `experiments/DenoiseLab/EVALUATION_PLAN.md` を正とする。

## 合格ゲート（概要）

- 平坦部ノイズ: `flat_dark` / `flat_mid` / `flat_light` の高周波 σ 比の最悪値 ≤ 0.40（入力比）
- 再構成: 全画面 PSNR（clean 基準）が入力比 +2.0 dB 以上
- ディテール: `fine_lines` の線コントラスト保持 ≥ 0.85
- エッジ位置: 50% 交差位置の clean との差 ≤ 0.25 px
- 色: OKLab ΔE 平均の改善（**閾値 0.5 はスケール誤り、改訂待ち**）
- 階調: `ramp` の distinct level 保持 ≥ 0.85、隣接ステップ最大 ≤ 2 code（**この 2 code は control 自身が超過、改訂待ち**）
- 彩度ノイズ: 彩度平坦部の a/b ハイパス σ 比 ≤ 0.60
- 時間: 連番の平坦平均 luma 標準偏差比 ≤ 1.25
- 目視: 人間の独立レビューは未実施。ぼけ・まだら・捏造ディテールが疑われる候補は合格扱いにしない

## rd0 実測結果

`captures/rd0/metrics.json`（全数値）と `gates.json`（判定）。チャートの PSNR（luma、clean 基準）:

| 候補 | fine | strong | lowlight |
|---|---|---|---|
| `baseline_raw` | 33.35 | 24.63 | 18.69 |
| `median3` | 25.89 | 25.29 | 23.43 |
| `gaussian_s1` | 26.28 | 25.86 | 24.63 |
| `bilateral` | 39.42 | 26.77 | 19.02 |
| `nlm_light` | 42.36 | 24.78 | 18.68 |
| `nlm_strong` | 41.08 | 35.32 | 20.50 |

- **判定 0 / 15**（候補 5 × ノイズ 3）。不合格の内訳にゲート側の不備が含まれるため、「古典手法は全滅」とは表現しない。
- **固定 `h` の NLM は暗部でほぼ無効。** `flat_dark`（値 26）の σ 比は h=4 で 0.998、h=10 で 0.994〜0.997。`flat_mid` / `flat_light` は 0.14 / 0.14。単一平坦パッチでも再現（26: 5.80→5.43、128: 5.83→0.95）。`h` が CIELAB L 基準のため、同じノイズ量でも暗部では相対的に弱い。固定 `h` は製品経路へそのまま持ち込めない。
- **1px 細線・毛髪はエッジ保存系でしか残らない。** `median3` / `gaussian_s1` は線コントラストを 26〜29% まで落とす。`bilateral` / `nlm_*` は 0.99〜1.01。
- **必要な強さはノイズ量に依存。** 個別最良は fine `nlm_light`（+9.01 dB）、strong `nlm_strong`（+10.69 dB）、lowlight `gaussian_s1`（+5.94 dB）。lowlight で `bilateral` / `nlm_light` はほぼ no-op（σ 比 0.96 / 1.00）。
- **NLM は時間安定性を悪化させる方向。** 連番の平坦平均 luma 標準偏差は fine で control 0.032 に対し `nlm_strong` 0.091（2.84 倍、ゲート 1.25 倍）。
- **実写の改善幅は小さい。** fine で `nlm_light` +0.21 dB、`bilateral` +3.15 dB。合成チャートの数値だけで実素材の優劣は決められない。
- **性能差は 3 桁。** `median3` / `gaussian_s1` ≈ 1 ms、`bilateral` 6 ms、`nlm_*` 0.23〜0.45 s（1536×1024、CPU）。
- エッジ位置ずれは全候補・全ノイズで ≤ 0.01 px。

### ゲートの不備（提案、未適用）

`gates.json` の `gate_review`（`status: proposed, not approved`）。ゲート値は登録時のまま残し、判定の再現性を保っている。

1. `ramp_max_adjacent_step_codes`（≤ 2 code）は control 自身が 3.6 / 5.8 / 11.3 code で超過。279 個の隣接差分の最大値は残ノイズに支配され、ポスタリゼーションを測れていない。control 比、または median 差分・平坦連続長へ変更する。
2. `delta_e_improvement_min`（≥ 0.5）はスケール誤り。本ラボの OKLab ΔE は control でも 0.017 / 0.052 / 0.109。40% 相対改善などへ再スケールする。

改訂する場合は理由・前後値・承認者・影響する結果を `gates.json` の `gate_revision` に記録し、rd1 の比較前に確定する。

## リスクと未確認事項

- ノイズは加算ガウスで、実際の撮像ノイズ（信号依存・相関・圧縮ノイズ）の分布を代表しない。実素材での正解は未整備。
- 実写 GT は撮影時点で既にノイズを含むため厳密な clean ではない。
- 8-bit sRGB 前提。linear working space / HDR での `h` の意味は未検証。
- GPU 実装コスト、D3D12 / Vulkan parity、`Temporal Denoise` との併用時の挙動は未検証。
- 人間の独立した視覚レビューは未実施（レビューシートは `captures/rd0/review/` に生成済み）。
- 学習型デノイズとの比較は未実施。

## 関連資料

- `experiments/DenoiseLab/README.md`
- `experiments/DenoiseLab/EVALUATION_PLAN.md`
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/planned/MILESTONE_TEMPORAL_EFFECTS_RD_2026-11-12.md`（既存 Temporal Denoise の経路監査）
- `docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`
- `docs/planned/MILESTONES_BACKLOG.md`
