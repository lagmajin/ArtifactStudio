# ビューポート DCC パリティ分析（C4D / Houdini / Maya / Autograph / Blender / その他 → ArtifactStudio）

**最終更新:** 2026-09-26

更新内容: 露出コントロール P1-5 とカラーサンプルバー P1-12 が実装済みであることを実コード照合で確認。

## 目的

C4D / Houdini / Maya のビューポート機能を公式ドキュメントから棚卸しし、ArtifactStudio の
Composition Viewport に不足している機能を特定して導入候補を優先度付けする。

## 情報源（2026-09-22 取得）

| DCC | ページ |
| --- | --- |
| Houdini | `sidefx.com/docs/houdini20.0/ref/windows/displayopts_3d.html`（Display Options (3D viewer)）、`/docs/houdini/basics/view.html`（Viewing the scene）、`/docs/houdini/model/aids.html`（Snapping, construction plane, and alignment）、`/docs/houdini/basics/radialmenus.html`（Radial menus） |
| Maya | `help.autodesk.com/.../GUID-9BBB6035-...`（Viewport 2.0 Options）、`.../GUID-C6188583-...`（Panel menu: Shading） |
| C4D | `help.maxon.net/c4d/2026/en-us/Content/html/5593.html`（HUD）、`/c4d/en-us/Content/html/5860.html`（Display）、`/DBASEDRAW-BASEDRAW_GROUP_FILTER.html`（Filter）、`/52991.html`（Viewport Solo Mode）、`/c4d/r25/en-us/Content/html/OSNIPER.html`（Interactive Render Region）、`/45030.html`（Options） |
| Autograph | `help.maxon.net/ag/en-us/Content/html/Category_Viewer.html`（Viewer）、`/Connecting_and_Navigating.html`、`/Creation_and_editing_tools.html`、`/Path_overlays.html`、`/Vierwer_format_override.html` |
| Blender | `docs.blender.org/manual/en/latest/editors/3dview/display/shading.html`（Viewport Shading）、`/display/overlays.html`（Viewport Overlays）、`/3dview/sidebar.html`（Sidebar）。Navigation は `display` 配下の目次から確認（`editors/3dview/navigation/index.html` は 404） |
| 3ds Max / Unreal / Nuke | 新規調査はせず、`docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` の比較表（2026-08-15 更新）を引用 |

注: C4D の Display / Options / Filter / HUD ページはナビゲーション領域が大きく本文抽出が部分的なため、
機能名の完全一致は未確認。Houdini / Maya / Autograph は主要項目を本文から取得できた。

## 調査方法

1. 公式ドキュメントの見出し・設定名を列挙し、カテゴリへ分類する
   （表示/シェーディング、オーバーレイ/ヘルパー、ナビゲーション、選択/分離、スナップ/作業平面、
   診断/可視化、レンダリング品質、ビューポート管理）。
2. 現行コードを静的照合する。対象は `ArtifactCompositionRenderController` の公開 API、
   `ArtifactViewMenu`、`ArtifactCompositionEditor`、および既存の監査文書。
3. 各機能を「実装済み / 部分 / なし」で判定し、根拠（API 名・ファイル）を付ける。
4. 導入候補は AGENTS.md の制約（新規 signal/slot 禁止、QtCSS/QImage/QPainter 合成禁止、
   ホットパス確保の抑制、固定キー禁止）を満たす形へ落とし込む。

## DCC 側の機能カテゴリ（要約）

### Houdini

- **Display Options (3D viewer) タブ**: Markers（Points/Primitives/Vertices/Draw Boundaries/UV、
  visibility level メニュー、Scene/Selected/Ghost/Display Model/Current Model/Template の
  カテゴリ別設定、Link toolbar）、Guides、Visualize、Geometry（Tessellation/Volumes/Wireframe/
  Particles/Instancing/Normals & Tangents）、Scene（Scene Information/Onion Skinning/Color
  Correction/Viewport Split）、Camera（Clipping/Depth of Field/Foreground Image）、
  Lights（Lighting/Headlight/Shadows/Reflections）、Material（Effects/Default Material/
  Material Assignment/Interactive Update Materials）、Fog（Uniform/Volumetric/Bloom/Node
  Override）、Grid（3D/Ortho/Texture）、Background（General/Video Texture）、
  Texture（Cache/2D/3D）、Optimize（Culling/Crowd Agents/Interactivity）。
- **ナビゲーション**: Space+Tumble / Dolly / Track / Tilt、Ctrl+Alt+Box zoom、
  Ctrl+Alt+Box crop、Ctrl+Alt+Screen pan、Space+Z（カーソル下に tumble pivot）、
  Space+H/G/F（Home all / Home selected / Frame）、Set Pivot / Set Pivot And Center、
  Ortho/Perspective トグル、Top/Bottom/Front/Back/Right/Left/Persp/UV ビューポート切替、
  Single/Quad トグル、Tear off viewport copy、Ghosted objects、Color correction、
  Material display toggle、Display toolbar。
- **スナップ/作業平面**: Reference plane と Construction plane（ハンドルで移動/回転、
  面・辺・点へ整列、viewing plane に整列、既定へ戻す）、Quick-views / quick-planes、
  Snap Options メニュー、Align handles to geometry（primary/secondary orientation）。
- **Radial menus**: X（Snapping controls）、C（current menu）、V（View controls）、
  N（Network editor の Navigation）、8 スロット、サブメニュー階層、テンキー選択、
  スクリプト生成（`hou.SceneViewer.displayRadialMenu`）、Radial Menu Editor。

### Maya

- **Panel menu: Shading**: Wireframe、Smooth Shade All / Selected Items、Flat Shade All /
  Selected Items、Bounding Box、Use default material、Wireframe on Shaded、X-Ray、
  X-Ray Joints、X-Ray Active Components、Cycle rig display mode（Alt+A）、Color Index Mode、
  Backface Culling、Smooth Wireframe、Hardware Texturing、Hardware Fog、Depth of Field、
  Apply Current to All。
- **Viewport 2.0 Options**: Default Lighting / Light Intensity、Performance（Consolidate
  World、GPU Instancing、Light Limit）、Transparency Algorithm（Simple / Object Sorting /
  Weighted Average / Depth Peeling）、アンチエイリアス、SSAO、SSR、Motion Blur、Culling、
  Isoline、Shadow、Hardware Fog、Depth of Field、Hold-Out、X-Ray Mode、X-Ray Joint Display、
  Lighting Mode（All / Default / Active / None / Full Ambient）、Single-sided Lighting、
  Render Mode（wireframe / shaded / textured / wireframe on shaded）、Render Override、
  Object Type Filter（Components / Ornaments / Misc UI）。

### C4D

- **Display / Options / Filter**: シェーディングモードと表示エレメントの種類別フィルタ、
  Options 側の画質系トグル（SSAO / FXAA などの系統。本文抽出が部分的なため個別名は未確定）。
- **Viewport Solo Mode**: ビューポート単位の分離表示。
- **Interactive Render Region (IRR)**: ビューポート上に自由配置・拡縮できる描画領域。
  解像度スライダで品質を調整し、変更のたびに再レンダー。Alpha Mode、Lock to View、
  Gadget Overlay、シーンへ保存、ホットキー。
- **HUD**: オブジェクト/パラメータの常設表示と操作（ページ本文の抽出は部分的）。
- **Workplane / Snapping / Quantize**: 作業平面とスナップ、量子化移動。
- **Camera Navigation プリセット**: C4D / Maya / Blender / Unreal 互換のナビゲーション切替。

### Autograph（2026-09-22 追補）

- **Viewer 接続と比較**: プライマリ／セカンダリ接続スロット（コンポジション以外も接続可）、
  Lock / Freeze、2 要素の比較（ブレンドモード＋コントロールウィジェット）。
- **ズーム/ナビゲーション**: ズーム%（手動入力）、Auto-fit（f キー）、100%、
  回転の 90/180/270 度スナップ、Zoom = Ctrl/Cmd+Alt+LMB またはホイール、
  Pan = Alt+LMB / MMB / Space+LMB。
- **Channel Selector**: RGB / R / G / B / Alpha（premultiplied）、**Luminance（Rec 709）**、
  **RGB Straight / R / G / B Straight（unpremultiplied）**、**Matte（アルファを赤へ加算）**。
  ホバーでモードを仮プレビューし、左右で戻る。
- **Post-processing（露出系）**: Gain（-6〜+6 stop）、Gamma（0〜5）、Saturation（0〜4）を
  ビューポートに掛けて HDR の明部・暗部を確認する。全体トグルと個別スイッチ付き。
  エンジンは linear / 32bit 浮動小数で、表示範囲外の輝度をピッカーで保持する。
- **Viewer Format Overriding**: ビューポート単位で解像度・Pixel Aspect Ratio を上書き。
  Source フォーマット、プリセット、ユーザー定義形式のリスト。同一コンポジションを
  3 ビューポートで別定義にレンダーできる（Responsive Design video）。
- **Path Overlays**: コンポジション内の全パス（Shape Layer / Mask / その他
  （follow-path 軌跡など））の輪郭を表示し、種類ごとに表示可否を切替。
  Shape Contour Visibility は Always / Never / Hovered or selected / Selected Layers の4モード。
  Transform / Layout / Motion-Path ツールでもパス選択が可能（hover=シアン、
  active=黄、他の選択=オレンジ、locked=白）。選択矩形は shift 追加 / ctrl+shift 除外。
  パス修飾子（Swirl 等）はヒットテストに反映されない。
- **Creation and Editing Tools**: Draw（接線ハンドル、Shift で 45 度スナップ、
  Ctrl で接線ブレイク）、Feather point（法線方向ハンドル、形状点とのリンク切替）、
  Preset / Procedural Shape、Add / Remove Point、Selection（矩形選択、ctrl+shift で除外）、
  Reset Tangent（cusp）、Open / Close、Set First Point。
- **その他（インデックス確認のみ）**: Render Options、Side Panels and Side Bars、
  Color Picker、Timeline Control、3D Mode、レイヤー単位の Bounding Box 表示、
  Filter and Supersampling、Preserve underlying Transparency、Guide Layer、
  Motion Path / Motion Path Tool、Retiming。

### Blender（2026-09-22 追補）

- **Shading modes**: Wireframe / Solid / Material Preview / Rendered。Solid は Studio lighting
  と MatCap、Background（Theme/World/Custom）、Backface Culling、Outline、Specular
  Highlighting、**X-Ray（Alt+Z、不透明度スライダ付き）**、Shadow（darkness / direction /
  offset / focus）、Depth of Field（アクティブカメラの設定を VP で使用）、
  **Cavity（World / Screen / Both、Ridge / Valley）**。Material Preview は HDRI 環境
  （rotation / world space lighting / strength / world opacity / blur）、**Render Pass
  selector**、Compositor プレビュー（Disabled / Camera / Always）。
- **Overlays**: Guides、Objects、Geometry（edge/face/vertex 関連）、Viewer Node、Motion
  Tracking、Mesh Edit Mode、Shading、**Mesh Analysis**、Measurement、Normals、Freestyle、
  Sculpt / Vertex Paint / **Weight Paint（Weight Contours 含む）**、Texture Paint、
  Pose Mode（Fade Geometry）、Grease Pencil（Onion Skin、Fade Inactive Layers/Objects、
  Edit Lines、Handles、Canvas grid）。
- **Navigation / 表示管理**: Fly / Walk Navigation、Align View、Perspective/Orthographic、
  **Local View（isolate）**、Camera View、Viewpoint、**View Regions（表示クリップ領域）**、
  3D Cursor、Snapping、Proportional Editing、**Object Type Visibility**、Viewport Gizmos、
  Viewport Overlays、Measure、Viewport Render Image。
- **Sidebar（N パネル）**: Item（Transform）、Tool、View（**Focal Length**、
  **Clip Start/End**、**Local Camera（ビューポート固有のカメラ）**、Passepartout、
  **Render Region（Ctrl+B）**、View Lock（**Lock to Object** / To 3D Cursor /
  **Camera to View** / Rotation）、3D Cursor（location / rotation mode）、
  **Collections（ビューポート単位の可視性と isolate）**、Annotations、Global Transform。

### その他（3ds Max / Unreal / Nuke — 既存監査からの引用）

`docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` の比較表（2026-08-15 更新）による。

- **3ds Max / Unreal**: Viewport Background Image（部分）、Viewport Clipping Planes（部分）、
  **Show Flags（要素別表示切替）なし**、Immersive Mode 実装済み、**SteeringWheels なし**、
  Action Centers（部分）、Work Plane Auto-Align なし、Isolate Selection with State Restore（部分）。
- **Nuke**: **Sample Points（永続サンプル点の RGBA 表示）なし**、Dope Sheet in Viewer なし、
  Layer Cop なし、Buffer Visualization は `ViewportChannelDisplayMode` 拡張で到達、
  **Pre-render Region なし**（C4D IRR と同じく ROI + Progressive の再利用候補）。


## 突き合わせ結果（Artifact 現状）

### 実装済み（DCC 相当が既にある）

| 機能 | 相当 DCC | 根拠 |
| --- | --- | --- |
| チャンネル/バッファ分離表示 | Maya Buffer / Nuke Buffer Visualization | `ViewportChannelDisplayMode`（Color/Alpha/RGB+A/R/G/B/Depth/Emission/ObjectId/MaterialId/Albedo(+R/G/B)/Normal(+X/Y/Z)/Velocity(+X/Y)/Position(+X/Y/Z)/UV+U/V） |
| オニオンスキン | Houdini Scene tab | `setShowOnionSkin` / `setOnionSkinFrameCount` / `setOnionSkinOpacity` |
| X-Ray | Maya Shading | `setShowXRayOverlay` |
| グリッド/極座標/等角/自動ステップ | Houdini Grid tab | `setGridSettings` / `setGridPolarMode` / `setGridIsometricMode` / `setGridAutoStepEnabled` |
| ルーラー/原点/ピクセルグリッド/コンポジション外表示 | Houdini Guides | `setShowViewportRuler` / `setViewportOriginMode` / `setShowPixelGrid` / `setShowOutsideComposition` |
| セーフマージン/ガイド/スナップ | Maya・Houdini | `setShowSafeMargins` / `setShowGuides` / `setSnapToGuides` / SmartGuides |
| チェッカーボード/背景モード | C4D Options | `setShowCheckerboard` / `setCompositionBackgroundMode`（Solid/Checkerboard/MayaGradient/Skybox） |
| カメラフラスタム/モーションパス/アンカー/原点/リグ | Maya・Houdini | `setShowCameraFrustumOverlay` / `setShowMotionPathOverlay` / `setShowAnchorCenterOverlay` / `setShowOriginOverlay` / `setShowRigOverlay` |
| マグニファイア/カラーサンプラ/作業カーソル | C4D・Houdini | `setMagnifierEnabled` / `setShowColorSamplerOverlay` / `setWorkCursor*` |
| 参照画像オーバーレイ（ピン留め/フレーム指定） | C4D Background | `setReferenceOverlayImage` / `setReferencePinned` / `setReferenceFrame` |
| マルチビューポート | Houdini Viewport Split / Quad | `PresentationLayout`（Single/TwoUp/FourUp）、`PaneState` |
| プレビュー品質/LOD/デバッグ | C4D Options | `setPreviewQualityPreset` / `setLODEnabled` / `setDebugMode` |
| カメラブックマーク / View テンプレート / 選択セット | Maya Bookmarks | `ArtifactViewMenu` の camera bookmark / view template / selection set |
| ピエメニュー（放射状） | Houdini Radial menus | `pieMenuVisible_` / `confirmPieMenuOverlaySelection` |
| Isolation（選択以外を描画しない） | Maya Isolate Select | `setShowIsolationOverlay` |
| オーディオ波形/スペクトラム表示 | （編集系） | `setShowAudioWaveformOverlay` / `setShowAudioSpectrumOverlay` |
| ズーム%表示 / 100% / フィット | Autograph Zoom Factor | `ArtifactViewMenu` の Zoom In / Out / 100% / Fit to Screen |
| キャンバス回転＋角度スナップ | Autograph 回転スナップ | `rotateCanvas` と回転スナップ/リセット（`docs/planned/MILESTONE_VIEWPORT_CANVAS_ROTATION_2026-06-27.md`） |
| A/B 比較（ブレンド込み） | Autograph Compare two elements | `setCompareMode` / Contents Viewer の Wipe・Split・Difference。Composition Viewport は 2026-09-19 の検討モック段階 |

### 部分実装・未実装（導入候補）

| 機能 | DCC | 現状 | 根拠・備考 |
| --- | --- | --- | --- |
| ビューポートの種類別フィルタ | C4D Filter | **なし** | `CompositionLayerRenderFilter` は `All` / `SelectedOnly` の2値のみ。種別（ライト/カメラ/Null 等）ごとの表示トグルが無い |
| Interactive Render Region / Pre-render Region | C4D IRR / Nuke | **なし** | `ArtifactRenderROI` と `ProgressiveRenderer`（`ArtifactFrameCache.cppm`）は存在するが、VP 上で矩形を移動/拡縮して部分レンダーする UI が無い |
| Box zoom / Box crop（画面窓） | Houdini | **なし** | 該当 API なし。既存のマーキー入力とズーム補間の再利用が可能 |
| Tumble pivot をカーソル下に設定 | Houdini（Space+Z）/ Maya | **なし** | `setPivotUnderCursor` 相当なし。既存 orientation 回転にピボット状態を追加する形 |
| Ghosted context display | Houdini（Ghost Scene Geometry 等） | **なし** | X-Ray は全体減衰で、「編集中の対象以外をゴースト表示」する段階設定が無い |
| Isolate Select の状態復元 | Maya | **部分** | `setShowIsolationOverlay` はあるが、分離前の表示状態スナップショット復元が無い（`docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` に記載） |
| Per-viewport 設定 + Apply to all split views | Houdini | **未確認／部分** | `PaneState` はあるが、設定の適用範囲（全ペイン/単一ペイン）のセマンティクスは未確認 |
| Viewport tear-off copy | Houdini | **なし** | Dock 分離とは別の「ビューポート複製ウィンドウ」 |
| Construction plane handle（面/辺/点へ整列） | Houdini | **部分** | `ArtifactConstructionLayer` は editor-only ガイドで、インタラクティブ作業平面ではない |
| Workplane スナップ / Quantize | C4D | **部分** | グリッド/ガイドスナップはあるが、作業平面基準のスナップ・量子化は無い |
| Cycle rig display mode | Maya（Alt+A） | **部分** | X-Ray と Rig overlay は個別にあるが、3モード循環が無い |
| Bounding Box / Wireframe on Shaded / Smooth Wireframe | Maya | **部分** | 3D モデルレイヤーの `render.mode` と LOD に依存。ビューポート全体の表示モード切替ではない |
| Backface culling の表示トグル | Maya | **未確認** | シェーダ側の cull はあるが、ビューポート設定としての切替は未確認 |
| Hardware Fog / Volumetric fog / Bloom | Maya・Houdini Fog tab | **なし** | ビューポート用フォグ/ブルームのパスが無い |
| Per-viewport Depth of Field | Maya | **部分** | カメラ DOF パラメータはあるが Diligent の DOF pass は未統合（`docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md` P4） |
| Object Type Filter（レンダー/ビューポート） | Maya | **部分** | Render Queue 側の除外はあるが、VP 上の要素種別除外 UI は無い |
| C4D HUD 相当の常設パラメータ表示 | C4D | **部分** | info overlay とドラッグ HUD はあるが、任意パラメータを常設表示して直接ドラッグする仕組みは無い |
| Radial menu の用途別プリセットと割当 | Houdini（X/C/V/N） | **部分** | ピエメニューはあるが、スナップ専用/ビュー専用メニューの定義・カスタマイズは無い |
| Marker visibility level | Houdini Markers | **部分** | `setLineDebugKindVisible` の個別トグルのみで、レベル段階が無い |

## 導入候補（優先度）

導入は「既存インフラの再利用」「データモデル非変更」「操作は ShortcutBindings のローカル
コンテキストへ登録」を前提に段階化する。詳細なフェーズと受入条件は
`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` に記録した。

### P0 — 確認・ナビゲーションの基礎（低リスク、既存 API 再利用）

1. **Box zoom / Box crop**（Houdini）: ドラッグ矩形でズーム/クロップ。既存のマーキー入力、
   `zoomAtFactor`、pan 状態、プレビュー品質の操作中ダウンサンプルを再利用。
2. **Tumble pivot under cursor**（Houdini / Maya）: カーソル下のジオメトリ点を一時ピボットにし、
   既存 orientation 回転の中心を差し替える。ピボット位置の小マーカー表示は既存 overlay API。
3. **Interactive Render Region**（C4D IRR / Nuke Pre-render Region）: 既存 `ArtifactRenderROI` と
   `ProgressiveRenderer` を再利用し、VP 上の矩形移動/拡縮＋解像度スライダ（HUD）だけを追加。
4. **ビューポート タイプ別フィルタ**（C4D Filter）: レイヤー種別ごとの表示可否を
   ビューポート専用 override として保持（レイヤーの `visible` は変更しない）。

### P1 — 編集ワークフロー

5. **Isolate Select の状態復元**（Maya）: 分離開始時の表示状態を snapshot し、解除で復元。
6. **Ghosted context display**（Houdini）: 編集対象以外を低不透明度で残す段階表示。
7. **Per-viewport 設定 + Apply to all split views**（Houdini）: ペイン単位の表示設定と一括適用。
8. **Maya 風シェーディングトグル**: Wireframe on Shaded / Backface Culling / Bounding Box /
   Cycle rig display（Alt+A 相当）を既存の `render.mode`・LOD・X-Ray・Rig overlay に接続。
9. ~~**Viewer exposure controls**（Autograph）~~ → **2026-09-26 実装済み（P1-5）**。Gain / Gamma / Saturation を表示専用 compute 段として追加。`Color` モードの `finalizeGpuRenderToViewport` で scratch surface に書き、`presentationSRV` 選択時のみ差し込むため出力・サンプリング値は不変。ビルド・実機未確認。
10. **チャンネル表示の Straight / Luminance / Matte バリアント**（Autograph）:
    `ViewportChannelDisplayMode` へ Unpremultiplied / Luminance（Rec 709）/ Matte を追加。
11. **パス overlay の可視性モードと種類別フィルタ**（Autograph）: Always / Never /
    Hovered or selected / Selected Layers の4モードと、shape / mask / その他の切替。
12. **Per-viewport Local Camera / Focal Length / Clip Start-End**（Blender Sidebar）:
    ペイン単位のカメラ・焦点距離・クリップ範囲（P1-3 / P2-6 と関連）。
13. **Local View / Local Collections の分離と復元**（Blender）:
    Isolation overlay を選択ベースから、コレクション/グループ単位・ペイン単位の分離へ拡張。

### P2 — 品質・診断

12. **C4D HUD 相当のパラメータ常設表示**: 選択オブジェクトの主要パラメータを VP に常設表示し、
    ドラッグで編集（既存の modal gizmo 入力へ接続）。
13. **Hardware fog / volumetric fog / bloom**（Maya・Houdini Fog tab）: Diligent パス追加。
14. **Viewport tear-off copy**（Houdini）: 複製ウィンドウ（Dock 分離とは別設計）。
15. **Construction plane handle**（Houdini）: 作業平面の移動/回転と面・辺・点への整列。
16. **Object Type Filter のビューポート拡張**（Maya）: 要素種別の除外を VP 側にも適用。
17. **Viewer format overriding**（Autograph）: ビューポート単位の解像度 / Pixel Aspect Ratio
    の上書き（Responsive Design 相当）。
18. **View Regions（表示クリップ領域）**（Blender）: IRR と別の、表示をクリップする矩形。
19. **Cavity / Studio Shadow**（Blender Solid shading）: 凹凸強調（World/Screen）と簡易影。
20. **Fly / Walk navigation**（Blender）: WASD 系の一人称ナビゲーション。
21. **SteeringWheels / Sample Points / Dope Sheet in Viewer / Show Flags**
    （3ds Max / Unreal / Nuke、既存監査引用）。Show Flags は P0-4 で部分的に代替可能。
- 未確認の追加候補: X-Ray の不透明度スライダ、Annotations（VP 注釈）、Measurement overlay、
  View Lock（Lock to Object / Camera to View）、Backface culling の表示トグル。

## 実装制約（AGENTS.md 準拠）

- 新規 signal/slot を作らない。既存の command / service / event と公開 API を再利用する。
- QtCSS（`setStyleSheet`）、`QColorDialog`、`QImage` 化、`QPainter` 合成を新規追加しない。
  オーバーレイは既存の Diligent 描画 API（`drawSolidLine` / `drawSolidRect` / `drawText`）に限定する。
- ホットパスで確保を増やさない。追加する描画は矩形・線・テキストの少数に留める。
- 固定キーのハードコードを避け、新しい操作は `ShortcutBindings` に `ShortcutId`、既定キー、
  表示名、永続化キー、所有コンテキストを登録してローカルコンテキストで解決する。
- Composition Viewport の採用モックを根拠に、キャンバス内の既存ギズモ・HUD を変更しない。
- ビルド・実機確認はユーザーの明示指示が必要（本分析は静的照合のみ）。

## 検証状況

- 2026-09-22: 公式ドキュメントの取得と、現行コードの静的照合を実施。ビルド・実機確認は未実施。
- C4D の Display / Options / Filter / HUD は本文抽出が部分的で、個別設定名の完全一致は未確認。
- `Box zoom` / `tumble pivot` / `tear-off` / `Hardware Fog` などはコード検索で該当なしを確認したが、
  実装が別名で存在する可能性は排除していない。
- この環境では git が起動しないため、差分・コミットの確認は未実施。

## 関連文書

- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`（導入計画）
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`（ビューポート総合監査）
- `docs/planned/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_TODO_2026-09-04.md`（ナビゲーション契約）
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`（3D レンダリング強化）
- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md`
- `docs/analysis/THREED_LAYER_FEATURE_GAP_DCC_COMPARISON_2026-08-08.md`

| Camera Navigation プリセット切替 | C4D | **未確認** | Blender 系 Alt+LMB が既定。C4D/Maya 互換プリセット切替の有無は未確認 |
| Viewer exposure controls（Gain / Gamma / Saturation） | Autograph Post-processing | **実装済み（2026-09-26）** | P1-5 として表示専用 compute 段へ実装。`finalPresentSRV` / `lastPresentedReadbackSRV_` を無変更で保つため出力・color sampler・scopes には影響しない。UI は View > Overlays > 露出調整。ビルド・実機未確認 |
| チャンネル表示の Straight / Luminance / Matte バリアント | Autograph Channel Selector | **部分** | `ViewportChannelDisplayMode` は premultiplied 前提。Unpremultiplied（Straight）、Luminance（Rec 709）、Matte（αを赤へ加算）の表示モードが無い |
| Viewer format overriding（VP 単位の解像度/PAR 上書き） | Autograph | **なし** | 同一コンポジションを複数ビューポートで別定義に表示する仕組みが無い（Responsive Design 相当） |
| パス overlay の可視性モードと種類別フィルタ | Autograph Path Overlays | **部分** | シェイプ/マスクの overlay はあるが、Always / Never / Hovered or selected / Selected Layers の4モードと種類別（shape/mask/その他）の切替が無い |
| Viewer lock / freeze（接続アイテムの凍結） | Autograph | **部分** | Reference overlay の pin / frame 指定が近いが、ビューポート接続スロットの凍結ではない |
| ホバーによるチャンネル仮プレビュー | Autograph | **なし** | チャンネル selector のホバーで一時的に別チャンネルを表示する UX |
| X-Ray の不透明度スライダ | Blender（Alt+Z） | **部分** | `setShowXRayOverlay` はトグル。透過度の段階調整は未確認 |
| Per-viewport Local Camera / Focal Length / Clip Start-End | Blender Sidebar | **部分** | ペイン単位のカメラ・焦点距離・クリップ範囲は未確認（P1-3 / P2-6 と関連） |
| View Lock（Lock to Object / Camera to View） | Blender | **部分** | カメラフラスタム overlay はある。orbit 中心のオブジェクトロック、カメラをビューへ貼り付ける経路は未確認 |
| Local View / Local Collections（ビューポート単位の分離） | Blender | **部分** | Isolation overlay は選択ベース。コレクション/グループ単位・ペイン単位の分離と復元は未実装 |
| View Regions（表示クリップ領域） | Blender | **なし** | IRR（部分レンダー）と別の、表示をクリップする矩形領域 |
| Cavity / Studio Shadow | Blender Solid shading | **なし** | 凹凸強調（World/Screen）と簡易影の表示パスが無い |
| Mesh Analysis / Measurement overlay | Blender | **部分** | Density heatmap はあるが、メッシュ解析・計測オーバーレイは未確認 |
| Fly / Walk navigation | Blender | **なし** | WASD 系の一人称ナビゲーション |
| Annotations（ビューポート注釈） | Blender | **部分** | レイヤー note はあるが、VP 上の注釈描画は別 |
| SteeringWheels / Show Flags / Sample Points / Dope Sheet in Viewer | 3ds Max / UE / Nuke | **なし** | 既存監査（2026-08-15）の通り。Show Flags は P0-4 のタイプ別フィルタで部分的に代替可能 |
