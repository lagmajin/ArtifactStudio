# AI Shared Tech Memos

このディレクトリは、AI 同士で共有したい技術メモの置き場です。

用途:
- 既存実装の要点整理
- 仕様の読み取りメモ
- 調査結果の短い要約
- 途中で見つけた設計上の注意点
- 次の AI がすぐ再開できる状態メモ

運用ルール:
- 短く、具体的に書く
- 事実と推測を分ける
- 変更対象ファイルをできるだけ明記する
- 1 件 1 テーマにする
- 重要な前提は日付付きで残す

推奨ファイル名:
- `TOPIC_YYYY-MM-DD.md`
- `TOPIC_PART1_YYYY-MM-DD.md`
- `TOPIC_NOTE_YYYY-MM-DD.md`

書いておくと役立つ項目:
- 何を見たか
- 何が分かったか
- どこがまだ不確かか
- 次に触るべきファイル
- 実装で壊しやすい点

テンプレート:

```md
# Topic

> YYYY-MM-DD

## Summary

## Evidence

## Risks

## Next Steps
```

このディレクトリは、マイルストーン文書の代わりではなく、実装中の小さい観測メモを共有するためのものです。

関連メモ:
- 関数入口を固定するなら [After Effects Parity P0 Preview Cache Functions](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_FUNCTIONS_2026-05-31.md)
- 対象ファイルを固定するなら [After Effects Parity P0 Preview Cache Target Files](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_TARGET_FILES_2026-05-31.md)
- コードを追うための 1 ページ手順は [After Effects Parity P0 Preview Cache Investigation](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_INVESTIGATION_2026-05-31.md)
- 3 ステップの最短版は [After Effects Parity P0 Preview Cache Quickcheck](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_QUICKCHECK_2026-05-31.md)
- preview / cache だけを見るなら [After Effects Parity P0 Preview Cache](AFTER_EFFECTS_PARITY_P0_PREVIEW_CACHE_2026-05-31.md)
- P0 の確認表は [After Effects Parity P0 Checklist](AFTER_EFFECTS_PARITY_P0_CHECKLIST_2026-05-31.md)
- P0 だけを切り出した起点は [After Effects Parity P0 Starter](AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md)
- 実行順の地図は [After Effects Parity Execution Roadmap](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md)
- まず読む総括は [After Effects Parity Master Summary](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)
- After Effects 比較の入口は [INDEX.md](INDEX.md) を見る
- 比較用のまとめは [After Effects Parity Comparison Notes](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)
- P0〜P3 の抜き出し表は [After Effects Parity Checklist](AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md)
- 再開点の案内は [After Effects Parity Handoff](AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md)
- 未解決の論点は [After Effects Parity Open Questions](AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md)
- 入口の整理図は [After Effects Parity Document Map](AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md)
