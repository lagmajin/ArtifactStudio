# RenderGraph 実装ハンドオフレポート

**最終更新:** 2026-08-05

> **2026-08-05 後日訂正**: 本レポートの「診断が誰も投入していない／Widget は常に診断なし」という前提は**誤り**でした。`frameDebugSnapshot()`（L17724-17804）にすでに診断グラフ構築→`compileDiagnosticSnapshot()`→`hasRenderGraphDiagnostic=true` の実装があります（ブランチ `feat_implement_audit_missing_features` 未コミット）。したがって「戦略 B（診断接続）」は**すでに実装済み**です。残る未接続は `GIRenderGraphAdapter` / `PointwiseRenderGraphAdapter` の2アダプタ（診断グラフはインライン構築のためこれらを経由していない）。以降のタスクは「インライン構築をアダプタ経由へ正規化する」か、「実行バックボーン化（戦略 A）」に読み替えること。
**対象リーダー:** 実装用 AI（RenderGraph の活用／接続作業を担当）
**前提文書:** `docs/analysis/RENDERGRAPH_AUDIT_2026-08-05.md`（調査・成熟度評価）

---

## 1. 目的

`ArtifactCore` の `Graphics.RenderGraph` は完成度の高いスケジューリング／診断データ構造だが、**現在は誰も populate せず完全に休眠している**。本レポートは、その未接続の根本原因を実装者視点で整理し、具体的な接続作業（戦略 B：診断接続）を定義する。

---

## 2. アーキテクチャ乖離マップ（最重要）

同じ「マルチパス」に対して **2 つの並列実装**が存在し、ライブ側が採用されグラフ側が孤児になっている。

| 関心 | ライブ実装（採用済み） | グラフスカフォールド（孤立） | 橋渡し |
|------|----------------------|----------------------------|--------|
| 点毎(Playwise)効果 | `Artifact.Render.Pipeline` → `RenderPipeline::applyPointwise` (`Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:66`) → `PointwiseEffectFusion::makeComputePlan` (`Artifact.Render.PointwiseEffectFusion`) → `LayerBlendPipeline::applyPointwise` | `Graphics.RenderPipelineFoundation` の `PointwiseRenderGraphAdapter::append(RenderGraph&, const PointwiseFusionGraph&, ...)` (`ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx:107`) | **なし**。`PointwiseFusionGraph`（`Graphics.PointwiseFusion`）を構築するコードが不在 |
| グローバル照明(GI/SSGI) | `RenderPipeline::dispatchScreenSpaceGlobalIllumination` (`ArtifactRenderLayerPipeline.ixx:106`) | `GIRenderGraphAdapter::append(RenderGraph&, const GIFrameContext&)` (`RenderPipelineFoundation.ixx:29`) | **なし**。`GIFrameContext`（`Graphics.GIResources`）を構築・渡す live コード不在 |
| シーケンス駆動 | `ArtifactCompositionRenderController::renderOneFrameImpl` の即時モード（`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:25943`） | `RenderGraph::compile()` トポロジカルソート（`Graphics.RenderGraph` L122） | **なし**。`RenderGraph` のインスタンス生成が live コードに不在 |
| 診断可視化 | `FramePipelineViewWidget` が `snapshot_.renderGraphDiagnostic` を参照（`Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm:207`） | `RenderGraphDiagnosticSnapshot` を JSON 化する `renderGraphDiagnosticFromJson`（`ArtifactCore/include/Frame/FrameDebug.ixx:49`） | **なし**。`hasRenderGraphDiagnostic` を `true` にする箇所が不在 → Widget は常に「診断なし」 |

**決定的事実:**
- ライブ Pointwise は `Artifact.Render.PointwiseEffectFusion`（モジュール）を使う。**アダプタが参照する `Graphics.PointwiseFusion` は別モジュール**（`PointwiseFusionGraph`）。
- ライブ GI は `RenderPipeline::dispatchScreenSpaceGlobalIllumination` を使う。**アダプタが参照する `GIFrameContext`（`Graphics.GIResources`）は別表現**。
- よって「アダプタが呼ばれない」のは未実装ではなく、**実行モデルの分岐による孤児化**。

---

## 3. 実装制約（AGENTS.md より必読）

- **ビルド／CMake 実行はユーザー依頼時のみ**。実装中はコンパイル確認を勝手に行わない。
- **サブモジュール・外部ライブラリの変更禁止**: `libs/DiligentEngine`、`third_party/*`、`ArtifactWidgets` 等は原則触らない。本タスクは `Artifact` / `ArtifactCore` 内の既存モジュール編集で完結させる。
- **Diligent 低レベル（PSO／ラスタライザ／シェーダ）の変更は慎重**に。今回の狙いは「グラフの接続（テレメトリ／シーケンス化）」であり、描画本体の書き換えではない。
- **新規シグナル＆スロット接続・グローバルイベントの新規追加は禁止**。
- **QtCSS / `QColorDialog` / `QImage` の新規採用は禁止**（本タスクでは該当しないが念のため）。
- C++20 modules の循環参照に注意。`export import` の乱用禁止。変更した `.ixx` は全インポーター再コンパイル対象になるため最小化する。
- 既存ファイル編集は `edit` ツールを使い、**CRLF 改行を維持**する（write は新規ファイルのみ）。

---

## 4. 実装戦略

### 戦略 A（大規模）: RenderGraph を実行バックボーンに昇格
`renderOneFrameImpl` の即時モード手書きシーケンスを、`RenderGraph` の pass 構築 → `compile()` → 順次実行へ段階移行。
- リスク高・影響大。GI / Pointwise の live エントリ（`dispatchScreenSpaceGlobalIllumination` / `applyPointwise`）をグラフの各 pass から呼ぶよう再構成が必要。

### 戦略 B（推奨・低リスク）: グラフを診断オーバーレイに接続
実際の描画は即時モードのまま、**各フレームでライブ実行ステップから `RenderGraph` を構築し、依存解決・診断スナップショットを `FrameDebug` へ注入**する。
- グラフ側の 2 モジュール（`Graphics.PointwiseFusion` / `Graphics.GIResources`）を**ライブ入力から構築**し、既存アダプタ（`GIRenderGraphAdapter` / `PointwiseRenderGraphAdapter`）を初めて本番で呼ぶ。
- 描画本体に手を触れず、純増のテレメトリ化。リスク最小。
- `FramePipelineViewWidget` が初めて意味を持つ。

**以降は戦略 B を前提に記述。**

---

## 5. 推奨タスクの具体的ステップ（戦略 B）

### T1. ライブ Pointwise スタックから `PointwiseFusionGraph` を構築するブリッジを追加
- 場所: `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm` の `RenderPipeline::applyPointwise`（:442）付近、または呼び出し側 `ArtifactCompositionRenderController.cppm:9645`。
- 内容: `PointwiseEffectStack`（`Artifact.Render.PointwiseEffectFusion` の入力型）のノード列を `Graphics.PointwiseFusion` の `PointwiseFusionGraph`（`ArtifactCore/include/Graphics/PointwiseFusion.ixx:45`）へ変換する補助関数を新設。
  - `PointwiseFusionGraph::addInput(semantic, type)` / `addConstant(literal)` / `addOperation(kind, inputs, type)` を使い、stack の順にオペレーションを追加。
  - `PointwiseEffectFusion` の `PointwiseEffectNode` 種別 → `PointwiseOperationKind`（Input/Constant/Add/Multiply/Lerp/Saturate/Select）のマッピングが必要（`Graphics.PointwiseFusion` L20 の enum 参照）。
- 構築したグラフを `PointwiseRenderGraphAdapter::append(graph, fusionGraph, input, output)` へ渡す。

### T2. ライブ GI 設定から `GIFrameContext` を構築するブリッジを追加
- 場所: `RenderPipeline::dispatchScreenSpaceGlobalIllumination`（:106）の呼び出し元（おそらく `renderOneFrameImpl` の DOF/GI フェーズ）。
- 内容: `GlobalIlluminationInputs` と解像度・強度パラメータから `GISettings` を作り、`GIFrameContext`（`Graphics.GIResources` L262: `explicit GIFrameContext(GISettings)`）を構築。`GISettings::fast()/quality()/disabled()` を活用。
- 構築した `GIFrameContext` を `GIRenderGraphAdapter::append(graph, context)` へ渡す（同アダプタは `context.plan().empty()` なら早期リターンするので、enabled / mode を適切に設定すること）。

### T3. フレーム内で `RenderGraph` を構築・コンパイルし `FrameDebug` へ注入
- 場所: `renderOneFrameImpl`（`ArtifactCompositionRenderController.cppm:25943`）の末尾（全パス描画後）。
- 内容:
  1. `ArtifactCore::RenderGraph graph;` を生成（モジュール `Graphics.RenderGraph` を import）。
  2. T1/T2 のブリッジで pass を追加。必要に応じ 2D（`drawGpuLayerToIntermediate` :9570）、3D（`drawMesh` 経由）、ブレンド（`LayerBlendPipeline`）、ポスト（`ArtifactFinalPostProcess`）の代表 pass も `graph.addPass()` で手動追加し、read/write リソースで依存をつなぐ。
  3. `const auto compiled = graph.compile();`（L122）でトポロジカルソート。失敗時は `compiled.error` をログに出し、診断投入をスキップ。
  4. `graph.compileDiagnosticSnapshot(executionId)`（L254）で `RenderGraphDiagnosticSnapshot` を得る。
  5. `FrameDebug` スナップショット構造体の `hasRenderGraphDiagnostic = true` をセットし、`renderGraphDiagnostic` へ代入（構造体定義は `ArtifactCore/include/Frame/FrameDebug.ixx`、JSON 化は `renderGraphDiagnosticFromJson` L49 / L730）。

### T4. `FramePipelineViewWidget` の確認
- `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm:207-285` は既に `snapshot_.renderGraphDiagnostic` を表示するコードがあり、`hasRenderGraphDiagnostic` が `true` なら描画される。T3 で投入されればそのまま機能するはず。実機でパス数／リソース数が表示されることを確認。

---

## 6. 受け入れ基準（Acceptance）

- [ ] `RenderGraph` のインスタンスが `renderOneFrameImpl` 内で構築され、`compile()` が成功（error 空）すること。
- [ ] `GIRenderGraphAdapter::append` と `PointwiseRenderGraphAdapter::append` が**少なくとも 1 箇所から実際に呼ばれる**ようになる（grep で呼び出し元が現れる）。
- [ ] `FramePipelineViewWidget` にパス数・リソース数が表示される（`hasRenderGraphDiagnostic == true` となる）。
- [ ] 既存の描画結果（2D／3D／ブレンド／ポスト／GI／Pointwise）が**変化しない**（テレメトリ追加のみ）。
- [ ] ビルドがユーザー依頼なしで行われていないこと。
- [ ] 循環参照・モジュール再コンパイル爆発が発生しないこと（`export import` の増減を最小に）。

---

## 7. リスク／注意点

- **2 モジュールの型不一致**: `Graphics.PointwiseFusion` の `PointwiseOperationKind` と `Artifact.Render.PointwiseEffectFusion` の `PointwiseEffectNode` 種別は 1:1 でない可能性がある。マッピング表を明示し、未知種別は `fusible=false` 扱いにしてアダプタ投入をスキップすること。
- **GI pass 種別の不一致**: `GIExecutionPlan`（`Graphics.GIResources` L90）の pass 種別（Reconstruct/DepthPyramid/ScreenSpaceGather/BilateralDenoise/TemporalResolve/Composite）は、ライブ `dispatchScreenSpaceGlobalIllumination` の引数（resolutionScale/raySteps/intensity/denoise 等）と 1:1 対応しない。グラフは「計画の可視化」に留め、実ディスパッチはライブ関数へ委譲する形にする（グラフが実行を代替しない）。
- **依存エッジの正確性**: `addPass` の `reads`/`writes` が実際のリソースフローと食い違うと `compile()` が cycle / transient-before-write で失敗する。まずは最小限の pass セット（2D→3D→ブレンド→ポスト）で検証し、GI/Pointwise は独立 pass として追加。
- **パフォーマンス**: 毎フレームのグラフ構築／コンパイルは軽量（線形）だが、デバッグビルド以外では `FrameDebug` 診断が有効な時のみ構築するガードを入れること（例: 既存の debug flag で short-circuit）。

---

## 8. キーファイル・行番号一覧

| ファイル:行 | 内容 |
|------------|------|
| `ArtifactCore/include/Graphics/RenderGraph.ixx:122` | `RenderGraph::compile()` トポロジカルソート |
| `ArtifactCore/include/Graphics/RenderGraph.ixx:254` | `compileDiagnosticSnapshot()` |
| `ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx:29` | `GIRenderGraphAdapter::append` |
| `ArtifactCore/include/Graphics/RenderPipelineFoundation.ixx:107` | `PointwiseRenderGraphAdapter::append` |
| `ArtifactCore/include/Graphics/PointwiseFusion.ixx:45` | `PointwiseFusionGraph`（孤立グラフ型） |
| `ArtifactCore/include/Graphics/GIResources.ixx:90` | `GIExecutionPlan`（Reconstruct…Composite） |
| `ArtifactCore/include/Graphics/GIResources.ixx:262` | `GIFrameContext` |
| `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:66` | `RenderPipeline::applyPointwise`（LIVE） |
| `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx:106` | `RenderPipeline::dispatchScreenSpaceGlobalIllumination`（LIVE） |
| `Artifact/src/Render/ArtifactRenderLayerPipeline.cppm:442` | `RenderPipeline::applyPointwise` 実体 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:25943` | `renderOneFrameImpl`（即時モード駆動） |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:9645` | `applyPointwise` 呼び出し |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:9570` | `drawGpuLayerToIntermediate`（2D 中間描画） |
| `Artifact/src/Widgets/Diagnostics/FramePipelineViewWidget.cppm:207` | `renderGraphDiagnostic` 表示 |
| `ArtifactCore/include/Frame/FrameDebug.ixx:49` | `RenderGraphDiagnosticSnapshot` JSON 化 |
| `ArtifactCore/src/Graphics/LayerBlendPipeline.cppm:340` | `LayerBlendPipeline::applyPointwise`（LIVE 実行） |
| `ArtifactCore/include/Render/PointwiseEffectFusion.ixx:428` | `PointwiseEffectFusion::makeComputePlan`（LIVE） |

---

## 9. 備考

- 戦略 A（実行バックボーン化）は影響が大きいため、まず戦略 B で「グラフが実際に populate される」ことを証明し、その上で段階的に即時モードをグラフ駆動へ移行するのが安全。
- 本タスクは既存 `Insight.md` の記録対象（閃き）ではなく、明示的な実装依頼に基づく。作業中に気づいた設計仮説は `Insight.md` へ記録してよいが、実装範囲の勝手な拡大はしないこと。

## 10. 初回診断接続の実装状況

2026-08-05 時点で、戦略 B の最小接続を実装した。

- `CompositionRenderController::frameDebugSnapshot()` 内で、実描画を駆動しない診断専用 `RenderGraph` を構築する。
- composition の可視・アクティブレイヤーごとに `LayerRaster` パスを登録し、`LayerBlend` → `FinalPostProcess` へ依存を接続する。
- リソース寸法と概算バイト数は現在の preview pipeline から取得する。
- renderer / composition / pipeline が未準備の場合は診断を有効化しない。
- `FrameDebugSnapshot` の既存経路を通じて `FramePipelineViewWidget` へ渡る。
- ライブ Pointwise 適用が成功したフレームだけ、意味を捏造しない独立診断パス `Composition.PointwiseEffects` を追加する。演算ノードの変換は行わない。
- ライブ Screen-Space GI のディスパッチが成功したフレームだけ、`Composition.ScreenSpaceGI` を独立診断パスとして追加する。`GIFrameContext` の捏造やGIアダプタの呼び出しは行わない。
- 実描画、GI 実行、Pointwise 実行は変更していない。GI / Pointwise のアダプタ接続と実機表示確認は未完了。
