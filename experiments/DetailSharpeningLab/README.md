# Detail Sharpening Lab

**最終更新:** 2026-11-14
**ステータス:** rd1 / rd2 / rd3b 実行済み（ソフトエッジとCC0実写で luma ガード候補が事前登録ゲートを通過、適応しきい値は部分達成）

ArtifactStudio 本体から独立した、ディテール強調（unsharp / local contrast）の研究用 Python ラボです。輪郭の先鋭化と、ハロー・オーバーシュート・色フリンジ・ノイズ増幅・クリップ・時間ちらつきのトレードオフを、再現可能な fixture と数値で切り分けます。ここで得た結果は製品実装や既定値の承認ではありません。

評価計画は実行前に [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) へ登録済みです（rd1 のゲート、rd2 の追加素材、rd3 と、初回失敗を受けた rd3b の修正理由を含む）。`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従います。

## 対象と非対象

**対象**

- 現行 `Artifact/src/Effects/Sharpen/SharpenEffect.cpp` の CPU 式を写した `product_usm_mirror`（製品キャプチャではない）
- luma ディテール + ソフトしきい値、ブロック単位ノイズ床による適応しきい値
- 局所極値クランプ（per-channel / luma delta）付きディテール、ハイライトのロールオフ
- 細部 + 粗い clarity の多重スケール版、局所コントラスト比キャップ
- CC0 実写 2 枚（neon streets / street light）

**非対象（未検証として記録）**

- Artifact GPU / Composition Viewer / Render Queue / preview-final 経路
- D3D12 / Vulkan パリティ、HDR 表示変換、OCIO
- 動画、モーションブラー、グレイン、圧縮映像
- 製品の既定値・プロパティ範囲・UI・エフェクトカタログ
- 製品ホットパスのアロケーション（本ラボはオフライン NumPy/OpenCV のみ）

## 実行

Python 3.10 以降。`numpy` / `opencv-python-headless` / `pillow` が必要です（`requirements.txt`）。リポジトリルートから順に実行します。

```powershell
python experiments/DetailSharpeningLab/detail_sharpen_lab.py
python experiments/DetailSharpeningLab/followup_soft_edge.py
python experiments/DetailSharpeningLab/rd3_adaptive.py
```

出力は `captures/rd1/`, `captures/rd2/`, `captures/rd3/`。fixture は seed `20261114` で決定的に生成し、実写は `experiments/GlowLab/assets/SOURCES.md` 記録の CC0 素材のみを使います。

## 主な結果

### rd1: 1px ハードエッジ（960×540、amount 1.0 / sigma 1.0）

| model | rise | 改善 | halo 率 | overshoot | fine energy | 新規クリップ | HL クリップ | chroma Δ | noise(low) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `product_usm_mirror` | 0.425 px | 29.1% | 3.72% | +0.0608 | 3.44× | 0.29% | 4.41% | 6.28 | 2.37× |
| `luma_usm_soft` | 0.412 px | 31.3% | 1.25% | +0.0608 | 2.28× | 0.28% | 4.13% | 0.00 | 1.35× |
| `halo_limited` | 0.599 px | 0.2% | 0.00% | 0.0000 | 1.00× | 0% | 0% | 0.02 | 1.06× |
| `multiscale_limited` | 0.596 px | 0.7% | 0.00% | 0.0000 | 1.00× | 0% | 0% | 0.04 | 1.17× |

- 中性 `amount=0` は全モデル最大差 0.0。per-channel USM は色エッジに 6.28 code のフリンジ、luma は 0.00。
- 局所極値クランプはハローを消すが、**1px ハードエッジでは先鋭化も 0.2%** まで消える。画素限界のエッジにはヘッドルームがない。
- 近接ハイライト（0.985）はどの USM でも約 4% が新規クリップ。
- alpha: 未加重 luma は透明縁の黒背景合成誤差 26.1 code（ミラーは 15.9）。

### rd2: ソフトエッジ（Gaussian σ=1.2px、t=0 アブレーション）

| variant | rise | 改善 | halo | overshoot | ゲート |
|---|---:|---:|---:|---:|---|
| `product_usm_mirror_t0` | 1.81 px | 15.8% | 1.60% | +0.0084 | 不合格 |
| `halo_limited_t0` | 1.81 px | 15.8% | 0.00% | 0.0000 | **通過** |
| `alpha_weighted_limited_t0` | 1.81 px | 15.8% | 0.00% | 0.0000 | **通過** |
| `detail_capped_t0` | 1.87 px | 13.2% | 1.60% | +0.0066 | 不合格 |
| `luma_usm_soft_t02` | 2.15 px | 0.0% | 0.00% | 0.0000 | 応答なし |

- ソフトエッジでは **USM と同じ 15.8% の先鋭化を halo 0 で達成**。amount 2.0 では 23.9% 対 27.7%。
- 絶対しきい値 0.02 はソフトエッジ応答を完全停止。alpha 加重だけで合成誤差 26.1 → 10.7 code、クランプ併用で 0.0。
- 時間: high-pass std 比 1.196（ゲート内）だがフレーム差分比 1.583。

### rd3b: 適応しきい値・ロールオフ・luma ガード・CC0 実写

ノイズ床 `σ̂` は 16×16 ブロック MAD の 10 パーセンタイル。しきい値は `k·σ̂`。

| fixture / model | 結果 |
|---|---|
| noisy chart `noise_rel_k1/k2/k3` | 平坦ノイズ比 2.018 / 1.684 / **1.512**（ゲート ≤1.15 は不合格）、rise 改善 29.2 / 29.4 / 30.0% |
| clean chart `halo_rolloff` | HL 新規クリップ **4.41% → 0.00%**、rise 改善は 0.2%（ハードエッジ） |
| soft edge 全クランプ系 | rise 改善 **15.8%**、halo 0、overshoot 0 |
| neon（CC0）mirror | 新規クリップ 1.47%、luma 極値超過 15.50%、chroma 1.69 |
| neon `luma_guard_combo` | 新規クリップ **0%**、極値超過 **0%**、chroma **0.07**、detail ROI 高域エネルギー 1.58×（mirror 3.07×） |
| street light `luma_guard_combo` | 新規クリップ 0%、極値超過 0%、chroma 0.03、detail ROI 1.34×（mirror 2.67×） |
| per-channel クランプ（neon） | 極値超過 **3.40%** が残る → per-channel は luma ハローを保証しない |
| クランプ半径 1/2/3/6/12 px | soft edge の改善はすべて 15.8% で差なし |

- **適応しきい値は k=3 ではノイズを 1.15 倍以下に抑えられない**（1.512）。約 30% の低減まで。ゲートは不合格として記録し、k を上げる案は未検証。
- **ハイライトのロールオフはクリップを 0 にし、ソフトエッジのゲインを落とさない**（0.158 のまま）。
- **luma ガードは実写でも極値超過 0 を達成**し、クリップ・色差も最小。ただし同 amount のミラーに対して高域エネルギーは約半分（neon 1.58× vs 3.07×）。ハロー除去とディテール量のトレードオフが数値で残る。
- クランプ半径はこの演算子の調整軸にならない。ゲインを決めるのはエッジ幅とヘッドルーム。

## 証跡

- `captures/rd1|rd2|rd3/manifest.json` + `manifest.sha256`（環境、入力/出力 hash、ROI、全指標、ゲート判定）
- `captures/rd3/manifest_first_attempt.json`: 初回 rd3 の失敗記録（global median 推定器が 0 に潰れた）
- preview / contact / ROI crops / diff は目視用（Reinhard+sRGB、測定には使わない）
- rd3 の生 float32 `.npz` は mirror と `luma_guard_combo` のみ。他モデルは float32 のメモリ上結果から指標を記録

## 既知の限界

- モデルはオフライン実装で、製品キャプチャではありません。
- ソフトエッジは宣言した Gaussian PSF、実測レンズ PSF ではありません。
- ノイズ床は静かな 10% ブロックから推定するため、混合グレインや圧縮ノイズは対象外です。
- 実写は 8-bit sRGB JPEG。HDR とクリップ済みディテールは復元できません。
- 極値超過は空間的なハロー代理指標で、知覚的なハロー測定ではありません。
- 独立した人間レビューは未実施です。

## 関連

- `docs/planned/MILESTONE_DETAIL_SHARPENING_LAB_2026-11-14.md`
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/technical/HOT_PATH_RULES.md`
- `Artifact/src/Effects/Sharpen/SharpenEffect.cpp`
