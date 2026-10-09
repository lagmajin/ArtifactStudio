#include <gtest/gtest.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTemporaryDir>
#include <QString>
#include <QUuid>

#include <utility>

import Asset;
import Asset.Database;
import Asset.Import.Setting;
import Asset.Importer;
import AssetType;

using namespace ArtifactCore;

namespace {

bool writeTextFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(contents) == contents.size();
}

QByteArray onePixelPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg=="));
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

} // namespace

TEST(AssetImporterContractTest, SupportedExtensionsAreCaseInsensitiveAndAcceptOptionalDot)
{
    EXPECT_TRUE(AssetImporter::isSupported(QStringLiteral("png")));
    EXPECT_TRUE(AssetImporter::isSupported(QStringLiteral(".PNG")));
    EXPECT_TRUE(AssetImporter::isSupported(QStringLiteral("json")));
    EXPECT_TRUE(AssetImporter::isSupported(QStringLiteral(".obj")));
    EXPECT_FALSE(AssetImporter::isSupported(QString()));
    EXPECT_FALSE(AssetImporter::isSupported(QStringLiteral(".exe")));
}

TEST(AssetImporterContractTest, RejectsMissingFilesAndUnsupportedExtensions)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString missing = directory.filePath(QStringLiteral("missing.json"));
    const QString unsupported = directory.filePath(QStringLiteral("tool.exe"));
    ASSERT_TRUE(writeTextFile(unsupported, QByteArrayLiteral("binary")));

    const AssetImportSettings settings;
    EXPECT_TRUE(AssetImporter::importFile(missing, settings).isNull());
    EXPECT_TRUE(AssetImporter::importFile(unsupported, settings).isNull());
    EXPECT_TRUE(AssetDatabase::instance().findAssetByPath(unsupported).isNull());
}

TEST(AssetImporterContractTest, RejectsDirectoriesEvenWhenTheirSuffixIsSupported)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString imageDirectory =
        directory.filePath(QStringLiteral("not-an-image.png"));
    ASSERT_TRUE(QDir().mkpath(imageDirectory));

    const AssetImportSettings settings;
    EXPECT_TRUE(AssetImporter::importFile(imageDirectory, settings).isNull());
    EXPECT_TRUE(AssetDatabase::instance().findAssetByPath(imageDirectory).isNull());
    EXPECT_FALSE(QFile::exists(
        ArtifactAssetMetaFile::metaPathFor(imageDirectory)));
}

TEST(AssetImporterContractTest, ImportsJsonAsDataAndWritesMatchingSidecar)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("scene.json"));
    ASSERT_TRUE(writeTextFile(sourcePath,
                              QByteArrayLiteral("{\"name\":\"scene\"}")));

    AssetImportSettings settings;
    settings.generateProxyOnImport = true; // Non-image assets must not generate proxies.
    const QUuid id = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);

    const AssetInfo info = AssetDatabase::instance().getAssetInfo(id);
    EXPECT_EQ(info.type, AssetType::Data);
    EXPECT_EQ(AssetDatabase::instance().findAssetByPath(sourcePath), id);

    const ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::load(sourcePath);
    EXPECT_TRUE(meta.isValid());
    EXPECT_EQ(meta.uuid(), id);
    EXPECT_EQ(meta.type(), AssetType::Data);
    EXPECT_EQ(meta.sourcePath(), QFileInfo(sourcePath).absoluteFilePath());
    EXPECT_TRUE(meta.importedAt().isValid());
    EXPECT_TRUE(meta.proxyResolutions().isEmpty());
}

TEST(AssetImporterContractTest, ReimportKeepsAssetIdentityAndOriginalImportTime)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("scene.json"));
    ASSERT_TRUE(writeTextFile(sourcePath, QByteArrayLiteral("{\"name\":\"scene\"}")));

    const AssetImportSettings settings;
    const QUuid firstId = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(firstId.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), firstId);
    const QDateTime originalImportTime =
        ArtifactAssetMetaFile::load(sourcePath).importedAt();
    ASSERT_TRUE(originalImportTime.isValid());

    const QUuid secondId = AssetImporter::importFile(sourcePath, settings);
    const ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::load(sourcePath);

    EXPECT_EQ(secondId, firstId);
    EXPECT_EQ(meta.uuid(), firstId);
    EXPECT_EQ(meta.importedAt(), originalImportTime);
}

TEST(AssetImporterContractTest, ImportsPngAndPersistsDecodedImageMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("still.png"));
    ASSERT_TRUE(writeTextFile(sourcePath, onePixelPng()));

    const AssetImportSettings settings;
    const QUuid id = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);

    const AssetInfo info = AssetDatabase::instance().getAssetInfo(id);
    EXPECT_EQ(info.type, AssetType::Image);
    EXPECT_EQ(AssetDatabase::instance().findAssetByPath(sourcePath), id);

    const ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::load(sourcePath);
    ASSERT_TRUE(meta.isValid());
    EXPECT_EQ(meta.uuid(), id);
    EXPECT_EQ(meta.type(), AssetType::Image);
    EXPECT_EQ(meta.sourcePath(), QFileInfo(sourcePath).absoluteFilePath());
    EXPECT_TRUE(meta.importedAt().isValid());
    EXPECT_TRUE(meta.proxyResolutions().isEmpty());
    EXPECT_EQ(meta.customValue(QStringLiteral("image/width")).toInt(), 1);
    EXPECT_EQ(meta.customValue(QStringLiteral("image/height")).toInt(), 1);
    EXPECT_EQ(meta.customValue(QStringLiteral("image/channels")).toInt(), 4);
    EXPECT_GT(meta.customValue(QStringLiteral("image/bitDepth")).toInt(), 0);
    EXPECT_FALSE(meta.customValue(QStringLiteral("image/pixelType")).toString().isEmpty());
}

TEST(AssetImporterContractTest, ContentTypeOverridesImageExtensionForTextPayload)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("notes.png"));
    ASSERT_TRUE(writeTextFile(sourcePath, QByteArrayLiteral("plain text payload")));

    const AssetImportSettings settings;
    const QUuid id = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);

    EXPECT_EQ(AssetDatabase::instance().getAssetInfo(id).type, AssetType::Data);
    const ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::load(sourcePath);
    ASSERT_TRUE(meta.isValid());
    EXPECT_EQ(meta.type(), AssetType::Data);
    EXPECT_FALSE(meta.customValue(QStringLiteral("image/width")).isValid());
}
