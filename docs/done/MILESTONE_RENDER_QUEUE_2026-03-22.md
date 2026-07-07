# Render Queue

**Date**: 2026-03-22
**Status**: Completed
**Parent**: `ArtifactRenderQueueService`

`ArtifactRenderQueueService` に background rendering、失敗理由の追跡、progress 更新、checkpoint / resume、farm / external renderer の実行導線が揃っているため、この milestone を完了扱いにした。

## Evidence

- `Artifact/src/Render/ArtifactRenderQueueService.cppm`

## Result

- rendering progress が queue manager と同期する
- failure reason が job 単位で残る
- checkpoint / resume 用の farm path がある
- external renderer と integrated render の両方を扱える

