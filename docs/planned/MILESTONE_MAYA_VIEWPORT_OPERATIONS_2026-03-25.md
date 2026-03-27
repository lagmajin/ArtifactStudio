# Milestone: Maya-Like Viewport Operations for AE-Style App (2026-03-25)

**Status:** Not Started
**Goal:** After Effects風アプリに「Maya/Blender ライクなビューポート操作」を導入。
構造（レイヤー/DAG/ノード）は変更せず、入力と操作体験のみ改善。

---

## Constraints

- レイヤー構造は変更しない
- ノード/DAGは導入しない
- 既存の ArtifactIRenderer / PrimitiveRenderer2D を利用
- 既存の TransformGizmo / ToolManager を拡張
- Diligent の描画関数を使う

---

## Existing Infrastructure

| コンポーネント | 状態 | 場所 |
|---|---|---|
| Pan/Zoom (wheel, MMB, Space) | ✅ 完成 | `ArtifactCompositionEditor.cppm:127-257` |
| ViewportTransformer | ✅ 完成 | `ViewportTransformer.ixx:52-106` |
| TransformGizmo (Move/Scale/Rotate) | ✅ 完成 (2D) | `TransformGizmo.cppm:112-346` |
| ToolManager + ToolType enum | ⚠️ フレームワークのみ | `ArtifactToolManager.ixx:26-40` |
| ToolMode enum (ArtifactCore) | ⚠️ 未接続 | `ToolMode.ixx:6` |
| GizmoMode ix | ❌ 空スタブ | `GizmoMode.ixx` |
| ツールショートカット (V/H/Z/W) | ✅ 部分実装 | `ArtifactToolBar.cppm:128-182` |
| グリッドスナップ (メニュー) | ⚠️ UIのみ | `ArtifactViewMenu.cppm:108-110` |
| Snap helper | ⚠️ 限定的 | `ArtifactCompositionRenderWidget.cppm:192` |
| ArtifactViewportCamera | ❌ 未使用 | `ArtifactViewportCamera.ixx:9-50` |
| ViewportOperator | ❌ 空スタブ | ArtifactCore |

---

## Milestone 1: Viewport Navigation（最優先）

**目的:** Alt + マウス操作で直感的なビューナビゲーション

### 要件
- Alt + LMB: ビュー回転（2D では無効 or 30°スナップ回転）
- Alt + MMB: パン（現状の MMB パンを Alt+MMB に変更 or 追加）
- Alt + RMB: ズーム（カーソル位置基準）
- ズームはカーソル位置を基準点にする（重要）
- パン・ズームはスムーズ（フレームレート非依存）

### 既存の実装
- `CompositionViewport::wheelEvent()` — Alt/Ctrl+wheel = zoom ✅
- `CompositionViewport::mousePressEvent()` — MMB = pan ✅
- `renderer_->panBy(dx, dy)` — 完成 ✅
- `renderer_->zoomAroundViewportPoint()` — 完成 ✅
- `renderer_->setZoom()`, `renderer_->getZoom()` — 実装済み ✅

### 追加が必要なもの
| タスク | 場所 | 見積 |
|---|---|---|
| Alt+LMB パン（現状 Space+LMB の代替） | `CompositionViewport::mousePressEvent` | 30min |
| Alt+RMB ズーム（ドラッグでズーム量変更） | `CompositionViewport` 新規ハンドリング | 2h |
| マウスデルタ → ズーム変換 | `wheelEvent` を参考に新規実装 | 1h |
| カーソル位置基準ズームの確認・修正 | 既存 `zoomAroundViewportPoint` を使用 | 30min |
| フレーム非依存のスムーズ移動 | delta をフレーム時間で正規化 | 1h |
| 回転（2D オプション） | `renderer_->setRotation()` が未実装なら追加 | 2h |

### Acceptance Criteria
- Alt+MMB でストレスなくパンできる
- Alt+RMB でカーソル位置を基準にズームできる
- 60fps でも 30fps でも同じ速度で動く

---

## Milestone 2: Transform Gizmo（W/E/R）

**目的:** W/E/R でギズモを切り替え、直接ドラッグで変形

### 要件
- W: Move（現状のデフォルト）
- E: Rotate
- R: Scale
- 選択レイヤーにギズモ表示
- 直接ドラッグで変形

### 既存の実装
- `TransformGizmo` — Move, 8-point Scale, Rotate ハンドル全て実装済み ✅
- `HandleType` enum — Move, Scale_TL/TR/BL/BR/T/B/L/R, Rotate ✅
- `hitTest()` — 全ハンドルの当たり判定 ✅
- `handleMouseMove()` — Move/Scale/Rotate のドラッグ処理 ✅
- `ToolManager` — `ToolType` enum + `setActiveTool()` ✅
- `W` ショートカット — Rotation ツール ✅

### 追加が必要なもの
| タスク | 場所 | 見積 |
|---|---|---|
| `E` キー → Scale ツールにバインド | `ArtifactToolBar.cppm` / `keyPressEvent` | 30min |
| `R` キー → Rotate ツールにバインド | `ArtifactToolBar.cppm` / `keyPressEvent` | 30min |
| ギズモの表示/非表示を ToolType に応じて切り替え | `TransformGizmo::draw()` | 1h |
| ハンドルの可視性を ToolType で制御 | `TransformGizmo::draw()` | 1h |
| GizmoMode enum の実装 or 廃止 | `GizmoMode.ixx` | 30min |
| Scale ツール時に Move ハンドルを隠す | `TransformGizmo::draw()` | 30min |
| Rotate ツール時に Scale ハンドルを隠す | `TransformGizmo::draw()` | 30min |

### Acceptance Criteria
- W で Move ギズモ表示、ドラッグで移動
- E で Scale ギズモ表示、ハンドルドラッグでリサイズ
- R で Rotate ギズモ表示、ドラッグで回転
- ショートカットで即座に切り替わる

---

## Milestone 3: Pivot Mode

**目的:** ピボットポイントのみを編集する専用モード

### 要件
- Pivot 編集専用モード（Y キー推奨）
- Pivot のみ移動可能、Transform は変えない
- 表示: 小さい十字 or 円
- スナップ対応（グリッド or 中心）

### 既存の実装
- `TransformGizmo` に `anchor` ハンドルが既にある ✅
- `anchorX_`, `anchorY_` が `AnimatableTransform3D` に存在 ✅
- `setAnchorPoint()` API が存在 ✅

### 追加が必要なもの
| タスク | 場所 | 見積 |
|---|---|---|
| Y キー → Pivot モードにバインド | `keyPressEvent` | 30min |
| Pivot モード時のギズモ表示制御 | `TransformGizmo::draw()` | 1h |
| anchor ハンドルのドラッグ処理確認 | 既存コード確認 + 修正 | 1h |
| グリッドスナップ適用 | `Snap()` 関数を anchor 移動に適用 | 2h |
| 中心スナップ | レイヤー中心にスナップ | 1h |

### Acceptance Criteria
- Y キーで Pivot モードに入る/出る
- Pivot のみドラッグ移動可能
- 位置/スケール/回転は変わらない
- Shift でグリッドスナップ

---

## Milestone 4: Direct Manipulation

**目的:** ギズモを使わずオブジェクトを直接ドラッグで移動

### 要件
- ギズモ外クリック → 選択レイヤー全体を移動
- delta をワールド座標へ変換
- 数値 UI 不要

### 既存の実装
- `TransformGizmo::handleMousePress()` — body 領域クリックで Move 検出 ✅
- `TransformGizmo::handleMouseMove()` — Move ドラッグ処理 ✅
- `handleMousePress()` — レイヤー当たり判定 + cyclic selection ✅

### 追加が必要なもの
| タスク | 場所 | 見積 |
|---|---|---|
| ギズモ外クリック時のレイヤードラッグ移動 | `handleMousePress` / `handleMouseMove` | 2h |
| viewport delta → canvas 座標変換 | `renderer_->viewportToCanvas()` を使用 | 1h |
| ドラッグ中のビジュアルフィードバック | オパシティ変更 or ゴースト表示 | 1h |
| マルチレイヤー選択時の同時移動 | selection manager から全選択レイヤー取得 | 2h |

### Acceptance Criteria
- レイヤーをクリック＆ドラッグで直接移動できる
- マウスの動きに正確に追従する
- 複数選択時は同時に移動する

---

## Milestone 5: Snap System

**目的:** Shift でスナップ ON、グリッド/中心/境界にスナップ

### 要件
- Shift でスナップ有効化
- 対象: グリッド、レイヤー中心、境界ボックス

### 既存の実装
- `snapValue()` — 簡易スナップ関数 ✅ (限定的)
- スナップメニュー UI (Grid/Guide) — 存在するが未接続 ⚠️

### 追加が必要なもの
| タスク | 場所 | 見積 |
|---|---|---|
| `SnapManager` or `snap()` ユーティリティ | 新規: `ArtifactCore/include/Transform/Snap.ixx` | 2h |
| グリッドスナップ | グリッドサイズを取得して丸め | 1h |
| レイヤー中心スナップ | 全レイヤーの center を収集して最近傍検索 | 2h |
| 境界ボックススナップ | bbox の辺/角にスナップ | 2h |
| Shift 修飾子のハンドリング | `handleMouseMove` で Shift 検出 | 30min |
| スナップ距離の設定 | 設定UI or 定数 | 30min |
| スナップ時のビジュアルフィードバック | スナップライン描画 | 2h |
| View メニューのスナップ項目を接続 | `ArtifactViewMenu.cppm` | 1h |

### Acceptance Criteria
- Shift を押しながらドラッグでグリッドにスナップ
- 近くのレイヤー中心/境界にスナップ
- スナップラインが視覚的に表示される
- View メニューのスナップ設定が動作する

---

## Milestone 6: Mini Channel Box

**目的:** 選択レイヤーの主要値をビューポート上で直接表示・編集

### 要件
- 選択レイヤーの Position / Scale / Rotation / Opacity 表示
- ドラッグで値変更（SpinBox的）
- 軽量オーバーレイ UI

### 既存の実装
- `ArtifactInspectorWidget` — プロパティ表示 ✅ (別パネル)
- `ArtifactPropertyWidget` — プロパティ編集 ✅ (別パネル)

### 追加が必要なもの
| タスク | 場所 | 見積 |
|---|---|---|
| MiniChannelBox クラス | 新規: `Artifact/include/Widgets/MiniChannelBox.ixx` | 3h |
| コンポジションビューポートへのオーバーレイ描画 | `CompositionViewport::paintEvent` or renderer overlay | 2h |
| 選択レイヤーの値取得 | `layer->transform3D()` の各チャンネル | 1h |
| ドラッグで値変更 (SpinBox) | `mouseMoveEvent` で delta → 値変換 | 3h |
| 値変更 → レイヤー適用 | `transform3D().setPosition()` 等 | 1h |
| キーボード直接入力 (オプション) | クリックで QLineEdit 表示 | 2h |

### Acceptance Criteria
- レイヤー選択時にビューポート右上に Position/Scale/Rotation/Opacity が表示される
- 各値をドラッグで変更できる
- Inspector パネルと値が同期する

---

## Deliverables

| ファイル | 内容 |
|---|---|
| `CompositionViewport.cppm` (拡張) | Alt 操作、イベント処理 |
| `TransformGizmo.cppm` (拡張) | W/E/R 表示制御 |
| `ToolManager.cpp` (拡張) | ツール切替とショートカット |
| `Snap.ixx` (新規) | スナップユーティリティ |
| `MiniChannelBox.ixx` (新規) | チャンネルボックスオーバーレイ |

---

## Recommended Order

1. **M1 Viewport Navigation** — 基盤、他の操作の前提
2. **M2 Transform Gizmo** — 既存コード拡張、即効性高い
3. **M4 Direct Manipulation** — ギズモ拡張、UX向上大
4. **M3 Pivot Mode** — 短時間で完了
5. **M5 Snap System** — 精度向上
6. **M6 Mini Channel Box** — 独立系、いつでも

---

## Related Files

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | 72-418 | CompositionViewport (入力処理) |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 770-804 | panBy, zoomInAt, zoomOutAt, zoomFit |
| `Artifact/src/Widgets/Render/TransformGizmo.cppm` | 112-346 | ギズモ描画・ヒットテスト・ドラッグ |
| `Artifact/include/Widgets/Render/TransformGizmo.ixx` | 17-60 | HandleType, TransformGizmo 宣言 |
| `Artifact/include/Tool/ArtifactToolManager.ixx` | 10-40 | ToolType, ArtifactToolManager |
| `ArtifactCore/include/Tool/ToolMode.ixx` | 6 | ToolMode enum (未接続) |
| `ArtifactCore/include/Transform/ViewportTransformer.ixx` | 52-106 | ViewportTransformer (pan/zoom/NDC) |
| `Artifact/include/Render/ArtifactIRenderer.ixx` | 69-81 | レンダラーのビューメソッド |
| `Artifact/src/Widgets/ArtifactToolBar.cppm` | 128-182 | ツールショートカット定義 |
| `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` | 108-118 | スナップメニュー (未接続) |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` | 192 | snapValue ヘルパー |
| `ArtifactCore/include/Transform/AnimatableTransform3D.ixx` | - | Transform3D (position, scale, rotation, anchor) |
