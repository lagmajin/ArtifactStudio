# 3D版フレームギズモ ドラッグ無反応 調査メモ

**日付**: 2026-07-31
**状態**: 調査中（原因特定済み）

## 現象

3Dレイヤーのフレームギズモ（選択枠のコーナーハンドル）は正常に**描画**されるが、ドラッグしてもリサイズされない（無反応）。

## 原因

**2D TransformGizmo が 3D投影空間とキャンバス空間の不一致を起こしている。**

### 描画（正しく動作）

- `drawSelectionFrameOverlay()` (L28785) が 3D カメラの view/projection 行列を使ってコーナーハンドルをビューポート空間に投影描画 → OK

### ヒットテスト（正しく動作）

- `handleMousePress()` (L17633) → L18376-18407
- `hitTestProjectedFrameCorner()` (L5570) が 3D view/projection 行列でコーナーをビューポート空間に投影し、クリック位置と照合 → OK

### ドラッグ処理（ここが破綻）

1. `hitTestProjectedFrameCorner` がコーナーを検出 → `gizmo_->beginHandleDrag(frameHandle, viewportPos, ...)` を呼ぶ（2D TransformGizmo）
2. `beginHandleDrag()` 内 (L2505-2506):
   ```
   auto canvasMouse = renderer->viewportToCanvas(viewportPos);
   dragStartCanvasPos_ = QPointF(canvasMouse.x, canvasMouse.y);
   ```
   → **ビューポート位置を 2D キャンバス座標に変換**
3. `handleMouseMove()` → `gizmo_->handleMouseMove()` (L19762) → L2623:
   ```
   auto canvasMouse = renderer->viewportToCanvas(viewportPos);
   delta = currentCanvasPos - dragStartCanvasPos_;
   ```
   → 2Dキャンバス空間での差分を計算

### なぜ破綻するか

3Dレイヤーの投影フレームは**カメラの view/projection 行列を通してビューポートに投影**されている。一方、`viewportToCanvas()` で得られる 2D キャンバス座標は 2D コンポジション空間の座標であり、3D 投影とは無関係。

例:
- 3D キューブが 2D キャンバス上で (960, 540) にあっても、3D カメラの角度/距離によってビューポート上の投影位置はまったく異なる場所になる
- ユーザーがビューポート上で 100px ドラッグしたとき、それが 2D キャンバス空間で何 px に相当するかはズームとパンだけでは決まらない（3D 視点に依存する）

結果: ドラッグデルタが意図とまったく異なる値になり、フレームが動かない・または異常な場所に飛ぶ。

## 修正方針（案）

### 案A: 3D フレームリサイズを Artifact3DGizmo に委譲
`hitTestProjectedFrameCorner` がヒットした場合、`gizmo_->beginHandleDrag()` ではなく `gizmo3D_` の Scale モードで開始する。3D ギズモは view/projection 行列を考慮したインタラクションが可能。

### 案B: 投影空間でデルタを計算
`beginHandleDrag` / `handleMouseMove` に 3D レイヤー用の分岐を追加し、ビューポート空間のドラッグデルタを 3D localBounds のリサイズに変換する。`QVector3D::unproject()` 等を使用。

## キーファイル

| ファイル | 役割 |
|----------|------|
| ArtifactCompositionRenderController.cppm L5570-5622 | `hitTestProjectedFrameCorner()` |
| ArtifactCompositionRenderController.cppm L18376-18407 | frame corner press → beginHandleDrag |
| ArtifactCompositionRenderController.cppm L19760-19768 | gizmo mouse move dispatch |
| TransformGizmo.cppm L2486-2601 | `beginHandleDrag()` |
| TransformGizmo.cppm L2623-2992 | `handleMouseMove()` resize logic |
| TransformGizmo.cppm L3093 | `handleMouseRelease()` |
| Artifact3DGizmo.cppm | 3Dギズモ本体（Scaleモード対応済み） |
| ArtifactCompositionRenderController.cppm L18411-18442 | gizmo3D beginDrag |
| ArtifactCompositionRenderController.cppm L19640-19738 | gizmo3D mouse move → updateDrag |
