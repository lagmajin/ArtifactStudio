# マイルストーン: コンポジションエディター & レイヤービュー

> 2026-03-21 作成

## 現状サマリー

コンポジションエディターは 2D 変換 (選択/移動/スケール/Undo/グリッド/セーフエリア) が動作する基盤がある。
主な欠落: **回転ハンドル**、**ガイド線**、**非機能 UI コントロール** (解像度ドロップダウン、ファストプレビュー)、**3D カメラ**。
レイヤービューは単一レイヤー描画のみで、インタラクションやオーバーレイがない。

---

## Phase 1: ビューポート変換の完成 (P0)

TransformGizmo に回転とアンカーポイントを追加し、2D 変換を完全にする。

| タスク | 対象ファイル | 見積 |
|---|---|---|
| TransformGizmo に回転ハンドル追加 | `TransformGizmo.cppm` | +80行 |
| CompositionRenderWidget に回転ドラッグ処理 | `ArtifactCompositionRenderWidget.cppm` | +60行 |
| アンカーポイントハンドル追加 | `TransformGizmo.cppm` | +40行 |
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
| **ガイド線** | ❌ フラッグのみ、描画なし |
| **解像度切り替え** | ❌ UI のみ、ハンドラが qDebug |
| **ファストプレビュー** | ❌ UI のみ、未接続 |
| **3D カメラ** | ❌ ViewportCamera が 2D のみ |
| **GridRenderer** | ❌ 全メソッドスタブ |
| **レイヤービュー: バウンディングボックス** | ❌ 未実装 |
| **レイヤービュー: edit/display mode** | ❌ 空メソッド |
| **ルーラー** | ❌ 未実装 |
| **マルチビュー** | ❌ 未実装 |
