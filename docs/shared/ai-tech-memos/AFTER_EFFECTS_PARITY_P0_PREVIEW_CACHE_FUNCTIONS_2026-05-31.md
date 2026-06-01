# After Effects Parity P0 Preview Cache Functions

> 2026-05-31

## Purpose

`preview / cache / playback` の P0 を追うときに、まず飛ぶべき関数入口を固定するメモ。

## Primary Entry Points

### Playback Service

- `ArtifactPlaybackService::setCurrentComposition` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1462]`
  - composition 切り替え時の state reset を見る
- `ArtifactPlaybackService::play` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1162]`
  - playback がどこから始まるかを見る
- `ArtifactPlaybackService::stop` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1187]`
  - playback 停止時に preview state がどう扱われるかを見る
- `ArtifactPlaybackService::ramPreviewFrameState` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1858]`
  - `requested / ready / failed` の state source
- `ArtifactPlaybackService::ramPreviewSummary` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1863]`
  - preview cache の集計 source
- `ArtifactPlaybackService::tryGetRamPreviewFrameImage` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1876]`
  - ready のときに実画像が取れるかを見る
- `ArtifactPlaybackService::storeCompositionPreviewFrameImage` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1914]`
  - preview frame を cache に入れる経路
- `ArtifactPlaybackService::invalidateRamPreviewCache` `[/Artifact/src/Service/ArtifactPlaybackService.cppm:1772]`
  - cache invalidation の契機

### Composition Render Controller

- `CompositionRenderController::setViewportSize` `[/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:3163]`
  - viewport の基準と DPR の扱いを見る
- `CompositionRenderController::zoomFit` `[/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:4025]`
  - fit の基準を見る
- `CompositionRenderController::zoomFill` `[/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:4039]`
  - cover / crop の基準を見る
- `CompositionRenderController::zoom100` `[/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:4053]`
  - 100% の基準を見る
- `CompositionRenderController::renderOneFrame` `[/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:5442]`
  - render と preview update の接点を見る
- `CompositionRenderController::markRenderDirty` `[/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:5463]`
  - dirty / rerender の流れを見る

### Composition Editor

- `ArtifactCompositionEditor::resizeEvent` `[/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4392]`
  - resize と viewport 再計算のタイミングを見る
- `ArtifactCompositionEditor::setComposition` `[/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4425]`
  - composition 切り替え時の初期化を見る
- `ArtifactCompositionEditor::zoomFill` `[/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4508]`
  - UI から fill を呼ぶ経路を見る
- `ArtifactCompositionEditor::zoom100` `[/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4514]`
  - UI から 100% を呼ぶ経路を見る
- `ArtifactCompositionEditor::play` `[/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4462]`
  - UI 側から playback を始める経路を見る
- `ArtifactCompositionEditor::stop` `[/Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm:4471]`
  - UI 側から playback を止める経路を見る

### Timeline Widget

- `ArtifactTimelineWidget::updateCacheVisuals` `[/Artifact/src/Widgets/ArtifactTimelineWidget.cpp:2049]`
  - timeline の cache 表示の source
- `ArtifactTimelineWidget::syncPlayheadOverlay` `[/Artifact/src/Widgets/ArtifactTimelineWidget.cpp:4529]`
  - playback と playhead 表示の同期
- `ArtifactTimelineWidget::setComposition` `[/Artifact/src/Widgets/ArtifactTimelineWidget.cpp:3754]`
  - timeline 側の composition 切り替え
- `ArtifactTimelineWidget::updateKeyframeState` `[/Artifact/src/Widgets/ArtifactTimelineWidget.cpp:4305]`
  - current frame と keyframe 状態の接続
- `ArtifactTimelineWidget::handleTimelineAction` `[/Artifact/src/Widgets/ArtifactTimelineWidget.cpp:4199]`
  - playback / scrub / seek 操作の入口

## Reading Order

1. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_TARGET_FILES_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_TARGET_FILES_2026-05-31.md)
2. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md)
3. This file
4. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md)
5. [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md)

## Notes

- ここでは修正方針を決めない
- まずは `state source` と `UI source` を分けて追う
- `Fill / 100%` は viewport contract 側の論点として扱う
