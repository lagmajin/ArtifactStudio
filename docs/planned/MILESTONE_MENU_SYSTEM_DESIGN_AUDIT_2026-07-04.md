# マイルストーン: メニューシステム 機能監査 (2026-07-04)

**最終更新:** 2026-08-15

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

## Update 2026-08-15

- `ArtifactMenuBar` は File／Edit／Composition／Layer／Effect／Animation／Script／Render／Time／View／Option／Help 等の専用メニューを生成し、Animation 系の keyframe、interpolation、graph、time remap、freeze、time reverse などを実 action として接続している。旧 P0 表をそのまま「未実装」と扱うのは現行コードと一致しない。
- `ArtifactContextShortcutHelperWidget` と `ArtifactCore` の shortcut provider により、action 名・現在のキー割り当ての検索／一覧表示は存在する。メニューバー側も設定由来のフォントスケールと左利き時の配置補正、主要 UI の Accessible Name／Description を持つ。
- なお Recent Commands 履歴、Help メニュー内のコマンド検索、Maya 風 Marking Menu、全 action の共通 inventory／重複検出、全パネル間での enabled／checked 状態の一貫した同期は確認できない。
- 判定は **主要メニューと既存 action 導線は実装済み／横断的なメニュー統合・追加 UX は未完了** を維持する。ビルド・テスト・runtime 確認は未実施。
