# Milestones Backlog

### Color / Professional Media
- **M-PRO-MEDIA-1** Professional Media Materials Support
  - EXR/HDR、広色域、高ビット深度、log素材のメタデータ保持と明示的な解釈経路
  - Phase 1（OIIO ingest metadata）とPhase 2の入力解釈オーバーライド基盤を実装済み。入力色空間・transfer・HDR/log判定を `RawImage` に保持し、`SourceInterpretOverride` で明示指定できる
  - 詳細: `docs/planned/MILESTONE_PROFESSIONAL_MEDIA_MATERIALS_2026-07-16.md`


### Layer Component / Simulation
- **M-LC-1** Layer Component Pipeline / Simulation Contract
  - `cloner / layout / crowd / physics / fracture / emit` を phase-based に矛盾なく接続する
  - preview fallback と authoritative simulation を分離し、将来の crowd / rigid / soft-body / pyro / bake に耐える土台を作る
  - 詳細: `docs/planned/MILESTONE_LAYER_COMPONENT_PIPELINE_2026-07-01.md`

### Composition / Workflow
- **M-LCW-1** Quick Layer Creation Dialog
  - 素材・平面・簡単なマスク・入退場 Envelope を一回の確定で作成する新規ダイアログ
  - 透明度とエフェクト強度の同時 / 先行 / 遅延プリセットを提供する
  - 既存の `CreateSolidLayerSettingDialog` は維持し、作成オーケストレーターだけを新設する
  - 詳細: `docs/done/MILESTONE_QUICK_LAYER_CREATION_DIALOG_2026-07-10.md`

- **M-PRECOMP-2** Precompose Workflow Completion
  - `PreCompose` の「呼べる」状態から、`unprecompose()` を含む実務 finish line まで閉じる
  - layer restore、time/range integrity、undo/redo、`Master Properties` 前提の責務境界を固める
  - 詳細: `docs/done/MILESTONE_PRECOMPOSE_WORKFLOW_COMPLETION_2026-07-09.md`

- **M-LC-2** Generator / Modifier / Field Stack Migration
  - `single cloner` 前提から、複数 generator と独立 field stack を持つ構造へ段階移行する
  - `component.cloner.*` 互換を維持しつつ、`generators[] / modifiers[] / fields[]` の内部モデルへ寄せる
  - 詳細: `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`

- **M-LC-3** Live Field Authoring UX
  - 先行実装済みの live radial field を、viewport direct manipulation と field stack 操作まで引き上げる
  - center / radius drag、active field 選択、strength / blend / invert、shape 拡張の順で進める
  - 詳細: `docs/planned/MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md`

### Core / Type System
- **M-CORE-5** Custom Collections Design
  - `std` / `Qt` の境界を薄くしつつ、用途ごとの自前コレクションへ段階移行する
  - `Array<T>`, `String`, `Ptr<T>`, `Ref<T>`, `Owned<T>` の最小 API と採用順を整理する
  - 詳細: `docs/planned/MILESTONE_CUSTOM_COLLECTIONS_DESIGN_2026-07-04.md`

- **M-CORE-6** Domain Type Wrappers
  - 素のテンプレート露出を避け、`LayerList`, `EffectRegistry`, `PropertyBag` のような自己文書化型へ包む
  - `add()`, `remove()`, `contains()`, `get()` を揃え、暗黙変換を避ける
  - 詳細: `docs/planned/MILESTONE_DOMAIN_TYPE_WRAPPERS_2026-07-04.md`

- **M-CORE-7** std → Qt Migration Plan
  - `vector`, `mutex`, `string`, `shared_ptr` などの置換優先度と例外を整理する
  - 置換しない標準型も明文化して、混在を減らす
  - 詳細: `docs/planned/MILESTONE_STD_TO_QT_MIGRATION_2026-07-04.md`

- **M-CORE-8** Core Keyframe / Property Update Hardening ✅ (static verified 2026-07-12)
  - `AbstractProperty` / `AnimatableValueT` / `Core.KeyFrame` の多重実装を整理し、時刻比較・値検証・評価の堅牢性を上げる
  - まずは現状挙動を固定する回帰テストから入り、`RationalTime` の正規化比較とスレッド安全な評価へ段階移行する
  - 詳細: `docs/done/MILESTONE_CORE_KEYFRAME_ROBUSTNESS_2026-07-10.md`

空いている時間に進めやすいよう、分野別に小さめのマイルストーンへ分割したバックログ。

## Completed Milestones (2026-04-14 verified)

以下は実装確認済みの完了マイルストーン。詳細は各マイルストーン文書を参照。

### Diagnostics / Profiling
- **M-DIAG-1** Audio Engine Profiler ✅ (2026-04-15)
  - `AudioEngineProfiler` lock-free singleton, callback timing, fill-loop timing, buffer level
  - `ProfilerPanelWidget` に "Audio Engine" セクション + Reset ボタン追加 (Ctrl+Shift+D)
  - 主要ファイル: `ArtifactCore/include/Utils/PerformanceProfiler.ixx`, `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`

- **M-DIAG-2** EventBus Debugger ✅ (2026-04-15)
  - `EventBus`: PublishHook, type-name registry, `forEachRegisteredType`, `registerTypeNameRaw`
  - `EventBusDebugger`: attach/detach, fire log (ring buffer), subscriber snapshot, frequency snapshot
  - `EventBusDebuggerWidget`: 3-tab UI — Fire Log / Subscribers / Frequency (Ctrl+Shift+E)
  - 主要ファイル: `ArtifactCore/include/Event/EventBusDebugger.ixx`, `Artifact/src/Widgets/Diagnostics/EventBusDebuggerWidget.cppm`

- **M-DIAG-3** Lightweight Tracer / Frame Timeline
  - crash stack / scope tracer / frame timeline / thread trace を超軽量でまとめる
  - `Render / Decode / UI / Event` を frame ごとに並べる
  - 主要ファイル: `ArtifactCore/include/Diagnostics/*`, `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
  - 詳細: `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_2026-04-21.md`
  - 実行メモは親文書へ統合済み

- **M-DIAG-4** Live Frame Pipeline / Resource Watcher / State Diff Tracker
  - Pass DAG / RT・Texture・Buffer lifetime / barrier hazard を常時追う
  - 任意 resource のライブ inspector と pixel inspect を持つ
  - 前フレームとの差分から壊れ始めた瞬間を自動検出する
  - 主要ファイル: `ArtifactCore/include/Render/*`, `Artifact/src/Widgets/Diagnostics/*`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - 詳細: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`
  - 実行メモは親文書へ統合済み

### Project View / Asset System
- **M-PV-1** Project View Basic Operations ✅ (verified 2026-04-14)
  - selection center/quick actions/sync chip/inline rename 実装済み
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`
  - マイルストーン文書 `MILESTONE_PROJECT_VIEW_INTERACTION_POLISH` は内容完了につき `docs/done/` へ移動済み (2026-06-23)

- **M-PV-2** Project View Asset Presentation ✅ (verified 2026-04-14)
  - selection summary/detail、HoverThumbnailPopup 実装済み
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`

- **M-AS-4** Asset System Integration ✅ (verified 2026-04-14)
  - sync chip両方向に配置済み、Asset Browser↔Project View 往復同期
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`, `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

### UI / Property Editor
- **M-UI-23** Property Widget Row Alignment ✅ (verified 2026-04-14)
  - Phase 1-2完了、row bg/hover/keyframe chromeをowner-draw化
  - 主要ファイル: `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`

- **M-ACC-1** Accessibility and Left-Handed UI Support
  - 左利き向け補助、片手操作補助、視認性補助、障碍者向け補助をまとめて整理する
  - Phase 1: 利き手設定の土台、ヒット領域調整、主要 widget からの参照
  - 詳細: `docs/planned/MILESTONE_ACCESSIBILITY_AND_LEFT_HANDED_UI_2026-06-28.md`

- **M-UI-27** Color Constraint Rules / Palette-Conform Correction
  - `main / accent / background` などのデザイントークンに対して、比較演算子 + 値でルールを GUI 編集できるようにする
  - ルール違反時の warning / block と、選択色を最寄りパレット色へ補正する導線をまとめる
  - `ArtifactColorSciencePanel` か `PropertyEditor` のどちらに寄せるかは責務確認後に確定する
  - 詳細: `docs/planned/MILESTONE_COLOR_CONSTRAINT_RULES_2026-06-07.md`

- **M-UI-3** Inspector Usability ✅ (verified 2026-04-14)
  - キーボードショートカット/ステータスバー/レイヤーラベルカラー/整列分布機能
  - 主要ファイル: `Artifact/src/Widgets/ArtifactAlignmentWidget.cppm`, `Artifact/src/Widgets/ArtifactStatusBar.cpp`

- **M-UI-5** Contents Viewer Expansion ✅ (verified 2026-04-14)
  - テキストレイヤーインライン編集実装済み、Ctrl+Enter commit shortcutあり
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

### Layer Editing
- **M-LE-1** Layer Solo View 編集機能強化（平面・シェイプ） ✅ 全 Phase 完了 (2026-06-26)
  - Phase 1: シェイプ固有ビューポートハンドル（角丸・星内半径） ✅ 実装済み
  - Phase 2: グラデーションフィル（シェイプ・平面レイヤー + プロパティピッカー） ✅ 実装済み (2026-06-26)
  - Phase 3: ストロークスタイル（破線・端点・接合・配置） ✅ 完了 (2026-06-23)
  - Phase 4: ギズモ XYWH 数値 HUD オーバーレイ ✅ 完了 (2026-06-26)
  - Phase 5: シェイプ頂点ベジェカーブ編集 ✅ 完了 (2026-06-26)
  - 詳細: `docs/done/MILESTONE_LAYER_EDIT_2026-04-25.md`
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`, `Artifact/src/Layer/ArtifactShapeLayer.cppm`, `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyEditor.cppm`, `Artifact/src/Widgets/Render/TransformGizmo.cppm`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`

- **M-LE-2** Layer Transform からの Crop / Pan 導線と Source Reframe 透明化 ✅ (2026-06-24 完了)
  - `Layer Transform` 直下に `Add Crop / Pan` を出し、既存の `Source Reframe` を再利用する
  - `SourceCrop` の crop 外側を透明として扱い、元サイズのレイヤー寸法を維持する
  - 詳細: `docs/done/MILESTONE_LAYER_SOURCE_REFRAME_NLE_2026-06-24.md`
  - 主要ファイル: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`, `Artifact/src/Layer/ArtifactImageLayer.cppm`, `Artifact/src/Layer/ArtifactVideoLayer.cppm`, `Artifact/src/Layer/ArtifactSourceCrop.cppm`

- **M-LE-3** 2D Shape Modeling Editing
  - `ArtifactShapeLayer` を primitive layer から `editable path + modifier stack` を持つ 2D モデリング対象へ昇格させる
  - `Shape Edit` mode、vertex/segment/tangent editing、`Convert To Editable Path`、shape operator の modifier UX を段階導入する
  - 詳細: `docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md`

### Composition Editor / Cache
- **M-CE-1** Composition Editor Cache System ✅ (verified 2026-04-14)
  - Surface cache / render key suppression / ROIシステム実装済み
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- **M-CE-2** Static Layer GPU Cache ⚠️ (partial - 2026-04-14)
  - マイルストーン文書と設計は存在するが、専用GPU cacheクラスの実装は未確認
  - `PrimitiveRenderer2D` の cacheKey ベース最適化は実装済み
  - 主要ファイル: `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`

- **M-RAM-3** Preview Range Policy and Priority ✅ (verified 2026-06-26)
  - `ramPreviewPriorityReason()` / `ramPreviewPriorityState()` / `orderedRamPreviewFramesForRange()` in `ArtifactPlaybackService.cppm`
  - immediate / near / directional / safety-backfill / work-area / out-of-range の priority reason 実装済み
  - 再生方向バイアス、一時停止中 warmup、work area 判定まで完了

- **M-CE-CRIT-1** Critical Render / Media Stability Program ✅ (verified 2026-06-26)
  - マイルストーン文書 `docs/done/` へ移動済み

### Debug / Regression Surface
- **M-CE-CRIT-2** Debug Render Harness ✅ 既存 regression surface
  - particle-only / video-only / blend-only / overlay-only / mixed-media の最小再現 surface は実装済み
  - `AppMain` から独立 dock として開け、`AppDebuggerWidget` からも同じ frame snapshot vocabulary を読める
  - 既存の regression surface として `M-CE-CRIT-1` の診断・回帰確認に使う
  - 詳細: `docs/planned/MILESTONE_DEBUG_RENDER_HARNESS_2026-04-30.md`

### M-DIAG-6 Harness Engineering / Goal-First Loop
- `Debug Render Harness` と `App Debugger` の report vocabulary を goal-first に揃える
- `goal / expected / actual / next action` を共通の作業単位にする
- 詳細: `docs/planned/MILESTONE_HARNESS_ENGINEERING_2026-05-12.md`
- 実行メモは親文書へ統合済み

### Workstream Containers

以下は個別機能ではなく、複数 milestone を束ねる進行管理の器。  
active milestone の重複名としては扱わない。

- `M-OPS-1` Active Implementation Triad
- `M-OPS-2` Continuation Sprint
- 詳細: `docs/planned/MILESTONE_ACTIVE_IMPLEMENTATION_TRIAD_2026-05-12.md`
- 詳細: `docs/planned/MILESTONE_CONTINUATION_SPRINT_2026-05-20.md`

### Render Execution / Isolation
- **M-RE-1** External Renderer Design ✅ Phase 1 closeout
  - 内蔵レンダラは維持しつつ、オフラインレンダリングだけ別プロセスへ切り出す
  - job snapshot / CLI / progress / diagnostics の設計を先に固める
  - 詳細: `docs/planned/MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md`
  - Phase 1 completion: `docs/done/MILESTONE_EXTERNAL_RENDERER_DESIGN_PHASE1_2026-06-25.md`

### Creative Effects / Exploratory Backlog
- **M-FX-FORM-1** Form Grid Particle Layer
  - Trapcode Form 風の grid / point-cloud particle layer を、既存 `ParticleSystem` / `ClonerGenerator` とは独立した generator layer として設計する
  - 既存資産との統合は renderer reuse に留め、`ParticleRenderData` / `ArtifactIRenderer::drawParticles()` だけを共有する
  - 詳細: `docs/planned/MILESTONE_FORM_GRID_PARTICLE_LAYER_2026-06-26.md`

- **M-FX-EXP-1** New Image Effect Exploration
  - 既存の `blur / glow / chromatic aberration` から少し離れた、Artifact らしい画像エフェクト案を保管する
  - まずは look-dev 用の発想メモとして扱い、制作体験に効くものから実装候補へ昇格させる
  - `Temporal Fossil`
  - `Pigment Separation`
  - `Surface Memory`
  - `Depth Melt`
  - `Edge Echo`
  - `Light Pressure`
  - `LuminescenceCaustics`
  - `QuantumGlitch / WavefunctionCollapse`
  - `DynamicFluidVortex`
  - `ReactionDiffusionStylizer`
  - `VectorFlowGlitch`
  - `AnisotropicFlowBlur`
  - `ReactionDiffusionBlur`
  - `ApertureShapeBlur`
  - `Glow Variants Pack`
  - `Chromatic Relief`
  - `Signal Collapse`
  - `Ink Delay`
  - `Atmospheric Slicing`
  - 実装候補メモ:
    - `Temporal Fossil`: 過去フレームの輪郭や色を薄く堆積させる。モーションブラーではなく時間の層を見せる方向
    - `Pigment Separation`: RGB 分離ではなく、顔料やインクのにじみとして色がほどける方向
    - `Surface Memory`: 素材表面に前の像の痕跡が焼き付く。キャンバス、金属、ガラスなど質感差を活かしやすい
    - `LuminescenceCaustics`: 輪郭やハイライトから集光の網目を生成する。液体金属、氷、クリスタル、魔法オーラ向け
    - `QuantumGlitch / WavefunctionCollapse`: タイルと隣接ルールで画像を再構成する。破壊ではなく自己構成の抽象コラージュ向け
    - `DynamicFluidVortex`: 流体速度場で画像を移流させる。インク、水流、渦、粘性のある歪み向け
    - `ReactionDiffusionStylizer`: 反応拡散で有機的パターンを生成する。キリン柄、シマウマ、指紋、サンゴ向け
    - `VectorFlowGlitch`: 輪郭や流れに沿って引き裂く。構造テンソルや動き場に追従する知的グリッチ向け
    - `AnisotropicFlowBlur`: 構造テンソルで方向場を取り、流れに沿ってだけぼかす。髪、木目、水流、美肌向け
    - `ReactionDiffusionBlur`: 拡散しながら模様が育つ。溶けるトランジションや細胞分裂風のブラー向け
    - `ApertureShapeBlur`: 任意マスクを PSF にするレンズボケ。ハート型玉ボケや汚れたレンズ向け
    - `Glow Variants Pack`: 輪郭発光、色収差、残光、液体感などを分けた発光亜種群
  - Core library 候補:
    - `Temporal Fossil` は frame history / accumulation 基盤を持てるなら `ArtifactCore` 側に置く価値が高い
    - `Pigment Separation` は CPU reference と GPU backend の両方を作りやすく、creative effect pack に馴染みやすい
    - `Surface Memory` は texture/history/mask を跨ぐので、effect host contract が固まってから `ArtifactCore` 候補として再評価する
    - `LuminescenceCaustics` は `Final Effect` または shared bus 寄りの投影表現に昇格しやすいので、最初は stylized rasterizer として検証する
    - `QuantumGlitch / WavefunctionCollapse` は `Mosaic` / `AutoMosaic` / tile 系資産と相性が良いので、まずは rasterizer として評価する
    - `DynamicFluidVortex` は `FluidSolver2D` / `FluidVisualizer` / `FluidForce` と相性が良いので、まずは fluid solver 連携の effect として評価する
    - `ReactionDiffusionStylizer` は `FluidSolver2D` の低解像度格子思想を流用しやすいので、まずは stylized rasterizer として評価する
    - `VectorFlowGlitch` は `StructureTensor` / `Distortion` / `ChromaSpread` と相性が良いので、まずは edge-aware rasterizer として評価する
    - `AnisotropicFlowBlur` は `StructureTensor` の方向場をそのまま使えるので、まずは edge-preserving blur として評価する
    - `ReactionDiffusionBlur` は `FluidSolver2D` / `AnisotropicFlowBlur` / `Distortion` と相性が良いので、まずは transition-oriented blur として評価する
    - `ApertureShapeBlur` は FFT 系の基盤と aperture UI と相性が良いので、まずは PSF-driven blur として評価する
    - `Glow Variants Pack` は `Glow` / `DirectionalGlow` / `ChromaSpreadGlow` / `Halation` と相性が良いので、まずは glow family として評価する
  - 関連:
    - `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` の `C-GFX-1 Creative Effect Base`
    - `ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md` の `C-GFX-2 Creative Effect Pack`

### Export / Review
- **M-FE-2** Export / Review / Share ✅ Phase 1完了 (2026-06-23)
  - Copy Path追加、Reveal/Open/Historyは既存、マイルストーン文書は `docs/done/` へ移動済み

### AI / Tooling
- **M-AI-1** MCP/Tool Bridge ✅ Phase 1完了 (verified 2026-04-14)
  - McpBridge::handleRequest() / AIContext 実装済み
  - 主要ファイル: `ArtifactCore/include/AI/McpBridge.ixx`

- **M-AI-2** AI Command Sandbox ✅ (verified 2026-04-14)
  - CommandSandbox.ixx（674行）で policy/execution/timeout すべて実装済み
  - 主要ファイル: `ArtifactCore/include/AI/CommandSandbox.ixx`

- **M-CMD-1** Command IR / Automation Foundation
  - AI / MCP / DSL / Python から low-level API を直叩きさせず、Command IR を正規の automation 入口にする
  - Primitive Command と Macro Command の二層を前提に、validation / transaction / Undo 単位を固定する
  - 詳細: `docs/planned/MILESTONE_COMMAND_IR_AUTOMATION_FOUNDATION_2026-06-28.md`

### Asset Browser
- **M-AB** Asset Browser Improvement (Unity風) ✅ (verified 2026-04-14)
  - Icon/List切替実装済み（viewModeButton）、Name/Date/Size/Typeソート、Status filter
  - 主要ファイル: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

## Widget Ownership Guardrails

このバックログで milestone を触るときは、まず次の責務境界を確認する。

- `ArtifactContentsViewer`: 内容閲覧 / compare / recent sources / mode routing
- `ArtifactAssetBrowser`: ファイル探索 / サムネイル / favorites / project bridge
- `ArtifactCompositionEditor`: composition 編集 / viewport 操作 / playback
- `ArtifactTimelineWidget`: タイムライン全体の orchestration
- `ArtifactLayerPanelWidget`: タイムライン左ペインの行操作
- `ArtifactPropertyWidget` / `PropertyEditor`: property row の編集
- `ArtifactInspectorWidget`: summary / selection / effect stack の窓口

境界が曖昧な場合は、`docs/WIDGET_MAP.md` を先に更新してから milestone を触る。

## Application

### M-ARCH-1 Host / Context / ROI / Property Core
- render context / property registry / effect host contract / ROI partial evaluation を段階導入する
- まずは無挙動変更で入れやすい read-only registry / adapter を優先する
- 詳細は `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_2026-04-20.md`
- AE 1.0 向けの必須/重要/後回し仕分けと 6 か月順は `docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`
- Month 1 の実行順は `docs/planned/MILESTONE_AE1_0_MONTH1_EXECUTION_2026-04-20.md`
- 実行メモは親文書へ統合済み

### M-WKR-1 Background Utility Worker Process
- サムネイル / waveform / proxy / メタデータ抽出 / preflight / autosave / log collection などの雑用を、専用 worker process に段階分離する
- まずは共通 job contract と in-process runtime を作り、その後 protocol と外部プロセスへ進める
- 詳細は `docs/planned/MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md`
- Phase 1 の実装表は同文書内の `実装表 A` を参照
- Phase 2-5 は `job contract -> scheduler -> facade -> protocol -> dedicated worker process` の順で進める

### M-CORE-4 Module Hygiene / Build Stabilization
- module boundary / Qt type / STL numeric helper / API compatibility をまとめて安定化する
- いま出ている `SessionLedger` / `Property` / `LayerMatte` / `ArtifactRenderROI` / `Acoustic` 系の compile break を代表例として扱う
- 詳細は `docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_2026-04-21.md`
- 実行メモは親文書へ統合済み

### M-APP-1 Application Cross-Cutting Improvement
- menu / toolbar / shortcut / view / diagnostics / workflow を横断で揃える
- central widget の横幅不足と下部パネルの高さ不足を layout issue として追跡
- 詳細は `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`

### M-APP-2 Deferred UI Initialization / Lazy Load
- icon / thumbnail / viewer / dock の eager load を減らして初回体感を軽くする
- 詳細は `docs/planned/MILESTONE_DEFERRED_UI_INITIALIZATION_2026-03-27.md`

### M-APP-3 Frame Debug View / Simple RenderDoc-like
- 1 フレームを固定して pass / resource / attachment / compare / step を追える内蔵フレームデバッグビューを作る
- 詳細は `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`

### M-APP-3a Frame Debug Goal-First Summary
- `FrameDebugViewWidget` の上部サマリを `goal / frame / warning / next` に固定する
- harness report と同じ語彙でフレーム単位の判断を読めるようにする
- 詳細: `docs/planned/MILESTONE_FRAME_DEBUG_GOAL_FIRST_SUMMARY_2026-05-12.md`
- 実行メモは親文書へ統合済み

### M-APP-4 App Debugger Visual Hierarchy / Color Semantics
- App Debugger の情報階層、色の意味、異常時の見え方を整えて、人間が読みやすい diagnostics surface に寄せる
- 詳細は `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`
- 実行メモは親文書へ統合済み
- Phase 5-12 は first-glance / focus / report / legend / quick actions / auto focus / session history / render cost の派生 slice として進める

### M-APP-4a App Debugger Goal-First Summary
- `AppDebuggerWidget` の上部サマリを `goal / now / warning / next` で固定する
- harness report と同じ語彙で作業面を読めるようにする
- 詳細: `docs/planned/MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md`
- 実行メモは親文書へ統合済み

### M-APP-5 Project Health / Problem View Wiring
- `ArtifactProjectHealthChecker` と `ArtifactProblemViewWidget` / `ArtifactProjectHealthDashboard` の語彙を揃える
- `DiagnosticEngine` と smoke gate の failure vocabulary を合わせる
- 詳細は `docs/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`
- 実行メモは親文書へ統合済み

### M-APP-6 App Surface Cohesion
- Project / Asset / Timeline / Composition / Contents Viewer / Inspector / Debugger の current / recent / selection / status を揃える
- empty state と summary strip の文法をアプリ全体で統一する
- 詳細は `docs/planned/MILESTONE_APP_SURFACE_COHESION_2026-05-13.md`

### M-APP-7 App Diagnostic Cohesion
- Project Health / Problem View / App Debugger / Frame Debug View / harness report の diagnostics 文法を揃える
- warning / error / next action を surface 横断で統一する
- 派生 slice は同じ文法で読む
- 詳細は `docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`
- 実行メモは親文書へ統合済み

### M-IR-8 ImmediateContext Boundary / De-direct
- `DiligentEngine` の `ImmediateContext` / `IDeviceContext` を layer / widget / controller から直接触らない構造へ寄せる
- `ArtifactIRenderer` / `RenderCommandBuffer` / `DiligentImmediateSubmitter` を正式な描画境界として固定する
- 詳細は `docs/planned/MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`

### M-IR-9 Render Boundary Safety Gate
- 境界変更を壊れにくい順序で進めるための安全ゲート
- いったん置いておく対象と再開順を固定する
- 詳細は `docs/planned/MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md`

### M-DIAG-5 Startup Thread Churn / Worker Burst Trace
- 起動直後 / 初回コンポ表示時の worker thread burst を trace で可視化する
- `sharedBackgroundThreadPool()`、video/image/svg prefetch、render scheduler、playback worker の寄与を切り分ける
- 詳細は `docs/planned/MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md`

## AI / Tooling

### M-AI-0 AI Tooling Expansion
- AI の読み取り、提案、安全な write tool、自動化を一本化するマスター方針
- まずは `AIContext` / description catalog / inspection tool を強化し、その後に safe write tool と creative assist を広げる
- creative assist は `keyframe suggestion` と `color grading suggestion` を先行させる
- 詳細は `docs/planned/MILESTONE_AI_TOOLING_EXPANSION_2026-04-21.md`
- 推奨順は `read -> safe write -> keyframe suggestion -> color grading suggestion -> automation`

### M-AI-2 Safe Write Tools
- AI の提案を確認付きで編集へ反映する安全な write surface
- `ArtifactProjectService` / `ArtifactEffectService` / render queue service を再利用する
- 詳細は `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_2026-04-21.md`
- 実装メモは親文書へ統合済み

### M-AI-6 Workflow Automation
- `WorkspaceAutomation` を中心に project / composition / selection / render queue を束ねる
- 詳細は `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_2026-04-21.md`
- 実装メモは親文書へ統合済み

### M-AI-2 AI Command Sandbox / CLI Execution
- AI 縺ｫ縺ｯ shell string 縺ｧ縺ｪ縺上↑繧峨〒縺・、program + argv 繧帝攝縺励※謇薙∴繧・
- allowlist / timeout / working directory / output cap 繧定ｨ倬鹸縺励※縲∝ｧ・ｭｷ螟夜Κ繧ｳ繝槭Φ繝峨ｒ縺ｾ縺・☆繧・
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_AI_COMMAND_SANDBOX_2026-04-10.md`

### M-AI-1 MCP / Tool Bridge Foundation
- `DescriptionRegistry` / `AIToolExecutor` / `AIContext` を使って AI tool schema を安定化する
- local / cloud / 将来の MCP bridge から共通で使える tool 境界を切る
- 詳細は `docs/planned/MILESTONE_AI_MCP_TOOL_BRIDGE_2026-04-10.md`
- 実行メモは親文書へ統合済み

### M-AI-2 Cloud UI Compact View / Settings Split
- Cloud AI の詳細設定を dialog 側へ寄せ、常時表示を減らす
- `ArtifactAICloudWidget` を compact view と advanced panel に分ける
- 詳細は `Artifact/docs/MILESTONE_AI_CLOUD_UI_2026-04-09.md`

### M-AI-3 AI Assisted Keyframe Generation ⭐ **新規追加**
- 軌跡解析と自動キーフレーム生成でアニメーション作成を支援
- `AIKeyframeGenerator` で動きのパターンを学習し、スムーズなキーフレーム提案を返す
- `EasingLabWidget` とタイムライン keyframe 表示を使って比較・適用できるようにする
- **機能:** 軌跡データからのキーフレーム提案、タイムライン統合、既存 undo path での適用
- **見積:** 45-60h
- **詳細:** `docs/planned/MILESTONE_AI_KEYFRAME_SUGGESTION_2026-04-21.md`

### M-AI-4 AI Color Grading Suggestion ⭐ **新規追加**
- シーン分析と自動カラーグレーディング提案で色調整を支援
- `AIColorAnalyzer` / `ColorGradingSuggester` で画像を解析し、LUTやパラメータの候補を提案
- `ArtifactColorSciencePanel` と `ArtifactColorGradingEngine` を提案経路に載せる
- **機能:** 画像分析からの色調整提案、LUT/preset 統合、既存 grading 経路での適用
- **見積:** 60-75h
- **詳細:** `docs/planned/MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-21.md`

### M-AI-5 AI Basic Assistant ⭐ **新規追加**
- 基本的なAIアシスタント機能で質問応答とプロジェクト情報提供
- `AIBasicAssistant` でクエリに応答し、MCP経由で外部AIと連携
- **機能:** 質問応答、ドキュメント/コード検索、UI統合
- **見積:** 35-50h
- **詳細:** `docs/planned/MILESTONE_AI_BASIC_ASSISTANT_2026-04-11.md`

## Feature Expansion Support

### Priority Execution Trio
- `M-FE-9 Motion Tracking Workflow`
- `M-AS-4b Vector / SVG Layer Import`
- `M-RD-5 Animated Image Export`
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_FOCUS_TRIO_2026-03-28.md`

### M-FE-1 Onboarding / Empty States ✅ (2026-06-23)
- ✅ WelcomeWidget: empty project の案内画面（最近開いたプロジェクト一覧、New / Import / Open ボタン）
- ✅ Playhead Phase 1: `currentFrame_` を単一権威に統一、全 UI への fan-out を `setCurrentFrameForAll()` に集約
- 未着手: empty selection / empty asset / empty timeline の案内
- マイルストーン文書は `docs/done/` へ移動済み

### M-FE-2 Export / Review / Share ✅ (2026-06-23)
- Phase 1 Result Surface 完了: Copy Path追加、Reveal/Open/Historyは既存
- マイルストーン文書は `docs/done/` へ移動済み

### M-FE-3 Automation Helpers
- command palette / batch / preset / macro entry を増やす
- ✅ レイヤーエディタ用コマンドパレット (Ctrl+F)
- 詳細は `docs/planned/MILESTONE_AUTOMATION_HELPERS_2026-03-27.md`

### M-FE-4 Workspace / Layout / Session
- workspace 保存 / 読み込み、dock layout preset、window state 復元
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`

### M-FE-5 Templates / Presets / Starter Kits
- project / composition / layer / effect の preset と starter project
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`

### M-FE-6 Batch / Macro / Script Entry
- batch rename / relink / export、macro、script hook
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- **AE差別化:** インクリメンタルサーチ、メタデータ（解像度/fps/デュレーション）でフィルタ可能
- 詳細は `docs/planned/MILESTONE_SEARCH_COLLECTIONS_SMART_ORGANIZATION_2026-03-28.md`

### M-UI-21 Asset Browser Navigator / Search / Presentation Surface
- Asset Browser を Unity 風のナビゲータとして整理し、search / breadcrumb / favorites / grid-list / thumbnail slider / workflow bridge を段階導入する
- 既存の search / thumbnail / unused / DnD を土台にして、探索と presentation を揃える
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_SEARCH_PRESENTATION_2026-04-03.md`
- 実行メモは親文書へ統合済み

### M-TL-10 Timeline Feature Implementation / Interaction Surface
- Timeline の layer / clip / keyframe / search / visual language / owner-draw を一つの実行計画として束ねる
- 既存の timeline milestone を置き換えず、順序と責務をまとめる
- 詳細は `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md`
- 実行メモは親文書へ統合済み

### M-TL-11 Timeline Right Pane Full Owner-Draw
- `ArtifactTimelineWidget` の右ペインを `ArtifactTimelineTrackPainterView` 正規経路へ固定し、`TimelineTrackView / TimelineScene / ClipItem` を退役させる
- clip / keyframe / playhead / selection / input の責務を painter 側へ寄せ、右ペインを完全 owner-draw surface にする
- 詳細は `docs/planned/MILESTONE_TIMELINE_RIGHT_PANE_FULL_OWNER_DRAW_2026-04-08.md`

### M-TL-12 DAW-Style Input Surface
- timeline / inspector を DAW 風に、real-time input と step input の 2 系統で扱えるようにする
- playback 中の live capture と停止中の 1-frame step entry を同じ property / keyframe model に書き込む
- 詳細は `docs/planned/MILESTONE_DAW_STYLE_INPUT_SURFACE_2026-04-08.md`
- 進捗: Core 側の `InputSurfaceManager` と `InputSurfaceStateChangedEvent` を実装済み

### M-TL-13 Timeline Scrub Bar Frame Cache Overlay
- `ArtifactTimelineScrubBar` 上に AE 風の cache range 可視化を追加し、frame cache / RAM preview の有効範囲を緑の帯で見せる
- 現在フレームの赤い進捗表示と共存させ、playback / scrub / seek の状態を読み取りやすくする
- 詳細は `docs/planned/MILESTONE_TIMELINE_SCRUBBAR_FRAME_CACHE_OVERLAY_2026-04-10.md`
- timeline index では補助線扱い。単独で追うより `docs/planned/MILESTONE_TIMELINE_INDEX_2026-04-22.md` を先に読む。

### M-FE-9 Motion Tracking Workflow
- tracker editor / overlay / stabilize / bake を制作導線としてまとめる
- 詳細は `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- Phase 1 実装順は `docs/planned/MILESTONE_MOTION_TRACKING_PHASE1_EXECUTION_2026-03-28.md`

### M-FE-10 Animation Dynamics Core
- Physics2D とは別に、animation 用の spring / damping / follow-through を Core に置く
- 詳細は `docs/planned/MILESTONE_ANIMATION_DYNAMICS_CORE_2026-03-28.md`

### M-FE-11 Virtual Pointer Core
- モーショングラフィック向けの仮想マウス演出を Core の再生可能データとして定義する
- 詳細は `docs/planned/MILESTONE_VIRTUAL_POINTER_CORE_2026-03-28.md`

## UI / UX

### M-UI-1 Timeline Finish
- playhead、不感帯、余白、行揃え、ホイール、ドラッグ挙動の最終整理

### M-UI-2 Dock / Tab Polish
- アクティブタブ装飾
- スプリッター幅
- 空パネルや初期レイアウトの見直し

### M-UI-11 UI Theme System / Studio Skin
- `QSS` に責務を寄せすぎず、背景 / surface / accent / selection を意味ベースで統一する
- `Maya / Blender / Modo / DaVinci` 系の制作 UI を参考にしつつ、Artifact 独自の studio skin を作る
- 詳細は `docs/planned/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`

### M-UI-14 QSS Reduction / Style Ownership
- `QSS` を主責務から外し、theme tokens / palette / 共通 widget / owner-draw へ移す
- 詳細は `docs/planned/MILESTONE_QSS_REDUCTION_2026-03-31.md`

### M-UI-15 Inline Interaction Surfaces
- property / viewport / timeline / layer panel の inline 展開を共通化する
- color picker / gradient editor / scrub input / expression / waveform / blend mode をその場で扱えるようにする
- 詳細は `docs/planned/MILESTONE_INLINE_INTERACTION_SURFACES_2026-03-31.md`

### M-UI-16 UI EventBus Adoption
- UI 層の広域更新を `ArtifactCore::EventBus` に寄せ、Project / Timeline / Inspector / Render Queue / Asset Browser の fan-out を抑える
- Qt signal は高頻度入力と局所 UI に限定し、state change だけ bus 化する
- 詳細は `docs/planned/MILESTONE_UI_EVENT_BUS_ADOPTION_2026-04-01.md`

### M-UI-17 Console Widget Enhancement
- `ArtifactDebugConsoleWidget` をログ診断のハブとして強化する
- search / filter / export / stats / event log integration / theme ownership をまとめる
- 詳細は `docs/planned/MILESTONE_CONSOLE_WIDGET_ENHANCEMENT_2026-03-31.md`

### M-RQ-1 Render Queue GPU Backend Selection / Fallback
- Render Queue から GPU backend を選べるようにし、CPU backend と fallback を並行運用できる状態にする
- backend contract / GPU encode path / UI diagnostics を段階導入する
- 詳細は `docs/planned/MILESTONE_RENDER_QUEUE_GPU_BACKEND_2026-04-03.md`

### M-RD-13 Multi-Frame Rendering (MFR) for Render Queue
- Render Queue の export を複数フレーム並列で進められるようにし、直列 render の待ち時間を埋める
- まずは export-only で導入し、live preview は対象外にする
- 詳細は `docs/planned/MILESTONE_MULTI_FRAME_RENDERING_2026-04-09.md`

### M-APP-4 Session Ledger / Recovery Workspace
- project / render job / failed task / recovery point を一つの作業台帳にまとめる
- crash 後復帰、長時間 render、未保存作業の回収導線を統合する
- 詳細は `docs/planned/MILESTONE_SESSION_LEDGER_RECOVERY_WORKSPACE_2026-04-09.md`

### M-UI-18 Property Widget Update / Cleanup / Theme Ownership
- `ArtifactPropertyWidget` / `PropertyEditor` / `Inspector` の責務を整理し、property UI の見た目と構造を揃える
- `QSS` 依存を減らし、theme / palette / widget ownership を property pane に反映する
- 進捗: section / search / row chrome を palette ベースへ移行中
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_PROPERTY_WIDGET_UPDATE_CLEANUP_THEME_2026-04-02.md`

### M-UI-19 QSS Exorcism / Property Theme Ownership
- property / inspector / dock / queue 周辺の `QSS` を段階的に追放し、theme token と owner-draw に寄せる
- `M-UI-14` と `M-UI-18` をつなぐ実行 milestone
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_QSS_EXORCISM_PROPERTY_THEME_2026-04-02.md`

### M-UI-23 Property Widget Row Alignment / Inspector Layout
- `ArtifactPropertyWidget` の行揃え、keyframe / reset / badge / value column の位置を揃え、インスペクタらしい整列レイアウトへ段階移行する
- `PropertyEditor` row widget に layout 責務を寄せ、見た目の整いを構造の統一へつなげる
- 進捗: row bg / hover / keyframe chrome を owner-draw 化した
- 詳細は `docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_INSPECTOR_LAYOUT_2026-04-03.md`
- 実行メモは親文書へ統合済み

### M-UI-24 Visual Density Monitor
- 画面の詰まり具合を density / heatmap / warning で読む診断 surface
- 完了記録は `docs/done/MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md`
- 実行メモは `docs/done/MILESTONE_VISUAL_DENSITY_MONITOR_PHASE1_EXECUTION_2026-06-03.md` で、canonical completion は done 側

### M-APP-5 Render Preflight / Output Safety Check
- render queue / export dialog / debugger / problem view に出力前検査を共通文法で流す
- 完了記録は `docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`

### M-CLIP-1 Keyframe Copy & Paste
- timeline から keyframe を copy / paste できるようにする
- 完了記録は `docs/done/MILESTONE_KEYFRAME_COPY_PASTE_2026-06-16.md`

### M-UI-22 QSS Decommission / CommonStyle Path ✅ 完了
- 完了: `docs/done/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`

### M-SC-2 Shortcut Context Map / Blender-Like Keymap Routing
- `InputOperator` の context 解決順と widget / region 単位の分割を固定し、Blender 風の「場所とモードで意味が変わる」ショートカット routing を明文化する
- `ArtifactCompositionRenderWidget` / `ArtifactTimelineWidget` / `ArtifactLayerPanelWidget` / `ArtifactAssetBrowser` / `ArtifactInspectorWidget` を先行対象にする
- 詳細は `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- 実行メモは親文書へ統合済み

### Composition Editor Suggested Order
- `M-UI-7 Composition Editor Mask / Roto Editing`
- `M-UI-15 Inline Interaction Surfaces`
- `M-UI-6 Composition Motion Path Overlay`
- `M-FE-7 Review / Compare / Annotation`
- `M-TL-4 Timeline TrackView Owner-Draw Migration`
- `M-TL-8 Timeline QGraphicsScene Elimination`
- CompositionEditor の内部同期は signal 直結ではなく deferred event を正規経路にする
- 順序の目安:
  1. `M-UI-15 Inline Interaction Surfaces`
  2. `M-UI-6 Composition Motion Path Overlay`
  3. `M-UI-7 Composition Editor Mask / Roto Editing`
  4. `M-UI-5 Contents Viewer Expansion` の inline edit 連携
  5. `M-TL-4` / `M-TL-8` の painter 化が終わったら viewport/overlay 連携を深める
  6. `M-FE-7` で review / annotation の脇道を足す
- `M-UI-7` の内部では、geometry editing と mask parameter の time-addressable 化を分ける
- time-addressable 化の first slice は `docs/planned/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` に寄せる
- Phase 1 実行メモは親文書へ統合済み

### M-UI-12 Composition Notes / Scratchpad
- コンポジション / レイヤー / フレームに紐づく軽量メモを残せるようにする
- review / annotation より前段の、制作中の書きなぐりメモを扱う
- 詳細は `docs/done/MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md`

### M-UI-3 Inspector Usability
- effect / property の見つけやすさ
- 空状態の整理
- 選択同期とラベル整理
- **AE差別化:** 全プロパティ一覧表示（P/S/R/T/Aショートカット不要）、ネストグループのフラット化、複数レイヤー一括編集、Blender風数値入力（スクロール変更）、数値スクラブ（ドラッグ変更＋Ctrl/Shift精度調整）、インラインキーフレーム操作（プロパティ横ミニタイムライン）、Expressionエディタ強化（シンタックスハイライト/補完/視覚的エラー表示）
- ✅ キーボードショートカット追加 (Home/End/Ctrl+A/Ctrl+D)
- ✅ ステータスバーコンポジション情報表示
- ✅ レイヤーラベルカラー機能
- ✅ レイヤー整列・分布機能

### M-UI-8 Animation Dynamics UI Surface
- Physics2D とは別に、animation 用の spring / damping / follow-through を Inspector / Layer Panel から触れるようにする
- 詳細は `docs/planned/MILESTONE_ANIMATION_DYNAMICS_UI_2026-03-28.md`

### M-UI-6 Composition Motion Path Overlay ✅ (verified 2026-06-26)
- 全 Phase 実装済み: path+dot+frame rect overlay, click-to-seek, hover, cache, toggle (Ctrl+Alt+M), interpolation色分け, Shift+click追加/Alt+click削除+Undo
- 主要コード: `ArtifactCompositionRenderController.cppm:9409-9581`、`buildMotionPathSamples()`、`hitTestMotionPathSample()`
- 詳細は `docs/planned/MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md`

### M-UI-7 Composition Editor Mask / Roto Editing ✅ (verified 2026-06-26)
- マイルストーン文書 `docs/done/` に移動済み、各サブ milestone 完了
- 実装内容: `EditMode::Mask` entry → path creation → vertex move/delete → bezier handle edit (Ctrl+drag) → undo/redo (`MaskEditCommand`)
- 主要ファイル: `ArtifactRenderLayerWidgetv2.cppm` (`drawMaskOverlay()`, mouse handlers), `ArtifactCompositionRenderController.cppm` (pending mask, segment insert, handle set)
- 残ポリッシュ項目: segment insert UX未配線、inspector detail △、context menu未対応、`RotoMaskEditor` (Core側 standalone) は未接続

### M-UI-7a Mask Keyframe Foundation ✅ (verified 2026-06-26)
- `mask.opacity`, `mask.feather` など scalar parameters の property exposure 完了
- マイルストーン文書 `docs/done/` へ移動済み
- 詳細: `docs/done/MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md`
- Phase 1 実行メモは親文書へ統合済み

### M-UI-4 Menu-to-App Command Routing
- File / Composition / Edit / View / Layer / Render / Help の menu を app service / command に正しく接続する
- 詳細は `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`

### M-UI-4b Toolbar / App Integration
- `ArtifactToolBar` を app command surface として整理し、menu / shortcut / workspace state と揃える
- Qt の新規 signal / slot は増やさず、既存 service / event / 明示 refresh で同期する
- 詳細は `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`

### M-UI-4b Toolbar / App Integration ✅ (verified 2026-06-26)
- tool selection group (Select/Hand/Zoom/Move/Rotate等), Zoom In/Out/100%/Fit, Guide toggle, More overflow button
- `ArtifactToolBar.cppm` 全アクション app command surface に配線済み

### M-UI-5 Contents Viewer Expansion
- image / video / audio / 3D model / source / final / compare を横断する viewer の拡充
- audio playback と live waveform preview を同一 surface で確認できるようにする
- ✅ テキストレイヤーインライン編集 (実装済み)
- 詳細は `docs/done/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- 追加の review / compare / annotation 方向は `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`

### M-UI-20 Contents Viewer DCC Surface Layout / A-B / Wipe
- viewer を 4 段構成の DCC surface として整理し、title / viewer badge / transport / channel-meta を統一する
- recent source dropdown / multi-viewer assignment / wipe compare を 1 つの導線として扱う
- 詳細は `docs/planned/MILESTONE_CONTENTS_VIEWER_DCC_SURFACE_LAYOUT_2026-04-03.md`

### M-UI-9 3D Model Review in Contents Viewer
- OBJ / FBX を Contents Viewer で確認し、model inspection の導線を固める
- 詳細は `docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md`

### M-UI-10 3D Model Import and Contents Viewer Integration
- `ufbx` / `tinyobjloader` を使った 3D model 読み込み経路を整え、Contents Viewer へ正式に接続する
- 詳細は `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`

### M-CP-1 Camera Projection Integration ⭐ **新規追加**
- 3D rendering のために camera の projection を適切に扱う
- **機能:** Perspective/Orthographic projection, viewport sync, matrix calculation
- **見積:** 20-30h
- **詳細:** `docs/planned/MILESTONE_CAMERA_PROJECTION_2026-03-31.md`

### M-CP-2 3D Viewport Stabilization / Solid / Overlay
- 3D 表示を「読める」状態へ寄せ、solid shading / camera / overlay の責務を分けて安定化する
- gizmo / bounds / HUD の重なり順を固定し、wireframe と solid の両方で破綻しにくくする
- 詳細は `docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md`

### M-LL-1 Light Linking System ⭐ **新規追加**
- 3D scene での light の影響を layer ごとに制御する
- **機能:** Light-to-Object linking, include/exclude lists, per-layer light influence
- **見積:** 25-35h
- **詳細:** `docs/planned/MILESTONE_LIGHT_LINKING_2026-03-31.md`

### M-MAT-1 3D Material System ⭐ **新規追加**
- 3D objects の material を定義し、適切な shading を実現する
- **機能:** Basic materials (diffuse/specular), texture mapping, material assignment
- **見積:** 30-40h
- **詳細:** `docs/planned/MILESTONE_3D_MATERIAL_SYSTEM_2026-03-31.md`

### M-MAT-2 MaterialX Document / Exchange Bridge
- MaterialX XML を Material asset / inspector / export の橋渡しにする
- **機能:** document presence, canonical storage, import/export, preview summary
- **見積:** 18-28h
- **詳細:** `docs/planned/MILESTONE_MATERIALX_DOCUMENT_EXCHANGE_2026-04-10.md`

### M-TY-1 Advanced Typography Engine ⭐
- **詳細:** `docs/planned/MILESTONE_ADVANCED_TYPOGRAPHY_ENGINE_2026-03-29.md` (Core 実装)

### M-TY-2 Typography Preset & Motion Style UI ⭐ **新規提案**
- 高度なタイポグラフィ制御とアニメーションシステムを UI/プリセット化
- **機能:** プリセットライブラリ・文字単位インスペクタ・パス追従 UI
- **見積:** 30-40h
- **詳細:** `docs/planned/MILESTONE_TYPOGRAPHY_PRESET_UI_2026-03-30.md`

### M-CS-1 Advanced Color Science Pipeline
- **詳細:** `docs/done/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`

### M-SC-3 Color Grading Workspace ⭐ **新規提案**
- プロフェッショナルなグレーディング環境の構築
- **機能:** リアルタイムスコープ (Waveform/RGB Parade/Vectorscope)・比較表示・専用パネル
- **見積:** 32h
- **詳細:** `docs/planned/MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md`

## Timeline / Layer

タイムライン系の整理用入口は [MILESTONE_TIMELINE_INDEX_2026-04-22.md](MILESTONE_TIMELINE_INDEX_2026-04-22.md) を先に見る。
古い文書は残しつつ、`Completed / Foundation` と `Active / Current` を分けて読む前提にする。
個別の `M-TL` 番号は legacy と current でぶつかることがあるので、本文のリンク先ファイル名を優先する。

### M-TL-4 Timeline TrackView Owner-Draw Migration
- 右ペインを `QGraphicsView` から owner-draw へ段階移行する
- 詳細は `docs/planned/MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md`

### M-TL-8 Timeline QGraphicsScene Elimination
- 右タイムラインの `QGraphicsScene` 依存を painter 側へ外し切る
- 詳細は `docs/planned/MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md`

### M-TL-9 Timeline Visual Language
- レイヤーバー、キーフレーム、再生ヘッド、選択ハイライトの色と形を意味ベースで統一する
- 詳細は `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`

### M-TL-14 Timeline Layer Specialization Execution
- `Audio / Video / Text / Shape / Image / Particle` の最小専用化を、共通編集を壊さずに段階導入する
- 詳細は `docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md`
- timeline index では補助線扱い。view / input / lane の本筋に吸収される。

### M-TL-15 Timeline Ripple Edit / Downstream Shift ✅ Phase 1 (verified 2026-06-26)
- `RippleTrimOutCommand`, `RippleDeleteCommand`, `SlideClipCommand` + undo/redo 実装済み
- マイルストーン文書: `docs/done/MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md`
- Trim / Delete ripple は完了。全レイヤー一括伸縮など Phase 2+ は未着手

### M-TL-5 Timeline Keyframe Editing ✅ (verified 2026-06-26)
- Add/remove/toggle keyframes, drag-move, multi-selection, copy/paste/cut
- Full interpolation control (Linear/Ease/Bezier/Hold) via context menu + Easing Lab
- Key pattern generation (12 presets), reverse, distribute, duplicate, color labels
- Jump navigation (Ctrl+PgUp/PgDn), status summary, undo/redo for all operations
- マイルストーン文書 `docs/done/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md` + `docs/done/MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md`
- **未完了 (stretch goal):** キーフレームの時間軸スケーリング（全レイヤー一括伸縮）

### M-TL-17 Timeline Proportional Keyframe Editing 🚧
- 右ペイン `ArtifactTimelineTrackPainterView` に Blender 風の proportional editing を導入する
- Phase 1 は selected keyframes の time move のみに絞る
- `O` で on/off、`[` `]` で半径変更
- 詳細は `docs/planned/MILESTONE_TIMELINE_PROPORTIONAL_KEYFRAME_EDITING_2026-07-06.md`

### M-UI-6b Composition Motion Path Display Improvement
- モーションパス overlay のサンプリングと描画を分離し、適応サンプリング・速度可視化・spatial bezier 表示へ進める
- まずは既存挙動を壊さない Phase 1 から入り、表示基盤を整えてから編集導線を重ねる
- 詳細: `docs/planned/MILESTONE_MOTION_PATH_DISPLAY_IMPROVEMENT_2026-07-10.md`

### M-TL-6 Timeline Layer Search
- タイムライン上部の検索バーで layer / effect / tag / state をインクリメンタルに絞り込む
- 詳細は `docs/planned/MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md`

### M-TL-7 Timeline Search / Keyframe Integration
- search 結果から keyframe へ素早く飛べるようにし、header / status / highlight を統合する
- 詳細は `docs/done/MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md`

### M-LG-1 Layer Group System
- レイヤーグループの保存 / 表示 / 親子 / 可視性 / 操作単位を整理する
- 詳細は `docs/planned/MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md`
- Phase 1 実行メモは親文書へ統合済み

### M-LG-2 Layer Components: Physics / Behavior
- layer 側に軽量 component system を追加し、追従・減衰・トリガーの受け皿を作る
- 詳細は `docs/planned/MILESTONE_LAYER_COMPONENT_SYSTEM_UNITY_LIKE_2026-04-08.md`
- 固定評価順と Cloner / Layout / Crowd / Physics / Fracture / Particle の統合契約:
  `docs/planned/MILESTONE_LAYER_COMPONENT_EVALUATION_PIPELINE_2026-06-28.md`

### M-PH Playhead 整備 ✅ Phase 1 (2026-06-23), 🚧 Phase 3-4 partial
- ✅ Phase 1: 状態統一 — `currentFrame_` を単一権威、fan-out → `setCurrentFrameForAll()`、9 箇所の手動書替を統合
- 🚧 Phase 2: 不感帯/スナップ/再生中シーク/スクロール追従 → **未実装**
- 🚧 Phase 3: 表示品質 → ✅ HH:MM:SS:FF (TimeCodeWidget), HiDPI, コンポジションビュー連携 済み / ❌ F<n> 形式
- 🚧 Phase 4: 操作拡充 → ✅ JKL シャトル, ホイールシーク, ドラッグシーク 済み / ❌ タイムコード入力未実装
- マイルストーン文書は `docs/done/` へ移動済み

## Render

### M-IR-1 ArtifactIRender API Cleanup
- viewport / canvas / pan / zoom の整理
- primitive API の責務固定

### M-IR-2 ArtifactIRender Software Backend
- Qt painter fallback の強化
- overlay / gizmo 用 2D 描画

### M-IR-3 ArtifactIRender Backend Parity
- software と Diligent の primitive 差分を縮める

### M-RD-1 Software Render Pipeline
- コンポ作成
- Solid 追加
- preview
- effect
- 静止画シーケンス

### M-RD-9 Render Path Decomposition / Buffer Migration
- `QImage` を render path の内部から段階的に追放し、typed buffer ベースへ寄せる
- `RawImage` は I/O 境界、内部は `ImageF32x4_RGBA` 系に分離する
- 詳細は `docs/planned/MILESTONE_RENDER_PATH_DECOMPOSITION_2026-03-31.md`

### M-RD-10 Deep Compositing Support
- OpenEXR ベースの deep sample / deep merge / deep read-write の基盤を作る
- flat RGBA compositing と分離し、deep 用 buffer と IO を別系統で持つ
- 詳細は `docs/planned/MILESTONE_DEEP_COMPOSITING_2026-03-31.md`

### M-RD-11 GPU Mask Cutout Compute Pipeline 🚧 Phase 1-2 ✅, Phase 4 🆕
- Phase 1 (Mask Texture Contract) ✅ — `MaskCutoutPipeline` (+ `MaskCutout.ixx`) 完成済み
- Phase 2 (Compute Mask Apply) ✅ — 既存 `MaskCutoutPipeline::apply()` で compute shader cutout 可能
- Phase 4 (GPU Path Rasterizer) 🆕 — `MaskPathRasterizerPipeline` (+ `MaskPathRasterizer.ixx`) で MaskPath 頂点から直接 GPU マスク生成可能
- **未接続**: Phase 3 (Composition Integration) — `ArtifactCompositionRenderController` への配線は未着手
- CPU fallback (`LayerMask::applyToImage()`) は維持
- 主要ファイル:
  - `ArtifactCore/include/Graphics/Shader/Compute/HLSL/MaskPathRasterizer.ixx`
  - `ArtifactCore/include/Graphics/Shader/Compute/MaskPathRasterizerPipeline.ixx`
  - `ArtifactCore/src/Graphics/Shader/Compute/MaskPathRasterizerPipeline.cppm`
  - `ArtifactCore/include/Graphics/Shader/Compute/HLSL/MaskCutout.ixx`
  - `ArtifactCore/include/Graphics/Shader/Compute/MaskCutoutPipeline.ixx`
  - `ArtifactCore/src/Graphics/Shader/Compute/MaskCutoutPipeline.cppm`
- 詳細は `docs/planned/MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md`

### M-RD-6 FFmpeg GPU Decode Backend
- CPU decode とは別に FFmpeg hwaccel backend を持ち、video layer / playback / preview から選べるようにする
- 詳細は `docs/planned/MILESTONE_FFMPEG_GPU_DECODE_BACKEND_2026-03-28.md`
- 低レベルAI実装メモ: `docs/planned/MILESTONE_FFMPEG_81_PRORES_GPU_DECODE_LOW_LEVEL_AI_2026-05-23.md`

### M-RD-7 Unified Audio / Video Render Output
- video render の後段で audio を mux し、音声付き出力を render queue から扱えるようにする
- 詳細は `docs/planned/MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md`

### M-RD-12 FFmpeg GPU Encode Backend ⭐ **新規追加**
- FFmpeg の hardware-accelerated encode backend を追加し、Render Queue から backend 選択できるようにする
- **機能:** NVENC/QSV/AMF/VAAPI 対応、自動検出、手動選択、品質/性能プリセット
- **見積:** 30-40h
- **詳細:** `docs/planned/MILESTONE_FFMPEG_GPU_ENCODE_BACKEND_2026-04-03.md`

### M-RD-8 Integrated Rendering Engine
- video / audio を同一 job として扱う render 本体の統合骨格を作る
- 詳細は `docs/planned/MILESTONE_INTEGRATED_RENDERING_ENGINE_2026-03-28.md`

### M-RD-2 Render Queue Hardening
→ 詳細: [MILESTONE_RENDER_QUEUE_2026-03-22.md](MILESTONE_RENDER_QUEUE_2026-03-22.md)
- job 編集
- 範囲指定
- 失敗理由表示
- 履歴と再実行
- in/out と work area の反映
- バックグラウンドレンダーの安定化
- 分散レンダリングの土台
- レンダー完了後の自動アクション
- checkpoint / resume

### M-RD-3 Dual Backend Parity
- software と Diligent の見た目差分を減らす

### M-RD-5 Animated Image Export
- GIF / APNG / Animated WebP などの web 向け animated image 出力
- 詳細は `docs/planned/MILESTONE_ANIMATED_IMAGE_EXPORT_2026-03-27.md`

### M-LV-1 Layer Solo View (Diligent)
- 詳細は `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md`
- 現行の更新案は `docs/planned/MILESTONE_LAYER_VIEW_ENHANCEMENT_2026-07-08.md`
- 実装順は LW-0 〜 LW-4（LayerPreviewPipeline 整理 → 共有 ViewportState / Overlay / DisplayFilter → レイヤー固有比較 → ドック統合）
- current composition / current layer の追従
- solo 表示の安定化
- mask / roto 入口の整理
- software test widget との見え方差分縮小
- inspect / impact / compare の導線整備
- inspect HUD / compare / effect stack summary の追加
- effect の部分適用 (Rect / Mask) の可視化

### M-CE-1 Composition Editor Cache System
- `Composition Viewer` の surface cache / render key / GPU blend fast path を整理
- ✅ ROI (Region of Interest) システム実装済み
- 詳細は `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`

### M-CE-2 Static Layer GPU Cache
- 静止レイヤーの GPU texture を長く使い回す cache 層
- ✅ ギズモ描画最適化 (Phase 2) 完了
- 詳細は `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`

### M-CE-3 Composition Editor Figma-like Overlay / Snap / HUD
- smart guides / selection overlay / useful HUD を足して、Figma っぽい操作補助を入れる
- snap と選択オーバーレイを先に本体描画へ寄せ、その後 context HUD / probe を足す
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_2026-04-21.md`
- 実行メモは親文書へ統合済み

### M-CE-4 Composition Editor Selection / Comparison Upgrade
- 矩形選択、追加 / 除外選択、ラッソ選択をビューポート側にまとめる
- A/B 切替、参照フレーム固定、差分オーバーレイを同じ比較導線として扱う
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_SELECTION_COMPARISON_2026-06-27.md`

## Shared Notes

- `docs/shared/ai-tech-memos/README.md`
- AI 同士で実装メモや調査要点を共有するための軽量な置き場

## Effects

### M-FX-1 Inspector Effect Stack Bridge
- Inspector から effect 追加、削除、順序変更

### M-FX-2 Solid Color Effects ✅ 完了
- 完了: `docs/done/MILESTONE_SOLID_COLOR_EFFECTS_2026-06-27.md`

### M-FX-3 Creative Effects Bridge
- Halftone
- Posterize
- Pixelate
- Mirror などを接続

### M-FX-4 Creative Workflow & Inspector Refinement
- Creative Effect Pack (Halftone, etc.) 縺ｮ謗･邯・
- Inspector (Effect Stack) 縺ｨ Property Editor 縺ｮ驕｣蜍輔・蜷梧悄
- 隧ｳ邏ｰ縺ｯ `Artifact/docs/MILESTONE_CREATIVE_WORKFLOW_REFINEMENT_2026-03-13.md`

### M-FX-5 GPU Effect Parity
- CPU effect は reference として残しつつ、GPU equivalent effect を順に実装する
- `supportsGPU()` の宣言や CPU 呼び出しだけの GPU wrapper は完了扱いにしない。HLSL/compute dispatch を実装する
- CPU reference はテスト／比較／fallback 用として維持し、空間 effect は行・tile 単位で安全に MT 化する
- temporal / history effect は history の read/write 契約を固定してから MT 化する
- GPU 失敗時は結果契約を変えず CPU reference へ fallback する
- 実装・runtime 検証状況は `docs/analysis/EFFECT_MAP_2026-07-16.md` を正本にする
- 詳細は `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`

### M-FX-6 Color Correction / Grading
- CPU reference を残しつつ、GPU 側の color correction / grading を実装する
- 詳細は `docs/planned/MILESTONE_COLOR_CORRECTION_2026-03-27.md`

### M-FX-7 Partial Application
- `Rect` / `Mask` などの部分適用をエフェクトに導入する
- 全体適用と局所適用の両方を同じ stack で扱えるようにする
- mask を切らずに effect 単体で範囲指定できる導線を含める
- 詳細は `docs/planned/MILESTONE_EFFECT_SYSTEM_IMPROVEMENT_2026-03-28.md`

### M-FX-8 Composition Final Effect
- composition 全体の最後に掛かる final effect / end-stage effect を検討する
- layer / effect stack の後段で、出力直前に 1 回だけ効く処理を想定する
- before / after の比較や render output 調整と合わせて扱う

### M-FX-10 Visual Effect Bus
- composition final effect を起点に、group/shared render target を使う visual bus を検討する
- send / return を映像向けの中間レンダーターゲット共有として扱う
- 詳細は `docs/planned/MILESTONE_VISUAL_EFFECT_BUS_2026-06-02.md`

### M-FX-11 Effect UI Standardization
- すべてのエフェクトに共通の `Preview / Preset / Appearance` 契約を持たせる
- 標準エフェクトと OFX サードパーティエフェクトで UI の枠を揃える
- 実装順: `descriptor / section classification -> Inspector bridge -> preset browser / starter flow bridge -> appearance catalog -> OFX fallback -> Property alignment`
- 先行対象: `Gaussian Blur`, `Sharpen`, `Curves`, `Levels`, `Glow`, `OFX Plugin`
- 詳細は `docs/planned/MILESTONE_EFFECT_UI_STANDARDIZATION_2026-06-07.md`

### M-FX-9 Face Detection & Auto-Mosaic
- OpenCV による顔認識 → 自動モザイク/ぼかしエフェクト
- Haar Cascade / DNN による検出、追従トラッキング
- 詳細は `docs/planned/MILESTONE_FACE_DETECTION_MOSAIC_2026-04-01.md`

### M-UI-14 Multi-Display Support
- デュアル/マルチディスプレイ環境での制作ワークフロー強化
- セカンドモニタープレビュー、フルスクリーンプレビュー、モニター検出
- 詳細は `docs/planned/MILESTONE_MULTI_DISPLAY_SUPPORT_2026-04-01.md`

### M-AS-4 Asset Browser Improvement
- ファイルシステム監視、TBB 並列サムネイル、ブレッドクラム、お気に入り
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md`

### M-AS-4 Asset Browser Improvement
- ファイルシステム監視、TBB 並列サムネイル、ブレッドクラム、お気に入り
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md`

### M-VFX-1 Real-time Particle & Fluid Simulation ⭐ **新規追加**
- Compute Shader ベースの高性能視覚効果
- **機能:** GPU パーティクル・2D 流体ソルバー (Smoke/Fire)・インタラクティブ・シミュレーション
- **見積:** 40-60h
- **詳細:** `docs/planned/MILESTONE_VFX_PARTICLE_FLUID_2026-03-30.md`
- **補足:** `fluid` は layer component、`pyro` は独立 volume domain として分離する。`docs/planned/MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md`

### M-VFX-2 AE-Style Simple Rain Effect
- 既存 particle / effect 基盤で、AE っぽい簡易雨を最短構成で実現する
- streak / density / direction / splash / depth feel を preset 中心でまとめる
- **見積:** 8-14h
- **詳細:** `docs/planned/MILESTONE_AE_STYLE_SIMPLE_RAIN_EFFECT_2026-05-31.md`

## Audio

### M-AU-1 Composition Audio Mixer
- mute / solo / volume / layer 同期

### M-AU-2 Playback Sync
- 再生位置と音の同期

### M-AU-3 Audio Visualization
- waveform / meter
- 詳細は `docs/planned/MILESTONE_AUDIO_WAVEFORM_2026-03-29.md`

### M-AU-8 Audio Widget Enhancement / Mixer Surface
- `ArtifactCompositionAudioMixerWidget` を中心に、mute / solo / volume / pan / waveform / meter / state badge をまとめる
- 詳細は `docs/planned/MILESTONE_AUDIO_WIDGET_ENHANCEMENT_2026-04-09.md`

### M-AU-7 Audio Waveform Thumbnail Preview
- audio file の thumbnail として waveform を表示する
- Asset Browser / inspector / detail panel で見た目の判別力を上げる
- **AE差別化:** ホバーで波形アニメーション（プロっぽさ向上）
- 詳細は `docs/planned/MILESTONE_AUDIO_WAVEFORM_THUMBNAIL_PREVIEW_2026-03-31.md`

### M-AU-6 Audio Reactor System ⭐ **新規提案**
- オーディオ解析による自動アニメーションシステム
- **機能:** リアルタイム FFT 解析・オーディオ駆動プロパティリンク・スムージング制御
- **見積:** 36h
- **詳細:** `docs/planned/MILESTONE_AUDIO_REACTOR_SYSTEM_2026-03-30.md`

### M-AU-4 Audio Layer Integration & UX
- Audio Layer の source / mute / volume / loaded state を inspector と timeline に自然に接続する
- 詳細は `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`
- `MILESTONE_AUDIO_ENGINE_2026-03.md` の再生基盤とは分けて、layer 側の見え方と導線を詰める

### M-AU-5 Audio Playback Stabilization
- start-up pre-roll, stop/seek hygiene, buffer diagnostics, format normalization
- 詳細は `docs/planned/MILESTONE_AUDIO_PLAYBACK_STABILIZATION_2026-03-28.md`

## Project / Asset

### M-PV-1 Project View Basic Operations
- Project View selection と current composition の同期
- rename / delete / double-click
- 基本検索と filter
- footage selection を Asset Browser に返す往復同期を追加し、Project View 起点の探索を短くした
- selection chrome に Asset Browser linked の sync chip を出して、同期状態を読めるようにした
- **AE差別化:** コンポジションとアセットの明確な分離（混在しない構造）、仮想フォルダ vs 実フォルダの分離（実FS同期＋スマートコレクション）

### M-PV-2 Project View Asset Presentation
- thumbnail
- type icon
- size / duration / fps / missing 状態
- selection summary と selection detail を使って、現在選択中 item の path / status を読めるようにしている
- **AE差別化:** ホバープレビュー（サムネイルホバーで動画パラパラ再生、Finder風）、コンポのサムネイルプレビュー、レンダリング状態バッジ（レンダー済み/未レンダー/キャッシュあり）、依存関係の可視化（コンポの依存ツリー表示・逆引き検索）

### M-PV-3 Project View Organization
- folder / bin 整理
- expand / collapse
- unused / tag / virtual view
- **AE差別化:** タグ・カラーラベルでフィルタリング (AEのプロジェクトパネルより使いやすく)、カラムビュー（Finder風ミラー列表示）、ピン留め・スター機能（よく使うコンポ/アセットのクイックアクセス）、未使用アセット・コンポの可視化（グレーアウト/バッジ表示）

### M-PV-4 Project View Interaction Polish
- selection center / quick actions / open-reveal-rename-delete-relink の整理
- **AE差別化:** 最近使ったアセット履歴（プロジェクト跨ぎ）、未使用アセット検出ハイライト、賢いD&D（自動レイヤー生成・複数整列オプション）、構造化クエリ検索（type:comp duration:>30s used:false などのメタデータ検索）
- 詳細は `docs/planned/MILESTONE_PROJECT_VIEW_INTERACTION_POLISH_2026-03-28.md`

### M-PV-5 Project View Search / Filter / Presentation
- incremental search / multi filter pills / unused emphasis / list-grid presentation / status bar を Project View surface にまとめる
- 詳細は `docs/planned/MILESTONE_PROJECT_VIEW_SEARCH_FILTER_PRESENTATION_2026-04-03.md`

### M-PV-6 Project View Scroll Stability ✅ (2026-06-08)
- import で Project View の scroll position を勝手に先頭へ戻さない
- 新規素材追加時も現在の表示位置を維持する
- 詳細は `docs/done/MILESTONE_PROJECT_VIEW_SCROLL_STABILITY_2026-06-07.md`

### M-AS-1 Asset Import Flow
- 読み込み
- 再リンク
- メタ表示
- 未使用管理

### M-AS-2 Composition / Project Organization
- project tree
- 検索
- 並び
- タグ

### M-AS-4 Asset System Integration
- `AssetBrowser` と `Project View` の同期
- import / metadata / relink / missing / unused の統合
- Project View の footage selection から Asset Browser への追従もつなぎ、往復同期へ前進
- Asset Browser / Project View の両方に sync chip を置き、連動状態を見える化
- 詳細は `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`

### M-AS-9 Project / Asset Workflow Bridge
- Project View / Asset Browser / Contents Viewer / Render Queue を一続きにする
- import / relink / recent / favorite / missing / dependency の導線整理
- **AE差別化:** ファイルシステムと直結（AEはインポートしないと使えない）・ホットリロード対応（ファイル更新で自動反映）・未使用アセット検出（UI改善）
- 詳細は `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`
- search / collections / review の派生詳細は別文書へ分割

### M-AS-4b Vector / SVG Layer Import
- SVG などの vector asset を layer として取り込む
- source 保持 / raster preview / relink / persistence
- 詳細は `docs/planned/MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md`

### M-AS-5 Video Layer Unification
- `Video` 縺ｫ荳譛ｬ蛹・
- layer factory / serialization / property / project presentation 縺ｮ豕ｨ諢丈ｺ・
- 隧ｳ邏ｰ縺ｯ `Artifact/docs/MILESTONE_VIDEO_LAYER_UNIFICATION_2026-03-13.md`

### M-AS-6 File Menu Workflow
- project create / open / save / close / restart / quit 縺ｮ謨ｴ逅・
- recent projects / unsaved changes / import / composition create 縺ｮ邨ｱ蜷・
- 隧ｳ邏ｰ縺ｯ `Artifact/docs/MILESTONE_FILE_MENU_2026-03-13.md`

### M-AS-7 Edit Menu Workflow
- undo / redo / copy / cut / paste / delete / duplicate の実コマンド接続
- split / trim / select all / find / preferences の context-aware menu state
- 詳細は `Artifact/docs/MILESTONE_EDIT_MENU_2026-03-13.md`

## Core / Architecture

### M-AR-1 Service Boundary Cleanup
- UI 直参照を減らして service 経由へ統一

### M-AR-2 import std Rollout
- 安全な module から順に C++23 / `import std;` 化

### M-AR-3 Serialization Cleanup
- layer / composition / effect の JSON 保存整理

## Test / Validation

### M-QA-1 Software Test Windows
- current composition / current layer 追従の検証窓を強化

### M-QA-2 Manual Regression Checklist
- タイムライン、render、audio、dock の確認表

### M-QA-3 Crash / Diagnostics
- recovery

## Render / Playback

### M-RP-1 RAM Preview Cache
- frame cache を RAM preview の主経路として扱う
- prewarm / fill / cache range
- playback / scrub / loop との連動
- hit rate / stale cache / dropped frame の可視化
- 詳細は `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`
- 低レベルAI実装メモ: `docs/planned/MILESTONE_PREVIEW_PLAYBACK_PERFORMANCE_LOW_LEVEL_AI_2026-05-23.md`

### M-RP-2 Disk Cache System
- 永続 preview cache / manifest / eviction / diagnostics
- 詳細は `docs/planned/MILESTONE_DISK_CACHE_SYSTEM_2026-03-26.md`

### M-RP-3 GPU-Driven MDI Render
- GPU 側で visibility / compaction / batch formation を行い、MDI submission に繋げる
- 既存の CPU render queue を壊さず、fallback 付きで段階導入する
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_GPU_DRIVEN_MDI_RENDER_2026-04-02.md`

## Matte

### M-LYR-1 Matte Stack / Child Matte Nodes
- matte を layer の child / attached node として扱う
- Add / Common / Subtract の複数 matte 合成
- Alpha / Luminance / Inverted の評価
- dependency order / cycle check / diagnostics
- 詳細は `ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md`

---

## App Layer Completeness

### M-RV-1 Reactive Event Editor Window
- 独立ウィンドウで reactive event ルールを編集する
- Target Tree は owner-draw、他は既存 Qt widget を使う
- Target Tree / Event Rules / Inspector / Event Log を 1 画面にまとめる
- ルールはフレーム末キュー前提で、`PropertyOverlay` と `ContactSubscription` を編集対象にする
- `TimelineReaction` / `TriggerReaction` / `PhysicsReaction` の編集導線を整理する
- 詳細は `docs/planned/MILESTONE_REACTIVE_EVENT_EDITOR_WINDOW_2026-03-29.md`

### M-APP ApplicationLayer completeness
- 詳細は `docs/planned/MILESTONE_APP_LAYER_COMPLETENESS.md`
- Phase 1: サービス層の穴埋め (EffectService, AudioService, TranslationManager)
- Phase 2: Undo Add/RemoveLayerCommand の実装
- Phase 3: EditMode / DisplayMode の UI 接続
- Phase 4: エフェクトパイプライン接続 (Generator::apply, DAG eval, renderFrame)
- Phase 5: データ/永続化 (PreCompose 時間変換, VideoProxy, AspectRatio)
- Phase 6: 拡張 (OFX ホスト, WebBridge)
- ログ
- 診断導線

### M-INF-1 Internal Event System Migration
- `ArtifactCore` の `EventBus` を使って、Project / Timeline / Inspector / Render Queue / Asset Browser の広域更新を段階的に置き換える
- Qt signal は高頻度入力と widget 内部状態に限定し、fan-out の大きい状態変化だけ EventBus に寄せる
- 詳細は `docs/planned/MILESTONE_EVENT_SYSTEM_MIGRATION_2026-03-25.md`
- widget 別の切り替え表は `docs/planned/MILESTONE_EVENT_BUS_WIDGET_MIGRATION_2026-04-01.md`

### M-DEV-1 Crash Diagnostics & Recovery
- 目的: アプリケーションのクラッシュ原因を迅速に特定し、回復フローと診断情報収集を整備する
- 期待結果: クラッシュ時に一貫した診断データ（スタックトレース、重要オブジェクトスナップショット、環境情報）が収集され、主要クラッシュに対するセーフモード起動や自動復旧案内が提供される
- タスク:
  - (1) 既存のログ＆クラッシュダンプ取得フローを調査してドキュメント化
  - (2) 例外／シグナルハンドラでのスタックトレース収集と簡易ダンプの実装
  - (3) 重要オブジェクト（Project, Composition, Asset コンテナ等）のスナップショット保存ロジック追加
  - (4) ユーザ向け回復案内（セーフモード起動、ログ送信）の実装
  - (5) CI/QA 向け再現手順と小規模回帰テストを用意
- 見積: 4-12時間（段階的実装を想定）

---

## Composition Editor & Layer View

### M-CE-GZ-1 ImGuizmo Direct Code Port
- 詳細は `Artifact/docs/MILESTONE_IMGUIZMO_DIRECT_CODE_2026-04-09.md`
- `ImGuizmo` を外部ライブラリとして使うのではなく、描画プリミティブと操作ロジックを Artifact のコードとして移植する
- `TransformGizmo` / `ArtifactIRenderer` / composition overlay へ直接接続する
- translation / rotation / scale を direct code で順に移す
- hit test と draw の座標系を一致させ、backend parity を確認する

### M-CE-TEXT-1 Text Layer Inline Editing
- コンポジットエディタ上で text layer を直接編集する
- caret / selection / IME / commit / cancel を layer モデルに接続する
- 詳細は `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- 編集導線の最小入り口は実装済みで、Phase 2 以降の in-canvas input を残す
- `Ctrl+Enter` の commit shortcut を追加し、Phase 1 の確定導線を少し強化した
- 起動時に全文選択するようにして、置き換え入力の初動を軽くした

### M-TXT-1 Text Animator Next Gen
- AE 風 Text Animator の残タスクを UI / selector / preset / timeline まで詰める
- 詳細は `docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`
- 実行メモは親文書へ統合済み

### M-TXT-2 Text Animator Range Color Editing
- テキスト上の範囲選択から直接 color property を割り当てる
- 文字ごとの色変更を、複製 + マスクや expression に逃げずに扱えるようにする
- 詳細は `docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_WORKFLOW_GAPS_2026-06-07.md`

### C-TXT-6 GPU Text Rendering / Japanese Shaping
- DX12 / Vulkan backend での日本語 text rendering
- glyph atlas / shaping / backend parity
- 詳細は `ArtifactCore/docs/MILESTONE_GPU_TEXT_RENDERING_JA_2026-04-01.md`
- 実行メモは親文書へ統合済み

### Text Workstream Index
- `docs/planned/MILESTONE_TEXT_WORKSTREAM_INDEX_2026-04-30.md`
- Text Animator と GPU Text の入口を 1 枚に束ねる索引

### M-CE-2 Composition Editor Playback Feel Refinement
- playhead / scrub / preview の体感を軽くし、ワープ感や重さを減らす
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md`

### M-CE-3 Responsive Layout Composition
- `ResponsiveComposition` を別種のコンポとして増やすのではなく、`Composition` に `Responsive Layout` 機能を載せる
- 1つの composition 内に `16:9 / 9:16 / 1:1` の layout variant を持たせ、出力先ごとのレイアウト差分を管理する
- 詳細は `docs/planned/MILESTONE_RESPONSIVE_LAYOUT_COMPOSITION_2026-06-05.md`

### M-CE-CONST-1 Construction Layer
- レンダーされない作業用の設計レイヤーを、composition 内で親子付け・アニメーション可能な形で管理する
- line / circle / grid / annotation / safe area / orbit guide を同じ制作文脈に寄せる
- final render では除外しつつ、editor / timeline では見えるようにする
- 詳細は `docs/planned/MILESTONE_CONSTRUCTION_LAYER_2026-06-05.md`

### M-AB Asset Browser Improvement (Unity 風)
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT.md`
- Phase 0: 左ペイン owner-draw 化の基盤作り
- Phase 1: ビュー切替 & ソート (リストビュー、ソートドロップダウン)
- Phase 2: キーボード操作 & ステータス表示 (矢印/Delete、サムネイルバッジ)
- Phase 3: ナビゲーション & プレビュー (ブレッドクラム、ホバープレビュー、お気に入り)
- Phase 4: 同期 & インスペクタ (Browser↔Project 同期、右パネル)
- Phase 5: 高度な機能 (依存関係追跡、Find References、再リンク)
- Phase 6: 右ペイン owner-draw 拡張を将来検討
- 進捗: 状態バー、Icon/List 切替、Name/Type ソート切替は実装済み、owner-draw へ段階移行中

### M-AB-2 Asset Browser Sequence Grouping
- `image_0001.png` 〜 `image_0100.png` のような連番を 1 アセットとして自動グルーピングする
- 正規表現ベースで basename / frame / padding を検出し、展開可能な論理 item として扱う
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_SEQUENCE_GROUPING_2026-03-31.md`

### M-AB-4 Asset Browser Hover Preview
- アセットにホバーすると高品質なプレビューをポップアップ表示
- キャッシュシステム、遅延ローディング、フォールバック機構を実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_HOVER_PREVIEW_2026-06-28.md`

### M-AB-10 Asset Browser Relink Workflow
- 移動/リネームされたアセットファイルの再リンクをサポート
- 依存関係トラッカー、ダイアログUI、アンドゥサポートを実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_RELINK_WORKFLOW_2026-06-28.md`

### M-AB-11 Asset Browser Advanced Sort
- 複数キーによる高度なソート機能を実装
- 自然順序ソート、プリセット保存、設定ダイアログをサポート
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_ADVANCED_SORT_2026-06-28.md`

### M-AB-12 Asset Browser Tag System
- アセットにタグ付け機能を追加
- タグデータベース、タグエディタウィジェット、フィルタリング、クラウド表示を実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_TAG_SYSTEM_2026-06-28.md`

### M-AB-15 Asset Browser AI Support
- AI支援機能をアセットブラウザに統合
- アナライザー、自動タグ付け、類似性検索、レコメンド機能を実装
- 詳細は `docs/planned/MILESTONE_ASSET_BROWSER_AI_SUPPORT_2026-06-28.md`

### M-CP-2 Camera Overlay Experiment ⭐ **新規追加**
- Composition Editor 縺ｧ camera frustum / frame overlay 繧帝∈謚槭〒縺阪ｋ experimental mode
- 2D composition view 縺ｯ縺ｿ縺ｿ螳夂ｾ｡縲・3D editing 縺ｯ縺ゅｊ縺ｪ縺・
- **隧ｳ邏ｰ:** `docs/planned/MILESTONE_CAMERA_OVERLAY_EXPERIMENT_2026-04-02.md`

### M-UI-11a UI Theme System Rollout
- `UI Theme System` 繧貞ｫｸ蜿ｳ縺ｮ螳御ｺ・task 繧定ｵｷ繧後ｋ
- `ArtifactInspectorWidget` / `ArtifactPropertyWidget` / `Render Queue` / `Dock` 縺ｮ謨ｴ逅・priorities 繧偵ｃ縺九■繧阪ｋ
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_UI_THEME_SYSTEM_ROLLOUT_2026-04-02.md`

### M-PY-2 Script Menu / menu.py Loader
- `Script` menu を固定入口として保ちつつ、`scripts/menu.py` から command を追加できるようにする
- Nuke 風の menu script 拡張を将来の安全な入口として準備する
- 詳細: `docs/planned/MILESTONE_SCRIPT_MENU_PY_LOADER_2026-04-02.md`

### M-PY-3 ExtendScript-Style Script Runtime
- `app / project / selection` を中心にした、アプリ内自動化用の script runtime を作る
- AE ExtendScript 風の操作感で、automation / batch / macro / console 実行を扱えるようにする
- 詳細: `docs/planned/MILESTONE_EXTENDSCRIPT_STYLE_SCRIPT_RUNTIME_2026-04-06.md`
- Phase 1 実行メモは親文書へ統合済み

### M-TL-13 Timeline Curve Editor Mode
- `ArtifactTimelineWidget` 繧帝ｸ縺､縺ｮ mode 縺ｫ縺吶ｋ縲ゅΝ繝ｼ繝・ヨ timeline / curve editor 繧偵→縺ｪ縺｣縺ｦ縺ｯ縺薙→縺後ｒ謹ｭ縺｣縺励※縺上□縺輔＞
- `U` / `Tab` 繧ｷ繝ｧ繝ｼ繝･縺ｧ playhead / selection / zoom 繧堤舌・縺励※遉ｾ縺ｦ縺薙・繧ｹ繝医Ο繝・ヱ繝ｫ
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md`

### M-EAS-1 EasingLab
- Compare easing presets side by side for a selected keyframe segment.
- Keep the first slice read-only, then wire apply through the existing undo path.
- Details: `docs/planned/MILESTONE_EASING_LAB_2026-04-21.md`
- execution memo は親文書へ統合済み

### M-EXPR-1 Expression System Completeness ⭐ **新規追加**
- エクスプレッションエンジンはパーサー＋評価器が存在するが、AE ライクな表現力に不足がある
- **不足機能:**
  - Pick-whip UI（プロパティ間ドラッグリンク）
  - 組込みプロパティアクセサ（`position`, `opacity`, `rotation` 等）
  - 特殊変数（`thisComp`, `thisLayer`, `thisProperty`, `time`, `value`）
  - ユーティリティ関数（`wiggle()`, `loopIn()/loopOut()`, `pingPong()`, `valueAtTime()`, `speedAtTime()`, `velocityAtTime()`）
  - `effect()` アクセサ
  - エクスプレッションエラー表示（タイムライン上）
  - エクスプレッション→キーフレーム変換
  - 音声リアクティブ変数（`audioLevels`）
- **見積:** 60-80h
- **依存:** `ExpressionParser` ✅, `ExpressionEvaluator` ✅, `ExprIntrinsics` ✅

### M-EXPR-2 Property Reference Linking / Pick Whip
- property 間の参照関係を視覚的に張る
- expression と driven property の target 選択を簡単にする
- 詳細は `docs/planned/MILESTONE_PROPERTY_REFERENCE_LINKING_2026-05-11.md`
- execution memo は親文書へ統合済み

### M-BLEND-1 Blend Mode Completeness ⭐ **新規追加**
- 現在 18/38 モード実装。以下のモードを追加する:
  - Dissolve / Dancing Dissolve
  - Linear Burn / Classic Color Burn
  - Linear Dodge / Classic Color Dodge
  - Linear Light / Vivid Light / Pin Light / Hard Mix
  - Classic Difference
  - Divide
  - Stencil Alpha / Stencil Luma / Silhouette Alpha / Silhouette Luma
- CPU (`QPainter::CompositionMode`) と GPU (compute shader) の両方で対応
- **見積:** 20-30h
- **依存:** `ColorBlendMode` ✅, 各 render path

### M-FX-10 Effects Coverage Expansion ⭐ **新規追加**
- 既存 ~15 エフェクトから AE ライクなエフェクトカタログへ拡充する
- **不足カテゴリ（優先度順）:**
  - **Generate:** Fill, Stroke, Circle, Ellipse, Checkerboard, Gradient Ramp, Grid
  - **Distort:** Displacement Map, Turbulent Displace, Mesh Warp, Liquify, Optics Compensation
  - **Stylize:** Cartoon, Emboss, Find Edges, Mosaic, Brush Strokes, Scatter
  - **Perspective:** Drop Shadow, Radial Shadow, Basic 3D, Bevel Alpha
  - **Transition:** Dissolve, Iris Wipe, Linear Wipe, Card Wipe, Gradient Wipe
  - **Time:** Echo, Time Difference, Posterize Time, CC Force Motion Blur
  - **Utility:** Cineon Converter, Apply Color LUT, Color Profile Converter
- 各エフェクトは CPU reference 実装→GPU compute shader の 2 段階
- **見積:** 120-180h（カテゴリ単位で分割可能）
- **依存:** `EffectStack` ✅, `OFXHost` ✅, `GPUComputeContext` ✅

## Good Small Tasks

- `M-AR-2 import std Rollout`
- `M-UI-2 Dock / Tab Polish`
- `M-QA-1 Software Test Windows`
- `M-FX-2 Solid Color Effects`
- `M-FX-4 Creative Workflow (Bridge only)`
- `M-PS-1 AE Utility Script Pack`
  - `Quick Rename Layers` / `Clean Layers` / `Trim Comp to Content` をまとめた AE 風の小型自動化群

## Next Recommended

- `M-REACTIVE-1 Reactive Events Engine`
  - `ReactiveEvents` はデータモデルが揃っているので、次は `evaluate()` とリアクション実行の最短経路を閉じる
  - まずは layer property callback と EventBus 送出までに絞り、UI 連携は後段へ分ける
  - 詳細: `docs/analysis/REACTIVE_EVENTS_ENGINE_DESIGN_2026-07-25.md`

### Legacy Note: Timeline Curve Editor Mode
- `ArtifactTimelineWidget` 縺ｧ normal timeline / curve editor 繧偵→縺ｪ縺｣縺ｦ縺ｯ縺薙→縺後ｒ謹ｭ縺｣縺励※縺上□縺輔＞
- `U` 繧ｷ繝ｧ繝ｼ繝･縺ｧ mode toggle, `Tab` / `Shift+Tab` 縺ｧ curve editor 内 focus traversal
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md`

## Timeline / Layer (new)

### M-TM-1 Track Matte Drag-Link UX
- レイヤーパネルから Alt + ドラッグでトラックマット受け側レイヤーを指定する UI
- Inspector の Matte セクション強化（MatteType 即切替 / 参照表示）
- ドラッグ中のハイライトインジケータ、循環参照拒否、Undo/Redo 対応
- 詳細: `docs/planned/MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md`

### M-TA-2 Timeline Audio Waveform Display
- タイムラインの Audio Layer 行で波形(peak min/max)をトラック内に描画
- ズームに応じた粗密制御、フェードハンドル/音量オートメーション keyframe 可視化
- 波形クリックでの seek、trim/gain/fade のドラッグ編集、Undo 対応
- 完了: `docs/done/MILESTONE_TIMELINE_AUDIO_WAVEFORM_2026-06-01.md`

## Multi-Viewport / Preview

### M-VP-1 Multi-Viewport Layout System
- Single / HorizontalSplit / Four-Up / OnePlusThree レイアウト切替 API
- 各ペインに任意の Camera Layer (Perspective / Orthographic: Top/Front/Left) をバインド
- ペインごとの独立 Zoom/Pan 状態保持、playhead の同期更新
- EventBus でのペインイベント multicast、非アクティブペイン低Hz ポーリングによる最適化
- 詳細: `docs/planned/MILESTONE_MULTI_VIEWPORT_LAYOUT_2026-06-01.md`

### M-VP-9 Viewport Interaction / Navigation / 3D Cursor
- C4D 的な Point of Interest navigation、Frame Selected、View Undo / Redo、軽量 HUD を共通操作にする
- Blender 的な 3D Cursor / Work Cursor を、pivot、orientation、生成位置、snap の共通基準点にする
- preview-only view と render camera、3D Cursor と object pivot を明確に分離する
- 詳細: `docs/planned/MILESTONE_VIEWPORT_INTERACTION_NAVIGATION_CURSOR_2026-07-04.md`

### M-VP-4 Viewport Canvas Rotation System
- After Effects風のキャンバス回転機能（任意角度 -180°〜+180°）
- マウスジェスチャー（Shift+ドラッグ）で回転、Rキーでリセット
- 回転中心はキャンバス中央、状態はプロジェクトに保存
- `ViewportTransformer` に回転フィールドと座標変換ロジックを追加
- `ViewportCB` に回転情報（ラジアン）を追加
- 詳細: `docs/planned/MILESTONE_VIEWPORT_CANVAS_ROTATION_2026-06-27.md`

### M-VP-5 Viewport Dynamic Resolution Switching
- 表示解像度をリアルタイムで切り替え（25%/50%/75%/100%/150%/200%/カスタム）
- Ctrl+ホイールで解像度変更、メニューからのプリセット選択
- DPR（Device Pixel Ratio）との適切な連動
- `ViewportTransformer` に解像度スケールとDPRフィールドを追加
- レンダーターゲットの再作成に解像度を考慮
- 詳細: `docs/planned/MILESTONE_VIEWPORT_DYNAMIC_RESOLUTION_2026-06-27.md`

### M-VP-8 Viewport Bookmarks System
- ビューポート状態（ズーム・パン・回転・解像度）をブックマークとして保存/復元
- 1-9キーでブックマーク適用、Ctrl+1-9で現在の状態を保存
- `ViewportBookmarkManager` シングルトンによる集中管理
- プロジェクトごとの保存、名前付けと整理、削除と並べ替え
- 詳細: `docs/planned/MILESTONE_VIEWPORT_BOOKMARKS_2026-06-27.md`

### M-PQ-1 Proxy Quality Toggle in Preview UI
- Playback Control / Viewer フッターから Draft(1/4) / Preview(1/2) / Full を切替
- quality 切替で render cache invalidation と必要に応じて warm-up 再キャッシュ
- Composition 設定として quality preset 保存（新規作成時に復元）
- 詳細: `docs/planned/MILESTONE_PROXY_QUALITY_TOGGLE_UI_2026-06-01.md`

### M-VP-9 Viewport Interaction / Navigation / 3D Cursor
- C4D 的な Point of Interest navigation、Frame Selected、View Undo / Redo、軽量 HUD を共通操作にする
- Blender 的な 3D Cursor / Work Cursor を、pivot、orientation、生成位置、snap の共通基準点にする
- preview-only view と render camera、3D Cursor と object pivot を明確に分離する
- 詳細: `docs/planned/MILESTONE_VIEWPORT_INTERACTION_NAVIGATION_CURSOR_2026-07-04.md`

### M-PQ-2 Footage Interpret Safety / Proxy Workflow
- footage interpret の frame rate 変更時に keyframe / time remap への影響を明示する
- proxy の生成と切り替えを 1 つの workflow にまとめる
- 詳細は `docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_WORKFLOW_GAPS_2026-06-07.md`

### M-MASK-2 Mask Feather Directional / Render FPS Safety
- mask feather を horizontal / vertical / inner / outer に分ける
- export / render の frame rate 初期値を composition に同期する
- 詳細は `docs/planned/MILESTONE_MASK_FEATHER_DIRECTIONAL_AND_RENDER_FPS_SAFETY_2026-06-07.md`

### M-3D-2 3D Viewport Orbit / Pan / Preview Mode
- `Alt + Left Drag` orbit / `Middle Drag` pan / wheel zoom を 3D viewport の共通操作にする
- camera を直接動かすモードと preview-only mode を分離する
- 詳細は `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`

### M-TL-2 Scrub Accuracy / Expression Recursion / Cache Reuse
- フレーム単位の scrub で特定 frame を飛ばさない
- expression に安全な再帰と loop を追加する
- render / export で frame cache を共有する
- 詳細は `docs/planned/MILESTONE_SCRUB_EXPRESSION_CACHE_REUSE_2026-06-07.md`

### M-EXPR-2 Expression Subframe / Timestep Policy ⚠️ (Phase 1+2 done 2026-06-08)
- expression の評価を frame locked だけに固定せず、subframe / adaptive step を選べるようにする
- 30fps / 60fps で物理系式の挙動が変わりにくい評価ポリシーを作る
- Phase 1 (Time Evaluation Contract) ✅: EvaluationMode enum, frameRate 変数, evaluateAtTime
- Phase 2 (Subframe Sampling) ✅: 任意時刻評価, evaluateOverRange (4 mode対応)
- Phase 3-5 未着手
- 詳細は `docs/planned/MILESTONE_EXPRESSION_SUBFRAME_TIMESTEP_POLICY_2026-06-07.md`

### M-UI-24 UI Layout Undo History
- panel close / tab move / split / dock rearrange を undoable にする
- 間違えて閉じた UI を `Ctrl+Z` で戻せるようにする
- 詳細は `docs/planned/MILESTONE_UI_LAYOUT_UNDO_HISTORY_2026-06-07.md`

### M-UI-25 Context Menu Compact Actions
- 右クリックメニューを frequent / all に分けて、初期表示を 10 項目以内に抑える
- よく使う項目のカスタマイズとカテゴリ分けを導入する
- 詳細は `docs/planned/MILESTONE_CONTEXT_MENU_COMPACT_ACTIONS_2026-06-07.md`

### M-UI-26 Numeric Field Quick Calc
- 数値フィールドで `+10` / `-5` / `*2` / `/3` の簡易計算式を受け付ける
- Enter 確定で計算結果を反映し、数値の再入力を減らす
- 詳細は `docs/done/MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md`

### M-TL-3 Keyframe Nudge / Temp Snap Override ✅ 完了
- 完了: `docs/done/MILESTONE_KEYFRAME_NUDGE_AND_TEMP_SNAP_OVERRIDE_2026-06-07.md`

### M-CE-4 Aspect Ratio / Resolution Remap Wizard ⚠️ (Phase 3 done 2026-06-08)
- aspect ratio 変更時に mask / keyframe / anchor を自動再計算する
- `Center Locked` / `Top Left Locked` / `Stretch To Fit` などの保持基準を選べる
- Phase 1 (Preflight) ✅: 差分表示, impact 列挙, 警告表示
- Phase 2 (Policy + Remap) ✅: ResolutionRemap utility, ウィザード, mask 頂点 remap
- Phase 3 (Preview + Apply + Undo) ✅: アスペクト比プレビュー, アンカー検出有効化, 影響表示
  - 残: undo/redo基盤, プロパティキーフレームremap, アンカー位置remap実装
- 詳細は `docs/planned/MILESTONE_ASPECT_RATIO_RESOLUTION_REMAP_WIZARD_2026-06-07.md`

## Motion Graphics

### M-MG-1 Motion Graphics Template System (mogrt-like)
- `ArtifactTemplateDocument` — exposedParams 定義 + layer tree + keyframe snapshot
- Export / Import (.artemplate) — 選択レイヤー郡からテンプレート抽出・再配置
- Inspector に Template Parameters セクション（Scalar/Point/Color/Text/Enum）
- Template Library Browser（カテゴリ/タグ/サムネイル / DnD 配置）
- .mogrt 互換読込の option（unzip → JSON ヘッダ + layer tree 抽出）
- 詳細: `docs/planned/MILESTONE_MOTION_GRAPHICS_TEMPLATE_2026-06-01.md`

## Terminal / Shell

### M-UI-24 Terminal Shell / Command Surface
- debug console とは別の、power user 向けの command terminal surface を用意する
- `PowerShellWidget` を使って command / history / working dir / exit code を扱う
- 詳細は `docs/planned/MILESTONE_TERMINAL_SHELL_2026-04-06.md`


### M-RD-14 VideoLayer Playback Stability
- `ArtifactVideoLayer` の play → stop が不安定。非同期デコードパイプラインに stop / cancel / reset が存在しない
- `stop()` 新設、デコード世代管理（generation counter）、`seekToFrame()` と `decodeCurrentFrame()` の同期、`currentFrameImageBuffer()` のリファクタ
- 詳細は `docs/done/MILESTONE_VIDEO_LAYER_PLAYBACK_STABILITY_2026-06-25.md`


















