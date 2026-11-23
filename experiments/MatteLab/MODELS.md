# 学習モデルの入手と記録

**最終更新:** 2026-11-15
**Status:** U2Net（rd1）、MODNet（rd3）、BiRefNet 512（rd4）の 3 本を入手済み。RMBG 系はライセンス上見送り

重みはリポジトリに含めません。`models/` は `.gitignore` 済みです。使用前に以下を記録します。

- 出所 URL、ライセンス（コードと重みを分けて確認）
- ファイル名、サイズ、SHA-256
- 入力契約: 解像度、正規化（0〜1 / ImageNet）、チャンネル順、動的サイズ可否
- 出力契約: チャンネル数、sigmoid 済みか logits か、出力解像度
- 入手日と入手手段

記録先は本ファイルと、実行時の `captures/<rd>/manifest.json`。

## 入手済み: U2Net（rd1）

| 項目 | 値 |
|---|---|
| 配置 | `models/u2net.onnx`（`experiments/MatteLab/` 配下、git 管理外） |
| 出所 URL | `https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net.onnx` |
| 配布元 | rembg（danielgatis/rembg）のリリース添付 |
| サイズ | 175,997,641 bytes |
| SHA-256 | `8d10d2f3bb75ae3b6d527c77944fc5e7dcd94b29809d47a739a7a728a912b491` |
| 入手日 / 手段 | 2026-11-15 / `Invoke-WebRequest` で直接ダウンロード、変換なし |
| ライセンス（配布元コード） | rembg は MIT（配布元 README の記載） |
| ライセンス（上流モデル） | U-2-Net（xuebinqin/U-2-Net）は Apache-2.0（上流 README の記載） |
| 入力契約 | `input.1`, float32, `[1, 3, 320, 320]` 固定。RGB、0〜1 へ変換後に ImageNet mean `(0.485, 0.456, 0.406)` / std `(0.229, 0.224, 0.225)` |
| 前処理（実装） | 8-bit RGB のまま `cv2.resize` `INTER_AREA` で 320×320、その後 `/255` と mean/std 正規化 |
| 出力契約 | 7 出力（d0〜d6、`1959`〜`1965`）。いずれも `[1, 1, 320, 320]`、モデル内で sigmoid 済みの 0〜1 |
| 使用出力 | 0 番目（`1959` = fused d0）。入力解像度へ bilinear でリサンプルして α とする |
| 実行 | onnxruntime 1.31.1 / CPUExecutionProvider / intra・inter op 1 スレッド |

プリセットは `matte_lab.py` の `ONNX_PRESETS["u2net"]` に固定し、実行時に `--onnx-preset u2net=u2net` で対応付けます。manifest には入力名・入力 shape・出力名・出力 shape・SHA-256・プリセット名を記録します。

**未検証**

- 重みのライセンス条項そのもの（商用可否、再配布条件）は本ラボでは法的確認をしていない。配布元と上流の README 記載に留まる。
- 320×320 固定入力が境界精度に与える影響の切り分け（解像度を上げた変種との比較は未実施）。
- d0 以外の side output を使った場合の差。
- 量子化・最適化版との数値一致。

## 入手済み: MODNet（rd3）

| 項目 | 値 |
|---|---|
| 配置 | `models/modnet.onnx`（`experiments/MatteLab/` 配下、git 管理外） |
| 出所 URL | `https://huggingface.co/Xenova/modnet/resolve/main/onnx/model.onnx` |
| 配布元 | Hugging Face `Xenova/modnet`（MODNet の transformers.js 向け ONNX 変換） |
| サイズ | 25,888,640 bytes |
| SHA-256 | `07c308cf0fc7e6e8b2065a12ed7fc07e1de8febb7dc7839d7b7f15dd66584df9`（HF の LFS oid と一致） |
| 入手日 / 手段 | 2026-11-15 / `Invoke-WebRequest`、変換なし |
| ライセンス | HF リポジトリのタグは `apache-2.0`（上流 MODNet は Apache-2.0）。重みの条項は本ラボで法的確認をしていない |
| 入力契約 | `input`, float32, `[batch_size, 3, height, width]`（動的）。512×512 を渡す。RGB、`/255` 後に mean 0.5 / std 0.5 |
| 前処理（実装） | 8-bit RGB のまま `cv2.INTER_AREA` で 512×512、`/255`、`(x-0.5)/0.5` |
| 出力契約 | `output`, `[batch_size, 1, height, width]`。モデル内で sigmoid 済みの 0〜1 α |
| 使用出力 | 0 番目のみ。入力解像度へ bilinear でリサンプル |
| 実行 | onnxruntime 1.31.1 / CPUExecutionProvider / 1 スレッド、0.30〜0.33 s / 枚 |

**未検証**

- MODNet は人物 matting 用に学習されており、本ラボの合成 fixture は人物ではない。`hair_strands` / `soft_shadow` / `green_screen` で α がほぼ 0 になったのはドメイン外のためと考えられるが、切り分けは未実施。
- 学習データ・人物以外での汎化、実写人物での品質。

## 入手済み: BiRefNet 512（rd4）

| 項目 | 値 |
|---|---|
| 配置 | `models/birefnet_512.onnx`（`experiments/MatteLab/` 配下、git 管理外） |
| 出所 URL | `https://huggingface.co/onnx-community/BiRefNet_512x512-ONNX/resolve/main/onnx/model.onnx` |
| 配布元 | Hugging Face `onnx-community/BiRefNet_512x512-ONNX`（ZhengPeng7/BiRefNet_512x512 の ONNX 変換） |
| サイズ | 940,413,603 bytes |
| SHA-256 | `617a11ca04cb13a2817bfad605969b4a9a040fceedbaaa46348a3957f0f6c254`（HF の LFS oid と一致） |
| 入手日 / 手段 | 2026-11-15 / `Invoke-WebRequest`、変換なし |
| ライセンス | HF リポジトリのタグは `mit`（上流 BiRefNet も MIT）。重みの条項は本ラボで法的確認をしていない |
| 入力契約 | `input_image`, float32, `[1, 3, 512, 512]` 固定。RGB、`/255` 後に mean `(0.485, 0.456, 0.406)` / std `(0.229, 0.224, 0.225)` |
| 前処理（実装） | 8-bit RGB のまま `cv2.INTER_AREA` で 512×512、`/255`、ImageNet mean/std |
| 出力契約 | `output_image`, `[1, 1, 512, 512]`。sigmoid 済みの 0〜1 α 1 枚のみ |
| 実行 | onnxruntime 1.31.1 / CPUExecutionProvider / 1 スレッド、0.63〜0.70 s / 枚 |

**未検証**

- 1024×1024 入力版（`BiRefNet_lite-ONNX` / `BiRefNet-ONNX`）との差。
- fp16 版での数値差と速度。
- 実写人物・実素材での品質。

## 候補（未入手）

| 候補 | 種別 | 用途 | 備考 |
|---|---|---|---|
| RMBG-2.0 | 一般前景抽出 | 汎用の基準候補 | 商用条件は要確認。ONNX 変換版は CC-BY-NC-4.0 タグのものが多く、ライセンス上見送り |
| MODNet | 人物 matte | 人物・髪の基準 | 軟らかい α を持つ（rd3 で入手・実行済み。上記参照） |
| BiRefNet | 高精度 dichotomous segmentation | 品質上限の候補 | 512 版を rd4 で入手・実行済み（上記参照）。1024 版は未入手 |
| U2Net / ISNet | 軽量前景抽出 | 「小さいモデルで足りるか」の対照 | 出力は概ね sigmoid 済み（U2Net は実測で確認） |

いずれも ONNX 形式での入手可否と、入手元ごとの数値一致は未確認です。PyTorch 重みからの変換が必要な場合は、変換手順・変換元バージョン・変換後 hash を一次証跡として記録します。

## ライセンス確認の方針

- コードと重みのライセンスを分けて確認する。
- 再配布が制限される重みはローカル配置のみとし、リポジトリや成果物に含めない。
- 商用利用可否は本ラボでは判断せず、未確認として残す。

## 未確認事項

- 各候補の ONNX 入手可否と、前処理・後処理の一致
- 512×512 固定入力か動的サイズかの違いが SAD に与える影響
- 出力 α の解像度が入力と異なる場合のリサンプル方法の統一
