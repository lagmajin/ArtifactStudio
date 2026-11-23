# Upscale Lab

**最終更新:** 2026-11-15
**ステータス:** rd0 未実行（骨組みと事前登録評価計画のみ）

静止画・連番画像レイヤーの拡大（等倍超えのスケールアップ）に関する研究用 Python ラボです。現行のリサンプル（bicubic / Lanczos）と「bicubic + シャープ」を control とし、学習型超解像がディテール・エッジ・色・時間安定性・コストのどれを改善し、何を壊すかを切り分けます。ここで得た結果は製品実装・既定値・UI の承認ではありません。

製品コード（Artifact / ArtifactCore）は変更しません。オフラインの NumPy / OpenCV のみで完結し、Artifact バイナリは起動しません。

評価計画は実行前に [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md) へ登録済みです。`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従います。マイルストンは `docs/planned/MILESTONE_UPSCALER_SUPER_RESOLUTION_RD_2026-11-15.md`。

## 対象と非対象

**対象**

- 合成チャート（決定論生成、HR 1920×1080）と CC0 実写 2 枚
- 倍率 ×2 / ×4
- classical control（bilinear / bicubic / Lanczos4 / bicubic+unsharp）と学習型候補
- straight alpha の縁、連番 5 フレームの時間安定性

**非対象（未検証として記録）**

- Artifact の GPU 経路、Composition Viewer、Render Queue、preview / final
- D3D12 / Vulkan parity、OCIO、HDR 表示変換、linear working space
- 動画、optical flow フレーム補間、3D テクスチャ
- 製品の既定値・プロパティ範囲・UI・エフェクトカタログ
- DirectML 推論、VRAM 使用量

## 実行

Python 3.10 以降。`numpy` / `opencv-python-headless` / `pillow` が必要です（`requirements.txt`）。学習モデルを使う場合のみ `onnxruntime` を追加します。

```powershell
python experiments/UpscaleLab/upscale_lab.py --rd rd0
```

出力は `captures/rd0/`（`manifest.json`, `metrics.json`, `gates.json`, レビュー PNG、ROI crop）。fixture は seed `20261115` で決定論生成します。

学習モデルは `models/` に配置します（リポジトリには含めません）。入手方法とライセンスは [`MODELS.md`](MODELS.md) を参照してください。モデルが無い場合、その候補は `model-missing` として記録され、比較は classical control のみで実行されます。

## ディレクトリ構成

| パス | 内容 |
|---|---|
| `EVALUATION_PLAN.md` | 事前登録済みの評価計画（目的・素材・条件・ゲート・証跡） |
| `MODELS.md` | 学習モデルの出所・ライセンス・配置手順 |
| `upscale_lab.py` | fixture 生成、classical control、学習モデル実行、指標測定、manifest 出力 |
| `models/` | 学習モデル（git 管理外） |
| `captures/` | 実行結果（rd0 / rd1 …） |

## 主な結果

rd0（2026-11-15 実行）の全数値は `captures/rd0/metrics.json`、確定ゲートは `gates.json`。chart fixture（HR 1920×1080、seed 20261115）の実測:

| 候補 | ×2 PSNR / SSIM / ΔE | ×2 rise | ×4 PSNR / SSIM | ×4 rise | リンギング ×2 / ×4 | ms |
|---|---|---|---|---|---|---|
| `baseline_bilinear` | 28.38 / 0.9972 / 0.0109 | 1.41 | 26.03 / 0.9948 | 2.40 | 0.0 / 0.0 | 0.8 |
| `baseline_bicubic` | 28.92 / 0.9976 / 0.0099 | 1.24 | 26.07 / 0.9948 | 2.25 | 0.0 / 0.0 | 1.3 |
| `baseline_lanczos4` | 28.78 / 0.9971 / 0.0100 | 1.17 | 25.99 / 0.9937 | 2.20 | 5.8% / 9.0% | 3.0 |
| `bicubic_then_unsharp` | 28.88 / 0.9976 / 0.0102 | 0.78 | 26.09 / 0.9949 | 1.95 | 0.0 / 0.0 | 36 |

（rise は step edge の 20→80% 立ち上がり距離。原板は 0.60。リンギングは原板 5×5 局所 min/max + 2 code の外に出た画素率）

- **bicubic は PSNR / SSIM で bilinear を上回るが、エッジ立ち上がりは悪化する**（0.60 → 1.24）。数値の良さとエッジの鋭さが一致しない。
- **Lanczos4 は立ち上がり最良だがリンギングが出る**（×2 5.8% / ×4 9.0%、最大超過 15〜18 code）。細線・文字では実害が出る可能性がある。
- **bicubic + unsharp は rise 0.78 まで戻すが、noise_fine の σ 比が 0.38 → 0.61 に上がる**。平坦部を持ち上げる代償が見える。処理時間も 25 倍。
- 平坦部（`flat_dark` / `flat_mid` / `flat_light`）はいずれも σ 比が定義できない（原板が完全平坦）。ノイズ評価は `noise_fine` / `noise_coarse` でしか効かない。
- 実写 2 枚は bicubic が bilinear より +0.9〜2.0 dB。×4 では差が縮む（neon streets 24.00 → 24.74）。Lanczos は写真で bicubic と同等以上。
- 連番 5 フレーム（0.5px 移動）は全候補で平坦部の平均 luma が完全一致（std 0.0）。ゾーンプレートの高周波 std は bicubic 10.4 に対し unsharp 19.7 で、フレーム間の揺れが増える方向。
- **alpha 合成誤差は事前登録ゲート（2/255）が成立しない**。×2 の bicubic でも黒背景 max 60.5 / mean 4.5。1px の α ランプはどの拡大でも復元できないため、絶対ゲートでは全候補が落ちる。相対ゲートへの変更可否を rd1 前に確定する必要がある。

学習モデルの比較（rd1）は重み未入手のため未実施。したがって「学習型が classical を上回るか」は本ラボでは未検証。

## 免責

本ラボの出力は研究用の測定値であり、製品の画質・性能・既定値を保証しません。実写素材の出所とライセンスは `experiments/GlowLab/assets/SOURCES.md` の記録に従います。数値の改善を視覚品質の証明として扱いません。