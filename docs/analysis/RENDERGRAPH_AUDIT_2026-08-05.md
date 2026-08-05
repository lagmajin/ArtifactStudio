# RenderGraph レポート

**最終更新:** 2026-08-05

> **2026-08-05 後日訂正**: 本レポートの「完全に休眠」という結論の一部は**誤り**でした。`ArtifactCompositionRenderController::frameDebugSnapshot()`（`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:17724-17804`）内に、すでに `RenderGraph` 診断グラフを構築し `compileDiagnosticSnapshot()` で `snapshot.renderGraphDiagnostic` へ投入し `hasRenderGraphDiagnostic` をセットする実装が存在します（作業ブランチ `feat_implement_audit_missing_features` の未コミット変更、+201 行）。したがって「診断が誰も投入していない」は不正解。正確な所見は: **診断グラフは即時モードのフレームデバッグスナップショット経路で既に稼働しているが、`GIRenderGraphAdapter` / `PointwiseRenderGraphAdapter` の2アダプタは依然として未使用（診断グラフはインラインで構築されているため）**。

**対象:** `ArtifactCore` の `Graphics.RenderGraph` モジュールと、それを利用する `Graphics.RenderPipelineFoundation`（GI / Pointwise アダプタ）、および診断側の `FramePipelineViewWidget` / `FrameDebug`。

---

## 1. 概要

`RenderGraph` は、レンダリングパス（Graphics / Compute / Copy）とリソース（Texture / Buffer）の依存関係を記述し、

- トポロジカルソートによる実行順序の決定
- リソースライフタイム（Transient / Persistent / External）の計算
- 循環依存の検出
- 診断スナップショット（パス数・リソース数・推定メモリ・実行順序・GPU 時間）

を提供する**純粋なデータ構造／スケジューリング抽象**である。

設計意図としては「マルチパスレンダリングの実行バックボーン」だが、実態は後述の通り**診断・テレメトリ用途にとどまり、実際のフレーム描画は駆動されていない**。

---

## 2. 設計・実装状態（高完成）

`ArtifactCore/include/Graphics/RenderGraph.ixx`

- `addResource()` / `addPass()` でグラフを構築。
- `compile()`（L122-198）は **Kahn のアルゴリズムによるトポロジカルソート**を実装。
  - read/write のリソース依存から有向辺を生成（`addEdge`）。
  - `indegree==0` のキューから順次展開し `passOrder` を決定。
  - 全パスを処理しきれなければ `render graph contains a cycle` で失敗扱い（L177）。
  - Transient リソースが write 前に read される場合は即失敗（L151-153）。
- `diagnosticSnapshot()` / `compileDiagnosticSnapshot()`（L200-258）で、パスごとの `Scheduled/Disabled/Blocked` 状態・実行順序・GPU 計時、リソースごとの first/last パス・推定バイト数を生成。
- 実装は自己完結的で、単体で正しく動作する。アルゴリズムの成熟度は高い。

> 注: `docs/analysis/GRAPHICS_GPU_AUDIT_2026-08-02.md` の「RenderGraph トポロジカルソート未実装」という記述は**現行コードと矛盾する旧記述**。`docs/analysis/CORRECTED_AUDIT_2026-08-02.md` でも同様の修正が言及されており、ソース側の修正は不要。

---

## 3. 実際の利用状況（重要: 休眠中）

コード全体を検索した結果、`Graphics.RenderGraph` は**ライブレンダリングで一切インスタンス化・実行されていない**。

| 確認項目 | 結果 |
|----------|------|
| `RenderGraph` のインスタンス生成（`RenderGraph graph;` 等） | **アプリコード内に存在せず**（`RenderGraph.ixx` 定義・`FrameDebug` のスナップショット保持のみ） |
| `GIRenderGraphAdapter::append()` の呼び出し元 | **存在せず（定義のみ）** |
| `PointwiseRenderGraphAdapter::append()` の呼び出し元 | **存在せず（定義のみ）** |
| `compileDiagnosticSnapshot()` の呼び出し元 | **存在せず（定義のみ）** |
| `hasRenderGraphDiagnostic` を `true` にする箇所 | **存在せず（構造体メンバ初期値 `false` のみ）** |
| `FramePipelineViewWidget` の扱い | `hasRenderGraphDiagnostic` が常に `false` なため、**常に「診断なし」分岐**に入る（L275） |

### 結論

- `RenderGraph` は「スケジューラとして完成しているが、誰も `addPass` して `compile` し、その結果でドローを発行していない」。
- 診断スナップショットも**誰も投入していない**ため、`FramePipelineViewWidget` には実データが届かない。
- すなわち `RenderGraph` は現在 **完全に休眠（dead/unwired）** の状態。

---

## 4. 合成パイプラインとの乖離

実際の合成フレームは `ArtifactCompositionRenderController::renderOneFrameImpl()`（`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:25943`）内で**即時モード（imperative）**に駆動されている。

- レイヤーごとの 2D ラスタ → `drawGpuLayerToIntermediate()`（L9570）
- 2D コマンドは `submitQueuedDraws()` でドレイン → 3D メッシュは即時 `renderer->draw()`（Diligent 即時ドロー、L839-848）
- ブレンド／トラックマットは `LayerBlendPipeline`（コンピュート）
- 最終ポストは `ArtifactFinalPostProcess`（OCIO LUT / カラーグレード）— 実装済み・運用中

つまり「マルチパス」は**存在するが、それは RenderGraph ではなく即時モードの手書きシーケンス**として成立している。

> 補足: `Artifact/include/Engine/DAG/` の `LayerGraphBuilder` / `CompositionGraph`（`.compile()` あり）は**レイヤー依存 DAG** であり、本 `Graphics.RenderGraph`（描画パス依存）とは別システム。混同しないこと。

---

## 5. GI / Pointwise アダプタの状態

`ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx`

- `GIRenderGraphAdapter::append()`（L29-63）: GI パス（Reconstruct / ScreenSpaceGather / DepthPyramid / BilateralDenoise / TemporalResolve / Composite）をグラフへ追加するアダプタ。**呼び出し元なし＝未接続**。
- `PointwiseRenderGraphAdapter::append()`（L107-126）: PointwiseFusion を単一パスとして追加。**呼び出し元なし＝未接続**。
- `GITemporalHistoryAdapter`（L128-153）: テンポラル履歴キー管理のみ。

`docs/analysis/GRAPHICS_GPU_AUDIT_2026-08-02.md` の別評価:
- `GIRenderPipeline` 🟡 65%「SSGI パイプライン実装中。プロトタイプ段階」
- `PointwiseFusion` 🟢 80%「ポイントワイズエフェクト融合。GPU最適化」

アダプタ自体は正しく書かれているが、**基盤となる GI / Pointwise 実行本体がプロトタイプ段階のため、グラフ接続が行われていない**と読める。

---

## 6. 成熟度評価

| 次元 | 評価 | 根拠 |
|------|------|------|
| データ構造・スケジューリング設計 | 🟢 高 | トポロジカルソート・ライフタイム・循環検出・診断スナップショットが完備 |
| 単体実装の正しさ | 🟢 高 | 自己完結、アルゴリズムは正しい |
| 実行バックボーンとしての活用 | 🔴 未活用 | インスタンス化・`compile()` 呼出し・ドロー発行が皆無 |
| 診断テレメトリとしての運用 | 🔴 休眠 | スナップショット投入箇所がなく、Widget は常に「診断なし」 |
| GI / Pointwise 接続 | 🟡 未接続 | アダプタ定義のみ、呼び出し元ゼロ |
| 合成フレーム全体のグラフ化 | 🔴 なし | 即時モードで駆動中 |

**総合判定: RenderGraph は「立派な未完成ピース」。** アルゴリズム・診断は本番品質だが、それを食わせるプロデューサ（パス構築）も、結果で描画するコンシューマ（グラフ駆動のドロー発行）も未実装。現状は**設計資産・将来の足場**として存在するのみ。

---

## 7. 推奨アクション

優先度順:

1. **方向決定（戦略）**: RenderGraph を (A) 実行バックボーンに昇格するか、(B) 純粋な診断/可視化ツールに留めるか決める。現在は中途半端な「休眠実装」状態。
2. **(A) を選ぶ場合**:
   - `renderOneFrameImpl` の即時モード手書きシーケンスを、`RenderGraph` の pass 構築 → `compile()` → 順次実行へ段階的に移行。
   - まずは GI / Pointwise アダプタの呼び出しを有効化し、`GIFrameContext` / `PointwiseFusionGraph` から実パスを生成・実行する。
3. **(B) を選ぶ場合**:
   - 休眠コード（`GIRenderGraphAdapter` / `PointwiseRenderGraphAdapter` の未使用分、診断投入漏れ）を明示的に接続するか、あるいは「設計のみ」と明記して迷いを排除。
4. **診断の即効性**: `FramePipelineViewWidget` が意味を持つよう、`renderOneFrameImpl` の各フェーズ（2D / 3D / ブレンド / ポスト）を最小限の `RenderGraph` として構築し、スナップショットを `FrameDebug` へ投入するだけでも可視化価値は高い。

---

## 8. GI / Pointwise 未接続の根本原因（調査 2026-08-05）

アダプタが「呼び出し元ゼロ」なのは単なる未実装ではなく、**実行モデルの分岐**によるもの。

### 8.1 Pointwise: 生きた実装と孤立したグラフが別モジュール

- **生きた実行パス（LIVE）**:
  - `Artifact.Render.PointwiseEffectFusion`（`PointwiseEffectFusion::makeComputePlan`）→ `LayerBlendPipeline::applyPointwise`（`ArtifactCore/src/Graphics/LayerBlendPipeline.cppm:340`）
  - 呼び出し元: `RenderPipeline::applyPointwise`（`Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:442`）→ `ArtifactCompositionRenderController.cppm:9645`
  - つまり pointwise 融合は**即時コンピュートディスパッチ**で実際に動いている（監査の「🟢 80%」はこの実装）。
- **アダプタが参照するグラフ**: `Graphics.PointwiseFusion` の `PointwiseFusionGraph`（`ArtifactCore/include/Graphics/PointwiseFusion.ixx:45`）。
  - この型を構築しアダプタへ渡すコードは**存在せず**、アダプタ自身の引数としてのみ出現。
- **結論**: アダプタは「グラフ表現 `Graphics.PointwiseFusion`」向けに書かれたが、renderer は別の「実行表現 `Artifact.Render.PointwiseEffectFusion`」を採用して即時実行した。グラフ表現を誰も populate しないためアダプタは孤立。

### 8.2 GI: スカフォールドのみ、実行パイプライン不在

- `GIFrameContext`（`Graphics.GIResources.ixx:262`）は `GISettings` / `GIExecutionPlan`（Reconstruct→DepthPyramid→ScreenSpaceGather→BilateralDenoise→TemporalResolve→Composite）を保持するが、**これを構築して何処かへ渡す live コードは存在しない**。
- `GIRenderPipeline` に相当する実行クラスもソース内になく（docs に「GIRenderPipeline 🟡 65%」とあるのみ）、GI は純スカフォールド。
- よって `GIRenderGraphAdapter` も投入される `GIFrameContext` が生まれないため孤立。

### 8.3 総括

`RenderGraph` とその GI / Pointwise アダプタは、**「グラフ駆動マルチパス」アーキテクチャのスカフォールド**として先行実装された。しかし実際の renderer は同じマルチパス効果（とくに pointwise）を**即時ディスパッチ型の `LayerBlendPipeline` / `ArtifactRenderLayerPipeline` 経路**で実装・採用した。結果として:

- グラフ表現（`Graphics.PointwiseFusion`, `Graphics.GIResources`）とそのアダプタは**誰も populate しない孤児**
- 実行は即時モードで成立し、`RenderGraph` は**設計資産としてのみ存在**

これが「完成度の高いデータ構造が休眠している」真正の理由。

---

## 9. キーファイル

| ファイル | 役割 |
|----------|------|
| `ArtifactCore/include/Graphics/RenderGraph.ixx` | RenderGraph データ構造・`compile()`・診断スナップショット |
| `ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx` | `GIRenderGraphAdapter` / `PointwiseRenderGraphAdapter`（未接続） |
| `ArtifactCore/include/Graphics/GIResources.ixx` | `GIFrameContext`（GI 実行計画・リソース） |
| `ArtifactCore/include/Graphics/PointwiseFusion.ixx` | PointwiseFusion グラフ |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:25943` | `renderOneFrameImpl`（即時モード駆動の実体） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:9570` | `drawGpuLayerToIntermediate`（2D 中間描画） |
| `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm:207-285` | RenderGraph 診断表示（常に「診断なし」） |
| `ArtifactCore/include/Frame/FrameDebug.ixx` | `RenderGraphDiagnosticSnapshot` の JSON シリアライズ |
| `Artifact/include/Engine/DAG/LayerGraphBuilder.ixx` / `CompositionGraph.ixx` | 別系統のレイヤー依存 DAG（本 RenderGraph とは無関係） |
