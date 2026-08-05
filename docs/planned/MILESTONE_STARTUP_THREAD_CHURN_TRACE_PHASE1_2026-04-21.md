> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md](MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md)

# M-DIAG-5 Phase 1

## Trace Tagging

- startup / initial composition open / first preview の worker 起動を trace domain で分類する
- `Render / Decode / Asset / Playback / Project / AI / Import / Export` の lane を持たせる
- `TraceRecorder` に startup burst 用の lightweight tagging を追加する

## Target

- `ArtifactCore/include/Diagnostics/Trace.ixx`
- `Artifact/src/Widgets/Diagnostics/TraceTimelineWidget.cppm`
- `Artifact/src/AppMain.cppm`

## Done When

- thread 開始/終了イベントが domain 別に読める
- startup burst を thread lane 上で視認できる
