# M-VP-DCC-1: ビューポート DCC パリティ導入（C4D / Houdini / Maya）

**最終更新:** 2026-09-22

**ステータス:** P0-1 / P0-2 / P0-3a 着手済み（コード変更のみ、ビルド・実機確認はユーザー明示指示待ち）。P0-3b（RenderContext 統合） / P0-4 / P1〜P2 は未着手（分析完了）。
C4D / Houdini / Maya / Autograph / Blender / 3ds Max / Unreal / Nuke を含む

## 目的

`docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md` で特定したビューポートの
欠落機能を、既存インフラの再利用を優先して段階的に導入する。新規機能はデータモデルを変えず、
ビューポート表示・操作の追加に限定する。

## 前提（分析で確定した事実）

- チャンネル分離表示、オニオンスキン、X-Ray、グリッド/ルーラー、セーフマージン、ガイド、
  マルチビューポート、ブックマーク、ピエメニュー、Isolation（表示のみ）は実装済み。
- 未実装の代表は、種類別ビューポートフィルタ、Interactive Render Region、Box zoom/crop、
  tumble pivot のカーソル下設定、ghosted context、isolate の状態復元、tear-off viewport。
- 既存の再利用対象: `ArtifactRenderROI`、`ProgressiveRenderer`（`ArtifactFrameCache.cppm`）、
  既存ズーム補間 API、`PaneState`、`ShortcutBindings`、Diligent オーバーレイ描画 API。

## 導入計画

| Phase | 内容 | 状態 | 主な変更対象 |
| --- | --- | --- | --- |
| P0-1 | Box zoom / Box crop | Code Changes Pending（実装追加、ビルド未確認） | `ArtifactCompositionEditor` / `ArtifactCompositionRenderController` |
| P0-2 | Tumble pivot under cursor | Code Changes Pending（実装追加、ビルド未確認） | 同上 |
| P0-3 | Interactive Render Region（ROI + Progressive） | 着手分割: P0-3a / P0-3b.0 / P0-3b.1 / P0-3b.2 着手済み、P0-3c 以降未着手 | `ArtifactFrameCache` / ROI / overlay / `ArtifactCompositionRenderController` / `ArtifactRenderLayerPipeline` |
| P0-4 | ビューポート タイプ別フィルタ | Not Started | `ArtifactCompositionRenderController` |
| P1-1 | Isolate Select の状態復元 | Not Started | 同上 |
| P1-2 | Ghosted context display | Not Started | 同上 |
| P1-3 | Per-viewport 設定 + Apply to all split views | Not Started | `PaneState` / 表示設定 |
| P1-4 | Maya 風シェーディングトグル | Not Started | 同上 |
| P1-5 | Viewer exposure controls（Gain/Gamma/Saturation） | Not Started | 表示ポストプロセス |
| P1-6 | チャンネル表示の Straight / Luminance / Matte バリアント | Not Started | `ViewportChannelDisplayMode` |
| P1-7 | パス overlay の可視性モードと種類別フィルタ | Not Started | overlay / 表示フィルタ |
| P1-8 | Per-viewport Local Camera / Focal Length / Clip | Not Started | `PaneState` / カメラ状態 |
| P1-9 | Local View / Local Collections の分離と復元 | Not Started | Isolation overlay / 選択管理 |
| P2-1 | C4D HUD 相当のパラメータ常設表示 | Not Started | オーバーレイ / 既存 modal gizmo |
| P2-2 | Hardware fog / volumetric fog / bloom | Not Started | Diligent パス |
| P2-3 | Viewport tear-off copy | Not Started | ペイン管理 |
| P2-4 | Construction plane handle | Not Started | Construction Layer / 既存ギズモ |
| P2-5 | Object Type Filter のビューポート拡張 | Not Started | レンダーフィルタ / overlay |
| P2-6 | Viewer format overriding（VP 単位の解像度/PAR） | Not Started | ペイン / レンダーターゲット |
| P2-7 | Cavity / Studio Shadow | Not Started | シェーディングパス |
| P2-8 | View Regions（表示クリップ領域） | Not Started | `ArtifactCompositionEditor` / renderer |

### P0-1 Box zoom / Box crop

- ドラッグ矩形の内側へズームイン、外側へズームアウト（Houdini の Ctrl+Alt ボックス挙動に相当）。
  クロップは「画面窓」の保持として扱い、カメラパラメータは変更しない。
- 既存のマーキー選択入力、`zoomAtFactor` 系の補間、操作中プレビュー品質の downsample を再利用する。
- 新規操作は `ShortcutBindings` の Viewport ローカルコンテキストへ登録し、固定キーを追加しない。

### P0-2 Tumble pivot under cursor

- Space+Z 相当でカーソル下の点を一時ピボットにし、カメラ位置のみ移動して向きは保持する。
- ピボットはプレビュー専用とし、カメラレイヤーのパラメータや保存値は変更しない。
- ピボットの小マーカーは既存 overlay 描画（線・矩形）で表現する。

### P0-3 Interactive Render Region

- 既存 ROI（viewport ROI / scissor ROI）と `ProgressiveRenderer` を組み合わせ、
  矩形内のみを解像度スライダ付きで再レンダーする。
- 矩形の移動/拡縮ハンドルは既存ギズモ描画を再利用し、確定/取消は既存 modal 入力へ接続する。
- 負荷対策として、ドラッグ中は品質を落とし、確定後にのみ progressive アップグレードを要求する。

### P0-4 ビューポート タイプ別フィルタ

- レイヤー種別（ライト/カメラ/Null/スプライン/ジェネレータ/ボリューム等）ごとの表示可否を
  ビューポート専用 override として保持する。レイヤーの `visible` とシリアライズは変更しない。
- 既存 `CompositionLayerRenderFilter` を拡張するのではなく、別の表示フィルタ状態として追加し、
  Render Queue / 出力系のフィルタとは分離する。

### P1-1 Isolate Select の状態復元

- 分離開始時に表示状態をスナップショットし、解除時に復元する。Undo は既存経路を使い、
  新しい signal/slot は追加しない。

### P1-2 Ghosted context display

- 編集対象以外を低不透明度で残す表示段階を追加する。X-Ray の全体減衰とは別状態として扱い、
  既存の overlay/render 経路へ不透明度パラメータを渡す形にする。

### P1-3 Per-viewport 設定 + Apply to all split views

- `PaneState` にビューポート表示設定の適用範囲を持たせ、Houdini の
  "Apply operations to all split views" 相当を再現する。設定の保存形式は既存の
  View テンプレート経路を再利用する。

### P1-4 Maya 風シェーディングトグル

- Wireframe on Shaded / Backface Culling / Bounding Box / Cycle rig display を、
  3D モデルの `render.mode`、LOD、X-Ray、Rig overlay を組み合わせて提供する。

### P1-5 Viewer exposure controls（Autograph Post-processing 相当）

- Gain（-6〜+6 stop）/ Gamma（0〜5）/ Saturation（0〜4）をビューポート表示にのみ掛け、
  HDR の明部・暗部を確認できるようにする。全体トグルと個別スイッチを付ける。
- 既存の linear / 32bit 合成パイプライン上の表示ポストプロセスとして実装し、
  保存・出力・カラーサンプラの読み取り値は変えない（表示専用であることを HUD に明示）。

### P1-6 チャンネル表示の Straight / Luminance / Matte バリアント

- `ViewportChannelDisplayMode` に、Unpremultiplied（RGB/R/G/B Straight）、
  Luminance（Rec 709 係数）、Matte（αを赤へ加算）を追加する。
- 既存のチャンネル表示 SRV 選択・合成表示経路を再利用し、D3D12 / Vulkan で同一結果にする。

### P1-7 パス overlay の可視性モードと種類別フィルタ（Autograph Path Overlays 相当）

- パス overlay を Shape Layer / Mask / その他（follow-path 軌跡など）の種類別に表示可否を切替。
- Shape Contour Visibility を Always / Never / Hovered or selected / Selected Layers の
  4モードで切替（既存のシェイプ・マスク overlay 描画に可視性ゲートを追加する形）。
- P0-4 のレイヤー種別フィルタとは別の、パス単位の表示制御として実装する。

### P1-8 Per-viewport Local Camera / Focal Length / Clip Start-End（Blender Sidebar 相当）

- ペインごとに視点カメラの焦点距離と near/far クリップ範囲を上書きできるようにする
  （`PaneState` の拡張）。P1-3（ペイン単位の表示設定）および P2-6（フォーマット上書き）と
  同じ所有境界に置く。カメラレイヤーのパラメータは変更しない。

### P1-9 Local View / Local Collections の分離と復元（Blender 相当）

- 既存の Isolation overlay（選択ベース）を拡張し、グループ/コレクション単位・ペイン単位の
  分離と復元を提供する。Maya Isolate の状態復元（P1-1）と同じ snapshot 機構を共有する。

### P2 以降

- P2-1 は既存 modal gizmo 入力とドラッグ HUD を再利用する。P2-2 は Diligent パス追加のため
  `MILESTONE_3D_VIEWPORT_HARDENING.md` の P4（Volume/DOF）と合流させる。
- P2-3 tear-off は Dock 分離・マルチビューポートとは別のウィンドウ管理設計が必要なため、
  既存のペイン管理方針を確認してから着手する。
- P2-6 Viewer format overriding は、ペイン単位のレンダーターゲット解像度・Pixel Aspect Ratio
  の上書きと、既存 View テンプレート/ブックマークへの保存要否の設計が先行条件。
- P2-7 Cavity / Studio Shadow は Solid シェーディング用の小さなポストプロセス
  （World / Screen の2方式、Ridge / Valley の強度）。
- P2-8 View Regions は IRR（部分レンダー）と違い表示のみをクリップする。ROI の
  描画スキップ機構を表示クリップにも流用できるかを P0-3 で確認する。
- 未確認候補: Fly / Walk navigation、Annotations（VP 注釈）、Measurement overlay、
  View Lock（Lock to Object / Camera to View）、X-Ray 不透明度スライダ、
  SteeringWheels、Sample Points、Dope Sheet in Viewer、Show Flags。
  着手前に現行コードでの有無を再確認する。

## 受入条件

- 追加した各操作が、既存のナビゲーション・ギズモ操作と競合しない
  （マーキー、パン、ズーム、Orbit、モディファイア併用）。
- 追加した表示状態は、Composition を再読込しても既存データを変更しない
  （ビューポート専用 override は保存対象を明示する）。
- 新規 signal/slot、QtCSS、QImage、QPainter 合成を追加しない。
- ドラッグ中のフレームで新規の大きな確保を発生させない。
- D3D12 と Vulkan で同じ表示・操作結果になる。
- ビルド・実機確認はユーザーの明示指示後に実施する。

## 実装順序の推奨

- **実装順序の推奨:** P0-1 / P0-2（ナビゲーション基礎）→ P1-5 / P1-6（表示ポストプロセスと
  チャンネルバリアント。既存経路の小拡張で受入が容易）→ P0-3 IRR → P0-4 / P1-1 / P1-2 / P1-7 →
  P1-3 / P1-8 / P1-9（ペイン単位の状態管理をまとめて実施）、以降 P2。

## 検証状況

- 2026-09-22: 分析（`docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`）のみ完了。
  実装・ビルド・実機確認は未実施。
- 本環境では git が起動しないため、差分・コミット・gitlink の確認は未実施。
- 2026-09-22: P0-1 / P0-2 を着手。`CompositionRenderController.ixx/cppm` に
  Box zoom / Tumble pivot の公開 API と modality ガード、render loop の
  target 差し替え、overlay 描画、mouse handler 経路、Esc 経路を追加。
  `ShortcutBindings` に `ViewBoxZoom` / `ViewTumblePivotUnderCursor` を
  追加（`Count = 155`）。`CompositionEditor` に right-click cancel、
  command palette、keyPressEvent 経由の発動を追加。`docs/planned/P0_DESIGN_NOTES_2026-09-22.md`
  に着手前メモ、 `Insight.md` に 2026-09-22 の着手記録を追加。
  ビルド・実機確認は未実施（ユーザー明示指示待ち）。
- 2026-09-22: P0-3a（矩形 ROI のみ）を着手。実装前の再照合で
  `RenderContext::roi` は CompositionRenderController から現在呼ばれて
  いないことが判明したため、当初の「RenderContext に矩形を注入」計画を
  「矩形保持 + overlay + HUD のみ」に切り替え、RenderContext 統合は
  P0-3b（または別マイルストーン）に分離。`CompositionRenderController.ixx/cppm`
  に `setInteractiveRenderRegion / clearInteractiveRenderRegion /
  isInteractiveRenderRegionActive / interactiveRenderRegion /
  setInteractiveRenderRegionResolutionScale /
  interactiveRenderRegionResolutionScale` およびハンドル hit-test /
  drag API（`interactiveRenderRegionHandleAt /
  beginInteractiveRenderRegionDrag / updateInteractiveRenderRegionDrag /
  endInteractiveRenderRegionDrag / cancelInteractiveRenderRegionDrag /
  isInteractiveRenderRegionDragActive`）を追加。overlay に矩形枠 +
  8 ハンドル + 移動 hitbox を描画。HUD は既存 `setInfoOverlayText` を
  使用し `"IRR"` タイトル + `"N% res  W x H"` 詳細を表示。マウス
  ハンドラに IRR ドラッグ modality を追加。`ShortcutBindings` に
  `ViewInteractiveRenderRegion` を追加（`Count = 156`、既定キー
  `Ctrl+Shift+R`）。`CompositionEditor` に keyPressEvent トグル、
  right-click クリア、コマンドパレット、mousePressEvent のハンドル
  ヒットテスト発動を追加。`Insight.md` に P0-3a の着手記録と
  RenderContext 統合先送り理由を記載。ビルド・実機確認は未実施。
- 2026-09-22: P0-3b.0（`RenderContext` getter 追加のみ）を着手。
  `docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`
  decision doc の D8（getter のみ、setter なし）に従い、
  `CompositionRenderController.ixx` に `import Artifact.Render.Context`
  と `const RenderContext& renderContext() const` を追加。
  `CompositionRenderController.cppm` に `Impl::renderContext_` を
  所有させ、`initialize` で `setMode(RenderMode::Editor)` を呼び、
  `destroy` で `renderContext_.reset()` を呼ぶ。ROI 矩形はまだ
  pipeline に流さない（P0-3b.1 の範囲）。`Insight.md` に着手記録を
  追加。ビルド・実機確認は未実施。
- 2026-09-22: P0-3b.1（render path への RenderContext 同期）を着手。
  `CompositionRenderController::Impl::renderOneFrameImpl` の冒頭
  （host 可視性チェック直後）に同期ブロックを追加し、毎フレーム
  `setViewportSize(hostWidth_, hostHeight_)` →
  `canvasSize = composition->effectiveCompositionSize()` →
  `setZoom(renderer_->getZoom())` → `setPan(renderer_->getPan)`
  → `setResolutionScale(interactiveRenderRegionActive_ ?
  interactiveRenderRegionResolutionScale_ : 1.0f)` →
  `setROI(interactiveRenderRegionActive_ ? RenderROI(rect) : RenderROI())`
  の順で `RenderContext` を更新。`setROI` 1 回で `updateViewportROI`
  と `updateScissorROI` が連動発火するため、二重計算を回避。IRR
  非アクティブ時は空 `RenderROI()` を渡し full-frame にフォールバック
  （decision doc D6 P0-3b 段階 = 矩形外キャッシュ保持）。
  composition の size は `composition->size()` が存在しないため
  `effectiveCompositionSize()` を使用。`currentFrame` は同期ブロック
  に含めず default (0) のまま。矩形はまだ `RenderPipeline::renderComposition`
  に届かず（P0-3b.2 の範囲）、`RenderContext.viewportROI / scissorROI`
  の更新だけが効果。`Insight.md` に着手記録を追加。ビルド・実機確認は
  未実施。
- 2026-09-22: P0-3b.2（`RenderPipeline::renderComposition` に `RenderROI`
  引数追加 + `SetScissorRects` 呼び出し + `ArtifactIRenderer::setViewportRect`
  適用）を着手。`ArtifactRenderLayerPipeline.ixx` に
  `Artifact.Render.ROI` を import し、`renderComposition` のシグネチャに
  `const RenderROI& renderROI = RenderROI()`（デフォルト引数）を追加。
  cppm 実装で `renderROI` が空でないとき `ctx->SetScissorRects` を呼び、
  `impl_->width_/height_` を render target size として渡す。スタブで
  あるため、既存呼び出し側への影響なし（grep で他からの呼び出しが
  無いことを確認）。CompositionRenderController 側は `renderOneFrameImpl`
  の comp あり入口に `irrScissorApplied` フラグ +
  `setViewportRect(s.x, s.y, s.w, s.h, hostWidth, hostHeight)` を追加し、
  `present()` 直後に `setViewportRect(hostWidth_, hostHeight_)` で
  full-frame に restore。`ArtifactIRenderer` の `setViewportRect` 5引数版
  が既存の `SetViewports + SetScissorRects` 一括呼び出し経路を使うため、
  新規 API を追加せずに済む。`Insight.md` に着手記録を追加。ビルド・実機
  確認は未実施。

## 関連文書

- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`
- `docs/planned/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_TODO_2026-09-04.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
- `docs/planned/P0_DESIGN_NOTES_2026-09-22.md`（P0 着手前調査メモ）
- `docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`（P0-3b/c 設計統合 decision doc）
- `Insight.md`（2026-09-22 の項目）
