# レイヤースタイルプリセットの実装
**マイルストーン**: M-LA-6 Layer Style Presets
**作成日**: 2026-04-10
**最終更新**: 2026-08-15
**見積もり**: 6-8h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のレイヤーに適用したスタイル (ドロップシャドウ、アウトラインなど) をプリセットとして保存・再利用できるようにする。
繰り返しのスタイル適用作業を効率化。

## 機能仕様

### プリセット管理
**保存/読み込み:**
- 現在のレイヤーのスタイルをプリセットとして保存
- プリセット名とカテゴリ設定
- プリセットの一覧表示と検索
- プリセットの削除/名前変更

### スタイル内容
**保存対象:**
- ドロップシャドウ: 色/不透明度/角度/距離/広がり/サイズ
- アウトライン: 色/不透明度/サイズ
- グロー: 色/不透明度/サイズ/広がり
- ベベル: スタイル/サイズ/ソフトネス
- グラデーションオーバーレイ: 色/不透明度/角度/スケール

### クイック適用
**レイヤーパネル統合:**
- レイヤー右クリックメニューにプリセット適用
- ドラッグ&ドロップでの適用
- キーボードショートカット対応

### 実装要件
- JSON形式でのプリセット保存
- 既存スタイルシステム拡張
- undo/redo 対応
- デフォルトプリセット同梱

### 実装場所
- `Artifact/src/Core/Styles/ArtifactLayerStyleManager.cppm` (新規)
- UI: レイヤーパネル右クリックメニュー

## 技術的考慮
- スタイルデータの構造化
- メモリ使用量の最適化
- バージョン互換性

## AEとの差別化
- より詳細なプリセット管理
- ドラッグ&ドロップ対応
- デフォルトプリセットの充実

## テストケース
- プリセットの保存/読み込み精度
- 各種スタイルの適用確認
- undo/redo の動作

---

## 2026-07-25 現状確認

## 2026-08-15 現行コード監査

- `ArtifactEffectPreset`／`ArtifactEffectPresetCollection` は ID、名前、カテゴリ、説明、typed parameter、thumbnail、JSON 保存・読込、削除、カテゴリ一覧、既定プリセットを実装している。
- `ArtifactPresetManager` と Inspector には effect／mask の Save／Load Preset 導線があり、WorkspaceAutomation には layer effect preset の保存・読込・一覧 API がある。
- ただし本マイルストーンの「複数 style を一つの layer-style snapshot として保存」「Layer Panel の適用・D&D・専用検索／削除／名前変更」「style 全体を一括 Undo する UI」は確認できない。
- 判定は **Effect Preset 基盤と個別適用は実装済み、Layer Style Preset としての統合 UI は未完了**。旧記述より実装範囲は進んでいるが、runtime 検証は未実施。

部分実装。現行コードには `ArtifactEffectPreset` 系の既定プリセット読み込みと、`ArtifactEffectService`／`WorkspaceAutomation` 経由の layer effect preset 保存・読み込み・適用・一覧 API がある。

一方、本マイルストーンが定義する Layer Style 専用の管理面（ドロップシャドウ／アウトライン／グロー等をまとめたカテゴリ付きプリセット、一覧・検索・削除・名前変更、レイヤーパネル右クリック、D&D、ショートカット、デフォルト同梱 UI）は確認できない。JSON の effect preset 経路は再利用候補だが、レイヤースタイル全体の snapshot と Undo/Redo を一つの導線で扱う実装は未確認である。

したがって「Effect preset の基盤は部分実装、Layer Style Presets の UI／統合は未完了」と整理する。
