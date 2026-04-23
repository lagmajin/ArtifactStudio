# プロパティリセットボタンの追加
**マイルストーン**: M-PR-1 Property Reset Buttons
**作成日**: 2026-04-10
**見積もり**: 3-5h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のプロパティパネルにリセットボタンを追加し、素早くデフォルト値に戻せるようにする。
アニメーション調整時の効率化に役立つ。

## 機能仕様

### リセットボタン
**各プロパティに追加:**
- 小さなリセットアイコン (⟲) をプロパティ値の右側に配置
- ホバーでツールチップ "Reset to default"
- クリックで即座にデフォルト値にリセット

### スマートリセット
**文脈に応じた動作:**
- キーフレームなし: デフォルト値にリセット
- キーフレームあり: 全キーフレーム削除 + デフォルト値
- 選択キーフレームのみ: 選択キーフレームを削除

### 対象プロパティ
**主要プロパティ:**
- Transform: Position, Scale, Rotation, Opacity
- テキスト: Font Size, Tracking, Leading
- エフェクト: 全パラメータ
- マテリアル: Color, Roughness, Metalness

### 実装要件
- 既存プロパティウィジェット拡張
- undo/redo 対応
- 視覚的フィードバック
- 設定でON/OFF可能

### 実装場所
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyWidget.cppm` (拡張)
- 設定項目: `Preferences > Properties > Show reset buttons`

## 技術的考慮
- UIレイアウトの調整
- クリックイベントの処理
- デフォルト値の管理

## AEとの差別化
- より直感的な配置
- スマートリセット機能
- 視覚的フィードバック

## テストケース
- 各種プロパティのリセット動作
- キーフレーム有無での動作差
- undo/redo の動作確認