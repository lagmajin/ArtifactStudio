# Milestone: Multi-Viewport Layout System

作成日: 2026-06-01
最終更新: 2026-08-15
親: `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` (Multi-view / Viewer)
関連: `MILESTONE_CAMERA_PROJECTION_2026-03-31.md` (Camera Layer Projection)

---

## 目的

After Effects の 4-up ビュー（Active Camera / Custom 1 / Custom 2 / Custom 3）に相当する
複数独立ビューポートのレイアウトを Viewer エリアに提供する。中央の Composition Viewer
を単一ビューから多ビューへ拡張し、orthographic（Top / Front / Left）カメラレイヤーを各
ペインに割り当てられるようにする。

---

## 既存の土台

- `ArtifactCameraLayer` — perspective / orthographic モード（M-CP-1）
- `ArtifactViewportCamera` — viewport カメラ状態管理
- `ArtifactCompositionRenderController` — 1 ビュー当該レンダリング制御
- `Artifact3DModelViewer` / `ArtifactSecondaryPreviewWindow` — 追加ビューインスタンス例
- `ArtifactMainWindow::addDockedWidget()` / タブグループ機構 — レイアウト側 API
- `docs/bugs/MULTI_VIEW_RENDER_CONCURRENCY_HYPOTHESES_2026-03-24.md` — 複数ビュー同時
  常駐のパフォーマンス課題整理済み

---

## 未着手要素

- Viewer エリアを 2x2 grid または 1+3（主ペイン + 3 サブペイン）にレイアウト切替
- 各ペインを任意の Camera Layer（perspective / orthographic）にバインド
- ペインごとの Zoom / Pan 状態が独立に保持される仕組み
- ペイン c の選択したカメラが、同期プレイバック時に Composition Renderer の
  authoritative viewport としても使われる経路
- ペインリサイズ時の`projection matrix` 更新（Phase 4 of M-CP-1 を統合）
- 複数ビュー同時更新における playhead 共有・イベント重畳最適化（performance hypothesis
  群を元に最初に単純実装、後に最適化）

---

## フェーズ

### Phase 1: レイアウト管理 API

- `ViewportLayoutManager` を新設（singleton）。レイアウト定義:
  - `Single` — 既存の 1 ペイン
  - `HorizontalSplit` — 左右 2 ペイン
  - `FourUp` — 2x2
  - `OnePlusThree` — 主ペイン + subs
- `ViewportAssignment` — 各ペインに紐づく Camera Layer ID を保持
- main window の中央領域タブにViewportLayout ドッカブルを登録

### Phase 2: ペイン描画の分離

- `ArtifactCompositionRenderController` をペインごとに instantiate
- 各ペインが `currentComposition` + `viewportCameraRef` を持ち、
  `CompositionCreatedEvent` / `FrameChangedEvent` を各 controller へ multicast
- Zoom / Pan は `ArtifactViewportCameraState` としてペインごとに保存

### Phase 3: Camera Layer との統合

- Camera Layer の orthographic パラメータ（width/height / pivot）を反映
- ペインからのドラッグ操作が選択カメラレイヤーの transform へ反映される
  （AE の Custom View 相当で ortho size のドラッグ操作を許可）
- ビューポート切替時の event を EventBus に発行し、timeline / inspector の同期更新

### Phase 4: Activity / Performance 最適化

- `MULTI_VIEW_RENDER_CONCURRENCY_HYPOTHESES` の指摘に対し、
  - 非アクティブペインは低Hzポーリングに切り下げる
  - active ペインのみ immediate render request を受け付ける
  - event 重畳検出で同一フレーム内の重い再評価を抑制
- デバッグ UI でペインごとの render time を表示するモード追加

---

## 検証条件

- View → 「4-Up Layout」実行で中央が 2x2 grid に切り替わる
- 各ペインに異なる Camera Layer を割り当てられる（Perspective / Top / Front / Left）
- 各ペインで独立にズーム・パン可能で、移動しても他ペイン座標は影響を受けない
- 4-Up 状態で再生したとき、4 ペインとも各フレームが更新される
- パフォーマンス: 4 ペイン稼働時の playhead 移動時間が Single の 2 倍未満

---

## UI 変更点

- View メニュー: `Use Single View / 2-Up / 4-Up / 1+3 Sub-views` を追加
- 各ペイン左上に小さなカメラ名表示ドロップダウン
- ペイン分割境界にドラッグハンドル

---

## 関連ファイル（影響）

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Layer/ArtifactCameraLayer.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Widgets/ArtifactViewMenu.cppm`

---

## 見積

- Phase 1: 10–14h
- Phase 2: 14–18h
- Phase 3: 10–14h
- Phase 4: 8–12h

合計: 42–58h

---

## 2026-07-25 現状確認

静的確認では、`ArtifactCompositionEditor` に Single / TwoUp / FourUp のレイアウト状態、最大4ペインの `PaneState`、ペインごとの view / render controller、active pane、Splitter 配置、active pane 表示が実装されている。したがってレイアウト切替と複数 controller の基礎は存在する。

一方、仕様の OnePlusThree、各ペインへの任意 Camera Layer 割当、ペインごとの独立 camera／zoom／pan の完全な永続化、authoritative viewport 切替、4ペイン再生時の性能制御・render time diagnostics は確認できない。共通イベントの multicast と `IDeviceContext` 競合回避も、本マイルストーンの完了条件としては未検証。よって「Phase 1〜2 の基礎実装は存在、Camera統合と性能／永続化は未完了」と判定する。

確認範囲: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`。ビルド・実機操作による動作確認は未実施。

## Update 2026-08-15

- `ArtifactCompositionEditor::Impl` に `Single` / `TwoUp` / `FourUp` の3レイアウト、最大4つの `PaneState`、ペインごとの `CompositionViewport` と `CompositionRenderController`、active pane、水平／垂直 splitter が実装されている。レイアウト切替の toolbar action／shortcut と、ペインごとの resize callback も確認できる。
- したがって旧仕様の `HorizontalSplit` という名称は現行コードでは `TwoUp` に相当し、`OnePlusThree` はまだ enum・レイアウト計算に存在しない。現行の実装済み範囲は Single / 2-Up / 4-Up である。
- 各 controller が分離され、orientation／zoom／pan の状態を個別に保持できる基盤はあるが、現行 `PaneState` 自体には Camera Layer ID の assignment フィールドがない。各ペインへ任意の Perspective / Top / Front / Left Camera Layer を割り当てる UI／保存経路は確認できない。
- `activePane` と active controller の切替、overlay indicator、共通 composition の更新経路は存在する。一方、authoritative viewport の明示的な切替契約、ペインごとの設定永続化、4ペイン時の重複評価抑制・低Hz化・render time diagnostics は未確認である。
- よって現状は `Phase 1〜2 layout/controller foundation implemented / Phase 3 camera assignment and authoritative viewport pending / Phase 4 performance and persistence pending` と判定する。4ペイン性能の「Single の2倍未満」は静的コードからは証明できない。
