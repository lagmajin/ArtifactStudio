#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSize>
#include <QStringList>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

import Artifact.Composition.InitParams;
import Artifact.Export.GamefaceWriter;
import Artifact.Export.LottieWriter;
import Artifact.Export.NoesisXamlWriter;
import Artifact.Export.RmlUiWriter;
import Artifact.Export.Session;
import Artifact.Export.UnityUxmlWriter;
import Artifact.Layer.InitParams;
import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Artifact.Layer.Text;
import Artifact.Project.Manager;
import Text.Style;
import Utils.String.UniString;

namespace {

class ExportWriterApplication final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char appName[] = "ArtifactExportWriterContractTest";
        static char* argv[] = {appName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

ExportWriterApplication* const exportWriterApplication =
    new ExportWriterApplication();
const bool exportWriterApplicationRegistered = [] {
    testing::AddGlobalTestEnvironment(exportWriterApplication);
    return true;
}();

QString readTextFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QByteArray readFileBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void expectNonEmptyFile(const QString& path)
{
    const QFileInfo info(path);
    EXPECT_TRUE(info.exists()) << path.toStdString();
    EXPECT_TRUE(info.isFile()) << path.toStdString();
    EXPECT_GT(info.size(), 0) << path.toStdString();
}

bool isWellFormedXml(const QString& path)
{
    QXmlStreamReader reader(readTextFile(path));
    while (!reader.atEnd()) {
        reader.readNext();
    }
    return !reader.hasError();
}

bool hasXmlRoot(const QString& path, const QString& localName,
                const QString& namespaceUri)
{
    QXmlStreamReader reader(readTextFile(path));
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            return reader.name() == localName &&
                   reader.namespaceUri().toString() == namespaceUri;
        }
    }
    return false;
}

} // namespace

TEST(ExportWriterContractTest, ExportSessionRejectsMissingCompositionAndCanRetry)
{
    Artifact::ArtifactExportSession session(Artifact::ArtifactCompositionPtr{});
    QString error;
    EXPECT_FALSE(session.build(&error));
    EXPECT_FALSE(session.isBuilt());
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(session.snapshot().layers.isEmpty());
    error.clear();
    EXPECT_FALSE(session.copyAssets(QString(), &error));
    EXPECT_FALSE(error.isEmpty());
}

TEST(ExportWriterContractTest,
     ExportSessionClassifiesSingleSvgLayerForPreRender)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QString svgPath = QDir(temporary.path()).filePath(
        QStringLiteral("only-vector.svg"));
    QFile svgFile(svgPath);
    ASSERT_TRUE(svgFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray svgBytes = QByteArrayLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"6\">"
        "<rect width=\"8\" height=\"6\" fill=\"#4080c0\"/>"
        "</svg>");
    ASSERT_EQ(svgFile.write(svgBytes), svgBytes.size());
    svgFile.close();

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("SingleSvgExportSessionContractTest"),
                           true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("SingleSvgContract")));
    compositionParams.setResolution(64, 36);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(1);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactSvgInitParams svgParams(QStringLiteral("OnlySvg"));
    svgParams.setSvgPath(svgPath);
    const auto svgResult = projects.addLayerToComposition(
        composition.id, svgParams);
    ASSERT_TRUE(svgResult.success);

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();

    const auto& snapshot = session.snapshot();
    ASSERT_EQ(snapshot.layers.size(), 1);
    const auto& layer = snapshot.layers.front();
    EXPECT_EQ(layer.name, QStringLiteral("OnlySvg"));
    EXPECT_EQ(layer.serialized.value(QStringLiteral("svg.sourcePath")).toString(),
              svgPath);
    EXPECT_TRUE(layer.requiresPreRender);
    EXPECT_EQ(layer.preRenderReason, QStringLiteral("SVGレイヤー"));
    EXPECT_TRUE(snapshot.assets.isEmpty())
        << "SVG source files are inputs to pre-rendering, not copied image assets";
    EXPECT_TRUE(std::any_of(snapshot.warnings.cbegin(), snapshot.warnings.cend(),
        [](const QString& warning) {
            return warning.contains(QStringLiteral("OnlySvg")) &&
                   warning.contains(QStringLiteral("SVGレイヤー"));
        }));
}

TEST(ExportWriterContractTest,
     ExportSessionMissingAssetPreservesExistingCopiedFile)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QString sourcePath = QDir(temporary.path()).filePath(
        QStringLiteral("source.png"));
    QImage source(2, 2, QImage::Format_RGBA8888);
    source.fill(QColor(64, 128, 192, 255));
    ASSERT_TRUE(source.save(sourcePath));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("MissingAssetExportSessionContractTest"),
                           true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("MissingAssetContract")));
    compositionParams.setResolution(16, 16);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(1);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("SourceImage"));
    imageParams.setImagePath(sourcePath);
    ASSERT_TRUE(projects.addLayerToComposition(composition.id, imageParams).success);

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_EQ(session.snapshot().assets.size(), 1);
    const QString relativePath = session.snapshot().assets.front().relativePath;
    EXPECT_EQ(session.assetPathFor(sourcePath), relativePath);

    ASSERT_TRUE(QFile::remove(sourcePath));
    const QString outputDirectory = QDir(temporary.path()).filePath(
        QStringLiteral("existing-output"));
    const QString protectedPath = QDir(outputDirectory).filePath(relativePath);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(protectedPath).absolutePath()));
    const QByteArray sentinel = QByteArrayLiteral("existing asset survives");
    QFile protectedAsset(protectedPath);
    ASSERT_TRUE(protectedAsset.open(QIODevice::WriteOnly));
    ASSERT_EQ(protectedAsset.write(sentinel), sentinel.size());
    protectedAsset.close();

    error.clear();
    EXPECT_FALSE(session.copyAssets(outputDirectory, &error));
    EXPECT_TRUE(error.contains(sourcePath));
    EXPECT_EQ(readFileBytes(protectedPath), sentinel);
}

void expectWriterRejectsMissingComposition(const QString& writerName)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QByteArray sentinel = QByteArrayLiteral("existing output survives");
    const auto createProtectedOutput = [&](const QString& directory,
                                          const QString& fileName,
                                          QString& path) {
        path = QDir(directory).filePath(fileName);
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            return false;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(sentinel) != sentinel.size()) {
            return false;
        }
        file.close();
        return true;
    };
    const auto expectRejectedWithoutMutation = [&](const QString& path,
                                                    const auto& writer) {
        QString error;
        EXPECT_FALSE(writer(&error));
        EXPECT_FALSE(error.isEmpty());
        EXPECT_EQ(readFileBytes(path), sentinel);
    };
    const Artifact::ArtifactCompositionPtr missingComposition;

    const QString outputDirectory = QDir(temporary.path()).filePath(writerName);
    QString protectedPath;
    if (writerName == QStringLiteral("Gameface")) {
        ASSERT_TRUE(createProtectedOutput(outputDirectory,
            QStringLiteral("index.html"), protectedPath));
        expectRejectedWithoutMutation(protectedPath, [&](QString* error) {
            return Artifact::ArtifactExportGamefaceWriter::write(
                missingComposition, outputDirectory, {}, error);
        });
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        ASSERT_TRUE(createProtectedOutput(outputDirectory,
            QStringLiteral("Main.xaml"), protectedPath));
        expectRejectedWithoutMutation(protectedPath, [&](QString* error) {
            return Artifact::ArtifactExportNoesisXamlWriter::write(
                missingComposition, outputDirectory, {}, error);
        });
    } else if (writerName == QStringLiteral("RmlUi")) {
        ASSERT_TRUE(createProtectedOutput(outputDirectory,
            QStringLiteral("main.rml"), protectedPath));
        expectRejectedWithoutMutation(protectedPath, [&](QString* error) {
            return Artifact::ArtifactExportRmlUiWriter::write(
                missingComposition, outputDirectory, {}, error);
        });
    } else if (writerName == QStringLiteral("UnityUxml")) {
        ASSERT_TRUE(createProtectedOutput(outputDirectory,
            QStringLiteral("Main.uxml"), protectedPath));
        expectRejectedWithoutMutation(protectedPath, [&](QString* error) {
            return Artifact::ArtifactExportUnityUxmlWriter::write(
                missingComposition, outputDirectory, {}, error);
        });
    } else if (writerName == QStringLiteral("Lottie")) {
        protectedPath = QDir(temporary.path()).filePath(
            QStringLiteral("animation.json"));
        ASSERT_TRUE(createProtectedOutput(temporary.path(),
            QStringLiteral("animation.json"), protectedPath));
        expectRejectedWithoutMutation(protectedPath, [&](QString* error) {
            return Artifact::ArtifactExportLottieWriter::write(
                missingComposition, protectedPath, {}, error);
        });
    } else {
        FAIL() << "unknown writer: " << writerName.toStdString();
    }
}

TEST(ExportWriterContractTest, WritersRejectMissingCompositionBeforeTouchingOutputs)
{
    for (const QString& writerName : {QStringLiteral("Gameface"),
                                      QStringLiteral("NoesisXaml"),
                                      QStringLiteral("RmlUi"),
                                      QStringLiteral("UnityUxml"),
                                      QStringLiteral("Lottie")}) {
        expectWriterRejectsMissingComposition(writerName);
    }
}

TEST(ExportWriterContractTest, GamefaceRejectsMissingCompositionBeforeTouchingOutput)
{
    expectWriterRejectsMissingComposition(QStringLiteral("Gameface"));
}

TEST(ExportWriterContractTest, NoesisXamlRejectsMissingCompositionBeforeTouchingOutput)
{
    expectWriterRejectsMissingComposition(QStringLiteral("NoesisXaml"));
}

TEST(ExportWriterContractTest, RmlUiRejectsMissingCompositionBeforeTouchingOutput)
{
    expectWriterRejectsMissingComposition(QStringLiteral("RmlUi"));
}

TEST(ExportWriterContractTest, UnityUxmlRejectsMissingCompositionBeforeTouchingOutput)
{
    expectWriterRejectsMissingComposition(QStringLiteral("UnityUxml"));
}

TEST(ExportWriterContractTest, LottieRejectsMissingCompositionBeforeTouchingOutput)
{
    expectWriterRejectsMissingComposition(QStringLiteral("Lottie"));
}

void expectWriterRejectsBlockedOutputParent(const QString& writerName)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("BlockedWriterOutputContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("BlockedOutputContract")));
    compositionParams.setResolution(16, 16);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(1);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);

    const QString blockedParent = QDir(temporary.path()).filePath(
        QStringLiteral("blocked-output-parent"));
    const QByteArray sentinel = QByteArrayLiteral("keep output parent intact");
    QFile parentFile(blockedParent);
    ASSERT_TRUE(parentFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(parentFile.write(sentinel), sentinel.size());
    parentFile.close();

    const QString output = QDir(blockedParent).filePath(writerName);
    QString error;
    bool succeeded = false;
    if (writerName == QStringLiteral("Gameface")) {
        succeeded = Artifact::ArtifactExportGamefaceWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        succeeded = Artifact::ArtifactExportNoesisXamlWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("RmlUi")) {
        succeeded = Artifact::ArtifactExportRmlUiWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("UnityUxml")) {
        succeeded = Artifact::ArtifactExportUnityUxmlWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("Lottie")) {
        succeeded = Artifact::ArtifactExportLottieWriter::write(
            compositionPtr, QDir(output).filePath(QStringLiteral("animation.json")),
            {}, &error);
    } else {
        FAIL() << "unknown writer: " << writerName.toStdString();
        return;
    }
    EXPECT_FALSE(succeeded);
    EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(readFileBytes(blockedParent), sentinel);
}

TEST(ExportWriterContractTest, GamefaceRejectsBlockedOutputParent)
{
    expectWriterRejectsBlockedOutputParent(QStringLiteral("Gameface"));
}

TEST(ExportWriterContractTest, NoesisXamlRejectsBlockedOutputParent)
{
    expectWriterRejectsBlockedOutputParent(QStringLiteral("NoesisXaml"));
}

TEST(ExportWriterContractTest, RmlUiRejectsBlockedOutputParent)
{
    expectWriterRejectsBlockedOutputParent(QStringLiteral("RmlUi"));
}

TEST(ExportWriterContractTest, UnityUxmlRejectsBlockedOutputParent)
{
    expectWriterRejectsBlockedOutputParent(QStringLiteral("UnityUxml"));
}

TEST(ExportWriterContractTest, LottieRejectsBlockedOutputParent)
{
    expectWriterRejectsBlockedOutputParent(QStringLiteral("Lottie"));
}

void expectWriterReportsFinalFileWriteFailure(const QString& writerName)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("FinalFileWriteFailureContractTest"),
                           true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("FinalFileWriteFailureContract")));
    compositionParams.setResolution(16, 16);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(1);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);

    const QString outputDirectory = QDir(temporary.path()).filePath(writerName);
    ASSERT_TRUE(QDir().mkpath(outputDirectory));
    QString outputPath;
    QString writerOutput = outputDirectory;
    if (writerName == QStringLiteral("Gameface")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("styles.css"));
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        outputPath = QDir(outputDirectory).filePath(
            QStringLiteral("composition_styles.xaml"));
    } else if (writerName == QStringLiteral("RmlUi")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("composition.rcss"));
    } else if (writerName == QStringLiteral("UnityUxml")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("composition.uss"));
    } else if (writerName == QStringLiteral("Lottie")) {
        outputPath = QDir(temporary.path()).filePath(QStringLiteral("animation.json"));
        writerOutput = outputPath;
    } else {
        FAIL() << "unknown writer: " << writerName.toStdString();
        return;
    }
    ASSERT_TRUE(QDir().mkpath(outputPath));

    QString error;
    bool succeeded = false;
    if (writerName == QStringLiteral("Gameface")) {
        succeeded = Artifact::ArtifactExportGamefaceWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        succeeded = Artifact::ArtifactExportNoesisXamlWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else if (writerName == QStringLiteral("RmlUi")) {
        succeeded = Artifact::ArtifactExportRmlUiWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else if (writerName == QStringLiteral("UnityUxml")) {
        succeeded = Artifact::ArtifactExportUnityUxmlWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else {
        succeeded = Artifact::ArtifactExportLottieWriter::write(
            compositionPtr, writerOutput, {}, &error);
    }
    EXPECT_FALSE(succeeded);
    EXPECT_FALSE(error.isEmpty());
}

TEST(ExportWriterContractTest, GamefaceReportsFinalFileWriteFailure)
{
    expectWriterReportsFinalFileWriteFailure(QStringLiteral("Gameface"));
}

TEST(ExportWriterContractTest, NoesisXamlReportsFinalFileWriteFailure)
{
    expectWriterReportsFinalFileWriteFailure(QStringLiteral("NoesisXaml"));
}

TEST(ExportWriterContractTest, RmlUiReportsFinalFileWriteFailure)
{
    expectWriterReportsFinalFileWriteFailure(QStringLiteral("RmlUi"));
}

TEST(ExportWriterContractTest, UnityUxmlReportsFinalFileWriteFailure)
{
    expectWriterReportsFinalFileWriteFailure(QStringLiteral("UnityUxml"));
}

TEST(ExportWriterContractTest, LottieReportsFinalFileWriteFailure)
{
    expectWriterReportsFinalFileWriteFailure(QStringLiteral("Lottie"));
}

TEST(ExportWriterContractTest, MinimalTextCompositionProducesWriterSchemas)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("ExportWriterContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("ExportContract")));
    compositionParams.setResolution(320, 180);
    compositionParams.setFrameRate(30.0);
    compositionParams.setDurationFrames(30);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("ContractText"));
    const auto textResult = projects.addLayerToComposition(composition.id, textParams);
    ASSERT_TRUE(textResult.success);
    auto* textLayer = dynamic_cast<Artifact::ArtifactTextLayer*>(textResult.layer.get());
    ASSERT_NE(textLayer, nullptr);
    textLayer->setText(ArtifactCore::UniString(QStringLiteral("Export schema marker")));
    textLayer->setFontFamily(ArtifactCore::UniString(QStringLiteral("Contract Sans")));
    textLayer->setFontSize(37.0f);
    textLayer->setHorizontalAlignment(ArtifactCore::TextHorizontalAlignment::Center);

    const QString root = temporary.path();
    const QString firstAssetDirectory = QDir(root).filePath(QStringLiteral("first"));
    const QString secondAssetDirectory = QDir(root).filePath(QStringLiteral("second"));
    ASSERT_TRUE(QDir().mkpath(firstAssetDirectory));
    ASSERT_TRUE(QDir().mkpath(secondAssetDirectory));
    QImage firstImage(1, 1, QImage::Format_RGBA8888);
    firstImage.fill(QColor(220, 30, 60, 255));
    QImage secondImage(1, 1, QImage::Format_RGBA8888);
    secondImage.fill(QColor(20, 100, 230, 255));
    const QString firstSourcePath =
        QDir(firstAssetDirectory).filePath(QStringLiteral("shared.png"));
    const QString secondSourcePath =
        QDir(secondAssetDirectory).filePath(QStringLiteral("shared.png"));
    ASSERT_TRUE(firstImage.save(firstSourcePath, "PNG"));
    ASSERT_TRUE(secondImage.save(secondSourcePath, "PNG"));

    Artifact::ArtifactImageInitParams firstImageParams(QStringLiteral("FirstAsset"));
    firstImageParams.setImagePath(firstSourcePath);
    const auto firstImageResult =
        projects.addLayerToComposition(composition.id, firstImageParams);
    ASSERT_TRUE(firstImageResult.success);
    Artifact::ArtifactImageInitParams secondImageParams(QStringLiteral("SecondAsset"));
    secondImageParams.setImagePath(secondSourcePath);
    const auto secondImageResult =
        projects.addLayerToComposition(composition.id, secondImageParams);
    ASSERT_TRUE(secondImageResult.success);
    Artifact::ArtifactImageInitParams repeatedImageParams(
        QStringLiteral("RepeatedFirstAsset"));
    repeatedImageParams.setImagePath(firstSourcePath);
    const auto repeatedImageResult =
        projects.addLayerToComposition(composition.id, repeatedImageParams);
    ASSERT_TRUE(repeatedImageResult.success);

    const QString gameface = QDir(root).filePath(QStringLiteral("gameface"));
    const QString noesis = QDir(root).filePath(QStringLiteral("noesis"));
    const QString rmlui = QDir(root).filePath(QStringLiteral("rmlui"));
    const QString unity = QDir(root).filePath(QStringLiteral("unity"));
    const QString lottie = QDir(root).filePath(QStringLiteral("composition.json"));
    QString error;

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);

    Artifact::ArtifactExportSession session(compositionPtr);
    EXPECT_FALSE(session.isBuilt());
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_TRUE(session.isBuilt());
    const auto& snapshot = session.snapshot();
    EXPECT_EQ(snapshot.name, QStringLiteral("ExportContract"));
    EXPECT_EQ(snapshot.size, QSize(320, 180));
    EXPECT_DOUBLE_EQ(snapshot.frameRate, 30.0);
    EXPECT_EQ(snapshot.inPoint, 0);
    EXPECT_EQ(snapshot.outPoint, 30);
    ASSERT_EQ(snapshot.layers.size(), 4);
    const auto textLayerIt = std::find_if(snapshot.layers.cbegin(), snapshot.layers.cend(),
        [](const Artifact::ArtifactExportLayerSnapshot& layer) {
            return layer.name == QStringLiteral("ContractText");
        });
    ASSERT_NE(textLayerIt, snapshot.layers.cend());
    EXPECT_FALSE(textLayerIt->id.isEmpty());
    EXPECT_EQ(textLayerIt->type, textLayerIt->serialized.value(
        QStringLiteral("type")).toInt());
    EXPECT_TRUE(textLayerIt->visible);
    EXPECT_FALSE(textLayerIt->is3D);
    EXPECT_FALSE(textLayerIt->requiresPreRender);
    EXPECT_GE(textLayerIt->inPoint, snapshot.inPoint);
    EXPECT_GT(textLayerIt->outPoint, textLayerIt->inPoint);
    EXPECT_EQ(textLayerIt->serialized.value(QStringLiteral("name")).toString(),
              QStringLiteral("ContractText"));
    ASSERT_EQ(snapshot.assets.size(), 2);
    const QString firstRelativePath = session.assetPathFor(firstSourcePath);
    const QString secondRelativePath = session.assetPathFor(secondSourcePath);
    EXPECT_TRUE(firstRelativePath.startsWith(QStringLiteral("assets/")));
    EXPECT_TRUE(secondRelativePath.startsWith(QStringLiteral("assets/")));
    EXPECT_NE(firstRelativePath, secondRelativePath);
    EXPECT_EQ(session.assetPathFor(firstSourcePath), firstRelativePath);
    EXPECT_EQ(session.assetPathFor(secondSourcePath), secondRelativePath);
    EXPECT_EQ(QFileInfo(firstRelativePath).suffix(), QStringLiteral("png"));
    EXPECT_EQ(QFileInfo(secondRelativePath).suffix(), QStringLiteral("png"));
    EXPECT_EQ(QDir::cleanPath(firstRelativePath), firstRelativePath);
    EXPECT_EQ(QDir::cleanPath(secondRelativePath), secondRelativePath);
    EXPECT_FALSE(firstRelativePath.contains(QStringLiteral("..")));
    EXPECT_FALSE(secondRelativePath.contains(QStringLiteral("..")));
    EXPECT_TRUE(session.assetPathFor(QString()).isEmpty());
    const QString copiedAssetsDirectory = QDir(root).filePath(QStringLiteral("copied"));
    ASSERT_TRUE(session.copyAssets(copiedAssetsDirectory, &error)) << error.toStdString();
    EXPECT_EQ(readFileBytes(QDir(copiedAssetsDirectory).filePath(firstRelativePath)),
              readFileBytes(firstSourcePath));
    EXPECT_EQ(readFileBytes(QDir(copiedAssetsDirectory).filePath(secondRelativePath)),
              readFileBytes(secondSourcePath));
    EXPECT_EQ(QDir(QDir(copiedAssetsDirectory).filePath(QStringLiteral("assets")))
                  .entryList(QDir::Files | QDir::NoDotAndDotDot).size(),
              2);

    QStringList initialLayerIds;
    for (const auto& layer : snapshot.layers) {
        initialLayerIds.push_back(layer.id);
    }
    const QStringList initialAssetPaths{
        snapshot.assets.at(0).relativePath,
        snapshot.assets.at(1).relativePath};
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_TRUE(session.isBuilt());
    ASSERT_EQ(snapshot.layers.size(), initialLayerIds.size());
    ASSERT_EQ(snapshot.assets.size(), initialAssetPaths.size());
    for (qsizetype index = 0; index < initialLayerIds.size(); ++index) {
        EXPECT_EQ(snapshot.layers.at(index).id, initialLayerIds.at(index));
    }
    for (qsizetype index = 0; index < initialAssetPaths.size(); ++index) {
        EXPECT_EQ(snapshot.assets.at(index).relativePath,
                  initialAssetPaths.at(index));
    }

    const QString unsafeAssetSource = QDir(root).filePath(
        QStringLiteral(".. escape image.png"));
    QImage unsafeNamedImage(1, 1, QImage::Format_RGBA8888);
    unsafeNamedImage.fill(QColor(70, 80, 90, 255));
    ASSERT_TRUE(unsafeNamedImage.save(unsafeAssetSource, "PNG"));
    Artifact::ArtifactCompositionInitParams pathSafetyCompositionParams;
    pathSafetyCompositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("ExportPathSafety")));
    pathSafetyCompositionParams.setResolution(16, 16);
    pathSafetyCompositionParams.setFrameRate(24.0);
    pathSafetyCompositionParams.setDurationFrames(1);
    const auto pathSafetyComposition =
        projects.createComposition(pathSafetyCompositionParams);
    ASSERT_TRUE(pathSafetyComposition.success);
    Artifact::ArtifactImageInitParams unsafeAssetParams(
        QStringLiteral("UnsafeAssetName"));
    unsafeAssetParams.setImagePath(unsafeAssetSource);
    ASSERT_TRUE(projects.addLayerToComposition(
        pathSafetyComposition.id, unsafeAssetParams).success);
    const auto pathSafetyCompositionResult =
        projects.findComposition(pathSafetyComposition.id);
    ASSERT_TRUE(pathSafetyCompositionResult.success);
    const auto pathSafetyCompositionPtr =
        pathSafetyCompositionResult.ptr.lock();
    ASSERT_TRUE(pathSafetyCompositionPtr);
    Artifact::ArtifactExportSession pathSafetySession(
        pathSafetyCompositionPtr);
    ASSERT_TRUE(pathSafetySession.build(&error)) << error.toStdString();
    const QString sanitizedRelativePath =
        pathSafetySession.assetPathFor(unsafeAssetSource);
    EXPECT_TRUE(sanitizedRelativePath.startsWith(QStringLiteral("assets/")));
    EXPECT_FALSE(sanitizedRelativePath.contains(QStringLiteral("..")));
    EXPECT_FALSE(sanitizedRelativePath.contains(QLatin1Char(' ')));
    const QString pathSafetyOutput = QDir(root).filePath(QStringLiteral("path_safety"));
    ASSERT_TRUE(pathSafetySession.copyAssets(pathSafetyOutput, &error))
        << error.toStdString();
    const QString copiedUnsafeAsset =
        QDir(pathSafetyOutput).absoluteFilePath(sanitizedRelativePath);
    EXPECT_TRUE(copiedUnsafeAsset.startsWith(
        QDir(pathSafetyOutput).absoluteFilePath(QStringLiteral("assets")) +
        QDir::separator()));
    EXPECT_EQ(readFileBytes(copiedUnsafeAsset), readFileBytes(unsafeAssetSource));

    const QString gamefaceWithAssets = QDir(root).filePath(QStringLiteral("gameface_assets"));
    const QString noesisWithAssets = QDir(root).filePath(QStringLiteral("noesis_assets"));
    const QString rmlWithAssets = QDir(root).filePath(QStringLiteral("rml_assets"));
    const QString unityWithAssets = QDir(root).filePath(QStringLiteral("unity_assets"));
    error.clear();
    ASSERT_TRUE(Artifact::ArtifactExportGamefaceWriter::write(
        compositionPtr, gamefaceWithAssets, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportNoesisXamlWriter::write(
        compositionPtr, noesisWithAssets, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportRmlUiWriter::write(
        compositionPtr, rmlWithAssets, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportUnityUxmlWriter::write(
        compositionPtr, unityWithAssets, &error)) << error.toStdString();

    const auto expectCopiedAsset = [&](const QString& outputDirectory,
                                       const QString& relativePath,
                                       const QString& sourcePath) {
        const QString copiedPath = QDir(outputDirectory).filePath(relativePath);
        EXPECT_TRUE(QFileInfo::exists(copiedPath)) << copiedPath.toStdString();
        EXPECT_EQ(readFileBytes(copiedPath), readFileBytes(sourcePath))
            << copiedPath.toStdString();
    };
    expectCopiedAsset(gamefaceWithAssets, firstRelativePath, firstSourcePath);
    expectCopiedAsset(gamefaceWithAssets, secondRelativePath, secondSourcePath);
    expectCopiedAsset(noesisWithAssets, firstRelativePath, firstSourcePath);
    expectCopiedAsset(noesisWithAssets, secondRelativePath, secondSourcePath);
    expectCopiedAsset(rmlWithAssets, firstRelativePath, firstSourcePath);
    expectCopiedAsset(rmlWithAssets, secondRelativePath, secondSourcePath);
    expectCopiedAsset(unityWithAssets, firstRelativePath, firstSourcePath);
    expectCopiedAsset(unityWithAssets, secondRelativePath, secondSourcePath);
    expectNonEmptyFile(QDir(unityWithAssets).filePath(firstRelativePath +
                                                       QStringLiteral(".meta")));
    expectNonEmptyFile(QDir(unityWithAssets).filePath(secondRelativePath +
                                                       QStringLiteral(".meta")));
    const QString gamefaceMarkup = readTextFile(
        QDir(gamefaceWithAssets).filePath(QStringLiteral("index.html")));
    const QString noesisMarkupWithAssets = readTextFile(
        QDir(noesisWithAssets).filePath(QStringLiteral("composition.xaml")));
    const QString rmlMarkupWithAssets = readTextFile(
        QDir(rmlWithAssets).filePath(QStringLiteral("composition.rml")));
    const QString unityMarkupWithAssets = readTextFile(
        QDir(unityWithAssets).filePath(QStringLiteral("composition.uxml")));
    EXPECT_FALSE(gamefaceMarkup.isEmpty());
    EXPECT_FALSE(noesisMarkupWithAssets.isEmpty());
    EXPECT_FALSE(rmlMarkupWithAssets.isEmpty());
    EXPECT_FALSE(unityMarkupWithAssets.isEmpty());
    EXPECT_TRUE(isWellFormedXml(QDir(noesisWithAssets).filePath(
        QStringLiteral("composition.xaml"))));
    EXPECT_TRUE(isWellFormedXml(QDir(rmlWithAssets).filePath(
        QStringLiteral("composition.rml"))));
    EXPECT_TRUE(isWellFormedXml(QDir(unityWithAssets).filePath(
        QStringLiteral("composition.uxml"))));
    EXPECT_TRUE(hasXmlRoot(QDir(noesisWithAssets).filePath(
        QStringLiteral("composition.xaml")), QStringLiteral("Canvas"),
        QStringLiteral("http://schemas.microsoft.com/winfx/2006/xaml/presentation")));
    EXPECT_TRUE(hasXmlRoot(QDir(rmlWithAssets).filePath(
        QStringLiteral("composition.rml")), QStringLiteral("rml"),
        QStringLiteral("")));
    EXPECT_TRUE(hasXmlRoot(QDir(unityWithAssets).filePath(
        QStringLiteral("composition.uxml")), QStringLiteral("UXML"),
        QStringLiteral("UnityEngine.UIElements")));
    EXPECT_TRUE(gamefaceMarkup.contains(QStringLiteral("<!doctype html>"),
                                        Qt::CaseInsensitive));
    EXPECT_TRUE(gamefaceMarkup.contains(QStringLiteral("<div id=\"composition\"")));
    EXPECT_TRUE(gamefaceMarkup.contains(QStringLiteral("Export schema marker")));
    const QString gamefaceCss = readTextFile(
        QDir(gamefaceWithAssets).filePath(QStringLiteral("styles.css")));
    const QString unityStyles = readTextFile(
        QDir(unityWithAssets).filePath(QStringLiteral("composition.uss")));
    const QString noesisStyles = readTextFile(
        QDir(noesisWithAssets).filePath(QStringLiteral("composition_styles.xaml")));
    EXPECT_TRUE(gamefaceCss.contains(QStringLiteral(".composition{")));
    EXPECT_TRUE(unityStyles.contains(QStringLiteral(".composition{")));
    EXPECT_TRUE(isWellFormedXml(QDir(noesisWithAssets).filePath(
        QStringLiteral("composition_styles.xaml"))));
    EXPECT_TRUE(hasXmlRoot(QDir(noesisWithAssets).filePath(
        QStringLiteral("composition_styles.xaml")),
        QStringLiteral("ResourceDictionary"),
        QStringLiteral("http://schemas.microsoft.com/winfx/2006/xaml/presentation")));
    EXPECT_TRUE(noesisMarkupWithAssets.contains(QStringLiteral("ContractText")));
    EXPECT_TRUE(unityMarkupWithAssets.contains(QStringLiteral("Export schema marker")));
    const QString gamefaceScript = readTextFile(
        QDir(gamefaceWithAssets).filePath(QStringLiteral("app.js")));
    EXPECT_TRUE(gamefaceScript.contains(QStringLiteral("frameRate: 30")));
    EXPECT_TRUE(unityStyles.contains(firstRelativePath));
    EXPECT_TRUE(unityStyles.contains(secondRelativePath));
    for (const QString& relativePath : {firstRelativePath, secondRelativePath}) {
        EXPECT_TRUE(gamefaceMarkup.contains(relativePath)) << relativePath.toStdString();
        EXPECT_TRUE(noesisMarkupWithAssets.contains(relativePath))
            << relativePath.toStdString();
        EXPECT_TRUE(rmlMarkupWithAssets.contains(relativePath))
            << relativePath.toStdString();
        EXPECT_TRUE(unityMarkupWithAssets.contains(relativePath))
            << relativePath.toStdString();
    }

    const QString externalLottie =
        QDir(root).filePath(QStringLiteral("external/composition.json"));
    Artifact::ArtifactLottieExportOptions externalImageOptions;
    externalImageOptions.embedImages = false;
    error.clear();
    ASSERT_TRUE(Artifact::ArtifactExportLottieWriter::write(
        compositionPtr, externalLottie, externalImageOptions, &error))
        << error.toStdString();
    QFile externalLottieFile(externalLottie);
    ASSERT_TRUE(externalLottieFile.open(QIODevice::ReadOnly));
    QJsonParseError externalParseError;
    const QJsonDocument externalDocument = QJsonDocument::fromJson(
        externalLottieFile.readAll(), &externalParseError);
    ASSERT_EQ(externalParseError.error, QJsonParseError::NoError)
        << externalParseError.errorString().toStdString();
    ASSERT_TRUE(externalDocument.isObject());
    const QJsonObject externalRoot = externalDocument.object();
    EXPECT_FALSE(externalRoot.value(QStringLiteral("v")).toString().isEmpty());
    EXPECT_EQ(externalRoot.value(QStringLiteral("fr")).toDouble(), 30.0);
    EXPECT_EQ(externalRoot.value(QStringLiteral("ip")).toInt(), 0);
    EXPECT_EQ(externalRoot.value(QStringLiteral("op")).toInt(), 30);
    EXPECT_EQ(externalRoot.value(QStringLiteral("w")).toInt(), 320);
    EXPECT_EQ(externalRoot.value(QStringLiteral("h")).toInt(), 180);
    EXPECT_EQ(externalRoot.value(QStringLiteral("nm")).toString(),
              QStringLiteral("ExportContract"));
    const QJsonArray externalLayers =
        externalRoot.value(QStringLiteral("layers")).toArray();
    const QJsonArray externalAssets =
        externalRoot.value(QStringLiteral("assets")).toArray();
    ASSERT_EQ(externalLayers.size(), 4);
    ASSERT_EQ(externalAssets.size(), 3);
    QStringList externalAssetIds;
    QStringList externalAssetNames;
    const QByteArray firstExternalSourceBytes = readFileBytes(firstSourcePath);
    const QByteArray secondExternalSourceBytes = readFileBytes(secondSourcePath);
    int firstExternalAssetCount = 0;
    int secondExternalAssetCount = 0;
    for (const QJsonValue& value : externalAssets) {
        const QJsonObject asset = value.toObject();
        const QString assetId = asset.value(QStringLiteral("id")).toString();
        const QString assetName = asset.value(QStringLiteral("p")).toString();
        ASSERT_FALSE(assetId.isEmpty());
        ASSERT_FALSE(assetName.isEmpty());
        EXPECT_EQ(asset.value(QStringLiteral("w")).toInt(), 1);
        EXPECT_EQ(asset.value(QStringLiteral("h")).toInt(), 1);
        EXPECT_EQ(asset.value(QStringLiteral("u")).toString(),
                  QStringLiteral("assets/"));
        EXPECT_EQ(QFileInfo(assetName).fileName(), assetName);
        EXPECT_EQ(QDir::cleanPath(assetName), assetName);
        EXPECT_FALSE(assetName.contains(QStringLiteral("..")));
        externalAssetIds.push_back(assetId);
        externalAssetNames.push_back(assetName);
        const QString copiedAssetPath = QDir(QFileInfo(externalLottie).absolutePath())
            .filePath(QStringLiteral("assets/") + assetName);
        ASSERT_TRUE(QFileInfo::exists(copiedAssetPath));
        const QByteArray copiedBytes = readFileBytes(copiedAssetPath);
        if (copiedBytes == firstExternalSourceBytes) {
            ++firstExternalAssetCount;
        } else if (copiedBytes == secondExternalSourceBytes) {
            ++secondExternalAssetCount;
        } else {
            ADD_FAILURE() << "external Lottie asset bytes match neither source";
        }
    }
    EXPECT_EQ(firstExternalAssetCount, 2);
    EXPECT_EQ(secondExternalAssetCount, 1);
    for (int index = 0; index < externalAssetIds.size(); ++index) {
        for (int previous = 0; previous < index; ++previous) {
            EXPECT_NE(externalAssetIds.at(index), externalAssetIds.at(previous));
        }
    }
    QStringList uniqueExternalAssetNames = externalAssetNames;
    uniqueExternalAssetNames.removeDuplicates();
    EXPECT_EQ(uniqueExternalAssetNames.size(), 2);
    QStringList externalImageReferences;
    for (const QJsonValue& value : externalLayers) {
        const QJsonObject layer = value.toObject();
        if (layer.value(QStringLiteral("ty")).toInt() == 2) {
            externalImageReferences.push_back(
                layer.value(QStringLiteral("refId")).toString());
        }
    }
    ASSERT_EQ(externalImageReferences.size(), 3);
    QStringList uniqueExternalImageReferences = externalImageReferences;
    uniqueExternalImageReferences.removeDuplicates();
    EXPECT_EQ(uniqueExternalImageReferences.size(), 3);
    externalAssetIds.sort();
    externalImageReferences.sort();
    EXPECT_EQ(externalImageReferences, externalAssetIds);

    const QByteArray copiedFirstBytes =
        readFileBytes(QDir(copiedAssetsDirectory).filePath(firstRelativePath));
    QFile changedSource(firstSourcePath);
    ASSERT_TRUE(changedSource.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray changedSourceBytes = QByteArrayLiteral("new source contents");
    ASSERT_EQ(changedSource.write(changedSourceBytes), changedSourceBytes.size());
    changedSource.close();
    error.clear();
    ASSERT_TRUE(session.copyAssets(copiedAssetsDirectory, &error))
        << error.toStdString();
    EXPECT_EQ(readFileBytes(QDir(copiedAssetsDirectory).filePath(firstRelativePath)),
              copiedFirstBytes)
        << "existing copied assets are preserved on a repeated copy";
    EXPECT_EQ(readFileBytes(firstSourcePath), changedSourceBytes);

    Artifact::ArtifactCompositionInitParams missingAssetCompositionParams;
    missingAssetCompositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("MissingAssetContract")));
    missingAssetCompositionParams.setResolution(32, 32);
    missingAssetCompositionParams.setFrameRate(24.0);
    missingAssetCompositionParams.setDurationFrames(1);
    const auto missingAssetComposition =
        projects.createComposition(missingAssetCompositionParams);
    ASSERT_TRUE(missingAssetComposition.success);
    Artifact::ArtifactImageInitParams missingAssetParams(QStringLiteral("MissingAsset"));
    missingAssetParams.setImagePath(secondSourcePath);
    ASSERT_TRUE(projects.addLayerToComposition(
        missingAssetComposition.id, missingAssetParams).success);
    const auto missingAssetCompositionResult =
        projects.findComposition(missingAssetComposition.id);
    ASSERT_TRUE(missingAssetCompositionResult.success);
    const auto missingAssetCompositionPtr =
        missingAssetCompositionResult.ptr.lock();
    ASSERT_TRUE(missingAssetCompositionPtr);
    Artifact::ArtifactExportSession missingAssetSession(
        missingAssetCompositionPtr);
    ASSERT_TRUE(missingAssetSession.build(&error)) << error.toStdString();
    ASSERT_EQ(missingAssetSession.snapshot().assets.size(), 1);

    QFile::remove(secondSourcePath);
    const QString missingAssetRelativePath =
        missingAssetSession.snapshot().assets.front().relativePath;
    const QString protectedAssetDirectory =
        QDir(root).filePath(QStringLiteral("protected"));
    const QString protectedAssetPath =
        QDir(protectedAssetDirectory).filePath(missingAssetRelativePath);
    ASSERT_TRUE(QDir().mkpath(QFileInfo(protectedAssetPath).absolutePath()));
    const QByteArray protectedBytes = QByteArrayLiteral("preserve-existing-asset");
    QFile protectedAsset(protectedAssetPath);
    ASSERT_TRUE(protectedAsset.open(QIODevice::WriteOnly));
    ASSERT_EQ(protectedAsset.write(protectedBytes), protectedBytes.size());
    protectedAsset.close();
    error.clear();
    EXPECT_FALSE(missingAssetSession.copyAssets(protectedAssetDirectory, &error));
    EXPECT_TRUE(error.contains(secondSourcePath));
    EXPECT_EQ(readFileBytes(protectedAssetPath), protectedBytes);

    const auto protectWriterOutput = [&](const QString& outputDirectory,
                                         const QString& fileName,
                                         QString& protectedPath) {
        protectedPath = QDir(outputDirectory).filePath(fileName);
        if (!QDir().mkpath(QFileInfo(protectedPath).absolutePath())) {
            return false;
        }
        QFile file(protectedPath);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(protectedBytes) != protectedBytes.size()) {
            return false;
        }
        file.close();
        return true;
    };
    const auto expectMissingAssetFailure = [&](const QString& protectedPath,
                                               const auto& writer) {
        error.clear();
        EXPECT_FALSE(writer(&error));
        EXPECT_TRUE(error.contains(secondSourcePath)) << error.toStdString();
        EXPECT_EQ(readFileBytes(protectedPath), protectedBytes)
            << protectedPath.toStdString();
    };

    QString missingGamefaceSentinel;
    const QString missingGameface = QDir(root).filePath(
        QStringLiteral("missing_gameface"));
    ASSERT_TRUE(protectWriterOutput(missingGameface,
                                    QStringLiteral("index.html"),
                                    missingGamefaceSentinel));
    expectMissingAssetFailure(missingGamefaceSentinel,
        [&](QString* writerError) {
            return Artifact::ArtifactExportGamefaceWriter::write(
                missingAssetCompositionPtr, missingGameface, {}, writerError);
        });

    QString missingNoesisSentinel;
    const QString missingNoesis = QDir(root).filePath(
        QStringLiteral("missing_noesis"));
    ASSERT_TRUE(protectWriterOutput(missingNoesis,
                                    QStringLiteral("composition.xaml"),
                                    missingNoesisSentinel));
    expectMissingAssetFailure(missingNoesisSentinel,
        [&](QString* writerError) {
            return Artifact::ArtifactExportNoesisXamlWriter::write(
                missingAssetCompositionPtr, missingNoesis, {}, writerError);
        });

    QString missingRmlSentinel;
    const QString missingRml = QDir(root).filePath(
        QStringLiteral("missing_rml"));
    ASSERT_TRUE(protectWriterOutput(missingRml,
                                    QStringLiteral("composition.rml"),
                                    missingRmlSentinel));
    expectMissingAssetFailure(missingRmlSentinel,
        [&](QString* writerError) {
            return Artifact::ArtifactExportRmlUiWriter::write(
                missingAssetCompositionPtr, missingRml, {}, writerError);
        });

    QString missingUnitySentinel;
    const QString missingUnity = QDir(root).filePath(
        QStringLiteral("missing_unity"));
    ASSERT_TRUE(protectWriterOutput(missingUnity,
                                    QStringLiteral("composition.uxml"),
                                    missingUnitySentinel));
    expectMissingAssetFailure(missingUnitySentinel,
        [&](QString* writerError) {
            return Artifact::ArtifactExportUnityUxmlWriter::write(
                missingAssetCompositionPtr, missingUnity, {}, writerError);
        });

    QString missingLottieSentinel;
    const QString missingLottie = QDir(root).filePath(
        QStringLiteral("missing_lottie.json"));
    ASSERT_TRUE(protectWriterOutput(root,
                                    QStringLiteral("missing_lottie.json"),
                                    missingLottieSentinel));
    expectMissingAssetFailure(missingLottieSentinel,
        [&](QString* writerError) {
            return Artifact::ArtifactExportLottieWriter::write(
                missingAssetCompositionPtr, missingLottie, {}, writerError);
        });

    ASSERT_TRUE(projects.removeLayerFromComposition(
        composition.id, firstImageResult.layer->id()));
    ASSERT_TRUE(projects.removeLayerFromComposition(
        composition.id, secondImageResult.layer->id()));
    ASSERT_TRUE(projects.removeLayerFromComposition(
        composition.id, repeatedImageResult.layer->id()));

    ASSERT_TRUE(Artifact::ArtifactExportGamefaceWriter::write(
        compositionPtr, gameface, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportNoesisXamlWriter::write(
        compositionPtr, noesis, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportRmlUiWriter::write(
        compositionPtr, rmlui, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportUnityUxmlWriter::write(
        compositionPtr, unity, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportLottieWriter::write(
        compositionPtr, lottie, {}, &error)) << error.toStdString();

    const QString gamefaceHtml =
        readTextFile(QDir(gameface).filePath(QStringLiteral("index.html")));
    expectNonEmptyFile(QDir(gameface).filePath(QStringLiteral("index.html")));
    expectNonEmptyFile(QDir(gameface).filePath(QStringLiteral("styles.css")));
    expectNonEmptyFile(QDir(gameface).filePath(QStringLiteral("animations.css")));
    expectNonEmptyFile(QDir(gameface).filePath(QStringLiteral("app.js")));
    ASSERT_FALSE(gamefaceHtml.isEmpty());
    EXPECT_TRUE(gamefaceHtml.contains(QStringLiteral("id=\"composition\"")));
    EXPECT_TRUE(gamefaceHtml.contains(QStringLiteral("href=\"styles.css\"")));
    EXPECT_TRUE(gamefaceHtml.contains(QStringLiteral("href=\"animations.css\"")));
    EXPECT_TRUE(gamefaceHtml.contains(QStringLiteral("src=\"app.js\"")));
    EXPECT_TRUE(gamefaceHtml.contains(QStringLiteral("Export schema marker")));
    const QString gamefaceCss =
        readTextFile(QDir(gameface).filePath(QStringLiteral("styles.css")));
    EXPECT_TRUE(gamefaceCss.contains(QStringLiteral(".composition")));
    EXPECT_TRUE(gamefaceCss.contains(QStringLiteral("font-family:'Contract Sans'")));
    EXPECT_TRUE(gamefaceCss.contains(QStringLiteral("font-size:37px")));
    EXPECT_TRUE(gamefaceCss.contains(QStringLiteral("text-align:center")));
    EXPECT_FALSE(readTextFile(QDir(gameface).filePath(QStringLiteral("animations.css"))).isEmpty());
    EXPECT_FALSE(readTextFile(QDir(gameface).filePath(QStringLiteral("app.js"))).isEmpty());

    const QString noesisXaml = QDir(noesis).filePath(QStringLiteral("composition.xaml"));
    const QString noesisStyles =
        QDir(noesis).filePath(QStringLiteral("composition_styles.xaml"));
    expectNonEmptyFile(noesisXaml);
    expectNonEmptyFile(noesisStyles);
    EXPECT_TRUE(isWellFormedXml(noesisXaml));
    EXPECT_TRUE(hasXmlRoot(noesisXaml, QStringLiteral("Canvas"),
        QStringLiteral("http://schemas.microsoft.com/winfx/2006/xaml/presentation")));
    const QString noesisMarkup = readTextFile(noesisXaml);
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("<Canvas xmlns=")));
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("composition_styles.xaml")));
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("Export schema marker")));
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("FontFamily=\"Contract Sans\"")));
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("FontSize=\"37\"")));
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("TextAlignment=\"Center\"")));
    EXPECT_TRUE(isWellFormedXml(noesisStyles));

    const QString rml = QDir(rmlui).filePath(QStringLiteral("composition.rml"));
    const QString rmlCss = QDir(rmlui).filePath(QStringLiteral("composition.rcss"));
    expectNonEmptyFile(rml);
    expectNonEmptyFile(rmlCss);
    EXPECT_TRUE(isWellFormedXml(rml));
    EXPECT_TRUE(hasXmlRoot(rml, QStringLiteral("rml"), QString()));
    const QString rmlMarkup = readTextFile(rml);
    EXPECT_TRUE(rmlMarkup.contains(QStringLiteral("<rml>")));
    EXPECT_TRUE(rmlMarkup.contains(QStringLiteral("id=\"composition\"")));
    EXPECT_TRUE(rmlMarkup.contains(QStringLiteral("Export schema marker")));
    EXPECT_TRUE(rmlCss.contains(QStringLiteral("body.composition")));
    EXPECT_TRUE(rmlCss.contains(QStringLiteral("font-family:'Contract Sans'")));
    EXPECT_TRUE(rmlCss.contains(QStringLiteral("font-size:37px")));
    EXPECT_TRUE(rmlCss.contains(QStringLiteral("text-align:center")));

    const QString uxml = QDir(unity).filePath(QStringLiteral("composition.uxml"));
    const QString unityStyles = QDir(unity).filePath(QStringLiteral("composition.uss"));
    const QString uxmlMeta = QDir(unity).filePath(QStringLiteral("composition.uxml.meta"));
    const QString ussMeta = QDir(unity).filePath(QStringLiteral("composition.uss.meta"));
    expectNonEmptyFile(uxml);
    expectNonEmptyFile(unityStyles);
    expectNonEmptyFile(uxmlMeta);
    expectNonEmptyFile(ussMeta);
    EXPECT_TRUE(isWellFormedXml(uxml));
    EXPECT_TRUE(hasXmlRoot(uxml, QStringLiteral("UXML"),
                           QStringLiteral("UnityEngine.UIElements")));
    const QString uxmlMarkup = readTextFile(uxml);
    EXPECT_TRUE(uxmlMarkup.contains(QStringLiteral("<ui:UXML")));
    EXPECT_TRUE(uxmlMarkup.contains(QStringLiteral("ui:VisualElement name=\"composition\"")));
    EXPECT_TRUE(uxmlMarkup.contains(QStringLiteral("Export schema marker")));
    EXPECT_TRUE(unityStyles.contains(QStringLiteral(".composition")));
    EXPECT_TRUE(unityStyles.contains(QStringLiteral("font-size:37px")));
    EXPECT_TRUE(unityStyles.contains(QStringLiteral("-unity-text-align:upper-center")));

    expectNonEmptyFile(lottie);
    QFile lottieFile(lottie);
    ASSERT_TRUE(lottieFile.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonDocument lottieDocument =
        QJsonDocument::fromJson(lottieFile.readAll(), &parseError);
    ASSERT_EQ(parseError.error, QJsonParseError::NoError)
        << parseError.errorString().toStdString();
    ASSERT_TRUE(lottieDocument.isObject());
    const QJsonObject lottieRoot = lottieDocument.object();
    EXPECT_FALSE(lottieRoot.value(QStringLiteral("v")).toString().isEmpty());
    EXPECT_EQ(lottieRoot.value(QStringLiteral("w")).toInt(), 320);
    EXPECT_EQ(lottieRoot.value(QStringLiteral("h")).toInt(), 180);
    EXPECT_DOUBLE_EQ(lottieRoot.value(QStringLiteral("fr")).toDouble(), 30.0);
    EXPECT_DOUBLE_EQ(lottieRoot.value(QStringLiteral("ip")).toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(lottieRoot.value(QStringLiteral("op")).toDouble(), 30.0);
    ASSERT_TRUE(lottieRoot.value(QStringLiteral("layers")).isArray());
    const QJsonArray lottieLayers = lottieRoot.value(QStringLiteral("layers")).toArray();
    ASSERT_EQ(lottieLayers.size(), 1);
    const QJsonObject lottieTextLayer = lottieLayers.front().toObject();
    EXPECT_EQ(lottieTextLayer.value(QStringLiteral("ty")).toInt(), 5);
    EXPECT_EQ(lottieTextLayer.value(QStringLiteral("nm")).toString(),
              QStringLiteral("ContractText"));
    EXPECT_DOUBLE_EQ(lottieTextLayer.value(QStringLiteral("ip")).toDouble(), 0.0);
    EXPECT_DOUBLE_EQ(lottieTextLayer.value(QStringLiteral("op")).toDouble(), 30.0);
    const QJsonArray textKeyframes = lottieTextLayer.value(QStringLiteral("t"))
        .toObject().value(QStringLiteral("d")).toObject()
        .value(QStringLiteral("k")).toArray();
    ASSERT_EQ(textKeyframes.size(), 1);
    const QJsonObject textDocument = textKeyframes.front().toObject()
        .value(QStringLiteral("s")).toObject();
    EXPECT_EQ(textDocument.value(QStringLiteral("t")).toString(),
              QStringLiteral("Export schema marker"));
    EXPECT_EQ(textDocument.value(QStringLiteral("f")).toString(),
              QStringLiteral("Contract Sans"));
    EXPECT_DOUBLE_EQ(textDocument.value(QStringLiteral("s")).toDouble(), 37.0);
    EXPECT_EQ(textDocument.value(QStringLiteral("j")).toInt(), 1);

    const QString blockedOutput = QDir(root).filePath(
        QStringLiteral("blocked_output_parent"));
    const QByteArray blockedOutputBytes =
        QByteArrayLiteral("keep output parent intact");
    QFile blockedOutputFile(blockedOutput);
    ASSERT_TRUE(blockedOutputFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(blockedOutputFile.write(blockedOutputBytes),
              blockedOutputBytes.size());
    blockedOutputFile.close();
    const auto expectBlockedOutputFailure = [&](const QString& target,
                                                const auto& writer) {
        error.clear();
        EXPECT_FALSE(writer(&error)) << target.toStdString();
        EXPECT_FALSE(error.isEmpty()) << target.toStdString();
        EXPECT_EQ(readFileBytes(blockedOutput), blockedOutputBytes)
            << target.toStdString();
    };
    expectBlockedOutputFailure(
        QStringLiteral("Gameface"), [&](QString* writerError) {
            return Artifact::ArtifactExportGamefaceWriter::write(
                compositionPtr,
                QDir(blockedOutput).filePath(QStringLiteral("gameface")),
                {}, writerError);
        });
    expectBlockedOutputFailure(
        QStringLiteral("Noesis XAML"), [&](QString* writerError) {
            return Artifact::ArtifactExportNoesisXamlWriter::write(
                compositionPtr,
                QDir(blockedOutput).filePath(QStringLiteral("noesis")),
                {}, writerError);
        });
    expectBlockedOutputFailure(
        QStringLiteral("RmlUi"), [&](QString* writerError) {
            return Artifact::ArtifactExportRmlUiWriter::write(
                compositionPtr,
                QDir(blockedOutput).filePath(QStringLiteral("rmlui")),
                {}, writerError);
        });
    expectBlockedOutputFailure(
        QStringLiteral("Unity UXML"), [&](QString* writerError) {
            return Artifact::ArtifactExportUnityUxmlWriter::write(
                compositionPtr,
                QDir(blockedOutput).filePath(QStringLiteral("unity")),
                {}, writerError);
        });
    expectBlockedOutputFailure(
        QStringLiteral("Lottie"), [&](QString* writerError) {
            return Artifact::ArtifactExportLottieWriter::write(
                compositionPtr,
                QDir(blockedOutput).filePath(QStringLiteral("animation.json")),
                {}, writerError);
        });
}

void expectWriterMissingAssetPreservesOutput(const QString& writerName)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QString sourcePath = QDir(temporary.path()).filePath(
        QStringLiteral("source/missing-after-build.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(sourcePath).absolutePath()));
    QImage sourceImage(2, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(80, 140, 200, 255));
    ASSERT_TRUE(sourceImage.save(sourcePath, "PNG"));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("MissingWriterAssetContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("MissingWriterAssetContract")));
    compositionParams.setResolution(16, 16);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(1);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("MissingImage"));
    imageParams.setImagePath(sourcePath);
    ASSERT_TRUE(projects.addLayerToComposition(composition.id, imageParams).success);
    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_EQ(session.snapshot().assets.size(), 1);
    ASSERT_TRUE(QFile::remove(sourcePath));

    const QString outputDirectory = QDir(temporary.path()).filePath(writerName);
    QString outputPath;
    QString writerOutput = outputDirectory;
    if (writerName == QStringLiteral("Gameface")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("index.html"));
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("composition.xaml"));
    } else if (writerName == QStringLiteral("RmlUi")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("composition.rml"));
    } else if (writerName == QStringLiteral("UnityUxml")) {
        outputPath = QDir(outputDirectory).filePath(QStringLiteral("composition.uxml"));
    } else if (writerName == QStringLiteral("Lottie")) {
        outputPath = QDir(temporary.path()).filePath(QStringLiteral("animation.json"));
        writerOutput = outputPath;
    } else {
        FAIL() << "unknown writer: " << writerName.toStdString();
        return;
    }

    ASSERT_TRUE(QDir().mkpath(QFileInfo(outputPath).absolutePath()));
    const QByteArray sentinel = QByteArrayLiteral("existing output survives");
    QFile outputFile(outputPath);
    ASSERT_TRUE(outputFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(outputFile.write(sentinel), sentinel.size());
    outputFile.close();

    error.clear();
    bool succeeded = false;
    if (writerName == QStringLiteral("Gameface")) {
        succeeded = Artifact::ArtifactExportGamefaceWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        succeeded = Artifact::ArtifactExportNoesisXamlWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else if (writerName == QStringLiteral("RmlUi")) {
        succeeded = Artifact::ArtifactExportRmlUiWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else if (writerName == QStringLiteral("UnityUxml")) {
        succeeded = Artifact::ArtifactExportUnityUxmlWriter::write(
            compositionPtr, writerOutput, {}, &error);
    } else {
        succeeded = Artifact::ArtifactExportLottieWriter::write(
            compositionPtr, writerOutput, {}, &error);
    }
    EXPECT_FALSE(succeeded);
    EXPECT_TRUE(error.contains(sourcePath)) << error.toStdString();
    EXPECT_EQ(readFileBytes(outputPath), sentinel);
}

TEST(ExportWriterContractTest, MissingImageAssetFailureContractCoversAllWriters)
{
    for (const QString& writerName : {QStringLiteral("Gameface"),
                                      QStringLiteral("NoesisXaml"),
                                      QStringLiteral("RmlUi"),
                                      QStringLiteral("UnityUxml"),
                                      QStringLiteral("Lottie")}) {
        expectWriterMissingAssetPreservesOutput(writerName);
    }
}

TEST(ExportWriterContractTest, GamefaceMissingImageAssetPreservesOutput)
{
    expectWriterMissingAssetPreservesOutput(QStringLiteral("Gameface"));
}

TEST(ExportWriterContractTest, NoesisXamlMissingImageAssetPreservesOutput)
{
    expectWriterMissingAssetPreservesOutput(QStringLiteral("NoesisXaml"));
}

TEST(ExportWriterContractTest, RmlUiMissingImageAssetPreservesOutput)
{
    expectWriterMissingAssetPreservesOutput(QStringLiteral("RmlUi"));
}

TEST(ExportWriterContractTest, UnityUxmlMissingImageAssetPreservesOutput)
{
    expectWriterMissingAssetPreservesOutput(QStringLiteral("UnityUxml"));
}

TEST(ExportWriterContractTest, LottieMissingImageAssetPreservesOutput)
{
    expectWriterMissingAssetPreservesOutput(QStringLiteral("Lottie"));
}

void expectSingleTextWriterSchema(const QString& writerName)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid()) << writerName.toStdString();

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("SingleLayerExportContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("SingleLayerContract")));
    compositionParams.setResolution(64, 36);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(12);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success) << writerName.toStdString();

    Artifact::ArtifactTextLayerInitParams textParams(QStringLiteral("OnlyLayer"));
    const auto textResult = projects.addLayerToComposition(
        composition.id, textParams);
    ASSERT_TRUE(textResult.success) << writerName.toStdString();
    auto* textLayer = dynamic_cast<Artifact::ArtifactTextLayer*>(textResult.layer.get());
    ASSERT_NE(textLayer, nullptr) << writerName.toStdString();
    const QString specialText = QStringLiteral("schema & <one-layer>");
    const QString escapedText =
        QStringLiteral("schema &amp; &lt;one-layer&gt;");
    textLayer->setText(ArtifactCore::UniString(specialText));

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success) << writerName.toStdString();
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr) << writerName.toStdString();
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_EQ(session.snapshot().layers.size(), 1) << writerName.toStdString();
    EXPECT_TRUE(session.snapshot().assets.isEmpty()) << writerName.toStdString();

    const QString root = temporary.path();
    if (writerName == QStringLiteral("Gameface")) {
        const QString output = QDir(root).filePath(QStringLiteral("gameface"));
        ASSERT_TRUE(Artifact::ArtifactExportGamefaceWriter::write(
            compositionPtr, output, {}, &error)) << error.toStdString();
        const QString markup = readTextFile(
            QDir(output).filePath(QStringLiteral("index.html")));
        EXPECT_EQ(markup.count(QStringLiteral("class=\"layer ")), 1);
        EXPECT_TRUE(markup.contains(escapedText));
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        const QString output = QDir(root).filePath(QStringLiteral("noesis"));
        ASSERT_TRUE(Artifact::ArtifactExportNoesisXamlWriter::write(
            compositionPtr, output, {}, &error)) << error.toStdString();
        const QString path = QDir(output).filePath(QStringLiteral("composition.xaml"));
        ASSERT_TRUE(isWellFormedXml(path));
        const QString markup = readTextFile(path);
        EXPECT_EQ(markup.count(QStringLiteral("<TextBlock ")), 1);
        EXPECT_TRUE(markup.contains(escapedText));
    } else if (writerName == QStringLiteral("RmlUi")) {
        const QString output = QDir(root).filePath(QStringLiteral("rmlui"));
        ASSERT_TRUE(Artifact::ArtifactExportRmlUiWriter::write(
            compositionPtr, output, {}, &error)) << error.toStdString();
        const QString path = QDir(output).filePath(QStringLiteral("composition.rml"));
        ASSERT_TRUE(isWellFormedXml(path));
        const QString markup = readTextFile(path);
        EXPECT_EQ(markup.count(QStringLiteral("class=\"layer_0\"")), 1);
        EXPECT_TRUE(markup.contains(escapedText));
    } else if (writerName == QStringLiteral("UnityUxml")) {
        const QString output = QDir(root).filePath(QStringLiteral("unity"));
        ASSERT_TRUE(Artifact::ArtifactExportUnityUxmlWriter::write(
            compositionPtr, output, {}, &error)) << error.toStdString();
        const QString path = QDir(output).filePath(QStringLiteral("composition.uxml"));
        ASSERT_TRUE(isWellFormedXml(path));
        const QString markup = readTextFile(path);
        EXPECT_EQ(markup.count(QStringLiteral("<ui:Label ")), 1);
        EXPECT_TRUE(markup.contains(escapedText));
    } else if (writerName == QStringLiteral("Lottie")) {
        const QString output = QDir(root).filePath(QStringLiteral("animation.json"));
        ASSERT_TRUE(Artifact::ArtifactExportLottieWriter::write(
            compositionPtr, output, {}, &error)) << error.toStdString();
        const QJsonDocument document = QJsonDocument::fromJson(readFileBytes(output));
        ASSERT_TRUE(document.isObject());
        const QJsonArray layers = document.object()
            .value(QStringLiteral("layers")).toArray();
        ASSERT_EQ(layers.size(), 1);
        const QJsonObject textLayer = layers.front().toObject();
        EXPECT_EQ(textLayer.value(QStringLiteral("nm")).toString(),
                  QStringLiteral("OnlyLayer"));
        const QJsonArray textKeyframes = textLayer.value(QStringLiteral("t"))
            .toObject().value(QStringLiteral("d")).toObject()
            .value(QStringLiteral("k")).toArray();
        ASSERT_EQ(textKeyframes.size(), 1);
        EXPECT_EQ(textKeyframes.front().toObject().value(QStringLiteral("s"))
                      .toObject().value(QStringLiteral("t")).toString(),
                  specialText);
    } else {
        FAIL() << "unknown writer: " << writerName.toStdString();
    }
}

TEST(ExportWriterContractTest, SingleTextLayerProducesGamefaceSchema)
{
    expectSingleTextWriterSchema(QStringLiteral("Gameface"));
}

TEST(ExportWriterContractTest, SingleTextLayerProducesNoesisXamlSchema)
{
    expectSingleTextWriterSchema(QStringLiteral("NoesisXaml"));
}

TEST(ExportWriterContractTest, SingleTextLayerProducesRmlUiSchema)
{
    expectSingleTextWriterSchema(QStringLiteral("RmlUi"));
}

TEST(ExportWriterContractTest, SingleTextLayerProducesUnityUxmlSchema)
{
    expectSingleTextWriterSchema(QStringLiteral("UnityUxml"));
}

TEST(ExportWriterContractTest, SingleTextLayerProducesLottieSchema)
{
    expectSingleTextWriterSchema(QStringLiteral("Lottie"));
}

void expectSingleShapeOrImageWriterSchema(const QString& writerName,
                                          const bool imageLayer)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("SingleLayerWriterSchemaContractTest"),
                           true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    QString sourcePath;
    QByteArray sourceBytes;
    if (imageLayer) {
        sourcePath = QDir(temporary.path()).filePath(
            QStringLiteral("source/only-image.png"));
        ASSERT_TRUE(QDir().mkpath(QFileInfo(sourcePath).absolutePath()));
        QImage sourceImage(3, 2, QImage::Format_RGBA8888);
        sourceImage.fill(QColor(40, 120, 220, 255));
        ASSERT_TRUE(sourceImage.save(sourcePath, "PNG"));
        sourceBytes = readFileBytes(sourcePath);
    }

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(ArtifactCore::UniString(
        imageLayer ? QStringLiteral("SingleImageContract")
                   : QStringLiteral("SingleShapeContract")));
    compositionParams.setResolution(64, 36);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(12);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    if (imageLayer) {
        Artifact::ArtifactImageInitParams imageParams(QStringLiteral("OnlyImage"));
        imageParams.setImagePath(sourcePath);
        ASSERT_TRUE(projects.addLayerToComposition(
            composition.id, imageParams).success);
    } else {
        Artifact::ArtifactLayerInitParams shapeParams(
            QStringLiteral("OnlyShape"), Artifact::LayerType::Shape);
        const auto shapeResult = projects.addLayerToComposition(
            composition.id, shapeParams);
        ASSERT_TRUE(shapeResult.success);
        auto* shapeLayer = dynamic_cast<Artifact::ArtifactShapeLayer*>(
            shapeResult.layer.get());
        ASSERT_NE(shapeLayer, nullptr);
        shapeLayer->setSize(23, 11);
        shapeLayer->setFillColor(
            ArtifactCore::FloatColor(0.25f, 0.5f, 0.75f, 1.0f));
    }

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_EQ(session.snapshot().layers.size(), 1);
    EXPECT_EQ(session.snapshot().layers.front().name,
              imageLayer ? QStringLiteral("OnlyImage")
                         : QStringLiteral("OnlyShape"));
    if (imageLayer) {
        ASSERT_EQ(session.snapshot().assets.size(), 1);
    } else {
        EXPECT_TRUE(session.snapshot().assets.isEmpty());
    }
    const QString relativeAssetPath = imageLayer
        ? session.snapshot().assets.front().relativePath
        : QString();

    const QString root = temporary.path();
    const QString output = QDir(root).filePath(writerName);
    bool writeSucceeded = false;
    if (writerName == QStringLiteral("Gameface")) {
        writeSucceeded = Artifact::ArtifactExportGamefaceWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        writeSucceeded = Artifact::ArtifactExportNoesisXamlWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("RmlUi")) {
        writeSucceeded = Artifact::ArtifactExportRmlUiWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("UnityUxml")) {
        writeSucceeded = Artifact::ArtifactExportUnityUxmlWriter::write(
            compositionPtr, output, {}, &error);
    } else if (writerName == QStringLiteral("Lottie")) {
        Artifact::ArtifactLottieExportOptions options;
        options.embedImages = false;
        writeSucceeded = Artifact::ArtifactExportLottieWriter::write(
            compositionPtr, QDir(root).filePath(QStringLiteral("animation.json")),
            options, &error);
    } else {
        FAIL() << "unknown writer: " << writerName.toStdString();
        return;
    }
    ASSERT_TRUE(writeSucceeded) << error.toStdString();

    if (writerName == QStringLiteral("Gameface")) {
        const QString markup = readTextFile(
            QDir(output).filePath(QStringLiteral("index.html")));
        EXPECT_EQ(markup.count(QStringLiteral("class=\"layer ")), 1);
        if (imageLayer) {
            EXPECT_TRUE(markup.contains(relativeAssetPath));
        } else {
            EXPECT_TRUE(readTextFile(QDir(output).filePath(
                QStringLiteral("styles.css"))).contains(
                    QStringLiteral("width:23px;height:11px;")));
        }
    } else if (writerName == QStringLiteral("NoesisXaml")) {
        const QString path = QDir(output).filePath(QStringLiteral("composition.xaml"));
        ASSERT_TRUE(isWellFormedXml(path));
        const QString markup = readTextFile(path);
        EXPECT_EQ(markup.count(imageLayer ? QStringLiteral("<Image ")
                                          : QStringLiteral("<Rectangle ")), 1);
        EXPECT_TRUE(markup.contains(imageLayer
            ? relativeAssetPath
            : QStringLiteral("Width=\"23\" Height=\"11\"")));
    } else if (writerName == QStringLiteral("RmlUi")) {
        const QString path = QDir(output).filePath(QStringLiteral("composition.rml"));
        ASSERT_TRUE(isWellFormedXml(path));
        const QString markup = readTextFile(path);
        EXPECT_EQ(markup.count(imageLayer ? QStringLiteral("<img ")
                                          : QStringLiteral("class=\"layer_0\"")), 1);
        if (imageLayer) EXPECT_TRUE(markup.contains(relativeAssetPath));
    } else if (writerName == QStringLiteral("UnityUxml")) {
        const QString path = QDir(output).filePath(QStringLiteral("composition.uxml"));
        ASSERT_TRUE(isWellFormedXml(path));
        const QString markup = readTextFile(path);
        EXPECT_EQ(markup.count(QStringLiteral("class=\"layer layer_0\"")), 1);
        if (imageLayer) {
            EXPECT_TRUE(readTextFile(QDir(output).filePath(
                QStringLiteral("composition.uss"))).contains(relativeAssetPath));
        }
    } else {
        const QString path = QDir(root).filePath(QStringLiteral("animation.json"));
        const QJsonDocument document = QJsonDocument::fromJson(readFileBytes(path));
        ASSERT_TRUE(document.isObject());
        const QJsonObject rootObject = document.object();
        const QJsonArray layers = rootObject.value(QStringLiteral("layers")).toArray();
        ASSERT_EQ(layers.size(), 1);
        const QJsonObject layer = layers.front().toObject();
        EXPECT_EQ(layer.value(QStringLiteral("nm")).toString(),
                  imageLayer ? QStringLiteral("OnlyImage")
                             : QStringLiteral("OnlyShape"));
        EXPECT_EQ(layer.value(QStringLiteral("ty")).toInt(), imageLayer ? 2 : 4);
        if (imageLayer) {
            const QString assetId = layer.value(QStringLiteral("refId")).toString();
            ASSERT_FALSE(assetId.isEmpty());
            const QJsonArray assets = rootObject.value(QStringLiteral("assets")).toArray();
            ASSERT_EQ(assets.size(), 1);
            const QJsonObject asset = assets.front().toObject();
            EXPECT_EQ(asset.value(QStringLiteral("id")).toString(), assetId);
            const QString copiedPath = QDir(root).filePath(
                QStringLiteral("assets/") + asset.value(QStringLiteral("p")).toString());
            EXPECT_EQ(readFileBytes(copiedPath), sourceBytes);
        } else {
            EXPECT_FALSE(layer.value(QStringLiteral("shapes")).toArray().isEmpty());
        }
    }

    if (imageLayer && writerName != QStringLiteral("Lottie")) {
        const QString copiedPath = QDir(output).filePath(relativeAssetPath);
        ASSERT_TRUE(QFileInfo::exists(copiedPath));
        EXPECT_EQ(readFileBytes(copiedPath), sourceBytes);
    }
}

TEST(ExportWriterContractTest, SingleShapeLayerProducesGamefaceSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("Gameface"), false);
}

TEST(ExportWriterContractTest, SingleShapeLayerProducesNoesisXamlSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("NoesisXaml"), false);
}

TEST(ExportWriterContractTest, SingleShapeLayerProducesRmlUiSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("RmlUi"), false);
}

TEST(ExportWriterContractTest, SingleShapeLayerProducesUnityUxmlSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("UnityUxml"), false);
}

TEST(ExportWriterContractTest, SingleShapeLayerProducesLottieSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("Lottie"), false);
}

TEST(ExportWriterContractTest, SingleImageLayerProducesGamefaceSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("Gameface"), true);
}

TEST(ExportWriterContractTest, SingleImageLayerProducesNoesisXamlSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("NoesisXaml"), true);
}

TEST(ExportWriterContractTest, SingleImageLayerProducesRmlUiSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("RmlUi"), true);
}

TEST(ExportWriterContractTest, SingleImageLayerProducesUnityUxmlSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("UnityUxml"), true);
}

TEST(ExportWriterContractTest, SingleImageLayerProducesLottieSchema)
{
    expectSingleShapeOrImageWriterSchema(QStringLiteral("Lottie"), true);
}

TEST(ExportWriterContractTest, LottieEmbedsSingleImageDataUriAndSourceBytes)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QString sourcePath = QDir(temporary.path()).filePath(
        QStringLiteral("source/embedded.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(sourcePath).absolutePath()));
    QImage sourceImage(3, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(40, 120, 220, 255));
    ASSERT_TRUE(sourceImage.save(sourcePath, "PNG"));
    const QByteArray sourceBytes = readFileBytes(sourcePath);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("LottieEmbeddedImageContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("EmbeddedImageContract")));
    compositionParams.setResolution(64, 36);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(1);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("EmbeddedImage"));
    imageParams.setImagePath(sourcePath);
    ASSERT_TRUE(projects.addLayerToComposition(composition.id, imageParams).success);
    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);

    Artifact::ArtifactLottieExportOptions options;
    options.embedImages = true;
    const QString outputPath = QDir(temporary.path()).filePath(
        QStringLiteral("embedded.json"));
    QString error;
    ASSERT_TRUE(Artifact::ArtifactExportLottieWriter::write(
        compositionPtr, outputPath, options, &error)) << error.toStdString();

    const QJsonDocument document = QJsonDocument::fromJson(readFileBytes(outputPath));
    ASSERT_TRUE(document.isObject());
    const QJsonObject root = document.object();
    const QJsonArray layers = root.value(QStringLiteral("layers")).toArray();
    ASSERT_EQ(layers.size(), 1);
    const QJsonObject imageLayer = layers.front().toObject();
    EXPECT_EQ(imageLayer.value(QStringLiteral("ty")).toInt(), 2);
    const QString layerAssetId = imageLayer.value(QStringLiteral("refId")).toString();
    ASSERT_FALSE(layerAssetId.isEmpty());

    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    ASSERT_EQ(assets.size(), 1);
    const QJsonObject asset = assets.front().toObject();
    EXPECT_EQ(asset.value(QStringLiteral("id")).toString(), layerAssetId);
    EXPECT_EQ(asset.value(QStringLiteral("w")).toInt(), 3);
    EXPECT_EQ(asset.value(QStringLiteral("h")).toInt(), 2);
    EXPECT_EQ(asset.value(QStringLiteral("e")).toInt(), 1);
    const QString dataUri = asset.value(QStringLiteral("p")).toString();
    const QString prefix = QStringLiteral("data:image/png;base64,");
    ASSERT_TRUE(dataUri.startsWith(prefix));
    EXPECT_EQ(QByteArray::fromBase64(dataUri.mid(prefix.size()).toLatin1()),
              sourceBytes);
}

TEST(ExportWriterContractTest, SingleShapeLayerProducesMinimalWriterSchemas)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("SingleShapeExportContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("SingleShapeContract")));
    compositionParams.setResolution(64, 36);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(12);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactLayerInitParams shapeParams(
        QStringLiteral("OnlyShape"), Artifact::LayerType::Shape);
    const auto shapeResult = projects.addLayerToComposition(
        composition.id, shapeParams);
    ASSERT_TRUE(shapeResult.success);
    auto* shapeLayer =
        dynamic_cast<Artifact::ArtifactShapeLayer*>(shapeResult.layer.get());
    ASSERT_NE(shapeLayer, nullptr);
    shapeLayer->setSize(23, 11);
    shapeLayer->setFillColor(ArtifactCore::FloatColor(0.25f, 0.5f, 0.75f, 1.0f));

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_EQ(session.snapshot().layers.size(), 1);
    EXPECT_EQ(session.snapshot().layers.front().name, QStringLiteral("OnlyShape"));
    EXPECT_TRUE(session.snapshot().layers.front().serialized.contains(
        QStringLiteral("shapeWidth")));
    EXPECT_TRUE(session.snapshot().assets.isEmpty());

    const QString root = temporary.path();
    const QString gameface = QDir(root).filePath(QStringLiteral("gameface"));
    const QString noesis = QDir(root).filePath(QStringLiteral("noesis"));
    const QString rmlui = QDir(root).filePath(QStringLiteral("rmlui"));
    const QString unity = QDir(root).filePath(QStringLiteral("unity"));
    const QString lottie = QDir(root).filePath(QStringLiteral("animation.json"));
    ASSERT_TRUE(Artifact::ArtifactExportGamefaceWriter::write(
        compositionPtr, gameface, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportNoesisXamlWriter::write(
        compositionPtr, noesis, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportRmlUiWriter::write(
        compositionPtr, rmlui, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportUnityUxmlWriter::write(
        compositionPtr, unity, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportLottieWriter::write(
        compositionPtr, lottie, {}, &error)) << error.toStdString();

    const QString gamefaceHtml = readTextFile(
        QDir(gameface).filePath(QStringLiteral("index.html")));
    EXPECT_EQ(gamefaceHtml.count(QStringLiteral("class=\"layer ")), 1);
    EXPECT_TRUE(readTextFile(QDir(gameface).filePath(QStringLiteral("styles.css")))
                    .contains(QStringLiteral("width:23px;height:11px;")));

    const QString noesisPath = QDir(noesis).filePath(QStringLiteral("composition.xaml"));
    ASSERT_TRUE(isWellFormedXml(noesisPath));
    const QString noesisMarkup = readTextFile(noesisPath);
    EXPECT_EQ(noesisMarkup.count(QStringLiteral("<Rectangle ")), 1);
    EXPECT_TRUE(noesisMarkup.contains(QStringLiteral("Width=\"23\" Height=\"11\"")));

    const QString rmlPath = QDir(rmlui).filePath(QStringLiteral("composition.rml"));
    ASSERT_TRUE(isWellFormedXml(rmlPath));
    EXPECT_EQ(readTextFile(rmlPath).count(QStringLiteral("class=\"layer_0\"")), 1);

    const QString unityPath = QDir(unity).filePath(QStringLiteral("composition.uxml"));
    ASSERT_TRUE(isWellFormedXml(unityPath));
    EXPECT_EQ(readTextFile(unityPath).count(QStringLiteral("class=\"layer")), 1);

    const QJsonDocument lottieDocument =
        QJsonDocument::fromJson(readFileBytes(lottie));
    ASSERT_TRUE(lottieDocument.isObject());
    const QJsonArray layers = lottieDocument.object()
        .value(QStringLiteral("layers")).toArray();
    ASSERT_EQ(layers.size(), 1);
    const QJsonObject shape = layers.front().toObject();
    EXPECT_EQ(shape.value(QStringLiteral("nm")).toString(),
              QStringLiteral("OnlyShape"));
    EXPECT_EQ(shape.value(QStringLiteral("ty")).toInt(), 4);
    EXPECT_FALSE(shape.value(QStringLiteral("shapes")).toArray().isEmpty());
}

TEST(ExportWriterContractTest, SingleImageLayerProducesMinimalWriterSchemas)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("SingleImageExportContractTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());
    struct Cleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~Cleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};

    const QString sourcePath = QDir(temporary.path()).filePath(
        QStringLiteral("source/only-image.png"));
    ASSERT_TRUE(QDir().mkpath(QFileInfo(sourcePath).absolutePath()));
    QImage sourceImage(3, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(40, 120, 220, 255));
    ASSERT_TRUE(sourceImage.save(sourcePath, "PNG"));

    Artifact::ArtifactCompositionInitParams compositionParams;
    compositionParams.setCompositionName(
        ArtifactCore::UniString(QStringLiteral("SingleImageContract")));
    compositionParams.setResolution(64, 36);
    compositionParams.setFrameRate(24.0);
    compositionParams.setDurationFrames(12);
    const auto composition = projects.createComposition(compositionParams);
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("OnlyImage"));
    imageParams.setImagePath(sourcePath);
    const auto imageResult = projects.addLayerToComposition(
        composition.id, imageParams);
    ASSERT_TRUE(imageResult.success);

    const auto compositionResult = projects.findComposition(composition.id);
    ASSERT_TRUE(compositionResult.success);
    const auto compositionPtr = compositionResult.ptr.lock();
    ASSERT_TRUE(compositionPtr);
    Artifact::ArtifactExportSession session(compositionPtr);
    QString error;
    ASSERT_TRUE(session.build(&error)) << error.toStdString();
    ASSERT_EQ(session.snapshot().layers.size(), 1);
    EXPECT_EQ(session.snapshot().layers.front().name, QStringLiteral("OnlyImage"));
    ASSERT_EQ(session.snapshot().assets.size(), 1);
    const QString relativeAssetPath =
        session.snapshot().assets.front().relativePath;
    EXPECT_TRUE(relativeAssetPath.startsWith(QStringLiteral("assets/")));

    const QString root = temporary.path();
    const QString gameface = QDir(root).filePath(QStringLiteral("gameface"));
    const QString noesis = QDir(root).filePath(QStringLiteral("noesis"));
    const QString rmlui = QDir(root).filePath(QStringLiteral("rmlui"));
    const QString unity = QDir(root).filePath(QStringLiteral("unity"));
    const QString lottie = QDir(root).filePath(QStringLiteral("animation.json"));
    ASSERT_TRUE(Artifact::ArtifactExportGamefaceWriter::write(
        compositionPtr, gameface, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportNoesisXamlWriter::write(
        compositionPtr, noesis, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportRmlUiWriter::write(
        compositionPtr, rmlui, {}, &error)) << error.toStdString();
    ASSERT_TRUE(Artifact::ArtifactExportUnityUxmlWriter::write(
        compositionPtr, unity, {}, &error)) << error.toStdString();
    Artifact::ArtifactLottieExportOptions lottieOptions;
    lottieOptions.embedImages = false;
    ASSERT_TRUE(Artifact::ArtifactExportLottieWriter::write(
        compositionPtr, lottie, lottieOptions, &error)) << error.toStdString();

    const QString gamefaceHtml = readTextFile(
        QDir(gameface).filePath(QStringLiteral("index.html")));
    EXPECT_EQ(gamefaceHtml.count(QStringLiteral("class=\"layer ")), 1);
    EXPECT_TRUE(gamefaceHtml.contains(relativeAssetPath));
    const QString noesisPath = QDir(noesis).filePath(
        QStringLiteral("composition.xaml"));
    ASSERT_TRUE(isWellFormedXml(noesisPath));
    const QString noesisMarkup = readTextFile(noesisPath);
    EXPECT_EQ(noesisMarkup.count(QStringLiteral("<Image ")), 1);
    EXPECT_TRUE(noesisMarkup.contains(relativeAssetPath));
    const QString rmlPath = QDir(rmlui).filePath(
        QStringLiteral("composition.rml"));
    ASSERT_TRUE(isWellFormedXml(rmlPath));
    const QString rmlMarkup = readTextFile(rmlPath);
    EXPECT_EQ(rmlMarkup.count(QStringLiteral("<img ")), 1);
    EXPECT_TRUE(rmlMarkup.contains(relativeAssetPath));
    const QString unityPath = QDir(unity).filePath(
        QStringLiteral("composition.uxml"));
    ASSERT_TRUE(isWellFormedXml(unityPath));
    EXPECT_EQ(readTextFile(unityPath).count(
                  QStringLiteral("class=\"layer layer_0\"")),
              1);
    const QString unityStyles = readTextFile(QDir(unity).filePath(
        QStringLiteral("composition.uss")));
    EXPECT_TRUE(unityStyles.contains(relativeAssetPath));

    const QByteArray sourceBytes = readFileBytes(sourcePath);
    for (const QString& outputDirectory : {gameface, noesis, rmlui, unity}) {
        const QString copiedAsset = QDir(outputDirectory).filePath(relativeAssetPath);
        ASSERT_TRUE(QFileInfo::exists(copiedAsset)) << copiedAsset.toStdString();
        EXPECT_EQ(readFileBytes(copiedAsset), sourceBytes)
            << outputDirectory.toStdString();
    }

    const QJsonDocument lottieDocument =
        QJsonDocument::fromJson(readFileBytes(lottie));
    ASSERT_TRUE(lottieDocument.isObject());
    const QJsonObject lottieRoot = lottieDocument.object();
    const QJsonArray lottieLayers =
        lottieRoot.value(QStringLiteral("layers")).toArray();
    ASSERT_EQ(lottieLayers.size(), 1);
    const QJsonObject lottieImageLayer = lottieLayers.front().toObject();
    EXPECT_EQ(lottieImageLayer.value(QStringLiteral("nm")).toString(),
              QStringLiteral("OnlyImage"));
    EXPECT_EQ(lottieImageLayer.value(QStringLiteral("ty")).toInt(), 2);
    const QString lottieAssetId =
        lottieImageLayer.value(QStringLiteral("refId")).toString();
    ASSERT_FALSE(lottieAssetId.isEmpty());
    const QJsonArray lottieAssets =
        lottieRoot.value(QStringLiteral("assets")).toArray();
    ASSERT_EQ(lottieAssets.size(), 1);
    const QJsonObject lottieAsset = lottieAssets.front().toObject();
    EXPECT_EQ(lottieAsset.value(QStringLiteral("id")).toString(), lottieAssetId);
    EXPECT_EQ(lottieAsset.value(QStringLiteral("u")).toString(),
              QStringLiteral("assets/"));
    EXPECT_EQ(lottieAsset.value(QStringLiteral("w")).toInt(), 3);
    EXPECT_EQ(lottieAsset.value(QStringLiteral("h")).toInt(), 2);
    const QString lottieAssetPath = QDir(root).filePath(
        QStringLiteral("assets/") +
        lottieAsset.value(QStringLiteral("p")).toString());
    ASSERT_TRUE(QFileInfo::exists(lottieAssetPath));
    EXPECT_EQ(readFileBytes(lottieAssetPath), sourceBytes);
}
