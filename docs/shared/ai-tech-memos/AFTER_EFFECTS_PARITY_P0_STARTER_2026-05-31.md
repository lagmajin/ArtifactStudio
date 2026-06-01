# After Effects Parity P0 Starter

> 2026-05-31

## Purpose

After Effects parity のうち、まず壊れてはいけない P0 領域だけを切り出した着手メモ。

## Scope

- preview / cache / playback stability
- track matte / alpha compositing correctness
- blend mode coverage
- viewport contract for `Fill` / `100%`

## Start Here

1. `preview / cache / playback` が同じ state contract を見ているか確認する
2. `requested / ready / failed` の状態遷移が曖昧になっていないか確認する
3. track matte / alpha / blend の評価順を確認する
4. `Fill` / `100%` が `fit / cover` と `logical / physical` を混同していないか確認する

## Expected Outcomes

- RAM preview の状態が UI と renderer で食い違わない
- compositing の評価結果が予測可能になる
- `Fill` と `100%` の見え方が仕様として説明できる
- 破綻時の reason を追えるようになる

## Suggested File Order

1. [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)
2. [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)
3. [AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md](AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md)
4. [AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)

## Notes

- これは P0 の起点だけを切り出したメモで、P1 以降を否定しない
- 実装の判断は [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md) を基準にする
- `Fill` / `100%` の問題は AE parity ではなく UI contract の問題として扱う

