# Content-Aware Retarget 評価計画（事前登録）

**最終更新:** 2026-11-16
**状態:** rd0 実行前の事前登録。数値ゲートは候補比較の前にここで確定させ、結果を見てから変更しない。

[`../../docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../../docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md) に従う。

## 1. 目的と対象

16:9 の素材を 1:1 / 9:16 などへ**内容を保ったまま**縦横比変換する手段として、seam carving（エネルギー最小の列を反復除去）が、単純 crop / 非等方拡大より優れる条件を測る。

- 対象: 静止画 1 枚、8-bit sRGB、固定 seed の合成 fixture、CPU のみ、列方向のみの縮小（384→216）
- 対象外: 動画・フレーム間の seam 一貫性、列の**挿入**（拡大方向）、AI outpainting、saliency 自動推定、GPU 実装、製品 UI、性能予算の主張

## 2. 比較対象

| 区分 | 名前 | 役割 |
|---|---|---|
| baseline | `crop_center` | 中央 crop。内容を捨てるが幾何は完全に保つ |
| baseline | `stretch` | 非等方 LANCZOS 拡大縮小。内容は全て残るが形状が歪む |
| candidate | `seam_backward` | 後方エネルギー（勾配和）による古典 seam carving |
| candidate | `seam_forward` | forward energy（Rubinstein）。線の連続性を考慮 |
| oracle | `seam_forward_oracle` | GT subject mask をエネルギーに加算した上限確認用。**採用候補ではない** |

正解として扱うもの: 合成 fixture の**既知の subject mask** と**既知の直線位置**のみ。実写や他製品の出力を正解にしない。

## 3. 評価素材

seed 20261116、384×216、linear RGB float32。PNG は sRGB transfer で書き出す。

| fixture | 内容 | 測るもの |
|---|---|---|
| `portrait` | 左三分の一に楕円 subject（rx34/ry58） | subject 保持率、subject 縦横比の歪み |
| `facade` | 縦線12本・横線8本の格子 | 縦線の生存数と連続性 |
| `horizon` | 水平線 y=140、右端x=300 に太陽 | 太陽の保持と形状歪み |
| `wires` | 1px 幅の縦線6本 | 細線の生存数と画素保持率 |

列方向のみの変換では行の幾何が定義上保存されるため、水平線の曲がりは判別指標にならない（全候補 0 になる）。代わりに縦構造の連続性を測る。

素材はコード生成で決定的。generator と各 PNG の SHA-256 を `captures/rd0/metrics.json` に記録する。

## 4. 画像条件

入力は全て同一 fixture。設定は列除去 168 本固定。色空間は linear float32 で計算し、比較用 PNG は sRGB。alpha は扱わない（全 fixture 不透明）。出力形式は PNG（8-bit sRGB）と metrics JSON。

## 5. 評価項目と合格条件

| 項目 | 指標 | 合格条件 |
|---|---|---|
| 効果の正しさ | 出力サイズ | 全候補 216×216 |
| 画質と細部（subject） | `subject_retention` | ≥ 0.97（oracle lane は ≥ 0.99 を期待） |
| 画質と細部（形状） | `subject_aspect_error` | ≤ 0.05 |
| 構造 | `facade.vertical_lines` | ≥ 11 / 12 |
| 構造の連続性 | `facade.vertical_line_integrity` | ≥ 0.90 |
| 細線 | `wires_surviving` | ≥ 5 / 6 |
| 細線 | `wires.wire_pixel_retention` | ≥ 0.95（stretch は除去なしのため 1.0 と定義） |
| 色と alpha | 色変換の有無 | 適用（画素の再構成のみで色変換しないこと）。stretch は補間のみ |
| 時間安定性 | 対象外 | 静止画のみのため未適用。理由: 動画伝播は本 R&D の対象外 |
| 経路と解像度 | 対象外 | 製品経路・GPU は未接続 |
| 性能 | `runtime_ms` | 参考値として記録。合否には使わない（製品予算を主張しない） |

重大な欠陥（無条件に不合格）: subject を分断する seam、直線が不連続に途切れる、同一内容の重複出現。

## 6. 目視

全 fixture × 全候補の画像と `contact_sheet.png` を同条件で並べ、上記の重大欠陥の有無を確認する。目視は数値の代替にせず、数値合格でも欠陥があれば不合格とする。判定者は未定（この R&D では未実施として記録）。

## rd1（実写追補）— 2026-11-16 事前登録

合成 fixture では出ない**テクスチャ・髪・文字様の高周波構造**での破綻を確認するため、ライセンス記録済み CC0 実写3点を追加する。この追補は rd1 の**測定前に**書き、以下のゲートは結果を見て変更しない。

### 素材（出所・hash）

出所記録は各ラボの `SOURCES.md` を正とし、実行時に SHA-256 を照合して一致しなければ停止する。

| fixture | ファイル | 出所 | license | SHA-256 | 評価対象 |
|---|---|---|---|---|---|
| portrait | `experiments/CinematicLooksLab/assets/portrait-2024-1536.jpg` | [Wikimedia Commons — Portrait 2024.jpg](https://commons.wikimedia.org/wiki/File:Portrait_2024.jpg)（撮影 Louis Rivest-Henault） | CC0 1.0 | `9c14e3c0206ad6fe080c349368bf29219071892002fc01f1b6fc947af3bd40e2` | 髪、肌、窓のハイライト |
| forest | `experiments/CinematicLooksLab/assets/forest-wild-1536.jpg` | [Wikimedia Commons — Landscape of a wild forest.jpg](https://commons.wikimedia.org/wiki/File:Landscape_of_a_wild_forest.jpg)（SpessiG） | CC0 1.0 | `b417d6ffe3b494ee80906d6b433544ee7a2afa51c1557dcdf3b8feae9536ce6c` | 葉・枝の細部、深い影 |
| neon | `experiments/GlowLab/assets/cc0-neon-streets-1536.jpg` | [Wikimedia Commons — Neon streets (Unsplash).jpg](https://commons.wikimedia.org/wiki/File:Neon_streets_(Unsplash).jpg)（JINDONG H.） | CC0 1.0 | `8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4` | 看板・文字様構造、点光源 |

出所記録: `experiments/CinematicLooksLab/assets/SOURCES.md`、`experiments/GlowLab/assets/SOURCES.md`。CC0 は著作権ライセンスの記載であり、写真を見た目や測色の正解として扱わない。

### 実行記録

rd1 を実行済み。数値は `captures/rd1/metrics.json`、結果と解釈は `docs/planned/MILESTONE_CONTENT_AWARE_RETARGET_RD_2026-11-16.md` の R1 節。以下のゲート・前処理は実行前に確定し、結果を見て変更していない。

### 前処理（固定）

高さ 432 px へ Pillow LANCZOS で比例縮小（sRGB 8-bit）→ sRGB 復号して linear float32 → 幅を 432 へ列除去（1:1）。作業コピーの SHA-256 と前処理パラメータを manifest に記録する。

### GT が無いことの扱い

実写には正解 matte が無い。**subject mask を発明しない**ため、rd0 の `subject_retention` / `subject_aspect_error` は実写に適用しない（未適用と明記）。代わりに画素 identity の厳密追跡によるマスク不要指標を使う。

### 指標

- `edge_pixel_retention`: ソース勾配上位10%画素のうち、リターゲット後も残った割合（厳密）
- `thin_ridge_retention`: 左右両隣と大きく異なる1px幅構造（髪・枝・文字・細線の代理）の残存割合（厳密）
- `gradient_energy_retention`: 勾配エネルギー総和の残存割合
- `compression_run_max` / `p95`: 1行で連続して消えたソース列数の最大・95%点（局所的な潰れ）
- `seam_jaggedness_mean` / `max`: 隣接行間の seam 横移動量（px）
- `anisotropy`: 横縮小率と縦縮小率の比（stretch は 1.5 になる）

### ゲート（rd1）

| ID | 条件 |
|---|---|
| P1 | `edge_pixel_retention` ≥ 0.95 |
| P2 | `thin_ridge_retention` ≥ 0.95 |
| P3 | `gradient_energy_retention` ≥ 0.95 |
| P4 | `seam_jaggedness_mean` ≤ 1.0 px かつ `max` ≤ 6 px |
| P5 | `compression_run_max` ≤ 12 px |
| P6 | `anisotropy` ≤ 1.02 |
| 重大欠陥 | 同一内容の重複出現、直線構造の不連続な途切れ、顔・文字の判読不能な破壊 |

3素材すべてで P1〜P6 を満たす lane のみ「実写で破綻が確認されなかった」と記す。1素材でも落ちれば不合格とし、不足を未達として残す。

### rd1 の対象外

実写の主観画質、GT matte、saliency 自動推定、拡大方向、動画一貫性、GPU/製品経路、性能予算。目視は数値判定の代替にしない（未実施なら未実施と記録する）。

## rd2（局所化指標の再設計）— 2026-11-16 事前登録

### なぜ作り直すか

rd1 の P5（`compression_run_max` ≤ 12 px）は **全 lane が不合格**になり、判別力が無かった。seam carving は 216 本の除去を低エネルギー帯に集中させるため連続除去長が 80〜123 px になるが、`removed_gradient_ratio` 0.04〜0.15 が示す通りそれは**意図された背景圧縮**であり、破壊的圧縮と区別できない。連続長は「どこに集中したか」しか見ておらず、「そこが潰してよい内容だったか」を見ていない。

P5 の rd1 結果は履歴として保持し、削除しない。以下の P7〜P9 を追加し、**P5 は合否に使わない**（廃止の提案であり、承認者は未定）。

### 指標（16×16 ブロック単位）

ソースを 16 px ブロックに分割し、各ブロックのソース勾配エネルギーで四分位に分類する。

- `compression` = ブロック内で除去された画素の割合
- `energy_retention` = ブロック内で残存した勾配エネルギー ÷ 元の勾配エネルギー
- **Q4** = エネルギー上位25%のブロック（詳細・構造がある領域）
- **Q1** = 下位25%（平坦・空・ぼけ領域）

### ゲート（rd2）

| ID | 条件 | 意味 |
|---|---|---|
| P7 | Q4 `compression` の 95%点 ≤ 0.25 | 詳細領域を潰していない |
| P8 | `energy_retention`（Q4）≥ 0.90 | 詳細領域の構造エネルギーを保つ |
| P9 | `selectivity` = Q1 平均 compression ÷ Q4 平均 compression ≥ 3.0 | 除去が平坦部に偏っている |

`stretch` は除去が無いため P7〜P9 は未適用（P6 の異方性で落ちる）。

### 実行記録

rd2 を実行済み。数値は `captures/rd2/metrics.json`、結果は `docs/planned/MILESTONE_CONTENT_AWARE_RETARGET_RD_2026-11-16.md` の R2 節。ゲートは実行前に確定し、結果を見て変更していない。

### 指標の妥当性確認

既知の失敗ケース（合成 `wires`: 平坦背景上の孤立 1px 線）で P7〜P9 が **不合格になること**を確認する。合格してしまう指標は判別力が無いとみなし、その事実を記録する。これは指標の自己検証であり、実写の画質判定ではない。

### rd2 の対象外

目視、GT matte、saliency 自動推定、拡大方向、動画一貫性、GPU/製品経路、性能予算。ブロックサイズ・四分位の境界は固定し、結果を見て変更しない。

## rd3（P7/P9 の閾値校正と rd2 の再測定）— 2026-11-16 事前登録

### 問題

rd2 の P7（Q4 除去率 p95 ≤0.25）と P9（selectivity ≥3.0）は**一回の推測で決めた値**で、測定に基づく根拠がない。0.25 も 3.0 も、どの除去量で詳細が壊れるかを測っていない。本節は閾値を**校正**し、その値で rd2 を測り直す手順を事前に定める。rd2 の結果は履歴として保持する。

### A. 校正（対照 lane の測定）

同じ fixture 同じ前処理に、除去量を変えた対照 lane を追加する。

| lane | 内容 |
|---|---|
| `none` | 除去 0%。サニティ確認（除去率0、保持1.0 になるべき） |
| `decimate` | **内容を見ない一様除去**。列を等間隔で k 本消す。選択性のない基準線 |
| `seam_backward` / `seam_forward` | 同じ k 本を seam carving で除去 |

除去量 k: 幅の **0%、10%、20%、30%、最大**（合成 43.75%、実写 33.3%）。各条件で rd2 と同じ16px ブロック指標と、合成 fixture の**既知構造ゲート**（rd0 と同じ: wires ≥5/6、facade 縦線 ≥11/12、portrait subject 保持 ≥0.97 かつ縦横比誤差 ≤0.05）を測る。

### B. 閾値の導出規則

1. **既知破綻の境界を測る。** 合成 fixture で既知構造ゲートが初めて不合格になる除去量と、そのときの Q4 指標値を記録する。合格している構成の最大値と不合格の最小値を並べ、重なりがなければその間を閾値にする。重なる場合は「単一閾値では分離できない」と結論し、分離できないことを欠陥として記録する。
2. **P9 は絶対値ではなく基準線に対する比で校正する。** `decimate` は選択性が無いので selectivity ≈ 1 になるはず。seam の selectivity は「一様除去の何倍、平坦部に偏らせたか」として読み、3.0 という推測値を捨てる。
3. **P7 の p95 は合否に使わない。** p95 は除去の集中度であって画質ではない。校正データで p95 と既知破綻の対応が取れない場合は診断値に降格する。
4. 導出した値は、再測定の**前に**本節へ追記して固定する。導出後も推測に頼る項目は「政策的選択」と明記し、品質事実として扱わない。

### C. 導出結果（2026-11-16、再測定前に記録）

校正データは `captures/rd3/metrics.json`。まずサニティ: `none` lane は Q4 除去率0・保持1.0、`decimate` は Q4 平均除去率 ≈ 要求除去率（0.09 / 0.20 / 0.30 / 0.44）で selectivity ≈ 1.0。指標の定義どおりに振る舞うことを確認した。

既知構造ゲートの合否と Q4 指標の対応（合成3点、全条件）:

| fixture | 合格側 Q4 エネルギー保持 | 不合格側 Q4 エネルギー保持 | 分離 |
|---|---|---|---|
| wires | 1.000, 0.891, 0.792 | **1.000, 0.998**, 0.778, 0.721, 0.669, 0.636, 0.417 | 不可（不合格側に保持1.000がある） |
| facade | 1.000, 0.953, 0.917, 0.915, 0.898, 0.865, 0.804, 0.802, 0.699, **0.455** | 0.820, 0.738, 0.574 | 不可（0.574 が不合格で 0.455 が合格） |
| portrait | 1.000, 0.994, 0.986, 0.985, 0.977, 0.976, 0.969, 0.967 | 0.902, 0.803, 0.677, 0.546 | **可**（境界 0.902〜0.967） |

Q4 平均除去率でも同じ重なりが出る（wires: 不合格側 0.046 < 合格側 0.202）。

**結論（規則 B.1 の「重なる」ケース）: 単一の絶対閾値は導出できない。** 理由は閾値の選び方ではなく、ブロック指標が既知破綻を予測しないこと。具体例:

- wires は除去 10% の時点で **6本すべて消えている**のに、そのときの Q4 エネルギー保持は **0.9998**（ほぼ無傷と報告）。
- facade は seam_backward 44% で 12本中7本（保持 0.574）だが、decimate 44% は 12本すべて残って 0.455。
- portrait クラスだけは 0.93 前後で分離する（最大不合格 0.902、最小合格 0.967）。

旧 P7（p95 ≤0.25）が不当であることも校正で確定した。**破綻ゼロの構成を不合格にする**: facade seam_forward 44% は縦線 12/12 を保つのに p95 0.812、decimate 33% も p95 0.375。旧 P8（≥0.90）は portrait の decimate 10%（subject 保持 0.899 で不合格）を保持 0.902 で**合格させてしまう**。

### C-2. 導出した扱い

1. **P7 / P9 は合否ゲートから退役**。診断値（集中度・selectivity）としてのみ記録する。
2. **P8 も普遍ゲートとしては退役**。portrait 型の内容に限れば校正境界 0.93 が使えるが、facade（0.802 で 12/12）と wires（1.000 で 0/6）に適用すると誤判定する。内容クラスを事前に知らない限り使わない。
3. 代わりに**同一除去量の `decimate` に対する比**を報告する。これは品質判定ではなく「内容を見ない除去よりどれだけ詳細を避けたか」という選択性の測定で、閾値は設けない（3割除去の実写で 0.05〜0.47、合成線画で 0.79〜0.93）。
4. 細線・格子のように既知構造がある内容では、ブロック指標を使わず**直接カウント**する（wires 本数、facade 縦線本数）。本 R&D の合成 fixture はこの経路で判定する。

### D. 再測定

導出した閾値で、最大除去量の `seam_backward` / `seam_forward` を実写3点と合成4点について再判定する。比較対象は同じ除去量の `decimate` と `crop_center`。

### 対象外

目視、GT matte、saliency、拡大方向、動画、GPU/製品経路、性能予算。閾値導出後に素材を追加しない。

## rd4（複合リターゲット）— 2026-11-16 事前登録

### 目的

純粋な seam carving の弱点は、rd3 で「局所テール（Q4 除去率 p95 が 0.40〜0.62）と素材依存（selectivity 2.85〜40.4）」として測定された。削減量の一部を scale（一律圧縮）や crop（低エネルギー帯の切除）に配分すれば、seam が担当する列数が減り、テールと素材依存が改善するかを測る。

### 配分の定義

全削減量 R = W − 出力幅を 3 演算子に配分する。

1. `scale` : 横方向のみ係数 s で LANCZOS 再サンプル（縦は 1.0）。`anisotropy = 1/s`。
2. `crop` : 連続した幅を1か所切除。位置は**中間画像の列エネルギー和が最大になる窓**（内容を見る crop）。
3. `seam` : 残りを forward energy の seam carving で除去。

探索 grid: s ∈ {1.00, 0.90, 0.85} × `crop_share` ∈ {0.0, 0.5}（scale 後の残差に対する割合）。加えて対照として `stretch_only`（s = 出力幅/入力幅、seam なし）と `seam_only`（s=1, crop=0、= rd4 grid の (1.00, 0.0)）。

### 選択規則（評価前に固定）

- **制約**: `anisotropy ≤ 1.18`（s ≥ 0.85）。
- **目的関数**: `structure_correlation` の最大化。
- `structure_correlation` は、出力と同じ幅へ LANCZOS で縮小したソース（= `stretch_only` の出力）との、勾配マグニチュードの Pearson 相関。位置合わせを揃えた上での「構造がどこにどれだけ残ったか」の指標で、再サンプル自体を罰しない。
- **循環を避ける**: 選択には correlation のみを使い、局所テール（Q4 p95）と既知構造カウントは評価用の独立証拠として報告する。

### 評価指標（同一 fixture・同一対照）

- `anisotropy`、`structure_correlation`（全 config で共通）
- 中間画像（scale 後）に対する **crop+seam の局所テール**: Q4 除去率 mean / p95（rd2 と同じ 16px ブロック定義）
- 合成 fixture の既知構造: wires 生存数、facade 縦線本数と幅、portrait subject 保持と縦横比誤差
- 対照: rd3 の `decimate`（全削減量を等間隔除去）と同じ数値と並べる
- `material dependence`: 実写3点における Q4 p95 と structure_correlation の **max−min 幅**。狭いほど素材依存が小さい。

### ゲート（新規に設けない）

rd3 でブロック指標の絶対閾値は廃止済み。rd4 は**比較と幅の報告**を行い、合否ゲートは設けない。ただし前回の P6（anisotropy ≤1.02）は scale を含む config には適用できないため、上記の 1.18 を制約として使う。

### 実行記録

rd4 を実行済み。数値は `captures/rd4/metrics.json`、結果と判断は `docs/planned/MILESTONE_CONTENT_AWARE_RETARGET_RD_2026-11-16.md` の R4 節。

実行後の訂正 2 点（ゲート・grid・選択規則は変更していない）:

- `structure_correlation` は水平変位を強く罰するため、目的関数として不適切だった。目的関数を差し替えると選択が変わり、forest では相関最大化がテール非改善の構成を選ぶ。rd4 の選択結果は推奨として扱わない。
- 合成 `wires` は列エネルギーが同点多数で、異なる配分（s1.00_c0.0 と s1.00_c0.5）が**同一出力バイト列**になった。この fixture の配分比較は無効。

### 対象外

目視、saliency 保護、拡大方向、動画、GPU/製品経路、性能予算、thirds 以外の配分（縦方向 scale、複数 crop 窓）。

## 7. 実行条件

CPU のみ、Python 3.14 / numpy / Pillow。GPU・D3D12・Vulkan は使わない。処理時間は同一プロセス内の実測で、他プロセス負荷の影響を受ける。

## 8. 既知の制限

- fixture は合成でノイズが無く、実写のテクスチャ・髪・文字を含まない。
- 列方向の縮小のみ。拡大方向（seam insertion）は測らない。
- `seam_forward_oracle` は GT mask を与えた上限であり、saliency 検出の実現性を示さない。
- `stretch` は uint8 経由のため量子化誤差を含む。
