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
