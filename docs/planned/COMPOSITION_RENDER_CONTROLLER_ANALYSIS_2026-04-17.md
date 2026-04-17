# `ArtifactCompositionRenderController` クラス分析報告書

**分析日**: 2026-04-17  
**ファイル**: `Artifact/Widgets/Render/ArtifactCompositionRenderController`  
**モジュール**: `Artifact.Widgets.CompositionRenderController`  
**担当範囲**: コンポジションの表示制御・ビューポート操作・Gizmo オーバーレイ・LOD 管理

---

## 1. クラス定義所在

| 項目 | 内容 |
|---|---|
| **宣言ファイル** | `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` |
| **実装ファイル** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` |
| **インスタンス生成** | `ArtifactCompositionEditor` (`src/Widgets/Render/ArtifactCompositionEditor.cppm:2177`) で `new CompositionRenderController(this)` |
| **基底クラス** | `QObject` |
| **実装パターン** | Pimpl（`Impl` 内部クラス） |
| **スレッドセーフ** | 一部 `QMutex` 使用（`CompositionChangeDetector` 等） |

---

## 2. クラス辞典（主要メソッド一覧）

### 初期化・ライフサイクル

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `CompositionRenderController(QObject* parent = nullptr)` | コンストラクタ | Impl 初期化、Gizmo 生成、EventBus 購読設定 | ✔ 実装済 |
| `~CompositionRenderController()` | デストラクタ | `destroy()` 呼び出し、Impl 削除 | ✔ 実装済 |
| `initialize(QWidget* hostWidget)` | `void` | レンダラー・コンポジションレンダラー初期化、タイマー設定 | ✔ 実装済 |
| `destroy()` | `void` | レンダラー破棄、キャックリア、停止 | ✔ 実装済 |
| `isInitialized() const` | `bool` | 初期化完了フラグ取得 | ✔ 実装済 |
| `start()` | `void` | レンダリング開始（時刻駆動） | ✔ 実装済 |
| `stop()` | `void` | レンダリング停止、`flushAndWait()` | ✔ 実装済 |
| `isRunning() const` | `bool` | 実行中フラグ取得 | ✔ 実装済 |

### コンポジション管理

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `setComposition(ArtifactCompositionPtr composition)` | `void` | コンポジション設定、キャッシュ・パイプライン再初期化 | ✔ 実装済 |
| `composition() const` | `ArtifactCompositionPtr` | 現在のコンポジション取得 | ✔ 実装済 |
| `setSelectedLayerId(const LayerID& id)` | `void` | 選択レイヤー設定、Gizmo 同期 | ✔ 実装済 |
| `selectedLayerId() const` | `LayerID` | 選択レイヤー ID 取得 | ✔ 実装済 |

### 表示設定

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `setClearColor(const FloatColor& color)` | `void` | ビューポート背景色設定 | ✔ 実装済 |
| `setCompositionBackgroundMode(CompositionBackgroundMode mode)` | `void` | 背景モード（Solid/Checkerboard/MayaGradient） | ✔ 実装済 |
| `compositionBackgroundMode() const` | `CompositionBackgroundMode` | 現在の背景モード取得 | ✔ 実装済 |
| `setShowGrid(bool show)` | `void` | グリッド表示切替 | ✔ 実装済 |
| `isShowGrid() const` | `bool` | グリッド表示状態取得 | ✔ 実装済 |
| `setShowCheckerboard(bool show)` | `void` | チェッカーボード表示切替 | ✔ 実装済 |
| `isShowCheckerboard() const` | `bool` | チェッカーボード状態取得 | ✔ 実装済 |
| `setShowGuides(bool show)` | `void` | ガイド表示切替 | ✔ 実装済 |
| `isShowGuides() const` | `bool` | ガイド表示状態取得 | ✔ 実装済 |
| `setShowSafeMargins(bool show)` | `void` | セーフマージン表示切替 | ✔ 実装済 |
| `isShowSafeMargins() const` | `bool` | セーフマージン状態取得 | ✔ 実装済 |
| `setShowMotionPathOverlay(bool show)` | `void` | モーションパスオーバーレイ表示切替 | ✔ 実装済 |
| `isShowMotionPathOverlay() const` | `bool` | モーションパス状態取得 | ✔ 実装済 |
| `setGpuBlendEnabled(bool enabled)` | `void` | GPU ブレンド有効／無効切替 | ✔ 実装済 |
| `isGpuBlendEnabled() const` | `bool` | GPU ブレンド状態取得 | ✔ 実装済 |

### レンダリング制御

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `recreateSwapChain(QWidget* hostWidget)` | `void` | スワップチェイン再作成 | ✔ 実装済 |
| `setViewportSize(float width, float height)` | `void` | ビューポートサイズ設定 | ✔ 実装済 |
| `setPreviewQualityPreset(PreviewQualityPreset preset)` | `void` | プレビュー品質プリセット設定 | ✔ 実装済 |
| `panBy(const QPointF& viewportDelta)` | `void` | ビューポートをパン（ドラッグ） | ✔ 実装済 |
| `notifyViewportInteractionActivity()` | `void` | ビューポート操作中通知（120ms タイマー開始） | ✔ 実装済 |
| `finishViewportInteraction()` | `void` | ビューポート操作終了通知 | ✔ 実装済 |
| `renderOneFrame()` | `void` *(slot)* | 単フレーム描画（タイマー・イベントから呼出） | ✔ 実装済 |
| `invalidateBaseComposite()` | `void` *(非公開)* | ベース合成キャッシュ無効化 | ✔ 実装済 |
| `invalidateOverlayComposite()` | `void` *(非公開)* | オーバーレイ合成キャッシュ無効化 | ✔ 実装済 |

### カメラ・ズーム操作

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `resetView()` | `void` | ビューリセット | ✔ 実装済 |
| `zoomInAt(const QPointF& viewportPos)` | `void` | 指定位置でズームイン | ✔ 実装済 |
| `zoomOutAt(const QPointF& viewportPos)` | `void` | 指定位置でズームアウト | ✔ 実装済 |
| `zoomFit()` | `void` | コンポジション全体にフィット | ✔ 実装済 |
| `zoomFill()` | `void` | ビューポート全体にフィル（ Fill ） | ✔ 実装済 |
| `zoom100()` | `void` | 100% ズーム（1:1） | ✔ 実装済 |
| `focusSelectedLayer()` | `void` | 選択レイヤーに焦点を合わせる | ✔ 実装済 |

### Gizmo 操作

| メソッド | 戻り値 | 概要 | 実装状態 |
|---|---|---|---|
| `setGizmoMode(TransformGizmo::Mode mode)` | `void` | Gizmo モード切替（All/Translation/Rotation/Scale/Anchor） | ✔ 実装済 |
| `gizmoMode() const` | `TransformGizmo::Mode` | 現在の Gizmo モード取得 | ✔ 実装済 |
| `gizmo() const` | `TransformGizmo*` | 2D Gizmo 取得 | ✔ 実装済 |
| `gizmo3D() const` | `Artifact3DGizmo*` | 3D Gizmo 取得 | ✔ 実装済 |
| `setRenderQueueActive(bool active)` | `void` | Render Queue 有効／無効切替（キャッシュ無効化抑制） | ✔ 実装済 |
| `isRenderQueueActive() const` | `bool` | Render Queue 状態取得 | ✔ 実装済 |

### オーバーレイ表示（Ghost / Info HUD）

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `setDropGhostPreview(const QRectF& viewportRect, const QString& title, const QString& hint, const QString& label)` | `void` | ドロップゴーストプレビュー表示 | ✔ 実装済 |
| `clearDropGhostPreview()` | `void` | ドロップゴースト非表示 | ✔ 実装済 |
| `setInfoOverlayText(const QString& title, const QString& detail = QString())` | `void` | 情報 HUD テキスト設定 | ✔ 実装済 |
| `clearInfoOverlayText()` | `void` | 情報 HUD 非表示 | ✔ 実装済 |

### LOD（Level of Detail）・デバッグ

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `lodManager() const` | `LODManager*` | LOD マネージャ取得 | ✔ 実装済 |
| `setLODEnabled(bool enabled)` | `void` | LOD 有効／無効切替 | ✔ 実装済 |
| `isLODEnabled() const` | `bool` | LOD 状態取得 | ✔ 実装済 |
| `setDebugMode(bool enabled)` | `void` | ROI デバッグ表示切替 | ✔ 実装済 |
| `isDebugMode() const` | `bool` | ROI デバッグ状態取得 | ✔ 実装済 |

### 入力イベント処理

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `handleMousePress(QMouseEvent* event)` | `void` | マウス左／中／右ボタン押下処理、選択・Gizmo・ドラッグ判定 | ✔ 実装済 |
| `handleMouseMove(const QPointF& viewportPos)` | `void` | マウス移動処理、Gizmo ドラッグ・頂点ドラッグ（Pen ツール） | ✔ 実装済 |
| `handleMouseRelease()` | `void` | マウス解放処理、ドラッグ終了・Undo 登録 | ✔ 実装済 |
| `hasPendingMaskEdit() const` | `bool` | マスク編集中フラグ取得 | ✔ 実装済 |

### クエリ・Utility

| メソッド | 戻り値 | 概要 | 実装状況 |
|---|---|---|---|
| `layerAtViewportPos(const QPointF& viewportPos) const` | `LayerID` | 指定ビューポート位置の最上面レイヤー ID | ✔ 実装済 |
| `renderer() const` | `ArtifactIRenderer*` | 内部レンダラー取得 | ✔ 実装済 |
| `cameraFrustumVisual() const` | `CameraFrustumVisual` | カメラフラスタム可視化データ | ✔ 実装済 |
| `setViewportOrientation(ArtifactCore::ViewOrientationHotspot hotspot)` | `void` | ビューポート向き設定 | ✔ 実装済 |
| `viewportOrientation() const` | `ArtifactCore::ViewOrientationHotspot` | 現在のビューポート向き取得 | ✔ 実装済 |
| `createPickingRay(const QPointF& viewportPos) const` | `Ray` | ピッキングレイ生成 | ✔ 実装済 |
| `cursorShapeForViewportPos(const QPointF& viewportPos) const` | `Qt::CursorShape` | カーソル形状判定 | ✔ 実装済 |

---

## 3. 依存関係マップ

```
[CompositionRenderController]
    │
    ├─► レンダリング基盤
    │    ├─ Artifact.Render.IRenderer       (レンダラー抽象)
    │    ├─ Artifact.Render.CompositionRenderer  (コンポジション描画担当)
    │    ├─ Artifact.Render.CompositionViewDrawing (オーバーレイ描画)
    │    ├─ Artifact.Render.Config          (レンダリング設定)
    │    ├─ Artifact.Render.ROI             (ROI デバッグ)
    │    ├─ Artifact.Render.Context         (レンダリングコンテキスト)
    │    ├─ Artifact.Render.Pipeline        (描画パイプライン)
    │    ├─ Artifact.Render.Offscreen       (オフスクーン関連)
    │    ├─ Artifact.Render.GPUTextureCacheManager (GPU テクスチャキャッシュ)
    │    └─ Graphics.LayerBlendPipeline     (GPU ブレンドパイプライン)
    │
    ├─► コンポジション・レイヤー
    │    ├─ Artifact.Composition.Abstract   (コンポジション基底)
    │    ├─ Artifact.Layer.Abstract         (レイヤー基底)
    │    ├─ Artifact.Layer.CloneEffectSupport
    │    ├─ Artifact.Layer.Camera
    │    ├─ Artifact.Layer.Light
    │    ├─ Artifact.Layer.Image
    │    ├─ Artifact.Layer.Svg
    │    ├─ Artifact.Layer.Video
    │    ├─ Artifact.Layer.Solid2D
    │    ├─ Artifact.Layers.SolidImage
    │    ├─ Artifact.Layer.Text
    │    ├─ Artifact.Layer.Composition (ネストコンポジション)
    │    ├─ Artifact.Layers.Model3D
    │    ├─ Artifact.Layers.Selection.Manager (レイヤー選択管理)
    │    └─ Artifact.Mask.LayerMask / Path (マスク編集)
    │
    ├─► サービス・イベント
    │    ├─ Artifact.Service.Project        (プロジェクトサービス)
    │    ├─ Artifact.Service.Playback       (再生サービス)
    │    ├─ Artifact.Service.ActiveContext  (アクティブコンテキスト)
    │    ├─ Artifact.Preview.Pipeline       (プレビューパイプライン)
    │    ├─ Event.Bus                       (イベントバス)
    │    ├─ Artifact.Event.Types            (イベント型定義)
    │    └─ Artifact.Tool.Manager           (ツール管理)
    │
    ├─► UI・ウィジェット
    │    ├─ Artifact.Widgets.TransformGizmo (2D トランスフォーム Gizmo)
    │    ├─ Artifact.Widgets.Gizmo3D        (3D Gizmo)
    │    ├─ UI.View.Orientation.Navigator   (ビュー向きナビゲータ)
    │    └─ Widgets.Utils.CSS               (CSS ユーティリティ)
    │
    ├─► 数学・ユーティリティ
    │    ├─ Geometry.CameraGuide            (カメラガイド形状生成)
    │    ├─ Utils.Id                        (ID 処理)
    │    ├─ Time.Rational                   (有理時間)
    │    ├─ Frame.Position                  (フレーム位置)
    │    ├─ Color.Float                     (浮動小数点色)
    │    ├─ Image                           (画像変換)
    │    ├─ CvUtils                         (OpenCV 変換)
    │    └─ ArtifactCore.Utils.PerformanceProfiler (プロファイラ)
    │
    └─► 基盤ライブラリ
         ├─ Qt (QObject, QTimer, QImage, QTransform, ...)
         ├─ DiligentEngine (DeviceContext, 各種グラフィックス API)
         └─ TBB (タスク並列処理 — Indirect 使用)
```

### import 文リスト（実装ファイルより抽出）

```cpp
import Artifact.Render.IRenderer;
import Artifact.Render.GPUTextureCacheManager;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.CompositionRenderer;
import Artifact.Render.Config;
import Artifact.Render.ROI;
import Artifact.Render.Context;
import Artifact.Preview.Pipeline;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.CloneEffectSupport;
import Artifact.Layer.Camera;
import Artifact.Layer.Light;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Svg;
import Artifact.Layer.Video;
import Artifact.Layer.Solid2D;
import Artifact.Layers.SolidImage;
import Artifact.Layer.Text;
import Artifact.Layer.Composition;
import Artifact.Layers.Model3D;
import Artifact.Render.Offscreen;
import Frame.Position;
import Artifact.Application.Manager;
import Artifact.Layers.Selection.Manager;
import Artifact.Widgets.TransformGizmo;
import Artifact.Widgets.Gizmo3D;
import UI.View.Orientation.Navigator;
import Geometry.CameraGuide;
import Artifact.Tool.Manager;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Utils.Id;
import Time.Rational;
import Artifact.Render.Pipeline;
import Graphics.LayerBlendPipeline;
import Graphics.GPUcomputeContext;
import Widgets.Utils.CSS;
import Artifact.Service.Project;
import Artifact.Service.Playback;
import Event.Bus;
import Artifact.Event.Types;
import Undo.UndoManager;
import ArtifactCore.Utils.PerformanceProfiler;
```

---

## 4. 使用箇所・インスタンス化

| ファイル | ロール | 行数 | 方法 |
|---|---|---|---|
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | 所有・管理（親ウィジェット） | 2177 | `new CompositionRenderController(this)` |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` | **间接的关联**：独自の Impl で `ArtifactIRenderer`・`ArtifactPreviewCompositionPipeline` を直接保持（CompositionRenderController 未使用） | — | — |

**注**: `ArtifactCompositionRenderWidget` は CompositionRenderController を**使用していない**。同名のロールだが別実装。  
CompositionRenderController は `ArtifactCompositionEditor` 内でのみインスタンス化されている。

---

## 5. 責務表（IN / OUT 明確化）

| 責務分野 | IN（入力／依存） | OUT（出力／提供） | 備考 |
|---|---|---|---|
| **コンポジション表示** | `ArtifactCompositionPtr`、`ArtifactIRenderer`、`CompositionRenderer` | コンポジション全体をビューポートに描画 | `renderOneFrameImpl()` で `CompositionRenderer::Render()` 呼出 |
| **レイヤー選択** | `LayerID`、`ArtifactLayerSelectionManager`、EventBus（`LayerSelectionChangedEvent`） | 選択レイヤーの Gizmo 表示・ハイライト | `setSelectedLayerId()` で選択状態更新、Gizmo を同期 |
| **ビューポート操作** | マウス・ホイールイベント（Widget から委譲）、`ArtifactIRenderer` | パン・ズーム・フィット状態の維持 | `panBy()`, `zoom*()` は Impl のレンダラーに forwarded |
| **Gizmo 表示** | `TransformGizmo`、`Artifact3DGizmo`、選択レイヤーの変換行列 | 2D/3D トランスフォームハンドル描画 | `drawLayerForCompositionView()` 内で Gizmo 描画呼出 |
| **オーバーレイ表示** | `showGrid_`, `showGuides_`, `showSafeMargins_` フラグ | グリッド／ガイド／セーフマージン描画 | `drawViewportGhostOverlay()` で実施 |
| **Ghost プレビュー & Info HUD** | 矩形・タイトル・ヒント文字列 | ドラッグ中のゴースト矩形、操作情報テキスト描画 | `setDropGhostPreview()`, `setInfoOverlayText()` |
| **LOD 管理** | `LODManager`、ズーム倍率 | 詳細レベルに応じた描画制御 | `detailLevelFromZoom()` で LOD 判定 |
| **スワップチェイン管理** | `QWidget* hostWidget`、`QTimer` | サイズ変更時再作成、DPR 対応 | `recreateSwapChain()`, `setViewportSize()` |
| **キャッシュ管理** | `QHash<QString, LayerSurfaceCacheEntry>`、`GPUTextureCacheManager` | レイヤー表面・GPU テクスチャキャッシュ | `buildLayerSurfaceCacheKey()` でキー生成、`invalidateLayerSurfaceCache()` で破棄 |
| **差分レンダリング** | `CompositionChangeDetector`（内部）、`RenderKeyState` パック構造 | 変更最小化による無駄描画抑制 | `renderOneFrameImpl()` で `lastRenderKeyState_` 比較 |
| **モーションパス** | レイヤーの `Transform3D`、現在フレーム | モーションパス曲線（キーフレーム＋現在）の描画 | `buildMotionPathSamples()` でサンプル生成 |
| **レンダーキュー統合** | `setRenderQueueActive()` フラグ | Render Queue 実行中はキャッシュ無効化抑制 | `LayerChangedEvent` で `renderQueueActive_` チェック |
| **ROI デバッグ** | `setDebugMode()` フラグ | ROI（関心領域）矩形描画 | 未実装箇所あり（`#if 0` 相当） |
| **マスク編集** | `LayerMask`, `MaskPath`, `Pen` ツール状態 | 頂点ドラッグ・パス閉じ処理、Undo 登録 | `handleMousePress/Move` 内で実施、`commitMaskEditTransaction()` で Undo |

### 外部委譲項目（明示的な委譲先）

| 機能 | 委譲先クラス | 備考 |
|---|---|---|
| 実際のコンポジション描画 | `CompositionRenderer` | `compositionRenderer_->Render()` |
| 低レベル描画コマンド | `ArtifactIRenderer` | `drawSpriteTransformed()`, `drawSolidRectTransformed()`, `drawCheckerboard()` 等 |
| GPU ブレンド処理 | `ArtifactCore::LayerBlendPipeline` | `blendPipeline_->blendLayers()` |
| GPU テクスチャキャッシュ | `GPUTextureCacheManager` | キャッシュ取得・無効化 |
| プロジェクト当前コンポジション解決 | `ArtifactProjectService` | `resolvePreferredComposition()` で最優先参照 |
| 再生中フレーム取得 | `ArtifactPlaybackService` | `currentFrameForComposition()` で合成 |
| レイヤー選択管理 | `ArtifactLayerSelectionManager` | EventBus 経由で更新・参照 |
| ツール種別取得 | `ArtifactToolManager` | `ToolType::Selection/Move/Rotation/...` |
| Undo 登録 | `UndoManager` | `MaskEditCommand`, `MoveLayerCommand` 等 |

---

## 6. 実装上の特徴・注意点

1. **Pimpl イディオム**: `Impl` 内部クラスで実装隠蔽、ABI 穩定を図る。
2. **EventBus 中心の更新**: `LayerChangedEvent`, `CurrentCompositionChangedEvent` 等をサブスクライバし、状態を自動更新。
3. **キャッシュ戦略**:
   - `surfaceCache_`（CPU イメージ）と `gpuTextureCacheManager_`（GPU ハンドル）の2層キャッシュ。
   - `buildLayerSurfaceCacheKey()` でレイヤー種別＋サイズ＋フレーム番号をキー化。
4. **差分レンダリング**: `RenderKeyState` 構造体ですべての描画状態をパックし、`operator==` で変更検出。変更がなければ描画スキップ。
5. **モーションパス最適化**: `MotionPathCacheEntry` でモーションパス点列をキャッシュ。`overlaySerial_` 変更時にInvalidate。
6. **タイマーベース.render**: `QTimer`（`renderTimer_`）で駆動していたが、現在はイベント駆動（`frameChanged`, `layerChanged`）中心に移行。`renderOneFrame()` は slot。
7. **Device Pixel Ratio 対応**: 論理座標→物理座標変換を `devicePixelRatio_` で管理。マウス座標はイベント時に乗算。
8. **Render Queue モード**: バッチ編集時に `setRenderQueueActive(true)` でキャッシュ無効化を抑制。
9. **GPU ブレンド切替**: `gpuBlendEnabled_` は環境変数 `ARTIFACT_COMPOSITION_DISABLE_GPU_BLEND` でも制御可能。
10. **Undo 統合**: レイヤー移動・マスク編集で `UndoManager` にコマンドプッシュ。

---

## 7. 関連ファイル一覧

| 役割 | ファイルパス |
|---|---|
| 宣言 | `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` |
| 実装 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` |
| 親ウィジェット | `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` |
| 関連ウィジェット（別実装） | `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` |
| コンポジションレンダラー | `Artifact/include/Render/CompositionRenderer.ixx`, `Artifact/src/Render/CompositionRenderer.cppm` |
| レイヤー抽象 | `Artifact/include/Layer/Abstract.ixx` |
| イベント型 | `Artifact/include/Event/Types.ixx` |

---

## 8. 実装状況サマリ

- **状態**: 完全実装（スタブ・未実装含まず）
- **主要機能**: コンポジション描画・ビューポート操作・Gizmo 表示・オーバーレイ・LOD・キャッシュ管理・Undo 連携
- **パフォーマンス**: 差分レンダリング・GPU ブレンド・GPU キャッシュ活用
- **今後の拡張点**: ROI デバッグ描画（一部 `#if 0`）、`setRenderQueueActive()` 連携の深化

---

*以上、2026-04-17 時点のコードベース（Artifact/）からの分析結果。*
