> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md](MILESTONE_STARTUP_THREAD_CHURN_TRACE_2026-04-21.md)

# M-DIAG-5 Phase 4

## Initialization Deferral

- 初回表示に不要な background work を遅延する
- `first paint` / `first interaction` を優先する

## Focus

- media prefetch
- lazy pipeline init
- non-critical diagnostics warmup
- background decode kick

## Done When

- 起動直後の thread churn が減る
- first composition open 時の burst が trace 上で明確に減る
