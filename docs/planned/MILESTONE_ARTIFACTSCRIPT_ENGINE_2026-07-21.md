# MILESTONE_ARTIFACTSCRIPT_ENGINE_2026-07-21

**Status:** ✅ Complete (3/3)
**最終更新:** 2026-08-15
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
| 2 | ✅ ホットリロード | `ArtifactScript.ixx` / `ArtifactScript.cppm` | ファイル監視、再コンパイル、フィールド値保持 |
| 3 | ✅ 基本 VM | `ArtifactScript.ixx` / `ArtifactScript.cppm` | スタックベース評価、変数スコープ、フック実行 |

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

## 2026-07-25 実装監査

- `ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx` に `ArtifactScriptEvaluator` と `ArtifactScriptHotReload` の公開 API が存在する。
- `ArtifactCore/src/Script/ArtifactScript/ArtifactScript.cppm` にメソッド本体 AST の再帰下降パーサー、Evaluator の式／文実行、HotReload の監視・再読み込み・フィールド保持が実装されている。
- 当初想定されていた専用ファイル分割ではなく、既存の `ArtifactScript` モジュールへ統合された実装であるため、計画表のファイル表記を実際の配置に合わせた。
- UI 統合、ループ、任意のクラスメソッド呼び出しは Non-goals のため、未実装でも本マイルストーンの完了判定には含めない。

## 2026-08-15 現行コード照合

`ArtifactCore/include/Script/ArtifactScript/ArtifactScript.ixx` と対応する `ArtifactScript.cppm` で、メソッド本体 AST、式／文の評価、変数スコープ、フック、ファイル監視、再読み込み、フィールド値保持の実装を再確認した。配置も計画当初の分割案ではなく、既存 ArtifactScript モジュールへの統合で整合している。

`Artifact` 側には別系統の Expression parser/evaluator と Inspector/Copilot 導線も存在するが、これは本 milestone の ArtifactScript VM 完了条件とは分離して扱う。ループ、任意のクラスメソッド呼び出し、UI／レイヤー接続は引き続き Non-goals。今回はビルド・テストを実行していないため、実行時受入れは未検証として残す。
