# AI マット抽出 R&D — 事前登録済み画像評価計画

**最終更新:** 2026-11-15
**Status:** 候補画像を生成する前に登録。ゲートは rd0 の baseline 計測直後に確定し、学習モデル（rd1）の実行前に固定する。

`docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md` に従う。オフライン・決定論の CPU スタディであり、Artifact バイナリを起動しない。製品パリティや機能採用を証明しない。

## 1. 目的と対象

**目的:** 静止画 1 枚から前景 alpha matte を取り出す品質を、既知の GT alpha を持つ fixture で測定し、GrabCut 系の二値マスクが破綻する境界（髪・獣毛・細線・半透明・ソフトシャドウ）で、classical baseline と学習型モデルがそれぞれ何を改善し何を壊すかを切り分ける。

**対象**

- 合成 fixture（GT alpha が厳密に既知、seed 固定）: アンチエイリアス円、髪状の細線、ガラス板、細いワイヤ、獣毛境界、ソフトシャドウ、グリーンスクリーン、CC0 実写の前景を別背景へ合成したもの
- classical baseline: GrabCut（矩形初期化）、GrabCut（GT trimap = oracle control）、chroma key、Otsu luma
- 学習型マット候補（rd1 以降）: RMBG / MODNet / BiRefNet 系の ONNX。rd1 で U2Net、rd3 で MODNet、rd4 で BiRefNet 512 を実行（入手記録は `MODELS.md`）。RMBG 系は ONNX 変換版が CC-BY-NC-4.0 タグのため見送り
- 併用経路（rd2）: 学習モデルの α を trimap として GrabCut に渡す派生候補。`--derive-trimap` で `<model>_trimap_grabcut`（二値）と `<model>_trimap_soft`（未知帯のみモデル α を残す）を追加する。学習型と同じゲートを適用し、`trimap_stats`（確実前景率・未知帯率・退化フラグ）を証跡に残す
- 後処理（rd5）: guided filter（入力画像をガイド）による α refinement。`--refine` で `<model>_refined` を追加し、raw と並べて比較する
- 指標: SAD / MSE / Gradient / Connectivity、境界帯 MAE、半透明領域 MAE、複数背景合成誤差、軟らかさ、runtime

**対象外（未検証として記録）**

- 動画・フレーム間伝播、temporal stability（静止画のみを対象）
- trimap の自動生成、Roto Brush の操作 UX、Undo
- 製品 GPU 経路、Composition Viewer、Render Queue、D3D12 / Vulkan parity
- OCIO / HDR、linear working space、EXR 入力
- DirectML 推論、VRAM、モデル配布の運用
- 実写素材の主観品質（GT matte 未整備）

## 2. 比較対象と役割

| 項目 | 役割 | ground truth と呼べるか |
|---|---|---|
| GT alpha（生成時に使用した α） | 正解 | Yes（合成 fixture のみ。実写の真値ではない） |
| GT foreground / background | 合成誤差と spill の参照 | Yes（合成 fixture のみ） |
| `grabcut_rect` | 製品に近い classical baseline（矩形のみ、ユーザー trimap なし） | No |
| `chroma_key` | 背景色が単色な場合の古典的手法 | No |
| `otsu_luma` | 高コントラスト素材の下限 | No |
| `grabcut_gt_trimap` | GT から作った trimap を使う oracle control。候補ではない | No（oracle） |
| 学習型候補（rd1） | 候補 | No |

oracle control は「trimap が与えられれば GrabCut がどこまで行けるか」の上限を示すために置き、合格判定には使わない。

## 3. 評価素材

`matte_lab.py` が `seed=20261115` で決定論生成する。各 fixture は `image`（合成入力）、`alpha`（GT）、`fg`、`bg` を保持し、ファイル名と SHA-256 を manifest に記録する。

| fixture | 内容 | 主に落とす手法 |
|---|---|---|
| `disc_aa` | 1px アンチエイリアス円、テクスチャ背景 | 二値化 |
| `hair_strands` | 1px 曲線 60 本、α 0.2〜1.0 | 二値マスク、境界帯 |
| `glass_panel` | α 0.35 の半透明板＋ハイライト | 不透明前提の手法 |
| `thin_wire` | 1〜2px の線、背景にノイズ | モルフォロジー収縮 |
| `fur_edge` | 値ノイズで変調された境界 | 境界帯 |
| `soft_shadow` | 不透明物体＋軟らかい接地影 | α=0/1 前提 |
| `green_screen` | 単色グリーン背景＋不透明body＋毛状フリンジ | 二値化、境界帯。chroma key の適用可能領域 |
| `photo_composite` | CC0 実写の前景を別 CC0 背景へ、有機的形状の軟らかい α で合成 | 実写テクスチャでの汎化 |

`green_screen` は rd0 で chroma key がテクスチャ背景の fixture すべてで空マスクになったため追加した。色キーの役割（単色背景）を満たす素材が 1 つも無く、baseline の適用範囲を測れなかったためである。fixture の追加であり、ゲートの変更ではない。

実写は `experiments/GlowLab/assets/` の CC0 素材のみを使い、出所と hash は `experiments/GlowLab/assets/SOURCES.md` に従う。

## 4. 固定画像条件

- 色: 8-bit sRGB。入力は `I = F·α + B·(1−α)` を 8-bit で丸めた合成画像。GT は丸める前の float32 α。
- α: straight（unassociated）。GT alpha は float32、0〜1。
- 解像度: 512×512（`photo_composite` は 640×640）。リサイズは行わない。
- classical の設定を固定: GrabCut は矩形 = 全画面の 2% 内側、`iterCount=5`、`GC_INIT_WITH_RECT`。trimap oracle は GT α を 3px 収縮（FGD）/ 3px 膨張（BGD）/ その間（PR_FGD）。chroma key は背景色を画像境界 8px 帯の中央値から推定。Otsu は luma に対して適用。
- 学習モデル: 入力正規化（0〜1 か ImageNet 平均・分散）と出力（sigmoid 済みか logits か）を候補ごとに記録。単一スレッド CPU。
- 乱数: fixture 生成のみ `seed=20261115`。候補処理に乱数は入れない。
- 出力: レビュー用 PNG（入力 / GT α / 候補 α / 灰背景合成 / 緑背景合成）。測定は float32。

## 5. 必須評価項目と事前登録ゲート

| 項目 | 領域・手順 | 事前登録ゲート |
|---|---|---|
| マット精度 | 全 fixture、SAD / MSE（α を 0〜1 で評価し SAD は 1000 倍） | 学習型候補は `grabcut_rect` の SAD を 40% 以上改善 |
| 勾配整合 | 全 fixture、Gradient error（Sobel 差分） | `grabcut_rect` 比で改善（符号付きで記録、悪化は不合格） |
| 連結性 | 全 fixture、Connectivity error（4-neighborhood） | `grabcut_rect` 比で悪化しない |
| 境界帯 | \|α−0.5\|<0.5 を 5px 膨張した帯の MAE | 0.05 以下 |
| 半透明 | 0.05<α<0.95 の画素の MAE（`glass_panel` と `soft_shadow`） | 0.08 以下 |
| 軟らかさ（改訂） | GT 部分 α（1/255<α<254/255）のうち、予測が ±0.1 以内の画素率（`hair_strands` / `fur_edge`） | 0.50 以上 |
| 合成誤差（改訂） | 灰・緑・白の 3 背景で `α_out·F + (1−α_out)·bg` と GT 合成の差（境界帯） | `grabcut_rect` 比で band mean +1.0 code 以内、band max 1.25 倍以内 |
| color spill | 前景色と背景色の混色方向への偏り（`photo_composite`） | 数値は記述。ゲートは rd0 後に確定 |
| 解像度・経路 | 対象外（単一解像度のみ） | 未検証 |
| 性能 | 候補ごとの wall time（3 回 median） | ゲートなし |

rd0 完了時に `captures/rd0/gates.json` へ baseline 実測と確定値を記録する。学習モデル実行後の変更は、理由・前後・承認者・再実行を記録する。

rd0 で判明したゲートの不備は 2026-11-15 のユーザー承認で改訂済みである。前後値・理由・決定日は `captures/rd0/gates.json` の `gate_revision` に記録した。

- 軟らかさの「`grabcut_rect` の 5 倍」は baseline の部分 α 率が 0 のため比が定義できず、GT 部分 α の回収率 0.50 以上（±0.1）へ変更した。rd0 の baseline 実測は `hair_strands` 0.071 / `fur_edge` 0.193。
- 合成誤差は絶対値の根拠が無いため、`grabcut_rect` 比（band mean +1.0 code 以内、band max 1.25 倍以内）へ変更した。
- SAD の 40% 改善は二値/軟らかいの差を含むため、境界帯 MAE と併記して判断する（ゲート自体は維持）。
- 上記は rd1 実行前に確定済みであり、学習モデルの結果を見た後の変更ではない。

1 つでも未測定の行が残る候補は全体の合格を主張しない。

## 6. 記録する画像統計

- α の min / max / mean / p5 / p50 / p95、0 と 1 に張り付いた画素率、非有限値数
- SAD / MSE / Gradient / Connectivity（定義と正規化を manifest に記録）
- 境界帯と半透明領域の MAE、最大誤差、誤差の分位点
- 前景・背景・境界の領域別誤差
- 合成後の色差（OKLab ΔE 平均 / p95）と、背景色方向への spill 量
- runtime（median）と入力サイズ

全画面平均と領域別を併記する。

## 7. 実行条件と証跡

- Python 3.14.2、NumPy 2.4.3、OpenCV 4.13.0、Pillow 12.2.0。学習モデルは onnxruntime（CPU、単一スレッド）。
- 実行: `python experiments/MatteLab/matte_lab.py --rd rd0`
- 成果物: `manifest.json`（fixture hash、環境、seed、設定）、`metrics.json`、`gates.json`、レビュー PNG。
- 判定者: 実行主体は AI エージェント。独立した人間の視覚レビューは未実施であることを明記する。
- モデル重みはリポジトリに含めない。

## 8. 比較と判定

- 入力・GT α・候補 α・複数背景合成を同じ表示条件で並べ、数値と目視の両方で確認する。
- 候補と baseline は同一 fixture・同一条件で比較し、改善と新たな欠陥（境界の欠け、内部の穴、spill、軟らかすぎる α）を併記する。
- SAD の改善だけで視覚品質の向上を断定しない。二値マスクが SAD で善戦しても、軟らかさのゲートで落ちることを明示する。

## 9. 完了条件

rd0（GT fixture と classical baseline、ゲート確定）、rd1（学習モデル比較）、rd2（実写合成・境界・spill・性能）、rd3（採用／不採用と後続実装の受入条件）の結果を残す。未達・未検証はそのまま明示し、製品統合完了とは表現しない。