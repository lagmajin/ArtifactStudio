# F2 キー レイヤー名編集 実装状況

**調査日:** 2026-03-28  
**ステータス:** 実装済み  
**実装場所:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp:2243-2260`

---

## 実装内容

### 既存の実装

**ファイル:** `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp`

**行数:** 2243-2260

```cpp
if (impl_->layerNameEditable && event->key() == Qt::Key_F2 && !impl_->inlineNameEditor) {
   int selectedIdx = -1;
   for (int i = 0; i < impl_->visibleRows.size(); ++i) {
    if (impl_->visibleRows[i].layer && impl_->visibleRows[i].layer->id() == impl_->selectedLayerId) {
     selectedIdx = i;
     break;
    }
   }
   if (selectedIdx >= 0) {
    const int y = selectedIdx * kLayerRowHeight + kLayerRowHeight / 2;
    const int x = kLayerColumnWidth * kLayerPropertyColumnCount + 20;
    QMouseEvent fakeEvent(QEvent::MouseButtonDblClick, QPointF(x, y), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mouseDoubleClickEvent(&fakeEvent);
    event->accept();
    return;
   }
}
```

---

## 機能

### 動作

1. **F2 キーを押下**
2. 選択中のレイヤーを検出
3. ダブルクリックイベントを擬似発行
4. インラインエディタが表示される

### 条件

- `layerNameEditable` が true の場合のみ
- インラインエディタが未表示の場合のみ
- 選択中のレイヤーが存在する場合のみ

---

## 追加で発見された機能

### ✅ Delete キーでレイヤー削除

**行数:** 2127-2148

```cpp
if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
    // ... 複数選択の削除処理 ...
    if (!impl_->selectedLayerId.isNil()) {
        service->removeLayerFromComposition(compId, impl_->selectedLayerId);
        event->accept();
        return;
    }
}
```

**機能:**
- ✅ Delete/Backspace で削除
- ✅ 複数選択対応
- ✅ 確認ダイアログ（サービス側で実装）

---

### ✅ Alt+↑/↓ でレイヤー順序移動

**行数:** 2188-2206

```cpp
if ((event->modifiers() & Qt::AltModifier) &&
    !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier))) {
    if (event->key() == Qt::Key_Up) {
        if (moveSelectedLayerBy(-1)) {
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Down) {
        if (moveSelectedLayerBy(+1)) {
            event->accept();
            return;
        }
    }
}
```

**機能:**
- ✅ Alt+↑ で上に移動
- ✅ Alt+↓ で下に移動
- ✅ 複数選択対応

---

### ✅ Ctrl+[ / ] でレイヤー順序移動

**行数:** 2208-2218

```cpp
if (event->modifiers() & Qt::ControlModifier) {
    if (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_BracketRight) {
        if (moveSelectedLayerBy(event->key() == Qt::Key_BracketLeft ? -1 : +1)) {
            event->accept();
            return;
        }
    }
}
```

**機能:**
- ✅ Ctrl+[ で上に移動
- ✅ Ctrl+] で下に移動
- ✅ After Effects と同様のキーバインド

---

### ✅ 左右キーでフォルダ展開/折りたたみ

**行数:** 2220-2241

```cpp
if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
    // ... 選択行の検出 ...
    if (row.layer && row.hasChildren) {
        const bool current = impl_->expandedByLayerId.value(idStr, true);
        const bool next = (event->key() == Qt::Key_Right) ? true : false;
        if (current != next) {
            impl_->expandedByLayerId[idStr] = next;
            updateLayout();
        }
        event->accept();
        return;
    }
}
```

**機能:**
- ✅ → でフォルダ展開
- ✅ ← でフォルダ折りたたみ
- ✅ エクスプローラーと同様の操作感

---

### ✅ Escape キーで編集中キャンセル

**行数:** 2261-2266

```cpp
else if (event->key() == Qt::Key_Escape && impl_->inlineNameEditor) {
   impl_->clearInlineEditors();
   update();
   event->accept();
   return;
}
```

**機能:**
- ✅ Escape で編集キャンセル
- ✅ インラインエディタを閉じる

---

## 実装済みキーボードショートカット一覧

| ショートカット | 機能 | 実装 |
|--------------|------|------|
| **F2** | レイヤー名編集 | ✅ |
| **Delete** | レイヤー削除 | ✅ |
| **Alt+↑** | レイヤーを上に移動 | ✅ |
| **Alt+↓** | レイヤーを下に移動 | ✅ |
| **Ctrl+[** | レイヤーを上に移動 | ✅ |
| **Ctrl+]** | レイヤーを下に移動 | ✅ |
| **←** | フォルダ折りたたみ | ✅ |
| **→** | フォルダ展開 | ✅ |
| **Escape** | 編集キャンセル | ✅ |

---

## 未実装のショートカット

### ❌ Home キー - 最初のレイヤーへ選択

**提案:**
```cpp
if (event->key() == Qt::Key_Home && !impl_->visibleRows.isEmpty()) {
    impl_->selectedLayerId = impl_->visibleRows[0].layer->id();
    update();
    event->accept();
    return;
}
```

**工数:** 2-3 時間

---

### ❌ End キー - 最後のレイヤーへ選択

**提案:**
```cpp
if (event->key() == Qt::Key_End && !impl_->visibleRows.isEmpty()) {
    impl_->selectedLayerId = impl_->visibleRows.last().layer->id();
    update();
    event->accept();
    return;
}
```

**工数:** 2-3 時間

---

### ❌ Ctrl+A - 全選択

**提案:**
```cpp
if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier) {
    // 全レイヤーを選択
    for (const auto& row : impl_->visibleRows) {
        if (row.layer) {
            // 選択処理
        }
    }
    update();
    event->accept();
    return;
}
```

**工数:** 4-6 時間

---

### ❌ Ctrl+D - レイヤー複製

**提案:**
```cpp
if (event->key() == Qt::Key_D && event->modifiers() & Qt::ControlModifier) {
    if (!impl_->selectedLayerId.isNil()) {
        auto* svc = ArtifactProjectService::instance();
        svc->duplicateLayerInCurrentComposition(impl_->selectedLayerId);
        event->accept();
        return;
    }
}
```

**工数:** 4-6 時間

---

## 結論

**F2 キー機能は既に実装済みでした。**

さらに、以下の機能も実装済み：
- ✅ Delete キーで削除
- ✅ Alt+↑/↓ で移動
- ✅ Ctrl+[ / ] で移動
- ✅ 左右キーで展開/折りたたみ
- ✅ Escape でキャンセル

**未実装:**
- ❌ Home キー
- ❌ End キー
- ❌ Ctrl+A（全選択）
- ❌ Ctrl+D（複製）

---

**文書終了**
