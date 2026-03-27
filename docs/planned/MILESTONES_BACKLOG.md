# Milestones Backlog

空いている時間に進めやすいよう、分野別に小さめのマイルストーンへ分割したバックログ。

## Application

### M-APP-1 Application Cross-Cutting Improvement
- menu / toolbar / shortcut / view / diagnostics / workflow を横断で揃える
- 詳細は `docs/planned/MILESTONE_APP_CROSS_CUTTING_IMPROVEMENT_2026-03-27.md`

### M-APP-2 Deferred UI Initialization / Lazy Load
- icon / thumbnail / viewer / dock の eager load を減らして初回体感を軽くする
- 詳細は `docs/planned/MILESTONE_DEFERRED_UI_INITIALIZATION_2026-03-27.md`

## Feature Expansion Support

### M-FE-1 Onboarding / Empty States
- empty project / empty selection / empty asset / empty timeline の案内
- 詳細は `docs/planned/MILESTONE_ONBOARDING_EMPTY_STATES_2026-03-27.md`

### M-FE-2 Export / Review / Share
- export 後の quick review / reveal / share を短くする
- 詳細は `docs/planned/MILESTONE_EXPORT_REVIEW_SHARE_2026-03-27.md`

### M-FE-3 Automation Helpers
- command palette / batch / preset / macro entry を増やす
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

### M-FE-7 Review / Compare / Annotation
- compare view / side-by-side review / note / bookmark
- 詳細は `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`

### M-FE-8 Search / Collections / Smart Organization
- global search / smart bin / tag / dependency / missing / duplicate detection
- 詳細は `docs/planned/MILESTONE_SEARCH_COLLECTIONS_SMART_ORGANIZATION_2026-03-28.md`

### M-FE-9 Motion Tracking Workflow
- tracker editor / overlay / stabilize / bake を制作導線としてまとめる
- 詳細は `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`

## UI / UX

### M-UI-1 Timeline Finish
- playhead、不感帯、余白、行揃え、ホイール、ドラッグ挙動の最終整理

### M-UI-2 Dock / Tab Polish
- アクティブタブ装飾
- スプリッター幅
- 空パネルや初期レイアウトの見直し

### M-UI-3 Inspector Usability
- effect / property の見つけやすさ
- 空状態の整理
- 選択同期とラベル整理

### M-UI-4 Menu-to-App Command Routing
- File / Composition / Edit / View / Layer / Render / Help の menu を app service / command に正しく接続する
- 詳細は `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`

### M-UI-5 Contents Viewer Expansion
- image / video / 3D model / source / final / compare を横断する viewer の拡充
- 詳細は `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- 追加の review / compare / annotation 方向は `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`

## Timeline / Layer

### M-TL-1 Layer Basic Operations ✅ 完了
- 追加、削除、複製、rename、親子、並び替え
- 残差分: Undo スタブ実装 (M-AS-7 側), track matte mode (未定義)

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

### M-TL-5 Timeline Keyframe Editing
- Timeline 上で property keyframe を見て、打って、移動できるようにする
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

### M-RD-2 Render Queue Hardening
→ 詳細: [MILESTONE_RENDER_QUEUE_2026-03-22.md](MILESTONE_RENDER_QUEUE_2026-03-22.md)
- job 編集
- 範囲指定
- 失敗理由表示
- 履歴と再実行

### M-RD-3 Dual Backend Parity
- software と Diligent の見た目差分を減らす

### M-RD-4 Render / Output Feel Refinement
- 途中失敗からの再開
- frame / layer / effect cost の可視化
- rename / history / visibility inspector
- 詳細は `docs/planned/MILESTONE_RENDER_OUTPUT_FEEL_REFINEMENT_2026-03-27.md`

### M-RD-5 Animated Image Export
- GIF / APNG / Animated WebP などの web 向け animated image 出力
- 詳細は `docs/planned/MILESTONE_ANIMATED_IMAGE_EXPORT_2026-03-27.md`

### M-LV-1 Layer Solo View (Diligent)
- 詳細は `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md`
- current composition / current layer の追従
- solo 表示の安定化
- mask / roto 入口の整理
- software test widget との見え方差分縮小

### M-CE-1 Composition Editor Cache System
- `Composition Viewer` の surface cache / render key / GPU blend fast path を整理
- 詳細は `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_CACHE_SYSTEM_2026-03-26.md`

### M-CE-2 Static Layer GPU Cache
- 静止レイヤーの GPU texture を長く使い回す cache 層
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

## Audio

### M-AU-1 Composition Audio Mixer
- mute / solo / volume / layer 同期

### M-AU-2 Playback Sync
- 再生位置と音の同期

### M-AU-3 Audio Visualization
- waveform
- meter
- 簡易可視化

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

### M-PV-2 Project View Asset Presentation
- thumbnail
- type icon
- size / duration / fps / missing 状態

### M-PV-3 Project View Organization
- folder / bin 整理
- expand / collapse
- unused / tag / virtual view

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

### M-AS-3 Save / Load Integrity
- 保存再読込で composition / layer / effect が落ちない

### M-AS-4 Asset System Integration
- `AssetBrowser` と `Project View` の同期
- import / metadata / relink / missing / unused の統合
- 詳細は `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`

### M-AS-9 Project / Asset Workflow Bridge
- Project View / Asset Browser / Contents Viewer / Render Queue を一続きにする
- import / relink / recent / favorite / missing / dependency の導線整理
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

### M-AS-8 Composition Menu Workflow
- composition create / preset / duplicate / rename / delete / settings の整理
- current composition sync / background color / development-only action cleanup
- 詳細は `Artifact/docs/MILESTONE_COMPOSITION_MENU_2026-03-13.md`

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

### M-QA-4 Project File Validation / Spell Check
- project / composition / layer / asset name の typo 検出
- tags / notes / ai metadata の表記ゆれ検査
- custom dictionary / ignore list
- `ArtifactProjectHealthDashboard` への統合

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

## Matte

### M-LYR-1 Matte Stack / Child Matte Nodes
- matte を layer の child / attached node として扱う
- Add / Common / Subtract の複数 matte 合成
- Alpha / Luminance / Inverted の評価
- dependency order / cycle check / diagnostics
- 詳細は `ArtifactCore/docs/MILESTONE_TRACK_MATTE_CORE_2026-03-26.md`

---

## App Layer Completeness

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

### M-CE Composition Editor & Layer View
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_LAYER_VIEW.md`
- Phase 1: ビューポート変換の完成 (回転ハンドル、アンカー、解像度/プレビュー接続)
- Phase 2: ガイド & オーバーレイ (ガイド線、GridRenderer、ルーラー、フレーム情報)
- Phase 3: レイヤービュー強化 (バウンディングボックス、edit/display mode、情報表示)
- Phase 4: 3D ビューポート基盤 (3D カメラ、3D ギズモ、Z-depth)
- Phase 5: 品質 & マルチビュー (品質設定、スプリットビュー、背景オプション)

### M-CE-TEXT-1 Text Layer Inline Editing
- コンポジットエディタ上で text layer を直接編集する
- caret / selection / IME / commit / cancel を layer モデルに接続する
- 詳細は `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- 編集導線の最小入り口は実装済みで、Phase 2 以降の in-canvas input を残す
- `Ctrl+Enter` の commit shortcut を追加し、Phase 1 の確定導線を少し強化した
- 起動時に全文選択するようにして、置き換え入力の初動を軽くした

### M-CE-SEL-1 Rubber Band Multi-Selection
- 詳細は `docs/planned/MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md`
- composition editor 上の矩形選択
- 複数レイヤーの hit test / selection sync
- Shift / Ctrl を含む複数選択操作
- timeline / inspector との current selection 一致

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

## Good Small Tasks

- `M-AR-2 import std Rollout`
- `M-UI-2 Dock / Tab Polish`
- `M-QA-1 Software Test Windows`
- `M-FX-2 Solid Color Effects`
- `M-AS-3 Save / Load Integrity`
- `M-FX-4 Creative Workflow (Bridge only)`
