# マイルストーン: 機能追加 / Feature Expansion

> 2026-03-25 作成

## 目的

既存機能を速く・気持ちよくするマイルストーンとは別に、制作能力そのものを増やす。

この文書は「新しいことができるようになる」側を扱う。

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
- error reason の明確化

### 連携先

- `RenderQueueManagerWidget`
- `ArtifactPlaybackEngine`
- `ArtifactVideoLayer`
- `FFmpeg` 系

---

## 優先順位

### 最優先

1. Motion Tracking
2. Audio Production
3. Camera / Transform Workflow

### 高

1. Effect Stack Expansion
2. Timeline Workflows
3. Project / Asset Workflow

### 中

1. Export / Render
2. Advanced stabilization
3. Automation helpers

---

## 関連文書

- `docs/planned/MILESTONE_OPERATION_FEEL_REFINEMENT_2026-03-25.md`
- `docs/planned/MILESTONE_MOTION_TRACKING_SYSTEM_2026-03-25.md`
- `docs/planned/MILESTONE_UNDO_AND_AUDIO_PIPELINE_COMPLETION_2026-03-25.md`
- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md`
- `docs/planned/MILESTONE_APP_LAYER_COMPLETENESS.md`
- `docs/planned/MILESTONES_BACKLOG.md`

---

## Next Step

最初に着手するなら次の順番が妥当。

1. Motion Tracking の UI 導線
2. Audio の再生/ミキシング拡張
3. Camera / Transform workflow の整備

