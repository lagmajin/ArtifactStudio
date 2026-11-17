# Retarget Lab

**最終更新:** 2026-11-16
**ステータス:** rd0（合成4 × 5 lane）、rd1（CC0 実写3点 × 4 lane）、rd2（局所化指標の初版）、rd3（対照 lane による校正と再測定）、rd4（scale/crop/seam の複合配分）を実行。ブロック指標は破綻予測に使えず、複合化はテールを下げるが素材依存は変わらない。目視判定・拡大方向・動画一貫性・製品経路は未実施

アスペクト比変換（16:9 → 1:1 など）の R&D ラボ。crop / 非等方拡大 / seam carving を、既知の subject mask と既知の直線を持つ決定的 fixture で比較する。

製品コードは import しない。ここで得た数値は製品品質や採用可否を証明しない。

## 実行

```powershell
# rd0: 合成 fixture
python experiments/RetargetLab/retarget_study.py

# rd1: CC0 実写 fixture
python experiments/RetargetLab/photo_study.py

# rd2: 除去の局所性（16px ブロック × エネルギー四分位）
python experiments/RetargetLab/localization_study.py

# rd3: 除去量 0/10/20/30/最大の対照 lane による閾値校正
python experiments/RetargetLab/calibration_study.py

# rd4: scale + crop + seam の配分 grid
python experiments/RetargetLab/composite_study.py
```

rd0 の出力は `captures/rd0/`、rd1 は `captures/rd1/`（作業コピー、候補ごとの PNG、`metrics.json`、contact sheet）。rd2 / rd3 / rd4 は数値のみ（`metrics.json`）を書き、PNG は保存しない。rd0 の seed は 20261116 固定。

rd1 の素材は複製せず既存ラボの作業コピーを参照し、実行時に SHA-256 を照合する（不一致で停止）。出所は [`SOURCES.md`](SOURCES.md)。

## 評価

事前登録した計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)。数値ゲート・対象外・既知の制限はそちらを正とする。

- baseline: `crop_center`（内容を捨てる）、`stretch`（形状が歪む）
- candidate: `seam_backward`、`seam_forward`
- oracle: `seam_forward_oracle`（GT mask をエネルギーに加算した上限。採用候補ではない）

計測は subject 保持率（除去画素の identity を厳密追跡）、subject 縦横比誤差、縦線の本数と幅、細線の生存数と画素保持率、実行時間。

## rd0 結果（要約）

| fixture | crop | stretch | seam_backward | seam_forward | oracle |
|---|---|---|---|---|---|
| portrait subject 保持率 | 0.621 | n/a | 1.000 | 1.000 | 1.000 |
| portrait 縦横比誤差 | 0.402 | 0.436 | 0.006 | 0.006 | 0.006 |
| facade 縦線（幅） | 6（3.0） | 12（1.0） | 7（2.0） | 12（2.0） | 12（2.0） |
| horizon 太陽保持率 | 0.482 | n/a | 1.000 | 1.000 | 1.000 |
| wires 生存（画素保持） | 4（0.667） | 6（1.000） | **0（0.000）** | **0（0.000）** | 6（1.000） |

## rd1 結果（実写、高さ432 → 1:1）

| plate | lane | edge 保持 | 1px ridge 保持 | 勾配エネルギー | 最大連続除去 |
|---|---|---|---|---|---|
| portrait | crop | 0.599 | 0.913 | 0.635 | 108 |
| portrait | seam_forward | 0.977 | 0.990 | 0.936 | 80 |
| forest | crop | 0.801 | 0.670 | 0.775 | 108 |
| forest | seam_forward | 0.990 | 0.968 | 0.932 | 123 |
| neon | crop | 0.940 | 0.937 | 0.919 | 108 |
| neon | seam_forward | 0.999 | 1.000 | 0.980 | 106 |

`stretch` は除去が無いため保持指標は未適用（異方性 1.50）。

- 合成 `wires` で見えた 1px 細線の全滅は**実写では再現しなかった**（髪・枝・文字様構造の保持 0.968〜1.000）。合成 fixture は平坦背景上の孤立線という縮退ケースだった。
- ただしテクスチャ画像では勾配エネルギーが約 6.7% 落ち、事前登録の P3（≥0.95）に届かない。
- seam の除去はほぼ低エネルギー画素に限られる（除去画素の勾配比 0.04〜0.15）。一方で除去が低エネルギー帯に集中するため、最大連続除去 80〜123 px になる。

3 素材すべてで P1〜P6 を満たす lane は無い。文字の可読性・顔・髪の見た目は**目視未実施**で、数値は画質合格ではない。

## rd2 結果（除去の局所性）

rd1 の P5（最大連続除去長）は全 lane 不合格で判別力が無かったため、「詳細ブロックを潰したか」を見る指標に置き換えた。16×16 ブロックをソース勾配エネルギーで四分位に分ける。

| fixture | lane | Q4 除去率 p95 | selectivity | Q4 エネルギー保持 |
|---|---|---|---|---|
| synthetic wires（既知の失敗） | seam_forward | 1.000 | 1.45 | **0.635** |
| portrait | seam_forward | 0.604 | 2.85 | 0.966 |
| forest | seam_forward | 0.403 | 5.72 | 0.976 |
| neon | seam_forward | 0.135 | 40.37 | 0.998 |

> **rd3 で訂正:** この「既知の失敗ケースを検出できた」という結論は誤りだった。除去量を下げると、wires は 10% で 6本すべて消えているのに Q4 保持 0.9998 を報告する。44% の低値は偶然で、指標は破綻を予測していない。

実写ではグローバル保持が良好でも、portrait / forest は詳細ブロックの 5% が 40〜60% 除去される。neon のみ全ゲート合格で、素材依存が強い。

## rd3 結果（閾値の校正と rd2 の再測定）

除去量 0 / 10 / 20 / 30 / 最大の `none` / `decimate`（内容を見ない等間隔除去）/ `seam` を同一 fixture で測り、既知構造ゲート（wires 本数、facade 縦線、subject）と対応づけた。

- サニティ: `none` は除去0・保持1.0、`decimate` は Q4 平均除去率 ≈ 要求除去率、selectivity ≈ 1.0。
- **単一の絶対閾値は導出できない。** wires は 10% 除去で 6本全滅（Q4 保持 0.9998）、facade は 12本すべて残って Q4 保持 0.455（decimate 44%）と、合格側と不合格側の値が重なる。
- 旧 P7（p95 ≤0.25）は**破綻ゼロの構成を落とす**（facade seam_forward 44% は 12/12 維持で p95 0.812）。旧 P8（≥0.90）は**破綻する構成を通す**（portrait decimate 10% は subject 0.899 不合格、Q4 保持 0.902）。

再測定（最大除去量、同一条件で rd2 と数値一致）:

| fixture | seam Q4 除去率 | decimate | 比 | seam Q4 保持 |
|---|---|---|---|---|
| portrait | 0.157 | 0.333 | 0.47 | 0.966 |
| forest | 0.105 | 0.335 | 0.31 | 0.976 |
| neon | 0.017 | 0.332 | 0.05 | 0.998 |
| synthetic facade | 0.406 | 0.438 | 0.93 | 0.802 |
| synthetic wires | 0.393 | 0.438 | 0.90 | 0.635 |

実写では同じ 33% 除去で詳細ブロックの除去を内容無視の 5〜47% に抑えられている。これが数値的に支持できる唯一の優位。合成の線画・格子では比 0.79〜0.93 で優位はほぼ無い。

P7/P8/P9 は合否ゲートから退役（診断値のみ）。

## rd4 結果（複合配分）

削減量を scale（s ∈ {1.00, 0.90, 0.85}）× crop（残差の 0 / 50%、内容を見る連続窓）× seam に配分して、純粋 seam と比較した。

| plate | pure seam テール | s0.85 + seam | 異方性 |
|---|---|---|---|
| portrait | 0.604 | 0.500（−17%） | 1.18 |
| forest | 0.403 | 0.190（−53%） | 1.18 |
| neon | 0.135 | 0.000（−100%） | 1.18 |

- **局所テールは下がる**が、素材による差が大きい（−17%〜−100%）。
- **素材依存は改善しない**: テール幅は 0.469 → 0.500（crop 併用 0.491）。correlation の幅もほぼ不変。
- **crop の寄与は小さい**: facade の縦線を 12→10 に減らし、portrait の correlation を下げる。
- 代償は幾何の歪み: 異方性 1.18、合成 subject の縦横比誤差 0.006→0.1396。
- `structure_correlation` は水平変位を罰するため目的関数として不適切で、目的関数を変えると選択が変わる。**rd4 の選択結果は推奨ではない。**
- 合成 `wires` は列エネルギーが同点多数で、異なる配分が同一出力になった。配分比較には使えない。

詳細は [`../../docs/planned/MILESTONE_CONTENT_AWARE_RETARGET_RD_2026-11-16.md`](../../docs/planned/MILESTONE_CONTENT_AWARE_RETARGET_RD_2026-11-16.md)。

## 未実施

- 実写素材、文字・顔・髪を含む素材
- 列の挿入（拡大方向）、行方向、動画・フレーム間の一貫性
- saliency の自動推定（oracle lane は GT mask 前提）
- GPU / 製品レンダー経路、性能予算
- 目視判定者によるブラインド比較
