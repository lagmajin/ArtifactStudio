# マイルストーン: コンポジションエディター & レイヤービュー

> 2026-03-21 作成

## 現状サマリー

コンポジションエディターは 2D 変換 (選択/移動/スケール/Undo/グリッド/セーフエリア) が動作する基盤がある。
主な欠落: **回転ハンドル**、**ガイド線**、**非機能 UI コントロール** (解像度ドロップダウン、ファストプレビュー)、**3D カメラ**。
レイヤービューは単一レイヤー描画のみで、インタラクションやオーバーレイがない。

---

## Phase 1: ビューポート変換の完成 (P0)

TransformGizmo に回転とアンカーポイントを追加し、2D 変換を完全にする。
現在は `Move / Rotate / Scale` のハンドルを持つが、UI から操作モードを切り替えられるようにして、移動以外の gizmo を明示的に選べるようにする。

| タスク | 対象ファイル | 見積 |
|---|---|---|
| TransformGizmo に回転ハンドル追加 | `TransformGizmo.cppm` | +80行 |
| CompositionRenderWidget に回転ドラッグ処理 | `ArtifactCompositionRenderWidget.cppm` | +60行 |
| アンカーポイントハンドル追加 | `TransformGizmo.cppm` | +40行 |
| 2D gizmo mode 切替 (All / Move / Rotate / Scale) | `ArtifactCompositionEditor.cppm`, `TransformGizmo.cppm` | +60行 |
| 解像度ドロップダウンを実装 | `ArtifactCompositionEditor.cppm` | +20行 |
| ファストプレビューメニュー接続 | `ArtifactCompositionEditor.cppm` | +30行 |

---

## Phase 2: ガイド & オーバーレイ (P1)

`showGuides_` フラッグが存在するが描画コードがない。профессиональный composition 作業に必要なオーバーレイを実装。

| タスク | 対象ファイル | 見積 |
|---|---|---|
| ガイド線描画 | `ArtifactCompositionRenderController.cppm` | +60行 |
| ガイド管理 (追加/削除/移動) | 新規 or エディタ内 | +150行 |
| GridRenderer 実装 (現在スタブ) | `GridRenderer.cpp` | +100行 |
| フレーム情報オーバーレイ (フレーム番号/タイムコード) | `CompositionRenderController` | +40行 |
| ルーラーオーバーレイ | 新規 | +120行 |

---

## Phase 3: レイヤービュー強化 (P1)

単一レイヤーの検査ツールとして使いやすくする。

| タスク | 対象ファイル | 見積 |
|---|---|---|
| バウンディングボックス + 選択ハイライト | `ArtifactRenderLayerWidgetv2.cppm` | +80行 |
| `setEditMode()` / `setDisplayMode()` 実装 | `ArtifactRenderLayerWidgetv2.cppm` | +60行 |
| レイヤー情報オーバーレイ (名前/サイズ/位置) | `ArtifactRenderLayerWidgetv2.cppm` | +40行 |
| グリッド/セーフエリア表示 | `ArtifactRenderLayerWidgetv2.cppm` | +50行 |

---

## Phase 4: 3D ビューポート基盤 (P2)

3D カメラと基本的な 3D レイヤー対応。

| タスク | 対象ファイル | 見積 |
|---|---|---|
| 3D カメラ (オービット/パン/ドリー) 実装 | `ArtifactViewportCamera.ixx` | +100行 |
| Alt+MMB オービット操作 | `ArtifactCompositionRenderWidget.cppm` | +50行 |
| 3D 変換ギズモ (移動/回転/スケール) | 新規 `TransformGizmo3D.cppm` | +300行 |
| レイヤーの Z-depth プロパティ | Layer 抽象クラス群 | +60行 |

---

## Phase 5: ビューポート品質 & マルチビュー (P2)

プロ仕様のビューポート制御。

| タスク | 対象ファイル | 見積 |
|---|---|---|
| ビューポート品質設定 (AA, 解像度スケール, FPS cap) | 新規設定パネル | +150行 |
| スプリットビュー / マルチビュー | 新規 `MultiViewportWidget` | +300行 |
| ビューポート背景オプション (チェッカーボード/ソリッド/カスタム) | `CompositionRenderController` | +40行 |

---

## Phase 6: Editing Workflow Expansion (P1/P2)

コンポジットエディタを「見る場所」から「編集する場所」へさらに寄せる。

### 対象

- text layer の直接編集導線
- mask / roto 入口
- rubber band multi-selection
- current layer / active layer / solo / lock の編集同期
- keyframe / playhead / selection の連携

### 連携先

- `docs/planned/MILESTONE_TEXT_LAYER_INLINE_EDIT_2026-03-27.md`
- `docs/planned/MILESTONE_COMPOSITION_EDITOR_RUBBER_BAND_MULTI_SELECTION_2026-03-26.md`
- `docs/planned/MILESTONE_TIMELINE_KEYFRAME_EDITING_2026-03-27.md`
- `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md`

### 作業項目

| タスク | 対象ファイル | 見積 |
|---|---|---|
| Text Layer 直接編集を viewport 内で開始できるようにする | `ArtifactCompositionEditor.cppm` | +120行 |
| mask / roto 編集への入口を gizmo と衝突しない形で足す | `ArtifactCompositionEditor.cppm`, `ArtifactRenderLayerWidgetv2.cppm` | +120行 |
| rubber band multi-selection の selection sync を editor 側で扱う | `ArtifactCompositionEditor.cppm` | +100行 |
| keyframe / timeline との selection 整合を強める | `ArtifactCompositionEditor.cppm`, `ArtifactTimelineWidget.cpp` | +80行 |

### 進捗

- 2026-03-27 時点で、`TransformGizmo` の `All / Move / Rotate / Scale` 切替は既に入っている
- 2026-03-27 時点で、Text Layer 直接編集の最小導線は別 milestone として Phase 1 が進んでいる
- 2026-03-27 時点で、Layer Solo View 側の mask 編集入口も別系統で進行中
- 2026-03-27 時点で、Composition Editor に `Edit Text` の明示アクションを追加し、Text Layer の直接編集導線を増やした
- 2026-03-27 時点で、Composition Editor の viewport context menu からも Text Layer 編集を開始できるようにした
- 2026-03-27 時点で、Composition Editor の toolbar / pie menu / shortcut から `Mask` tool (Pen) を選べるようにし、mask / roto 入口の最小導線を追加した
- 2026-03-28 時点で、mask / roto 編集を `docs/planned/MILESTONE_COMPOSITION_EDITOR_MASK_ROTO_EDITING_2026-03-28.md` に切り出した
- 2026-04-01 時点で、Composition Editor に pivot の最小導線として `Pivot: Center / Top Left` を追加した
- 2026-04-01 時点で、Composition Editor と Layer Editor の immersive fullscreen を `F11` で切り替えられるようにした

---

## 現状の機能マップ

| 機能 | 状態 |
|---|---|
| 2D パン/ズーム | ✅ 動作 |
| レイヤー選択 (クリック) | ✅ 動作 |
| レイヤー移動 (ドラッグ) | ✅ Undo 対応 |
| レイヤー縮小拡大 (コーナーハンドル) | ✅ 動作 |
| マルチ選択 (Shift+クリック) | ✅ 動作 |
| 制約/スナップ (ドラッグ中) | ✅ 動作 |
| グリッドオーバーレイ | ✅ 動作 |
| セーフエリア | ✅ 動作 |
| コンテキストメニュー | ✅ 動作 |
| キーボードショートカット | ✅ 動作 |
| 再生フレーム同期 | ✅ 動作 |
| Zoom Fit/100%/Reset | ✅ 動作 |
| ソフトウェアプレビュー | ✅ 動作 |
| **回転ハンドル** | ❌ 未実装 |
| **アンカーポイント編集** | ❌ 未実装 |
| **2D gizmo mode 切替** | ✅ 動作 |
| **ガイド線** | ❌ フラッグのみ、描画なし |
| **解像度切り替え** | ❌ UI のみ、ハンドラが qDebug |
| **ファストプレビュー** | ❌ UI のみ、未接続 |
| **3D カメラ** | ❌ ViewportCamera が 2D のみ |
| **GridRenderer** | ❌ 全メソッドスタブ |
| **レイヤービュー: バウンディングボックス** | ❌ 未実装 |
| **レイヤービュー: edit/display mode** | ❌ 空メソッド |
| **ルーラー** | ❌ 未実装 |
| **マルチビュー** | ❌ 未実装 |
| **Text Layer viewport edit** | △ 最小導線のみ |
| **Mask / roto entry** | △ 別 milestone で拡張中 |
| **Rubber band multi-selection** | △ 別 milestone で設計済み |
