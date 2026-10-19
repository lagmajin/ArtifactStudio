#include <gtest/gtest.h>

#include <QByteArray>
#include <QMatrix4x4>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QVector3D>
#include <QVector4D>
#include <QtCore/qglobal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "DiligentHeadlessRenderer.hpp"
import Graphics.ParticleData;
import Image.ImageF32x4_RGBA;
import Artifact.Layer.Particle;
import Artifact.Generator.Particle;

namespace {

class HeadlessRendererEnvironment final : public testing::Environment {
public:
    void SetUp() override { renderer_.initialize(96, 96); }
    void TearDown() override { renderer_.destroy(); }

    bool available() const { return renderer_.isInitialized(); }
    Artifact::ArtifactIRenderer& renderer() { return renderer_; }

private:
    ArtifactTest::HeadlessRenderer renderer_;
};

auto* const headlessRenderer = static_cast<HeadlessRendererEnvironment*>(
    testing::AddGlobalTestEnvironment(new HeadlessRendererEnvironment()));

struct RenderedParticle {
    bool rendererReady = false;
    bool imageValid = false;
    bool camera3D = false;
    int pixelCount = 0;
    double centerX = 0.0;
    std::array<std::uint8_t, 96 * 96> coverage{};
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
                                      float particleSize = 0.35f,
                                      int deferredPaddingPackets = 0)
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

    // The submitter switches to deferred recording at 16 packets. These
    // offscreen rectangles exercise that packet-count path without changing
    // the visible particle image.
    for (int index = 0; index < deferredPaddingPackets; ++index) {
        renderer.drawSolidRect(-40.0f - static_cast<float>(index) * 3.0f,
                               -40.0f, 2.0f, 2.0f,
                               ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f));
    }

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
                result.coverage[static_cast<std::size_t>(y * width + x)] = 1;
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

TEST(Particle3DRenderContractTest, DeferredSubmitPreservesParticleImage)
{
    const auto immediate = renderSingleParticle(0.0f, 0.0f);
    const auto deferred = renderSingleParticle(0.0f, 0.0f, 0.0f, 0.35f, 15);
    if (!immediate.rendererReady || !deferred.rendererReady) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    ASSERT_TRUE(immediate.imageValid);
    ASSERT_TRUE(deferred.imageValid);
    EXPECT_TRUE(immediate.camera3D);
    EXPECT_TRUE(deferred.camera3D);
    EXPECT_GT(immediate.pixelCount, 0);
    EXPECT_GT(deferred.pixelCount, 0);
    EXPECT_EQ(deferred.pixelCount, immediate.pixelCount);
    EXPECT_NEAR(deferred.centerX, immediate.centerX, 0.01);
    EXPECT_EQ(deferred.coverage, immediate.coverage);
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

TEST(Particle3DRenderContractTest, ParticleLayerSurfacePathAppliesScaleBasedLODSteps)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 18000.0f;
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);

    const auto queuedCount = [](const QString& state) {
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };
    const auto queuedCountAtScale = [&](double scaleX, double scaleY,
                                        double rotationDegrees = 0.0) {
        QTransform transform;
        transform.rotate(rotationDegrees);
        transform.scale(scaleX, scaleY);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, 1)) return -1;
        return queuedCount(renderer.particleDebugState());
    };

    // Reuse frame 1 for every draw so the measured source population is identical.
    const int highCount = queuedCountAtScale(1.0, 1.0);
    const int thresholdCount = queuedCountAtScale(0.75, 0.75);
    const int justAboveThresholdCount = queuedCountAtScale(
        std::nextafter(0.75, 1.0), std::nextafter(0.75, 1.0));
    const int adjacentBelowThresholdCount = queuedCountAtScale(
        std::nextafter(0.75, 0.0), std::nextafter(0.75, 0.0));
    const int justBelowThresholdCount = queuedCountAtScale(0.749, 0.749);
    const int mediumCount = queuedCountAtScale(0.60, 0.60);
    const int lowCount = queuedCountAtScale(0.375, 0.375);
    const int rotatedLowCount = queuedCountAtScale(0.375, 0.375, 37.0);
    const int rotatedMediumCount = queuedCountAtScale(0.60, 0.60, -53.0);
    const int mirroredLowCount = queuedCountAtScale(-0.375, -0.375);
    const int mirroredSingleAxisCount = queuedCountAtScale(-0.375, 0.375);
    const int zeroScaleCount = queuedCountAtScale(0.0, 0.0);
    const int tinyPositiveScaleCount = queuedCountAtScale(1.0e-30, 1.0e-30);
    const int hugeFiniteScaleCount = queuedCountAtScale(1.0e20, 1.0e20);
    const int anisotropicCount = queuedCountAtScale(0.375, 0.9);
    const int swappedAnisotropicCount = queuedCountAtScale(0.9, 0.375);
    const double nanScale = std::numeric_limits<double>::quiet_NaN();
    const int nanScaleCount = queuedCountAtScale(nanScale, nanScale);
    const int infiniteScaleCount = queuedCountAtScale(
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity());
    const int restoredHighCount = queuedCountAtScale(1.0, 1.0);
    ASSERT_GT(highCount, 256);
    EXPECT_EQ(thresholdCount, highCount);
    EXPECT_EQ(justAboveThresholdCount, highCount)
        << "the representable scale immediately above the threshold should keep all particles";
    EXPECT_GE(static_cast<double>(adjacentBelowThresholdCount) / highCount, 0.98)
        << "the first representable scale below the threshold should avoid an abrupt count drop";
    EXPECT_GE(static_cast<double>(justBelowThresholdCount) / highCount, 0.98)
        << "crossing just below the LOD threshold should not abruptly discard "
           "about half of the particles";
    // A nominal 80% target is sampled with a fixed stride. For 1000 source
    // particles that produces 500 selected particles at this scale.
    EXPECT_NEAR(static_cast<double>(mediumCount) / highCount, 0.5, 0.02);
    EXPECT_NEAR(static_cast<double>(lowCount) / highCount, 0.5, 0.02);
    EXPECT_GT(highCount, mediumCount);
    EXPECT_GT(mediumCount, lowCount);
    EXPECT_EQ(mirroredLowCount, lowCount);
    EXPECT_EQ(rotatedLowCount, lowCount)
        << "rotation should not change the effective uniform screen scale";
    EXPECT_EQ(rotatedMediumCount, mediumCount)
        << "rotation should preserve the medium-LOD particle budget";
    EXPECT_EQ(mirroredSingleAxisCount, lowCount)
        << "mirroring either axis should preserve the effective LOD scale";
    EXPECT_EQ(zeroScaleCount, 64)
        << "a fully collapsed surface should use the minimum particle budget";
    EXPECT_EQ(tinyPositiveScaleCount, zeroScaleCount)
        << "a tiny positive scale should hit the same minimum-scale LOD budget";
    EXPECT_EQ(hugeFiniteScaleCount, highCount)
        << "a huge but finite screen scale should retain every particle";
    EXPECT_EQ(anisotropicCount, highCount)
        << "the largest axis scale should control particle LOD";
    EXPECT_EQ(swappedAnisotropicCount, anisotropicCount)
        << "swapping anisotropic axes should preserve the selected particle count";
    EXPECT_EQ(nanScaleCount, highCount)
        << "non-finite surface scale should preserve the complete particle set";
    EXPECT_EQ(infiniteScaleCount, highCount)
        << "infinite surface scale should not reduce the particle set";
    EXPECT_EQ(restoredHighCount, highCount)
        << "returning to full scale after a reduced draw should restore every particle";

    const auto firstParticleSampleAtScale = [&](double scale) {
        QTransform transform;
        transform.scale(scale, scale);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, 1)) return QString{};
        const QString state = renderer.particleDebugState();
        const int sampleStart = state.indexOf(QStringLiteral(" sampleRGBA="));
        const int sampleEnd =
            state.indexOf(QStringLiteral(" targetFormat="), sampleStart);
        if (sampleStart < 0 || sampleEnd < 0) return QString{};
        return state.mid(sampleStart, sampleEnd - sampleStart);
    };
    const QString highSample = firstParticleSampleAtScale(1.0);
    const QString lowSample = firstParticleSampleAtScale(0.375);
    const QString restoredLowSample = firstParticleSampleAtScale(0.375);
    ASSERT_FALSE(highSample.isEmpty());
    ASSERT_FALSE(lowSample.isEmpty());
    ASSERT_FALSE(restoredLowSample.isEmpty());
    EXPECT_EQ(lowSample, highSample)
        << "LOD subsampling should retain the first source particle's color, "
           "size, and age";
    EXPECT_EQ(restoredLowSample, lowSample)
        << "repeating the same reduced LOD should preserve the sampled particle attributes";
}

TEST(Particle3DRenderContractTest, ParticleLODMediumCountIsDeterministic)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 18000.0f;
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);

    const auto queuedCountAtMediumScale = [&] {
        QTransform transform;
        transform.scale(0.60, 0.60);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, 1)) return -1;
        const QString state = renderer.particleDebugState();
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };

    const int firstCount = queuedCountAtMediumScale();
    const int secondCount = queuedCountAtMediumScale();
    ASSERT_GT(firstCount, 0);
    EXPECT_EQ(secondCount, firstCount);
    EXPECT_EQ(firstCount, 500);
}

TEST(Particle3DRenderContractTest,
     ThreeDimensionalLayerDrawAppliesModelScaleLODAndRestoresFullCount)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->setIs3D(true);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 18000.0f;
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(45.0f, 1.0f, 0.1f, 100.0f);

    const auto queuedCountAtModelScale = [&](const float scaleX,
                                             const float scaleY) {
        layer->transform3D().setCurrentScale(scaleX, scaleY);
        renderer.clear();
        renderer.set3DCameraMatrices(view, projection);
        layer->draw(&renderer);
        const QString state = renderer.particleDebugState();
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };

    const int fullCount = queuedCountAtModelScale(1.0f, 1.0f);
    const int reducedCount = queuedCountAtModelScale(0.375f, 0.375f);
    const int wideThinCount = queuedCountAtModelScale(1.0f, 0.1f);
    const int narrowTallCount = queuedCountAtModelScale(0.1f, 1.0f);
    const int restoredFullCount = queuedCountAtModelScale(1.0f, 1.0f);
    renderer.reset3DCameraMatrices();
    ASSERT_GE(fullCount, 256);
    EXPECT_NEAR(static_cast<double>(reducedCount) / fullCount, 0.5, 0.02)
        << "the 3D layer model scale should drive the shared particle LOD budget";
    EXPECT_EQ(wideThinCount, fullCount)
        << "a full horizontal model scale should preserve the particle budget";
    EXPECT_EQ(narrowTallCount, fullCount)
        << "a full vertical model scale should preserve the particle budget";
    EXPECT_EQ(restoredFullCount, fullCount)
        << "returning the 3D layer to full scale should restore every particle";
}

TEST(Particle3DRenderContractTest,
     ParticleLODSubsamplingKeepsAlphaSortOrderAtEveryScale)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->setIs3D(true);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 18000.0f;
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);
    layer->particleSystem()->renderSettings().blendMode =
        Artifact::ParticleBlendMode::Normal;
    layer->particleSystem()->renderSettings().depthTest = false;
    layer->particleSystem()->renderSettings().depthWrite = false;

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 5.0f), QVector3D(),
                QVector3D(0.0f, 1.0f, 0.0f));
    QMatrix4x4 projection;
    projection.perspective(45.0f, 1.0f, 0.1f, 100.0f);

    struct SortSample {
        int count = -1;
        float firstAge = -1.0f;
    };
    const auto drawAtScale = [&](const float scale,
                                 const Artifact::ParticleRenderSettings::SortMode sortMode) {
        layer->transform3D().setCurrentScale(scale, scale);
        layer->particleSystem()->renderSettings().sortMode = sortMode;
        renderer.clear();
        renderer.set3DCameraMatrices(view, projection);
        layer->draw(&renderer);

        const QString state = renderer.particleDebugState();
        constexpr char countMarker[] = "state=queued count=";
        constexpr char ageMarker[] = " sampleAge=";
        const int countPosition = state.indexOf(QLatin1String(countMarker));
        const int agePosition = state.indexOf(QLatin1String(ageMarker));
        if (countPosition < 0 || agePosition < 0) return SortSample{};

        const int countStart = countPosition +
            static_cast<int>(sizeof(countMarker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        const int ageStart = agePosition +
            static_cast<int>(sizeof(ageMarker) - 1);
        const int ageEnd = state.indexOf(QLatin1Char('/'), ageStart);
        bool countOk = false;
        bool ageOk = false;
        const int count = state.mid(countStart, countEnd - countStart)
                              .toInt(&countOk);
        const float age = state.mid(ageStart, ageEnd - ageStart)
                              .toFloat(&ageOk);
        return countOk && ageOk ? SortSample{count, age} : SortSample{};
    };

    const auto noneHigh = drawAtScale(
        1.0f, Artifact::ParticleRenderSettings::SortMode::None);
    const auto oldestHigh = drawAtScale(
        1.0f, Artifact::ParticleRenderSettings::SortMode::OldestFirst);
    const auto youngestHigh = drawAtScale(
        1.0f, Artifact::ParticleRenderSettings::SortMode::YoungestFirst);
    const auto oldestLow = drawAtScale(
        0.375f, Artifact::ParticleRenderSettings::SortMode::OldestFirst);
    const auto youngestLow = drawAtScale(
        0.375f, Artifact::ParticleRenderSettings::SortMode::YoungestFirst);
    renderer.reset3DCameraMatrices();

    ASSERT_GT(noneHigh.count, 256);
    ASSERT_EQ(oldestHigh.count, noneHigh.count);
    ASSERT_EQ(youngestHigh.count, noneHigh.count);
    ASSERT_GT(oldestLow.count, 64);
    EXPECT_LT(oldestLow.count, oldestHigh.count)
        << "Low LOD should reduce the particle population before sorting";
    EXPECT_GT(oldestHigh.firstAge, youngestHigh.firstAge)
        << "full-quality alpha sorting should order the oldest particle first "
           "and the youngest particle first, respectively";
    EXPECT_GT(oldestLow.firstAge, youngestLow.firstAge)
        << "LOD subsampling must preserve OldestFirst and YoungestFirst order";
}

TEST(Particle3DRenderContractTest, SmallParticleSetsKeepAllParticlesAtLowScale)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 7650.0f; // Exactly 255 particles in the first 1/30 s step.
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);

    const auto queuedCountAtScale = [&](double scaleX, double scaleY) {
        QTransform transform;
        transform.scale(scaleX, scaleY);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, 1)) return -1;
        const QString state = renderer.particleDebugState();
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };

    const int fullCount = queuedCountAtScale(1.0, 1.0);
    const int lowCount = queuedCountAtScale(0.1, 0.1);
    ASSERT_GT(fullCount, 0);
    ASSERT_EQ(fullCount, 255);
    EXPECT_EQ(lowCount, fullCount);
}

TEST(Particle3DRenderContractTest, ParticleLODStartsReducingAt256Particles)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 7680.0f; // Exactly 256 particles in the first 1/30 s step.
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);

    const auto queuedCountAtScale = [&](double scale) {
        QTransform transform;
        transform.scale(scale, scale);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, 1)) return -1;
        const QString state = renderer.particleDebugState();
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };

    const int fullCount = queuedCountAtScale(1.0);
    const int thresholdCount = queuedCountAtScale(0.75);
    const int lowCount = queuedCountAtScale(0.1);
    ASSERT_EQ(fullCount, 256);
    EXPECT_EQ(thresholdCount, fullCount)
        << "the 0.75 screen-scale threshold should retain all 256 particles";
    EXPECT_EQ(lowCount, 64);
}

TEST(Particle3DRenderContractTest,
     ParticleLODReclassifiesPopulationAcrossFramesAndRestoresEarlierFrame)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 3840.0f; // 128 particles per 1/30 s frame.
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);

    const auto queuedCountAtFrameAndScale = [&](const int frame,
                                                 const double scale) {
        QTransform transform;
        transform.scale(scale, scale);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, frame)) return -1;
        const QString state = renderer.particleDebugState();
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };

    const int firstFrameFull = queuedCountAtFrameAndScale(1, 1.0);
    const int firstFrameLow = queuedCountAtFrameAndScale(1, 0.1);
    const int thresholdFrameFull = queuedCountAtFrameAndScale(2, 1.0);
    const int thresholdFrameLow = queuedCountAtFrameAndScale(2, 0.1);
    const int restoredFrameLow = queuedCountAtFrameAndScale(1, 0.1);

    EXPECT_EQ(firstFrameFull, 128);
    EXPECT_EQ(firstFrameLow, firstFrameFull)
        << "a population below the LOD threshold should remain complete";
    EXPECT_EQ(thresholdFrameFull, 256);
    EXPECT_EQ(thresholdFrameLow, 64)
        << "crossing the threshold on the next frame should activate Low LOD";
    EXPECT_EQ(restoredFrameLow, firstFrameLow)
        << "returning to the smaller earlier population should disable "
           "subsampling again";
}

TEST(Particle3DRenderContractTest, ParticleLodMinimumBudgetSurvivesOddPopulation)
{
    if (!headlessRenderer->available()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    auto& renderer = headlessRenderer->renderer();
    renderer.setCanvasSize(96.0f, 96.0f);
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    auto layer = Artifact::createParticleLayer();
    ASSERT_NE(layer.get(), nullptr);
    layer->clearEmitters();
    Artifact::EmitterParams emitter;
    emitter.rate = 7710.0f; // Exactly 257 particles in the first 1/30 s step.
    emitter.lifeMin = 10.0f;
    emitter.lifeMax = 10.0f;
    emitter.speedMin = 0.0f;
    emitter.speedMax = 0.0f;
    emitter.directionSpread = 0.0f;
    emitter.maxParticles = 1000;
    ASSERT_NE(layer->addEmitter(emitter), nullptr);

    const auto queuedCountAtLowScale = [&] {
        QTransform transform;
        transform.scale(0.1, 0.1);
        renderer.clear();
        if (!layer->drawSurfaceGPU(&renderer, transform, 1)) return -1;
        const QString state = renderer.particleDebugState();
        constexpr char marker[] = "state=queued count=";
        const int markerPosition = state.indexOf(QLatin1String(marker));
        if (markerPosition < 0) return -1;
        const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
        const int countEnd = state.indexOf(QLatin1Char(' '), countStart);
        return state.mid(countStart, countEnd - countStart).toInt();
    };

    QTransform fullScale;
    renderer.clear();
    ASSERT_TRUE(layer->drawSurfaceGPU(&renderer, fullScale, 1));
    const QString fullState = renderer.particleDebugState();
    constexpr char marker[] = "state=queued count=";
    const int markerPosition = fullState.indexOf(QLatin1String(marker));
    ASSERT_GE(markerPosition, 0);
    const int countStart = markerPosition + static_cast<int>(sizeof(marker) - 1);
    const int countEnd = fullState.indexOf(QLatin1Char(' '), countStart);
    const int fullCount = fullState.mid(countStart, countEnd - countStart).toInt();
    ASSERT_EQ(fullCount, 257);

    const int firstLowCount = queuedCountAtLowScale();
    const int secondLowCount = queuedCountAtLowScale();
    EXPECT_EQ(firstLowCount, secondLowCount)
        << "the low-LOD sample count should be deterministic";
    EXPECT_GE(firstLowCount, 64)
        << "stride rounding must not lower the configured minimum particle budget";
    EXPECT_LT(firstLowCount, fullCount);
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
