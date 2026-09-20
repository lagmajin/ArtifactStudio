# M-COLLIDER-VP-1: 2D Collider Viewport Edit

**最終更新:** 2026-09-19

**状態:** Phase 0 実装完了。Coreの値DTOとレイヤーの一括適用入口を追加済み。ArtifactCoreターゲットはビルド確認済み。Viewport表示、入力、Undo接続、実機検証は未実装。

## 目的

Collision componentが有効な2Dレイヤーについて、物理シミュレーションのランタイム状態と分離したまま、ViewportでBoxおよびCircleのオフセット・寸法を直接編集できるようにする。Polygonは現在のシェイプ輪郭から導出される実効形状を比較表示するPreviewから開始する。

## 所有境界

| 領域 | 所有者 | 責務 |
| --- | --- | --- |
| 編集値DTO・形状解決 | ArtifactCore `Physics.Collider2DEdit` | Box/Circleの評価寸法、Polygon Preview状態。Box2Dオブジェクトを保持しない。 |
| 永続プロパティ・JSON | `ArtifactAbstractLayer` | `component.collision.*` の正規値と一括適用。 |
| 編集モード・ヒットテスト・HUD | `CompositionRenderController` | Collider Edit中だけの入力と表示。 |
| 描画 | `CompositionRenderOverlay` / 既存Diligent renderer | シアンのコライダー、元形状ゴースト、数値HUD。 |
| Undo | 既存 `MacroUndoCommand` / `SetLayerPropertyValueCommand` | 1ドラッグを1回のUndoへ畳む。 |

新規のQt signal/slot、QtCSS、QImage経由の描画、QPainter合成、物理エンジンや`ReactiveEvents`の変更は行わない。

## Phase 0 — 編集値境界（完了）

- `Collider2DEditState`、形状、ハンドル種別、評価済みBox/Circle幾何を追加。
- 既存の `component.collision.*` をDTOへ投影し、一度だけ物理同期する一括適用入口を追加。
- Polygonはソース頂点数とRigidBody用の最大8頂点Preview数だけを返し、頂点編集は行わない。

## Phase 1 — 表示とモード選択

- ComponentsのCollision詳細面から明示的にCollider Editを開始・終了する。
- Viewportは通常Transform frameと競合しないCollider専用オーバーレイを描画する。
- Boxは辺／角／中心、Circleは半径／中心を表示する。Auto Boundsは読み取り専用。
- Polygonはソース輪郭と実効簡略輪郭を表示するが、頂点ハンドルは表示しない。

## Phase 2 — ドラッグとUndo

- クリック、ドラッグ、Esc取消、確定を既存modal gizmo入力ルートへ接続する。
- 操作中はDTOだけを更新し、確定時に既存プロパティコマンドをMacroとして1回だけ積む。
- サイズ操作中は元形状ゴーストと `W/H` または `Radius`、`Offset X/Y` を表示する。
- シミュレーションの再構築は確定時に一度だけ行い、ドラッグ中に固定ステップworldを更新しない。

## Phase 3 — Polygon authoring（別判断）

- 独立Collider Pathの保存形式、JSON versioning、Undoシリアライズ、凸化／8頂点縮約、fallback可視化を設計レビュー後に追加する。
- 現在のshape layer輪郭の直接改変をCollider編集として扱わない。

## 受入基準

- Box/Circleが親Transformを変えずにローカルCollider値だけを更新する。
- 1回のドラッグが1回のUndo/Redoで完全に往復する。
- 無効化、Auto Bounds、Polygon Preview、ロック済みレイヤー、非2Dレイヤーでは編集開始しない。
- RigidBody/SoftBodyが有効な場合も、確定後に一度だけ同期される。
- D3D12/Vulkan共通の既存Diligent overlay APIのみを使う。

## 検証状況

ArtifactCoreターゲットのビルドは完了。Artifact全体ビルドは、既存の未関連変更（VST3/Localization）のコンパイルエラーと、環境側のWindows resource compiler失敗により未完了。ユニットテストと実機Viewport操作は未実行。
