# Phase 1 実行メモ: Frame Graph / Pipeline View - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE1_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: `FramePipelineGraph`

対象候補:
- `ArtifactCore/include/Render/FramePipelineGraph.ixx`
- `ArtifactCore/include/Render/FrameResourceLifetime.ixx`
- `ArtifactCore/include/Render/FrameHazardRecord.ixx`

やること:
- pass node と edge を定義する
- resource lifetime band を保持する
- hazard を簡易記録する

完了条件:
- frame graph の型が読める

## チケット 2: render summary bridge

対象:
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`

やること:
- render path から pass / attachment / readback の要約を組み立てる
- ROI / partial eval / backend summary を拾う

完了条件:
- DAG 要約を組み立てられる

