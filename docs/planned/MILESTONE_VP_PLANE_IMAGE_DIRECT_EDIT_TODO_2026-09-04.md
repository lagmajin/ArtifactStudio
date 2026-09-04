# MILESTONE: VP Plane/Image Layer Direct Edit TODO (2026-09-04)

**最終更新:** 2026-09-04
**ステータス:** Draft (未着手)
**マイルストーン ID:** M-VP-F
**統合先:** `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`

**目的:** 「平面 (Solid) / 画像 (Image) レイヤーの VP 操作が十分か？」への
回答として、**未着手の個別機能** を TODO として明文化。着手優先度・既存経路との
依存・確認方法を以下にまとめる。

**状態マップ:** `docs/analysis/GAP_AE_NUKE_2026-08-01.md` のレイヤー種別スコア
(画像 🟢 95%、平面 🟢 95%、ともに十分判定) と、`docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`
の実装状況表。

---

## 着手候補（低 → 中リスク）

### F1. Composition VP での Track Matte 直接ドラッグ

- **内容:** 現在 `ArtifactLayerPanelWidget` (Alt+Drag) のみで実装されている
  Track Matte リンクを、Composition Editor 上の layer 上で直接ドラッグで設定
- **既存経路:**
  - `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm` の
    Alt+Drag 実装 (Track Matte badge 表示と循環参照拒否確認済み)
  - `Artifact/src/Service/ArtifactProjectService.cppm` の matte 関連 API
- **着手箇所:** `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
  に `mousePressEvent` / `mouseMoveEvent` で Alt+Drag 検出を追加
- **依存:** Alt+LMB orbit (`isAltOrbiting_`) との modifier 競合解決。
  `Mask mode` / `Modal.Mask` context との routing 競合解決
- **制約:** 既存 Alt+LMB orbit との modifier 競合回避、`maskNavigationLocked` 経路尊重
- **確認:** Composition 上で 2 レイヤー間に Alt+Drag で matte 線が引け、
  Timeline Panel 上の badge と一致
- **優先度:** 中

### F2. Layer Solo View の画像 / 平面レイヤー編集拡張

- **内容:** `ArtifactLayerEditorWidget` (旧 `ArtifactRenderLayerWidgetv2`) に
  画像 / 平面レイヤー専用の編集ハンドル (Rect 角丸 / Star 内径に相当) を追加
- **既存経路:**
  - `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm` の
    `ShapeParameterController` (Rect 角丸 / Star 内径ハンドル実装済み)
  - `MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md` Phase 1 実装完了、
    Phase 3 以降未未
- **着手箇所:** `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm` に
  `ImageParameterController` / `Solid2DParameterController` を追加。
  Shape と並列の編集コントローラ構成
- **依存:** `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` の
  `ShapeEditSession` 抽出計画と相互作用
- **制約:**
  - AGENTS.md の「ArtifactPropertyWidget の通常のレイヤープロパティ表示では、
    レイヤー固有の主要編集項目を優先し、Components / Collision / Layout /
    Cloner / Crowd / Particle Emitter / Fluid などは露出させない」
  - Shape / Image / Solid2D で**並列**の設計にし、PropertyWidget 側で
    `getLayerPropertyGroups()` の優先順位を維持
- **確認:** Layer Solo View で画像 / 平面レイヤーを選択すると、レイヤー固有の
  主要編集項目がハンドルとして表示される
- **優先度:** 中（Phase 1 実装済み Phase 3-5 未着手）

### F3. DisplayFilter 拡張 (Zebra / BufferVisualization)

- **内容:** `ViewportChannelDisplayMode` enum に `Zebra` / `BufferVisualization` を追加
- **既存経路:**
  - `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx:62-84` の
    `ViewportChannelDisplayMode` (22 値実装済み)
  - `MILESTONE_VIEWPORT_ENHANCEMENT_PROPOSAL_2026-07-08.md` の 12.4 A-1 DisplayFilter
- **着手箇所:** `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
  に enum 値追加。`ArtifactCompositionRenderController.cppm` の
  `setViewportChannelDisplayMode` 内部で SRV 選択分岐を追加
- **依存:** `ArtifactIRenderer` の既存 `setChannelEnabled` 経路を尊重
- **制約:** 既存 enum 値の ID を変えないこと（serialization / project 互換性）
- **確認:** 補助チャンネル選択メニューに Zebra / BufferVisualization が出て、
  クリッピング可視化 / GPU buffer 直接表示が動作
- **優先度:** 低（既存 22 値で「画像 / 平面レイヤーの表示モード」は十分カバー済み）

### F4. Composition 内 Wipe 比較 (B-1)

- **内容:** `CompositionCompareMode` に `WipeHorizontal` / `WipeVertical` / `WipeFree` を追加
- **既存経路:**
  - `ArtifactCompositionRenderController.cppm:8343` 付近の
    `finalizeGpuRenderToViewport`
  - `setReferenceOverlayImage` / `isReferencePinned` (159-163)
- **着手箇所:** `ArtifactCompositionRenderController.cppm` の `CompositionCompareMode`
  enum 拡張と `drawComparisonWipe()` 追加
- **依存:** 既存 `CompositionCompareMode` (Off/A/B/Diff) と並列、RenderScheduler / DX12
  経路を尊重
- **制約:** 「RenderScheduler / DX12 パスはシビア扱い」の AGENTS.md ルールに従い、
  推測で広く触らず変更範囲を最小化
- **確認:** 比較モードで Wipe を指定すると、2 つの frame が wipe 境界で分割されて表示
- **優先度:** 中（VP レビュー支援機能として価値高）

### F5. TransformGizmo BBox 上辺サイズ表示（Phase 4 撤回内容）

- **撤回内容:** `drawBBoxSizeLabel()` を TransformGizmo に追加し、選択中
  レイヤーの `WxH` を BBox 上辺中央に表示する計画を、LF/CRLF 改行問題と
  「既存挙動を不用意に変えない」の AGENTS.md ルールで撤回
- **再着手時の前提:**
  - 既存 `TransformGizmo.cppm` 全体を LF 統一してから着手
  - `isTextLayer` のみ除外、 Shape / 3D は `shape.size` の canonical geometry
    ではないので、ラベル位置 / 色を調整
- **着手箇所:** `Artifact/src/Widgets/Render/TransformGizmo.cppm` の `showBBox` ブロック
- **確認:** Scale / None モード時に BBox 上辺に `"WxH"` ラベルが出る
- **優先度:** 低（情報追加で Phase 1-2 のノイズ削減と方向性が異なる）

## 着手禁止 / 要ユーザー判断

- **AGENTS.md の PImpl ルール**: 新規コントローラクラスは `Impl*` 明示所有。
  `std::shared_ptr` / `std::unique_ptr<Impl>` は DLL 境界・ABI 不整合リスクから避ける
- **AGENTS.md の独自コンテナ優先**: ImageParameter データは独自
  `NamedVector` / `ArtifactArray` / `HashMap` 優先。`std::vector` /
  `std::unordered_map` への置換は最小限
- **新規 signal/slot 接続禁止**: track matte ドラッグ通知は既存の
  ProjectService 経由、または Composition Editor 内コールバックで処理
- **QPainter / QImage / QColorDialog / QtCSS 禁止**: Layer Solo View の
  編集ハンドル描画は既存 IRenderer プリミティブのみ使用

## 確認手順

1. **F1 (Track Matte VP ドラッグ)** から着手することを推奨。
   LayerPanelWidget 既存経路を尊重し、最小拡張で始め
3. **F4 (Wipe 比較)** は M-VP-2 pane manager 移行の後
4. **F2-F5** は AGENTS.md の「RenderScheduler / DX12 シビア扱い」遵守で慎重に進める

## 関連文書

- `docs/analysis/GAP_AE_NUKE_2026-08-01.md` (AE/NUKE 比較スコア)
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` (統合監査)
- `docs/planned/MILESTONE_LAYER_SOLO_VIEW_DILIGENT_2026-03-26.md` (Layer Solo View)
- `docs/planned/MILESTONE_VIEWPORT_ENHANCEMENT_PROPOSAL_2026-07-08.md` (A-1 / B-1)
- `docs/planned/MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` (モジュール分割計画)
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactLayerEditorWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cppm`