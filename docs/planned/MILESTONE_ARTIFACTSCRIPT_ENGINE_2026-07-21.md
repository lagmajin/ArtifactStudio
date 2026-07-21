# MILESTONE_ARTIFACTSCRIPT_ENGINE_2026-07-21

**Status:** ✅ Complete (3/3)
**Goal:** ArtifactScript にホットリロード、メソッド本体の AST パース、および基本 VM 実行エンジンを実装する。

## 背景

`ArtifactScript` は C# ライクな独自スクリプト言語。パーサーとデータモデル（クラス/フィールド/メソッド宣言）は完成済みでテストも 5 件あるが、以下が欠落している：

- メソッド本体 `{ }` の実行（パースされるが捨てられている）
- ファイル変更検知とホットリロード
- スクリプト実行エンジン

## Scope

| # | 項目 | ファイル | 内容 |
|---|------|----------|------|
| 1 | ✅ メソッド本体 AST | `ArtifactScript.cppm` 拡張 | 再帰下降パーサー（式: リテラル/変数/二項/単項/関数呼出、文: 代入/if-else/return/ブロック）+ 本体収集ロジック |
| 2 | ホットリロード | `ArtifactScriptHotReload.ixx` + `.cppm` 新規 | ファイル監視、再コンパイル、フィールド値保持 |
| 3 | 基本 VM | `ArtifactScriptEvaluator.cppm` 新規 | スタックベース評価、変数スコープ、フック実行 |

## Non-goals

- ループ構文 (for/while)
- クラスメソッドの任意呼び出し
- UI 統合（Inspector / エディタ）
- レイヤー/コンポーネント接続
- ArtifactWidgets 変更

## 実施順序

1. メソッド本体 AST パース
2. ホットリロード
3. 基本 VM
