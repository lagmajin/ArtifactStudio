# Seamless Texture Lab

**最終更新:** 2026-11-18
**ステータス:** RD0（決定論的 fixture 10 件・素の反復 baseline）と RD1（候補 3 種の出力・記述的測定・blank blind シート）まで実施。**gate（合否閾値）は未設定**、人手レビュー・順位付け・製品経路は未実施

継ぎ目のないテクスチャ生成のオフライン R&D ラボ。1 枚の画像からタイリング可能なテクスチャを合成し、継ぎ目の視認性と motif の重複を分離して評価する。製品アルゴリズムの移植ではない。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)、候補メモは `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md` の 17 番。

製品コード（Artifact / ArtifactCore / ArtifactWidgets）は変更しない。オフラインの NumPy / OpenCV / Pillow のみで完結し、Artifact バイナリは起動しない。ビルド・CMake・製品テスト・Windows GUI 操作も行わない。

## セットアップ

```powershell
python -m pip install -r requirements.txt
```

## 実行

作業ディレクトリは `experiments/SeamlessTextureLab`。

```powershell
python generate_fixtures.py --output captures/rd0
python generate_fixtures.py --output captures/rd0 --check
python rd0_baseline.py --output captures/rd0
python rd1_candidates.py --rd0 captures/rd0 --output captures/rd1
```

`--check` は `captures/rd0/fixtures_manifest.json` の SHA-256 と実ファイルを突き合わせる（再生成は不要）。

## 各段の意味

| 段 | 内容 | 出力 |
|---|---|---|
| RD0 | 決定論的 fixture（seed 20261118、512×320 と 1536×1024）と baseline（元画像をそのまま 3×3 反復） | `captures/rd0/fixtures/`・`baselines/`・`metrics_rd0.json`・`manifest_rd0.json`・`review/RD0_TABLES.md`・`review/rd0_baseline_sheet.png`・`review/GATE_FORM.md`（空） |
| RD1 | 候補 3 種の出力と記述的測定 | `captures/rd1/candidates/`・`metrics_rd1.json`・`manifest_rd1.json`・`review/RD1_TABLES.md`・`review/rd1_candidate_blind_sheet.png`・`blind_key.json`・`review/CANDIDATE_BLIND_FORM.md`（空）・`review/GATE_FORM.md`（空） |

### fixture（`generate_fixtures.py`）

checker（硬いエッジ）、radial（中心ずらしの勾配）、clouds（multi-octave ノイズ）、blobs（ランダム色块）、alpha_patch（半透明境界、straight / premultiplied の両版）。**すべて意図的にタイリング不可**にしてあり、素の反復 baseline で継ぎ目が出る。

### 候補モデル（`seamless_reference.py`）

| ID | 方式 | 特徴 |
|---|---|---|
| `st_mirror` | A: 反転タイリング | 境界連続性は構造的に保証。motif の反転による重複は避けられない |
| `st_quilt` | B: 半周期ずらし + 最小コスト継ぎ目接合 | DP 経路で継ぎ目を決める。**本実装は 2 次元オフセットの領域を分離できず、継ぎ目は残った**（実装限界） |
| `st_periodic` | C: トーラス（巡回 FFT）低域化 | 境界で連続な場が構造的に保証。代わりに質感（局所コントラスト）が落ちる |

いずれも premultiplied linear float で処理し、RGB と alpha を同時に扱い、最後に unpremultiply は一度だけ。alpha ≤ 1e-3 の領域は除算せず RGB = 0 とし、結果は 16.0 でクランプする。

### 測定項目

- 継ぎ目勾配エネルギー比（境界線 / タイル内部。1.0 で境界が内部と区別できない）
- 境界をまたぐ輝度段差・RGB 段差
- 境界モードエネルギー比（スペクトルの境界周波数側の占有率）
- motif 重複（8%〜50% ラグ帯の自己相関ピーク / 零ラグ）
- descriptor 差（straight / premultiplied 入力）、近透明の絶対値、>1 チャンネル数、非有限値数
- 処理時間・tracemalloc peak（Python heap のみ）

## 既存資産との関係・重複回避

- `experiments/RetroImageLab/`: ディザ・位相の研究。テクスチャ合成は別軸
- `experiments/MaterialSeparationLab/`: 質感分離。生成タイルの素材分解は対象外。premultiplied 空間処理と descriptor 契約の知見を継承
- `experiments/DetailSharpeningLab/`: ディテール**強調**。「継ぎ目除去」とは別軸
- `ArtifactCore/include/ImageProcessing/`: Blur / Sharpen 系は比較対象。継ぎ目除去の既存実装は無い

## 解釈の制限

- **gate（合否閾値）は設定していない。** §5.0 の手順どおり、baseline 取得後に別レビュー者が数値を記入する運用。RD1 の数値は記述的 baseline であり、候補の優劣・品質合格を示さない
- 継ぎ目勾配エネルギー比は**同一出力内の比較**であり、低域化で内部エネルギーも落ちると比は 1 に近づく。絶対的な段差値と併用すること
- alpha_patch の RGB は设计上ほぼ一様なため、継ぎ目の視覚品質評価には使わない（descriptor 契約のみ）
- 人手レビュー（blind シートの採点）は未実施。`review/CANDIDATE_BLIND_FORM.md` は空のまま
- 実写テクスチャは**未取得**。実写での品質判定は未検証
- `st_quilt` の不備は実装の単純さに起因するもので、quilt 手法そのものの否定ではない。正しい実装（オフセット領域を囲む閉じた継ぎ目）は未着手
- 実行時間は Python のオフライン reference model のみで、GPU 実装の予算ではない。HDR、OCIO、製品 preview / final、D3D12 / Vulkan parity、実アプリ経路、製品性能（1080p / 4K）、Undo / 保存・再読込は未検証
