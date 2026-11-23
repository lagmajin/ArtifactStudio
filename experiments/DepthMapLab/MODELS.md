# DepthMapLab models

**最終更新:** 2026-11-16

モデル重みはリポジトリに含めない。`experiments/DepthMapLab/models/` は git 管理外であり、取得は明示的な操作でのみ行う。

## 固定済みモデル

### Depth Anything V2 Small (int8 ONNX)

| 項目 | 値 |
|---|---|
| リポジトリ | `onnx-community/depth-anything-v2-small` |
| revision | `4472b7362082ad9968fee890ca0f1e5aca36b93d` |
| ファイル | `onnx/model_int8.onnx` |
| サイズ | 27,258,801 bytes |
| SHA-256 | `01aa7a23de3f4a0ee1a2bb9997e6918104c85a9f95dea46d27b9b3fb0c6b9001` |
| ライセンス | Apache-2.0（モデルカードの表記） |
| preset | `da2_small`（短辺 518、14 の倍数、ImageNet mean / std、RGB） |

取得（明示オプション。hash 検証あり）:

```powershell
python experiments/DepthMapLab/depth_map_lab.py --rd rd1 `
  --download-model `
  --onnx da2s=models/depth-anything-v2-small-int8.onnx `
  --onnx-preset da2s=da2_small
```

モデルカード: <https://huggingface.co/onnx-community/depth-anything-v2-small/tree/4472b7362082ad9968fee890ca0f1e5aca36b93d>

GlowLab の Flash R&D が同じ revision と同じ hash を既に記録している（`experiments/GlowLab/DEPTH_MODEL_NOTICE.md`）。本ラボはその固定情報を再利用し、重みは再配布しない。

## 未固定の候補（必要になった時点で追加）

以下は候補として検討できるが、本ラボは hash を推測して固定しない。追加する場合は、取得したファイルの SHA-256 とライセンスをこの表へ実測で記録してから使う。

| 候補 | preset | 注意 |
|---|---|---|
| Depth Anything V2 Base / Large | `da2_small` と同じ前処理系 | サイズと推論時間が増える。ライセンスはモデルカードで再確認する |
| Depth Anything V2 Metric (indoor / outdoor) | 入力正規化が異なる場合がある | 出力がメートル深度の場合は相対深度比較の前提が変わる。別ゲートが必要 |
| MiDaS small | `midas_small` | 出力は inverse depth。ライセンスは MIT 系だがファイルごとに確認する |
| 独自 ONNX | `generic_512`（0〜1、mean 0 / std 1） | 前処理を合わせないと比較にならない。`--onnx-preset name=preset` で指定する |

## 前処理 preset

| preset | 短辺 | multiple | mean | std |
|---|---|---|---|---|
| `da2_small` | 518 | 14 | 0.485, 0.456, 0.406 | 0.229, 0.224, 0.225 |
| `midas_small` | 384 | 32 | 0.5, 0.5, 0.5 | 0.5, 0.5, 0.5 |
| `generic_512` | 512 | 32 | 0, 0, 0 | 1, 1, 1 |

前処理が違うモデルを同じ preset で走らせると、指標は「モデル差」ではなく「入力契約の不一致」を測ってしまう。manifest に preset 名と実入力サイズを残すこと。

## 出力の向き

単眼深度の ONNX は、近いほど大きい（inverse depth / disparity）ものと、遠いほど大きいものがある。本ラボは `scene_boxes` の GT との Spearman 符号で 1 回だけ極性を較正し、その結果を `manifest.json` の `depth_orientation` に記録する。極性は品質ではなく出力契約の問題であり、fixture ごとに合わせ直さない。
