# Milestones Backlog

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
  - Phase 1 実行メモ: `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE1_2026-04-21.md`
  - Phase 2 実行メモ: `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE2_2026-04-21.md`
  - Phase 3 実行メモ: `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE3_2026-04-21.md`
  - Phase 4 実行メモ: `docs/planned/MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE4_2026-04-21.md`

- **M-DIAG-4** Live Frame Pipeline / Resource Watcher / State Diff Tracker
  - Pass DAG / RT・Texture・Buffer lifetime / barrier hazard を常時追う
  - 任意 resource のライブ inspector と pixel inspect を持つ
  - 前フレームとの差分から壊れ始めた瞬間を自動検出する
  - 主要ファイル: `ArtifactCore/include/Render/*`, `Artifact/src/Widgets/Diagnostics/*`, `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
  - 詳細: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_2026-04-21.md`
  - Phase 1 実行メモ: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE1_EXECUTION_2026-04-21.md`
  - Phase 2 実行メモ: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE2_EXECUTION_2026-04-21.md`
  - Phase 3 実行メモ: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE3_EXECUTION_2026-04-21.md`
  - Phase 4 実行メモ: `docs/planned/MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE4_EXECUTION_2026-04-21.md`

### Project View / Asset System
- **M-PV-1** Project View Basic Operations ✅ (verified 2026-04-14)
  - selection center/quick actions/sync chip/inline rename 実装済み
  - 主要ファイル: `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`

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

- **M-UI-3** Inspector Usability ✅ (verified 2026-04-14)
  - キーボードショートカット/ステータスバー/レイヤーラベルカラー/整列分布機能
  - 主要ファイル: `Artifact/src/Widgets/ArtifactAlignmentWidget.cppm`, `Artifact/src/Widgets/ArtifactStatusBar.cpp`

- **M-UI-5** Contents Viewer Expansion ✅ (verified 2026-04-14)
  - テキストレイヤーインライン編集実装済み、Ctrl+Enter commit shortcutあり
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

### Composition Editor / Cache
- **M-CE-1** Composition Editor Cache System ✅ (verified 2026-04-14)
  - Surface cache / render key suppression / ROIシステム実装済み
  - 主要ファイル: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- **M-CE-2** Static Layer GPU Cache ⚠️ (partial - 2026-04-14)
  - マイルストーン文書と設計は存在するが、専用GPU cacheクラスの実装は未確認
  - `PrimitiveRenderer2D` の cacheKey ベース最適化は実装済み
  - 主要ファイル: `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`

### Render Execution / Isolation
- **M-RE-1** External Renderer Design
  - 内蔵レンダラは維持しつつ、オフラインレンダリングだけ別プロセスへ切り出す
  - job snapshot / CLI / progress / diagnostics の設計を先に固める
  - 詳細: `docs/planned/MILESTONE_EXTERNAL_RENDERER_DESIGN_2026-04-22.md`

### AI / Tooling
- **M-AI-1** MCP/Tool Bridge ✅ Phase 1完了 (verified 2026-04-14)
  - McpBridge::handleRequest() / AIContext 実装済み
  - 主要ファイル: `ArtifactCore/include/AI/McpBridge.ixx`

- **M-AI-2** AI Command Sandbox ✅ (verified 2026-04-14)
  - CommandSandbox.ixx（674行）で policy/execution/timeout すべて実装済み
  - 主要ファイル: `ArtifactCore/include/AI/CommandSandbox.ixx`

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
- Phase 2 の実行メモは `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE2_EXECUTION_2026-04-20.md`
- Phase 3 の実行メモは `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE3_EXECUTION_2026-04-20.md`
- Phase 4 の実行メモは `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE4_EXECUTION_2026-04-20.md`
- Phase 5 の実行メモは `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE5_EXECUTION_2026-04-20.md`
- Phase 6 の実行メモは `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE6_EXECUTION_2026-04-20.md`
- Phase 7 の実行メモは `docs/planned/MILESTONE_HOST_CONTEXT_ROI_PROPERTY_CORE_PHASE7_EXECUTION_2026-04-20.md`

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
- Phase 1 実行メモ: `docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE1_2026-04-21.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE2_2026-04-21.md`
- Phase 3 実行メモ: `docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_PHASE3_2026-04-21.md`

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

### M-APP-4 App Debugger Visual Hierarchy / Color Semantics
- App Debugger の情報階層、色の意味、異常時の見え方を整えて、人間が読みやすい diagnostics surface に寄せる
- 詳細は `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_2026-04-23.md`
- Phase 1 実行メモ: `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE1_EXECUTION_2026-04-24.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE2_EXECUTION_2026-04-24.md`
- Phase 3 実行メモ: `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE3_EXECUTION_2026-04-24.md`
- Phase 4 実行メモ: `docs/planned/MILESTONE_APP_DEBUGGER_VISUAL_HIERARCHY_COLOR_SEMANTICS_PHASE4_EXECUTION_2026-04-24.md`

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
- AI の提案を確認付きで編集へ反映するための安全な write surface
- `ArtifactProjectService` / `ArtifactEffectService` / render queue service を薄い wrapper として再利用する
- dry-run / confirmation / undo を前提に、rename / import / queue / bulk edit を先行する
- 詳細は `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_2026-04-21.md`
- Phase 1 実装メモは `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE1_2026-04-21.md`
- Phase 2 実装メモは `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE2_2026-04-21.md`
- Phase 3 実装メモは `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE3_2026-04-21.md`

### M-AI-6 Workflow Automation
- `WorkspaceAutomation` を中心に project / composition / selection / render queue の作業を束ねる
- snapshot / safe edit / queue control / batch automation を 1 つの流れにする
- 詳細は `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_2026-04-21.md`
- Phase 1 実装メモは `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE1_2026-04-21.md`
- Phase 2 実装メモは `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE2_2026-04-21.md`
- Phase 3 実装メモは `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE3_2026-04-21.md`

### M-AI-2 AI Command Sandbox / CLI Execution
- AI 縺ｫ縺ｯ shell string 縺ｧ縺ｪ縺上↑繧峨〒縺・、program + argv 繧帝攝縺励※謇薙∴繧・
- allowlist / timeout / working directory / output cap 繧定ｨ倬鹸縺励※縲∝ｧ・ｭｷ螟夜Κ繧ｳ繝槭Φ繝峨ｒ縺ｾ縺・☆繧・
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_AI_COMMAND_SANDBOX_2026-04-10.md`

### M-AI-1 MCP / Tool Bridge Foundation
- `DescriptionRegistry` / `AIToolExecutor` / `AIContext` を使って AI tool schema を安定化する
- local / cloud / 将来の MCP bridge から共通で使える tool 境界を切る
- 詳細は `docs/planned/MILESTONE_AI_MCP_TOOL_BRIDGE_2026-04-10.md`
- Phase 1 実行メモ: `docs/planned/MILESTONE_AI_MCP_TOOL_BRIDGE_PHASE1_EXECUTION_2026-04-10.md`

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

### M-FE-1 Onboarding / Empty States
- empty project / empty selection / empty asset / empty timeline の案内
- 詳細は `docs/planned/MILESTONE_ONBOARDING_EMPTY_STATES_2026-03-27.md`

### M-FE-2 Export / Review / Share
- export 後の quick review / reveal / share を短くする
- 詳細は `docs/planned/MILESTONE_EXPORT_REVIEW_SHARE_2026-03-27.md`

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
- Phase 1 実行メモ: `docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_PHASE1_EXECUTION_2026-04-03.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_PHASE2_EXECUTION_2026-04-03.md`
- Phase 3 実行メモ: `docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_PHASE3_EXECUTION_2026-04-03.md`
- Phase 4 実行メモ: `docs/planned/MILESTONE_ASSET_BROWSER_NAVIGATOR_PHASE4_EXECUTION_2026-04-03.md`

### M-TL-10 Timeline Feature Implementation / Interaction Surface
- Timeline の layer / clip / keyframe / search / visual language / owner-draw を一つの実行計画として束ねる
- 既存の timeline milestone を置き換えず、順序と責務をまとめる
- 詳細は `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md`
- Phase 1 実行メモ: `docs/planned/MILESTONE_TIMELINE_FEATURE_PHASE1_EXECUTION_2026-04-03.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_TIMELINE_FEATURE_PHASE2_EXECUTION_2026-04-03.md`
- Phase 3 実行メモ: `docs/planned/MILESTONE_TIMELINE_FEATURE_PHASE3_EXECUTION_2026-04-03.md`
- Phase 4 実行メモ: `docs/planned/MILESTONE_TIMELINE_FEATURE_PHASE4_EXECUTION_2026-04-03.md`

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
- Phase 1 実行メモ: `docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_PHASE1_EXECUTION_2026-04-03.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_PROPERTY_WIDGET_ROW_ALIGNMENT_PHASE2_EXECUTION_2026-04-03.md`

### M-UI-22 QSS Decommission / CommonStyle Path to QCommonStyle
- `QSS` を新規追加しない方針へ切り替え、theme / palette / common widget / owner-draw を経由して最終的に `QCommonStyle` ベースへ寄せる
- 既存の `QSS Reduction` と `Theme System Rollout` の実行計画をまとめ直す
- 詳細は `docs/planned/MILESTONE_QSS_DECOMMISSION_COMMONSTYLE_2026-04-03.md`

### M-SC-2 Shortcut Context Map / Blender-Like Keymap Routing
- `InputOperator` の context 解決順と widget / region 単位の分割を固定し、Blender 風の「場所とモードで意味が変わる」ショートカット routing を明文化する
- `ArtifactCompositionRenderWidget` / `ArtifactTimelineWidget` / `ArtifactLayerPanelWidget` / `ArtifactAssetBrowser` / `ArtifactInspectorWidget` を先行対象にする
- 詳細は `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- Phase 1 実行メモ: `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_PHASE1_EXECUTION_2026-04-21.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_PHASE2_EXECUTION_2026-04-21.md`
- Phase 3 実行メモ: `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_PHASE3_EXECUTION_2026-04-21.md`
- Phase 4 実行メモ: `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_PHASE4_2026-04-21.md`

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

### M-UI-12 Composition Notes / Scratchpad
- コンポジション / レイヤー / フレームに紐づく軽量メモを残せるようにする
- review / annotation より前段の、制作中の書きなぐりメモを扱う
- 詳細は `docs/planned/MILESTONE_COMPOSITION_NOTES_SCRATCHPAD_2026-03-30.md`

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

### M-UI-6 Composition Motion Path Overlay
- composition viewport 上で selected layer の motion path と keyframe dot を重ね描きする
- **AE差別化:** 点の色をレイヤーカラーと連動（どのレイヤーのパスかひと目でわかる）、ホバーで該当フレームのタイムコードをツールチップ表示、パスの点をドラッグして直接位置編集（Nuke風）
- 詳細は `docs/planned/MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md`

### M-UI-7 Composition Editor Mask / Roto Editing
- composition editor 上で layer mask / roto を直接編集できるようにする
- **大幅改善:** モード自動切り替え（コンテキスト判定）、頂点操作直感化（ハンドル/追加削除ツール不要）、マスク管理UI強化（色設定/ドラッグ順序）、視覚フィードバック改善（オーバーレイ/境界線強化）
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md`

### M-UI-4 Menu-to-App Command Routing
- File / Composition / Edit / View / Layer / Render / Help の menu を app service / command に正しく接続する
- 詳細は `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`

### M-UI-4b Toolbar / App Integration
- `ArtifactToolBar` を app command surface として整理し、menu / shortcut / workspace state と揃える
- Qt の新規 signal / slot は増やさず、既存 service / event / 明示 refresh で同期する
- 詳細は `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`

### M-UI-5 Contents Viewer Expansion
- image / video / audio / 3D model / source / final / compare を横断する viewer の拡充
- audio playback と live waveform preview を同一 surface で確認できるようにする
- ✅ テキストレイヤーインライン編集 (実装済み)
- 詳細は `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
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
- **詳細:** `docs/planned/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`

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
- 詳細は `docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_EXECUTION_2026-04-23.md`

### M-TL-5 Timeline Keyframe Editing
- Timeline 上で property keyframe を見て、打って、移動できるようにする
- **AE差別化:** キーフレームの時間軸スケーリング（全レイヤー一括でタイミング伸縮、pivot点基準で相対関係維持）
- 詳細は `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`

### M-TL-6 Timeline Layer Search
- タイムライン上部の検索バーで layer / effect / tag / state をインクリメンタルに絞り込む
- 詳細は `docs/planned/MILESTONE_TIMELINE_LAYER_SEARCH_2026-03-28.md`

### M-TL-7 Timeline Search / Keyframe Integration
- search 結果から keyframe へ素早く飛べるようにし、header / status / highlight を統合する
- 詳細は `docs/planned/MILESTONE_TIMELINE_SEARCH_KEYFRAME_INTEGRATION_2026-03-28.md`

### M-LG-1 Layer Group System
- レイヤーグループの保存 / 表示 / 親子 / 可視性 / 操作単位を整理する
- 詳細は `docs/planned/MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md`
- Phase 1 実行メモ: `docs/planned/MILESTONE_LAYER_GROUP_SYSTEM_PHASE1_EXECUTION_2026-04-10.md`

### M-LG-2 Layer Components: Physics / Behavior
- layer 側に軽量 component system を追加し、追従・減衰・トリガーの受け皿を作る
- 詳細は `docs/planned/MILESTONE_LAYER_COMPONENT_SYSTEM_UNITY_LIKE_2026-04-08.md`

### M-PH Playhead 整備
- 詳細は `docs/planned/MILESTONE_PLAYHEAD.md`
- Phase 1: 状態統一 (position_ を唯一の権威に集約)
- Phase 2: シーク UX 改善 (不感帯、スナップ、再生中シーク、スクロール追従)
- Phase 3: 表示品質 (F<n>/HH:MM:SS:FF 同期、高DPI、コンポジションビュー連携)
- Phase 4: 操作拡充 (J/K/L シャトル、タイムコード入力、ホイールシーク、ドラッグシーク)

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

### M-RD-11 GPU Mask Cutout Compute Pipeline
- layer mask の cutout を compute shader 経由に寄せ、CPU の `cv::Mat` 切り抜きを段階的に減らす
- CPU fallback を残しつつ、mask texture を GPU 側の中間資源として扱えるようにする
- 詳細は `docs/planned/MILESTONE_GPU_MASK_COMPUTE_PIPELINE_2026-04-03.md`

### M-RD-6 FFmpeg GPU Decode Backend
- CPU decode とは別に FFmpeg hwaccel backend を持ち、video layer / playback / preview から選べるようにする
- 詳細は `docs/planned/MILESTONE_FFMPEG_GPU_DECODE_BACKEND_2026-03-28.md`

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
- current composition / current layer の追従
- solo 表示の安定化
- mask / roto 入口の整理
- software test widget との見え方差分縮小
- context / impact / before-after の可視化
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
- Phase 1 実行メモ: `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_PHASE1_EXECUTION_2026-04-21.md`
- Phase 2 実行メモ: `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_PHASE2_EXECUTION_2026-04-21.md`
- Phase 3 実行メモ: `docs/planned/MILESTONE_COMPOSITION_EDITOR_FIGMA_LIKE_OVERLAY_PHASE3_EXECUTION_2026-04-21.md`

## Shared Notes

- `docs/shared/ai-tech-memos/README.md`
- AI 同士で実装メモや調査要点を共有するための軽量な置き場

## Effects

### M-FX-1 Inspector Effect Stack Bridge
- Inspector から effect 追加、削除、順序変更

### M-FX-2 Solid Color Effects
- Color Wheels
- Curves
- Grader を Solid に通す

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

### M-CE-2 Composition Editor Playback Feel Refinement
- playhead / scrub / preview の体感を軽くし、ワープ感や重さを減らす
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_PLAYBACK_FEEL_REFINEMENT_2026-04-23.md`

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
- Phase 1 実行メモ: `docs/planned/MILESTONE_EXTENDSCRIPT_STYLE_SCRIPT_RUNTIME_PHASE1_EXECUTION_2026-04-06.md`

### M-TL-13 Timeline Curve Editor Mode
- `ArtifactTimelineWidget` 繧帝ｸ縺､縺ｮ mode 縺ｫ縺吶ｋ縲ゅΝ繝ｼ繝・ヨ timeline / curve editor 繧偵→縺ｪ縺｣縺ｦ縺ｯ縺薙→縺後ｒ謹ｭ縺｣縺励※縺上□縺輔＞
- `U` / `Tab` 繧ｷ繝ｧ繝ｼ繝･縺ｧ playhead / selection / zoom 繧堤舌・縺励※遉ｾ縺ｦ縺薙・繧ｹ繝医Ο繝・ヱ繝ｫ
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md`

### M-EAS-1 EasingLab
- Compare easing presets side by side for a selected keyframe segment.
- Keep the first slice read-only, then wire apply through the existing undo path.
- Details: `docs/planned/MILESTONE_EASING_LAB_2026-04-21.md`
- Phase 1 execution: `docs/planned/MILESTONE_EASING_LAB_PHASE1_EXECUTION_2026-04-21.md`
- Phase 2 execution: `docs/planned/MILESTONE_EASING_LAB_PHASE2_EXECUTION_2026-04-21.md`
- Phase 3 execution: `docs/planned/MILESTONE_EASING_LAB_PHASE3_EXECUTION_2026-04-21.md`

## Good Small Tasks

- `M-AR-2 import std Rollout`
- `M-UI-2 Dock / Tab Polish`
- `M-QA-1 Software Test Windows`
- `M-FX-2 Solid Color Effects`
- `M-FX-4 Creative Workflow (Bridge only)`

### Legacy Note: Timeline Curve Editor Mode
- `ArtifactTimelineWidget` 縺ｧ normal timeline / curve editor 繧偵→縺ｪ縺｣縺ｦ縺ｯ縺薙→縺後ｒ謹ｭ縺｣縺励※縺上□縺輔＞
- `U` 繧ｷ繝ｧ繝ｼ繝･縺ｧ mode toggle, `Tab` / `Shift+Tab` 縺ｧ curve editor 内 focus traversal
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TIMELINE_CURVE_EDITOR_MODE_2026-04-10.md`

## Terminal / Shell

### M-UI-24 Terminal Shell / Command Surface
- debug console とは別の、power user 向けの command terminal surface を用意する
- `PowerShellWidget` を使って command / history / working dir / exit code を扱う
- 詳細は `docs/planned/MILESTONE_TERMINAL_SHELL_2026-04-06.md`
