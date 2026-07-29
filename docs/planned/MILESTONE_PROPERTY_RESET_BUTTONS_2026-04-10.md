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

## Static Audit (2026-07-25)

現行の Property Editor には、プロパティ行ごとの reset button、reset icon／tooltip、hover 時の action 表示、context menu の `Reset Value`、表示設定 `artifactShouldShowPropertyResetButtons()`、crop／pan 専用の reset 導線が存在する。通常の layer property rows では reset button を表示する設定も確認でき、keyframe 操作・favorites・expression などの row chrome と同じ位置で扱われている。

ただし、すべての通常プロパティに default 値を復元する reset handler が接続されているか、keyframe 付き property で「全 keyframe 削除＋default 値」、選択 keyframe のみ削除、undo／redo、Transform／Text／Effect／Material 全対象の挙動は静的検索だけでは確認できない。設定値は現状プロセス内の表示フラグとして見え、Preferences への永続化も未確認である。視覚的配置は実装済みだが、runtime のクリック結果と reset 後の再評価・保存は未検証である。

判定: **UI 基盤は実装済み。** スマートリセット、Preferences 永続化、undo／redo と全対象の runtime 検証が残っている。

## Static Audit Update (2026-07-29)

- Reset handler がキーフレーム付きプロパティを検出し、既存の `SetLayerPropertyKeyframesCommand` で全キーフレーム削除をUndo対象にした後、デフォルト値を適用するようにした。
- キーフレームのないプロパティと既存の reset UI／context menu 経路は従来どおり。
- `ArtifactStudio/PropertyEditor/ShowResetButtons` を `QSettings` に保存し、アプリ再起動後も表示設定を復元するようにした。
- UIクリック、Undo/Redo、Preferences永続化、Transform/Text/Effect/Material全対象のruntime検証は未実施。

判定: **スマートリセットと表示設定の永続化を実装。runtime検証 pending。**
