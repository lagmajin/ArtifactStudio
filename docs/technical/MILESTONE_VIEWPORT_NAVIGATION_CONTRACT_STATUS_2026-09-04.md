# Viewport Navigation Contract Status (2026-09-04)

**最終更新:** 2026-09-04
**カテゴリ:** 状態マップ（実装済み / 部分実装 / 未着手の仕分け）
**関連:**
- `docs/planned/MILESTONE_VIEWPORT_INTERACTION_NAVIGATION_CURSOR_2026-07-04.md` (M-VP-9)
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` (統合監査)
- `docs/technical/GIZMO_2D_3D_CAMERA_COORDINATE_CONTRACT_2026-07-16.md`
- `docs/technical/GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md`

---

## 目的

M-VP-9「Navigation / 3D Cursor / Work Cursor」契約の**現状実装マップ**を記録する。
次の着手候補（navigation cross / active viewport 枠 / preview-only と camera layer の厳密分離 /
pivot source selector / surface snap）に入る前に、既存実装がどこまで踏み込んでいるかを
静的に固定する。ビルド・runtime 検証は実施していない。

---

## 1. 既存実装（静的確認済み）

### 1.1 Editor 側ナビゲーション state

| 項目 | 場所 | 状態 |
|---|---|---|
| Alt+LMB Orbit | `ArtifactCompositionEditor.cppm:5013, 5434` `isAltOrbiting_` | ✅ 実装済み |
| MMB Pan | `ArtifactCompositionEditor.cppm` `isPanning_` | ✅ 実装済み |
| Wheel Zoom | `ArtifactCompositionEditor.cppm` `isAltZooming_` | ✅ 実装済み |
| Pan 慣性 | 既存 | ✅ 実装済み（監査 2026-07-25 確認） |
| ナビゲーション開始/終了 cross | （本ドキュメント時点、未実装） | ❌ 未着手 |
| Active viewport の細い枠 | （pane manager 移行と密結合） | ❌ 未着手 |

### 1.2 Preview Orbit Mode（M-VP-9 "preview-only view state" の最小実装）

| 項目 | 場所 | 状態 |
|---|---|---|
| Toolbar action | `previewOrbitAction_` (`10256`) | ✅ |
| ON/OFF 切替 | `setPreviewOrbitMode()` (`9220-9272`) | ✅ |
| スナップショット保存 | orientation / pan / zoom を `PreviewOrbitSnapshot` に保存 (`9238-9247`) | ✅ |
| スナップショット復元 | OFF 時に snapshot から復元 (`9251-9266`) | ✅ |
| Navigation session 状態 (isAltOrbiting_ 等) の保存 | （実装されていない） | ⚠️ 部分実装 |
| `maskNavigationLocked` 時の抑制 | `9222-9228` | ✅ |

**注意点**: 現状の `setPreviewOrbitMode` は viewport の **camera 状態**（orientation/pan/zoom）
のみを保存・復元する。navigation session 自体（orbit 中フラグ、drag 中の一時 transform
delta）は snapshot に含まれない。これは「preview-only 時に render camera を直接触らない」
という M-VP-9 契約の核と、**まだ厳密に一致しない**。

### 1.3 Frame Selected / Frame All / View Undo/Redo

| 項目 | 場所 | 状態 |
|---|---|---|
| Frame Selected (QAction + QShortcut) | `10070-10083`, `11875-11884` | ✅ |
| Frame All (QAction + QShortcut) | `10084-10097`, `11885-11894` | ✅ |
| View Undo (QShortcut) | `12325-12335` | ✅ |
| View Redo (QShortcut) | `12337-12347` | ✅ |

すべて `CompositionRenderController` に対する **active viewport** 操作。

### 1.4 Active Pane / Active Viewport（M-VP-2 との接点）

| 項目 | 場所 | 状態 |
|---|---|---|
| `activeViewport()` | `ArtifactCompositionEditor.cppm:7983` | ✅ |
| `activeViewportPaneCount()` | `8002, 8023` | ✅ |
| `forEachActiveViewport()` | `8184-...` | ✅ |
| Pane rect hit test による active 切替 | （pane manager 移行に含む） | ⚠️ 部分実装 |
| Active viewport の細い枠表示 | （未着手） | ❌ |

### 1.5 Work Cursor（XY 平面上の 2D 基準点）

| 項目 | 場所 | 状態 |
|---|---|---|
| 配置 / 中央化 / 消去 / overlay 表示 | 既存 | ✅（監査 2026-07-25 確認） |
| Pivot source 切替 | （未実装） | ❌ |
| Orbit source selector | （未実装） | ❌ |
| Surface snap | （未実装） | ❌ |
| Project persistence | Phase 3 で決定する仕様 → **未着手** | ❌ |

---

## 2. 未着手 / 部分的ギャップ

### 2.1 M-VP-9 仕様の厳密契約

| 項目 | 現状 | 仕様上の目標 |
|---|---|---|
| preview-only vs camera layer の厳密分離 | 部分実装（camera state snapshot のみ） | orbit/pan/dolly が render camera layer を直接触らない経路を作る |
| Point of Interest (POI) の決定 | cursor-under-point 優先の厳密契約なし | cursor-under-point → 選択中心 → 3D Cursor → viewport 中央の優先順 |
| pivot source selector | なし | `Object / Selection / 3D Cursor / Individual` 切替 |
| orbit source selector | なし | 同上 |

### 2.2 UI 上の cross / 枠表示

| 項目 | 状態 |
|---|---|
| navigation 開始時の画面中央 cross | ❌ 未実装 |
| Active viewport の細い枠 | ❌ 未実装（M-VP-2 pane manager 移行と密結合） |
| Work Cursor overlay 表示 | ✅ 既存（`drawCameraPoiOverlay` 系） |

### 2.3 Cursor snap / 3D 深度

| 項目 | 状態 |
|---|---|
| surface snap | ❌ |
| depth ambiguity 解決 | ❌ |
| cursor placement miss 防御 | ❌ |

---

## 3. 着手優先度（提案）

M-VP-9 の Phase 1-5 のうち、現状の低リスク着手候補:

1. **navigation cross 描画の最小実装**: theme token のみ使用、画面中央の + マーク。`previewOrbitMode_` が ON の間だけ表示。Editor 側に navigation active フラグを `RenderOverlay` 側に渡す軽量経路を追加。
2. **preview-orbit snapshot の navigation session 拡張**: `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを snapshot に含める。
4. **active viewport 細い枠**: pane manager 移行 (M-VP-2) とセット。低優先。

これらは AGENTS.md の「既存挙動を不用意に変えない」「新規 signal/slot 接続禁止」「QPainter /
Qt CompositionMode / QImage / QColorDialog 禁止」「RenderScheduler / DX12 パスはシビア扱い」
に従って、最小・局所・theme token 寄せで進める必要がある。

---

## 4. 確認範囲

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm` (Phase 1-2 改変済み)
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` (関数一覧のみ)
- 関連マイルストーン文書

ビルド・runtime 検証は未実施。