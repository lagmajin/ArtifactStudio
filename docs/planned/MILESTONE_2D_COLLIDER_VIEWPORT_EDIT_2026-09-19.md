# M-COLLIDER-VP-1: 2D Collider Viewport Edit

**最終更新:** 2026-10-02

**状態:** Phase 0 実装完了。Coreの値DTOとレイヤーの一括適用入口に加え、Layer Editorの読み取り専用輪郭プレビューを確認。専用編集モード、ハンドル、入力、Undo接続、実機検証は未完了。

## 2026-10-02 現行コード再監査

- `drawLayerEditorColliderOverlay()` は Collision enabled のレイヤーに Box／Circle／Polygon の輪郭を描く。呼び出しは Layer Editor の通常 Edit 面で、専用 Collider Edit モードではない。
- Box／Circle のハンドルや寸法 HUD、入力・ドラッグ・Undo は確認できない。Polygon は `collisionOutlineLocalPoints()` を輪郭表示するだけで頂点編集はしない。
- よって既存プレビューを専用 viewport editing の完了として数えず、Phase 1 の「表示」部分のみ実装済み、明示的なモード選択と編集 affordance は pending とする。

判定: **値 DTO／一括適用と Layer Editor 輪郭表示はコード上確認済み。Collider の直接編集は未実装。**

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

## 2026-10-02 — Collider handle geometry foundation

- Core DTOにBox/Circle各handleのローカル位置取得、許容距離付きhit test、Offset／Box edge・corner／Circle radiusのdelta適用を追加した。
- Boxはドラッグした側だけを動かし、反対側を固定する。辺が反対側を越えた場合は幅／高さ0で止める。Auto BoundsとPolygonはhit test／直接編集対象外のまま。
- 無効な状態、非有限pointer delta、float範囲を超える半径／寸法は状態へ書き込まない。
- これはUIとUndoから独立した幾何計算基盤である。Layer Editor／Composition Viewportへのハンドル描画・入力接続、ドラッグ中のpreview transaction、確定時一回の物理同期とUndoは未実装。Phase 1／2完了とは数えない。

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
