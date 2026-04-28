# 調査報告書: コンポジットエディタのギズモゴースト現象

**調査日**: 2026-04-25  
**対象**: コンポジットエディタでオブジェクト選択時の左上に表示されるゴーストギズモ  
**重大度**: 🟡 中（視覚的バグ、機能への影響軽微）

---

## 現象の概要

オブジェクト（レイヤー）をピック選択すると：
1. 正常なフレームと四角いハンドルが表示される（正しい）
2. それとは別に、左上に「ゴーストのようなギズモ」が表示される（異常）

---

## 🔍 仮説

### 仮説1: 3Dギズモ（Artifact3DGizmo）がクリアされずに残っている

**確度**: ⭐⭐⭐⭐⭐ (高)

**メカニズム**:
1. コード内に2つの独立したギズモシステムが存在する：
   - `TransformGizmo`（2D用、通称GIZ-1）
   - `Artifact3DGizmo`（3D用、通称GIZ-2）

2. `syncGizmo3DFromLayer()` (ArtifactCompositionRenderController.cppm:1713) では、3Dレイヤーと2Dレイヤーの両方に対応している

3. **問題**: 3Dレイヤーを選択後、2Dレイヤーに切り替える際：
   - `gizmo3D_` が正しくクリア/無効化されていない
   - 前の3Dレイヤーの変換状態が残り、左上（原点近辺）に描画される

**関連コード**:
```cpp
// ArtifactCompositionRenderController.cppm:4884
if (gizmo3D_ && selectedLayer->is3D()) {
    syncGizmo3DFromLayer(selectedLayer);
    gizmo3D_->draw(renderer_.get(), view, proj);
}
```

**検証方法**:
- 3Dレイヤーを選択 → 2Dレイヤーを選択 → ゴーストが残るか確認
- `gizmo3D_` の `setTransform()` が呼ばれた後、適切にクリアされているか確認

---

### 仮説2: 2Dギズモのキャッシュバグによる二重描画

**確度**: ⭐⭐⭐⭐ (中高)

**メカニズム**:
1. 既知のバグ `BUG_TRANSFORM_GIZMO_CACHE_DISABLED_2026-04-19.md` により、2Dギズモのキャッシュが機能していない
2. キャッシュが無効化されているため、毎フレーム `cachedLayerTransform_` と実際の変換を比較しているが、タイミングにより：
   - 正しい位置での描画
   - デフォルト位置（0,0）での描画
   が同時に発生している可能性

**関連コード**:
```cpp
// TransformGizmo.cppm (キャッシュ無効化バグ)
cachedZoom_ = zoom;
cachedLayerTransform_ = globalTransform;
// ↑ 先にキャッシュを上書きしてしまう
const bool needsGeometryUpdate = 
    cachedZoom_ != zoom ||  // ← 常に false
    cachedLayerTransform_ != globalTransform ||  // ← 常に false
    cachedLocalRect_ != localRect;
```

**検証方法**:
- 2Dギズモの `draw()` が1フレーム内で複数回呼ばれていないか確認
- `cachedLayerTransform_` が適切に更新されているかデバッグ出力

---

### 仮説3: ギズモ描画順序の問題（2Dと3Dの重複）

**確度**: ⭐⭐⭐ (中)

**メカニズム**:
1. レンダリングループでは以下の順序でギズモが描画される：
   ```
   1. 2Dギズモ (gizmo_->draw())
   2. 条件付きで3Dギズモ (gizmo3D_->draw())
   ```

2. レイヤーの `is3D()` フラグが正しく設定されていない、または切り替えが遅延している場合：
   - 2Dギズモが正しい位置に描画
   - 3Dギズモも同時に別位置（左上など）に描画

**関連コード**:
```cpp
// ArtifactCompositionRenderController.cppm:4861-4929
if (showGizmoOverlay_ && gizmo_) {
    gizmo_->setLayer(selectedLayer);  // 2Dギズモ設定
    gizmo_->draw(renderer_.get());
    
    if (gizmo3D_ && selectedLayer->is3D()) {  // ← この判定が不安定？
        syncGizmo3DFromLayer(selectedLayer);
        gizmo3D_->draw(renderer_.get(), view, proj);
    }
}
```

**検証方法**:
- `selectedLayer->is3D()` の戻り値をログ出力
- 2Dレイヤー選択時に `gizmo3D_->draw()` が呼ばれていないか確認

---

### 仮説4: 座標変換のエラー（3Dギズモがスクリーン座標の原点に描画）

**確度**: ⭐⭐⭐ (中)

**メカニズム**:
1. `syncGizmo3DFromLayer()` で2Dレイヤーの場合、スクリーン座標に変換して設定：
   ```cpp
   // ArtifactCompositionRenderController.cppm:1726-1747
   const QPointF center = ...; // スクリーン座標での中心位置
   gizmo3D_->setTransform(QVector3D(center.x(), center.y(), 0.0f), ...);
   ```

2. しかし、変換行列（view/proj）が適用される際に：
   - 深度が無効化されているにもかかわらず、何らかの理由で原点（0,0,0）に描画される
   - または、`setTransform()` が呼ばれる前に `draw()` が実行される

**検証方法**:
- `syncGizmo3DFromLayer()` の前後で `gizmo3D_->position()` を確認
- `renderer_->getViewMatrix()` と `renderer_->getProjectionMatrix()` が正しい値を返しているか確認

---

### 仮説5: イベント順序の問題（選択状態の不整合）

**確度**: ⭐⭐ (低中)

**メカニズム**:
1. オブジェクトピック時のイベントフロー：
   - マウスプレス → ヒットテスト → 選択変更 → ギズモ更新
   
2. この過程で：
   - 古い選択状態のギズモがクリアされる前に新しいギズモが描画される
   - `LayerChangedEvent` の非同期処理により、状態が不整合になる

**関連コード**:
```cpp
// ArtifactCompositionRenderController.cppm:3418
impl_->gizmo_->setLayer(primaryLayer);
if (impl_->gizmo3D_ && primaryLayer) {
    impl_->syncGizmo3DFromLayer(primaryLayer);
}
```

**検証方法**:
- イベントバスの `LayerChangedEvent` 処理を一時的に無効化して再現するか確認
- `setLayer(nullptr)` が適切に呼ばれているか確認

---

## 🧪 推奨されるデバッグ手順

1. **まず最初に**: 3Dレイヤーと2Dレイヤーを切り替えて、ゴーストがいつ出現するか特定
2. `gizmo3D_` の `position()` と `isDragging()` を描画直前にログ出力
3. `Artifact3DGizmo::draw()` 内で、渡された変換行列が正しいか確認
4. 既知のキャッシュバグ（BUG_TRANSFORM_GIZMO_CACHE_DISABLED）を修正して、2Dギズモの動作を安定化
5. 3Dレイヤー非選択時に `gizmo3D_->setTransform()` を呼ばず、描画もスキップするように修正

---

## 🔬 追加調査結果（コード解析に基づく）

### 重要な発見: カメラ行列の不整合

**関連コード**: `ArtifactCompositionRenderController.cppm:5122-5123`
```cpp
// Using hardcoded zoom=1/pan=0 here placed the bounding box at wrong
// screen coordinates (appeared as an orange L-shape at top-left corner).
```

このコメントは、カメラ行列（zoom/pan）が正しく設定されていない場合、オブジェクトが画面左上に表示されることを示している。

### ギズモ描画の2つの経路

1. **2Dギズモ（TransformGizmo）**: 通常のレンダラー呼び出しを使用（zoom/panが適用されたキャンバス空間）
2. **3Dギズモ（Artifact3DGizmo）**: 専用のview/proj行列を使用

**問題の核心（淋漓: 4861-4929）**:
```cpp
// 3Dギズモの描画で使用される行列
if (viewportW > 0.0f && viewportH > 0.0f) {
    QMatrix4x4 view;
    view.translate(panX, panY, 0.0f);  // ← ここで取得したpan/zoomが古い可能性
    view.scale(zoom, zoom, 1.0f);
    QMatrix4x4 proj;
    proj.ortho(0.0f, viewportW, viewportH, 0.0f, -1000.0f, 1000.0f);
    gizmo3D_->draw(renderer_.get(), view, proj);
} else {
    // この分岐に入ると、rendererの古い行列が使用される
    gizmo3D_->draw(renderer_.get(), renderer_->getViewMatrix(), 
                   renderer_->getProjectionMatrix());
}
```

### 修正の方向性

1. **resetGizmoCameraMatrices()の確認**: 
   - `IRenderer::resetGizmoCameraMatrices()` が正しい行列を設定しているか確認
   - 必要に応じて `setGizmoCameraMatrices(view, proj)` を明示的に呼び出す

2. **3Dギズモの選択的描画**:
   - 2Dレイヤー選択時は `gizmo3D_` の描画を完全にスキップ
   - `syncGizmo3DFromLayer()` の呼び出しも条件分岐内のみに限定

3. **カメラ行列の不整合修正**:
   - `renderOneFrameImpl()` 内での `resetGizmoCameraMatrices()` の呼び出し順序を確認
   - オフスクリーンレンダリング後の行列復元が正しく行われているか確認

---

## 📝 補足: 関連ファイル

| ファイル | 役割 |
|---------|------|
| `Artifact/src/Widgets/Render/TransformGizmo.cppm` | 2Dギズモ実装（キャッシュバグあり） |
| `Artifact/src/Widgets/Render/Artifact3DGizmo.cppm` | 3Dギズモ実装 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | レンダリング制御・ギズモ描画呼び出し |
| `docs/bugs/BUG_TRANSFORM_GIZMO_CACHE_DISABLED_2026-04-19.md` | 既知の2Dギズモキャッシュバグ |
| `Artifact/include/Widgets/Render/TransformGizmo.ixx` | 2Dギズモヘッダ |
| `Artifact/include/Widgets/Render/Artifact3DGizmo.ixx` | 3Dギズモヘッダ |

---

**作成者**: AI Assistant  
**次のステップ**: 上記仮説に基づく修正の実施
