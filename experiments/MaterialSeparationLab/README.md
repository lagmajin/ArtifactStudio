# Material Separation Lab

**最終更新:** 2026-11-18
**ステータス:** RD0（決定論的 fixture 11 件・baseline 30 出力）と RD1（候補 5 種の出力・記述的測定・blank blind シート）まで実施。**gate（合否閾値）は未設定**、人手レビュー・順位付け・製品経路は未実施

質感分離（マテリアル分離）のオフライン R&D ラボ。静止画を低周波の base（色・トーンマップ）と高周波の detail（質感・テクスチャ）に分離し、両者を独立に調整できる状態を検証する。製品アルゴリズムの移植ではない。評価計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)、候補メモは `docs/planned/MILESTONE_NEW_IMAGE_RND_IDEAS_2026-11-16.md` の 18 番。

製品コード（Artifact / ArtifactCore / ArtifactWidgets）は変更しない。オフラインの NumPy / OpenCV / Pillow のみで完結し、Artifact バイナリは起動しない。ビルド・CMake・製品テスト・Windows GUI 操作も行わない。

## セットアップ

```powershell
python -m pip install -r requirements.txt
```

## 実行

作業ディレクトリは `experiments/MaterialSeparationLab`。

```powershell
python generate_fixtures.py --output captures/rd0
python generate_fixtures.py --output captures/rd0 --check
python rd0_baseline.py --output captures/rd0
python rd1_candidates.py --rd0 captures/rd0 --output captures/rd1
```

`--check` は `captures/rd0/fixtures_manifest.json` に記録した SHA-256 と実ファイルを突き合わせる（再生成は不要）。

## 各段の意味

| 段 | 内容 | 出力 |
|---|---|---|
| RD0 | 決定論的 fixture（seed 20261118、512×320 と 1536×1024）と baseline（無処理／低域のみ／高域のみ） | `captures/rd0/fixtures/`・`baselines/`・`metrics_rd0.json`・`manifest_rd0.json`・`review/RD0_TABLES.md`・`review/rd0_*_sheet.png`・空の gate 票 |
| RD1 | 候補 5 種の出力と記述的測定 | `captures/rd1/candidates/`・`metrics_rd1.json`・`manifest_rd1.json`・`review/RD1_TABLES.md`・`review/rd1_blind_sheet.png`・`blind_key.json`・`review/CANDIDATE_BLIND_FORM.md`（空） |

### fixture（`generate_fixtures.py`）

- 8 色パッチ + 灰色ランプ（低域の色・階調と中間調の保存を測る）
- 細線チャート 1〜4px（高域の保持と halo）
- Procedural 多オクターブ value noise の微細テクスチャ（質感の抽出。平坦部とテクスチャ部を併置し局所コントラストを比べる）
- 平坦部 + ハイライト（平坦部への texture 混入、縁の halo）
- 半透明 alpha 境界（hidden RGB 含む。straight / premultiplied の両版を用意）
- 連続フレーム 8 枚（0.5px 平行移動、時間安定性用）

出力は測定用 float32 `.npy` と目視用 sRGB 8bit `.png`。`fixtures_manifest.json` に生成手順とハッシュを保存する。

### 候補モデル（`material_reference.py`）

| ID | 方式 | 特徴 |
|---|---|---|
| `ms_a_guided` / `ms_a_guided_wide` | A: 自己二層分解（guided filter） | エッジ保持の base/detail。半径で質感の粒徑が変わる |
| `ms_b_freq` | B: 周波数分離（gaussian 差分） | 単純な低域/高域分離。sigma で帯域を規定 |
| `ms_c_emphasis` / `ms_c_emphasis_boost` | C: 強調型 | base / coarse texture / micro texture を別々にスケールできる |

いずれも **premultiplied linear float** で処理し、RGB と alpha を**同じ近傍演算で同時に**層分解し、recompose した層に対応する alpha を unpremultiply の除数に使う（出力 alpha も同じ値）。最後に unpremultiply は一度だけ行う。alpha ≤ 1e-3 の領域では除算せず RGB = 0 とし（hidden RGB を捨てる）、結果は 16.0 でクランプして無限大を防ぐ。straight 入力と premultiplied 入力は bitwise 同一の premultiplied バッファを作るため、復元 straight RGB の差は 0 になる。

パラメータは `base_gain` / `detail_gain`（C は `coarse_gain` / `fine_gain`）。全 gain = 1.0 が中立設定、全 detail gain = 0 が「質感 0」の設定。

### 測定項目（RD1）

- detail 層の振幅統計（abs mean / std / p50 / p95 / p99 / max、base に対する相対量）
- 再合成誤差（`base + detail` が premultiplied 入力から再構成できること）
- 中立性（gain=1.0 で入力が再現されること。premultiplied 空間・straight 空間・sRGB8 コードの 3 通りで記録）
- 中間調ランプの保存（灰色ランプの step mean / std / min / max / 単調 fraction）
- 局所コントラスト（平坦部 luma std とテクスチャ部 luma std の比、入力 / base 層 / 出力の 3 点）
- descriptor 差（straight 入力と premultiplied 入力から復元した straight RGB の最大差）
- 近透明領域の絶対値、>1 チャンネル数、非有限値数
- 処理時間と `tracemalloc` peak（Python heap のみ）
- alpha 境界の gradient 差分プローブ（halo の手掛かり）

## 既存資産との関係・重複回避

- `experiments/DetailSharpeningLab/`: ディテール**強調**（増幅）を研究済み。本ラボは「強調」ではなく「**分離**」と両層の独立調整が対象。結果は比較対象であり、既存実装を触らない
- `experiments/WatercolorLab/`: premultiplied 空間処理と descriptor 契約の知見を継承（descriptor 差 0 でも絶対値の健全性は別問題）
- `experiments/PencilPrintLab/`: 参照の取り違えに関する測定方法的欠陥の教訓を継承。RD1 の descriptor 比較は「同じ処理を通した straight 入力」と「premultiplied 入力」を直接突き合わせる
- `experiments/RetroImageLab/`: ディザ・位相の研究。screen 固定ノイズは質感を合成する材料として再利用可能だが、分離処理とは別軸
- `ArtifactCore/include/ImageProcessing/`: Blur / Sharpen 系は比較対象。二層分解 API を製品コードへ追加しない

重複ファイルは作っていない。新規ファイルはすべて `experiments/MaterialSeparationLab/` 配下。

## 解釈の制限

- **gate（合否閾値）は設定していない。** §5.0 の手順どおり、baseline 取得後に別レビュー者が数値を記入する運用。RD1 の数値は記述的 baseline であり、候補の優劣・品質合格を示さない
- 人手レビュー（blind シートの採点）は未実施。`review/CANDIDATE_BLIND_FORM.md` は空のまま
- 実写素材（質感を持つ実画像）は未取得。実写での品質判定は未検証
- HDR、OCIO、製品 preview / final、D3D12 / Vulkan parity、実アプリ経路、製品性能（1080p / 4K）、Undo / 保存・再読込は未検証
- 実行時間は Python のオフライン reference model のみで、GPU 実装の予算ではない
