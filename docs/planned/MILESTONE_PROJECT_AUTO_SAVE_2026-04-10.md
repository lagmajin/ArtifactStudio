# プロジェクト自動保存機能
**マイルストーン**: M-PJ-1 Project Auto-Save Feature
**作成日**: 2026-04-10
**最終更新**: 2026-08-15
**見積もり**: 8-10h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のようなプロフェッショナルツールに欠かせない自動保存機能を追加。
作業中のデータを定期的にバックアップし、クラッシュ時のデータ損失を防ぐ。

## 機能仕様

### 自動保存設定
**詳細設定:**
- 保存間隔: 1/5/10/30分ごと
- 保存世代数: 3/5/10世代保持
- 自動保存ファイルの場所: プロジェクトフォルダ内 `.autosave/`
- 大きな変更時のみ保存 (設定変更可能)

### 自動保存ファイル管理
**賢い管理:**
- ファイル名: `projectname_autosave_YYYYMMDD_HHMMSS.aep`
- 古いファイルの自動削除
- 保存中の視覚的フィードバック
- 手動保存との競合回避

### クラッシュリカバリー
**復元機能:**
- 起動時の自動保存ファイル検出
- リカバリーダイアログ表示
- 差分表示 (何が変更されたか)
- リカバリー後の手動保存確認

## 2026-08-15 現行コード監査

`ArtifactAutoSaveManager` の起動／停止、dirty 通知、Recovery snapshot 作成・検出・最新点読み込み、起動時の recovery prompt、設定値の保持、`QSaveFile`／Project backup 世代管理、定期 timer の接続は実装を確認した。一方、仕様にある1/5/10/30分・世代数設定の完全なUI、`.autosave/` 命名規約、差分表示、保存中フィードバック、手動保存との詳細な競合回避、クラッシュ後の実ファイル復元と性能検証は確認できない。したがって自動保存・復旧の基盤は実装済みだが、全仕様と runtime 検証は未完了とする。

## Update 2026-08-15

- 現行コードでは `ArtifactAutoSaveManager` の起動／停止、dirty 通知、recovery snapshot、`QSaveFile` の原子的 commit、設定 interval の timer 接続、起動時 recovery prompt、Project backup 世代管理を確認できる。
- 世代数 UI、`.autosave/` 命名規約、差分表示、保存中フィードバック、手動保存との競合仕様、実ファイル復元と runtime／性能検証は未完了または未確認。

## 2026-07-29 実装ループ: 設定反映と原子的スナップショット

- ✅ `ArtifactAppSettings::autoSaveIntervalMinutes()` の値を recovery timer の実行間隔へ接続した。
- ✅ recovery snapshot の書き込みを `QFile` から `QSaveFile` へ変更し、`commit()` 成功後だけ保存完了と扱うようにした。
- ⏳ UI での世代数設定、差分表示、保存中フィードバック、実ファイル復元、および runtime/build 検証は未完了。したがって本マイルストーン全体は `Partial` のままとする。

### 実装要件
- バックグラウンドスレッドでの保存
- UIブロッキングの回避
- 設定保存
- クロスプラットフォーム対応

### 実装場所
- `Artifact/src/Core/AutoSave/ArtifactAutoSaveManager.cppm` (新規)
- 設定項目: `Preferences > General > Auto-save`

## 技術的考慮
- 保存処理のパフォーマンス
- ファイルロックの管理
- エラーハンドリング
- メモリ使用量

## AEとの差別化
- より詳細な設定オプション
- 賢い保存タイミング
- 視覚的フィードバックの充実

## テストケース
- 各種保存間隔の動作確認
- クラッシュ後のリカバリー
- ファイル管理の正確性
- パフォーマンス影響の測定
