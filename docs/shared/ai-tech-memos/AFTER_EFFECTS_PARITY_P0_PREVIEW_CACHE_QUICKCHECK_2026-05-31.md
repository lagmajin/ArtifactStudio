# After Effects Parity P0 Preview Cache Quickcheck

> 2026-05-31

## Purpose

preview / cache / playback を最短で確認するための 3 ステップ版。

## Quick Steps

1. `requested / ready / failed` の状態遷移を追う
2. cache hit と final image readiness を切り分ける
3. playback / scrub / diagnostics の truth が一致しているか見る

## What To Look For

- UI が都合よく状態を省略していない
- renderer が ready でないのに ready 扱いされていない
- cache の存在が描画完了と混同されていない
- fallback の理由が追跡可能

## If Broken

- state contract を先に直す
- 表示上の見え方で隠さない
- ログだけでなく責務境界を見直す

## Reference

- 詳細: [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md)
- 確認表: [AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md)
- 起点: [AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md)
- 実行順: [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)

