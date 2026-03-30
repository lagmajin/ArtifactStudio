# Blender 風ショートカットシステム 調査レポート

**作成日:** 2026-03-28  
**ステータス:** 調査完了  
**関連コンポーネント:** InputOperator, ActionManager, KeyMap, InputBinding

---

## 概要

**Blender 風の柔軟なショートカットシステムは既に実装済み**でした。

`ArtifactCore::InputOperator` システムが、Blender の operator/keymap システムと非常に似たアーキテクチャを持っています。

---

## 既存システム構成

### コアクラス

| クラス | 役割 | Blender 相当 |
|--------|------|-------------|
| **InputOperator** | 入力処理の総括 | `wmWindowManager` |
| **ActionManager** | アクション管理 | `wmOperator` |
| **KeyMap** | キーマップ | `wmKeyMap` |
| **InputBinding** | キーバインド | `wmKeyMapItem` |
| **Action** | 操作定義 | `wmOperatorType` |

---

## 実装済み機能

### ✅ InputBinding（キーバインド）

```cpp
class InputBinding : public QObject {
    // 基本情報
    QString id_;
    QString name_;
    QString description_;
    
    // キー設定
    int keyCode_;
    InputEvent::Modifiers modifiers_;
    InputEvent::Modifiers requiredModifiers_;  // 必須修飾キー
    InputEvent::Modifiers forbiddenModifiers_; // 禁止修飾キー
    
    // 多段キー（コード）
    bool isChord_;          // G→R のような多段入力
    int firstKey_;
    std::vector<int> chordKeys_;
    
    // コンテキスト
    QString context_;       // "Timeline", "NodeGraph", "3DView"
    int priority_;
    
    // コールバック
    std::function<void()> callback_;
    QString actionId_;
};
```

**特徴:**
- ✅ 単一キー（Space）
- ✅ 修飾キー組み合わせ（Ctrl+S, Shift+Ctrl+Z）
- ✅ 多段キー（G→R で Grab→Rotate）
- ✅ コンテキスト依存（"Timeline" でのみ有効）
- ✅ 優先度設定

---

### ✅ ActionManager（アクション管理）

```cpp
class ActionManager : public QObject {
    // アクション登録
    Action* registerAction(const QString& id,
                          const QString& name,
                          const QString& description,
                          const QString& category);
    
    // アクション取得
    Action* getAction(const QString& id);
    std::vector<Action*> getActionsByCategory();
    
    // アクション実行
    void executeAction(const QString& id, const QVariantMap& params);
    
    // コールバック付き作成
    Action* createAction(const QString& id,
                        const QString& name,
                        const QString& description,
                        std::function<void()> callback);
};
```

**特徴:**
- ✅ アクション ID での登録
- ✅ カテゴリ分類
- ✅ パラメータ付き実行
- ✅ コールバック登録

---

### ✅ KeyMap（キーマップ）

```cpp
class KeyMap : public QObject {
    // 名前・コンテキスト
    QString name();
    QString context();
    
    // バインディング追加
    InputBinding* addBinding(int key,
                            InputEvent::Modifiers modifiers,
                            Action* action,
                            const QString& description);
    
    // 文字列から追加（"Ctrl+S"）
    InputBinding* addBinding(const QString& keySequence,
                            Action* action,
                            const QString& description);
    
    // 削除
    void removeBinding(InputBinding* binding);
    void removeBinding(const QString& actionId);
    
    // 検索
    InputBinding* findBinding(int key, InputEvent::Modifiers mods);
    std::vector<InputBinding*> findBindingsForAction(const QString& actionId);
    
    // 入出力（ユーザーカスタマイズ用）
    QString toJSON();
    bool fromJSON(const QString& json);
};
```

**特徴:**
- ✅ コンテキスト別キーマップ
- ✅ 文字列からのバインディング（"Ctrl+S"）
- ✅ JSON 入出力（設定保存用）
- ✅ アクション逆引き

---

### ✅ InputOperator（入力処理）

```cpp
class InputOperator : public QObject {
    // キーマップ管理
    KeyMap* addKeyMap(const QString& name, const QString& context);
    KeyMap* getKeyMap(const QString& name);
    
    // アクティブコンテキスト
    QString activeContext();
    void setActiveContext(const QString& ctx);
    
    // 入力処理
    bool processKeyEvent(const InputEvent& event);
    bool processKeyPress(int key, InputEvent::Modifiers modifiers);
    
    // マルチモーダル操作
    void startInteractiveAction(InteractiveAction* action, const InputEvent& event);
    InteractiveAction* activeInteractiveAction();
    
    // 多段キー
    bool isInChord();
    void cancelChord();
    
    // デバッグ
    QString dumpKeyMaps();
};
```

**特徴:**
- ✅ 複数キーマップの切り替え
- ✅ コンテキスト依存処理
- ✅ インタラクティブ操作（Grab/Move/Scale）
- ✅ 多段キー処理
- ✅ デバッグ出力

---

## Blender との比較

| 機能 | Blender | Artifact | 状態 |
|------|---------|----------|------|
| **アクション登録** | `wmOperatorType` | `Action` | ✅ |
| **キーマップ** | `wmKeyMap` | `KeyMap` | ✅ |
| **キーバインド** | `wmKeyMapItem` | `InputBinding` | ✅ |
| **コンテキスト** | Context | `context_` | ✅ |
| **多段キー** | Key Chord | `isChord_` | ✅ |
| **優先度** | Priority | `priority_` | ✅ |
| **修飾キー制御** | Modifier | `required/forbidden` | ✅ |
| **JSON 入出力** | User Preferences | `toJSON/fromJSON` | ✅ |
| **インタラクティブ** | Modal Operator | `InteractiveAction` | ✅ |

**結論:** Blender と同等の機能を網羅 ✅

---

## 実装例

### 1. アクション登録

```cpp
// ActionManager を使用
auto* actionManager = ActionManager::instance();

// アクションを登録
auto* deleteAction = actionManager->registerAction(
    "artifact.delete",
    "Delete",
    "Delete selected items",
    "Edit"
);

// コールバックを設定
deleteAction->setExecuteCallback([](const QVariantMap& params) {
    // 削除処理
    qDebug() << "Deleting items...";
});

// ショートカットテキスト（表示用）
deleteAction->setShortcutText("Del");
```

### 2. キーマップ設定

```cpp
// InputOperator を使用
auto* inputOp = InputOperator::instance();

// タイムライン用キーマップを作成
auto* timelineKeyMap = inputOp->addKeyMap("Timeline", "Timeline");

// アクションをキーにバインド
timelineKeyMap->addBinding(
    Qt::Key_Delete,           // キー
    InputEvent::Modifiers(),  // 修飾キーなし
    deleteAction,             // アクション
    "Delete selected layers"  // 説明
);

// 文字列からバインド（Ctrl+D）
auto* duplicateAction = actionManager->getAction("artifact.duplicate");
timelineKeyMap->addBinding(
    "Ctrl+D",
    duplicateAction,
    "Duplicate selected layers"
);
```

### 3. コンテキスト切り替え

```cpp
// タイムラインにフォーカス時
inputOp->setActiveContext("Timeline");

// コンポジションビューにフォーカス時
inputOp->setActiveContext("CompositionView");

// グローバル（常に有効）
auto* globalKeyMap = inputOp->addKeyMap("Global", "Global");
```

### 4. 多段キー（コード）

```cpp
// G→R で Grab→Rotate
auto* grabAction = actionManager->getAction("artifact.grab");
auto* binding = timelineKeyMap->addBinding(
    Qt::Key_G,
    InputEvent::Modifiers(),
    grabAction,
    "Grab/Move"
);

// 多段キー設定
binding->setIsChord(true);
binding->setFirstKey(Qt::Key_G);
binding->chordKeys_ = { Qt::Key_R };  // G→R
```

### 5. 修飾キー制御

```cpp
auto* binding = timelineKeyMap->addBinding(
    Qt::Key_A,
    InputEvent::ModifierKey::LCtrl,  // Ctrl 必須
    selectAllAction,
    "Select All"
);

// Ctrl 必須、Alt は禁止
binding->setRequiredModifiers(InputEvent::ModifierKey::LCtrl);
binding->setForbiddenModifiers(InputEvent::ModifierKey::LAlt);
```

### 6. JSON 入出力（設定保存）

```cpp
// キーマップを JSON にエクスポート
QString json = timelineKeyMap->toJSON();

// ファイルに保存
QFile file("timeline_keymap.json");
file.write(json.toUtf8());

// 後からインポート
QString loadedJson = file.readAll();
timelineKeyMap->fromJSON(loadedJson);
```

---

## 既存の使用例

### StandardActions.ixx

```cpp
// アクション登録とショートカット設定
auto* am = ActionManager::instance();

// インポート
auto* import = am->registerAction("artifact.import", "Import");
import->setShortcutText("Ctrl+I");

// 新規コンポジション
auto* newComp = am->registerAction("artifact.comp.new", "New Composition");
newComp->setShortcutText("Ctrl+N");

// 削除
auto* del = am->registerAction("artifact.delete", "Delete");
del->setShortcutText("Del");

// 名前変更
auto* rename = am->registerAction("artifact.rename", "Rename");
rename->setShortcutText("F2");

// 複製
auto* duplicate = am->registerAction("artifact.duplicate", "Duplicate");
duplicate->setShortcutText("Ctrl+D");
```

---

## 現在の課題

### ❌ 1. ウィジェットごとの柔軟な設定

**現状:**
- キーマップはグローバル
- ウィジェット固有の設定がない

**解決案:**
```cpp
// ウィジェット固有キーマップ
auto* timelineKeyMap = inputOp->addKeyMap(
    "Timeline", 
    "Timeline"  // コンテキスト
);

// ウィジェットインスタンスを保存
timelineKeyMap->setProperty("widget", QVariant::fromValue(timelineWidget));
```

---

### ❌ 2. ユーザーカスタマイズ UI

**現状:**
- JSON 入出力はあるが UI がない

**解決案:**
```cpp
class ShortcutConfigDialog : public QDialog {
    QTreeWidget* categoryTree;
    QKeySequenceEdit* keyEdit;
    QPushButton* resetButton;
    
    // キーマップをツリー表示
    for (auto* km : inputOp->allKeyMaps()) {
        for (auto* binding : km->allBindings()) {
            tree->addItem(binding->name(), binding->toString());
        }
    }
    
    // 編集
    connect(keyEdit, &QKeySequenceEdit::keySequenceChanged,
            [binding](const QKeySequence& seq) {
        binding->setKeyCode(seq[0].key());
        binding->setModifiers(seq[0].modifiers());
    });
};
```

---

### ❌ 3. コンフリクト検出

**現状:**
- 重複チェックがない

**解決案:**
```cpp
bool KeyMap::addBinding(...) {
    // 重複チェック
    auto* existing = findBinding(key, modifiers);
    if (existing) {
        qWarning() << "Conflict:" << existing->actionId() 
                   << "vs" << action->id();
        return nullptr;
    }
    // ...
}
```

---

## 将来の拡張案

### Phase 1: ウィジェット別キーマップ（4-6h）

```cpp
// ウィジェットが独自のキーマップを登録
void ArtifactLayerPanelWidget::initialize() {
    auto* inputOp = InputOperator::instance();
    
    // このウィジェット用のキーマップ
    keyMap_ = inputOp->addKeyMap("LayerPanel", "LayerPanel");
    
    // ローカルアクション
    auto* deleteAction = keyMap_->addBinding(
        Qt::Key_Delete,
        new Action("layerpanel.delete", "Delete Layer"),
        "Delete selected layer"
    );
}

// フォーカス時にコンテキスト切り替え
void ArtifactLayerPanelWidget::focusInEvent(QFocusEvent* event) {
    InputOperator::instance()->setActiveContext("LayerPanel");
}
```

---

### Phase 2: ショートカット設定 UI（12-16h）

```cpp
class ShortcutConfigWidget : public QWidget {
    // カテゴリ別表示
    QTreeWidget* tree;
    
    // 検索
    QLineEdit* searchBox;
    
    // 編集
    QKeySequenceEdit* keyEdit;
    QComboBox* contextCombo;
    
    // リセット
    QPushButton* resetButton;
    
    // プリセット
    QComboBox* presetCombo;  // "Blender", "After Effects", "Default"
};
```

---

### Phase 3: プリセットシステム（8-12h）

```cpp
class ShortcutPreset {
    QString name;  // "Blender", "After Effects", "Maya"
    QString description;
    QMap<QString, QString> bindings;  // actionId -> keySequence
};

// プリセット読み込み
void ShortcutPresetManager::loadPreset(const QString& name) {
    auto* inputOp = InputOperator::instance();
    
    // 全キーマップをクリア
    for (auto* km : inputOp->allKeyMaps()) {
        km->clear();
    }
    
    // プリセットを適用
    auto preset = loadPresetFile(name + ".json");
    applyPreset(preset);
}

// プリセット保存
void ShortcutPresetManager::savePreset(const QString& name) {
    auto preset = createPresetFromCurrent();
    savePresetFile(name + ".json", preset);
}
```

---

### Phase 4: マクロ・多段キー強化（12-16h）

```cpp
// マクロ登録
auto* macro = new InteractiveAction();
macro->setSequence({
    { Qt::Key_G, InputEvent::Modifiers() },  // G
    { Qt::Key_R, InputEvent::Modifiers() },  // R
    { Qt::Key_S, InputEvent::Modifiers() },  // S
});

// G→R→S で Grab→Rotate→Scale
inputOp->addKeyMap("Macro", "Global")->addBinding(
    macro,
    "GrabRotateScale"
);
```

---

## 実装推奨順序

### 第 1 段階：基盤整備（4-6h）

1. **ウィジェット別キーマップ**
   - コンテキスト切り替えの自動化
   - フォーカス連動

### 第 2 段階：UI 実装（12-16h）

2. **ショートカット設定ダイアログ**
   - カテゴリ別表示
   - 検索機能
   - 編集機能

### 第 3 段階：機能拡充（20-28h）

3. **プリセットシステム**
   - Blender/After Effects/Maya プリセット
   - エクスポート/インポート

4. **コンフリクト検出**
   - 重複チェック
   - 警告表示

---

## 結論

**Blender 風ショートカットシステムは既に実装済み**です。

現在のシステムは非常に良く設計されており、Blender の operator/keymap システムと同等の機能を網羅しています。

**次に注力すべきは:**
1. ウィジェット別キーマップの整備
2. ユーザーカスタマイズ UI
3. プリセットシステム

これらを実装することで、Blender と同等の柔軟なショートカットシステムが完成します。

---

## 関連ドキュメント

- `ArtifactCore/include/UI/InputOperator.ixx` - コア定義
- `ArtifactCore/src/UI/InputOperator.cppm` - 実装
- `ArtifactCore/include/UI/StandardActions.ixx` - 標準アクション
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md` - メニュー統合

---

**文書終了**
