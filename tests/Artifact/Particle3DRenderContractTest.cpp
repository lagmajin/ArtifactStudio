#include <gtest/gtest.h>

#include <QByteArray>
#include <QGuiApplication>
#include <QMatrix4x4>
#include <QRectF>
#include <QString>
#include <QVector3D>
#include <QVector4D>
#include <QtCore/qglobal.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

import Artifact.Render.IRenderer;
import Graphics.ParticleData;
import Image.ImageF32x4_RGBA;

namespace {

class OffscreenGuiApplication final : public testing::Environment {
public:
    void SetUp() override
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
        static int argc = 1;
        static char applicationName[] = "ArtifactParticle3DRenderContractTest";
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

testing::Environment* const guiApplication =
    testing::AddGlobalTestEnvironment(new OffscreenGuiApplication());

class HeadlessRendererEnvironment final : public testing::Environment {
public:
    void SetUp() override { renderer_.initializeHeadless(96, 96); }
    void TearDown() override { renderer_.destroy(); }

    bool available() const { return renderer_.isInitialized(); }
    Artifact::ArtifactIRenderer& renderer() { return renderer_; }

private:
    Artifact::ArtifactIRenderer renderer_;
};

auto* const headlessRenderer = static_cast<HeadlessRendererEnvironment*>(
    testing::AddGlobalTestEnvironment(new HeadlessRendererEnvironment()));

struct RenderedParticle {
    bool rendererReady = false;
    bool imageValid = false;
    bool camera3D = false;
    int pixelCount = 0;
    double centerX = 0.0;
};

struct RenderedParticleBounds {
    bool rendererReady = false;
    bool imageValid = false;
    bool camera3D = false;
    int pixelCount = 0;
    int minX = 96;
    int maxX = -1;
    int minY = 96;
    int maxY = -1;
};

RenderedParticle renderSingleParticle(float modelX, float cameraX,
                                      float modelZ = 0.0f,
                                      float particleSize = 0.35f)
{
    constexpr int width = 96;
    constexpr int height = 96;
    if (!headlessRenderer->available()) {
        return {};
    }
    auto& renderer = headlessRenderer->renderer();
    RenderedParticle result;
    result.rendererReady = true;

    renderer.setCanvasSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.set3DCameraMatrices(
        [cameraX] {
            QMatrix4x4 view;
            view.lookAt(QVector3D(cameraX, 0.0f, 5.0f),
                        QVector3D(cameraX, 0.0f, 0.0f),
                        QVector3D(0.0f, 1.0f, 0.0f));
            return view;
        }(),
        [] {
            QMatrix4x4 projection;
            projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
            return projection;
        }());
    renderer.clear();

    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    data.options.depthTest = false;
    data.options.depthWrite = false;
    data.particles.emplace_back();
    auto& particle = data.particles.back();
    particle.px = 0.0f;
    particle.py = 0.0f;
    particle.pz = 0.0f;
    particle.vx = 0.0f;
    particle.vy = 0.0f;
    particle.vz = 0.0f;
    particle.r = 1.0f;
    particle.g = 0.2f;
    particle.b = 0.05f;
    particle.a = 1.0f;
    particle.size = particleSize;
    particle.lifetime = 1.0f;
    data.modelMatrix = {
        1.0f, 0.0f, 0.0f, modelX,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, modelZ,
        0.0f, 0.0f, 0.0f, 1.0f};

    renderer.drawParticles(data);
    result.camera3D = renderer.particleDebugState().contains(
        QStringLiteral("cameraMode=3d"));
    renderer.flushAndWait();
    const auto image = renderer.readbackToImageF32();
    if (image.width() != width || image.height() != height) {
        return result;
    }
    result.imageValid = true;

    double sumX = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            if (std::isfinite(pixel.r()) && std::isfinite(pixel.a()) &&
                pixel.r() > 0.5f && pixel.a() > 0.5f) {
                ++result.pixelCount;
                sumX += x;
            }
        }
    }
    if (result.pixelCount > 0) {
        result.centerX = sumX / result.pixelCount;
    }

    return result;
}

RenderedParticleBounds renderVelocityAlignedParticle(
    float velocityX, float velocityY, bool tiltedCamera = false)
{
    constexpr int width = 96;
    constexpr int height = 96;
    if (!headlessRenderer->available()) {
        return {};
    }
    auto& renderer = headlessRenderer->renderer();
    RenderedParticleBounds result;
    result.rendererReady = true;

    renderer.setCanvasSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    // Under the tilted camera, world X projects along screen up. This makes
    // view-space velocity alignment differ from a world-space atan2 rotation.
    QMatrix4x4 view;
    view.lookAt(tiltedCamera ? QVector3D(10.0f, 5.0f, 0.0f)
                             : QVector3D(0.0f, 0.0f, 5.0f),
                QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);
    renderer.clear();

    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::VelocityAligned;
    data.options.depthTest = false;
    data.options.depthWrite = false;
    data.particles.emplace_back();
    auto& particle = data.particles.back();
    particle.px = 0.0f;
    particle.py = 0.0f;
    particle.pz = 0.0f;
    particle.vx = velocityX;
    particle.vy = velocityY;
    particle.vz = 0.0f;
    particle.r = 1.0f;
    particle.g = 0.2f;
    particle.b = 0.05f;
    particle.a = 1.0f;
    particle.size = 0.08f;
    particle.stretch = 3.0f;
    particle.lifetime = 1.0f;

    renderer.drawParticles(data);
    result.camera3D = renderer.particleDebugState().contains(
        QStringLiteral("cameraMode=3d"));
    renderer.flushAndWait();
    const auto image = renderer.readbackToImageF32();
    if (image.width() != width || image.height() != height) {
        return result;
    }
    result.imageValid = true;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = image.getPixel(x, y);
            if (std::isfinite(pixel.r()) && std::isfinite(pixel.a()) &&
                pixel.r() > 0.5f && pixel.a() > 0.5f) {
                ++result.pixelCount;
                result.minX = std::min(result.minX, x);
                result.maxX = std::max(result.maxX, x);
                result.minY = std::min(result.minY, y);
                result.maxY = std::max(result.maxY, y);
            }
        }
    }
    return result;
}

struct DepthRenderResult {
    bool available = false;
    bool camera3D = false;
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    QVector4D occluder;
    QVector4D center;
};

DepthRenderResult renderParticleAgainstDepth(bool particleInFront)
{
    constexpr int width = 96;
    constexpr int height = 96;
    if (!headlessRenderer->available()) {
        return {};
    }
    auto& renderer = headlessRenderer->renderer();

    renderer.setCanvasSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);
    renderer.clear();

    QMatrix4x4 occluderModel;
    occluderModel.translate(0.0f, 0.0f, 1.0f);
    renderer.draw3DCard(QRectF(-1.0, -1.0, 2.0, 2.0), occluderModel,
                        ArtifactCore::FloatColor(0.0f, 0.1f, 1.0f, 1.0f),
                        1.0f, true);
    renderer.flushAndWait();
    const auto cardImage = renderer.readbackToImageF32();
    if (cardImage.width() != width || cardImage.height() != height) {
        return {true, false, false, false, {}, {}};
    }
    const auto cardCenter = cardImage.getPixel(width / 2, height / 2);

    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    data.options.depthTest = true;
    data.options.depthWrite = true;
    data.particles.emplace_back();
    auto& particle = data.particles.back();
    particle.px = 0.0f;
    particle.py = 0.0f;
    particle.pz = 0.0f;
    particle.vx = 0.0f;
    particle.vy = 0.0f;
    particle.vz = 0.0f;
    particle.r = 1.0f;
    particle.g = 0.05f;
    particle.b = 0.0f;
    particle.a = 1.0f;
    particle.size = 0.7f;
    particle.lifetime = 1.0f;
    data.modelMatrix[11] = particleInFront ? 2.0f : 0.0f;
    renderer.drawParticles(data);
    renderer.flushAndWait();
    const QString particleState = renderer.particleDebugState();
    const bool camera3D = particleState.contains(QStringLiteral("cameraMode=3d"));
    const bool depthTestEnabled = particleState.contains(QStringLiteral("depthTest=1"));
    const bool depthWriteEnabled = particleState.contains(QStringLiteral("depthWrite=1"));
    const auto image = renderer.readbackToImageF32();
    if (image.width() != width || image.height() != height) {
        return {true, camera3D, depthTestEnabled, depthWriteEnabled, {}, {}};
    }
    const auto center = image.getPixel(width / 2, height / 2);
    const DepthRenderResult result{
        true,
        camera3D,
        depthTestEnabled,
        depthWriteEnabled,
        QVector4D(cardCenter.r(), cardCenter.g(), cardCenter.b(), cardCenter.a()),
        QVector4D(center.r(), center.g(), center.b(), center.a())};

    return result;
}

struct DepthWriteRenderResult {
    bool available = false;
    bool camera3D = false;
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    bool particleQueued = false;
    QVector4D particleCenter;
    QVector4D center;
};

DepthWriteRenderResult renderCardBehindDepthReadOnlyParticle()
{
    constexpr int width = 96;
    constexpr int height = 96;
    if (!headlessRenderer->available()) {
        return {};
    }
    auto& renderer = headlessRenderer->renderer();

    renderer.setCanvasSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(60.0f, 1.0f, 0.1f, 100.0f);
    renderer.set3DCameraMatrices(view, projection);
    renderer.clear();

    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    data.options.depthTest = true;
    data.options.depthWrite = false;
    data.particles.emplace_back();
    auto& particle = data.particles.back();
    particle.px = 0.0f;
    particle.py = 0.0f;
    particle.pz = 0.0f;
    particle.vx = 0.0f;
    particle.vy = 0.0f;
    particle.vz = 0.0f;
    particle.r = 1.0f;
    particle.g = 0.05f;
    particle.b = 0.0f;
    particle.a = 1.0f;
    particle.size = 0.7f;
    particle.lifetime = 1.0f;
    data.modelMatrix[11] = 1.0f;
    renderer.drawParticles(data);
    const bool particleQueued = renderer.particleDebugState().contains(
        QStringLiteral("state=queued"));
    renderer.flushAndWait();
    const QString particleState = renderer.particleDebugState();
    const bool camera3D = particleState.contains(QStringLiteral("cameraMode=3d"));
    const bool depthTestEnabled = particleState.contains(QStringLiteral("depthTest=1"));
    const bool depthWriteEnabled = particleState.contains(QStringLiteral("depthWrite=1"));
    const auto particleImage = renderer.readbackToImageF32();
    DepthWriteRenderResult result;
    result.available = true;
    result.camera3D = camera3D;
    result.depthTestEnabled = depthTestEnabled;
    result.depthWriteEnabled = depthWriteEnabled;
    result.particleQueued = particleQueued;
    if (particleImage.width() != width || particleImage.height() != height) {
        return result;
    }
    const auto particleCenterPixel = particleImage.getPixel(width / 2, height / 2);
    result.particleCenter = QVector4D(
        particleCenterPixel.r(), particleCenterPixel.g(),
        particleCenterPixel.b(), particleCenterPixel.a());

    QMatrix4x4 cardModel;
    renderer.draw3DCard(QRectF(-1.0, -1.0, 2.0, 2.0), cardModel,
                        ArtifactCore::FloatColor(0.0f, 0.1f, 1.0f, 1.0f),
                        1.0f, true);
    renderer.flushAndWait();
    const auto image = renderer.readbackToImageF32();
    if (image.width() != width || image.height() != height) {
        return result;
    }
    const auto center = image.getPixel(width / 2, height / 2);
    result.center = QVector4D(center.r(), center.g(), center.b(), center.a());
    return result;
}

} // namespace

TEST(Particle3DRenderContractTest, CameraAndModelTransformRenderInThreeDimensions)
{
    const auto centered = renderSingleParticle(0.0f, 0.0f);
    if (!centered.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }
    const auto modelTranslated = renderSingleParticle(0.8f, 0.0f);
    if (!modelTranslated.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }
    const auto cameraMoved = renderSingleParticle(0.0f, 0.8f);
    if (!cameraMoved.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    EXPECT_TRUE(centered.imageValid);
    EXPECT_TRUE(modelTranslated.imageValid);
    EXPECT_TRUE(cameraMoved.imageValid);
    EXPECT_GT(centered.pixelCount, 0);
    EXPECT_GT(modelTranslated.pixelCount, 0);
    EXPECT_GT(cameraMoved.pixelCount, 0);
    EXPECT_TRUE(centered.camera3D);
    EXPECT_TRUE(modelTranslated.camera3D);
    EXPECT_TRUE(cameraMoved.camera3D);
    EXPECT_GT(std::abs(modelTranslated.centerX - centered.centerX), 4.0)
        << "the 3D model matrix translation should move the particle on screen";
    EXPECT_GT(std::abs(cameraMoved.centerX - centered.centerX), 4.0)
        << "the 3D view matrix change should move the particle on screen";
}

TEST(Particle3DRenderContractTest, DepthTranslationChangesPerspectiveFootprint)
{
    constexpr float smallParticleSize = 0.08f;
    const auto farther = renderSingleParticle(0.0f, 0.0f, 0.0f,
                                               smallParticleSize);
    if (!farther.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }
    const auto nearer = renderSingleParticle(0.0f, 0.0f, 1.0f,
                                              smallParticleSize);
    if (!nearer.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    EXPECT_TRUE(farther.imageValid);
    EXPECT_TRUE(nearer.imageValid);
    EXPECT_GT(farther.pixelCount, 0);
    EXPECT_TRUE(farther.camera3D);
    EXPECT_TRUE(nearer.camera3D);
    EXPECT_GT(nearer.pixelCount, farther.pixelCount)
        << "moving the particle toward the perspective camera should enlarge its footprint";
}

TEST(Particle3DRenderContractTest, VelocityAlignedBillboardFollowsViewSpaceVelocity)
{
    const auto horizontalVelocity = renderVelocityAlignedParticle(1.0f, 0.0f);
    if (!horizontalVelocity.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }
    const auto verticalVelocity = renderVelocityAlignedParticle(0.0f, 1.0f);
    if (!verticalVelocity.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    ASSERT_TRUE(horizontalVelocity.imageValid);
    ASSERT_TRUE(verticalVelocity.imageValid);
    ASSERT_GT(horizontalVelocity.pixelCount, 0);
    ASSERT_GT(verticalVelocity.pixelCount, 0);
    EXPECT_TRUE(horizontalVelocity.camera3D);
    EXPECT_TRUE(verticalVelocity.camera3D);
    const int verticalWidth = horizontalVelocity.maxX - horizontalVelocity.minX + 1;
    const int verticalHeight = horizontalVelocity.maxY - horizontalVelocity.minY + 1;
    const int horizontalWidth = verticalVelocity.maxX - verticalVelocity.minX + 1;
    const int horizontalHeight = verticalVelocity.maxY - verticalVelocity.minY + 1;
    EXPECT_LT(verticalWidth, verticalHeight)
        << "X velocity should orient the stretched billboard vertically";
    EXPECT_GT(horizontalWidth, horizontalHeight)
        << "Y velocity should orient the stretched billboard horizontally";

    const auto tiltedXVelocity = renderVelocityAlignedParticle(1.0f, 0.0f, true);
    if (!tiltedXVelocity.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }
    ASSERT_TRUE(tiltedXVelocity.imageValid);
    ASSERT_GT(tiltedXVelocity.pixelCount, 0);
    EXPECT_TRUE(tiltedXVelocity.camera3D);
    const int tiltedWidth = tiltedXVelocity.maxX - tiltedXVelocity.minX + 1;
    const int tiltedHeight = tiltedXVelocity.maxY - tiltedXVelocity.minY + 1;
    EXPECT_GT(tiltedWidth, tiltedHeight)
        << "the tilted camera projects world X vertically; "
           "the billboard should follow screen-space velocity";
}

TEST(Particle3DRenderContractTest, DepthTestRespectsOccluderAndNearParticle)
{
    const auto behindOccluder = renderParticleAgainstDepth(false);
    const auto inFront = renderParticleAgainstDepth(true);
    if (!behindOccluder.available || !inFront.available) {
        GTEST_SKIP() << "Diligent headless depth rendering is unavailable on this host";
    }

    EXPECT_TRUE(behindOccluder.camera3D);
    EXPECT_TRUE(inFront.camera3D);
    EXPECT_TRUE(behindOccluder.depthTestEnabled);
    EXPECT_TRUE(behindOccluder.depthWriteEnabled);
    EXPECT_TRUE(inFront.depthTestEnabled);
    EXPECT_TRUE(inFront.depthWriteEnabled);
    EXPECT_GT(behindOccluder.center.w(), 0.95f)
        << "the occluder must produce an opaque center pixel";
    EXPECT_GT(behindOccluder.occluder.z(), 0.7f)
        << "the 3D card must write the expected color before testing occlusion";
    EXPECT_GT(inFront.center.w(), 0.95f)
        << "the front particle must produce an opaque center pixel";
    EXPECT_GT(behindOccluder.center.z(), 0.7f)
        << "a nearer opaque 3D card should occlude the particle behind it; rgba="
        << behindOccluder.center.x() << ',' << behindOccluder.center.y() << ','
        << behindOccluder.center.z() << ',' << behindOccluder.center.w()
        << " occluder=" << behindOccluder.occluder.x() << ','
        << behindOccluder.occluder.y() << ',' << behindOccluder.occluder.z()
        << ',' << behindOccluder.occluder.w();
    EXPECT_GT(inFront.center.x(), 0.7f)
        << "a particle in front of the card should pass the depth test; rgba="
        << inFront.center.x() << ',' << inFront.center.y() << ','
        << inFront.center.z() << ',' << inFront.center.w();
}

TEST(Particle3DRenderContractTest, DepthWriteDisabledDoesNotOccludeLaterCard)
{
    const auto result = renderCardBehindDepthReadOnlyParticle();
    if (!result.available) {
        GTEST_SKIP() << "Diligent headless depth rendering is unavailable on this host";
    }

    EXPECT_TRUE(result.camera3D);
    EXPECT_TRUE(result.depthTestEnabled);
    EXPECT_FALSE(result.depthWriteEnabled);
    EXPECT_TRUE(result.particleQueued);
    EXPECT_GT(result.particleCenter.x(), 0.7f)
        << "the particle must render before testing its depth-write behavior";
    EXPECT_GT(result.particleCenter.w(), 0.95f)
        << "the pre-card particle pixel must be opaque";
    EXPECT_GT(result.center.w(), 0.95f);
    EXPECT_LT(result.center.x(), 0.3f)
        << "the card behind a depth-read-only particle should render over it; rgba="
        << result.center.x() << ',' << result.center.y() << ','
        << result.center.z() << ',' << result.center.w();
    EXPECT_GT(result.center.z(), 0.7f)
        << "depthWrite=false must leave the depth buffer available to later geometry";
}

TEST(Particle3DRenderContractTest, TwoDimensionalParticleStillRendersWithoutDepth)
{
    constexpr int width = 96;
    constexpr int height = 96;
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }
    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.reset3DCameraMatrices();
    renderer.clear();

    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    data.options.depthTest = true;
    data.options.depthWrite = true;
    data.particles.emplace_back();
    auto& particle = data.particles.back();
    particle.px = static_cast<float>(width) * 0.5f;
    particle.py = static_cast<float>(height) * 0.5f;
    particle.pz = 0.0f;
    particle.vx = 0.0f;
    particle.vy = 0.0f;
    particle.vz = 0.0f;
    particle.r = 1.0f;
    particle.g = 0.2f;
    particle.b = 0.05f;
    particle.a = 1.0f;
    particle.size = 8.0f;
    particle.lifetime = 1.0f;
    data.modelMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    renderer.drawParticles(data);
    renderer.flushAndWait();
    const QString drawState = renderer.particleDebugState();
    EXPECT_TRUE(drawState.contains(QStringLiteral("cameraMode=2d")))
        << "the 2D particle contract must use the 2D canvas path";
    EXPECT_TRUE(drawState.contains(QStringLiteral("depthTest=0")))
        << "the 2D particle path must disable depth testing";
    EXPECT_TRUE(drawState.contains(QStringLiteral("depthWrite=0")))
        << "the 2D particle path must disable depth writes";
    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), width);
    ASSERT_EQ(image.height(), height);

    int redPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto pixel = image.getPixel(x, y);
            if (pixel.r() > 0.5f && pixel.a() > 0.5f) {
                ++redPixels;
            }
        }
    }
    EXPECT_GT(redPixels, 0)
        << "the 2D particle path must continue drawing with depth disabled";
}
