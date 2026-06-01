# After Effects Parity P0 Preview Cache

> 2026-05-31

## Purpose

P0 のうち `preview / cache / playback` だけを最短で確認するための超短い着手メモ。

## Focus

- RAM preview の state contract
- cache hit と final image readiness の分離
- playback / scrub / diagnostics の整合
- fallback policy の見え方

## Check

1. `requested / ready / failed` が 1 本の状態遷移になっているか
2. cache hit が画像完成と同義になっていないか
3. playback と scrub が別の truth を読んでいないか
4. UI が fallback を隠していないか

## Success Criteria

- preview の状態が renderer と UI で一致する
- cache の命名と実体がずれない
- 失敗時の理由が追える
- 最小限のログで再現可能になる

## Reference

- P0 確認表: [AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md)
- P0 起点: [AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md)
- 実行順: [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)
- 総括: [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)

