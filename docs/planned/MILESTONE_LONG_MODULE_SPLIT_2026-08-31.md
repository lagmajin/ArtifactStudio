**最終更新:** 2026-08-31

**ステータス:** Not Started

# 実装案: 巨大 C++20 module / ソース分割

## 目的

`ArtifactCore` と `Artifact` の `.cppm` / `.ixx` に存在する **数千〜数万行の巨大ソースファイル**を責務単位に分割し、
- ビルド時間の局所化（部分再ビルドが効く粒度に）
- モジュール間の循環依存の予防
- コード理解とコードレビューのコスト低減
- 1ファイル = 1責務 / 1クラス の原則に近づける

を実現する。本マイルストーンは「分割対象の優先順位」「各ファイルの責務分解案」「新モジュール名（案）」「CMakeLists への影響範囲」を提示する。

実装は **リスクと波及の小さい Phase から段階的に**行い、各 Phase でビルド・主要ユニット・既存 fixture を壊さないことを完了条件とする。

---

## 現状とギャップ

### 行数ベースの問題ファイル

#### ArtifactCore（src実装）

| 順位 | ファイル | 行数 | import数 | 責務の複合度 |
|---|---|---|---|---|
| 1 | `src/Graphics/MeshRenderer.cppm` | 3,549 | 5 | 11責務（init/PSO/buffer/mesh-shader/matrix/instance/draw/shadow/env/material/light） |
| 2 | `src/Tracking/MotionTracker.cppm` | 2,575 | - | 単一クラスだが巨大（詳細未取得） |
| 3 | `src/Render/RenderFarmMaster.cppm` | 2,202 | - | ジョブ+RPC+HTTP+Alert+Auth+TLS+Checkpoint混在 |
| 4 | `src/Shape/ShapePath.cppm` | 1,792 | 6 | パス構築+変換+トポロジー+ポリゴン化+Simplify |
| 5 | `src/Geometry/MeshImporter.cppm` | 1,665 | - | 23メソッド |
| 6 | `src/Media/MediaPlaybackController.cppm` | 1,655 | - | 104メソッド、Playback/Source/Decode/Sync混在 |
| 7 | `src/Script/Expression/ExpressionEvaluator.cppm` | 1,458 | - | 単一責務、27メソッド |
| 8 | `src/Script/ArtifactScript.cppm` | 1,323 | - | スクリプトエンジン |
| 9 | `src/Text/TextShapingBackend.cppm` | 1,289 | - | Unicode検出60+フリーファンクション群 |
| 10 | `src/Shape/ShapeLayer.cppm` | 1,293 | 2 | レイヤー+SVG I/O+9プリミティブ生成+Factory |
| 11 | `src/Mesh/Mesh.cppm` | 1,376 | - | 80メソッド、Mesh data/Skinning/Tangent/UV/Topology/Simplify混在 |
| 12 | `src/Image/FFmpegEncoder.cppm` | 1,179 | - | 18メソッド |
| 13 | `src/Property/AbstractProperty.cppm` | 1,129 | 7 | 11責務+9 Factory（最優先） |
| 14 | `src/Physics/FluidSolver2D.cppm` | 1,207 | - | 16メソッド |
| 15 | `src/Physics/MpmSolver2D.cppm` | 1,132 | - | 39メソッド |
| 16 | `src/Physics/SoftBodySolver.cppm` | 964 | - | ソフトボディ |
| 17 | `src/Time/TimeRemap.cppm` | 1,090 | - | 単一責務 |
| 18 | `src/Color/ColorLUT.cppm` | 1,085 | - | LUT処理 |

#### ArtifactCore（ixxインターフェース）

| ファイル | 行数 |
|---|---|
| `include/AI/McpBridge.ixx` | 1,675 |
| `include/Core/ArtifactRegex.ixx` | 1,207 |
| `include/Container/SmallVector.ixx` | 926 |
| `include/Frame/FrameDebug.ixx` | 893 |
| `include/Render/PointwiseEffectFusion.ixx` | 867 |
| `include/Particle/ParticleSystem.ixx` | 840 |
| `include/AI/CommandIR.ixx` | 778 |
| `include/Diagnostics/Trace.ixx` | 755 |
| `include/Container/NamedVector.ixx` | 714 |
| `include/AI/CommandSandbox.ixx` | 689 |
| `include/Utils/PerformanceProfiler.ixx` | 652 |
| `include/Utils/WindowStyleCSS.ixx` | 600 |
| `include/AI/IDescribable.ixx` | 480 |
| `include/Container/NameMap.ixx` | 460 |

#### Artifact（src実装 — **行数が桁違い**）

| 順位 | ファイル | 行数 | import数 | 責務の複合度 |
|---|---|---|---|---|
| 1 | `src/Widgets/Render/ArtifactCompositionRenderController.cppm` | **28,100** | 40+ | Render Controller の名だが UndoCommand 20+, Gizmo/Rig/Motion/Shape/Camera/Puppet/CompositionChangeDetector/RenderPass/OnionSkinFrame/CompositionSpaceGpuCache 等全部入り。**明らかに神モジュール** |
| 2 | `src/Widgets/Render/ArtifactCompositionEditor.cppm` | **13,965** | 74 | TextEditorDialog/CubeFace/Edge/Corner/ViewOrientationWidget/CompositionOverlay/Paste/Solo/Cleanup/ImportPlacementSession 等。Composition編集のすべて |
| 3 | `src/Layer/ArtifactAbstractLayer.cppm` | 11,933 | 46 | メソッド253個、Cloner/Fracture/Liquid/MaskProperty/MotionTrail等 |
| 4 | `src/Widgets/ArtifactTimelineWidget.cppm` | 9,941 | - | タイムラインUI |
| 5 | `src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm` | 9,853 | - | トラック描画 |
| 6 | `src/Widgets/ArtifactInspectorWidget.cppm` | 8,573 | - | プロパティインスペクタ |
| 7 | `src/Render/ArtifactRenderQueueService.cppm` | 7,974 | - | レンダーキューサービス |
| 8 | `src/Widgets/ArtifactProjectManagerWidget.cppm` | 7,474 | - | プロジェクトマネージャUI |
| 9 | `src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` | 7,353 | - | レイヤーパネル |
| 10 | `src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` | 5,740 | - | Render Layer Widget v2 |
| 11 | `src/Composition/ArtifactAbstractComposition.cppm` | 5,312 | - | コンポジション抽象 |
| 12 | `src/Widgets/Asset/ArtifactAssetBrowser.cppm` | 5,226 | - | アセットブラウザ |
| 13 | `src/Widgets/Menu/ArtifactLayerMenu.cppm` | 4,860 | - | レイヤーメニュー |
| 14 | `src/Render/ArtifactIRenderer.cppm` | 4,704 | - | IRenderer |
| 15 | `src/Layer/ArtifactTextLayer.cppm` | 4,669 | - | テキストレイヤー |
| 16 | `src/Service/ArtifactProjectService.cppm` | 4,660 | - | プロジェクトサービス |
| 17 | `src/AppMain.cppm` | 4,230 | - | アプリエントリ |
| 18 | `src/Widgets/ArtifactMainWindow.cppm` | 3,508 | - | メインウィンドウ |
| 19 | `src/Widgets/Viewer/ArtifactContentsViewer.cppm` | 3,506 | - | コンテンツビューア |
| 20 | `src/Widgets/Render/TransformGizmo.cppm` | 3,392 | - | 変形ギズモ |
| 21 | `src/Service/ArtifactPlaybackService.cppm` | 3,345 | - | 再生サービス |
| 22 | `src/Layer/ArtifactShapeLayer.cppm` | 3,300 | - | シェイプレイヤー |
| 23 | `src/Layer/ArtifactParticleLayer.cppm` | 3,078 | - | パーティクルレイヤー |
| 24 | `src/Layer/ArtifactVideoLayer.cppm` | 2,864 | - | ビデオレイヤー |
| 25 | `src/Effect/ArtifactCreativeEffects.cppm` | 2,838 | - | クリエイティブエフェクト |

#### Artifact（ixxインターフェース）

| ファイル | 行数 |
|---|---|
| `include/AI/WorkspaceAutomation.ixx` | **6,176** — `WorkspaceAutomation`, `KeyframeSnapshotUndoCommand`, `CommandExecutorImpl` 等 nested class 多数 |
| `include/Layer/ArtifactCloneEffectSupport.ixx` | 1,309 |
| `include/Generator/ArtifactParticleGenerator.ixx` | 810 |
| `include/Layer/ArtifactLayerComponentSystem.ixx` | 802 |
| `include/Undo/UndoManager.ixx` | 770 |
| `include/Effects/Generator/ClonerGenerator.ixx` | 743 |
| `include/Widgets/ArtifactNativeDockSurface.ixx` | 677 |
| `include/Render/ArtifactRenderROI.ixx` | 646 |
| `include/Layer/ArtifactAbstractLayer.ixx` | 586 |
| `include/Render/ArtifactRenderContext.ixx` | 584 |
| `include/Widgets/PropertyEditor/ArtifactPropertyEditor.ixx` | 518 |
| `include/Render/ArtifactIRenderer.ixx` | 510 |

### 既存の分割済みパターン（参照）

良い分割例（再利用すべき命名規則・粒度）:

- **Audio**: 1 effect = 1 ファイル（`AudioCompressor`, `AudioDelay`, `AudioReverb`, `AudioChorus`, `AudioBassTreble`, `AudioHighLowPass`, `AudioParametricEQ`）。サブドメイン分割も: `Audio/Modulation/`, `Audio/DSP/`, `Audio/Spatial/`
- **Render**: 機能別分割（ジョブ管理、キュー、ファーム、ログ、チェックポイント、レイトレ、ボリューム、ノイズ、大気）
- **Layer**: 構造体別分割（`BlendModeInfo.ixx`, `LayerBlend.ixx`, `Opacity.ixx` を `Layer2D.cppm` から分離）
- **Graphics/Shader/Compute/**: HLSL/Computeシェーダ単位分割（`MaskPathRasterizerPipeline.ixx`, `MaskCutoutPipeline.ixx`, `LayerBlendPipeline.ixx`）
- **Container**: 1データ構造 = 1モジュール（`SmallVector.ixx`, `NamedVector.ixx`, `NamedList.ixx`, `NameMap.ixx`, `IdMap.ixx`）
- **Effects** (Artifact): 既に1 effect = 1ファイルが揃っている（`Effects/Blur/BlurEffect`, `Effects/Glow/GlowEffect` など）

命名規則: `<Domain>.<Entity>` のPascalCaseが標準。サブドメイン階層: `Audio.Modulation.Router`, `Graphics.Shader.Compute.LayerBlendComputeShader`。

---

## 分割対象と新モジュール名案

### Phase 1: 緊急（god-module、ビルド影響大）

#### 1-A. `ArtifactCompositionRenderController.cppm`（28,100行、import 40+）の分割

| 切り出し候補 | 想定新モジュール名 | 想定行数 | 切り出し基準 |
|---|---|---|---|
| UndoCommand 群（Gizmo/Rig/Motion/Shape/Camera/Puppet 全部） | `Artifact.Widgets.RenderController.UndoCommands` | ~3,000 | すべて `UndoCommand` 派生、ファイル先頭〜中盤に集中 |
| Render Pass pipeline（`RenderPass`, `FunctionalRenderPass`, `RenderPassExecutor`, `GpuBasePassState`, `GpuLayerBlendResult`） | `Artifact.Render.Controller.Passes` | ~2,500 | 描画パイプライン制御 |
| Gizmo 状態（`GizmoTransformSnapshot`, `GizmoGroupLayerState`, `GizmoGroupUndoEntry`） | `Artifact.Widgets.GizmoState` | ~1,500 | ギズモ関連状態 |
| Motion Path スナップショット系 | `Artifact.Widgets.MotionPathSnapshots` | ~800 | Motion Path 編集のsnapshot群 |
| Composition Change Detector | `Artifact.Render.CompositionChangeDetector` | ~500 | 単独クラス |
| Onion Skin / Precomp GPU cache | `Artifact.Render.OnionSkin` | ~600 | 専用 |
| Core `CompositionRenderController::Impl`（本体） | `Artifact.Widgets.CompositionRenderController`（**縮小版**） | ~15,000 | 上記切り出し後の核 |

**Phase 1-A 完了条件:**
- 公開API（`ArtifactCompositionRenderController` クラス）のメソッド・シグネチャが不変
- `include/Widgets/Render/ArtifactCompositionRenderController.ixx` の exportシンボルが不変
- `cmake/ArtifactSources.cmake` の登録が新モジュール分追加される
- 既存テスト (`ArtifactTestRenderQueue` 等) がパス

#### 1-B. `ArtifactCompositionEditor.cppm`（13,965行、import 74）の分割

| 切り出し候補 | 想定新モジュール名 | 想定行数 |
|---|---|---|
| Text Editor Dialog（`ArtifactTextEditorDialog`, `TextEditorState`） | `Artifact.Widgets.CompositionTextEditorDialog` | ~800 |
| Viewport Layout Button / Orientation Widget（Cube Face/Edge/Corner） | `Artifact.Widgets.ViewOrientationNavigator` | ~1,500 |
| Composition Overlay Widget（`CompositionOverlayWidget`, `EmptyCompositionOverlayWidget`） | `Artifact.Widgets.CompositionOverlayWidget` | ~1,200 |
| Drop Asset 系（`PendingDroppedAsset`, `ImportPlacementSession`） | `Artifact.Widgets.CompositionAssetDrop` | ~600 |
| Cleanup 系（`CompositionCleanupMove`, `CompositionCleanupCandidate`） | `Artifact.Widgets.CompositionCleanup` | ~400 |
| Core `ArtifactCompositionEditor::Impl` | `Artifact.Widgets.CompositionEditor`（**縮小版**） | ~9,000 |

**Phase 1-B 完了条件:** 1-A と同じ（公開API不変・テスト不変）。

#### 1-C. `ArtifactAbstractLayer.cppm`（11,933行、import 46、253メソッド）の分割

| 切り出し候補 | 想定新モジュール名 | 想定行数 |
|---|---|---|
| Cloner Transform 関連（`ClonerTransformOperation`, `ClonerTransformPropertyAddress`） | `Artifact.Layer.ClonerSupport` | ~500 |
| Fracture 関連（`FractureShardRenderPrimitive`, `FractureRenderElement`） | `Artifact.Layer.FractureSupport` | ~700 |
| Mask Property Address | `Artifact.Layer.MaskPropertyAddress` | ~300 |
| Motion Trail Ring Buffer | `Artifact.Layer.MotionTrailSupport` | ~400 |
| Liquid Layer Checkpoint | `Artifact.Layer.LiquidLayerCheckpoint` | ~300 |
| Layer Component Runtime Snapshot | `Artifact.Layer.ComponentSnapshot` | ~500 |
| Core `ArtifactAbstractLayer::Impl` | `Artifact.Layer.Abstract`（**縮小版**） | ~9,000 |

**Phase 1-C 完了条件:** `ArtifactAbstractLayer.ixx` の export が不変、各具象Layer（`ArtifactImageLayer.cppm` 等）からの import が不変。

### Phase 2: 高優先（複合責務クラス）

#### 2-A. `AbstractProperty.cppm`（1,129行、11責務）

| 切り出し候補 | 想定新モジュール名 | 想定行数 |
|---|---|---|
| Expression評価（`evaluateValue` の Expression関連） | `ArtifactCore.Property.Expression` | ~200 |
| Envelope 関連（`EnvelopeTrack`, `EnvelopePreset`, 9個の `make*EnvelopePreset`） | `ArtifactCore.Property.Envelopes` | ~350 |
| KeyFrame 関連（操作・補間） | `ArtifactCore.Property.Keyframes` | ~250 |
| Modulation 連携 | `ArtifactCore.Property.Modulation` | ~150 |
| Validation/Clamp/Range | `ArtifactCore.Property.Validation` | ~100 |
| Core `AbstractProperty` | `ArtifactCore.Property.Abstract`（**縮小版**） | ~500 |

**Phase 2-A のリスク:** Expression/Modulation/Envelope の依存方向整理が必要（循環依存回避）。Phase 1完了後に着手。

#### 2-B. `ShapePath.cppm`（1,792行、6 import）

| 切り出し候補 | 想定新モジュール名 | 想定行数 |
|---|---|---|
| Simplify（Douglas-Peucker等、~300行） | `ArtifactCore.Shape.Path.Simplify` | ~350 |
| Topology（`reverse`/`addPath`/clipping） | `ArtifactCore.Shape.Path.Topology` | ~350 |
| Polygon化（`earClipContour`/`mergeHoleIntoOuter`/穴埋め） | `ArtifactCore.Shape.Path.Polygon` | ~400 |
| Primitives（`setRectangle`/`setEllipse`/`setPolygon`/`setStar`） | `ArtifactCore.Shape.Path.Primitives` | ~250 |
| Core `ShapePath` | `ArtifactCore.Shape.Path`（**縮小版**） | ~500 |

#### 2-C. `ShapeLayer.cppm`（1,293行）

| 切り出し候補 | 想定新モジュール名 | 想定行数 |
|---|---|---|
| SVG I/O（`toSvg`/`fromSvg`、`SvgExportContext`、`elementToSvg`、`renderElement`） | `ArtifactCore.Shape.Layer.Svg` | ~350 |
| 9 個のプリミティブ生成 static | `ArtifactCore.Shape.Layer.Primitives` | ~250 |
| `ShapeLayerFactory` | `ArtifactCore.Shape.Layer.Factory` | ~50 |
| Core `ShapeLayer` | `ArtifactCore.Shape.Layer`（**縮小版**） | ~700 |

#### 2-D. `MeshRenderer.cppm`（3,549行、11責務）

| 切り出し候補 | 想定新モジュール名 | 想定行数 |
|---|---|---|
| Mesh Shader パス | `ArtifactCore.Graphics.MeshRenderer.MeshShader` | ~600 |
| PBR Material（テクスチャ 11個） | `ArtifactCore.Graphics.MeshRenderer.Material` | ~800 |
| Environment Map | `ArtifactCore.Graphics.MeshRenderer.Environment` | ~300 |
| Shadow Pass | `ArtifactCore.Graphics.MeshRenderer.Shadow` | ~300 |
| Buffer 管理（`createBuffers`/`updateMeshGeometry`/`updateMeshletGeometry`） | `ArtifactCore.Graphics.MeshRenderer.Buffers` | ~500 |
| PSO管理（`createPSO`） | `ArtifactCore.Graphics.MeshRenderer.PipelineState` | ~400 |
| Core `MeshRenderer` | `ArtifactCore.Graphics.MeshRenderer`（**縮小版**） | ~800 |

### Phase 3: 中優先（行数大だが責務単一寄り）

| 元 | 新モジュール名（案） | 想定行数 | 切り出し基準 |
|---|---|---|---|
| `src/Render/RenderFarmMaster.cppm` (2202) | `ArtifactCore.Render.Farm.Job` / `ArtifactCore.Render.Farm.RPC` / `ArtifactCore.Render.Farm.HTTP` / `ArtifactCore.Render.Farm.Alert` / `ArtifactCore.Render.Farm.Checkpoint` | 各400-600 | 機能別 |
| `src/Media/MediaPlaybackController.cppm` (1655) | `ArtifactCore.Media.Playback.Source` / `ArtifactCore.Media.Playback.Decode` / `ArtifactCore.Media.Playback.Sync` | 各300-500 | 機能別 |
| `src/Mesh/Mesh.cppm` (1376) | `ArtifactCore.Mesh.Data` / `ArtifactCore.Mesh.Skinning` / `ArtifactCore.Mesh.Tangent` / `ArtifactCore.Mesh.UV` / `ArtifactCore.Mesh.Topology` / `ArtifactCore.Mesh.Simplify` | 各100-300 | 機能別 |
| `src/Geometry/MeshImporter.cppm` (1665) | `ArtifactCore.Geometry.MeshImporter.<Format>` (obj, fbx, gltf, etc.) | 各200-400 | フォーマット別 |
| `src/Text/TextShapingBackend.cppm` (1289) | `ArtifactCore.Text.Shaping.Unicode` / `ArtifactCore.Text.Shaping.Layout` | 各400-600 | フリーファンクション群 |
| `include/AI/McpBridge.ixx` (1675) | 機能別分割 | 各300-500 | 機能別（未調査） |
| `include/Core/ArtifactRegex.ixx` (1207) | 機能別分割 | 各200-400 | 機能別（未調査） |
| `src/Script/Expression/ExpressionEvaluator.cppm` (1458) | `ArtifactCore.Script.Expression.Evaluator`（核） + `ArtifactCore.Script.Expression.Builtins` | 各600-800 | builtins別（43 builtin） |

### Phase 4: Widget側（Artifact）の細分化

`Artifact` 側は Phase 1-A〜1-C で核を縮小後、各 Widget の責務を整理：

| ファイル | 想定分割 |
|---|---|
| `ArtifactTimelineWidget.cppm` (9941) | `TimelineWidget.Core` + `TimelineWidget.KeyframeEditor` + `TimelineWidget.Selection` |
| `ArtifactTimelineTrackPainterView.cppm` (9853) | `TimelinePainter.Core` + `TimelinePainter.Layers` + `TimelinePainter.Keyframes` + `TimelinePainter.Playhead` |
| `ArtifactInspectorWidget.cppm` (8573) | `Inspector.Core` + `Inspector.PropertyTab` + `Inspector.EffectTab` + `Inspector.ComponentTab` |
| `ArtifactRenderQueueService.cppm` (7974) | `RenderQueue.JobModel` + `RenderQueue.Scheduler` + `RenderQueue.Progress` + `RenderQueue.FFmpegPipe` |
| `ArtifactProjectManagerWidget.cppm` (7474) | `ProjectManager.Core` + `ProjectManager.ListModel` + `ProjectManager.SaveLoad` |
| `ArtifactLayerPanelWidget.cppm` (7353) | `LayerPanel.Tree` + `LayerPanel.Selection` + `LayerPanel.ContextMenu` |
| `ArtifactAbstractComposition.cppm` (5312) | `AbstractComposition.Core` + `AbstractComposition.Render` + `AbstractComposition.Undo` |

### Phase 5: 大型 ixx（Artifact）

`include/AI/WorkspaceAutomation.ixx`（6,176行、nested class 多数）:

- `WorkspaceAutomation` 核: 1,500行程度
- `KeyframeSnapshotUndoCommand`: 単独モジュール化
- `CommandExecutorImpl`: 単独モジュール化
- 各種ヘルパー: 機能別分割

`include/Layer/ArtifactCloneEffectSupport.ixx`（1,309行）:

- `CloneEffectSupport` 核 + `ClonerEffectFactories` 分割

`include/Undo/UndoManager.ixx`（770行）:

- 現状は比較的小さいので優先度低、必要時のみ

---

## 完了条件

### Phase 1 共通
1. god-module 3ファイル（`ArtifactCompositionRenderController`, `ArtifactCompositionEditor`, `ArtifactAbstractLayer`）の各 `.cppm` が **それぞれ 5,000行以下**になる
2. `cmake/ArtifactSources.cmake` に新モジュールが登録され、ビルド成功
3. 既存テストが**全パス**（`ArtifactTestRenderQueue`, `ArtifactTestShapePath`, `ArtifactTestPropertyKeyframe`, `ArtifactTestLayerGroup`, `ArtifactTestPreCompose` 等）
4. 各新モジュールの依存方向が **一方向**（循環なし）
5. `ArtifactCompositionRenderController.ixx` 等の公開 export シンボル一覧が不変（呼び出し側の再ビルドが最小）

### Phase 2 共通
1. 各巨大ファイル（`AbstractProperty`, `ShapePath`, `ShapeLayer`, `MeshRenderer`）が **1,000行以下**になる
2. 責務ごとに1モジュール化の原則が確立
3. CMakeLists.txt への影響が CMakeLists.txt の `/reference:` チェーンに**追加のみ**で反映される

### Phase 3-5
- 行数目標: 主要巨大ファイルが全て **1,500行以下**になる
- 既存パターン（Audio, Render, Container）の粒度に揃う

### 全体
- C++20 module の**循環依存ゼロ**を維持
- ビルド時間の **局所化** が体感できる（修正1ファイルで関連モジュールだけが再ビルドされる）
- コードレビューの単位が **1責務 ≒ 500行 ±200** に収束

---

## Scope（想定する変更ファイル）

### 新規作成

ArtifactCore:
- `src/Graphics/MeshRenderer.*.cppm` 系（6個）
- `src/Shape/ShapePath.{Simplify,Topology,Polygon,Primitives}.cppm`
- `src/Shape/ShapeLayer.{Svg,Primitives,Factory}.cppm`
- `src/Property/{Expression,Envelopes,Keyframes,Modulation,Validation}.cppm`
- `src/Render/RenderFarm.{Job,RPC,HTTP,Alert,Checkpoint}.cppm`
- `src/Media/Playback.{Source,Decode,Sync}.cppm`
- `src/Mesh/{Data,Skinning,Tangent,UV,Topology,Simplify}.cppm`
- `src/Text/Shaping.{Unicode,Layout}.cppm`

Artifact:
- `src/Widgets/Render/CompositionRenderController.{UndoCommands,Passes,GizmoState,MotionPathSnapshots,OnionSkin}.cppm`
- `src/Widgets/Render/CompositionEditor.{TextEditorDialog,ViewOrientationNavigator,OverlayWidget,AssetDrop,Cleanup}.cppm`
- `src/Layer/{ClonerSupport,FractureSupport,MotionTrailSupport,LiquidLayerCheckpoint,ComponentSnapshot,MaskPropertyAddress}.cppm`
- `src/Widgets/Timeline/{TimelineWidget,TimerPainterView} 系分割
- `src/Widgets/{Inspector,ProjectManager,LayerPanel,MainWindow,TileWidget} 系分割
- `src/Widgets/AI/WorkspaceAutomation.{Core,Snapshots,Executor}.cppm`
- `src/Render/RenderQueue.{JobModel,Scheduler,Progress,FFmpegPipe}.cppm`

### 修正

- `ArtifactCore/CMakeLists.txt` — `/reference:` チェーンへの追加
- `Artifact/cmake/ArtifactSources.cmake` — 同上
- 各 god-module `.cppm` — 核のみ残す
- 各対応 `.ixx` — export の調整（必要な型を再 export または前方宣言に変更）

---

## リスク・注意事項

1. **W_OBJECT / W_SIGNAL**: Q_OBJECT派生クラスは分割時、`W_OBJECT_IMPL(...)` の整合確認が必須。`signal`/`slot` を含むメソッドを別モジュールに動かすと接続が切れる可能性。
2. **PImpl のデストラクタ位置**: `std::unique_ptr<Impl>` をヘッダで持つ場合、デストラクタは `.cppm` 側で定義する必要がある（`ArtifactCompositionRenderController` の `Impl` を分割する場合に要検討）。
3. **CMakeLists の再ビルド影響**: god-module を分割すると依存モジュールの再スキャンが発生。Ninja dyndep の動作が不安定化する可能性あり。**分割はビルドが通った状態で動作確認しながら進める**。
4. **`export import` の連鎖**: 新規分割モジュールが `export import` で既存モジュールを取り込むと、循環の温床になる。**前方宣言 + 必要に応じてimport** のパターンを徹底。
5. **CRLF/LF**: 新規ファイルは LF 強制（AGENTS.md の「LF統一」ルール）。
6. **`ArtifactCompositionRenderController` の「Render Controller の責務か」問題**: 内部に Gizmo/Rig/Motion があるのは疑義あり。本来は別モジュールが所有すべき責務が同居している可能性。Phase 1-A の分割時に「誰が owner か」を再判定する。
7. **命名衝突**: 新モジュール名が既存 `.ixx` のモジュール名と衝突しないか CMakeLists 登録前に必ず確認。
8. **`include/AI/WorkspaceAutomation.ixx` (6,176行) の nested class 多数**: nested class は独立モジュール化が困難。**Phase 5 で先行着手せず、Phase 1〜3 完了後に判断**。
9. **`AppMain.cppm` (4,230行)**: エントリポイントの巨大化は構造的問題。`Application::Bootstrap`, `Application::Startup` 等への関数分割は可能だが、mainloop/初期化順序の依存があるため Phase 4 以降。
10. **ビルド検証未実施**: AGENTS.md 制約により、本マイルストーンの実装途中でビルド・テストはユーザー許可が必要。**各 Phase 完了時に「ビルドしたいか」確認する**。

---

## 関連ファイル

- `docs/DOC_LIFECYCLE.md`
- `ArtifactCore/CMakeLists.txt` — `/reference:` チェーン定義
- `Artifact/cmake/ArtifactSources.cmake` — ソースマニフェスト
- `.github/GIT_WORKFLOW_PARENT_CHILD.md` — submodule運用
- 既存分割済みモジュール（参照パターン）: `src/Audio/`, `src/Render/`, `src/Container/`, `src/Graphics/Shader/Compute/`, `include/Layer/`

---

## 関連マイルストーン（既存/計画中）

- `docs/planned/MILESTONE_TEXT_SYSTEM_2026-03-12.md` — Text 系の Core 整備
- `docs/planned/MILESTONE_ARTIFACTSCRIPT_BINDING_AND_CLASS_2026-08-28.md` — ArtifactScript 拡張
- `docs/planned/MILESTONE_TIMELINE_DCC_FEEL_GAPS_2026-08-29.md` — Timeline UI 改善

---

## 次の確認事項

1. **Phase 1-A（CompositionRenderController 分割）から着手**して良いか？あるいは Phase 2（`AbstractProperty` 等）からの方がリスク低い？
2. 各 Phase のビルド検証を**ユーザー側で実施**していただける前提で進めて良いか？
3. god-module 3ファイル以外で**最優先**としたい分割対象はあるか？（例: `RenderQueueService`、`TimelineWidget`、`InspectorWidget` 等）
4. `include/AI/WorkspaceAutomation.ixx`（6,176行）の分割は Phase 5 まで後回しで良いか？それとも早期着手？