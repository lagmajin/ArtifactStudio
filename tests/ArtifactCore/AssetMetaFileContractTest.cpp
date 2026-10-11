#include <gtest/gtest.h>

#include <QDateTime>
#include <QByteArray>
#include <QDir>
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
#include <QVariantMap>

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

TEST(AssetMetaFileContractTest, SettersAndSavePreserveUnknownJsonFields)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("extended.png"));
    const QUuid id = QUuid::createUuid();
    const QJsonObject originalAsset{
        {QStringLiteral("sourcePath"), sourcePath},
        {QStringLiteral("vendorToken"), QStringLiteral("asset-extension")}};
    const QJsonObject original{
        {QStringLiteral("version"), 1},
        {QStringLiteral("futureSection"), QJsonObject{{QStringLiteral("enabled"), true}}},
        {QStringLiteral("asset"), originalAsset}};

    auto meta = ArtifactAssetMetaFile::fromJson(
        QJsonDocument(original).toJson(QJsonDocument::Compact), sourcePath);
    meta.setUuid(id);
    meta.setType(AssetType::Image);
    ASSERT_TRUE(meta.save());

    const auto loaded = ArtifactAssetMetaFile::load(sourcePath);
    EXPECT_TRUE(loaded.isValid());
    EXPECT_EQ(loaded.uuid(), id);
    EXPECT_EQ(loaded.sourcePath(), sourcePath);
    EXPECT_TRUE(loaded.toJson().value(QStringLiteral("futureSection")).toObject()
                    .value(QStringLiteral("enabled")).toBool());
    EXPECT_EQ(loaded.toJson().value(QStringLiteral("asset")).toObject()
                  .value(QStringLiteral("vendorToken")).toString(),
              QStringLiteral("asset-extension"));
}

TEST(AssetMetaFileContractTest, ImportedTimestampRemainsUtcAfterSidecarRoundTrip)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("dated.png"));
    const QDateTime importedAt = QDateTime::fromString(
        QStringLiteral("2026-10-08T21:34:56+09:00"), Qt::ISODate);
    ASSERT_TRUE(importedAt.isValid());

    auto meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"), sourcePath);
    meta.setImportedAt(importedAt);
    ASSERT_TRUE(meta.save());

    const auto loaded = ArtifactAssetMetaFile::load(sourcePath);
    EXPECT_EQ(loaded.importedAt(), importedAt.toUTC());
    EXPECT_EQ(loaded.toJson().value(QStringLiteral("asset")).toObject()
                  .value(QStringLiteral("importedAt")).toString(),
              QStringLiteral("2026-10-08T12:34:56Z"));
}

TEST(AssetMetaFileContractTest, InvalidImportedTimestampRemainsInvalidAfterRoundTrip)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("undated.png"));
    auto meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"), sourcePath);
    meta.setImportedAt(QDateTime{});
    ASSERT_TRUE(meta.save());

    const auto loaded = ArtifactAssetMetaFile::load(sourcePath);
    EXPECT_FALSE(loaded.importedAt().isValid());
    EXPECT_TRUE(loaded.toJson().value(QStringLiteral("asset")).toObject()
                    .value(QStringLiteral("importedAt")).toString().isEmpty());
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

TEST(AssetMetaFileContractTest, MissingSidecarLoadsAsInvalidEmptyMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("offline.png"));

    const ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::load(sourcePath);

    EXPECT_FALSE(meta.isValid());
    EXPECT_TRUE(meta.sourcePath().isEmpty());
    EXPECT_TRUE(meta.toJson().isEmpty());
}

TEST(AssetMetaFileContractTest, MissingAssetFieldsReturnEmptyValuesAndUnknownType)
{
    const QJsonObject document{{QStringLiteral("version"), 1},
                               {QStringLiteral("asset"), QJsonObject{}}};
    const auto meta = ArtifactAssetMetaFile::fromJson(
        QJsonDocument(document).toJson(QJsonDocument::Compact));

    EXPECT_TRUE(meta.isValid());
    EXPECT_TRUE(meta.sourcePath().isEmpty());
    EXPECT_TRUE(meta.uuid().isNull());
    EXPECT_EQ(meta.type(), AssetType::Unknown);
    EXPECT_FALSE(meta.importedAt().isValid());
}

TEST(AssetMetaFileContractTest, ProxyUpdateReplacesExistingResolution)
{
    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"));
    meta.addProxy(QStringLiteral("720p"), QStringLiteral("proxy/old.png"));
    meta.addProxy(QStringLiteral("720p"), QStringLiteral("proxy/new.png"));

    EXPECT_EQ(meta.proxyPath(QStringLiteral("720p")), QStringLiteral("proxy/new.png"));
    EXPECT_EQ(meta.proxyResolutions(), (QStringList{QStringLiteral("720p")}));
}

TEST(AssetMetaFileContractTest, RemovingUnknownOrRepeatedTagIsSafe)
{
    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"));
    meta.addTag(QStringLiteral(" review "));
    meta.removeTag(QStringLiteral("missing"));
    EXPECT_EQ(meta.tags(), (QStringList{QStringLiteral("review")}));

    meta.removeTag(QStringLiteral("review"));
    meta.removeTag(QStringLiteral("review"));
    EXPECT_TRUE(meta.tags().isEmpty());
}

TEST(AssetMetaFileContractTest, RemovingTagClearsAllLegacyDuplicateEntries)
{
    const QJsonObject document{
        {QStringLiteral("version"), 1},
        {QStringLiteral("tags"), QJsonArray{
             QStringLiteral("review"), QStringLiteral("keep"),
             QStringLiteral("review")}}};
    auto meta = ArtifactAssetMetaFile::fromJson(
        QJsonDocument(document).toJson(QJsonDocument::Compact));

    meta.removeTag(QStringLiteral("review"));

    EXPECT_EQ(meta.tags(), (QStringList{QStringLiteral("keep")}));
}

TEST(AssetMetaFileContractTest, RemovingTagTrimsInputLikeAddingTag)
{
    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"));
    meta.addTag(QStringLiteral(" review "));
    meta.removeTag(QStringLiteral(" review "));

    EXPECT_TRUE(meta.tags().isEmpty());
}

TEST(AssetMetaFileContractTest, AssetTypeNamesParseCaseInsensitivelyAndUnknownFallsBack)
{
    const struct {
        const char* name;
        AssetType expected;
    } cases[] = {
        {"IMAGE", AssetType::Image},
        {"audio", AssetType::Audio},
        {"Video", AssetType::Video},
        {"model", AssetType::Model},
        {"script", AssetType::Script},
        {"folder", AssetType::Folder},
        {"data", AssetType::Data},
        {"unsupported", AssetType::Unknown},
    };

    for (const auto& testCase : cases) {
        const QJsonObject asset{{QStringLiteral("type"), QString::fromLatin1(testCase.name)}};
        const QJsonObject document{{QStringLiteral("version"), 1},
                                   {QStringLiteral("asset"), asset}};
        const auto meta = ArtifactAssetMetaFile::fromJson(
            QJsonDocument(document).toJson(QJsonDocument::Compact));
        EXPECT_EQ(meta.type(), testCase.expected) << testCase.name;
    }
}

TEST(AssetMetaFileContractTest, AssetTypeNameParsingTrimsSurroundingWhitespace)
{
    const QJsonObject asset{{QStringLiteral("type"),
                             QStringLiteral(" \t IMAGE \n ")}};
    const QJsonObject document{{QStringLiteral("version"), 1},
                               {QStringLiteral("asset"), asset}};

    const auto meta = ArtifactAssetMetaFile::fromJson(
        QJsonDocument(document).toJson(QJsonDocument::Compact));

    EXPECT_EQ(meta.type(), AssetType::Image);
}

TEST(AssetMetaFileContractTest, MissingCustomValueIsInvalidAndOverwriteKeepsOtherValues)
{
    ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"));
    EXPECT_FALSE(meta.customValue(QStringLiteral("absent")).isValid());
    meta.setCustomValue(QStringLiteral("count"), 4);
    meta.setCustomValue(QStringLiteral("label"), QStringLiteral("first"));
    meta.setCustomValue(QStringLiteral("count"), 9);

    EXPECT_EQ(meta.customValue(QStringLiteral("count")).toInt(), 9);
    EXPECT_EQ(meta.customValue(QStringLiteral("label")).toString(), QStringLiteral("first"));
}

TEST(AssetMetaFileContractTest, NestedCustomVariantRoundTripsThroughSidecar)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("nested.png"));
    QVariantMap nested;
    nested.insert(QStringLiteral("colorSpace"), QStringLiteral("ACEScg"));
    nested.insert(QStringLiteral("channels"), QStringList{QStringLiteral("R"),
                                                          QStringLiteral("G"),
                                                          QStringLiteral("B")});
    QVariantMap custom;
    custom.insert(QStringLiteral("image"), nested);
    custom.insert(QStringLiteral("premultiplied"), true);

    auto meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"), sourcePath);
    meta.setCustomValue(QStringLiteral("pipeline"), custom);
    ASSERT_TRUE(meta.save());

    const QVariantMap restored = ArtifactAssetMetaFile::load(sourcePath)
                                     .customValue(QStringLiteral("pipeline")).toMap();
    const QVariantMap restoredImage = restored.value(QStringLiteral("image")).toMap();
    EXPECT_EQ(restored.value(QStringLiteral("premultiplied")).toBool(), true);
    EXPECT_EQ(restoredImage.value(QStringLiteral("colorSpace")).toString(),
              QStringLiteral("ACEScg"));
    EXPECT_EQ(restoredImage.value(QStringLiteral("channels")).toStringList(),
              (QStringList{QStringLiteral("R"), QStringLiteral("G"), QStringLiteral("B")}));
}

TEST(AssetMetaFileContractTest, SaveRequiresSourcePath)
{
    const auto meta = ArtifactAssetMetaFile::fromJson(QByteArrayLiteral("{}"));
    EXPECT_FALSE(meta.save());
}

TEST(AssetMetaFileContractTest, SaveFailsWhenSidecarDestinationIsDirectory)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("blocked.png"));
    const QString sidecarPath = ArtifactAssetMetaFile::metaPathFor(sourcePath);
    ASSERT_TRUE(QDir().mkpath(sidecarPath));
    const auto meta = ArtifactAssetMetaFile::fromJson(
        QByteArrayLiteral("{\"version\":1}"), sourcePath);

    EXPECT_FALSE(meta.save());
    EXPECT_TRUE(QDir(sidecarPath).exists());
}
