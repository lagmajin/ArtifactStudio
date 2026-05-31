# MILESTONE: RAM Preview Cache - Parity Execution Slice

作成日: 2026-05-31
対象: `M-RP-1 RAM Preview Cache`
参照:
- [MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md)
- [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
- [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md)

## Purpose

既存の `RAM Preview Cache` マイルストーンを、AE parity の P0 観点でそのまま着手できる execution slice に落とし直す。

ここで重要なのは、cache を増やすことではなく、`requested / ready / failed` の状態契約と、UI / playback / diagnostics の真実を揃えること。

## Why This Is Ready Now

- `ArtifactPlaybackService` に RAM preview の状態 source が既にある
- `ArtifactCompositionRenderController` に fallback / preview image store の導線が既にある
- `ArtifactTimelineWidget` に cache 表示の導線が既にある
- 調査入口が [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md](X:/Dev/ArtifactStudio/docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md) で固定済み

## Recommended Start Order

### 1. State Contract Fix

- `requested / ready / failed` の意味を service 側で固定する
- `cache hit` と `final image readiness` を同義にしない
- `playback / scrub / diagnostics` が同じ state source を使うようにする

主に見る場所:
- [ArtifactPlaybackService.cppm](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

### 2. Preview Fallback Policy Clarification

- 再生中 fallback を許す条件を明文化する
- `ready なのに画像が無い` 状態を例外扱いとして追えるようにする
- `not ready` と `fallback used` を UI で区別できるようにする

主に見る場所:
- [ArtifactPlaybackService.cppm](X:/Dev/ArtifactStudio/Artifact/src/Service/ArtifactPlaybackService.cppm)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)
- [ArtifactTimelineWidget.cpp](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp)

### 3. Diagnostics Surface Alignment

- timeline tooltip / frame debug / controller snapshot の語彙を揃える
- `cache hit`, `pending build`, `failed`, `range ready` を同じ意味で見せる

主に見る場所:
- [ArtifactTimelineWidget.cpp](X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactTimelineWidget.cpp)
- [ArtifactCompositionRenderController.cppm](X:/Dev/ArtifactStudio/Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm)

## In Scope

- RAM preview frame state contract
- fallback policy
- preview image store / fetch の一貫性
- timeline / controller / diagnostics の語彙統一

## Out of Scope

- 新しい disk cache 設計
- GPU cache の全面再設計
- video decoder 最適化単独

## Success Criteria

- `requested / ready / failed` の意味を 1 つに説明できる
- `ready` なのに image missing という破綻を追跡できる
- timeline / controller / playback の表示が同じ真実を指す
- AE parity 的に「RAM preview が壊れて見える」状況が減る

## Execution Note

- Phase 1 実行メモ: [MILESTONE_RAM_PREVIEW_CACHE_PARITY_PHASE1_EXECUTION_2026-05-31.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_RAM_PREVIEW_CACHE_PARITY_PHASE1_EXECUTION_2026-05-31.md)
