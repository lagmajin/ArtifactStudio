# Milestone: Viewport Bookmarks System (M-VP-8)

**マイルストーンID**: M-VP-8
**作成日**: 2026-06-27
**優先度**: Low
**推定工数**: 2-3日
**カテゴリ**: Composition Editor / Viewport / UX
**状態**: Planned

---

## 目的

よく使うビューポート状態（ズーム・パン・回転）をブックマークとして保存・復元できる機能を実装する。ユーザーは頻繁に使用する視点を素早く切り替えることができ、作業効率を大幅に向上させる。

---

## 背景

### 現状
- 既に`ViewportBookmarkStore`クラスの概念は存在（`Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`に設計あり）
- ビューポート状態の保存/復元機能は**未実装**
- 実装が大変とのこと（ユーザー指摘）
- 類似機能として、After Effectsの「ビューポートのズーム位置を保存」がある

### 要件
- ビューポート状態（ズーム、パン、回転、解像度）をブックマークとして保存
- 1-9のショートカットキーで素早く切り替え
- ブックマークの名前付けと整理
- プロジェクトごとの保存
- 現在の状態を新しいブックマークとして保存
- ブックマークの削除と並べ替え

### ユースケース
1. 複数の作業領域を素早く切り替え（例: 全体像→詳細部→別の詳細部）
2. قالメーションシーンのキーフレーム位置を確認
3. クライアントレビュー用の特定視点を保存
4. チュートリアルやドキュメント作成用の視点管理
5. 3Dシーンの複数のカメラアングルを管理

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `ArtifactCore/include/Viewport/ViewportBookmark.ixx` | ブックマークデータ構造 |
| **新規** | `ArtifactCore/src/Viewport/ViewportBookmark.cppm` | 実装（任意） |
| **新規** | `ArtifactCore/include/Viewport/ViewportBookmarkManager.ixx` | ブックマーク管理クラス |
| **新規** | `ArtifactCore/src/Viewport/ViewportBookmarkManager.cppm` | 実装 |
| **新規** | `ArtifactCore/include/Event/ViewportBookmarkChangedEvent.ixx` | イベント |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | 状態取得/復元 |
| **変更** | `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` | ショートカット処理 |
| **変更** | `Artifact/src/Widgets/ArtifactViewMenu.cppm` | ブックマークメニュー |
| **新規** | `Artifact/src/Widgets/ViewportBookmarkDialog.cppm` | ブックマーク管理ダイアログ |
| **新規** | `Artifact/include/Widgets/ViewportBookmarkDialog.ixx` | ヘッダー |

---

## 変更詳細

### 1. ViewportBookmark データ構造

**新規ファイル**: `ArtifactCore/include/Viewport/ViewportBookmark.ixx`

```cpp
module;
#include <string>
#include <vector>

export module Viewport.Bookmark;

import Core.Scale.Zoom;

export namespace ArtifactCore {

/**
 * @brief ビューポート状態を保存するデータ構造
 */
export struct ViewportState {
    float zoom = 1.0f;           // ズーム倍率
    float panX = 0.0f;          // パンX
    float panY = 0.0f;          // パンY
    float rotation = 0.0f;      // 回転角度（度）
    float resolutionScale = 1.0f; // 解像度スケール
};

/**
 * @brief ビューポートブックマークを表すデータ構造
 */
export struct ViewportBookmark {
    std::string id;             // 一意の識別子 (UUID)
    std::string name;           // 表示名
    ViewportState state;       // ビューポート状態
    int shortcutKey = -1;       // ショートカットキー (0-9, -1=未割り当て)
    int order = 0;              // 表示順序
};

/**
 * @brief ブックマークコレクション
 */
export struct ViewportBookmarkCollection {
    std::vector<ViewportBookmark> bookmarks;
    int nextShortcutKey = 0;    // 次に割り当てるショートカットキー
};

} // namespace ArtifactCore
```

### 2. ViewportBookmarkManager クラス

**新規ファイル**: `ArtifactCore/include/Viewport/ViewportBookmarkManager.ixx`

```cpp
module;
#include <string>
#include <vector>
#include <memory>

export module Viewport.BookmarkManager;

import Viewport.Bookmark;
import Core.EventBus.Event;
import Core.Settings.FastSettingsStore;

export namespace ArtifactCore {

/**
 * @brief ビューポートブックマークを管理するシングルトンクラス
 */
export class ViewportBookmarkManager {
public:
    static ViewportBookmarkManager& instance();
    
    // ブックマーク操作
    std::string addBookmark(const ViewportBookmark& bookmark);
    bool updateBookmark(const std::string& id, const ViewportBookmark& bookmark);
    bool removeBookmark(const std::string& id);
    bool removeBookmarkByShortcut(int key);
    ViewportBookmark getBookmark(const std::string& id) const;
    ViewportBookmark getBookmarkByShortcut(int key) const;
    std::vector<ViewportBookmark> getAllBookmarks() const;
    
    // ショートカット管理
    bool assignShortcut(const std::string& id, int key);
    void releaseShortcut(int key);
    int getShortcutById(const std::string& id) const;
    std::string getIdByShortcut(int key) const;
    
    // 状態操作
    void applyBookmark(const ViewportBookmark& bookmark) const;
    ViewportBookmark captureCurrentState() const;
    
    // 保存/読み込み
    void saveToSettings(const std::string& projectId = "");
    void loadFromSettings(const std::string& projectId = "");
    void clearAll(const std::string& projectId = "");
    
    // イベント
    void setEventCallback(std::function<void(const ViewportBookmark&, BookmarkAction)> callback);
    
    enum class BookmarkAction {
        Added,
        Updated,
        Removed,
        Applied,
        Reordered
    };
    
    using BookmarkCallback = std::function<void(const ViewportBookmark&, BookmarkAction)>;
    
private:
    ViewportBookmarkManager();
    ~ViewportBookmarkManager();
    
    ViewportBookmarkManager(const ViewportBookmarkManager&) = delete;
    ViewportBookmarkManager& operator=(const ViewportBookmarkManager&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
```

**新規ファイル**: `ArtifactCore/src/Viewport/ViewportBookmarkManager.cppm`

```cpp
module;
#include <algorithm>
#include <unordered_map>
#include <uuid.h> // or custom UUID generator

import Viewport.Bookmark;
import Viewport.BookmarkManager;

namespace ArtifactCore {

class ViewportBookmarkManager::Impl {
public:
    std::vector<ViewportBookmark> bookmarks_;
    std::unordered_map<std::string, size_t> idToIndex_;
    std::unordered_map<int, std::string> shortcutToId_;
    std::string currentProjectId_;
    BookmarkCallback callback_;
    
    std::string generateId() const {
        // UUID v4 生成
        uuids::uuid uuid = uuids::uuid_system_generator{}();
        return uuids::to_string(uuid);
    }
    
    void notify(const ViewportBookmark& bookmark, BookmarkAction action) const {
        if (callback_) {
            callback_(bookmark, action);
        }
    }
};

ViewportBookmarkManager::ViewportBookmarkManager() 
    : impl_(std::make_unique<Impl>()) {}

ViewportBookmarkManager::~ViewportBookmarkManager() = default;

ViewportBookmarkManager& ViewportBookmarkManager::instance() {
    static ViewportBookmarkManager instance;
    return instance;
}

std::string ViewportBookmarkManager::addBookmark(const ViewportBookmark& bookmark) {
    auto& impl = *impl_;
    
    ViewportBookmark newBookmark = bookmark;
    newBookmark.id = impl.generateId();
    
    // ショートカット割り当て
    if (newBookmark.shortcutKey >= 0 && newBookmark.shortcutKey <= 9) {
        if (impl.shortcutToId_.find(newBookmark.shortcutKey) != impl.shortcutToId_.end()) {
            // 既に割り当てられている場合は解放
            releaseShortcut(newBookmark.shortcutKey);
        }
        impl.shortcutToId_[newBookmark.shortcutKey] = newBookmark.id;
    }
    
    impl.bookmarks_.push_back(newBookmark);
    impl.idToIndex_[newBookmark.id] = impl.bookmarks_.size() - 1;
    
    impl.notify(newBookmark, BookmarkAction::Added);
    
    return newBookmark.id;
}

bool ViewportBookmarkManager::updateBookmark(
    const std::string& id, const ViewportBookmark& bookmark) {
    auto& impl = *impl_;
    
    auto it = impl.idToIndex_.find(id);
    if (it == impl.idToIndex_.end()) {
        return false;
    }
    
    size_t index = it->second;
    ViewportBookmark& existing = impl.bookmarks_[index];
    
    // ショートカット変更
    if (existing.shortcutKey != bookmark.shortcutKey) {
        if (existing.shortcutKey >= 0) {
            impl.shortcutToId_.erase(existing.shortcutKey);
        }
        if (bookmark.shortcutKey >= 0 && bookmark.shortcutKey <= 9) {
            if (impl.shortcutToId_.find(bookmark.shortcutKey) != impl.shortcutToId_.end()) {
                releaseShortcut(bookmark.shortcutKey);
            }
            impl.shortcutToId_[bookmark.shortcutKey] = id;
        }
        existing.shortcutKey = bookmark.shortcutKey;
    }
    
    existing.name = bookmark.name;
    existing.state = bookmark.state;
    existing.order = bookmark.order;
    
    impl.notify(existing, BookmarkAction::Updated);
    
    return true;
}

bool ViewportBookmarkManager::removeBookmark(const std::string& id) {
    auto& impl = *impl_;
    
    auto it = impl.idToIndex_.find(id);
    if (it == impl.idToIndex_.end()) {
        return false;
    }
    
    size_t index = it->second;
    ViewportBookmark bookmark = impl.bookmarks_[index];
    
    // ショートカット解放
    if (bookmark.shortcutKey >= 0) {
        impl.shortcutToId_.erase(bookmark.shortcutKey);
    }
    
    //vectorから削除
    impl.bookmarks_.erase(impl.bookmarks_.begin() + index);
    
    // idToIndex_更新
    impl.idToIndex_.erase(it);
    for (size_t i = index; i < impl.bookmarks_.size(); ++i) {
        impl.idToIndex_[impl.bookmarks_[i].id] = i;
    }
    
    impl.notify(bookmark, BookmarkAction::Removed);
    
    return true;
}

ViewportBookmark ViewportBookmarkManager::getBookmark(const std::string& id) const {
    auto& impl = *impl_;
    auto it = impl.idToIndex_.find(id);
    if (it == impl.idToIndex_.end()) {
        return {};
    }
    return impl.bookmarks_[it->second];
}

ViewportBookmark ViewportBookmarkManager::getBookmarkByShortcut(int key) const {
    auto& impl = *impl_;
    auto it = impl.shortcutToId_.find(key);
    if (it == impl.shortcutToId_.end()) {
        return {};
    }
    return getBookmark(it->second);
}

std::vector<ViewportBookmark> ViewportBookmarkManager::getAllBookmarks() const {
    auto& impl = *impl_;
    auto bookmarks = impl.bookmarks_;
    // 順序でソート
    std::sort(bookmarks.begin(), bookmarks.end(), 
        [](const ViewportBookmark& a, const ViewportBookmark& b) {
            return a.order < b.order;
        });
    return bookmarks;
}

bool ViewportBookmarkManager::assignShortcut(const std::string& id, int key) {
    auto& impl = *impl_;
    
    if (key < 0 || key > 9) {
        return false;
    }
    
    // 既存の割り当てを解放
    if (impl.shortcutToId_.find(key) != impl.shortcutToId_.end()) {
        std::string existingId = impl.shortcutToId_[key];
        auto it = impl.idToIndex_.find(existingId);
        if (it != impl.idToIndex_.end()) {
            impl.bookmarks_[it->second].shortcutKey = -1;
        }
    }
    
    // 新しい割り当て
    auto it = impl.idToIndex_.find(id);
    if (it == impl.idToIndex_.end()) {
        return false;
    }
    
    // 既存のショートカットを解放
    int oldKey = impl.bookmarks_[it->second].shortcutKey;
    if (oldKey >= 0) {
        impl.shortcutToId_.erase(oldKey);
    }
    
    impl.bookmarks_[it->second].shortcutKey = key;
    impl.shortcutToId_[key] = id;
    
    return true;
}

void ViewportBookmarkManager::releaseShortcut(int key) {
    auto& impl = *impl_;
    auto it = impl.shortcutToId_.find(key);
    if (it == impl.shortcutToId_.end()) {
        return;
    }
    
    std::string id = it->second;
    auto idxIt = impl.idToIndex_.find(id);
    if (idxIt != impl.idToIndex_.end()) {
        impl.bookmarks_[idxIt->second].shortcutKey = -1;
    }
    impl.shortcutToId_.erase(it);
}

void ViewportBookmarkManager::saveToSettings(const std::string& projectId) {
    auto& impl = *impl_;
    impl.currentProjectId_ = projectId;
    
    // FastSettingsStore に保存
    FastSettingsStore store;
    std::string key = "viewport/bookmarks/" + projectId;
    
    // Simple serialization (JSON等)
    // 実装は省略 - 実際にはJSONシリアライザを使用
    // store.setValue(key, serialize(bookmarks_));
}

void ViewportBookmarkManager::loadFromSettings(const std::string& projectId) {
    auto& impl = *impl_;
    impl.currentProjectId_ = projectId;
    
    // FastSettingsStore から読み込み
    FastSettingsStore store;
    std::string key = "viewport/bookmarks/" + projectId;
    
    // 実装は省略 - 実際にはJSONデシリアライザを使用
    // auto data = store.value(key);
    // if (!data.isNull()) {
    //     bookmarks_ = deserialize<ViewportBookmarkCollection>(data);
    //     rebuildIndexes();
    // }
}

void ViewportBookmarkManager::setEventCallback(BookmarkCallback callback) {
    impl_->callback_ = callback;
}
```

### 3. CompositionRenderController への統合

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

```cpp
// ViewportState 取得
ViewportState CompositionRenderController::getViewportState() const {
    ViewportState state;
    state.zoom = impl_->renderer_->getZoom();
    
    float panX, panY;
    impl_->renderer_->getPan(panX, panY);
    state.panX = panX;
    state.panY = panY;
    
    state.rotation = impl_->renderer_->getRotation();
    state.resolutionScale = impl_->renderer_->resolutionScale();
    
    return state;
}

// ViewportState 適用
void CompositionRenderController::applyViewportState(const ViewportState& state) {
    impl_->renderer_->setZoom(state.zoom);
    impl_->renderer_->setPan(state.panX, state.panY);
    impl_->renderer_->setRotation(state.rotation);
    impl_->renderer_->setResolutionScale(state.resolutionScale);
    
    // イベント発行
    ViewportBookmarkAppliedEvent evt{state};
    EventBus::publish(evt);
}

// ブックマーク適用
void CompositionRenderController::applyBookmark(const ViewportBookmark& bookmark) {
    applyViewportState(bookmark.state);
}
```

### 4. CompositionEditor へのショートカット処理

**ファイル**: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`

```cpp
// KeyPressEvent ハンドラーに追加
void ArtifactCompositionEditor::keyPressEvent(QKeyEvent* event) {
    // 数字キー (1-9) でブックマーク適用
    if (event->modifiers() == Qt::NoModifier) {
        int key = event->key();
        if (key >= Qt::Key_1 && key <= Qt::Key_9) {
            int shortcut = key - Qt::Key_1;  // 0-8
            
            auto bookmark = ViewportBookmarkManager::instance().getBookmarkByShortcut(shortcut);
            if (!bookmark.id.empty()) {
                impl_->renderController_->applyBookmark(bookmark);
                update();
                return;
            }
        }
    }
    
    // Ctrl+数字キーでブックマーク保存
    if (event->modifiers() == Qt::ControlModifier) {
        int key = event->key();
        if (key >= Qt::Key_1 && key <= Qt::Key_9) {
            int shortcut = key - Qt::Key_1;
            saveCurrentStateAsBookmark(shortcut);
            return;
        }
    }
    
    // ... 既存処理
}

// 現在の状態をブックマークとして保存
void ArtifactCompositionEditor::saveCurrentStateAsBookmark(int shortcutKey) {
    auto state = impl_->renderController_->getViewportState();
    
    ViewportBookmark bookmark;
    bookmark.name = tr("Bookmark %1").arg(shortcutKey + 1);
    bookmark.state = state;
    bookmark.shortcutKey = shortcutKey;
    bookmark.order = shortcutKey;
    
    ViewportBookmarkManager::instance().addBookmark(bookmark);
    
    // 通知
    QMessageBox::information(this, tr("Bookmark Saved"), 
        tr("Current viewport state saved as bookmark %1").arg(shortcutKey + 1));
}
```

### 5. ViewMenu へのブックマークメニュー

**ファイル**: `Artifact/src/Widgets/ArtifactViewMenu.cppm`

```cpp
// ブックマークサブメニュー追加
void ArtifactViewMenu::setupViewportMenu() {
    // ... 既存コード
    
    viewportMenu->addSeparator();
    
    // ブックマークメニュー
    QMenu* bookmarksMenu = viewportMenu->addMenu(tr("Bookmarks"));
    
    // ブックマーク管理
    QAction* manageAction = bookmarksMenu->addAction(tr("Manage Bookmarks..."));
    connect(manageAction, &QAction::triggered, [this]() {
        showBookmarkManagerDialog();
    });
    
    QAction* saveAction = bookmarksMenu->addAction(tr("Save Current State"));
    connect(saveAction, &QAction::triggered, [this]() {
        saveCurrentStateAsBookmark();
    });
    
    bookmarksMenu->addSeparator();
    
    // ショートカット割り当て表示
    for (int i = 0; i < 9; ++i) {
        QAction* action = bookmarksMenu->addAction(
            tr("Bookmark %1").arg(i + 1));
        action->setShortcut(QKeySequence(tr("%1").arg(i + 1)));
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(action, &QAction::triggered, [this, i]() {
            applyBookmark(i);
        });
        
        // 既存のブックマークがあれば名前を更新
        auto bookmark = ViewportBookmarkManager::instance().getBookmarkByShortcut(i);
        if (!bookmark.id.empty()) {
            action->setText(QString::fromStdString(bookmark.name));
        }
    }
}

// ブックマークマネージャダイアログ表示
void ArtifactViewMenu::showBookmarkManagerDialog() {
    ViewportBookmarkDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // 更新通知
        updateBookmarksMenu();
    }
}

// ブックマーク適用
void ArtifactViewMenu::applyBookmark(int shortcutKey) {
    auto bookmark = ViewportBookmarkManager::instance().getBookmarkByShortcut(shortcutKey);
    if (!bookmark.id.empty()) {
        emit bookmarkApplied(bookmark);
    }
}

// 現在の状態を保存
void ArtifactViewMenu::saveCurrentStateAsBookmark() {
    // ダイアログでショートカットを選択
    bool ok;
    QString name = QInputDialog::getText(this, tr("Save Bookmark"), 
        tr("Bookmark name:"), QLineEdit::Normal, tr("Untitled"), &ok);
    
    if (ok && !name.isEmpty()) {
        int nextKey = ViewportBookmarkManager::instance().getNextShortcut();
        if (nextKey <= 9) {
            ViewportBookmark bookmark;
            bookmark.name = name.toStdString();
            bookmark.state = impl_->renderController->getViewportState();
            bookmark.shortcutKey = nextKey;
            bookmark.order = nextKey;
            
            ViewportBookmarkManager::instance().addBookmark(bookmark);
            updateBookmarksMenu();
        } else {
            QMessageBox::warning(this, tr("Error"), 
                tr("All shortcut keys are assigned. Please free a key first."));
        }
    }
}

// メニュー更新
void ArtifactViewMenu::updateBookmarksMenu() {
    // メニューを再構築
    // 実装は省略
}
```

### 6. ViewportBookmarkDialog

**新規ファイル**: `Artifact/include/Widgets/ViewportBookmarkDialog.ixx`

```cpp
module;
#include <QDialog>
#include <memory>

export module Widgets.ViewportBookmarkDialog;

import Viewport.Bookmark;
import Viewport.BookmarkManager;

namespace Artifact {

class ViewportBookmarkDialogImpl;

export class ViewportBookmarkDialog : public QDialog {
    Q_OBJECT
    W_OBJECT(ViewportBookmarkDialog)

public:
    explicit ViewportBookmarkDialog(QWidget* parent = nullptr);
    ~ViewportBookmarkDialog();
    
    // 現在のビューポート状態を取得して設定
    void setCurrentState(const ArtifactCore::ViewportState& state);
    
public slots:
    void accept() override;
    
signals:
    void bookmarkSaved(const ArtifactCore::ViewportBookmark& bookmark);
    void bookmarkRemoved(const std::string& id);
    
private:
    void setupUi();
    void loadBookmarks();
    void saveBookmarks();
    
    std::unique_ptr<ViewportBookmarkDialogImpl> impl_;
};

} // namespace Artifact
```

### 7. イベント定義

**新規ファイル**: `ArtifactCore/include/Event/ViewportBookmarkChangedEvent.ixx`

```cpp
module;
#include <string>

export module Event.ViewportBookmarkChangedEvent;

import Core.EventBus.Event;
import Viewport.Bookmark;

export namespace ArtifactCore {

// ブックマーク変更イベント
struct ViewportBookmarkChangedEvent : Event {
    enum class Action { Added, Updated, Removed, Cleared };
    Action action;
    ViewportBookmark bookmark;
    std::string projectId;
};

// ブックマーク適用イベント
struct ViewportBookmarkAppliedEvent : Event {
    ViewportState state;
    std::string bookmarkId;
};

} // namespace ArtifactCore
```

---

## タスク分割 (優先度付き)

### 优先度レベル
- **P0 (Critical)**: コア機能、なければ動作しない
- **P1 (High)**: 主要機能、ないと使い勝手が悪い
- **P2 (Medium)**: 便利機能、あってもなくても動作する
- **P3 (Low)**: 見栄え/UX向上、なくても機能する

### Phase 1: データ構造と管理クラス (1日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `ViewportState` 構造体定義 (zoom, panX, panY, rotation, resolutionScale) | P0 | 1h | なし | ✅ |
| `ViewportBookmark` 構造体定義 (id, name, state, shortcutKey, order) | P0 | 1h | 上記 | ✅ |
| `ViewportBookmarkCollection` 構造体定義 | P0 | 0.5h | 上記 | ✅ |
| `ViewportBookmarkManager` クラス基本設計 | P0 | 1h | 上記 | ✅ |
| `ViewportBookmarkManager::Impl` 実装 | P0 | 2h | 上記 | ✅ |
| CRUD操作 (`addBookmark`, `updateBookmark`, `removeBookmark`, `getBookmark`) | P0 | 2h | 上記 | ✅ |
| ショートカット管理 (`assignShortcut`, `releaseShortcut`, `getBookmarkByShortcut`) | P0 | 1h | 上記 | ✅ |

### Phase 2: 状態保存/読み込み (0.5日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `FastSettingsStore` へのシリアライゼーション (JSON) | P0 | 2h | Phase 1 | ✅ |
| プロジェクトIDごとの保存機能 | P0 | 1h | 上記 | ✅ |
| JSONシリアライザ/デシリアライザ実装 | P0 | 1h | 上記 | ✅ |
| バージョン管理 (v1: 基本版, v2: 拡張版) | P0 | 0.5h | 上記 | ✅ |
| 古いバージョンとの互換性 | P0 | 0.5h | 上記 | ✅ |

### Phase 3: Renderer統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `CompositionRenderController` に `getViewportState()` 追加 | P1 | 1h | Phase 1 | ✅ |
| `CompositionRenderController` に `applyViewportState()` 追加 | P1 | 1h | 上記 | ✅ |
| `CompositionRenderController` に `applyBookmark()` 追加 | P1 | 0.5h | 上記 | ✅ |
| `ViewportBookmarkManager` との連携テスト | P1 | 0.5h | 上記 | ✅ |
| `ViewportBookmarkAppliedEvent` 構造体定義 | P1 | 0.5h | Phase 1 | ✅ |
| イベント発行ロジック実装 | P1 | 0.5h | 上記 | ✅ |

### Phase 4: UI統合 (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `CompositionEditor` への1-9キーショートカット処理 | P1 | 2h | Phase 3 | ❌ (UIスレッド) |
| `CompositionEditor` へのCtrl+1-9ショートカット処理 | P1 | 1h | 上記 | ❌ (UIスレッド) |
| `ViewMenu` へのブックマークメニュー追加 | P1 | 1h | Phase 3 | ❌ (UIスレッド) |
| 解像度プリセットアクション (1-9) | P1 | 0.5h | 上記 | ❌ (UIスレッド) |
| ブックマーク管理メニュー項目 | P1 | 0.5h | 上記 | ❌ (UIスレッド) |
| 現在の状態を保存メニュー項目 | P1 | 0.5h | 上記 | ❌ (UIスレッド) |
| `ViewportBookmarkDialog` 基本UI | P1 | 2h | Phase 3 | ❌ (UIスレッド) |
| `ViewportBookmarkDialog` 有効/無効化 | P2 | 1h | 上記 | ❌ (UIスレッド) |
| `ViewportBookmarkDialog` 並べ替え機能 | P2 | 1h | 上記 | ❌ (UIスレッド) |
| `ViewportBookmarkDialog` 削除機能 | P1 | 0.5h | 上記 | ❌ (UIスレッド) |
| メニュー状態の同期 | P2 | 0.5h | 上記 | ❌ (UIスレッド) |
| 通知メッセージ | P3 | 0.5h | 上記 | ❌ (UIスレッド) |

### 並行作業可能性
- **Phase 1 (Core)**: 独立して並行可能
- **Phase 2 (保存/読み込み)**: Phase 1完了後、独立して並行可能
- **Phase 3 (Renderer)**: Phase 1完了後、独立して並行可能
- **Phase 4 (UI)**: UIスレッド依存のため、基本的には直列実施

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト (全てパスすること)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `ViewportState` 構造体のシリアライゼーション | 自動 | 正しく保存/復元できる | 1h |
| `ViewportBookmark` 構造体のシリアライゼーション | 自動 | 正しく保存/復元できる | 1h |
| `addBookmark` 通常ケース | 自動 | IDが生成され、リストに追加 | 1h |
| `addBookmark` ショートカット割り当て | 自動 | ショートカットが正しく割り当て | 1h |
| `updateBookmark` 通常ケース | 自動 | 既存のブックマークが更新 | 1h |
| `removeBookmark` 通常ケース | 自動 | ブックマークが削除 | 1h |
| `removeBookmark` ショートカット解放 | 自動 | ショートカットが解放 | 0.5h |
| `getBookmarkByShortcut` 通常ケース | 自動 | 正しいブックマークを返す | 0.5h |
| `getBookmarkByShortcut` 存在しないキー | 自動 | 空のブックマークを返す | 0.5h |
| `assignShortcut` 通常ケース | 自動 | ショートカットが再割り当て | 0.5h |
| `assignShortcut` 競合解決 | 自動 | 既存の割り当てが解放 | 0.5h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `getViewportState` 正確性 | 自動 | 現在の状態と一致 | 1h |
| `applyViewportState` 正確性 | 自動 | 状態が正しく適用 | 1h |
| `applyBookmark` 正確性 | 自動 | ブックマークの状態が正しく適用 | 1h |
| 状態保存/復元 | 自動 | 保存前と復元後の状態が同一 | 1h |
| プロジェクトごとの保存 | 自動 | 異なるプロジェクトで状態が分離 | 1h |
| 古いバージョンとの互換性 | 自動 | v1形式を読み込める | 1h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| ブックマーク適用時のビューポート状態 | 手動 | 状態が正しく復元 | 1h |
| 複数ブックマークの切り替え | 手動 | 全てのブックマークが正しく動作 | 1h |
| ショートカット競合の解決 | 手動 | 競合が正しく解決 | 1h |
| ブックマークの上限テスト (50個) | 手動 | 50個以上は保存できない | 0.5h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| 1-9キーでブックマーク適用 | 即座に適用 | 1h |
| Ctrl+1-9で現在の状態を保存 | 即座に保存 | 1h |
| ブックマーク管理ダイアログの操作性 | 直感的で分かりやすい | 2h |
| ブックマークの名前付け | 分かりやすい名前が付けられる | 0.5h |
| ブックマークの並べ替え | 直感的で分かりやすい | 0.5h |
| 通知メッセージの分かりやすさ | 明確で理解しやすい | 0.5h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| ブックマーク適用時の画質 | 通常と同じ品質 | 1h |
| ブックマーク適用時のパフォーマンス | 60fps維持 | 1h |
| 複数ブックマークの素早い切り替え | 切り替えがなめらか | 1h |
| 回転/解像度状態を含むブックマーク | 全ての状態が正しく復元 | 2h |

### 3. テスト実行計画

#### Phase 1: Unit Tests (Core + Manager)
```
日数: 1日
対象: ViewportBookmarkManager クラス
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 2: Serialization Tests (保存/読み込み)
```
日数: 0.5日
対象: JSONシリアライザ
方法: 自動テスト
実行: CIパイプラインで自動実行
```

#### Phase 3: Integration Tests (Renderer)
```
日数: 0.5日
対象: CompositionRenderController + ViewportBookmarkManager
方法: 自動テスト + 手動テスト
実行: CIパイプライン (自動) + QAチーム (手動)
```

#### Phase 4: UX Tests (UI)
```
日数: 1日
対象: CompositionEditor + ViewMenu + ViewportBookmarkDialog
方法: 手動テスト
実行: UXデザイナー + QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] パフォーマンス目標達成 (60fps維持)
- [ ] UXテストで問題なし
- [ ] 全てのショートカットが正常に動作
- [ ] ブックマークの保存/復元/削除が正常
- [ ] M-VP-4 (回転) 依存機能が正常に動作
- [ ] M-VP-5 (解像度) 依存機能が正常に動作

---

## 成果物

1. **コア機能**: ビューポート状態のブックマーク保存/復元
2. **API**: `ViewportBookmarkManager` クラス
3. **UI操作**:
   - 1-9キーでブックマーク適用
   - Ctrl+1-9で現在の状態を保存
   - メニューからの管理
4. **状態管理**: プロジェクトごとのブックマーク保存
5. **イベントシステム**: ブックマーク操作時の通知

---

## 依存関係

### 必要な前提条件
- **M-VP-4 (キャンバス回転)** - 回転状態をブックマーク
- **M-VP-5 (解像度動的切替)** - 解像度状態をブックマーク

### 連携する機能
- `ViewportBookmarkManager` - 中核管理クラス
- `CompositionRenderController` - 状態取得/適用
- `CompositionEditor` - ショートカット処理
- `ViewMenu` - メニューUI

### 影響を受ける機能
- 既存のビューポート操作は影響なし
- 新規機能として完全に独立

---

## リスクと対策

### リスク1: 状態同期の複雑さ
**内容**: 複数のビューポートやマルチモニター環境での状態管理
**対策**:
- 当初はシングルビューポート前提で実装
- マルチビューポート対応は後続タスク
- プロジェクトIDで状態を分離

### リスク2: ショートカット競合
**内容**: 既存のショートカットとの競合
**対策**:
- 1-9キーはビューポート操作専用とする
- 既存のショートカットを確認し、競合を回避
- 競合がある場合はShift/Ctrl修飾子を使用

### リスク3: 状態保存の互換性
**内容**: 古いバージョンではブックマークを読み込めない
**対策**:
- バージョン管理を導入
- 古いバージョンではブックマークを無視
- マイグレーションパスを提供

### リスク4: メモリ使用量
**内容**: 大量のブックマークを保存した場合のメモリ消費
**対策**:
- ブックマーク数に上限を設ける（例: 50個）
- 古いブックマークを自動で削除
- 使用頻度に基づく優先順位

---

## テスト項目

- [ ] 1-9キーでブックマーク適用
- [ ] Ctrl+1-9で現在の状態を保存
- [ ] ブックマークの名前付け
- [ ] ブックマークの削除
- [ ] ブックマークの並べ替え
- [ ] 状態保存/読み込み
- [ ] プロジェクトごとの保存
- [ ] ショートカットの再割り当て
- [ ] 全てのビューポート状態（ズーム/パン/回転/解像度）の保存
- [ ] メニューの表示と更新

---

## 完了基準

- [ ] 全てのショートカットが正常に動作
- [ ] ブックマークの保存/削除/並べ替えが直感的
- [ ] 状態保存/読み込みが正常に動作
- [ ] プロジェクトごとにブックマークが分離
- [ ] 既存のビューポート操作との競合がない
- [ ] 全てのテスト項目がパス
- [ ] ドキュメントが更新

---

## 関連文書

- `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm` - 既存のブックマークストア設計
- `docs/planned/MILESTONE_VIEWPORT_CANVAS_ROTATION_2026-06-27.md` - 回転機能
- `docs/planned/MILESTONE_VIEWPORT_DYNAMIC_RESOLUTION_2026-06-27.md` - 解像度切替

---

## メモ

- 本マイルストーンはM-VP-4とM-VP-5に依存
- M-VP-4とM-VP-5を先に実装することを推奨
- 単独で実装する場合は、回転と解像度状態を除外した基本版とする
- 将来的には、ブックマークを他ユーザーと共有できるように拡張
- クラウド同期機能も検討

---

## 2026-07-25 現状確認

静的確認では、`ArtifactViewMenu.cppm` に `ViewportBookmarkStore` があり、composition ID ごとに CBOR 設定へ名前付きブックマークを保存・一覧・削除し、View メニューから保存／削除／復元できる。保存対象は zoom、pan、viewport orientation で、Composition Editor には専用の Bookmark ボタンもある。

一方、仕様の 1〜9 ショートカット、並べ替え、編集ダイアログ、回転角度・解像度スケールの保存、プロジェクトファイルへの埋め込みは確認できない。保存先はプロジェクト内ではなく AppData の `ViewportBookmarks/viewport_bookmarks.cbor` で、回転・解像度マイルストーンも未実装のため完全な状態保存には至っていない。したがって本マイルストーンは「zoom / pan / orientation の基本版は実装済み、完全仕様は未完了」と判定する。

確認範囲: `Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`。ビルド・実機操作による動作確認は未実施。
