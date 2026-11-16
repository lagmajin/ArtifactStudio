# 静止画アップスケーラ（超解像）R&D

**最終更新:** 2026-11-15
**ステータス:** In Progress（R0 / rd0 実行済み。学習モデル比較の rd1 は重み未入手で未着手）
**マイルストン ID:** M-IMG-UP-RD-1
**ラボ:** `experiments/UpscaleLab/`

## 目的

静止画レイヤーおよび連番画像レイヤーの拡大（等倍を超えるスケールアップ）について、現行のリサンプル（bicubic / Lanczos 相当）と「bicubic + シャープ」を control とし、学習型超解像がディテール・エッジ・色・時間安定性・コストのどれを改善し、何を壊すかを、再現できる fixture と数値で判定する。

この文書は研究プロジェクトの起点であり、製品実装・既定値変更・UI 追加・エフェクトカタログ追加を承認しない。調査結果から実装候補を絞り、変更は後続の明示された実装段階で行う。

## なぜ今か

- 開発優先方針（`AGENTS.md`）が静止画・連番画像レイヤーの完成度を上位に置いている。
- `docs/analysis/TECH_ADOPTION_CANDIDATES_2026-07-27.md` で Real-ESRGAN 系が「静止画レイヤー品質向上」の候補として挙がっている。
- ONNX Runtime + DirectML は既に導入済み（`vcpkg.json` の `onnxruntime`、`ArtifactCore/src/AI/OnnxDmlLocalAgent.cpp`、`OnnxImageSegmenter.cpp`）で、新規ネイティブ依存を増やさずに検証できる。
- リポジトリ側に「素材を拡大するための超解像」は存在しない。`ArtifactIRenderer::setUpscaleConfig()` / `upscaleRenderScale()` は 0.70〜1.00 の**レンダースケール**（合成解像度）の設定であり、レイヤー素材の拡大とは別系統である。したがって本 R&D は既存実装の改善ではなく、未着手領域の feasibility 評価になる。

## 現行実装の確認済み範囲

- `ArtifactIRenderer::setUpscaleConfig(bool enable, float sharpness)` は `upscaleScale = 0.70 + 0.30 * sharpness` を返し、`ArtifactIRenderer::initialize()` 系の解像度計算にだけ使う。素材の拡大処理ではない。
- 静止画レイヤーの拡大は GPU テクスチャサンプリング（linear）に依存する経路で、専用の超解像パスやエフェクトは無い。この理解は grep による確認であり、実機の GPU キャプチャで確認した事実ではない（未検証）。
- 画像エフェクト（`Artifact/src/Effects/`）にアップスケーラは無い。近い名前の `Transform` / `Wave` などは幾何変換であり超解像ではない。
- `ArtifactCore` 側に LUT / 3D LUT の適用経路（`ColorLUT`, `LUT3DComputer`, `LUTEffect`）はあるが、本 R&D の対象外。

## 対象 / 非対象

| 区分 | 内容 |
|---|---|
| 対象 | 静止画 1 枚、連番 5 フレーム、倍率 ×2 / ×4、classical control（bilinear / bicubic / Lanczos4 / bicubic+unsharp）、学習型候補、straight alpha の縁 |
| 対象外 | 製品 GPU 経路、Composition Viewer、Render Queue、preview / final、D3D12 / Vulkan parity、OCIO、HDR 表示変換、linear working space、動画、optical flow フレーム補間、3D テクスチャ、製品既定値・UI・カタログ、DirectML 推論、VRAM 計測 |

対象外は「未検証」として記録し、合否の根拠にしない。

## 進め方

| 段階 | 内容 | 状態 |
|---|---|---|
| R0 | fixture 生成、classical control の計測、ゲート確定、証跡整備（rd0） | 実行済み |
| R1 | 学習モデル 1〜3 本の比較（rd1）。ゲートは rd0 確定値のみ使用 | 未着手 |
| R2 | 連番・alpha・解像度依存・性能（rd2） | 未着手 |
| R3 | 採用／不採用判断と、後続実装の受入条件（rd3） | 未着手 |

各段階の詳細な条件・素材・指標・ゲートは `experiments/UpscaleLab/EVALUATION_PLAN.md` を正とする。開始前に `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` の必須項目を満たすこと。

## 合格ゲート（概要）

- 忠実度: 学習型候補が `baseline_bicubic` より PSNR +0.5 dB 以上、または SSIM +0.005 以上
- エッジ応答: `step_edge` の 20→80% rise 距離を bicubic 比で 10% 以上短縮
- リンギング: 原板局所以外の画素率 2.0% 以下、平均超過 1.5 code 以下
- 平坦部ノイズ: σ 比 1.25 以下
- 色: bicubic 比で OKLab ΔE 平均の悪化 +0.004 以下
- 透明: bicubic と同じ alpha を前提に、3 背景合成誤差が同一倍率の `baseline_bicubic` 比で band mean +1.0 code 以内、band max 1.25 倍以内（rd0 で絶対 2/255 から改訂、承認済み）
- 時間: bicubic 比の標準偏差比 1.5 以下
- 解像度依存: ×2 と ×4 で傾向が一致
- 捏造ディテール: 数値では判定不能。人間の視覚レビュー未実施の候補は合格扱いにしない

未測定の行が残る候補は全体の合格を主張しない。

## R0（rd0）実測結果

chart（HR 1920×1080）と CC0 実写 2 枚、×2 / ×4、control 4 種。全数値は `experiments/UpscaleLab/captures/rd0/metrics.json`。

- bicubic は bilinear に対し chart ×2 で PSNR 28.38 → 28.92、SSIM 0.9972 → 0.9976、ΔE 0.0109 → 0.0099。ただし step edge の 20→80% rise は 0.60 → 1.24 と悪化する（原板 0.60）。
- Lanczos4 は rise 1.17 で最良だがリンギングが出る（原板局所外 5.8% / ×4 9.0%、最大超過 15〜18 code）。
- bicubic + unsharp（amount 1.0, sigma 1.0）は rise 0.78 まで戻すが、`noise_fine` の σ 比が 0.38 → 0.61。処理時間は bicubic の約 25 倍（36 ms 対 1.3 ms）。
- 実写は bicubic が bilinear より +0.9〜2.0 dB。×4 で差が縮む。Lanczos4 は写真で bicubic と同等以上。
- 連番 5 フレームの平坦部平均 luma は全候補で std 0.0。ゾーンプレート高周波 std は bicubic 10.4 に対し unsharp 19.7。
- **ゲート改訂（承認済み）:** alpha 合成誤差 2/255 は bicubic baseline 自身が max 60.5（×2）で超過し識別力が無いため、同一倍率の `baseline_bicubic` 比（band mean +1.0 code、band max 1.25 倍）へ変更した。`captures/rd0/gates.json` の `gate_revision` に前後値と決定日を記録。
- 平坦部（`flat_*`）は原板が完全平坦で σ 比が定義できない。ノイズ評価は `noise_fine` / `noise_coarse` のみで成立する。

## リスクと未確認事項

- 学習モデルの重み入手・ライセンス・再配布可否が未確認（`experiments/UpscaleLab/MODELS.md`）。
- 8-bit sRGB 前提のモデルを製品の linear 経路へ載せる場合の色空間契約が未確定。本ラボでは扱わない。
- CPU ラボでは VRAM と DirectML の数値一致を評価できない。
- ダウンサンプル往復による再構成評価は、実素材の真値ではない。実素材での画質判断には別途素材が必要。
- PSNR / SSIM の改善を視覚品質の証明として扱わない。

## 関連資料

- `experiments/UpscaleLab/README.md`
- `experiments/UpscaleLab/EVALUATION_PLAN.md`
- `experiments/UpscaleLab/MODELS.md`
- `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`
- `docs/analysis/TECH_ADOPTION_CANDIDATES_2026-07-27.md`
- `docs/planned/MILESTONES_BACKLOG.md`