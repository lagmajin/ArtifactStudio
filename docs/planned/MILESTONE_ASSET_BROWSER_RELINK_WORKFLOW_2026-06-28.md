# Milestone: Asset Browser Relink Workflow (M-AB-10)

**マイルストーンID**: M-AB-10
**作成日**: 2026-06-28
**優先度**: P1 (High)
**推定工数**: 2-3日
**カテゴリ**: Asset Browser / Workflow / Project Management
**状態**: Planned
**依存**: M-AB (Asset Browser base), M-AS-4 (Asset System Integration)

---

## 目的

アセットブラウザーに再リンクワークフロー機能を実装する。ユーザーは移動されたファイルを再リンクし、プロジェクト内の参照を修復できる。また、未使用アセットの特定と参照関係の追跡機能を提供する。

---

## 背景

### 現状
- 既存の`MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` (Phase 3) で「Find References / Select Unused」の概念が言及されているが未実装
- `ArtifactAssetBrowser.cppm`には`isImportedAssetPath()`/`isUnusedAssetPath()`/`isMissingAssetPath()`などのステータスチェック機能がある
- `AssetMenuItem`構造体にはステータス情報を表示する機能があるが、再リンク操作は未実装
- プロジェクト内のアセット参照は`ProjectManager`で管理されている
- ファイルの移動/リネームに対応した自動再リンク機能なし

### 要件
- **Find References**: 選択したアセットが使用されているコンポジション/レイヤーを特定
- **Select Unused**: 未使用のアセットをハイライト/フィルタ
- **Relink Assets**: 移動されたファイルを新しいパスに再リンク
- **Missing Assets Detection**: 存在しないファイルを検出して警告
- **Batch Relink**: 複数のアセットをまとめて再リンク
- **Relink Dialog**: 古いパスと新しいパスのマッピングを表示
- **Undo Support**: 再リンク操作をUndo/Redo可能

### ユースケース
1. ファイルを別のフォルダに移動した後、プロジェクトを修復
2. 外部ドライブからのアセットがオフラインになった際の再リンク
3. プロジェクト内で未使用のアセットを特定してクリーンアップ
4. 特定のアセットがどこで使用されているかを確認
5. 複数のアセットを新しい場所に一括移動

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `ArtifactCore/include/Asset/AssetReferenceTracker.ixx` | アセット参照追跡クラス |
| **新規** | `ArtifactCore/src/Asset/AssetReferenceTracker.cppm` | 実装 |
| **新規** | `Artifact/src/Widgets/Asset/AssetRelinkDialog.cppm` | 再リンクダイアログ |
| **新規** | `Artifact/include/Widgets/Asset/AssetRelinkDialog.ixx` | ヘッダー |
| **新規** | `Artifact/src/Widgets/Asset/ReferencesPanel.cppm` | 参照パネルウィジェット |
| **新規** | `Artifact/include/Widgets/Asset/ReferencesPanel.ixx` | ヘッダー |
| **変更** | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | コンテキストメニュー追加、再リンクロジック |
| **変更** | `Artifact/include/Widgets/ArtifactAssetBrowser.ixx` | シグナル/スロット追加 |
| **変更** | `Artifact/src/Asset/AssetMenuModel.ixx` | ステータス情報拡張 |
| **新規** | `ArtifactCore/include/Event/AssetRelinkedEvent.ixx` | 再リンクイベント |
| **新規** | `ArtifactCore/include/Undo/RelinkAssetsCommand.ixx` | Undoコマンド |

---

## 変更詳細

### 1. AssetReferenceTracker - アセット参照追跡

**新規ファイル**: `ArtifactCore/include/Asset/AssetReferenceTracker.ixx`

```cpp
module;
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

export module Asset.ReferenceTracker;

import std;

namespace ArtifactCore {

/**
 * @brief プロジェクト内のアセット参照を追跡するクラス
 *アセットがどのコンポジション/レイヤーで使用されているかを管理
 */
export class AssetReferenceTracker {
public:
    static AssetReferenceTracker& instance();
    
    // 参照情報の登録
    void registerReference(const std::string& assetPath, 
                          const std::string& compositionId, 
                          const std::string& layerId);
    
    void removeReference(const std::string& assetPath, 
                         const std::string& compositionId, 
                         const std::string& layerId);
    
    void removeAllReferences(const std::string& assetPath);
    void removeCompositionReferences(const std::string& compositionId);
    
    // 参照情報の取得
    std::vector<std::pair<std::string, std::string>> getReferences(
        const std::string& assetPath) const;
    
    std::vector<std::string> getReferencingCompositions(
        const std::string& assetPath) const;
    
    std::vector<std::string> getReferencingLayers(
        const std::string& assetPath) const;
    
    std::set<std::string> getAllReferencedAssets() const;
    std::set<std::string> getUnusedAssets(
        const std::set<std::string>& allAssets) const;
    
    std::set<std::string> getMissingAssets() const;
    
    // 状態チェック
    bool isReferenced(const std::string& assetPath) const;
    bool isUnused(const std::string& assetPath, 
                 const std::set<std::string>& allAssets) const;
    bool isMissing(const std::string& assetPath) const;
    
    // 更新
    void updateFromProject();
    void clear();
    
    // 通知
    void setChangeCallback(std::function<void()> callback);
    
private:
    AssetReferenceTracker();
    ~AssetReferenceTracker();
    
    AssetReferenceTracker(const AssetReferenceTracker&) = delete;
    AssetReferenceTracker& operator=(const AssetReferenceTracker&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
```

**新規ファイル**: `ArtifactCore/src/Asset/AssetReferenceTracker.cppm`

```cpp
module;
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>

import Asset.ReferenceTracker;

namespace ArtifactCore {

class AssetReferenceTracker::Impl {
public:
    // assetPath -> {(compositionId, layerId)}
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> references_;
    
    // compositionId -> {assetPath}
    std::unordered_map<std::string, std::set<std::string>> compositionReferences_;
    
    // assetPath -> isMissing
    std::unordered_map<std::string, bool> missingStatus_;
    
    std::function<void()> changeCallback_;
    
    void notifyChange() {
        if (changeCallback_) changeCallback_();
    }
};

AssetReferenceTracker::AssetReferenceTracker() : impl_(std::make_unique<Impl>()) {}

AssetReferenceTracker::~AssetReferenceTracker() = default;

AssetReferenceTracker& AssetReferenceTracker::instance() {
    static AssetReferenceTracker instance;
    return instance;
}

void AssetReferenceTracker::registerReference(
    const std::string& assetPath, 
    const std::string& compositionId, 
    const std::string& layerId) {
    
    auto& refs = impl_->references_[assetPath];
    auto it = std::find(refs.begin(), refs.end(), 
                       std::make_pair(compositionId, layerId));
    if (it == refs.end()) {
        refs.emplace_back(compositionId, layerId);
        impl_->compositionReferences_[compositionId].insert(assetPath);
        impl_->notifyChange();
    }
}

void AssetReferenceTracker::removeReference(
    const std::string& assetPath, 
    const std::string& compositionId, 
    const std::string& layerId) {
    
    auto it = impl_->references_.find(assetPath);
    if (it != impl_->references_.end()) {
        auto& refs = it->second;
        auto refIt = std::find(refs.begin(), refs.end(), 
                             std::make_pair(compositionId, layerId));
        if (refIt != refs.end()) {
            refs.erase(refIt);
            
            // compositionReferences_も更新
            auto compIt = impl_->compositionReferences_.find(compositionId);
            if (compIt != impl_->compositionReferences_.end()) {
                compIt->second.erase(assetPath);
            }
            
            impl_->notifyChange();
        }
        
        // 空になったら削除
        if (refs.empty()) {
            impl_->references_.erase(it);
        }
    }
}

void AssetReferenceTracker::removeAllReferences(const std::string& assetPath) {
    auto it = impl_->references_.find(assetPath);
    if (it != impl_->references_.end()) {
        // compositionReferences_からも削除
        for (const auto& [compId, layerId] : it->second) {
            auto compIt = impl_->compositionReferences_.find(compId);
            if (compIt != impl_->compositionReferences_.end()) {
                compIt->second.erase(assetPath);
            }
        }
        impl_->references_.erase(it);
        impl_->notifyChange();
    }
}

void AssetReferenceTracker::removeCompositionReferences(const std::string& compositionId) {
    auto compIt = impl_->compositionReferences_.find(compositionId);
    if (compIt != impl_->compositionReferences_.end()) {
        for (const auto& assetPath : compIt->second) {
            auto refIt = impl_->references_.find(assetPath);
            if (refIt != impl_->references_.end()) {
                auto& refs = refIt->second;
                refs.erase(
                    std::remove_if(refs.begin(), refs.end(),
                        [&](const auto& p) { return p.first == compositionId; }),
                    refs.end()
                );
                if (refs.empty()) {
                    impl_->references_.erase(refIt);
                }
            }
        }
        impl_->compositionReferences_.erase(compIt);
        impl_->notifyChange();
    }
}

std::vector<std::pair<std::string, std::string>> 
AssetReferenceTracker::getReferences(const std::string& assetPath) const {
    auto it = impl_->references_.find(assetPath);
    if (it != impl_->references_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> AssetReferenceTracker::getReferencingCompositions(
    const std::string& assetPath) const {
    auto refs = getReferences(assetPath);
    std::set<std::string> result;
    for (const auto& [compId, layerId] : refs) {
        result.insert(compId);
    }
    return std::vector<std::string>(result.begin(), result.end());
}

std::vector<std::string> AssetReferenceTracker::getReferencingLayers(
    const std::string& assetPath) const {
    auto refs = getReferences(assetPath);
    std::vector<std::string> result;
    for (const auto& [compId, layerId] : refs) {
        result.push_back(layerId);
    }
    return result;
}

std::set<std::string> AssetReferenceTracker::getAllReferencedAssets() const {
    std::set<std::string> result;
    for (const auto& [assetPath, refs] : impl_->references_) {
        if (!refs.empty()) {
            result.insert(assetPath);
        }
    }
    return result;
}

std::set<std::string> AssetReferenceTracker::getUnusedAssets(
    const std::set<std::string>& allAssets) const {
    std::set<std::string> result;
    for (const auto& assetPath : allAssets) {
        if (!isReferenced(assetPath)) {
            result.insert(assetPath);
        }
    }
    return result;
}

std::set<std::string> AssetReferenceTracker::getMissingAssets() const {
    std::set<std::string> result;
    for (const auto& [assetPath, isMissing] : impl_->missingStatus_) {
        if (isMissing) {
            result.insert(assetPath);
        }
    }
    return result;
}

bool AssetReferenceTracker::isReferenced(const std::string& assetPath) const {
    return impl_->references_.find(assetPath) != impl_->references_.end() &&
           !impl_->references_.at(assetPath).empty();
}

bool AssetReferenceTracker::isUnused(
    const std::string& assetPath, 
    const std::set<std::string>& allAssets) const {
    return allAssets.find(assetPath) != allAssets.end() &&
           !isReferenced(assetPath);
}

bool AssetReferenceTracker::isMissing(const std::string& assetPath) const {
    auto it = impl_->missingStatus_.find(assetPath);
    return it != impl_->missingStatus_.end() && it->second;
}

void AssetReferenceTracker::updateFromProject() {
    // ProjectManagerから参照情報を更新
    // 実装は省略
    impl_->notifyChange();
}

void AssetReferenceTracker::clear() {
    impl_->references_.clear();
    impl_->compositionReferences_.clear();
    impl_->missingStatus_.clear();
    impl_->notifyChange();
}

void AssetReferenceTracker::setChangeCallback(std::function<void()> callback) {
    impl_->changeCallback_ = callback;
}

} // namespace ArtifactCore
```

### 2. AssetRelinkDialog - 再リンクダイアログ

**新規ファイル**: `Artifact/include/Widgets/Asset/AssetRelinkDialog.ixx`

```cpp
module;
#include <QDialog>
#include <memory>

export module Widgets.Asset.AssetRelinkDialog;

import std;
import QList;
import QString;

namespace Artifact {

struct RelinkItem {
    QString oldPath;
    QString newPath;
    bool isConfirmed = false;
    bool isMissing = true;
};

class AssetRelinkDialogImpl;

/**
 * @brief アセットの再リンクを行うダイアログ
 *古いパスと新しいパスのマッピングを表示して確認
 */
export class AssetRelinkDialog : public QDialog {
    Q_OBJECT
    W_OBJECT(AssetRelinkDialog)

public:
    explicit AssetRelinkDialog(QWidget* parent = nullptr);
    ~AssetRelinkDialog();
    
    // アイテムを設定
    void setRelinkItems(const QList<RelinkItem>& items);
    QList<RelinkItem> getConfirmedItems() const;
    
    // 設定
    void setAutoSearchEnabled(bool enabled);
    void setSearchPath(const QString& path);
    
signals:
    void relinkConfirmed(const QList<RelinkItem>& items);
    
public slots:
    void accept() override;
    
private:
    void setupUi();
    void searchForMissingFiles();
    void updateTable();
    
    std::unique_ptr<AssetRelinkDialogImpl> impl_;
};

} // namespace Artifact
```

**新規ファイル**: `Artifact/src/Widgets/Asset/AssetRelinkDialog.cppm`

```cpp
module;
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>

import Widgets.Asset.AssetRelinkDialog;

namespace Artifact {

class AssetRelinkDialogImpl {
public:
    QTableWidget* table_ = nullptr;
    QLineEdit* searchPathEdit_ = nullptr;
    QPushButton* searchButton_ = nullptr;
    QPushButton* autoSearchButton_ = nullptr;
    QList<RelinkItem> items_;
    QList<RelinkItem> confirmedItems_;
    bool autoSearchEnabled_ = true;
};

AssetRelinkDialog::AssetRelinkDialog(QWidget* parent)
    : QDialog(parent), impl_(std::make_unique<Impl>()) {
    setWindowTitle(tr("Relink Assets"));
    setMinimumSize(600, 400);
    resize(800, 500);
    
    setupUi();
}

AssetRelinkDialog::~AssetRelinkDialog() = default;

void AssetRelinkDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(11, 11, 11, 11);
    
    // 情報ラベル
    auto* infoLabel = new QLabel(
        tr("The following assets are missing. Please specify new locations."),
        this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    
    // 検索パス
    auto* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(4);
    
    searchLayout->addWidget(new QLabel(tr("Search Path:"), this));
    impl_->searchPathEdit_ = new QLineEdit(this);
    searchLayout->addWidget(impl_->searchPathEdit_);
    
    impl_->searchButton_ = new QPushButton(tr("Browse..."), this);
    connect(impl_->searchButton_, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getExistingDirectory(
            this, tr("Select Search Directory"));
        if (!path.isEmpty()) {
            impl_->searchPathEdit_->setText(path);
        }
    });
    searchLayout->addWidget(impl_->searchButton_);
    
    impl_->autoSearchButton_ = new QPushButton(tr("Auto Search"), this);
    connect(impl_->autoSearchButton_, &QPushButton::clicked, 
            this, &AssetRelinkDialog::searchForMissingFiles);
    searchLayout->addWidget(impl_->autoSearchButton_);
    
    layout->addLayout(searchLayout);
    
    // テーブル
    impl_->table_ = new QTableWidget(this);
    impl_->table_->setColumnCount(4);
    impl_->table_->setHorizontalHeaderLabels({
        tr("Status"), tr("Original Path"), tr("New Path"), tr("Confirm")
    });
    impl_->table_->horizontalHeader()->setStretchLastSection(true);
    impl_->table_->verticalHeader()->setVisible(false);
    impl_->table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl_->table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    impl_->table_->setSelectionMode(QAbstractItemView::SingleSelection);
    
    layout->addWidget(impl_->table_);
    
    // ボタン
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    auto* okButton = new QPushButton(tr("OK"), this);
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, this, &AssetRelinkDialog::accept);
    buttonLayout->addWidget(okButton);
    
    layout->addLayout(buttonLayout);
    
    // ダブルクリックでファイル選択
    connect(impl_->table_, &QTableWidget::cellDoubleClicked, 
            [this](int row, int col) {
        if (col == 2) { // New Path列
            QString path = QFileDialog::getOpenFileName(
                this, tr("Select New File"));
            if (!path.isEmpty()) {
                impl_->items_[row].newPath = path;
                impl_->items_[row].isConfirmed = true;
                updateTable();
            }
        }
    });
}

void AssetRelinkDialog::setRelinkItems(const QList<RelinkItem>& items) {
    impl_->items_ = items;
    impl_->confirmedItems_.clear();
    updateTable();
}

QList<RelinkItem> AssetRelinkDialog::getConfirmedItems() const {
    return impl_->confirmedItems_;
}

void AssetRelinkDialog::setAutoSearchEnabled(bool enabled) {
    impl_->autoSearchEnabled_ = enabled;
}

void AssetRelinkDialog::setSearchPath(const QString& path) {
    impl_->searchPathEdit_->setText(path);
}

void AssetRelinkDialog::accept() {
    // 確認されたアイテムを収集
    impl_->confirmedItems_.clear();
    for (const auto& item : impl_->items_) {
        if (!item.newPath.isEmpty()) {
            impl_->confirmedItems_.append(item);
        }
    }
    
    if (impl_->confirmedItems_.isEmpty()) {
        QMessageBox::warning(this, tr("No Changes"), 
                           tr("No files were relinked. Please specify new paths."));
        return;
    }
    
    emit relinkConfirmed(impl_->confirmedItems_);
    QDialog::accept();
}

void AssetRelinkDialog::searchForMissingFiles() {
    QString searchPath = impl_->searchPathEdit_->text();
    if (searchPath.isEmpty()) return;
    
    QDir dir(searchPath);
    if (!dir.exists()) return;
    
    bool foundAny = false;
    
    for (auto& item : impl_->items_) {
        if (item.isMissing && item.newPath.isEmpty()) {
            QFileInfo oldInfo(item.oldPath);
            QString fileName = oldInfo.fileName();
            
            // 同じファイル名を検索
            QFileInfoList files = dir.entryInfoList(
                QDir::Files | QDir::NoDotAndDotDot | QDir::AllEntries);
            
            for (const QFileInfo& file : files) {
                if (file.fileName() == fileName) {
                    item.newPath = file.absoluteFilePath();
                    item.isMissing = false;
                    item.isConfirmed = true;
                    foundAny = true;
                    break;
                }
            }
        }
    }
    
    if (foundAny) {
        updateTable();
        QMessageBox::information(this, tr("Auto Search"),
            tr("%1 files were found and linked automatically.").arg(foundAny ? QString::number(foundAny) : "0"));
    } else {
        QMessageBox::information(this, tr("Auto Search"),
            tr("No matching files were found."));
    }
}

void AssetRelinkDialog::updateTable() {
    impl_->table_->setRowCount(impl_->items_.size());
    
    for (int i = 0; i < impl_->items_.size(); ++i) {
        const auto& item = impl_->items_[i];
        
        // Status
        auto* statusItem = new QTableWidgetItem();
        if (item.isMissing) {
            statusItem->setText(tr("Missing"));
            statusItem->setTextColor(Qt::red);
        } else if (!item.newPath.isEmpty()) {
            statusItem->setText(tr("Found"));
            statusItem->setTextColor(Qt::green);
        } else {
            statusItem->setText(tr("OK"));
            statusItem->setTextColor(Qt::gray);
        }
        impl_->table_->setItem(i, 0, statusItem);
        
        // Original Path
        auto* oldPathItem = new QTableWidgetItem(item.oldPath);
        oldPathItem->setToolTip(item.oldPath);
        impl_->table_->setItem(i, 1, oldPathItem);
        
        // New Path
        auto* newPathItem = new QTableWidgetItem(item.newPath);
        newPathItem->setToolTip(item.newPath);
        impl_->table_->setItem(i, 2, newPathItem);
        
        // Confirm
        auto* confirmItem = new QTableWidgetItem();
        confirmItem->setCheckState(item.isConfirmed ? Qt::Checked : Qt::Unchecked);
        confirmItem->setTextAlignment(Qt::AlignCenter);
        impl_->table_->setItem(i, 3, confirmItem);
    }
    
    impl_->table_->resizeColumnsToContents();
}

} // namespace Artifact
```

### 3. ReferencesPanel - 参照パネル

**新規ファイル**: `Artifact/include/Widgets/Asset/ReferencesPanel.ixx`

```cpp
module;
#include <QWidget>
#include <memory>

export module Widgets.Asset.ReferencesPanel;

import std;

namespace Artifact {

class ReferencesPanelImpl;

/**
 * @brief 選択したアセットの参照情報を表示するパネル
 *Find References機能のUI
 */
export class ReferencesPanel : public QWidget {
    Q_OBJECT
    W_OBJECT(ReferencesPanel)

public:
    explicit ReferencesPanel(QWidget* parent = nullptr);
    ~ReferencesPanel();
    
    // アセットを設定
    void setAssetPath(const std::string& assetPath);
    
    // 表示を更新
    void refresh();
    
signals:
    void referenceSelected(const std::string& compositionId, 
                           const std::string& layerId);
    void showInProjectViewRequested(const std::string& compositionId);
    
private:
    void setupUi();
    void updateReferences();
    
    std::unique_ptr<ReferencesPanelImpl> impl_;
};

} // namespace Artifact
```

**新規ファイル**: `Artifact/src/Widgets/Asset/ReferencesPanel.cppm`

```cpp
module;
#include <QWidget>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

import Widgets.Asset.ReferencesPanel;
import Asset.ReferenceTracker;

namespace Artifact {

class ReferencesPanelImpl {
public:
    QTreeWidget* referencesTree_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    std::string currentAssetPath_;
};

ReferencesPanel::ReferencesPanel(QWidget* parent)
    : QWidget(parent), impl_(std::make_unique<Impl>()) {
    setupUi();
}

ReferencesPanel::~ReferencesPanel() = default;

void ReferencesPanel::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);
    
    // ステータスラベル
    impl_->statusLabel_ = new QLabel(tr("Select an asset to view references"), this);
    impl_->statusLabel_->setWordWrap(true);
    impl_->statusLabel_->setStyleSheet("color: #888;");
    layout->addWidget(impl_->statusLabel_);
    
    // 参照ツリー
    impl_->referencesTree_ = new QTreeWidget(this);
    impl_->referencesTree_->setHeaderLabels({tr("Composition"), tr("Layer")});
    impl_->referencesTree_->setColumnCount(2);
    impl_->referencesTree_->header()->setStretchLastSection(false);
    impl_->referencesTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    impl_->referencesTree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    
    layout->addWidget(impl_->referencesTree_);
    
    // ダブルクリックで選択
    connect(impl_->referencesTree_, &QTreeWidget::itemDoubleClicked,
            [this](QTreeWidgetItem* item, int col) {
        if (!item) return;
        
        QString compositionId = item->data(0, Qt::UserRole).toString();
        QString layerId = item->data(1, Qt::UserRole).toString();
        
        emit referenceSelected(compositionId.toStdString(), layerId.toStdString());
    });
    
    // コンテキストメニュー
    impl_->referencesTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(impl_->referencesTree_, &QTreeWidget::customContextMenuRequested,
            [this](const QPoint& pos) {
        QTreeWidgetItem* item = impl_->referencesTree_->itemAt(pos);
        if (item) {
            QString compositionId = item->data(0, Qt::UserRole).toString();
            // コンテキストメニューを表示
            // 実装は省略
        }
    });
}

void ReferencesPanel::setAssetPath(const std::string& assetPath) {
    impl_->currentAssetPath_ = assetPath;
    refresh();
}

void ReferencesPanel::refresh() {
    if (impl_->currentAssetPath_.empty()) {
        impl_->statusLabel_->setText(tr("Select an asset to view references"));
        impl_->referencesTree_->clear();
        return;
    }
    
    auto& tracker = ArtifactCore::AssetReferenceTracker::instance();
    auto refs = tracker.getReferences(impl_->currentAssetPath_);
    
    if (refs.empty()) {
        impl_->statusLabel_->setText(
            tr("This asset is not used in any composition."));
        impl_->referencesTree_->clear();
        return;
    }
    
    impl_->statusLabel_->setText(
        tr("Used in %1 compositions").arg(refs.size()));
    
    impl_->referencesTree_->clear();
    
    // Compositionごとにグループ化
    std::map<std::string, std::vector<std::string>> compToLayers;
    for (const auto& [compId, layerId] : refs) {
        compToLayers[compId].push_back(layerId);
    }
    
    for (const auto& [compId, layers] : compToLayers) {
        auto* compItem = new QTreeWidgetItem(impl_->referencesTree_);
        compItem->setText(0, QString::fromStdString(compId));
        compItem->setData(0, Qt::UserRole, QString::fromStdString(compId));
        compItem->setExpanded(true);
        
        for (const auto& layerId : layers) {
            auto* layerItem = new QTreeWidgetItem(compItem);
            layerItem->setText(1, QString::fromStdString(layerId));
            layerItem->setData(0, Qt::UserRole, QString::fromStdString(compId));
            layerItem->setData(1, Qt::UserRole, QString::fromStdString(layerId));
        }
    }
    
    impl_->referencesTree_->resizeColumnToContents(0);
    impl_->referencesTree_->resizeColumnToContents(1);
}

} // namespace Artifact
```

### 4. ArtifactAssetBrowser への統合

**ファイル**: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

```cpp
// Impl クラスにフィールド追加
class ArtifactAssetBrowser::Impl {
    // ... 既存フィールド
    
    // 再リンク機能
    ReferencesPanel* referencesPanel_ = nullptr;
    QAction* findReferencesAction_ = nullptr;
    QAction* selectUnusedAction_ = nullptr;
    QAction* relinkAssetsAction_ = nullptr;
    QAction* showMissingAction_ = nullptr;
};

// コンストラクタでアクション初期化
void ArtifactAssetBrowser::Impl::setupContextMenu() {
    // ... 既存コード
    
    // 分離線
    menu->addSeparator();
    
    // 参照機能
    findReferencesAction_ = menu->addAction(tr("Find References"));
    connect(findReferencesAction_, &QAction::triggered, [this]() {
        QModelIndexList selected = fileView_->selectionModel()->selectedIndexes();
        if (!selected.isEmpty()) {
            int row = selected.first().row();
            auto item = assetModel_->itemAt(row);
            if (!item.path.empty()) {
                if (!referencesPanel_) {
                    referencesPanel_ = new ReferencesPanel();
                    referencesPanel_->setWindowTitle(tr("References"));
                }
                referencesPanel_->setAssetPath(item.path.c_str());
                referencesPanel_->show();
                referencesPanel_->raise();
            }
        }
    });
    
    selectUnusedAction_ = menu->addAction(tr("Select Unused"));
    connect(selectUnusedAction_, &QAction::triggered, [this]() {
        selectUnusedAssets();
    });
    
    // 分離線
    menu->addSeparator();
    
    // 再リンク機能
    relinkAssetsAction_ = menu->addAction(tr("Relink Assets..."));
    connect(relinkAssetsAction_, &QAction::triggered, [this]() {
        showRelinkDialog();
    });
    
    showMissingAction_ = menu->addAction(tr("Show Missing Only"));
    showMissingAction_->setCheckable(true);
    connect(showMissingAction_, &QAction::toggled, [this](bool checked) {
        if (checked) {
            setStatusFilter("missing");
        } else {
            setStatusFilter("all");
        }
    });
}

// 未使用アセットを選択
void ArtifactAssetBrowser::Impl::selectUnusedAssets() {
    auto& tracker = ArtifactCore::AssetReferenceTracker::instance();
    
    // 全てのアセットを取得
    std::set<std::string> allAssets;
    // 実装は省略 - AssetMenuModelから取得
    
    auto unusedAssets = tracker.getUnusedAssets(allAssets);
    
    if (unusedAssets.empty()) {
        QMessageBox::information(self_, tr("No Unused Assets"),
                               tr("All assets are being used in the project."));
        return;
    }
    
    // 未使用アセットを選択
    fileView_->selectionModel()->clearSelection();
    
    for (int i = 0; i < assetModel_->rowCount(); ++i) {
        auto item = assetModel_->itemAt(i);
        for (const auto& unusedPath : unusedAssets) {
            if (item.path == unusedPath) {
                QModelIndex index = assetModel_->index(i);
                fileView_->selectionModel()->select(
                    index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                break;
            }
        }
    }
    
    QMessageBox::information(self_, tr("Unused Assets Selected"),
                           tr("%1 unused assets selected.").arg(unusedAssets.size()));
}

// 再リンクダイアログを表示
void ArtifactAssetBrowser::Impl::showRelinkDialog() {
    // 缶和されたアセットを取得
    std::set<std::string> missingAssets;
    for (int i = 0; i < assetModel_->rowCount(); ++i) {
        auto item = assetModel_->itemAt(i);
        QFileInfo info(QString::fromStdString(item.path.c_str()));
        if (!info.exists()) {
            missingAssets.insert(item.path.c_str());
        }
    }
    
    if (missingAssets.empty()) {
        QMessageBox::information(self_, tr("No Missing Assets"),
                               tr("All assets are accessible."));
        return;
    }
    
    QList<RelinkItem> items;
    for (const auto& path : missingAssets) {
        RelinkItem item;
        item.oldPath = QString::fromStdString(path);
        item.newPath = "";
        item.isMissing = true;
        item.isConfirmed = false;
        items.append(item);
    }
    
    AssetRelinkDialog dialog(self_);
    dialog.setRelinkItems(items);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto confirmedItems = dialog.getConfirmedItems();
        if (!confirmedItems.isEmpty()) {
            performRelink(confirmedItems);
        }
    }
}

// 再リンクを実行
void ArtifactAssetBrowser::Impl::performRelink(
    const QList<RelinkItem>& items) {
    
    // Undoコマンドを作成
    auto* cmd = new RelinkAssetsCommand(items);
    
    // 実行
    cmd->redo();
    
    // UndoStackに追加
    UndoManager::instance().push(cmd);
    
    // アセットモデルを更新
    assetModel_->refresh();
    
    // 参照トラッカーを更新
    auto& tracker = ArtifactCore::AssetReferenceTracker::instance();
    tracker.updateFromProject();
    
    QMessageBox::information(self_, tr("Relink Complete"),
                           tr("%1 assets were relinked.").arg(items.size()));
}
```

### 5. Undoコマンド

**新規ファイル**: `ArtifactCore/include/Undo/RelinkAssetsCommand.ixx`

```cpp
module;
#include <QUndoCommand>
#include <QStringList>

export module Undo.RelinkAssetsCommand;

import std;
import QList;

namespace Artifact {

struct RelinkItem;

/**
 * @brief アセット再リンクのUndoコマンド
 */
export class RelinkAssetsCommand : public QUndoCommand {
public:
    RelinkAssetsCommand(const QList<RelinkItem>& items, 
                       QUndoCommand* parent = nullptr);
    ~RelinkAssetsCommand();
    
    void redo() override;
    void undo() override;
    
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;
    
private:
    QList<RelinkItem> items_;
    QList<RelinkItem> oldItems_;  // Undo用に古い状態を保存
};

} // namespace Artifact
```

**新規ファイル**: `ArtifactCore/src/Undo/RelinkAssetsCommand.cppm`

```cpp
module;
#include <QUndoCommand>
#include <QDebug>

import Undo.RelinkAssetsCommand;
import Widgets.Asset.AssetRelinkDialog;
import Artifact.Project.Manager;

namespace Artifact {

RelinkAssetsCommand::RelinkAssetsCommand(const QList<RelinkItem>& items, 
                                         QUndoCommand* parent)
    : QUndoCommand(parent), items_(items) {
    
    // 元の状態を保存
    for (const auto& item : items) {
        RelinkItem oldItem;
        oldItem.oldPath = item.oldPath;
        oldItem.newPath = item.oldPath;  // 元のパス
        oldItem.isConfirmed = true;
        oldItem.isMissing = false;
        oldItems_.append(oldItem);
    }
    
    setText(tr("Relink %1 Assets").arg(items.size()));
}

RelinkAssetsCommand::~RelinkAssetsCommand() = default;

void RelinkAssetsCommand::redo() {
    for (const auto& item : items_) {
        // ProjectManagerを通してアセットパスを更新
        ArtifactCore::ProjectManager::instance().relinkAsset(
            item.oldPath.toStdString(), 
            item.newPath.toStdString()
        );
    }
    
    // イベント発行
    // AssetRelinkedEventを発行
}

void RelinkAssetsCommand::undo() {
    for (const auto& item : oldItems_) {
        // 元のパスに戻す
        ArtifactCore::ProjectManager::instance().relinkAsset(
            item.oldPath.toStdString(), 
            item.newPath.toStdString()
        );
    }
}

int RelinkAssetsCommand::id() const {
    return 10004; //Relink Assets Command ID
}

bool RelinkAssetsCommand::mergeWith(const QUndoCommand* other) {
    // 連続したRelinkコマンドはマージ可能
    const RelinkAssetsCommand* cmd = dynamic_cast<const RelinkAssetsCommand*>(other);
    if (!cmd) return false;
    
    // 簡易マージロジック
    // 実装は省略
    return true;
}

} // namespace Artifact
```

### 6. イベント定義

**新規ファイル**: `ArtifactCore/include/Event/AssetRelinkedEvent.ixx`

```cpp
module;
#include <string>
#include <vector>

export module Event.AssetRelinkedEvent;

import Core.EventBus.Event;

namespace ArtifactCore {

// アセット再リンクイベント
struct AssetRelinkedEvent : Event {
    std::vector<std::pair<std::string, std::string>> relinkedPairs; // (oldPath, newPath)
};

// 参照情報変更イベント
struct AssetReferencesChangedEvent : Event {
    std::string assetPath;
    enum class ChangeType { Added, Removed, AllUpdated };
    ChangeType changeType;
};

// 缶和アセット検出イベント
struct MissingAssetsDetectedEvent : Event {
    std::vector<std::string> missingAssets;
};

} // namespace ArtifactCore
```

---

## タスク分割 (優先度付き)

### 優先度レベル
- **P0 (Critical)**: コア機能、なければ動作しない
- **P1 (High)**: 主要機能、ないと使い勝手が悪い
- **P2 (Medium)**: 便利機能、あってもなくても動作する
- **P3 (Low)**: 見栄え/UX向上、なくても機能する

### Phase 1: 参照追跡基盤 (1日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetReferenceTracker` クラス設計 | P0 | 1h | なし | ✅ |
| `AssetReferenceTracker::Impl` 実装 | P0 | 3h | 上記 | ✅ |
| 参照登録/削除機能 | P0 | 2h | 上記 | ✅ |
| 参照取得機能 | P0 | 2h | 上記 | ✅ |
| 未使用/缶和判定機能 | P0 | 2h | 上記 | ✅ |

### Phase 2: 再リンクUI (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetRelinkDialog` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| `AssetRelinkDialog::Impl` 実装 | P1 | 3h | 上記 | ✅ |
| テーブル表示と編集 | P1 | 2h | 上記 | ✅ |
| オートサーチ機能 | P1 | 2h | 上記 | ✅ |
| `ReferencesPanel` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| `ReferencesPanel::Impl` 実装 | P1 | 2h | 上記 | ✅ |

### Phase 3: AssetBrowser統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| コンテキストメニューにアクション追加 | P1 | 1h | Phase 2 | ❌ (UIスレッド) |
| Find References機能 | P1 | 1h | Phase 2 | ❌ (UIスレッド) |
| Select Unused機能 | P1 | 1h | Phase 2 | ❌ (UIスレッド) |
| Show Missing Onlyフィルタ | P1 | 0.5h | Phase 2 | ❌ (UIスレッド) |

### Phase 4: Undoサポート (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `RelinkAssetsCommand` クラス設計 | P1 | 1h | Phase 3 | ✅ |
| `RelinkAssetsCommand` 実装 | P1 | 2h | 上記 | ✅ |
| UndoManager統合 | P1 | 1h | 上記 | ✅ |
| コマンドのマージ | P2 | 1h | 上記 | ✅ |

### Phase 5: イベントシステム (0.5日) - **P2**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetRelinkedEvent` 構造体定義 | P2 | 0.5h | Phase 4 | ✅ |
| `AssetReferencesChangedEvent` 構造体定義 | P2 | 0.5h | Phase 4 | ✅ |
| `MissingAssetsDetectedEvent` 構造体定義 | P2 | 0.5h | Phase 4 | ✅ |
| イベント発行ロジック | P2 | 1h | 上記 | ✅ |
| イベントリスナー登録 | P2 | 1h | 上記 | ✅ |

### 並行作業可能性
- **Phase 1 (Core)**: 独立して並行可能
- **Phase 2 (UI)**: Phase 1完了後、独立して並行可能
- **Phase 3 (Integration)**: UIスレッド依存のため、基本的には直列実施
- **Phase 4 (Undo)**: Phase 3完了後、独立して並行可能
- **Phase 5 (Events)**: Phase 4完了後、独立して並行可能

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト (全てパスすること)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `AssetReferenceTracker::registerReference` | 自動 | 参照が正しく登録される | 1h |
| `AssetReferenceTracker::removeReference` | 自動 | 参照が正しく削除される | 1h |
| `AssetReferenceTracker::getReferences` | 自動 | 正しい参照リストを返す | 1h |
| `AssetReferenceTracker::getUnusedAssets` | 自動 | 未使用アセットを正しく特定 | 1h |
| `AssetReferenceTracker::getMissingAssets` | 自動 | 缶和アセットを正しく特定 | 1h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `AssetRelinkDialog::setRelinkItems` | 自動 | テーブルが正しく更新 | 1h |
| `AssetRelinkDialog::searchForMissingFiles` | 自動 | 自動検索が動作 | 1h |
| `ReferencesPanel::setAssetPath` | 自動 | 参照が正しく表示 | 1h |
| `RelinkAssetsCommand::redo` | 自動 | 再リンクが実行される | 1h |
| `RelinkAssetsCommand::undo` | 自動 | 元の状態に戻る | 1h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| Find References機能 | 手動 | 参照が正しく表示 | 2h |
| Select Unused機能 | 手動 | 未使用アセットが選択 | 2h |
| Relink Assets機能 | 手動 | 再リンクが成功 | 2h |
| Show Missing Onlyフィルタ | 手動 | 缶和アセットのみ表示 | 1h |
| Undo/Redo | 手動 | 操作が取り消せる | 1h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| コンテキストメニューの操作性 | 直感的で分かりやすい | 1h |
| 再リンクダイアログの使いやすさ | 分かりやすい | 2h |
| 参照パネルの表示 | 分かりやすい | 1h |
| オートサーチの速度 | 素早い | 1h |
| 通知メッセージ | 明確で理解しやすい | 0.5h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| アセットのステータス表示 | 明確で分かりやすい | 1h |
| 再リンク後のUI更新 | 即座に反映 | 1h |
| 複数アセットの一括再リンク | なめらか | 1h |

### 3. テスト実行計画

#### Phase 1: Unit Tests (Reference Tracker)
```
日数: 1日
対象: AssetReferenceTracker クラス
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 2: UI Tests (Dialog + Panel)
```
日数: 1日
対象: AssetRelinkDialog + ReferencesPanel
方法: 手動テスト
実行: QAチーム
```

#### Phase 3: Integration Tests
```
日数: 1日
対象: AssetBrowser 統合
方法: 手動テスト
実行: QAチーム
```

#### Phase 4: Undo Tests
```
日数: 0.5日
対象: Undo/Redo 機能
方法: 手動テスト
実行: QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] Find Referencesが正しく動作
- [ ] Select Unusedが正しく動作
- [ ] Relink Assetsが正しく動作
- [ ] Show Missing Onlyフィルタが正しく動作
- [ ] Undo/Redoが正しく動作
- [ ] 全てのテスト項目がパス

---

## 成果物

1. **コア機能**: アセットの再リンク
2. **API**: `AssetReferenceTracker` クラス
3. **UIコンポーネント**: `AssetRelinkDialog`, `ReferencesPanel`
4. **Undoサポート**: `RelinkAssetsCommand`
5. **統合**: AssetBrowser への完全統合
6. **イベントシステム**: 再リンク/参照変更イベント

---

## 依存関係

### 必要な前提条件
- **M-AB (Asset Browser 基盤)** - 基本的なAsset Browser機能
- **M-AS-4 (Asset System Integration)** - プロジェクトとの統合
- **`ProjectManager`** - プロジェクト管理
- **`UndoManager`** - Undo機能

### 連携する機能
- `AssetReferenceTracker` - 参照追跡
- `AssetRelinkDialog` - 再リンクUI
- `ReferencesPanel` - 参照表示
- `ArtifactAssetBrowser` - ホストアプリケーション
- `ProjectManager` - プロジェクト管理

### 影響を受ける機能
- AssetBrowserのコンテキストメニュー
- アセットのステータス表示
- プロジェクトの保存/読み込み

---

## リスクと対策

### リスク1: 参照追跡のパフォーマンス
**内容**: 多数のアセット/コンポジションがある場合のパフォーマンス低下
**対策**:
- 参照情報をIndex付きで管理
- 更新はバッチ処理で実行
- 変化通知はディボンス（遅延実行）を使用

### リスク2: ファイルシステムの変化
**内容**: 再リンク中にファイルが移動/削除された場合
**対策**:
- 再リンク前にファイルの存在を確認
- 存在しないファイルは缶和リストに追加
- エラーメッセージを明確に表示

### リスク3: パスの一意性
**内容**: 同じファイル名の異なるファイルがある場合の競合
**対策**:
- フルパスで比較
- 同名ファイルがある場合はユーザーに選択を促す
- サムチェックを実装（将来的な拡張）

### リスク4: Undoの管理
**内容**: 大量の再リンク操作のUndo管理
**対策**:
- コマンドをマージしてUndoスタックのサイズを制限
- 連続した再リンク操作は1つのコマンドにまとめる
- メモリ使用量を監視

---

## テスト項目

- [ ] Find Referencesで参照を特定
- [ ] Select Unusedで未使用アセットを選択
- [ ] Relink Assetsでファイルを再リンク
- [ ] Show Missing Onlyで缶和アセットをフィルタ
- [ ] オートサーチ機能
- [ ] Undo/Redoが正しく動作
- [ ] 複数ファイルの一括再リンク
- [ ] 参照トラッカーの更新
- [ ] イベントの発行と受信
- [ ] 通知メッセージ

---

## 完了基準

- [ ] Find Referencesが正しく動作
- [ ] Select Unusedが正しく動作
- [ ] Relink Assetsが正しく動作
- [ ] Show Missing Onlyフィルタが正しく動作
- [ ] Undo/Redoが正しく動作
- [ ] 全てのファイルタイプに対応
- [ ] パフォーマンスに影響なし
- [ ] 全てのテスト項目がパス
- [ ] UXテストで問題なし
- [ ] ドキュメントが更新

---

## 関連文書

- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` - 統合先メインファイル
- `Artifact/src/Asset/AssetMenuModel.ixx` - アイテム構造
- `Artifact/src/Project/ArtifactProjectManager.cppm` - プロジェクト管理
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` - 元のロードマップ
- `docs/planned/MILESTONES_BACKLOG.md` - マイルストーンバックログ

---

## メモ

- 再リンクワークフローはプロジェクト管理の重要な機能
- After Effectsの「Link」機能と類似のUXを目指す
- 将来的には、リンクの相対パス/絶対パスの切り替えを実装
- クラウドストレージとの統合も検討
- 同名ファイルの競合解決を強化
# 2026-07-10 Quick Replace Progress

- Composition Editor Command Palette に複数選択向け `Quick Replace Selected Sources` を追加
- 既存の `replaceLayerSourceInCurrentComposition()` を再利用し、layer実体を置換せずsourceだけを更新
- Transform / timing / mask / effect / parent を保持し、locked layerと非対応layerはskipする
- 1ファイル選択時は全対象へ適用し、複数ファイル選択時はlayer順に1対1で割り当てる
- source path のbefore / after snapshotをUndo履歴へ積み、複数置換を1回で戻せるようにした
- layer source kind と選択ファイル拡張子を事前照合し、非互換な割り当てはmutation前にskipする
- 複数ファイル時に Layer Order または Layer Name to Filename matching を選択可能にした
- name matching はcase・空白・underscore・hyphen等を正規化し、fileの重複割当を防ぐ
- mutation前に最大12件の layer-to-file 対応表を確認表示する

## 2026-07-25 実装監査

- `ArtifactAssetBrowser` の欠落素材向け relink 操作、`ArtifactProjectService` の単一／複数 footage relink と path-based relink、`RelinkAssetCommand` の Undo／Redo 経路を確認できる。
- AI `WorkspaceAutomation` から `relinkFootageByPath` を呼ぶ経路も存在し、基本的な relink workflow は計画書作成時より実装が進んでいる。
- 一方、専用 `AssetReferenceTracker`、References Panel、Find References／Select Unused のユーザー導線、古いパスと新しいパスを一覧表示する batch dialog、プロジェクト全体の参照追跡・診断は確認できない。
- よって単一 relink と Undo は実装済み、参照管理を含む全体 milestone は未完了の Partial 判定とする。
## 2026-07-25 実装監査（更新）

判定: 単一／複数 footage の relink、missing footage の検索ルート操作、Undo/Redo、AI automation は実装済み。参照管理全体の高度な機能は未完了。

- `ArtifactAssetBrowser` と `ArtifactProjectManagerWidget` に missing footage の単一／複数 relink UI があり、`ArtifactProjectService` の path-based relink を利用する。
- `RelinkAssetCommand` により old path / new path を Undo/Redo でき、`WorkspaceAutomation.relinkFootageByPath` からも操作できる。
- `ArtifactProjectHealthChecker` は missing asset 診断と、設定時の missing entry 除去を持つ。Project/Asset 側の missing 表示も確認できる。
- ただし専用 `AssetReferenceTracker`、References Panel、Find References / Select Unused の完成したユーザー導線、同名候補の衝突解決、参照一覧の一括診断は確認できない。
- したがって relink の基本 workflow は実装済みだが、参照追跡・診断・全ファイル種別の統合を含む本 milestone は Partial のまま。runtime の Undo/Redo と複数ファイル結果は未検証。

ビルド・実行確認はリポジトリ方針により未実施。
