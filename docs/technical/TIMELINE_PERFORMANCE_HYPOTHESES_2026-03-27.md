# レイヤータイムラインウィンドウ 性能改善仮説レポート (2026-03-27)

**作成日:** 2026-03-27  
**ステータス:** 調査完了・仮説立案  
**関連コンポーネント:** TimelineTrackView, ArtifactLayerPanelWidget, TimelineScene, ClipItem

---

## 概要

レイヤータイムラインウィンドウ（`ArtifactTimelineWidget`）の性能ボトルネックを特定し、改善仮説を立てた。

主な問題領域：
1. **QGraphicsView のオーバーヘッド** - 大量の QGraphicsItem の描画・イベント処理
2. **レイヤーパネルの全再描画** - `updateLayout()` によるフルリフレッシュ
3. **シグナル過多** - フレーム変更・プロパティ変更時の過剰な更新
4. **ドラッグ＆ドロップのブロッキング** - `QDrag::exec()` による UI フリーズ

---

## 目次

1. [アーキテクチャ概要](#1-アーキテクチャ概要)
2. [ボトルネック仮説](#2-ボトルネック仮説)
3. [改善案](#3-改善案)
4. [実装優先度](#4-実装優先度)
5. [関連ドキュメント](#5-関連ドキュメント)

---

## 1. アーキテクチャ概要

### 1.1 タイムライン構成

```
ArtifactTimelineWidget (QWidget)
├── ArtifactLayerTimelinePanelWrapper (左ペイン)
│   ├── ArtifactLayerPanelHeaderWidget
│   └── QScrollArea
│        └── ArtifactLayerPanelWidget (QWidget, オーナードロー)
└── TimelineTrackView (QGraphicsView, 右ペイン)
     └── TimelineScene (QGraphicsScene)
          ├── ClipItem (QGraphicsObject) × N レイヤー
          └── ResizeHandle (QGraphicsObject) × 2 per Clip
```

### 1.2 描画フロー

**左ペイン (ArtifactLayerPanelWidget):**
```
レイヤー変更
  → updateLayout()
  → performUpdateLayout()
  → paintEvent()
  → 全レイヤー行を QPainter で描画
```

**右ペイン (TimelineTrackView):**
```
フレーム変更
  → playhead 更新
  → QGraphicsScene 再描画
  → 全 ClipItem::paint() 呼び出し
```

---

## 2. ボトルネック仮説

### ★★★ 仮説 1: QGraphicsView の描画オーバーヘッド

**場所:** `TimelineTrackView (QGraphicsView)` + `ClipItem (QGraphicsObject)`

**問題:**
- `QGraphicsView` は高機能だが、大量のアイテム（N レイヤー × 2 ハンドル）で重くなる
- 各 `ClipItem` が個別に `paint()` を呼び出し、QPainter のステート変更が頻発
- `QGraphicsScene` のインデックス構造が N 増で探索コスト増

**影響:**
- 100 レイヤーで 200 個の `QGraphicsItem` + ハンドル
- スクラブ中に全アイテムの再描画が発生
- 60fps 維持が困難

**証拠:**
```cpp
// ClipItem::paint() が毎フレーム呼ばれる
void ClipItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
    // 矩形描画、テキスト描画、ハンドル描画
    // ... 毎フレーム全クリップで実行
}
```

**仮説の確度:** **高** (QGraphicsView の一般的な制約)

---

### ★★★ 仮説 2: レイヤーパネルの全再描画

**場所:** `ArtifactLayerPanelWidget::updateLayout()`

**問題:**
- レイヤー 1 つの追加/削除/移動で **全レイヤー行を再描画**
- `performUpdateLayout()` がレイヤー数分ループ
- `paintEvent()` で全行を 0 から描画

**影響:**
- 100 レイヤーで 100 行の再描画
- レイヤー追加ごとに 0.1-0.3 秒のラグ

**証拠:**
```cpp
void ArtifactLayerPanelWidget::updateLayout() {
    performUpdateLayout();  // 全レイヤーのレイアウト再計算
    update();  // 全領域の再描画
}
```

**仮説の確度:** **高** (コードから明確)

---

### ★★ 仮説 3: フレーム変更時の過剰なシグナル

**場所:** `ArtifactTimelineWidget`, `TimelineTrackView`

**問題:**
- フレーム変更 → `playhead` 更新 → `update()` → 全再描画
- `seekPositionChanged` シグナルが他ウィジェットに伝播
- プロパティウィジェットが `scheduleRebuild()` を発火

**シグナルフロー:**
```
frameChanged
  → TimelineTrackView::setPosition()
  → update()
  → drawForeground() (playhead 描画)
  → seekPositionChanged シグナル
  → PropertyWidget::scheduleRebuild()  ← 重い！
```

**影響:**
- スクラブ中にプロパティウィジェットが毎フレームリビルド
- CPU 使用率が跳ね上がる

**仮説の確度:** **中** (プロパティウィジェットの既知の問題から推測)

---

### ★★ 仮説 4: ドラッグ＆ドロップのブロッキング

**場所:** `ArtifactLayerPanelWidget::mouseMoveEvent()`

**問題:**
- `QDrag::exec()` が**ブロッキング**呼び出し
- ドラッグ中に UI スレッドがブロック
- スクロール位置の同期が困難

**証拠:**
```cpp
void ArtifactLayerPanelWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragDistance >= QApplication::startDragDistance()) {
        QDrag drag(this);
        const Qt::DropAction dropResult = drag.exec(Qt::MoveAction);  // ← ブロック
        impl_->clearDragState();
    }
}
```

**影響:**
- ドラッグ開始時に UI が一瞬フリーズ
- スクロールしながらドラッグできない

**関連:** `docs/bugs/TIMELINE_LAYER_REORDER_INVESTIGATION_2026-03-27.md` で詳細調査済み

**仮説の確度:** **高** (実装から明確)

---

### ★ 仮説 5: レイヤーパネルのスクロール転送オーバーヘッド

**場所:** `ArtifactLayerTimelinePanelWrapper`, `LayerPanelDragForwardFilter`

**問題:**
- `QScrollArea` の内部ウィジェットでイベントフィルターを使用
- マウスイベントの転送が多重
- ドラッグイベントの転送が複雑

**影響:**
- イベント処理の遅延
- ドラッグ開始の判定が不安定

**仮説の確度:** **中** (間接的な証拠あり)

---

### ★ 仮説 6: ClipItem のアイテム管理コスト

**場所:** `TimelineScene::Impl`

**問題:**
- `std::vector<ClipItem*> clips_` を線形探索
- `clipTracks_` (unordered_map) のハッシュ計算
- ドラッグ時に `initialPos`, `initialTrack` をコピー

**証拠:**
```cpp
struct DragContext
{
  bool active = false;
  ClipItem* anchorClip = nullptr;
  int anchorTrack = -1;
  double anchorStartX = 0.0;
  std::vector<ClipItem*> movingClips;
  std::unordered_map<ClipItem*, QPointF> initialPos;
  std::unordered_map<ClipItem*, int> initialTrack;
} drag_;
```

**影響:**
- 複数クリップ選択時にコピーコスト
- ドラッグ開始時のメモリアロケーション

**仮説の確度:** **低** (影響は小)

---

## 3. 改善案

### 3.1 仮説 1 対策: QGraphicsView をオーナードローに変更

**内容:**
`TimelineTrackView` を `QGraphicsView` から `QWidget` に変更し、`QPainter` で直接描画。

**メリット:**
- `QGraphicsItem` のオーバーヘッドが削除
- 描画をバッチ処理可能
- メモリ使用量削減

**デメリット:**
- 衝突判定を自前で実装
- 拡大縮小・スクロールを自前で実装

**工数:** 大 (1-2 週間)

**コード例:**
```cpp
// 現在: QGraphicsView + QGraphicsItem
class TimelineTrackView : public QGraphicsView { ... };
class ClipItem : public QGraphicsObject { ... };

// 修正後: QWidget + オーナードロー
class TimelineTrackView : public QWidget {
protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter painter(this);
        // クリップをバッチ描画
        for (const auto& clip : visibleClips_) {
            drawClip(&painter, clip);
        }
        // playhead を描画
        drawPlayhead(&painter);
    }
};
```

---

### 3.2 仮説 2 対策: 差分更新の導入

**内容:**
`updateLayout()` を変更し、変更があった行のみ再描画。

**実装方針:**
1. 各行の状態をキャッシュ（矩形、テキスト、アイコン）
2. 変更検出フラグを追加
3. `paintEvent()` でキャッシュされた矩形のみ描画

**工数:** 中 (2-3 日)

**コード例:**
```cpp
class ArtifactLayerPanelWidget {
private:
    struct RowCache {
        LayerID id;
        QRect rect;
        QString name;
        bool dirty = true;  // 変更フラグ
    };
    QVector<RowCache> rowCaches_;

    void updateRow(int index) {
        if (index < rowCaches_.size()) {
            rowCaches_[index].dirty = true;
            update(rowCaches_[index].rect);  // 一部のみ更新
        }
    }

    void paintEvent(QPaintEvent* event) {
        QPainter painter(this);
        for (const auto& row : rowCaches_) {
            if (!row.dirty && !event->rect().intersects(row.rect)) {
                continue;  // キャッシュ使用、描画スキップ
            }
            drawRow(&painter, row);
        }
    }
};
```

---

### 3.3 仮説 3 対策: シグナルの間引き

**内容:**
フレーム変更時のシグナル発火を制限。

**実装方針:**
1. `debounce` 機構の導入（16ms 間隔）
2. プロパティウィジェットのリビルドを遅延
3. スクラブ中は playhead 描画のみ

**工数:** 小 (1 日)

**コード例:**
```cpp
class ArtifactTimelineWidget {
private:
    QElapsedTimer lastSignalTime_;
    static constexpr int kMinSignalIntervalMs = 16;

    void setPosition(double frame) {
        if (lastSignalTime_.isValid() &&
            lastSignalTime_.elapsed() < kMinSignalIntervalMs) {
            return;  // 間引き
        }
        lastSignalTime_.start();
        emit seekPositionChanged(frame / duration());
    }
};
```

---

### 3.4 仮説 4 対策: QDrag を非ブロッキングに

**内容:**
`QDrag::exec()` を使わず、マウスイベントでドラッグを自前実装。

**実装方針:**
1. `mouseMoveEvent` でドラッグ中の座標を更新
2. `paintEvent` でドラッグ中の視覚効果を描画
3. `mouseReleaseEvent` でドロップ処理

**工数:** 中 (2-3 日)

**コード例:**
```cpp
class ArtifactLayerPanelWidget {
private:
    struct DragState {
        bool active = false;
        LayerID draggedId;
        int dropIndex = -1;
        QPoint currentPos;
    } dragState_;

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragState_.active) {
            dragState_.currentPos = event->pos();
            dragState_.dropIndex = insertionRowForY(event->pos().y());
            update();  // ドラッグ中の視覚効果を更新
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (dragState_.active) {
            // ドロップ処理
            moveLayer(dragState_.draggedId, dragState_.dropIndex);
            dragState_.active = false;
            update();
        }
    }
};
```

---

### 3.5 仮説 5 対策: QScrollArea を最適化

**内容:**
`QScrollArea` のイベントフィルターを簡素化。

**実装方針:**
1. `viewportEvent()` をオーバーライド
2. 不要なイベント転送を削除
3. スクロールバーとパネルの同期を改善

**工数:** 小 (0.5 日)

---

### 3.6 仮説 6 対策: データ構造の最適化

**内容:**
`TimelineScene::Impl` のデータ構造を最適化。

**実装方針:**
1. `std::vector` を `QVector` に変更（Qt との統合）
2. ドラッグコンテキストをプール
3. ハッシュマップの事前確保

**工数:** 小 (0.5 日)

---

## 4. 実装優先度

### 優先度 P0 (即時対応)

| 仮説 | 対策 | 効果 | 工数 | リスク |
|------|------|------|------|--------|
| 仮説 2 | 差分更新の導入 | 大 | 中 | 低 |
| 仮説 3 | シグナルの間引き | 中 | 小 | 低 |

**理由:** 実装が容易で、効果が大きい。

---

### 優先度 P1 (短期対応)

| 仮説 | 対策 | 効果 | 工数 | リスク |
|------|------|------|------|--------|
| 仮説 4 | QDrag を非ブロッキングに | 中 | 中 | 中 |
| 仮説 5 | QScrollArea 最適化 | 小 | 小 | 低 |

**理由:** UX 向上に寄与。

---

### 優先度 P2 (中長期対応)

| 仮説 | 対策 | 効果 | 工数 | リスク |
|------|------|------|------|--------|
| 仮説 1 | QGraphicsView をオーナードローに | 大 | 大 | 高 |
| 仮説 6 | データ構造最適化 | 小 | 小 | 低 |

**理由:** 大規模変更のため、十分なテストが必要。

---

## 5. 関連ドキュメント

### 5.1 内部ドキュメント

- `docs/bugs/TIMELINE_LAYER_REORDER_INVESTIGATION_2026-03-27.md` - D&D 調査
- `docs/bugs/TIMELINE_DRAG_DROP_NOT_WORKING_2026-03-27.md` - D&D 不具合
- `docs/bugs/PROPERTY_WIDGET_PERFORMANCE_2026-03-27.md` - プロパティウィジェット性能
- `docs/WIDGET_MAP.md` - ウィジェット対応表

### 5.2 関連コード

| ファイル | 内容 |
|---------|------|
| `Artifact/include/Widgets/ArtifactTimelineWidget.ixx` | タイムライン全体 |
| `Artifact/include/Widgets/Timeline/ArtifactLayerPanelWidget.ixx` | 左ペイン |
| `Artifact/include/Widgets/Timeline/ArtifactTimelineObjects.ixx` | ClipItem |
| `Artifact/src/Widgets/ArtifactTimelineScene.cppm` | TimelineScene |
| `Artifact/src/Widgets/Timeline/ArtifactLayerHierarchyWidget.cppm` | 階層ビュー |

---

## 付録 A: パフォーマンス測定指標

### 測定項目

| 項目 | 現在値 (推定) | 目標値 |
|------|-------------|--------|
| フレームレート (スクラブ中) | 15-30fps | 60fps |
| レイヤー追加時のラグ | 100-300ms | <50ms |
| ドラッグ開始遅延 | 50-100ms | <16ms |
| メモリ使用量 (100 レイヤー) | 不明 | <50MB |

### 測定方法

1. **QElapsedTimer** で各操作の時間を計測
2. **Qt Creator のプロファイラー** で CPU 使用率を測定
3. **Perfetto / GPUView** でフレームタイミングを可視化

---

## 付録 B: 実装チェックリスト

### 差分更新 (仮説 2)

- [ ] `RowCache` 構造体を定義
- [ ] `updateRow(int index)` を実装
- [ ] `paintEvent()` でキャッシュを使用
- [ ] 変更検出ロジックを追加
- [ ] テスト（100 レイヤーでの追加/削除）

### シグナル間引き (仮説 3)

- [ ] `QElapsedTimer` を追加
- [ ] `setPosition()` で間引きロジック
- [ ] プロパティウィジェットの `scheduleRebuild()` を遅延
- [ ] テスト（スクラブ中の CPU 使用率）

### QDrag 非ブロッキング (仮説 4)

- [ ] `DragState` 構造体を定義
- [ ] `mouseMoveEvent()` でドラッグ座標更新
- [ ] `paintEvent()` でドラッグ視覚効果
- [ ] `mouseReleaseEvent()` でドロップ処理
- [ ] テスト（スクロール中のドラッグ）

---

**文書終了**
