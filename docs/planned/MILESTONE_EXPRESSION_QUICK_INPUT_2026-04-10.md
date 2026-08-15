# エクスプレッションクイック入力の実装
**マイルストーン**: M-EX-1 Expression Quick Input System
**作成日**: 2026-04-10
**最終更新:** 2026-08-15
**見積もり**: 6-8h
**優先度**: Low (細かいUX改善)

## 現行コード監査 (2026-08-15)

`ArtifactExpressionCopilotWidget` は `thisComp`／`thisLayer`／root 候補の文脈補完、候補ポップアップ、Tab／Enter適用、構文解析・runtime検証、エラー位置の波線表示を実装している。`ArtifactAnimationMenu` と Property Widget から新規／アクティブ expression の導線も確認できる。

一方、旧仕様の右クリック `Add Expression` サブメニュー内の固定関数一覧・定数挿入、引数型ヒント、snippet の保存／分類／import-export は現行コード検索では確認できない。したがって入力補完・検証の基盤は実装済みだが、Quick Input の Definition of Done 全体は未完了とする。

## 概要

After Effects のエクスプレッション入力で、よく使う関数や定数を素早く入力できるようにする。
コーディングの手間を減らし、ワークフローを効率化する。

## 機能仕様

### クイックインサートメニュー
**プロパティ右クリック時:**
- `Add Expression...` サブメニュー
- よく使う関数の一覧表示
- 定数/変数のクイック挿入

**メニュー項目例:**
```
Add Expression >
├── wiggle(10, 20)
├── loopOut()
├── linear(time, 0, 1, 100, 200)
├── Math.PI
├── thisComp.width
└── Custom... (手動入力)
```

### インテリジェント補完
**入力支援:**
- 関数名の自動補完
- 引数の型ヒント表示
- 構文エラーのリアルタイムチェック
- 関連プロパティの提案

### スニペット管理
**カスタムスニペット:**
- よく使うエクスプレッションの保存
- カテゴリ別整理
- インポート/エクスポート

### 実装要件
- 既存エクスプレッションエディタ拡張
- コンテキストメニュー統合
- undo/redo 対応
- ヘルプドキュメント連携

### 実装場所
- `Artifact/src/Script/Expression/ArtifactExpressionHelper.cppm` (新規)
- UI: プロパティパネルの右クリックメニュー

## 技術的考慮
- JavaScript 構文解析
- メモリ使用量の最適化
- 国際化対応

## AEとの差別化
- より詳細なクイックメニュー
- インテリジェント補完
- カスタムスニペット管理

## テストケース
- 各種関数の正確な挿入
- 補完機能の動作確認
- 構文チェックの正確性
- スニペットの保存/読み込み

---

## Static audit follow-up (2026-07-25)

現行ソースでは ArtifactExpressionCopilotWidget に expression の候補提示と loopOut などの挿入候補があり、AbstractProperty には expression の保存・評価経路がある。一方、仕様にあるプロパティ右クリックの Quick Insert メニュー、関数／引数補完、型ヒント、リアルタイム構文検査、カスタム snippet の保存・入出力は専用実装として確認できない。

したがって既存の Copilot／Evaluator 基盤は部分利用可能だが、本マイルストーンの Quick Input 完了条件は未達。次の候補は、既存 expression editor の入口を再利用した最小の挿入メニューと構文検査表示の確認である。
