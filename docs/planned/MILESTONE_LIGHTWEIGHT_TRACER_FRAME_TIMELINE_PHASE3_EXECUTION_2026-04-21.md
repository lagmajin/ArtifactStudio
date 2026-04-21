# Phase 3 実行メモ: Frame Timeline Visualization - File Tickets

> 2026-04-21 作成

## 目的

`MILESTONE_LIGHTWEIGHT_TRACER_FRAME_TIMELINE_PHASE3_2026-04-21.md` を、実装にそのまま切れる作業票へ落とす。

## チケット 1: `ProfilerPanelWidget`

対象:
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/ProfilerPanelWidget.ixx`

やること:
- frame timeline の入口を作る
- `Render / Decode / UI / Event` のレーン表示を置く

完了条件:
- timeline が profiler panel から開ける

## チケット 2: `Frame Timeline View`

対象候補:
- `Artifact/src/Widgets/Diagnostics/FrameTimelineViewWidget.cppm`
- `Artifact/src/Widgets/Diagnostics/FrameTimelineViewWidget.ixx`

やること:
- 1 frame の scope を帯で見せる
- 重いレーンが一目で分かるようにする

完了条件:
- frame ごとの時間配分が読める

