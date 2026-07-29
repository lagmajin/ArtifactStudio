**ステータス:** FX-1/FX-2 completed; remaining phases pending

# M-FX-PERF: GPU エフェクト追加・プロパティ更新時のもたつき改善 — 修正リスト

調査日: 2026-07-22
症状: GPU (HLSL) エフェクト追加時にもたつく、プロパティ更新時に重い
対象領域: `Artifact/src/Effects/`、`ArtifactCore/src/Graphics/Compute.cppm`、
`Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`

## 根本原因サマリ（調査証拠）

| # | 原因 | 証拠 |
|---|------|------|
| R1 | エフェクト毎に GPU 完全ドレイン（`Flush + WaitForIdle + Map`）して CPU に readback。チェーン N 個で N 回の往復 | `Artifact/src/Effects/Blur/BlurEffect.cppm:504`（`readbackTexture`）、同:144 |
| R2 | 中間結果を GPU に保持できず、チェーン境界型が CPU 画像（`ImageF32x4RGBAWithCache`） | `ArtifactCompositionViewDrawing.cppm:669-675` |
| R3 | Brightness 系パターン: `GpuContext`/`Executor`/テクスチャを毎フレーム生成 | `BrightnessEffect.cppm:106-107, 146-157`（31エフェクトでパターン混在） |
| R4 | `pipelineReady_` はメンバだが executor はローカル → 2フレーム目以降 PSO 未構築で dispatch（正確性バグ） | `BrightnessEffect.cppm:129-144` |
| R5 | HLSL ランタイムコンパイルにキャッシュ無し。エフェクト追加の初回 apply で D3DCompile がレンダースレッドをブロック | `ArtifactCore/src/Graphics/Compute.cppm:57-71` |
| R6 | 静的プロパティ値が surface cache key に含まれず、広い単位のキャッシュ無効化に依存 | `ArtifactCompositionViewDrawing.cppm:451-470` |
| R7 | チェーン入口で QImage → cv::Mat → CV_32FC4 変換が毎フレーム走る | 同:606-613 |
| R8 | ホットパスに ROI hint の `qDebug` ログ | 同:654, 662 |

## 修正リスト

### P0: 正確性バグ・小規模（先行して切り出し可能）

- [x] **FX-1: `pipelineReady_` とローカル executor の不一致を修正**
  - 症状: Brightness 系エフェクトが2フレーム目以降、PSO 未構築の executor で dispatch する（実質 CPU フォールバック or 無効 dispatch）。
  - 修正: executor をメンバ保持にするか（= FX-3 で統一解決）、暫定ではローカル executor を使う限り毎回 build する（非推奨）。
  - 対象: `BrightnessEffect.cppm` 及び同パターンの全エフェクト（FX-3 の監査で特定）。
  - 完了条件: 2フレーム目以降も GPU dispatch が有効な PSO で実行される。

- [x] **FX-2: ホットパスの `qDebug` ログ削除**
  - 対象: `ArtifactCompositionViewDrawing.cppm:654, 662`（ROI hint ログ）。
  - 完了条件: フレーム毎・エフェクト毎のデバッグ出力が消える。
  - 完了: `ArtifactCompositionViewDrawing.cppm` の ROI hint ログを削除。

### P1: リソースキャッシュ統一・コンパイルキャッシュ

- [ ] **FX-3: 31エフェクトのパターン監査と Blur 型（メンバキャッシュ）への統一**
  - 症状: エフェクト毎に `GpuContext`/`Executor`/テクスチャの生成コストが毎フレームかかる（R3/R4）。
  - 修正: `executor_` / `gpuContext_` / `paramsCB_` / テクスチャ群をメンバ保持し、サイズ変更時のみ再生成する Blur 型パターンに統一。監査結果を本書に追記する。
  - 対象: `Artifact/src/Effects/` 配下の `make_unique<ArtifactCore::GpuContext>` 使用 31 ファイル（ColorCorrection 系を優先）。
  - 完了条件: 全対象エフェクトで定常状態（サイズ不変）の1フレームあたり GPU オブジェクト生成が 0 件。

- [ ] **FX-4: PSO/シェーダーのグローバルキャッシュ**
  - 症状: エフェクト追加の初回 apply で HLSL ランタイムコンパイルがブロックする（R5）。同一シェーダーを複数インスタンスが個別にコンパイルする。
  - 修正: `ComputeExecutor::build` にキャッシュ層を追加（キー: shader source ハッシュ + entryPoint + variables）。`ArtifactCore/src/Graphics/PSOCache.cppm` の土台を活用。
  - 完了条件: 同一ソースの2回目以降の build がコンパイルを伴わず、エフェクト追加時の初回ヒッチが解消される。

### P2: readback 構造の撤廃（本丸）

- [ ] **FX-5: チェーン内を GPU テクスチャのまま受け渡し（ping-pong）、readback は末尾1回に**
  - 症状: R1/R2。エフェクト毎の全ドレイン + 33MB 級の往復コピー。
  - 修正: チェーン境界型に GPU テクスチャ保持の中間表現を導入し、`applyConfigured` の入出力で GPU テクスチャを ping-pong させる。readback はチェーン末尾（CPU 側が必要とする直前）の1回のみ。CPU-only エフェクトが挟まる場合はその前後でのみ境界変換。
  - 対象: `ArtifactCompositionViewDrawing.cppm:633-681`（`buildRasterizedSurfaceBuffer` のチェーン）、`ArtifactAbstractEffect` の入出力契約。
  - 完了条件: 全 GPU チェーンで1フレームあたり readback が1回。AGENTS ルール遵守: GPU ダウンロードは明示関数経由。

- [ ] **FX-6: `WaitForIdle` 撤去 → fence/イベント待ち化**
  - 対象: `readbackTexture` 実装（`BlurEffect.cppm:144` 付近、GauusianBlur.cppm:175 付近ほか）。
  - 修正: `Flush + WaitForIdle` を fence wait（タイムアウト付き）に置換。FX-5 で readback が1回になる前提で、残る1回の待機を最小化。
  - 完了条件: エフェクト適用経路に `WaitForIdle` 呼び出しが残らない（解放処理 `releaseResources` 内は許容）。

### P3: 構造改善（効果は大きいが後回し可）

- [ ] **FX-7: pointwise 系エフェクトの融合（`PointwiseEffectFusion` 接続）**
  - 修正: Exposure/Brightness/Levels/Hue/Saturation 等の pointwise 系を `ArtifactCore` の `PointwiseEffectFusion` / `LayerBlendPipeline`（:374-378 の fused shader 経路）に接続し、連続する pointwise 区間を1パスに融合。
  - 完了条件: 典型的なカラー補正チェーン（2〜4個）が1 dispatch で完了する。

- [ ] **FX-8: surface cache key に静的プロパティ signature を追加**
  - 症状: R6。静的プロパティ変更が広い単位のキャッシュ無効化を引く。
  - 修正: `buildLayerSurfaceCacheKey` に静的エフェクトプロパティの値ハッシュ（`ArtifactInspectorWidget.cppm:1637` の signature 類似）を含め、レイヤー surface 単位の正確な無効化にする。
  - 完了条件: 静的プロパティ変更時に当該レイヤーの surface cache のみが無効化され、他レイヤーの cache が維持される。

- [ ] **FX-9: ドラッグ中の adaptive resolution**
  - 修正: プロパティドラッグ中は半解像度でエフェクトを適用し、確定時にフル解像度で再レンダー（AE の adaptive resolution 相当）。既存の debounce タイマー（`b6f0b502`）と接続。
  - 完了条件: スライダードラッグ中の1更新あたり処理量が概ね 1/4 になり、確定時にフル品質へ戻る。

- [ ] **FX-10: チェーン入口の QImage → cv::Mat 変換の削減**（R7）
  - 修正: `normalizeQImageForCvEffectBoundary` / `qImageToCvMat` の毎フレーム変換を、surface のキャッシュ済み CV_32FC4 表現で置き換えるか、FX-5 の GPU チェーン化で入口自体を消す。
  - 完了条件: エフェクトチェーン入口での形式変換がフレームあたり高々1回。

## 推奨実施順

1. **FX-2 → FX-1/FX-3**（小さく効く。FX-3 が FX-1 を包含）
2. **FX-4**（追加時ヒッチ解消）
3. **FX-5 + FX-6**（本丸。設計レビュー推奨）
4. **FX-7 以降**（余力で）

## 非目標（スコープ外）

- エフェクト個別のアルゴリズム最適化（カーネルサイズ削減等）
- CPU reference 実装の並列化
- FinalPostProcess / コンポジション単位エフェクトの最適化

## 検証方法

- `ArtifactCore::ScopedPerformanceTimer` / FrameDebug でエフェクト適用区間の ms を計測し、FX-3 完了後と FX-5 完了後で比較。
- 1080p レイヤーに Blur + Brightness + Glow の3個チェーンで、プロパティドラッグ中のフレーム時間を目標値（現状比 50% 削減）と比較。

## 2026-07-25 実装監査

- FX-2 の ROI hint `qDebug` 削除は完了している。
- 現行ソースでは Bevel、CreativeEffects、Dithering、Kaleidoscope、Color Correction 系などに `Flush()`／`WaitForIdle()`／staging texture／CPU readback の組み合わせが残っており、FX-5／FX-6 は未着手と判定する。
- GPU エフェクトの一部は `CreateTexture` と readback を apply 経路内で行うため、FX-3 の定常フレーム資源再利用と FX-10 の境界変換削減も未完了である。
- PSO／shader global cache、pointwise fusion、静的 property signature、adaptive resolution の完了証拠は確認できない。
- よって本マイルストーンは `Phase 0 completed (FX-2); remaining phases pending` のままとする。性能目標値は build／runtime／FrameDebug の測定未実施のため未検証である。

## 2026-07-29 実装監査（更新）

`Artifact/src/Effects/ColorCorrection/BrightnessEffect.cppm` を再確認し、`GpuContext`、`ComputeExecutor`、`paramsCB_`、`pipelineReady_` が GPU 実装インスタンスのメンバーとして保持され、PSO/SRB が初回構築後に再利用されることを確認した。したがって FX-1 は実装済みとしてマークする。

ただし、31エフェクト全体の資源再利用（FX-3）、チェーン内 readback 撤去（FX-5/6）、global PSO cache（FX-4）、性能測定は未完了のため、マイルストーン全体のステータスは変更しない。

- 追加確認: `Artifact/src/Effects/Bevel/BevelEffect.cppm` は `GpuContext`、`ComputeExecutor`、params buffer、input/output/staging texture を `applyGPU()` 内で生成している。device/context の共有寿命契約を確認せず executor だけをメンバー化するのは危険なため、FX-3 の安全な移行対象として別途 lifecycle 設計が必要。
