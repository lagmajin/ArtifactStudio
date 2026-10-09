#include <gtest/gtest.h>

#include <QFile>
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
