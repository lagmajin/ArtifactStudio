#include <gtest/gtest.h>

#include <QColor>
#include <QCoreApplication>
#include <QByteArray>
#include <QElapsedTimer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QThread>
#include <QTemporaryDir>
#include <QVector3D>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>

#include <cmath>

#include <DebugOutput.h>
#include <Errors.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Win32Debug.hpp>
#endif

import Artifact.Layer.Abstract;
import Artifact.Layer.AdjustableLayer;
import Artifact.Application.Manager;
import Artifact.Effect.Glow;
import Artifact.Composition.InitParams;
import Artifact.Layer.Composition;
import Artifact.Layer.Image;
import Artifact.Layer.Light;
import Artifact.LOD.Manager;
import Artifact.Layer.Particle;
import Artifact.Layer.FormParticle;
import Artifact.Project.Manager;
import Artifact.Layer.Svg;
import Artifact.Layers.Model3D;
import Artifact.Export.PreRenderPipeline;
import Artifact.Service.Project;
import Artifact.Layer.Text;
import Artifact.Layer.Video;
import Artifact.Generator.Particle;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.CompositionViewDrawing;
import Artifact.Render.IRenderer;
import Artifact.Render.OffscreenComposition;
import Artifact.Widgets.CompositionRenderOverlay;
import Composition3D;
import Color.Float;
import Core.Light;
import Frame.Position;
import Memory.SharedPtr;
import Math.Vec;
import Utils.String.UniString;
import Utils.Id;

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 240;
std::atomic_bool diligentErrorDetected{false};

void layerImageDiligentMessageCallback(
    Diligent::DEBUG_MESSAGE_SEVERITY severity, const Diligent::Char* message,
    const Diligent::Char*, const Diligent::Char*, int)
{
    if (severity >= Diligent::DEBUG_MESSAGE_SEVERITY_ERROR) {
        diligentErrorDetected.store(true, std::memory_order_relaxed);
        std::fprintf(stderr, "[DiligentTestError] %s\n",
                     message ? message : "(empty Diligent error)");
    }
}

class LayerImageEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        diligentErrorDetected.store(false, std::memory_order_relaxed);
#ifdef _WIN32
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                     SEM_NOOPENFILEERRORBOX);
#endif
        Diligent::SetDebugMessageCallback(layerImageDiligentMessageCallback);
#ifdef _WIN32
        Diligent::WindowsDebug::SetBreakOnError(false);
#endif

        static int argc = 1;
        static char applicationName[] = "ArtifactModel3DAndLightLayerImageTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
        QCoreApplication::setApplicationName(
            QStringLiteral("ArtifactModel3DAndLightLayerImageTest"));
        QStandardPaths::setTestModeEnabled(true);

        deviceManager_ = std::make_unique<Artifact::DiligentDeviceManager>();
        deviceManager_->initializeHeadless();
        if (deviceManager_->device()) {
            renderer_ = std::make_unique<Artifact::OffscreenCompositionRenderer>(
                deviceManager_->device(), deviceManager_->immediateContext(),
                kWidth, kHeight);
            configureCamera();
        }
    }

    void TearDown() override
    {
        renderer_.reset();
        deviceManager_.reset();
        EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
            << "Diligent reported an error during the standalone image test";
        delete application_;
        application_ = nullptr;
    }

    bool available() const { return renderer_ != nullptr; }
    Artifact::ArtifactIRenderer* renderer() const
    {
        return renderer_ ? renderer_->renderer() : nullptr;
    }

    QImage render(Artifact::ArtifactAbstractLayer* layer)
    {
        if (!renderer_ || !layer) {
            return {};
        }
        renderer_->renderLayerFrame(ArtifactCore::FramePosition(0), layer);
        // Layer draws queue 3D gizmo primitives; the editor normally flushes
        // these at the end of its frame. This standalone target owns that
        // frame boundary explicitly before reading the render target back.
        renderer_->renderer()->flushGizmo3D();
        return renderer_->renderer()->readbackToImage();
    }

    void saveCapture(const QString& name, const QImage& image)
    {
        const QString root = qEnvironmentVariable(
            "ARTIFACT_MODEL3D_LIGHT_CAPTURE_DIR");
        if (root.isEmpty()) {
            return;
        }
        if (!QDir().mkpath(root)) {
            ADD_FAILURE() << "Could not create capture directory: "
                          << root.toStdString();
            return;
        }
        const QString path = QDir(root).filePath(name + QStringLiteral(".png"));
        EXPECT_TRUE(image.save(path, "PNG"))
            << "Could not save capture: " << path.toStdString();
    }

private:
    void configureCamera()
    {
        QMatrix4x4 view;
        view.lookAt(QVector3D(420.0f, 320.0f, 520.0f), QVector3D(0, 0, 0),
                    QVector3D(0, 1, 0));
        QMatrix4x4 projection;
        projection.perspective(42.0f,
                               static_cast<float>(kWidth) / kHeight,
                               1.0f, 5000.0f);
        auto* renderApi = renderer_->renderer();
        renderApi->set3DCameraMatrices(view, projection);
        renderApi->setGizmoCameraMatrices(view, projection);
    }

    QGuiApplication* application_ = nullptr;
    std::unique_ptr<Artifact::DiligentDeviceManager> deviceManager_;
    std::unique_ptr<Artifact::OffscreenCompositionRenderer> renderer_;
};

auto* const imageEnvironment = static_cast<LayerImageEnvironment*>(
    testing::AddGlobalTestEnvironment(new LayerImageEnvironment()));

TEST(DiligentDiagnosticsTest, ErrorCallbackCapturesDiagnosticAndContinues)
{
    EXPECT_FALSE(diligentErrorDetected.load(std::memory_order_relaxed))
        << "Diligent reported an error during standalone renderer startup";

    LOG_ERROR_MESSAGE("Synthetic diagnostic used to verify test capture");

    EXPECT_TRUE(diligentErrorDetected.exchange(false,
                                                std::memory_order_relaxed))
        << "The Diligent ERROR callback did not mark the test process";
}

int visiblePixelCount(const QImage& image)
{
    if (image.isNull()) {
        return 0;
    }
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                ++count;
            }
        }
    }
    return count;
}

int changedPixelCount(const QImage& first, const QImage& second)
{
    if (first.isNull() || second.isNull() || first.size() != second.size()) {
        return -1;
    }
    int count = 0;
    for (int y = 0; y < first.height(); ++y) {
        for (int x = 0; x < first.width(); ++x) {
            const QColor a = first.pixelColor(x, y);
            const QColor b = second.pixelColor(x, y);
            if (std::abs(a.red() - b.red()) > 3 ||
                std::abs(a.green() - b.green()) > 3 ||
                std::abs(a.blue() - b.blue()) > 3 ||
                std::abs(a.alpha() - b.alpha()) > 3) {
                ++count;
            }
        }
    }
    return count;
}

QVector3D averageVisibleColor(const QImage& image,
                              const QRect& region = QRect())
{
    if (image.isNull()) {
        return {};
    }
    std::array<double, 3> channelTotals{};
    int count = 0;
    const QRect bounds = region.isEmpty()
        ? image.rect()
        : region.intersected(image.rect());
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() <= 8) {
                continue;
            }
            channelTotals[0] += pixel.red();
            channelTotals[1] += pixel.green();
            channelTotals[2] += pixel.blue();
            ++count;
        }
    }
    if (count == 0) {
        return {};
    }
    return QVector3D(static_cast<float>(channelTotals[0] / count),
                     static_cast<float>(channelTotals[1] / count),
                     static_cast<float>(channelTotals[2] / count));
}

ArtifactCore::Light sceneLightForTest(
    const Artifact::ArtifactLightLayer& layer)
{
    ArtifactCore::Light sceneLight;
    switch (layer.lightType()) {
    case Artifact::LightType::Point:
        sceneLight.setType(ArtifactCore::LightType::Point);
        break;
    case Artifact::LightType::Spot:
        sceneLight.setType(ArtifactCore::LightType::Spot);
        break;
    case Artifact::LightType::Parallel:
        sceneLight.setType(ArtifactCore::LightType::Directional);
        break;
    case Artifact::LightType::Ambient:
        sceneLight.setType(ArtifactCore::LightType::Ambient);
        break;
    case Artifact::LightType::Area:
        sceneLight.setType(ArtifactCore::LightType::Area);
        break;
    }

    const auto color = layer.color();
    sceneLight.setColor(
        ArtifactCore::float3<float>{color.r(), color.g(), color.b()});
    sceneLight.setIntensity(layer.intensity().value / 100.0f);

    const QMatrix4x4 global = layer.getGlobalTransform4x4();
    const QVector3D position = global.map(QVector3D(0.0f, 0.0f, 0.0f));
    const QVector3D direction =
        global.mapVector(QVector3D(0.0f, 0.0f, 1.0f)).normalized();
    sceneLight.setPosition(ArtifactCore::Coordinates::WorldPoint3{
        position.x(), position.y(), position.z()});
    sceneLight.setDirection(ArtifactCore::Coordinates::WorldVector3{
        direction.x(), direction.y(), direction.z()});

    if (layer.lightType() == Artifact::LightType::Point) {
        sceneLight.setRange(layer.range());
    } else if (layer.lightType() == Artifact::LightType::Spot) {
        sceneLight.setRange(layer.coneLength());
        const auto outerHalfAngle = layer.coneAngle() * 0.5f;
        const ArtifactCore::Units::Degrees innerHalfAngle{
            std::max(0.0f,
                     (layer.coneAngle() - layer.coneFeather()).value) * 0.5f};
        sceneLight.setCutoff(innerHalfAngle, outerHalfAngle);
    } else if (layer.lightType() == Artifact::LightType::Area) {
        sceneLight.setAreaSize(layer.areaWidth(), layer.areaHeight());
        sceneLight.setAreaShape(static_cast<int>(layer.areaShape()));
    }
    return sceneLight;
}

void expectRedLightChangesModel(Artifact::Artifact3DLayer& model,
                                Artifact::ArtifactLightLayer& light,
                                const QString& captureName)
{
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);

    renderer->setSceneLights({});
    const QImage noLightImage = imageEnvironment->render(&model);
    renderer->setSceneLights({sceneLightForTest(light)});
    const QImage litImage = imageEnvironment->render(&model);
    imageEnvironment->saveCapture(captureName, litImage);

    const QRect frontFaceSample(150, 110, 20, 30);
    const QVector3D unlitColor =
        averageVisibleColor(noLightImage, frontFaceSample);
    const QVector3D litColor = averageVisibleColor(litImage, frontFaceSample);
    EXPECT_GT(visiblePixelCount(litImage), 250)
        << "The model disappeared when a scene light was enabled";
    EXPECT_GT(changedPixelCount(noLightImage, litImage), 100)
        << "Adding the scene light did not change the model image";
    EXPECT_GT(litColor.x(), unlitColor.x() + 30.0f)
        << "The red light did not increase the model's red channel; unlit="
        << unlitColor.x() << ", lit=" << litColor.x();
    EXPECT_GT(litColor.x(), litColor.y() * 1.4f)
        << "The model did not show the red light tint; lit RGB="
        << litColor.x() << ", " << litColor.y() << ", " << litColor.z();
}

bool rendererUnavailable()
{
    return !imageEnvironment->available();
}

} // namespace

TEST(Model3DLayerImageTest, DefaultCubeProducesVisiblePixels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }
    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.isVisible());
    ASSERT_TRUE(model.is3D());
    ASSERT_EQ(model.mesh().vertexCount(), 24);
    ASSERT_EQ(model.mesh().polygonCount(), 6);
    ASSERT_GT(model.opacity(), 0.0f);
    const QImage image = imageEnvironment->render(&model);

    imageEnvironment->saveCapture(QStringLiteral("model3d_default_cube"), image);
    EXPECT_EQ(image.size(), QSize(kWidth, kHeight));
    EXPECT_GT(visiblePixelCount(image), 250)
        << "The production 3D model layer did not produce a visible image";

    model.setRenderMode(Artifact::ModelRenderMode::Wireframe);
    const QImage wireframeImage = imageEnvironment->render(&model);
    imageEnvironment->saveCapture(
        QStringLiteral("model3d_default_cube_wireframe"), wireframeImage);
    EXPECT_GT(visiblePixelCount(wireframeImage), 100)
        << "The production 3D model layer did not produce a visible wireframe";
}

TEST(Model3DLayerImageTest, LowDetailDropsNormalMapWhileHighDetailKeepsIt)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage normalMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < normalMap.height(); ++y) {
        for (int x = 0; x < normalMap.width(); ++x) {
            normalMap.setPixelColor(
                x, y, x < normalMap.width() / 2 ? QColor(255, 128, 128, 255)
                                                 : QColor(128, 128, 255, 255));
        }
    }
    const QString normalMapPath =
        textureDirectory.filePath(QStringLiteral("normal.png"));
    ASSERT_TRUE(normalMap.save(normalMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.normalTexture"), normalMapPath));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.normalStrength"), 10.0f));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetailImage = imageEnvironment->render(&model);
    imageEnvironment->saveCapture(
        QStringLiteral("model3d_normal_map_high_lod"), highDetailImage);
    imageEnvironment->saveCapture(
        QStringLiteral("model3d_normal_map_low_lod"), lowDetailImage);

    EXPECT_GT(visiblePixelCount(highDetailImage), 250);
    EXPECT_GT(visiblePixelCount(mediumDetailImage), 250);
    EXPECT_GT(visiblePixelCount(lowDetailImage), 250);
    EXPECT_GT(visiblePixelCount(unknownDetailImage), 250);
    EXPECT_EQ(changedPixelCount(highDetailImage, mediumDetailImage), 0)
        << "Medium detail should retain the normal map like High detail";
    EXPECT_GT(changedPixelCount(highDetailImage, lowDetailImage), 20)
        << "Low detail should omit the normal map contribution";
    EXPECT_EQ(changedPixelCount(highDetailImage, unknownDetailImage), 0)
        << "unknown detail values should preserve the full material inputs";
    EXPECT_EQ(changedPixelCount(highDetailImage, restoredHighDetailImage), 0)
        << "switching back from Low should restore the original High detail material";
}

TEST(Model3DLayerImageTest, LowDetailDropsOcclusionMapWhileMediumAndHighKeepIt)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage occlusionMap(16, 16, QImage::Format_RGBA8888);
    occlusionMap.fill(QColor(0, 0, 0, 255));
    const QString occlusionMapPath =
        textureDirectory.filePath(QStringLiteral("occlusion.png"));
    ASSERT_TRUE(occlusionMap.save(occlusionMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.occlusionTexture"), occlusionMapPath));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.occlusionStrength"), 1.0f));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(
        static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highAfterUnknown = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetailImage.isNull());
    ASSERT_FALSE(mediumDetailImage.isNull());
    ASSERT_FALSE(lowDetailImage.isNull());
    ASSERT_FALSE(restoredHighDetailImage.isNull());
    ASSERT_FALSE(unknownDetailImage.isNull());
    ASSERT_FALSE(highAfterUnknown.isNull());
    EXPECT_GT(visiblePixelCount(highDetailImage), 250);
    EXPECT_GT(visiblePixelCount(mediumDetailImage), 250);
    EXPECT_GT(visiblePixelCount(lowDetailImage), 250);
    EXPECT_EQ(changedPixelCount(highDetailImage, mediumDetailImage), 0)
        << "Medium detail should retain the occlusion map like High detail";
    EXPECT_GT(changedPixelCount(highDetailImage, lowDetailImage), 20)
        << "Low detail should omit the occlusion map contribution";
    EXPECT_EQ(changedPixelCount(highDetailImage, restoredHighDetailImage), 0)
        << "switching back from Low should restore the original High detail material";
    EXPECT_EQ(changedPixelCount(highDetailImage, unknownDetailImage), 0)
        << "unknown material LOD should retain the occlusion map";
    EXPECT_EQ(changedPixelCount(highDetailImage, highAfterUnknown), 0)
        << "returning from unknown LOD should restore the original High material";
}

TEST(Model3DLayerImageTest, LowDetailPreservesBaseMaterialOpacity)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.opacity"), 0.45f));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetail = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 250);
    ASSERT_FALSE(mediumDetail.isNull());
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Medium material LOD should preserve base opacity and color";
    EXPECT_GT(visiblePixelCount(lowDetail), 250);
    EXPECT_EQ(changedPixelCount(highDetail, lowDetail), 0)
        << "Low material LOD should preserve base opacity and color";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHighDetail), 0)
        << "High material output should be stable after returning from Low";
}

TEST(Model3DLayerImageTest, LowDetailPreservesMaskedOpacityTexture)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage opacityMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < opacityMap.height(); ++y) {
        for (int x = 0; x < opacityMap.width(); ++x) {
            opacityMap.setPixelColor(
                x, y, x < opacityMap.width() / 2
                    ? QColor(255, 255, 255, 0)
                    : QColor(255, 255, 255, 255));
        }
    }
    const QString opacityMapPath =
        textureDirectory.filePath(QStringLiteral("opacity.png"));
    ASSERT_TRUE(opacityMap.save(opacityMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.opacityTexture"), opacityMapPath));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.alphaMode"), 1)); // Masked
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.alphaCutoff"), 0.5f));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetail = imageEnvironment->render(&model);
    const QImage highAfterUnknown = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    ASSERT_FALSE(highAfterUnknown.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 100);
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Medium material LOD must preserve alpha masking and its opacity map";
    EXPECT_GT(visiblePixelCount(lowDetail), 100);
    EXPECT_EQ(changedPixelCount(highDetail, lowDetail), 0)
        << "Low material LOD must preserve alpha masking and its opacity map";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHighDetail), 0)
        << "High alpha-masked output should remain stable after returning from Low";
}

TEST(Model3DLayerImageTest,
     MinifiedOpacityTextureUsesMipAlphaIndependentOfMaterialLOD)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage checker(128, 128, QImage::Format_RGBA8888);
    for (int y = 0; y < checker.height(); ++y) {
        for (int x = 0; x < checker.width(); ++x) {
            checker.setPixelColor(x, y, ((x + y) & 1) == 0
                ? QColor(255, 255, 255, 0)
                : QColor(255, 255, 255, 255));
        }
    }
    QImage averageAlpha(1, 1, QImage::Format_RGBA8888);
    averageAlpha.fill(QColor(255, 255, 255, 128));
    const QString checkerPath =
        textureDirectory.filePath(QStringLiteral("minified-opacity-checker.png"));
    const QString averagePath =
        textureDirectory.filePath(QStringLiteral("average-opacity.png"));
    ASSERT_TRUE(checker.save(checkerPath));
    ASSERT_TRUE(averageAlpha.save(averagePath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.opacityTexture"), checkerPath));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.alphaMode"), 2)); // Blended

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailChecker = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailChecker = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.opacityTexture"), averagePath));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailAverage = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetailChecker.isNull());
    ASSERT_FALSE(lowDetailChecker.isNull());
    ASSERT_FALSE(highDetailAverage.isNull());
    EXPECT_GT(visiblePixelCount(highDetailChecker), 250);
    EXPECT_EQ(changedPixelCount(highDetailChecker, lowDetailChecker), 0)
        << "Material LOD should preserve opacity-map mip selection";
    EXPECT_LT(changedPixelCount(highDetailChecker, highDetailAverage), 50)
        << "minified opacity should approach the average-alpha mip result";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest, LowDetailPreservesEmissionTexture)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage emissionMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < emissionMap.height(); ++y) {
        for (int x = 0; x < emissionMap.width(); ++x) {
            emissionMap.setPixelColor(
                x, y, x < emissionMap.width() / 2
                    ? QColor(255, 32, 16, 255)
                    : QColor(16, 64, 255, 255));
        }
    }
    const QString emissionMapPath =
        textureDirectory.filePath(QStringLiteral("emission.png"));
    ASSERT_TRUE(emissionMap.save(emissionMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.emissionTexture"), emissionMapPath));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.emission.color"), QColor(255, 255, 255)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.emissionStrength"), 8.0f));

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage restoredMediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(
        static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highAfterUnknown = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(restoredMediumDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(highAfterUnknown.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 250);
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Medium material LOD should preserve emission texture contribution";
    EXPECT_GT(visiblePixelCount(lowDetail), 250);
    EXPECT_EQ(changedPixelCount(highDetail, lowDetail), 0)
        << "Low material LOD should preserve emission texture contribution";
    EXPECT_EQ(changedPixelCount(mediumDetail, restoredMediumDetail), 0)
        << "Returning from Low should reproduce the Medium emission output";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHighDetail), 0)
        << "High emission output should remain stable after returning from Low";
    EXPECT_EQ(changedPixelCount(highDetail, unknownDetail), 0)
        << "unknown material LOD should keep the full-quality texture path";
    EXPECT_EQ(changedPixelCount(highDetail, highAfterUnknown), 0)
        << "returning from an unknown material LOD should reproduce High";
}

TEST(Model3DLayerImageTest, LowDetailPreservesRoughnessTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage roughnessMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < roughnessMap.height(); ++y) {
        for (int x = 0; x < roughnessMap.width(); ++x) {
            const int roughness = x < roughnessMap.width() / 2 ? 12 : 245;
            roughnessMap.setPixelColor(
                x, y, QColor(roughness, roughness, roughness, 255));
        }
    }
    const QString roughnessMapPath =
        textureDirectory.filePath(QStringLiteral("roughness-lod.png"));
    ASSERT_TRUE(roughnessMap.save(roughnessMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), 0.35f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughnessTexture"), roughnessMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughnessTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughnessTexture"), roughnessMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "The roughness-map fixture must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain the roughness map contribution";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "Restoring the roughness map after LOD changes should reproduce High";
}

TEST(Model3DLayerImageTest, LowDetailPreservesBaseColorTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage baseColorMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < baseColorMap.height(); ++y) {
        for (int x = 0; x < baseColorMap.width(); ++x) {
            baseColorMap.setPixelColor(
                x, y, x < baseColorMap.width() / 2
                    ? QColor(255, 24, 16, 255)
                    : QColor(16, 48, 255, 255));
        }
    }
    const QString baseColorMapPath =
        textureDirectory.filePath(QStringLiteral("base-color-lod.png"));
    ASSERT_TRUE(baseColorMap.save(baseColorMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColor"), QColor(Qt::white)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColorTexture"), baseColorMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColorTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColorTexture"), baseColorMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(mediumWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(unknownWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "The base-color map fixture must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, mediumWithMap), 0)
        << "Medium material LOD should preserve the base-color map";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should preserve the base-color map";
    EXPECT_EQ(changedPixelCount(highWithMap, unknownWithMap), 0)
        << "unknown material LOD should preserve the base-color map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "Restoring the base-color map after LOD changes should reproduce High";
}

TEST(Model3DLayerImageTest,
     MinifiedBaseColorTextureUsesMipFilteringIndependentOfMaterialLOD)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage checker(128, 128, QImage::Format_RGBA8888);
    for (int y = 0; y < checker.height(); ++y) {
        for (int x = 0; x < checker.width(); ++x) {
            checker.setPixelColor(x, y, ((x + y) & 1) == 0
                ? QColor(0, 0, 0, 255) : QColor(255, 255, 255, 255));
        }
    }
    QImage linearAverage(1, 1, QImage::Format_RGBA8888);
    linearAverage.fill(QColor(188, 188, 188, 255));
    const QString checkerPath =
        textureDirectory.filePath(QStringLiteral("minified-checker.png"));
    const QString averagePath =
        textureDirectory.filePath(QStringLiteral("linear-average.png"));
    ASSERT_TRUE(checker.save(checkerPath));
    ASSERT_TRUE(linearAverage.save(averagePath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColor"), QColor(Qt::white)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColorTexture"), checkerPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailChecker = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailChecker = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColorTexture"), averagePath));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailAverage = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetailChecker.isNull());
    ASSERT_FALSE(lowDetailChecker.isNull());
    ASSERT_FALSE(highDetailAverage.isNull());
    EXPECT_GT(visiblePixelCount(highDetailChecker), 250);
    EXPECT_EQ(changedPixelCount(highDetailChecker, lowDetailChecker), 0)
        << "Material LOD should not change texture mip selection";
    const QVector3D checkerAverage = averageVisibleColor(highDetailChecker);
    const QVector3D mipReferenceAverage = averageVisibleColor(highDetailAverage);
    EXPECT_NEAR(checkerAverage.x(), mipReferenceAverage.x(), 18.0f)
        << "minified sRGB checker red should approach its linear-light mip average";
    EXPECT_NEAR(checkerAverage.y(), mipReferenceAverage.y(), 18.0f)
        << "minified sRGB checker green should approach its linear-light mip average";
    EXPECT_NEAR(checkerAverage.z(), mipReferenceAverage.z(), 18.0f)
        << "minified sRGB checker blue should approach its linear-light mip average";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     MinifiedMetallicRoughnessTextureUsesMipFilteringIndependentOfMaterialLOD)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage checker(128, 128, QImage::Format_RGBA8888);
    for (int y = 0; y < checker.height(); ++y) {
        for (int x = 0; x < checker.width(); ++x) {
            const bool highMetal = ((x + y) & 1) != 0;
            checker.setPixelColor(
                x, y, QColor(255, highMetal ? 0 : 255,
                             highMetal ? 255 : 0, 255));
        }
    }
    QImage linearAverage(1, 1, QImage::Format_RGBA8888);
    linearAverage.fill(QColor(255, 128, 128, 255));
    const QString checkerPath =
        textureDirectory.filePath(QStringLiteral("metallic-roughness-checker.png"));
    const QString averagePath =
        textureDirectory.filePath(QStringLiteral("metallic-roughness-average.png"));
    ASSERT_TRUE(checker.save(checkerPath));
    ASSERT_TRUE(linearAverage.save(averagePath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColor"), QColor(180, 135, 90)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallic"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), 0.7f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicRoughnessTexture"), checkerPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailChecker = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailChecker = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicRoughnessTexture"), averagePath));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailAverage = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetailChecker.isNull());
    ASSERT_FALSE(lowDetailChecker.isNull());
    ASSERT_FALSE(highDetailAverage.isNull());
    EXPECT_GT(visiblePixelCount(highDetailChecker), 250);
    EXPECT_EQ(changedPixelCount(highDetailChecker, lowDetailChecker), 0)
        << "Material LOD should not change metallic-roughness mip selection";
    const QVector3D checkerAverage = averageVisibleColor(highDetailChecker);
    const QVector3D mipReferenceAverage = averageVisibleColor(highDetailAverage);
    EXPECT_NEAR(checkerAverage.x(), mipReferenceAverage.x(), 18.0f)
        << "minified metallic-roughness metallic should approach its linear mip average";
    EXPECT_NEAR(checkerAverage.y(), mipReferenceAverage.y(), 18.0f)
        << "minified metallic-roughness roughness should approach its linear mip average";
    EXPECT_NEAR(checkerAverage.z(), mipReferenceAverage.z(), 18.0f)
        << "minified metallic-roughness should approach its linear mip average";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     MinifiedNonPowerOfTwoNormalTextureUsesVarianceFilteredMips)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage checker(127, 95, QImage::Format_RGBA8888);
    for (int y = 0; y < checker.height(); ++y) {
        for (int x = 0; x < checker.width(); ++x) {
            checker.setPixelColor(
                x, y, QColor(((x + y) & 1) != 0 ? 204 : 51, 128, 230, 255));
        }
    }
    QImage averageDirection(1, 1, QImage::Format_RGBA8888);
    averageDirection.fill(QColor(128, 128, 255, 255));
    const QString checkerPath =
        textureDirectory.filePath(QStringLiteral("normal-minified-checker.png"));
    const QString averagePath =
        textureDirectory.filePath(QStringLiteral("normal-average-direction.png"));
    ASSERT_TRUE(checker.save(checkerPath));
    ASSERT_TRUE(averageDirection.save(averagePath));

    Artifact::Artifact3DLayer model;
    constexpr float authoredRoughness = 0.5f;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.baseColor"), QColor(180, 135, 90)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), authoredRoughness));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.normalStrength"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.normalTexture"), checkerPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailChecker = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailChecker = imageEnvironment->render(&model);
    const float averageNormalLength = 205.0f / 255.0f;
    const float normalVariance =
        (1.0f - averageNormalLength) / averageNormalLength;
    const float alpha = authoredRoughness * authoredRoughness;
    const float filteredRoughness = std::sqrt(std::sqrt(
        alpha * alpha + std::min(2.0f * normalVariance, 0.2f)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), filteredRoughness));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.normalTexture"), averagePath));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailAverage = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetailChecker.isNull());
    ASSERT_FALSE(lowDetailChecker.isNull());
    ASSERT_FALSE(highDetailAverage.isNull());
    EXPECT_GT(visiblePixelCount(highDetailChecker), 250);
    EXPECT_EQ(changedPixelCount(highDetailChecker, lowDetailChecker), 0)
        << "Material LOD should not change normal-map mip selection";
    const QVector3D checkerAverage = averageVisibleColor(highDetailChecker);
    const QVector3D mipReferenceAverage = averageVisibleColor(highDetailAverage);
    EXPECT_NEAR(checkerAverage.x(), mipReferenceAverage.x(), 20.0f)
        << "minified normal mips should renormalize the average direction";
    EXPECT_NEAR(checkerAverage.y(), mipReferenceAverage.y(), 20.0f)
        << "minified normal variance should filter the material roughness";
    EXPECT_NEAR(checkerAverage.z(), mipReferenceAverage.z(), 20.0f)
        << "normal-mip variance filtering should match its flat-map reference";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest, LowDetailPreservesMetallicTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage metallicMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < metallicMap.height(); ++y) {
        for (int x = 0; x < metallicMap.width(); ++x) {
            const int metallic = x < metallicMap.width() / 2 ? 0 : 255;
            metallicMap.setPixelColor(
                x, y, QColor(metallic, metallic, metallic, 255));
        }
    }
    const QString metallicMapPath =
        textureDirectory.filePath(QStringLiteral("metallic-lod.png"));
    ASSERT_TRUE(metallicMap.save(metallicMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallic"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), 0.35f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicTexture"), metallicMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicTexture"), metallicMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(mediumWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(unknownWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "The metallic map fixture must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, mediumWithMap), 0)
        << "Medium material LOD should retain the metallic map";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain the metallic map";
    EXPECT_EQ(changedPixelCount(highWithMap, unknownWithMap), 0)
        << "unknown material LOD should retain the full-quality metallic map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "restoring the metallic map should reproduce High after LOD changes";
}

TEST(Model3DLayerImageTest,
     LowDetailPreservesMetallicRoughnessTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage metallicRoughnessMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < metallicRoughnessMap.height(); ++y) {
        for (int x = 0; x < metallicRoughnessMap.width(); ++x) {
            const bool smoothMetal = x >= metallicRoughnessMap.width() / 2;
            metallicRoughnessMap.setPixelColor(
                x, y, smoothMetal ? QColor(0, 10, 255, 255)
                                  : QColor(0, 245, 0, 255));
        }
    }
    const QString metallicRoughnessMapPath =
        textureDirectory.filePath(QStringLiteral("metallic-roughness-lod.png"));
    ASSERT_TRUE(metallicRoughnessMap.save(metallicRoughnessMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallic"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), 0.5f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicRoughnessTexture"),
        metallicRoughnessMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicRoughnessTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallicRoughnessTexture"),
        metallicRoughnessMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "The metallic-roughness-map fixture must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain the packed metallic-roughness map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "Restoring the packed map after LOD changes should reproduce High";
}

TEST(Model3DLayerImageTest,
     LowDetailPreservesOpacityAcrossImportedMaterialSlots)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir fixtureDirectory;
    ASSERT_TRUE(fixtureDirectory.isValid());
    const QString materialPath =
        fixtureDirectory.filePath(QStringLiteral("lod-slots.mtl"));
    QFile materialFile(materialPath);
    ASSERT_TRUE(materialFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(materialFile.write(
                  "newmtl opaque_slot\nKd 0.9 0.2 0.1\nd 1.0\n"
                  "newmtl transparent_slot\nKd 0.1 0.3 0.9\nd 0.45\n"),
              0);
    materialFile.close();

    const QString modelPath =
        fixtureDirectory.filePath(QStringLiteral("lod-slots.obj"));
    QFile modelFile(modelPath);
    ASSERT_TRUE(modelFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(modelFile.write(
                  "mtllib lod-slots.mtl\n"
                  "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                  "usemtl opaque_slot\nf 1/1 2/2 3/3\n"
                  "usemtl transparent_slot\nf 1/1 3/3 4/4\n"),
              0);
    modelFile.close();

    Artifact::Artifact3DLayer model;
    model.loadFromFile(modelPath);
    ASSERT_GT(model.mesh().materialSlotCount(), 1);
    ASSERT_GT(model.meshMaterials().size(), 1u);
    EXPECT_NE(model.meshMaterials()[0].opacity(),
              model.meshMaterials()[1].opacity())
        << "The OBJ fixture must exercise distinct imported slot opacities";

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetail = imageEnvironment->render(&model);
    const QImage highAfterUnknown = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    ASSERT_FALSE(highAfterUnknown.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 100);
    EXPECT_GT(visiblePixelCount(mediumDetail), 100);
    EXPECT_GT(visiblePixelCount(lowDetail), 100);
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Medium material LOD should preserve each imported slot's base opacity";
    EXPECT_EQ(changedPixelCount(highDetail, lowDetail), 0)
        << "Low material LOD should preserve each imported slot's base opacity";
    EXPECT_EQ(changedPixelCount(highDetail, unknownDetail), 0)
        << "unknown material LOD should preserve every imported slot";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHighDetail), 0)
        << "Returning to High should restore imported material-slot output";
    EXPECT_EQ(changedPixelCount(highDetail, highAfterUnknown), 0)
        << "returning from unknown LOD should reproduce imported material slots";
}

TEST(Model3DLayerImageTest,
     LowDetailDropsNormalMapsAcrossImportedMaterialSlots)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir fixtureDirectory;
    ASSERT_TRUE(fixtureDirectory.isValid());
    QImage normalMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < normalMap.height(); ++y) {
        for (int x = 0; x < normalMap.width(); ++x) {
            normalMap.setPixelColor(
                x, y, x < normalMap.width() / 2
                    ? QColor(255, 128, 128, 255)
                    : QColor(128, 128, 255, 255));
        }
    }
    const QString normalMapPath =
        fixtureDirectory.filePath(QStringLiteral("slot-normal.png"));
    ASSERT_TRUE(normalMap.save(normalMapPath));

    const QString materialPath =
        fixtureDirectory.filePath(QStringLiteral("normal-lod-slots.mtl"));
    QFile materialFile(materialPath);
    ASSERT_TRUE(materialFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(materialFile.write(
                  "newmtl left_slot\nKd 0.8 0.3 0.2\nmap_Bump slot-normal.png\n"
                  "newmtl right_slot\nKd 0.2 0.4 0.9\nmap_Bump slot-normal.png\n"),
              0);
    materialFile.close();

    const QString modelPath =
        fixtureDirectory.filePath(QStringLiteral("normal-lod-slots.obj"));
    QFile modelFile(modelPath);
    ASSERT_TRUE(modelFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(modelFile.write(
                  "mtllib normal-lod-slots.mtl\n"
                  "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                  "usemtl left_slot\nf 1/1 2/2 3/3\n"
                  "usemtl right_slot\nf 1/1 3/3 4/4\n"),
              0);
    modelFile.close();

    Artifact::Artifact3DLayer model;
    model.loadFromFile(modelPath);
    ASSERT_GT(model.mesh().materialSlotCount(), 1);
    ASSERT_GT(model.meshMaterials().size(), 1u);
    ASSERT_FALSE(model.meshMaterials()[0].normalTexture().toQString().isEmpty());
    ASSERT_FALSE(model.meshMaterials()[1].normalTexture().toQString().isEmpty());

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHigh = imageEnvironment->render(&model);
    const QImage highAfterUnknown = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    ASSERT_FALSE(highAfterUnknown.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 100);
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Medium material LOD should retain each slot's normal map";
    EXPECT_GT(changedPixelCount(highDetail, lowDetail), 20)
        << "Low material LOD should drop normal maps from imported slots";
    EXPECT_EQ(changedPixelCount(highDetail, unknownDetail), 0)
        << "unknown material LOD should retain every slot's normal map";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHigh), 0)
        << "returning to High should restore imported slot normal maps";
    EXPECT_EQ(changedPixelCount(highDetail, highAfterUnknown), 0)
        << "returning from unknown LOD should restore imported slot normal maps";
}

TEST(Model3DLayerImageTest,
     LowDetailDropsOcclusionMapsAcrossImportedMaterialSlots)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir fixtureDirectory;
    ASSERT_TRUE(fixtureDirectory.isValid());
    QImage occlusionMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < occlusionMap.height(); ++y) {
        for (int x = 0; x < occlusionMap.width(); ++x) {
            const int value = x < occlusionMap.width() / 2 ? 12 : 255;
            occlusionMap.setPixelColor(
                x, y, QColor(value, value, value, 255));
        }
    }
    const QString occlusionMapPath =
        fixtureDirectory.filePath(QStringLiteral("slot-occlusion.png"));
    ASSERT_TRUE(occlusionMap.save(occlusionMapPath));

    const QString materialPath =
        fixtureDirectory.filePath(QStringLiteral("occlusion-lod-slots.mtl"));
    QFile materialFile(materialPath);
    ASSERT_TRUE(materialFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(materialFile.write(
                  "newmtl left_slot\nKd 0.8 0.3 0.2\n"
                  "newmtl right_slot\nKd 0.2 0.4 0.9\n"),
              0);
    materialFile.close();

    const QString modelPath =
        fixtureDirectory.filePath(QStringLiteral("occlusion-lod-slots.obj"));
    QFile modelFile(modelPath);
    ASSERT_TRUE(modelFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(modelFile.write(
                  "mtllib occlusion-lod-slots.mtl\n"
                  "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                  "usemtl left_slot\nf 1/1 2/2 3/3\n"
                  "usemtl right_slot\nf 1/1 3/3 4/4\n"),
              0);
    modelFile.close();

    Artifact::Artifact3DLayer model;
    model.loadFromFile(modelPath);
    ASSERT_GT(model.mesh().materialSlotCount(), 1);
    ASSERT_GT(model.meshMaterials().size(), 1u);

    QJsonObject layerJson = model.toJson();
    QJsonArray materialSlots =
        layerJson.value(QStringLiteral("material.slots")).toArray();
    ASSERT_GT(materialSlots.size(), 1);
    for (int index = 0; index < materialSlots.size(); ++index) {
        QJsonObject slot = materialSlots[index].toObject();
        slot.insert(QStringLiteral("occlusionTexture"), occlusionMapPath);
        materialSlots[index] = slot;
    }
    layerJson.insert(QStringLiteral("material.slots"), materialSlots);
    model.fromJsonProperties(layerJson);
    ASSERT_FALSE(model.meshMaterials()[0].occlusionTexture().toQString().isEmpty());
    ASSERT_FALSE(model.meshMaterials()[1].occlusionTexture().toQString().isEmpty());

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(
        static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownDetail = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHigh = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 100);
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Medium material LOD should retain both imported slot occlusion maps";
    EXPECT_GT(changedPixelCount(highDetail, lowDetail), 20)
        << "Low material LOD should drop occlusion maps from imported slots";
    EXPECT_EQ(changedPixelCount(highDetail, unknownDetail), 0)
        << "unknown material LOD should retain both slot occlusion maps";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHigh), 0)
        << "returning to High should restore imported slot occlusion maps";

    QImage clearcoatNormalMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < clearcoatNormalMap.height(); ++y) {
        for (int x = 0; x < clearcoatNormalMap.width(); ++x) {
            clearcoatNormalMap.setPixelColor(
                x, y, x < clearcoatNormalMap.width() / 2
                    ? QColor(255, 128, 128, 255)
                    : QColor(128, 128, 255, 255));
        }
    }
    const QString clearcoatNormalPath =
        fixtureDirectory.filePath(QStringLiteral("slot-clearcoat-normal.png"));
    ASSERT_TRUE(clearcoatNormalMap.save(clearcoatNormalPath));

    layerJson = model.toJson();
    materialSlots = layerJson.value(QStringLiteral("material.slots")).toArray();
    ASSERT_GT(materialSlots.size(), 1);
    for (int index = 0; index < materialSlots.size(); ++index) {
        QJsonObject slot = materialSlots[index].toObject();
        slot.remove(QStringLiteral("occlusionTexture"));
        slot.insert(QStringLiteral("clearcoatNormalTexture"), clearcoatNormalPath);
        slot.insert(QStringLiteral("clearcoat"), 1.0);
        slot.insert(QStringLiteral("clearcoatNormalStrength"), 1.0);
        materialSlots[index] = slot;
    }
    layerJson.insert(QStringLiteral("material.slots"), materialSlots);
    model.fromJsonProperties(layerJson);
    ASSERT_FALSE(model.meshMaterials()[0].clearcoatNormalTexture()
                     .toQString().isEmpty());
    ASSERT_FALSE(model.meshMaterials()[1].clearcoatNormalTexture()
                     .toQString().isEmpty());

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highClearcoatNormal = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumClearcoatNormal = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowClearcoatNormal = imageEnvironment->render(&model);
    renderer->setDetailLevel(
        static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownClearcoatNormal = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighClearcoatNormal = imageEnvironment->render(&model);

    ASSERT_FALSE(highClearcoatNormal.isNull());
    ASSERT_FALSE(mediumClearcoatNormal.isNull());
    ASSERT_FALSE(lowClearcoatNormal.isNull());
    ASSERT_FALSE(unknownClearcoatNormal.isNull());
    ASSERT_FALSE(restoredHighClearcoatNormal.isNull());
    EXPECT_EQ(changedPixelCount(highClearcoatNormal, mediumClearcoatNormal), 0)
        << "Medium material LOD should retain both slot clearcoat normal maps";
    EXPECT_GT(changedPixelCount(highClearcoatNormal, lowClearcoatNormal), 20)
        << "Low material LOD should drop clearcoat normal maps from both slots";
    EXPECT_EQ(changedPixelCount(highClearcoatNormal, unknownClearcoatNormal), 0)
        << "unknown material LOD should retain both slot clearcoat normal maps";
    EXPECT_EQ(changedPixelCount(highClearcoatNormal,
                                restoredHighClearcoatNormal), 0)
        << "returning to High should restore both slot clearcoat normal maps";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     LowDetailPreservesBaseColorTexturesAcrossImportedMaterialSlots)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir fixtureDirectory;
    ASSERT_TRUE(fixtureDirectory.isValid());
    QImage leftTexture(8, 8, QImage::Format_RGBA8888);
    leftTexture.fill(QColor(255, 24, 16, 255));
    QImage rightTexture(8, 8, QImage::Format_RGBA8888);
    rightTexture.fill(QColor(16, 48, 255, 255));
    ASSERT_TRUE(leftTexture.save(fixtureDirectory.filePath(
        QStringLiteral("left-base-color.png"))));
    ASSERT_TRUE(rightTexture.save(fixtureDirectory.filePath(
        QStringLiteral("right-base-color.png"))));

    const QString materialPath =
        fixtureDirectory.filePath(QStringLiteral("base-color-lod-slots.mtl"));
    QFile materialFile(materialPath);
    ASSERT_TRUE(materialFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(materialFile.write(
                  "newmtl left_slot\nKd 1 1 1\nmap_Kd left-base-color.png\n"
                  "newmtl right_slot\nKd 1 1 1\nmap_Kd right-base-color.png\n"),
              0);
    materialFile.close();

    const QString modelPath =
        fixtureDirectory.filePath(QStringLiteral("base-color-lod-slots.obj"));
    QFile modelFile(modelPath);
    ASSERT_TRUE(modelFile.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_GT(modelFile.write(
                  "mtllib base-color-lod-slots.mtl\n"
                  "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                  "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                  "usemtl left_slot\nf 1/1 2/2 3/3\n"
                  "usemtl right_slot\nf 1/1 3/3 4/4\n"),
              0);
    modelFile.close();

    Artifact::Artifact3DLayer model;
    model.loadFromFile(modelPath);
    ASSERT_GT(model.mesh().materialSlotCount(), 1);
    ASSERT_GT(model.meshMaterials().size(), 1u);
    ASSERT_FALSE(model.meshMaterials()[0].baseColorTexture().toQString().isEmpty());
    ASSERT_FALSE(model.meshMaterials()[1].baseColorTexture().toQString().isEmpty());

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMaps = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMaps = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMaps = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMaps = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighWithMaps = imageEnvironment->render(&model);
    ASSERT_FALSE(highWithMaps.isNull());
    ASSERT_FALSE(mediumWithMaps.isNull());
    ASSERT_FALSE(lowWithMaps.isNull());
    ASSERT_FALSE(unknownWithMaps.isNull());
    ASSERT_FALSE(restoredHighWithMaps.isNull());
    EXPECT_GT(visiblePixelCount(highWithMaps), 100);
    EXPECT_EQ(changedPixelCount(highWithMaps, mediumWithMaps), 0)
        << "Medium material LOD should preserve both imported base-color maps";
    EXPECT_EQ(changedPixelCount(highWithMaps, lowWithMaps), 0)
        << "Low material LOD should preserve both imported base-color maps";
    EXPECT_EQ(changedPixelCount(highWithMaps, unknownWithMaps), 0)
        << "unknown material LOD should preserve every imported base-color map";
    EXPECT_EQ(changedPixelCount(highWithMaps, restoredHighWithMaps), 0)
        << "returning to High after cycling LODs should reproduce every "
           "imported base-color material slot";
}

TEST(Model3DLayerImageTest,
     LowDetailDropsClearcoatNormalMapWhileMediumAndHighKeepIt)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage clearcoatNormalMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < clearcoatNormalMap.height(); ++y) {
        for (int x = 0; x < clearcoatNormalMap.width(); ++x) {
            clearcoatNormalMap.setPixelColor(
                x, y, x < clearcoatNormalMap.width() / 2
                    ? QColor(255, 128, 128, 255)
                    : QColor(128, 128, 255, 255));
        }
    }
    const QString clearcoatNormalPath =
        textureDirectory.filePath(QStringLiteral("clearcoat-normal.png"));
    ASSERT_TRUE(clearcoatNormalMap.save(clearcoatNormalPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoatNormalTexture"), clearcoatNormalPath));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoat"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoatRoughness"), 0.04f));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowDetailImage = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage restoredHighDetailImage = imageEnvironment->render(&model);

    ASSERT_FALSE(highDetailImage.isNull());
    ASSERT_FALSE(mediumDetailImage.isNull());
    ASSERT_FALSE(lowDetailImage.isNull());
    ASSERT_FALSE(restoredHighDetailImage.isNull());
    EXPECT_GT(visiblePixelCount(highDetailImage), 250);
    EXPECT_GT(visiblePixelCount(mediumDetailImage), 250);
    EXPECT_GT(visiblePixelCount(lowDetailImage), 250);
    EXPECT_EQ(changedPixelCount(highDetailImage, mediumDetailImage), 0)
        << "Medium detail should retain the clearcoat normal map like High detail";
    EXPECT_GT(changedPixelCount(highDetailImage, lowDetailImage), 20)
        << "Low detail should omit the clearcoat normal map contribution";
    EXPECT_EQ(changedPixelCount(highDetailImage, restoredHighDetailImage), 0)
        << "switching back from Low should restore the original High detail material";
}

TEST(Model3DLayerImageTest,
     LowDetailPreservesClearcoatRoughnessTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage clearcoatRoughnessMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < clearcoatRoughnessMap.height(); ++y) {
        for (int x = 0; x < clearcoatRoughnessMap.width(); ++x) {
            const int roughness = x < clearcoatRoughnessMap.width() / 2
                ? 20 : 235;
            clearcoatRoughnessMap.setPixelColor(
                x, y, QColor(roughness, roughness, roughness, 255));
        }
    }
    const QString clearcoatRoughnessMapPath =
        textureDirectory.filePath(QStringLiteral("clearcoat-roughness.png"));
    ASSERT_TRUE(clearcoatRoughnessMap.save(clearcoatRoughnessMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoat"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoatRoughness"), 0.8f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoatRoughnessTexture"),
        clearcoatRoughnessMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoatRoughnessTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.clearcoatRoughnessTexture"),
        clearcoatRoughnessMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(mediumWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(unknownWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "the clearcoat roughness map must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, mediumWithMap), 0)
        << "Medium material LOD should retain clearcoat roughness";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain clearcoat roughness";
    EXPECT_EQ(changedPixelCount(highWithMap, unknownWithMap), 0)
        << "unknown material LOD should retain the full-quality map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "restoring the map should reproduce High after LOD changes";
}

TEST(Model3DLayerImageTest, LowDetailPreservesSheenColorTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage sheenColorMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < sheenColorMap.height(); ++y) {
        for (int x = 0; x < sheenColorMap.width(); ++x) {
            sheenColorMap.setPixelColor(
                x, y, x < sheenColorMap.width() / 2
                    ? QColor(255, 24, 12, 255)
                    : QColor(12, 48, 255, 255));
        }
    }
    const QString sheenColorMapPath =
        textureDirectory.filePath(QStringLiteral("sheen-color.png"));
    ASSERT_TRUE(sheenColorMap.save(sheenColorMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheen"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenRoughness"), 0.5f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenTint"), QColor(255, 255, 255)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenColorTexture"), sheenColorMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenColorTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenColorTexture"), sheenColorMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(mediumWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(unknownWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "the sheen color map must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, mediumWithMap), 0)
        << "Medium material LOD should retain the sheen color map";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain the sheen color map";
    EXPECT_EQ(changedPixelCount(highWithMap, unknownWithMap), 0)
        << "unknown material LOD should retain the full-quality sheen map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "restoring the sheen map should reproduce High after LOD changes";
}

TEST(Model3DLayerImageTest, LowDetailPreservesSheenRoughnessTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage sheenRoughnessMap(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < sheenRoughnessMap.height(); ++y) {
        for (int x = 0; x < sheenRoughnessMap.width(); ++x) {
            const int roughness = x < sheenRoughnessMap.width() / 2 ? 12 : 245;
            sheenRoughnessMap.setPixelColor(
                x, y, QColor(roughness, roughness, roughness, 255));
        }
    }
    const QString sheenRoughnessMapPath =
        textureDirectory.filePath(QStringLiteral("sheen-roughness.png"));
    ASSERT_TRUE(sheenRoughnessMap.save(sheenRoughnessMapPath));

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheen"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenRoughness"), 0.8f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenTint"), QColor(255, 255, 255)));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenRoughnessTexture"),
        sheenRoughnessMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenRoughnessTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.sheenRoughnessTexture"),
        sheenRoughnessMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(mediumWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(unknownWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "the sheen roughness map must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, mediumWithMap), 0)
        << "Medium material LOD should retain the sheen roughness map";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain the sheen roughness map";
    EXPECT_EQ(changedPixelCount(highWithMap, unknownWithMap), 0)
        << "unknown material LOD should retain the full-quality sheen map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "restoring the map should reproduce High after LOD changes";
}

TEST(Model3DLayerImageTest, LowDetailPreservesAnisotropyTextureContribution)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir textureDirectory;
    ASSERT_TRUE(textureDirectory.isValid());
    QImage anisotropyMap(32, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < anisotropyMap.height(); ++y) {
        for (int x = 0; x < anisotropyMap.width(); ++x) {
            const QColor direction = x < anisotropyMap.width() / 2
                ? QColor(255, 128, 255, 255)
                : QColor(128, 255, 255, 255);
            anisotropyMap.setPixelColor(x, y, direction);
        }
    }
    const QString anisotropyMapPath =
        textureDirectory.filePath(QStringLiteral("anisotropy.png"));
    ASSERT_TRUE(anisotropyMap.save(anisotropyMapPath));

    Artifact::Artifact3DLayer model;
    model.setFixedGeometry(Artifact::FixedGeometry3D::Sphere);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("geometry.segments"), 64));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("geometry.rings"), 40));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.metallic"), 0.9f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.roughness"), 0.3f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.anisotropy"), 1.0f));
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.anisotropyTexture"), anisotropyMapPath));

    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Parallel);
    light.setColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.transform3D().setCurrentRotationY(90.0f);
    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({sceneLightForTest(light)});

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Medium);
    const QImage mediumWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::Low);
    const QImage lowWithMap = imageEnvironment->render(&model);
    renderer->setDetailLevel(static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage unknownWithMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.anisotropyTexture"), QString()));
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
    const QImage highWithoutMap = imageEnvironment->render(&model);
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.anisotropyTexture"), anisotropyMapPath));
    const QImage restoredHighWithMap = imageEnvironment->render(&model);

    ASSERT_FALSE(highWithMap.isNull());
    ASSERT_FALSE(mediumWithMap.isNull());
    ASSERT_FALSE(lowWithMap.isNull());
    ASSERT_FALSE(unknownWithMap.isNull());
    ASSERT_FALSE(highWithoutMap.isNull());
    ASSERT_FALSE(restoredHighWithMap.isNull());
    EXPECT_GT(visiblePixelCount(highWithMap), 250);
    EXPECT_GT(changedPixelCount(highWithMap, highWithoutMap), 20)
        << "the anisotropy map must visibly affect the model";
    EXPECT_EQ(changedPixelCount(highWithMap, mediumWithMap), 0)
        << "Medium material LOD should retain the anisotropy map";
    EXPECT_EQ(changedPixelCount(highWithMap, lowWithMap), 0)
        << "Low material LOD should retain the anisotropy map";
    EXPECT_EQ(changedPixelCount(highWithMap, unknownWithMap), 0)
        << "unknown material LOD should retain the full-quality anisotropy map";
    EXPECT_EQ(changedPixelCount(highWithMap, restoredHighWithMap), 0)
        << "restoring the map should reproduce High after LOD changes";
}

TEST(Model3DLayerImageTest,
     ModelGeometryDrawLODCurrentlyIgnoresRequestedDetailLevel)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::Artifact3DLayer model;
    model.setFixedGeometry(Artifact::FixedGeometry3D::Sphere);
    ASSERT_TRUE(model.setLayerPropertyValue(QStringLiteral("geometry.segments"), 48));
    ASSERT_TRUE(model.setLayerPropertyValue(QStringLiteral("geometry.rings"), 32));

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setSceneLights({});

    for (const Artifact::DetailLevel lod : {Artifact::DetailLevel::Low,
                                            Artifact::DetailLevel::Medium,
                                            Artifact::DetailLevel::High,
                                            static_cast<Artifact::DetailLevel>(99)}) {
        renderer->setDetailLevel(lod);
        renderer->clear();
        model.draw(renderer);
        renderer->flushGizmo3D();
        const QImage regularDraw = renderer->readbackToImage();
        ASSERT_FALSE(regularDraw.isNull())
            << "regular draw LOD=" << static_cast<int>(lod);

        renderer->setDetailLevel(lod);
        renderer->clear();
        model.drawLOD(renderer, lod);
        renderer->flushGizmo3D();
        const QImage lodDraw = renderer->readbackToImage();
        ASSERT_FALSE(lodDraw.isNull()) << "LOD=" << static_cast<int>(lod);
        EXPECT_EQ(changedPixelCount(regularDraw, lodDraw), 0)
            << "current 3D model drawLOD delegates to regular draw; LOD="
            << static_cast<int>(lod);
    }
    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest, SelectionWireframeImageChangesAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    auto layer = ArtifactCore::makeShared<Artifact::Artifact3DLayer>();
    layer->setFixedGeometry(Artifact::FixedGeometry3D::Sphere);
    ASSERT_TRUE(layer->setLayerPropertyValue(QStringLiteral("geometry.segments"),
                                             176));
    ASSERT_TRUE(layer->setLayerPropertyValue(QStringLiteral("geometry.rings"),
                                             176));
    ASSERT_GT(layer->mesh().polygonCount(), 24000);

    QMatrix4x4 view;
    view.lookAt(QVector3D(420.0f, 320.0f, 520.0f), QVector3D(0, 0, 0),
                QVector3D(0, 1, 0));
    QMatrix4x4 projection;
    projection.perspective(42.0f, static_cast<float>(kWidth) / kHeight,
                           1.0f, 5000.0f);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    const auto renderOverlayAt = [&](Artifact::LODManager::DetailLevel detail) {
        renderer->clear();
        renderer->setDetailLevel(detail);
        Artifact::draw3DSelectionWireframeOverlay(
            renderer, layer, &view, &projection);
        renderer->flushGizmo3D();
        return renderer->readbackToImage();
    };

    const QImage lowDetail = renderOverlayAt(Artifact::LODManager::DetailLevel::Low);
    const QImage mediumDetail =
        renderOverlayAt(Artifact::LODManager::DetailLevel::Medium);
    const QImage highDetail = renderOverlayAt(Artifact::LODManager::DetailLevel::High);
    const QImage unknownDetail = renderOverlayAt(
        static_cast<Artifact::LODManager::DetailLevel>(99));
    const QImage restoredLowDetail = renderOverlayAt(Artifact::LODManager::DetailLevel::Low);

    EXPECT_GT(visiblePixelCount(lowDetail), 20);
    EXPECT_GT(visiblePixelCount(mediumDetail), 20);
    EXPECT_GT(visiblePixelCount(highDetail), 20);
    EXPECT_EQ(changedPixelCount(highDetail, unknownDetail), 0)
        << "unknown detail values should use the High marker budget";
    EXPECT_LT(visiblePixelCount(lowDetail), visiblePixelCount(mediumDetail))
        << "Low detail should draw fewer sampled edges and vertex markers";
    EXPECT_LE(visiblePixelCount(mediumDetail), visiblePixelCount(highDetail))
        << "Higher detail should not reduce the selection overlay sampling budget";
    EXPECT_GT(changedPixelCount(lowDetail, mediumDetail), 20)
        << "Low and Medium detail should sample different polygon and marker budgets";
    EXPECT_GT(changedPixelCount(mediumDetail, highDetail), 20)
        << "Medium and High detail should sample different polygon and marker budgets";
    EXPECT_EQ(changedPixelCount(lowDetail, restoredLowDetail), 0)
        << "returning to Low should restore its polygon and marker sampling budget";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest, PlainTextCompositionDrawRemainsStableAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactTextLayer>();
    ASSERT_TRUE(layer);
    layer->setText(ArtifactCore::UniString(QStringLiteral("Text LOD")));
    layer->setFontSize(36.0f);
    layer->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    // Prime glyph layout and source bounds before the composition draw path
    // performs its early invalid-bounds check.
    layer->draw(renderer);
    renderer->flushGizmo3D();

    const auto renderAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer->flushGizmo3D();
        return renderer->readbackToImage();
    };

    const QImage highDetail = renderAtDetail(Artifact::DetailLevel::High);
    const QImage mediumDetail = renderAtDetail(Artifact::DetailLevel::Medium);
    const QImage lowDetail = renderAtDetail(Artifact::DetailLevel::Low);
    const QImage unknownDetail = renderAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredHighDetail = renderAtDetail(Artifact::DetailLevel::High);

    ASSERT_FALSE(highDetail.isNull());
    ASSERT_FALSE(mediumDetail.isNull());
    ASSERT_FALSE(lowDetail.isNull());
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_FALSE(restoredHighDetail.isNull());
    EXPECT_GT(visiblePixelCount(highDetail), 20);
    EXPECT_EQ(changedPixelCount(highDetail, mediumDetail), 0)
        << "Plain text uses direct glyph drawing and should not be downsampled";
    EXPECT_EQ(changedPixelCount(highDetail, lowDetail), 0)
        << "Plain text should remain legible at Low detail";
    EXPECT_EQ(changedPixelCount(highDetail, unknownDetail), 0)
        << "unknown detail values should retain direct full-quality text";
    EXPECT_EQ(changedPixelCount(highDetail, restoredHighDetail), 0)
        << "returning to High should preserve the original text output";

    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     DirectImageBufferPathRemainsStableAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QImage source(48, 32, QImage::Format_RGBA8888);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixelColor(x, y, ((x + y) & 1) == 0
                ? QColor(245, 180, 70, 255)
                : QColor(25, 70, 190, 255));
        }
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(layer);
    layer->setLayerName(QStringLiteral("Direct Image LOD"));
    layer->setFromQImage(source);
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto drawAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer->flushGizmo3D();
        return renderer->readbackToImage();
    };

    const QImage high = drawAtDetail(Artifact::DetailLevel::High);
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium);
    const QImage low = drawAtDetail(Artifact::DetailLevel::Low);
    const QImage unknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredHigh = drawAtDetail(Artifact::DetailLevel::High);

    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_GT(visiblePixelCount(high), 0);
    EXPECT_EQ(changedPixelCount(high, medium), 0)
        << "direct image buffers should retain the same sampling at Medium";
    EXPECT_EQ(changedPixelCount(high, low), 0)
        << "direct image buffers should retain the same sampling at Low";
    EXPECT_EQ(changedPixelCount(high, unknown), 0)
        << "unknown detail values should preserve direct image output";
    EXPECT_EQ(changedPixelCount(high, restoredHigh), 0)
        << "returning to High should reproduce the direct image output";

    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     RasterizedTextSurfaceCacheTracksDetailScaleAndRestoresHigh)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactTextLayer>();
    ASSERT_TRUE(layer);
    layer->setText(ArtifactCore::UniString(QStringLiteral("Text Surface LOD")));
    layer->setFontSize(36.0f);
    layer->setTextColor(ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(3.0f);
    glow->setGlowGain(1.0f);
    layer->addEffect(glow);
    layer->updateImage();
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    const auto dimensions = layer->sourceSize();
    ASSERT_GT(dimensions.width, 0);
    ASSERT_GT(dimensions.height, 0);

    const auto processedSize = [](const Artifact::LayerSurfaceCacheEntry& entry) {
        if (entry.processedBuffer) {
            return QSize(entry.processedBuffer->width(),
                         entry.processedBuffer->height());
        }
        return entry.processedSurface.size();
    };
    const auto expectedSize = [&](const float scale) {
        return QSize(std::max(1, static_cast<int>(std::round(
                                  static_cast<float>(dimensions.width) * scale))),
                     std::max(1, static_cast<int>(std::round(
                                  static_cast<float>(dimensions.height) * scale))));
    };
    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const auto draw = [&](const Artifact::DetailLevel detail,
                          QHash<QString, Artifact::LayerSurfaceCacheEntry>* cache) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), renderer, 1.0f, nullptr, cache, nullptr,
            0, false, detail);
        renderer->flushGizmo3D();
        return renderer->readbackToImage();
    };

    const QString ownerId = layer->id().toString();
    struct DetailCase {
        Artifact::DetailLevel detail;
        float scale;
    };
    const std::array<DetailCase, 3> details{{
        {Artifact::DetailLevel::Low, 0.5f},
        {Artifact::DetailLevel::Medium, 0.75f},
        {Artifact::DetailLevel::High, 1.0f},
    }};
    QString highSignature;
    QString previousSignature;
    QImage lowImage;
    QImage highImage;
    for (const DetailCase& detailCase : details) {
        const QImage cachedBuild = draw(detailCase.detail, &surfaceCache);
        ASSERT_FALSE(cachedBuild.isNull());
        ASSERT_TRUE(surfaceCache.contains(ownerId));
        const Artifact::LayerSurfaceCacheEntry entry = surfaceCache.value(ownerId);
        EXPECT_EQ(processedSize(entry), expectedSize(detailCase.scale));
        ASSERT_FALSE(entry.cacheSignature.isEmpty());
        if (!previousSignature.isEmpty()) {
            EXPECT_NE(entry.cacheSignature, previousSignature);
        }
        previousSignature = entry.cacheSignature;

        const QImage cachedReuse = draw(detailCase.detail, &surfaceCache);
        const QImage cacheBypass = draw(detailCase.detail, nullptr);
        ASSERT_FALSE(cachedReuse.isNull());
        ASSERT_FALSE(cacheBypass.isNull());
        EXPECT_EQ(changedPixelCount(cachedBuild, cachedReuse), 0)
            << "reusing a text surface cache should preserve output";
        EXPECT_EQ(changedPixelCount(cachedBuild, cacheBypass), 0)
            << "text surface cache and bypass should render identical pixels";

        if (detailCase.detail == Artifact::DetailLevel::High) {
            highImage = cachedBuild;
            highSignature = entry.cacheSignature;
        } else if (detailCase.detail == Artifact::DetailLevel::Low) {
            lowImage = cachedBuild;
        }
    }

    const QImage unknownDetail = draw(
        static_cast<Artifact::DetailLevel>(99), &surfaceCache);
    ASSERT_FALSE(unknownDetail.isNull());
    ASSERT_TRUE(surfaceCache.contains(ownerId));
    const Artifact::LayerSurfaceCacheEntry unknownEntry =
        surfaceCache.value(ownerId);
    EXPECT_EQ(processedSize(unknownEntry), expectedSize(1.0f));
    EXPECT_EQ(unknownEntry.cacheSignature, highSignature);
    EXPECT_EQ(changedPixelCount(highImage, unknownDetail), 0)
        << "unknown detail values should use and reproduce the High text surface";

    const QImage lowAgain = draw(Artifact::DetailLevel::Low, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(ownerId));
    EXPECT_EQ(processedSize(surfaceCache.value(ownerId)), expectedSize(0.5f));
    EXPECT_EQ(changedPixelCount(lowImage, lowAgain), 0)
        << "returning to Low should reproduce the original cached text output";
    const QImage highAgain = draw(Artifact::DetailLevel::High, &surfaceCache);
    ASSERT_TRUE(surfaceCache.contains(ownerId));
    EXPECT_EQ(processedSize(surfaceCache.value(ownerId)), expectedSize(1.0f));
    EXPECT_EQ(surfaceCache.value(ownerId).cacheSignature, highSignature);
    EXPECT_EQ(changedPixelCount(highImage, highAgain), 0)
        << "returning to High should restore the original text output";
    EXPECT_FALSE(lowAgain.isNull());

    layer->setText(ArtifactCore::UniString(
        QStringLiteral("Updated text after Low LOD")));
    layer->updateImage();
    ASSERT_TRUE(layer->hasCurrentFrameBuffer());
    const QImage updatedHigh = draw(Artifact::DetailLevel::High, &surfaceCache);
    const QImage updatedHighBypass = draw(Artifact::DetailLevel::High, nullptr);
    ASSERT_FALSE(updatedHigh.isNull());
    ASSERT_FALSE(updatedHighBypass.isNull());
    ASSERT_TRUE(surfaceCache.contains(ownerId));
    EXPECT_NE(surfaceCache.value(ownerId).cacheSignature, highSignature)
        << "changing text after visiting Low must invalidate the old High surface";
    EXPECT_EQ(processedSize(surfaceCache.value(ownerId)),
              QSize(layer->sourceSize().width, layer->sourceSize().height));
    EXPECT_EQ(changedPixelCount(updatedHigh, updatedHighBypass), 0)
        << "the rebuilt High surface should match a cache-bypassed render";

    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     NonPowerOfTwoSvgSurfaceCacheRemainsFullResolutionAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString svgPath = directory.filePath(QStringLiteral("lod-vector.svg"));
    QFile svgFile(svgPath);
    ASSERT_TRUE(svgFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray svgData = R"(<svg xmlns="http://www.w3.org/2000/svg" width="127" height="95" viewBox="0 0 127 95"><rect width="127" height="95" fill="#102030"/><circle cx="63.5" cy="47.5" r="31" fill="#ff4020"/><path d="M15 79 L111 12" stroke="#ffffff" stroke-width="3"/></svg>)";
    ASSERT_EQ(svgFile.write(svgData), svgData.size());
    svgFile.close();

    auto layer = ArtifactCore::makeShared<Artifact::ArtifactSvgLayer>();
    ASSERT_TRUE(layer);
    ASSERT_TRUE(layer->loadFromPath(svgPath));
    const auto dimensions = layer->sourceSize();
    ASSERT_EQ(dimensions.width, 127);
    ASSERT_EQ(dimensions.height, 95);
    layer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(2.0f);
    glow->setGlowGain(0.75f);
    layer->addEffect(glow);

    QImage sourceImage;
    QElapsedTimer prefetchTimeout;
    prefetchTimeout.start();
    while (sourceImage.isNull() && prefetchTimeout.elapsed() < 5000) {
        sourceImage = layer->toQImage();
        if (sourceImage.isNull()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
    }
    ASSERT_FALSE(sourceImage.isNull()) << "SVG prefetch did not produce a raster surface";

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    QHash<QString, Artifact::LayerSurfaceCacheEntry> surfaceCache;
    const QString ownerId = layer->id().toString();
    const auto draw = [&](const Artifact::DetailLevel detail,
                          QHash<QString, Artifact::LayerSurfaceCacheEntry>* cache) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            layer.get(), renderer, 1.0f, nullptr, cache, nullptr,
            0, false, detail);
        renderer->flushGizmo3D();
        return renderer->readbackToImage();
    };

    QString svgCacheSignature;
    QImage baselineImage;
    QImage highImage;
    for (const Artifact::DetailLevel detail : {
             Artifact::DetailLevel::Low,
             Artifact::DetailLevel::Medium,
             Artifact::DetailLevel::High,
             static_cast<Artifact::DetailLevel>(99)}) {
        const QImage cachedBuild = draw(detail, &surfaceCache);
        ASSERT_FALSE(cachedBuild.isNull());
        ASSERT_TRUE(surfaceCache.contains(ownerId));
        const Artifact::LayerSurfaceCacheEntry entry = surfaceCache.value(ownerId);
        const QSize cachedSize = entry.processedBuffer
            ? QSize(entry.processedBuffer->width(), entry.processedBuffer->height())
            : entry.processedSurface.size();
        EXPECT_EQ(cachedSize, QSize(dimensions.width, dimensions.height));
        ASSERT_FALSE(entry.cacheSignature.isEmpty());
        if (svgCacheSignature.isEmpty()) {
            svgCacheSignature = entry.cacheSignature;
        } else {
            EXPECT_EQ(entry.cacheSignature, svgCacheSignature)
                << "SVG raster cache identity should not depend on preview LOD";
        }

        const QImage cachedReuse = draw(detail, &surfaceCache);
        const QImage cacheBypass = draw(detail, nullptr);
        ASSERT_FALSE(cachedReuse.isNull());
        ASSERT_FALSE(cacheBypass.isNull());
        EXPECT_EQ(changedPixelCount(cachedBuild, cachedReuse), 0);
        EXPECT_EQ(changedPixelCount(cachedBuild, cacheBypass), 0);
        if (baselineImage.isNull()) {
            baselineImage = cachedBuild;
        } else {
            EXPECT_EQ(changedPixelCount(baselineImage, cachedBuild), 0)
                << "SVG effects should keep the same raster detail at every zoom LOD";
        }
        if (detail == Artifact::DetailLevel::High) {
            highImage = cachedBuild;
        }
    }

    const QImage highAgain = draw(Artifact::DetailLevel::High, &surfaceCache);
    EXPECT_EQ(changedPixelCount(highImage, highAgain), 0);
    renderer->setDetailLevel(Artifact::LODManager::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     AdjustmentLayerBackgroundSurfaceChangesAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(QStringLiteral("Adjustment LOD")),
        ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    params.setResolution(kWidth, kHeight);
    params.setDurationFrames(2);
    auto composition = ArtifactCore::makeShared<Artifact::ArtifactComposition3D>(
        ArtifactCore::CompositionID(QStringLiteral("adjustment-lod")), params);
    ASSERT_TRUE(composition);

    QImage checker(kWidth, kHeight, QImage::Format_RGBA8888);
    for (int y = 0; y < checker.height(); ++y) {
        for (int x = 0; x < checker.width(); ++x) {
            checker.setPixelColor(x, y, ((x + y) % 2 == 0)
                ? QColor(255, 255, 255, 255)
                : QColor(0, 0, 0, 255));
        }
    }
    auto imageLayer = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(imageLayer);
    imageLayer->setLayerName(QStringLiteral("Adjustment LOD checker"));
    imageLayer->setFromQImage(checker);
    ASSERT_TRUE(imageLayer->hasCurrentFrameBuffer());
    imageLayer->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    Artifact::ArtifactAdjustableLayer adjustmentLayer;
    adjustmentLayer.setComposition(composition.get());
    ASSERT_TRUE(adjustmentLayer.isAdjustmentLayer());
    ASSERT_TRUE(adjustmentLayer.localBounds().isValid());

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto renderAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            imageLayer.get(), renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, Artifact::DetailLevel::High);
        Artifact::drawLayerForCompositionView(
            &adjustmentLayer, renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer->flushAndWait();
        return renderer->readbackToImage();
    };

    const QImage low = renderAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = renderAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = renderAtDetail(Artifact::DetailLevel::High);
    const QImage unknown = renderAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredHigh = renderAtDetail(Artifact::DetailLevel::High);
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_GT(visiblePixelCount(high), 20)
        << "The checkerboard background must reach the adjustment layer";
    EXPECT_GT(changedPixelCount(low, medium), 20)
        << "Adjustment background sampling should differ between Low and Medium";
    EXPECT_GT(changedPixelCount(medium, high), 20)
        << "Adjustment background sampling should differ between Medium and High";
    EXPECT_EQ(changedPixelCount(high, unknown), 0)
        << "an unknown detail level should restore full-resolution adjustment "
           "background sampling";
    EXPECT_EQ(changedPixelCount(high, restoredHigh), 0)
        << "returning to High should restore the original adjustment output";

    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     VideoCompositionPreviewDownsamplesAcrossDetailLevelsAndRestoresHigh)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    const QString sourceDirectory =
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    const QString fixturePath = QDir::cleanPath(QDir(sourceDirectory).filePath(
        QStringLiteral("../ArtifactCore/fixtures/walkway-orbit-cc0.mp4")));
    ASSERT_TRUE(QFileInfo::exists(fixturePath)) << fixturePath.toStdString();

    Artifact::ArtifactVideoLayer layer;
    ASSERT_TRUE(layer.loadFromPath(fixturePath));
    QElapsedTimer loadTimeout;
    loadTimeout.start();
    while (!layer.isLoaded() && loadTimeout.elapsed() < 30000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    ASSERT_TRUE(layer.isLoaded()) << layer.debugState().toStdString();
    layer.preloadFrames(0, 1);
    layer.setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto drawAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            &layer, renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer->flushAndWait();
        return renderer->readbackToImage();
    };

    const QImage low = drawAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = drawAtDetail(Artifact::DetailLevel::High);
    const QImage unknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredHigh = drawAtDetail(Artifact::DetailLevel::High);

    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_EQ(low.size(), QSize(kWidth, kHeight));
    EXPECT_EQ(medium.size(), high.size());
    EXPECT_GT(changedPixelCount(low, medium), 0)
        << "Low and Medium should sample the decoded video frame at distinct scales";
    EXPECT_GT(changedPixelCount(medium, high), 0)
        << "Medium and High should sample the decoded video frame at distinct scales";
    EXPECT_GT(changedPixelCount(low, high), 0);
    EXPECT_EQ(changedPixelCount(high, unknown), 0)
        << "an unknown detail level should retain the full-resolution video "
           "preview output";
    EXPECT_EQ(changedPixelCount(high, restoredHigh), 0)
        << "returning to High should restore the original video preview output";

    const auto drawOfflineAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            &layer, renderer, 1.0f, nullptr, nullptr, nullptr,
            0, true, detail);
        renderer->flushAndWait();
        return renderer->readbackToImage();
    };
    const QImage offlineLow = drawOfflineAtDetail(Artifact::DetailLevel::Low);
    const QImage offlineMedium =
        drawOfflineAtDetail(Artifact::DetailLevel::Medium);
    const QImage offlineHigh = drawOfflineAtDetail(Artifact::DetailLevel::High);
    const QImage offlineUnknown = drawOfflineAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage offlineRestoredLow =
        drawOfflineAtDetail(Artifact::DetailLevel::Low);

    ASSERT_FALSE(offlineLow.isNull());
    ASSERT_FALSE(offlineMedium.isNull());
    ASSERT_FALSE(offlineHigh.isNull());
    ASSERT_FALSE(offlineUnknown.isNull());
    ASSERT_FALSE(offlineRestoredLow.isNull());
    EXPECT_GT(changedPixelCount(offlineLow, offlineMedium), 0)
        << "offline decode should use the requested Low and Medium scales";
    EXPECT_GT(changedPixelCount(offlineMedium, offlineHigh), 0)
        << "offline decode should use the requested Medium and High scales";
    EXPECT_EQ(changedPixelCount(offlineHigh, offlineUnknown), 0)
        << "an unknown detail level should preserve full-resolution offline "
           "video output";
    EXPECT_EQ(changedPixelCount(offlineLow, offlineRestoredLow), 0)
        << "returning to Low should reproduce the same offline video output";
}

TEST(Model3DLayerImageTest,
     ParticleLayerSpriteOutputRemainsStableAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::ArtifactParticleLayer layer;
    ASSERT_NE(layer.particleSystem(), nullptr);
    ASSERT_FALSE(layer.particleSystem()->emitters().empty());
    auto emitterParams = layer.particleSystem()->emitters().front()->params();
    emitterParams.position = QVector3D(kWidth * 0.5f, kHeight * 0.5f, 0.0f);
    emitterParams.rate = 1000.0f;
    emitterParams.lifeMin = 10.0f;
    emitterParams.lifeMax = 10.0f;
    emitterParams.speedMin = 0.0f;
    emitterParams.speedMax = 0.0f;
    emitterParams.directionSpread = 0.0f;
    emitterParams.scaleMin = 12.0f;
    emitterParams.scaleMax = 12.0f;
    emitterParams.scaleEndMin = 12.0f;
    emitterParams.scaleEndMax = 12.0f;
    emitterParams.colorStart = QColor(255, 32, 16, 255);
    emitterParams.colorMid = QColor(255, 32, 16, 255);
    emitterParams.colorEnd = QColor(255, 32, 16, 255);
    emitterParams.texturePath.clear();
    emitterParams.deterministic = true;
    emitterParams.randomSeed = 1337u;
    emitterParams.maxParticles = 128;
    layer.clearEmitters();
    ASSERT_NE(layer.addEmitter(emitterParams), nullptr);
    layer.setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto drawAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            &layer, renderer, 1.0f, nullptr, nullptr, nullptr,
            1, false, detail);
        renderer->flushAndWait();
        return renderer->readbackToImage();
    };

    const QImage low = drawAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = drawAtDetail(Artifact::DetailLevel::High);
    const QImage unknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredLow = drawAtDetail(Artifact::DetailLevel::Low);
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredLow.isNull());
    EXPECT_GT(visiblePixelCount(high), 0)
        << "the deterministic emitter should draw visible particles";
    EXPECT_EQ(changedPixelCount(low, medium), 0)
        << "the direct particle sprite path should not change at Medium LOD";
    EXPECT_EQ(changedPixelCount(medium, high), 0)
        << "the direct particle sprite path should not change at High LOD";
    EXPECT_EQ(changedPixelCount(high, unknown), 0)
        << "an unknown detail level should retain full-resolution particle sprites";
    EXPECT_EQ(changedPixelCount(low, restoredLow), 0)
        << "returning to Low should reproduce the same particle output";

    auto glow = ArtifactCore::makeShared<Artifact::GlowEffect>();
    ASSERT_TRUE(glow);
    glow->setBaseSigma(4.0f);
    glow->setGlowGain(1.0f);
    layer.addEffect(glow);

    const QImage effectedLow = drawAtDetail(Artifact::DetailLevel::Low);
    const QImage effectedMedium = drawAtDetail(Artifact::DetailLevel::Medium);
    const QImage effectedHigh = drawAtDetail(Artifact::DetailLevel::High);
    const QImage effectedUnknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredEffectedHigh = drawAtDetail(Artifact::DetailLevel::High);
    ASSERT_FALSE(effectedLow.isNull());
    ASSERT_FALSE(effectedMedium.isNull());
    ASSERT_FALSE(effectedHigh.isNull());
    ASSERT_FALSE(effectedUnknown.isNull());
    ASSERT_FALSE(restoredEffectedHigh.isNull());
    EXPECT_GT(changedPixelCount(effectedLow, effectedMedium), 0)
        << "rasterized particle effects should respond to the resolution LOD";
    EXPECT_GT(changedPixelCount(effectedMedium, effectedHigh), 0)
        << "Medium and High particle effect surfaces should remain distinct";
    EXPECT_EQ(changedPixelCount(effectedHigh, effectedUnknown), 0)
        << "an unknown detail level should use the full-resolution effect surface";
    EXPECT_EQ(changedPixelCount(effectedHigh, restoredEffectedHigh), 0)
        << "returning to High should reproduce the effected particle output";
    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     ParticleRenderPopulationLODTracksProjectedScaleAndRestores)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::ArtifactParticleLayer layer;
    ASSERT_NE(layer.particleSystem(), nullptr);
    ASSERT_FALSE(layer.particleSystem()->emitters().empty());
    auto* emitter = layer.particleSystem()->emitters().front().get();
    ASSERT_NE(emitter, nullptr);
    auto emitterParams = emitter->params();
    emitterParams.position = QVector3D(kWidth * 0.5f, kHeight * 0.5f, 0.0f);
    emitterParams.rate = 0.0f;
    emitterParams.burstCount = 512;
    emitterParams.lifeMin = 10.0f;
    emitterParams.lifeMax = 10.0f;
    emitterParams.speedMin = 0.0f;
    emitterParams.speedMax = 0.0f;
    emitterParams.scaleMin = 4.0f;
    emitterParams.scaleMax = 4.0f;
    emitterParams.scaleEndMin = 4.0f;
    emitterParams.scaleEndMax = 4.0f;
    emitterParams.texturePath.clear();
    emitterParams.deterministic = true;
    emitterParams.randomSeed = 20261017u;
    emitterParams.maxParticles = 512;
    layer.clearEmitters();
    ASSERT_NE(layer.addEmitter(emitterParams), nullptr);
    layer.setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto drawAtScale = [&](const float scale) {
        layer.transform2D().setScale(scale, scale);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            &layer, renderer, 1.0f, nullptr, nullptr, nullptr,
            1, false, Artifact::DetailLevel::High);
        renderer->flushAndWait();
        const QString state = renderer->particleDebugState();
        const qsizetype countMarker = state.indexOf(QStringLiteral(" count="));
        EXPECT_GE(countMarker, 0) << state.toStdString();
        if (countMarker < 0) return std::size_t{0};
        bool countOk = false;
        const qulonglong count = state.mid(countMarker + 7)
            .section(QLatin1Char(' '), 0, 0).toULongLong(&countOk);
        EXPECT_TRUE(countOk) << state.toStdString();
        return countOk ? static_cast<std::size_t>(count) : std::size_t{0};
    };

    const std::size_t fullScaleCount = drawAtScale(1.0f);
    const std::size_t reducedScaleCount = drawAtScale(0.25f);
    const std::size_t restoredFullScaleCount = drawAtScale(1.0f);
    const std::size_t restoredReducedScaleCount = drawAtScale(0.25f);

    EXPECT_EQ(fullScaleCount, 512u)
        << "the initial burst should exercise the particle-count LOD threshold";
    EXPECT_GE(reducedScaleCount, 64u)
        << "particle LOD should preserve its minimum representative population";
    EXPECT_LT(reducedScaleCount, fullScaleCount)
        << "a smaller projected layer should submit fewer particle instances";
    EXPECT_EQ(restoredFullScaleCount, fullScaleCount)
        << "restoring projected scale should restore the full particle population";
    EXPECT_EQ(restoredReducedScaleCount, reducedScaleCount)
        << "repeating the same projected scale should select the same particles";
    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     FormParticleDirectDrawRemainsStableAcrossDetailLevels)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::ArtifactFormParticleLayer layer;
    ASSERT_TRUE(layer.setLayerPropertyValue(QStringLiteral("form.columns"), 12));
    ASSERT_TRUE(layer.setLayerPropertyValue(QStringLiteral("form.rows"), 8));
    ASSERT_TRUE(layer.setLayerPropertyValue(QStringLiteral("form.spacingX"), 12.0));
    ASSERT_TRUE(layer.setLayerPropertyValue(QStringLiteral("form.spacingY"), 12.0));
    ASSERT_TRUE(layer.setLayerPropertyValue(QStringLiteral("particle.size"), 4.0));
    ASSERT_TRUE(layer.setLayerPropertyValue(
        QStringLiteral("particle.solidColor"), QColor(255, 64, 32, 255)));

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto drawAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->setDetailLevel(detail);
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            &layer, renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer->flushAndWait();
        return renderer->readbackToImage();
    };

    const QImage low = drawAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = drawAtDetail(Artifact::DetailLevel::High);
    const QImage unknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredLow = drawAtDetail(Artifact::DetailLevel::Low);
    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredLow.isNull());
    EXPECT_GT(visiblePixelCount(high), 0)
        << "the configured form-particle grid should draw visible points";
    EXPECT_EQ(changedPixelCount(low, medium), 0)
        << "direct form-particle drawing should remain stable at Medium LOD";
    EXPECT_EQ(changedPixelCount(medium, high), 0)
        << "direct form-particle drawing should remain stable at High LOD";
    EXPECT_EQ(changedPixelCount(high, unknown), 0)
        << "an unknown detail level should preserve full form-particle output";
    EXPECT_EQ(changedPixelCount(low, restoredLow), 0)
        << "returning to Low should reproduce the same form-particle output";
    renderer->setDetailLevel(Artifact::DetailLevel::High);
}

TEST(Model3DLayerImageTest,
     PrecompositionPreviewDownsamplesChildImageAndRestoresHigh)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    QTemporaryDir projectRoot;
    ASSERT_TRUE(projectRoot.isValid());
    qputenv("ARTIFACT_UI_TEST_PROJECT_ROOT", projectRoot.path().toUtf8());
    QCoreApplication::setApplicationName(
        QStringLiteral("ArtifactStudioUiTest"));
    QStandardPaths::setTestModeEnabled(true);

    Artifact::ArtifactApplicationManager::instance();
    auto& projects = Artifact::ArtifactProjectManager::getInstance();
    struct ProjectCleanup final {
        Artifact::ArtifactProjectManager& projects;
        ~ProjectCleanup() { projects.closeCurrentProject(); }
    } cleanup{projects};
    projects.closeCurrentProject();
    projects.createProject(QStringLiteral("CompositionLodTest"), true);
    ASSERT_TRUE(projects.isProjectCreated());

    auto* projectService = Artifact::ArtifactProjectService::instance();
    ASSERT_NE(projectService, nullptr);
    const auto createComposition = [&](const QString& name) {
        Artifact::ArtifactCompositionInitParams params;
        params.setCompositionName(ArtifactCore::UniString(name));
        params.setResolution(64, 64);
        params.setFrameRate(30.0);
        params.setDurationFrames(2);
        projectService->createComposition(params);
        return projectService->currentComposition().lock();
    };

    auto childComposition = createComposition(QStringLiteral("LOD Child"));
    ASSERT_TRUE(childComposition);
    QImage checker(64, 64, QImage::Format_RGBA8888);
    for (int y = 0; y < checker.height(); ++y) {
        for (int x = 0; x < checker.width(); ++x) {
            checker.setPixelColor(x, y, ((x + y) & 1) == 0
                ? QColor(255, 255, 255, 255)
                : QColor(0, 0, 0, 255));
        }
    }
    auto childImage = ArtifactCore::makeShared<Artifact::ArtifactImageLayer>();
    ASSERT_TRUE(childImage);
    childImage->setFromQImage(checker);
    childImage->setLayerCachePolicy(Artifact::LayerCachePolicy::Disabled);
    ASSERT_TRUE(childComposition->appendLayerTop(childImage).success);

    auto parentComposition = createComposition(QStringLiteral("LOD Parent"));
    ASSERT_TRUE(parentComposition);
    auto precomposition =
        ArtifactCore::makeShared<Artifact::ArtifactCompositionLayer>();
    ASSERT_TRUE(precomposition);
    precomposition->setCompositionId(childComposition->id());
    ASSERT_TRUE(parentComposition->appendLayerTop(precomposition).success);
    ASSERT_EQ(precomposition->sourceComposition().get(), childComposition.get());
    ASSERT_FALSE(childComposition->getThumbnailAtFrame(0, 64, 64).isNull());

    auto* renderer = imageEnvironment->renderer();
    ASSERT_NE(renderer, nullptr);
    renderer->setCanvasSize(static_cast<float>(kWidth),
                            static_cast<float>(kHeight));
    renderer->setViewportRect(static_cast<float>(kWidth),
                              static_cast<float>(kHeight));
    renderer->setPan(0.0f, 0.0f);
    renderer->setZoom(1.0f);
    renderer->setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    const auto drawAtDetail = [&](const Artifact::DetailLevel detail) {
        renderer->clear();
        Artifact::drawLayerForCompositionView(
            precomposition.get(), renderer, 1.0f, nullptr, nullptr, nullptr,
            0, false, detail);
        renderer->flushAndWait();
        return renderer->readbackToImage();
    };

    const QImage low = drawAtDetail(Artifact::DetailLevel::Low);
    const QImage medium = drawAtDetail(Artifact::DetailLevel::Medium);
    const QImage high = drawAtDetail(Artifact::DetailLevel::High);
    const QImage unknown = drawAtDetail(
        static_cast<Artifact::DetailLevel>(99));
    const QImage restoredHigh = drawAtDetail(Artifact::DetailLevel::High);

    ASSERT_FALSE(low.isNull());
    ASSERT_FALSE(medium.isNull());
    ASSERT_FALSE(high.isNull());
    ASSERT_FALSE(unknown.isNull());
    ASSERT_FALSE(restoredHigh.isNull());
    EXPECT_GT(changedPixelCount(low, medium), 0)
        << "Low and Medium should sample the child-composition thumbnail at "
           "distinct scales";
    EXPECT_GT(changedPixelCount(medium, high), 0)
        << "Medium and High should sample the child-composition thumbnail at "
           "distinct scales";
    EXPECT_GT(changedPixelCount(low, high), 0);
    EXPECT_EQ(changedPixelCount(high, unknown), 0)
        << "an unknown detail level should retain the full-resolution child "
           "composition output";
    EXPECT_EQ(changedPixelCount(high, restoredHigh), 0)
        << "returning to High should restore the original precomposition output";
}

TEST(Model3DLayerImageTest, PreRenderSequenceAppliesResolutionScaleToOutputSize)
{
    QTemporaryDir outputDirectory;
    ASSERT_TRUE(outputDirectory.isValid());

    Artifact::Artifact3DLayer layer;
    const auto originalFrame = layer.currentFrame();
    struct ScaleCase {
        double requestedScale;
        QSize expectedSize;
    };
    const std::array<ScaleCase, 9> scaleCases{{
        {0.25, QSize{31, 17}},
        {1.0, QSize{31, 17}},
        {1.5, QSize{47, 26}},
        {4.0, QSize{124, 68}},
        {0.0, QSize{31, 17}},
        {-2.0, QSize{31, 17}},
        {8.0, QSize{124, 68}},
        {std::numeric_limits<double>::quiet_NaN(), QSize{31, 17}},
        {std::numeric_limits<double>::infinity(), QSize{31, 17}},
    }};

    for (std::size_t index = 0; index < scaleCases.size(); ++index) {
        Artifact::ArtifactExportPreRenderSequenceOptions options;
        options.resolution = QSize{31, 17};
        options.startFrame = 0;
        options.endFrame = 1;
        options.frameStep = 1;
        options.resolutionScale = scaleCases[index].requestedScale;
        QVector<QString> outputPaths;
        QString errorMessage;
        const QString fileStem = QStringLiteral("lod_preview_%1").arg(index);

        ASSERT_TRUE(Artifact::ArtifactExportPreRenderPipeline::renderLayerSequence(
            &layer, outputDirectory.path(), fileStem, options,
            &outputPaths, &errorMessage))
            << errorMessage.toStdString();
        ASSERT_EQ(outputPaths.size(), 2);

        for (const QString& path : outputPaths) {
            const QImage image(path);
            ASSERT_FALSE(image.isNull()) << path.toStdString();
            EXPECT_EQ(image.size(), scaleCases[index].expectedSize)
                << "requested resolution scale " << scaleCases[index].requestedScale;
        }
    }
    EXPECT_EQ(layer.currentFrame(), originalFrame);
}

TEST(LightLayerImageTest, SpotGizmoDiffersFromPointGizmo)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }
    Artifact::ArtifactLightLayer light;
    light.setGlowEnabled(false);

    light.setLightType(Artifact::LightType::Point);
    const QImage pointImage = imageEnvironment->render(&light);
    imageEnvironment->saveCapture(QStringLiteral("light_point_gizmo"),
                                  pointImage);

    light.setLightType(Artifact::LightType::Spot);
    light.setConeLength(ArtifactCore::Units::Pixels{150.0f});
    light.setConeAngle(ArtifactCore::Units::Degrees{48.0f});
    const QImage spotImage = imageEnvironment->render(&light);
    imageEnvironment->saveCapture(QStringLiteral("light_spot_gizmo"),
                                  spotImage);

    EXPECT_GT(visiblePixelCount(pointImage), 25)
        << "Point light gizmo produced no visible pixels";
    EXPECT_GT(visiblePixelCount(spotImage), 25)
        << "Spot light gizmo produced no visible pixels";
    EXPECT_GT(changedPixelCount(pointImage, spotImage), 40)
        << "Changing the light type did not visibly change the gizmo image";
}

TEST(LightLayerImageTest, ColoredParallelLightChangesModelLighting)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }

    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.affectedByLights());
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.base.color"), QColor(255, 255, 255)));

    Artifact::ArtifactLightLayer lightLayer;
    lightLayer.setLightType(Artifact::LightType::Parallel);
    lightLayer.setColor(ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
    lightLayer.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    lightLayer.transform3D().setCurrentRotationY(180.0f);
    expectRedLightChangesModel(model, lightLayer,
                               QStringLiteral("model3d_red_parallel_light"));
}

TEST(LightLayerImageTest, ColoredPointLightChangesModelLighting)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }
    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.base.color"), QColor(255, 255, 255)));
    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Point);
    light.setColor(ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.setRange(ArtifactCore::Units::Pixels{5000.0f});
    light.transform3D().setCurrentPositionZ(350.0f);
    expectRedLightChangesModel(model, light,
                               QStringLiteral("model3d_red_point_light"));
}

TEST(LightLayerImageTest, ColoredSpotLightChangesModelLighting)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }
    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.base.color"), QColor(255, 255, 255)));
    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Spot);
    light.setColor(ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.setConeLength(ArtifactCore::Units::Pixels{5000.0f});
    light.setConeAngle(ArtifactCore::Units::Degrees{60.0f});
    light.setConeFeather(ArtifactCore::Units::Degrees{10.0f});
    light.transform3D().setCurrentPositionZ(350.0f);
    light.transform3D().setCurrentRotationY(180.0f);
    expectRedLightChangesModel(model, light,
                               QStringLiteral("model3d_red_spot_light"));
}

TEST(LightLayerImageTest, ColoredAreaLightChangesModelLighting)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }
    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.base.color"), QColor(255, 255, 255)));
    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Area);
    light.setColor(ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{1000.0f});
    light.setAreaSize(ArtifactCore::Units::Pixels{1000.0f},
                      ArtifactCore::Units::Pixels{1000.0f});
    light.transform3D().setCurrentPositionZ(350.0f);
    light.transform3D().setCurrentRotationY(180.0f);
    expectRedLightChangesModel(model, light,
                               QStringLiteral("model3d_red_area_light"));
}

TEST(LightLayerImageTest, ColoredAmbientLightChangesModelLighting)
{
    if (rendererUnavailable()) {
        GTEST_SKIP() << "No supported headless Diligent device is available";
    }
    Artifact::Artifact3DLayer model;
    ASSERT_TRUE(model.setLayerPropertyValue(
        QStringLiteral("material.base.color"), QColor(255, 255, 255)));
    Artifact::ArtifactLightLayer light;
    light.setLightType(Artifact::LightType::Ambient);
    light.setColor(ArtifactCore::FloatColor(1.0f, 0.04f, 0.02f, 1.0f));
    light.setIntensity(ArtifactCore::Units::Percent{100.0f});
    expectRedLightChangesModel(model, light,
                               QStringLiteral("model3d_red_ambient_light"));
}
