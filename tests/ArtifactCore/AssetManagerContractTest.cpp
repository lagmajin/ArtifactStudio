#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QIODevice>
#include <QUuid>
#include <QTemporaryDir>
#include <QString>

#include <cstdint>
#include <utility>

import Asset.Database;
import Asset.Manager;
import AssetType;
import Memory.SharedPtr;

using namespace ArtifactCore;

namespace {

class RegisteredAssetGuard {
public:
    explicit RegisteredAssetGuard(QUuid assetId) : assetId_(std::move(assetId)) {}
    ~RegisteredAssetGuard()
    {
        if (!assetId_.isNull()) {
            AssetDatabase::instance().unregisterAsset(assetId_);
        }
    }

private:
    QUuid assetId_;
};

class AssetDatabaseSnapshotGuard {
public:
    explicit AssetDatabaseSnapshotGuard(AssetDatabase& database)
        : database_(database), snapshot_(database.toJson())
    {
    }

    ~AssetDatabaseSnapshotGuard()
    {
        database_.fromJson(snapshot_);
    }

private:
    AssetDatabase& database_;
    const QByteArray snapshot_;
};

bool createSourceFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("source") == 6;
}

} // namespace

TEST(AssetManagerContractTest, RepeatedAcquireSharesIdentityAndBalancesUseCount)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("still.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid first = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(first.isNull());
    RegisteredAssetGuard cleanup(first);
    const QUuid second = manager.acquireSource(path, AssetType::Image);

    EXPECT_EQ(second, first);
    EXPECT_EQ(manager.sourceId(path), first);
    EXPECT_EQ(manager.useCount(first), 2);
    EXPECT_TRUE(manager.releaseSource(first));
    EXPECT_EQ(manager.useCount(first), 1);
    EXPECT_TRUE(manager.releaseSource(second));
    EXPECT_EQ(manager.useCount(first), 0);
    EXPECT_FALSE(manager.releaseSource(first));
}

TEST(AssetManagerContractTest, AcquireExistingSourceRestoresReleasedReference)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("retained.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    ASSERT_TRUE(manager.releaseSource(assetId));
    ASSERT_EQ(manager.useCount(assetId), 0);

    EXPECT_TRUE(manager.acquireExistingSource(assetId));
    EXPECT_EQ(manager.useCount(assetId), 1);
    EXPECT_TRUE(manager.releaseSource(assetId));
    EXPECT_EQ(manager.useCount(assetId), 0);
}

TEST(AssetManagerContractTest, AcquireExistingSourceRejectsUnknownId)
{
    AssetManager manager;

    EXPECT_FALSE(manager.acquireExistingSource(QUuid::createUuid()));
}

TEST(AssetManagerContractTest, InvalidatingUnknownSourceDoesNotCreateRegistryEntry)
{
    AssetManager manager;
    const QUuid unknownId = QUuid::createUuid();

    EXPECT_EQ(manager.invalidateSource(unknownId), 0u);
    EXPECT_EQ(manager.sourceVersion(unknownId), 0u);
    EXPECT_FALSE(manager.acquireExistingSource(unknownId));
}

TEST(AssetManagerContractTest, ResetClearsSourceStateAndPayloadsButKeepsAssetIdentity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("reset.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = manager.sourceVersion(assetId);
    auto payload = makeShared<int>(41);
    const SharedPtr<void> opaquePayload = payload;
    ASSERT_TRUE(manager.publishDecodedPayload(
        assetId, version, QStringLiteral("RGBA"), opaquePayload));

    manager.resetSourceRegistry();

    EXPECT_EQ(manager.sourceVersion(assetId), 0u);
    EXPECT_EQ(manager.useCount(assetId), 0);
    EXPECT_FALSE(manager.acquireExistingSource(assetId));
    EXPECT_EQ(manager.decodedPayload(assetId, version, QStringLiteral("RGBA")).get(),
              nullptr);
    EXPECT_EQ(AssetDatabase::instance().findAssetByPath(path), assetId);
}

TEST(AssetManagerContractTest, CanonicalPathAliasSharesSourceIdentity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("still.png"));
    ASSERT_TRUE(createSourceFile(path));
    const QString alias = directory.filePath(QStringLiteral("./still.png"));

    AssetManager manager;
    const QUuid first = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(first.isNull());
    RegisteredAssetGuard cleanup(first);
    const QUuid second = manager.acquireSource(alias, AssetType::Image);

    EXPECT_EQ(second, first);
    EXPECT_EQ(manager.sourceId(alias), first);
    EXPECT_EQ(manager.useCount(first), 2);
    EXPECT_TRUE(manager.releaseSource(first));
    EXPECT_TRUE(manager.releaseSource(second));
}

TEST(AssetManagerContractTest, RejectsEmptyPathAndUnknownAssetType)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("unsupported.dat"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    EXPECT_TRUE(manager.acquireSource(QString(), AssetType::Image).isNull());
    EXPECT_TRUE(manager.acquireSource(path, AssetType::Unknown).isNull());
    EXPECT_TRUE(manager.sourceId(path).isNull());
}

TEST(AssetManagerContractTest, DecodedPayloadIsVersionScopedAndWeaklyCached)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("preview.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = manager.sourceVersion(assetId);
    ASSERT_NE(version, 0u);

    const auto payload = makeShared<int>(7);
    const SharedPtr<void> opaquePayload = payload;
    const SharedPtr<void> published =
        manager.publishDecodedPayload(assetId, version, QStringLiteral(" RGBA "),
                                      opaquePayload);
    ASSERT_NE(published.get(), nullptr);
    EXPECT_EQ(published.get(), payload.get());
    EXPECT_EQ(manager.decodedPayload(assetId, version, QStringLiteral("RGBA")).get(),
              payload.get());
    EXPECT_EQ(manager.decodedPayload(assetId, version,
                                     QStringLiteral("different representation")).get(),
              nullptr);

    const auto replacement = makeShared<int>(11);
    const SharedPtr<void> duplicate =
        manager.publishDecodedPayload(assetId, version, QStringLiteral("RGBA"),
                                      SharedPtr<void>(replacement));
    EXPECT_EQ(duplicate.get(), payload.get());

    const std::uint64_t nextVersion = manager.invalidateSource(assetId);
    EXPECT_EQ(nextVersion, version + 1);
    EXPECT_EQ(manager.sourceVersion(assetId), nextVersion);
    EXPECT_EQ(manager.decodedPayload(assetId, version, QStringLiteral("RGBA")).get(),
              nullptr);
    EXPECT_FALSE(manager.publishDecodedPayload(assetId, version,
                                               QStringLiteral("RGBA"), opaquePayload));

    auto transientPayload = makeShared<int>(13);
    ASSERT_TRUE(manager.publishDecodedPayload(
        assetId, nextVersion, QStringLiteral("transient"),
        SharedPtr<void>(transientPayload)));
    transientPayload.reset();
    EXPECT_EQ(manager.decodedPayload(assetId, nextVersion,
                                     QStringLiteral("transient")).get(), nullptr);
}

TEST(AssetManagerContractTest, InvalidatingOneSourcePreservesOtherSourcesPayloads)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first-cache.png"));
    const QString secondPath = directory.filePath(QStringLiteral("second-cache.png"));
    ASSERT_TRUE(createSourceFile(firstPath));
    ASSERT_TRUE(createSourceFile(secondPath));

    AssetManager manager;
    const QUuid firstId = manager.acquireSource(firstPath, AssetType::Image);
    ASSERT_FALSE(firstId.isNull());
    RegisteredAssetGuard cleanupFirst(firstId);
    const QUuid secondId = manager.acquireSource(secondPath, AssetType::Image);
    ASSERT_FALSE(secondId.isNull());
    RegisteredAssetGuard cleanupSecond(secondId);
    const std::uint64_t firstVersion = manager.sourceVersion(firstId);
    const std::uint64_t secondVersion = manager.sourceVersion(secondId);
    auto firstRgba = makeShared<int>(1);
    auto firstThumbnail = makeShared<int>(2);
    auto secondRgba = makeShared<int>(3);
    const SharedPtr<void> firstRgbaPayload = firstRgba;
    const SharedPtr<void> firstThumbnailPayload = firstThumbnail;
    const SharedPtr<void> secondRgbaPayload = secondRgba;
    ASSERT_TRUE(manager.publishDecodedPayload(
        firstId, firstVersion, QStringLiteral("RGBA"), firstRgbaPayload));
    ASSERT_TRUE(manager.publishDecodedPayload(
        firstId, firstVersion, QStringLiteral("thumbnail"), firstThumbnailPayload));
    ASSERT_TRUE(manager.publishDecodedPayload(
        secondId, secondVersion, QStringLiteral("RGBA"), secondRgbaPayload));

    EXPECT_EQ(manager.invalidateSource(firstId), firstVersion + 1);

    EXPECT_EQ(manager.decodedPayload(firstId, firstVersion, QStringLiteral("RGBA")).get(),
              nullptr);
    EXPECT_EQ(manager.decodedPayload(firstId, firstVersion,
                                     QStringLiteral("thumbnail")).get(), nullptr);
    EXPECT_EQ(manager.decodedPayload(secondId, secondVersion, QStringLiteral("RGBA")).get(),
              secondRgba.get());
    EXPECT_EQ(manager.sourceVersion(secondId), secondVersion);
}

TEST(AssetManagerContractTest, RejectsInvalidDecodedPayloadKeysAndNullPayload)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("payload.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = manager.sourceVersion(assetId);
    const auto payload = makeShared<int>(7);
    const SharedPtr<void> opaquePayload = payload;

    EXPECT_EQ(manager.decodedPayload(QUuid(), version, QStringLiteral("RGBA")).get(),
              nullptr);
    EXPECT_EQ(manager.decodedPayload(assetId, 0, QStringLiteral("RGBA")).get(),
              nullptr);
    EXPECT_EQ(manager.decodedPayload(assetId, version, QStringLiteral("  ")).get(),
              nullptr);
    EXPECT_EQ(manager.publishDecodedPayload(
                  QUuid(), version, QStringLiteral("RGBA"), opaquePayload).get(),
              nullptr);
    EXPECT_EQ(manager.publishDecodedPayload(
                  assetId, 0, QStringLiteral("RGBA"), opaquePayload).get(),
              nullptr);
    EXPECT_EQ(manager.publishDecodedPayload(
                  assetId, version, QStringLiteral("  "), opaquePayload).get(),
              nullptr);
    EXPECT_EQ(manager.publishDecodedPayload(
                  assetId, version, QStringLiteral("RGBA"), SharedPtr<void>{}).get(),
              nullptr);
}

TEST(AssetManagerContractTest, LocalizationMovesOwnershipAndCarriesDecodedPayload)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("localized.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid originalId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(originalId.isNull());
    RegisteredAssetGuard cleanup(originalId);
    const std::uint64_t version = manager.sourceVersion(originalId);
    const auto payload = makeShared<int>(29);
    ASSERT_TRUE(manager.publishDecodedPayload(
        originalId, version, QStringLiteral("RGBA"), SharedPtr<void>(payload)));

    const QUuid localizedId = manager.localizeSource(originalId);

    ASSERT_FALSE(localizedId.isNull());
    EXPECT_NE(localizedId, originalId);
    EXPECT_EQ(manager.useCount(originalId), 0);
    EXPECT_EQ(manager.useCount(localizedId), 1);
    EXPECT_FALSE(manager.isLocalizedSource(originalId));
    EXPECT_TRUE(manager.isLocalizedSource(localizedId));
    EXPECT_EQ(manager.sourceVersion(localizedId), version);
    EXPECT_EQ(manager.decodedPayload(localizedId, version, QStringLiteral("RGBA")).get(),
              payload.get());
    EXPECT_FALSE(manager.releaseSource(originalId));
    EXPECT_TRUE(manager.releaseSource(localizedId));
    EXPECT_EQ(manager.useCount(localizedId), 0);
}

TEST(AssetManagerContractTest, LocalizationRejectsReleasedSourceWithoutSideEffects)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("released-localization.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = manager.sourceVersion(assetId);
    ASSERT_TRUE(manager.releaseSource(assetId));
    ASSERT_EQ(manager.useCount(assetId), 0);

    EXPECT_TRUE(manager.localizeSource(assetId).isNull());
    EXPECT_TRUE(manager.localizeSource(QUuid::createUuid()).isNull());
    EXPECT_EQ(manager.useCount(assetId), 0);
    EXPECT_EQ(manager.sourceVersion(assetId), version);
    EXPECT_FALSE(manager.isLocalizedSource(assetId));
    EXPECT_EQ(manager.sourceId(path), assetId);
    EXPECT_EQ(AssetDatabase::instance().findAssetByPath(path), assetId);
}

TEST(AssetManagerContractTest, SourceRegistrySnapshotRestoresIdentityAndVersion)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("snapshot.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager original;
    const QUuid assetId = original.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = original.invalidateSource(assetId);
    const QJsonObject snapshot = original.sourceRegistrySnapshot();

    AssetManager restored;
    ASSERT_TRUE(restored.restoreSourceRegistrySnapshot(snapshot));

    EXPECT_EQ(restored.sourceVersion(assetId), version);
    EXPECT_EQ(restored.sourceId(path), assetId);
    EXPECT_FALSE(restored.isLocalizedSource(assetId));
    EXPECT_TRUE(restored.acquireExistingSource(assetId));
    EXPECT_EQ(restored.useCount(assetId), 1);
}

TEST(AssetManagerContractTest, EmptySourceRegistrySnapshotsPreserveExistingState)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("preserved.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = manager.sourceVersion(assetId);
    const QJsonObject emptySnapshot{
        {QStringLiteral("schemaVersion"), 2},
        {QStringLiteral("sources"), QJsonArray{}},
    };

    EXPECT_TRUE(manager.restoreSourceRegistrySnapshot(QJsonObject{}));
    EXPECT_TRUE(manager.restoreSourceRegistrySnapshot(emptySnapshot));

    EXPECT_EQ(manager.sourceVersion(assetId), version);
    EXPECT_EQ(manager.useCount(assetId), 1);
    EXPECT_EQ(manager.sourceId(path), assetId);
}

TEST(AssetManagerContractTest, SnapshotMergeKeepsMaximumVersionAndExistingUseCount)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("merge.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    manager.invalidateSource(assetId);
    const std::uint64_t currentVersion = manager.invalidateSource(assetId);
    auto makeSnapshot = [&](const std::uint64_t incomingVersion) {
        const QJsonObject source{
            {QStringLiteral("id"), assetId.toString(QUuid::WithoutBraces)},
            {QStringLiteral("path"), path},
            {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
            {QStringLiteral("version"), QString::number(incomingVersion)},
        };
        return QJsonObject{
            {QStringLiteral("schemaVersion"), 2},
            {QStringLiteral("sources"), QJsonArray{source}},
        };
    };

    ASSERT_TRUE(manager.restoreSourceRegistrySnapshot(makeSnapshot(1)));
    EXPECT_EQ(manager.sourceVersion(assetId), currentVersion);
    EXPECT_EQ(manager.useCount(assetId), 1);
    ASSERT_TRUE(manager.restoreSourceRegistrySnapshot(makeSnapshot(7)));
    EXPECT_EQ(manager.sourceVersion(assetId), 7u);
    EXPECT_EQ(manager.useCount(assetId), 1);
}

TEST(AssetManagerContractTest, SourceRegistryRestoreAcceptsLegacySchemaVersions)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const QUuid assetId = QUuid::createUuid();
    RegisteredAssetGuard cleanup(assetId);
    const QJsonObject source{
        {QStringLiteral("id"), assetId.toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), directory.filePath(QStringLiteral("legacy.png"))},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("7")},
    };
    AssetManager manager;
    const QJsonObject legacyWithoutVersion{
        {QStringLiteral("sources"), QJsonArray{source}},
    };
    const QJsonObject schemaVersionOne{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("sources"), QJsonArray{source}},
    };

    ASSERT_TRUE(manager.restoreSourceRegistrySnapshot(legacyWithoutVersion));
    EXPECT_EQ(manager.sourceVersion(assetId), 7u);
    EXPECT_FALSE(manager.isLocalizedSource(assetId));
    EXPECT_TRUE(manager.restoreSourceRegistrySnapshot(schemaVersionOne));
    EXPECT_EQ(manager.sourceVersion(assetId), 7u);
}

TEST(AssetManagerContractTest, SourceRegistrySnapshotRestoresLocalizedOriginRelationship)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("localized-snapshot.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager original;
    const QUuid originId = original.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(originId.isNull());
    RegisteredAssetGuard cleanup(originId);
    const QUuid localizedId = original.localizeSource(originId);
    ASSERT_FALSE(localizedId.isNull());
    const QJsonObject snapshot = original.sourceRegistrySnapshot();

    AssetManager restored;
    ASSERT_TRUE(restored.restoreSourceRegistrySnapshot(snapshot));

    EXPECT_EQ(restored.sourceVersion(originId), original.sourceVersion(originId));
    EXPECT_EQ(restored.sourceVersion(localizedId), original.sourceVersion(localizedId));
    EXPECT_FALSE(restored.isLocalizedSource(originId));
    EXPECT_TRUE(restored.isLocalizedSource(localizedId));
    EXPECT_EQ(restored.sourceId(path), originId);
    EXPECT_EQ(restored.useCount(originId), 0);
    EXPECT_EQ(restored.useCount(localizedId), 0);
    EXPECT_TRUE(restored.acquireExistingSource(localizedId));
    EXPECT_EQ(restored.useCount(localizedId), 1);
}

TEST(AssetManagerContractTest, InvalidSourceRegistrySnapshotHasNoPartialEffects)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    AssetDatabase& database = AssetDatabase::instance();
    AssetManager manager;
    const QUuid firstId = QUuid::createUuid();
    RegisteredAssetGuard cleanup(firstId);
    const QString firstPath = directory.filePath(QStringLiteral("would-register.png"));
    const QJsonObject validEntry{
        {QStringLiteral("id"), firstId.toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), firstPath},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("4")},
    };
    const QJsonObject invalidEntry{
        {QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), directory.filePath(QStringLiteral("invalid.png"))},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("0")},
    };
    const QJsonObject snapshot{
        {QStringLiteral("schemaVersion"), 2},
        {QStringLiteral("sources"), QJsonArray{validEntry, invalidEntry}},
    };

    EXPECT_FALSE(manager.restoreSourceRegistrySnapshot(snapshot));
    EXPECT_TRUE(database.findAssetByPath(firstPath).isNull());
    EXPECT_EQ(manager.sourceVersion(firstId), 0u);
    EXPECT_TRUE(manager.sourceRegistrySnapshot().value(QStringLiteral("sources"))
                    .toArray().isEmpty());
}

TEST(AssetManagerContractTest, UnsupportedSnapshotSchemaVersionsHaveNoSideEffects)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    AssetManager manager;

    const QJsonValue unsupportedVersions[] = {
        QJsonValue(0), QJsonValue(3), QJsonValue(1.5),
        QJsonValue(QStringLiteral("2")),
    };
    for (int index = 0; index < 4; ++index) {
        const QUuid assetId = QUuid::createUuid();
        RegisteredAssetGuard cleanup(assetId);
        const QString path = directory.filePath(
            QStringLiteral("unsupported-%1.png").arg(index));
        const QJsonObject source{
            {QStringLiteral("id"), assetId.toString(QUuid::WithoutBraces)},
            {QStringLiteral("path"), path},
            {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
            {QStringLiteral("version"), QStringLiteral("1")},
        };
        const QJsonObject snapshot{
            {QStringLiteral("schemaVersion"), unsupportedVersions[index]},
            {QStringLiteral("sources"), QJsonArray{source}},
        };

        EXPECT_FALSE(manager.restoreSourceRegistrySnapshot(snapshot)) << index;
        EXPECT_TRUE(AssetDatabase::instance().findAssetByPath(path).isNull()) << index;
        EXPECT_EQ(manager.sourceVersion(assetId), 0u) << index;
    }
}

TEST(AssetManagerContractTest, DuplicateSnapshotIdsAreRejectedWithoutRegistration)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    AssetManager manager;
    const QUuid duplicateId = QUuid::createUuid();
    RegisteredAssetGuard cleanup(duplicateId);
    const QString firstPath = directory.filePath(QStringLiteral("first.png"));
    const QString secondPath = directory.filePath(QStringLiteral("second.png"));
    const QJsonObject first{
        {QStringLiteral("id"), duplicateId.toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), firstPath},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("2")},
    };
    const QJsonObject second{
        {QStringLiteral("id"), duplicateId.toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), secondPath},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("3")},
    };
    const QJsonObject snapshot{
        {QStringLiteral("schemaVersion"), 2},
        {QStringLiteral("sources"), QJsonArray{first, second}},
    };

    EXPECT_FALSE(manager.restoreSourceRegistrySnapshot(snapshot));
    EXPECT_TRUE(AssetDatabase::instance().findAssetByPath(firstPath).isNull());
    EXPECT_TRUE(AssetDatabase::instance().findAssetByPath(secondPath).isNull());
    EXPECT_EQ(manager.sourceVersion(duplicateId), 0u);
}

TEST(AssetManagerContractTest, PreferredIdPathCollisionDoesNotPartiallyRestoreSnapshot)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    AssetDatabase& database = AssetDatabase::instance();
    AssetDatabaseSnapshotGuard restoreDatabase(database);
    database.clear();
    const QUuid occupiedId = QUuid::createUuid();
    const QString occupiedPath = directory.filePath(QStringLiteral("occupied.png"));
    ASSERT_EQ(database.registerAsset(occupiedPath, AssetType::Image, occupiedId),
              occupiedId);
    const QByteArray beforeRestore = database.toJson();
    const QUuid firstId = QUuid::createUuid();
    const QString firstPath = directory.filePath(QStringLiteral("first.png"));
    const QString collidingPath = directory.filePath(QStringLiteral("different.png"));
    const QJsonObject first{
        {QStringLiteral("id"), firstId.toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), firstPath},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("1")},
    };
    const QJsonObject colliding{
        {QStringLiteral("id"), occupiedId.toString(QUuid::WithoutBraces)},
        {QStringLiteral("path"), collidingPath},
        {QStringLiteral("type"), static_cast<int>(AssetType::Image)},
        {QStringLiteral("version"), QStringLiteral("1")},
    };
    const QJsonObject snapshot{
        {QStringLiteral("schemaVersion"), 2},
        {QStringLiteral("sources"), QJsonArray{first, colliding}},
    };
    AssetManager manager;

    EXPECT_FALSE(manager.restoreSourceRegistrySnapshot(snapshot));
    EXPECT_EQ(database.toJson(), beforeRestore);
    EXPECT_EQ(manager.sourceVersion(firstId), 0u);
    EXPECT_EQ(manager.sourceVersion(occupiedId), 0u);
}

TEST(AssetManagerContractTest, SourceHealthSnapshotCountsLiveAndExpiredPayloads)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("health.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    const std::uint64_t version = manager.sourceVersion(assetId);
    auto healthEntry = [&]() {
        const QJsonArray health = manager.sourceHealthSnapshot();
        EXPECT_EQ(health.size(), 1);
        return health.isEmpty() ? QJsonObject{} : health.front().toObject();
    };

    QJsonObject entry = healthEntry();
    EXPECT_EQ(entry.value(QStringLiteral("id")).toString(),
              assetId.toString(QUuid::WithoutBraces));
    EXPECT_EQ(entry.value(QStringLiteral("path")).toString(), path);
    EXPECT_EQ(entry.value(QStringLiteral("type")).toInt(),
              static_cast<int>(AssetType::Image));
    EXPECT_EQ(entry.value(QStringLiteral("version")).toString(),
              QString::number(version));
    EXPECT_EQ(entry.value(QStringLiteral("useCount")).toInt(), 1);
    EXPECT_EQ(entry.value(QStringLiteral("livePayloadCount")).toInt(), 0);
    EXPECT_FALSE(entry.value(QStringLiteral("localized")).toBool());

    auto payload = makeShared<int>(31);
    SharedPtr<void> opaquePayload = payload;
    auto published = manager.publishDecodedPayload(
        assetId, version, QStringLiteral("RGBA"), opaquePayload);
    ASSERT_NE(published.get(), nullptr);
    EXPECT_EQ(healthEntry().value(QStringLiteral("livePayloadCount")).toInt(), 1);

    published.reset();
    opaquePayload.reset();
    payload.reset();
    EXPECT_EQ(healthEntry().value(QStringLiteral("livePayloadCount")).toInt(), 0);
}

TEST(AssetManagerContractTest, SourceHealthSnapshotUsesOriginPathForLocalizedSource)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("localized-health.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid originId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(originId.isNull());
    RegisteredAssetGuard cleanup(originId);
    const QUuid localizedId = manager.localizeSource(originId);
    ASSERT_FALSE(localizedId.isNull());
    ASSERT_NE(localizedId, originId);

    const QJsonArray health = manager.sourceHealthSnapshot();
    ASSERT_EQ(health.size(), 2);
    QJsonObject localizedEntry;
    for (const QJsonValue& value : health) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() ==
            localizedId.toString(QUuid::WithoutBraces)) {
            localizedEntry = entry;
            break;
        }
    }

    ASSERT_FALSE(localizedEntry.isEmpty());
    EXPECT_EQ(localizedEntry.value(QStringLiteral("path")).toString(), path);
    EXPECT_TRUE(localizedEntry.value(QStringLiteral("localized")).toBool());
    EXPECT_EQ(localizedEntry.value(QStringLiteral("useCount")).toInt(), 1);
}

TEST(AssetManagerContractTest, SourceHealthSnapshotOmitsUnregisteredAssetIdentity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("unregistered.png"));
    ASSERT_TRUE(createSourceFile(path));

    AssetManager manager;
    const QUuid assetId = manager.acquireSource(path, AssetType::Image);
    ASSERT_FALSE(assetId.isNull());
    RegisteredAssetGuard cleanup(assetId);
    ASSERT_EQ(manager.sourceHealthSnapshot().size(), 1);

    AssetDatabase::instance().unregisterAsset(assetId);

    EXPECT_TRUE(manager.sourceHealthSnapshot().isEmpty());
    EXPECT_NE(manager.sourceVersion(assetId), 0u);
}
