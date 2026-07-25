# Milestone: Asset Browser Tag System (M-AB-12)

**マイルストーンID**: M-AB-12
**作成日**: 2026-06-28
**優先度**: P2 (Medium)
**推定工数**: 2-3日
**カテゴリ**: Asset Browser / Metadata / Organization
**状態**: Planned
**依存**: M-AB (Asset Browser base), M-AB-11 (Advanced Sort)

---

## 目的

アセットブラウザーにタグシステムを実装する。ユーザーはアセットにカスタムタグを付与でき、タグによるフィルタリング、ソート、グループ化が可能になる。これにより、大量のアセットを柔軟に分類・管理できる。

---

## 背景

### 現状
- アセットブラウザーには`AssetMenuItem`構造体があるが、タグ機能は未実装
- 既存のフィルタリングはファイルタイプ、ステータス、検索文字列に限定
- `ArtifactAssetBrowser.cppm`の`applyFilters()`はカテゴリベースのフィルタリング
- お気に入り（Favorites）機能はあるが、カスタムタグは未実装
- タグベースの整理機能なし

### 要件
- **Tag Management**: タグの作成、編集、削除
- **Tag Assignment**: アセットに対するタグの付与/削除
- **Multi-select Tags**: 1つのアセットに複数のタグを付与
- **Tag Filtering**: タグによるアセットフィルタリング
- **Tag Cloud**: タグの可視化（使用頻度、色分けなど）
- **Tag Groups**: タグの階層的管理（カテゴリ/サブカテゴリ）
- **Tag Search**: タグ名による検索
- **Tag Persistence**: タグの永続化（プロジェクトごと）
- **Tag Import/Export**: タグのエクスポート/インポート

### ユースケース
1. アセットをプロジェクト固有のカテゴリ（例えば「Character», "Background", "Props"）で分類
2. アセットを状態（"Final", "WIP", "Review"）で管理
3. 複数の条件でタグフィルタリング（例: "Character" AND "Final")
4. タグによるクイック検索
5. タグの一括編集
6. タグの色分け表示

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **新規** | `ArtifactCore/include/Asset/AssetTag.ixx` | タグデータ構造 |
| **新規** | `ArtifactCore/src/Asset/AssetTag.cppm` | 実装 |
| **新規** | `ArtifactCore/include/Asset/TagManager.ixx` | タグ管理クラス |
| **新規** | `ArtifactCore/src/Asset/TagManager.cppm` | タグ管理実装 |
| **新規** | `ArtifactCore/include/Asset/TagDatabase.ixx` | タグデータベース |
| **新規** | `ArtifactCore/src/Asset/TagDatabase.cppm` | タグデータベース実装 |
| **新規** | `Artifact/src/Widgets/Asset/TagEditorWidget.cppm` | タグ編集ウィジェット |
| **新規** | `Artifact/include/Widgets/Asset/TagEditorWidget.ixx` | ヘッダー |
| **新規** | `Artifact/src/Widgets/Asset/TagFilterWidget.cppm` | タグフィルタウィジェット |
| **新規** | `Artifact/include/Widgets/Asset/TagFilterWidget.ixx` | ヘッダー |
| **新規** | `Artifact/src/Widgets/Asset/TagCloudWidget.cppm` | タグクラウドウィジェット |
| **新規** | `Artifact/include/Widgets/Asset/TagCloudWidget.ixx` | ヘッダー |
| **新規** | `Artifact/src/Widgets/Asset/TagManagementDialog.cppm` | タグ管理ダイアログ |
| **新規** | `Artifact/include/Widgets/Asset/TagManagementDialog.ixx` | ヘッダー |
| **変更** | `Artifact/include/Asset/AssetMenuModel.ixx` | タグフィールド追加 |
| **変更** | `Artifact/src/Asset/AssetMenuModel.cppm` | タグ統合 |
| **変更** | `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` | タグUI統合 |
| **新規** | `ArtifactCore/include/Event/AssetTagsChangedEvent.ixx` | タグ変更イベント |

---

## 変更詳細

### 1. AssetTag - タグデータ構造

**新規ファイル**: `ArtifactCore/include/Asset/AssetTag.ixx`

```cpp
module;
#include <string>
#include <vector>
#include <memory>

#include <QColor>
#include <QString>

export module Asset.Tag;

import std;

namespace ArtifactCore {

/**
 * @brief タグを表すデータ構造
 */
export struct AssetTag {
    std::string id;              // 一意の識別子 (UUID)
    std::string name;           // タグ名
    QColor color;               // タグの色
    std::string description;    // 説明文
    std::string group;          // タググループ（カテゴリ）
    int order;                  // 表示順序
    bool isSystemTag;           // システム定義タグか
    
    // 使用統計
    int usageCount = 0;        // 使用回数
    
    bool operator==(const AssetTag& other) const;
    bool operator<(const AssetTag& other) const;
    
    // 表示用
    QString displayName() const;
};

/**
 * @brief タグのコレクション
 */
export struct TagCollection {
    std::vector<AssetTag> tags;
    std::unordered_map<std::string, size_t> nameToIndex;
    
    void addTag(const AssetTag& tag);
    bool removeTag(const std::string& id);
    bool removeTagByName(const std::string& name);
    AssetTag* findTagById(const std::string& id);
    AssetTag* findTagByName(const std::string& name);
    bool contains(const std::string& id) const;
    bool containsByName(const std::string& name) const;
};

/**
 * @brief アセットとタグの関連付け
 */
export struct AssetTagAssignment {
    std::string assetPath;      // アセットのパス
    std::vector<std::string> tagIds;  // 付与されたタグID
    
    void addTagId(const std::string& tagId);
    void removeTagId(const std::string& tagId);
    bool hasTagId(const std::string& tagId) const;
};

} // namespace ArtifactCore
```

**新規ファイル**: `ArtifactCore/src/Asset/AssetTag.cppm`

```cpp
module;
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include <QColor>
#include <QString>

import Asset.Tag;

namespace ArtifactCore {

bool AssetTag::operator==(const AssetTag& other) const {
    return id == other.id;
}

bool AssetTag::operator<(const AssetTag& other) const {
    if (order != other.order) return order < other.order;
    return name < other.name;
}

QString AssetTag::displayName() const {
    return QString::fromStdString(name);
}

void TagCollection::addTag(const AssetTag& tag) {
    tags.push_back(tag);
    nameToIndex[tag.name] = tags.size() - 1;
    
    // orderでソート
    std::sort(tags.begin(), tags.end());
    rebuildIndex();
}

bool TagCollection::removeTag(const std::string& id) {
    auto it = std::find_if(tags.begin(), tags.end(),
        [&](const AssetTag& t) { return t.id == id; });
    
    if (it != tags.end()) {
        size_t index = std::distance(tags.begin(), it);
        nameToIndex.erase(it->name);
        tags.erase(it);
        rebuildIndex();
        return true;
    }
    return false;
}

bool TagCollection::removeTagByName(const std::string& name) {
    auto it = nameToIndex.find(name);
    if (it != nameToIndex.end()) {
        size_t index = it->second;
        nameToIndex.erase(it);
        nameToIndex.erase(tags[index].name);
        tags.erase(tags.begin() + index);
        rebuildIndex();
        return true;
    }
    return false;
}

AssetTag* TagCollection::findTagById(const std::string& id) {
    auto it = std::find_if(tags.begin(), tags.end(),
        [&](const AssetTag& t) { return t.id == id; });
    return it != tags.end() ? &(*it) : nullptr;
}

AssetTag* TagCollection::findTagByName(const std::string& name) {
    auto it = nameToIndex.find(name);
    return it != nameToIndex.end() ? &tags[it->second] : nullptr;
}

bool TagCollection::contains(const std::string& id) const {
    return std::find_if(tags.begin(), tags.end(),
        [&](const AssetTag& t) { return t.id == id; }) != tags.end();
}

bool TagCollection::containsByName(const std::string& name) const {
    return nameToIndex.find(name) != nameToIndex.end();
}

void TagCollection::rebuildIndex() {
    nameToIndex.clear();
    for (size_t i = 0; i < tags.size(); ++i) {
        nameToIndex[tags[i].name] = i;
    }
}

void AssetTagAssignment::addTagId(const std::string& tagId) {
    if (!hasTagId(tagId)) {
        tagIds.push_back(tagId);
    }
}

void AssetTagAssignment::removeTagId(const std::string& tagId) {
    auto it = std::find(tagIds.begin(), tagIds.end(), tagId);
    if (it != tagIds.end()) {
        tagIds.erase(it);
    }
}

bool AssetTagAssignment::hasTagId(const std::string& tagId) const {
    return std::find(tagIds.begin(), tagIds.end(), tagId) != tagIds.end();
}

} // namespace ArtifactCore
```

### 2. TagDatabase - タグデータベース

**新規ファイル**: `ArtifactCore/include/Asset/TagDatabase.ixx`

```cpp
module;
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include <QString>

export module Asset.TagDatabase;

import Asset.Tag;

namespace ArtifactCore {

/**
 * @brief タグとアセットの関連付けを管理するデータベース
 *SQLiteまたはJSONファイルで永続化
 */
export class TagDatabase {
public:
    static TagDatabase& instance();
    
    // タグ管理
    std::string createTag(const AssetTag& tag);
    bool updateTag(const AssetTag& tag);
    bool deleteTag(const std::string& tagId);
    AssetTag getTag(const std::string& tagId) const;
    TagCollection getAllTags() const;
    TagCollection getTagsByGroup(const std::string& group) const;
    
    // タグ検索
    TagCollection findTagsByName(const std::string& namePattern) const;
    
    // アセット-タグ関連
    void assignTagToAsset(const std::string& assetPath, 
                          const std::string& tagId);
    void unassignTagFromAsset(const std::string& assetPath, 
                              const std::string& tagId);
    std::vector<std::string> getTagIdsForAsset(const std::string& assetPath) const;
    std::vector<std::string> getAssetPathsForTag(const std::string& tagId) const;
    
    // タグフィルタ
    std::vector<std::string> getAssetPathsWithAllTags(
        const std::vector<std::string>& tagIds) const;
    std::vector<std::string> getAssetPathsWithAnyTag(
        const std::vector<std::string>& tagIds) const;
    
    // 保存/読み込み
    void save(const QString& projectId = "");
    void load(const QString& projectId = "");
    void clear();
    
    // 通知
    void setChangeCallback(std::function<void()> callback);
    
private:
    TagDatabase();
    ~TagDatabase();
    
    TagDatabase(const TagDatabase&) = delete;
    TagDatabase& operator=(const TagDatabase&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
```

**新規ファイル**: `ArtifactCore/src/Asset/TagDatabase.cppm`

```cpp
module;
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUuid>

import Asset.TagDatabase;
import Asset.Tag;

namespace ArtifactCore {

class TagDatabase::Impl {
public:
    TagCollection tags_;
    std::unordered_map<std::string, std::vector<std::string>> assetToTags_;
    std::unordered_map<std::string, std::vector<std::string>> tagToAssets_;
    std::function<void()> changeCallback_;
    mutable std::mutex mutex_;
    QString currentProjectId_;
    
    std::string generateTagId() const {
        return QUuid::createUuid().toString().toStdString();
    }
    
    void notifyChange() {
        if (changeCallback_) changeCallback_();
    }
};

TagDatabase::TagDatabase() : impl_(std::make_unique<Impl>()) {}

TagDatabase::~TagDatabase() = default;

TagDatabase& TagDatabase::instance() {
    static TagDatabase instance;
    return instance;
}

std::string TagDatabase::createTag(const AssetTag& tag) {
    std::lock_guard lock(impl_->mutex_);
    
    AssetTag newTag = tag;
    newTag.id = impl_->generateTagId();
    
    impl_->tags_.addTag(newTag);
    impl_->notifyChange();
    
    return newTag.id;
}

bool TagDatabase::updateTag(const AssetTag& tag) {
    std::lock_guard lock(impl_->mutex_);
    
    AssetTag* existing = impl_->tags_.findTagById(tag.id);
    if (!existing) return false;
    
    *existing = tag;
    impl_->tags_.rebuildIndex();
    impl_->notifyChange();
    
    return true;
}

bool TagDatabase::deleteTag(const std::string& tagId) {
    std::lock_guard lock(impl_->mutex_);
    
    if (!impl_->tags_.removeTag(tagId)) return false;
    
    // tagToAssets_からも削除
    impl_->tagToAssets_.erase(tagId);
    
    // assetToTags_から該当するタグを削除
    for (auto& [assetPath, tagIds] : impl_->assetToTags_) {
        auto it = std::find(tagIds.begin(), tagIds.end(), tagId);
        if (it != tagIds.end()) {
            tagIds.erase(it);
        }
    }
    
    impl_->notifyChange();
    return true;
}

AssetTag TagDatabase::getTag(const std::string& tagId) const {
    std::lock_guard lock(impl_->mutex_);
    AssetTag* tag = impl_->tags_.findTagById(tagId);
    return tag ? *tag : AssetTag{};
}

TagCollection TagDatabase::getAllTags() const {
    std::lock_guard lock(impl_->mutex_);
    return impl_->tags_;
}

TagCollection TagDatabase::getTagsByGroup(const std::string& group) const {
    std::lock_guard lock(impl_->mutex_);
    TagCollection result;
    for (const auto& tag : impl_->tags_.tags) {
        if (tag.group == group) {
            result.addTag(tag);
        }
    }
    return result;
}

TagCollection TagDatabase::findTagsByName(const std::string& namePattern) const {
    std::lock_guard lock(impl_->mutex_);
    TagCollection result;
    QString pattern = QString::fromStdString(namePattern);
    
    for (const auto& tag : impl_->tags_.tags) {
        if (QString::fromStdString(tag.name).contains(pattern, Qt::CaseInsensitive)) {
            result.addTag(tag);
        }
    }
    return result;
}

void TagDatabase::assignTagToAsset(const std::string& assetPath, 
                                const std::string& tagId) {
    std::lock_guard lock(impl_->mutex_);
    
    // assetToTags_
    impl_->assetToTags_[assetPath].push_back(tagId);
    
    // tagToAssets_
    impl_->tagToAssets_[tagId].push_back(assetPath);
    
    // 重複を削除
    auto& tagIds = impl_->assetToTags_[assetPath];
    std::sort(tagIds.begin(), tagIds.end());
    tagIds.erase(std::unique(tagIds.begin(), tagIds.end()), tagIds.end());
    
    auto& assetPaths = impl_->tagToAssets_[tagId];
    std::sort(assetPaths.begin(), assetPaths.end());
    assetPaths.erase(std::unique(assetPaths.begin(), assetPaths.end()), assetPaths.end());
    
    impl_->notifyChange();
}

void TagDatabase::unassignTagFromAsset(const std::string& assetPath, 
                                       const std::string& tagId) {
    std::lock_guard lock(impl_->mutex_);
    
    // assetToTags_
    auto assetIt = impl_->assetToTags_.find(assetPath);
    if (assetIt != impl_->assetToTags_.end()) {
        auto& tagIds = assetIt->second;
        auto tagIt = std::find(tagIds.begin(), tagIds.end(), tagId);
        if (tagIt != tagIds.end()) {
            tagIds.erase(tagIt);
        }
        if (tagIds.empty()) {
            impl_->assetToTags_.erase(assetIt);
        }
    }
    
    // tagToAssets_
    auto tagIt = impl_->tagToAssets_.find(tagId);
    if (tagIt != impl_->tagToAssets_.end()) {
        auto& assetPaths = tagIt->second;
        auto assetIt2 = std::find(assetPaths.begin(), assetPaths.end(), assetPath);
        if (assetIt2 != assetPaths.end()) {
            assetPaths.erase(assetIt2);
        }
        if (assetPaths.empty()) {
            impl_->tagToAssets_.erase(tagIt);
        }
    }
    
    impl_->notifyChange();
}

std::vector<std::string> TagDatabase::getTagIdsForAsset(
    const std::string& assetPath) const {
    std::lock_guard lock(impl_->mutex_);
    auto it = impl_->assetToTags_.find(assetPath);
    if (it != impl_->assetToTags_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> TagDatabase::getAssetPathsForTag(
    const std::string& tagId) const {
    std::lock_guard lock(impl_->mutex_);
    auto it = impl_->tagToAssets_.find(tagId);
    if (it != impl_->tagToAssets_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> TagDatabase::getAssetPathsWithAllTags(
    const std::vector<std::string>& tagIds) const {
    std::lock_guard lock(impl_->mutex_);
    
    if (tagIds.empty()) return {};
    
    // 最初のタグのアセットを基準
    std::vector<std::string> result = getAssetPathsForTag(tagIds[0]);
    
    // ほかのタグとの共通部分を抽出
    for (size_t i = 1; i < tagIds.size(); ++i) {
        auto tagAssets = getAssetPathsForTag(tagIds[i]);
        std::vector<std::string> temp;
        std::set_intersection(
            result.begin(), result.end(),
            tagAssets.begin(), tagAssets.end(),
            std::back_inserter(temp));
        result = std::move(temp);
    }
    
    return result;
}

std::vector<std::string> TagDatabase::getAssetPathsWithAnyTag(
    const std::vector<std::string>& tagIds) const {
    std::lock_guard lock(impl_->mutex_);
    
    std::vector<std::string> result;
    for (const auto& tagId : tagIds) {
        auto tagAssets = getAssetPathsForTag(tagId);
        result.insert(result.end(), tagAssets.begin(), tagAssets.end());
    }
    
    // 重複を削除
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    
    return result;
}

void TagDatabase::save(const QString& projectId) {
    std::lock_guard lock(impl_->mutex_);
    impl_->currentProjectId_ = projectId;
    
    // QSettingsで保存
    QSettings settings("Artifact", "AssetBrowser");
    settings.beginGroup("TagDatabase");
    settings.beginGroup(projectId);
    
    // タグを保存
    QJsonArray tagsArray;
    for (const auto& tag : impl_->tags_.tags) {
        QJsonObject tagObj;
        tagObj["id"] = QString::fromStdString(tag.id);
        tagObj["name"] = QString::fromStdString(tag.name);
        tagObj["color"] = tag.color.name();
        tagObj["description"] = QString::fromStdString(tag.description);
        tagObj["group"] = QString::fromStdString(tag.group);
        tagObj["order"] = tag.order;
        tagObj["isSystemTag"] = tag.isSystemTag;
        tagsArray.append(tagObj);
    }
    settings.setValue("tags", tagsArray);
    
    // assetToTags_を保存
    QJsonObject assetToTagsObj;
    for (const auto& [assetPath, tagIds] : impl_->assetToTags_) {
        QJsonArray tagIdsArray;
        for (const auto& tagId : tagIds) {
            tagIdsArray.append(QString::fromStdString(tagId));
        }
        assetToTagsObj[QString::fromStdString(assetPath)] = tagIdsArray;
    }
    settings.setValue("assetToTags", assetToTagsObj);
    
    settings.endGroup();
    settings.endGroup();
}

void TagDatabase::load(const QString& projectId) {
    std::lock_guard lock(impl_->mutex_);
    impl_->currentProjectId_ = projectId;
    impl_->tags_.tags.clear();
    impl_->tags_.nameToIndex.clear();
    impl_->assetToTags_.clear();
    impl_->tagToAssets_.clear();
    
    QSettings settings("Artifact", "AssetBrowser");
    settings.beginGroup("TagDatabase");
    settings.beginGroup(projectId);
    
    // タグを読み込み
    QJsonArray tagsArray = settings.value("tags").toJsonArray();
    for (const QJsonValue& tagVal : tagsArray) {
        QJsonObject tagObj = tagVal.toObject();
        AssetTag tag;
        tag.id = tagObj["id"].toString().toStdString();
        tag.name = tagObj["name"].toString().toStdString();
        tag.color = QColor(tagObj["color"].toString());
        tag.description = tagObj["description"].toString().toStdString();
        tag.group = tagObj["group"].toString().toStdString();
        tag.order = tagObj["order"].toInt();
        tag.isSystemTag = tagObj["isSystemTag"].toBool();
        impl_->tags_.addTag(tag);
    }
    
    // assetToTags_を読み込み
    QJsonObject assetToTagsObj = settings.value("assetToTags").toJsonObject();
    for (auto it = assetToTagsObj.constBegin(); it != assetToTagsObj.constEnd(); ++it) {
        QJsonArray tagIdsArray = it.value().toJsonArray();
        std::vector<std::string> tagIds;
        for (const QJsonValue& tagIdVal : tagIdsArray) {
            tagIds.push_back(tagIdVal.toString().toStdString());
        }
        impl_->assetToTags_[it.key().toStdString()] = tagIds;
    }
    
    // tagToAssets_を再構築
    for (const auto& [assetPath, tagIds] : impl_->assetToTags_) {
        for (const auto& tagId : tagIds) {
            impl_->tagToAssets_[tagId].push_back(assetPath);
        }
    }
    
    settings.endGroup();
    settings.endGroup();
}

void TagDatabase::clear() {
    std::lock_guard lock(impl_->mutex_);
    impl_->tags_.tags.clear();
    impl_->tags_.nameToIndex.clear();
    impl_->assetToTags_.clear();
    impl_->tagToAssets_.clear();
    impl_->notifyChange();
}

void TagDatabase::setChangeCallback(std::function<void()> callback) {
    impl_->changeCallback_ = callback;
}

} // namespace ArtifactCore
```

### 3. TagManager - タグ管理クラス

**新規ファイル**: `ArtifactCore/include/Asset/TagManager.ixx`

```cpp
module;
#include <string>
#include <vector>
#include <memory>

#include <QObject>
#include <QList>
#include <QStringList>

export module Asset.TagManager;

import Asset.Tag;
import Asset.TagDatabase;

namespace ArtifactCore {

/**
 * @brief タグ管理を担当する高レベルクラス
 *TagDatabaseとの連携、便利なメソッドを提供
 */
export class TagManager : public QObject {
    Q_OBJECT
public:
    static TagManager& instance();
    
    // タグ操作
    QString createTag(const QString& name, const QColor& color = QColor(),
                     const QString& group = QString(),
                     const QString& description = QString());
    bool updateTag(const QString& tagId, const QString& name, 
                  const QColor& color, const QString& group,
                  const QString& description);
    bool deleteTag(const QString& tagId);
    QList<AssetTag> getAllTags() const;
    QList<AssetTag> getTagsByGroup(const QString& group) const;
    AssetTag getTag(const QString& tagId) const;
    
    // タグ検索
    QList<AssetTag> findTagsByName(const QString& namePattern) const;
    
    // タグ取得（名前から）
    QString getTagIdByName(const QString& name) const;
    
    // アセット-タグ操作
    void assignTagToAsset(const QString& assetPath, const QString& tagId);
    void assignTagsToAsset(const QString& assetPath, 
                          const QStringList& tagIds);
    void unassignTagFromAsset(const QString& assetPath, const QString& tagId);
    void unassignAllTagsFromAsset(const QString& assetPath);
    QStringList getTagIdsForAsset(const QString& assetPath) const;
    QList<AssetTag> getTagsForAsset(const QString& assetPath) const;
    QStringList getAssetPathsForTag(const QString& tagId) const;
    
    // タグフィルタ
    QStringList getAssetPathsWithAllTags(const QStringList& tagIds) const;
    QStringList getAssetPathsWithAnyTag(const QStringList& tagIds) const;
    
    // タグの使用状況
    int getTagUsageCount(const QString& tagId) const;
    QList<AssetTag> getUnusedTags() const;
    
    // 保存/読み込み
    void save(const QString& projectId = "");
    void load(const QString& projectId = "");
    void clear();
    
    // カラー管理
    static QList<QColor> getDefaultTagColors();
    static QColor getColorForTag(const QString& tagId);
    
signals:
    void tagsChanged();
    void tagCreated(const QString& tagId);
    void tagDeleted(const QString& tagId);
    void tagAssigned(const QString& assetPath, const QString& tagId);
    void tagUnassigned(const QString& assetPath, const QString& tagId);
    
private:
    TagManager();
    ~TagManager();
    
    TagManager(const TagManager&) = delete;
    TagManager& operator=(const TagManager&) = delete;
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ArtifactCore
```

### 4. TagEditorWidget - タグ編集ウィジェット

**新規ファイル**: `Artifact/include/Widgets/Asset/TagEditorWidget.ixx`

```cpp
module;
#include <QWidget>
#include <memory>

#include <QStringList>

export module Widgets.Asset.TagEditorWidget;

namespace Artifact {

class TagEditorWidgetImpl;

/**
 * @brief アセットにタグを編集するウィジェット
 *タグの選択、追加、削除を提供
 */
export class TagEditorWidget : public QWidget {
    Q_OBJECT
    W_OBJECT(TagEditorWidget)

public:
    explicit TagEditorWidget(QWidget* parent = nullptr);
    ~TagEditorWidget();
    
    // アセットを設定
    void setAssetPath(const QString& assetPath);
    QString assetPath() const;
    
    // タグを設定
    void setTagIds(const QStringList& tagIds);
    QStringList tagIds() const;
    
    // 表示モード
    enum class DisplayMode { 
        MultiSelect,   // 複数選択可能
        SingleSelect,  // 単一選択
        ReadOnly       // 表示のみ
    };
    void setDisplayMode(DisplayMode mode);
    
    // カラー表示
    void setShowColors(bool show);
    
    // 通知
    void setTagChangedCallback(std::function<void(const QStringList&)> callback);
    
signals:
    void tagChanged(const QStringList& tagIds);
    void tagSelected(const QString& tagId);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void setupUi();
    void updateTagDisplay();
    void showTagMenu(const QPoint& pos);
    
    std::unique_ptr<TagEditorWidgetImpl> impl_;
};

} // namespace Artifact
```

### 5. TagFilterWidget - タグフィルタウィジェット

**新規ファイル**: `Artifact/include/Widgets/Asset/TagFilterWidget.ixx`

```cpp
module;
#include <QWidget>
#include <memory>

#include <QStringList>

export module Widgets.Asset.TagFilterWidget;

namespace Artifact {

class TagFilterWidgetImpl;

/**
 * @brief タグによるフィルタリングを行うウィジェット
 *タグの選択、フィルタモードの切り替え
 */
export class TagFilterWidget : public QWidget {
    Q_OBJECT
    W_OBJECT(TagFilterWidget)

public:
    enum class FilterMode {
        None,           // フィルタなし
        AllTags,        // 全てのタグを含む（AND）
        AnyTag,         // どれかのタグを含む（OR）
        ExcludeTags     // タグを含まない（NOT）
    };
    
    explicit TagFilterWidget(QWidget* parent = nullptr);
    ~TagFilterWidget();
    
    // フィルタモードを設定
    void setFilterMode(FilterMode mode);
    FilterMode filterMode() const;
    
    // 選択したタグを設定
    void setSelectedTagIds(const QStringList& tagIds);
    QStringList selectedTagIds() const;
    
    // 使用可能なタグを設定
    void setAvailableTags(const QStringList& tagIds);
    
    // 通知
    void setFilterChangedCallback(std::function<void(const QStringList&, FilterMode)> callback);
    
signals:
    void filterChanged(const QStringList& tagIds, FilterMode mode);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    
private:
    void setupUi();
    void updateDisplay();
    
    std::unique_ptr<TagFilterWidgetImpl> impl_;
};

} // namespace Artifact
```

### 6. TagCloudWidget - タグクラウドウィジェット

**新規ファイル**: `Artifact/include/Widgets/Asset/TagCloudWidget.ixx`

```cpp
module;
#include <QWidget>
#include <memory>

#include <QStringList>

export module Widgets.Asset.TagCloudWidget;

namespace Artifact {

class TagCloudWidgetImpl;

/**
 * @brief タグをクラウド形式で表示するウィジェット
 *使用頻度に応じてサイズを変化
 */
export class TagCloudWidget : public QWidget {
    Q_OBJECT
    W_OBJECT(TagCloudWidget)

public:
    explicit TagCloudWidget(QWidget* parent = nullptr);
    ~TagCloudWidget();
    
    // タグを設定
    void setTags(const QList<std::pair<QString, int>>& tagsWithCounts);
    
    // 表示オプション
    void setShowCounts(bool show);
    void setMinFontSize(int size);
    void setMaxFontSize(int size);
    void setColorMode(bool useCustomColors);
    
    // 通知
    void setTagClickedCallback(std::function<void(const QString&)> callback);
    void setTagHoveredCallback(std::function<void(const QString&)> callback);
    
signals:
    void tagClicked(const QString& tagId);
    void tagHovered(const QString& tagId);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void layoutTags();
    QRect tagRect(const QString& tagId) const;
    
    std::unique_ptr<TagCloudWidgetImpl> impl_;
};

} // namespace Artifact
```

### 7. TagManagementDialog - タグ管理ダイアログ

**新規ファイル**: `Artifact/include/Widgets/Asset/TagManagementDialog.ixx`

```cpp
module;
#include <QDialog>
#include <memory>

#include <QStringList>

export module Widgets.Asset.TagManagementDialog;

namespace Artifact {

class TagManagementDialogImpl;

/**
 * @brief タグの管理を行うダイアログ
 *タグの作成、編集、削除、グループ管理
 */
export class TagManagementDialog : public QDialog {
    Q_OBJECT
    W_OBJECT(TagManagementDialog)

public:
    explicit TagManagementDialog(QWidget* parent = nullptr);
    ~TagManagementDialog();
    
public slots:
    void accept() override;
    
private:
    void setupUi();
    void loadTags();
    void saveTags();
    void addNewTag();
    void editSelectedTag();
    void deleteSelectedTag();
    void updateTagList();
    
    std::unique_ptr<TagManagementDialogImpl> impl_;
};

} // namespace Artifact
```

### 8. AssetMenuModel への統合

**ファイル**: `Artifact/include/Asset/AssetMenuModel.ixx`

```cpp
// AssetMenuItemにタグフィールド追加
export struct AssetMenuItem {
    // ... 既存フィールド
    
    // タグ
    QStringList tagIds;        // 付与されたタグID
    QStringList tagNames;      // タグ名（表示用）
    
    // ... 既存のoperator==等
};
```

**ファイル**: `Artifact/src/Asset/AssetMenuModel.cppm`

```cpp
// Impl クラスにタグ管理追加
class AssetMenuModel::Impl {
    // ... 既存フィールド
    
public:
    void updateTagInformation();
};

// タグ情報を更新
void AssetMenuModel::Impl::updateTagInformation() {
    auto& tagManager = ArtifactCore::TagManager::instance();
    
    for (int i = 0; i < items_.size(); ++i) {
        auto& item = items_[i];
        QString path = QString::fromStdString(item.path.c_str());
        
        // タグIDを取得
        auto tagIds = tagManager.getTagIdsForAsset(path);
        item.tagIds = tagIds;
        
        // タグ名を取得
        item.tagNames.clear();
        for (const auto& tagId : tagIds) {
            auto tag = tagManager.getTag(tagId);
            if (!tag.id.isEmpty()) {
                item.tagNames.append(QString::fromStdString(tag.name));
            }
        }
    }
}

// タグによるフィルタリング
void AssetMenuModel::applyTagFilter(const QStringList& tagIds, 
                                   ArtifactCore::TagManager::FilterMode mode) {
    // タグでフィルタリング
    // 実装は省略
}
```

### 9. ArtifactAssetBrowser への統合

**ファイル**: `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`

```cpp
// Impl クラスにタグUI追加
class ArtifactAssetBrowser::Impl {
    // ... 既存フィールド
    
    // タグ機能
    TagEditorWidget* tagEditorWidget_ = nullptr;
    TagFilterWidget* tagFilterWidget_ = nullptr;
    TagCloudWidget* tagCloudWidget_ = nullptr;
    QAction* manageTagsAction_ = nullptr;
    QAction* toggleTagFilterAction_ = nullptr;
};

// タグ関連のUIをセットアップ
void ArtifactAssetBrowser::Impl::setupTagUi() {
    // タグエディターウィジェット
    tagEditorWidget_ = new TagEditorWidget();
    tagEditorWidget_->setShowColors(true);
    tagEditorWidget_->setTagChangedCallback([this](const QStringList& tagIds) {
        // 選択中のアセットにタグを割り当て
        QModelIndexList selected = fileView_->selectionModel()->selectedIndexes();
        if (!selected.isEmpty()) {
            for (const QModelIndex& index : selected) {
                int row = index.row();
                auto item = assetModel_->itemAt(row);
                QString path = QString::fromUtf8(item.path.c_str());
                
                auto& tagManager = ArtifactCore::TagManager::instance();
                tagManager.assignTagsToAsset(path, tagIds);
            }
        }
    });
    
    // タグフィルターウィジェット
    tagFilterWidget_ = new TagFilterWidget();
    tagFilterWidget_->setFilterChangedCallback(
        [this](const QStringList& tagIds, TagFilterWidget::FilterMode mode) {
        // タグでフィルタリング
        applyTagFilter(tagIds, mode);
    });
    
    // タグクラウドウィジェット
    tagCloudWidget_ = new TagCloudWidget();
    tagCloudWidget_->setShowCounts(true);
    tagCloudWidget_->setTagClickedCallback([this](const QString& tagId) {
        // タグをクリックでフィルタ
        tagFilterWidget_->setSelectedTagIds({tagId});
        tagFilterWidget_->setFilterMode(TagFilterWidget::FilterMode::AnyTag);
    });
    
    // コンテキストメニューにアクション追加
    manageTagsAction_ = new QAction(tr("Manage Tags..."), this);
    connect(manageTagsAction_, &QAction::triggered, [this]() {
        TagManagementDialog dialog(self_);
        dialog.exec();
    });
    
    toggleTagFilterAction_ = new QAction(tr("Show Tag Filter"), this);
    toggleTagFilterAction_->setCheckable(true);
    connect(toggleTagFilterAction_, &QAction::toggled, 
            [this](bool checked) {
        tagFilterWidget_->setVisible(checked);
    });
}

// タグフィルタを適用
void ArtifactAssetBrowser::Impl::applyTagFilter(
    const QStringList& tagIds, 
    TagFilterWidget::FilterMode mode) {
    
    auto& tagManager = ArtifactCore::TagManager::instance();
    QStringList filteredPaths;
    
    switch (mode) {
        case TagFilterWidget::FilterMode::None:
            break;
        case TagFilterWidget::FilterMode::AllTags:
            filteredPaths = tagManager.getAssetPathsWithAllTags(tagIds);
            break;
        case TagFilterWidget::FilterMode::AnyTag:
            filteredPaths = tagManager.getAssetPathsWithAnyTag(tagIds);
            break;
        case TagFilterWidget::FilterMode::ExcludeTags:
            // 実装は省略
            break;
    }
    
    // フィルタリング結果を適用
    // 実装は省略
}

// 選択が変更された時にタグエディタを更新
void ArtifactAssetBrowser::Impl::onSelectionChanged() {
    QModelIndexList selected = fileView_->selectionModel()->selectedIndexes();
    
    if (selected.isEmpty()) {
        tagEditorWidget_->setAssetPath("");
        tagEditorWidget_->setTagIds({});
        return;
    }
    
    if (selected.size() == 1) {
        int row = selected.first().row();
        auto item = assetModel_->itemAt(row);
        QString path = QString::fromUtf8(item.path.c_str());
        
        tagEditorWidget_->setAssetPath(path);
        tagEditorWidget_->setTagIds(item.tagIds);
    } else {
        // 複数選択の場合
        QStringList commonTags;
        QString firstPath;
        
        for (int i = 0; i < selected.size(); ++i) {
            int row = selected[i].row();
            auto item = assetModel_->itemAt(row);
            QString path = QString::fromUtf8(item.path.c_str());
            
            if (i == 0) {
                firstPath = path;
                commonTags = item.tagIds;
            } else {
                // 共通タグを抽出
                QStringList itemTags = item.tagIds;
                QStringList temp;
                std::set_intersection(
                    commonTags.begin(), commonTags.end(),
                    itemTags.begin(), itemTags.end(),
                    std::back_inserter(temp));
                commonTags = temp;
            }
        }
        
        tagEditorWidget_->setAssetPath("");  // 複数選択
        tagEditorWidget_->setTagIds(commonTags);
    }
}

// タグが変更された時に呼ばれる
void ArtifactAssetBrowser::Impl::onTagsChanged() {
    // タグ情報を更新
    assetModel_->updateTagInformation();
    
    // タグクラウドを更新
    auto& tagManager = ArtifactCore::TagManager::instance();
    auto allTags = tagManager.getAllTags();
    
    QList<std::pair<QString, int>> tagsWithCounts;
    for (const auto& tag : allTags) {
        int count = tagManager.getTagUsageCount(QString::fromStdString(tag.id));
        tagsWithCounts.emplace_back(
            QString::fromStdString(tag.id),
            count
        );
    }
    
    tagCloudWidget_->setTags(tagsWithCounts);
}
```

### 10. イベント定義

**新規ファイル**: `ArtifactCore/include/Event/AssetTagsChangedEvent.ixx`

```cpp
module;
#include <string>
#include <vector>

#include <QStringList>

export module Event.AssetTagsChangedEvent;

import Core.EventBus.Event;

namespace ArtifactCore {

// タグ変更イベント
struct AssetTagsChangedEvent : Event {
    std::vector<std::string> assetPaths;  // 変更されたアセット
    enum class ChangeType { 
        TagAdded, 
        TagRemoved, 
        AllTagsCleared,
        TagRenamed,
        TagDeleted
    };
    ChangeType changeType;
    std::string tagId;  // 変更されたタグ
};

// タグフィルタ変更イベント
struct TagFilterChangedEvent : Event {
    std::vector<std::string> tagIds;
    bool useAndMode;  // true: AND, false: OR
};

// タグマネージャー更新イベント
struct TagManagerUpdatedEvent : Event {
    enum class UpdateType {
        TagsLoaded,
        TagsSaved,
        TagsCleared
    };
    UpdateType updateType;
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

### Phase 1: データ構造とデータベース (1日) - **P0**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetTag` 構造体定義 | P0 | 1h | なし | ✅ |
| `TagCollection` 構造体定義 | P0 | 1h | 上記 | ✅ |
| `AssetTagAssignment` 構造体定義 | P0 | 0.5h | 上記 | ✅ |
| `TagDatabase` クラス設計 | P0 | 1h | 上記 | ✅ |
| `TagDatabase::Impl` 実装 | P0 | 3h | 上記 | ✅ |
| 保存/読み込み（QSettings） | P0 | 2h | 上記 | ✅ |

### Phase 2: タグ管理クラス (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `TagManager` クラス設計 | P1 | 1h | Phase 1 | ✅ |
| `TagManager::Impl` 実装 | P1 | 2h | 上記 | ✅ |
| 便利メソッド実装 | P1 | 2h | 上記 | ✅ |
| シグナル/スロット追加 | P1 | 1h | 上記 | ✅ |

### Phase 3: UIコンポーネント (1日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `TagEditorWidget` クラス設計 | P1 | 1h | Phase 2 | ✅ |
| `TagEditorWidget::Impl` 実装 | P1 | 2h | 上記 | ✅ |
| `TagFilterWidget` クラス設計 | P1 | 1h | Phase 2 | ✅ |
| `TagFilterWidget::Impl` 実装 | P1 | 2h | 上記 | ✅ |
| `TagCloudWidget` クラス設計 | P2 | 1h | Phase 2 | ✅ |
| `TagCloudWidget::Impl` 実装 | P2 | 2h | 上記 | ✅ |
| `TagManagementDialog` クラス設計 | P1 | 1h | Phase 2 | ✅ |
| `TagManagementDialog::Impl` 実装 | P1 | 2h | 上記 | ✅ |

### Phase 4: AssetMenuModel 統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| `AssetMenuItem` にタグフィールド追加 | P1 | 1h | Phase 1 | ❌ |
| `AssetMenuModel::Impl` にタグ管理追加 | P1 | 2h | 上記 | ❌ |
| タグ情報更新メソッド | P1 | 2h | 上記 | ❌ |
| タグフィルタリング | P1 | 2h | 上記 | ❌ |

### Phase 5: AssetBrowser UI統合 (0.5日) - **P1**
| タスク | 優先度 | 推定時間 | 依存 | 並行可能 |
|---|---|---|---|---|
| タグUIコンポーネント配置 | P1 | 2h | Phase 3 | ❌ (UIスレッド) |
| コンテキストメニューにアクション追加 | P1 | 1h | Phase 3 | ❌ (UIスレッド) |
| 選択変更時のタグエディタ更新 | P1 | 1h | Phase 4 | ❌ (UIスレッド) |
| タグ変更時のUI更新 | P1 | 1h | Phase 4 | ❌ (UIスレッド) |

### 並行作業可能性
- **Phase 1 (Core)**: 独立して並行可能
- **Phase 2 (Manager)**: Phase 1完了後、独立して並行可能
- **Phase 3 (UI)**: Phase 2完了後、独立して並行可能
- **Phase 4 (Model)**: Phase 1完了後、独立して並行可能
- **Phase 5 (UI Integration)**: UIスレッド依存のため、基本的には直列実施

---

## テスト戦略

### 1. 自動テスト (Unit Tests)

#### P0 - 必須テスト (全てパスすること)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `AssetTag` 構造体の比較 | 自動 | 正しく比較 | 0.5h |
| `TagCollection::addTag` | 自動 | 正しく追加 | 1h |
| `TagCollection::removeTag` | 自動 | 正しく削除 | 1h |
| `TagCollection::findTagById` | 自動 | 正しく検索 | 0.5h |
| `TagDatabase::createTag` | 自動 | 正しく作成 | 1h |
| `TagDatabase::deleteTag` | 自動 | 正しく削除 | 1h |
| `TagDatabase::assignTagToAsset` | 自動 | 正しく関連付け | 1h |
| `TagDatabase::getTagIdsForAsset` | 自動 | 正しく取得 | 1h |

#### P1 - 主要テスト (可能な限りパス)
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| `TagDatabase::save`/`load` | 自動 | 正しく保存/復元 | 2h |
| `TagManager::getAllTags` | 自動 | 正しく取得 | 1h |
| `TagManager::getTagsForAsset` | 自動 | 正しく取得 | 1h |
| `TagManager::getAssetPathsWithAllTags` | 自動 | 正しくフィルタ | 1h |
| `TagManager::getAssetPathsWithAnyTag` | 自動 | 正しくフィルタ | 1h |

#### P2 - 統合テスト
| テスト項目 | 種別 | 判定基準 | 推定時間 |
|---|---|---|---|
| AssetMenuModelのタグ統合 | 手動 | 正しく表示 | 2h |
| TagEditorWidgetの操作 | 手動 | 正しく編集 | 2h |
| TagFilterWidgetの操作 | 手動 | 正しくフィルタ | 2h |
| TagCloudWidgetの表示 | 手動 | 正しく表示 | 1h |
| TagManagementDialogの操作 | 手動 | 正しく管理 | 2h |

### 2. 手動テスト (Manual Tests)

#### UXテスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| タグ編集の操作性 | 直感的で分かりやすい | 2h |
| タグフィルタの使いやすさ | 分かりやすい | 2h |
| タグクラウドの表示 | 分かりやすい | 1h |
| タグ管理ダイアログ | 分かりやすい | 2h |
| タグのカラー表示 | 分かりやすい | 0.5h |

#### 視覚的テスト
| テスト項目 | 判定基準 | 推定時間 |
|---|---|---|
| タグ付きアセットの表示 | 明確で分かりやすい | 1h |
| タグクラウドのレイアウト | 見やすい | 1h |
| タグフィルタの表示 | 分かりやすい | 0.5h |
| タグの色分け | 分かりやすい | 0.5h |

### 3. テスト実行計画

#### Phase 1: Unit Tests (Data Structures + Database)
```
日数: 1日
対象: AssetTag, TagCollection, TagDatabase
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 2: Unit Tests (Manager)
```
日数: 0.5日
対象: TagManager
方法: Google Test / Catch2
実行: CIパイプラインで自動実行
```

#### Phase 3: UI Tests (Components)
```
日数: 1日
対象: TagEditorWidget, TagFilterWidget, TagCloudWidget, TagManagementDialog
方法: 手動テスト
実行: QAチーム
```

#### Phase 4: Integration Tests
```
日数: 1日
対象: AssetMenuModel + AssetBrowser 統合
方法: 手動テスト
実行: QAチーム
```

### 4. テスト完了基準
- [ ] P0全てのテストがパス
- [ ] P1の80%以上のテストがパス
- [ ] 重大なバグなし
- [ ] タグ付与が正しく動作
- [ ] タグフィルタリングが正しく動作
- [ ] タグ管理が正しく動作
- [ ] UIが直感的で使いやすい
- [ ] 全てのテスト項目がパス

---

## 成果物

1. **コア機能**: タグシステム
2. **API**: `TagDatabase`, `TagManager` クラス
3. **UIコンポーネント**: `TagEditorWidget`, `TagFilterWidget`, `TagCloudWidget`, `TagManagementDialog`
4. **データ構造**: `AssetTag`, `TagCollection`, `AssetTagAssignment`
5. **統合**: AssetMenuModel + AssetBrowser への完全統合
6. **イベントシステム**: タグ変更イベント

---

## 依存関係

### 必要な前提条件
- **M-AB (Asset Browser 基盤)** - 基本的なAsset Browser機能
- **M-AB-11 (Advanced Sort)** - 高度なソート機能（オプション）
- **`AssetMenuItem`** - アイテム構造
- **`AssetMenuModel`** - アイテム表示モデル

### 連携する機能
- `TagDatabase` - タグデータベース
- `TagManager` - タグ管理
- `AssetMenuModel` - アイテムモデル
- `ArtifactAssetBrowser` - ホストアプリケーション
- 各種タグUIウィジェット

### 影響を受ける機能
- アセットリストの表示
- フィルタリング機能
- ソート機能

---

## リスクと対策

### リスク1: パフォーマンス
**内容**: 多数のタグ/アセットがある場合のパフォーマンス低下
**対策**:
- タグのIndexを維持
- 遅延ロードを実装
- 変更通知をバッチ処理
- 使用頻度の低いタグを非同期でロード

### リスク2: タグの一意性
**内容**: 同名のタグが複数作成される
**対策**:
- タグ名の一意性を強制
- 同名タグ作成時に警告
- 大文字/小文字を区別しない比較

### リスク3: タグの同期
**内容**: 複数のユーザーやデバイス間でのタグの同期
**対策**:
- プロジェクトごとにタグを管理
- 将来的にはクラウド同期を実装
- 競合解決のメカニズムを提供

### リスク4: タグの削除
**内容**: タグを削除した際のアセットとの関連付け
**対策**:
- タグ削除前に確認ダイアログ
- 使用中のタグは削除不可
- タグ削除時にアセットからの関連付けも削除

---

## テスト項目

- [ ] タグの作成、編集、削除
- [ ] アセットに対するタグの付与/削除
- [ ] 複数アセットに対する一括タグ付与
- [ ] タグによるフィルタリング（AND, OR, NOT）
- [ ] タグクラウドの表示
- [ ] タグの保存/読み込み
- [ ] タグの色分け表示
- [ ] タググループの管理
- [ ] タグの検索
- [ ] 性能テスト（1000+アセット、100+タグ）

---

## 完了基準

- [ ] タグの作成、編集、削除が正しく動作
- [ ] アセットに対するタグの付与/削除が正しく動作
- [ ] タグフィルタリングが正しく動作
- [ ] タグクラウドが正しく表示
- [ ] タグの保存/読み込みが正しく動作
- [ ] UIが直感的で使いやすい
- [ ] パフォーマンスに問題なし
- [ ] 全てのテスト項目がパス
- [ ] UXテストで問題なし
- [ ] ドキュメントが更新

---

## 関連文書

- `Artifact/src/Asset/AssetMenuModel.cppm` - アイテムモデル
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm` - Asset Browser
- `Artifact/include/Asset/AssetMenuModel.ixx` - アイテム構造
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md` - 元のロードマップ
- `docs/planned/MILESTONES_BACKLOG.md` - マイルストーンバックログ

---

## メモ

- タグシステムはアセット管理の柔軟性を大幅に向上させる
- LightroomやPhotoshopのタグシステムと類似のUXを目指す
- 将来的には、タグの自動付与（AI支援）を実装
- タグの階層化（親子関係）も検討
- タグのエイリアス（別名）機能の追加
- タグのマージ機能
- タグのエクスポート/インポート（CSV等）

## 2026-07-25 実装監査

- `AssetTag`／`TagManager`／`TagDatabase` の専用モジュール、AssetMenuModel へのタグ割り当て、TagEditor／TagFilter／TagCloud UI、タグ変更イベントは確認できない。
- `ArtifactAssetBrowser` には既存のカテゴリ・検索・お気に入り・メタデータ処理はあるが、複数タグ割り当て、AND フィルタ、階層グループ、タグ永続化／入出力の経路は存在しない。
- よって本マイルストーンは Planned／未着手の判定を維持する。最初にタグデータモデルとプロジェクト単位の永続化契約を追加する必要がある。