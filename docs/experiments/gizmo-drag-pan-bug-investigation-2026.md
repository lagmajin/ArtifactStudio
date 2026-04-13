# CompositeEditor 入力バグ調査レポート

**調査日:** 2026-04  
**対象ファイル:**
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

---

## Bug 1: ギズモドラッグで変形が反映されない (Move / Anchor / Rotate)

### 症状
Move / Anchor / Rotate ハンドルをドラッグしても、レイヤーの視覚的な位置・角度が更新されないことがある。

### 根本原因

`TransformGizmo::handleMouseMove` の Anchor ブロックと Rotate ブロックに、
以下のガード条件が存在していた：

```cpp
layer_->setDirty(LayerDirtyFlag::Transform);
if (!isDragging_) {  // ← 条件が逆
    if (auto* comp = ...) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(...);
    }
}
```

`handleMouseMove` は冒頭 (line ~1262) で  
```cpp
if (!isDragging_ || !layer_ || !renderer) return false;
```
という早期リターンがある。このため、`handleMouseMove` の本体が実行される時点で
`isDragging_` は **常に `true`** である。  
つまり `if (!isDragging_)` は **絶対に偽** となり、
`LayerChangedEvent` はドラッグ中に一切発行されなかった。

### レンダリングへの影響の伝播

```
LayerChangedEvent 未発行
  → invalidateBaseComposite() が呼ばれない
  → baseInvalidationSerial_ が変化しない
  → renderOneFrame() の RenderKeyState 比較で変化なしと判断
  → フレーム全体がスキップされる（early return）
```

`mouseMoveEvent` が 16ms タイマーで `renderOneFrame()` を呼んでいるが、
シリアル番号が変わらないため毎回スキップされ続ける。

### Scale が「動いて見えた」理由

Scale ハンドルは同じ `!isDragging_` バグを持っていたが、
`CompositionOverlayWidget::drawScaleGhost()` がレイヤーの `transformedBoundingBox()` を
直接参照して overlay を描画するため、Base Composite の更新なしに視覚フィードバックが得られていた。
Move / Anchor / Rotate にはこの ghost overlay がない。

### 修正内容

**TransformGizmo.cppm** の Anchor ハンドルブロック (line ~1393) および  
Rotate ハンドルブロック (line ~1420) の条件を変更：

```cpp
// 修正前
if (!isDragging_) {

// 修正後
if (isDragging_) {
```

`handleMouseMove` 内では `isDragging_` は常に `true` のため、
これにより `LayerChangedEvent` がドラッグ中に毎フレーム発行されるようになる。

Move ハンドルブロック (line ~1348) は先行セッションで同様に修正済み。

---

## Bug 2: ミドルマウスボタンでパンできない

### 症状
ミドルマウスボタンでクリック＆ドラッグしても、CompositeEditor がパンされない。

### 根本原因の候補 (複数)

#### 原因 1: mouseReleaseEvent Block 2 の条件が広すぎる

```cpp
// 修正前: LeftButton の解放でも isPanning_ が解除される
if ((event->button() == Qt::MiddleButton ||
     event->button() == Qt::LeftButton) && isPanning_) {
    isPanning_ = false;
```

ミドルボタンでパン中に左ボタンを離すと、`isPanning_` が解除されてしまう。

#### 原因 2: mouseReleaseEvent Block 3 の releaseMouse() が無条件

```cpp
// 修正前: パン中でも他ボタンのリリースで grabMouse が解放される
releaseMouse();
```

右クリックなど別ボタンのリリースで `releaseMouse()` が呼ばれ、
パン中のマウスグラブが失われる。

#### 原因 3: keyReleaseEvent の Space 処理が isPanningWithMiddle_ を考慮しない

```cpp
// 修正前: Space キー離しでミドルパンも停止してしまう
spacePressed_ = false;
isPanning_ = false;
```

Space + LeftButton パンとミドルパンを区別していなかった。

#### 原因 4: WA_NativeWindow 環境での grabMouse 信頼性

`WA_NativeWindow` + `WA_DontCreateNativeAncestors` + `WA_PaintOnScreen` の組み合わせで、
`grabMouse()` が Windows でイベントを確実にキャプチャできない場合がある。  
mousePressEvent でグラブしても mouseMoveEvent でボタン状態が `Qt::NoButton` になるケース。

### 修正内容

**ArtifactCompositionEditor.cppm** に以下の 6 点の変更を適用：

1. **`isPanningWithMiddle_` メンバ変数追加**  
   パンがミドルボタン起動か Space+Left 起動かを区別するフラグ。

2. **mousePressEvent でフラグをセット**  
   ```cpp
   isPanningWithMiddle_ = (event->button() == Qt::MiddleButton);
   ```

3. **mouseReleaseEvent Block 2 の条件修正**  
   ```cpp
   if (isPanning_ &&
       ((isPanningWithMiddle_ && event->button() == Qt::MiddleButton) ||
        (!isPanningWithMiddle_ && event->button() == Qt::LeftButton))) {
       isPanning_ = false;
       isPanningWithMiddle_ = false;
   ```
   各パンモードの正しいボタンリリースのみで解除される。

4. **mouseReleaseEvent Block 3 の releaseMouse() をガード**  
   ```cpp
   if (!isPanning_) {
       releaseMouse();
   }
   ```
   パン中は他ボタンのリリースでグラブが解除されない。

5. **keyReleaseEvent の Space 処理修正**  
   ```cpp
   if (!isPanningWithMiddle_) {
       isPanning_ = false;
   }
   ```
   ミドルボタンパン中は Space キー離しで停止しない。

6. **mouseMoveEvent にミドルボタン状態フォールバック追加**  
   ```cpp
   if (!isPanning_ && (event->buttons() & Qt::MiddleButton) && controller_) {
       isPanning_ = true;
       isPanningWithMiddle_ = true;
       lastMousePos_ = event->position();
   }
   ```
   `grabMouse()` が失敗して `mousePressEvent` が到達しなかったケースを
   `mouseMoveEvent` でのボタン状態から復元する。

---

## 今後の改善提案

- Move / Anchor / Rotate ハンドルにも Scale の ghost overlay を追加することで、
  render pipeline を経由せずに即時視覚フィードバックが得られる。
- `grabMouse()` の代替として、Qt の `QGuiApplication::setOverrideCursor()` と
  `QApplication::mouseButtons()` ポーリングによるフォールバックを検討。
- `CompositionViewport` で `WA_NativeWindow` が必要な理由 (Diligent Engine DX12 integration) は
  変えられないため、入力ハンドリングは常にボタン状態の直接チェックを優先すること。

---

## 変更ファイル一覧

| ファイル | 変更内容 |
|---|---|
| `Artifact/src/Widgets/Render/TransformGizmo.cppm` | Anchor/Rotate の `!isDragging_` → `isDragging_` (2箇所) |
| `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | `isPanningWithMiddle_` 追加、pan release ロジック修正、フォールバック追加 |
