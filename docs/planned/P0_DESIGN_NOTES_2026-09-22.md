# P0 設計メモ — VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22 着手前調査

**最終更新:** 2026-09-22

**ステータス:** 着手前調査のみ。コード変更なし。
対象は `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` の P0-1〜P0-4。
実装はユーザー指示待ち。本文書は静的照合結果と、設計上の接続点・注意点の整理。

## 0. 共通サマリ

- P0 4 項目すべて「既存 API への接続点が見つかる」が、**P0-3 のみ既存 ProgressiveRenderer
  は呼び出し側（CompositionRenderController）から未接続** であり、P0-3 の
  「ProgressiveRenderer を再利用する」記述は現状と一致しない。要再設計。
- 新規 `ShortcutId` は 4 項目合わせて最大 4 件増える（`ViewBoxZoom`,
  `ViewTumblePivotUnderCursor`, `ViewInteractiveRenderRegion`,
  `ViewToggleLayerTypeFilter` など）。`ShortcutId::Count` を更新する必要あり。
- すべて「ビューポート操作」系のため、`ShortcutBindings` のローカルコンテキスト
  （既存 `ViewZoomIn` / `ViewFitToScreen` / `CompositionViewportMoveGizmo` 等と
  並列）に登録する。固定キーのハードコードは禁止（AGENTS.md 遵守）。

## 1. P0-1 Box zoom / Box crop — 既存 API だけで成立可能

### 既存 API（接続点）

- マーキー入力: `CompositionRenderController::Impl` に
  `rubberBandStartViewportPos_` / `rubberBandCurrentViewportPos_` 、
  `isRubberBandSelecting_` / `isLassoSelecting_` 、
  `selectionMode_`（`SelectionMode::Replace / Add / Toggle`）、
  `lassoViewportPoints_`（QVector<QPointF>）、
  閾値 `6.0f / zoom`（小クリック jitter キャンセル）。
  `viewportRectToCanvasRect(renderer, start, end)` で viewport→canvas 変換あり。
  `rubberBandCanvasRect() const` で正規化済み canvas rect を取得できる。
- ズーム補間 API: `zoomAtFactor(viewportPos, factor)`（既存 1.1 倍刻み）、
  `zoomInAt / zoomOutAt(viewportPos)`、`zoomFit / zoomFitSelection /
  zoomFitVisible / zoomFitWorkArea / zoom100()`。
- 履歴: `pushViewHistory()`（viewUndoStack_ 最大 32）、`undoView / redoView`、
  `captureViewState()`（pan + zoom + orientation を持つ `ViewState`）。
- オーバーレイ描画: `drawSolidRect(x, y, w, h, color, opacity)`、
  `drawTaggedRectOutline(renderer, rect, color, showSelectionRect)` が既存。
  マーキー選択の枠描画は 45799 行付近で実装済み（半透明 + アウトライン）。

### 設計メモ

- ハンドラ追加: `handleMousePress / handleMouseMove / handleMouseRelease`
  に「BoxZoom 修飾（修飾キー: 例 Ctrl+Alt、ShortcutBindings ローカル）」を
  追加する。既存 `selectionModeFromModifiers(event->modifiers())` の
  マージンを避け、**独立した state machine**（`isBoxZooming_`、
  `boxZoomStartViewportPos_`、`boxZoomCurrentViewportPos_`）を持つ。
  既存 `isRubberBandSelecting_` とは排他。
- 確定処理: `viewportRectToCanvasRect` で canvas rect を取得し、
  `aspect = rect.w/rect.h`、viewport アスペクトと比較。
  - 矩形内側 → `zoomFit*` 系に内側 rect を渡す新規 API
    （`zoomFitToCanvasRect(QRectF canvasRect)` を追加）か、
    `renderer_->setZoom` / `setPan` を直接書き換える。
  - 矩形外側 → `zoomAtFactor(rect.center(), 0.9f)` のような縮小を
    rect 中心を pivot に実行。
  - Crop（画面窓保持）は分析側記述に揃えるなら pan/zoom をそのままにし、
    矩形外側をマスク描画だけする別モード。今回は zoom 動作のみで十分。
- View 履歴: `pushViewHistory()` を確定直前に呼ぶ。
- オーバーレイ: 既存 `drawTaggedRectOutline` を再利用し、
  `LineDebugKind::SelectionRect` の表示可否に従う。
- ショートカット: 既存 `ViewZoomIn / Out / FitToScreen / DefaultZoom`
  と並列に新規 `ShortcutId::ViewBoxZoom` を追加し、`ShortcutBindings::resetToDefaults()`
  に既定キー（例: `Shift+Space` などのローカル）を登録。
  Houdini の Ctrl+Alt+Box とは修飾セットが異なるが、ユーザー設定で上書き可能とする。
- ホットパス影響: ドラッグ中の overlay 描画は既存の半透明矩形のみ。
  追加の allocation は発生しない。`viewportRectToCanvasRect` は値返し。
- D3D12 / Vulkan 差分なし: `setZoom / setPan` 経由のみ。

### 注意点

- 既存の `isRubberBandSelecting_` と排他にするが、両方同時に起きないよう
  `handleMousePress` の入口で状態を確認する（既存の modality ガードと
  同じ場所に集約）。
- Crop を別モードにする場合、カメラパラメータを一切変更せず overlay だけで
  隠す実装は P2-8 View Regions と責務が近い。**P0-1 では zoom モードのみを実装し、
  crop マスクは将来 P2-8 とまとめて設計** することを推奨。
- 矩形が極小（`< 6.0f / zoom`）のときは BoxZoom を発動せず、
  クリックとして扱う既存挙動を維持する。

## 2. P0-2 Tumble pivot under cursor — 新規 state 追加が必要

### 既存 API（接続点）

- カメラ回転: `viewportOrientationNavigator_`（`ArtifactCore::ViewOrientationNavigator`）。
  `currentOrientation()` / `setCurrentOrientation(QQuaternion)` /
  `activeHotspot()` / `snapTo(hotspot, animate)` / `isAnimating()`。
- カメラビュー計算: `viewportOrientationViewMatrix(orientation, target, distance)`
  （37000 行付近）。**`target` が tumble pivot に相当する**。
- 既存ピボット描画: 20487 行付近の selection bounds center を「+」記号で
  描画するパターン（`FloatColor{1.0f, 0.92f, 0.30f, 0.95f}`、`max(5.0f, 7.0f / zoom)`
  半径の十字）。これを **tumble pivot marker としてそのまま再利用できる**。
- カーソル下のレイヤー取得: `layerAtViewportPos(const QPointF&) const` が既存。
  レイヤーが取れなければ composition の中心を使うフォールバック。
- canvas/ワールド変換: `viewportPosToCanvas` 系のヘルパ
  （`viewportRectToCanvasRect` を単点に使う）。
- 履歴: `pushViewHistory()`。

### 設計メモ

- 新規 state: `tumblePivotOverrideEnabled_` (bool)、
  `tumblePivotCanvasPos_` (QPointF)。**カメラの layer パラメータは変更しない
  （分析記述通り）**。`viewportOrientationViewMatrix` 呼び出しの
  `target` 引数だけ上書きする経路が最短。
- 確定処理: 修飾（例: Viewport ローカルの `Space+Z`）押下時に
  `viewportPos → canvas pos` へ変換し、`tumblePivotOverrideEnabled_ = true`、
  `tumblePivotCanvasPos_` を保存。ピボット解除は
  既存 `pushViewHistory()` で **保存前の状態を undo で戻せる**。
- 描画: 既存 selection-bounds 十字描画を `LineDebugKind` 経由で
  `LineDebugKind::Axis` または新規 `LineDebugKind::TumblePivot` に統合。
  ただし `LineDebugKind` enum を増やすと既存 JSON 互換性に影響するため、
  まずは `isLineDebugKindVisible(LineDebugKind::Axis)` を流用するか、
  `drawSolidRect` ベースの十字を inline で描く（既存 `pivotRadius` の式を踏襲）。
- ショートカット: `ShortcutId::ViewTumblePivotUnderCursor` を追加。
  Houdini の Space+Z に近いローカルの既定キーを登録する。
- 3D（spatial orientation）限定: 2D Front ortho では意味がないため、
  `usesSpatialViewportOrientation()` チェックで発動を抑止。

### 注意点

- 既存 `gizmoGroupPivotBefore_` は gizmo 編集専用で、tumble pivot とは別物。
  **名称衝突を避けるため state 名は `tumblePivot*` プレフィックスで統一**。
- camera layer の `target / interestPoint` 相当パラメータを上書きすると
  保存されてしまう。`viewportOrientationViewMatrix` のローカル引数として
  渡すだけに留め、永続化経路に入れない（layer parameter API を呼ばない）。
- フロント ortho（2D）表示では何もしない。空間 orientation のときだけ marker
  を出す。

## 3. P0-3 Interactive Render Region — 既存再利用が部分的、再設計が必要

### 既存 API（接続点）

- `ArtifactRenderROI`（`Artifact/include/Render/ArtifactRenderROI.ixx`）:
  `QRectF rect`（composition pixel 座標）、`expanded(pixels)` / `intersected` /
  `united` / `scaled(factor)` / `fitted(w, h)` 等の値型 API。
  `RenderModeSettings` に `useScissorTest / useROICache / skipEmptyROI /
  sampleCount / enableEffects` あり。
- `ArtifactRenderContext.ixx` の `RenderContext` 構造体に
  `roi`（composition 座標）、`viewportROI`（ビューポート座標）、
  `scissorROI`（スクリーン座標）が既に分離して存在する。
  これは IRR 矩形の格納先として流用できる。
- `ProgressiveRenderer`（`Artifact/src/Render/ArtifactFrameCache.cppm`）:
  `setQuality(RenderQuality)` / `setRenderCallback(callback)` /
  `setDraftQuality(downsampling)` / `setPreviewQuality(downsampling)` /
  `requestUpgrade()` / `cancelUpgrade()` / `forceFinalRender()` /
  `currentProgress()`。**しかし CompositionRenderController からは誰も呼んでいない**
  （grep で 0 hit）。`FrameCache` 内の独立クラスとして実装済みだが、
  CompositionRenderController の `requestUpgrade` 経路は未統合。
- モーダル gizmo 入力: `beginModalGizmoInteraction(mode, viewportPos)`、
  `commitModalGizmoInteraction()`、`isModalGizmoInteractionActive()`。
  矩形リサイズハンドル（4 隅 + 4 辺 + 移動）はこれを再利用できない
  （gizmo は 3D 用）。
- オーバーレイ矩形: `setDropGhostPreview(rect, title, hint, label)` /
  `clearDropGhostPreview()` が既存。これは IRR 矩形の表示に使える。
- HUD 表示: `setInfoOverlayText(title, detail)` が既存軽量 HUD。

### 重要な発見（実装直前の再照合で判明）

- **`RenderContext::roi` は CompositionRenderController から現在呼ばれていない**。
  `EffectContext ctx` のみが CompositionRenderController 内で
  使用されており、`RenderContext` 構造体は render path には接続されていない。
- 既存 ROI 経路は `damageTracker_.combinedDirtyROI()` を
  TileGrid の bounded plan 計算に渡す用途のみで、ビューポートの
  **インタラクティブな矩形 ROI** というよりは **キャッシュの dirty region** 寄り。
- 「`ArtifactFrameCache.cppm` の ProgressiveRenderer を再利用する」記述は
  **現状では成立しない**。CompositionRenderController 内に
  ProgressiveRenderer のインスタンスも、callback 接続も、ROI 連動もない。

### 設計メモ（2026-09-22 着手時に再構成）

- **P0-3a（本マイルストーン着手）**: 矩形を CompositionRenderController に
  保持し、overlay + HUD に露出する最小実装。**RenderContext 統合は P0-3b に送る**。
  - 公開 API: `setInteractiveRenderRegion(QRectF)` /
    `clearInteractiveRenderRegion()` /
    `isInteractiveRenderRegionActive() const` /
    `interactiveRenderRegion() const` /
    `setInteractiveRenderRegionResolutionScale(float)` /
    `interactiveRenderRegionResolutionScale() const`
  - ハンドル hit-test + drag: `interactiveRenderRegionHandleAt(viewportPos)`
    が 0（なし）/ 1（移動）/ 2-9（NW/N/NE/E/SE/S/SW/W）を返す。
    `begin/update/end/cancelInteractiveRenderRegionDrag` で 2D modal state を
    操作。`isInteractiveRenderRegionDragActive()` も公開。
  - overlay: `drawSelectionEditingOverlay` 末尾に矩形枠 +
    8 ハンドル（`drawSolidRect` の小矩形）+ 移動 hitbox を描画。
  - HUD: `setInfoOverlayText("IRR", "N% res  W x H")` を
    set/clear 経路で更新。
  - modality: `interactiveRenderRegionHandleDrag_ != 0` を `interactionBusy()`
    に加え、Detached Task を正しくブロック。
  - ショートカット: `ShortcutId::ViewInteractiveRenderRegion` を追加
    （`Count = 156`、既定キー `Ctrl+Shift+R`）。Ctrl+Alt+B / Z との衝突なし。
- **P0-3b（別マイルストーン）**: RenderContext を CompositionRenderController
  から使えるように setter/getter を追加し、`RenderContext::roi = IRR 矩形` を
  render path に注入。`ArtifactFrameCache.cppm` の `ProgressiveRenderer` を
  CompositionRenderController に所有させ、`setRenderCallback` で
  ROI 制限付き render を呼ぶ。`BoundedTileRefinementPlan::MaxTiles = 16` の
  上限内に収まる値型の利用に限定し、ホットパスの allocation を増やさない。
- 矩形の HUD: 既存 `setInfoOverlayText` を使用。解像度スライダの値は
  P0-3a では HUD 専用（実レンダーには影響しない）。

### 注意点

- ProgressiveRenderer 統合は **本マイルストーン外**とする。
  「矩形 ROI を pipeline に流すだけ」の P0-3a に絞り、
  Progressive は別マイルストーンで再設計する。マイルストーン本体
  （`MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`）にこの分割を追記すべき。
- `RenderMode::Final` との同時使用は禁止（既存規約）。ROI が active のとき
  は quality preset を Preview に強制する制御が必要（P0-3b 範囲）。
- 矩形が composition からはみ出した場合は `RenderROI::intersected(compBounds)`
  でクランプしてから pipeline に渡す（P0-3b 範囲）。
- Quad レイアウト時の 4 ペイン同期は P0-3b と PaneState 拡張
  （P1-3）と同時に着手。

## 4. P0-4 ビューポート タイプ別フィルタ — 既存 enum 拡張ではなく別状態追加

### 既存 API（接続点）

- `CompositionLayerRenderFilter`（ixx 58 行）: `All` / `SelectedOnly` の 2 値。
  `setLayerRenderFilter / layerRenderFilter()` が公開 API。
  実装は `passesLayerRenderFilter(filter, selectedIds, selectedLayerId, layer)`
  （5498 行）で、CompositionRenderController が全レイヤー走査時に呼ぶ。
- レイヤー種別メタ: `ArtifactAbstractLayer` に以下の仮想関数あり:
  - `isNullLayer() const`（オーバーライド多数。Camera, EnvMap は true を返す。
    AdjustableLayer も true。SandSim は false）
  - `isCloneLayer() const`
  - `isParticleLayer() const`
  - `isAdjustmentLayer() const`（SandSim, Adjustable で override）
  - `className()`（QString）
- 動的型判定: `ArtifactCore::dynamicPointerCast<ArtifactXxxLayer>(layer)`
  が `ArtifactLayerPanelWidget` 等で多数使われている（Light, Camera, Noise,
  Solid2D 等）。ビューポート側でも同手法が使える。

### 重要な発見

- Light 3D / Camera 3D は `isNullLayer()` が true を返す実装で、
  Light と Camera と Adjustable と Construction と EnvironmentMap と
  Adjustable と SandSim などがすべて `isNullLayer() == true`。
  「null かどうか」は **「可視か不可視か」のセマンティクス**で、
  ビューポート種別フィルタには使えない。
- 種別判定は現状 **dynamicPointerCast による分岐** が標準。
  Light / Camera / Null / Spline / Generator / Volume はクラス単位で識別する。
- **`CompositionLayerRenderFilter::All / SelectedOnly` は Render Queue 出力
  には影響しない**（実装を確認した結果、controller 内の描画レイヤー走査で
  スキップするだけ）。分析記述通り「拡張ではなく別状態追加」が正しい。

### 設計メモ

- 新規 enum を追加: `enum class CompositionViewportLayerCategoryMask` を
  `CompositionRenderController.ixx` に追加し、`Solid / Text / Image / Shape /
  Adjustment / Null / 3DModel / 3DLight / 3DCamera / Particle / Procedural /
  Mask / Audio` 等のカテゴリ フラグを持つ
  （`std::underlying_type` でビット OR 可能な `uint32_t` 値）。
- 状態: `layerCategoryMask_ = All`（既定全表示）を Impl に追加。
- API: `setViewportLayerCategoryMask(mask)` / `viewportLayerCategoryMask()` /
  `toggleViewportLayerCategory(LayerCategory)` を公開。
- 判定: `passesLayerCategoryMask(layer, mask)` を
  `passesLayerRenderFilter` の隣に置き、レイヤー描画ループで併用する。
  動的判定は `dynamicPointerCast` を 1 度だけ通すか、
  `className()` 比較（高速パス）のどちらかに寄せる。
- 永続化: 分析記述通り「ビューポート専用 override」。
  保存対象は `PaneState` 相当の場所に置き、Composition 再読込で
  既存データを変更しない。MOC は CompositionRenderController 内に閉じ、
  Composition 側 JSON には触らない。
- UI: `ArtifactViewMenu` に「Show: All / 2D / 3D / 3D Lights / 3D Cameras /
  Audio / Particle」のサブメニューを追加（既存フィルタと並列）。
  新規 `ShortcutId::ViewToggleLayerTypeFilter` を 1 件追加。
- ショートカット: `ViewToggleLayerTypeFilter` に Houdini Display Options の
  Marker visibility level を意識した既定キーを登録する。

### 注意点

- `CompositionLayerRenderFilter` を直接拡張すると
  Render Queue 経路に波及するリスクがある。**完全に別の状態として追加**し、
  既存 API の意味論を変えない。
- 種別判定で `dynamic_pointer_cast` を毎フレーム呼ぶとホットパスで重い。
  Impl に `std::array<LayerCategory, maxLayers>` 風のキャッシュを 1 度だけ
  作り、Composition 変更時のみ invalidate する。
- 3D 系 Light / Camera は現状 `is3D()` で識別可能（`ArtifactCameraLayer.ixx` で
  `bool is3D() const { return true; }`）。Light 側は要確認。
- Mask は表示カテゴリとして別フラグで扱う（既存 `LineDebugKind::MaskPath` と
  役割が近いので、表示可否は別カテゴリでも混乱しない）。

## 5. ショートカット・Signal/Slot まとめ

### 追加候補 `ShortcutId`（4 件、Count は 153→157）

| ShortcutId 名 | コンテキスト | 既定キー案 |
| --- | --- | --- |
| `ViewBoxZoom` | Viewport | `Ctrl+Alt+B`（Houdini の Box zoom 風） |
| `ViewTumblePivotUnderCursor` | Viewport | `Space+Z`（Houdini 風） |
| `ViewInteractiveRenderRegion` | Viewport | `Ctrl+Shift+R`（IRR 風） |
| `ViewToggleLayerTypeFilter` | View | なし or 修飾キー（Show サブメニューに集約） |

### Signal/Slot 方針（AGENTS.md 厳守）

- 新規 signal/slot を **一切追加しない**。`addChangeListener` で
  ShortcutBindings 変更を監視する既存経路、command palette / pie menu の
  既存 callback 経路をそのまま使う。
- `CompositionRenderController::Impl` 内の状態変化は
  既存の `markRenderDirty() / invalidateBaseComposite() / invalidateOverlayComposite()`
  のいずれかで通知し、外部には新たな signal を発火しない。

## 6. ホットパス・AGENTS.md 整合チェック

- P0-1: overlay 描画は半透明矩形 1 枚 + outline。allocation ゼロ。
- P0-2: 十字描画は `drawSolidRect` 4 本分の計算のみ。`QPointF` 値型。
- P0-3a: ROI は値型 `RenderROI` の代入 1 回。`BoundedTileRefinementPlan` の
  `MaxTiles = 16` 内に収まる（composition ピクセル 16 tile 上限）。問題なし。
- P0-4: 種別判定キャッシュを `Composition` 変更時のみ更新すれば毎フレームの
  `dynamic_pointer_cast` を回避できる。`is3D() / className()` 比較は軽量。
- すべて D3D12 / Vulkan 共通の `drawSolidRect / setZoom / setPan` 経由。
  backend 差は発生しない。
- 新規 `.ixx` / `.cppm` の追加は **P0 全項目で不要**。
  すべて既存 ixx/cppm の API 追加（ヘッダの関数宣言 + .cppm の実装）で完結。

## 7. 実装着手時の推奨手順（コード変更は未実施）

1. P0-1 のみを最小実装: `CompositionRenderController.ixx` に
   `beginBoxZoom / updateBoxZoom / endBoxZoom` を 3 つ追加し、
   `handleMousePress / Move / Release` の modality ガードに組み込む。
   View 履歴は `pushViewHistory` で確定。ショートカットは
   `ShortcutId::ViewBoxZoom` を追加して `ViewBoxZoom` のアクションとして登録。
   `ArtifactViewMenu.cppm` に「Navigation > Box Zoom」エントリを 1 行追加。
2. P0-2 を次に着手: `tumblePivotOverride*` state を Impl に追加し、
   `viewportOrientationViewMatrix` 呼び出し箇所（render path）にだけ
   target 上書きを挟む。ショートカット `ViewTumblePivotUnderCursor` 追加。
3. P0-3 は **P0-3a（矩形 ROI のみ）→ P0-3b（Progressive 統合）** に分割。
   マイルストーン本体に分割追記を先に行う。
4. P0-4 は最後の P0。新規 enum + mask 状態 + dynamic_pointer_cast キャッシュ。
   `CompositionLayerRenderFilter` には触らない。

## 8. 未確認事項（実装前に再照合が必要）

- Light 3D レイヤーの `is3D()` 実装の有無（`ArtifactLightLayer.ixx` 未確認）
- Spline / Generator / Volume レイヤーのクラス名と `className()` 戻り値
- `viewportOrientationViewMatrix(target, distance)` の `target` が
  canvas 座標か world 座標か（37000 行付近の実装で確認が必要）
- `RenderContext::roi` が CompositionRenderController の render ループで
  どこに上書きされるか（実装を掘らないと安全に渡せない）

## 9. 関連文書

- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`
- `docs/planned/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_TODO_2026-09-04.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
- `docs/technical/HOT_PATH_RULES.md`
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `Artifact/include/Render/ArtifactRenderContext.ixx`
- `Artifact/src/Render/ArtifactFrameCache.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`
- `ArtifactCore/include/UI/ShortcutBindings.ixx`
- `ArtifactCore/src/UI/ShortcutBindings.cppm`
