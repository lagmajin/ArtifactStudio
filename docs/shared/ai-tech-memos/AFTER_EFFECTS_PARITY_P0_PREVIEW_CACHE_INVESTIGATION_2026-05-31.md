# After Effects Parity P0 Preview Cache Investigation

> 2026-05-31

## Purpose

`preview / cache / playback` の P0 を、コードを見ながら確認するための 1 ページ手順。

## Investigation Order

1. state contract を探す
2. cache readiness の境界を探す
3. playback / scrub の truth source を探す
4. fallback 表示と renderer の関係を見る

## Where To Look

- preview state を持つ service / controller
- cache と final image readiness を分けている箇所
- playback / scrub / diagnostics の入口
- UI に status を返す layer
- `requested / ready / failed` を定義している型や enum

## Questions To Answer

- どの層が `ready` を宣言するのか
- cache hit は何を保証するのか
- playback は何を基準に進むのか
- UI はどの状態を省略しているのか

## Expected Findings

- state contract の責務が 1 本になる
- renderer と UI が違う真実を読まない
- fallback の理由が説明可能になる
- `cache` と `render complete` の意味が分離される

## If You Find A Bug

- まず責務境界のズレを記録する
- 次に state contract を固定する
- それから表示や文言を調整する

## Reference

- 3 ステップ最短版: [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md)
- 詳細版: [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md)
- P0 確認表: [AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md)
- 実行順: [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)

