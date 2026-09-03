# MILESTONE: シェイプ機能拡充（SVG品質・ブール演算・複数シェイプ・性能・頂点編集）

**最終更新:** 2026-09-02

ユーザー承認済みの拡充案 6 項目。段階的に実装する。

| Phase | 項目 | 状態 |
|---|---|---|
| A | 1. SVG グラデーション出力 / 2. SVG ストローク属性 | **実装済み (2026-08-23)、ビルド検証待ち** |
| B | 6. キャッシュのフレームキー化（アニメ時の差分再構築） | 未着手 |
| C | 3. Merge Paths（ブール演算オペレータ） | 未着手（設計メモあり） |
| D | 7. キャンバス頂点編集（メイン VP のシェイプ VP 操作増強） | **未着手（2026-09-02 計画追加）** |
| D-1 | 7a. vertex / tangent / segment overlay を `ArtifactCompositionRenderOverlay` に統合 | 未着手 |
| D-2 | 7b. Rect `cornerRadius` ハンドル、Star `starInnerRadius_` ハンドル、Polygon 頂点ドラッグ挿入を RenderController mousePress/Move に追加 | 未着手 |
| D-3 | 7c. `ToolType::Shape` のプリセット選択 UI（Rect/Ellipse/Star/Polygon/Line/Triangle のアクティブ切替）をツールバー／ツールオプションに追加 | 未着手 |
| D-4 | 7d. メイン VP 上の vertex / segment / tangent 選択 grammar 完成（Shift / Ctrl toggle、空クリック頂点追加、segment 挿入、Proportional 編集） | 未着手 |
| D-5 | 7e. Shape operator stack（TrimPaths / Merge Paths / Offset / Pucker / Rounded / Wiggle / ZigZag / Twist / HandDrawnWobble）の VP 上数値ハンドル／HUD 編集 | 未着手 |
| D-6 | 7f. パス open/closed トグル、smooth toggle、corner ↔ bezier 切替の VP ハンドル化 | 未着手 |
| E | 4. 1レイヤー複数シェイプ（グループコンテンツ） | 未着手（最大・設計レビュー推奨） |

## Phase A — 実装内容（2026-08-23）

### コア（`ArtifactCore`）

- `ShapeTypes.ixx`: `StrokePlacement` enum 追加。`StrokeSettings` に placement / taperStartScale / taperEndScale / isTapered() 追加。`FillSettings` に FillType::Repeating/Mirrored 追加＋gradientStart/End/Angle/Center/Radius フィールド
- `ShapeLayer.cppm`: SVG 出力を `<defs>` ベースのグラデーション対応に拡張
  - `SvgExportContext`（defs 収集 + gradient id キャッシュ）、`gradientFillReference()`（linear/radial を実出力、conic は線形近似、repeating/mirrored は spreadMethod）
  - `elementToSvg()`: gradient fill は `url(#gradN)`、Inside/Outside 配置とテーパー付きストロークは `QPainterPathStroker` による輪郭塗りパスとして出力（Inside=intersected / Outside=subtracted）

### アプリ（`Artifact`）

- `ArtifactShapeLayer::toCoreShapeLayer()`: fill グラデーション全パラメータと stroke align/taper をコア設定へマッピング（中間色縮退を廃止）

### 既知の制限

- テーパーは輪郭化時に均一幅（平均スケールではなく start 幅の輪郭）での近似。可変幅パス生成は将来課題
- Conic グラデーションは SVG 規格外のため線形近似

## Phase C 設計メモ（Merge Paths）

- `ArtifactCore::ShapeOperatorType::Merge` 新設、`ShapeOperator` 派生で mode(Add/Subtract/Intersect/Exclude) 保持
- パスブール演算はコア新規実装が必要（候補: Clipper2 相当の even-odd boolean、または triangulate 前提の領域演算）。`ShapePath` 単位で apply
- アプリ側は既存オペレータ UI 枠組み（createShapeOperator / getLayerPropertyGroups の operator group）に追加するのみ

## Phase D — シェイプ VP 操作増強（2026-09-02 計画追加）

「シェイプレイヤーの VP 操作機能増強」のメイン VP（コンポ VP = `ArtifactCompositionRenderController` / `ArtifactCompositionRenderOverlay`）側の計画。`ArtifactLayerEditorWidget`（LayerEditorPanel）側は 2026-09-02 時点で `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` の分割により `ShapeOverlay` / `ShapeEditSession` / `ShapeDragController` / `ShapeHoverController` / `ShapeParameterController` 等へ抽出済み。本 Phase D はその実装を **メイン VP に移植** する形で進める。

### スコープ方針

- 既存ツールパス（`ArtifactCompositionRenderController` の `isDraggingShapePathVertex_` / `shapePathEditPending_` / `hoveredShapePathVertex_` / `hoveredShapePathTangent_` 等の state 群、`beginShapePathVertexDrag` / `updateShapePathVertexDrag` / `endShapePathVertexDrag` の既存実装）と **`ArtifactLayerEditorWidget` 側の抽出モジュール** を接続する
- 既存の Line 端点ドラッグ (`isDraggingLineEndpoint_` / `draggingLineLayer_`) を踏襲し、Polygon / Star / Rect 系ハンドルは `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` の `ShapeParameterController` をそのまま流用する
- 新規モジュールは作成せず、巨大ファイルの分割は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` 側に委譲。本 Phase D は **既存関数のルート拡張と既存 state の有効化** に留める
- モジュール追加（`ShapeOverlay.cppm` 等）は行わない。CMakeLists 変更を伴わないことで再ビルド影響を最小化

### Phase D-1 — vertex / tangent / segment overlay を `ArtifactCompositionRenderOverlay` に統合

**現状:**
- `ArtifactCompositionRenderOverlay.cppm:1028-1105` でカスタム Polygon の頂点ストローク描画、Line の 2 端点描画、customPathVertices のベジェ描画は実装済み
- ただし **頂点ハンドル / タンジェントハンドル / 選択ハイライト / セグメント挿入マーカー / ホバー強調表示** のオーバーレイは未確認（grep 上このファイルには vertex overlay / hit-area / ハンドルサイズ定数が Shape 用に出てこない）
- RenderController 側は `hoveredShapePathVertex_` / `hoveredShapePathTangent_` を state に持っているが、Overlay への引き渡し経路がない

**実装:**
- `ArtifactCompositionRenderOverlay` に `drawShapeVertexOverlay(layer, viewportPos, selectedVertices, hovered, handleSizePx)` を追加
- RenderController の描画経路から選択中シェイプレイヤーに対して呼び出し、`hoveredShapePathVertex_` 等の state を DTO で渡す
- ハンドルは `PrimitiveRenderer2D::drawSolidCircle` で 8〜10px 相当、`shapeWidth() * shapeHeight() / maxDim` でシェイプサイズに対してスケール
- `customPathClosed_` を反映した開／閉ハイライト、トポロジ数（vertices.size）が表示ハンドルの最大値超過時のフェイルセーフ
- セグメント挿入マーカー（Shift 押下時のみ描画）は別関数で提供

**検証:**
- 2 頂点／3 頂点／複数頂点パスでホバー / 選択ハイライトが期待通り出ること
- グローバル座標とローカル座標の逆変換で 1px 以下の誤差に収まること
- 既存の mask vertex overlay と衝突しないこと（z-order を最後にする）

### Phase D-2 — Rect `cornerRadius` / Star `starInnerRadius_` / Polygon 頂点挿入ハンドル

**現状:**
- `hitTestCornerRadiusHandle()` / `hitTestStarInnerRadiusHandle()` は `ArtifactRenderLayerWidgetv2` 内（LayerEditorPanel）に存在
- RenderController 側は `setSize(width, height)` ドラッグ経路のみで、`cornerRadius` ハンドルと `starInnerRadius_` ハンドルは未実装

**実装:**
- `ArtifactCompositionRenderController` の mousePress / mouseMove / mouseRelease に `RectToolMode::CornerRadius` / `StarToolMode::InnerRadius` / `PolygonToolMode::VertexInsert` を追加（`RectangleToolMode` を拡張するか、`ShapeToolMode` を新設）
- 既存 `ShapeParameterController`（`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:24,30`）が hit-test / drag 状態 / 値更新 / Undo commit まで閉じているので、RenderController 側はマウスイベントの前段ルーティングと当該モード開始の thin wrapper のみを追加
- ポリゴン頂点挿入は既存の `beginShapePathVertexDrag` 経路（`RenderController:23527`）を流用し、segment ヒット時に `customPolygonPoints_` の指定 index に `QPointF` を挿入して `ShapePathVertexEditCommand` 経由で Undo
- 各ハンドルの描画は Phase D-1 の overlay 関数を再利用（コーナーハンドル = 8px 円、内径ハンドル = 8px 円、挿入マーカー = 6px 十字）

**検証:**
- Rect の 0 ≤ r ≤ min(w,h)/2 にクランプ（`MILESTONE_LAYER_EDIT_2026-04-25.md:59-60`）
- Star の inner radius を 0.0..2.0 にクランプ
- Polygon 挿入が編集モード中のみ発火、既存頂点ホバー時は選択扱い（挿入しない）

### Phase D-3 — `ToolType::Shape` プリセット選択 UI

**現状:**
- `ArtifactToolManager.cppm:44-46` で `ToolType::Shape / Rectangle / Ellipse` は定義済み
- 現状シェイプ作成は `Shape` 単独、または `Rectangle / Ellipse` ツールの `rectangleToolMode_` 切替のみで、Panel 上にプリセット導線なし（`MILESTONE_SHAPE_RECTANGLE_ELLIPSE_TOOLS_2026-07-31.md` を満たすには UI から到達できる必要がある）
- メインツールバーは `Rectangle / Ellipse` を独立アクション化済み（`MILESTONE_INSPIRATION_2026-08-11.md:2082` 関連）

**実装:**
- `ArtifactCompositionEditor` のツールバー（またはツールオプション）に Rect / Ellipse / Star / Polygon / Triangle / Line の 6 プリセットトグルを追加し、`activeTool == ToolType::Shape` の下で `shapeToolPreset_` enum を切替
- RenderController 側は `mousePress` の `ToolType::Shape` 分岐で `shapeToolPreset_` を見て `RectangleToolMode::Shape / EllipseShape / StarShape / PolygonShape / TriangleShape / LineShape` に振り分け
- Line のみ `rectangleToolFromCenter_`（Alt）と相性が悪いので、`Line` プリセット時は端点ドラッグ方式（既存 `draggingLineLayer_` 経路）に切替え
- `shapeToolPreset_` は `ToolType` ではなく別 state（`ArtifactCompositionRenderController::Impl::shapeToolPreset_`）として保持

**検証:**
- 6 プリセットのトグル後にドラッグで該当シェイプが作成されること
- Line プリセット時の Alt+FromCenter を無効化（無効時はヘルプ表示）

### Phase D-4 — メイン VP 上の vertex / segment / tangent 選択 grammar 完成

**現状:**
- LayerEditorPanel 側 (`ArtifactLayerEditorWidget`) は Ctrl-click 選択追加、Shift-click toggle、vertex duplication、segment hover summary 等を実装済み（`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:255-279`）
- RenderController 側は state として `hoveredShapePathVertex_` / `hoveredShapePathTangent_` / `shapePathEditPending_` / `shapePathEditDirty_` を保持するが、選択 grammar（複数選択保持、Shift toggle、Ctrl add）は未確認

**実装:**
- `Impl` に `selectedShapePathVertices_` (QSet<int>) / `selectedShapePathTangents_` (QSet<int>) を追加
- 既存マスク編集の `selectedMaskVertices_` パターン（L12806）を踏襲し、Shift/Ctrl/Alt の修飾ロジックを共通化
- 数値範囲：`customPathVertices_.size()` の `kMaxShapePathVertices = 100000` を超えないこと
- Backspace / Delete キーで頂点削除（既存 `removeLastPendingShapePathVertex` とは別経路。確定後のパスに対する削除）
- Escape で選択解除、Enter で全選択 → Grab（G キー）への導線
- `keyPressEvent` 内の Shape 選択時のキー判定は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:16` の `KeyInputController` パターンに揃える

**検証:**
- 5 頂点パスで Shift-click 複数選択、Ctrl-click 追加、空クリック解除の動作
- 選択中に Backspace で頂点削除 → Undo 復元できる
- 既存 mask 編集との modifier 衝突なし

### Phase D-5 — Shape operator stack の VP 上数値ハンドル／HUD 編集

**現状:**
- 9 種の operator（TrimPaths / Merge Paths / Offset / Pucker / Rounded / Wiggle / ZigZag / Twist / HandDrawnWobble）はデータ層実装済み（`ArtifactCore/include/Shape/AeOperators.ixx`）
- Inspector 経由で数値編集可能だが、VP 上でハンドル／HUD ドラッグ編集する経路はない

**実装:**
- TrimPaths: start / end / offset の 3 数値をパス端点のトリムハンドルとして描画。Trim ハンドルは path 端点マーカー上に三角
- Repeater: copies / offset / rotation / opacity を VP 上に数値 HUD（`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:30` の `TransformOverlay` と類似）として表示し、`U` キーで編集モードに入る
- OffsetPaths / PuckerBloat / RoundedCorners: amount 値を形状端の offset ハンドルとして描画
- Wiggle / ZigZag / HandDrawnWobble: 数値 HUD のみ（ハンドル位置が不定のため）
- operator stack 内のいずれか編集中は既存 `ShapePathVertexEditCommand` とは別の operator edit command を作成（`Artifact/CMakeLists.txt` への新規追加は伴わない、`ArtifactShapeLayer.cppm` 内に追加）

**検証:**
- Trim 開始点と終了点をドラッグしたとき、`evaluatePathAt(frame)` でパスがクリップされる
- Repeater のコピーがリアルタイムに増える
- operator HUD の数値編集が Undo/Redo で復元できる

### Phase D-6 — パス open/closed トグル、smooth toggle、corner ↔ bezier 切替

**現状:**
- `customPathClosed_` は `ArtifactShapeLayer::Impl` に存在、`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:266,279` で UI 経路は一部あるが VP ハンドルではない
- `CustomPathVertex::smooth` はデータ層に存在、UI からの toggle は未確認

**実装:**
- パス端点の右クリックメニューに `Open / Close` アクション、選択頂点の右クリックメニューに `Toggle Smooth` アクション
- corner ↔ bezier 切替は頂点タイプを `CustomPathVertex::smooth` で判別し、bezier 化時に `inTangent` / `outTangent` を `pos` ± ベクトルで初期化
- 各アクションは `ShapePathVertexEditCommand` 系の新 command subtype として実装し、Undo 対象
- 既存 Pen 経路の Escape 取消・Enter 確定・Backspace 取消と整合（確定前の pending path では無効化）

**検証:**
- 閉じた 4 頂点パスを `Open` で開いたとき `evaluatePathAt(frame)` の `customPathClosed_` が反映される
- smooth toggle でハンドルが描画／非描画される
- bezier 化した頂点を corner に戻したとき tangent が破棄される

### Phase D 共通の前提・リスク

- `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` の Phase 1-A 完了で `ArtifactLayerEditorWidget` 側のシェイプモジュールは分離済み。Phase D では **既存 controller / overlay への thin wiring 追加** に留め、新たなモジュール / CMakeLists 変更を避ける（AGENTS.md の再ビルド抑制方針）
- `ArtifactCompositionRenderController.cppm` 28214 行、`ArtifactShapeLayer.cppm` 3380 行、`ArtifactCompositionRenderOverlay.cppm` 1827 行という巨大ファイル状態では変更影響範囲の見積もりが難しい。`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md` の "incremental / stable" 方針に従い、各サブ Phase を **1 機能ずつ上げる**
- 各サブ Phase 完了時に「ビルドしたいか」を確認する（AGENTS.md 制約）
- Phase D-1〜D-6 は独立ではなく依存関係を持つ：D-1（D-2/D-4/D-5/D-6 で再利用する overlay）→ D-2 → D-4 → D-5 → D-3 → D-6 の順を推奨
- 既存実装の `INSIGHT_ARCHIVE_2026-09-01.md:4549-4551` の懸念（タンジェント smooth 反射の長さ保存比、パスキーフレームの UI、`shape.path.keyframes` の timeline 表示統合）は本 Phase D の対象外だが、D-5 のキーフレーム評価で副作用がないか確認する

### Phase D 完了条件

- D-1〜D-6 すべてのヘッダ・実装がレビュー通過
- 既存ビルド・runtime 検証で確認された回帰がないこと（マスク編集、Text Editor、Camera Gizmo、POI、Asset Library、Timeline との同時操作）
- AGENTS.md の Insight 記録ルールに従い、D-1〜D-6 の各 Phase で「ビルド未検証」「実装済み」の事実を `Insight.md` または `docs/analysis/INSIGHT_ARCHIVE_*.md` に記録
- 最終的にシェイプレイヤー上で AE 互換の編集操作が主・キーボード・数値 HUD のいずれかで完結すること

### 関連ファイル

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` (28214 行)
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm` (1827 行)
- `Artifact/src/Layer/ArtifactShapeLayer.cppm` (3380 行)
- `Artifact/src/Tool/ArtifactToolManager.cppm` (2171 行)
- `Artifact/src/Widgets/ArtifactToolBar.cppm`
- `Artifact/src/Widgets/ArtifactCompositionEditor.cppm`
- 既存シェイプモジュール（流用元）: `Artifact.Widgets.LayerEditor.{ShapeOverlay, ShapeEditSession, ShapeDragController, ShapeHoverController, ShapeParameterController, ShapeInputController, ShapeMoveController, ShapePressController, ShapePressInteractionController, ContextMenu, ModalTransformController, KeyInputController, ViewMoveController}`

## Phase D/E メモ

- D（頂点編集）: データモデルは `CustomPathVertex` 済み。CompositionEditor の gizmo/hit-test に vertex handle 層を追加する UI 実装が本体
- E（複数シェイプ）: アプリ単一プリミティブ→コア `ShapeGroup` モデル移行を伴うため設計レビューを推奨。`toCoreShapeLayer()` が変換層の雛形

## 未検証（ビルド検証待ち）

1. Core 変更（ShapeTypes/ShapeLayer.cppm）による ArtifactCore 再ビルド範囲
2. グラデーション fill の SVG 出力をブラウザ/Illustrator で表示確認
3. Inside/Outside stroke の輪郭出力見栄（自己交差パスでの破綻確認）

2026-08-22 のシェイプ領域コードベースウォークで判明した、後日修正予定項目。**P1 は案Aで実装済み（2026-08-23）、P2 も実装済み。ビルド・ランタイム検証は未実施。**

## P1. ベクター（SVG）出力の接続 — 実装済み（案A、2026-08-23）

ユーザー判断により案A（出力をやる）で実装。

- `ArtifactShapeLayer::toCoreShapeLayer()` を新設（`ArtifactShapeLayer.ixx` / `.cppm`）。`nativeShapePaths()` の演算子処理済みパス + fill/stroke 設定（色・幅・cap/join・dash）を コア `ShapeLayer` / `PathShape` に変換する
- `ArtifactRenderQueueService.cppm` の SVG 出力分岐の黒矩形スタブを置き換え。可視かつアクティブなシェイプレイヤーを `allLayerRef()` から収集し、`getGlobalTransformAt(f)` を外部 transform として `SvgFrameExporter::exportLayerToSvg` に渡してフレーム SVG を書き出す
- **既知の制限:** コア `svgStyleString()` は単色のみ対応のため、グラデーション fill/stroke は開始色と終了色の中間色に縮退される。stroke taper / stroke align (Inside/Outside) は未反映。将来拡張は Insight.md 参照

### 未検証（ビルド検証待ち）

1. SVG 出力ジョブが実パス幾何を出すこと（黒矩形が出ないこと）
2. グラデーション fill の中間色縮退の見栄差
3. `import Shape.Layer` 追加によるモジュール依存の再スキャン

## P2. シェイプパラメータのキーフレーム再生経路 — 欠落を確認、実装済み（2026-08-23）

検証の結果、`shape.width` 等の animatable プロパティは**再生・レンダリング経路で評価されていなかった**（transform.* / layer.opacity はアクセサ内で lazy 評価されるが、shape.* はメンバ値を直接返すのみ）。

対応として `ArtifactSolidImageLayer::color()` と同じ lazy 評価パターンを適用:

- `effectiveShapeTimelineTime()` / `animatedShapeNumber()` ヘルパーを新設（composition framePosition + fps から RationalTime を生成）
- **`ShapeGeomDims` 一元化**: `resolveShapeGeomDims()` を唯一のジオメトリ解決入口とし、GPU draw（native operator / soft-body / compatibility cache の全分岐）、ソフト描画 `toQImage()`、`localBounds()`、D3D card points、SVG 出力（`toCoreShapeLayer` → `nativeShapePaths`）のすべてが同一の評価値を使う
- キーフレーム存在時（`hasAnimatedShapeGeometry()`）は該当キャッシュをフレームごとに再構築（nativeGeometry は `cacheable=false`、rebuildCache/bounds/card points は再計算）。キーフレーム無しの場合は従来のキャッシュ経路・メンバ値フォールバックでホットパス影響ゼロ
- 新規 import（`.cppm` 実装側のみ）: `Property.Abstract` / `Artifact.Composition.Abstract` / `Time.Rational`

### 未検証（ビルド検証待ち）

- width キーフレームを打って再生追従を実機確認（GPU path / 互換キャッシュ path の両方）
- 式プロパティ（wiggle 等）との併用時の非影響
- キーフレーム有り時のキャッシュ再構築コスト（大サイズシェイプでのフレームレート影響）

## 関連メモ

- GPU path の非ソリッドフィル（グラデーション等）は QImage 互換キャッシュ経由のフォールバック（設計通り、fallback ログ付き）。P1 を進める際は SVG 側のグラデーション属性マッピングと合わせて扱う。
- `ShapePath::triangulate`（fill rule 対応・キャッシュ付き）とオペレータ5種のソフト/GPU 接続は良好、本計画では変更しない。
