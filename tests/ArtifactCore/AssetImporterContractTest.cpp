#include <gtest/gtest.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QSize>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <utility>

import Asset;
import Asset.Database;
import Asset.Import.Setting;
import Asset.Importer;
import Asset.VectorImport;
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

TEST(AssetImporterContractTest, VectorSourceKindNormalizesSupportedExtensions)
{
    const struct {
        const char* extension;
        VectorSourceKind expected;
    } cases[] = {
        {"ai", VectorSourceKind::Illustrator},
        {".PDF", VectorSourceKind::Pdf},
        {" eps ", VectorSourceKind::Eps},
        {".Svg", VectorSourceKind::Svg},
        {"AFDESIGN", VectorSourceKind::AffinityExport},
        {".afphoto", VectorSourceKind::AffinityExport},
        {"afpub", VectorSourceKind::AffinityExport},
        {"png", VectorSourceKind::Unknown},
        {"", VectorSourceKind::Unknown},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.extension);
        EXPECT_EQ(vectorSourceKindForExtension(
                      QString::fromLatin1(testCase.extension)),
                  testCase.expected);
    }
}

TEST(AssetImporterContractTest, VectorDocumentTypeAcceptsOnlyDocumentFileType)
{
    const struct {
        FileType type;
        bool expected;
    } cases[] = {
        {FileType::Unknown, false},
        {FileType::Image, false},
        {FileType::Video, false},
        {FileType::Audio, false},
        {FileType::Text, false},
        {FileType::Font, false},
        {FileType::Binary, false},
        {FileType::Document, true},
        {FileType::Archive, false},
        {FileType::Model3D, false},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(static_cast<int>(testCase.type));
        EXPECT_EQ(isVectorDocumentType(testCase.type), testCase.expected);
    }
}

TEST(AssetImporterContractTest, VectorImportResultSeparatesUnsupportedAndPartialState)
{
    VectorImportResult result;
    EXPECT_FALSE(result.hasUnsupportedFeatures());
    EXPECT_FALSE(result.isPartial());

    result.importMode = VectorImportMode::EditablePartial;
    EXPECT_FALSE(result.hasUnsupportedFeatures());
    EXPECT_TRUE(result.isPartial());

    result.importMode = VectorImportMode::PreviewOnly;
    result.unsupportedFeatures.append(QStringLiteral("blend mode"));
    EXPECT_TRUE(result.hasUnsupportedFeatures());
    EXPECT_TRUE(result.isPartial());

    result.unsupportedFeatures.clear();
    result.issues.append({QStringLiteral("unsupported-feature"),
                          QStringLiteral("feature was not imported")});
    EXPECT_FALSE(result.hasUnsupportedFeatures());
    EXPECT_TRUE(result.isPartial());
}

TEST(AssetImporterContractTest, VectorAssetFileExtractsSvgViewBoxAndBasicNodeSummary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.svg"));
    const QByteArray svg = QByteArrayLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10.2 5.1\">"
        "<g><path d=\"M0 0\"/><text>x</text><image href=\"asset.png\"/>"
        "</g></svg>");
    ASSERT_TRUE(writeTextFile(sourcePath, svg));

    const VectorAssetFile asset(sourcePath);
    const auto& result = asset.importResult();

    EXPECT_EQ(result.sourceKind, VectorSourceKind::Svg);
    EXPECT_EQ(result.sourceSize, QSize(11, 6));
    ASSERT_EQ(result.pages.size(), 1);
    EXPECT_EQ(result.pages.front().name, QStringLiteral("source.svg"));
    EXPECT_TRUE(result.pages.front().isArtboard);
    EXPECT_DOUBLE_EQ(result.pages.front().bounds.x(), 0.0);
    EXPECT_DOUBLE_EQ(result.pages.front().bounds.y(), 0.0);
    EXPECT_DOUBLE_EQ(result.pages.front().bounds.width(), 10.2);
    EXPECT_DOUBLE_EQ(result.pages.front().bounds.height(), 5.1);
    EXPECT_EQ(result.nodeSummary.groupNodes, 1);
    EXPECT_EQ(result.nodeSummary.pathNodes, 1);
    EXPECT_EQ(result.nodeSummary.textNodes, 1);
    EXPECT_EQ(result.nodeSummary.imageNodes, 1);
    EXPECT_EQ(result.nodeSummary.totalNodes, 4);
}

TEST(AssetImporterContractTest, VectorAssetFileReportsMalformedSvgAsPartialIssue)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("broken.svg"));
    ASSERT_TRUE(writeTextFile(
        sourcePath,
        QByteArrayLiteral("<svg viewBox=\"0 0 10 10\"><path d=\"M0 0\">")));

    const VectorAssetFile asset(sourcePath);
    const auto& result = asset.importResult();

    EXPECT_TRUE(result.exists);
    EXPECT_EQ(result.sourceKind, VectorSourceKind::Svg);
    EXPECT_FALSE(result.previewReady);
    EXPECT_FALSE(result.editableReady);
    EXPECT_EQ(result.importMode, VectorImportMode::EditablePartial);
    EXPECT_FALSE(result.hasUnsupportedFeatures());
    EXPECT_TRUE(result.isPartial());
    ASSERT_EQ(result.issues.size(), 1);
    EXPECT_EQ(result.issues.front().code, QStringLiteral("svg.parse"));
    EXPECT_FALSE(result.issues.front().message.isEmpty());
    EXPECT_EQ(result.nodeSummary.pathNodes, 1);
}

TEST(AssetImporterContractTest, VectorAssetFileReportsUnsupportedSvgFeatures)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("features.svg"));
    ASSERT_TRUE(writeTextFile(
        sourcePath,
        QByteArrayLiteral(
            "<svg><path/><filter/><mask/><pattern/><foreignObject/></svg>")));

    const VectorAssetFile asset(sourcePath);
    const auto& result = asset.importResult();

    EXPECT_EQ(result.unsupportedFeatures,
              (QStringList{QStringLiteral("filter"), QStringLiteral("mask"),
                           QStringLiteral("pattern"), QStringLiteral("foreignobject")}));
    EXPECT_TRUE(result.hasUnsupportedFeatures());
    EXPECT_TRUE(result.isPartial());
    EXPECT_EQ(result.importMode, VectorImportMode::EditablePartial);
    EXPECT_EQ(result.nodeSummary.pathNodes, 1);
}

TEST(AssetImporterContractTest, VectorAssetFileLeavesBoundsEmptyWithoutSvgViewBox)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("no-viewbox.svg"));
    ASSERT_TRUE(writeTextFile(
        sourcePath,
        QByteArrayLiteral("<svg><path d=\"M0 0\"/></svg>")));

    const VectorAssetFile asset(sourcePath);
    const auto& result = asset.importResult();

    EXPECT_TRUE(result.issues.isEmpty());
    EXPECT_TRUE(result.sourceSize.isEmpty());
    EXPECT_TRUE(result.pages.isEmpty());
    EXPECT_EQ(result.nodeSummary.pathNodes, 1);
    EXPECT_EQ(result.nodeSummary.totalNodes, 1);
}

TEST(AssetImporterContractTest, VectorAssetFileRejectsInvalidSvgViewBoxDimensions)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const struct {
        const char* fileName;
        const char* svg;
    } cases[] = {
        {"zero-width.svg", "<svg viewBox=\"0 0 0 10\"><path/></svg>"},
        {"negative-height.svg", "<svg viewBox=\"0 0 10 -1\"><path/></svg>"},
        {"nonnumeric-width.svg", "<svg viewBox=\"0 0 wide 10\"><path/></svg>"},
        {"wrong-component-count.svg", "<svg viewBox=\"0 0 10\"><path/></svg>"},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const QString sourcePath = directory.filePath(QString::fromLatin1(testCase.fileName));
        ASSERT_TRUE(writeTextFile(sourcePath, QByteArray(testCase.svg)));

        const VectorAssetFile asset(sourcePath);
        const auto& result = asset.importResult();

        EXPECT_TRUE(result.issues.isEmpty());
        EXPECT_TRUE(result.sourceSize.isEmpty());
        EXPECT_TRUE(result.pages.isEmpty());
        EXPECT_EQ(result.nodeSummary.pathNodes, 1);
    }
}

TEST(AssetImporterContractTest, VectorAssetFileReportsMissingSourceWithoutReadiness)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("missing.svg"));
    ASSERT_FALSE(QFileInfo::exists(sourcePath));

    const VectorAssetFile asset(sourcePath);
    const auto& result = asset.importResult();

    EXPECT_EQ(asset.sourcePath(), sourcePath);
    EXPECT_EQ(result.sourcePath, sourcePath);
    EXPECT_EQ(result.displayName, QStringLiteral("missing.svg"));
    EXPECT_EQ(result.sourceKind, VectorSourceKind::Svg);
    EXPECT_FALSE(result.exists);
    EXPECT_FALSE(result.readable);
    EXPECT_EQ(result.fileType, FileType::Unknown);
    EXPECT_FALSE(result.previewReady);
    EXPECT_FALSE(result.editableReady);
    EXPECT_EQ(result.importMode, VectorImportMode::PreviewOnly);
    EXPECT_TRUE(result.sourceSize.isEmpty());
    EXPECT_TRUE(result.pages.isEmpty());
    EXPECT_TRUE(result.issues.isEmpty());
}

TEST(AssetImporterContractTest, VectorAssetFileSeparatesPreviewAndEditableReadiness)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray pdfHeader = QByteArrayLiteral("%PDF-1.7\n");
    const struct {
        const char* fileName;
        VectorSourceKind sourceKind;
        bool editableReady;
        VectorImportMode importMode;
    } cases[] = {
        {"source.pdf", VectorSourceKind::Pdf, true,
         VectorImportMode::EditableAttempted},
        {"document.unknown", VectorSourceKind::Unknown, false,
         VectorImportMode::PreviewOnly},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const QString sourcePath = directory.filePath(QString::fromLatin1(testCase.fileName));
        ASSERT_TRUE(writeTextFile(sourcePath, pdfHeader));

        const VectorAssetFile asset(sourcePath);
        const auto& result = asset.importResult();

        EXPECT_TRUE(result.exists);
        EXPECT_TRUE(result.readable);
        EXPECT_EQ(result.assetType, AssetType::Data);
        EXPECT_EQ(result.fileType, FileType::Document);
        EXPECT_EQ(result.sourceKind, testCase.sourceKind);
        EXPECT_TRUE(result.previewReady);
        EXPECT_EQ(result.editableReady, testCase.editableReady);
        EXPECT_EQ(result.importMode, testCase.importMode);
    }
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

TEST(AssetImporterContractTest, ImportsLottieJsonAsDataWithoutGeneratingImageProxy)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("animation.lottie"));
    ASSERT_TRUE(writeTextFile(
        sourcePath,
        QByteArrayLiteral("{\"v\":\"5.7.4\",\"fr\":30,\"ip\":0,\"op\":60,\"layers\":[]}")));

    AssetImportSettings settings;
    settings.generateProxyOnImport = true;
    const QUuid id = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);

    EXPECT_EQ(AssetImporter::detectType(sourcePath), AssetType::Data);
    EXPECT_EQ(AssetDatabase::instance().getAssetInfo(id).type, AssetType::Data);
    EXPECT_EQ(AssetDatabase::instance().findAssetByPath(sourcePath), id);
    const ArtifactAssetMetaFile meta = ArtifactAssetMetaFile::load(sourcePath);
    ASSERT_TRUE(meta.isValid());
    EXPECT_EQ(meta.uuid(), id);
    EXPECT_EQ(meta.type(), AssetType::Data);
    EXPECT_TRUE(meta.proxyResolutions().isEmpty());
}

TEST(AssetImporterContractTest, RelativeInputResolvesAgainstCurrentDirectory)
{
    const QString temporaryTemplate = QDir::current().filePath(
        QStringLiteral("artifact-import-relative-XXXXXX"));
    QTemporaryDir directory(temporaryTemplate);
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("scene.json"));
    ASSERT_TRUE(writeTextFile(sourcePath, QByteArrayLiteral("{\"scene\":true}")));
    const QString relativePath = QDir::current().relativeFilePath(sourcePath);
    ASSERT_FALSE(QFileInfo(relativePath).isAbsolute());

    const AssetImportSettings settings;
    const QUuid id = AssetImporter::importFile(relativePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);

    EXPECT_EQ(AssetDatabase::instance().findAssetByPath(sourcePath), id);
    const auto meta = ArtifactAssetMetaFile::load(sourcePath);
    ASSERT_TRUE(meta.isValid());
    EXPECT_EQ(meta.uuid(), id);
    EXPECT_EQ(meta.sourcePath(), QFileInfo(sourcePath).absoluteFilePath());
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

TEST(AssetImporterContractTest, ReimportPreservesUserSidecarMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("annotated.json"));
    ASSERT_TRUE(writeTextFile(sourcePath, QByteArrayLiteral("{\"scene\":true}")));

    const AssetImportSettings settings;
    const QUuid id = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);
    auto meta = ArtifactAssetMetaFile::load(sourcePath);
    ASSERT_TRUE(meta.isValid());
    meta.addTag(QStringLiteral("  reviewed  "));
    meta.setCustomValue(QStringLiteral("review/owner"), QStringLiteral("artist"));
    ASSERT_TRUE(meta.save());

    ASSERT_EQ(AssetImporter::importFile(sourcePath, settings), id);

    const auto reimported = ArtifactAssetMetaFile::load(sourcePath);
    EXPECT_EQ(reimported.uuid(), id);
    EXPECT_EQ(reimported.tags(), (QStringList{QStringLiteral("reviewed")}));
    EXPECT_EQ(reimported.customValue(QStringLiteral("review/owner")).toString(),
              QStringLiteral("artist"));
}

TEST(AssetImporterContractTest, ReimportRepairsMalformedSidecar)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("recover.json"));
    ASSERT_TRUE(writeTextFile(sourcePath, QByteArrayLiteral("{\"scene\":true}")));
    ASSERT_TRUE(writeTextFile(ArtifactAssetMetaFile::metaPathFor(sourcePath),
                              QByteArrayLiteral("not-json")));

    const AssetImportSettings settings;
    const QUuid id = AssetImporter::importFile(sourcePath, settings);
    ASSERT_FALSE(id.isNull());
    RegisteredAssetGuard cleanup(AssetDatabase::instance(), id);

    const ArtifactAssetMetaFile repaired = ArtifactAssetMetaFile::load(sourcePath);
    ASSERT_TRUE(repaired.isValid());
    EXPECT_EQ(repaired.uuid(), id);
    EXPECT_EQ(repaired.type(), AssetType::Data);
    EXPECT_EQ(repaired.sourcePath(), QFileInfo(sourcePath).absoluteFilePath());
    EXPECT_TRUE(repaired.importedAt().isValid());
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

TEST(AssetImporterContractTest, RejectsBinaryPayloadWithSupportedImageExtensionWithoutSideEffects)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("opaque.png"));
    ASSERT_TRUE(writeTextFile(sourcePath, QByteArray::fromHex("00010203")));

    const AssetImportSettings settings;
    EXPECT_TRUE(AssetImporter::importFile(sourcePath, settings).isNull());
    EXPECT_TRUE(AssetDatabase::instance().findAssetByPath(sourcePath).isNull());
    EXPECT_FALSE(QFile::exists(
        ArtifactAssetMetaFile::metaPathFor(sourcePath)));
}
