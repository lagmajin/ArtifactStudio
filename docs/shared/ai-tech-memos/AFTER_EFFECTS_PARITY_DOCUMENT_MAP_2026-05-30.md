# After Effects Parity Document Map

> 2026-05-30

## Purpose

After Effects parity 関連のメモ群を、次の AI が迷わず辿れるように整理した索引メモ。

## Canonical Documents

- [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md)
  - 関数入口を固定するメモ
  - 調査開始時に最初に飛ぶ場所
- [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_TARGET_FILES_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_TARGET_FILES_2026-05-31.md)
  - 対象ファイルを固定するメモ
  - 調査開始時の実ファイル候補
- [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md)
  - preview / cache をコードで追う 1 ページ手順
- [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md)
  - preview / cache を 3 ステップで見る最短版
- [AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md)
  - P0 のうち preview / cache / playback だけを見る短いメモ
- [AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md)
  - P0 の確認表
  - 実際にチェックする項目を短く並べたもの
- [AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md)
  - P0 の着手点
  - preview / compositing / viewport contract に絞る
- [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)
  - 実務で何から潰すかの順序
  - 総括の次に読む実行用メモ
- [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)
  - まず読む 1 枚の総括
  - 読む順番を固定するための最上位入口
- [AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md](X:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md)
  - 原本メモ
  - 現行コード基準での不足感を短くまとめたもの
- [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
  - planned 側の再整理
  - 優先度と実装順の基準
- [AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)
  - 外部AI の調査要約との比較メモ
  - P0〜P3 の読み替えを残す
- [AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md](AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md)
  - P0〜P3 をざっと確認するためのチェック表
- [AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md](AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md)
  - 次の AI に渡す再開点メモ
- [AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md](AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md)
  - まだ答えが揃っていない論点だけを切り出したもの

## Recommended Reading Paths

### 1. ざっと全体像を掴む

1. `AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md`
2. `AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md`
3. `AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_TARGET_FILES_2026-05-31.md`
4. `AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md`
5. `AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md`
6. `AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md`
7. `AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md`
8. `AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md`
9. `AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md`
10. `README.md`
11. `INDEX.md`
12. `AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md`
13. `AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md`

### 2. 比較メモを読む

1. `AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`
2. `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`
3. `AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md`

### 3. 未解決の論点を追う

1. `AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md`
2. `BUG_COMPOSITION_EDITOR_ZOOM_FILL_100_PERCENT_MISPOSITION_2026-05-30.md`

### 4. 実装へ落とす

1. `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`
2. 関連する phase / execution memo
3. 対象 widget / core file

## Current Synthesis

- `feature missing` より `workflow not cohesive` の比重が高い
- P0 は preview / cache / playback stability と compositing correctness
- P1 は graph editor / text animator UX / motion blur / parent propagation
- P2 は precompose / markers / layer styles / expression completeness
- P3 は ecosystem / interop / templates / API coverage

## Notes

- `Fill` と `100%` の挙動は AE parity とは別の UI contract 問題として扱う
- 数字の未実装率は参考値として読む
- 実装優先度は [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](/x:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md) を基準にする
- 迷ったら master summary に戻る
- 実行順に迷ったら roadmap に戻る
- P0 に戻るなら starter に戻る
- P0 の具体チェックに戻るなら checklist に戻る
- preview/cache だけ見たいなら preview cache に戻る
- 3 ステップで済ませたいなら quickcheck に戻る
- コードを追うなら investigation に戻る
- 対象ファイルを固定したいなら target files に戻る
- 関数入口を固定したいなら functions に戻る

## Next Step

新しく調査する AI は、この文書を入口にしてから handoff と open questions を読むと流れが分かりやすい。
