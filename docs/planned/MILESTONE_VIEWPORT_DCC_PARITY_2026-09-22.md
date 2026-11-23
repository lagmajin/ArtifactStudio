# M-VP-DCC-1: ビューポート DCC パリティ導入（C4D / Houdini / Maya）

**最終更新:** 2026-09-26

**ステータス:** P0-1 / P0-2 / P0-3（a / b.0 / b.1 / b.2 / d）/ P0-4 / P1-5 / P1-10 着手済み（コード変更のみ、ビルド・実機確認はユーザー明示指示待ち）。P0-3d.1（ProgressiveRenderer 統合）/ P1-1〜P1-4 / P1-6 の readback 反映 / P1-7〜P1-9 / P1-11 以降 / P2 は未着手（分析完了）。
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
| P0-3 | Interactive Render Region（ROI + Progressive） | **P0-3a / P0-3b.0 / P0-3b.1 / P0-3b.2 / P0-3d 実装済み（ビルド・実機未確認）** / P0-3d.1（ProgressiveRenderer 統合）は未着手 | `ArtifactFrameCache` / ROI / overlay / `ArtifactCompositionRenderController` / `ArtifactRenderLayerPipeline` |
| P0-4 | ビューポート タイプ別フィルタ | **実装済み（コード変更のみ、ビルド・実機未確認）** | `ArtifactCompositionRenderController` |
| P1-1 | Isolate Select の状態復元 | Not Started | 同上 |
| P1-2 | Ghosted context display | Not Started | 同上 |
| P1-3 | Per-viewport 設定 + Apply to all split views | Not Started | `PaneState` / 表示設定 |
| P1-4 | Maya 風シェーディングトグル | Not Started | 同上 |
| P1-5 | Viewer exposure controls（Gain/Gamma/Saturation） | **実装済み（コード変更のみ、ビルド・実機未確認、2026-09-26）** | 表示専用 compute 段（`ArtifactIRenderer` / `ViewerHelperShaders`） |
| P1-6 | チャンネル表示の Straight / Luminance / Matte バリアント | **enum 拡張済み（コード変更のみ、ビルド・実機未確認）** / readback overlay での実描画反映は別マイルストーン | `ViewportChannelDisplayMode` |
| P1-7 | パス overlay の可視性モードと種類別フィルタ | Not Started | overlay / 表示フィルタ |
| P1-8 | Per-viewport Local Camera / Focal Length / Clip | Not Started | `PaneState` / カメラ状態 |
| P1-9 | Local View / Local Collections の分離と復元 | Not Started | Isolation overlay / 選択管理 |
| P1-10 | Clipping 警告（over/under exposure の false color、HieroPlayer 由来） | **コード変更のみ、ビルド・実機未確認（2026-09-26）** | P1-5 の表示専用 compute 段 |
| P1-11 | スコープ（Histogram / Waveform / Vector）+ ROI（HieroPlayer 由来） | **⚠ 一部実装済（2026-09-26 実コード照合）** — 4 スコープの表示は**既に存在**（`ArtifactColorSciencePanel` の 2×2 ダッシュボード、`ArtifactCompositionEditor` の dialog）。GPU 版 `ScopeComputer` / `Histogram` はソース完全だが `.cppm` がビルド除外。**残るのは ROI 集計と部分 readback のみ**。GPU 化は submodule 変更を要する | 既存 4 スコープ widget / `ArtifactColorSciencePanel` |
| P1-12 | カラーサンプルバー（ソース RGBA 生値、HieroPlayer 由来） | **✅ 実装済み（2026-09-26 実コード照合）** — 再実装不要。`updateColorSamplerOverlay` がカーソル下 1px を `captureCurrentFrameImage()` から読み、RGB / HSL / hex / Layer ID / canvas XY / image pixel を HUD 表示。トグルと状態復元は `ArtifactCompositionEditor` に既存。`finalPresentReadbackSRV_` を読むため今回の露出（P1-5）の影響を受けない |
| P1-13 | OCIO 表示色空間切替（Viewer color transform、HieroPlayer 由来） | **⚠ 一部実装済（2026-09-26 実コード照合）** — OCIO は実依存として**統合済み**、`bakeViewTransformLUT` による 33³ LUT も存在する。ただし適用は `applyDisplayColorTransform` 経由で **composition-space cache 経路の 2 か所だけ**で、メインの `finalizeGpuRenderToViewport` には入っていない。**残るのは present 経路への適用・per-pane 状態・UI 接続**。`setOCIOConfig` は未実装（文書の通り） | `ViewportColorPipeline` / `PaneState` |
| P1-14 | アスペクトマスク（16:9 等の表示専用マスク、HieroPlayer 由来） | Not Started | overlay / safe-area |
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
- **2026-09-26 実装:** `Color` モードの `finalizeGpuRenderToViewport` に表示専用 compute 段を追加した。`finalPresentSRV` と `lastPresentedReadbackSRV_` は無変更のまま維持し、露出結果は `tempUAV()` に書いて `presentationSRV` の選択時のみ差し込むため、color sampler・Color Science・虫眼鏡・RAM preview・Render Queue のいずれにも影響しない。compute は `ArtifactIRenderer::Impl` の自己完結 executor で `ArtifactCore::LayerBlendPipeline` に依存せず、`Artifact` 側のみで完結する（新規 `.cppm` / `.ixx` は追加していない）。PSO と 32 byte parameter buffer は `ArtifactIRenderer::initialize()` で一度だけ作り、フレーム中の初回生成を避ける。設定は `LayeredConfigStore` の `Viewport/Exposure/{Gain,Gamma,Saturation,Enabled}` に保存し、UI は View > Overlays > 露出調整（`QWidgetAction` のスライダーパネルとリセット）。既定値は厳密な恒等変換。ビルド・実機・D3D12/Vulkan の parity は未確認。

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

### P1-10 Clipping 警告（HieroPlayer Clipping warnings 相当）

- 表示画像の under（青）/ over（赤）exposure を false-color の警告表示で示す。
  P1-5 と同じ表示専用ポストプロセス段に追加し、保存・出力・カラーサンプルの読み取り値は変えない。
- 警告閾値は View > Overlays > Viewport Exposure から設定し、トグルは
  `ShortcutBindings` の `Viewport.Composition` ローカルコンテキストで切替える。
- **2026-09-26 実装:** P1-5 compute shader に under threshold（linear luminance）と
  over threshold（linear RGB channel）の比較を追加。既定値は 0.01 / 1.0。
  露出調整を無効にしても clipping warnings は独立して動作する。露出設定が恒等値かつ警告が
  無効なら全画素 dispatch を省略する。PSO と parameter buffer は renderer 初期化時に作り、
  フレーム中の初回生成を避ける。最終合成面、readback、Render Queue は変更しない。
  入力 accumulator は premultiplied のため、変換と閾値判定は
  alpha で戻した straight linear RGB に対して行い、出力 RGB は元の alpha で再乗算する。
  完全透明ピクセルはゼロのまま警告対象外。ビルド・実機確認待ちで、透明エッジの実表示は未検証。

### P1-11 スコープ（Histogram / Waveform / Vector）+ ROI（HieroPlayer Scopes 相当）

- ヒストグラムから開始し、waveform / vector を段階追加する。ROI 矩形でスコープ集計範囲を
  限定できるようにする（IRR 矩形との共有を検討）。
- 集計は GPU reduce または小さな CPU readback に留め、`ImageF32x4_RGBA` 系バッファから
  直接計算する。`QImage` / `QPainter` 合成の新規利用は禁止（AGENTS.md）。
- フレームごとの大規模アロケーションを避け、事前確保済みバッファで集計する（HOT_PATH_RULES）。

### P1-12 カラーサンプルバー（HieroPlayer Color Sample 相当）

- カーソル下ピクセルのソース RGBA 生値（表示変換・exposure 適用前）を常時バーに表示する。
- 1px 程度の readback に限定し、ホットパスでの大容量 readback を避ける。
- **2026-09-26 実コード照合: 満た済み（再実装不要）**。`CompositionRenderController::Impl::updateColorSamplerOverlay` が `captureCurrentFrameImage()` から 1px を読み、`drawColorSamplerOverlay` が RGB / HSL / hex / Layer ID / canvas XY / image pixel を HUD パネルへ描画する。表示トグルは `setShowColorSamplerOverlay`、UI と状態復元は `ArtifactCompositionEditor` に既存。readback は `lastPresentedReadbackSRV_`（露出適用前の合成面）基準なので、表示専用露出（P1-5）を入れても値は変わらない。残る差は HieroPlayer の複数点・常時バー形式への拡張のみで、これは本マイルストーンのスコープ外。

### P1-13 OCIO 表示色空間切替（HieroPlayer Viewer color transform 相当）

- sRGB / rec709 等の表示色空間をビューポート単位で切替える。適用位置は renderer の
  final output 段に固定し、P1-5 の exposure 調整は linear 段（変換前）に置く。
- OCIO 統合の大枠は M-FE-7-2（`setLUT` / `setOCIOConfig`、
  `docs/planned/MILESTONE_REVIEW_WORKSPACE_2026-04-03.md`）と共有し、二重実装しない。

### P1-14 アスペクトマスク（HieroPlayer Viewer masks 相当）

- 16:9 / 1.85:1 等の表示専用マスクを既存 safe-area overlay の派生として追加する。
  最終出力・合成結果には影響しない。

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
- 2026-09-22: P0-3d（`renderPartialRegion(RenderQuality, RenderROI)`
  private 関数 + D4 Preview 強制ダウングレード）を着手。
  `ProgressiveRenderer::setRenderCallback` のシグネチャが
  `std::function<bool(RenderQuality)>` 1 引数固定で decision doc D3
  と互換しないため、ProgressiveRenderer は所有せず
  `CompositionRenderController::Impl::renderPartialRegion` を private
  関数として実装する方針に変更。`ArtifactCompositionRenderController.ixx`
  に `import Artifact.Render.FrameCache;` を追加し `RenderQuality` を
  解決可能に。`Impl` に `previewQualityPreset_` / `irrForcedPreview_` /
  `lastPartialRenderQuality_` / `partialRenderCount_` を追加し、
  `setPreviewQualityPreset` で enum を保存。`setInteractiveRenderRegion`
  入口で `Final` なら `Preview` にダウングレードし `irrForcedPreview_`
  を true に、HUD に `(quality forced to Preview)` を追記。
  `clearInteractiveRenderRegion` で preset を復元。
  `renderContext_.setMode` を `interactiveRenderRegionActive_` 時に
  `RenderMode::Preview` に強制（decision doc D4）。`renderOneFrameImpl`
  の `damageTracker_.clearAll()` 直後で
  `interactiveRenderRegionResolutionScale_` を
  `RenderQuality`（閾値 0.34 / 0.67）にマッピングし、
  `renderPartialRegion(owner, quality, rect)` を呼ぶ。本体は品質記録 +
  `markFullRedraw + invalidateBaseComposite + markRenderDirty` で
  再描画要求を出す。実描画は既存 `renderOneFrameImpl` 経路に委譲し、
  IRR scissor が active なため矩形内だけ再レンダーされる。
  decision doc §3 P0-3d.1 で ProgressiveRenderer 統合または別経路で
  実装する想定。`Insight.md` に着手記録を追加。ビルド・実機確認は
  未実施。

## 関連文書

- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
- `docs/analysis/HIEROPLAYER_GAP_ANALYSIS_2026-09-22.md`（P1-10〜P1-14 の由来。2026-09-22 に P0「Viewer Inspection Controls」として本マイルストーンへ統合決定）
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`
- `docs/planned/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_TODO_2026-09-04.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
- `docs/planned/P0_DESIGN_NOTES_2026-09-22.md`（P0 着手前調査メモ）
- `docs/planned/DESIGN_INTERACTIVE_RENDER_REGION_2026-09-22.md`（P0-3b/c 設計統合 decision doc）
- `Insight.md`（2026-09-22 の項目）
