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

---

## Static audit follow-up (2026-07-25)

現行のメニュー実装を再確認した。File / Composition / View / Render / Time / Animation には実 action、trigger 接続、project・composition・layer・playback・queue に応じた enabled / checked 更新がある。したがって、この監査表の P0 は「未実装一覧」ではなく、現行実装との差分を再評価する必要がある。特に Close Project、View の rulers/grid、Composition Settings、Animation の time remap などは既存メニュー側に入口が存在する。

未確認または不足として残るのは、Recent Commands、Help 内メニュー検索、Maya 風 Marking Menu、全メニューのキーボード表示統一、共通 action inventory と実行時のクロスパネル同期である。今後は P0 表を source evidence 付きの実装状況表へ置き換え、重複した監査記述を整理する。
