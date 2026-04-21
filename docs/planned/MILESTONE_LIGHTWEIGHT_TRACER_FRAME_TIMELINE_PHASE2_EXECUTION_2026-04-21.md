# Phase 2 実行メモ: Scope Tracer Core - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE2_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: `Trace.Scope`

対象候補:
- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `ArtifactCore/include/Diagnostics/TraceScope.ixx`
- `ArtifactCore/include/Utils/PerformanceProfiler.ixx`

やること:
- `begin/end` の軽量 scope を記録する
- `Render / Decode / UI / Event` のラベルを共通化する
- thread-local に近い低オーバーヘッド構造にする

完了条件:
- scope 名が記録できる
- フレーム境界で集計できる

## チケット 2: 既存 hot path への hook

対象候補:
- `Artifact/src/Render/*`
- `Artifact/src/Playback/*`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

やること:
- render / playback の主要 scope を付ける
- 既存の profiling と重複しない最小記録にする

完了条件:
- `Render / Decode / UI / Event` が同じ model で扱える

