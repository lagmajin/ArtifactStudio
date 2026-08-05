> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md](MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md)

# M-DIAG-5 Phase 3

## Pool Consolidation

- `QtConcurrent::run(...)` の単発起動と専用 worker の乱立を整理する
- shared pool へ寄せるものと、専用 thread で残すものを決める

## Suspects

- `ArtifactVideoLayer`
- `ArtifactImageLayer`
- `ArtifactSvgLayer`
- `ArtifactRenderScheduler`
- `ArtifactPlaybackEngine`

## Done When

- startup burst の発生源が shared / dedicated で整理される
- 単発 worker の乱立が減る
