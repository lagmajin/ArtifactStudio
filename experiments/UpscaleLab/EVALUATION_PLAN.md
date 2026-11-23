# 静止画アップスケーラ（超解像）R&D — 事前登録済み画像評価計画

**最終更新:** 2026-11-15
**Status:** 候補画像を生成する前に登録。ゲートは rd0 の baseline 計測直後に確定し、学習モデル（rd1）の実行前に固定する。

本計画は `docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う。オフライン・決定論の CPU スタディであり、Artifact バイナリを起動しない。製品パリティ、エフェクト採用、既定値変更を証明しない。

## 1. 目的と対象

**目的:** 静止画レイヤーの拡大（等倍を超えるスケールアップ）において、現行のリサンプル（bicubic / Lanczos 相当）と「bicubic + シャープ」に対し、学習型超解像がディテール・エッジ・色・時間安定性の面で実際に改善をもたらすかを、忠実度・破綻・コストの三系統で切り分ける。採用可否ではなく、方式選定と後続実装の要否判断の材料を出す。

**対象**

- 合成チャート（決定論生成、HR 1920×1080）: ステップエッジ、1px 細線、文字風ストローク、色エッジ、グラデーション、クリップ寸前ハイライト、平坦部、ノイズ、ゾーンプレート
- CC0 実写 2 枚（`experiments/GlowLab/assets/` 記録の素材のみ）
- straight alpha の縁と半透明領域（RGB を 0 にした版と、透明部に RGB が残る版の 2 種）
- 連番 5 フレーム（0.5px サブピクセル移動）
- 倍率 ×2 / ×4
- 候補: `baseline_bilinear` / `baseline_bicubic` / `baseline_lanczos4` / `bicubic_then_unsharp`（現行のユーザーができる代替）/ 学習型（rd1 以降）

**対象外（未検証として記録し、合否の根拠にしない）**

- Artifact 本体の GPU 経路、Composition Viewer、Render Queue、preview / final のルーティング
- D3D12 / Vulkan の backend parity、OCIO、HDR 表示変換、working space の linear 変換
- 動画、optical flow によるフレーム補間、3D レイヤーのテクスチャ
- 製品の既定値・プロパティ範囲・UI・エフェクトカタログ
- DirectML 推論、VRAM 使用量、推論サーバー構成
- 実素材における「正解」の証明（ダウンサンプル往復は再構成上限の評価であり、実素材の真値ではない）

## 2. 比較対象と役割

| 項目 | 役割 | ground truth と呼べるか |
|---|---|---|
| HR 原板（チャート、実写） | 座標・エッジ・階調・ノイズ統計の基準 | 制限付きで Yes（「元の原板に戻るか」の基準）。実写は撮影・現像条件が固定されていないため物理的真値ではない |
| LR 入力（面積平均で ×2 / ×4 縮小） | すべての候補の共通入力 | No |
| `baseline_bilinear` | 製品が実行し得る最も単純な拡大 | No（control） |
| `baseline_bicubic` | 現行の標準的な拡大。以降の相対ゲートの基準 | No（control） |
| `baseline_lanczos4` | 高品質リサンプルの上限側 | No（control） |
| `bicubic_then_unsharp` | 現行のユーザーが手で再現できる代替 | No（control） |
| 学習型候補（rd1） | Real-ESRGAN 系など | No（候補） |

数値は原板と各 control の両方に対して報告し、control を「正解」として扱わない。PSNR / SSIM は補助指標であり、単独で画質の合格を証明しない。

## 3. 評価素材

すべて `upscale_lab.py` が `seed=20261115` で決定論生成する。実写のみリポジトリ内の CC0 素材を使う。各ファイルの SHA-256 と生成条件を `captures/rd0/manifest.json` に記録する。

- `chart_hr.png`: 1920×1080。ROI（HR 座標）は `step_edge` / `fine_lines` / `text_strokes` / `color_edge` / `ramp` / `highlight_step` / `flat_dark` / `flat_mid` / `flat_light` / `noise_fine` / `noise_coarse` / `zone_plate`
- `photo_neon_streets_hr.png`: 1536×1025（GlowLab の `cc0-neon-streets-1536.jpg` を無変換で使用）
- `photo_street_light_hr.png`: 576×1280（同 `cc0-street-light-night.jpg`）
- `alpha_straight_hr.png`: 512×512 RGBA。アンチエイリアスされた円形エッジ、半透明リング、α=0 の領域は RGB=0
- `alpha_hidden_rgb_hr.png`: 同上だが透明領域にも RGB パターンが残る版
- `sequence_hr/frame_??.png`: 640×400 タイル（ゾーンプレート＋細線＋平坦部）を 0.5px ずつサブピクセル移動させた 5 フレーム

LR 入力は `INTER_AREA` で生成する。連番は HR タイルを `warpAffine`（bicubic）で 0.5px 移動させてから縮小する。縮小・移動のパラメータは manifest に記録する。

## 4. 固定画像条件

- 色: 8-bit sRGB（gamma-encoded）、チャネル順 RGB。LR 生成と候補処理はこの 8-bit 表現で行う（学習モデルの学習分布に合わせるため）。製品の linear 経路との差は本ラボでは評価しない。
- 計測空間: PSNR / SSIM は gamma-encoded luma `Y = 0.2126R + 0.7152G + 0.0722B`（0〜255）。色差は OKLab（sRGB → linear → LMS → OKLab、ユークリッド距離）。透明合成誤差は 8-bit code。
- 出力: 各候補は HR 解像度へ拡大する。モデルが異なる倍率を返す場合は HR へ `INTER_AREA`（縮小）または `INTER_LINEAR`（拡大）で合わせ、その事実を記録する。
- alpha: 候補は RGB のみを処理し、alpha は `baseline_bicubic` と同じ bicubic で別途拡大する（製品に近い近似として固定）。straight alpha を前提とし、合成は `out.rgb·α + bg·(1−α)`。
- 設定: 学習モデルの前処理（0〜1 正規化、NCHW）と後処理（クリップ、丸め）は候補ごとに記録し、共通化できない場合は個別記録とする。classical の unsharp は `amount=1.0, sigma=1.0`（8-bit、ガウス blur 後の差を加算、0〜255 でクリップ）。
- 乱数: チャート生成とノイズ生成のみ `seed=20261115`。候補処理に乱数は入れない。推論の provider 決定性は CPU 単一スレッドを要求し、スレッド数は記録する。
- 出力形式: レビュー用 PNG は 8-bit。測定は float32 の内部配列に対して行い、表示用に正規化した差分画像は「表示用」と明記する。

## 5. 必須評価項目、領域、事前登録ゲート

各段の数値は「rd0 の baseline 実測」から算出した相対ゲートを用い、rd0 完了時に `captures/rd0/gates.json` として確定する。学習モデルの実行後にゲートを変更しない。変更する場合は理由・前後・承認者・再実行を記録する。

| 項目 | 領域・手順 | 事前登録ゲート（rd0 確定） |
|---|---|---|
| 効果の正しさ | `baseline_bilinear` で入力と同解像度の比較（等倍）: 完全同一を確認 | 最大絶対差 = 0 |
| 忠実度（再構成） | 全 ROI と全画面の PSNR / SSIM（luma） | 学習型候補は `baseline_bicubic` より PSNR が ≥ 0.5 dB 高い、または SSIM が ≥ 0.005 高い（いずれか）。主控は control として記録 |
| エッジ応答 | `step_edge` の行平均プロファイル、20→80% rise 距離 | 学習型候補は `baseline_bicubic` 比で rise 距離を ≥ 10% 短縮 |
| リンギング / オーバーシュート | `step_edge` / `fine_lines` / `text_strokes`。原板の 5×5 局所 min/max を外れた量（許容 2 code） | 学習型候補の out-of-range 画素率 ≤ 2.0%、平均超過 ≤ 1.5 code |
| ノイズ増幅 | `flat_*` と `noise_fine` / `noise_coarse` の高周波 σ 比（原板比） | 学習型候補の σ 比 ≤ 1.25（`flat_*`）。`noise_*` は参考値でゲートなし |
| 色 | 全画面と `color_edge` の OKLab ΔE 平均 / p95 | 学習型候補は `baseline_bicubic` 比で ΔE 平均の悪化 ≤ +0.004 |
| 透明・境界（改訂） | `alpha_*` の α は候補間で完全一致。α が 0.02〜0.98 の帯を 3px 拡張した領域で、黒／白／ Mid gray の 3 背景合成誤差 | α 最大差 = 0。合成誤差は `baseline_bicubic` 比で band mean +1.0 code 以内、band max 1.25 倍以内（rd0 の baseline 実測は `gates.json`） |
| 時間安定性 | `sequence_hr` の 5 フレーム。平坦 ROI の平均 luma 標準偏差、ゾーンプレートの高周波エネルギー標準偏差、隣接フレーム間平均絶対差 | 学習型候補の標準偏差比（対 `baseline_bicubic`）≤ 1.5 |
| 解像度依存 | ×2 と ×4 の両方で上記を評価 | ×2 と ×4 で符号と傾向が一致すること（不一致は要因分析し、未達なら保留） |
| 捏造ディテール | 全 ROI の拡大 crop と差分画像の目視レビュー | 数値では判定不能。**独立した人間の視覚レビューは未実施**。捏造の疑いがある出力は合格扱いにしない |
| 性能 | 候補ごとの wall time（3 回 median）、ピーク RSS（取得可能な場合）、入出力サイズ | ゲートなし。記述のみで、製品の予算を主張しない |

1 つでもゲート行が未測定のままの候補は、全体の合格を主張しない。未検証は未検証として残す。

rd0 で判明した不備は 2026-11-15 のユーザー承認で改訂済みである。前後値・理由・決定日は `captures/rd0/gates.json` の `gate_revision` に記録した。

- 透明・境界の合成誤差 2/255 は `baseline_bicubic` 自身が超過した（×2 黒背景 max 60.5 / mean 4.5、×4 max 83.9 / mean 13.9）。絶対値では候補を識別できないため、同じ倍率の `baseline_bicubic` 比へ変更した。
- α は全候補で同一の bicubic 拡大を使うため、「α 最大差 = 0」は候補間の前提条件であり、候補の優劣を示さない。

## 6. 記録する画像統計

- 画素値分布: チャンネル毎 min / max / mean / p1 / p50 / p99、非有限値数、新規クリップ率（入力 ≤ 255、出力 > 255）
- 参照差分: MAE / RMSE / 最大値 / p99、閾値超過率、差分画像（表示用は ×8 正規化）
- 構造: エッジ rise、高周波（Laplacian）エネルギー、ゾーンプレートの周波数応答
- 色: OKLab ΔE 平均 / p95 / max、チャンネル偏り
- 透明: α 分布、透明画素の RGB 漏れ、背景別合成誤差
- ノイズ・時間: 平坦領域 σ、フレーム間差、時間方向標準偏差
- 効果固有: エッジ近傍の色にじみ（`color_edge`）、細線（1px）の再現率、文字風ストロークの可読性

全画面の平均値と固定 ROI の値を必ず併記する（平均は局所欠陥を隠す）。

## 7. 実行条件と証跡

- Python 3.14.2（`C:\Users\lagma\AppData\Local\Programs\Python\Python314\python.exe`）、NumPy 2.4.3、OpenCV 4.13.0、Pillow 12.2.0。学習モデルを使う場合のみ onnxruntime（CPU provider、スレッド数固定）を追加する。
- 実行: リポジトリルートから `python experiments/UpscaleLab/upscale_lab.py --rd rd0`。出力は `captures/rd0/`。
- 成果物: `manifest.json`（素材 hash、環境、seed、条件、ROI）、`metrics.json`（全候補・全 ROI の測定値）、`gates.json`（rd0 確定ゲート）、レビュー PNG、ROI crop、必要な範囲の `.npz`。
- 各記録は入力・参照・出力の識別情報、測定空間・値域、α の扱い、指標の定義、集計方法、ツールバージョン、許容値、実行日を保持する。
- 判定者: 実行主体は AI エージェント。独立した人間の視覚レビューは未実施であることを manifest に明記する。
- モデル weights はリポジトリに置かない。出所・ライセンス・SHA-256 は `MODELS.md` と `manifest.json` に記録する。

## 8. 比較と判定

- 全体像と ROI crop を同じ表示条件で並べ、数値と目視の両方で確認する。必要に応じて複数背景へ合成する。
- 候補と control は同一入力・同一条件で比較し、改善と新たな欠陥（リンギング、色にじみ、ノイズ増幅、ディテールの捏造、時間ちらつき）を併記する。
- 欠陥は場所・再現条件・影響・重大度・証拠・対応状況を記録する。重大な欠陥または必須項目の未検証がある候補は合格扱いにしない。
- PSNR / SSIM の改善だけで視覚品質の向上を断定しない。

## 9. 完了条件

rd0（baseline とゲート確定）、rd1（学習モデル比較）、rd2（連番・alpha・解像度・性能）、rd3（採用／不採用判断と後続実装の受入条件）の結果を残す。未達・未検証が残る場合は、その状態のまま明示し、画質達成や製品統合完了とは表現しない。R&D の完了は製品実装や新機能追加の承認を意味しない。