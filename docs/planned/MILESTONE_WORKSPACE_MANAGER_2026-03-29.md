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
