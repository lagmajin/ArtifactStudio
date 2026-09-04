# MILESTONE: Viewport Navigation Contract TODO (2026-09-04)

**最終更新:** 2026-09-04
**ステータス:** Draft (未着手)
**マイルストーン ID:** M-VP-9-TODO
**統合先:** `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` (M-VP-9 / M-VP-2)

**目的:** M-VP-9「Navigation Contract」のうち、未着手 / 部分実装項目を TODO として明文化。
着手優先度・既存経路との依存・確認方法を以下にまとめる。

**状態マップ:** `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md`

---

## 着手候補（低 → 中リスク）

### T1. Navigation cross 描画の最小実装

- **内容:** `previewOrbitMode_` が ON の間だけ、画面中央に薄い + マークを描画
- **既存経路:**
  - `ArtifactCompositionEditor.cppm:9220-9272` の `setPreviewOrbitMode`
  - `ArtifactCompositionRenderOverlay.cppm` の `drawOverlayPanel` / `drawText`
- **着手箇所:** `ArtifactCompositionRenderOverlay.cppm` に新規 `drawNavigationCross()`
- **依存:** Editor → Overlay への状態渡し経路が必要（pane manager 移行とは分離して、
  `RenderController` 側に `previewOrbitMode` フラグ参照を追加）
- **制約:** 既存 IRenderer のみ使用、QPainter / QImage / QtCSS 禁止、新規 signal/slot 禁止
- **確認:** Editor toolbar で Preview Orbit を ON にした後、画面中央に cross が出る
- **優先度:** 中（M-VP-9 の Phase 1 着手点だが、Pivot / Orbit source selector に比べて
  視覚的効果が分かりやすい）

### T2. Preview-orbit snapshot に navigation session を含める

- **内容:** `setPreviewOrbitMode` の `PreviewOrbitSnapshot` に
  `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを含める
- **既存経路:** `ArtifactCompositionEditor.cppm:9238-9247` の `PreviewOrbitSnapshot` 構造
- **依存:** なし（同一ファイル内）
- **制約:** Editor 側 Impl にフラグ保存、`PreviewOrbitSnapshot` 構造拡張、
  `setPreviewOrbitMode(false)` 復元経路でフラグも復元
- **確認:** Preview Orbit ON 中に Alt+LMB orbit → OFF 時に orbit 状態が完全復元
- **優先度:** 中（M-VP-9 「preview-only view state と camera layer state の厳密分離」
  の核）

### T3. Active viewport 細い枠表示

- **内容:** pane manager 移行と分離した最小実装として、`activeViewport()` の
  `paneState` から枠 rect を取得し、Overlay 側で owner-draw
- **既存経路:**
  - `ArtifactCompositionEditor.cppm:7983` の `activeViewport()`
  - `ArtifactCompositionEditor.cppm:8184-...` の `forEachActiveViewport()`
  - `ArtifactCompositionRenderOverlay.cppm` の `drawEmphasizedRect` / `drawSolidLine`
- **依存:** M-VP-2 pane manager 移行（未着手）。PaneState.rect が
  pane manager 移行で得られるまで、QSplitter ベースで `splitterSizes` から rect 計算
- **制約:** QSplitter ベースの暫定実装は暫定。pane manager 移行時に置換必要
- **優先度:** 低（M-VP-2 移行の付帯作業）

### T4. Pivot source / Orbit source selector

- **内容:** `Object / Selection / 3D Cursor / Individual` の pivot 切替 UI と、
  orbit / pan / dolly 開始時の source 決定
- **既存経路:**
  - `ArtifactCompositionEditor.cppm:7983` の `activeViewport()`
  - `ArtifactCore::Animation::Transform3D` の `Transform3D` 構造
- **依存:** M-VP-9 の既存 Work Cursor 経路に接続。Selection center は
  `existing layer selection manager` を流用
- **制約:** ショートカットは Blender 互換（`G/R/S` + 軸拘束 `X/Y/Z`）で、
  既存 `ShortcutBindings` を尊重
- **優先度:** 中（Phase 4-5。M-VP-9 仕様の主要未実装）

### T5. Surface snap / depth picking

- **内容:** orbit / dolly 中の surface hit、cursor placement の depth 解決
- **既存経路:**
  - `ArtifactCompositionRenderController.cppm` の `createPickingRay`
  - `ArtifactCore::Render` の既存 hit test API
- **依存:** M-VP-9 の cursor placement で `3D Cursor` を surface に置く経路と統合
- **制約:** 「depth ambiguity 解決」と「origin や極端な depth へ飛ばない」を
  M-VP-9 仕様 2026-07-04 の検証シナリオ 8 番で担保
- **優先度:** 中（Phase 5。M-VP-9 仕様の最終段）

## 着手禁止 / 要ユーザー判断

- **ReactiveEvents** への接続: AGENTS.md の「**ReactiveEvents はユーザーから
  明示的に依頼されるまで一切触らない**」に従い、新規 event-driven 経路は追加禁止
- **ソフトレンダラー新機能追加**: AGENTS.md の「**ソフトレンダラー新機能追加は
  ユーザー指示待ち**」に従い、navigation cross / pivot source selector は
  GPU  経路優先
- **新規 signal/slot 接続**: 既存の event path / service を再利用

## 確認手順

1. **T1 (navigation cross)** から着手することを推奨。リスク低・視覚効果即時
2. **T2 (snapshot 拡張)** は T1 と並列着手可能。同一ファイル内
3. **T3 (active viewport 枠)** は M-VP-2 pane manager 移行の後
4. **T4-T5** は M-VP-9 Phase 3-5 全体として計画再立案

## 関連文書

- `docs/planned/MILESTONE_VIEWPORT_INTERACTION_NAVIGATION_CURSOR_2026-07-04.md` (M-VP-9)
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` (統合監査)
- `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md` (状態マップ)
- `docs/planned/IMPLEMENTATION_PLAN_VIEWPORT_PANE_MANAGER_2026-06-28.md` (M-VP-2)
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`