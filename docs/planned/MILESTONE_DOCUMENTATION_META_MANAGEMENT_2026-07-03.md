# M-DOCMETA: ドキュメントメタ管理 (Documentation Meta Management)

**作成日:** 2026-07-03  
**ステータス:** 計画  
**対象範囲:** `docs/`, `plans/`, `Artifact/docs/`, `ArtifactCore/docs/` 配下の全マークダウンファイル（推定 950+ ファイル）  
**目的:** ドキュメントの散逸を防ぎ、AI と人間の両方が効率的に文書を発見・管理できる基盤を構築する。  

---

## 1. 現状分析

### 1.1 規模

| ディレクトリ | ファイル数 | 備考 |
|-------------|----------|------|
| `docs/` (root) | 44 | WIDGET_MAP, 仕様書, 設計メモ等 |
| `docs/analysis/` | 17 | 分析レポート各種 |
| `docs/planned/` | ~550 | マイルストーン文書（最大カテゴリ） |
| `docs/done/` | ~60 | 完了マイルストーン |
| `docs/shared/` | ~20 | AI 共有メモ |
| `docs/technical/` | ~50 | 技術詳細 |
| `docs/worklog/` | 5 | 作業ログ |
| `docs/bugs/` | 2 | バグレポート |
| `docs/codereviews/` | 2 | コードレビュー |
| `docs/perf/` | 1 | パフォーマンス分析 |
| `docs/verification/` | 1 | 検証チェックリスト |
| `docs/theme-presets/` | 2 | テーマプリセット |
| `docs/trash/` | 1 | 未使用候補 |
| `plans/` | 5 | ルート計画文書 |
| `Artifact/docs/` | 88 | 子モジュール文書 |
| `ArtifactCore/docs/` | 33 | 子モジュール文書 |

### 1.2 既存のナビゲーション

- `docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md` — AE Parity 系のみの索引
- `docs/shared/ai-tech-memos/INDEX.md` — AI 共有メモの入口
- `docs/WIDGET_MAP.md` — ウィジェット責務一覧
- `AGENTS.md` — AI が作業を始める前に読むべきルール

→ いずれも **部分的な索引**。全体を横断するインデックスがない。

### 1.3 問題点

1. **発見困難**: 「あの分析結果どこだっけ？」が頻発する
2. **重複リスク**: 同じテーマの文書が複数存在（planned と analysis に同じテーマの文書）
3. **陳腐化**: `done` に移動した文書が `planned` のまま残っている
4. **リンク切れ**: 相対パスがファイル移動で壊れる
5. **AI 効率**: 毎回多くの文書を読まないと文脈が掴めない

---

## 2. 提案

### 2.1 Phase 1: ドキュメントインベントリ自動生成（すぐできる）

全マークダウンファイルのメタデータを自動収集するスクリプトを作成する。

**出力形式:** `docs/INDEX_GENERATED.md`

```markdown
# ドキュメントインベントリ（自動生成）

## カテゴリ: analysis
| ファイル | 日付 | タイトル（先頭行） | キーワード |
|---------|------|-------------------|-----------|
| docs/analysis/REPORT_AE_GAP_UPDATE_2026-07-03.md | 2026-07-03 | AE ギャップ最新実装状況レポート | AE, parity, gap |
| ... | ... | ... | ... |

## カテゴリ: planned
| ファイル | 日付 | タイトル | タグ |
|---------|------|---------|-----|
| ... | ... | ... | ... |
```

**収集するメタデータ:**
- ファイルパス（相対パス）
- ファイル名
- 作成日（ファイル名の日付 or H1 の日付行）
- タイトル（最初の H1）
- ファイルサイズ
- カテゴリ（ディレクトリ名）
- 最終更新日（git log）

### 2.2 Phase 2: ドキュメントライフサイクルルール

`planned/` から `done/` への移動ルールを明確化する。

**ルール:**
1. マイルストーン完了時は自動的に `planned/` → `done/` へ移動
2. `done/` の文書冒頭に Status: ✅ Complete を明記
3. 6ヶ月以上更新がない `planned/` 文書は `stale/` または `archived/` へ移動
4. `planned/` の文書は必ず `Status:` 行を持つ（Not Started / In Progress / Blocked / Complete）

**チェック用 CI ジョブ（オプション）:**
```bash
# planned/ 内の "Status: Complete" を含むファイルを検出 → done/ へ移動候補
grep -rl "Status:.*Complete\|✅ Complete" docs/planned/ | while read f; do
  echo "[WARN] $f is Complete but still in planned/"
done
```

### 2.3 Phase 3: クロスリファレンス検証

文書間の相対リンクが壊れていないかを検証する。

**検証ルール:**
- `[text](path)` 形式のリンクを抽出
- 相対パスを解決してファイル実在確認
- `X:/Dev/...` のような絶対パスも検出して警告

**出力例:**
```
WARNING: docs/planned/MILESTONE_FOO.md → ./some/other.md が存在しません
WARNING: X:/Dev/ArtifactStudio/docs/old/path.md は絶対パスです（相対パス推奨）
```

### 2.4 Phase 4: タグ/検索システム

各文書にタグを付与して検索可能にする。

**タグ付与ルール（手動/自動）:**
- 自動: ファイル名と H1 からキーワード抽出
- 手動: 文書冒頭の `Tags:` 行を追加（オプション）

**タグ例:**
```
Tags: AE parity, workflow, gap analysis, P0
Tags: timeline, keyframe, graph editor
Tags: render, GPU, Diligent, performance
```

---

## 3. 実装計画

### Phase 1: インベントリ自動生成（推定 2-4h）

| タスク | 内容 |
|-------|------|
| 1-1 | Python スクリプト作成: 全 md ファイルスキャン + メタデータ抽出 |
| 1-2 | 日付抽出: ファイル名 or H1 の日付行 or git log から日付を特定 |
| 1-3 | タイトル抽出: 最初の H1 行 |
| 1-4 | Markdown 形式で `docs/INDEX_GENERATED.md` に出力 |
| 1-5 | git add + commit → 親 submodule bump |

### Phase 2: ライフサイクルルール策定（推定 1-2h）

| タスク | 内容 |
|-------|------|
| 2-1 | ルール文書 `docs/DOC_LIFECYCLE.md` 作成 |
| 2-2 | `docs/planned/` 内の完了済み文書を `done/` へ移動（一括） |
| 2-3 | AGENTS.md にルールを追記 |

### Phase 3: クロスリファレンス検証（推定 2-3h）

| タスク | 内容 |
|-------|------|
| 3-1 | Python スクリプト作成: リンク抽出 + 実在検証 |
| 3-2 | 初期スキャン実行 → 壊れているリンクを修正 |
| 3-3 | CI 用の簡易チェックコマンド化 |

### Phase 4: タグ/検索システム（推定 2-4h）

| タスク | 内容 |
|-------|------|
| 4-1 | インベントリスクリプトにタグ推定ロジック追加 |
| 4-2 | Keywords 列を INDEX_GENERATED.md に追加 |
| 4-3 | `grep INDEX_GENERATED.md -i <keyword>` で検索できるようにする |

---

## 4. 関連

- `docs/shared/ai-tech-memos/INDEX.md` — 既存の部分索引
- `docs/WIDGET_MAP.md` — ウィジェット索引
- `AGENTS.md` — AI 向けルール
- `docs/shared/ai-tech-memos/AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md` — AE Parity 系索引

---

## 5. 成功条件

- `docs/INDEX_GENERATED.md` が存在し、全 md ファイルを網羅している
- `planned/` 内に Status: Complete の文書が存在しない
- クロスリファレンスリンク切れがゼロ
- 新しい文書作成時に `Tags:` 行を追加する習慣がつく

---

## 6. 更新履歴

- 2026-07-03: 初版作成
