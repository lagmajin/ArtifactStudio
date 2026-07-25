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

---

## Next Execution Slice

### Phase 1A の着手点

- `workspace_session.json` と `Presets/*.json` の責務差を明確にし、最後のセッション復元とユーザー保存プリセットを混ぜない
- 保存済みプリセット一覧の読み込み時に、壊れた JSON を 1 件ずつ切り分けて扱えるようにする
- `WorkspaceMode` の起動時デフォルト適用は、プリセット復元より先に走るのか後に走るのかを固定する

### Phase 1B の着手点

- `表示 > ワークスペース` メニューからの保存 / 復元 / 削除導線を、既存の `ArtifactViewMenu` に沿って整理する
- プリセット名変更は別ダイアログを増やさず、既存入力導線を流用できるか先に確認する
- 解像度変更や dock 再配置後も壊れにくい最小状態だけを優先して保存する

### Phase 2 前提

- `tab 順序` や細かな panel dependency は、一覧と破損対策が安定してから扱う
- 自動修復やショートカットの追加は、保存/読込が安定した後の改善に回す
- プリセット仕様は `WorkspaceManager` の責務メモと運用メモを分けて残す

---

## 2026-07-25 現状確認

静的なソース確認では、基礎的なプリセット保存・一覧・復元・削除・名前変更は実装済み。`ArtifactWorkspaceManager` は `Presets/*.json` と `workspace_session.json` を分離し、ウィンドウ geometry と `WorkspaceMode` を保存・復元する。`ArtifactViewMenu` には保存、削除、最後のセッション復元、および一覧からの個別プリセット復元導線がある。

一方、現仕様との差分として、保存対象は現在 geometry と workspace mode が中心で、ドック配置、タブ順序、全 dock の可視状態・サイズを包括的に保存する実装は確認できない。プリセット名変更の UI 導線、破損 JSON の個別エラー表示・自動修復、解像度変更時の補正、デフォルトプリセットの起動時適用、専用ショートカットも未確認。したがって本マイルストーンは「基礎 API / メニュー導線 実装済み、完全仕様と堅牢化は未完了」と判定する。

確認範囲: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。ビルド・実機操作による動作確認は未実施。
