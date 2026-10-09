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
