# MILESTONE: RAM Preview Cache - Parity Phase 1 Execution

作成日: 2026-05-31
対象: [MILESTONE_RAM_PREVIEW_CACHE_PARITY_EXECUTION_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_EXECUTION_2026-05-31.md)

## Goal

`requested / ready / failed` の state contract を固定し、`cache hit` と `final image readiness` を混同しない状態まで進める。

## Why Phase 1 First

- ここが曖昧なままだと UI を直しても再発しやすい
- timeline / controller / playback が別の真実を読む原因がここに集まりやすい
- 新しい cache 機能を足す前に、既存の意味を揃える方が安全

## Target Files

- [ArtifactPlaybackService.cppm](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [ArtifactTimelineWidget.cpp](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp)

## Primary Functions

- [ArtifactPlaybackService::ramPreviewFrameState](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm:1858)
- [ArtifactPlaybackService::ramPreviewSummary](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm:1863)
- [ArtifactPlaybackService::tryGetRamPreviewFrameImage](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm:1876)
- [ArtifactPlaybackService::storeCompositionPreviewFrameImage](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm:1914)
- [ArtifactPlaybackService::invalidateRamPreviewCache](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm:1772)
- [CompositionRenderController::renderOneFrame](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm:5442)
- [ArtifactTimelineWidget::updateCacheVisuals](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp:2049)

## Task Slice

### 1. State Vocabulary Audit

- `requested`
- `ready`
- `failed`
- `pending build`
- `cache hit`
- `ready missing image`

やること:
- service / controller / timeline で同じ語が同じ意味か確認する
- 同じ語で違う意味があれば、まず service 側を正にする

### 2. Ready/Image Contract Split

やること:
- `ready` と `image fetch success` を分けて扱う
- `ready` でも image が無いケースを例外として明示する
- `cache hit` を単なる bitmap 存在ではなく、必要なら別語に逃がす

### 3. Timeline Surface Alignment

やること:
- tooltip / cache bar / current frame status の文言を揃える
- `pending` と `failed` が UI で潰れないようにする

## Expected Outcome

- state source は playback service に寄る
- controller は fallback を使っても state を捏造しない
- timeline は `ready` と `requested` を混同しない

## Not Yet

- disk cache の見直し
- preview prewarm policy の拡張
- GPU cache path の再設計

## Done For Phase 1

- state contract を 1 文で説明できる
- `ready` と `image available` の差をコードと UI の両方で追える
- timeline / controller / playback の表示語彙が揃う

