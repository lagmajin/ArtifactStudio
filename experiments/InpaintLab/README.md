# Inpaint Lab — Content-Aware Fill R&D

**最終更新:** 2026-11-17

画像エフェクト R&D「Content-Aware Fill（inpaint）」の評価用 fixture ラボ。
評価計画は `docs/planned/MILESTONE_INPAINT_CONTENT_AWARE_FILL_RD_2026-11-17.md`。

## 必要性

inpaint は現行リポジトリに実装がなく（`MILESTONE_CONTENT_AWARE_FILL_2026-07-08.md` が
DRAFT／未着手を維持）、実装前に画像評価計画を定めることが必須ルール。
このラボは実アプリに依存しない合成 fixture と、測定用の元データを作る。

## 必要なもの

- Python 3
- NumPy
- OpenCV（`opencv-python` または同等の `cv2`）

## 実行

リポジトリルートから:

```powershell
python experiments/InpaintLab/build_inpaint_fixtures.py
python experiments/InpaintLab/build_inpaint_fixtures.py --output-dir experiments/InpaintLab/captures/custom-rd1 --seed 20261117
```

出力（既定）: `experiments/InpaintLab/fixtures/inpaint-rd1/`

- `manifest.json` — generator バージョン、seed、ライブラリバージョン、
  色／alpha 契約、全ファイルの SHA-256、fixture の目的、制限事項。
- 各 fixture ディレクトリ:
  - `source.npy` — 不要物入りの入力。float32 RGBA、linear scene-referred、straight alpha。
  - `mask.npy` — uint8、255 = 補完対象。
  - `ground_truth.npy` — 不要物を入れる前のクリーンプレート。float32 RGBA。
  - `source.png` / `mask.png` / `ground_truth.png` — 目視用。表示用に 0–1 クランプ＋sRGB OETF 済み。

`alpha_edge` にはさらに `source_composite_{black,gray,white}.png` と
`ground_truth_composite_{black,gray,white}.png` を置く。

## ベースライン統計

```powershell
python experiments/InpaintLab/analyze_inpaint_baseline.py
```

出力: `experiments/InpaintLab/captures/baseline-rd1/baseline-metrics.json` と静止 fixture の contact sheet（source ｜ ground truth ｜ 4 倍増幅の差分）。
数値は inpaint 未適用の baseline であり、合格閾値ではない。

## 候補比較（R2）

```powershell
python experiments/InpaintLab/compare_inpaint_candidates.py
```

出力: `experiments/InpaintLab/captures/candidates-rd2/candidate-metrics.json`、
候補別 contact sheet、`visual-review-rd2.json`。
OpenCV の対応形式に合わせ、8-bit sRGB 往路と per-channel float32 を別経路として比較する。

## データ契約

- **測定には NPY を使う。** PNG は表示用であり、HDR fixture の値はクランプされる。
- RGB は linear scene-referred。表示変換は sRGB OETF のみで、Artifact の active view
  transform は使わない。
- alpha は straight。`alpha_edge` の inpaint 領域で期待される alpha の扱いは未確定。
- 静止 fixture は 256×256、sequence は 160×160×8 フレーム。
- seed は固定（既定 20261117）。アルゴリズム側に乱数は使わない。

## これは何を証明しないか

- 合成 fixture とクリーンプレートは再現可能な比較条件を与えるだけであり、
  補完結果が人間から見て自然であることを証明しない。
- ここにはアルゴリズム実装、Artifact の effect stack、GPU dispatch、
  runtime capture、性能計測は含まれない。
- 実アプリのビルド／テスト／capture は明示指示なく実行しない。
