#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QTemporaryDir>
#include <QString>
#include <QUuid>

#include <utility>

import Asset.Database;
import AssetType;

using namespace ArtifactCore;

namespace {

bool createSourceFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("asset") == 5;
}

class RegisteredAssetGuard {
public:
    RegisteredAssetGuard(AssetDatabase& database, QUuid id)
        : database_(database), id_(std::move(id))
    {
    }

    ~RegisteredAssetGuard()
    {
        if (!id_.isNull()) {
            database_.unregisterAsset(id_);
        }
    }

private:
    AssetDatabase& database_;
    QUuid id_;
};

class DatabaseSnapshotGuard {
public:
    explicit DatabaseSnapshotGuard(AssetDatabase& database)
        : database_(database), snapshot_(database.toJson())
    {
    }

    ~DatabaseSnapshotGuard()
    {
        database_.fromJson(snapshot_);
    }

private:
    AssetDatabase& database_;
    const QByteArray snapshot_;
};

} // namespace

TEST(AssetDatabaseContractTest, CanonicalAliasesSharePreferredIdentity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(QDir(directory.path()).mkpath(QStringLiteral("nested")));
    const QString path = directory.filePath(QStringLiteral("still.png"));
    ASSERT_TRUE(createSourceFile(path));
    const QString alias = directory.filePath(QStringLiteral("nested/../still.png"));

    AssetDatabase& database = AssetDatabase::instance();
    const QUuid preferredId = QUuid::createUuid();
    const QUuid registered = database.registerAsset(path, AssetType::Image, preferredId);
    ASSERT_FALSE(registered.isNull());
    RegisteredAssetGuard cleanup(database, registered);

    EXPECT_EQ(registered, preferredId);
    EXPECT_EQ(database.registerAsset(alias, AssetType::Image), registered);
    EXPECT_EQ(database.findAssetByPath(alias), registered);
    EXPECT_EQ(database.getAssetInfo(registered).absolutePath,
              QFileInfo(path).canonicalFilePath());
    EXPECT_TRUE(database.registerAsset(QString(), AssetType::Image).isNull());
    EXPECT_TRUE(database.registerAsset(path, AssetType::Unknown).isNull());
}

TEST(AssetDatabaseContractTest, MissingSourceCanBeRegisteredByAbsolutePath)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString missingPath = directory.filePath(QStringLiteral("offline.exr"));
    ASSERT_FALSE(QFileInfo::exists(missingPath));

    AssetDatabase& database = AssetDatabase::instance();
    const QUuid id = database.registerAsset(missingPath, AssetType::Image);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(database, id);

    const AssetInfo info = database.getAssetInfo(id);
    EXPECT_EQ(info.absolutePath, QFileInfo(missingPath).absoluteFilePath());
    EXPECT_EQ(info.name, QStringLiteral("offline.exr"));
    EXPECT_EQ(database.findAssetByPath(missingPath), id);
}

TEST(AssetDatabaseContractTest, RelinkPreservesIdentityAndRejectsCollisionsAtomically)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString oldPath = directory.filePath(QStringLiteral("old.png"));
    const QString occupiedPath = directory.filePath(QStringLiteral("occupied.png"));
    const QString newPath = directory.filePath(QStringLiteral("renamed.png"));
    ASSERT_TRUE(createSourceFile(oldPath));
    ASSERT_TRUE(createSourceFile(occupiedPath));

    AssetDatabase& database = AssetDatabase::instance();
    const QUuid oldId = database.registerAsset(oldPath, AssetType::Image);
    RegisteredAssetGuard cleanupOld(database, oldId);
    const QUuid occupiedId = database.registerAsset(occupiedPath, AssetType::Image);
    RegisteredAssetGuard cleanupOccupied(database, occupiedId);
    ASSERT_FALSE(oldId.isNull());
    ASSERT_FALSE(occupiedId.isNull());

    EXPECT_FALSE(database.relinkAssetPath(oldPath, occupiedPath));
    EXPECT_FALSE(database.relinkAssetPath(oldPath, oldPath));
    EXPECT_FALSE(database.relinkAssetPath(QStringLiteral("missing.png"), newPath));
    EXPECT_EQ(database.findAssetByPath(oldPath), oldId);
    EXPECT_EQ(database.findAssetByPath(occupiedPath), occupiedId);

    EXPECT_TRUE(database.relinkAssetPath(oldPath, newPath));
    EXPECT_TRUE(database.findAssetByPath(oldPath).isNull());
    EXPECT_EQ(database.findAssetByPath(newPath), oldId);
    const AssetInfo relinked = database.getAssetInfo(oldId);
    EXPECT_EQ(relinked.absolutePath, QFileInfo(newPath).absoluteFilePath());
    EXPECT_EQ(relinked.name, QStringLiteral("renamed.png"));
}

TEST(AssetDatabaseContractTest, RelinkToCanonicalAliasOfCurrentPathIsRejected)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(QDir(directory.path()).mkpath(QStringLiteral("nested")));
    const QString path = directory.filePath(QStringLiteral("same.png"));
    ASSERT_TRUE(createSourceFile(path));
    const QString alias = directory.filePath(QStringLiteral("nested/../same.png"));

    AssetDatabase& database = AssetDatabase::instance();
    const QUuid id = database.registerAsset(path, AssetType::Image);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(database, id);
    const AssetInfo before = database.getAssetInfo(id);

    EXPECT_FALSE(database.relinkAssetPath(path, alias));
    EXPECT_EQ(database.findAssetByPath(path), id);
    EXPECT_EQ(database.findAssetByPath(alias), id);
    EXPECT_EQ(database.getAssetInfo(id).absolutePath, before.absolutePath);
    EXPECT_EQ(database.getAssetInfo(id).name, before.name);
}

TEST(AssetDatabaseContractTest, JsonLoadFiltersDuplicateAndMalformedEntries)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("loaded.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    const QUuid id = QUuid::createUuid();
    const QUuid duplicateId = QUuid::createUuid();
    QJsonObject metadata;
    metadata.insert(QStringLiteral("workingColorSpace"), QStringLiteral("ACEScg"));
    metadata.insert(QStringLiteral("unsupportedNumber"), 42);
    const QJsonObject entry{
        {QStringLiteral("id"), id.toString()},
        {QStringLiteral("name"), QString()},
        {QStringLiteral("path"), path},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("metadata"), metadata},
    };
    const QJsonObject duplicatePath{
        {QStringLiteral("id"), duplicateId.toString()},
        {QStringLiteral("name"), QStringLiteral("duplicate path")},
        {QStringLiteral("path"), path},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
    };
    const QJsonObject duplicateIdEntry{
        {QStringLiteral("id"), id.toString()},
        {QStringLiteral("name"), QStringLiteral("duplicate ID")},
        {QStringLiteral("path"), directory.filePath(QStringLiteral("other.png"))},
        {QStringLiteral("type"), static_cast<int>(AssetType::Audio)},
    };
    const QJsonObject invalidType{
        {QStringLiteral("id"), QUuid::createUuid().toString()},
        {QStringLiteral("path"), directory.filePath(QStringLiteral("invalid.bin"))},
        {QStringLiteral("type"), 0},
    };
    const QByteArray payload = QJsonDocument(QJsonArray{
        entry, duplicatePath, duplicateIdEntry, invalidType,
        QJsonValue(QStringLiteral("not an object"))}).toJson(QJsonDocument::Compact);

    ASSERT_TRUE(database.fromJson(payload));
    ASSERT_EQ(database.allAssets().size(), 1);
    const AssetInfo loaded = database.getAssetInfo(id);
    EXPECT_EQ(loaded.id, id);
    EXPECT_EQ(loaded.absolutePath, QFileInfo(path).canonicalFilePath());
    EXPECT_EQ(loaded.name, QStringLiteral("loaded.png"));
    EXPECT_EQ(loaded.type, AssetType::Image);
    EXPECT_EQ(loaded.metadata.value(QStringLiteral("workingColorSpace")),
              QStringLiteral("ACEScg"));
    EXPECT_FALSE(loaded.metadata.contains(QStringLiteral("unsupportedNumber")));

    const QByteArray wrongTopLevel =
        QJsonDocument(QJsonObject{{QStringLiteral("assets"), QJsonArray{}}})
            .toJson(QJsonDocument::Compact);
    EXPECT_FALSE(database.fromJson(wrongTopLevel));
    EXPECT_EQ(database.getAssetInfo(id).absolutePath, loaded.absolutePath);

    const QByteArray malformed = QByteArrayLiteral("{");
    EXPECT_FALSE(database.fromJson(malformed));
    EXPECT_EQ(database.getAssetInfo(id).absolutePath, loaded.absolutePath);
}

TEST(AssetDatabaseContractTest, JsonSnapshotRoundTripPreservesIdentityAndStringMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("roundtrip.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    const QUuid id = QUuid::createUuid();
    const QJsonObject entry{
        {QStringLiteral("id"), id.toString()},
        {QStringLiteral("name"), QStringLiteral("roundtrip.png")},
        {QStringLiteral("path"), path},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("metadata"), QJsonObject{
            {QStringLiteral("workingColorSpace"), QStringLiteral("ACEScg")},
            {QStringLiteral("proxyState"), QStringLiteral("ready")},
        }},
    };
    ASSERT_TRUE(database.fromJson(
        QJsonDocument(QJsonArray{entry}).toJson(QJsonDocument::Compact)));

    const QByteArray snapshot = database.toJson();
    ASSERT_FALSE(snapshot.isEmpty());
    database.clear();
    ASSERT_TRUE(database.fromJson(snapshot));

    ASSERT_EQ(database.allAssets().size(), 1);
    const AssetInfo restored = database.getAssetInfo(id);
    EXPECT_EQ(restored.id, id);
    EXPECT_EQ(restored.type, AssetType::Image);
    EXPECT_EQ(restored.absolutePath, QFileInfo(path).canonicalFilePath());
    EXPECT_EQ(restored.metadata.value(QStringLiteral("workingColorSpace")),
              QStringLiteral("ACEScg"));
    EXPECT_EQ(restored.metadata.value(QStringLiteral("proxyState")),
              QStringLiteral("ready"));
}

TEST(AssetDatabaseContractTest, PreferredUuidCollisionAllocatesDistinctIdentity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first.png"));
    const QString secondPath = directory.filePath(QStringLiteral("second.png"));
    ASSERT_TRUE(createSourceFile(firstPath));
    ASSERT_TRUE(createSourceFile(secondPath));

    AssetDatabase& database = AssetDatabase::instance();
    const QUuid preferredId = QUuid::createUuid();
    const QUuid firstId = database.registerAsset(firstPath, AssetType::Image, preferredId);
    RegisteredAssetGuard cleanupFirst(database, firstId);
    const QUuid secondId = database.registerAsset(secondPath, AssetType::Audio, preferredId);
    RegisteredAssetGuard cleanupSecond(database, secondId);

    ASSERT_FALSE(firstId.isNull());
    ASSERT_FALSE(secondId.isNull());
    EXPECT_EQ(firstId, preferredId);
    EXPECT_NE(secondId, firstId);
    EXPECT_EQ(database.findAssetByPath(firstPath), firstId);
    EXPECT_EQ(database.findAssetByPath(secondPath), secondId);
}

TEST(AssetDatabaseContractTest, UnregisterIsIdempotentAndAllowsPathReuse)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("reusable.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetDatabase& database = AssetDatabase::instance();
    const QUuid id = database.registerAsset(path, AssetType::Image);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(database, id);
    database.unregisterAsset(id);
    database.unregisterAsset(id);

    EXPECT_TRUE(database.findAssetByPath(path).isNull());
    EXPECT_TRUE(database.getAssetInfo(id).id.isNull());
    const QUuid replacement = database.registerAsset(path, AssetType::Image);
    EXPECT_FALSE(replacement.isNull());
    EXPECT_EQ(database.findAssetByPath(path), replacement);
    if (replacement != id) database.unregisterAsset(replacement);
}

TEST(AssetDatabaseContractTest, FindByTypeReturnsOnlyMatchingAssets)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("image.png"));
    const QString audioPath = directory.filePath(QStringLiteral("audio.wav"));
    const QString secondImagePath = directory.filePath(QStringLiteral("second.png"));
    ASSERT_TRUE(createSourceFile(imagePath));
    ASSERT_TRUE(createSourceFile(audioPath));
    ASSERT_TRUE(createSourceFile(secondImagePath));

    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    database.clear();
    const QUuid imageId = database.registerAsset(imagePath, AssetType::Image);
    RegisteredAssetGuard cleanupImage(database, imageId);
    const QUuid audioId = database.registerAsset(audioPath, AssetType::Audio);
    RegisteredAssetGuard cleanupAudio(database, audioId);
    const QUuid secondImageId = database.registerAsset(secondImagePath, AssetType::Image);
    RegisteredAssetGuard cleanupSecondImage(database, secondImageId);

    const QList<AssetInfo> images = database.findAssetsByType(AssetType::Image);
    ASSERT_EQ(images.size(), 2);
    EXPECT_EQ(images[0].type, AssetType::Image);
    EXPECT_EQ(images[1].type, AssetType::Image);
    const QList<AssetInfo> scripts = database.findAssetsByType(AssetType::Script);
    EXPECT_TRUE(scripts.isEmpty());
}

TEST(AssetDatabaseContractTest, ClearDropsBothIndexesAndDatabaseCanBeReused)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("before.png"));
    const QString secondPath = directory.filePath(QStringLiteral("after.png"));
    ASSERT_TRUE(createSourceFile(firstPath));
    ASSERT_TRUE(createSourceFile(secondPath));

    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    const QUuid firstId = database.registerAsset(firstPath, AssetType::Image);
    ASSERT_FALSE(firstId.isNull());
    database.clear();
    EXPECT_TRUE(database.allAssets().isEmpty());
    EXPECT_TRUE(database.findAssetByPath(firstPath).isNull());

    const QUuid secondId = database.registerAsset(secondPath, AssetType::Audio);
    ASSERT_FALSE(secondId.isNull());
    EXPECT_EQ(database.findAssetByPath(secondPath), secondId);
}

TEST(AssetDatabaseContractTest, EmptyJsonArrayReplacesCurrentDatabase)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("replace.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    const QUuid id = database.registerAsset(path, AssetType::Image);
    ASSERT_FALSE(id.isNull());
    const QByteArray emptyArray = QJsonDocument(QJsonArray{}).toJson(QJsonDocument::Compact);
    ASSERT_TRUE(database.fromJson(emptyArray));
    EXPECT_TRUE(database.allAssets().isEmpty());
    EXPECT_TRUE(database.findAssetByPath(path).isNull());
}

TEST(AssetDatabaseContractTest, CanonicalAliasRegistrationKeepsOriginalTypeAndIdentity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(QDir(directory.path()).mkpath(QStringLiteral("nested")));
    const QString path = directory.filePath(QStringLiteral("asset.png"));
    ASSERT_TRUE(createSourceFile(path));
    const QString alias = directory.filePath(QStringLiteral("nested/../asset.png"));

    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    database.clear();
    const QUuid preferredId = QUuid::createUuid();
    const QUuid firstId = database.registerAsset(path, AssetType::Image, preferredId);
    ASSERT_FALSE(firstId.isNull());
    const QUuid secondPreferredId = QUuid::createUuid();
    const QUuid aliasId = database.registerAsset(alias, AssetType::Audio, secondPreferredId);

    EXPECT_EQ(aliasId, firstId);
    EXPECT_NE(aliasId, secondPreferredId);
    EXPECT_EQ(database.getAssetInfo(firstId).type, AssetType::Image);
    EXPECT_EQ(database.allAssets().size(), 1);
}

TEST(AssetDatabaseContractTest, MissingUuidLookupAndUnregisterLeaveDatabaseUnchanged)
{
    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    database.clear();
    const QUuid missingId = QUuid::createUuid();

    const AssetInfo missing = database.getAssetInfo(missingId);
    EXPECT_TRUE(missing.id.isNull());
    EXPECT_TRUE(missing.absolutePath.isEmpty());
    database.unregisterAsset(missingId);
    EXPECT_TRUE(database.allAssets().isEmpty());
}

TEST(AssetDatabaseContractTest, WhitespaceOnlyPathsAreRejectedWithoutChangingIndexes)
{
    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    database.clear();

    EXPECT_TRUE(database.registerAsset(QStringLiteral(" \t\n "), AssetType::Image).isNull());
    EXPECT_TRUE(database.allAssets().isEmpty());
    EXPECT_TRUE(database.findAssetByPath(QStringLiteral(" \t\n ")).isNull());
}

TEST(AssetDatabaseContractTest, InvalidJsonDoesNotReplaceExistingDatabase)
{
    AssetDatabase& database = AssetDatabase::instance();
    DatabaseSnapshotGuard restore(database);
    database.clear();

    const QUuid id = database.registerAsset(
        QStringLiteral("preserved.asset"), AssetType::Image);
    ASSERT_FALSE(id.isNull());
    const QByteArray before = database.toJson();

    EXPECT_FALSE(database.fromJson(QByteArrayLiteral("{")));
    EXPECT_EQ(database.toJson(), before);
    EXPECT_EQ(database.findAssetByPath(QStringLiteral("preserved.asset")), id);

    EXPECT_FALSE(database.fromJson(QByteArrayLiteral("{}")));
    EXPECT_EQ(database.toJson(), before);
    EXPECT_EQ(database.findAssetByPath(QStringLiteral("preserved.asset")), id);
}
