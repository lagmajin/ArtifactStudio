# タイムライン左ペイン レイヤー順序変更 D&D 調査 (2026-03-27)

## 症状

タイムライン左ペイン（レイヤーパネル）でドラッグ＆ドロップでレイヤーの順番を変更できない。
ドラッグイベント自体が発火しない。

---

## 最終調査結果: QScrollArea 内での QDrag::exec() の問題

### 構造

```
ArtifactLayerTimelinePanelWrapper (QWidget)
  ├── header (ArtifactLayerPanelHeaderWidget)
  └── scroll (QScrollArea)
       └── panel (ArtifactLayerPanelWidget)  ← mousePressEvent / mouseMoveEvent 在这里
```

### 問題の根本原因

**`ArtifactLayerPanelWidget` は `QScrollArea` の内部ウィジェット。**

マウスイベントの流れ:
1. マウスクリック → `QScrollArea::viewport()` が受信
2. `QScrollArea` がビューポートイベントを内部ウィジェット (`panel`) に転送
3. `panel::mousePressEvent` が発火 → `dragCandidateLayerId` 設定
4. `panel::mouseMoveEvent` が発火 → `QDrag drag(this)` 作成 → `drag.exec()` 呼び出し
5. `drag.exec()` がブロック → **Qt がドラッグを開始**

**ここで問題が発生する可能性:**

`QScrollArea` のビューポートは、内部ウィジェットの `mousePressEvent` / `mouseMoveEvent` を転送するが、
`QDrag::exec()` はマウスキャプチャを奪う。スクロール領域のマウスキャプチャとの競合で、
ドラッグが正常に開始されないケースがある。

特に:
- `QScrollArea` のスクロール操作がマウスイベントを処理する場合
- `setDragEnabled` が未設定の QWidget で `QDrag` を使う場合

### 証拠

**`mouseMoveEvent` (line 1472-1495):**
```cpp
void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event) {
  if ((event->buttons() & Qt::LeftButton) && !impl_->dragCandidateLayerId.isNil()) {
    const int dragDistance = (event->pos() - impl_->dragStartPos).manhattanLength();
    if (impl_->draggedLayerId.isNil() && !impl_->dragStarted_ &&
        dragDistance >= QApplication::startDragDistance()) {
      impl_->dragStarted_ = true;
      impl_->draggedLayerId = impl_->dragCandidateLayerId;
      QDrag drag(this);                        // ← QDrag 作成
      const Qt::DropAction dropResult = drag.exec(Qt::MoveAction);  // ← ブロック
      impl_->clearDragState();
      // ...
    }
  }
}
```

**`dragEnterEvent` (line 1946):**
```cpp
void ArtifactLayerPanelWidget::dragEnterEvent(QDragEnterEvent* e) {
  if (mime->hasFormat(kLayerReorderMimeType)) {
    e->acceptProposedAction();  // ← accept している
  }
}
```

問題: `dragEnterEvent` は `panel` に到達するか？
→ `QScrollArea` のビューポートがドラッグイベントを受信するが、
   `LayerPanelDragForwardFilter` がビューポートからの DragEnter を `panel` に転送する。
→ しかし `QDrag::exec()` 後の最初の DragEnter イベントは、
   **ドラッグを開始したウィジェット自身に直接送られる**場合がある。
→ ビューポート経由でない場合、フィルターを通さず `panel::dragEnterEvent` が直接呼ばれる。
→ これは問題ないはず...。

**より深い問題:** `QScrollArea` の内部で `QDrag::exec()` を呼ぶと、
スクロール領域のマウス処理と競合する可能性がある。
`QScrollArea` は自前のスクロール操作（ドラッグスクロール）を持っており、
これが `QDrag::exec()` の開始を妨げる。

### 対策案

#### 対策1: QScrollArea を廃止して QWidget + QScrollBar に変更

`QScrollArea` を使わず、`QWidget` に直接 `QScrollBar` を接続。
スクロール位置を手動管理し、マウスイベントの転送問題を回避。

#### 対策2: drag を viewport 上で開始する

`mouseMoveEvent` ではなく、`scroll->viewport()` のイベントフィルターで
ドラッグを開始する:

```cpp
// LayerPanelDragForwardFilter の mouseMoveEvent ハンドリングを追加
if (event->type() == QEvent::MouseMove && leftButton) {
    // パネルに代わってドラッグを開始
    auto* drag = new QDrag(panel_);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    return true;
}
```

#### 対策3: QDrag を new で作成し deleteLater

スタック上の `QDrag drag(this)` だと `exec()` のブロック中に
オブジェクトライフタイムの問題が発生する可能性がある:

```cpp
// 現在: スタックオブジェクト
QDrag drag(this);
drag.exec(Qt::MoveAction);  // ブロック中も drag は有効

// 修正: ヒープオブジェクト
auto* drag = new QDrag(this);
drag->setMimeData(mime);
drag->exec(Qt::MoveAction);  // exec はブロックするが...
delete drag;  // 必要に応じて
```

ただし Qt のドキュメントではスタックオブジェクトでも問題ないとされている。

#### 対策4: DragDropMode を QTreeView に戻す

オーナードローをやめて、`QTreeView` + `QAbstractItemModel` の標準的な D&D 方式に戻す。
`flags()` で `ItemIsDragEnabled | ItemIsDropEnabled` を返し、
`setDragDropMode(QAbstractItemView::InternalMove)` を QTreeView に設定。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 789-793 | コンストラクタ — setAcceptDrops(true) |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1022-1059 | mousePressEvent — dragCandidate 設定 |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1472-1495 | mouseMoveEvent — QDrag::exec() 呼び出し |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1946-1982 | dragEnterEvent — accept |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 2007-2087 | dropEvent — moveLayer 呼び出し |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 166-218 | LayerPanelDragForwardFilter |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 2167-2184 | Wrapper — QScrollArea + eventFilter 設定 |

## 現状の実装（オーナードロー）

タイムラインは完全にオーナードローに変更済み。
`ArtifactLayerPanelWidget` (QWidget, QPainter 描画) + `TimelineTrackView` (QGraphicsView) の構成。

### D&D フロー（実装済み）

```
mousePressEvent
  → dragCandidateLayerId = layer->id()     (line 1056)
  → dragStartPos = event->pos()            (line 1055)

mouseMoveEvent
  → dragDistance >= startDragDistance()      (line 1475)
  → QDrag drag(this)                        (line 1483)
  → mime->setData(kLayerReorderMimeType, id.toString().toUtf8())  (line 1480)
  → drag.exec(Qt::MoveAction)               (line 1488) ← ブロッキング

dragEnterEvent
  → mime->hasFormat(kLayerReorderMimeType) → accept              (line 1949)

dragMoveEvent
  → dragInsertVisibleRow = insertionVisibleRowForY(y)             (line 1988)
  → accept, update()                                              (line 1989-1990)

dropEvent
  → dragLayerId = LayerID(mime->data(...))                        (line 2020)
  → targetIndex = layerCountBeforeVisibleRowExcluding(...)        (line 2050-2053)
  → svc->moveLayerInCurrentComposition(dragLayerId, newIndex)     (line 2079)
  → updateLayout()                                                (line 2080)
```

### 設定確認済み

| 設定 | 場所 | 状態 |
|------|------|------|
| `setAcceptDrops(true)` | LayerPanelWidget.cppm:793 | ✅ 設定済み |
| `scroll->viewport()->setAcceptDrops(true)` | LayerPanelWidget.cppm:2192 | ✅ 設定済み |
| `kLayerReorderMimeType` 定義 | LayerPanelWidget.cppm:69 | ✅ |
| `QDrag` 作成 | LayerPanelWidget.cppm:1483 | ✅ |
| `drag.exec(Qt::MoveAction)` | LayerPanelWidget.cppm:1488 | ✅ |
| `svc->moveLayerInCurrentComposition()` | LayerPanelWidget.cppm:2079 | ✅ |

### 裏側のメソッド確認済み

| メソッド | 場所 | 動作確認 |
|---------|------|---------|
| `moveLayerInCurrentComposition()` | ArtifactProjectService.cpp:559-576 | ✅ `moveLayerToIndex()` を呼ぶ |
| `moveLayerToIndex()` | ArtifactAbstractComposition.cppm:239-247 | ✅ `layerMultiIndex_.move()` + `changed()` シグナル |

---

## 仮説（実装確認後の残存問題）

### ★★★ 1. drag.exec() がブロックする問題

**場所:** `ArtifactLayerPanelWidget.cppm:1488`

```cpp
const Qt::DropAction dropResult = drag.exec(Qt::MoveAction);
impl_->clearDragState();
```

`QDrag::exec()` は**ブロッキング**呼び出し。
マウスがウィジェットの外に出ると、ドラッグが `Qt::IgnoreAction` で終了する可能性がある。

特に:
- スクロール位置が変わるとドロップ位置の計算がずれる
- ウィンドウ外に出ると `dragLeaveEvent` が発火し `dragInsertVisibleRow = -1` にリセット

### ★★ 2. スクロール位置が考慮されていない

**場所:** `ArtifactLayerPanelWidget.cppm:1498`

```cpp
int idx = event->pos().y() / kLayerRowHeight;
```

`mouseMoveEvent` の `event->pos()` はビューポート座標。
`dragMoveEvent` の `e->position()` もビューポート座標。

但是如果 `QScrollArea` のスクロール位置がある場合、`event->pos()` とスクロール位置のオフセットがずれる可能性がある。

`insertionVisibleRowForY()` でスクロールオフセットを考慮しているか確認が必要。

### ★ 3. QDrag のホットスポットが不正確

**場所:** `ArtifactLayerPanelWidget.cppm:1485`

```cpp
drag.setHotSpot(event->pos() - impl_->dragStartPos);
```

ホットスポットが 0,0 だとドロップ位置がずれる。

### ★ 4. updateLayout() が呼ばれない

`dropEvent` で `updateLayout()` が呼ばれているが (line 2080)、
`notifyProjectMutation()` の後にタイマー遅延で更新される場合、
即座に UI に反映されない。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1472-1495 | mouseMoveEvent — QDrag 作成 |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1946-1982 | dragEnterEvent |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1984-1998 | dragMoveEvent |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 2007-2087 | dropEvent — moveLayer 呼び出し |
| `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` | 1053-1059 | mousePressEvent — dragCandidate 設定 |
| `Artifact/src/Service/ArtifactProjectService.cpp` | 559-576 | moveLayerInCurrentComposition |
| `Artifact/src/Composition/ArtifactAbstractComposition.cppm` | 239-247 | moveLayerToIndex |
