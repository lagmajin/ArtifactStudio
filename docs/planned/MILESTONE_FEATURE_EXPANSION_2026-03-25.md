# マイルストーン: 機能追加 / Feature Expansion

> 2026-03-25 作成

## 目的

既存機能を速く・気持ちよくするマイルストーンとは別に、制作能力そのものを増やす。

この文書は「新しいことができるようになる」側を扱う。

機能拡充は、単発の新機能だけでなく、アプリ全体の command surface を増やすことも含む。
そのため、メニューやショートカットの整理は機能拡充の前段として扱う。

---

## 方針

### 原則

1. 既存の保存形式と壊れないように足す
2. Core に機能を置き、UI は導線に徹する
3. 単発の便利機能より、制作ワークフローを増やす
4. 将来の自動化や拡張に再利用できる形で実装する

### 重点領域

- Motion Tracking
- Audio
- Camera / Transform workflow
- Effect Stack
- Timeline / Keyframe workflow
- Project / Asset workflow
- Export / Render workflow
- Command Surface / Menu routing
- Workspace / Layout / Session
- Templates / Presets / Starter Kits
- Batch / Macro / Script entry
- Review / Compare / Annotation
- Search / Collections / Smart organization

### 既存の詳細ワークストリーム

- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_AUDIO_LAYER_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`
- `docs/planned/MILESTONE_COLOR_CORRECTION_2026-03-27.md`
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_CONTENTS_VIEWER_EXPANSION_2026-03-27.md`
- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`
- `docs/planned/MILESTONE_SEARCH_COLLECTIONS_SMART_ORGANIZATION_2026-03-28.md`
- `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`
- `docs/planned/MILESTONE_ONBOARDING_EMPTY_STATES_2026-03-27.md`
- `docs/planned/MILESTONE_EXPORT_REVIEW_SHARE_2026-03-27.md`
- `docs/planned/MILESTONE_AUTOMATION_HELPERS_2026-03-27.md`
- `docs/planned/MILESTONE_DEFERRED_UI_INITIALIZATION_2026-03-27.md`

---

## Phase 0: Command Surface / Menu Routing

### 目的

各メニューを単なる UI ではなく、機能追加を受け止める command surface として整える。

### 機能

- File / Composition / Edit / View / Layer / Render / Help の command routing 整備
- shortcut と menu の実行経路統一
- enabled / disabled / checked state の共通化
- 将来の feature expansion を受けるための action ownership 固定

### 連携先

- `Artifact/src/Widgets/Menu/*`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Service/*`
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`

---

## Phase 1: Motion Tracking

### 目的

映像内の対象を追跡し、レイヤー変形・マスク・カメラ補助・安定化へ使えるようにする。

### 機能

- 追跡点 / 追跡領域の作成
- forward / backward / range tracking
- 追跡結果の確認
- motion path 表示
- tracked transform の bake
- stabilize 生成

### 連携先

- `ArtifactCore::MotionTracker`
- `AbstractProperty`
- `AnimatableTransform3D`
- `ArtifactCameraLayer`
- `ArtifactAbstractLayer`

---

## Phase 2: Audio Production

### 目的

音声レイヤーを実用的にし、最低限の編集・再生・ミキシングができるようにする。

### 機能

- Audio layer の再生安定化
- mute / solo / volume / pan
- waveform 表示
- audio track / mixer 導線
- 再生位置同期
- basic crossfade / clip handling

### 連携先

- `ArtifactAudioLayer`
- `ArtifactPlaybackEngine`
- `AudioRenderer`
- `ArtifactCompositionAudioMixerWidget`

---

## Phase 3: Camera / Transform Workflow

### 目的

カメラとレイヤー変形を、編集対象として扱いやすくする。

### 機能

- Camera layer 追加
- camera move / zoom / focus
- `F` で選択レイヤーへフォーカス
- transform handle の編集改善
- track-driven transform bake

### 連携先

- `ArtifactCameraLayer`
- `ArtifactCompositionEditor`
- `TransformGizmo`
- `AbstractProperty`

---

## Phase 4: Effect Stack Expansion

### 目的

エフェクトスタックを「あるだけ」から「実用的に組める」状態へ進める。

### 機能

- effect add / remove / reorder
- effect enable / disable
- effect presets
- parameter groups の整理
- color / curve / blur / distort 系の拡張
- layer / composition 両対応の整理

### 連携先

- `ArtifactInspectorWidget`
- `ArtifactPropertyWidget`
- `ArtifactEffectService`
- `AbstractGeneratorEffector`

---

## Phase 5: Timeline Workflows

### 目的

タイムラインを、レイヤーを置くだけの場所から編集中心の場所へ進める。

### 機能

- keyframe lane の可視化
- track select / isolate / lock
- trim / split / ripple 系の基本操作
- range preset
- track matte 表示
- nested composition 表示

### 連携先

- `ArtifactTimelineWidget`
- `ArtifactLayerPanelWidget`
- `ArtifactPropertyWidget`
- `ArtifactPlaybackControlWidget`

---

## Phase 6: Project / Asset Workflow

### 目的

素材管理とプロジェクト構成を強化し、制作の入口を広げる。

### 機能

- Asset Browser / Project View の同期強化
- import / relink / missing / unused 管理
- thumbnail / metadata / duration / fps 表示
- folder / bin / favorite / recent
- browser から timeline への高速投入

### 連携先

- `ArtifactAssetBrowser`
- `ArtifactProjectManagerWidget`
- `ArtifactProjectModel`
- `AssetDatabase`
- 詳細ワークストリーム: `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md`

---

## Phase 7: Export / Render

### 目的

制作物を書き出すところまでを機能として完成させる。

### 機能

- render queue
- batch render
- preset export
- proxy generation
- sequence / video / audio export
- animated image export (GIF / APNG / Animated WebP)
- error reason の明確化

### 連携先

- `RenderQueueManagerWidget`
- `ArtifactPlaybackEngine`
- `ArtifactVideoLayer`
- `FFmpeg` 系

---

## Phase 8: Onboarding / Empty States

### 目的

初回起動や空プロジェクト時に、次の操作を見つけやすくする。

### 機能

- empty project / empty selection / empty asset / empty timeline の案内
- first action hint
- recent / import / create の入口

### 連携先

- `ArtifactMainWindow`
- `ArtifactProjectManagerWidget`
- `ArtifactContentsViewer`
- `ArtifactTimelineWidget`

---

## Phase 9: Export Review / Share

### 目的

書き出し後の確認・共有導線を短くする。

### 機能

- open output folder
- quick review
- copy path / reveal / share
- export result の軽量 viewer 連携

### 連携先

- `RenderQueueManagerWidget`
- `ArtifactContentsViewer`
- `ArtifactProjectManagerWidget`

---

## Phase 10: Automation Helpers

### 目的

繰り返し作業を定型化して、制作の手数を減らす。

### 機能

- command palette / quick action
- batch rename / relink / export
- preset save / recall
- macro / script hook entry

### 連携先

- `ArtifactMainWindow`
- `ArtifactProjectManagerWidget`
- `ArtifactRenderQueueManagerWidget`
- `ArtifactContentsViewer`

---

## Phase 11: Deferred UI Initialization

### 目的

起動直後の初期化コストを下げ、最初の表示と初回操作を軽くする。

### 機能

- icon warmup の分割
- thumbnail / preview の遅延生成
- heavy widget の first show load
- playback / render の bootstrap 遅延

### 連携先

- `ArtifactContentsViewer`
- `ArtifactPropertyWidget`
- `ArtifactAssetBrowser`
- `ArtifactMainWindow`

---

## Phase 12: Workspace / Layout / Session

### 目的

アプリ全体の作業環境を、プロジェクト単位や用途単位で切り替えやすくする。

### 機能

- workspace 保存 / 読み込み
- dock layout preset
- window state / panel state の復元
- recent workspace
- last used workspace の自動復元

### 連携先

- `ArtifactMainWindow`
- `ArtifactDockManager`
- `ArtifactProjectService`
- `ArtifactSettings`

---

## Phase 13: Templates / Presets / Starter Kits

### 目的

新規作成や定型作業の開始コストを下げる。

### 機能

- project template
- composition preset
- layer preset
- effect preset
- startup kit / starter project

### 連携先

- `ArtifactProjectManagerWidget`
- `ArtifactCompositionEditor`
- `ArtifactInspectorWidget`
- `ArtifactEffectService`

---

## Phase 14: Batch / Macro / Script Entry

### 目的

繰り返し作業をまとめて実行できるようにする。

### 機能

- batch rename / relink / export 拡張
- command palette
- macro entry
- script hook entry
- preset-driven batch jobs

### 連携先

- `ArtifactMainWindow`
- `RenderQueueManagerWidget`
- `ArtifactProjectManagerWidget`
- `ArtifactToolService`

---

## Phase 15: Review / Compare / Annotation

### 目的

制作途中の確認とフィードバックを、アプリ内で完結しやすくする。

### 機能

- compare view
- side-by-side review
- annotation / note
- snapshot bookmark
- export result quick review

### 連携先

- `ArtifactContentsViewer`
- `ArtifactCompositionEditor`
- `RenderQueueManagerWidget`
- `ArtifactProjectManagerWidget`

---

## Phase 16: Search / Collections / Smart Organization

### 目的

素材・レイヤー・エフェクト・プロジェクトを横断して、探しやすくする。

### 機能

- global search
- collection / smart bin
- tag / favorites / recent
- reference / dependency view
- unused / missing / duplicate detection

### 連携先

- `ArtifactAssetBrowser`
- `ArtifactProjectManagerWidget`
- `ArtifactInspectorWidget`
- `ArtifactProjectService`

---

## 優先順位

### 最優先

1. Command Surface / Menu Routing
2. Motion Tracking
3. Audio Production

### 高

1. Camera / Transform Workflow
2. Effect Stack Expansion
3. Timeline Workflows
4. Workspace / Layout / Session
5. Templates / Presets / Starter Kits

### 中

1. Project / Asset Workflow
2. Export / Render
3. Advanced stabilization
4. Automation helpers
5. Onboarding / Empty States
6. Export / Review / Share
7. Deferred UI Initialization
8. Batch / Macro / Script entry
9. Review / Compare / Annotation
10. Search / Collections / Smart organization

---

## 関連文書

- `docs/planned/MILESTONE_OPERATION_FEEL_REFINEMENT_2026-03-25.md`
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md`
- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md`
- `docs/planned/MILESTONE_APP_LAYER_COMPLETENESS.md`
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONES_BACKLOG.md`

---

## Next Step

最初に着手するなら次の順番が妥当。

1. Command Surface / Menu Routing
2. Motion Tracking の UI 導線
3. Audio Layer の見え方と編集導線
4. Workspace / Layout / Session の保存復元

