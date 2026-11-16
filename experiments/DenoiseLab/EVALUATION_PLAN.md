# 空間デノイズ R&D — 事前登録済み画像評価計画

**最終更新:** 2026-11-15
**Status:** 候補画像を生成する前に登録。ゲートは rd0 の baseline 計測前後で確定し、候補比較（rd1 以降）の前に固定する。

本計画は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う。オフライン・決定論の CPU スタディであり、Artifact バイナリを起動しない。製品パリティ、エフェクト採用、既定値変更を証明しない。

## 1. 目的と対象

**目的:** 静止画レイヤーおよび連番画像レイヤーに写り込んだ撮像ノイズ（高 ISO、低照度、CG の未収束ノイズ）を空間フィルタでどこまで落とせるか、そのときに何を壊すかを、既知の clean 画像に対する再構成誤差とディテール保持の両面で切り分ける。採用可否ではなく、方式選定と後続実装の要否判断の材料を出す。

**対象**

- 合成チャート（決定論生成、1536×1024）: ステップエッジ、1px 細線、文字風ストローク、色エッジ、平坦部（暗・中・明）、肌色グラデーション、ランプ、高周波ディテール、ゾーンプレート、彩度を持つ平坦部、毛髪状細線
- CC0 実写 1 枚（`experiments/GlowLab/assets/cc0-neon-streets-1536.jpg`）
- 3 つのノイズモデル（`fine` / `strong` / `lowlight`）: luma 成分とチャネル独立の chroma 成分を加算し、8-bit へ量子化
- 連番 5 フレーム（0.5px サブピクセル移動）での時間安定性
- 候補: `baseline_raw`（無処理）/ `median3` / `gaussian_s1` / `bilateral` / `nlm_light` / `nlm_strong`

**対象外（未検証として記録し、合否の根拠にしない）**

- Artifact 本体の GPU 経路、Composition Viewer、Render Queue、preview / final のルーティング
- 既存の `Temporal Denoise` エフェクトとの統合、フレーム履歴・`ArtifactEffectFrameSampler` 経路
- D3D12 / Vulkan backend parity、OCIO、HDR 表示変換、linear working space
- 学習型デノイズ（DnCNN / SwinIR / BM3D など）と DirectML 推論
- 動画圧縮ノイズ、フィルムグレインの意図的保持、scanner noise のモデル化
- 製品の既定値・プロパティ範囲・UI・エフェクトカタログ
- 実素材における「正解」の証明（ノイズ注入は再構成上限の評価であり、実際の撮像ノイズの分布を代表しない）

## 2. 比較対象と役割

| 項目 | 役割 | ground truth と呼べるか |
|---|---|---|
| clean 合成チャート | 座標・エッジ・階調・ディテールの基準 | Yes（合成なので厳密） |
| 実写 clean（CC0） | 実テクスチャ上での再構成基準 | 制限付き（撮影時点で既にノイズを含むため厳密な clean ではない） |
| ノイズ注入画像 | 全候補の共通入力 | No |
| `baseline_raw` | 無処理。ゲートの分母 | No（control） |
| `median3` / `gaussian_s1` / `bilateral` | 現行のユーザーが代替できる古典的手法 | No（control） |
| `nlm_light` / `nlm_strong` | 古典的手法の品質側の上限 | No（候補） |

実写は GT が厳密でないため、実写ケースでは平坦部ノイズ比・ランプ・エッジ位置のゲートを適用せず未適用として記録する。

## 3. 評価素材

すべて `denoise_lab.py` が `seed=20261115` で決定論生成する。実写のみリポジトリ内の CC0 素材を使う。各ファイルの SHA-256 と生成条件を `captures/rd0/manifest.json` に記録する。

- `chart.png`: 1536×1024。ROI は `step_edge` / `fine_lines` / `text_strokes` / `color_edge` / `flat_dark` / `flat_mid` / `flat_light` / `skin_tone` / `ramp` / `fine_detail` / `zone_plate` / `flat_colored` / `hair_strands`。座標と画素数は manifest に記録
- `photo_neon_streets.png`: 1536×1025（GlowLab の `cc0-neon-streets-1536.jpg` を無変換で使用）
- `chart_<noise>.png` / `photo_neon_streets_<noise>.png`: ノイズ注入版（入力）
- 連番: 640×400 タイル（平坦部・ゾーンプレート・細線）を 0.5px ずつ移動した 5 フレーム。ノイズはフレームごとに独立 seed

ノイズ契約: `channel = clean + N(0, luma_sigma²) + N(0, chroma_sigma²)`（chroma はチャネル独立）、0〜255 でクリップして 8-bit へ丸め。パラメータは `fine` = (5, 3)、`strong` = (12, 12)、`lowlight` = (20, 30)。量子化後の実測 σ は平坦 ROI で測り manifest に残す。

## 4. 固定画像条件

- 色: 8-bit sRGB（gamma-encoded）、チャネル順 RGB。ノイズ注入・フィルタ・計測はすべてこの表現で行う。製品の linear 経路との差は本ラボでは評価しない
- 計測空間: PSNR / SSIM は gamma-encoded luma `Y = 0.2126R + 0.7152G + 0.0722B`（0〜255）。色差・彩度ノイズは OKLab
- フィルタ: すべて 3 チャネル同時処理。`bilateral` は `d=7, sigmaColor=25, sigmaSpace=5`、`nlm_*` は `fastNlMeansDenoisingColored`（template 7、search 15 / 21、h 4 / 10）
- 乱数: フィクスチャ生成とノイズ注入のみ `seed=20261115` 系。フィルタ自体に乱数は無い
- 出力形式: レビュー用 PNG は 8-bit。測定は uint8 画像から float32 に変換して行う
- 実行時間: 各候補 3 回の median を wall time として記録（同一プロセス内、ウォームアップ 1 回）

## 5. 必須評価項目、領域、事前登録ゲート

チャートケースのゲートはすべて「同じケース・同じノイズモデルの `baseline_raw`」に対する相対値とする。rd0 の実測でゲートが識別力を持たないと判明した場合は、理由・前後・承認者・再実行を `captures/rd0/gates.json` の `gate_revision` に記録する。

| 項目 | 領域・手順 | 事前登録ゲート |
|---|---|---|
| 効果の正しさ | `baseline_raw` の PSNR / SSIM が入力と完全一致すること | 完全一致（control の健全性） |
| ノイズ低減（平坦部） | `flat_dark` / `flat_mid` / `flat_light` の高周波 σ（Gaussian σ2 のハイパス）比 | 3 領域の最悪値 ≤ 0.40（= 60% 以上の低減） |
| 再構成 | 全画面 PSNR（luma, clean 基準） | 入力比 +2.0 dB 以上 |
| ディテール保持 | `fine_lines` の列平均プロファイル p95−p5（線コントラスト）の clean 比 | ≥ 0.85 |
| エッジ位置 | `step_edge` の 50% 交差位置（サブピクセル）と clean の差 | ≤ 0.25 px |
| 色 | 全画面 OKLab ΔE 平均の入力比改善 | 改善 ≥ 0.5 |
| 階調・バンディング | `ramp` の distinct level 数（clean 比）と隣接列ステップ最大 | level 保持 ≥ 0.85、最大ステップ ≤ 2 code |
| 彩度ノイズ | `flat_colored` の OKLab a/b ハイパス σ の入力比 | ≤ 0.60 |
| 時間安定性 | 連番 5 フレームの平坦 ROI 平均 luma の標準偏差（`baseline_raw` 比） | ≤ 1.25 |
| 性能 | 候補ごとの wall time（3 回 median） | ゲートなし。記述のみで製品予算を主張しない |
| 目視 | ROI crop の clean / noisy / candidate 3 面シート | 数値では判定不能。**独立した人間の視覚レビューは未実施**。ぼけ・まだら・ディテール捏造が疑われる出力は合格扱いにしない |

1 つでもゲート行が未適用・未測定のままの候補は、全体の合格を主張しない。未検証は未検証として残す。

## 6. 記録する画像統計

- 画素値分布: チャネル毎 min / max / mean、非有限値数
- 参照差分: PSNR / SSIM / ΔE（mean, p95, max）、平均絶対誤差
- 構造: ハイパス σ、Laplacian エネルギー比、線コントラスト、エッジ交差位置
- 色: OKLab a/b のハイパス σ（彩度ノイズ）、ΔE
- 階調: distinct level 数、隣接ステップ最大（バンディング・ポスタリゼーション）
- 時間: 平坦平均 luma の標準偏差、ゾーンプレートの高周波エネルギー標準偏差、隣接フレーム平均絶対差
- 効果固有: 毛髪状細線（`hair_strands`）の線の連続性、肌色グラデーション（`skin_tone`）のまだら

全画面の平均値と固定 ROI の値を必ず併記する（平均は局所欠陥を隠す）。

## 7. 実行条件と証跡

- Python 3.14.2、NumPy 2.4.3、OpenCV 4.13.0（`cv2.ximgproc` は無いため guided filter は候補に含めない）
- 実行: リポジトリルートから `python experiments/DenoiseLab/denoise_lab.py --rd rd0`
- 成果物: `manifest.json`（素材 hash、環境、seed、ノイズモデル、候補パラメータ、ROI）、`metrics.json`（全候補 × ケース × ノイズ × ROI）、`gates.json`（ゲート、判定、連番指標）、レビュー PNG
- 各記録は入力・参照・出力の識別情報、測定空間・値域、指標の定義、集計方法、ツールバージョン、許容値、実行日を保持する
- 判定者: 実行主体は AI エージェント。独立した人間の視覚レビューは未実施であることを manifest に明記する

## 8. 比較と判定

- 全体像と ROI crop を同じ表示条件で並べ、数値と目視の両方で確認する
- 候補と control は同一入力・同一条件で比較し、改善（σ 低減・PSNR・ΔE）と新たな欠陥（ぼけ、線の消失、まだら、エッジ移動、階調の平板化）を併記する
- 欠陥は場所・再現条件・影響・重大度・証拠・対応状況を記録する。重大な欠陥または必須項目の未検証がある候補は合格扱いにしない
- PSNR / SSIM の改善だけで視覚品質の向上を断定しない

## 9. 完了条件

rd0（baseline とゲート確定）、rd1（候補比較とパラメータ依存）、rd2（連番・実写・性能・解像度依存）、rd3（採用／不採用判断と後続実装の受入条件）の結果を残す。未達・未検証が残る場合は、その状態のまま明示し、画質達成や製品統合完了とは表現しない。R&D の完了は製品実装や新機能追加の承認を意味しない。
