#include <gtest/gtest.h>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QRect>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QVector3D>

#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
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
import Artifact.Layer.Light;
import Artifact.Layers.Model3D;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.IRenderer;
import Artifact.Render.OffscreenComposition;
import Color.Float;
import Core.Light;
import Frame.Position;
import Math.Vec;

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

\n