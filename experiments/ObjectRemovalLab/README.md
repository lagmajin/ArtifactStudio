# Object Removal Lab

**最終更新:** 2026-11-16
**ステータス:** 初期準備（fixture生成器・評価計画を作成。製品経路比較と人手評価は未実施）

ArtifactStudio に既存の `Wire / Object Remover` があるため、新規の除去機能を重ねず、その初期品質・適用範囲・限界を決定論的な人工 fixture で調べるオフラインラボ。コード変更や製品接続を承認するものではない。

## 実行

Python 3.10 以降、NumPy / Pillow / OpenCV が必要。Artifact のビルド・起動・テストはこのラボの実行条件に含めない。

```powershell
python experiments/ObjectRemovalLab/generate_fixtures.py
python experiments/ObjectRemovalLab/measure_baseline.py
```

fixture と `manifest.json` は `captures/rd0/` に出力する。生成器は背景、除去対象入り入力、対象マスク、正解クリーン画像をペアで保存する。baseline 解析は正解に対するマスク内・3px境界帯の誤差とマスク外変更量を記録する。rd0 は候補アルゴリズムを実行せず、品質判定を行わない。

## 評価計画

事前計画は [`EVALUATION_PLAN.md`](EVALUATION_PLAN.md)。画像評価は [`../../docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md`](../../docs/technical/IMAGE_EFFECT_RND_EVALUATION_RULES.md) に従う。全 fixture は固定 seed、固定寸法、linear RGB float32 を基準とし、PNG は目視用とする。マスクと正解画像を保持し、入力から隠した背景部分の再構成を同座標で比較する。

初期比較対象は既存 `WireRemove` の SideAverage / CloneOffset / Blur / Telea / Navier-Stokes。Python/OpenCV の結果は Artifact の実装と同一とはみなさない。製品経路へ進む場合は、同一 source revision と設定で製品出力を capture して差を別途確認する。

## 現時点の境界

- 静止画処理の既存実装はある。`ArtifactCore/src/ImageProcessing/WireRemove.cpp` と `Artifact/src/Effect/ArtifactCreativeEffects.cpp` を参照。
- 既存 C++ inpaint は OpenCV 8-bit へ変換するため、HDR／float の忠実な保持を前提にしない。
- 連続フレーム間の背景伝播、視覚品質、人手レビュー、GPU/CPU parity、性能は未評価。
- 評価結果だけで製品既定値や新機能を採用しない。
