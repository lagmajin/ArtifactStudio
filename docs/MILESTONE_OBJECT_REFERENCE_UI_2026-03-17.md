# M13 Object Reference UI (2026-03-17)

日付：2026-03-17  
目標：Unity 風オブジェクト参照選択 UI を実装する

---

## Goal

- Inspector でオブジェクト参照を UI から選択可能に
- ドラッグ＆ドロップでの参照設定
- オブジェクトピッカーダイアログ

---

## Definition of Done

- [ ] **PropertyType::ObjectReference** 追加
- [ ] **ObjectReferenceWidget** 実装（○ボタン + 名表示）
- [ ] **ObjectPickerDialog** 実装
- [ ] ドラッグ＆ドロップ対応
- [ ] 参照クリア機能

---

## Design Concept

### Unity Inspector を参考にした理由

1. **直感的** - ○ボタンでピッカー開く
2. **統一感** - 全ての参照タイプで同じ UI
3. **柔軟** - ドラッグ＆ドロップ両対応
4. **検証容易** - 設定値が一目で確認

---

## Architecture

### UI 構成

```
┌─────────────────────────────────────────────┐
│ Target Object                               │
│ ┌─────────────────────────────────────┐     │
│ │  ○ Main Camera                  │   │     │
│ └─────────────────────────────────────┘     │
│                                             │
│  ○ = クリックでオブジェクトピッカー         │
│  ドラッグ＆ドロップも可能                   │
└─────────────────────────────────────────────┘
```

### データフロー

```
User Action
    │
    ├─→ ○ Button Click ─→ ObjectPickerDialog ─→ Selection
    │                                              │
    └─→ Drag & Drop ───────────────────────────────┘
                                                   │
                                                   ▼
                                    ObjectReferenceWidget
                                                   │
                                                   ▼
                                    Property System Update
```

---

## Property System Extension

### PropertyType 拡張

**既存：**
```cpp
enum class PropertyType {
    Float,
    Integer,
    Boolean,
    Color,
    String
};
```

**追加後：**
```cpp
enum class PropertyType {
    Float,
    Integer,
    Boolean,
    Color,
    String,
    ObjectReference  // ← 新規
};
```

### PropertyMetadata 拡張

```cpp
struct PropertyMetadata {
    // ... 既存フィールド ...
    
    // 参照タイプ用
    QString referenceTypeName;      // 参照可能タイプ名
    bool allowNull = true;          // null 許可
};
```

### 使用例

```cpp
// プロパティ定義
auto* prop = new Property();
prop->setName("targetObject");
prop->setType(PropertyType::ObjectReference);
prop->setMetadata({
    .displayLabel = "Target Object",
    .referenceTypeName = "ArtifactAbstractLayer",
    .allowNull = true
});
prop->setDefaultValue(QVariant::fromValue<QObject*>(nullptr));
```

---

## UI Components

### ObjectReferenceWidget

**役割**: 単一オブジェクト参照の表示・編集

```cpp
class ArtifactObjectReferenceWidget : public QWidget {
    Q_OBJECT
public:
    explicit ArtifactObjectReferenceWidget(QWidget* parent = nullptr);
    ~ArtifactObjectReferenceWidget();
    
    // 設定
    void setReferenceType(const QString& typeName);
    void setCurrentReference(QObject* obj);
    void setAllowNull(bool allow);
    
    // 取得
    QObject* currentReference() const;
    QString referenceType() const;
    bool allowNull() const;

signals:
    void referenceChanged(QObject* newRef);
    void referenceCleared();

private slots:
    void onPickButtonClicked();
    void onNameEditChanged();
    void onDropEvent(QObject* droppedObj);

private:
    void updateDisplay();
    void showObjectPicker();
    
    QLineEdit* nameEdit_;        // 参照名表示（編集可能）
    QPushButton* pickButton_;    // ○ ピッカーボタン
    QPushButton* clearButton_;   // × クリアボタン
    QString referenceType_;      // 参照可能タイプ
    QObject* currentRef_ = nullptr;
    bool allowNull_ = true;
};
```

**UI 構造：**
```
┌──────────────────────────────────────────────────────┐
│ [○] [Target Object Name______________________] [×]   │
│  │                │                                  │
│  │                └─ 参照名表示（編集可能）           │
│  │                                                   │
│  └─ ピッカーボタン                                   │
│                                                      │
│                              [×] = クリアボタン       │
└──────────────────────────────────────────────────────┘
```

---

### ObjectPickerDialog

**役割**: オブジェクト選択ダイアログ

```cpp
class ArtifactObjectPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ArtifactObjectPickerDialog(QWidget* parent = nullptr);
    ~ArtifactObjectPickerDialog();
    
    // 設定
    void setReferenceType(const QString& typeName);
    void setCurrentSelection(QObject* obj);
    
    // 結果取得
    QObject* selectedObject() const;

private slots:
    void onObjectDoubleClicked(QObject* obj);
    void onSearchTextChanged(const QString& text);
    void onOkClicked();
    void onCancelClicked();

private:
    void buildObjectTree();
    void filterObjectTree(const QString& filter);
    
    QTreeWidget* objectTree_;      // オブジェクトツリー
    QLineEdit* searchEdit_;        // 検索フィルター
    QDialogButtonBox* buttonBox_;  // OK/Cancel
    QString referenceType_;
    QObject* currentSelection_ = nullptr;
};
```

**UI 構造：**
```
┌─────────────────────────────────────────┐
│ Select Object                           │
├─────────────────────────────────────────┤
│ Filter: [________________]              │
│                                         │
│ ┌─────────────────────────────────────┐ │
│ │ 📁 Compositions                     │ │
│ │   ├─ Comp 1                         │ │
│ │   ├─ Comp 2                         │ │
│ │   └─ Comp 3                         │ │
│ │ 📁 Layers                           │ │
│ │   ├─ Solid 1                        │ │
│ │   ├─ Image 1                        │ │
│ │   └─ Text 1                         │ │
│ └─────────────────────────────────────┘ │
│                                         │
│              [OK] [Cancel]              │
└─────────────────────────────────────────┘
```

---

## Integration

### PropertyEditor 統合

```cpp
// PropertyEditorWidget の一部として
void PropertyEditorWidget::createObjectReferenceEditor(Property* prop) {
    auto* refWidget = new ArtifactObjectReferenceWidget(this);
    refWidget->setReferenceType(prop->metadata().referenceTypeName);
    refWidget->setAllowNull(prop->metadata().allowNull);
    
    // 現在値設定
    QObject* currentObj = prop->value().value<QObject*>();
    refWidget->setCurrentReference(currentObj);
    
    // 変更検知
    connect(refWidget, &ArtifactObjectReferenceWidget::referenceChanged,
            this, [this, prop](QObject* newRef) {
        prop->setValue(QVariant::fromValue(newRef));
        emit propertyChanged(prop);
    });
    
    layout_->addRow(prop->metadata().displayLabel, refWidget);
}
```

### ドラッグ＆ドロップ対応

```cpp
// ObjectReferenceWidget のドラッグ受付
void ArtifactObjectReferenceWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-artifact-object-id")) {
        event->acceptProposedAction();
    }
}

void ArtifactObjectReferenceWidget::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    if (mime->hasFormat("application/x-artifact-object-id")) {
        QByteArray data = mime->data("application/x-artifact-object-id");
        // ID からオブジェクトを検索して設定
        QObject* droppedObj = findObjectById(data);
        if (droppedObj) {
            setCurrentReference(droppedObj);
            emit referenceChanged(droppedObj);
        }
    }
}
```

---

## Milestones

### M-OBJREF-1: PropertyType Extension

**目標**: PropertyType に ObjectReference を追加

**完了条件**:
- [ ] `PropertyType::ObjectReference` 列挙値追加
- [ ] `PropertyMetadata` に参照タイプ情報追加
- [ ] シリアライズ対応（JSON 保存）

**見積**: 2-3h

---

### M-OBJREF-2: ObjectReferenceWidget

**目標**: 参照選択ウィジェット実装

**完了条件**:
- [ ] ○ボタン表示
- [ ] 参照名表示（編集可能）
- [ ] クリアボタン
- [ ] ドラッグ＆ドロップ受付
- [ ] シグナル実装

**UI 実装**:
```
[○] [______________________] [×]
```

**見積**: 4-6h

---

### M-OBJREF-3: ObjectPickerDialog

**目標**: オブジェクト選択ダイアログ実装

**完了条件**:
- [ ] ツリー表示（Composition/Layer 階層）
- [ ] 検索フィルター
- [ ] 現在選択中のハイライト
- [ ] ダブルクリックで選択確定
- [ ] OK/Cancel ボタン

**見積**: 4-6h

---

### M-OBJREF-4: PropertyEditor Integration

**目標**: PropertyEditor に統合

**完了条件**:
- [ ] ObjectReference タイプの編集ウィジェット表示
- [ ] 値の同期（Property ↔ Widget）
- [ ] 変更検知シグナル
- [ ] 既存プロパティとの併用

**見積**: 3-4h

---

### M-OBJREF-5: Drag & Drop Support

**目標**: ドラッグ＆ドロップ対応

**完了条件**:
- [ ] Timeline からのドラッグ受付
- [ ] Project View からのドラッグ受付
- [ ] MIME データ形式定義
- [ ] ドロップ時のタイプ検証
- [ ] ハイライト表示（ドラッグ中）

**見積**: 3-4h

---

## Total Estimate

| フェーズ | 見積時間 |
| --- | --- |
| M-OBJREF-1 | 2-3h |
| M-OBJREF-2 | 4-6h |
| M-OBJREF-3 | 4-6h |
| M-OBJREF-4 | 3-4h |
| M-OBJREF-5 | 3-4h |
| **合計** | **16-23h** |

---

## Dependencies

### 既存システム
- `PropertyType` - プロパティタイプ列挙
- `PropertyGroup` - プロパティグループ管理
- `PropertyEditorWidget` - プロパティ編集 UI
- `ArtifactProjectService` - オブジェクト検索

### 新規実装
- `ArtifactObjectReferenceWidget` - 参照選択ウィジェット
- `ArtifactObjectPickerDialog` - オブジェクトピッカー
- MIME データ形式（ドラッグ＆ドロップ用）

---

## Implementation Notes

### MIME データ形式

```cpp
// ドラッグ開始（Timeline 側）
void TimelineLayerWidget::startDrag() {
    QDrag* drag = new QDrag(this);
    QMimeData* mime = new QMimeData;
    
    // オブジェクト ID を MIME データに設定
    mime->setData("application/x-artifact-object-id",
                  QByteArray::number(currentLayerId_.toLongLong()));
    
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

// ドロップ受付（ObjectReferenceWidget 側）
void ObjectReferenceWidget::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    if (mime->hasFormat("application/x-artifact-object-id")) {
        // ID からオブジェクトを検索
        qlonglong id = mime->data("application/x-artifact-object-id").toLongLong();
        QObject* obj = findObjectById(id);
        if (obj) {
            setCurrentReference(obj);
        }
    }
}
```

### オブジェクト検索

```cpp
// ProjectService から検索
QObject* findObjectById(qlonglong id) {
    auto* service = ArtifactProjectService::instance();
    if (!service) return nullptr;
    
    // Layer ID で検索
    LayerID layerId(id);
    auto comp = service->currentComposition().lock();
    if (comp) {
        auto layer = comp->layerById(layerId);
        if (layer) {
            return layer.get();
        }
    }
    
    // Composition ID で検索
    CompositionID compId(id);
    auto found = service->findComposition(compId);
    if (found.success) {
        return found.ptr.lock().get();
    }
    
    return nullptr;
}
```

### シリアライズ

```cpp
// JSON シリアライズ
QJsonObject Property::toJson() const {
    QJsonObject obj;
    obj["name"] = name_;
    obj["type"] = static_cast<int>(type_);
    
    if (type_ == PropertyType::ObjectReference) {
        QObject* refObj = value_.value<QObject*>();
        if (refObj) {
            // オブジェクト ID を保存
            obj["value"] = refObj->property("id").toLongLong();
            obj["referenceType"] = metadata_.referenceTypeName;
        } else {
            obj["value"] = QJsonValue::Null;
        }
    } else {
        // 他のタイプは従来通り
        obj["value"] = variantToJson(value_);
    }
    
    return obj;
}

// JSON デシリアライズ
void Property::fromJson(const QJsonObject& obj) {
    name_ = obj["name"].toString();
    type_ = static_cast<PropertyType>(obj["type"].toInt());
    
    if (type_ == PropertyType::ObjectReference) {
        qlonglong refId = obj["value"].toVariant().toLongLong();
        // 起動時に ID からオブジェクトを再解決
        pendingReferenceId_ = refId;  // 後で解決
    } else {
        value_ = jsonToVariant(obj["value"]);
    }
}
```

---

## Future Extensions

### 未実装（将来）

- **複数参照対応** - 配列参照（N 対 N）
- **タイプ継承対応** - 派生クラスも受け入れ
- **フィルタ関数** - カスタム検証コールバック
- **お気に入り登録** - よく使うオブジェクト
- **履歴機能** - 最近使用した参照
- **プレビュー表示** - サムネイル/プロパティ一部表示
- **ショートカット** - 右クリックメニュー

---

## First Step

**M-OBJREF-1** から着手。

最初にやること：
1. `PropertyType::ObjectReference` 追加
2. `PropertyMetadata` 拡張
3. テスト用プロパティ定義
