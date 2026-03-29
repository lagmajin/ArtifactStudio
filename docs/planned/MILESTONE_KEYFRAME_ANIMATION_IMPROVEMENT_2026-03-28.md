# キーフレーム・アニメーション改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactPropertyWidget, ArtifactTimelineWidget, AnimatableProperty, KeyframeManager

---

## 概要

キーフレーム編集とアニメーション機能を改善し、直感的で効率的なアニメーション制作を可能にする。

---

## 発見された問題点

### ★★★ 問題 1: キーフレームエディタの未実装

**場所:** `Artifact/src/Widgets/ArtifactPropertyWidget.cppm`

**問題:**
- プロパティウィジェットにキーフレーム機能がない
- タイムライン上での編集のみ
- 値の直接編集が困難

**影響:**
- 細かい調整が困難
- アニメーション制作に時間

**工数:** 12-16 時間

---

### ★★★ 問題 2: グラフエディタの不足

**場所:** 全体

**問題:**
- イージングカーブの視覚化がない
- ベジェハンドルでの調整ができない
- グラフ上でのキーフレーム移動ができない

**影響:**
- 直感的な調整ができない
- 複雑なイージングが困難

**工数:** 16-20 時間

---

### ★★ 問題 3: キーフレーム操作の不足

**場所:** 全体

**問題:**
- コピー＆ペーストがない
- 複数キーフレームの選択ができない
- キーフレームの簡易化（簡略化）がない

**工数:** 8-10 時間

---

### ★★ 問題 4: イージングプリセットの不足

**場所:** `ArtifactCore/include/Animation/Keyframe.ixx`

**問題:**
- イージングの種類が少ない
- カスタムイージングが保存できない
- プリセットの管理がない

**工数:** 6-8 時間

---

### ★ 問題 5: モーショングラフの未実装

**場所:** 全体

**問題:**
- レイヤーの動きを視覚化できない
- パスの編集ができない
- 3D モーションの可視化がない

**工数:** 12-16 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **キーフレームエディタ** | 12-16h | 🔴 高 |
| **グラフエディタ** | 16-20h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **キーフレーム操作** | 8-10h | 🟡 中 |
| **イージングプリセット** | 6-8h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **モーショングラフ** | 12-16h | 🟢 低 |

**合計工数:** 54-70 時間

---

## Phase 構成

### Phase 1: キーフレームエディタ

- 目的:
  - プロパティごとのキーフレーム編集

- 作業項目:
  - キーフレームリスト表示
  - 値の直接編集
  - タイミング調整

- 完了条件:
  - キーフレームの追加/削除/編集
  - 値と時間の直接入力

- 実装案:
  ```cpp
  class KeyframeEditorWidget : public QWidget {
      Q_OBJECT
      
  public:
      KeyframeEditorWidget(QWidget* parent = nullptr)
          : QWidget(parent) {
          
          auto layout = new QVBoxLayout(this);
          
          // キーフレームリスト
          keyframeList_ = new QTreeWidget();
          keyframeList_->setHeaderLabels(
              {"Time", "Value", "Interpolation"});
          keyframeList_->setAllColumnsShowFocus(true);
          layout->addWidget(keyframeList_);
          
          // 編集コントロール
          auto editLayout = new QHBoxLayout();
          
          timeEdit_ = new QDoubleSpinBox();
          timeEdit_->setSuffix(" f");
          editLayout->addWidget(new QLabel("Time:"));
          editLayout->addWidget(timeEdit_);
          
          valueEdit_ = new QLineEdit();
          editLayout->addWidget(new QLabel("Value:"));
          editLayout->addWidget(valueEdit_);
          
          addBtn_ = new QPushButton("Add");
          editLayout->addWidget(addBtn_);
          
          removeBtn_ = new QPushButton("Remove");
          editLayout->addWidget(removeBtn_);
          
          layout->addLayout(editLayout);
          
          // シグナル接続
          connect(addBtn_, &QPushButton::clicked,
                  this, &KeyframeEditorWidget::addKeyframe);
          connect(removeBtn_, &QPushButton::clicked,
                  this, &KeyframeEditorWidget::removeKeyframe);
          connect(keyframeList_, &QTreeWidget::itemSelectionChanged,
                  this, &KeyframeEditorWidget::onSelectionChanged);
      }
      
      void setProperty(ArtifactProperty* prop) {
          currentProperty_ = prop;
          refreshList();
      }
      
  signals:
      void keyframeAdded(int frame, const QVariant& value);
      void keyframeRemoved(int frame);
      void keyframeModified(int frame, const QVariant& value);
      
  private slots:
      void addKeyframe() {
          int frame = static_cast<int>(timeEdit_->value());
          QVariant value = parseValue(valueEdit_->text());
          
          currentProperty_->addKeyframe(frame, value);
          emit keyframeAdded(frame, value);
          refreshList();
      }
      
      void removeKeyframe() {
          auto items = keyframeList_->selectedItems();
          for (auto item : items) {
              int frame = item->data(0, Qt::UserRole).toInt();
              currentProperty_->removeKeyframe(frame);
              emit keyframeRemoved(frame);
          }
          refreshList();
      }
      
      void refreshList() {
          keyframeList_->clear();
          
          if (!currentProperty_) return;
          
          auto keyframes = currentProperty_->keyframes();
          for (const auto& kf : keyframes) {
              auto item = new QTreeWidgetItem(keyframeList_);
              item->setData(0, Qt::DisplayRole, kf.frame);
              item->setData(1, Qt::DisplayRole, kf.value.toString());
              item->setData(2, Qt::DisplayRole, 
                          interpolationToString(kf.interpolation));
              item->setData(0, Qt::UserRole, kf.frame);
          }
      }
      
  private:
      QTreeWidget* keyframeList_;
      QDoubleSpinBox* timeEdit_;
      QLineEdit* valueEdit_;
      QPushButton *addBtn_, *removeBtn_;
      ArtifactProperty* currentProperty_ = nullptr;
  };
  ```

### Phase 2: グラフエディタ

- 目的:
  - イージングカーブの視覚的編集

- 作業項目:
  - グラフ表示
  - ベジェハンドル
  - カーブ編集

- 完了条件:
  - グラフ上でキーフレームを移動
  - イージングを視覚的に調整

- 実装案:
  ```cpp
  class GraphEditorWidget : public QWidget {
      Q_OBJECT
      
  public:
      GraphEditorWidget(QWidget* parent = nullptr)
          : QWidget(parent) {
          setMinimumSize(400, 300);
          setMouseTracking(true);
          
          zoom_ = 1.0;
          offset_ = QPointF(0, 0);
          
          connect(this, &GraphEditorWidget::curveChanged,
                  this, &GraphEditorWidget::update);
      }
      
      void setProperty(ArtifactProperty* prop) {
          currentProperty_ = prop;
          update();
      }
      
  protected:
      void paintEvent(QPaintEvent* event) override {
          QPainter painter(this);
          painter.setRenderHint(QPainter::Antialiasing);
          
          // グリッド描画
          drawGrid(&painter);
          
          // カーブ描画
          if (currentProperty_) {
              drawCurve(&painter);
              drawKeyframes(&painter);
              drawHandles(&painter);
          }
      }
      
      void mousePressEvent(QMouseEvent* event) override {
          if (event->button() == Qt::LeftButton) {
              // キーフレームまたはハンドルを選択
              selectedHandle_ = findHandleAt(event->pos());
              if (selectedHandle_) {
                  dragging_ = true;
              }
          }
      }
      
      void mouseMoveEvent(QMouseEvent* event) override {
          if (dragging_ && selectedHandle_) {
              // ハンドルを移動
              QPointF newPos = screenToGraph(event->pos());
              selectedHandle_->setPosition(newPos);
              emit curveChanged();
          }
      }
      
  private:
      void drawGrid(QPainter* painter) {
          painter->setPen(QPen(QColor(60, 60, 60), 1));
          
          // 時間軸グリッド
          for (int i = 0; i < width(); i += 50) {
              painter->drawLine(i, 0, i, height());
          }
          
          // 値軸グリッド
          for (int i = 0; i < height(); i += 50) {
              painter->drawLine(0, i, width(), i);
          }
      }
      
      void drawCurve(QPainter* painter) {
          painter->setPen(QPen(QColor(100, 149, 237), 2));
          
          auto keyframes = currentProperty_->keyframes();
          if (keyframes.size() < 2) return;
          
          QPainterPath path;
          path.moveTo(graphToScreen(keyframes[0].frame, 
                                    keyframes[0].value.toFloat()));
          
          for (int i = 1; i < keyframes.size(); ++i) {
              QPointF p1 = graphToScreen(keyframes[i-1].frame, 
                                        keyframes[i-1].value.toFloat());
              QPointF p2 = graphToScreen(keyframes[i].frame, 
                                        keyframes[i].value.toFloat());
              
              // ベジェカーブ
              QPointF h1 = p1 + keyframes[i-1].outHandle;
              QPointF h2 = p2 + keyframes[i].inHandle;
              
              path.cubicTo(h1, h2, p2);
          }
          
          painter->drawPath(path);
      }
      
      void drawKeyframes(QPainter* painter) {
          auto keyframes = currentProperty_->keyframes();
          
          for (const auto& kf : keyframes) {
              QPointF pos = graphToScreen(kf.frame, kf.value.toFloat());
              
              painter->setBrush(QColor(255, 255, 255));
              painter->setPen(QPen(QColor(0, 0, 0), 1));
              painter->drawEllipse(pos, 6, 6);
          }
      }
      
      void drawHandles(QPainter* painter) {
          auto keyframes = currentProperty_->keyframes();
          
          for (const auto& kf : keyframes) {
              QPointF pos = graphToScreen(kf.frame, kf.value.toFloat());
              
              // In ハンドル
              QPointF inPos = pos + kf.inHandle;
              painter->drawLine(pos, inPos);
              painter->drawEllipse(inPos, 4, 4);
              
              // Out ハンドル
              QPointF outPos = pos + kf.outHandle;
              painter->drawLine(pos, outPos);
              painter->drawEllipse(outPos, 4, 4);
          }
      }
      
      QPointF graphToScreen(float frame, float value) {
          float x = (frame - offset_.x()) * zoom_;
          float y = height() - (value - offset_.y()) * zoom_;
          return QPointF(x, y);
      }
      
      QPointF screenToGraph(const QPointF& screen) {
          float frame = screen.x() / zoom_ + offset_.x();
          float value = (height() - screen.y()) / zoom_ + offset_.y();
          return QPointF(frame, value);
      }
      
      QPointF* findHandleAt(const QPointF& pos) {
          // 最も近いハンドルを探す
          // ...
          return nullptr;
      }
      
      ArtifactProperty* currentProperty_ = nullptr;
      double zoom_;
      QPointF offset_;
      bool dragging_ = false;
      QPointF* selectedHandle_ = nullptr;
  };
  ```

### Phase 3: キーフレーム操作

- 目的:
  - 効率的なキーフレーム編集

- 作業項目:
  - コピー＆ペースト
  - 複数選択
  - 簡略化

- 完了条件:
  - キーフレームのコピー/ペースト
  - 複数キーフレームの同時編集

### Phase 4: イージングプリセット

- 目的:
  - 簡単にイージングを適用

- 作業項目:
  - プリセットの追加
  - カスタムプリセットの保存
  - プリセット管理

- 完了条件:
  - ワンクリックでイージング適用
  - お気に入りプリセットを保存

### Phase 5: モーショングラフ

- 目的:
  - レイヤーの動きを視覚化

- 作業項目:
  - パスの表示
  - パスの編集
  - 3D モーションの可視化

- 完了条件:
  - モーションパスを編集
  - 3D 空間での動きを確認

---

## 技術的課題

### 1. グラフエディタのパフォーマンス

**課題:**
- 多数のキーフレームで重い

**解決案:**
- LOD（Level of Detail）
- 部分的な再描画
- キャッシュ

### 2. ベジェハンドルの操作性

**課題:**
- 細かい調整が困難

**解決案:**
- スナップ機能
- 数値入力
- 拡大・縮小

### 3. データ構造

**課題:**
- 複雑なイージングの表現

**解決案:**
- ベジェカーブベース
- イージング関数のサポート
- カスタムカーブ

---

## 期待される効果

### 制作効率

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **キーフレーム編集** | 手動 | 視覚的 | +200% |
| **イージング調整** | 数値入力 | ドラッグ | +300% |
| **アニメーション制作** | 時間 | 短時間 | +150% |

### ユーザー体験

- 直感的なアニメーション制作
- 複雑な動きも簡単に
- 視覚的なフィードバック

---

## 関連ドキュメント

- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_UNIFICATION_2026-03-25.md` - プロパティ/キーフレーム統一
- `docs/planned/MILESTONE_PROPERTY_KEYFRAME_EXECUTION_PLAN_2026-03-25.md` - 実行計画

---

## 実装順序の推奨

1. **Phase 1: キーフレームエディタ** - 基盤機能
2. **Phase 2: グラフエディタ** - 視覚的編集
3. **Phase 3: キーフレーム操作** - 効率化
4. **Phase 4: イージングプリセット** - 使いやすさ
5. **Phase 5: モーショングラフ** - 高度な機能

---

**文書終了**
