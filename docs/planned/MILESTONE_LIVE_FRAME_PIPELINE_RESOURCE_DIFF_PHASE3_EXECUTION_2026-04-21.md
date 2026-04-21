# Phase 3 実行メモ: State Diff Tracker - File Tickets

# 2026-04-21 作成

## 目的

`MILESTONE_LIVE_FRAME_PIPELINE_RESOURCE_DIFF_PHASE3_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: diff model

対象候補:
- `ArtifactCore/include/Render/FrameStateDiff.ixx`
- `ArtifactCore/include/Render/FrameStateDiffChange.ixx`

やること:
- before / after / severity を持つ diff を定義する
- PSO / CB / SRV / UAV の変化を分類する

完了条件:
- diff が読める

## チケット 2: diff bridge

対象:
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`

やること:
- 前フレームとの差分を作る
- 壊れ始めたフレームを判定する

完了条件:
- 差分で壊れ始めた瞬間が追える

