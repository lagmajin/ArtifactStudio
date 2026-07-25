# マイルストーン: Dock / Workspace 機能監査 (2026-07-04)

> VS Code / Blender / Maya / Premiere ワークスペース比較。

## 🔴 P0

| 機能 | 参照元 | 状態 |
|---|---|---|
| **ワークスペースレイアウト保存** | VS Code/Blender | ❌ |
| **ワークスペースプリセット切替（Compositing/Animation/Color等）** | Blender/Premiere | ❌ |
| **デフォルトレイアウトにリセット** | VS Code/Blender | ❌ |
| **パネルの最大化/復元（ダブルクリックタブ）** | VS Code/Blender | ❌ |
| **パネルを新しいウィンドウに分離** | VS Code/Premiere | ❌ |

## 🟡 P1

| 機能 | 参照元 | 状態 |
|---|---|---|
| **タブのピン留め（閉じられないように）** | VS Code | ❌ |
| **タブの並び替え（ドラッグ）** | VS Code | ⚠️ |
| **タブの分割ビュー（左右/上下）** | VS Code | ⚠️ |
| **タブの色分け（プロジェクト/パネル種別）** | - | ❌ |
| **クイックパネル切替（Ctrl+1/2/3/4）** | Blender | ❌ |
| **エディタ領域の最大化（Shift+Space）** | Blender | ❌ |

---

## Static audit follow-up (2026-07-25)

`ArtifactMainWindow` と公開 dock API を確認した。元表の ❌ は、現行実装で一部が進んでいるため更新が必要である。

| 機能 | 現行確認 | 判定 |
|---|---|---|
| ワークスペースレイアウト保存 | ADS の `saveDockManagerState()` / `restoreDockManagerState()` が公開されている | 基盤実装済み |
| プリセット切替 | `WorkspaceMode` と mode ごとの dock visibility rule がある | 部分実装 |
| デフォルトへリセット | mode 切替・dock visibility API はあるが、専用 reset workflow は未確認 | 未確認 |
| パネル最大化 / 復元 | immersive dock の visibility 保存・復元経路はあるが、ダブルクリックタブ操作は未確認 | 部分実装 |
| 新しいウィンドウへの分離 | floating dock API、geometry、container refresh がある | 実装済み（基盤） |
| タブ並び替え / 分割 | ADS の標準 dock 操作に依存する受け口はあるが、専用 UX の検証は未確認 | 部分実装・未確認 |
| タブ pin / 色分け | 専用 pin と種別カラーの実装は未確認 | 未実装 |
| Ctrl+1/2/3/4 切替 | workspace button / mode API はあるが、指定ショートカットは未確認 | 未確認 |
| Shift+Space 最大化 | 専用 shortcut workflow は未確認 | 未実装 |
| lazy dock | placeholder、factory、first-show materialization、floating 遅延生成と失敗メタデータがある | 実装済み（基盤） |

**判定**: Dock の保存・復元、workspace mode、floating、lazy initialization は基盤実装済み。P0/P1 の操作仕様（reset、pin、専用 shortcut、double-click maximize）は未完了または検証待ち。
