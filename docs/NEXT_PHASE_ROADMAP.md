# プロジェクトビュー → タイムライン統合の次フェーズ提案

## ✅ 実施済み改善（今回）

### **Phase 1: ドラッグ&ドロップの堅牢化**
- ✅ ファイルタイプ検証の強化
- ✅ エラーハンドリング充実
- ✅ ビジュアルフィードバック基盤

---

## 🚀 推奨される次フェーズ

### **Phase 2: ビジュアルフィードバック（推奨: 即実装）**

#### 2-1. ドラッグ中のハイライト表示
```cpp
// ArtifactLayerPanelWidget::Impl に追加
bool dragOverHighlight = false;

// dragEnterEvent内で設定
dragOverHighlight = true;

// dragLeaveEvent内で解除
dragOverHighlight = false;

// paintEvent内でハイライト描画
if (dragOverHighlight) {
  QPainter painter(this);
  painter.fillRect(rect(), QColor(100, 150, 255, 40));
  painter.drawRect(rect().adjusted(1, 1, -2, -2));
}
```

**効果:**
- ユーザーが「ここにドロップできる」と明確に認識
- プロ級アプリケーションの体験を提供

#### 2-2. カーソル変化
```cpp
void dragEnterEvent(QDragEnterEvent* e)
{
  // ... 検証コード ...
  if (isValidFiletype) {
    e->setDropAction(Qt::CopyAction);
    setCursor(Qt::DropCursor);  // または Qt::CopyDragCursor
    e->acceptProposedAction();
  } else {
    setCursor(Qt::ForbiddenCursor);
    e->ignore();
  }
}

void dragLeaveEvent(QDragLeaveEvent* e)
{
  unsetCursor();  // デフォルトカーソルに戻す
  e->accept();
}
```

---

### **Phase 3: ドロップ位置に基づく配置（推奨: 1週間後）**

#### 3-1. Y座標からレイヤーインデックスを計算
```cpp
int ArtifactLayerPanelWidget::calculateLayerIndexFromYPos(int yPos) const
{
  // レイヤーのY座標とドロップ位置を比較
  const int rowHeight = kLayerRowHeight;  // 28px
  const int headerHeight = kLayerHeaderHeight;  // 26px

  int layerIndex = (yPos - headerHeight) / rowHeight;

  auto visibleLayers = visibleTimelineRows();
  if (layerIndex < 0) layerIndex = 0;
  if (layerIndex > visibleLayers.size()) layerIndex = visibleLayers.size();

  return layerIndex;
}
```

#### 3-2. ドロップ位置プレビュー表示
```cpp
void ArtifactLayerPanelWidget::dragMoveEvent(QDragMoveEvent* e)
{
  const QMimeData* mime = e->mimeData();
  if (mime->hasUrls()) {
    // ドロップ予定位置を計算
    int targetIndex = calculateLayerIndexFromYPos(e->pos().y());

    // 挿入位置を示す線を描画（update()で paintEvent呼び出し）
    m_dragDropIndicatorLine = targetIndex * kLayerRowHeight + kLayerHeaderHeight;
    update();

    e->acceptProposedAction();
  } else {
    e->ignore();
  }
}
```

#### 3-3. 挿入位置に新規レイヤーを追加
```cpp
void dropEvent(QDropEvent* event)
{
  // ... 既存の検証コード ...

  int targetIndex = calculateLayerIndexFromYPos(event->pos().y());

  for (const auto& path : imported) {
    LayerType type = inferLayerTypeFromFile(path);
    ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);

    // 既存のaddLayerToCurrentComposition()では最上部に追加されるため、
    // 新しいメソッドが必要:
    // svc->insertLayerToCurrentComposition(params, targetIndex);

    // 代替案（既存APIのみで実現）:
    svc->addLayerToCurrentComposition(params);
    // その後、レイヤーを targetIndex 位置に移動するコマンドを実行
  }

  event->acceptProposedAction();
}
```

---

### **Phase 4: 複数ファイル一括処理（推奨: 2週間後）**

#### 4-1. UndoCommand統合
```cpp
void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
{
  // ... 検証コード ...

  if (auto* svc = ArtifactProjectService::instance()) {
    // 複数レイヤー追加を1つのUndoコマンドにまとめる
    QUndoCommand* macroCommand = new QUndoCommand("Add Multiple Layers from Project");

    auto imported = svc->importAssetsFromPaths(validPaths);
    for (const auto& path : imported) {
      LayerType type = inferLayerTypeFromFile(path);
      ArtifactLayerInitParams params(QFileInfo(path).baseName(), type);

      // 各レイヤー追加をサブコマンドとして記録
      new AddLayerToCompositionCommand(svc, params, macroCommand);
    }

    // 1つのアンドゥアクションで全レイヤーを削除可能に
    undoStack()->push(macroCommand);
  }

  event->acceptProposedAction();
}
```

#### 4-2. 複数レイヤーのアニメーション追加
```cpp
void ArtifactLayerPanelWidget::dropEvent(QDropEvent* event)
{
  // ... 既存コード ...

  // 追加されたレイヤーをハイライト表示
  QTimer::singleShot(100, this, [this, importedPaths]() {
    for (const auto& path : importedPaths) {
      LayerID layerId = findLayerBySourcePath(path);
      if (!layerId.isNil()) {
        highlightLayerTemporarily(layerId, 1000);  // 1秒間ハイライト
      }
    }
  });
}
```

---

### **Phase 5: エクスペリエンス向上（推奨: 1ヶ月後）**

#### 5-1. プロジェクトビューのサムネイル表示
```cpp
// ArtifactProjectView::paintEvent()
// 画像ファイルのサムネイルをプレビュー表示
QPixmap thumbnail = getThumbnailForFile(assetPath);
if (!thumbnail.isNull()) {
  painter.drawPixmap(itemRect, thumbnail.scaled(itemSize, Qt::KeepAspectRatio));
}
```

#### 5-2. ドラッグ中のカスタム画像表示
```cpp
void ArtifactProjectView::mouseMoveEvent(QMouseEvent* event)
{
  if (dragStarted && (event->pos() - dragStartPos).manhattanLength() > QApplication::startDragDistance()) {
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(selectedFileUrls());

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // カスタムドラッグピクスマップ生成
    QPixmap dragPixmap = generateDragPixmap(selectedFileUrls().size());
    drag->setPixmap(dragPixmap);

    drag->exec(Qt::CopyAction | Qt::MoveAction);
  }
}

QPixmap ArtifactProjectView::generateDragPixmap(int fileCount)
{
  QPixmap pixmap(100, 100);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);

  // 最初のサムネイル表示
  if (fileCount > 0) {
    QPixmap thumb = getThumbnailForFile(selectedFiles[0]);
    painter.drawPixmap(0, 0, thumb.scaledToWidth(80));
  }

  // ファイル数表示
  if (fileCount > 1) {
    painter.drawText(QRect(60, 60, 40, 40), Qt::AlignCenter,
                     QString("+ %1").arg(fileCount - 1));
  }

  return pixmap;
}
```

---

## 📈 実装優先度マップ

```
高効果 ┤
      │  ┌─────────────┐
      │  │Phase 2(推奨)│
      │  │ ビジュアル │
      │  └─────────────┘
      │    ┌─────────────┐
      │    │ Phase 3    │
      │    │ドロップ位置│
      │    └─────────────┘
      │      ┌─────────────┐
低効果 ┤      │Phase 5(将来)│
      └──────┴─────────────────→ 実装難度

优先実装: Phase 2 → Phase 3 → Phase 4
```

---

## 💡 クイック実装ガイド

### **30分で実装できる改善: Phase 2**
1. `Impl`にフラグ追加: `bool dragOverHighlight = false;`
2. `dragEnterEvent()`で`dragOverHighlight = true;`に設定
3. `dragLeaveEvent()`で`dragOverHighlight = false;`に設定
4. `paintEvent()`でハイライト描画
5. カーソル設定: `setCursor(Qt::DropCursor);`

### **2時間で実装できる改善: Phase 3**
1. `calculateLayerIndexFromYPos()`メソッド追加
2. `dragMoveEvent()`に挿入位置計算
3. `paintEvent()`に位置インジケーター描画
4. `dropEvent()`で位置考慮レイヤー追加

---

## ✅ 完了チェックリスト

- [x] Phase 1: ドラッグ&ドロップ堅牢化 ← **完了**
- [ ] Phase 2: ビジュアルフィードバック
- [ ] Phase 3: ドロップ位置に基づく配置
- [ ] Phase 4: 複数ファイル一括UndoCommand
- [ ] Phase 5: プレビュー＆エクスペリエンス向上

