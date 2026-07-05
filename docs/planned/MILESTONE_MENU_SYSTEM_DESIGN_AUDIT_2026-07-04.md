# マイルストーン: メニューシステム 機能監査 (2026-07-04)

> AE / Premiere / Blender / Maya / C4D / VS Code メニュー比較。

## 🔴 P0: メニュー構成の不足

| メニュー | 不足項目 |
|---|---|
| **File** | Save As Template / Export As / Project Settings / Close Project |
| **Edit** | Duplicate / Select All / Deselect / Invert Selection / Paste in Place / Paste Attributes |
| **View** | Show/Hide Rulers / Grid Snap Toggle / Resolution Gate / Channel View (R/G/B/A) |
| **Composition** | Composition Settings / Add to Render Queue / Save Frame As / Preview RAM |
| **Layer** | Layer Styles / Convert to Editable Text / Create Outlines / Auto-trace |
| **Animation** | Keyframe Assistant / Easy Ease / Roving Keyframes / Time-Reverse / Exponential Scale |
| **Window** | Reset Workspace / Save Workspace / Workspace Presets |

## 🟡 P1: メニュー UX

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Command Palette（Ctrl+Shift+P 全コマンド検索）** | VS Code/C4D Commander | ⚠️ Ctrl+K で showCommandPalette あり |
| **Recent Commands 履歴** | VS Code | ❌ |
| **メニュー検索（Help メニューの検索ボックス）** | VS Code/Blender | ❌ |
| **キーボードショートカット表示（メニュー右端）** | 全アプリ | ⚠️ |
| **コンテキストメニューのカスケード整理** | Blender | ⚠️ |
| **Marking Menu（ジェスチャー方向メニュー）** | Maya | ❌ |