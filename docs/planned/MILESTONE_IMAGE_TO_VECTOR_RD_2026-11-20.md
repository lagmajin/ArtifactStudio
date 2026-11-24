# 画像→ベクター変換（Image-to-Vector / Tracing）R&D

**最終更新:** 2026-11-20
**ステータス:** In Progress（R0 / rd0 実行済み。7 fixture の identity 一致、制御／baseline 2 方式の計測、ゲート確定まで完了。候補比較・人手視覚判定・製品経路は未実施）
**マイルストン ID:** M-IMG-VEC-RD-1
**ラボ:** `experiments/VectorTraceLab/`

## 目的

ロゴ・イラストなどのラスタ画像を、輪郭と色面に分解して拡大可能なベクタ形状にする処理について、
**再現可能な合成 fixture と事前登録したゲートで品質を評価する**。本 R&D はオフラインの CPU 実験であり、
トレース機能の製品実装、既存 `ArtifactSvgLayer` / Shape 系の変更、既定値・UI の追加を承認しない。

候補メモ [`MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md`](MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md) の
候補 19「画像からベクター形状への変換」を起案したものである。候番 1〜5・10〜11・13〜14 に初期資産があるのに対し、
19 番は未着手だったため、当該メモの「優先候補は上から順に検討する」に基づき 6〜18 を飛ばして着手する理由をここに記録する。
理由: シェイプレイヤー優先方針（AGENTS.md「開発優先方針」）と直接結びつき、既存の ShapePath / MaskPath /
カラー量子化資産を評価対象として再利用でき、新規依存なしで決定論的 fixture を作れるため。

## なぜ今か / 先行資産との関係（重複監査結果）

着手前に `docs/INDEX_GENERATED.md`、関連ソース、関連マイルストンを確認した（静的確認であり実キャプチャは未取得）。

- **直接の先行仕様**: [`MILESTONE_AUTO_TRACE_2026-06-02.md`](MILESTONE_AUTO_TRACE_2026-06-02.md)（M-MOTION-6）。
  `cv::findContours` + `approxPolyDP`、チャンネル／しきい値 UI、RDP + Bezier 近似、
  Shape／Mask 出力を想定するが、2026-08-15 監査で `ArtifactCore/src/Trace/AutoTrace.cppm` は存在せず
  Phase 1〜3 は未着手。**本 R&D はこの未実装機能の実装ではなく、そのアルゴリズム選択を評価する。**
- **実質的な既存 tracer**: `Artifact/src/Mask/MaskPath.cpp` の `MaskPath::fromAlphaMask()`
  （しきい値 127 / 0.5 → `RETR_CCOMP + CHAIN_APPROX_NONE` → `approxPolyDP` → hierarchy で外側 Add / 穴 Subtract →
  正規化座標 → コーナー判定と 1/6 接線による平滑化）。`Artifact/include/Mask/MaskPath.ixx` に
  `fromAlphaMask` / `fromQPainterPath` / `toShapePath` / `fromShapePath` がある。
  パラメータは `simplificationTolerance=1.5 / cornerThreshold=10.0 / minPathVertices=6 / closedPath=true` で、
  しきい値は固定（[`MILESTONE_MASK_ALPHA_ROUNDTRIP_LITE_2026-09-10.md`](MILESTONE_MASK_ALPHA_ROUNDTRIP_LITE_2026-09-10.md) 契約）。
- **出力先の幾何 API**: `ArtifactCore/include/Shape/ShapePath.ixx` + `src/Shape/ShapePath.cpp`
  （`moveTo/lineTo/cubicTo/quadTo/setPolygon`、`toSegments`、`triangulate` は穴・fill rule 対応）、
  `ShapeTypes.ixx`、`ShapeGroup.ixx`、`ShapeLayer.ixx`。生成経路の契約は
  [`SHAPEPATH_GEOMETRY_CONTRACT_2026-07-27.md`](../technical/SHAPEPATH_GEOMETRY_CONTRACT_2026-07-27.md) を参照する。
- **色面分解の候補**: `ArtifactCore/include/Analyze/SmartPaletteAnalyzer.ixx`（100×100 リサイズ + `cv::kmeans`）、
  `PosterizeCV`、`experiments/RetroImageLab` の median-cut / palette 研究。RD では Core 式の mirror を
  オフラインで比較し、Core の変更は行わない。
- **既存の svg 経路との違い**: `ArtifactSvgLayer` は nanosvg による bitmap-only ラスタライズであり、
  編集可能なパス生成ではない（M-AS-4b の vector 編集は未完了、[`MILESTONES_BACKLOG.md`](MILESTONES_BACKLOG.md) 参照）。
- potrace / autotrace / marching squares の実装はリポジトリに存在しない。OpenCV は Artifact / ArtifactCore の既存依存であり、
  新規サードパーティ依存は不要。

## 評価計画

画像品質を扱うため、開始前に [`experiments/VectorTraceLab/EVALUATION_PLAN.md`](../../experiments/VectorTraceLab/EVALUATION_PLAN.md)
へ [`IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md) 準拠の評価計画を登録した。
要点: 合成ベクタを正解とする区域分解・輪郭幾何・穴・細線・alpha 境界・色の指標、固定 seed / 解像度 / 色空間、
事前登録ゲート、実行条件、証跡形式。ゲートは rd0 の baseline 実測後に確定し、rd1 の候補比較より前に計画へ反映する。

## 対象 / 非対象

| 区分 | 内容 |
|---|---|
| 対象 | 既知のベクタ定義から生成した合成ロゴ／イラスト（平坦色・穴・細線・半透明・階調エッジ区域） |
| 対象 | オフライン Python での color 量子化 → 領域分割 → 輪郭／Polygon 近似 → ShapePath 相当データの生成 |
| 対象 | 再ラスタライズによる区域 IoU、境界距離、穴保持、細線再現、色差、alpha 合成、頂点数、処理時間 |
| 対象外（未検証） | 実ロゴ／実イラスト（rd2 で external fixture として追加しても正解とは呼ばない） |
| 対象外（未検証） | Artifact GPU / Diligent 経路、Composition Viewer、Render Queue、preview / final の差、backend parity |
| 対象外（未検証） | 動画・連番の一貫性、学習モデル、人手による美的品質判定 |
| 対象外 | 製品 UI（Auto-Trace ダイアログ等）、Undo、ショートカット、既定値、Effect カタログ変更 |

## 進め方（R0〜R3 / rd0〜rd3）

| 段階 | 内容 | 状態 |
|---|---|---|
| R0 (rd0) | 決定的 fixture 生成、正解ベクタの再ラスタライズ整合、baseline 2 方式の計測、ゲート確定、証跡整備 | 完了（2026-11-20。identity 差 0、7 fixture×2 方式を計測、ゲート確定） |
| R1 (rd1) | rd0 で確定したゲートのみで第 1 候補（Color 量子化 + 輪郭近似のバリエーション）を比較。ゲートを事後緩めない | 未着手 |
| R2 (rd2) | 条件拡張: 実ロゴ／イラスト fixture、alpha・階調エッジ、解像度違い、細線ストレス、処理時間 | 未着手 |
| R3 (rd3) | Bezier 近似の採用判断、ShapePath 契約との照合、後続実装の受入条件。または不採用判断 | 未着手 |

rd0 は「調査開始のための準備・baseline 取得」であり、品質達成の根拠にしない。各 run は
`experiments/VectorTraceLab/captures/rdN/` に manifest・metrics・gates・レビュー画像を保存する。

## 合格ゲート（概要）

詳細は `EVALUATION_PLAN.md` の確定済み表を正とする。rd0 baseline 実測後に確定した値である。

| 指標 | 領域 | 確定ゲート |
|---|---|---|
| fixture 再現性 | 全 fixture | 同一 seed で再生成した source の最大絶対差 = 0（rd0 実測 0.0） |
| 区域 IoU | 可視領域 | 平均 ≥ 0.90、最小 ≥ 0.70 |
| 境界距離 | IoU≥0.5 のマッチのみ | 対称 chamfer の p95 ≤ 3.0 px（512 px 幅基準） |
| 穴保持 | `nested_holes` / `logo_badge` | 正解の穴を 50% 以上覆う出力穴が全件あること |
| 細線再現 | `thin_strokes` | 正解細線ピクセルの recall ≥ 0.80 |
| 色差 | 各領域の平均色 | 平均 ΔE76 ≤ 3.0 |
| alpha 境界 | 黒／中灰／白への合成 | ±1 px 位置合わせ後の p95 ≤ 60/255 code |
| 解像度 | 256 / 1024 | 幅で正規化した chamfer の変化 ≤ 25% |
| 性能 | 512×320、3 回実行 | ゲートなし（記述のみ。製品性能の主張をしない） |

rd0 の主な知見: naive な「色量子化＋輪郭＋1 段の穴」は、`ring` の穴の内側に `star` がある入れ子トポロジーを潰し、
`logo_badge` / `nested_holes` で境界距離が 16〜23 px に悪化した。細線は recall 0.898 でゲート通過したが IoU は 0.278 に留まる。
半透明領域は k-means が合成後の色を返すため color ΔE が 21.8 となり色ゲートを失敗した。これらは rd1 の候補が解くべき課題である。

数値改善のみで視覚品質を合格としない。人間レビュー未実施の項目は合格扱いにせず「未検証」と記録する。
ゲート改訂は理由・前後値・決定日を manifest と `EVALUATION_PLAN.md` に残し、影響する比較をやり直す。

## 非対象（変更禁止事項）

- ArtifactCore / Artifact / ArtifactWidgets のソース変更、新規 `.ixx` / `.cppm`、CMake 編集。
- 新規 signal/slot、グローバルイベント配線、QtCSS、`QColorDialog`、`QImage` / `QPainter` による新規描画・合成。
- GPU / Diligent / shader の変更、ホットパスへの負荷持ち込み。
- ビルド・CMake 再実行・製品テスト・実アプリの runtime capture（ユーザーの明示指示まで禁止）。
- 生成画像・配列の無条件 tracking。`experiments/*/captures/` は既定 ignore。選んだ最終証跡だけを条件・日付・出所・hash 付きで個別 `git add -f` する。

## 成果物

- `docs/planned/MILESTONE_IMAGE_TO_VECTOR_RD_2026-11-20.md`（本书）
- `experiments/VectorTraceLab/README.md`、`EVALUATION_PLAN.md`、`requirements.txt`、`.gitignore`
- `experiments/VectorTraceLab/generate_fixtures.py`、`vector_reference.py`、`run_rd0.py`
- `experiments/VectorTraceLab/captures/rd0/`（manifest.json / manifest.sha256、metrics.json、gates.json、
  fixtures/、review 用 PNG と ROI crop。生成物は既定 ignore）
- `experiments/VectorTraceLab/RESEARCH_NOTES.md`（実行所見。実行後に追記）

## 完了条件

必須評価の結果・合否と根拠・未解決事項・採用／不採用判断・後続実装の受入条件を残す。
未検証経路（人手視覚判定、製品 runtime、GPU parity、実素材の正解）はそのまま明示し、
R&D 完了を「画質達成」や「製品統合完了」と表現しない。

## 関連資料

- [`IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md)
- [`MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md`](MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md)（候補 19）
- [`MILESTONE_AUTO_TRACE_2026-06-02.md`](MILESTONE_AUTO_TRACE_2026-06-02.md)（直接の先行仕様）
- [`MILESTONE_MASK_ALPHA_ROUNDTRIP_LITE_2026-09-10.md`](MILESTONE_MASK_ALPHA_ROUNDTRIP_LITE_2026-09-10.md)
- [`SHAPEPATH_GEOMETRY_CONTRACT_2026-07-27.md`](../technical/SHAPEPATH_GEOMETRY_CONTRACT_2026-07-27.md)
