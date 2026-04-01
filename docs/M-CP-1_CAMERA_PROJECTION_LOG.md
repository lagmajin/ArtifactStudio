# M-CP-1: Camera Projection Integration - 実装ログ

> 2026-03-31 実装

## 概要

3D rendering のために camera の projection を適切に扱い、perspective / orthographic の両方をサポートする。

## 実施内容

### Phase 1: Camera LayerにProjection Modeを追加

**ファイル**: `Artifact/include/Layer/ArtifactCameraLayer.ixx`

- `ProjectionMode` enum追加 (Perspective = 0, Orthographic = 1)
- `fov()` / `setFov()` - 直接FOV指定（manual/auto切替）
- `orthoWidth()` / `orthoHeight()` - Orthographic用プロパティ
- `nearClipPlane()` / `farClipPlane()` - クリップ面調整
- `projectionMode()` / `setProjectionMode()` - プロジェクションモード切替

**ファイル**: `Artifact/src/Layer/ArtifactCameraLayer.cppm`

- `Impl`構造体に新プロパティ追加:
  - `projectionMode_` (デフォルト: Perspective)
  - `fov_` / `useManualFov_` (0 = auto from zoom, >0 = manual)
  - `orthoWidth_` (1920), `orthoHeight_` (1080)
  - `nearClipPlane_` (1.0), `farClipPlane_` (100000.0)
- `projectionMatrix()`でortho/perspective両方に対応:
  - Perspective: FOV + aspect + near/far
  - Orthographic: orthoWidth/orthoHeight + aspect補正 + near/far
- `getLayerPropertyGroups()`に新プロパティをInspector表示用として追加
  - Projection Mode (enum: 0=Perspective, 1=Orthographic)
  - Zoom (px)
  - FOV (deg)
  - Ortho Width / Height (px)
  - Near Clip / Far Clip (px)
  - Depth of Field (bool)
  - Focus Distance / Aperture (px)
- `setLayerPropertyValue()`に新プロパティのセッター追加

### Phase 2: View/Projection Matrix計算

- `viewMatrix()`: 既存実装維持 (`getGlobalTransform4x4().inverted()`)
- `projectionMatrix()`: Phase 1でortho/perspective両対応済み
- near/far clip planeが固定値(1.0/100000.0)から可変に

### Phase 3: Renderer Integration

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

- `renderOneFrameImpl()`内でcomposition内のcamera layerを自動検出:
  - 最初のvisibleかつactiveな`ArtifactCameraLayer`を`activeCamera`として使用
  - cameraが存在しない場合は3D camera matrixは設定しない
- `drawLayerForCompositionView()`のシグネチャ変更:
  - 引数追加: `bool useGpuPath`, `const QMatrix4x4* cameraView`, `const QMatrix4x4* cameraProj`
- 3Dレイヤー描画時にcamera matrixを設定:
  - `renderer->set3DCameraMatrices(*cameraView, *cameraProj)` を描画前に呼ぶ
  - 描画後に`renderer->reset3DCameraMatrices()` でリセット
- GPU blend path / fallback path 両方からcamera matrixを渡すように修正

## 未実施（スキップ）

| 項目 | 理由 |
|------|------|
| Phase 4: Viewport Sync | 既存実装で毎フレームaspect ratio計算済みのため不要 |
| Phase 5: Camera Frustum Visualization | 計算ロジックは存在するが描画は後回し |
| P3D-2: Filled/solid polygon rendering | 別マイルストーンとして扱う |
| P3D-3: Mesh GPU upload/cache | 別マイルストーンとして扱う |

## 依存関係

- `M-CP-1` は `M-MAT-1` (Material System) と `M-LL-1` (Light Linking) の前提条件
- `P3D-2` (Filled rendering) と `P3D-3` (Mesh GPU cache) が3Dレンダリング品質向上に必要

## 次のステップ候補

1. **P3D-2**: Filled triangle rendering + depth buffer + basic shading
2. **P3D-3**: Mesh GPU upload + cache
3. **M-MAT-1**: Material System (Material classをレンダリングパイプラインに接続)
4. **M-LL-1**: Light Linking System (Light-to-Object linking)

---

# M-UI-13: Keyboard Overlay - 実装ログ

> 2026-03-31 実装

## 概要

アプリ内ショートカットを俯瞰できる軽量overlayを完成させる。

## 実施内容

**ファイル**: `Artifact/src/Widgets/Menu/ArtifactHelpMenu.cppm`

- `keyboardOverlayAction_`にショートカットキーを追加:
  - `Ctrl+/` (カスタム)
  - `F1` (HelpContents標準)
  - `setShortcuts()`で複数ショートカットを同時設定

## 備考

- Helpメニューからの接続は既に実装済み
- ダイアログ本体（検索、テーブル表示、compactモード）は既に実装済み
- ArtifactMenuBarへの統合は既に完了

---

# M-UI-12: Composition Notes / Scratchpad - 実装状況確認

> 2026-03-31 確認

## 結果: 既に実装済み

### Composition Note
- `ArtifactAbstractComposition::compositionNote()` / `setCompositionNote()` 存在
- シリアライゼーション: `toJson()` / `fromJson()` で `compositionNote` フィールド対応済み
- Inspector UI: `ArtifactInspectorWidget` に `QPlainTextEdit` で編集実装済み
- シグナル: `compositionNoteChanged(QString)` 接続済み

### Layer Note
- `ArtifactAbstractLayer::layerNote()` / `setLayerNote()` 存在
- シリアライゼーション: `toJson()` / `fromJson()` で `layerNote` フィールド対応済み
- Inspector UI: `ArtifactInspectorWidget` に `QPlainTextEdit` で編集実装済み
- シグナル: `layerNoteChanged(QString)` 接続済み

---

# M-AU-1: Composition Audio Mixer - 実装状況確認

> 2026-03-31 確認

## 結果: 既に実装済み

### Audio Mixer Widget
- `ArtifactCompositionAudioMixerWidget` 実装済み
- Master bus → PlaybackService同期:
  - `volumeChanged` → `setAudioMasterVolume()`
  - `muteChanged` → `setAudioMasterMuted()`
- 自動refresh:
  - `projectChanged` で refresh
  - `currentCompositionChanged` で refresh
  - `layerCreated` / `layerRemoved` で refresh

---

# M-AR-2: import std Rollout - 実装ログ

> 2026-03-31 実装

## 概要

C++23 `import std;`への段階的移行。影響の小さいファイルから変換。

## 実施内容

以下の4ファイルで`#include <vector>`, `<algorithm>`, `<cmath>`, `<limits>`, `<numeric>`を`import std;`に置換:

| ファイル | 変更前include | 変更後 |
|----------|--------------|--------|
| `Artifact/src/Layer/ArtifactGroupLayer.cppm` | `<vector>`, `<algorithm>` | `import std;` |
| `Artifact/src/Layer/ArtifactAudioLayer.cppm` | `<algorithm>`, `<cmath>` | `import std;` |
| `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` | `<cmath>`, `<algorithm>`, `<limits>` | `import std;` |
| `Artifact/src/Render/ArtifactHDRMonitor.cppm` | `<algorithm>`, `<cmath>`, `<numeric>` | `import std;` |

## 変換ルール

- Qt関連の`#include` (`<QDebug>`, `<QJsonObject>`等) は維持
- カスタムmoduleの`import` は維持
- 標準ライブラリのみ`import std;`に置換
- `import std;` はmodule宣言の直後に配置

---

# M-QA-4: Project File Validation - 実装ログ

> 2026-03-31 実装

## 概要

プロジェクト名のtypo検出、表記ゆれ検査、プロジェクト全体のバリデーション機能を実装。

## 実施内容

### 1. `ProjectValidationIssue` 構造体追加

**ファイル**: `Artifact/include/Project/ArtifactProjectSetting.ixx`

```cpp
struct ProjectValidationIssue {
  enum class Severity { Info, Warning, Error };
  Severity severity;
  QString field;
  QString message;
  QString suggestion;
};
```

### 2. `ArtifactProjectSettings::validate()` 実装

**ファイル**: `Artifact/src/Project/ArtifactProjectSetting.cppm`

**プロジェクト名チェック**:
- 空の名前 → Warning
- 使用不可能な文字(`<>:"/\|?*`) → Error
- 前後の空白 → Info
- 100文字超 → Warning
- デフォルトテンプレート名 → Info

**著者名チェック**:
- 200文字超 → Warning

### 3. `ArtifactProject::validate()` 実装

**ファイル**: 
- `Artifact/include/Project/ArtifactProject.ixx` (宣言追加)
- `Artifact/src/Project/ArtifactProject.cppm` (実装追加)

**検証項目**:
- Settingsのバリデーション
- コンポジション名の空チェック
- 空のコンポジション検出
- レイヤー名の空チェック

---

# 不足機能実装ログ

> 2026-03-31 実装

## #13 3D Model Layer: ファイル読込 + N-gon Triangulation

**ファイル**: `Artifact/src/Layer/Artifact3DModelLayer.cppm`

- `loadFromFile()` を実装。`MeshImporter`(ufbx/tinyobjloader)経由でFBX/OBJ読込
- `loadFromFile(const QString& filePath)` 追加。ファイルパス指定で直接読込
- N-gon triangulation: fan triangulationに置き換え（3角形・4角形・N角形全て対応）
- 読込失敗時はデフォルト立方体にフォールバック

**ファイル**: `Artifact/include/Layer/Artifact3DModelLayer.ixx`

- `loadFromFile(const QString& filePath)` 宣言追加

**ファイル**: `Artifact/src/Layer/Artifact3DModelLayer.cppm` (imports)

- `MeshImporter`, `Utils.String.UniString`, `QVector2D`, `QFileDialog`, `QFileInfo` import追加

## #15 PreCompose: 時間変換ロジック

**ファイル**: `ArtifactCore/src/Composition/PreCompose.cppm`

- `nestingLevel` の計算を実装。親の階層レベル+1を記録
- `parentToChildTime()`: Pre-compose layerのin-pointを考慮した変換（現在は恒等変換、in-point取得基盤整備済み）
- `childToParentTime()`: 逆変換（同上）
- `convertTime()`: 階層パス計算の骨組み（現在は恒等変換）
- `getRemappedTime()`: タイムリマップ対応の骨組み

## #18 Color Space Node: 色空間変換

**ファイル**: `Artifact/src/Color/ArtifactColorNode.cppm`

- `ColorSpaceNode::process()` を実装
- `ColorSpaceConverter::getConversionMatrix()` で4x4変換行列を取得
- 各RGBAピクセルに行列を適用して色空間変換
- `import Color.Space;` 追加

---

# Fitボタン左寄り修正ログ

> 2026-03-31 実装

## ルート原因

viewportSize（論理ピクセル）とDiligentのscreenSize（物理ピクセル = DPR倍）の不一致。

- setViewportSize(w, h): 論理ピクセル (例: 960x540)
- Diligent VP: width * DPR, height * DPR (例: 1920x1080 @ 2x DPR)
- シェーダー: ndc = pos / screenSize * 2.0 - 1.0 → screenSizeが2倍なのでキャンバスが1/4に縮小

## 修正内容

| ファイル | 変更 |
|----------|------|
| ArtifactCompositionEditor.cppm:265-268 | debounce後のsetViewportSizeにDPR乗算 |
| ArtifactCompositionEditor.cppm:486-498 | resizeEventのsetViewportSizeにDPR乗算 |
| ArtifactCompositionRenderController.cppm:1262-1268 | 初期化時のhostWidth/HeightにDPR乗算 |
| ArtifactCompositionRenderWidget.cppm:152-155 | debounce後のsetViewportSizeにDPR乗算 |
| ArtifactCompositionRenderWidget.cppm:397-398 | 初期化時のsetViewportSizeにDPR乗算 |

---

# EventBus採用: プロジェクトビュー サムネイル更新

> 2026-03-31 実装

## 概要

ArtifactCoreのEventBusを初めて採用。プロジェクトビューのサムネイル更新をイベント駆動に変更。

## 実施内容

### 1. イベント型定義

**ファイル**: Artifact/include/Event/ArtifactEventTypes.ixx (新規作成)

- CompositionCreatedEvent - コンポジション作成イベント
- CompositionThumbnailUpdatedEvent - サムネイル更新イベント
- LayerChangedEvent - レイヤー変更イベント (Created/Removed/Modified)

### 2. ProjectManagerWidgetでのEventBus採用

**ファイル**: Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm

- import Event.Bus; / import Artifact.Event.Types; 追加
- Impl::eventBus_ (EventBusインスタンス) 追加
- Impl::thumbnailUpdateDebounce_ (300ms debounceタイマー) 追加
- Qtシグナル → EventBus.post 変換ブリッジ:
  - compositionCreated → post<CompositionCreatedEvent>()
  - layerCreated → post<LayerChangedEvent>(Created)
  - layerRemoved → post<LayerChangedEvent>(Removed)
- EventBus.subscribe:
  - CompositionCreatedEvent → debounceタイマー起動
  - LayerChangedEvent → debounceタイマー起動
- event() で eventBus_.drain() を毎フレーム呼び出し

## 意義

- EventBusの**初の採用事例**
- QtシグナルとEventBusのブリッジパターン確立
- debounce付きサムネイル更新でパフォーマンス最適化
