# ShapeLayer Gizmo移動ラグ修正案

Date: 2026-04-25

## 修正対象

### 1. ShapeLayer::localBounds() キャッシュ導入（最も効果大）

**対象ファイル**: `Artifact/src/Layer/ArtifactShapeLayer.cppm`

**変更内容**:
- `Impl` 構造体に `mutable QRectF cachedLocalBounds_` と `bool localBoundsCacheDirty_ = true` を追加
- `localBounds()` 初呼び出し時にパス構築して結果をキャッシュ、以後はキャッシュを返す
- シェイプ形状に影響する全setter（`setSize`, `setShapeType`, `setCornerRadius`, `setStarPoints`, `setStarInnerRadius`, `setPolygonSides`, `setCustomPolygonPoints`, `setCustomPathVertices` など）で `localBoundsCacheDirty_ = true` を立てる

**効果**: `transformedBoundingBox()` → `localBounds()` のたびに O(path構築) が走っていたのが O(1) になる。Gizmoドラッグ中に毎フレームbenefit。

---

### 2. gizmoDragRenderTimer_ interval 引上げ

**対象ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

**変更内容**:
- `kGizmoDragRenderIntervalMs = 14` を `kGizmoDragRenderIntervalMs = 33` に変更（~70fps → ~30fps）
- ドラッグ中のレンダリング回数が半分になり、GPU詰まりが缓解

**効果**: ドラッグ中のフレームレンダリングコストが半分になり、ShapeLayer描画の重さが/science的に目立ちにくくなる。人間の体感では30fpsでも充分スムーズ。

---

### 3. PropertyWidget側のLayerChangedEvent debounce（プロパティ編集ラグ対応）

**対象ファイル**: `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

**変更内容**:
- `notifyLayerPropertyAnimationChanged()` 内の `globalEventBus().publish()` をdebounceする機構を追加
- 方法は `CompositionRenderController` 側の `gizmoDragRenderTimer_` 类似的に single-shot QTimer で遅延publish

**効果**: プロパティスライダーdrag中にイベントが詰まることを防止。Aのlag仮説→Bの修正に対応。

---

## 修正優先度とリスク

| 修正 | 優先度 | リスク | 副作用 |
|------|--------|--------|--------|
| 1. localBoundsキャッシュ | 高 | 低 | キャッシュinvalidation洩れバグの可能性（setter全部確認すればOK） |
| 2. gizmo interval引上げ | 高 | 極低 | ドラッグ中30fps落ちても通常は問題なし |
| 3. PropertyWidget debounce | 中 | 中 | 編集確定までの遅延がユーザーに不快感を与える可能性あり |

---

## 次

承認があれば実際に修正に入ります。哪个から手を付けるか決めたもらった上で进めます。
