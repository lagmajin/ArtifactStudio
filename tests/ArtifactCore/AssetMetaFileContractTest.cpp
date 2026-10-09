#include <gtest/gtest.h>

#include <QDateTime>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariant>

import Asset;
import AssetType;

using namespace ArtifactCore;

namespace {

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

} // namespace

TEST(AssetMetaFileContractTest, SettersPreserveAssetFieldsAndCreateVersion)
{
    const QString assetPath = QStringLiteral("/project/media/still.png");
    const QUuid id = QUuid::createUuid();
    const QDateTime importedAt =
        QDateTime::fromString(QStringLiteral("2026-10-08T21:34:56+09:00"), Qt::ISODate);
    ASSERT_TRUE(importedAt.isValid());

    QJsonObject asset{{QStringLiteral("sourcePath"), assetPath}};
    const QJsonObject document{{QStringLiteral("asset"), asset}};
    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(
        QJsonDocument(document).toJson(QJsonDocument::Compact),
        assetPath);
    meta.setUuid(id);
    meta.setType(AssetType::Image);
    meta.setImportedAt(importedAt);

    EXPECT_TRUE(meta.isValid());
    EXPECT_EQ(meta.sourcePath(), assetPath);
    EXPECT_EQ(meta.uuid(), id);
    EXPECT_EQ(meta.type(), AssetType::Image);
    EXPECT_EQ(meta.importedAt(), importedAt.toUTC());
    EXPECT_EQ(meta.toJson().value(QStringLiteral("asset")).toObject()
                  .value(QStringLiteral("importedAt")).toString(),
              QStringLiteral("2026-10-08T12:34:56Z"));
    EXPECT_EQ(meta.toJson().value(QStringLiteral("version")).toInt(), 1);
}

TEST(AssetMetaFileContractTest, ProxiesTagsAndCustomValuesRoundTrip)
{
    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"));
    meta.addProxy(QStringLiteral("720p"), QStringLiteral("proxy/720.png"));
    meta.addProxy(QStringLiteral("1080p"), QStringLiteral("proxy/1080.png"));
    meta.addTag(QStringLiteral("  sequence  "));
    meta.addTag(QStringLiteral("sequence"));
    meta.addTag(QStringLiteral("  "));
    meta.addTag(QStringLiteral("review"));
    meta.setCustomValue(QStringLiteral("colorSpace"), QStringLiteral("ACEScg"));
    meta.setCustomValue(QStringLiteral("frameCount"), 48);

    EXPECT_TRUE(meta.isValid());
    EXPECT_EQ(meta.proxyPath(QStringLiteral("720p")), QStringLiteral("proxy/720.png"));
    EXPECT_EQ(meta.proxyPath(QStringLiteral("missing")), QString());
    EXPECT_EQ(meta.proxyResolutions(),
              (QStringList{QStringLiteral("1080p"), QStringLiteral("720p")}));
    EXPECT_EQ(meta.tags(),
              (QStringList{QStringLiteral("sequence"), QStringLiteral("review")}));
    EXPECT_EQ(meta.customValue(QStringLiteral("colorSpace")).toString(),
              QStringLiteral("ACEScg"));
    EXPECT_EQ(meta.customValue(QStringLiteral("frameCount")).toInt(), 48);

    meta.removeTag(QStringLiteral("sequence"));
    EXPECT_EQ(meta.tags(), (QStringList{QStringLiteral("review")}));
}

TEST(AssetMetaFileContractTest, SavesAndLoadsTheSidecarBesideItsSource)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("shot.png"));
    const QString expectedMetaPath = sourcePath + QStringLiteral(".assetmeta");
    const QUuid id = QUuid::createUuid();

    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(
        QByteArrayLiteral("{}"), sourcePath);
    meta.setUuid(id);
    meta.setType(AssetType::Image);
    meta.addProxy(QStringLiteral("720p"), QStringLiteral("shot_720.png"));
    meta.addTag(QStringLiteral("approved"));
    ASSERT_TRUE(meta.save());

    EXPECT_EQ(ArtifactAssetMetaFile::metaPathFor(sourcePath), expectedMetaPath);
    EXPECT_TRUE(QFile::exists(expectedMetaPath));
    const ArtifactAssetMetaFile loaded = ArtifactAssetMetaFile::load(sourcePath);
    EXPECT_TRUE(loaded.isValid());
    EXPECT_EQ(loaded.uuid(), id);
    EXPECT_EQ(loaded.type(), AssetType::Image);
    EXPECT_EQ(loaded.proxyPath(QStringLiteral("720p")),
              QStringLiteral("shot_720.png"));
    EXPECT_EQ(loaded.tags(), (QStringList{QStringLiteral("approved")}));
}

TEST(AssetMetaFileContractTest, InvalidJsonAndMissingVersionRemainInvalid)
{
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{")).isValid());
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(
                     QJsonDocument(QJsonArray{}).toJson(QJsonDocument::Compact))
                     .isValid());
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(
                     QByteArrayLiteral("{\"asset\":{\"type\":\"Image\"}}"))
                     .isValid());
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(
                     QByteArrayLiteral("{\"version\":0,\"asset\":{}}"))
                     .isValid());
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(
                     QByteArrayLiteral("{\"version\":-1,\"asset\":{}}"))
                     .isValid());
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(
                     QByteArrayLiteral("{\"version\":\"1\",\"asset\":{}}"))
                     .isValid());
    EXPECT_FALSE(ArtifactAssetMetaFile::fromJson(
                     QByteArrayLiteral("{\"version\":1.5,\"asset\":{}}"))
                     .isValid());

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("broken.png"));
    EXPECT_FALSE(ArtifactAssetMetaFile::load(sourcePath).isValid());
    ASSERT_TRUE(writeBytes(ArtifactAssetMetaFile::metaPathFor(sourcePath),
                           QByteArrayLiteral("not-json")));
    EXPECT_FALSE(ArtifactAssetMetaFile::load(sourcePath).isValid());
}
