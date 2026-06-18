# ワークスペースプリセット管理の実装
**マイルストーン**: M-WS-1 Workspace Preset Management
**作成日**: 2026-04-10
**見積もり**: 8-10h
**優先度**: Low (細かいUX改善)

## 概要

After Effects 風のワークスペースプリセット機能を目指す。
パネルの配置やサイズを保存/読み込みできるようにする。

## 現状

- `Artifact/src/Core/ArtifactWorkspaceManager.cppm` に保存/復元がある
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` から保存・削除・復元を操作できる
- 現在は「レイアウト + dock 可視状態 + workspaceMode の保存」が主

## 機能仕様

### プリセット管理
**基本機能:**
- 現在のワークスペースをプリセットとして保存
- 保存済みプリセットの一覧表示
- プリセットの削除/名前変更

**プリセット内容:**
- 全パネルの表示/非表示状態
- パネルのサイズと位置
- ドック配置
- タブ順序

### デフォルトプリセット
**現行実装の考え方:**
- 保存済みプリセットはユーザー作成が中心
- プリセットは JSON ファイル単位で管理する
- `workspace_session.json` は最後のセッション復元用

### クイックアクセス
- `表示 > ワークスペース` メニュー
- `プリセット` サブメニューから保存 / 削除 / 復元
- 独立したショートカットは未整理

### 実装要件
- QMainWindow のステート保存/復元
- 設定ファイル(JSON)への保存
- 起動時のデフォルト workspace mode 適用
- 破損プリセットの扱いは今後の改善点

### 実装場所
- [Artifact/src/Core/ArtifactWorkspaceManager.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Core/ArtifactWorkspaceManager.cppm)
- [Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm)

## 技術的考慮
- パネル状態の正確な保存/復元
- 画面解像度変更への対応
- パネル依存関係の解決

## AEとの差別化
- より詳細なプリセット管理
- キーボードショートカット
- 自動修復機能

## Source Alignment Notes

- `WorkspaceMode` の本体は [Artifact/src/Widgets/ArtifactMainWindow.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactMainWindow.cppm)
- `WorkspaceMode` の選択 UI は [Artifact/src/Widgets/ArtifactToolBar.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactToolBar.cppm)
- 実際の保存/復元は [Artifact/src/Core/ArtifactWorkspaceManager.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Core/ArtifactWorkspaceManager.cppm)

## テストケース
- プリセットの保存/読み込み精度
- 画面解像度変更時の対応
- 破損ファイルの処理
- デフォルトプリセットの適用
