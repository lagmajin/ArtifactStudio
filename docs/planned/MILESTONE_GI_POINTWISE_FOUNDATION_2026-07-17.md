# GI / Pointwise Fusion Foundation

**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

`Graphics.PointwiseFusion` は `RenderPipeline::applyPointwise()` から validation／compute plan／GPU blend pipeline へ接続されています。GI も `ArtifactRenderLayerPipeline` に `ScreenSpaceGICS` と temporal resolve compute shader、history／depth／normal／velocity、denoise パラメータ、低解像度 dispatch の実装があり、前回監査の「SSGI shader 本体未確認」は現状と一致しません。

ただし、GI の結果を通常の PBR lighting／最終 composition へ合成する呼び出し経路、depth pyramid／本格 bilateral denoise、`Graphics.GIResources` の計画と実 GPU resource の一貫した接続、GPU／CPU 品質比較は未確認です。現状は **Core foundation＋SSGI compute／temporal resolve の部分実装**と判定します。

## Update 2026-08-15

- `Graphics.PointwiseFusion` の validation／compute plan／GPU blend pipeline 接続と、`ArtifactRenderLayerPipeline` の SSGI compute、temporal resolve、history／depth／normal／velocity、低解像度 dispatch を再確認。
- 前回の「SSGI shader 本体未確認」は現行コードには当たらず、SSGI／temporal 部分実装として更新。
- 通常の PBR lighting／最終 composition への GI 合成、depth pyramid、bilateral denoise、`GIResources` の一貫した GPU resource 接続、GPU／CPU 品質比較は未完了・未検証。

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
  - 必要resourceの算出と`resourcesReady()`検証

## Phase 1 状態

**ArtifactCore-only foundation:** Complete

接続なしのCore基盤として、graph / HLSL lowering / GI settings / execution plan /
temporal state / resource readinessまで実装済み。ここで一旦区切り、次回はGPU接続フェーズとして再開する。

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

## 2026-07-25 実装監査

- `Graphics.PointwiseFusion` と `Graphics.GIResources` は存在し、pointwise graph／HLSL lowering、GI settings、frame resources、execution plan、temporal state、resource readiness を確認できる。
- `Graphics.RenderPipelineFoundation` に Pointwise／GI の Render Graph adapter 契約も存在するが、Diligent resource／barrier と実描画 pass への接続は未完了である。
- `Artifact` 側には SSGI の設定・compute pipeline 接続の既存経路がある一方、本文書が要求する SSGI shader 本体、depth pyramid、bilateral denoise、PBR lighting 合成の一連の完成は確認できない。
- GPU／CPU の実測と品質比較も未検証であり、Phase 1 の Core foundation のみ完了、GPU integration phase は未着手・進行中と判定する。
