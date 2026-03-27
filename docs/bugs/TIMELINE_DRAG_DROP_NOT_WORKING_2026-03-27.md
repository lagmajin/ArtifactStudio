# レイヤーパネル ドラッグ＆ドロップ非機能バグ仮説 (2026-03-27)

## 症状

タイムライン左ペイン（レイヤーパネル）でレイヤーの順番をドラッグ＆ドロップで変更できない。

## 仮説

### ★★★ 1. setDragDropMode がヘッダーにセットされている（最も根本的）

**場所:** `ArtifactLayerHierarchyWidget.cppm:96`

```cpp
// ArtifactLayerHierarchyHeaderView のコンストラクタ内:
setDragDropMode(QAbstractItemView::InternalMove);  // ← ヘッダーにセット！
setDragEnabled(true);                                // ← ヘッダーにセット！
```

`setDragDropMode` と `setDragEnabled` が **QTreeView ではなく QHeaderView にセット**されている。

実際のレイヤーリスト (`ArtifactLayerHierarchyView`, QTreeView) のコンストラクタ (line 134-148):
```cpp
ArtifactLayerHierarchyView::ArtifactLayerHierarchyView(QWidget* parent)
 : QTreeView(parent), impl_(new Impl())
{
 auto model = new ArtifactHierarchyModel();
 setModel(model);
 setHeader(new ArtifactLayerHierarchyHeaderView);
 header()->setSectionResizeMode(0, QHeaderView::Fixed);
 header()->resizeSection(0, 20);
 header()->setStretchLastSection(false);
 setRootIsDecorated(false);
 // ← setDragEnabled なし
 // ← setDragDropMode なし
 // ← setAcceptDrops なし
}
```

→ ヘッダー列のリオーダーは可能かもしれないが、レイヤー行のドラッグ＆ドロップは**不可能**。

### ★★★ 2. モデルがドラッグ＆ドロップ API を実装していない

**場所:** `ArtifactHierarchyModel.ixx` / `ArtifactHierarchyModel.cppm`

`ArtifactHierarchyModel` に**以下のメソッドが一切存在しない**:

| 必須メソッド | 状態 |
|---|---|
| `flags()` | 未オーバーライド → `ItemIsEnabled | ItemIsSelectable` のみ。`ItemIsDragEnabled` / `ItemIsDropEnabled` がない |
| `mimeTypes()` | 未実装 |
| `mimeData()` | 未実装 |
| `dropMimeData()` | 未実装 |
| `supportedDropActions()` | 未実装 → デフォルト `Qt::IgnoreAction` |

→ Qt のモデル/ビュー フレームワークでドラッグ＆ドロップを有効にするには、
モデル側でこれら5つのメソッドをオーバーライドする必要がある。

### ★★ 3. モデルがスタブ状態でレイヤーデータを表示しない

**場所:** `ArtifactHierarchyModel.cppm:76-115`

```cpp
int ArtifactHierarchyModel::rowCount(const QModelIndex& parent) const {
 if (parent.isValid()) return 0;  // 子なし
 return 1;                        // ルートに1行だけ
}

QVariant ArtifactHierarchyModel::data(const QModelIndex& index, int role) const {
 if (role == Qt::BackgroundRole) {
  if (index.row() % 2 == 0) return QColor(30, 30, 30);
  else return QColor(45, 45, 45);
 }
 return QVariant();  // DisplayRole は "Test" を返すが、BackgroundRole の中の if 内にあるため到達不能
}
```

- `rowCount()` が固定で 1 を返す → 1行しか表示されない
- `data()` の `DisplayRole` ブロックが `BackgroundRole` の if 内にあるため到達不能
- `index()` が `createIndex(row, column, nullptr)` で `internalPointer` が nullptr
- `parent()` が常に `QModelIndex()` を返す → ツリー構造なし
- コンポジションのレイヤーリストに接続されていない

### ★ 4. ドロップ操作完了後にレイヤー順序変更の通知がない

`dropMimeData()` が仮に実装されたとしても、ドロップ後にコンポジションのレイヤー順序を変更し、
`ArtifactComposition::setLayerOrder()` や同等の API を呼び出す必要がある。
現在はその接続が一切ない。

---

## 対策案

### 対策1: QTreeView に setDragDropMode を移動

```cpp
// ArtifactLayerHierarchyView コンストラクタに追加:
setDragEnabled(true);
setDragDropMode(QAbstractItemView::InternalMove);
setAcceptDrops(true);
setDefaultDropAction(Qt::MoveAction);
```

### 対策2: モデルにドラッグ＆ドロップ API を実装

```cpp
// ArtifactHierarchyModel に追加:
Qt::ItemFlags flags(const QModelIndex& index) const override {
    auto f = QAbstractItemModel::flags(index);
    if (index.isValid()) {
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }
    return f;
}

QStringList mimeTypes() const override { return {"application/x-layer-id"}; }
QMimeData* mimeData(const QModelIndexList& indices) const override { ... }
bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override { ... }
Qt::DropActions supportedDropActions() const override { return Qt::MoveAction; }
```

### 対策3: モデルをコンポジションに接続

モデルの `rowCount()` をコンポジションのレイヤー数に、
`data()` をレイヤー名にマッピングする。

---

## 関連ファイル

| ファイル | 行 | 内容 |
|---|---|---|
| `Artifact/src/Widgets/Timeline/ArtifactLayerHierarchyWidget.cppm` | 91-108 | QHeaderView に setDragDropMode が誤ってセット |
| `Artifact/src/Widgets/Timeline/ArtifactLayerHierarchyWidget.cppm` | 134-148 | QTreeView にドラッグ＆ドロップ設定なし |
| `Artifact/src/Layer/ArtifactHierarchyModel.cppm` | 51-121 | モデルがスタブ状態 |
| `Artifact/include/Layer/ArtifactHierarchyModel.ixx` | 48-66 | flags/mimeData/dropMimeData 未定義 |
