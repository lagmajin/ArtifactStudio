# Milestone: Workspace Manager (2026-03-29)

**Status:** Implemented
**Goal:** カスタムワークスペースレイアウトの保存/読込。
用途別にパネル配置を切り替えられるようにする。

---

## 現状

| 機能 | 状態 |
|------|------|
| ドッキングパネル (Qt ADS) | ✅ 完成 |
| レイアウトリセット | ✅ 既存レイアウト復元と併用 |
| ワークスペース保存/読込 | ✅ 実装済み |
| デフォルトワークスペース | ✅ 設定値から起動時に適用 |

---

## 実装

### 1. WorkspaceManager
- `Artifact/src/Core/ArtifactWorkspaceManager.cppm` に実装済み
- Qt ADS の `saveState()` / `restoreState()` と `QMainWindow` の状態を JSON で保存
- `workspace_session.json` と `Presets/*.json` を使い分ける
- 実際の workspace mode は `Default / Import / Layout / Animation / VFX / Compositing / Text / Export / Debug / Audio`

### 2. プリセットワークスペース

| 名前 | パネル配置 |
|------|----------|
| **Preset A** | 保存した `QMainWindow` レイアウトと dock 可視状態 |
| **Preset B** | 保存時点の `WorkspaceMode` も含む |
| **Preset C** | プリセット名で追加削除可能 |

### 3. UI
- メニュー: 「表示 > ワークスペース」で mode 切替
- 追加メニュー: プリセット保存 / 削除 / 最後のセッション復元
- ショートカット: 既定ではメニュー経由、独立した F-key は未確認

---

## 現在の実装メモ

- 起動時の復元は [Artifact/src/AppMain.cppm](/X:/Dev/ArtifactStudio/Artifact/src/AppMain.cppm) で呼ばれている
- メニュー統合は [Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm)
- 既存の `WorkspaceMode` は [Artifact/src/Widgets/ArtifactMainWindow.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactMainWindow.cppm) と [Artifact/src/Widgets/ArtifactToolBar.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactToolBar.cppm) が担当

この文書は「未実装計画」ではなく、`ArtifactWorkspaceManager` の責務メモとして残すのが適切。

---

## 2026-07-25 現状確認

静的確認では、`ArtifactWorkspaceManager` の JSON セッション／プリセット API と `ArtifactViewMenu` の操作導線は存在する。また、アプリ終了時には `ArtifactWorkspaceManager::saveSession()` が呼ばれ、別系統の `FastSettingsStore` に ADS の dock state と geometry も保存されている。

ただし本ドキュメントの記述には差分がある。`ArtifactWorkspaceManager` 自身が保存する `UiLayoutState` は現在 geometry が中心で、コメントにもある通り `QMainWindow::saveState()` は使わず、dock 配置は `AppMain` の `main_window_layout.cbor` 保存系が担当する。さらに `restoreSession()` の呼び出しはメニュー操作に限られ、起動時に自動復元する呼び出しは確認できない。したがって「保存／読込の基盤は実装済み」だが、「WorkspaceManager が ADS 配置を JSON で一元管理し、起動時デフォルト／セッション復元まで完了」という当初記述は未達として整理する。

確認範囲: `Artifact/src/Core/ArtifactWorkspaceManager.cppm`、`Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`。ビルド・実機操作による動作確認は未実施。

## Update 2026-08-15

- 現行コードでは WorkspaceManager の session／preset JSON、保存・復元・削除・rename API、View Menu の操作導線を確認できる。
- geometry／UiLayoutState と ADS dock state／main-window layout は別保存系で、WorkspaceManager 単独の一元管理や起動時自動 session restore、独立 shortcut は未完了または未確認。
