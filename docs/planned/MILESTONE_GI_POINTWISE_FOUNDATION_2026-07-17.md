# GI / Pointwise Fusion Foundation

**ステータス:** In Progress

## 目的

`ArtifactCore` に、GPU接続なしで再利用できる pointwise fusion と GI のデータ契約・実行計画を整備する。

## 完了済み

- `Graphics.PointwiseFusion`
  - pointwise operation graph
  - 定数ノード
  - 依存関係・入力数・型の検証
  - HLSL body / function lowering
  - 最終出力value管理
- `Graphics.GIResources`
  - `Fast` / `Quality` / `Disabled` settings
  - frame resource descriptors
  - temporal accumulation state
  - quality別 GI execution plan
  - `GIFrameContext`

## 未実装

- Diligent device / dispatchへの接続
- SSGI shader本体
- depth pyramid / bilateral denoise shader本体
- PBR lightingへの合成
- GPU/CPUの実測と品質比較

## 方針

- `ArtifactCore` の型・計画・loweringを先に安定させる
- `QImage` をhot pathへ追加しない
- GPU接続は契約が固まった後に `Artifact` 側で行う
- Fastは低解像度・低サンプル、Qualityはdepth pyramid・denoise・高サンプルを使う
