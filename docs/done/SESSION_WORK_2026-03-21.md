# セッション作業レポート

> 2026-03-21 作業分

---

## 1. LayerView(Diligent) マウスホイール拡大縮小 不安定修正

### 問題
`ArtifactLayerEditorWidgetV2` の `zoomLevel_` が renderer の `ViewportTransformer` 内の実ズームと乖離。`fitToViewport()` 呼び出し後にホイール操作すると基点のズーム値がジャンプして不安定に見える。

### 原因
`zoomLevel_` はキャッシュ値で renderer の真実値と非同期に更新される。`fitToViewport()` 後に `zoomLevel_ = 1.0f` にハードコードされるが、renderer 側は計算値 (例: 0.75) になるため不整合。

### 修正ファイル
`Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`

| 箇所 | 変更内容 |
|---|---|
| `wheelEvent` (line 467) | `zoomLevel_` の累積乗算 → `renderer_->getZoom()` を基点に計算 |
| `fitToViewport()` (line 563) | `zoomLevel_ = 1.0f` → `fitToViewport()` 呼び出し後に `renderer_->getZoom()` で同期 |
| `setTargetLayer` (line 547) | `zoomLevel_ = 1.0f` → `renderer_->getZoom()` で同期 |
| Key_F handler (line 144) | `zoomLevel_ = 1.0f` → `renderer_->getZoom()` で同期 |

---

## 2. CompositionEditor ヒットテストレイヤー選択 追加

### 問題
`ArtifactCompositionEditor` (新) にはレイヤー選択のヒットテストが未実装。マウス押下は Gizmo 操作 (`TransformGizmo::handleMousePress`) にしか流れていない。

### 修正ファイル
**3ファイル変更:**

#### `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `#include <QMouseEvent>` 追加
- `handleMousePress(const QPointF&)` → `handleMousePress(QMouseEvent*)` にシグネチャ変更

#### `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `import Artifact.Layers.Selection.Manager;` 追加
- `handleMousePress` にレイヤー選択ロジック追加:
  - Gizmo ヒットテストを先に処理
  - 左クリック時に canvas 座標に変換して全レイヤーのバウンディングボックスを後ろから順に判定
  - ヒットしたら `layerSelectionManager()->selectLayer()` で選択 (Shift で追加選択)
  - ミスしたら `clearSelection()` で選択解除
  - Gizmo のターゲットレイヤーも連動更新

#### `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `controller_->handleMousePress(event->position())` → `controller_->handleMousePress(event)` に変更
- Gizmo ドラッグ有無に関わらず `event->accept()` で確定

---

## 3. ComputeShader ベース レイヤーブレンドシステム マイルストーン作成

### 作成ファイル
`docs/planned/MILESTONE_GPU_LAYER_BLEND_COMPUTE_2026-03-21.md`

### 内容
- 現状調査サマリー (18種ブレンドモード定義済、GPU シェーダ 5/18、ComputeExecutor 完成済)
- 5 Phase 構成 (HLSL補完 → Computeパイプライン → テクスチャ管理 → 統合 → 最適化)
- 見積 ≈ 12.5日

---

## 4. HLSL ブレンドシェーダ 全18種作成

### 新規作成ファイル (14ファイル)

| ファイル | ブレンドモード |
|---|---|
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/BlendCommon.hlsli` | 共通ヘッダ (HSL変換ヘルパー) |
| `CS_BlendSubtract.hlsl` | Subtract |
| `CS_BlendDarken.hlsl` | Darken |
| `CS_BlendLighten.hlsl` | Lighten |
| `CS_BlendColorDodge.hlsl` | ColorDodge |
| `CS_BlendColorBurn.hlsl` | ColorBurn |
| `CS_BlendHardLight.hlsl` | HardLight |
| `CS_BlendDifference.hlsl` | Difference |
| `CS_BlendExclusion.hlsl` | Exclusion |
| `CS_BlendHue.hlsl` | Hue (HSL変換) |
| `CS_BlendSaturation.hlsl` | Saturation (HSL変換) |
| `CS_BlendColor.hlsl` | Color (HSL変換) |
| `CS_BlendLuminosity.hlsl` | Luminosity (HSL変換) |

全シェーダ共通仕様:
- `Texture2D<float4> SrcTex : register(t0)` (前景)
- `Texture2D<float4> DstTex : register(t1)` (背景)
- `RWTexture2D<float4> ResultTex : register(u0)` (出力)
- `[numthreads(8,8,1)]`

### 既存シェーダ (5ファイル、変更なし)
`CS_BlendNormal.hlsl`, `CS_BlendAdd.hlsl`, `CS_BlendScreen.hlsl`, `CS_BlendOverlay.hlsl`, `CS_BlendSoftlight.hlsl`

---

## 5. LayerBlendComputeShader.ixx 更新 (18種登録 + opacity CB)

### 変更ファイル
`ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx`

### 変更内容
- 全18種のインライン HLSL シェーダ文字列を opacity 対応に更新
- `BlendParams` 構造体追加 (`opacity`, `blendMode`, `_pad`)
- 各シェーダに `ConstantBuffer<BlendParams> : register(b0)` 追加
- `BlendShaders` マップに 18種全て登録:
  ```
  Normal, Add, Subtract, Multiply, Screen, Overlay, Darken, Lighten,
  ColorDodge, ColorBurn, HardLight, SoftLight, Difference, Exclusion,
  Hue, Saturation, Color, Luminosity
  ```
- opacity 適用: `lerp(dst.rgb, blended, opacity)`

---

## 6. CompositionEditor ヒットテスト: `isActiveAt` フィルタ追加

### 問題
描画ループは `isActiveAt(currentFrame)` でフィルタするが、ヒットテストはしていなかった。クリップ範囲外のレイヤーもヒット対象になっていた。

### 修正ファイル
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

### 変更内容
- `isActiveAt(currentFrame)` フィルタ追加（描画ループと条件一致）
- `currentFrame` の取得ロジックを描画ループと同一化（PlaybackService → Composition の優先順位）
- `impl_->renderer_` null チェック追加

---

## 7. レイヤーパネル ドラッグ＆ドロップ順序変更 修正

### 問題
`dropEvent` が MIME データからレイヤーIDを読み取らず、`impl_->draggedLayerId` に依存。さらに `visibleLayerIds`（ドラッグ中レイヤーを含む）でターゲットを検索していたため、インデックスがずれていた。加えて `QScrollArea` のビューポートがドラッグイベントを子ウィジェットに転送していなかった。

### 修正ファイル
`Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

### 変更内容

#### 1. `LayerPanelDragForwardFilter` クラス追加 (line 165-196)
- ビューポート上の `DragEnter` / `DragMove` / `DragLeave` / `Drop` イベントを `QCoreApplication::sendEvent` でパネルに転送

#### 2. ビューポートの `setAcceptDrops(true)` (line 2048)

#### 3. ドラッグフィルタのインストール (line 2055)
```cpp
auto* dragFilter = new LayerPanelDragForwardFilter(impl_->panel, this);
impl_->scroll->viewport()->installEventFilter(dragFilter);
```

#### 4. `dropEvent` インデックスバグ修正 (line 1898-1969)
- MIME データから `dragLayerId` を直接読み取る（別インスタンスでも動作）
- `remainingVisibleLayerIds` でターゲットを検索（ドラッグ中レイヤー除外済み）
- 末尾挿入・先頭挿入の境界処理を追加

---

## 修正対象ファイルサマリー

| ファイル | 変更内容 |
|---|---|
| `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm` | zoom 同期修正 |
| `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx` | handleMousePress シグネチャ変更 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | ヒットテスト選択 + isActiveAt |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | handleMousePress 呼び出し変更 |
| `ArtifactCore/include/Graphics/Shader/Compute/LayerBlendComputeShader.ixx` | 18種シェーダ登録 + opacity CB |
| `ArtifactCore/include/Graphics/Shader/HLSL/Blend/*.hlsl` (14新規) | ブレンドシェーダ |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | D&D 修正 + ビューポート転送 |
