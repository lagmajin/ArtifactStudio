# Design — Interactive Render Region (IRR) 設計統合

**最終更新:** 2026-09-22

**ステータス:** 設計のみ。実装は未着手。着手前提はビルド・実機確認の許可後。

## 0. 背景と動機

`docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md` で
C4D Interactive Render Region / Nuke Pre-render Region 相当を P0-3 として
位置づけ、`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`
で導入計画を起こした。

P0-3a は「矩形保持 + overlay + HUD のみ」を `CompositionRenderController`
に追加済み（2026-09-22）。矩形がパイプラインに流れず**表示専用**。

P0-3b / P0-3c でこの矩形を実際にレンダリング結果に反映させ、
C4D IRR / Nuke Pre-render Region のセマンティクス（「矩形内のみを
再レンダー」「解像度スライダで段階品質」）に到達する。本文書は
その前に決めるべき 8 項目を整理する。

## 1. 着手前の静的照合（2026-09-22）

### 1.1 RenderContext は孤児モジュール

- `Artifact/include/Render/ArtifactRenderContext.ixx` に
  `struct RenderContext { roi / viewportROI / scissorROI / useROICache /
  setMode(RenderMode) / setROI(RenderROI) / updateViewportROI() /
  updateScissorROI() }` が定義されている
- `Artifact/include/Render/ArtifactRenderROI.ixx` に `struct RenderROI`
  と `enum class RenderMode { Editor, Preview, Final }` もある
- **しかし 2026-09-22 時点で `Artifact` / `ArtifactCore` のどちらからも
  RenderContext は include されていない**（grep でヒット無し）
- `ArtifactRenderROI` は CompositionRenderController の
  `damageTracker_.combinedDirtyROI()` と `TileGrid::makeBoundedDirtyTilePlan`
  の引数にのみ使われている

### 1.2 ArtifactIRenderer は partial render 経路を持たない

- `ArtifactIRenderer::render(...)` 系メソッドは `roi` / `partialRender` /
  `setRenderROI` を受け取らない
- `renderComposition(ctx, layers, currentFrame, outputRTV)` も `RenderROI`
  を受け取らない
- CompositionRenderController は `previewRenderPipelineSlots_` を 2 スロット
  所有し、`RenderPipeline` を直接保持している（パイプラインを外部所有しない）

### 1.3 PaneState は単一 controller を共有

- `ArtifactCompositionEditor.cppm` の `PaneState { paneId, rect, view,
  controller, visible }` は 4 ペイン分あるが、`renderController_` は
  1 つだけ（`composition_view / editor` の単一 controller インスタンス）
- Quad レイアウト時、4 ペインは同じ controller を共有し、PaneState は
  表示位置と widget ポインタのみを保持する
- P1-3「Per-viewport 設定 + Apply to all split views」で PaneState に
  表示設定を移す計画があり、ROI もその中に乗る候補

### 1.4 ProgressiveRenderer は孤立

- `ArtifactFrameCache.cppm` の `ProgressiveRenderer` は
  `setQuality / setRenderCallback / requestUpgrade / forceFinalRender` を持つ
- CompositionRenderController からは誰も呼んでいない
- 実装は callback ベースで、`RenderCallback` が `RenderQuality` を受け取り
  bool を返す契約

## 2. 設計 8 項目（結論まで書き出す）

### D1. 矩形の状態をどこに置くか

**結論: CompositionRenderController::Impl を一次所有、
PaneState には mirror しない。**

- 一次所有: `interactiveRenderRegionRect_` (QRectF, canvas pixel) を
  `CompositionRenderController::Impl` に置く（今日の P0-3a 実装と同じ）
- 理由: CompositionRenderController は `RenderPipeline` を直接所有し、
  render path に最も近い。矩形 → pipeline の流れを 1 ホップで済む
- 反射: `interactiveRenderRegionActive_` フラグだけ `PaneState` に持たせ、
  Quad レイアウト時の「特定ペインだけ ROI 表示」を可能にする
  （P1-3 範囲、**P0-3b では全ペイン同期で十分**）

### D2. PaneState との関係

**結論: P0-3b では全ペイン同期、P1-3 でペイン単位対応。**

- P0-3b: CompositionRenderController の単一矩形を全ペインが共有描画。
  Quad レイアウトでも 1 つの矩形だけ
- P1-3: PaneState に `roiEnabled` / `roiRect` を追加し、4 ペイン独立に。
  「Apply to all split views」コマンドで全ペインへ伝播
- 実装の順序: P0-3b → P1-3 で 2 段。後者が前者を含意する形で拡張
- 理由: P0-3b の段階で PaneState を増やすと Quad レイアウトの
  リファクタ（controller 4 つ化）に巻き込まれるが、そのリファクタは
  P1-3 で本来扱うべき問題

### D3. ProgressiveRenderer callback への ROI 渡し方

**結論: callback 引数追加ではなく、`RenderQuality` と同じ段にもう一段
「ROI 矩形」を加えた callback を新設する。**

- 既存: `using RenderCallback = std::function<bool(RenderQuality)>;`
- 新設: `using PartialRenderCallback = std::function<bool(RenderQuality,
  const RenderROI&)>;` を `ProgressiveRenderer` の隣に置かず、
  `CompositionRenderController::Impl` 内にローカル typedef として置く
- 理由: `ProgressiveRenderer` は今は孤立モジュールで、公開 API を
  変更すると将来の統合時に戻れない。**CompositionRenderController の
  所有 callback を「ROI 受け型」に進化**させ、ProgressiveRenderer 統合は
  その callback に render を委譲する形にする
- 順序: まず `CompositionRenderController::Impl::renderPartialRegion` を
  内部 private 関数として実装（既存 `renderOneFrameImpl` のサブパス）。
  callback ベースは後段で

### D4. `RenderMode::Final` との同時使用禁止をどこで enforce するか

**結論: `CompositionRenderController::setInteractiveRenderRegion` の入口で
quality preset を Preview に強制。**

- 既存 `setPreviewQualityPreset(PreviewQualityPreset)` を
  `setInteractiveRenderRegion` 内で呼び、`RenderMode::Final` 相当の
  preset が指定されていれば Preview にダウングレード
- RenderContext 側の `setMode(RenderMode::Final)` は呼ばせない
  （`RenderMode` は `getModeSettings` 経由で `useScissorTest=false /
  useROICache=false / skipEmptyROI=false` を返すので、ROI 統合と
  構造的に噛み合わない）
- HUD に「IRR active — quality forced to Preview」と明示して
  暗黙のダウングレードをユーザーに伝える
- 解除時の preset 復帰は `clearInteractiveRenderRegion` で
  `pushViewHistory` と同じ感覚で「保存前の preset」を復元する

### D5. 既存 damage tracker との優先順位

**結論: IRR 矩形を damage tracker の `combinedDirtyROI` と **和集合** する。
IRR のほうが強い制限。**

- 既存: `damageTracker_.combinedDirtyROI()` が
  `makeBoundedDirtyTilePlan(grid, RenderROI(visibleCanvasRect), 8)`
  に渡る
- 新規: `effectiveROI = damageROI.united(irrROI)` を計算し、
  tile plan に渡す
- 和集合の理由: damage は「再描画が必要」、IRR は「再描画を許可」。
  両方を含む領域だけ再レンダーすれば、damage を満たしつつ
  IRR 外の余計なレンダーを省ける
- 順序: damage tracker → IRR 矩形の順に評価。和集合が空 =
  「どちらも再レンダー不要」で cache hit
- 矩形外完全スキップの是非は D6 で決定

### D6. 矩形外描画の取り扱い

**結論: 矩形外は「前のフレーム結果を保持（キャッシュ）」。
完全スキップは P0-3c に送る。**

- P0-3b: 矩形外はキャッシュヒットで前回の値をそのまま使う。
  結果として矩形内だけ更新されて見える（ユーザー期待通り）
- P0-3c: 矩形外を完全スキップして CPU/GPU 負荷を減らす。
  CompositionRenderController 内に `isInteractiveRenderRegionActive()`
  フラグで分岐するパスを追加
- 理由: 完全スキップは Scissor / Viewport 設定の追加が必要で、
  RenderContext 統合と同時にやると検証範囲が広くなる
- バックエンド差: D3D12 と Vulkan で `IDeviceContext::SetScissor` の
  セマンティクスは同等のはずだが、**P0-3b はキャッシュ経路で統一**し、
  差が出にくい段階で先に統合する

### D7. 解像度スライダの単位

**結論: `composition pixel / viewport pixel / DPR 倍率` の 3 つを
内部で区別し、公開 API は `composition pixel` を基準にする。**

- 内部保持: `interactiveRenderRegionResolutionScale_`（既存 P0-3a）=
  [0.25, 1.0] の **composition pixel に対する描画密度**。
  - 0.25 = 矩形の 1/4 ピクセル密度でレンダー（高速、低品質）
  - 1.0 = 矩形の完全ピクセル密度（最高品質）
- viewport pixel は IRR 矩形の viewport 上での見た目を制御するだけで
  解像度スライダとは別物（既に BoxZoom で zoom factor が
  viewport ↔ canvas 変換を担う）
- DPR 倍率は renderer の初期化時に固定される値（composition 単位に
  換算済み）。スライダには露出しない
- 表示: HUD に `"N% res  W x H"` を出しているが、これは
  `resolutionScale * 100%` で十分で、viewport pixel は含めない
  （P0-3a 実装済み、維持）

### D8. RenderContext を CompositionRenderController から使えるようにするための最小インターフェース

**結論: `getRenderContext() const -> const RenderContext&` のみ追加。
setter は作らない。**

- 公開 API: `const RenderContext& CompositionRenderController::renderContext() const;`
- 内部: `Impl::renderContext_` をメンバ追加し、`initialize` で
  `setMode(RenderMode::Preview)` を呼んで初期化
- render path: `renderOneFrameImpl` の冒頭で `renderContext_.setROI(irrRect)`
  を呼び、`updateViewportROI() / updateScissorROI()` を連動発火
- setter なし: RenderContext は CompositionRenderController の
  レンダリング状態を反映する read-only ハンドル。外部から
  `setMode` / `setROI` を直接呼ぶと CompositionRenderController の
  状態管理と不整合になるため、**getter のみ**
- 段階: P0-3b で getter 追加 → P0-3c で `setROI(irrRect)` を
  render path に組み込み → それぞれビルド・実機確認後に進む

## 3. 着手順序（decision doc 承認後）

1. **P0-3b.0** `RenderContext` getter を `CompositionRenderController` に追加。
   `Impl::renderContext_` を所有、`initialize` で `setMode(Preview)`、
   `destroy` で解放。**ROI 矩形はまだ流さない**（getter が壊れていない
   ことだけ確認）
2. **P0-3b.1** `renderOneFrameImpl` の冒頭で `renderContext_.setROI(irrRect)`
   を呼び、`updateViewportROI / updateScissorROI` を経由した
   `RenderPipeline::renderComposition` への橋渡しを検討
   （`renderComposition` に `RenderROI` 引数を追加する必要があるか確認）
3. **P0-3b.2** `RenderPipeline::renderComposition` に `RenderROI` パラメータ
   を追加し、内部で `IDeviceContext::SetScissor` を呼ぶ
   （または viewport 設定で矩形に切り取る）。D3D12 / Vulkan 両方で
   結果が変わるかを実機確認
4. **P0-3c** D6 採用（矩形外スキップ）実装。`RenderMode::Final` との
   排他制御（D4）を `setInteractiveRenderRegion` 入口に追加
5. **P0-3d** ProgressiveRenderer を CompositionRenderController に所有させ、
   `PartialRenderCallback` を経由して段階品質レンダを委譲（D3）
6. **P1-3 連動** PaneState に ROI を拡張。「Apply to all split views」を
   既存 `setPresentationLayout(Quad)` 経路に追加

## 4. 検証ポイント（各段で実施）

- D3D12 / Vulkan で矩形内のみ品質が変化すること（スクリーンショット比較）
- 矩形サイズ変更中の CPU / GPU プロファイリング
- 矩形外キャッシュヒット時のメモリ帯域
- Quad レイアウト時のペイン間同期（P0-3b は全ペイン同期のみ検証）
- IRR 矩形と damage tracker の和集合ロジックでキャッシュ漏れが無いこと
- `RenderMode::Final` が誤って選択された場合に自動で Preview に
  ダウングレードされ、ユーザーに通知されること

## 5. リスクと未確認事項

- **`RenderPipeline::renderComposition` の内部で Scissor を設定すると、
  他パスの画面全体エフェクト（SSAO / Bloom 等）が矩形の外で
  描かれないリスク** → 矩形外キャッシュ戦略（D6 P0-3c）の検証が必要
- **`RenderContext` の `useROICache` フラグ**は現状 CompositionRenderController
  から制御されていない。`setMode(Preview)` 経由でデフォルトが `true` に
  なることを期待するが、`RenderModeSettings::useROICache` の
  デフォルト値を確認していない
- **PaneState 拡張時に controller を 4 つに分けるか**、
  1 つの controller に 4 ペイン分の ROI を持たせるかは P1-3 まで保留
- **`ArtifactFrameCache.cppm` の `ProgressiveRenderer` は
  `RenderCallback` の戻り値 bool を見ている**が、
  矩形のサブセット描画に失敗した場合の挙動（キャンセル / グレースフル
  デグラデーション）が未定義

## 6. 関連文書

- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`
- `docs/planned/P0_DESIGN_NOTES_2026-09-22.md` §3
- `Artifact/include/Render/ArtifactRenderContext.ixx`
- `Artifact/include/Render/ArtifactRenderROI.ixx`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/include/Render/ArtifactRenderLayerPipeline.ixx`
- `Artifact/src/Render/ArtifactFrameCache.cppm`（`ProgressiveRenderer`）
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`（`PaneState`）
- `Insight.md` 2026-09-22 「VP DCC パリティ P0-3a」エントリ
