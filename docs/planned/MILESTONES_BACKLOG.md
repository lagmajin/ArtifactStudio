# Milestones Backlog

空いている時間に進めやすいよう、分野別に小さめのマイルストーンへ分割したバックログ。

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

## 📋 最近の実装完了状況 (2026-03-29 更新)

| カテゴリ | 完了項目 | ステータス |
|----------|----------|-----------|
| **UI/UX** | テキストレイヤーインライン編集 | ✅ |
| **UI/UX** | キーボードショートカット追加 | ✅ |
| **UI/UX** | ステータスバーコンポジション情報 | ✅ |
| **UI/UX** | レイヤーラベルカラー機能 | ✅ |
| **UI/UX** | レイヤー整列・分布機能 | ✅ |
| **UI/UX** | コマンドパレット (Ctrl+F) | ✅ |
| **Layer** | Undo/Redo統合 | ✅ |
| **Render** | ROIシステム実装 | ✅ |
| **Render** | ギズモ描画最適化 | ✅ |
| **Core** | Expression Evaluator修正 | ✅ |
| **QA** | Project File Validation Phase 1 (命名検査) | ✅ |
| **UI** | Keyboard Overlay (Ctrl+/ ショートカット) | ✅ |
| **QA** | Project File Validation Phase 2 (スペルチェック) | ✅ |
| **AS** | Save/Load Integrity Phase 1+2 (原子操作的保存+バージョン管理) | ✅ |
| **AS** | Save/Load Integrity Phase 3-5 (参照切れ検出+バリデーション+増分保存) | ✅ |

---

## Application

### M-APP-1 Application Cross-Cutting Improvement
- menu / toolbar / shortcut / view / diagnostics / workflow を横断で揃える
- 詳細は `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`

### M-APP-2 Deferred UI Initialization / Lazy Load
- icon / thumbnail / viewer / dock の eager load を減らして初回体感を軽くする
- 詳細は `docs/planned/MILESTONE_DEFERRED_UI_INITIALIZATION_2026-03-27.md`

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

### M-PY-1 Python API & Scripting Console ⭐ **新規追加**
- プロ向け自動化基盤と組み込みコンソール
- **機能:** Python ブリッジ (pybind11)・プラグインマネージャー・REPL ウィジェット
- **見積:** 40-50h
- **詳細:** `docs/planned/MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md`

### M-FE-7 Review Workspace: Frame-Accurate Compare & Annotation ⭐ **再定義**
- 正確な再生 (frame-accurate, LUT/OCIO, タイムコード固定)
- A/B比較 (swap/wipe/diff), ショット管理 (versions), 注釈 (vector annotation), JKLナビ
- **目標:** レビュー作業専用の独立 workspace を構築し、確認精度を向上させる
- **見積:** 40-50h
- **詳細:** `docs/planned/MILESTONE_REVIEW_WORKSPACE_2026-04-03.md`

### M-FE-8 Search / Collections / Smart Organization
- global search / smart bin / tag / dependency / missing / duplicate detection
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

### M-UI-13 Keyboard Overlay ✅ 完了
- アプリ内ショートカットを一覧できる軽量 overlay を実装する
- `Help` メニューやショートカットからすぐ開けるようにする
- 詳細は `docs/planned/MILESTONE_KEYBOARD_OVERLAY_2026-03-30.md`
- ✅ `KeyboardOverlayDialog` 実装済み（検索、コンパクトモード、Ctrl+/ ショートカット、Helpメニュー接続）

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

### M-TL-1 Layer Basic Operations ✅ 完了
- 追加、削除、複製、rename、親子、並び替え
- ✅ Undo/Redo 統合 (実装済み)
- 残差分: track matte mode (未定義)

### M-TL-2 Layer View Sync ✅ 完了
- 左ツリー展開と右トラック行の同期
- 1レイヤー1クリップの維持
- 残差分: track matte 表示, audio state 連携 (M-AU 側), スクロール双方向同期

### M-TL-3 Work Area / Range Unification ✅ 完了
- in / out
- work area
- seek
- render 範囲の一本化
- 残差分: レンジプリセット UI (M-RD-2 側), 共通レンジサービス (M-RANGE-2)
- 詳細は `Artifact/docs/MILESTONE_TIMELINE_RANGE_UNIFICATION_2026-03-17.md`

### M-TL-4 Timeline TrackView Owner-Draw Migration
- 右ペインを `QGraphicsView` から owner-draw へ段階移行する
- 詳細は `docs/planned/MILESTONE_TIMELINE_TRACKVIEW_OWNER_DRAW_MIGRATION_2026-03-27.md`

### M-TL-8 Timeline QGraphicsScene Elimination
- 右タイムラインの `QGraphicsScene` 依存を painter 側へ段階的に外していく
- 詳細は `docs/planned/MILESTONE_TIMELINE_QGRAPHICSSCENE_ELIMINATION_2026-03-31.md`

### M-TL-9 Timeline Visual Language
- レイヤーバー、キーフレーム、再生ヘッド、選択ハイライトの色と形を意味ベースで統一する
- 詳細は `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`

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

### M-LG-2 Layer Components: Physics / Behavior
- layer 側に Physics / Behavior の component group を追加し、追従・減衰・トリガーの受け皿を作る
- 詳細は `docs/planned/MILESTONE_LAYER_COMPONENTS_PHYSICS_BEHAVIOR_2026-03-28.md`

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

### M-RD-4 Render / Output Feel Refinement ✅ 完了
- 途中失敗からの再開
- frame / layer / effect cost の可視化
- rename / history / visibility inspector
- 詳細は `docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md`
- ✅ Phase 1: 失敗フレーム検出 + 再レンダリング（`detectFailedFrames`, `rerenderFailedFrames`）
- ✅ Phase 3: ジョブ名編集（`jobName` フィールド + `setJobNameAt`）+ 設定記憶（`toJson`/`fromJson` に jobName 追加）

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
- effect の部分適用 (Rect / Mask) の可視化

### M-CE-1 Composition Editor Cache System
- `Composition Viewer` の surface cache / render key / GPU blend fast path を整理
- ✅ ROI (Region of Interest) システム実装済み
- 詳細は `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`

### M-CE-2 Static Layer GPU Cache
- 静止レイヤーの GPU texture を長く使い回す cache 層
- ✅ ギズモ描画最適化 (Phase 2) 完了
- 詳細は `Artifact/docs/MILESTONE_STATIC_LAYER_GPU_CACHE_2026-03-26.md`

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
- **AE差別化:** コンポジションとアセットの明確な分離（混在しない構造）、仮想フォルダ vs 実フォルダの分離（実FS同期＋スマートコレクション）

### M-PV-2 Project View Asset Presentation
- thumbnail
- type icon
- size / duration / fps / missing 状態
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

### M-AS-3 Save / Load Integrity ✅ 完了
- 保存再読込で composition / layer / effect が落ちない
- ✅ Phase 1: 原子操作的保存（テンポラリファイル保存→バックアップ→リネーム）
- ✅ Phase 2: バージョン管理（minVersion互換性チェック、savedAtタイムスタンプ）
- ✅ Phase 3: 参照切れ検出（保存前のヘルスチェック統合、BrokenReference/MissingAsset警告）
- ✅ Phase 4: バリデーション強化（Composition/Layer整合性チェック、空プロジェクト警告）
- ✅ Phase 5: 増分保存（`saveIncremental()` — バージョン番号自動インクリメント）

### M-AS-4 Asset System Integration
- `AssetBrowser` と `Project View` の同期
- import / metadata / relink / missing / unused の統合
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

### M-AS-8 Composition Menu Workflow ✅ 完了
- composition create / preset / duplicate / rename / delete / settings の整理
- current composition sync / background color / development-only action cleanup
- 詳細は `Artifact/docs/MILESTONE_COMPOSITION_MENU_2026-03-13.md`
- ✅ 新規/プリセット/複製/名前変更/削除/背景色変更 実装済み
- ✅ enabled/disabled state 管理実装済み

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

### M-QA-4 Project File Validation / Spell Check ✅ 完了
- project / composition / layer / asset name の typo 検出
- tags / notes / ai metadata の表記ゆれ検査
- custom dictionary / ignore list
- `ArtifactProjectHealthDashboard` への統合
- ✅ Phase 1: 命名規則検査（空名前、プレースホルダー名、不正文字）実装済み
- ✅ Phase 2: 辞書ベースのスペルチェック（英単語辞書、typoパターン、トークン解析）実装済み
- ✅ Phase 2: 辞書ベースのスペルチェック（英単語辞書、typoパターン、トークン解析）実装済み

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

### M-CE Composition Editor & Layer View ✅ 完了
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_LAYER_VIEW.md`
- ✅ Phase 1: ビューポート変換の完成 (gizmo mode切替実装済み)
- ✅ Phase 2: ガイド＆オーバーレイ (ガイド線描画、GridRenderer、フレーム情報)
- ✅ Phase 3: レイヤービュー強化 (バウンディングボックス、effect範囲可視化)
- ⚠️ Phase 4: 3Dビューポート基盤 (一部実装済み)
- ⚠️ Phase 5: 品質＆マルチビュー (一部実装済み)
- **AE差別化:** アセットブラウザからD&Dでダイレクトにレイヤー追加（ゴーストつき）

### M-CE-SEL-1 Rubber Band Multi-Selection ✅ 完了
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md`
- ✅ composition editor 上の矩形選択
- ✅ 複数レイヤーの hit test / selection sync
- ✅ Shift / Ctrl を含む複数選択操作 (Replace/Toggle モード)
- ✅ timeline / inspector との current selection 一致

### M-CE-TEXT-1 Text Layer Inline Editing
- コンポジットエディタ上で text layer を直接編集する
- caret / selection / IME / commit / cancel を layer モデルに接続する
- 詳細は `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- 編集導線の最小入り口は実装済みで、Phase 2 以降の in-canvas input を残す
- `Ctrl+Enter` の commit shortcut を追加し、Phase 1 の確定導線を少し強化した
- 起動時に全文選択するようにして、置き換え入力の初動を軽くした

### M-CE-SEL-1 Rubber Band Multi-Selection ✅ 完了
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md`
- ✅ composition editor 上の矩形選択 (Replace/Toggle モード)
- ✅ 複数レイヤーの hit test / selection sync
- ✅ Shift / Ctrl を含む複数選択操作
- ✅ timeline / inspector との current selection 一致

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

## Good Small Tasks

- `M-AR-2 import std Rollout`
- `M-UI-2 Dock / Tab Polish`
- `M-QA-1 Software Test Windows`
- `M-FX-2 Solid Color Effects`
- `M-AS-3 Save / Load Integrity` ✅ 完了 ✅ 完了
- `M-FX-4 Creative Workflow (Bridge only)`
- ~~`M-UI-13 Keyboard Overlay`~~ ✅ 完了
- ~~`M-QA-4 Project File Validation / Spell Check`~~ ✅ Phase 1 完了
- ~~`M-SHAPE-1 Shape Layers`~~ ✅ Phase 1+3 完了
## Terminal / Shell

### M-UI-24 Terminal Shell / Command Surface
- debug console 縺ｨ蜷後§縺ｪ縺・power user 向けの command terminal surface 繧偵・ｧ譁ｭ縺吶ｋ
- `PowerShellWidget` 繧定ｦ九※縲∝・逕溘↓ command / history / working dir / exit code 繧定ｨｭ螳壹☆繧・
- 隧ｳ邏ｰ縺ｯ `docs/planned/MILESTONE_TERMINAL_SHELL_2026-04-06.md`
