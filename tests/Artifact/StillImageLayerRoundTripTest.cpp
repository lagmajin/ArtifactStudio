#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QVariant>
#include <QVector3D>
#include <QVector4D>
#include <QUuid>

#include <opencv2/core.hpp>

import Artifact.Composition.InitParams;
import Artifact.Composition.Result;
import Artifact.Effect.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.InitParams;
import Artifact.Layer.SourceCrop;
import Artifact.Mask.LayerMask;
import Artifact.Project.Manager;
import Animation.Transform2D;
import Asset.Manager;
import BrightnessEffect;
import Core.AI.ImageSegmenter;
import Core.Diagnostics.FallbackPolicy;
import Geometry.DepthMeshGenerator;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import Image.ImageSurfaceView;
import Memory.SharedPtr;
import Mesh;
import Utils.String.UniString;

namespace {

class StillImageRoundTripEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactStillImageLayerRoundTripTest";
        static char* argv[] = {applicationName, nullptr};
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

class NotReadyImageSegmenter final : public ArtifactCore::IImageSegmenter {
public:
    bool isReady() const noexcept override { return false; }
    bool segment(const ArtifactCore::ImageF32x4_RGBA&,
                 ArtifactCore::DepthMap&) override
    {
        ++segmentCalls;
        return false;
    }
    QString lastError() const override { return QStringLiteral("model unavailable"); }
    int segmentCalls = 0;
};

StillImageRoundTripEnvironment* const environment =
    new StillImageRoundTripEnvironment();
const bool environmentRegistered = [] {
    testing::AddGlobalTestEnvironment(environment);
    return true;
}();

struct ProjectCleanup {
    Artifact::ArtifactProjectManager& projects;
    ~ProjectCleanup() { projects.closeCurrentProject(); }
};

Artifact::ArtifactCompositionInitParams makeCompositionParams()
{
    Artifact::ArtifactCompositionInitParams params;
    params.setCompositionName(ArtifactCore::UniString(QStringLiteral("ImageComp")));
    params.setResolution(64, 48);
    params.setFrameRate(30.0);
    params.setDurationFrames(48);
    return params;
}

void isolateProjectRoot(const QTemporaryDir& dir)
{
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", dir.path().toUtf8());
    QCoreApplication::setApplicationName(
        QStringLiteral("ArtifactStillImageLayerTest"));
    QStandardPaths::setTestModeEnabled(true);
}

QImage waitForImage(Artifact::ArtifactImageLayer& layer, const QSize& expectedSize)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        const QImage image = layer.toQImage();
        if (image.size() == expectedSize) {
            return image;
        }
        QThread::msleep(1);
    }
    return layer.toQImage();
}

bool waitForImageBuffer(Artifact::ArtifactImageLayer& layer,
                        const QSize& expectedSize)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        (void)layer.toQImage();
        const auto& buffer = layer.currentFrameBuffer();
        if (!buffer.isEmpty() &&
            buffer.width() == expectedSize.width() &&
            buffer.height() == expectedSize.height()) {
            return true;
        }
        QThread::msleep(1);
    }
    return false;
}

QImage waitForSequenceFrame(Artifact::ArtifactImageLayer& layer,
                            qint64 expectedFrameIndex)
{
    QElapsedTimer timer;
    timer.start();
    QImage frame;
    while (timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        frame = layer.toQImage();
        if (layer.sequenceCachedFrameIndex() == expectedFrameIndex) {
            return frame;
        }
        QThread::msleep(1);
    }
    return frame;
}

bool writeBase64Fixture(const QString& path, const QByteArray& base64)
{
    const QByteArray bytes = QByteArray::fromBase64(base64);
    QFile file(path);
    return bytes.size() == 70 && file.open(QIODevice::WriteOnly) &&
           file.write(bytes) == bytes.size();
}

void expectDimensionFallbackRecorded(const QString& imagePath)
{
    const auto events = ArtifactCore::FallbackTracker::instance()
                            ->getEventsByCategory(
                                ArtifactCore::FallbackCategory::Image);
    bool found = false;
    for (const auto& event : events) {
        if (event.originalId == imagePath &&
            event.message.contains(QStringLiteral("Unsupported image specification"))) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "No dimension-limit diagnostic was recorded for "
                       << imagePath.toStdString();
}

} // namespace

TEST(StillImageLayerRoundTripTest, PixelAspectChangesDisplayWidthUnlessFitToLayer)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(4, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(40, 90, 160, 255));
    const QString imagePath = dir.filePath(QStringLiteral("pixel-aspect.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    ASSERT_TRUE(waitForImageBuffer(layer, QSize(4, 2)));

    QJsonObject state = layer.toJson();
    QJsonObject metadata = state.value(QStringLiteral("image.sourceMetadata")).toObject();
    metadata[QStringLiteral("pixelAspect")] = 1.5;
    state[QStringLiteral("image.sourceMetadata")] = metadata;
    layer.fromJsonProperties(state);
    ASSERT_TRUE(waitForImageBuffer(layer, QSize(4, 2)));

    const auto pixelAspectLayout = layer.sourceCropDrawLayout();
    EXPECT_DOUBLE_EQ(pixelAspectLayout.outputLocalRect.width(), 6.0);
    EXPECT_DOUBLE_EQ(pixelAspectLayout.outputLocalRect.height(), 2.0);
    EXPECT_DOUBLE_EQ(layer.localBounds().width(), 6.0);

    layer.setFitToLayer(true);
    const auto fitLayout = layer.sourceCropDrawLayout();
    EXPECT_DOUBLE_EQ(fitLayout.outputLocalRect.width(), 4.0);
    EXPECT_DOUBLE_EQ(fitLayout.outputLocalRect.height(), 2.0);
    EXPECT_DOUBLE_EQ(layer.localBounds().width(), 4.0);
}

TEST(StillImageLayerRoundTripTest,
     InputInterpretationIsTrimmedBoundedAndRestored)
{
    Artifact::ArtifactImageLayer source;
    source.setInputInterpretation(QStringLiteral("  Portable Space  "),
                                  QStringLiteral("  scene-linear  "));
    EXPECT_EQ(source.inputColorSpace(), QStringLiteral("Portable Space"));
    EXPECT_EQ(source.inputTransferFunction(), QStringLiteral("scene-linear"));

    source.setInputInterpretation(QString(5000, QLatin1Char('C')),
                                  QString(2000, QLatin1Char('T')));
    ASSERT_EQ(source.inputColorSpace().size(), 4096);
    ASSERT_EQ(source.inputTransferFunction().size(), 1024);

    Artifact::ArtifactImageLayer restored;
    restored.fromJsonProperties(source.toJson());
    EXPECT_EQ(restored.inputColorSpace(), source.inputColorSpace());
    EXPECT_EQ(restored.inputTransferFunction(), source.inputTransferFunction());
}

TEST(StillImageLayerRoundTripTest,
     MissingSourceMetadataIsClampedAndSerializedWithinBounds)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QJsonObject metadata;
    metadata[QStringLiteral("channelCount")] = 3;
    metadata[QStringLiteral("alphaChannel")] = 3;
    metadata[QStringLiteral("alphaAssociated")] = true;
    metadata[QStringLiteral("orientation")] = 99;
    metadata[QStringLiteral("pixelAspect")] = -4.0;
    metadata[QStringLiteral("bitsPerChannel")] = -12;
    metadata[QStringLiteral("iccProfileBytes")] = -8;
    metadata[QStringLiteral("colorSpace")] = QString(5000, QLatin1Char('S'));
    QJsonArray channelNames;
    for (int index = 0; index < 66; ++index) {
        channelNames.append(QString(300, QLatin1Char('N')));
    }
    metadata[QStringLiteral("channelNames")] = channelNames;

    QJsonObject state;
    state[QStringLiteral("image.sourcePath")] =
        dir.filePath(QStringLiteral("missing-metadata-source.png"));
    state[QStringLiteral("image.sourceMetadata")] = metadata;

    Artifact::ArtifactImageLayer layer;
    layer.fromJsonProperties(state);
    const QJsonObject restored = layer.toJson()
                                     .value(QStringLiteral("image.sourceMetadata"))
                                     .toObject();

    EXPECT_EQ(restored.value(QStringLiteral("channelCount")).toInt(), 3);
    EXPECT_EQ(restored.value(QStringLiteral("alphaChannel")).toInt(), -1);
    EXPECT_FALSE(restored.value(QStringLiteral("alphaAssociated")).toBool());
    EXPECT_EQ(restored.value(QStringLiteral("orientation")).toInt(), 8);
    EXPECT_DOUBLE_EQ(restored.value(QStringLiteral("pixelAspect")).toDouble(), 1.0);
    EXPECT_EQ(restored.value(QStringLiteral("bitsPerChannel")).toInt(), 0);
    EXPECT_EQ(restored.value(QStringLiteral("iccProfileBytes")).toInt(), 0);
    EXPECT_EQ(restored.value(QStringLiteral("colorSpace")).toString().size(), 4096);
    const QJsonArray restoredNames =
        restored.value(QStringLiteral("channelNames")).toArray();
    ASSERT_EQ(restoredNames.size(), 64);
    EXPECT_EQ(restoredNames.at(0).toString().size(), 256);
}

TEST(StillImageLayerRoundTripTest, PsdSubimageIndexClampsAndRestores)
{
    Artifact::ArtifactImageLayer layer;
    EXPECT_EQ(layer.psdSubimageIndex(), -1);

    layer.setPsdSubimageIndex(-20);
    EXPECT_EQ(layer.psdSubimageIndex(), -1);
    layer.setPsdSubimageIndex(100001);
    EXPECT_EQ(layer.psdSubimageIndex(), 100000);

    Artifact::ArtifactImageLayer restored;
    restored.fromJsonProperties(layer.toJson());
    EXPECT_EQ(restored.psdSubimageIndex(), 100000);

    QJsonObject malformedState;
    malformedState[QStringLiteral("image.psdSubimageIndex")] = -3;
    restored.fromJsonProperties(malformedState);
    EXPECT_EQ(restored.psdSubimageIndex(), -1);
}

TEST(StillImageLayerRoundTripTest, DepthMeshOptionsClampWhenRestoredFromJson)
{
    QJsonObject state;
    state[QStringLiteral("image.depthMeshColumns")] = 1;
    state[QStringLiteral("image.depthMeshRows")] = 900;
    state[QStringLiteral("image.depthMeshWidth")] = -50.0;
    state[QStringLiteral("image.depthMeshHeight")] = 200000.0;
    state[QStringLiteral("image.depthMeshDepthScale")] = -200000.0;
    state[QStringLiteral("image.depthMeshInvert")] = true;

    Artifact::ArtifactImageLayer layer;
    layer.fromJsonProperties(state);
    const QJsonObject restored = layer.toJson();
    EXPECT_EQ(restored.value(QStringLiteral("image.depthMeshColumns")).toInt(), 2);
    EXPECT_EQ(restored.value(QStringLiteral("image.depthMeshRows")).toInt(), 512);
    EXPECT_NEAR(restored.value(QStringLiteral("image.depthMeshWidth")).toDouble(),
                0.001, 1e-9);
    EXPECT_DOUBLE_EQ(restored.value(QStringLiteral("image.depthMeshHeight")).toDouble(),
                     100000.0);
    EXPECT_DOUBLE_EQ(restored.value(QStringLiteral("image.depthMeshDepthScale")).toDouble(),
                     -100000.0);
    EXPECT_TRUE(restored.value(QStringLiteral("image.depthMeshInvert")).toBool());
}

TEST(StillImageLayerRoundTripTest, PropertiesAndPixelsSurviveProjectReload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(3, 2, QImage::Format_RGBA8888);
    const QColor expectedColors[2][3] = {
        {QColor(17, 83, 149, 211), QColor(230, 12, 61, 255),
         QColor(4, 190, 77, 127)},
        {QColor(91, 33, 202, 96), QColor(128, 128, 128, 160),
         QColor(250, 201, 9, 238)}};
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            sourceImage.setPixelColor(x, y, expectedColors[y][x]);
        }
    }
    const QString imagePath = dir.filePath(QStringLiteral("still-image.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("StillImageLayerRoundTrip"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(makeCompositionParams());
    ASSERT_TRUE(composition.success);

    Artifact::ArtifactLayerInitParams layerParams(
        QStringLiteral("StillImage"), Artifact::LayerType::Image);
    const auto added = projects.addLayerToComposition(composition.id, layerParams);
    ASSERT_TRUE(added.success);
    ASSERT_NE(added.layer.get(), nullptr);

    auto* imageLayer = dynamic_cast<Artifact::ArtifactImageLayer*>(added.layer.get());
    ASSERT_NE(imageLayer, nullptr);
    ASSERT_TRUE(imageLayer->loadFromPath(imagePath));
    imageLayer->transform2D().setPosition(12.5f, -7.0f);
    imageLayer->transform2D().setRotation(27.0f);
    imageLayer->transform2D().setScale(1.25f, 0.75f);
    imageLayer->setOpacity(0.4f);
    imageLayer->setVisible(false);
    imageLayer->setFitToLayer(false);
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(0.25)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropY"), QVariant(0.25)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.5)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(1.5)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.panX"), QVariant(0.125)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.panY"), QVariant(-0.25)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(1.5)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.rotation"), QVariant(13.0)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"), QVariant(0.25)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorY"), QVariant(0.75)));
    ASSERT_TRUE(imageLayer->setLayerPropertyValue(
        QStringLiteral("sourceCrop.preserveAspect"), QVariant(false)));

    const QString projectPath = dir.filePath(QStringLiteral("still-image.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const auto loadedComposition = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(loadedComposition);
    const auto loadedLayerBase = loadedComposition->frontMostLayer();
    ASSERT_NE(loadedLayerBase.get(), nullptr);
    auto* loadedImageLayer =
        dynamic_cast<Artifact::ArtifactImageLayer*>(loadedLayerBase.get());
    ASSERT_NE(loadedImageLayer, nullptr);

    EXPECT_EQ(loadedImageLayer->layerName(), QStringLiteral("StillImage"));
    EXPECT_EQ(loadedImageLayer->sourcePath(), imagePath);
    EXPECT_FALSE(loadedImageLayer->isVisible());
    EXPECT_NEAR(loadedImageLayer->opacity(), 0.4f, 1e-6f);
    EXPECT_FALSE(loadedImageLayer->fitToLayer());
    const Artifact::SourceCrop crop = loadedImageLayer->sourceCrop();
    EXPECT_FALSE(crop.enabled());
    EXPECT_NEAR(crop.cropRect().x(), 0.25, 1e-6);
    EXPECT_NEAR(crop.cropRect().y(), 0.25, 1e-6);
    EXPECT_NEAR(crop.cropRect().width(), 2.5, 1e-6);
    EXPECT_NEAR(crop.cropRect().height(), 1.5, 1e-6);
    EXPECT_NEAR(crop.pan().x(), 0.125, 1e-6);
    EXPECT_NEAR(crop.pan().y(), -0.25, 1e-6);
    EXPECT_NEAR(crop.zoom(), 1.5, 1e-6);
    EXPECT_NEAR(crop.rotation(), 13.0, 1e-6);
    EXPECT_NEAR(crop.anchor().x(), 0.25, 1e-6);
    EXPECT_NEAR(crop.anchor().y(), 0.75, 1e-6);
    EXPECT_FALSE(crop.preserveAspect());

    float x = 0.0f;
    float y = 0.0f;
    loadedImageLayer->transform2D().position(x, y);
    EXPECT_NEAR(x, 12.5f, 1e-6f);
    EXPECT_NEAR(y, -7.0f, 1e-6f);
    EXPECT_NEAR(loadedImageLayer->transform2D().rotation(), 27.0f, 1e-6f);
    float scaleX = 0.0f;
    float scaleY = 0.0f;
    loadedImageLayer->transform2D().scale(scaleX, scaleY);
    EXPECT_NEAR(scaleX, 1.25f, 1e-6f);
    EXPECT_NEAR(scaleY, 0.75f, 1e-6f);

    const QImage loadedPixels = waitForImage(*loadedImageLayer, QSize(3, 2));
    ASSERT_EQ(loadedPixels.size(), QSize(3, 2));
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            const QColor actual = loadedPixels.pixelColor(x, y);
            const QColor expected = expectedColors[y][x];
            EXPECT_NEAR(actual.red(), expected.red(), 2)
                << "red at (" << x << ", " << y << ")";
            EXPECT_NEAR(actual.green(), expected.green(), 2)
                << "green at (" << x << ", " << y << ")";
            EXPECT_NEAR(actual.blue(), expected.blue(), 2)
                << "blue at (" << x << ", " << y << ")";
            EXPECT_NEAR(actual.alpha(), expected.alpha(), 1)
                << "alpha at (" << x << ", " << y << ")";
        }
    }
}

TEST(StillImageLayerRoundTripTest, ThumbnailFitsTargetWithoutChangingAspect)
{
    QImage sourceImage(4, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(60, 120, 180, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);

    EXPECT_EQ(layer.getThumbnail(4, 4).size(), QSize(4, 2));
    EXPECT_EQ(layer.getThumbnail(8, 8).size(), QSize(8, 4));
    EXPECT_EQ(layer.getThumbnail(0, 0).size(), QSize(1, 1));
}

TEST(StillImageLayerRoundTripTest, MissingImageThumbnailScalesPlaceholder)
{
    Artifact::ArtifactImageLayer layer;
    const QImage thumbnail = layer.getThumbnail(64, 32);
    EXPECT_EQ(thumbnail.size(), QSize(32, 32));
}

TEST(StillImageLayerRoundTripTest,
     SourceVersionInvalidationReloadsUpdatedImageAtSamePath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QString imagePath = dir.filePath(QStringLiteral("versioned.png"));
    const QColor originalColor(35, 85, 135, 255);
    const QColor replacementColor(185, 125, 65, 255);
    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(originalColor);
    ASSERT_TRUE(image.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    ASSERT_EQ(waitForImage(layer, QSize(1, 1)).pixelColor(0, 0), originalColor);
    const QUuid sourceId = layer.sourceAssetId();
    ASSERT_FALSE(sourceId.isNull());
    const std::uint64_t originalVersion = layer.sourceVersion();
    ASSERT_GT(originalVersion, 0u);

    image.fill(replacementColor);
    ASSERT_TRUE(image.save(imagePath, "PNG"));
    const std::uint64_t replacementVersion =
        ArtifactCore::AssetManager::instance().invalidateSource(sourceId);
    ASSERT_GT(replacementVersion, originalVersion);

    const QImage refreshed = waitForImage(layer, QSize(1, 1));
    EXPECT_EQ(refreshed.pixelColor(0, 0), replacementColor);
    EXPECT_EQ(layer.sourceVersion(), replacementVersion);
}

TEST(StillImageLayerRoundTripTest,
     ImageBufferInputReplacesFileSourceAndClearsIdentity)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(1, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(40, 80, 120, 255));
    const QString imagePath = dir.filePath(QStringLiteral("buffer-source.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    ASSERT_EQ(layer.sourcePath(), imagePath);
    ASSERT_FALSE(layer.sourceAssetId().isNull());

    const float pixels[] = {0.25f, 0.5f, 0.75f, 1.0f,
                            0.8f, 0.6f, 0.4f, 0.2f};
    ArtifactCore::ImageF32x4_RGBA buffer;
    buffer.setFromRGBA32F(pixels, 2, 1);
    layer.setFromImageBuffer(buffer);

    EXPECT_TRUE(layer.sourcePath().isEmpty());
    EXPECT_TRUE(layer.sourceAssetId().isNull());
    EXPECT_FALSE(layer.isImageSequence());
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.currentFrameBuffer().width(), 2);
    EXPECT_EQ(layer.currentFrameBuffer().height(), 1);
}

TEST(StillImageLayerRoundTripTest, EmptyImageBufferClearsCurrentFrame)
{
    Artifact::ArtifactImageLayer layer;
    const float pixels[] = {0.2f, 0.4f, 0.6f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA buffer;
    buffer.setFromRGBA32F(pixels, 1, 1);
    layer.setFromImageBuffer(buffer);
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());

    const ArtifactCore::ImageF32x4_RGBA emptyBuffer;
    layer.setFromImageBuffer(emptyBuffer);
    EXPECT_FALSE(layer.hasCurrentFrameBuffer());
    EXPECT_TRUE(layer.sourcePath().isEmpty());
    EXPECT_TRUE(layer.sourceAssetId().isNull());
    EXPECT_EQ(layer.currentFrameBuffer().width(), 0);
}

TEST(StillImageLayerRoundTripTest,
     ImageBufferInputRecordsRgbaFloatSourceMetadata)
{
    const float pixels[] = {0.2f, 0.4f, 0.6f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA buffer;
    buffer.setFromRGBA32F(pixels, 1, 1);

    Artifact::ArtifactImageLayer layer;
    layer.setFromImageBuffer(buffer);

    const QJsonObject metadata = layer.toJson()
        .value(QStringLiteral("image.sourceMetadata")).toObject();
    EXPECT_EQ(metadata.value(QStringLiteral("channelCount")).toInt(), 4);
    EXPECT_EQ(metadata.value(QStringLiteral("alphaChannel")).toInt(), 3);
    EXPECT_EQ(metadata.value(QStringLiteral("bitsPerChannel")).toInt(), 32);
    const QJsonArray channelNames =
        metadata.value(QStringLiteral("channelNames")).toArray();
    ASSERT_EQ(channelNames.size(), 4);
    EXPECT_EQ(channelNames.at(0).toString(), QStringLiteral("R"));
    EXPECT_EQ(channelNames.at(1).toString(), QStringLiteral("G"));
    EXPECT_EQ(channelNames.at(2).toString(), QStringLiteral("B"));
    EXPECT_EQ(channelNames.at(3).toString(), QStringLiteral("A"));
}

TEST(StillImageLayerRoundTripTest, ImageBufferInputDeepCopiesCallerStorage)
{
    const float originalPixels[] = {0.25f, 0.5f, 0.75f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA callerBuffer;
    callerBuffer.setFromRGBA32F(originalPixels, 1, 1);

    Artifact::ArtifactImageLayer layer;
    layer.setFromImageBuffer(callerBuffer);
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());

    const float replacementPixels[] = {0.9f, 0.1f, 0.3f, 0.4f};
    callerBuffer.setFromRGBA32F(replacementPixels, 1, 1);

    const float* layerPixels = layer.currentFrameBuffer().rgba32fData();
    ASSERT_NE(layerPixels, nullptr);
    EXPECT_FLOAT_EQ(layerPixels[0], 0.25f);
    EXPECT_FLOAT_EQ(layerPixels[1], 0.5f);
    EXPECT_FLOAT_EQ(layerPixels[2], 0.75f);
    EXPECT_FLOAT_EQ(layerPixels[3], 1.0f);
    EXPECT_EQ(layer.currentFrameBuffer().width(), 1);
    EXPECT_EQ(layer.currentFrameBuffer().height(), 1);
}

TEST(StillImageLayerRoundTripTest,
     OversizedQImageInputIsRejectedWithoutReplacingCurrentPixels)
{
    QImage original(1, 1, QImage::Format_RGBA8888);
    original.fill(QColor(45, 95, 145, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(original);

    QImage oversized(16385, 1, QImage::Format_RGBA8888);
    ASSERT_FALSE(oversized.isNull());
    oversized.fill(QColor(200, 30, 60, 255));
    layer.setFromQImage(oversized);

    EXPECT_EQ(layer.currentFrameBuffer().width(), 1);
    EXPECT_EQ(layer.currentFrameBuffer().height(), 1);
    EXPECT_EQ(layer.toQImage().pixelColor(0, 0), QColor(45, 95, 145, 255));
}

TEST(StillImageLayerRoundTripTest,
     OversizedImageBufferInputIsRejectedWithoutReplacingCurrentPixels)
{
    const float originalPixels[] = {0.2f, 0.4f, 0.6f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA original;
    original.setFromRGBA32F(originalPixels, 1, 1);
    Artifact::ArtifactImageLayer layer;
    layer.setFromImageBuffer(original);
    const QImage originalQtImage = layer.toQImage();

    ArtifactCore::ImageF32x4_RGBA oversized;
    oversized.resize(16385, 1);
    ASSERT_EQ(oversized.width(), 16385);
    layer.setFromImageBuffer(oversized);

    EXPECT_EQ(layer.currentFrameBuffer().width(), 1);
    EXPECT_EQ(layer.currentFrameBuffer().height(), 1);
    EXPECT_EQ(layer.toQImage(), originalQtImage);
}

TEST(StillImageLayerRoundTripTest,
     PremultipliedQImageInputRestoresStraightColorAndAlpha)
{
    QImage premultiplied(1, 1, QImage::Format_ARGB32_Premultiplied);
    premultiplied.setPixelColor(0, 0, QColor(200, 100, 50, 128));

    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(premultiplied);
    const QColor output = layer.toQImage().pixelColor(0, 0);
    EXPECT_NEAR(output.red(), 200, 2);
    EXPECT_NEAR(output.green(), 100, 2);
    EXPECT_NEAR(output.blue(), 50, 2);
    EXPECT_NEAR(output.alpha(), 128, 1);
}

TEST(StillImageLayerRoundTripTest, QImageInputIsIndependentOfCallerMutation)
{
    QImage callerImage(2, 1, QImage::Format_RGBA8888);
    callerImage.setPixelColor(0, 0, QColor(35, 85, 135, 255));
    callerImage.setPixelColor(1, 0, QColor(185, 125, 65, 255));

    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(callerImage);
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());

    callerImage.fill(QColor(5, 15, 25, 255));

    const QImage layerImage = layer.toQImage();
    ASSERT_EQ(layerImage.size(), QSize(2, 1));
    EXPECT_EQ(layerImage.pixelColor(0, 0), QColor(35, 85, 135, 255));
    EXPECT_EQ(layerImage.pixelColor(1, 0), QColor(185, 125, 65, 255));
}

TEST(StillImageLayerRoundTripTest,
     RgbQImageInputStaysOpaqueAndRecordsNoAlphaChannel)
{
    QImage rgb(3, 1, QImage::Format_RGB888);
    rgb.setPixelColor(0, 0, QColor(15, 45, 75));
    rgb.setPixelColor(1, 0, QColor(85, 125, 165));
    rgb.setPixelColor(2, 0, QColor(205, 175, 145));

    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(rgb);
    const QImage output = layer.toQImage();
    ASSERT_EQ(output.size(), QSize(3, 1));
    EXPECT_EQ(output.pixelColor(0, 0), QColor(15, 45, 75, 255));
    EXPECT_EQ(output.pixelColor(1, 0), QColor(85, 125, 165, 255));
    EXPECT_EQ(output.pixelColor(2, 0), QColor(205, 175, 145, 255));

    const QJsonObject metadata = layer.toJson()
        .value(QStringLiteral("image.sourceMetadata")).toObject();
    EXPECT_EQ(metadata.value(QStringLiteral("channelCount")).toInt(), 4);
    EXPECT_EQ(metadata.value(QStringLiteral("alphaChannel")).toInt(), -1);
    EXPECT_EQ(metadata.value(QStringLiteral("channelNames")).toArray().size(), 3);
}

TEST(StillImageLayerRoundTripTest, CvMatBgrInputConvertsToRgbPixels)
{
    cv::Mat bgr(1, 2, CV_8UC3);
    bgr.at<cv::Vec3b>(0, 0) = cv::Vec3b(10, 20, 30);
    bgr.at<cv::Vec3b>(0, 1) = cv::Vec3b(70, 110, 150);

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(bgr);
    const QImage converted = layer.toQImage();
    ASSERT_EQ(converted.size(), QSize(2, 1));
    EXPECT_EQ(converted.pixelColor(0, 0), QColor(30, 20, 10, 255));
    EXPECT_EQ(converted.pixelColor(1, 0), QColor(150, 110, 70, 255));
}

TEST(StillImageLayerRoundTripTest, CvMatBgrRoiRespectsNonContiguousRowStride)
{
    cv::Mat backing(2, 4, CV_8UC3, cv::Scalar(1, 2, 3));
    backing.at<cv::Vec3b>(0, 1) = cv::Vec3b(10, 20, 30);
    backing.at<cv::Vec3b>(0, 2) = cv::Vec3b(40, 50, 60);
    backing.at<cv::Vec3b>(1, 1) = cv::Vec3b(70, 80, 90);
    backing.at<cv::Vec3b>(1, 2) = cv::Vec3b(100, 110, 120);
    const cv::Mat roi = backing(cv::Rect(1, 0, 2, 2));
    ASSERT_FALSE(roi.isContinuous());

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(roi);
    const QImage converted = layer.toQImage();
    ASSERT_EQ(converted.size(), QSize(2, 2));
    EXPECT_EQ(converted.pixelColor(0, 0), QColor(30, 20, 10, 255));
    EXPECT_EQ(converted.pixelColor(1, 0), QColor(60, 50, 40, 255));
    EXPECT_EQ(converted.pixelColor(0, 1), QColor(90, 80, 70, 255));
    EXPECT_EQ(converted.pixelColor(1, 1), QColor(120, 110, 100, 255));
}

TEST(StillImageLayerRoundTripTest,
     FloatBgraCvMatRoiRespectsRowStrideAndPreservesAlpha)
{
    cv::Mat backing(2, 4, CV_32FC4, cv::Scalar(0.01f, 0.02f, 0.03f, 1.0f));
    backing.at<cv::Vec4f>(0, 1) = cv::Vec4f(0.1f, 0.2f, 0.3f, 0.4f);
    backing.at<cv::Vec4f>(0, 2) = cv::Vec4f(0.25f, 0.5f, 0.75f, 1.0f);
    backing.at<cv::Vec4f>(1, 1) = cv::Vec4f(0.8f, 0.6f, 0.4f, 0.2f);
    backing.at<cv::Vec4f>(1, 2) = cv::Vec4f(0.0f, 0.1f, 0.2f, 0.3f);
    const cv::Mat roi = backing(cv::Rect(1, 0, 2, 2));
    ASSERT_FALSE(roi.isContinuous());

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(roi);
    const QImage converted = layer.toQImage();
    ASSERT_EQ(converted.size(), QSize(2, 2));
    EXPECT_NEAR(converted.pixelColor(0, 0).red(), 77, 1);
    EXPECT_NEAR(converted.pixelColor(0, 0).green(), 51, 1);
    EXPECT_NEAR(converted.pixelColor(0, 0).blue(), 26, 1);
    EXPECT_NEAR(converted.pixelColor(0, 0).alpha(), 102, 1);
    EXPECT_NEAR(converted.pixelColor(1, 1).red(), 51, 1);
    EXPECT_NEAR(converted.pixelColor(1, 1).green(), 26, 1);
    EXPECT_EQ(converted.pixelColor(1, 1).blue(), 0);
    EXPECT_NEAR(converted.pixelColor(1, 1).alpha(), 77, 1);
}

TEST(StillImageLayerRoundTripTest, CvMatGrayscaleExpandsToOpaqueRgb)
{
    cv::Mat gray(1, 3, CV_8UC1);
    gray.at<std::uint8_t>(0, 0) = 23;
    gray.at<std::uint8_t>(0, 1) = 128;
    gray.at<std::uint8_t>(0, 2) = 231;

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(gray);
    const QImage converted = layer.toQImage();
    ASSERT_EQ(converted.size(), QSize(3, 1));
    EXPECT_EQ(converted.pixelColor(0, 0), QColor(23, 23, 23, 255));
    EXPECT_EQ(converted.pixelColor(1, 0), QColor(128, 128, 128, 255));
    EXPECT_EQ(converted.pixelColor(2, 0), QColor(231, 231, 231, 255));
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferConvertsSixteenBitCvMatGrayscaleToRgbaFloat)
{
    cv::Mat gray(1, 2, CV_16UC1);
    gray.at<std::uint16_t>(0, 0) = 32768;
    gray.at<std::uint16_t>(0, 1) = 65535;

    ArtifactCore::ImageF32x4_RGBA buffer;
    buffer.setFromCVMat(gray);
    ASSERT_EQ(buffer.width(), 2);
    ASSERT_EQ(buffer.height(), 1);
    const float* rgba = buffer.rgba32fData();
    ASSERT_NE(rgba, nullptr);
    const float middle = 32768.0f / 65535.0f;
    EXPECT_NEAR(rgba[0], middle, 1e-6f);
    EXPECT_NEAR(rgba[1], middle, 1e-6f);
    EXPECT_NEAR(rgba[2], middle, 1e-6f);
    EXPECT_FLOAT_EQ(rgba[3], 1.0f);
    EXPECT_FLOAT_EQ(rgba[4], 1.0f);
    EXPECT_FLOAT_EQ(rgba[5], 1.0f);
    EXPECT_FLOAT_EQ(rgba[6], 1.0f);
    EXPECT_FLOAT_EQ(rgba[7], 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferConvertsSixteenBitCvMatBgrToRgbaFloat)
{
    cv::Mat bgr(1, 1, CV_16UC3);
    bgr.at<cv::Vec<std::uint16_t, 3>>(0, 0) =
        cv::Vec<std::uint16_t, 3>(16384, 32768, 65535);

    ArtifactCore::ImageF32x4_RGBA buffer;
    buffer.setFromCVMat(bgr);
    const float* rgba = buffer.rgba32fData();
    ASSERT_NE(rgba, nullptr);
    EXPECT_FLOAT_EQ(rgba[0], 1.0f);
    EXPECT_NEAR(rgba[1], 32768.0f / 65535.0f, 1e-6f);
    EXPECT_NEAR(rgba[2], 16384.0f / 65535.0f, 1e-6f);
    EXPECT_FLOAT_EQ(rgba[3], 1.0f);
}

TEST(StillImageLayerRoundTripTest, SixteenBitCvMatGrayscaleIsNotAccepted)
{
    cv::Mat gray(1, 3, CV_16UC1);
    gray.at<std::uint16_t>(0, 0) = 0;
    gray.at<std::uint16_t>(0, 1) = 32768;
    gray.at<std::uint16_t>(0, 2) = 65535;

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(gray);
    EXPECT_FALSE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.toQImage().size(), QSize(256, 256));
}

TEST(StillImageLayerRoundTripTest, SixteenBitCvMatBgrIsNotAccepted)
{
    cv::Mat bgr(1, 1, CV_16UC3);
    bgr.at<cv::Vec<std::uint16_t, 3>>(0, 0) =
        cv::Vec<std::uint16_t, 3>(16384, 32768, 65535);

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(bgr);
    EXPECT_FALSE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.toQImage().size(), QSize(256, 256));
}

TEST(StillImageLayerRoundTripTest, CvMatBgraInputPreservesChannelOrderAndAlpha)
{
    cv::Mat bgra(1, 1, CV_8UC4);
    bgra.at<cv::Vec4b>(0, 0) = cv::Vec4b(10, 20, 30, 40);

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(bgra);
    EXPECT_EQ(layer.toQImage().pixelColor(0, 0), QColor(30, 20, 10, 40));
}

TEST(StillImageLayerRoundTripTest,
     FloatCvMatInputClampsChannelsAndReplacesNaN)
{
    cv::Mat bgr(1, 2, CV_32FC3);
    bgr.at<cv::Vec3f>(0, 0) = cv::Vec3f(0.2f, 0.4f, 1.5f);
    bgr.at<cv::Vec3f>(0, 1) = cv::Vec3f(
        std::numeric_limits<float>::quiet_NaN(), -0.3f, 0.75f);

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(bgr);
    const QImage converted = layer.toQImage();
    ASSERT_EQ(converted.size(), QSize(2, 1));
    EXPECT_NEAR(converted.pixelColor(0, 0).red(), 255, 1);
    EXPECT_NEAR(converted.pixelColor(0, 0).green(), 102, 1);
    EXPECT_NEAR(converted.pixelColor(0, 0).blue(), 51, 1);
    EXPECT_EQ(converted.pixelColor(0, 0).alpha(), 255);
    EXPECT_NEAR(converted.pixelColor(1, 0).red(), 191, 1);
    EXPECT_EQ(converted.pixelColor(1, 0).green(), 0);
    EXPECT_EQ(converted.pixelColor(1, 0).blue(), 0);
    EXPECT_EQ(converted.pixelColor(1, 0).alpha(), 255);
}

TEST(StillImageLayerRoundTripTest,
     FloatCvMatGrayscaleExpandsToOpaqueRgb)
{
    cv::Mat gray(1, 3, CV_32FC1);
    gray.at<float>(0, 0) = 0.5f;
    gray.at<float>(0, 1) = std::numeric_limits<float>::quiet_NaN();
    gray.at<float>(0, 2) = 1.4f;

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(gray);
    const QImage converted = layer.toQImage();
    ASSERT_EQ(converted.size(), QSize(3, 1));
    const QColor middle = converted.pixelColor(0, 0);
    EXPECT_NEAR(middle.red(), 128, 1);
    EXPECT_NEAR(middle.green(), 128, 1);
    EXPECT_NEAR(middle.blue(), 128, 1);
    EXPECT_EQ(middle.alpha(), 255);
    EXPECT_EQ(converted.pixelColor(1, 0), QColor(0, 0, 0, 255));
    EXPECT_EQ(converted.pixelColor(2, 0), QColor(255, 255, 255, 255));
}

TEST(StillImageLayerRoundTripTest, FloatCvMatBgraPreservesAlphaAndChannelOrder)
{
    cv::Mat bgra(1, 1, CV_32FC4);
    bgra.at<cv::Vec4f>(0, 0) = cv::Vec4f(-0.2f, 0.4f, 1.5f, 0.5f);

    Artifact::ArtifactImageLayer layer;
    layer.setFromCvMat(bgra);
    const QColor pixel = layer.toQImage().pixelColor(0, 0);
    EXPECT_EQ(pixel.red(), 255);
    EXPECT_NEAR(pixel.green(), 102, 1);
    EXPECT_EQ(pixel.blue(), 0);
    EXPECT_NEAR(pixel.alpha(), 128, 1);
}

TEST(StillImageLayerRoundTripTest, EmptyCvMatClearsExistingImage)
{
    QImage sourceImage(2, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(180, 70, 40, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());

    layer.setFromCvMat(cv::Mat{});
    EXPECT_FALSE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.toQImage().size(), QSize(256, 256));
}

TEST(StillImageLayerRoundTripTest,
     ImageSequenceNormalizesPathsAndRejectsEmptyInputAtomically)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage image(2, 1, QImage::Format_RGBA8888);
    image.fill(QColor(70, 110, 150, 255));
    const QString firstPath = dir.filePath(QStringLiteral("frame-0001.png"));
    const QString secondPath = dir.filePath(QStringLiteral("frame-0002.png"));
    ASSERT_TRUE(image.save(firstPath, "PNG"));
    image.setPixelColor(0, 0, QColor(180, 90, 30, 255));
    ASSERT_TRUE(image.save(secondPath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(
        QStringList{QString(), QStringLiteral("  ") + firstPath + QStringLiteral("  "),
                    firstPath, secondPath},
        24.0));
    EXPECT_TRUE(layer.isImageSequence());
    EXPECT_EQ(layer.sequenceFramePaths(), QStringList({firstPath, secondPath}));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 24.0);
    EXPECT_EQ(layer.sourcePath(), firstPath);

    EXPECT_FALSE(layer.setImageSequence({}, 48.0));
    EXPECT_TRUE(layer.isImageSequence());
    EXPECT_EQ(layer.sequenceFramePaths(), QStringList({firstPath, secondPath}));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 24.0);
    EXPECT_EQ(layer.sourcePath(), firstPath);
}

TEST(StillImageLayerRoundTripTest,
     ImageSequenceDisplaysRequestedFrameAndClampsOutOfRangeFrames)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QColor frameColors[] = {
        QColor(25, 65, 105, 255),
        QColor(85, 125, 165, 255),
        QColor(175, 135, 95, 255)};
    QStringList framePaths;
    for (int index = 0; index < 3; ++index) {
        QImage image(1, 1, QImage::Format_RGBA8888);
        image.fill(frameColors[index]);
        const QString path = dir.filePath(
            QStringLiteral("sequence-%1.png").arg(index));
        ASSERT_TRUE(image.save(path, "PNG"));
        framePaths.append(path);
    }

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(framePaths, 0.0));

    layer.goToFrame(1);
    EXPECT_EQ(waitForSequenceFrame(layer, 1).pixelColor(0, 0), frameColors[1]);
    EXPECT_EQ(layer.sequenceCachedFrameIndex(), 1);

    layer.goToFrame(100);
    EXPECT_EQ(waitForSequenceFrame(layer, 2).pixelColor(0, 0), frameColors[2]);
    EXPECT_EQ(layer.sequenceCachedFrameIndex(), 2);

    layer.goToFrame(-100);
    EXPECT_EQ(waitForSequenceFrame(layer, 0).pixelColor(0, 0), frameColors[0]);
    EXPECT_EQ(layer.sequenceCachedFrameIndex(), 0);
}

TEST(StillImageLayerRoundTripTest,
     MissingImageSequenceFrameDoesNotKeepStalePixels)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QColor firstColor(45, 95, 145, 255);
    QImage firstFrame(2, 1, QImage::Format_RGBA8888);
    firstFrame.fill(firstColor);
    const QString firstPath = dir.filePath(QStringLiteral("gap-0001.png"));
    const QString missingPath = dir.filePath(QStringLiteral("gap-0002.png"));
    ASSERT_TRUE(firstFrame.save(firstPath, "PNG"));
    ASSERT_FALSE(QFileInfo::exists(missingPath));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(
        QStringList{firstPath, missingPath}, 0.0));
    EXPECT_EQ(waitForSequenceFrame(layer, 0).pixelColor(0, 0), firstColor);

    layer.goToFrame(1);
    QImage unavailableFrame;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 1000) {
        QCoreApplication::processEvents();
        unavailableFrame = layer.toQImage();
        if (unavailableFrame.size() == QSize(256, 256)) {
            break;
        }
        QThread::msleep(2);
    }

    EXPECT_EQ(layer.sequenceCachedFrameIndex(), -1);
    EXPECT_EQ(unavailableFrame.size(), QSize(256, 256));
    EXPECT_NE(unavailableFrame.pixelColor(0, 0), firstColor);

    const QColor restoredColor(155, 105, 55, 255);
    QImage restoredFrame(2, 1, QImage::Format_RGBA8888);
    restoredFrame.fill(restoredColor);
    ASSERT_TRUE(restoredFrame.save(missingPath, "PNG"));
    ASSERT_TRUE(layer.setImageSequence(
        QStringList{firstPath, missingPath}, 0.0));
    layer.goToFrame(1);
    EXPECT_EQ(waitForSequenceFrame(layer, 1).pixelColor(0, 0), restoredColor);
    EXPECT_EQ(layer.sequenceCachedFrameIndex(), 1);
}

TEST(StillImageLayerRoundTripTest,
     ReplacedSequenceFrameCannotReuseStaleDecodedPixels)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QColor firstColor(35, 85, 135, 255);
    const QColor secondColor(185, 125, 65, 255);
    QImage firstFrame(1, 1, QImage::Format_RGBA8888);
    firstFrame.fill(firstColor);
    QImage secondFrame(1, 1, QImage::Format_RGBA8888);
    secondFrame.fill(secondColor);
    const QString firstPath = dir.filePath(QStringLiteral("replace-0001.png"));
    const QString secondPath = dir.filePath(QStringLiteral("replace-0002.png"));
    ASSERT_TRUE(firstFrame.save(firstPath, "PNG"));
    ASSERT_TRUE(secondFrame.save(secondPath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(
        QStringList{firstPath, secondPath}, 0.0));
    layer.goToFrame(1);
    EXPECT_EQ(waitForSequenceFrame(layer, 1).pixelColor(0, 0), secondColor);

    layer.goToFrame(0);
    EXPECT_EQ(waitForSequenceFrame(layer, 0).pixelColor(0, 0), firstColor);

    QFile replacement(secondPath);
    ASSERT_TRUE(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray corruptPayload(512, 'x');
    ASSERT_EQ(replacement.write(corruptPayload), qsizetype(corruptPayload.size()));
    replacement.close();

    layer.goToFrame(1);
    const QImage unavailableFrame = layer.toQImage();

    EXPECT_EQ(layer.sequenceCachedFrameIndex(), -1);
    EXPECT_EQ(unavailableFrame.size(), QSize(256, 256));
    EXPECT_NE(unavailableFrame.pixelColor(0, 0), secondColor);
}

TEST(StillImageLayerRoundTripTest,
     ReplacedSequenceFrameReloadsValidContentAtSamePath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);
    QImage firstFrame(64, 1, QImage::Format_RGBA8888);
    firstFrame.fill(QColor(20, 60, 100, 255));
    QImage secondFrame(64, 1, QImage::Format_RGBA8888);
    secondFrame.fill(QColor(40, 80, 120, 255));
    const QString firstPath = dir.filePath(QStringLiteral("valid-1.png"));
    const QString secondPath = dir.filePath(QStringLiteral("valid-2.png"));
    ASSERT_TRUE(firstFrame.save(firstPath, "PNG"));
    ASSERT_TRUE(secondFrame.save(secondPath, "PNG"));
    const qint64 originalSize = QFileInfo(secondPath).size();

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(
        QStringList{firstPath, secondPath}, 0.0));
    EXPECT_EQ(waitForSequenceFrame(layer, 1).pixelColor(0, 0),
              QColor(40, 80, 120, 255));

    for (int x = 0; x < secondFrame.width(); ++x) {
        secondFrame.setPixelColor(
            x, 0, QColor((x * 37) % 256, (x * 71) % 256,
                         (x * 113) % 256, 255));
    }
    ASSERT_TRUE(secondFrame.save(secondPath, "PNG"));
    ASSERT_NE(QFileInfo(secondPath).size(), originalSize);
    const QColor replacementColor = secondFrame.pixelColor(17, 0);

    layer.goToFrame(0);
    ASSERT_EQ(waitForSequenceFrame(layer, 0).pixelColor(0, 0),
              QColor(20, 60, 100, 255));
    layer.goToFrame(1);
    EXPECT_EQ(waitForSequenceFrame(layer, 1).pixelColor(17, 0),
              replacementColor);
    EXPECT_EQ(layer.sequenceCachedFrameIndex(), 1);
}

TEST(StillImageLayerRoundTripTest, ImageSequenceFrameRateClampsInvalidValues)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(QColor(90, 130, 170, 255));
    const QString firstPath = dir.filePath(QStringLiteral("rate-0001.png"));
    const QString secondPath = dir.filePath(QStringLiteral("rate-0002.png"));
    ASSERT_TRUE(image.save(firstPath, "PNG"));
    ASSERT_TRUE(image.save(secondPath, "PNG"));
    const QStringList paths{firstPath, secondPath};

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(paths, 0.0001));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 0.001);
    ASSERT_TRUE(layer.setImageSequence(paths, 5000.0));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 1000.0);
    ASSERT_TRUE(layer.setImageSequence(
        paths, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 0.0);
    ASSERT_TRUE(layer.setImageSequence(
        paths, std::numeric_limits<double>::infinity()));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 0.0);
}

TEST(StillImageLayerRoundTripTest,
     ImageSequenceWithOnePathUsesStillImageFrameSemantics)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);
    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(QColor(35, 75, 115, 255));
    const QString path = dir.filePath(QStringLiteral("single-frame.png"));
    ASSERT_TRUE(image.save(path, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(QStringList{path}, 24.0));
    EXPECT_FALSE(layer.isImageSequence());
    EXPECT_EQ(layer.sequenceFramePaths(), QStringList{path});
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 24.0);
    EXPECT_EQ(layer.sourcePath(), path);
    EXPECT_EQ(waitForImage(layer, QSize(1, 1)).pixelColor(0, 0),
              QColor(35, 75, 115, 255));
}

TEST(StillImageLayerRoundTripTest,
     ImageSequenceFrameRatePropertyClampsAndRejectsInvalidValuesAtomically)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);
    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(QColor(45, 85, 125, 255));
    const QString firstPath = dir.filePath(QStringLiteral("property-1.png"));
    const QString secondPath = dir.filePath(QStringLiteral("property-2.png"));
    ASSERT_TRUE(image.save(firstPath, "PNG"));
    ASSERT_TRUE(image.save(secondPath, "PNG"));
    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setImageSequence(
        QStringList{firstPath, secondPath}, 24.0));

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("image.sequenceFrameRate"), QVariant(1200.0)));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 1000.0);
    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("image.sequenceFrameRate"), QVariant(-1.0)));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 1000.0);
    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("image.sequenceFrameRate"),
        QVariant(std::numeric_limits<double>::quiet_NaN())));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 1000.0);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("image.sequenceFrameRate"), QVariant(0.0)));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 0.0);
}

TEST(StillImageLayerRoundTripTest,
     ImageSequenceJsonRestoreNormalizesEntriesAndFrameRate)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(QColor(100, 140, 180, 255));
    const QString firstPath = dir.filePath(QStringLiteral("json-0001.png"));
    const QString secondPath = dir.filePath(QStringLiteral("json-0002.png"));
    ASSERT_TRUE(image.save(firstPath, "PNG"));
    ASSERT_TRUE(image.save(secondPath, "PNG"));

    QJsonArray paths;
    paths.append(firstPath);
    paths.append(firstPath);
    paths.append(QStringLiteral("   "));
    paths.append(42);
    paths.append(QStringLiteral("  ") + secondPath + QStringLiteral("  "));
    QJsonObject state;
    state[QStringLiteral("image.sourcePath")] = firstPath;
    state[QStringLiteral("image.sequencePaths")] = paths;
    state[QStringLiteral("image.sequenceFrameRate")] = 5000.0;

    Artifact::ArtifactImageLayer layer;
    layer.fromJsonProperties(state);
    EXPECT_TRUE(layer.isImageSequence());
    EXPECT_EQ(layer.sequenceFramePaths(), QStringList({firstPath, secondPath}));
    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 1000.0);
    EXPECT_EQ(layer.sourcePath(), firstPath);
}

TEST(StillImageLayerRoundTripTest,
     TemporarySourceOverrideSelectsNonEmptyBufferAndFallsBackWhenEmpty)
{
    QImage sourceImage(1, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(20, 40, 60, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    const auto* originalBuffer = &layer.currentFrameBuffer();
    ASSERT_FALSE(originalBuffer->isEmpty());

    const float overridePixels[] = {0.8f, 0.4f, 0.2f, 0.6f,
                                    0.1f, 0.3f, 0.9f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA overrideBuffer;
    overrideBuffer.setFromRGBA32F(overridePixels, 2, 1);
    layer.setTemporarySourceOverride(&overrideBuffer);
    EXPECT_TRUE(layer.hasTemporarySourceOverride());
    EXPECT_TRUE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(&layer.currentFrameBuffer(), &overrideBuffer);
    EXPECT_EQ(layer.currentFrameBuffer().width(), 2);
    EXPECT_EQ(layer.currentFrameBuffer().height(), 1);

    ArtifactCore::ImageF32x4_RGBA emptyOverride;
    layer.setTemporarySourceOverride(&emptyOverride);
    EXPECT_FALSE(layer.hasTemporarySourceOverride());
    EXPECT_EQ(&layer.currentFrameBuffer(), originalBuffer);

    layer.setTemporarySourceOverride(nullptr);
    EXPECT_FALSE(layer.hasTemporarySourceOverride());
    EXPECT_EQ(&layer.currentFrameBuffer(), originalBuffer);
}

TEST(StillImageLayerRoundTripTest,
     TemporarySourceOverrideFeedsLayerSourceResolver)
{
    QImage sourceImage(1, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(40, 80, 120, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    const auto* originalBuffer = &layer.currentFrameBuffer();
    ASSERT_FALSE(originalBuffer->isEmpty());
    EXPECT_EQ(layer.resolveLayerSourceOverride(), originalBuffer);

    const float replacementPixels[] = {0.2f, 0.4f, 0.6f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA replacement;
    replacement.setFromRGBA32F(replacementPixels, 1, 1);
    layer.setTemporarySourceOverride(&replacement);
    EXPECT_EQ(layer.resolveLayerSourceOverride(), &replacement);

    const ArtifactCore::ImageF32x4_RGBA emptyOverride;
    layer.setTemporarySourceOverride(&emptyOverride);
    EXPECT_EQ(layer.resolveLayerSourceOverride(), originalBuffer);
}

TEST(StillImageLayerRoundTripTest, MissingSourceReplacesPreviousPixelsWithPlaceholder)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(2, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(210, 40, 90, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());
    ASSERT_EQ(layer.toQImage().size(), QSize(2, 2));

    const QString missingPath = dir.filePath(QStringLiteral("missing.png"));
    ASSERT_FALSE(layer.loadFromPath(missingPath));

    EXPECT_EQ(layer.sourcePath(), missingPath);
    EXPECT_TRUE(layer.hasCurrentFrameBuffer());
    const QImage placeholder = layer.toQImage();
    EXPECT_EQ(placeholder.size(), QSize(256, 256));
    EXPECT_NE(placeholder.size(), QSize(2, 2));
}

TEST(StillImageLayerRoundTripTest,
     LoadingAnotherStillImageReplacesPixelsAndKeepsLayerEdits)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage firstImage(2, 1, QImage::Format_RGBA8888);
    firstImage.fill(QColor(35, 75, 115, 255));
    QImage secondImage(3, 2, QImage::Format_RGBA8888);
    secondImage.fill(QColor(175, 125, 65, 255));
    const QString firstPath = dir.filePath(QStringLiteral("still-first.png"));
    const QString secondPath = dir.filePath(QStringLiteral("still-second.png"));
    ASSERT_TRUE(firstImage.save(firstPath, "PNG"));
    ASSERT_TRUE(secondImage.save(secondPath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(firstPath));
    ASSERT_TRUE(waitForImageBuffer(layer, QSize(2, 1)));
    layer.transform2D().setPosition(9.0f, -4.0f);
    layer.setOpacity(0.6f);

    ASSERT_TRUE(layer.loadFromPath(secondPath));
    ASSERT_TRUE(waitForImageBuffer(layer, QSize(3, 2)));

    EXPECT_EQ(layer.sourcePath(), secondPath);
    const QImage replacedImage = waitForImage(layer, QSize(3, 2));
    EXPECT_EQ(replacedImage.pixelColor(1, 1), QColor(175, 125, 65, 255));
    EXPECT_NEAR(layer.opacity(), 0.6f, 1e-6f);
    float x = 0.0f;
    float y = 0.0f;
    layer.transform2D().position(x, y);
    EXPECT_NEAR(x, 9.0f, 1e-6f);
    EXPECT_NEAR(y, -4.0f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest, EmptySourceClearsImageState)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(2, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(30, 120, 200, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.hasCurrentFrameBuffer());

    EXPECT_TRUE(layer.loadFromPath(QString()));
    EXPECT_TRUE(layer.sourcePath().isEmpty());
    EXPECT_FALSE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.toQImage().size(), QSize(256, 256));
}

TEST(StillImageLayerRoundTripTest, CorruptPayloadRetainsPathAndProvidesPlaceholder)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QString corruptPath = dir.filePath(QStringLiteral("corrupt.png"));
    QFile corruptFile(corruptPath);
    ASSERT_TRUE(corruptFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(corruptFile.write(QByteArrayLiteral("not a PNG payload")),
              qsizetype(17));
    corruptFile.close();

    Artifact::ArtifactImageLayer layer;
    EXPECT_FALSE(layer.loadFromPath(corruptPath));
    EXPECT_EQ(layer.sourcePath(), corruptPath);
    EXPECT_TRUE(layer.hasCurrentFrameBuffer());

    const QImage placeholder = layer.toQImage();
    EXPECT_EQ(placeholder.size(), QSize(256, 256));
}

TEST(StillImageLayerRoundTripTest, ActiveCropLeavesPixelsOutsideCropTransparent)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(3, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(35, 125, 205, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    const QImage croppedCanvas = layer.toQImage();
    ASSERT_EQ(croppedCanvas.size(), QSize(3, 2));
    EXPECT_EQ(croppedCanvas.pixelColor(0, 0).alpha(), 0);
    EXPECT_EQ(croppedCanvas.pixelColor(2, 1).alpha(), 0);
    EXPECT_EQ(croppedCanvas.pixelColor(1, 0), QColor(35, 125, 205, 255));
    EXPECT_EQ(croppedCanvas.pixelColor(1, 1), QColor(35, 125, 205, 255));
}

TEST(StillImageLayerRoundTripTest, SourceCropZoomAndPanDefineEffectiveSourceRect)
{
    Artifact::SourceCrop crop;
    crop.setCropRect(QRectF(2.0, 2.0, 8.0, 6.0));
    crop.setZoom(2.0);
    crop.setPan(QPointF(1.0, -0.5));

    const QRectF effective = crop.effectiveCropRect(QSizeF(20.0, 20.0));
    EXPECT_DOUBLE_EQ(effective.x(), 5.0);
    EXPECT_DOUBLE_EQ(effective.y(), 3.0);
    EXPECT_DOUBLE_EQ(effective.width(), 4.0);
    EXPECT_DOUBLE_EQ(effective.height(), 3.0);
}

TEST(StillImageLayerRoundTripTest, ActiveCropSelectsTheRequestedSourcePixels)
{
    QImage sourceImage(4, 2, QImage::Format_RGBA8888);
    const QColor sourceColors[2][4] = {
        {QColor(10, 20, 30), QColor(40, 50, 60), QColor(70, 80, 90),
         QColor(100, 110, 120)},
        {QColor(130, 140, 150), QColor(160, 170, 180), QColor(190, 200, 210),
         QColor(220, 230, 240)}};
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            sourceImage.setPixelColor(x, y, sourceColors[y][x]);
        }
    }

    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropY"), QVariant(0.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    const QImage croppedCanvas = layer.toQImage();
    ASSERT_EQ(croppedCanvas.size(), QSize(4, 2));
    for (int y = 0; y < 2; ++y) {
        EXPECT_EQ(croppedCanvas.pixelColor(0, y).alpha(), 0);
        EXPECT_EQ(croppedCanvas.pixelColor(3, y).alpha(), 0);
        EXPECT_EQ(croppedCanvas.pixelColor(1, y), sourceColors[y][1]);
        EXPECT_EQ(croppedCanvas.pixelColor(2, y), sourceColors[y][2]);
    }
}

TEST(StillImageLayerRoundTripTest,
     ActiveCropPanAndZoomSelectExpectedPixelsInCpuOutput)
{
    QImage sourceImage(8, 8, QImage::Format_RGBA8888);
    for (int y = 0; y < sourceImage.height(); ++y) {
        for (int x = 0; x < sourceImage.width(); ++x) {
            sourceImage.setPixelColor(
                x, y, QColor(10 + x * 20, 15 + y * 20, 30 + x + y, 255));
        }
    }

    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropY"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(4.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(4.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panX"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panY"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    const QImage output = layer.toQImage();
    ASSERT_EQ(output.size(), QSize(8, 8));
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            if (x >= 4 && x < 6 && y >= 4 && y < 6) {
                EXPECT_EQ(output.pixelColor(x, y),
                          sourceImage.pixelColor(x, y));
            } else {
                EXPECT_EQ(output.pixelColor(x, y).alpha(), 0)
                    << "outside crop at (" << x << ", " << y << ")";
            }
        }
    }
}

TEST(StillImageLayerRoundTripTest,
     ActiveCropAndPanZoomDoNotChangeLocalBounds)
{
    QImage sourceImage(4, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(65, 105, 145, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    const QRectF originalBounds = layer.localBounds();

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropY"), QVariant(0.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panX"), QVariant(0.5)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panY"), QVariant(-0.25)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(1.75)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    const QRectF croppedBounds = layer.localBounds();
    EXPECT_DOUBLE_EQ(croppedBounds.x(), originalBounds.x());
    EXPECT_DOUBLE_EQ(croppedBounds.y(), originalBounds.y());
    EXPECT_DOUBLE_EQ(croppedBounds.width(), originalBounds.width());
    EXPECT_DOUBLE_EQ(croppedBounds.height(), originalBounds.height());
}

TEST(StillImageLayerRoundTripTest, CropOutputCacheTracksCropEditsAndDisable)
{
    QImage sourceImage(4, 1, QImage::Format_RGBA8888);
    const QColor colors[] = {
        QColor(20, 40, 60, 255), QColor(70, 90, 110, 255),
        QColor(120, 140, 160, 255), QColor(190, 170, 150, 255)};
    for (int x = 0; x < 4; ++x) {
        sourceImage.setPixelColor(x, 0, colors[x]);
    }

    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    const QImage firstCrop = layer.toQImage();
    EXPECT_EQ(firstCrop.pixelColor(0, 0), colors[0]);
    EXPECT_EQ(firstCrop.pixelColor(2, 0).alpha(), 0);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(2.0)));
    const QImage movedCrop = layer.toQImage();
    EXPECT_EQ(movedCrop.pixelColor(0, 0).alpha(), 0);
    EXPECT_EQ(movedCrop.pixelColor(2, 0), colors[2]);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(false)));
    EXPECT_EQ(layer.toQImage(), sourceImage);
}

TEST(StillImageLayerRoundTripTest,
     ActiveCropSurvivesJsonRestoreAndStillSelectsSourcePixels)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(4, 1, QImage::Format_RGBA8888);
    const QColor sourceColors[4] = {
        QColor(15, 35, 55), QColor(75, 95, 115),
        QColor(135, 155, 175), QColor(195, 215, 235)};
    for (int x = 0; x < 4; ++x) {
        sourceImage.setPixelColor(x, 0, sourceColors[x]);
    }
    const QString imagePath = dir.filePath(QStringLiteral("active-crop.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer source;
    ASSERT_TRUE(source.loadFromPath(imagePath));
    ASSERT_TRUE(waitForImageBuffer(source, QSize(4, 1)));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(1.0)));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.0)));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(1.0)));
    ASSERT_TRUE(source.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    Artifact::ArtifactImageLayer restored;
    restored.fromJsonProperties(source.toJson());
    ASSERT_TRUE(waitForImageBuffer(restored, QSize(4, 1)));
    EXPECT_TRUE(restored.sourceCropEnabled());

    const QImage result = restored.toQImage();
    ASSERT_EQ(result.size(), QSize(4, 1));
    EXPECT_EQ(result.pixelColor(0, 0).alpha(), 0);
    EXPECT_EQ(result.pixelColor(1, 0), sourceColors[1]);
    EXPECT_EQ(result.pixelColor(2, 0), sourceColors[2]);
    EXPECT_EQ(result.pixelColor(3, 0).alpha(), 0);
}

TEST(StillImageLayerRoundTripTest, MissingSourceCanBeRelinkedAfterProjectReload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    ProjectCleanup cleanup{projects};
    projects.closeCurrentProject();
    projects.suppressDefaultCreate(true);
    projects.createProject(QStringLiteral("MissingImageSourceRoundTrip"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    const auto composition = projects.createComposition(makeCompositionParams());
    ASSERT_TRUE(composition.success);
    Artifact::ArtifactLayerInitParams layerParams(
        QStringLiteral("MissingStillImage"), Artifact::LayerType::Image);
    const auto added = projects.addLayerToComposition(composition.id, layerParams);
    ASSERT_TRUE(added.success);
    auto* imageLayer = dynamic_cast<Artifact::ArtifactImageLayer*>(added.layer.get());
    ASSERT_NE(imageLayer, nullptr);

    const QString missingPath = dir.filePath(QStringLiteral("moved-away.png"));
    ASSERT_FALSE(imageLayer->loadFromPath(missingPath));
    const QString projectPath = dir.filePath(QStringLiteral("missing-source.artifact"));
    ASSERT_TRUE(projects.saveToFile(projectPath).success);
    projects.closeCurrentProject();
    ASSERT_TRUE(projects.loadFromFile(projectPath));

    const auto loadedComposition = projects.findComposition(composition.id).ptr.lock();
    ASSERT_TRUE(loadedComposition);
    const auto loadedLayerBase = loadedComposition->frontMostLayer();
    auto* loadedImageLayer =
        dynamic_cast<Artifact::ArtifactImageLayer*>(loadedLayerBase.get());
    ASSERT_NE(loadedImageLayer, nullptr);
    EXPECT_EQ(loadedImageLayer->sourcePath(), missingPath);
    EXPECT_TRUE(loadedImageLayer->hasCurrentFrameBuffer());
    EXPECT_EQ(loadedImageLayer->toQImage().size(), QSize(256, 256));

    QImage restoredSource(2, 1, QImage::Format_RGBA8888);
    const QColor restoredColor(45, 135, 205, 255);
    restoredSource.fill(restoredColor);
    ASSERT_TRUE(restoredSource.save(missingPath, "PNG"));
    ASSERT_TRUE(loadedImageLayer->loadFromPath(missingPath));
    ASSERT_TRUE(waitForImageBuffer(*loadedImageLayer, QSize(2, 1)));
    EXPECT_EQ(loadedImageLayer->sourcePath(), missingPath);
    EXPECT_EQ(waitForImage(*loadedImageLayer, QSize(2, 1)).pixelColor(1, 0),
              restoredColor);
}

TEST(StillImageLayerRoundTripTest, SourceIdentityCanBeLocalizedAndRelinked)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(2, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(65, 155, 225, 255));
    const QString imagePath = dir.filePath(QStringLiteral("shared-source.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    EXPECT_FALSE(layer.isSourceIdentityLocalized());
    EXPECT_FALSE(layer.relinkSourceIdentityToShared());

    EXPECT_TRUE(layer.localizeSourceIdentity());
    EXPECT_TRUE(layer.isSourceIdentityLocalized());
    EXPECT_FALSE(layer.localizeSourceIdentity());

    EXPECT_TRUE(layer.relinkSourceIdentityToShared());
    EXPECT_FALSE(layer.isSourceIdentityLocalized());
    EXPECT_FALSE(layer.relinkSourceIdentityToShared());
}

TEST(StillImageLayerRoundTripTest, EmptyLayerRejectsSourceIdentityOperations)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    Artifact::ArtifactImageLayer layer;
    EXPECT_FALSE(layer.isSourceIdentityLocalized());
    EXPECT_FALSE(layer.localizeSourceIdentity());
    EXPECT_FALSE(layer.relinkSourceIdentityToShared());
}

TEST(StillImageLayerRoundTripTest, CropPropertiesSanitizeNonFiniteAndDegenerateValues)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(4, 3, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(110, 70, 30, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"),
        QVariant(std::numeric_limits<double>::quiet_NaN())));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.rotation"),
        QVariant(std::numeric_limits<double>::infinity())));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(-10.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(0.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"), QVariant(1.5)));

    const Artifact::SourceCrop crop = layer.sourceCrop();
    EXPECT_DOUBLE_EQ(crop.zoom(), 1.0);
    EXPECT_DOUBLE_EQ(crop.rotation(), 0.0);
    EXPECT_DOUBLE_EQ(crop.cropRect().width(), 1.0);
    EXPECT_DOUBLE_EQ(crop.cropRect().height(), 1.0);
    EXPECT_DOUBLE_EQ(crop.anchor().x(), 1.0);
    EXPECT_DOUBLE_EQ(crop.anchor().y(), 0.5);
}

TEST(StillImageLayerRoundTripTest, CropPanClampsAndIgnoresNonFiniteUpdates)
{
    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panX"), QVariant(2000000.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panY"), QVariant(-2000000.0)));
    EXPECT_DOUBLE_EQ(layer.sourceCrop().pan().x(), 1000000.0);
    EXPECT_DOUBLE_EQ(layer.sourceCrop().pan().y(), -1000000.0);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panX"),
        QVariant(std::numeric_limits<double>::quiet_NaN())));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panY"),
        QVariant(std::numeric_limits<double>::infinity())));
    EXPECT_DOUBLE_EQ(layer.sourceCrop().pan().x(), 1000000.0);
    EXPECT_DOUBLE_EQ(layer.sourceCrop().pan().y(), -1000000.0);
}

TEST(StillImageLayerRoundTripTest,
     CropAnchorIgnoresNonFiniteValuesAndClampsFiniteValues)
{
    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"), QVariant(0.25)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorY"), QVariant(0.75)));

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"),
        QVariant(std::numeric_limits<double>::quiet_NaN())));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorY"),
        QVariant(std::numeric_limits<double>::infinity())));
    EXPECT_DOUBLE_EQ(layer.sourceCrop().anchor().x(), 0.25);
    EXPECT_DOUBLE_EQ(layer.sourceCrop().anchor().y(), 0.75);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"), QVariant(-500.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorY"), QVariant(500.0)));
    EXPECT_DOUBLE_EQ(layer.sourceCrop().anchor().x(), 0.0);
    EXPECT_DOUBLE_EQ(layer.sourceCrop().anchor().y(), 1.0);
}

TEST(StillImageLayerRoundTripTest, CropRectangleClampsToSourceEdges)
{
    QImage sourceImage(4, 3, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(105, 65, 25, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(3.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropY"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(2.0)));

    const QRectF rect = layer.sourceCrop().cropRect();
    EXPECT_DOUBLE_EQ(rect.x(), 2.0);
    EXPECT_DOUBLE_EQ(rect.y(), 1.0);
    EXPECT_DOUBLE_EQ(rect.width(), 2.0);
    EXPECT_DOUBLE_EQ(rect.height(), 2.0);
}

TEST(StillImageLayerRoundTripTest, RestoreSourceCropSnapshotRestoresAllFields)
{
    QImage sourceImage(4, 3, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(120, 80, 40, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(0.5)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropY"), QVariant(0.25)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(1.5)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.panX"), QVariant(0.1)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(1.75)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.rotation"), QVariant(33.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"), QVariant(0.2)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.preserveAspect"), QVariant(false)));

    const QJsonObject snapshot = layer.sourceCrop().toJson();
    const QString expectedSignature = layer.sourceCropSignature();
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.rotation"), QVariant(-80.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(0.5)));
    ASSERT_NE(layer.sourceCropSignature(), expectedSignature);

    EXPECT_TRUE(layer.restoreSourceCropSnapshot(snapshot));
    EXPECT_EQ(layer.sourceCropSignature(), expectedSignature);
}

TEST(StillImageLayerRoundTripTest,
     InvalidSourceCropSnapshotIsRejectedWithoutChangingState)
{
    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(2.0)));
    const QString previousSignature = layer.sourceCropSignature();

    QJsonObject invalidSnapshot = layer.sourceCrop().toJson();
    invalidSnapshot[QStringLiteral("zoom")] = 0.0;
    EXPECT_FALSE(layer.restoreSourceCropSnapshot(invalidSnapshot));
    EXPECT_EQ(layer.sourceCropSignature(), previousSignature);
}

TEST(StillImageLayerRoundTripTest,
     SourceCropRevisionChangesOnlyWhenTheValueChanges)
{
    Artifact::ArtifactImageLayer layer;
    const std::uint64_t initialRevision = layer.sourceCropRevision();

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(2.0)));
    const std::uint64_t changedRevision = layer.sourceCropRevision();
    EXPECT_GT(changedRevision, initialRevision);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.zoom"), QVariant(2.0)));
    EXPECT_EQ(layer.sourceCropRevision(), changedRevision);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));
    EXPECT_GT(layer.sourceCropRevision(), changedRevision);
}

TEST(StillImageLayerRoundTripTest,
     SourceCropResetRestoresDefaultsAndIsIdempotent)
{
    Artifact::SourceCrop crop;
    crop.setEnabled(true);
    crop.setCropRect(QRectF(0.25, 0.5, 2.0, 1.5));
    crop.setPan(QPointF(0.1, -0.2));
    crop.setZoom(2.0);
    crop.setRotation(35.0);
    crop.setAnchor(QPointF(0.25, 0.75));
    crop.setPreserveAspect(false);
    const auto configuredRevision = crop.revision();

    crop.reset();
    const auto resetRevision = crop.revision();
    EXPECT_GT(resetRevision, configuredRevision);
    EXPECT_FALSE(crop.enabled());
    EXPECT_TRUE(crop.cropRect().isNull());
    EXPECT_EQ(crop.pan(), QPointF(0.0, 0.0));
    EXPECT_DOUBLE_EQ(crop.zoom(), 1.0);
    EXPECT_DOUBLE_EQ(crop.rotation(), 0.0);
    EXPECT_EQ(crop.anchor(), QPointF(0.5, 0.5));
    EXPECT_TRUE(crop.preserveAspect());

    crop.reset();
    EXPECT_EQ(crop.revision(), resetRevision);
}

TEST(StillImageLayerRoundTripTest,
     SourceCropClampWithInvalidSourceSizePreservesCropGeometry)
{
    Artifact::SourceCrop crop;
    const QRectF original(0.25, 0.5, 1.5, 1.25);
    crop.setCropRect(original);
    const auto revision = crop.revision();

    crop.clampToSource(QSizeF(0.0, 10.0));

    EXPECT_EQ(crop.cropRect(), original);
    EXPECT_EQ(crop.revision(), revision);
    EXPECT_TRUE(crop.effectiveCropRect(QSizeF(0.0, 10.0)).isEmpty());
}

TEST(StillImageLayerRoundTripTest,
     SourceCropClampMovesPartialCropInsideValidSourceAndIsIdempotent)
{
    Artifact::SourceCrop crop;
    crop.setCropRect(QRectF(-2.0, 1.0, 3.0, 2.0));
    const auto configuredRevision = crop.revision();

    crop.clampToSource(QSizeF(4.0, 4.0));

    EXPECT_EQ(crop.cropRect(), QRectF(0.0, 1.0, 3.0, 2.0));
    EXPECT_GT(crop.revision(), configuredRevision);
    const auto clampedRevision = crop.revision();

    crop.clampToSource(QSizeF(4.0, 4.0));

    EXPECT_EQ(crop.cropRect(), QRectF(0.0, 1.0, 3.0, 2.0));
    EXPECT_EQ(crop.revision(), clampedRevision);
}

TEST(StillImageLayerRoundTripTest,
     SourceCropFromEmptyJsonRestoresAllDefaults)
{
    Artifact::SourceCrop crop;
    crop.setEnabled(true);
    crop.setCropRect(QRectF(0.25, 0.5, 2.0, 1.5));
    crop.setPan(QPointF(0.2, -0.3));
    crop.setZoom(2.5);
    crop.setRotation(45.0);
    crop.setAnchor(QPointF(0.2, 0.8));
    crop.setPreserveAspect(false);
    const auto configuredRevision = crop.revision();

    crop.fromJson(QJsonObject{});

    EXPECT_FALSE(crop.enabled());
    EXPECT_TRUE(crop.cropRect().isNull());
    EXPECT_EQ(crop.pan(), QPointF(0.0, 0.0));
    EXPECT_DOUBLE_EQ(crop.zoom(), 1.0);
    EXPECT_DOUBLE_EQ(crop.rotation(), 0.0);
    EXPECT_EQ(crop.anchor(), QPointF(0.5, 0.5));
    EXPECT_TRUE(crop.preserveAspect());
    EXPECT_GT(crop.revision(), configuredRevision);
}

TEST(StillImageLayerRoundTripTest,
     SourceCropFromJsonNormalizesRectAndFallsBackForMalformedFields)
{
    Artifact::SourceCrop crop;
    QJsonObject state;
    state[QStringLiteral("enabled")] = true;
    state[QStringLiteral("cropRect")] = QJsonArray{
        3.0, 2.0, -2.0, -1.0};
    state[QStringLiteral("pan")] = QJsonArray{
        QStringLiteral("invalid"), 0.25};
    state[QStringLiteral("zoom")] = -4.0;
    state[QStringLiteral("rotation")] = 90.0;
    state[QStringLiteral("anchor")] = QJsonArray{0.25};
    state[QStringLiteral("preserveAspect")] = false;

    crop.fromJson(state);

    EXPECT_TRUE(crop.enabled());
    EXPECT_EQ(crop.cropRect(), QRectF(1.0, 1.0, 2.0, 1.0));
    EXPECT_EQ(crop.pan(), QPointF(0.0, 0.0));
    EXPECT_DOUBLE_EQ(crop.zoom(), 1.0);
    EXPECT_DOUBLE_EQ(crop.rotation(), 90.0);
    EXPECT_EQ(crop.anchor(), QPointF(0.5, 0.5));
    EXPECT_FALSE(crop.preserveAspect());
}

TEST(StillImageLayerRoundTripTest,
     SourceCropTransformIsIdentityWhenDisabled)
{
    Artifact::SourceCrop crop;
    crop.setCropRect(QRectF(2.0, 1.0, 4.0, 2.0));
    crop.setRotation(45.0);

    const QTransform transform = crop.sourceToOutputTransform(
        QSizeF(10.0, 8.0), QSizeF(8.0, 8.0));
    EXPECT_TRUE(transform.isIdentity());
}

TEST(StillImageLayerRoundTripTest,
     SourceCropTransformIsIdentityForInvalidInputSizes)
{
    Artifact::SourceCrop crop;
    crop.setEnabled(true);
    crop.setCropRect(QRectF(2.0, 1.0, 4.0, 2.0));

    EXPECT_TRUE(crop.sourceToOutputTransform(
        QSizeF(0.0, 8.0), QSizeF(8.0, 8.0)).isIdentity());
    EXPECT_TRUE(crop.sourceToOutputTransform(
        QSizeF(10.0, 8.0), QSizeF(8.0, 0.0)).isIdentity());
}

TEST(StillImageLayerRoundTripTest,
     SourceCropTransformPreservesAspectAndCentersCrop)
{
    Artifact::SourceCrop crop;
    crop.setEnabled(true);
    crop.setCropRect(QRectF(2.0, 1.0, 4.0, 2.0));
    crop.setPreserveAspect(true);

    const QTransform transform = crop.sourceToOutputTransform(
        QSizeF(10.0, 8.0), QSizeF(8.0, 8.0));
    const QPointF topLeft = transform.map(QPointF(2.0, 1.0));
    const QPointF center = transform.map(QPointF(4.0, 2.0));
    const QPointF bottomRight = transform.map(QPointF(6.0, 3.0));
    EXPECT_NEAR(topLeft.x(), 0.0, 1e-9);
    EXPECT_NEAR(topLeft.y(), 2.0, 1e-9);
    EXPECT_NEAR(center.x(), 4.0, 1e-9);
    EXPECT_NEAR(center.y(), 4.0, 1e-9);
    EXPECT_NEAR(bottomRight.x(), 8.0, 1e-9);
    EXPECT_NEAR(bottomRight.y(), 6.0, 1e-9);
}

TEST(StillImageLayerRoundTripTest,
     SourceCropTransformStretchesCropWhenAspectPreservationIsDisabled)
{
    Artifact::SourceCrop crop;
    crop.setEnabled(true);
    crop.setCropRect(QRectF(2.0, 1.0, 4.0, 2.0));
    crop.setPreserveAspect(false);

    const QTransform transform = crop.sourceToOutputTransform(
        QSizeF(10.0, 8.0), QSizeF(8.0, 8.0));
    const QPointF topLeft = transform.map(QPointF(2.0, 1.0));
    const QPointF bottomRight = transform.map(QPointF(6.0, 3.0));
    EXPECT_NEAR(topLeft.x(), 0.0, 1e-9);
    EXPECT_NEAR(topLeft.y(), 0.0, 1e-9);
    EXPECT_NEAR(bottomRight.x(), 8.0, 1e-9);
    EXPECT_NEAR(bottomRight.y(), 8.0, 1e-9);
}

TEST(StillImageLayerRoundTripTest, SourceCropRotationKeepsAnchorPivotFixed)
{
    QImage sourceImage(4, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(125, 85, 45, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.rotation"), QVariant(90.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorX"), QVariant(0.25)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.anchorY"), QVariant(0.75)));

    const auto rotatedLayout = layer.sourceCropDrawLayout();
    EXPECT_DOUBLE_EQ(rotatedLayout.outputLocalRect.width(), 4.0);
    EXPECT_DOUBLE_EQ(rotatedLayout.outputLocalRect.height(), 2.0);
    EXPECT_FALSE(rotatedLayout.localTransform.isIdentity());

    const float pivotX = static_cast<float>(
        rotatedLayout.outputLocalRect.x() +
        rotatedLayout.outputLocalRect.width() * 0.25);
    const float pivotY = static_cast<float>(
        rotatedLayout.outputLocalRect.y() +
        rotatedLayout.outputLocalRect.height() * 0.75);
    const QVector4D transformedPivot = rotatedLayout.localTransform *
        QVector4D(pivotX, pivotY, 0.0f, 1.0f);
    EXPECT_NEAR(transformedPivot.x(), pivotX, 1e-5f);
    EXPECT_NEAR(transformedPivot.y(), pivotY, 1e-5f);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.rotation"), QVariant(0.0)));
    EXPECT_TRUE(layer.sourceCropDrawLayout().localTransform.isIdentity());
}

TEST(StillImageLayerRoundTripTest,
     SourceCropLayoutPreservesAspectInsideLayerFrame)
{
    QImage sourceImage(4, 2, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(115, 75, 35, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropWidth"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropHeight"), QVariant(2.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.cropX"), QVariant(1.0)));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));

    const auto preservedLayout = layer.sourceCropDrawLayout();
    EXPECT_TRUE(preservedLayout.cropped);
    EXPECT_EQ(preservedLayout.sourcePixelRect, QRect(1, 0, 2, 2));
    EXPECT_DOUBLE_EQ(preservedLayout.outputLocalRect.x(), 1.0);
    EXPECT_DOUBLE_EQ(preservedLayout.outputLocalRect.y(), 0.0);
    EXPECT_DOUBLE_EQ(preservedLayout.outputLocalRect.width(), 2.0);
    EXPECT_DOUBLE_EQ(preservedLayout.outputLocalRect.height(), 2.0);

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.preserveAspect"), QVariant(false)));
    const auto stretchedLayout = layer.sourceCropDrawLayout();
    EXPECT_TRUE(stretchedLayout.cropped);
    EXPECT_EQ(stretchedLayout.sourcePixelRect, QRect(1, 0, 2, 2));
    EXPECT_DOUBLE_EQ(stretchedLayout.outputLocalRect.width(), 4.0);
    EXPECT_DOUBLE_EQ(stretchedLayout.outputLocalRect.height(), 2.0);
}

TEST(StillImageLayerRoundTripTest,
     PropertySetterRejectsUnknownReadOnlyAndInvalidValues)
{
    Artifact::ArtifactImageLayer layer;
    const QString cropSignature = layer.sourceCropSignature();

    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("source.localized"), QVariant(true)));
    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("source.sharedUseCount"), QVariant(4)));
    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("image.unknownProperty"), QVariant(1)));
    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("image.sequenceFrameRate"), QVariant(-1.0)));
    EXPECT_FALSE(layer.setLayerPropertyValue(
        QStringLiteral("image.sequenceFrameRate"),
        QVariant(std::numeric_limits<double>::quiet_NaN())));

    EXPECT_DOUBLE_EQ(layer.sequenceFrameRate(), 0.0);
    EXPECT_EQ(layer.sourceCropSignature(), cropSignature);
}

TEST(StillImageLayerRoundTripTest,
     SharedSourceTextureEligibilityTracksCropAndSequenceState)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage image(2, 1, QImage::Format_RGBA8888);
    image.fill(QColor(65, 105, 145, 255));
    const QString firstPath = dir.filePath(QStringLiteral("share-0001.png"));
    const QString secondPath = dir.filePath(QStringLiteral("share-0002.png"));
    ASSERT_TRUE(image.save(firstPath, "PNG"));
    ASSERT_TRUE(image.save(secondPath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    EXPECT_FALSE(layer.canShareSourceGpuTexture());
    ASSERT_TRUE(layer.loadFromPath(firstPath));
    EXPECT_TRUE(layer.canShareSourceGpuTexture());

    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(true)));
    EXPECT_FALSE(layer.canShareSourceGpuTexture());
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("sourceCrop.enabled"), QVariant(false)));
    EXPECT_TRUE(layer.canShareSourceGpuTexture());

    ASSERT_TRUE(layer.setImageSequence(QStringList{firstPath, secondPath}, 24.0));
    EXPECT_FALSE(layer.canShareSourceGpuTexture());
}

TEST(StillImageLayerRoundTripTest,
     SharedSourceTextureEligibilityRejectsLayerMasks)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage image(2, 1, QImage::Format_RGBA8888);
    image.fill(QColor(65, 105, 145, 255));
    const QString imagePath = dir.filePath(QStringLiteral("masked-share.png"));
    ASSERT_TRUE(image.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    EXPECT_TRUE(layer.canShareSourceGpuTexture());

    const Artifact::LayerMask emptyMask;
    layer.addMask(emptyMask);
    EXPECT_TRUE(layer.hasMasks());
    EXPECT_FALSE(layer.canShareSourceGpuTexture());

    layer.removeMask(0);
    EXPECT_FALSE(layer.hasMasks());
    EXPECT_TRUE(layer.canShareSourceGpuTexture());
}

TEST(StillImageLayerRoundTripTest,
     SharedSourceTextureEligibilityRejectsEffectsAndRecoversAfterClear)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);
    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(QColor(55, 95, 135, 255));
    const QString path = dir.filePath(QStringLiteral("effect-share.png"));
    ASSERT_TRUE(image.save(path, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(path));
    ASSERT_TRUE(layer.canShareSourceGpuTexture());
    const auto effect = ArtifactCore::makeShared<Artifact::BrightnessEffect>();
    layer.addEffect(effect);
    ASSERT_EQ(layer.effectCount(), 1);
    EXPECT_FALSE(layer.canShareSourceGpuTexture());

    layer.clearEffects();
    EXPECT_EQ(layer.effectCount(), 0);
    EXPECT_TRUE(layer.canShareSourceGpuTexture());
}

TEST(StillImageLayerRoundTripTest, SixteenBitGrayscaleKeepsSubEightBitStepsAndOpaqueAlpha)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(3, 1, QImage::Format_Grayscale16);
    auto* samples = reinterpret_cast<std::uint16_t*>(sourceImage.scanLine(0));
    samples[0] = 0x0800;
    samples[1] = 0x0801;
    samples[2] = 0x8000;
    const QString imagePath = dir.filePath(QStringLiteral("gray16.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    ASSERT_TRUE(waitForImageBuffer(layer, QSize(3, 1)));

    const auto& buffer = layer.currentFrameBuffer();
    ASSERT_EQ(buffer.width(), 3);
    ASSERT_EQ(buffer.height(), 1);
    const float* rgba = buffer.rgba32fData();
    ASSERT_NE(rgba, nullptr);
    for (int x = 0; x < 3; ++x) {
        const std::size_t offset = static_cast<std::size_t>(x) * 4u;
        EXPECT_NEAR(rgba[offset], rgba[offset + 1u], 1e-6f);
        EXPECT_NEAR(rgba[offset], rgba[offset + 2u], 1e-6f);
        EXPECT_NEAR(rgba[offset + 3u], 1.0f, 1e-6f);
        EXPECT_TRUE(std::isfinite(rgba[offset]));
    }
    EXPECT_GT(rgba[4u], rgba[0u]);
    EXPECT_GT(rgba[4u] - rgba[0u], 1e-7f)
        << "Adjacent 16-bit grayscale samples should not collapse to one 8-bit value";
    EXPECT_GT(rgba[8u], rgba[4u]);
}

TEST(StillImageLayerRoundTripTest, TwoChannelGrayAlphaPngExpandsAndPreservesAlpha)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    // PNG color type 4 stores one grayscale sample followed by alpha.
    const QByteArray pngBytes = QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAQAAABeK7cBAAAADUlEQVR4nGMIcDhxAAAEVQIZTuh/jwAAAABJRU5ErkJggg=="));
    ASSERT_EQ(pngBytes.size(), 70);
    const QString imagePath = dir.filePath(QStringLiteral("gray-alpha.png"));
    QFile imageFile(imagePath);
    ASSERT_TRUE(imageFile.open(QIODevice::WriteOnly));
    ASSERT_EQ(imageFile.write(pngBytes), pngBytes.size());
    imageFile.close();

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    const QImage decoded = waitForImage(layer, QSize(2, 1));
    ASSERT_EQ(decoded.size(), QSize(2, 1));

    const QColor expected[2] = {QColor(80, 80, 80, 64),
                                QColor(200, 200, 200, 192)};
    for (int x = 0; x < 2; ++x) {
        const QColor actual = decoded.pixelColor(x, 0);
        EXPECT_NEAR(actual.red(), expected[x].red(), 2);
        EXPECT_NEAR(actual.green(), expected[x].green(), 2);
        EXPECT_NEAR(actual.blue(), expected[x].blue(), 2);
        EXPECT_NEAR(actual.alpha(), expected[x].alpha(), 1);
    }

    ASSERT_TRUE(waitForImageBuffer(layer, QSize(2, 1)));
    const float* rgba = layer.currentFrameBuffer().rgba32fData();
    ASSERT_NE(rgba, nullptr);
    for (int x = 0; x < 2; ++x) {
        const std::size_t offset = static_cast<std::size_t>(x) * 4u;
        EXPECT_NEAR(rgba[offset], rgba[offset + 1u], 1e-6f);
        EXPECT_NEAR(rgba[offset], rgba[offset + 2u], 1e-6f);
    }
    EXPECT_NEAR(rgba[3u], 64.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(rgba[7u], 192.0f / 255.0f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest, ThreeChannelRgbPngDefaultsAlphaToOpaque)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    QImage sourceImage(2, 1, QImage::Format_RGB888);
    sourceImage.setPixelColor(0, 0, QColor(23, 117, 208));
    sourceImage.setPixelColor(1, 0, QColor(201, 44, 73));
    const QString imagePath = dir.filePath(QStringLiteral("rgb-only.png"));
    ASSERT_TRUE(sourceImage.save(imagePath, "PNG"));

    Artifact::ArtifactImageLayer layer;
    ASSERT_TRUE(layer.loadFromPath(imagePath));
    ASSERT_TRUE(waitForImageBuffer(layer, QSize(2, 1)));

    const QImage decoded = layer.toQImage();
    ASSERT_EQ(decoded.size(), QSize(2, 1));
    const QColor first = decoded.pixelColor(0, 0);
    const QColor second = decoded.pixelColor(1, 0);
    EXPECT_NEAR(first.red(), 23, 2);
    EXPECT_NEAR(first.green(), 117, 2);
    EXPECT_NEAR(first.blue(), 208, 2);
    EXPECT_EQ(first.alpha(), 255);
    EXPECT_NEAR(second.red(), 201, 2);
    EXPECT_NEAR(second.green(), 44, 2);
    EXPECT_NEAR(second.blue(), 73, 2);
    EXPECT_EQ(second.alpha(), 255);

    const float* rgba = layer.currentFrameBuffer().rgba32fData();
    ASSERT_NE(rgba, nullptr);
    EXPECT_FLOAT_EQ(rgba[3], 1.0f);
    EXPECT_FLOAT_EQ(rgba[7], 1.0f);
}

TEST(StillImageLayerRoundTripTest, DepthMapBuildsMeshAndClearRestores2DState)
{
    Artifact::ArtifactImageLayer layer;
    EXPECT_FALSE(layer.hasDepthMap());
    EXPECT_FALSE(layer.is3D());

    ArtifactCore::DepthMeshOptions options;
    options.columns = 3;
    options.rows = 2;
    options.width = 4.0f;
    options.height = 2.0f;
    options.depthScale = 0.5f;
    layer.setDepthMeshOptions(options);

    ArtifactCore::DepthMap depthMap;
    depthMap.resize(2, 2);
    depthMap.setValue(0, 0, 0.0f);
    depthMap.setValue(1, 0, 0.25f);
    depthMap.setValue(0, 1, 0.75f);
    depthMap.setValue(1, 1, 1.0f);
    layer.setDepthMap(depthMap);

    EXPECT_TRUE(layer.hasDepthMap());
    EXPECT_TRUE(layer.is3D());
    EXPECT_EQ(layer.depthMesh().vertexCount(), 6);
    EXPECT_TRUE(layer.depthMapPath().isEmpty());

    layer.clearDepthMap();
    EXPECT_FALSE(layer.hasDepthMap());
    EXPECT_FALSE(layer.is3D());
    EXPECT_EQ(layer.depthMesh().vertexCount(), 0);
    EXPECT_TRUE(layer.depthMapPath().isEmpty());
}

TEST(StillImageLayerRoundTripTest,
     DepthMeshRegenerationUsesLayerOwnedDepthMapCopy)
{
    ArtifactCore::DepthMap callerMap;
    callerMap.resize(2, 2);
    callerMap.setValue(0, 0, 0.0f);
    callerMap.setValue(1, 0, 0.25f);
    callerMap.setValue(0, 1, 0.75f);
    callerMap.setValue(1, 1, 1.0f);

    Artifact::ArtifactImageLayer layer;
    layer.setDepthMap(callerMap);
    ASSERT_TRUE(layer.hasDepthMap());

    callerMap.resize(2, 2, 0.0f);
    ArtifactCore::DepthMeshOptions options;
    options.columns = 2;
    options.rows = 2;
    options.depthScale = 2.0f;
    layer.setDepthMeshOptions(options);

    const auto positions = layer.depthMesh().vertexAttributes()
        .get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    ASSERT_EQ(positions->size(), 4);
    EXPECT_FLOAT_EQ((*positions)[0].z(), 0.0f);
    EXPECT_FLOAT_EQ((*positions)[1].z(), 0.5f);
    EXPECT_FLOAT_EQ((*positions)[2].z(), 1.5f);
    EXPECT_FLOAT_EQ((*positions)[3].z(), 2.0f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMeshGeneratorAppliesDimensionsAndInvertedDepth)
{
    ArtifactCore::DepthMap map;
    map.resize(1, 1, 0.5f);
    ArtifactCore::DepthMeshOptions options;
    options.columns = 2;
    options.rows = 2;
    options.width = 4.0f;
    options.height = 2.0f;
    options.depthScale = 3.0f;
    options.invertDepth = true;

    const ArtifactCore::Mesh mesh =
        ArtifactCore::DepthMeshGenerator::generate(map, options);
    const auto positions = mesh.vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    ASSERT_EQ(positions->size(), 4);
    EXPECT_FLOAT_EQ((*positions)[0].x(), -2.0f);
    EXPECT_FLOAT_EQ((*positions)[0].y(), -1.0f);
    EXPECT_FLOAT_EQ((*positions)[0].z(), -1.5f);
    EXPECT_FLOAT_EQ((*positions)[1].x(), 2.0f);
    EXPECT_FLOAT_EQ((*positions)[1].y(), -1.0f);
    EXPECT_FLOAT_EQ((*positions)[2].x(), -2.0f);
    EXPECT_FLOAT_EQ((*positions)[2].y(), 1.0f);
    EXPECT_FLOAT_EQ((*positions)[3].x(), 2.0f);
    EXPECT_FLOAT_EQ((*positions)[3].y(), 1.0f);
    EXPECT_FLOAT_EQ((*positions)[3].z(), -1.5f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMeshGeneratorEnforcesMinimumGridResolution)
{
    ArtifactCore::DepthMap map;
    map.resize(1, 1, 0.5f);
    ArtifactCore::DepthMeshOptions options;
    options.columns = -4;
    options.rows = 0;

    const ArtifactCore::Mesh mesh =
        ArtifactCore::DepthMeshGenerator::generate(map, options);
    EXPECT_EQ(mesh.vertexCount(), 4);
    EXPECT_EQ(mesh.polygonCount(), 1);
}

TEST(StillImageLayerRoundTripTest,
     DepthMeshGeneratorSamplesSmallMapAcrossDenseGridAndBuildsQuads)
{
    ArtifactCore::DepthMap map;
    map.resize(2, 2);
    map.setValue(0, 0, 0.0f);
    map.setValue(1, 0, 0.25f);
    map.setValue(0, 1, 0.75f);
    map.setValue(1, 1, 1.0f);
    ArtifactCore::DepthMeshOptions options;
    options.columns = 3;
    options.rows = 3;
    options.depthScale = 2.0f;

    const auto mesh = ArtifactCore::DepthMeshGenerator::generate(map, options);
    const auto positions = mesh.vertexAttributes().get<QVector3D>("position");
    ASSERT_NE(positions, nullptr);
    ASSERT_EQ(positions->size(), 9);
    EXPECT_EQ(mesh.polygonCount(), 4);
    EXPECT_FLOAT_EQ((*positions)[0].z(), 0.0f);
    EXPECT_FLOAT_EQ((*positions)[2].z(), 0.5f);
    EXPECT_FLOAT_EQ((*positions)[4].z(), 0.0f);
    EXPECT_FLOAT_EQ((*positions)[6].z(), 1.5f);
    EXPECT_FLOAT_EQ((*positions)[8].z(), 2.0f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMeshGeneratorReturnsEmptyMeshForEmptyDepthMap)
{
    const ArtifactCore::DepthMap emptyMap;
    ArtifactCore::DepthMeshOptions options;
    options.columns = 4;
    options.rows = 3;

    const auto mesh = ArtifactCore::DepthMeshGenerator::generate(
        emptyMap, options);
    EXPECT_EQ(mesh.vertexCount(), 0);
    EXPECT_EQ(mesh.polygonCount(), 0);
    EXPECT_EQ(mesh.vertexAttributes().get<QVector3D>("position"), nullptr);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferHorizontalFlipReversesEachRow)
{
    const float pixels[] = {
        1.0f, 0.0f, 0.0f, 0.1f,
        0.0f, 1.0f, 0.0f, 0.2f,
        0.0f, 0.0f, 1.0f, 0.3f,
        1.0f, 1.0f, 0.0f, 0.4f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 2);

    image.flipHorizontal();
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).g(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 1).r(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 1).b(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).a(), 0.2f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 1).a(), 0.4f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferVerticalFlipReversesRows)
{
    const float pixels[] = {
        1.0f, 0.0f, 0.0f, 0.1f,
        0.0f, 1.0f, 0.0f, 0.2f,
        0.0f, 0.0f, 1.0f, 0.3f,
        1.0f, 1.0f, 0.0f, 0.4f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 2);

    image.flipVertical();
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).b(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 0).r(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 1).r(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 1).g(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).a(), 0.3f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 1).a(), 0.2f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferCropReturnsDetachedPixelRegion)
{
    const float pixels[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        0.1f, 0.0f, 0.0f, 1.0f,
        0.2f, 0.0f, 0.0f, 1.0f,
        0.3f, 0.0f, 0.0f, 1.0f,
        0.4f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 3, 2);

    auto crop = image.crop(1, 0, 2, 2);
    ASSERT_EQ(crop.width(), 2);
    ASSERT_EQ(crop.height(), 2);
    EXPECT_FLOAT_EQ(crop.getPixel(0, 0).r(), 0.1f);
    EXPECT_FLOAT_EQ(crop.getPixel(1, 0).r(), 0.2f);
    EXPECT_FLOAT_EQ(crop.getPixel(0, 1).r(), 0.4f);
    EXPECT_FLOAT_EQ(crop.getPixel(1, 1).r(), 0.5f);
    crop.setPixel(0, 0, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));
    EXPECT_FLOAT_EQ(image.getPixel(1, 0).r(), 0.1f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferCropRejectsRegionPastImageEdges)
{
    const float pixels[] = {0.1f, 0.2f, 0.3f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1);

    const auto crop = image.crop(1, 0, 1, 1);
    EXPECT_TRUE(crop.isEmpty());
    EXPECT_EQ(crop.width(), 0);
    EXPECT_EQ(crop.height(), 0);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferBlendInterpolatesAllChannelsByWeight)
{
    const float firstPixels[] = {0.0f, 0.2f, 0.4f, 0.6f};
    const float secondPixels[] = {1.0f, 0.8f, 0.6f, 0.4f};
    ArtifactCore::ImageF32x4_RGBA first;
    first.setFromRGBA32F(firstPixels, 1, 1);
    ArtifactCore::ImageF32x4_RGBA second;
    second.setFromRGBA32F(secondPixels, 1, 1);

    const auto blended = first.blend(second, 0.25f);
    EXPECT_NEAR(blended.getPixel(0, 0).r(), 0.25f, 1e-6f);
    EXPECT_NEAR(blended.getPixel(0, 0).g(), 0.35f, 1e-6f);
    EXPECT_NEAR(blended.getPixel(0, 0).b(), 0.45f, 1e-6f);
    EXPECT_NEAR(blended.getPixel(0, 0).a(), 0.55f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferDeepCopyOwnsPixelsIndependently)
{
    const float pixels[] = {0.15f, 0.35f, 0.55f, 0.75f};
    ArtifactCore::ImageF32x4_RGBA original;
    original.setFromRGBA32F(pixels, 1, 1);

    auto copy = original.DeepCopy();
    copy.setPixel(0, 0, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));
    EXPECT_FLOAT_EQ(original.getPixel(0, 0).r(), 0.15f);
    EXPECT_FLOAT_EQ(original.getPixel(0, 0).a(), 0.75f);
    EXPECT_FLOAT_EQ(copy.getPixel(0, 0).r(), 0.9f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferFillAlphaChangesOnlyAlphaChannel)
{
    const float pixels[] = {
        0.1f, 0.3f, 0.5f, 0.2f,
        0.7f, 0.4f, 0.2f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1);

    image.fillAlpha(0.6f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).r(), 0.1f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).g(), 0.3f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).b(), 0.5f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).a(), 0.6f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 0).r(), 0.7f);
    EXPECT_FLOAT_EQ(image.getPixel(1, 0).a(), 0.6f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferCreateMaskLikeUsesSourceDimensionsAndFillColor)
{
    ArtifactCore::ImageF32x4_RGBA source;
    source.resize(3, 2);
    const FloatRGBA fillColor(0.2f, 0.4f, 0.6f, 0.8f);

    const auto mask = ArtifactCore::ImageF32x4_RGBA::createMaskLike(
        source, fillColor);
    ASSERT_EQ(mask.width(), 3);
    ASSERT_EQ(mask.height(), 2);
    const auto pixel = mask.getPixel(2, 1);
    EXPECT_FLOAT_EQ(pixel.r(), 0.2f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.6f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.8f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferMoveConstructionLeavesSourceReusable)
{
    const float pixels[] = {0.2f, 0.4f, 0.6f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 1, 1);

    ArtifactCore::ImageF32x4_RGBA moved(std::move(source));
    EXPECT_EQ(moved.width(), 1);
    EXPECT_FLOAT_EQ(moved.getPixel(0, 0).g(), 0.4f);
    EXPECT_TRUE(source.isEmpty());
    source.setFromRGBA32F(pixels, 1, 1);
    EXPECT_EQ(source.width(), 1);
    EXPECT_FLOAT_EQ(source.getPixel(0, 0).b(), 0.6f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferMoveAssignmentReleasesOldValueAndLeavesSourceReusable)
{
    const float firstPixels[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const float secondPixels[] = {0.7f, 0.6f, 0.5f, 0.4f};
    ArtifactCore::ImageF32x4_RGBA destination;
    destination.setFromRGBA32F(firstPixels, 1, 1);
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(secondPixels, 1, 1);

    destination = std::move(source);
    EXPECT_FLOAT_EQ(destination.getPixel(0, 0).r(), 0.7f);
    EXPECT_TRUE(source.isEmpty());
    source.setFromRGBA32F(firstPixels, 1, 1);
    EXPECT_FLOAT_EQ(source.getPixel(0, 0).g(), 0.2f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferCanonicalConversionsNormalizeBgraBacking)
{
    const float bgraPixels[] = {0.25f, 0.5f, 0.75f, 0.125f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(
        bgraPixels, 1, 1,
        ArtifactCore::SurfaceColorDescriptor::legacyOpenCvBgra32Float());

    const cv::Mat canonicalRgba = image.toCanonicalRGBA32FC4();
    const cv::Mat canonicalBgra = image.toCanonicalBGRA32FC4();
    ASSERT_EQ(canonicalRgba.type(), CV_32FC4);
    ASSERT_EQ(canonicalBgra.type(), CV_32FC4);
    const cv::Vec4f rgba = canonicalRgba.at<cv::Vec4f>(0, 0);
    const cv::Vec4f bgra = canonicalBgra.at<cv::Vec4f>(0, 0);
    EXPECT_FLOAT_EQ(rgba[0], 0.75f);
    EXPECT_FLOAT_EQ(rgba[1], 0.5f);
    EXPECT_FLOAT_EQ(rgba[2], 0.25f);
    EXPECT_FLOAT_EQ(rgba[3], 0.125f);
    EXPECT_FLOAT_EQ(bgra[0], 0.25f);
    EXPECT_FLOAT_EQ(bgra[1], 0.5f);
    EXPECT_FLOAT_EQ(bgra[2], 0.75f);
    EXPECT_FLOAT_EQ(bgra[3], 0.125f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferSurfaceViewReportsFloatLayoutAndColorDescriptor)
{
    const float pixels[] = {
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1);

    const auto surface = image.surfaceView();
    EXPECT_TRUE(surface.isValid());
    EXPECT_EQ(surface.width, 2);
    EXPECT_EQ(surface.height, 1);
    EXPECT_EQ(surface.rowStride, 2u * 4u * sizeof(float));
    EXPECT_EQ(surface.precision, ArtifactCore::SurfacePrecision::Float32);
    EXPECT_EQ(surface.descriptor.storage,
              ArtifactCore::SurfacePixelStorage::RGBA32Float);
    EXPECT_EQ(surface.descriptor.channelOrder,
              ArtifactCore::SurfaceChannelOrder::RGBA);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferEmptySurfaceViewIsInvalid)
{
    const ArtifactCore::ImageF32x4_RGBA image;
    const auto surface = image.surfaceView();

    EXPECT_FALSE(surface.isValid());
    EXPECT_EQ(surface.width, 0);
    EXPECT_EQ(surface.height, 0);
    EXPECT_EQ(surface.rowStride, 0u);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferRgba8InputNormalizesAndDetachesCallerMemory)
{
    std::uint8_t pixels[] = {255, 128, 0, 64};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA8(pixels, 1, 1);
    pixels[0] = 0;
    pixels[1] = 0;
    pixels[3] = 0;

    const auto pixel = image.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 1.0f);
    EXPECT_NEAR(pixel.g(), 128.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.0f);
    EXPECT_NEAR(pixel.a(), 64.0f / 255.0f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferInvalidRgba32FInputClearsPixelsAndDescriptor)
{
    const float pixels[] = {0.2f, 0.4f, 0.6f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1);
    ASSERT_FALSE(image.isEmpty());

    image.setFromRGBA32F(nullptr, 1, 1);
    EXPECT_TRUE(image.isEmpty());
    EXPECT_EQ(image.colorDescriptor().storage,
              ArtifactCore::SurfacePixelStorage::Unknown);
    EXPECT_EQ(image.colorDescriptor().channelOrder,
              ArtifactCore::SurfaceChannelOrder::Unknown);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferPixelAccessIgnoresOutOfBoundsWritesAndReturnsTransparent)
{
    const float pixels[] = {0.2f, 0.4f, 0.6f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1);

    image.setPixel(-1, 0, FloatRGBA(1, 1, 1, 1));
    image.setPixel(0, 1, FloatRGBA(1, 1, 1, 1));
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).r(), 0.2f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).a(), 0.8f);
    EXPECT_FLOAT_EQ(image.getPixel(-1, 0).r(), 0.0f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 1).a(), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferCanonicalConversionsReturnEmptyForEmptyImage)
{
    const ArtifactCore::ImageF32x4_RGBA image;

    EXPECT_TRUE(image.toCanonicalRGBA32FC4().empty());
    EXPECT_TRUE(image.toCanonicalBGRA32FC4().empty());
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferAlphaBlendCompositesOverlayOverOpaquePixels)
{
    const float basePixels[] = {0.2f, 0.4f, 0.6f, 1.0f};
    const float overlayPixels[] = {0.8f, 0.2f, 0.0f, 0.5f};
    ArtifactCore::ImageF32x4_RGBA base;
    base.setFromRGBA32F(basePixels, 1, 1);
    ArtifactCore::ImageF32x4_RGBA overlay;
    overlay.setFromRGBA32F(overlayPixels, 1, 1);

    base.alphaBlend(overlay, 0.5f);
    const auto result = base.getPixel(0, 0);
    EXPECT_NEAR(result.r(), 0.35f, 1e-6f);
    EXPECT_NEAR(result.g(), 0.35f, 1e-6f);
    EXPECT_NEAR(result.b(), 0.45f, 1e-6f);
    EXPECT_FLOAT_EQ(result.a(), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferAlphaBlendIgnoresMismatchedOverlayDimensions)
{
    const float basePixels[] = {
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f};
    const float overlayPixels[] = {1.0f, 0.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA base;
    base.setFromRGBA32F(basePixels, 2, 1);
    ArtifactCore::ImageF32x4_RGBA overlay;
    overlay.setFromRGBA32F(overlayPixels, 1, 1);

    base.alphaBlend(overlay);
    EXPECT_EQ(base.width(), 2);
    EXPECT_FLOAT_EQ(base.getPixel(0, 0).r(), 0.1f);
    EXPECT_FLOAT_EQ(base.getPixel(0, 0).a(), 0.4f);
    EXPECT_FLOAT_EQ(base.getPixel(1, 0).b(), 0.7f);
    EXPECT_FLOAT_EQ(base.getPixel(1, 0).a(), 0.8f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferBlendReturnsLeftImageWhenDimensionsDiffer)
{
    const float leftPixels[] = {
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f};
    const float rightPixels[] = {0.9f, 0.8f, 0.7f, 0.6f};
    ArtifactCore::ImageF32x4_RGBA left;
    left.setFromRGBA32F(leftPixels, 2, 1);
    ArtifactCore::ImageF32x4_RGBA right;
    right.setFromRGBA32F(rightPixels, 1, 1);

    const auto result = left.blend(right, 0.5f);
    ASSERT_EQ(result.width(), 2);
    ASSERT_EQ(result.height(), 1);
    EXPECT_FLOAT_EQ(result.getPixel(0, 0).r(), 0.1f);
    EXPECT_FLOAT_EQ(result.getPixel(1, 0).b(), 0.7f);
    EXPECT_FLOAT_EQ(left.getPixel(0, 0).r(), 0.1f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferCvMatDescriptorOverloadPreservesBgraSemantics)
{
    cv::Mat bgra(1, 1, CV_32FC4);
    bgra.at<cv::Vec4f>(0, 0) = cv::Vec4f(0.1f, 0.3f, 0.7f, 0.9f);
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromCVMat(
        bgra,
        ArtifactCore::SurfaceColorDescriptor::legacyOpenCvBgra32Float());

    const auto canonical = image.toCanonicalRGBA32FC4();
    ASSERT_EQ(canonical.type(), CV_32FC4);
    const cv::Vec4f rgba = canonical.at<cv::Vec4f>(0, 0);
    EXPECT_FLOAT_EQ(rgba[0], 0.7f);
    EXPECT_FLOAT_EQ(rgba[1], 0.3f);
    EXPECT_FLOAT_EQ(rgba[2], 0.1f);
    EXPECT_FLOAT_EQ(rgba[3], 0.9f);
}

TEST(StillImageLayerRoundTripTest,
     CoreSurfaceViewReadsRgbaChannelsByReference)
{
    alignas(float) float pixels[] = {
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f};
    const ArtifactCore::ImageSurfaceView surface{
        pixels, 2, 1, 2u * 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknownRgba32Float()};

    const auto view = ArtifactCore::Rgba32FView::tryCreate(surface);
    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->row(0)[1].r, 0.5f);
    EXPECT_FLOAT_EQ(view->row(0)[1].g, 0.6f);
    EXPECT_FLOAT_EQ(view->row(0)[1].b, 0.7f);
    EXPECT_FLOAT_EQ(view->row(0)[1].a, 0.8f);
}

TEST(StillImageLayerRoundTripTest,
     CoreMutableSurfaceViewWritesThroughToOwnedStorage)
{
    alignas(float) float pixels[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const ArtifactCore::ImageSurfaceView surface{
        pixels, 1, 1, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknownRgba32Float()};

    auto view = ArtifactCore::MutableRgba32FView::tryCreate(surface, pixels);
    ASSERT_TRUE(view.has_value());
    view->row(0)[0].g = 0.75f;
    EXPECT_FLOAT_EQ(pixels[1], 0.75f);
}

TEST(StillImageLayerRoundTripTest,
     CoreBgraSurfaceViewMapsLogicalChannelsToBgraStorage)
{
    alignas(float) float pixels[] = {0.1f, 0.2f, 0.7f, 0.9f};
    const ArtifactCore::ImageSurfaceView surface{
        pixels, 1, 1, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::legacyOpenCvBgra32Float()};

    auto view = ArtifactCore::MutableBgra32FView::tryCreate(surface, pixels);
    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->row(0)[0].r, 0.7f);
    EXPECT_FLOAT_EQ(view->row(0)[0].g, 0.2f);
    EXPECT_FLOAT_EQ(view->row(0)[0].b, 0.1f);
    view->row(0)[0].r = 0.8f;
    EXPECT_FLOAT_EQ(pixels[2], 0.8f);
}

TEST(StillImageLayerRoundTripTest,
     CoreSurfaceViewsRejectMismatchedLayoutAndForeignWritableStorage)
{
    alignas(float) float pixels[8]{};
    alignas(float) float otherPixels[8]{};
    auto surface = ArtifactCore::ImageSurfaceView{
        pixels, 2, 1, 2u * 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknownRgba32Float()};
    ASSERT_FALSE(ArtifactCore::Bgra32FView::tryCreate(surface).has_value());
    EXPECT_FALSE(ArtifactCore::MutableRgba32FView::tryCreate(
        surface, otherPixels).has_value());

    surface.rowStride = 4u * sizeof(float);
    EXPECT_FALSE(ArtifactCore::Rgba32FView::tryCreate(surface).has_value());
    surface.rowStride = 2u * 4u * sizeof(float);
    surface.precision = ArtifactCore::SurfacePrecision::Float16;
    EXPECT_FALSE(ArtifactCore::Rgba32FView::tryCreate(surface).has_value());
}

TEST(StillImageLayerRoundTripTest,
     CoreMutableColorFloat4DispatchHandlesRgbaLayout)
{
    alignas(float) float pixels[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const ArtifactCore::ImageSurfaceView surface{
        pixels, 1, 1, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknownRgba32Float()};
    int callbackCount = 0;

    const bool handled = ArtifactCore::withMutableColorFloat4View(
        surface, pixels, [&callbackCount](auto& view) {
            ++callbackCount;
            view.row(0)[0].b = 0.65f;
        });
    EXPECT_TRUE(handled);
    EXPECT_EQ(callbackCount, 1);
    EXPECT_FLOAT_EQ(pixels[2], 0.65f);
}

TEST(StillImageLayerRoundTripTest,
     CoreMutableColorFloat4DispatchHandlesBgraLogicalChannels)
{
    alignas(float) float pixels[] = {0.1f, 0.2f, 0.7f, 0.9f};
    const ArtifactCore::ImageSurfaceView surface{
        pixels, 1, 1, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::legacyOpenCvBgra32Float()};
    int callbackCount = 0;

    const bool handled = ArtifactCore::withMutableColorFloat4View(
        surface, pixels, [&callbackCount](auto& view) {
            ++callbackCount;
            view.row(0)[0].r = 0.8f;
        });
    EXPECT_TRUE(handled);
    EXPECT_EQ(callbackCount, 1);
    EXPECT_FLOAT_EQ(pixels[2], 0.8f);
}

TEST(StillImageLayerRoundTripTest,
     CoreMutableColorFloat4DispatchRejectsUnknownLayoutWithoutCallback)
{
    alignas(float) float pixels[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const ArtifactCore::ImageSurfaceView surface{
        pixels, 1, 1, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknown()};
    int callbackCount = 0;

    const bool handled = ArtifactCore::withMutableColorFloat4View(
        surface, pixels, [&callbackCount](auto&) { ++callbackCount; });
    EXPECT_FALSE(handled);
    EXPECT_EQ(callbackCount, 0);
    EXPECT_FLOAT_EQ(pixels[0], 0.1f);
}

TEST(StillImageLayerRoundTripTest,
     CoreSurfaceViewsRejectUnalignedPointerAndStride)
{
    alignas(float) std::uint8_t bytes[4u * sizeof(float) + 1u]{};
    auto surface = ArtifactCore::ImageSurfaceView{
        bytes + 1, 1, 1, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknownRgba32Float()};
    EXPECT_FALSE(ArtifactCore::Rgba32FView::tryCreate(surface).has_value());

    surface.data = bytes;
    surface.rowStride = 4u * sizeof(float) + 1u;
    EXPECT_FALSE(ArtifactCore::Rgba32FView::tryCreate(surface).has_value());
}

TEST(StillImageLayerRoundTripTest,
     CoreSurfaceViewRejectsAddressRangeOverflow)
{
    constexpr std::uintptr_t maxAddress =
        (std::numeric_limits<std::uintptr_t>::max)();
    constexpr std::uintptr_t alignedAddress =
        maxAddress & ~static_cast<std::uintptr_t>(alignof(float) - 1u);
    const auto* nearLimit = reinterpret_cast<const void*>(alignedAddress);
    const ArtifactCore::ImageSurfaceView surface{
        nearLimit, 1, 2, 4u * sizeof(float),
        ArtifactCore::SurfacePrecision::Float32,
        ArtifactCore::SurfaceColorDescriptor::unknownRgba32Float()};

    EXPECT_FALSE(ArtifactCore::Rgba32FView::tryCreate(surface).has_value());
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferSavesRgbaPixelsThroughQImagePath)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const float pixels[] = {1.0f, 0.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1);
    const QString path = dir.filePath(QStringLiteral("core-image.png"));

    ASSERT_TRUE(image.save(path));
    const QImage saved(path);
    ASSERT_FALSE(saved.isNull());
    ASSERT_EQ(saved.size(), QSize(1, 1));
    EXPECT_EQ(saved.pixelColor(0, 0), QColor(255, 0, 0, 255));
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferLoadsSimpleRgbaPng)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QImage input(1, 1, QImage::Format_RGBA8888);
    input.setPixelColor(0, 0, QColor(0, 255, 0, 255));
    const QString path = dir.filePath(QStringLiteral("core-input.png"));
    ASSERT_TRUE(input.save(path, "PNG"));
    ArtifactCore::ImageF32x4_RGBA image;

    ASSERT_TRUE(image.load(path));
    ASSERT_EQ(image.width(), 1);
    ASSERT_EQ(image.height(), 1);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).g(), 1.0f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).a(), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferFailedLoadPreservesCurrentPixels)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const float pixels[] = {0.2f, 0.4f, 0.6f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1);
    const QString missingPath = dir.filePath(QStringLiteral("missing.png"));

    EXPECT_FALSE(image.load(missingPath));
    ASSERT_EQ(image.width(), 1);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).r(), 0.2f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).g(), 0.4f);
    EXPECT_FLOAT_EQ(image.getPixel(0, 0).a(), 0.8f);
}

TEST(StillImageLayerRoundTripTest,
     CoreImageBufferRejectsExrSaveWhenEmpty)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const ArtifactCore::ImageF32x4_RGBA image;
    const QString path = dir.filePath(QStringLiteral("empty.exr"));

    EXPECT_FALSE(image.save(path));
    EXPECT_FALSE(QFile::exists(path));
}

TEST(StillImageLayerRoundTripTest, SegmentationMaskRejectsEmptyInputs)
{
    Artifact::ArtifactImageLayer emptyLayer;
    ArtifactCore::DepthMap mask;
    mask.resize(2, 1, 1.0f);
    EXPECT_FALSE(emptyLayer.applySegmentationMask(mask));

    QImage sourceImage(2, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(90, 120, 150, 200));
    Artifact::ArtifactImageLayer populatedLayer;
    populatedLayer.setFromQImage(sourceImage);
    const ArtifactCore::DepthMap emptyMask;
    EXPECT_FALSE(populatedLayer.applySegmentationMask(emptyMask));
}

TEST(StillImageLayerRoundTripTest,
     EmptySegmentationMaskRejectionPreservesImagePixels)
{
    QImage sourceImage(2, 1, QImage::Format_RGBA8888);
    sourceImage.setPixelColor(0, 0, QColor(30, 90, 150, 120));
    sourceImage.setPixelColor(1, 0, QColor(200, 100, 40, 230));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    const QImage before = layer.toQImage();

    const ArtifactCore::DepthMap emptyMask;
    EXPECT_FALSE(layer.applySegmentationMask(emptyMask, 0.5f));
    EXPECT_EQ(layer.toQImage(), before);
}

TEST(StillImageLayerRoundTripTest, SegmentationMaskInterpolatesAlphaAndKeepsRgb)
{
    QImage sourceImage(2, 1, QImage::Format_RGBA8888);
    sourceImage.setPixelColor(0, 0, QColor(31, 97, 183, 200));
    sourceImage.setPixelColor(1, 0, QColor(210, 73, 42, 200));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);
    const QImage before = layer.toQImage();

    ArtifactCore::DepthMap mask;
    mask.resize(2, 1);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(1, 0, 1.0f);
    ASSERT_TRUE(layer.applySegmentationMask(mask, 0.5f));

    const QImage after = layer.toQImage();
    ASSERT_EQ(after.size(), QSize(2, 1));
    for (int x = 0; x < 2; ++x) {
        const QColor beforeColor = before.pixelColor(x, 0);
        const QColor afterColor = after.pixelColor(x, 0);
        EXPECT_NEAR(afterColor.red(), beforeColor.red(), 1);
        EXPECT_NEAR(afterColor.green(), beforeColor.green(), 1);
        EXPECT_NEAR(afterColor.blue(), beforeColor.blue(), 1);
    }
    EXPECT_NEAR(after.pixelColor(0, 0).alpha(), 100, 1);
    EXPECT_EQ(after.pixelColor(1, 0).alpha(), 200);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationMaskResamplesDifferentDimensionsBilinearly)
{
    QImage sourceImage(3, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(75, 125, 175, 255));
    Artifact::ArtifactImageLayer layer;
    layer.setFromQImage(sourceImage);

    ArtifactCore::DepthMap mask;
    mask.resize(2, 1);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(1, 0, 1.0f);
    ASSERT_TRUE(layer.applySegmentationMask(mask));

    const QImage result = layer.toQImage();
    ASSERT_EQ(result.size(), QSize(3, 1));
    EXPECT_NEAR(result.pixelColor(0, 0).alpha(), 0, 1);
    EXPECT_NEAR(result.pixelColor(1, 0).alpha(), 128, 1);
    EXPECT_NEAR(result.pixelColor(2, 0).alpha(), 255, 1);
}

TEST(StillImageLayerRoundTripTest, SegmentationMaskClampsOpacityToUnitInterval)
{
    QImage sourceImage(2, 1, QImage::Format_RGBA8888);
    sourceImage.fill(QColor(80, 130, 190, 200));
    ArtifactCore::DepthMap mask;
    mask.resize(2, 1);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(1, 0, 1.0f);

    Artifact::ArtifactImageLayer overOpacityLayer;
    overOpacityLayer.setFromQImage(sourceImage);
    ASSERT_TRUE(overOpacityLayer.applySegmentationMask(mask, 2.0f));
    const QImage overOpacityResult = overOpacityLayer.toQImage();
    EXPECT_NEAR(overOpacityResult.pixelColor(0, 0).alpha(), 0, 1);
    EXPECT_EQ(overOpacityResult.pixelColor(1, 0).alpha(), 200);

    Artifact::ArtifactImageLayer negativeOpacityLayer;
    negativeOpacityLayer.setFromQImage(sourceImage);
    ASSERT_TRUE(negativeOpacityLayer.applySegmentationMask(mask, -1.0f));
    const QImage negativeOpacityResult = negativeOpacityLayer.toQImage();
    EXPECT_EQ(negativeOpacityResult.pixelColor(0, 0).alpha(), 200);
    EXPECT_EQ(negativeOpacityResult.pixelColor(1, 0).alpha(), 200);
}

TEST(StillImageLayerRoundTripTest,
     CoreSegmentationMaskThresholdReplacesAlphaAndPreservesRgb)
{
    const float sourcePixels[] = {
        0.2f, 0.4f, 0.6f, 0.25f,
        0.8f, 0.6f, 0.4f, 0.75f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(sourcePixels, 2, 1);

    ArtifactCore::DepthMap mask;
    mask.resize(2, 1);
    mask.setValue(0, 0, 0.25f);
    mask.setValue(1, 0, 0.75f);
    ArtifactCore::SegmentationMaskOptions options;
    options.threshold = 0.5f;
    options.softness = 0.5f;
    options.mode = ArtifactCore::SegmentationMaskMode::ReplaceAlpha;

    ASSERT_TRUE(ArtifactCore::applySegmentationMask(image, mask, options));
    const float* result = image.rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], sourcePixels[0]);
    EXPECT_FLOAT_EQ(result[1], sourcePixels[1]);
    EXPECT_FLOAT_EQ(result[2], sourcePixels[2]);
    EXPECT_FLOAT_EQ(result[3], 0.0f);
    EXPECT_FLOAT_EQ(result[4], sourcePixels[4]);
    EXPECT_FLOAT_EQ(result[5], sourcePixels[5]);
    EXPECT_FLOAT_EQ(result[6], sourcePixels[6]);
    EXPECT_FLOAT_EQ(result[7], 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     CoreSegmentationMaskInvertAndOpacityMultiplyExistingAlpha)
{
    const float sourcePixels[] = {0.3f, 0.5f, 0.7f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(sourcePixels, 1, 1);

    ArtifactCore::DepthMap mask;
    mask.resize(1, 1, 0.25f);
    ArtifactCore::SegmentationMaskOptions options;
    options.invert = true;
    options.opacity = 0.5f;
    ASSERT_TRUE(ArtifactCore::applySegmentationMask(image, mask, options));

    const float* result = image.rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[0], sourcePixels[0]);
    EXPECT_FLOAT_EQ(result[1], sourcePixels[1]);
    EXPECT_FLOAT_EQ(result[2], sourcePixels[2]);
    EXPECT_NEAR(result[3], 0.7f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationAnalysisReportsCoverageConfidenceAndPaddedBounds)
{
    ArtifactCore::DepthMap mask;
    mask.resize(3, 2);
    mask.setValue(1, 0, 0.6f);
    mask.setValue(2, 1, 0.8f);

    const auto statistics = ArtifactCore::analyzeSegmentationMask(mask);
    EXPECT_EQ(statistics.foregroundPixels, 2);
    EXPECT_NEAR(statistics.foregroundCoverage, 1.0f / 3.0f, 1e-6f);
    EXPECT_NEAR(statistics.meanConfidence, 1.4f / 6.0f, 1e-6f);
    EXPECT_EQ(statistics.left, 1);
    EXPECT_EQ(statistics.top, 0);
    EXPECT_EQ(statistics.right, 2);
    EXPECT_EQ(statistics.bottom, 1);

    const auto bounds =
        ArtifactCore::findSegmentationForegroundBounds(mask, 0.5f, 1);
    EXPECT_TRUE(bounds.isValid());
    EXPECT_EQ(bounds.x, 0);
    EXPECT_EQ(bounds.y, 0);
    EXPECT_EQ(bounds.width, 3);
    EXPECT_EQ(bounds.height, 2);

    ArtifactCore::SegmentationMaskAcceptanceOptions acceptance;
    acceptance.minimumCoverage = 0.3f;
    acceptance.maximumCoverage = 0.4f;
    acceptance.minimumMeanConfidence = 0.2f;
    EXPECT_TRUE(ArtifactCore::acceptsSegmentationMask(mask, acceptance));
    acceptance.minimumMeanConfidence = 0.3f;
    EXPECT_FALSE(ArtifactCore::acceptsSegmentationMask(mask, acceptance));
}

TEST(StillImageLayerRoundTripTest,
     SegmentationMaskCombineModesFollowTheirCoverageAlgebra)
{
    ArtifactCore::DepthMap source;
    source.resize(1, 1, 0.4f);
    const auto combineWithSource = [&source](float targetValue,
                                             ArtifactCore::SegmentationMaskCombineMode mode) {
        ArtifactCore::DepthMap target;
        target.resize(1, 1, targetValue);
        EXPECT_TRUE(ArtifactCore::combineSegmentationMasks(target, source, mode));
        return target.value(0, 0);
    };

    EXPECT_FLOAT_EQ(combineWithSource(
                        0.6f, ArtifactCore::SegmentationMaskCombineMode::Replace),
                    0.4f);
    EXPECT_FLOAT_EQ(combineWithSource(
                        0.6f, ArtifactCore::SegmentationMaskCombineMode::Union),
                    0.6f);
    EXPECT_FLOAT_EQ(combineWithSource(
                        0.6f, ArtifactCore::SegmentationMaskCombineMode::Intersect),
                    0.4f);
    EXPECT_NEAR(combineWithSource(
                    0.6f, ArtifactCore::SegmentationMaskCombineMode::Subtract),
                0.2f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationMaskCombineResamplesDifferentResolutions)
{
    ArtifactCore::DepthMap target;
    target.resize(3, 1, 0.5f);
    ArtifactCore::DepthMap source;
    source.resize(2, 1);
    source.setValue(0, 0, 0.0f);
    source.setValue(1, 0, 1.0f);

    ASSERT_TRUE(ArtifactCore::combineSegmentationMasks(
        target, source, ArtifactCore::SegmentationMaskCombineMode::Union));
    EXPECT_FLOAT_EQ(target.value(0, 0), 0.5f);
    EXPECT_FLOAT_EQ(target.value(1, 0), 0.75f);
    EXPECT_FLOAT_EQ(target.value(2, 0), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationMaskPreviewInvertsWithoutChangingTheSourceMask)
{
    ArtifactCore::DepthMap mask;
    mask.resize(2, 1);
    mask.setValue(0, 0, 0.25f);
    mask.setValue(1, 0, 0.75f);
    ArtifactCore::ImageF32x4_RGBA preview;

    ASSERT_TRUE(ArtifactCore::renderSegmentationMaskPreview(mask, preview, true));
    const float* rgba = preview.rgba32fData();
    ASSERT_NE(rgba, nullptr);
    EXPECT_FLOAT_EQ(rgba[0], 0.75f);
    EXPECT_FLOAT_EQ(rgba[1], 0.75f);
    EXPECT_FLOAT_EQ(rgba[2], 0.75f);
    EXPECT_FLOAT_EQ(rgba[3], 1.0f);
    EXPECT_FLOAT_EQ(rgba[4], 0.25f);
    EXPECT_FLOAT_EQ(rgba[5], 0.25f);
    EXPECT_FLOAT_EQ(rgba[6], 0.25f);
    EXPECT_FLOAT_EQ(rgba[7], 1.0f);
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.25f);
    EXPECT_FLOAT_EQ(mask.value(1, 0), 0.75f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationRefinementMapsSoftThresholdTransition)
{
    ArtifactCore::DepthMap mask;
    mask.resize(4, 1);
    mask.setValue(0, 0, 0.25f);
    mask.setValue(1, 0, 0.5f);
    mask.setValue(2, 0, 0.75f);
    mask.setValue(3, 0, 1.0f);
    ArtifactCore::SegmentationMaskRefinementOptions options;
    options.threshold = 0.5f;
    options.softness = 0.5f;

    ASSERT_TRUE(ArtifactCore::refineSegmentationMask(mask, options));
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
    EXPECT_NEAR(mask.value(1, 0), 0.5f, 1e-6f);
    EXPECT_FLOAT_EQ(mask.value(2, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(3, 0), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationRefinementUsesInclusiveHardThresholdAtZeroSoftness)
{
    ArtifactCore::DepthMap mask;
    mask.resize(3, 1);
    mask.setValue(0, 0, 0.49f);
    mask.setValue(1, 0, 0.5f);
    mask.setValue(2, 0, 0.51f);
    ArtifactCore::SegmentationMaskRefinementOptions options;
    options.threshold = 0.5f;

    ASSERT_TRUE(ArtifactCore::refineSegmentationMask(mask, options));
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(1, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(2, 0), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationRefinementExpandsForegroundWithinRadius)
{
    ArtifactCore::DepthMap mask;
    mask.resize(5, 1);
    mask.setValue(2, 0, 1.0f);
    ArtifactCore::SegmentationMaskRefinementOptions options;
    options.expandPixels = 1;

    ASSERT_TRUE(ArtifactCore::refineSegmentationMask(mask, options));
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(1, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(2, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(3, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(4, 0), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationRefinementContractsForegroundAroundBackground)
{
    ArtifactCore::DepthMap mask;
    mask.resize(5, 1, 1.0f);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(4, 0, 0.0f);
    ArtifactCore::SegmentationMaskRefinementOptions options;
    options.expandPixels = -1;

    ASSERT_TRUE(ArtifactCore::refineSegmentationMask(mask, options));
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(1, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(2, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(3, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(4, 0), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationRefinementFeathersAlongBothAxes)
{
    ArtifactCore::DepthMap mask;
    mask.resize(5, 3);
    mask.setValue(2, 1, 1.0f);
    ArtifactCore::SegmentationMaskRefinementOptions options;
    options.featherPixels = 1;

    ASSERT_TRUE(ArtifactCore::refineSegmentationMask(mask, options));
    EXPECT_NEAR(mask.value(2, 1), 1.0f / 9.0f, 1e-6f);
    EXPECT_NEAR(mask.value(1, 1), 1.0f / 9.0f, 1e-6f);
    EXPECT_NEAR(mask.value(2, 0), 1.0f / 9.0f, 1e-6f);
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(4, 2), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationHoleFillOnlyFillsEnclosedBackground)
{
    ArtifactCore::DepthMap mask;
    mask.resize(5, 5, 1.0f);
    mask.setValue(2, 2, 0.0f);
    mask.setValue(0, 0, 0.0f);

    ArtifactCore::SegmentationHoleFillOptions options;
    options.backgroundThreshold = 0.5f;
    options.maximumArea = 1;
    ASSERT_TRUE(ArtifactCore::fillSegmentationMaskHoles(mask, options));
    EXPECT_FLOAT_EQ(mask.value(2, 2), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationHoleFillHonorsMaximumAreaBoundary)
{
    ArtifactCore::DepthMap mask;
    mask.resize(5, 5, 1.0f);
    mask.setValue(2, 2, 0.0f);
    mask.setValue(2, 3, 0.0f);
    ArtifactCore::SegmentationHoleFillOptions options;
    options.backgroundThreshold = 0.5f;
    options.maximumArea = 1;

    ASSERT_TRUE(ArtifactCore::fillSegmentationMaskHoles(mask, options));
    EXPECT_FLOAT_EQ(mask.value(2, 2), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(2, 3), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationHoleFillTreatsThresholdEqualityAsForeground)
{
    ArtifactCore::DepthMap mask;
    mask.resize(3, 3, 1.0f);
    mask.setValue(1, 1, 0.5f);
    ArtifactCore::SegmentationHoleFillOptions options;
    options.backgroundThreshold = 0.5f;

    ASSERT_TRUE(ArtifactCore::fillSegmentationMaskHoles(mask, options));
    EXPECT_FLOAT_EQ(mask.value(1, 1), 0.5f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationSmallComponentRemovalRejectsZeroAreaLimitWithoutMutation)
{
    ArtifactCore::DepthMap mask;
    mask.resize(3, 1, 0.0f);
    mask.setValue(1, 0, 1.0f);
    ArtifactCore::SegmentationSmallComponentOptions options;
    options.maximumArea = 0;

    EXPECT_FALSE(ArtifactCore::removeSegmentationSmallComponents(mask, options));
    EXPECT_FLOAT_EQ(mask.value(1, 0), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationRefinementRejectsEmptyMask)
{
    ArtifactCore::DepthMap mask;
    const ArtifactCore::SegmentationMaskRefinementOptions options;

    EXPECT_FALSE(ArtifactCore::refineSegmentationMask(mask, options));
    EXPECT_TRUE(mask.isEmpty());
}

TEST(StillImageLayerRoundTripTest,
     SegmentationSmallComponentRemovalPreservesLargerForeground)
{
    ArtifactCore::DepthMap mask;
    mask.resize(5, 1, 0.0f);
    mask.setValue(0, 0, 1.0f);
    mask.setValue(2, 0, 1.0f);
    mask.setValue(3, 0, 0.8f);
    mask.setValue(4, 0, 1.0f);

    ArtifactCore::SegmentationSmallComponentOptions options;
    options.foregroundThreshold = 0.5f;
    options.maximumArea = 1;
    ASSERT_TRUE(ArtifactCore::removeSegmentationSmallComponents(mask, options));
    EXPECT_FLOAT_EQ(mask.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(mask.value(2, 0), 1.0f);
    EXPECT_FLOAT_EQ(mask.value(3, 0), 0.8f);
    EXPECT_FLOAT_EQ(mask.value(4, 0), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationMaskStabilizationBlendsAcrossResolutions)
{
    ArtifactCore::DepthMap previous;
    previous.resize(2, 1);
    previous.setValue(0, 0, 0.0f);
    previous.setValue(1, 0, 1.0f);
    ArtifactCore::DepthMap current;
    current.resize(3, 1, 0.0f);

    ASSERT_TRUE(ArtifactCore::stabilizeSegmentationMask(previous, current, 0.5f));
    EXPECT_FLOAT_EQ(current.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(current.value(1, 0), 0.25f);
    EXPECT_FLOAT_EQ(current.value(2, 0), 0.5f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationDifferenceUsesNormalizedResamplingAndThreshold)
{
    ArtifactCore::DepthMap previous;
    previous.resize(2, 1);
    previous.setValue(0, 0, 0.0f);
    previous.setValue(1, 0, 1.0f);
    ArtifactCore::DepthMap current;
    current.resize(3, 1);
    current.setValue(0, 0, 0.0f);
    current.setValue(1, 0, 0.25f);
    current.setValue(2, 0, 1.0f);

    const auto difference = ArtifactCore::analyzeSegmentationMaskDifference(
        previous, current, 0.2f);
    EXPECT_NEAR(difference.meanAbsoluteDifference, 1.0f / 12.0f, 1e-6f);
    EXPECT_FLOAT_EQ(difference.maximumAbsoluteDifference, 0.25f);
    EXPECT_NEAR(difference.changedCoverage, 1.0f / 3.0f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     LuminanceSegmenterMapsLumaAndRespectsSourceAlpha)
{
    const float pixels[] = {
        0.1f, 0.1f, 0.1f, 0.5f,
        0.5f, 0.5f, 0.5f, 0.5f,
        0.9f, 0.9f, 0.9f, 0.8f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 3, 1);
    ArtifactCore::DepthMap mask;
    ArtifactCore::LuminanceSegmentationOptions options;
    options.blackPoint = 0.0f;
    options.whitePoint = 1.0f;
    ArtifactCore::LuminanceImageSegmenter segmenter(options);

    ASSERT_TRUE(segmenter.isReady());
    ASSERT_TRUE(segmenter.segment(source, mask));
    ASSERT_EQ(mask.width(), 3);
    ASSERT_EQ(mask.height(), 1);
    EXPECT_NEAR(mask.value(0, 0), 0.05f, 1e-6f);
    EXPECT_NEAR(mask.value(1, 0), 0.25f, 1e-6f);
    EXPECT_NEAR(mask.value(2, 0), 0.72f, 1e-6f);
    EXPECT_TRUE(segmenter.lastError().isEmpty());
}

TEST(StillImageLayerRoundTripTest,
     LuminanceSegmenterAppliesGammaDarkForegroundAndAlphaOption)
{
    const float pixels[] = {
        0.25f, 0.25f, 0.25f, 0.2f,
        0.75f, 0.75f, 0.75f, 0.1f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 2, 1);
    ArtifactCore::DepthMap mask;
    ArtifactCore::LuminanceSegmentationOptions options;
    options.blackPoint = 0.0f;
    options.whitePoint = 1.0f;
    options.gamma = 2.0f;
    options.darkForeground = true;
    options.respectSourceAlpha = false;
    ArtifactCore::LuminanceImageSegmenter segmenter(options);

    ASSERT_TRUE(segmenter.segment(source, mask));
    EXPECT_NEAR(mask.value(0, 0), 0.5f, 1e-6f);
    EXPECT_NEAR(mask.value(1, 0), 1.0f - std::sqrt(0.75f), 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     LuminanceSegmenterClearsExistingMaskForEmptySource)
{
    ArtifactCore::ImageF32x4_RGBA emptySource;
    ArtifactCore::DepthMap mask;
    mask.resize(2, 1, 0.75f);
    ArtifactCore::LuminanceImageSegmenter segmenter;

    EXPECT_FALSE(segmenter.segment(emptySource, mask));
    EXPECT_TRUE(mask.isEmpty());
    EXPECT_EQ(mask.width(), 0);
    EXPECT_EQ(mask.height(), 0);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationDespillOnlyCorrectsSoftGreenEdgesAndKeepsAlpha)
{
    const float pixels[] = {
        0.2f, 0.8f, 0.1f, 0.6f,
        0.2f, 0.8f, 0.1f, 0.6f,
        0.2f, 0.8f, 0.1f, 0.6f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 3, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(3, 1);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(1, 0, 0.5f);
    mask.setValue(2, 0, 1.0f);
    ArtifactCore::SegmentationDespillOptions options;
    options.strength = 0.5f;
    options.edgeWidth = 0.25f;

    ASSERT_TRUE(ArtifactCore::despillSegmentationEdges(image, mask, options));
    const float* result = image.rgba32fData();
    ASSERT_NE(result, nullptr);
    EXPECT_FLOAT_EQ(result[1], 0.8f);
    EXPECT_NEAR(result[5], 0.5f, 1e-6f);
    EXPECT_FLOAT_EQ(result[9], 0.8f);
    EXPECT_FLOAT_EQ(result[3], 0.6f);
    EXPECT_FLOAT_EQ(result[7], 0.6f);
    EXPECT_FLOAT_EQ(result[11], 0.6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationForegroundExtractionLeavesSourceUnchanged)
{
    const float pixels[] = {
        0.2f, 0.4f, 0.6f, 0.5f,
        0.8f, 0.6f, 0.4f, 0.75f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 2, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(2, 1);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(1, 0, 1.0f);
    ArtifactCore::ImageF32x4_RGBA foreground;

    ASSERT_TRUE(ArtifactCore::extractSegmentationForeground(
        source, mask, foreground));
    ASSERT_EQ(foreground.width(), 2);
    const float* sourceAfter = source.rgba32fData();
    const float* foregroundPixels = foreground.rgba32fData();
    ASSERT_NE(sourceAfter, nullptr);
    ASSERT_NE(foregroundPixels, nullptr);
    EXPECT_FLOAT_EQ(sourceAfter[3], pixels[3]);
    EXPECT_FLOAT_EQ(sourceAfter[7], pixels[7]);
    EXPECT_FLOAT_EQ(foregroundPixels[3], 0.0f);
    EXPECT_FLOAT_EQ(foregroundPixels[7], pixels[7]);
    EXPECT_FLOAT_EQ(foregroundPixels[0], pixels[0]);
    EXPECT_FLOAT_EQ(foregroundPixels[4], pixels[4]);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationForegroundCropMapsLowResolutionMaskToSourcePixels)
{
    const float pixels[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        0.1f, 0.0f, 0.0f, 1.0f,
        0.2f, 0.0f, 0.0f, 1.0f,
        0.3f, 0.0f, 0.0f, 1.0f,
        0.4f, 0.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 5, 1);
    ArtifactCore::DepthMap lowResolutionMask;
    lowResolutionMask.resize(3, 1);
    lowResolutionMask.setValue(0, 0, 0.0f);
    lowResolutionMask.setValue(1, 0, 1.0f);
    lowResolutionMask.setValue(2, 0, 0.0f);
    ArtifactCore::ImageF32x4_RGBA foreground;

    ASSERT_TRUE(ArtifactCore::cropSegmentationForeground(
        source, lowResolutionMask, foreground));
    ASSERT_EQ(foreground.width(), 1);
    ASSERT_EQ(foreground.height(), 1);
    EXPECT_NEAR(foreground.getPixel(0, 0).r(), 0.2f, 1e-6f);
    EXPECT_FLOAT_EQ(foreground.getPixel(0, 0).a(), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationForegroundCropRejectsEmptyMaskWithoutChangingOutput)
{
    const float sourcePixels[] = {0.6f, 0.3f, 0.1f, 1.0f};
    const float sentinelPixels[] = {0.2f, 0.4f, 0.8f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(sourcePixels, 1, 1);
    ArtifactCore::ImageF32x4_RGBA foreground;
    foreground.setFromRGBA32F(sentinelPixels, 1, 1);
    const auto before = foreground.getPixel(0, 0);
    ArtifactCore::DepthMap emptyMask;

    EXPECT_FALSE(ArtifactCore::cropSegmentationForeground(
        source, emptyMask, foreground));
    EXPECT_NEAR(foreground.getPixel(0, 0).r(), before.r(), 1e-6f);
    EXPECT_NEAR(foreground.getPixel(0, 0).g(), before.g(), 1e-6f);
    EXPECT_NEAR(foreground.getPixel(0, 0).b(), before.b(), 1e-6f);
    EXPECT_NEAR(foreground.getPixel(0, 0).a(), before.a(), 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationBatchCountsSuccessesAndInvalidItems)
{
    const float pixel[] = {0.5f, 0.5f, 0.5f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA validSource;
    validSource.setFromRGBA32F(pixel, 1, 1);
    ArtifactCore::ImageF32x4_RGBA emptySource;
    ArtifactCore::DepthMap validOutput;
    ArtifactCore::DepthMap missingSourceOutput;
    ArtifactCore::DepthMap emptySourceOutput;
    const ArtifactCore::SegmentationBatchItem items[] = {
        {nullptr, &missingSourceOutput},
        {&emptySource, &emptySourceOutput},
        {&validSource, &validOutput}};
    ArtifactCore::LuminanceSegmentationOptions luminanceOptions;
    luminanceOptions.blackPoint = 0.0f;
    luminanceOptions.whitePoint = 1.0f;
    ArtifactCore::LuminanceImageSegmenter segmenter(luminanceOptions);
    ArtifactCore::SegmentationBatchOptions batchOptions;
    batchOptions.refine = false;

    const auto result = ArtifactCore::segmentBatch(
        segmenter, items, batchOptions);
    EXPECT_EQ(result.completed, 1);
    EXPECT_EQ(result.failed, 2);
    EXPECT_EQ(result.lastError,
              QStringLiteral("Segmentation batch item has no source or output mask."));
    ASSERT_EQ(validOutput.width(), 1);
    EXPECT_NEAR(validOutput.value(0, 0), 0.5f, 1e-6f);
    EXPECT_TRUE(emptySourceOutput.isEmpty());
}

TEST(StillImageLayerRoundTripTest,
     SegmentationBatchSkipsInferenceWhenSegmenterIsNotReady)
{
    const float pixel[] = {0.5f, 0.5f, 0.5f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixel, 1, 1);
    ArtifactCore::DepthMap firstOutput;
    ArtifactCore::DepthMap secondOutput;
    const ArtifactCore::SegmentationBatchItem items[] = {
        {&source, &firstOutput}, {&source, &secondOutput}};
    NotReadyImageSegmenter segmenter;

    const auto result = ArtifactCore::segmentBatch(segmenter, items);
    EXPECT_EQ(result.completed, 0);
    EXPECT_EQ(result.failed, 2);
    EXPECT_EQ(result.lastError, QStringLiteral("model unavailable"));
    EXPECT_EQ(segmenter.segmentCalls, 0);
    EXPECT_TRUE(firstOutput.isEmpty());
    EXPECT_TRUE(secondOutput.isEmpty());
}

TEST(StillImageLayerRoundTripTest,
     SegmentationBatchAppliesConfiguredRefinement)
{
    const float pixels[] = {
        0.25f, 0.25f, 0.25f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f,
        0.75f, 0.75f, 0.75f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 3, 1);
    ArtifactCore::DepthMap output;
    const ArtifactCore::SegmentationBatchItem items[] = {{&source, &output}};
    ArtifactCore::LuminanceSegmentationOptions luminanceOptions;
    luminanceOptions.blackPoint = 0.0f;
    luminanceOptions.whitePoint = 1.0f;
    ArtifactCore::LuminanceImageSegmenter segmenter(luminanceOptions);
    ArtifactCore::SegmentationBatchOptions batchOptions;
    batchOptions.refinement.threshold = 0.5f;

    const auto result = ArtifactCore::segmentBatch(
        segmenter, items, batchOptions);
    EXPECT_EQ(result.completed, 1);
    EXPECT_EQ(result.failed, 0);
    EXPECT_FLOAT_EQ(output.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(output.value(1, 0), 1.0f);
    EXPECT_FLOAT_EQ(output.value(2, 0), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationOverColorCompositesForegroundAndBackgroundEndpoints)
{
    const float pixels[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 3, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(3, 1);
    mask.setValue(0, 0, 0.0f);
    mask.setValue(1, 0, 0.5f);
    mask.setValue(2, 0, 1.0f);
    ArtifactCore::ImageF32x4_RGBA destination;
    const FloatRGBA blue(0.0f, 0.0f, 1.0f, 1.0f);

    ASSERT_TRUE(ArtifactCore::compositeSegmentationOverColor(
        source, mask, blue, destination));
    EXPECT_EQ(destination.width(), 3);
    EXPECT_NEAR(destination.getPixel(0, 0).b(), 1.0f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(1, 0).r(), 0.5f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(1, 0).b(), 0.5f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(2, 0).r(), 1.0f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(2, 0).b(), 0.0f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationOverImageSamplesDifferentResolutionBackground)
{
    const float sourcePixels[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f};
    const float backgroundPixels[] = {
        0.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(sourcePixels, 3, 1);
    ArtifactCore::ImageF32x4_RGBA background;
    background.setFromRGBA32F(backgroundPixels, 2, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(3, 1);
    mask.setValue(0, 0, 1.0f);
    mask.setValue(1, 0, 0.0f);
    mask.setValue(2, 0, 1.0f);
    ArtifactCore::ImageF32x4_RGBA destination;

    ASSERT_TRUE(ArtifactCore::compositeSegmentationOverImage(
        source, mask, background, destination));
    EXPECT_EQ(destination.width(), 3);
    EXPECT_NEAR(destination.getPixel(0, 0).r(), 1.0f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(1, 0).g(), 0.5f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(1, 0).b(), 0.5f, 1e-6f);
    EXPECT_NEAR(destination.getPixel(2, 0).r(), 1.0f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationBackgroundCompositorsRejectEmptyInputsWithoutChangingOutput)
{
    ArtifactCore::ImageF32x4_RGBA emptySource;
    ArtifactCore::DepthMap emptyMask;
    const float sentinelPixels[] = {0.25f, 0.5f, 0.75f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA destination;
    destination.setFromRGBA32F(sentinelPixels, 1, 1);
    const auto sentinel = destination.getPixel(0, 0);

    EXPECT_FALSE(ArtifactCore::compositeSegmentationOverColor(
        emptySource, emptyMask, FloatRGBA(0, 0, 0, 1),
        destination));
    EXPECT_NEAR(destination.getPixel(0, 0).r(), sentinel.r(), 1e-6f);
    EXPECT_NEAR(destination.getPixel(0, 0).g(), sentinel.g(), 1e-6f);
    EXPECT_FALSE(ArtifactCore::compositeSegmentationOverImage(
        emptySource, emptyMask, emptySource, destination));
    EXPECT_NEAR(destination.getPixel(0, 0).b(), sentinel.b(), 1e-6f);
    EXPECT_NEAR(destination.getPixel(0, 0).a(), sentinel.a(), 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationOverColorUsesStraightAlphaForTwoTranslucentInputs)
{
    const float sourcePixels[] = {1.0f, 0.0f, 0.0f, 0.5f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(sourcePixels, 1, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(1, 1, 1.0f);
    ArtifactCore::ImageF32x4_RGBA destination;
    const FloatRGBA background(0.0f, 0.0f, 1.0f, 0.5f);

    ASSERT_TRUE(ArtifactCore::compositeSegmentationOverColor(
        source, mask, background, destination));
    const auto result = destination.getPixel(0, 0);
    EXPECT_NEAR(result.r(), 2.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(result.g(), 0.0f);
    EXPECT_NEAR(result.b(), 1.0f / 3.0f, 1e-6f);
    EXPECT_FLOAT_EQ(result.a(), 0.75f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationOverColorInvertedMaskSelectsBackground)
{
    const float sourcePixels[] = {0.0f, 1.0f, 0.0f, 1.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(sourcePixels, 1, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(1, 1, 1.0f);
    ArtifactCore::ImageF32x4_RGBA destination;
    const FloatRGBA background(0.2f, 0.4f, 0.6f, 0.25f);

    ASSERT_TRUE(ArtifactCore::compositeSegmentationOverColor(
        source, mask, background, destination, true));
    const auto result = destination.getPixel(0, 0);
    EXPECT_NEAR(result.r(), 0.2f, 1e-6f);
    EXPECT_NEAR(result.g(), 0.4f, 1e-6f);
    EXPECT_NEAR(result.b(), 0.6f, 1e-6f);
    EXPECT_NEAR(result.a(), 0.25f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest,
     SegmentationOverImagePreservesForegroundAlphaOverTransparentBackground)
{
    const float sourcePixels[] = {0.0f, 1.0f, 0.0f, 0.4f};
    const float backgroundPixels[] = {1.0f, 0.0f, 0.0f, 0.0f};
    ArtifactCore::ImageF32x4_RGBA source;
    source.setFromRGBA32F(sourcePixels, 1, 1);
    ArtifactCore::ImageF32x4_RGBA background;
    background.setFromRGBA32F(backgroundPixels, 1, 1);
    ArtifactCore::DepthMap mask;
    mask.resize(1, 1, 1.0f);
    ArtifactCore::ImageF32x4_RGBA destination;

    ASSERT_TRUE(ArtifactCore::compositeSegmentationOverImage(
        source, mask, background, destination));
    const auto result = destination.getPixel(0, 0);
    EXPECT_FLOAT_EQ(result.r(), 0.0f);
    EXPECT_FLOAT_EQ(result.g(), 1.0f);
    EXPECT_FLOAT_EQ(result.b(), 0.0f);
    EXPECT_NEAR(result.a(), 0.4f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest, DepthMapClampsWritesAndBilinearCoordinates)
{
    ArtifactCore::DepthMap map;
    EXPECT_TRUE(map.isEmpty());

    map.resize(2, 2, 1.5f);
    ASSERT_FALSE(map.isEmpty());
    EXPECT_FLOAT_EQ(map.value(0, 0), 1.0f);
    map.setValue(0, 0, -2.0f);
    map.setValue(1, 0, 0.0f);
    map.setValue(0, 1, 1.0f);
    map.setValue(1, 1, 0.0f);
    map.setValue(2, 1, 0.7f);

    EXPECT_FLOAT_EQ(map.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.25f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(-1.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(2.0f, 0.0f), 0.0f);

    map.resize(0, 2);
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
}

TEST(StillImageLayerRoundTripTest,
     DepthMapResizeClampsInitialValueAcrossEveryPixel)
{
    ArtifactCore::DepthMap map;
    map.resize(2, 2, -0.5f);

    ASSERT_FALSE(map.isEmpty());
    EXPECT_FLOAT_EQ(map.value(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.value(1, 0), 0.0f);
    EXPECT_FLOAT_EQ(map.value(0, 1), 0.0f);
    EXPECT_FLOAT_EQ(map.value(1, 1), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMapClearIsIdempotentAndEmptySamplingReturnsZero)
{
    ArtifactCore::DepthMap map;
    map.resize(2, 3, 0.75f);
    map.clear();
    map.clear();

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.5f, 0.5f), 0.0f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMapBilinearSamplingHandlesSingleRowAndColumn)
{
    ArtifactCore::DepthMap singleRow;
    singleRow.resize(3, 1);
    singleRow.setValue(0, 0, 0.0f);
    singleRow.setValue(1, 0, 0.5f);
    singleRow.setValue(2, 0, 1.0f);
    EXPECT_FLOAT_EQ(singleRow.sampleBilinear(0.0f, 0.7f), 0.0f);
    EXPECT_FLOAT_EQ(singleRow.sampleBilinear(0.5f, -1.0f), 0.5f);
    EXPECT_FLOAT_EQ(singleRow.sampleBilinear(1.0f, 1.0f), 1.0f);

    ArtifactCore::DepthMap singleColumn;
    singleColumn.resize(1, 3);
    singleColumn.setValue(0, 0, 0.0f);
    singleColumn.setValue(0, 1, 0.5f);
    singleColumn.setValue(0, 2, 1.0f);
    EXPECT_FLOAT_EQ(singleColumn.sampleBilinear(0.2f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(singleColumn.sampleBilinear(2.0f, 0.5f), 0.5f);
    EXPECT_FLOAT_EQ(singleColumn.sampleBilinear(0.8f, 1.0f), 1.0f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMapBilinearSamplingUsesBothOffCenterInterpolationWeights)
{
    ArtifactCore::DepthMap map;
    map.resize(2, 2);
    map.setValue(0, 0, 0.0f);
    map.setValue(1, 0, 0.2f);
    map.setValue(0, 1, 0.6f);
    map.setValue(1, 1, 1.0f);

    EXPECT_NEAR(map.sampleBilinear(0.25f, 0.75f), 0.5375f, 1e-6f);
}

TEST(StillImageLayerRoundTripTest, DepthMapBilinearSamplingHandlesSinglePixel)
{
    ArtifactCore::DepthMap singlePixel;
    singlePixel.resize(1, 1, 0.625f);

    EXPECT_FLOAT_EQ(singlePixel.sampleBilinear(0.0f, 0.0f), 0.625f);
    EXPECT_FLOAT_EQ(singlePixel.sampleBilinear(0.5f, 0.5f), 0.625f);
    EXPECT_FLOAT_EQ(singlePixel.sampleBilinear(1.0f, 1.0f), 0.625f);
}

TEST(StillImageLayerRoundTripTest,
     DepthMapBilinearSamplingClampsInfiniteCoordinates)
{
    ArtifactCore::DepthMap map;
    map.resize(2, 2);
    map.setValue(0, 0, 0.1f);
    map.setValue(1, 0, 0.2f);
    map.setValue(0, 1, 0.3f);
    map.setValue(1, 1, 0.4f);

    const float negativeInfinity = -std::numeric_limits<float>::infinity();
    const float positiveInfinity = std::numeric_limits<float>::infinity();
    EXPECT_FLOAT_EQ(map.sampleBilinear(negativeInfinity, 0.0f), 0.1f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(positiveInfinity, 0.0f), 0.2f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.0f, negativeInfinity), 0.1f);
    EXPECT_FLOAT_EQ(map.sampleBilinear(0.0f, positiveInfinity), 0.3f);
}

TEST(StillImageLayerRoundTripTest, DepthMapResizeWithNegativeDimensionClears)
{
    ArtifactCore::DepthMap map;
    map.resize(2, 3, 0.75f);
    ASSERT_FALSE(map.isEmpty());

    map.resize(-1, 3, 0.25f);
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);

    map.resize(3, 2, 0.75f);
    ASSERT_FALSE(map.isEmpty());
    map.resize(3, -1, 0.25f);
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
}

TEST(StillImageLayerRoundTripTest, DepthMapLoadsGrayscaleAndClearsOnLoadFailure)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QImage depthImage(3, 1, QImage::Format_Grayscale8);
    uchar* row = depthImage.scanLine(0);
    row[0] = 0;
    row[1] = 128;
    row[2] = 255;
    const QString validPath = dir.filePath(QStringLiteral("grayscale-depth.png"));
    ASSERT_TRUE(depthImage.save(validPath, "PNG"));

    ArtifactCore::DepthMap map;
    ASSERT_TRUE(map.load(validPath));
    ASSERT_FALSE(map.isEmpty());
    EXPECT_EQ(map.width(), 3);
    EXPECT_EQ(map.height(), 1);
    EXPECT_FLOAT_EQ(map.value(0, 0), 0.0f);
    EXPECT_NEAR(map.value(1, 0), 128.0f / 255.0f, 1e-7f);
    EXPECT_FLOAT_EQ(map.value(2, 0), 1.0f);

    const QString missingPath = dir.filePath(QStringLiteral("missing-depth.png"));
    EXPECT_FALSE(map.load(missingPath));
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.width(), 0);
    EXPECT_EQ(map.height(), 0);
}

TEST(StillImageLayerRoundTripTest, FailedDepthMapRelinkPreservesActiveSource)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());

    QImage depthImage(2, 2, QImage::Format_Grayscale8);
    depthImage.setPixelColor(0, 0, QColor(0, 0, 0));
    depthImage.setPixelColor(1, 0, QColor(80, 80, 80));
    depthImage.setPixelColor(0, 1, QColor(160, 160, 160));
    depthImage.setPixelColor(1, 1, QColor(255, 255, 255));
    const QString validPath = dir.filePath(QStringLiteral("depth.png"));
    ASSERT_TRUE(depthImage.save(validPath, "PNG"));
    const QString missingPath = dir.filePath(QStringLiteral("missing-depth.png"));

    Artifact::ArtifactImageLayer layer;
    ArtifactCore::DepthMeshOptions options;
    options.columns = 2;
    options.rows = 2;
    layer.setDepthMeshOptions(options);
    ASSERT_TRUE(layer.setDepthMapPath(validPath));
    ASSERT_TRUE(layer.hasDepthMap());
    ASSERT_TRUE(layer.is3D());
    ASSERT_EQ(layer.depthMesh().vertexCount(), 4);

    EXPECT_FALSE(layer.setDepthMapPath(missingPath));
    EXPECT_FALSE(layer.setDepthMapPath(QString()));
    EXPECT_EQ(layer.depthMapPath(), validPath);
    EXPECT_TRUE(layer.hasDepthMap());
    EXPECT_TRUE(layer.is3D());
    EXPECT_EQ(layer.depthMesh().vertexCount(), 4);

    layer.clearDepthMap();
    EXPECT_FALSE(layer.hasDepthMap());
    EXPECT_FALSE(layer.is3D());
}

TEST(StillImageLayerRoundTripTest, RejectsImageDimensionOverLimitBeforeDecode)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QString imagePath = dir.filePath(QStringLiteral("dimension-over-limit.png"));
    ASSERT_TRUE(writeBase64Fixture(
        imagePath,
        QByteArrayLiteral("iVBORw0KGgoAAAANSUhEUgAAQAEAAAABCAYAAADJXd1mAAAADUlEQVR4nGPg5GD/DwABTgEYKZ81xwAAAABJRU5ErkJggg==")));

    Artifact::ArtifactImageLayer layer;
    EXPECT_FALSE(layer.loadFromPath(imagePath));
    EXPECT_EQ(layer.sourcePath(), imagePath);
    EXPECT_TRUE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.toQImage().size(), QSize(256, 256));
    expectDimensionFallbackRecorded(imagePath);
}

TEST(StillImageLayerRoundTripTest, RejectsDecodedPixelBudgetOverLimitBeforeDecode)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    isolateProjectRoot(dir);

    const QString imagePath = dir.filePath(QStringLiteral("pixel-budget-over-limit.png"));
    ASSERT_TRUE(writeBase64Fixture(
        imagePath,
        QByteArrayLiteral("iVBORw0KGgoAAAANSUhEUgAAIAAAACABCAYAAAC59hn8AAAADUlEQVR4nGPg5GD/DwABTgEYKZ81xwAAAABJRU5ErkJggg==")));

    Artifact::ArtifactImageLayer layer;
    EXPECT_FALSE(layer.loadFromPath(imagePath));
    EXPECT_EQ(layer.sourcePath(), imagePath);
    EXPECT_TRUE(layer.hasCurrentFrameBuffer());
    EXPECT_EQ(layer.toQImage().size(), QSize(256, 256));
    expectDimensionFallbackRecorded(imagePath);
}
\n
\n
\n
\n
\n
