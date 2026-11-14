# Detail Sharpening Lab — Research Notes

**最終更新:** 2026-11-14
**実行:** 2026-11-14、Windows、Python 3.14.2 / NumPy 2.4.3 / OpenCV 4.13.0 / Pillow 12.2.0
**評価計画:** [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)（rd1 ゲートは実行前、rd2 追加は rd2 実行前）

本ノートは rd1 / rd2 の結果、欠陥、未検証項目、次の仮説を記録する。数値はすべて合成 fixture 上のオフライン・モデル出力であり、製品品質の合格ではない。

## 1. 出発点（現行実装の確認）

`Artifact/src/Effects/Sharpen/SharpenEffect.cpp` の `SharpenEffectCPUImpl::applyCPU`:

- OpenCV で RGB 3ch を split し、`GaussianBlur(sigma, BORDER_REPLICATE)`。
- `result = color + (color - blurred) * amount` を **チャンネルごと** に適用。
- `threshold > 0` のとき `abs(color - blurred) * amount > threshold` の **ハード二値マスク**で原画へ戻す。
- 負値は 0 にクランプ。上限クランプは CPU に無く、GPU 側は `saturate`。
- `Amount 0..10`、`Sigma 0..10 px`、`Threshold 0..1`。ハロー/オーバーシュート制御、alpha 加重、ハイライト保護は無い。
- 同ファイルには `MagicSharpEffect` もあり、多重スケール係数と edge/shadow protection を持つが、本ラボの比較対象外（未読部分の挙動は未確認）。

この確認から、検証すべき仮説を「per-channel による色フリンジ」「ハードしきい値の不安定さ」「ハロー」「alpha 未加重」「ハイライトクリップ」に置いた。

## 2. rd1 — ハードエッジ基線

素材: `chart_960`（960×540、seed 20261114）。ROI は manifest に固定。設定は amount 1.0 / sigma 1.0、候補は soft threshold 0.02（knee 0.01）。

| model | rise px | 改善 | halo 率 | overshoot | fine energy | frame clip | HL clip | chroma | n_low | n_high |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| mirror | 0.425 | 29.1% | 3.716% | +0.0608 | 3.436 | 0.292% | 4.414% | 6.277 | 2.372 | 1.875 |
| luma_soft | 0.412 | 31.3% | 1.246% | +0.0608 | 2.275 | 0.276% | 4.129% | 0.000 | 1.346 | 1.372 |
| halo_limited | 0.599 | 0.2% | 0.000% | 0.0000 | 1.000 | 0.000% | 0.000% | 0.024 | 1.061 | 1.152 |
| multiscale | 0.596 | 0.7% | 0.000% | 0.0000 | 1.000 | 0.000% | 0.000% | 0.042 | 1.170 | 1.303 |

解釈:

0. **中性設定の恒等性**: 全 4 モデルで `amount=0` の最大絶対差は 0.0（RGB・alpha）。中心条件は満たす。
1. **ハローとエッジ先鋭化は 1px ハードエッジで不可分**。rise 0.60px はすでに画素限界で、USM はオーバーシュートによってのみ 20–80% 間隔を縮める。極値クランプはそのオーバーシュートを消すため、先鋭化も消える。
2. per-channel と luma の差は色エッジで明確（6.28 vs 0.00 code）。色フリンジは演算子の設計で除去できる。
3. 近接ハイライト 0.985 は、しきい値やクランプに関係なく USM で約 4% が新規クリップする。`Amount` を下げるか、ハイライトヘッドルームを別に扱う必要がある。
4. ノイズ: 絶対しきい値 0.02 は低ノイズ側を 2.37× → 1.35× に抑えるが、クランプ候補は応答自体が無いため比較にならない。
5. alpha: 未加重 luma は黒背景合成誤差 26.06 code。ミラーは 15.85 code（透明縁で per-channel の負ディテールが alpha に守られない）。クランプ候補は no-op のため 0.00 code で、alpha 加重の効果を分離できていない。
6. 時間: グラティング 8 フレームで mirror の high-pass 時間 std 比 1.265。クランプ候補は no-op のため 1.000 で、これも分離できていない。
7. 解像度: 1920 で sigma を ×2 にすると rise 改善は mirror 25.2%、luma 28.0%、クランプ 0.2%。960 と同方向で、解像度依存の破綻は見えない（ただしハードエッジのみ）。
8. 性能（960×540、オフライン、3 回中央値）: mirror 16.1ms、luma 21.9ms、halo_limited 33.0ms、multiscale 37.7ms。製品 GPU/CPU の時間ではない。

ゲート: mirror と luma は halo/overshoot/clip/noise で不合格。クランプ 2 種は rise と fine energy で不合格。**rd1 の時点で合格候補は無い。**

## 3. rd2 — ソフトエッジと alpha の分離

### 3.1 動機

- rd1 のクランプ失敗は入力のヘッドルーム不足による可能性が高い。実画像のエッジはレンズ/サンプリングで軟らかい。
- rd1 の alpha 誤差はクランプ no-op と混ざっている。
- rd1 の threshold 0.02 がソフトエッジに与える影響が未知。

### 3.2 素材

`soft_edge`: 960×540、0.03→0.19 のステップを σ=1.2px の Gaussian で遷移させた合成 PSF。ノイズ場は rd1 と同じ σ。`step_edge` ROI は遷移 ±10% 幅。しきい値の影響を分離するため、t=0 と t=0.02 の両方を比較。ゲートは rd1 の値を据え置き。

### 3.3 結果（t=0 はアブレーション、t=0.02 は rd1 プリセット）

| variant | rise px | 改善 | halo | overshoot | alpha 合成誤差 max（黒/灰/白） | ゲート |
|---|---:|---:|---:|---:|---|---|
| mirror_t0 | 1.81 | 15.8% | 1.60% | +0.0084 | 15.85 / 11.04 / 6.16 | 不合格（halo, ovs, alpha） |
| luma_t0 | 1.81 | 15.8% | 1.60% | +0.0084 | 26.06 / 8.51 / 3.65 | 不合格（halo, ovs, alpha） |
| halo_limited_t0 | 1.81 | 15.8% | 0.00% | 0.0000 | 0.00 / 0.00 / 0.00 | **通過** |
| alpha_weighted_limited_t0 | 1.81 | 15.8% | 0.00% | 0.0000 | 0.00 / 0.00 / 0.00 | **通過** |
| alpha_weighted_luma_t0 | 1.81 | 15.8% | 1.60% | +0.0084 | 10.68 / 4.79 / 2.02 | 不合格（halo, ovs, alpha） |
| detail_capped_t0 | 1.87 | 13.2% | 1.60% | +0.0066 | 13.56 / 5.95 / 2.52 | 不合格（rise, halo, ovs, alpha） |
| luma_usm_soft_t02 | 2.15 | 0.0% | 0.00% | 0.0000 | 26.06 / 8.51 / 3.65 | 応答なし |
| halo_limited_t02 | 2.15 | 0.0% | 0.00% | 0.0000 | 0.00 / 0.00 / 0.00 | 応答なし |

`halo_limited_t0` と `alpha_weighted_limited_t0` の出力 hash は同一（soft edge は全画素 alpha=1）。

### 3.4 amount トレードオフ（soft edge、t=0）

| amount | mirror gain / halo / ovs | halo_limited gain / halo / ovs | capped gain / halo / ovs |
|---:|---|---|---|
| 0.25 | 4.4% / 0.53% / +0.0021 | 4.4% / 0.00% / 0.0000 | 4.2% / 0.53% / +0.0020 |
| 0.50 | 8.5% / 0.53% / +0.0042 | 8.5% / 0.00% / 0.0000 | 7.6% / 0.53% / +0.0037 |
| 1.00 | 15.8% / 1.60% / +0.0084 | 15.8% / 0.00% / 0.0000 | 13.2% / 1.60% / +0.0066 |
| 2.00 | 27.7% / 1.60% / +0.0167 | 23.9% / 0.00% / 0.0000 | 21.2% / 1.60% / +0.0109 |

amount 1.0 以下ではクランプはゲインを落とさない。amount 2.0 で初めて 3.8 ポイント差が出る。局所コントラスト比キャップは、この fixture ではゲインを落とす割にオーバーシュートを十分に抑えない。

### 3.5 しきい値

絶対しきい値 0.02 は、ソフトエッジのディテール振幅（最大およそ 0.02〜0.03）と同程度で、smoothstep ゲートがほぼ 0 になる。**しきい値はノイズ相対（例: 局所ディテール分布の分位点）で設計しないと、軟らかいエッジを丸ごと無効化する。** これは rd1 のゲートを満たすための調整ではなく、rd1 で観測された応答の原因分解である。

### 3.6 時間

8 フレーム、振幅 0.02、0.6px/frame のグラティング:

| model | high-pass 時間 std 比 | フレーム差分比 |
|---|---:|---:|
| mirror_t0 | 1.265 | 1.265 |
| luma_t0 | 1.265 | 1.265 |
| halo_limited_t0 | 1.196 | 1.583 |

標準偏差比はゲート（≤1.50）を満たすが、**フレーム差分比 1.583 は線形スケールではない**。クランプは空間近傍に依存するため、動く低振幅テクスチャで時間方向の増幅率が一定にならない。モーション補償付き実写での確認が必要。

## 4. rd3 — 適応しきい値・ロールオフ・luma ガード・CC0 実写

### 4.1 初回の失敗（記録として保持）

事前登録した `σ̂ = 1.4826·median(|luma − Gaussian(luma,1px)|)`（フレーム全体）は、平坦部が大半を占めるプレートで 0 に潰れた。chart と soft edge の大半はノイズが無いため median が 0 付近になり、`k·σ̂ ≈ 3e-6` となって `noise_rel_k1/k2/k3` はしきい値 0 と同じ出力になった（rise 0.291 / 0.158、noise_low 比 2.372 / 1.667 が完全一致）。推定器と素材の欠陥であり、適応しきい値そのものの評価ではない。初回 manifest は `captures/rd3/manifest_first_attempt.json` に保持。

### 4.2 rd3b の修正（EVALUATION_PLAN §11 に実行前登録）

- 推定器をブロック単位ノイズ床に変更: 16×16 ブロックごとの `1.4826·median(|hp − median(hp)|)` の 10 パーセンタイル。
- 全画素に σ=0.004 の決定的ノイズを加えた `chart_960_noisy` を追加（seed 20261114）。
- 実写の luma ハローを保証するため `luma_guard_combo` を追加: ディテールを luma デルタとして局所 luma 極値にクランプし、そのデルタを RGB に適用する。
- per-channel クランプ版は比較対象として残した。

### 4.3 結果

ノイズ床: `chart_960` 1e-6、`chart_960_noisy` 0.002416、neon 0.000301、street light 0.000007。

| plate / model | noise_low 比 | HL 新規クリップ | rise 改善 | 備考 |
|---|---:|---:|---:|---|
| noisy / mirror | 2.133 | 8.87% | 29.2% | 制御 |
| noisy / noise_rel_k1 | 2.018 | 5.42% | 29.2% | |
| noisy / noise_rel_k2 | 1.684 | 5.42% | 29.4% | |
| noisy / noise_rel_k3 | **1.512** | 5.42% | 30.0% | ゲート ≤1.15 は不合格 |
| noisy / halo_noise_rel_k3_rolloff | 1.113 | 0% | 7.0% | |
| noisy / luma_guard_combo | 1.059 | 0% | 5.3% | ノイズは抑制、ハードエッジのゲインも抑制 |
| clean / halo_rolloff | 1.654 | **0%**（4.41→0） | 0.2% | ロールオフはハードエッジのゲインを変えない |

ソフトエッジ（全モデル）: `mirror` 15.8% / halo 1.60% / overshoot +0.0084、`halo_limited_t0`・`halo_rolloff`・`luma_guard_combo` は 15.8% / halo 0 / overshoot 0。

クランプ半径スイープ（soft edge, t=0）: 1 / 2 / 3 / 6 / 12 px のすべてで rise 改善 15.8%、halo 0。半径はゲインの調整軸にならない。

CC0 実写:

| plate | model | 新規クリップ | luma 極値超過 | chroma Δ | flat noise 比 | detail ROI 高域エネルギー |
|---|---|---:|---:|---:|---:|---:|
| neon | mirror | 1.465% | 15.50% | 1.69 | 1.363 | 3.07× |
| neon | per-channel clamp + rolloff | 0% | 3.40% | 0.43 | 1.111 | — |
| neon | `luma_guard_combo` | 0% | **0%** | 0.07 | 1.110 | 1.58× |
| street | mirror | 0.340% | 6.94% | 0.41 | 1.002 | 2.67× |
| street | per-channel clamp + rolloff | 0% | 0.79% | 0.10 | 1.000 | — |
| street | `luma_guard_combo` | 0% | **0%** | 0.03 | 1.001 | 1.34× |

ゲート: N1（noise_rel_k3）はノイズ比 1.512 で不合格、N2（halo_rolloff）通過、R1（luma_guard_combo soft edge）通過、N3（実写 2 枚）両方通過。

性能（960×540、オフライン、3 回中央値）: mirror 15.2ms、`luma_guard_combo` 45.5ms。製品時間ではない。

### 4.4 解釈

1. **ブロック単位ノイズ床は機能する**が、k=3 では平坦ノイズ比 1.512 までしか下がらない。ノイズの USM 振幅は入力 σ の数倍あり、k≈5 相当が必要という仮説は未検証。
2. **ハイライトのロールオフはクリップを完全に消し、ソフトエッジのゲインを落とさない**（0.158 のまま）。予測出力が 0.85 以上の領域だけディテールを絞るため、通常輝度の先鋭化に影響しない。
3. **per-channel クランプは luma ハローを保証しない**。neon で 3.40%、street で 0.79% の luma 極値超過が残る。チャンネルごとの範囲は luma の範囲と一致しない。
4. **luma ガードは実写でも極値超過 0・クリップ 0・色差最小**を達成する。ただし同 amount のミラーに対する高域エネルギーは neon 1.58× vs 3.07×、street 1.34× vs 2.67× で、**ハロー除去はディテール量の約半分と引き換え**になる。
5. クランプ半径 1〜12px でゲインが変わらないのは、ソフトエッジの局所極値幅が半径より広いため。半径を絞ってもゲインは増えない。

## 5. 欠陥ログ

| ID | 内容 | 再現条件 | 重大度 | 証拠 | 状態 |
|---|---|---|---|---|---|
| D1 | per-channel USM の色フリンジ | 色ステップ、amount 1.0 / sigma 1.0 | 中 | rd1 `chroma_delta_mean 6.28 code`、`chart_960.roi-crops.png` | 設計で回避可能（luma 0.00） |
| D2 | ハードしきい値が軟らかいディテールを全停止 | soft edge、threshold 0.02 | 高 | rd2 `t02` 行 rise gain 0.0 | ノイズ相対しきい値の設計課題 |
| D3 | 局所極値クランプがハードエッジの先鋭化を全停止 | 1px step、t=0.02 | 中（入力依存） | rd1 rise gain 0.2% | ソフトエッジでは解消を確認 |
| D4 | 近接ハイライトの新規クリップ | 0.985 ステップ、全 USM | 中 | rd1 HL clip 4.1–4.4% | 未解決。ハイライト保護は未実装 |
| D5 | 未加重ディテールの透明縁合成誤差 | straight alpha、luma USM | 高 | rd1/rd2 26.06 code | alpha 加重で 10.68、クランプ併用で 0.00 |
| D6 | クランプの時間方向非一様増幅 | 移動グラティング | 中 | rd2 frame diff ratio 1.583 | モーション補償付き検証が必要 |
| D7 | 局所コントラスト比キャップの効率不足 | soft edge amount 1.0 | 低 | rd2 capped 13.2% gain / 1.60% halo | この形では不採用候補 |
| D8 | グローバル median ノイズ推定が平坦部で 0 に潰れる | 平坦部が大半のプレート | 高（手法） | rd3 初回: k1/k2/k3 がしきい値 0 と同一、noise_low 比 2.372 不変 | ブロック単位床に置換（rd3b） |
| D9 | per-channel クランプが luma ハローを保証しない | 色差の大きい実写 | 高 | rd3b neon 3.40%、street 0.79% の luma 極値超過 | luma ガードで 0% |
| D10 | k=3 適応しきい値でノイズ比ゲート未達 | noisy chart | 中 | 1.512 > 1.15 | k 掃引が未実施 |

## 6. 未検証（完了主張に使わない）

- Artifact 製品キャプチャ（CPU/GPU）、Composition Viewer、Render Queue、preview/final。
- 実写は CC0 2 枚（夜景・街灯）のみ測定。肌、葉、布、文字、圧縮映像、動画は未検証。
- HDR 値域、表示変換、OCIO、premultiplied alpha 契約。
- モーション補償付き時間安定性、カット、シネマティックな動き。
- アロケーション、GPU メモリ、製品フレーム時間。
- 独立した人間の目視レビュー（接触画像はあるが判定者なし）。

## 7. 次の仮説（rd4 候補）

1. **k の掃引**: k=4〜8 でノイズ比 1.15 以下とエッジゲインの両立点を探す。rd3b で k=3 は不足（1.512）と判明済み。
2. **ゲイン配分**: luma ガードはディテール量がミラーの約半分になる。amount を 1.5〜2.5 に上げたとき、ハローを 0 に保ったまま高域エネルギーを近づけられるかを測る。
3. **エッジ適応ミックス**: 局所勾配が強い領域だけガードを緩め、平坦部とハイライトだけ制限する混合が、ゲインとハローのトレードオフを改善するか。
4. **時間一様性**: クランプ/ガードのフレーム差分比 1.583 の原因を分離し、動く実写でモーション補償付きに測る。
5. **premultiplied 契約**: alpha 加重の暫定措置を、premultiplied 処理と straight 入出力の明示変換に置き換えたときの差を測る。
6. **CPU/GPU 製品経路**: 現行 Sharpen の 3 プロパティで同じ fixture を製品キャプチャし、ミラー一致と GPU `saturate` の差を確認する（ビルド・実行はユーザー指示後）。

## 8. 再現

```powershell
python experiments/DetailSharpeningLab/detail_sharpen_lab.py
python experiments/DetailSharpeningLab/followup_soft_edge.py
python experiments/DetailSharpeningLab/rd3_adaptive.py
```

`captures/rd1/manifest.sha256` と `captures/rd2/manifest.sha256` で manifest の完全性を確認できる。fixture は seed 20261114 で再生成され、外部素材・ネットワーク・製品バイナリに依存しない。
